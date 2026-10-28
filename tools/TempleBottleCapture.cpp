#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/events/ISceneEventContext.h"
#include "game/maps/SaveGame.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *ReturnKey = "TempleInABottleReturn";
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}
}
struct GameApplicationTestAccess
{
    static int run(std::filesystem::path runtime, std::filesystem::path input,
        std::filesystem::path output, bool returning)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/"),
            "Use a new NVMe evidence directory");
        std::string error;
        const auto seed = loadGameDataFromPath(input, error);
        require(seed.has_value(), "Cannot decode input: " + error);
        require(returning ? seed->mapFileName == "7nwc.blv" && seed->sessionSavedLocations.contains(ReturnKey)
            : seed->mapFileName == "oute3.odm" && !seed->heldInventoryItemActive,
            "Expected normal MM6 seed or previous actual bottle save");
        std::filesystem::create_directories(output);
        std::filesystem::copy_file(input, output / "source-input.oysav");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json") << "{\"controlled\":true,\"natural_campaign\":false,"
            "\"mode\":" << std::quoted(returning ? "cold-return" : "enter")
            << ",\"scope\":\"Entry uses ordinary item service with one declared held1452 fixture; no pose,"
               "quest,AI or immunity edits. Return loads the actual inside save in a new App and dispatches"
               " original map501 through EventRuntime with the real bound world/session. This validates"
               " cross-map ownership, not physical exit clicking. Both continue30 normalframes and save.\","
               "\"new_unit_tests\":0,\"wall_limit_seconds\":90}\n";
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.debugConsole = false;
        settings.skipEventCutscenes = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save settings: " + error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Bottle173";
        config.assetRoot = runtime.string();
        config.activeWorldId = returning ? "mm7" : "mm6";
        config.startupMapFileOverride = seed->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned frames = 0, phase = 0, normalFrames = 0, captures = 0;
        const uint64_t started = SDL_GetTicks();
        bool complete = false;
        bool initialCaptureQueued = false;
        bool arrivalCaptureQueued = false;
        std::string pendingCapture;
        EventRuntimeState::SavedLocation origin;
        Party::Snapshot baseline;
        std::ofstream trace(output / "trace.jsonl");
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Overlapping screenshot");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto report = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"passed\":" << passed << ",\"mode\":" << std::quoted(returning ? "cold-return" : "enter")
                << ",\"frames\":" << frames << ",\"normal_frames\":" << normalFrames
                << ",\"captures\":" << captures << ",\"origin_map\":" << std::quoted(origin.mapName)
                << ",\"origin_xyz\":[" << origin.x << ',' << origin.y << ',' << origin.z << ']'
                << ",\"physical_exit_click\":false,\"new_unit_tests\":0,\"failure\":" << std::quoted(failure) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "source-input.oysav"),
                        "Cannot load seed in the actual App");
                    GameSession &session = game.m_gameSession;
                    auto &screen = session.gameplayScreenRuntime();
                    baseline = screen.partyReadOnly()->snapshot();
                    if (returning)
                    {
                        origin = seed->sessionSavedLocations.at(ReturnKey);
                        const auto &restored = session.sessionSavedLocations().at(ReturnKey);
                        require(restored.mapName == origin.mapName && restored.x == origin.x
                            && restored.y == origin.y && restored.z == origin.z,
                            "Cold load changed the session return point");
                    }
                    else
                    {
                        origin.mapName = session.currentMapFileName();
                        origin.x = int32_t(std::lround(screen.partyX()));
                        origin.y = int32_t(std::lround(screen.partyY()));
                        origin.z = int32_t(std::lround(screen.partyFootZ()));
                        InventoryItem bottle = {};
                        bottle.objectDescriptionId = 1452;
                        bottle.quantity = 1;
                        GameplayHeldItemController::setHeldInventoryItem(screen.heldInventoryItem(), bottle);
                    }
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames < 600 && SDL_GetTicks() - started < 90000, "Bottle flow timed out");
                    game.renderFrame(width, height, wheel, phase == 2 ? delta : 0.0f);
                    if (phase == 2) ++normalFrames;
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(surface != nullptr, "Cannot create screenshot surface");
                            const bool saved = SDL_SaveBMP(surface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(surface);
                            require(saved, "Cannot save screenshot");
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    GameSession &session = game.m_gameSession;
                    auto &screen = session.gameplayScreenRuntime();
                    if (phase == 0 && frames >= 8)
                    {
                        if (returning && !initialCaptureQueued)
                        {
                            capture("cold-inside");
                            initialCaptureQueued = true;
                            return;
                        }
                        if (!pendingCapture.empty()) return;
                        if (returning)
                        {
                            const auto &map = game.m_gameDataLoader.getSelectedMap();
                            IGameplayWorldRuntime *world = session.activeWorldRuntime();
                            require(map && map->localEventProgram && map->localEventProgram->hasEvent(501)
                                && world && world->eventRuntimeState(), "Original bottle exit is absent");
                            EventRuntime events(&game.m_gameDataLoader.getHouseTable(),
                                &game.m_gameDataLoader.getNpcDialogTable());
                            require(events.executeEventById(map->localEventProgram, map->globalEventProgram,
                                501, *world->eventRuntimeState(), world->party(),
                                dynamic_cast<ISceneEventContext *>(world)), "Source501 failed");
                            const auto &move = world->eventRuntimeState()->pendingMapMove;
                            require(move && move->mapName == origin.mapName && move->x == origin.x
                                && move->y == origin.y && move->z == origin.z && move->directionDegrees == 0
                                && move->lookAngleDegrees == 0 && move->clearSessionLocation == ReturnKey,
                                "Source501 did not request the exact return point");
                            require(session.sessionSavedLocations().contains(ReturnKey),
                                "Return point consumed before successful arrival");
                        }
                        else
                        {
                            require(screen.itemService().tryUseHeldItemOnPartyMember(screen, 0, false),
                                "Ordinary bottle item use failed");
                            require(session.pendingMapMove() && session.pendingMapMove()->mapName == "7nwc.blv"
                                && session.sessionSavedLocations().contains(ReturnKey),
                                "Item1452 did not publish the session origin and bottle map move");
                        }
                        phase = 1;
                    }
                    else if (phase == 1)
                    {
                        require(session.currentMapFileName() == (returning ? origin.mapName : "7nwc.blv"),
                            "Actual map transition did not reach the expected destination");
                        if (returning)
                        {
                            require(!session.sessionSavedLocations().contains(ReturnKey),
                                "Return point survived successful arrival");
                            require(std::abs(screen.partyX() - origin.x) < 1 && std::abs(screen.partyY() - origin.y) < 1
                                && std::abs(screen.partyFootZ() - origin.z) < 2, "Return pose changed");
                        }
                        else
                        {
                            const auto &stored = session.sessionSavedLocations().at(ReturnKey);
                            require(stored.mapName == origin.mapName && stored.x == origin.x
                                && stored.y == origin.y && stored.z == origin.z, "Entry lost the source position");
                        }
                        phase = 2;
                    }
                    else if (phase == 2 && normalFrames >= 8 && !arrivalCaptureQueued)
                    {
                        // Travel autosave owns the renderer's single screenshot slot during arrival.
                        capture("arrived");
                        arrivalCaptureQueued = true;
                    }
                    else if (phase == 2 && normalFrames >= 30 && pendingCapture.empty())
                    {
                        const auto path = output / (returning ? "returned.oysav" : "inside.oysav");
                        require(game.quickSaveToPath(path, "173瓶中神殿"), "Cannot save actual gameplay");
                        const auto saved = loadGameDataFromPath(path, error);
                        require(saved && saved->party.members.size() == baseline.members.size()
                            && saved->party.gold == baseline.gold && saved->heldInventoryItemActive
                            && saved->heldInventoryItem.objectDescriptionId == 1452,
                            "Save lost roster, Gold or unconsumed bottle: " + error);
                        for (size_t index = 0; index < baseline.members.size(); ++index)
                            require(saved->party.members[index].memberId == baseline.members[index].memberId,
                                "Bottle changed a member identity");
                        require(saved->sessionSavedLocations.contains(ReturnKey) == !returning,
                            "Save has incorrect return ownership");
                        capture("continued");
                        phase = 3;
                    }
                    else if (phase == 3 && pendingCapture.empty())
                    {
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                        phase = 4;
                    }
                    trace << "{\"frame\":" << frames << ",\"phase\":" << phase
                        << ",\"map\":" << std::quoted(session.currentMapFileName()) << "}\n";
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the bounded flow completed");
            report(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            report(false, failure.what());
            throw;
        }
    }
};
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 5 || (std::string(argv[4]) != "--enter" && std::string(argv[4]) != "--cold-return"))
            throw std::runtime_error("temple_bottle_capture RUNTIME INPUT NEW_OUTPUT --enter|--cold-return");
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3],
            std::string(argv[4]) == "--cold-return");
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Bottle capture: " << failure.what() << '\n';
        return 1;
    }
}
