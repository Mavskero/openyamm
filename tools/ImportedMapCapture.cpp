#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
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
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path characterSeed,
        const std::string &map, std::filesystem::path output, bool reload)
    {
        require(map == "6d09.blv" || map == "znwc.blv", "Expected a newly supported MM6 map");
        root = std::filesystem::canonical(root);
        characterSeed = std::filesystem::canonical(characterSeed);
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        std::string error;
        const std::optional<GameSaveData> seed = loadGameDataFromPath(characterSeed, error);
        require(seed.has_value() && seed->party.members.size() == 4, "Expected the normal four-member MM6 seed");
        require(!reload || seed->mapFileName == map, "Reload input belongs to a different map");
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
        config.appName = "OpenYAMM imported map validation";
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = map;
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
        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"map\":\"" << map << "\","
            << "\"scope\":\"" << (reload ? "Independent process reload of the controlled map result."
                : "Normal127 loaded, then explicit debug map jump to the original entry and direct original-event "
                  "invocation. Not natural arrival, campaign progression or physical interaction.") << "\"}\n";
        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal required");
                require(game.initializeRenderer(), "Renderer initialization failed");
                require(game.loadSessionFromPath(characterSeed), "Normal source save load failed");
                if (!reload)
                {
                    const MapStatsEntry *pMap = game.m_gameDataLoader.getMapStats().findByFileName(map);
                    require(pMap != nullptr, "Imported map missing from MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Controlled map jump failed");
                }
                require(game.m_gameSession.currentMapFileName() == map, "Startup fell back to a different map");
                require(game.quickSaveToPath(output / "entry.oysav", "受控地图入口"), "Entry save failed");
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
                require(SDL_GetTicks() - start < 30000, "Imported map capture finite guard exceeded");
                ++frames;
                if (frames == 90 && !reload)
                {
                    IMapSceneRuntime *pScene = game.m_pMapSceneRuntime.get();
                    require(pScene != nullptr && pScene->kind() == SceneKind::Indoor, "Expected indoor runtime");
                    EventRuntimeState *pState = pScene->eventRuntimeState();
                    require(pState != nullptr, "Missing map event state");
                    EventRuntime events(
                        &game.m_gameDataLoader.getHouseTable(), &game.m_gameDataLoader.getNpcDialogTable());
                    for (uint16_t id : map == "6d09.blv" ? std::vector<uint16_t>{21, 39}
                                                        : std::vector<uint16_t>{71})
                    {
                        require(events.executeEventById(pScene->localEventProgram(), pScene->globalEventProgram(),
                            id, *pState, &pScene->party(), pScene->sceneEventContext()), "Original event failed");
                    }
                    if (map == "6d09.blv")
                    {
                        require(pState->mechanisms.contains(21), "Original event did not address door21");
                        const uint16_t state = pState->mechanisms.at(21).state;
                        require(state == uint16_t(EvtMechanismState::Closing)
                            || state == uint16_t(EvtMechanismState::Closed), "Door21 did not close");
                    }
                    require(game.quickSaveToPath(output / "events.oysav", "受控原始事件"), "Event save failed");
                }
                game.renderFrame(width, height, wheel, delta);
                if (frames == 30 || frames == 180)
                {
                    require(pending.empty(), "Previous screenshot is still pending");
                    pending = frames == 30 ? "entry" : "events";
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pending.c_str());
                }
                if (!pending.empty())
                {
                    auto capture = Engine::BgfxContext::consumeScreenshot(pending);
                    if (capture)
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                            SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                        require(pSurface != nullptr, "Screenshot surface failed");
                        const bool saved = SDL_SaveBMP(pSurface, (output / (pending + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(saved, "Screenshot write failed");
                        pending.clear();
                        ++captures;
                    }
                }
                if (frames >= 180 && captures == 2 && !complete)
                {
                    require(game.quickSaveToPath(output / "final.oysav", "受控地图结果"), "Final save failed");
                    require(game.quickLoadFromPath(output / "final.oysav", true), "Warm map reload failed");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "受控地图重载"), "Reload save failed");
                    require(game.m_gameSession.currentMapFileName() == map, "Reload selected a different map");
                    complete = true;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]() { game.shutdownApplication(); });
        require(application.run() == 0 && complete, "Actual map capture did not complete");
        std::ofstream(output / "result.json") << "{\"status\":\"passed\",\"controlled\":true,\"map\":\""
            << map << "\",\"reload\":" << (reload ? "true" : "false")
            << ",\"renderer\":\"Metal\",\"frames\":" << frames << ",\"captures\":" << captures << "}\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        const bool reload = argc == 6 && std::string(argv[5]) == "--reload";
        if (argc != 5 && !reload)
            throw std::runtime_error("Usage: imported_map_capture RUNTIME INPUT_SAVE MAP NEW_OUTPUT [--reload]");
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4], reload);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
