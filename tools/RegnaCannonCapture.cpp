#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void queueWorldClick(float x, float y, int width, int height)
{
    int count = 0;
    SDL_Window **pWindows = SDL_GetWindows(&count);
    SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
    SDL_free(pWindows);
    require(pWindow != nullptr, "Expected one actual capture window");
    int pointWidth = 0, pointHeight = 0;
    require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot query capture window size");
    const auto edge = [&](uint8_t button, bool down)
    {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(pWindow);
        event.button.button = button;
        event.button.down = down;
        event.button.clicks = 1;
        event.button.x = x * float(pointWidth) / float(width);
        event.button.y = y * float(pointHeight) / float(height);
        require(SDL_PushEvent(&event), "Cannot queue ordinary SDL world click");
    };
    edge(SDL_BUTTON_RIGHT, true);
    edge(SDL_BUTTON_LEFT, true);
    edge(SDL_BUTTON_LEFT, false);
    edge(SDL_BUTTON_RIGHT, false);
}
}

// One actual cannon click; the unchanged map program and normal frame clock own every subsequent event.
struct GameApplicationTestAccess
{
    static int run(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(runtime / "engine.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm8.zip"), "Complete MM8 runtime required");
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(runtime).generic_string().starts_with("../"),
            "Use a new NVMe output directory outside the read-only runtime");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && !source->party.members.empty() && !source->heldInventoryItemActive
            && source->party.pendingItemDeliveries.empty() && !source->party.questBits.contains(37),
            "Expected the normal132 empty-handed MM8 save with Q37 false: " + error);
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot preserve the input save");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":168,\"controlled\":true,\"natural_campaign\":false,\"input\":"
            << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(runtime.string())
            << ",\"changes\":\"Load an unchanged normal132 MM8 save copy; debug-map jump to out13.odm. "
               "Require initial Q37=false and no662, then put exactly one real-table662 into an existing member. "
               "Set upstream Q224=true. Position feet at(-12960,12424,1857), supported by model85/face8 Z1856; "
               "aim at original cannon model85/face32 center(-12960,12680,1936). "
               "Apply initial FeatherFall and Invisibility once for3600 game seconds, equivalent to120 real seconds. "
               "No buff renewal, immortality, actor changes, direct event dispatch or completion-field fixtures. "
               "Freeze simulation only before the one ordinary SDL click; afterwards use normal render delta "
               "through the three timer stages and at least30 continued frames.\","
               "\"buff_duration_game_seconds\":3600,\"buff_duration_real_seconds_equivalent\":120,"
               "\"wall_limit_seconds\":45,\"frame_limit\":1800,\"audio\":\"dummy\","
               "\"screenshots\":[\"before-cannon\",\"cannon-action\",\"continued-game\"],"
               "\"visual_review_required\":true,\"new_unit_tests\":0,\"cold_replays\":0}\n";

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
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save capture settings: " + error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Regna Cannon168";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(9);
        enum class Phase { Warmup, Ready, Firing, Continuing, Finished };
        Phase phase = Phase::Warmup;
        uint64_t started = 0, carrierMemberId = 0;
        unsigned frames = 0, clickFrame = 0, normalFrames = 0, captures = 0;
        int lastStage = -1;
        std::array<bool, 4> observedStages{};
        bool jumpObserved = false, airborneObserved = false, actionRequested = false;
        bool complete = false, savedVerified = false;
        float maximumDistance = 0;
        double simulationSeconds = 0, continuedSeconds = 0;
        std::string pendingCapture;
        const bx::Vec3 center{-12960.0f, 12680.0f, 1936.0f};
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && game.m_gameSession.currentMapFileName() == "out13.odm", "Expected the actual out13 scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto stage = [&]() { return int(scene().eventRuntimeState()->mapVars.at(41)); };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto verifyCompletion = [&](const Party::Snapshot &party,
            const OutdoorWorldRuntime::Snapshot &world)
        {
            require(party.questBits.contains(37) && !party.questBits.contains(224)
                && world.eventRuntimeState && world.eventRuntimeState->mapVars.at(41) == 0,
                "Cannon quest flags or timer phase did not complete");
            Party restored;
            restored.restoreSnapshot(party);
            require(restored.inventoryItemCount(662) == 0, "Cannonball662 was not consumed exactly once");
            const auto &events = *world.eventRuntimeState;
            require(historyEventTimesForActiveContinent(events).contains(13), "Original History13 is absent");
            for (const auto [npc, house] : std::array<std::array<uint32_t, 2>, 3>{{{64, 899}, {20, 900}, {21, 900}}})
            {
                require(party.npcHouseOverrides.contains(npc) && party.npcHouseOverrides.at(npc) == house
                    && events.npcHouseOverrides.contains(npc) && events.npcHouseOverrides.at(npc) == house,
                    "Original NPC relocation is missing: " + std::to_string(npc));
            }
            // setFacetBit consumes the pending masks; inspect the applied geometry in snapshot order.
            size_t faceIndex = 0, hiddenCount = 0, shownCount = 0;
            const uint32_t bits = faceAttributeBit(FaceAttribute::Invisible)
                | faceAttributeBit(FaceAttribute::Untouchable);
            for (const auto &model : scene().worldRuntime().mapData()->bmodels)
            {
                for (const auto &face : model.faces)
                {
                    require(faceIndex < world.faceAttributes.size(), "Saved outdoor face attributes are incomplete");
                    const uint32_t attributes = world.faceAttributes[faceIndex++];
                    if (face.cogNumber == 30)
                    {
                        ++hiddenCount;
                        require((attributes & bits) == bits, "Original ship group30 remains visible or touchable");
                    }
                    else if (face.cogNumber == 31)
                    {
                        ++shownCount;
                        require((attributes & bits) == 0, "Original wreck group31 remains hidden or untouchable");
                    }
                }
            }
            require(hiddenCount && shownCount, "Original ship and wreck facet groups are absent");
        };
        const auto observe = [&]()
        {
            const int currentStage = stage();
            require(currentStage >= 0 && currentStage <= 3, "Unexpected cannon timer stage");
            observedStages[size_t(currentStage)] = true;
            const auto motion = scene().partyRuntime().snapshot();
            const auto &pose = motion.movementState;
            const bool activeJump = motion.jumpState.pendingSpecialJump || motion.jumpState.specialJumpActive;
            const bool newMotion = (activeJump && !jumpObserved) || (pose.airborne && !airborneObserved);
            jumpObserved = jumpObserved || activeJump;
            airborneObserved = airborneObserved || pose.airborne;
            maximumDistance = std::max(maximumDistance, std::hypot(pose.x + 12960.0f, pose.y - 12424.0f));
            if (currentStage != lastStage || newMotion)
            {
                trace << "{\"stage\":\"cannon-observation\",\"frame\":" << frames
                    << ",\"map_var41\":" << currentStage << ",\"Q37\":" << scene().party().hasQuestBit(37)
                    << ",\"special_jump\":" << activeJump << ",\"airborne\":" << pose.airborne
                    << ",\"pose\":[" << pose.x << ',' << pose.y << ',' << pose.footZ
                    << "],\"simulation_seconds\":" << simulationSeconds << "}\n";
                lastStage = currentStage;
            }
        };
        const auto result = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"passed\":" << passed << ",\"controlled\":true,\"natural_campaign\":false"
                << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"event451_ui_clicks\":" << (clickFrame ? 1 : 0)
                << ",\"model\":85,\"face\":32,\"ball_fixture_member_id\":" << carrierMemberId
                << ",\"phase1_observed\":" << observedStages[1] << ",\"phase2_observed\":" << observedStages[2]
                << ",\"phase3_observed\":" << observedStages[3] << ",\"jump_observed\":" << jumpObserved
                << ",\"airborne_observed\":" << airborneObserved << ",\"maximum_xy_distance\":" << maximumDistance
                << ",\"simulation_seconds\":" << simulationSeconds << ",\"continued_frames\":" << normalFrames
                << ",\"continued_seconds\":" << continuedSeconds << ",\"final_save_verified\":" << savedVerified
                << ",\"visual_review_required\":true,\"failure\":" << std::quoted(failure) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "source-input.oysav"),
                        "Cannot load the unchanged input copy");
                    const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName("out13.odm");
                    require(pMap != nullptr, "Regna is absent from MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared out13 map fixture failed");
                    Party &party = scene().party();
                    require(!party.hasQuestBit(37) && !party.hasItemAnywhere(662) && stage() == 0
                        && !historyEventTimesForActiveContinent(*scene().eventRuntimeState()).contains(13),
                        "Normal seed already owns a cannonball or began/completed the cannon sequence");
                    const ItemTable &items = game.m_gameDataLoader.getItemTable();
                    require(items.get(662) != nullptr, "Original cannonball662 is absent from the item table");
                    const InventoryItem ball = ItemGenerator::makeInventoryItem(662, items, ItemGenerationMode::Generic);
                    for (size_t index = 0; index < party.memberCount(); ++index)
                    {
                        if (party.tryAutoPlaceItemInMemberInventory(index, ball))
                        {
                            carrierMemberId = party.member(index)->memberId;
                            break;
                        }
                    }
                    require(carrierMemberId && party.inventoryItemCount(662) == 1,
                        "Cannot place exactly one cannonball in an existing member inventory");
                    party.setQuestBit(224, true);
                    party.applyPartyBuff(PartyBuffId::FeatherFall, 3600.0f, 0, 13, 1, SkillMastery::Normal, 0);
                    party.applyPartyBuff(PartyBuffId::Invisibility, 3600.0f, 0, 19, 1, SkillMastery::Normal, 0);
                    const auto *pData = scene().worldRuntime().mapData();
                    require(pData && pData->bmodels.size() > 85 && pData->bmodels[85].faces.size() > 32
                        && pData->bmodels[85].faces[32].cogTriggeredNumber == 451,
                        "Original cannon model85/face32/event451 is absent");
                    scene().partyRuntime().teleportTo(-12960.0f, 12424.0f, 1857.0f);
                    const auto &pose = scene().partyRuntime().movementState();
                    game.m_outdoorGameView.setCameraAngles(
                        std::atan2(center.y - pose.y, center.x - pose.x),
                        std::atan2(center.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                            std::hypot(center.x - pose.x, center.y - pose.y)));
                    trace << "{\"stage\":\"declared-cannon-fixture\",\"ball_member_id\":" << carrierMemberId
                        << ",\"pose\":[" << pose.x << ',' << pose.y << ',' << pose.footZ
                        << "],\"Q224\":true,\"Q37\":false,\"map_var41\":0,\"buff_game_seconds\":3600}\n";
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 1800 && SDL_GetTicks() - started < 45000,
                        "Bounded Regna cannon capture timed out");
                    const bool running = clickFrame != 0;
                    const bool continuing = phase == Phase::Continuing;
                    if (running) observe();
                    game.renderFrame(width, height, wheel, running ? delta : 0.0f);
                    require(game.m_screenManager.activeScreen() == nullptr
                        && !game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive
                        && scene().party().hasActableMember(), "Cannon gameplay was interrupted");
                    if (running)
                    {
                        simulationSeconds += std::max(0.0f, delta);
                        if (continuing && delta > 0) { ++normalFrames; continuedSeconds += delta; }
                        observe();
                    }
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create screenshot surface");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save capture image");
                            trace << "{\"capture\":" << std::quoted(pendingCapture)
                                << ",\"frame\":" << frames << "}\n";
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        capture("before-cannon");
                        phase = Phase::Ready;
                    }
                    else if (phase == Phase::Ready && pendingCapture.empty())
                    {
                        auto &world = scene().worldRuntime();
                        const auto camera = world.buildWorldPickRequest({0, 0, width, height, false});
                        float viewProjection[16];
                        bx::mtxMul(viewProjection, camera.viewMatrix.data(), camera.projectionMatrix.data());
                        const auto &model = world.mapData()->bmodels.at(85);
                        std::vector<bx::Vec3> points{center};
                        for (uint16_t index : model.faces.at(32).vertexIndices)
                        {
                            const auto &v = model.vertices.at(index);
                            points.push_back(bx::add(center,
                                bx::mul(bx::sub({float(v.x), float(v.y), float(v.z)}, center), 0.65f)));
                        }
                        std::optional<std::array<float, 2>> pixel;
                        for (const auto &point : points)
                        {
                            const auto projected = bx::mulH(point, viewProjection);
                            const float x = (projected.x + 1) * float(width) * 0.5f;
                            const float y = (1 - projected.y) * float(height) * 0.5f;
                            if (x < 0 || y < 0 || x >= width || y >= height) continue;
                            const auto hit = world.pickMouseInteractionTarget(
                                world.buildWorldPickRequest({x, y, width, height, true}));
                            if (hit.eventTarget && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                                && hit.eventTarget->targetIndex == 85 && hit.eventTarget->secondaryIndex == 32
                                && hit.eventTarget->triggeredEventId == 451
                                && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                            {
                                pixel = std::array<float, 2>{x, y};
                                break;
                            }
                        }
                        require(pixel.has_value(), "Original cannon85/32/event451 is unreachable by the real picker");
                        queueWorldClick((*pixel)[0], (*pixel)[1], width, height);
                        clickFrame = frames;
                        phase = Phase::Firing;
                        trace << "{\"stage\":\"queued-real-event451-click\",\"frame\":" << frames
                            << ",\"pixel\":[" << (*pixel)[0] << ',' << (*pixel)[1] << "]}\n";
                    }
                    else if (phase == Phase::Firing)
                    {
                        if (!actionRequested && pendingCapture.empty()
                            && ((jumpObserved && scene().partyRuntime().movementState().airborne) || stage() == 3))
                        {
                            capture("cannon-action");
                            actionRequested = true;
                        }
                        if (scene().party().hasQuestBit(37) && stage() == 0)
                        {
                            require(observedStages[1] && observedStages[2] && observedStages[3]
                                && jumpObserved && airborneObserved && maximumDistance > 8 && actionRequested,
                                "The actual three cannon stages and displacement were not observed");
                            verifyCompletion(scene().party().snapshot(), scene().worldRuntime().snapshot());
                            phase = Phase::Continuing;
                            trace << "{\"stage\":\"source-cannon-completed\",\"frame\":" << frames << "}\n";
                        }
                    }
                    else if (phase == Phase::Continuing && normalFrames >= 30 && pendingCapture.empty())
                    {
                        const auto live = scene().worldRuntime().snapshot();
                        verifyCompletion(scene().party().snapshot(), live);
                        const auto finalPath = output / "regna-cannon-complete.oysav";
                        require(game.quickSaveToPath(finalPath, "168雷格纳炮击后继续游戏"), "Cannot save final gameplay");
                        const auto saved = loadGameDataFromPath(finalPath, error);
                        require(saved && saved->mapFileName == "out13.odm" && !saved->heldInventoryItemActive
                            && saved->party.pendingItemDeliveries.empty(), "Final outdoor save is incomplete: " + error);
                        verifyCompletion(saved->party, saved->outdoorWorld);
                        require(historyEventTimesForActiveContinent(*saved->outdoorWorld.eventRuntimeState).at(13)
                            == historyEventTimesForActiveContinent(*live.eventRuntimeState).at(13),
                            "Saved History13 timestamp changed");
                        savedVerified = true;
                        capture("continued-game");
                        phase = Phase::Finished;
                    }
                    else if (phase == Phase::Finished && pendingCapture.empty())
                    {
                        require(captures == 3 && normalFrames >= 30 && savedVerified,
                            "Single cannon capture evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the bounded cannon sequence completed");
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            result(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "regna_cannon_capture RUNTIME READONLY_MM8_SAVE NEW_OUTPUT\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Regna cannon capture: " << failure.what() << '\n';
        return 1;
    }
}
