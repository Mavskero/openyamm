#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
void require(bool value, const std::string &text)
{
    if (!value) throw std::runtime_error(text);
}
}

struct GameApplicationTestAccess
{
    static int introduction(std::filesystem::path root, std::filesystem::path seed, std::filesystem::path output)
    {
        root = std::filesystem::canonical(root);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Expected a new introduction directory");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.assetRoot = root.string();
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM finite introduction141";
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = "oute3.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        uint64_t start = 0;
        unsigned frames = 0, captures = 0;
        std::string pending;
        bool complete = false;
        std::ofstream(output / "fixture.json") << R"json({"controlled":true,"seed":"accepted130 actual004 invitation-natural-greeting read-only; existing XP50001/item772 fixture","introduction_stat_changes":[],"dialogue_entry":"explicit pending NPC803 once through real scene Enter; original four GUI topic actions","later_letter_fixture":"after saving completed introduction, clear Q1104/Q1105 in new fixture save; original130 already had Q1104 and correctly did not grant another letter","later":"real warm AfterLoad source letter, then Hints root GUI click","generation_pickup":"accepted130 plus141 deterministic invitation tests; navigation attempt141 retained failed","natural_campaign":false,"audio":"dummy"})json" << '\n';
        const auto flag = [&](const char *pName)
        {
            const auto &vars = game.m_pMapSceneRuntime->eventRuntimeState()->namedGlobalVars;
            const auto found = vars.find(std::string("MMerge.CrossContinents.") + pName);
            return found != vars.end() && found->second != 0;
        };
        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                require(game.initializeRenderer() && game.loadSessionFromPath(seed), "Read-only seed load failed");
                require(!flag("MetVerdant") && !flag("GotMainQuest"), "Seed already introduced");
                require(game.m_pMapSceneRuntime->party().inventoryItemCount(772) == 1, "Original invitation absent");
                require(game.quickSaveToPath(output / "fixture.oysav", "141旧受控邀请入口"), "Fixture save failed");
                EventRuntimeState::PendingDialogueContext talk;
                talk.kind = DialogueContextKind::NpcTalk;
                talk.sourceId = 803;
                talk.npcEnterPending = true;
                game.m_pMapSceneRuntime->eventRuntimeState()->pendingDialogueContext = talk;
                start = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                    && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                    game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                require(SDL_GetTicks() - start < 45000, "Finite introduction guard exceeded");
                ++frames;
                auto &screen = game.m_gameSession.gameplayScreenRuntime();
                const auto click = [&](uint32_t id)
                {
                    const auto &actions = screen.activeEventDialog().actions;
                    for (size_t index = 0; index < actions.size(); ++index)
                    {
                        if (actions[index].kind == EventDialogActionKind::NpcTopic && actions[index].id == id)
                        {
                            screen.eventDialogSelectionIndex() = index;
                            screen.executeActiveDialogAction();
                            return;
                        }
                    }
                    throw std::runtime_error("Missing introduction topic " + std::to_string(id));
                };
                if (frames == 40) click(1778);
                if (frames == 90) click(1779);
                if (frames == 140) click(1779);
                if (frames == 190) click(1780);
                if (frames == 235)
                {
                    require(flag("MetVerdant") && flag("GotIntro1") && flag("GotIntro2")
                        && flag("GotMainQuest"), "Four original introduction flags absent");
                    require(game.m_pMapSceneRuntime->eventRuntimeState()->namedGlobalVars.at(
                        "MMerge.CrossContinents.StartedAt") == 3, "Physical StartedAt missing");
                    screen.closeActiveEventDialog();
                    require(game.quickSaveToPath(output / "introduction.oysav", "141四步介绍"), "Intro save failed");
                    auto *state = game.m_pMapSceneRuntime->eventRuntimeState();
                    Party &party = game.m_pMapSceneRuntime->party();
                    require(party.inventoryItemCount(2125) == 0, "Letter fixture seed contains2125");
                    EventRuntime::setQuestBitValue(*state, party, 1104, false);
                    EventRuntime::setQuestBitValue(*state, party, 1105, false);
                    require(game.quickSaveToPath(output / "letter-fixture.oysav", "141显式无信前置"),
                        "Letter fixture save failed");
                    require(game.quickLoadFromPath(output / "letter-fixture.oysav", true), "Real AfterLoad failed");
                }
                if (frames == 285)
                {
                    const Party &party = game.m_pMapSceneRuntime->party();
                    require(party.hasQuestBit(1104) && party.hasQuestBit(1105)
                        && party.inventoryItemCount(2125) == 1, "New source AfterLoad letter absent");
                    require(game.quickSaveToPath(output / "letter.oysav", "141真实续接信件"), "Letter save failed");
                    click(732);
                }
                if (frames == 350)
                {
                    screen.handleDialogueCloseRequest();
                    require(screen.activeEventDialog().isActive, "Hints close did not return root");
                    screen.closeActiveEventDialog();
                }
                game.renderFrame(width, height, wheel, delta);
                std::string shot;
                if (frames == 65) shot = "intro-first";
                if (frames == 220) shot = "intro-fourth";
                if (frames == 275) shot = "source-letter";
                if (frames == 325) shot = "hints-secondary";
                if (!shot.empty())
                {
                    require(pending.empty(), "Capture overlap");
                    pending = shot;
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pending.c_str());
                }
                if (!pending.empty())
                {
                    if (auto capture = Engine::BgfxContext::consumeScreenshot(pending))
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                            SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                        require(pSurface != nullptr, "Capture surface failed");
                        const bool saved = SDL_SaveBMP(pSurface, (output / (pending + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(saved, "Capture save failed");
                        pending.clear();
                        ++captures;
                    }
                }
                if (frames >= 375 && captures == 4 && !complete)
                {
                    require(game.quickSaveToPath(output / "final.oysav", "141介绍信件提示"), "Final save failed");
                    require(game.quickLoadFromPath(output / "final.oysav", true), "Warm load failed");
                    require(game.m_pMapSceneRuntime->party().inventoryItemCount(2125) == 1, "Letter repeated");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "141暖恢复"), "Warm save failed");
                    complete = true;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            }, [&]() { game.shutdownApplication(); });
        require(app.run() == 0 && complete, "Introduction incomplete");
        std::ofstream(output / "result.json") << "{\"status\":\"passed\",\"frames\":" << frames
            << ",\"captures\":4,\"controlled\":true,\"warm_reload\":true}\n";
        return 0;
    }

    static int run(std::filesystem::path root, std::filesystem::path seed, std::filesystem::path output,
        bool dialogue = false, bool reentry = false)
    {
        root = std::filesystem::canonical(root);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Expected a new capture directory");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.assetRoot = root.string();
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Verdant encounter check";
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = "outc2.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        uint64_t start = 0;
        unsigned frames = 0;
        unsigned captures = 0;
        std::string pending;
        bool complete = false;
        if (reentry)
            std::ofstream(output / "fixture.json") << R"json({"controlled":true,"seed":"138 hints.oysav read-only in independent process","changes":[],"talk":"pending NPC803 presented through real scene Enter hook; close then new talk","scope":"cold branch reset and actual reentry","natural_campaign":false})json" << '\n';
        else if (dialogue)
        {
            std::ofstream(output / "fixture.json") << R"json({"controlled":true,"seed":"137controlled letter final read-only","changes":["GotMainQuest=1","StartedAt=3"],"arrival":"debug outc2","talk":"controlled pending SpeakNPC803 through actual presentation Enter hook","actions":"real GameplayScreenRuntime topic/close actions","scope":"Hints secondary greeting, TTGuide Chinese long text, outdoor113 return and warm branch reset","natural_campaign":false})json" << '\n';
        }
        else
        std::ofstream(output / "fixture.json") << R"json({"controlled":true,"seed":"normal132 MM6 read-only","changes":["MetVerdant=1","VerdantAt=0","Q1104/Q1105 absent","no item2125"],"arrival":"explicit debug map jump to outc2 authored entry","scope":"Real committed AfterLoad letter/dialogue/location/save/warm reload. Not natural cross-world arrival or campaign completion.","invoked_source_event_directly":false,"immortal":false,"unlimited_mana":false,"audio":"dummy; no subjective acceptance"})json" << '\n';
        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                require(game.initializeRenderer(), "Renderer initialization failed");
                require(game.loadSessionFromPath(seed), "Read-only normal132 seed load failed");
                auto *scene = game.m_pMapSceneRuntime.get();
                require(scene != nullptr, "Missing seed scene");
                auto *state = scene->eventRuntimeState();
                require(state != nullptr, "Missing seed event state");
                if (!dialogue)
                {
                scene->party().setQuestBit(1104, false);
                scene->party().setQuestBit(1105, false);
                EventRuntime::setQuestBitValue(*state, scene->party(), 1104, false);
                EventRuntime::setQuestBitValue(*state, scene->party(), 1105, false);
                while (scene->party().hasItemAnywhere(2125))
                    require(scene->party().removeItem(2125), "Fixture item removal failed");
                }
                if (!reentry)
                for (const auto &[name, value] : std::vector<std::pair<std::string, int32_t>>{
                    {"MMerge.CrossContinents.MetVerdant", 1}, {"MMerge.CrossContinents.VerdantAt", 0}})
                {
                    state->namedGlobalVars[name] = value;
                    game.m_gameSession.setNamedGlobalVar(name, value);
                }
                if (dialogue && !reentry)
                {
                    for (const auto &[name, value] : std::vector<std::pair<std::string, int32_t>>{
                        {"MMerge.CrossContinents.GotMainQuest", 1}, {"MMerge.CrossContinents.StartedAt", 3}})
                    {
                        state->namedGlobalVars[name] = value;
                        game.m_gameSession.setNamedGlobalVar(name, value);
                    }
                }
                require(game.quickSaveToPath(output / "fixture.oysav", "137受控前置"), "Fixture save failed");
                if (!reentry)
                {
                const auto *map = game.m_gameDataLoader.getMapStats().findByFileName("outc2.odm");
                require(map != nullptr, "Missing authored map");
                game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = map->id};
                require(game.processPendingDebugMapJump(), "Controlled map jump failed");
                scene = game.m_pMapSceneRuntime.get();
                }
                require(scene->party().hasQuestBit(1104) && scene->party().hasQuestBit(1105), "Letter QBits not committed");
                require(scene->party().inventoryItemCount(2125, 0) == 1, "Letter not granted once to player0");
                require(scene->party().snapshot().npcHouseOverrides.at(803) == 1195, "Enroth location missing");
                if (!dialogue)
                    require(scene->party().snapshot().npcGreetingOverrides.at(803) == 330, "Letter greeting missing");
                if (dialogue)
                {
                    EventRuntimeState::PendingDialogueContext talk;
                    talk.kind = DialogueContextKind::NpcTalk;
                    talk.sourceId = 803;
                    talk.npcEnterPending = true;
                    scene->eventRuntimeState()->pendingDialogueContext = talk;
                }
                start = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                    && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                    game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                require(SDL_GetTicks() - start < 30000, "Finite capture guard exceeded");
                ++frames;
                auto &screen = game.m_gameSession.gameplayScreenRuntime();
                const auto clickTopic = [&](uint32_t id)
                {
                    const auto &actions = screen.activeEventDialog().actions;
                    for (size_t index = 0; index < actions.size(); ++index)
                    {
                        if (actions[index].kind == EventDialogActionKind::NpcTopic && actions[index].id == id)
                        {
                            screen.eventDialogSelectionIndex() = index;
                            screen.executeActiveDialogAction();
                            return;
                        }
                    }
                    throw std::runtime_error("Missing actual UI topic " + std::to_string(id));
                };
                if (dialogue && !reentry && frames == 50) clickTopic(732);
                if (dialogue && !reentry && frames == 110) clickTopic(1781);
                if (dialogue && !reentry && frames == 170)
                {
                    require(game.quickSaveToPath(output / "hints.oysav", "138提示分支"), "Hints save failed");
                    screen.handleDialogueCloseRequest();
                    require(screen.activeEventDialog().isActive, "113 closed outdoor NPC instead of returning");
                    require(game.m_pMapSceneRuntime->eventRuntimeState()->dialogueState.scriptedBranch.empty(),
                        "113 did not return to root");
                }
                if (reentry && frames == 85)
                {
                    EventRuntimeState::PendingDialogueContext talk;
                    talk.kind = DialogueContextKind::NpcTalk;
                    talk.sourceId = 803;
                    talk.npcEnterPending = true;
                    game.m_pMapSceneRuntime->eventRuntimeState()->pendingDialogueContext = talk;
                    screen.ensurePendingEventDialogPresented();
                    require(screen.activeEventDialog().isActive && screen.activeEventDialog().sourceId == 803,
                        "Reentry did not open actual NPC803");
                    require(game.m_pMapSceneRuntime->eventRuntimeState()->dialogueState.scriptedBranch.empty(),
                        "Reentry retained Hints");
                    const auto &topics = game.m_pMapSceneRuntime->eventRuntimeState()->npcTopicOverrides.at(803);
                    require(topics.at(0) == 1784 && topics.at(1) == 732, "Reentry did not rebuild root topics");
                }
                if (frames == (dialogue && !reentry ? 230u : 70u))
                {
                    auto &screen = game.m_gameSession.gameplayScreenRuntime();
                    require(screen.activeEventDialog().isActive, "Source NPC dialogue was not presented");
                    require(screen.activeEventDialog().sourceId == 803, "Wrong NPC dialogue");
                    screen.closeActiveEventDialog();
                }
                game.renderFrame(width, height, wheel, delta);
                if (frames == 30 || frames == 120 || (dialogue && !reentry && (frames == 80 || frames == 200)))
                {
                    require(pending.empty(), "Screenshot still pending");
                    pending = reentry ? (frames == 30 ? "cold-root" : "reentered-root")
                        : dialogue ? (frames == 30 ? "root" : frames == 80 ? "hints-secondary"
                        : frames == 120 ? "ttguide-long" : "returned-root")
                        : frames == 30 ? "letter-dialogue" : "enroth-location";
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pending.c_str());
                }
                if (!pending.empty())
                {
                    auto capture = Engine::BgfxContext::consumeScreenshot(pending);
                    if (capture)
                    {
                        auto *surface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                            SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                        require(surface != nullptr, "Capture surface failed");
                        const bool saved = SDL_SaveBMP(surface, (output / (pending + ".bmp")).string().c_str());
                        SDL_DestroySurface(surface);
                        require(saved, "Capture save failed");
                        pending.clear();
                        ++captures;
                    }
                }
                if (frames >= (dialogue && !reentry ? 250u : 150u)
                    && captures == (dialogue && !reentry ? 4u : 2u) && !complete)
                {
                    require(game.quickSaveToPath(output / "final.oysav", "137信件与位置"), "Final save failed");
                    require(game.quickLoadFromPath(output / "final.oysav", true), "Warm reload failed");
                    auto &party = game.m_pMapSceneRuntime->party();
                    require(party.inventoryItemCount(2125) == 1, "Reload duplicated or lost letter");
                    require(party.hasQuestBit(1104) && party.hasQuestBit(1105), "Reload lost QBits");
                    require(party.snapshot().npcHouseOverrides.at(803) == 1195, "Reload lost latest NPC home");
                    require(!game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive,
                        "Reload repeated letter dialogue");
                    require(game.m_pMapSceneRuntime->eventRuntimeState()->dialogueState.scriptedBranch.empty(),
                        "Reload preserved transient Hints branch");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "137信件重载"), "Reload save failed");
                    complete = true;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            }, [&]() { game.shutdownApplication(); });
        require(app.run() == 0 && complete, "Capture did not complete");
        std::ofstream(output / "result.json") << "{\"status\":\"passed\",\"controlled\":true,\"renderer\":\"Metal\",\"frames\":"
            << frames << ",\"captures\":" << captures << ",\"map\":\"outc2.odm\",\"warm_reload\":true,\"reentry\":"
            << (reentry ? "true" : "false") << "}\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 4 && argc != 5)
            throw std::runtime_error("Usage: merge_quest_capture RUNTIME READ_ONLY_SEED NEW_OUTPUT [dialogue|reentry|introduction]");
        if (argc == 5 && std::string(argv[4]) == "introduction")
            return OpenYAMM::Game::GameApplicationTestAccess::introduction(argv[1], argv[2], argv[3]);
        const bool reentry = argc == 5 && std::string(argv[4]) == "reentry";
        if (argc == 5 && std::string(argv[4]) != "dialogue" && !reentry)
            throw std::runtime_error("Unknown capture mode");
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argc == 5, reentry);
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
