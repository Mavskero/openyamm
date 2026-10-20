#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/maps/SaveGame.h"
#include "game/ui/screens/CutsceneVideoScreen.h"
#include "tools/mmerge_import/Mm8EndgameResources.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Lord
{
    uint16_t gate;
    const char *pMap;
    uint32_t npc;
    uint32_t house;
    uint16_t topic;
    uint32_t completed;
    uint32_t award;
};
constexpr std::array<Lord, 4> Lords{{
    {502, "d36.blv", 26, 661, 167, 55, 14},
    {504, "d38.blv", 24, 662, 168, 53, 15},
    {505, "d39.blv", 25, 663, 169, 51, 16},
    {503, "d37.blv", 23, 664, 170, 49, 17},
}};

struct Step
{
    std::string name;
    std::function<void()> action;
    unsigned duration = 40;
    bool screenshot = true;
};

void checkSavedCompletion(const GameSaveData &save)
{
    require(save.mapFileName == "out02.odm", "Completion save is not in Ravenshore");
    for (uint32_t bit : {49, 51, 53, 55, 56, 97, 228})
    {
        require(save.party.questBits.contains(bit), "Missing saved completion Q" + std::to_string(bit));
    }
    require(!save.party.questBits.contains(220), "Saved final reporting quest remains active");
    const auto finished = save.namedGlobalVars.find("MMerge.CrossContinents.ContinentFinished.1");
    require(finished != save.namedGlobalVars.end() && finished->second == 1, "Saved continent bridge missing");
    require(save.outdoorWorld.eventRuntimeState.has_value(), "Saved outdoor event state missing");
    const auto &history = historyEventTimesForActiveContinent(*save.outdoorWorld.eventRuntimeState);
    require(history.contains(17) && history.contains(18), "Saved ending history missing");
    for (const Lord &lord : Lords)
    {
        require(save.party.npcHouseOverrides.at(lord.npc) == 0, "Saved released lord remains in house");
        require(save.party.npcTopicOverrides.at(lord.npc).at(0) == 0, "Saved release topic remains selectable");
    }
}
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path originals,
        std::filesystem::path seed, std::filesystem::path output, const std::string &mode)
    {
        const bool crystal = mode == "crystal-escaton";
        const bool lords = mode == "lords-ending";
        const bool cold = mode == "reload-ending";
        require(crystal || lords || cold, "Unknown MM8 endgame capture mode");
        root = std::filesystem::canonical(root);
        originals = std::filesystem::canonical(originals);
        seed = std::filesystem::canonical(seed);
        const auto profile = Import::loadMm8EndgameProfile(originals);
        require(profile.has_value(), "Audited Escaton answer source required");
        require(!std::filesystem::exists(output), "Output directory must be new");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);

        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.skipEventCutscenes = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM MM8 ending146 controlled integration";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "d10.blv";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);

        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"seed\":" << std::quoted(seed.string())
            << ",\"answer_archive\":" << std::quoted(profile->answerArchive)
            << ",\"answer_resource_sha256\":" << std::quoted(profile->answerStrSha256)
            << ",\"renderer\":\"Metal\",\"audio\":\"dummy\","
            << "\"changes\":\"crystal-escaton declares a debug map jump from the normal seed to pbp then the original506 entry to d10; "
               "lords-ending continues the earned Escaton key save; reload-ending continues the completion save. "
               "Map face handlers are called through the real world runtime without physical navigation; "
               "transitions confirm actual UI actions, houses require actual residents and topic actions. "
               "Simulation delta is frozen except videos. Crystal demonstration uses ten half-minute advances "
               "through the production timer scheduler. No direct MapVar12, Q56, Q97, Q228 or key injection. "
               "The ending video renders, then is skipped via Escape. Cold repeat test debug-jumps to d10 "
               "and returns through its original501. Four-heart gathering/delivery, combat, physical door "
               "clicks, full-length movie completion and audio acceptance are excluded.\"}\n";
        std::ofstream proof(output / "milestones.jsonl");
        std::vector<Step> steps;
        unsigned frames = 0;
        unsigned captures = 0;
        unsigned nextStep = 1;
        unsigned captureAt = 0;
        unsigned movieFrame = 0;
        size_t stepIndex = 0;
        bool complete = false;
        bool movieStarted = false;
        bool movieClosed = false;
        uint64_t started = 0;
        std::string nextCapture;
        std::string pendingCapture;
        int initialGold = 0;
        std::vector<int64_t> initialExperience;

        const auto state = [&]() -> EventRuntimeState &
        {
            require(game.m_pMapSceneRuntime != nullptr, "Current map runtime missing");
            EventRuntimeState *pState = game.m_pMapSceneRuntime->eventRuntimeState();
            require(pState != nullptr, "Current event state missing");
            return *pState;
        };
        const auto party = [&]() -> Party & { return game.m_pMapSceneRuntime->party(); };
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(output / (name + ".oysav"), "146受控MM8 " + name),
                "Cannot save milestone " + name);
        };
        const auto click = [&](EventDialogActionKind kind, uint32_t id, bool matchId = true)
        {
            const auto &actions = screen().activeEventDialog().actions;
            const auto found = std::find_if(actions.begin(), actions.end(), [&](const EventDialogAction &action)
            {
                return action.kind == kind && (!matchId || action.id == id) && action.enabled;
            });
            require(found != actions.end(), "Actual dialog action missing: " + std::to_string(id));
            screen().eventDialogSelectionIndex() = size_t(found - actions.begin());
            screen().executeActiveDialogAction();
        };
        const auto mapEvent = [&](uint16_t id)
        {
            screen().closeActiveEventDialog();
            IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
            require(pWorld != nullptr, "Current world runtime missing");
            size_t previous = 0;
            require(pWorld->executeMapEvent(id, previous), "Actual map event failed: " + std::to_string(id));
            screen().presentPendingEventDialog(previous, true);
        };
        const auto transition = [&](uint16_t id, const std::string &target)
        {
            mapEvent(id);
            require(state().pendingDialogueContext && state().pendingDialogueContext->transitionMapMove,
                "Source map event did not offer a transition");
            const auto move = *state().pendingDialogueContext->transitionMapMove;
            require(move.mapName == std::optional<std::string>(target), "Source target map changed");
            proof << "{\"map_event\":" << id << ",\"source\":"
                << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"target\":" << std::quoted(target)
                << ",\"x\":" << move.x << ",\"y\":" << move.y << ",\"z\":" << move.z << "}\n";
            proof.flush();
            click(EventDialogActionKind::MapTransitionConfirm, 0, false);
            require(game.processPendingMapMove(), "Application map transition did not commit");
            require(game.m_gameSession.currentMapFileName() == target, "Wrong actual arrival map");
        };
        const auto jump = [&](const std::string &target)
        {
            screen().closeActiveEventDialog();
            const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName(target);
            require(pMap != nullptr, "Declared fixture map missing");
            game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
            require(game.processPendingDebugMapJump(), "Declared fixture map jump failed");
            require(game.m_gameSession.currentMapFileName() == target, "Declared fixture arrived elsewhere");
        };
        const auto enterNpc = [&](uint32_t npc, uint32_t house)
        {
            mapEvent(201);
            require(state().pendingDialogueContext && state().pendingDialogueContext->hostHouseId == house,
                "Original room201 entered wrong house");
            const HouseEntry *pHouse = game.m_gameDataLoader.getHouseTable().get(house);
            require(pHouse != nullptr, "Original house missing");
            const auto residents = collectSelectableResidentNpcIds(*pHouse,
                game.m_gameDataLoader.getNpcDialogTable(), state());
            require(std::find(residents.begin(), residents.end(), npc) != residents.end(),
                "Expected source NPC is not a real house resident");
            if (screen().activeEventDialog().sourceId != npc || screen().activeEventDialog().isHouseDialog)
            {
                click(EventDialogActionKind::HouseResident, npc);
            }
            require(screen().activeEventDialog().sourceId == npc, "Resident action did not select the source NPC");
        };
        const auto topic = [&](uint16_t id) { click(EventDialogActionKind::NpcTopic, id); };
        const auto input = [&](uint16_t id, bool correct)
        {
            require(state().pendingInputPrompt.has_value(), "Escaton input prompt missing");
            const auto prompt = *state().pendingInputPrompt;
            const auto &answers = profile->inputStringAnswerBindings.at(id - 164).answers;
            require(prompt.eventId == id && prompt.answers == answers, "Exported current-language answers mismatch");
            const std::string answer = correct ? answers.back() : "definitely-wrong";
            game.m_pendingInputText = answer;
            game.finishPendingInputPrompt(true);
            require(!state().pendingInputPrompt, "Input prompt survived production submission");
            proof << "{\"input_event\":" << id << ",\"answer\":" << std::quoted(answer)
                << ",\"expected_match\":" << (correct ? "true" : "false")
                << ",\"Q97\":" << (party().hasQuestBit(97) ? "true" : "false")
                << ",\"has629\":" << (party().hasItemAnywhere(629) ? "true" : "false") << "}\n";
            proof.flush();
        };
        const auto released = [&](const Lord &lord)
        {
            require(party().hasQuestBit(lord.completed), "Lord completion flag missing");
            require(state().npcHouseOverrides.at(lord.npc) == 0, "Released lord was not moved away");
            require(state().npcTopicOverrides.at(lord.npc).at(0) == 0, "Release topic was not cleared");
            for (size_t member = 0; member < party().memberCount(); ++member)
            {
                require(party().hasAward(member, lord.award), "Lord award missing for a party member");
            }
        };
        const auto add = [&](const std::string &name, std::function<void()> action,
            bool screenshot = true, unsigned duration = 40)
        {
            steps.push_back({name, std::move(action), duration, screenshot});
        };

        if (crystal)
        {
            add("crystal-gate-locked", [&]()
            {
                mapEvent(502);
                require(!state().pendingMapMove && !state().pendingDialogueContext, "Unsolved crystal allowed exit");
            });
            add("crystal-demo-start", [&]()
            {
                mapEvent(152);
                require(state().mapVars[11] == 1 && state().mapVars[12] == 1, "Original crystal demo did not start");
            });
            for (unsigned tick = 0; tick < 10; ++tick)
            {
                add("crystal-demo-" + std::to_string(tick), [&, tick]()
                {
                    game.m_pMapSceneRuntime->advanceTurnBasedGameMinutes(0.5f);
                    require(state().mapVars[11] == (tick == 9 ? 0 : int(tick + 2)),
                        "Original crystal timer did not advance expected demonstration step");
                }, tick == 0 || tick == 9);
            }
            add("crystal-wrong-input", [&]()
            {
                mapEvent(155);
                require(state().mapVars[12] == 1, "Wrong crystal input did not reset the source sequence");
            });
            constexpr std::array<uint16_t, 9> sequence{153, 154, 155, 153, 155, 154, 153, 154, 153};
            for (size_t index = 0; index < sequence.size(); ++index)
            {
                add("crystal-input-" + std::to_string(index + 1), [&, index, id = sequence[index]]()
                {
                    mapEvent(id);
                    require(state().mapVars[12] == int(index + 2), "Original crystal input sequence did not advance");
                    if (index == 3) save("crystal-partial");
                }, index == 3 || index == 8);
            }
            add("pbp-arrived", [&]() { transition(502, "pbp.odm"); });
            add("escaton-map-arrived", [&]() { transition(501, "d35.blv"); });
            add("escaton-resident", [&]() { enterNpc(22, 775); });
            add("escaton-introduction", [&]() { topic(159); });
            add("escaton-questions", [&]() { topic(163); });
            add("escaton-wrong-prompt", [&]() { topic(164); });
            add("escaton-wrong-result", [&]()
            {
                input(164, false);
                require(!party().hasQuestBit(94) && !party().hasQuestBit(97) && !party().hasItemAnywhere(629),
                    "Wrong Escaton input granted success state");
            });
            for (uint16_t id : {164, 165, 166})
            {
                add("escaton-reopen-" + std::to_string(id), [&]() { topic(171); }, false);
                add("escaton-input-" + std::to_string(id), [&, id]() { topic(id); });
                add("escaton-result-" + std::to_string(id), [&, id]()
                {
                    input(id, true);
                    require(party().hasQuestBit(97) == (id == 166), "Third original answer completion order changed");
                    require(party().hasItemAnywhere(629) == (id == 166), "Original input key reward timing changed");
                });
            }
            add("escaton-complete-topics", [&]()
            {
                topic(171);
                for (const auto &action : screen().activeEventDialog().actions)
                {
                    require(action.kind != EventDialogActionKind::NpcTopic || action.id < 164 || action.id > 166,
                        "Completed Escaton still exposes a riddle reward path");
                }
                require(GameplayHeldItemController::tryDisplaceHeldInventoryItem(screen()),
                    "Earned key could not be placed into the ordinary party inventory");
                require(party().inventoryItemCount(629) == 1, "Earned key does not have exactly one inventory owner");
                screen().closeActiveEventDialog();
                save("escaton-key-earned");
            });
        }
        else if (lords)
        {
            add("pbp-from-escaton", [&]() { transition(501, "pbp.odm"); });
            for (size_t index = 0; index < Lords.size(); ++index)
            {
                const Lord lord = Lords[index];
                add("prison-" + std::to_string(lord.npc), [&, lord]() { transition(lord.gate, lord.pMap); });
                add("lord-resident-" + std::to_string(lord.npc), [&, lord]() { enterNpc(lord.npc, lord.house); });
                add("lord-released-" + std::to_string(lord.npc), [&, lord, index]()
                {
                    topic(lord.topic);
                    released(lord);
                    require(party().hasQuestBit(56) == (index == 3), "Final lord did not exclusively earn Q56");
                    require(!party().hasQuestBit(228), "Ending fired before returning to Ravenshore");
                    require(party().gold() == initialGold + int(index + 1) * 10000, "Lord gold reward changed");
                    for (size_t member = 0; member < party().memberCount(); ++member)
                    {
                        require(party().member(member)->experience == initialExperience.at(member)
                            + int64_t(index + 1) * 100000, "Lord party experience reward changed");
                    }
                    screen().closeActiveEventDialog();
                    if (index == 2) save("third-lord-earned");
                    if (index == 3) save("four-lords-earned");
                });
                add("lord-room-empty-" + std::to_string(lord.npc), [&, lord]()
                {
                    mapEvent(201);
                    const auto *pHouse = game.m_gameDataLoader.getHouseTable().get(lord.house);
                    require(pHouse != nullptr, "Lord house missing on revisit");
                    const auto residents = collectSelectableResidentNpcIds(*pHouse,
                        game.m_gameDataLoader.getNpcDialogTable(), state());
                    require(std::find(residents.begin(), residents.end(), lord.npc) == residents.end(),
                        "Released lord is naturally reachable again");
                });
                add("pbp-return-" + std::to_string(lord.npc), [&]() { transition(501, "pbp.odm"); }, false);
            }
            add("return-crystal", [&]() { transition(506, "d10.blv"); });
            add("wingame-video", [&]()
            {
                transition(501, "out02.odm");
                require(party().hasQuestBit(228), "Ravenshore OnLoad did not earn Q228");
                require(state().pendingMovie && state().pendingMovie->movieName == "wingame",
                    "Source ending video was not queued");
                require(!state().pendingReturnToMainMenu, "MM8 ending incorrectly requested the menu");
                require(game.processPendingEventMovie(), "Application did not start wingame");
                require(dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr,
                    "A real video screen did not open");
                movieStarted = true;
                movieFrame = frames;
            }, true, 100);
            add("continued-ravenshore", [&]()
            {
                require(movieClosed && game.m_screenManager.activeScreen() == nullptr,
                    "Ending did not return to ordinary gameplay");
                for (size_t member = 0; member < party().memberCount(); ++member)
                {
                    require(party().member(member)->experience == initialExperience.at(member) + 500000,
                        "Ravenshore ending party reward changed");
                }
                save("mm8-ending-complete");
                const auto loaded = loadGameDataFromPath(output / "mm8-ending-complete.oysav", error);
                require(loaded.has_value(), error);
                checkSavedCompletion(*loaded);
            });
        }
        else
        {
            add("cold-completed-ravenshore", [&]()
            {
                require(!state().pendingMovie && !state().pendingReturnToMainMenu, "Cold completion replayed ending");
            });
            add("cold-crystal-revisit", [&]() { jump("d10.blv"); });
            add("cold-ravenshore-return", [&]()
            {
                transition(501, "out02.odm");
                require(!state().pendingMovie && !state().pendingReturnToMainMenu, "Revisit replayed ending");
                require(party().gold() == initialGold, "Revisit duplicated gold");
                for (size_t member = 0; member < party().memberCount(); ++member)
                {
                    require(party().member(member)->experience == initialExperience.at(member),
                        "Revisit duplicated experience");
                }
            });
        }
        add("continued-journal", [&]()
        {
            screen().closeActiveEventDialog();
            GameplayInputFrame inputFrame;
            inputFrame.screenWidth = 1210;
            inputFrame.screenHeight = 834;
            inputFrame.actions[keyboardActionIndex(KeyboardAction::History)] = {true, true, false};
            game.m_gameSession.updateGameplay(inputFrame, 0.0f);
            game.m_gameSession.updateGameplay({}, 0.0f);
            require(screen().journalScreenState().active, "Ordinary history input did not open the journal");
        });

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual MM8 capture requires Metal");
                require(game.initializeRenderer(), "Renderer initialization failed");
                require(game.loadSessionFromPath(seed), "Read-only source save could not load");
                screen().closeActiveEventDialog();
                if (crystal)
                {
                    require(!party().hasQuestBit(97) && !party().hasQuestBit(56) && !party().hasQuestBit(228)
                        && !party().hasItemAnywhere(629), "Fresh Escaton fixture seed already has terminal progress");
                    jump("pbp.odm");
                    transition(506, "d10.blv"); // Use the original authored arrival floor/camera at the puzzle.
                    require(state().mapVars[12] == 0, "Fixture d10 was already visited or solved");
                }
                else if (lords)
                {
                    require(game.m_gameSession.currentMapFileName() == "d35.blv", "Expected earned Escaton save");
                    require(party().hasQuestBit(97) && party().hasItemAnywhere(629), "Escaton key was not earned");
                    require(!party().hasQuestBit(56) && !party().hasQuestBit(228), "Seed already finished MM8");
                    for (const Lord &lord : Lords)
                    {
                        require(!party().hasQuestBit(lord.completed), "Lords mode requires no previously released lord");
                    }
                }
                else
                {
                    const auto loaded = loadGameDataFromPath(seed, error);
                    require(loaded.has_value(), error);
                    checkSavedCompletion(*loaded);
                }
                initialGold = party().gold();
                for (size_t member = 0; member < party().memberCount(); ++member)
                {
                    initialExperience.push_back(party().member(member)->experience);
                }
                save("fixture");
                started = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                    && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                {
                    game.handleSdlEvent(event);
                }
            },
            [&](int width, int height, float wheel, float delta)
            {
                require(SDL_GetTicks() - started < 180000, "Bounded MM8 capture timeout");
                ++frames;
                if (frames >= nextStep && stepIndex < steps.size() && pendingCapture.empty() && nextCapture.empty())
                {
                    const Step &step = steps[stepIndex++];
                    step.action();
                    proof << "{\"stage\":" << std::quoted(step.name) << ",\"frame\":" << frames
                        << ",\"map\":" << std::quoted(game.m_gameSession.currentMapFileName())
                        << ",\"Q56\":" << (party().hasQuestBit(56) ? "true" : "false")
                        << ",\"Q228\":" << (party().hasQuestBit(228) ? "true" : "false") << "}\n";
                    proof.flush();
                    nextStep = frames + step.duration;
                    if (step.screenshot)
                    {
                        nextCapture = step.name;
                        captureAt = frames + 20;
                    }
                }
                game.renderFrame(width, height, wheel, movieStarted && !movieClosed ? delta : 0.0f);
                if (!nextCapture.empty() && frames >= captureAt)
                {
                    require(pendingCapture.empty(), "Screenshot requests overlap");
                    pendingCapture = nextCapture;
                    nextCapture.clear();
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pendingCapture.c_str());
                }
                if (movieStarted && !movieClosed && frames >= movieFrame + 65)
                {
                    auto *pMovie = dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen());
                    require(pMovie != nullptr, "Ending video closed before bounded visual check");
                    SDL_Event escape{};
                    escape.type = SDL_EVENT_KEY_DOWN;
                    escape.key.key = SDLK_ESCAPE;
                    pMovie->handleSdlEvent(escape);
                    game.handleCompletedEventMovieScreen();
                    require(game.m_screenManager.activeScreen() == nullptr, "Video did not resume gameplay");
                    movieClosed = true;
                }
                if (!pendingCapture.empty())
                {
                    if (auto capture = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                            SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                        require(pSurface != nullptr, "Capture surface failed");
                        const bool written = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(written, "Cannot write capture");
                        pendingCapture.clear();
                        ++captures;
                    }
                }
                if (stepIndex == steps.size() && frames >= nextStep && nextCapture.empty() && pendingCapture.empty())
                {
                    GameplayInputFrame close;
                    close.screenWidth = width;
                    close.screenHeight = height;
                    close.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                    close.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                    game.m_gameSession.updateGameplay(close, 0.0f);
                    game.m_gameSession.updateGameplay({}, 0.0f);
                    require(!screen().journalScreenState().active, "Journal close did not resume gameplay");
                    save("final");
                    const auto loaded = loadGameDataFromPath(output / "final.oysav", error);
                    require(loaded.has_value(), error);
                    if (!crystal) checkSavedCompletion(*loaded);
                    require(captures == size_t(std::count_if(steps.begin(), steps.end(),
                        [](const Step &step) { return step.screenshot; })), "Capture set incomplete");
                    complete = true;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]() { game.shutdownApplication(); });
        require(application.run() == 0 && complete, "Actual MM8 endgame capture did not complete");
        std::ofstream(output / "result.json")
            << "{\"status\":\"passed\",\"mode\":" << std::quoted(mode)
            << ",\"controlled\":true,\"natural_campaign\":false,\"frames\":" << frames
            << ",\"captures\":" << captures << ",\"movie_started\":" << (movieStarted ? "true" : "false")
            << ",\"movie_skipped_after_render\":" << (movieClosed ? "true" : "false")
            << ",\"independent_completion_cold_process\":" << (cold ? "true" : "false")
            << ",\"full_movie_completion\":false,\"physical_door_navigation\":false}\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 6)
        {
            throw std::runtime_error("mm8_endgame_capture RUNTIME ORIGINAL_SOURCE READONLY_SEED NEW_OUTPUT "
                "crystal-escaton|lords-ending|reload-ending");
        }
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4], argv[5]);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
