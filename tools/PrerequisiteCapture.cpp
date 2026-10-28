#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/gameplay/ReputationRuntime.h"
#include "game/items/PriceCalculator.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/tables/MergedBaseTables.h"
#include "game/ui/screens/CutsceneVideoScreen.h"

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
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
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
        require(SDL_PushEvent(&event), "Cannot queue ordinary SDL door click");
    };
    edge(SDL_BUTTON_RIGHT, true);
    edge(SDL_BUTTON_LEFT, true);
    edge(SDL_BUTTON_LEFT, false);
    edge(SDL_BUTTON_RIGHT, false);
}

bool sameFollower(const HiredNpcFollower &left, const HiredNpcFollower &right)
{
    return left.npcId == right.npcId && left.professionId == right.professionId
        && left.weeklyCost == right.weeklyCost && left.abilityUsedDay == right.abilityUsedDay
        && left.name == right.name && left.pictureId == right.pictureId;
}

void queueEscape()
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_ESCAPE;
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    event.key.down = true;
    require(SDL_PushEvent(&event), "Cannot queue original intro Escape down");
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    require(SDL_PushEvent(&event), "Cannot queue original intro Escape up");
}
}

// A single bounded actual interaction per mode, with declared upstream fixtures and no direct event dispatch.
struct GameApplicationTestAccess
{
    static int run(std::filesystem::path runtime, std::filesystem::path input,
        std::filesystem::path output, bool harbor)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        const std::string mode = harbor ? "harbor" : "temple";
        const std::string entryMap = harbor ? "7out03.odm" : "outc2.odm";
        const std::string finalMap = harbor ? "7out01.odm" : entryMap;
        const uint16_t doorEvent = harbor ? 11 : 19;
        require(std::filesystem::is_regular_file(runtime / "engine.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm6.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm7.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm8.zip"), "Complete three-world runtime required");
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(runtime).generic_string().starts_with("../"),
            "Use a new NVMe output directory outside the read-only runtime");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && !source->party.members.empty() && !source->heldInventoryItemActive
            && source->party.pendingItemDeliveries.empty(), "Expected an empty-handed normal132 input: " + error);
        require(!harbor || !source->party.questBits.contains(519), "Harbor input must already have Q519=false");
        require(harbor || (!source->party.questBits.contains(1130) && !source->party.questBits.contains(1131)
            && !source->party.questBits.contains(1708) && !source->party.questBits.contains(1709)),
            "Temple input must not have repaired/completed the temple or own the obsolete artisan QBits");
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot preserve the input save");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":171,\"mode\":" << std::quoted(mode)
            << ",\"controlled\":true,\"natural_campaign\":false,\"device_acceptance\":false,\"input\":"
            << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(runtime.string())
            << ",\"changes\":" << std::quoted(harbor
                ? "Load an unchanged normal132 seed copy whose Q519 is already false. Debug-map jump to 7out03. "
                  "Advance the initial clock to the next Wednesday noon; add 10000 Gold. "
                  "Place the party 256 units outside the original event11 door using its real face normal. "
                  "One ordinary SDL world click opens house486. Render the visible route4, then select it through "
                  "GameplayScreenRuntime::executeActiveDialogAction, the normal dialogue action API. "
                  "The route selection is not a physical click. The original first-arrival intro may open; "
                  "close it through ordinary Escape. Never write Q519, route overrides or arrival state."
                : "Load an unchanged normal132 seed copy; require Q1130/Q1131/1708/1709 false. "
                  "Debug-map jump to outc2; set only the upstream repair quest Q1129. "
                  "Add real-table hired NPC1059/profession63 and NPC1069/profession64, with table names, portraits "
                  "and weekly costs. Retain existing unrelated followers; if none, add real NPC1112/profession20 "
                  "as an unrelated retention witness. These are initial hired-follower fixtures, not observed hiring. "
                  "Place the party 256 units outside original event19 using its real face normal. "
                  "One ordinary SDL world click performs repair. Source success is text29/Q1130/removal only; "
                  "Gold/XP remain unchanged, and no house entry is expected on the successful repair click.")
            << ",\"shared_controls\":\"Freeze simulation during initial framing and dialogue capture; "
               "run at least30 normal frames after the action. No actor/AI/buff/immortality fixtures, "
               "no direct event dispatch, no completion-field writes, no retry or cold replay.\","
               "\"wall_limit_seconds\":90,\"frame_limit\":900,\"audio\":\"dummy\","
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
        config.appName = "OpenYAMM Prerequisite171 " + mode;
        config.assetRoot = runtime.string();
        config.activeWorldId = harbor ? "mm7" : "mm6";
        config.startupMapFileOverride = entryMap;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(12);
        enum class Phase { Warmup, DoorReady, DoorClicked, ActionReady, Arriving, Continuing, Finished };
        Phase phase = Phase::Warmup;
        const uint64_t started = SDL_GetTicks();
        unsigned frames = 0, phaseFrame = 0, normalFrames = 0, captures = 0, doorClicks = 0, actionCalls = 0;
        size_t doorModel = 0, doorFace = 0;
        bx::Vec3 doorCenter{0.0f, 0.0f, 0.0f};
        bool complete = false, savedVerified = false, actionVisible = false, proprietorSelected = false;
        bool sourceMessageObserved = false;
        bool arrivalIntroObserved = false, arrivalIntroEscapeSent = false;
        unsigned arrivalIntroFrame = 0;
        int goldBefore = 0, fare = 0;
        double departureMinutes = 0, normalSeconds = 0;
        std::vector<std::pair<uint64_t, uint32_t>> experienceBefore;
        std::vector<HiredNpcFollower> retainedFollowers;
        std::string pendingCapture;
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Expected an actual outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto logDialog = [&]()
        {
            const auto &dialog = screen().activeEventDialog();
            trace << "{\"stage\":\"visible-dialogue\",\"frame\":" << frames
                << ",\"active\":" << dialog.isActive << ",\"house\":" << dialog.isHouseDialog
                << ",\"source_id\":" << dialog.sourceId << ",\"title\":" << std::quoted(dialog.title)
                << ",\"lines\":[";
            for (size_t index = 0; index < dialog.lines.size(); ++index)
            {
                if (index) trace << ',';
                trace << std::quoted(dialog.lines[index]);
            }
            trace << "],\"actions\":[";
            for (size_t index = 0; index < dialog.actions.size(); ++index)
            {
                if (index) trace << ',';
                const auto &action = dialog.actions[index];
                trace << "{\"kind\":" << int(action.kind) << ",\"id\":" << action.id
                    << ",\"argument\":" << std::quoted(action.argument)
                    << ",\"label\":" << std::quoted(action.label) << ",\"enabled\":" << action.enabled << '}';
            }
            trace << "]}\n";
        };
        const auto routeIndex = [&]()
        {
            const auto &dialog = screen().activeEventDialog();
            require(dialog.isActive && dialog.isHouseDialog && dialog.sourceId == 486,
                "Original event11 did not open transport house486");
            const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(), [](const auto &action)
            {
                return action.kind == EventDialogActionKind::HouseService
                    && action.id == uint32_t(HouseActionId::TransportRoute) && action.argument == "4";
            });
            require(found != dialog.actions.end() && found->enabled && !found->textOnly && !found->label.empty(),
                "Original Wednesday route4 is not visibly available");
            return size_t(found - dialog.actions.begin());
        };
        const auto verifyParty = [&](const Party::Snapshot &party)
        {
            require(party.members.size() == experienceBefore.size(), "Prerequisite action changed the roster size");
            for (size_t index = 0; index < experienceBefore.size(); ++index)
            {
                require(party.members[index].memberId == experienceBefore[index].first
                    && party.members[index].experience == experienceBefore[index].second,
                    "Prerequisite action changed member identity or original XP");
            }
            require(party.gold == goldBefore - fare, "Prerequisite action changed Gold beyond the ordinary fare");
            if (harbor)
            {
                require(!party.questBits.contains(519), "The driver or travel incorrectly completed Q519");
                return;
            }
            require(party.questBits.contains(1129) && party.questBits.contains(1130)
                && !party.questBits.contains(1131) && !party.questBits.contains(1708)
                && !party.questBits.contains(1709), "Source temple repair flags are incorrect");
            require(party.hiredNpcFollowers.size() == retainedFollowers.size(),
                "Temple repair did not remove exactly the two hired artisans");
            for (const HiredNpcFollower &expected : retainedFollowers)
            {
                require(std::any_of(party.hiredNpcFollowers.begin(), party.hiredNpcFollowers.end(),
                    [&](const HiredNpcFollower &actual) { return sameFollower(actual, expected); }),
                    "Temple repair changed an unrelated hired follower");
            }
            require(!party.unavailableNpcIds.contains(1059) && !party.unavailableNpcIds.contains(1069),
                "Removed artisans remain marked hired/unavailable");
        };
        const auto result = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(12)
                << "{\"passed\":" << passed << ",\"mode\":" << std::quoted(mode)
                << ",\"controlled\":true,\"natural_campaign\":false,\"device_acceptance\":false"
                << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"source_door_event\":" << doorEvent << ",\"door_sdl_clicks\":" << doorClicks
                << ",\"door_model\":" << doorModel << ",\"door_face\":" << doorFace
                << ",\"normal_dialogue_action_api_calls\":" << actionCalls
                << ",\"transport_selection_is_physical_click\":false,\"source_action_visible\":" << actionVisible
                << ",\"source_repair_message_observed\":" << sourceMessageObserved
                << ",\"original_arrival_intro_observed\":" << arrivalIntroObserved
                << ",\"original_arrival_intro_escape\":" << arrivalIntroEscapeSent
                << ",\"gold_before\":" << goldBefore << ",\"ordinary_fare\":" << fare
                << ",\"departure_minutes\":" << departureMinutes << ",\"normal_frames\":" << normalFrames
                << ",\"normal_seconds\":" << normalSeconds << ",\"final_save_verified\":" << savedVerified
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
                    const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName(entryMap);
                    require(pMap != nullptr, "Source entry map is absent from MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump() && game.m_gameSession.currentMapFileName() == entryMap,
                        "Declared entry map fixture failed");
                    Party &party = scene().party();
                    auto &events = *scene().eventRuntimeState();
                    if (harbor)
                    {
                        require(!party.hasQuestBit(519), "Input load changed Q519");
                        const uint64_t key = EventRuntime::transportRouteOverrideKey(486, 4);
                        const auto route = events.transportRouteOverrides.find(key);
                        require(route != events.transportRouteOverrides.end(), "Source harbor override was not loaded");
                        const auto &actual = route->second;
                        require(actual.mapFileName == "7out01.odm" && actual.travelDays == 6
                            && actual.x == 12552 && actual.y == 800 && actual.z == 193
                            && actual.directionDegrees == 90 && actual.requiredQBit == 0
                            && !actual.useMapStartPosition && !actual.destinationName.empty()
                            && actual.daysAvailable
                                == std::array<bool, 7>{false, false, true, false, false, false, false},
                            "Source101 harbor route differs from its original Wednesday destination");
                        const double now = scene().worldRuntime().gameMinutes();
                        int64_t day = int64_t(std::floor(now / 1440.0));
                        day += (2 - day % 7 + 7) % 7;
                        double noon = double(day) * 1440.0 + 720.0;
                        if (noon < now) noon += 7.0 * 1440.0;
                        scene().worldRuntime().advanceGameMinutes(noon - now);
                        game.m_gameSession.setGameMinutes(noon);
                        party.addGold(10000);
                        departureMinutes = scene().worldRuntime().gameMinutes();
                    }
                    else
                    {
                        require(!party.hasQuestBit(1130) && !party.hasQuestBit(1131)
                            && !party.hasQuestBit(1708) && !party.hasQuestBit(1709),
                            "Initial temple state changed before the repair fixture");
                        party.setQuestBit(1129, true);
                        retainedFollowers = party.snapshot().hiredNpcFollowers;
                        require(retainedFollowers.size() <= 2
                            && std::none_of(retainedFollowers.begin(), retainedFollowers.end(), [](const auto &follower)
                            {
                                return follower.professionId == 63 || follower.professionId == 64;
                            }), "Seed has conflicting artisan followers or insufficient ordinary follower slots");
                        const auto addFollower = [&](uint32_t npcId, uint32_t professionId)
                        {
                            const auto *pNpc = game.m_gameDataLoader.getNpcDialogTable().getNpc(npcId);
                            const auto *pProfession =
                                game.m_gameDataLoader.getMergedNpcProfessionTable().get(professionId);
                            require(pNpc && pNpc->professionId == professionId && pProfession && pProfession->joins,
                                "Original hireable NPC/profession is absent");
                            HiredNpcFollower follower{npcId, professionId, pProfession->weeklyCost};
                            follower.name = pNpc->name;
                            follower.pictureId = pNpc->pictureId;
                            party.addHiredNpcFollower(follower);
                            party.clearNpcHouseOverride(npcId);
                            trace << "{\"stage\":\"declared-hired-follower\",\"npc_id\":" << npcId
                                << ",\"profession_id\":" << professionId << ",\"name\":" << std::quoted(follower.name)
                                << ",\"weekly_cost\":" << follower.weeklyCost << "}\n";
                            return follower;
                        };
                        if (retainedFollowers.empty()) retainedFollowers.push_back(addFollower(1112, 20));
                        addFollower(1059, 63);
                        addFollower(1069, 64);
                        party.applyGlobalNpcStateTo(events);
                    }
                    goldBefore = party.gold();
                    for (const Character &member : party.members())
                    {
                        experienceBefore.emplace_back(member.memberId, member.experience);
                    }
                    const auto *pData = scene().worldRuntime().mapData();
                    require(pData != nullptr, "Source outdoor geometry is absent");
                    bool found = false;
                    for (size_t modelIndex = 0; modelIndex < pData->bmodels.size() && !found; ++modelIndex)
                    {
                        const auto &model = pData->bmodels[modelIndex];
                        for (size_t faceIndex = 0; faceIndex < model.faces.size(); ++faceIndex)
                        {
                            const auto &face = model.faces[faceIndex];
                            if (face.cogTriggeredNumber != doorEvent || face.vertexIndices.empty()
                                || !hasFaceAttribute(face.attributes, FaceAttribute::Clickable)
                                || hasFaceAttribute(face.attributes, FaceAttribute::Invisible)
                                || hasFaceAttribute(face.attributes, FaceAttribute::HasHint)) continue;
                            const float nx = float(face.planeNormalX), ny = float(face.planeNormalY);
                            const float length = std::hypot(nx, ny);
                            if (length == 0) continue;
                            float bottom = std::numeric_limits<float>::max();
                            for (uint16_t index : face.vertexIndices)
                            {
                                const auto &v = model.vertices.at(index);
                                doorCenter = bx::add(doorCenter, {float(v.x), float(v.y), float(v.z)});
                                bottom = std::min(bottom, float(v.z));
                            }
                            doorCenter = bx::mul(doorCenter, 1.0f / float(face.vertexIndices.size()));
                            scene().partyRuntime().teleportTo(doorCenter.x + nx / length * 256,
                                doorCenter.y + ny / length * 256, bottom + 1);
                            const auto &pose = scene().partyRuntime().movementState();
                            game.m_outdoorGameView.setCameraAngles(
                                std::atan2(doorCenter.y - pose.y, doorCenter.x - pose.x),
                                std::atan2(doorCenter.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                                    std::hypot(doorCenter.x - pose.x, doorCenter.y - pose.y)));
                            doorModel = modelIndex;
                            doorFace = faceIndex;
                            found = true;
                            trace << "{\"stage\":\"declared-entry-fixture\",\"map\":" << std::quoted(entryMap)
                                << ",\"event\":" << doorEvent << ",\"model\":" << doorModel
                                << ",\"face\":" << doorFace << ",\"pose\":[" << pose.x << ',' << pose.y << ','
                                << pose.footZ << "],\"game_minutes\":" << scene().worldRuntime().gameMinutes()
                                << ",\"Q519\":" << party.hasQuestBit(519)
                                << ",\"Q1130\":" << party.hasQuestBit(1130) << "}\n";
                            break;
                        }
                    }
                    require(found, "No original clickable source door binding was found");
                    require(game.quickSaveToPath(output / "declared-start.oysav", "171前置条件受控输入"),
                        "Cannot preserve the declared initial fixture");
                    require(SDL_GetTicks() - started < 90000, "Bounded startup timed out");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 900 && SDL_GetTicks() - started < 90000,
                        "Bounded prerequisite capture timed out");
                    const bool continuing = phase == Phase::Continuing;
                    const bool videoActive = dynamic_cast<CutsceneVideoScreen *>(
                        game.m_screenManager.activeScreen()) != nullptr;
                    game.renderFrame(width, height, wheel, (continuing || videoActive) ? delta : 0.0f);
                    if (harbor && dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()))
                    {
                        require(phase == Phase::Arriving || phase == Phase::Continuing,
                            "Unexpected movie before the original boat arrival");
                        if (!arrivalIntroObserved)
                        {
                            for (uint32_t bit = 513; bit <= 518; ++bit)
                            {
                                require(scene().party().hasQuestBit(bit),
                                    "Original arrival did not open the six scavenger-hunt quests");
                            }
                            arrivalIntroObserved = true;
                            arrivalIntroFrame = frames;
                            trace << "{\"stage\":\"original-scavenger-intro\",\"frame\":" << frames << "}\n";
                        }
                        if (!arrivalIntroEscapeSent && frames >= arrivalIntroFrame + 5)
                        {
                            queueEscape();
                            arrivalIntroEscapeSent = true;
                        }
                        require(frames < arrivalIntroFrame + 60, "Original arrival intro did not close on Escape");
                        trace.flush();
                        return;
                    }
                    require(game.m_screenManager.activeScreen() == nullptr && scene().party().hasActableMember(),
                        "Prerequisite gameplay was interrupted");
                    if (continuing && delta > 0)
                    {
                        require(!screen().activeEventDialog().isActive, "Continued gameplay is still in dialogue");
                        ++normalFrames;
                        normalSeconds += delta;
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
                            trace << "{\"capture\":" << std::quoted(pendingCapture) << ",\"frame\":" << frames << "}\n";
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        capture("before-source-door");
                        phase = Phase::DoorReady;
                    }
                    else if (phase == Phase::DoorReady && pendingCapture.empty())
                    {
                        auto &world = scene().worldRuntime();
                        const auto camera = world.buildWorldPickRequest({0, 0, width, height, false});
                        float viewProjection[16];
                        bx::mtxMul(viewProjection, camera.viewMatrix.data(), camera.projectionMatrix.data());
                        const auto &model = world.mapData()->bmodels.at(doorModel);
                        std::vector<bx::Vec3> points{doorCenter};
                        for (uint16_t index : model.faces.at(doorFace).vertexIndices)
                        {
                            const auto &v = model.vertices.at(index);
                            points.push_back(bx::add(doorCenter,
                                bx::mul(bx::sub({float(v.x), float(v.y), float(v.z)}, doorCenter), 0.65f)));
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
                                && hit.eventTarget->targetIndex == doorModel
                                && hit.eventTarget->secondaryIndex == doorFace
                                && hit.eventTarget->triggeredEventId == doorEvent
                                && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                            {
                                pixel = std::array<float, 2>{x, y};
                                break;
                            }
                        }
                        require(pixel.has_value(), "Original source door is unreachable by the real picker");
                        queueWorldClick((*pixel)[0], (*pixel)[1], width, height);
                        ++doorClicks;
                        phaseFrame = frames;
                        phase = Phase::DoorClicked;
                        trace << "{\"stage\":\"queued-source-door-click\",\"frame\":" << frames
                            << ",\"pixel\":[" << (*pixel)[0] << ',' << (*pixel)[1] << "]}\n";
                    }
                    else if (phase == Phase::DoorClicked && frames >= phaseFrame + 4)
                    {
                        logDialog();
                        if (harbor)
                        {
                            const auto &dialog = screen().activeEventDialog();
                            const auto proprietor = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                                [](const auto &action)
                                { return action.kind == EventDialogActionKind::HouseProprietor; });
                            if (proprietor != dialog.actions.end() && !proprietorSelected)
                            {
                                require(proprietor->enabled, "Original transport proprietor is unavailable");
                                screen().eventDialogSelectionIndex() = size_t(proprietor - dialog.actions.begin());
                                screen().executeActiveDialogAction();
                                ++actionCalls;
                                proprietorSelected = true;
                                phaseFrame = frames;
                            }
                            else
                            {
                                routeIndex();
                                actionVisible = true;
                                capture("visible-emerald-isle-route");
                                phase = Phase::ActionReady;
                            }
                        }
                        else
                        {
                            verifyParty(scene().party().snapshot());
                            // Original outc2.str[29], independently checked during the171 source import.
                            // Exported map Lua inlines STR text and does not populate evt.strings metadata.
                            const std::string text = "石匠和木匠开始重建神庙";
                            const auto &dialog = screen().activeEventDialog();
                            sourceMessageObserved = dialog.isActive
                                && std::find(dialog.lines.begin(), dialog.lines.end(), text) != dialog.lines.end();
                            require(sourceMessageObserved, "Original temple repair message29 is not visible");
                            actionVisible = true;
                            capture("source-temple-repair");
                            phase = Phase::ActionReady;
                        }
                    }
                    else if (phase == Phase::ActionReady && pendingCapture.empty())
                    {
                        if (harbor)
                        {
                            const auto *pHouse = screen().houseTable()->get(486);
                            require(pHouse != nullptr, "Original house486 is absent");
                            const Character *pMerchant = scene().party().bestPartyWideUtilitySkillMember("Merchant");
                            if (!pMerchant) pMerchant = scene().party().activeMember();
                            fare = PriceCalculator::transportPrice(pMerchant, *pHouse, true,
                                effectivePartyReputation(scene().worldRuntime().currentLocationReputation(),
                                    scene().eventRuntimeState()));
                            require(fare > 0 && goldBefore >= fare, "Ordinary transport fare is invalid");
                            screen().eventDialogSelectionIndex() = routeIndex();
                            screen().executeActiveDialogAction();
                            ++actionCalls;
                            const auto &eventMove = scene().eventRuntimeState()->pendingMapMove;
                            const auto &move = eventMove ? eventMove : game.m_gameSession.pendingMapMove();
                            require(move && move->mapName == "7out01.odm" && move->x == 12552 && move->y == 800
                                && move->z == 193 && move->directionDegrees == 90 && !move->useMapStartPosition
                                && move->traceSourceKind == "boat_route" && move->traceSourceId == 486,
                                "Normal transport action did not request the source101 arrival");
                            require(std::abs(scene().worldRuntime().gameMinutes() - departureMinutes - 8640.0) < 0.01,
                                "Normal route did not advance its six original travel days");
                            verifyParty(scene().party().snapshot());
                            phase = Phase::Arriving;
                        }
                        else
                        {
                            if (screen().activeEventDialog().isActive) screen().handleDialogueCloseRequest();
                            require(!screen().activeEventDialog().isActive, "Temple message did not close normally");
                            phase = Phase::Continuing;
                        }
                    }
                    else if (phase == Phase::Arriving)
                    {
                        require(game.m_gameSession.currentMapFileName() == finalMap,
                            "Normal transport did not arrive on Emerald Isle");
                        const auto &pose = scene().partyRuntime().movementState();
                        require(std::hypot(pose.x - 12552, pose.y - 800) < 1.0f,
                            "Normal transport lost the original landing coordinates");
                        require(std::abs(scene().worldRuntime().gameMinutes() - departureMinutes - 8640.0) < 0.01,
                            "Map transition changed the ordinary travel duration");
                        verifyParty(scene().party().snapshot());
                        trace << "{\"stage\":\"ordinary-boat-arrived\",\"frame\":" << frames
                            << ",\"map\":" << std::quoted(finalMap) << ",\"pose\":[" << pose.x << ','
                            << pose.y << ',' << pose.footZ << "],\"Q519\":false}\n";
                        phase = Phase::Continuing;
                    }
                    else if (phase == Phase::Continuing && normalFrames >= 30 && pendingCapture.empty())
                    {
                        verifyParty(scene().party().snapshot());
                        const auto finalPath = output / (mode + "-complete.oysav");
                        require(game.quickSaveToPath(finalPath, "171前置条件后继续游戏"), "Cannot save final gameplay");
                        const auto saved = loadGameDataFromPath(finalPath, error);
                        require(saved && saved->mapFileName == finalMap && !saved->heldInventoryItemActive
                            && saved->party.pendingItemDeliveries.empty(), "Final save is incomplete: " + error);
                        verifyParty(saved->party);
                        require(saved->outdoorWorld.eventRuntimeState.has_value(), "Final event state is absent");
                        if (!harbor)
                        {
                            const auto &followers = saved->outdoorWorld.eventRuntimeState->hiredNpcFollowers;
                            require(followers.size() == retainedFollowers.size()
                                && std::none_of(followers.begin(), followers.end(), [](const auto &follower)
                                {
                                    return follower.professionId == 63 || follower.professionId == 64;
                                }), "Saved map state resurrected the two removed artisans");
                        }
                        savedVerified = true;
                        capture("continued-game");
                        phase = Phase::Finished;
                    }
                    else if (phase == Phase::Finished && pendingCapture.empty())
                    {
                        require(captures == 3 && doorClicks == 1 && normalFrames >= 30 && savedVerified,
                            "Single prerequisite capture evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the bounded prerequisite flow completed");
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
    try
    {
        if (argc != 5 || (std::string(argv[4]) != "--harbor" && std::string(argv[4]) != "--temple"))
        {
            throw std::runtime_error(
                "prerequisite_capture RUNTIME READONLY_NORMAL132_SAVE NEW_OUTPUT --harbor|--temple");
        }
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3],
            std::string(argv[4]) == "--harbor");
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Prerequisite capture: " << failure.what() << '\n';
        return 1;
    }
}
