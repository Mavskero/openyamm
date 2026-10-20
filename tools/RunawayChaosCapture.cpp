#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *Prefix = "MMerge.CrossContinents.";

void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

int32_t flag(const std::unordered_map<std::string, int32_t> &vars, const std::string &name)
{
    const auto found = vars.find(std::string(Prefix) + name);
    return found == vars.end() ? 0 : found->second;
}

void checkSavedState(const GameSaveData &save, int expectedCount)
{
    require(flag(save.namedGlobalVars, "RiddlesAnswered") == expectedCount, "Saved answer count changed");
    int count = 0;
    for (int id = 1; id <= 11; ++id)
    {
        count += flag(save.namedGlobalVars, "RiddleSolved." + std::to_string(id)) != 0 ? 1 : 0;
    }
    require(count == expectedCount, "Saved solved set changed");
    require(flag(save.namedGlobalVars, "PendingRiddle") == 0, "Saved stale riddle continuation");
    require(flag(save.namedGlobalVars, "CoughtChaos") == 0, "Unexpected capture");
    require(flag(save.namedGlobalVars, "QuestFinished") == 0, "Unexpected campaign completion");
    require(std::none_of(save.party.hiredNpcFollowers.begin(), save.party.hiredNpcFollowers.end(),
        [](const HiredNpcFollower &follower) { return follower.npcId == 1092; }), "Unexpected Chaos follower");
}
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path seed,
        std::filesystem::path output, const std::string &mode)
    {
        const bool cold = mode == "reload";
        require(cold || mode == "dialogue", "Expected dialogue or reload mode");
        root = std::filesystem::canonical(root);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Output must be new");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);

        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.assetRoot = root.string();
        settings.windowMode = WindowMode::Windowed;
        std::string error;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Chaos142 controlled dialogue";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out01.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        uint64_t start = 0;
        unsigned frames = 0;
        unsigned captures = 0;
        int correctRiddle = 0;
        bool complete = false;
        bool actualAnswerHasAsciiLetter = false;
        std::string pendingCapture;
        std::ofstream proof(output / "input-proof.txt");

        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"mode\":" << std::quoted(mode)
            << ",\"seed\":" << std::quoted(seed.string())
            << ",\"changes\":" << std::quoted(cold
                ? "No quest mutations; open NPC1092 with declared house712 context on existing ordinary map"
                : "GotFQHint2/3 and GotFQHints3; clear riddle/capture state; declare NPC1092 house712 context. "
                  "No map jump, character stat changes or answered success fixture")
            << ",\"renderer\":\"Metal\",\"audio\":\"dummy\",\"natural_campaign\":false,"
                "\"limitations\":[\"Original Chinese riddle answers are numeric; actual case distinction unavailable, "
                "covered by declared ASCII headless fixture through production input matcher\","
                "\"Original one-second delayed result presentation remains unimplemented\","
                "\"Split party, dynamic Merge maps and QuestFinished are not exercised\"]}\n";

        const auto state = [&]() { return game.m_pMapSceneRuntime->eventRuntimeState(); };
        const auto get = [&](const std::string &name) { return flag(state()->namedGlobalVars, name); };
        const auto set = [&](const std::string &name, int value)
        {
            state()->namedGlobalVars[std::string(Prefix) + name] = value;
            game.m_gameSession.setNamedGlobalVar(std::string(Prefix) + name, value);
        };
        const auto openTalk = [&]()
        {
            game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
            EventRuntimeState::PendingDialogueContext context;
            context.kind = DialogueContextKind::NpcTalk;
            context.sourceId = 1092;
            context.hostHouseId = 712;
            context.npcEnterPending = true;
            state()->pendingDialogueContext = context;
        };
        const auto click = [&](uint32_t topic)
        {
            auto &screen = game.m_gameSession.gameplayScreenRuntime();
            const auto &actions = screen.activeEventDialog().actions;
            for (size_t index = 0; index < actions.size(); ++index)
            {
                if (actions[index].kind == EventDialogActionKind::NpcTopic && actions[index].id == topic)
                {
                    screen.eventDialogSelectionIndex() = index;
                    screen.executeActiveDialogAction();
                    return;
                }
            }
            throw std::runtime_error("Actual NPC topic missing: " + std::to_string(topic));
        };
        const auto answer = [&](int kind)
        {
            require(state()->pendingInputPrompt.has_value(), "Real input prompt missing");
            const auto prompt = *state()->pendingInputPrompt;
            require(prompt.eventId == 1790 && !prompt.trimWhitespace, "Original input policy missing");
            require(prompt.answers.size() == 1, "Expected one original riddle answer");
            const int id = get("PendingRiddle");
            const auto source = game.m_gameDataLoader.getNpcDialogTable().getText(2217 + id * 2);
            require(source.has_value() && *source == prompt.answers.front(), "Question answer differs from source");
            std::string submitted = kind == 0 ? "definitely not the source answer"
                : kind == 1 ? " " + *source + " " : *source;
            if (kind == 2)
            {
                std::transform(submitted.begin(), submitted.end(), submitted.begin(), [&](unsigned char value)
                {
                    actualAnswerHasAsciiLetter |= value < 128 && std::isalpha(value) != 0;
                    return value < 128 ? char(std::toupper(value)) : char(value);
                });
                correctRiddle = id;
            }
            game.m_pendingInputText = submitted;
            game.finishPendingInputPrompt(true);
            require(!state()->pendingInputPrompt.has_value(), "Input prompt survived submission");
            require(get("RiddlesAnswered") == (kind == 2 ? 1 : 0), "Real continuation gave wrong count");
            require(get("PendingRiddle") == 0, "Pending riddle was not consumed");
            require(state()->npcHouseOverrides.at(1092) == 0, "Chaos did not leave house after answering");
            require(!state()->pendingDialogueContext
                || state()->pendingDialogueContext->kind != DialogueContextKind::NpcTalk,
                "Original house NPC dialogue remains active");
            const std::string expected = game.m_gameDataLoader.getNpcDialogTable()
                .getText(kind == 2 ? 2264 : 2265).value();
            const bool messagePresent = std::find(state()->messages.begin(), state()->messages.end(), expected)
                != state()->messages.end();
            require(messagePresent, "Chinese answer result disappeared on dialogue close");
            proof << "kind=" << kind << " riddle=" << id << " input=" << std::quoted(submitted)
                << " expected=" << std::quoted(*source) << " count=" << get("RiddlesAnswered")
                << " result=" << std::quoted(expected) << '\n';
            proof.flush();
        };

        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                require(game.initializeRenderer(), "Renderer initialization failed");
                require(game.loadSessionFromPath(seed), "Read-only seed load failed");
                game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                if (!cold)
                {
                    require(get("QuestFinished") == 0, "Seed has already completed Merge");
                    set("GotFQHint2", 1);
                    set("GotFQHint3", 1);
                    set("GotFQHints", 3);
                    set("RiddlesAnswered", 0);
                    set("PendingRiddle", 0);
                    set("CoughtChaos", 0);
                    for (int id = 1; id <= 11; ++id) set("RiddleSolved." + std::to_string(id), 0);
                    require(game.quickSaveToPath(output / "fixture.oysav", "142声明混沌对话前置"), "Fixture save failed");
                    require(game.quickLoadFromPath(output / "fixture.oysav", true), "Fixture reload failed");
                }
                else
                {
                    require(game.quickSaveToPath(output / "fixture.oysav", "142独立冷载"), "Cold save failed");
                    const auto saved = loadGameDataFromPath(output / "fixture.oysav", error);
                    require(saved.has_value(), error);
                    checkSavedState(*saved, 1);
                }
                openTalk();
                start = SDL_GetTicks();
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
                require(SDL_GetTicks() - start < 45000, "Bounded Chaos Mac capture timeout");
                ++frames;
                if (!cold)
                {
                    if (frames == 30 || frames == 160 || frames == 290) click(1790);
                    if (frames == 70) answer(0);
                    if (frames == 130 || frames == 260) openTalk();
                    if (frames == 200) answer(1);
                    if (frames == 330) answer(2);
                }
                game.renderFrame(width, height, wheel, delta);
                std::string shot;
                if (cold && frames == 40) shot = "cold-chaos-topics";
                if (!cold && frames == 50) shot = "source-riddle-input";
                if (!cold && frames == 100) shot = "wrong-answer-result";
                if (!cold && frames == 230) shot = "space-answer-rejected";
                if (!cold && frames == 360) shot = "correct-answer-result";
                if (!shot.empty())
                {
                    require(pendingCapture.empty(), "Capture overlap");
                    pendingCapture = shot;
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, shot.c_str());
                }
                if (!pendingCapture.empty())
                {
                    if (auto capture = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                            SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                        require(pSurface != nullptr, "Capture surface failed");
                        const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(saved, "Capture BMP save failed");
                        pendingCapture.clear();
                        ++captures;
                    }
                }
                if (!complete && frames >= (cold ? 85u : 390u) && captures == (cold ? 1u : 4u))
                {
                    game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                    require(game.quickSaveToPath(output / "final.oysav", "142混沌局部对话结果"), "Final save failed");
                    const auto saved = loadGameDataFromPath(output / "final.oysav", error);
                    require(saved.has_value(), error);
                    checkSavedState(*saved, 1);
                    if (!cold)
                    {
                        require(flag(saved->namedGlobalVars, "RiddleSolved." + std::to_string(correctRiddle)) == 1,
                            "Actual correct riddle not persisted");
                    }
                    complete = true;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]() { game.shutdownApplication(); });
        require(app.run() == 0 && complete, "Actual Chaos capture incomplete");
        std::ofstream(output / "result.json") << "{\"status\":\"passed\",\"mode\":" << std::quoted(mode)
            << ",\"frames\":" << frames << ",\"captures\":" << captures
            << ",\"answers\":1,\"actual_answer_had_ascii_letter\":" << (actualAnswerHasAsciiLetter ? "true" : "false")
            << ",\"QuestFinished\":false,\"controlled\":true,\"natural_campaign\":false,"
                "\"original_delayed_message_timing_validated\":false}\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 5)
        {
            throw std::runtime_error("runaway_chaos_capture RUNTIME READONLY_SEED NEW_OUTPUT dialogue|reload");
        }
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
