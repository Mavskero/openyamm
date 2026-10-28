#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/screens/CutsceneVideoScreen.h"

#include <CommonCrypto/CommonDigest.h>
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
#include <sstream>
#include <stdexcept>
#include <string>
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

std::string fileHash(const std::filesystem::path &path)
{
    const uintmax_t length = std::filesystem::file_size(path);
    require(length > 0 && length <= 64 * 1024 * 1024, "Unexpected source save size");
    std::vector<uint8_t> bytes(size_t(length), 0);
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()))),
        "Cannot read preserved save for SHA256");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), digest.data());
    std::ostringstream result;
    for (unsigned char byte : digest)
    {
        result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    }
    return result.str();
}

void queueKey(SDL_Keycode key, SDL_Scancode scan, bool down)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = key;
    event.key.scancode = scan;
    event.key.down = down;
    require(SDL_PushEvent(&event), "Cannot queue ordinary SDL key");
}

void queueMouse(float x, float y, int width, int height, uint8_t button, bool down)
{
    int count = 0;
    SDL_Window **pWindows = SDL_GetWindows(&count);
    SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
    SDL_free(pWindows);
    require(pWindow != nullptr, "Expected one actual capture window");
    int pointWidth = 0;
    int pointHeight = 0;
    require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot query actual window size");
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(pWindow);
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = 1;
    event.button.x = x * float(pointWidth) / float(width);
    event.button.y = y * float(pointHeight) / float(height);
    require(SDL_PushEvent(&event), "Cannot queue ordinary SDL pointer edge");
}

void queueWorldClick(float x, float y, int width, int height)
{
    // GameInputSystem consumes one mouse edge per gameplay frame.
    queueMouse(x, y, width, height, SDL_BUTTON_RIGHT, true);
    queueMouse(x, y, width, height, SDL_BUTTON_LEFT, true);
    queueMouse(x, y, width, height, SDL_BUTTON_LEFT, false);
    queueMouse(x, y, width, height, SDL_BUTTON_RIGHT, false);
}

bool sameMember(const Character &left, const Character &right)
{
    return left.memberId == right.memberId && left.rosterId == right.rosterId
        && left.characterDataId == right.characterDataId && left.name == right.name
        && left.className == right.className && left.experience == right.experience;
}

void verifyRoster(const Party::Snapshot &before, const Party::Snapshot &after)
{
    require(after.members.size() == before.members.size()
        && after.adventurersInnMembers.size() == before.adventurersInnMembers.size()
        && after.detachedMembers.size() == before.detachedMembers.size()
        && after.nextMemberId == before.nextMemberId
        && after.reusableMercenaryRosterIds == before.reusableMercenaryRosterIds,
        "Submarine travel changed roster membership or member allocation");
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        require(sameMember(before.members[index], after.members[index]), "Active roster identity/order changed");
    }
    for (size_t index = 0; index < before.adventurersInnMembers.size(); ++index)
    {
        require(sameMember(before.adventurersInnMembers[index].character,
            after.adventurersInnMembers[index].character), "Inn roster identity/order changed");
    }
    for (size_t index = 0; index < before.detachedMembers.size(); ++index)
    {
        const auto &left = before.detachedMembers[index];
        const auto &right = after.detachedMembers[index];
        require(left.ownerKey == right.ownerKey && left.slotId == right.slotId
            && sameMember(left.character, right.character), "Detached roster identity/order changed");
    }
    require(after.gold == before.gold && after.bankGold == before.bankGold && after.food == before.food
        && after.fineGold == before.fineGold, "Scripted travel changed Gold, bank, food or fine");
}
}

// One controlled late-quest submarine round trip, driven by original surfaces and ordinary SDL input.
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
            "Use a new NVMe output outside the read-only runtime");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->mapFileName == "d07.blv" && !source->party.members.empty()
            && !source->heldInventoryItemActive && source->party.pendingItemDeliveries.empty()
            && !source->party.questBits.contains(214), "Expected the unchanged normal132 MM8 seed: " + error);
        Party originalParty;
        originalParty.restoreSnapshot(source->party);
        require(!originalParty.hasItemAnywhere(619), "Normal seed already owns submarine key619");
        const std::string sourceHash = fileHash(input);
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot preserve original input");
        require(fileHash(output / "source-input.oysav") == sourceHash, "Input copy hash differs from original");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":186,\"controlled\":true,\"natural_campaign\":false,\"input\":"
            << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(sourceHash)
            << ",\"runtime\":" << std::quoted(runtime.string())
            << ",\"scope\":\"Load an unchanged normal132 MM8 copy; one declared debug-map fixture to d06. "
               "Place exactly one real-table619 into an existing member as a late-quest prerequisite. "
               "Do not set Q214; original physical hatch451 must set it and retain619. "
               "Declare d06 pose7097,-1117,-639; d34 exit pose-28,-193,57 and return hatch pose-2416,1850,-687. "
               "Click original d06 face1625; observe Subcut with real movie delta, then ordinary Escape. "
               "Click original d34 face339/event501 and visible confirmation; record actual out13 landing "
               "before any pose edit. Aim for original out13 model50/face22/event504 at that actual landing. "
               "Only if the real picker has no usable point, declare one near-cave pose7425,-2300,829. "
               "Click visible return confirmation, then d34 face524/event451 and its confirmation. "
               "Final d06 continues30 normal frames and saves. Other framing/UI simulation delta is zero. "
               "No source event dispatch, Q214 fixture, actor/buff/health/immunity edits or skip-movie setting.\","
               "\"key_instance_id_available\":false,\"key_identity\":\"carrier memberId plus original inventory cell\","
               "\"wall_limit_seconds\":90,\"frame_limit\":900,\"transition_wait_frames\":60,"
               "\"useful_image_limit\":4,\"movie_sample_limit\":8,\"new_unit_tests\":0,"
               "\"cold_replays\":0,\"audio\":\"dummy\",\"visual_review_required\":true}\n";

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
        config.appName = "OpenYAMM Submarine186";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        enum class Phase
        {
            Warmup, EntryClicked, Movie, MovieClosing, DockArrival, ExitAim, ExitClicked, ExitConfirmation,
            RegnaArrival, RegnaCapture, RegnaAim, ReturnClicked, ReturnArrival, HatchAim, HatchClicked,
            FinalArrival, Continuing, Finished
        };
        Phase phase = Phase::Warmup;
        unsigned frames = 0;
        unsigned phaseFrame = 0;
        unsigned captures = 0;
        unsigned worldClicks = 0;
        unsigned confirmations = 0;
        unsigned normalFrames = 0;
        unsigned movieFrames = 0;
        unsigned movieSamples = 0;
        unsigned blankMovieSamples = 0;
        unsigned nextMovieSample = 0;
        uint64_t started = 0;
        uint64_t movieStartedAt = 0;
        uint64_t carrierMemberId = 0;
        double initialMinutes = 0;
        double normalSeconds = 0;
        double movieSeconds = 0;
        bool movieStarted = false;
        bool movieCaptured = false;
        bool movieClosed = false;
        bool movieSourceVerified = false;
        bool actualRegnaLanding = false;
        bool regnaCaptureRequested = false;
        bool nearCavePoseUsed = false;
        bool savedVerified = false;
        bool originalUnchanged = false;
        bool complete = false;
        std::string pendingCapture;
        InventoryItem keyFixture;
        Party::Snapshot baseline;
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(12);
        const auto indoor = [&]() -> IndoorSceneRuntime &
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Expected the current actual indoor scene");
            return *pScene;
        };
        const auto outdoor = [&]() -> OutdoorSceneRuntime &
        {
            auto *pScene = dynamic_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Expected the current actual outdoor scene");
            return *pScene;
        };
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto verifyParty = [&](const Party::Snapshot &party)
        {
            verifyRoster(baseline, party);
            require(party.questBits.contains(214) && party.pendingItemDeliveries.empty(),
                "Original submarine use or settled party deliveries are absent");
            size_t keys = 0;
            for (const Character &member : party.members)
            {
                for (const InventoryItem &item : member.inventory)
                {
                    if (item.objectDescriptionId != 619)
                    {
                        continue;
                    }
                    keys += item.quantity;
                    require(member.memberId == carrierMemberId && item.quantity == keyFixture.quantity
                        && item.gridX == keyFixture.gridX && item.gridY == keyFixture.gridY
                        && item.width == keyFixture.width && item.height == keyFixture.height,
                        "Retained key changed carrier, quantity or inventory cell");
                }
            }
            require(keys == 1, "Original submarine event did not retain exactly one619");
        };
        const auto record = [&](const std::string &stage)
        {
            require(game.m_pMapSceneRuntime != nullptr, "Actual map scene is absent");
            const Party &party = game.m_pMapSceneRuntime->party();
            trace << "{\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"phase\":" << int(phase) << ",\"map\":"
                << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"game_minutes\":"
                << game.m_gameSession.gameMinutes() << ",\"Q214\":" << party.hasQuestBit(214)
                << ",\"inventory619\":" << party.inventoryItemCount(619) << ",\"gold\":" << party.gold()
                << ",\"pose\":[";
            if (game.m_pMapSceneRuntime->kind() == SceneKind::Indoor)
            {
                const auto &pose = indoor().partyRuntime().movementState();
                trace << pose.x << ',' << pose.y << ',' << pose.footZ;
            }
            else
            {
                const auto &pose = outdoor().partyRuntime().movementState();
                trace << pose.x << ',' << pose.y << ',' << pose.footZ;
            }
            trace << "],\"members\":[";
            for (size_t index = 0; index < party.members().size(); ++index)
            {
                if (index != 0)
                {
                    trace << ',';
                }
                const Character &member = party.members()[index];
                trace << "{\"member_id\":" << member.memberId << ",\"roster_id\":" << member.rosterId
                    << ",\"name\":" << std::quoted(member.name) << ",\"health\":" << member.health << '}';
            }
            trace << "]}\n";
            trace.flush();
        };
        const auto advance = [&](Phase next)
        {
            phase = next;
            phaseFrame = frames;
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty() && captures < 4, "Overlapping or excessive useful image");
            if (name == "subcut-movie")
            {
                require(++movieSamples <= 8, "Movie exceeded bounded nonblack sampling");
            }
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto aimIndoor = [&](const bx::Vec3 &point)
        {
            const auto &pose = indoor().partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(point.y - pose.y, point.x - pose.x),
                std::atan2(point.z - pose.eyeZ(), std::hypot(point.x - pose.x, point.y - pose.y)));
        };
        const auto indoorClick = [&](size_t faceIndex, uint16_t eventId, int width, int height)
        {
            const auto &selected = game.m_gameDataLoader.getSelectedMap();
            require(selected && selected->indoorMapData, "Original indoor geometry is absent");
            const auto &map = *selected->indoorMapData;
            const auto &face = map.faces.at(faceIndex);
            require(face.cogTriggered == eventId && hasFaceAttribute(face.attributes, FaceAttribute::Clickable),
                "Original clickable indoor binding changed");
            bx::Vec3 center{0, 0, 0};
            for (uint16_t id : face.vertexIndices)
            {
                const auto &vertex = map.vertices.at(id);
                center = bx::add(center, {float(vertex.x), float(vertex.y), float(vertex.z)});
            }
            require(!face.vertexIndices.empty(), "Original face has no vertices");
            center = bx::mul(center, 1.0f / float(face.vertexIndices.size()));
            std::vector<bx::Vec3> points{center};
            for (uint16_t id : face.vertexIndices)
            {
                const auto &vertex = map.vertices.at(id);
                for (float fraction : {0.35f, 0.65f, 0.85f, 0.94f})
                {
                    points.push_back(bx::add(center, bx::mul(
                        bx::sub({float(vertex.x), float(vertex.y), float(vertex.z)}, center), fraction)));
                }
            }
            for (const bx::Vec3 &point : points)
            {
                float x = 0;
                float y = 0;
                if (!game.m_indoorRenderer.projectGameplayWorldPointToScreen(point, width, height, x, y))
                {
                    continue;
                }
                auto &world = indoor().worldRuntime();
                const auto hit = world.pickMouseInteractionTarget(
                    world.buildWorldPickRequest({x, y, width, height, true}));
                trace << "{\"stage\":\"indoor-picker\",\"expected_face\":" << faceIndex
                    << ",\"hit\":" << hit.hasHit << ",\"event\":"
                    << (hit.eventTarget ? hit.eventTarget->triggeredEventId : 0) << ",\"face\":"
                    << (hit.eventTarget ? int64_t(hit.eventTarget->secondaryIndex) : -1) << "}\n";
                if (!hit.eventTarget || hit.eventTarget->targetKind != GameplayWorldEventTargetKind::Surface
                    || hit.eventTarget->secondaryIndex != faceIndex || hit.eventTarget->triggeredEventId != eventId
                    || !world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                {
                    continue;
                }
                trace << "{\"stage\":\"physical-indoor-click\",\"map\":"
                    << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"face\":" << faceIndex
                    << ",\"event\":" << eventId << ",\"pixel\":[" << x << ',' << y
                    << "],\"distance\":" << hit.eventTarget->distance << "}\n";
                queueWorldClick(x, y, width, height);
                ++worldClicks;
                return;
            }
            throw std::runtime_error("Real picker cannot reach original indoor face " + std::to_string(faceIndex));
        };
        const bx::Vec3 caveCenter{7434, -2576, 1074};
        const auto aimCave = [&]()
        {
            const auto &pose = outdoor().partyRuntime().movementState();
            game.m_outdoorGameView.setCameraAngles(std::atan2(caveCenter.y - pose.y, caveCenter.x - pose.x),
                std::atan2(caveCenter.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                    std::hypot(caveCenter.x - pose.x, caveCenter.y - pose.y)));
        };
        const auto cavePoint = [&](int width, int height) -> std::optional<std::array<float, 2>>
        {
            auto &world = outdoor().worldRuntime();
            require(world.mapData() && world.mapData()->bmodels.size() > 50,
                "Original Regna cave model is absent");
            const auto &model = world.mapData()->bmodels.at(50);
            const auto &face = model.faces.at(22);
            require(face.cogTriggeredNumber == 504, "Original cave model50/face22/event504 binding changed");
            std::vector<bx::Vec3> points{caveCenter};
            for (uint16_t id : face.vertexIndices)
            {
                const auto &vertex = model.vertices.at(id);
                for (float fraction : {0.35f, 0.65f, 0.85f})
                {
                    points.push_back(bx::add(caveCenter, bx::mul(
                        bx::sub({float(vertex.x), float(vertex.y), float(vertex.z)}, caveCenter), fraction)));
                }
            }
            const auto camera = world.buildWorldPickRequest({0, 0, width, height, false});
            float viewProjection[16];
            bx::mtxMul(viewProjection, camera.viewMatrix.data(), camera.projectionMatrix.data());
            for (const bx::Vec3 &point : points)
            {
                const bx::Vec3 projected = bx::mulH(point, viewProjection);
                const float x = (projected.x + 1) * float(width) * 0.5f;
                const float y = (1 - projected.y) * float(height) * 0.5f;
                if (x < 0 || y < 0 || x >= width || y >= height)
                {
                    continue;
                }
                const auto hit = world.pickMouseInteractionTarget(
                    world.buildWorldPickRequest({x, y, width, height, true}));
                trace << "{\"stage\":\"outdoor-picker\",\"hit\":" << hit.hasHit
                    << ",\"event\":" << (hit.eventTarget ? hit.eventTarget->triggeredEventId : 0)
                    << ",\"model\":" << (hit.eventTarget ? int64_t(hit.eventTarget->targetIndex) : -1)
                    << ",\"face\":" << (hit.eventTarget ? int64_t(hit.eventTarget->secondaryIndex) : -1) << "}\n";
                if (hit.eventTarget && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                    && hit.eventTarget->targetIndex == 50 && hit.eventTarget->secondaryIndex == 22
                    && hit.eventTarget->triggeredEventId == 504
                    && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                {
                    trace << "{\"stage\":\"usable-original-cave-surface\",\"pixel\":[" << x << ',' << y
                        << "],\"distance\":" << hit.eventTarget->distance << "}\n";
                    return std::array<float, 2>{x, y};
                }
            }
            return std::nullopt;
        };
        const auto confirm = [&](const std::string &destination, uint32_t imageId, uint16_t eventId,
            int width, int height)
        {
            EventRuntimeState *pEvents = game.m_pMapSceneRuntime->eventRuntimeState();
            require(pEvents && pEvents->pendingDialogueContext
                && pEvents->pendingDialogueContext->transitionMapMove,
                "Original surface has not presented its travel confirmation");
            const auto &context = *pEvents->pendingDialogueContext;
            const auto &move = *context.transitionMapMove;
            const auto &dialog = screen().activeEventDialog();
            const std::array<int32_t, 3> arrival = destination == "out13.odm"
                ? std::array<int32_t, 3>{7425, -2538, 829}
                : destination == "d34.blv" ? std::array<int32_t, 3>{-28, -193, 57}
                : std::array<int32_t, 3>{7097, -1117, -639};
            require(dialog.isActive && dialog.presentation == EventDialogPresentation::Transition
                && !dialog.actions.empty() && dialog.actions[0].kind == EventDialogActionKind::MapTransitionConfirm
                && dialog.actions[0].enabled && !dialog.actions[0].label.empty()
                && move.mapName == destination && move.traceEventId == eventId
                && context.transitionImageId == imageId && move.x == arrival[0] && move.y == arrival[1]
                && move.z == arrival[2],
                "Visible original travel confirmation differs from its physical source event");
            const auto *pLayout = screen().findHudLayoutElement("DialogueOkButton");
            require(pLayout != nullptr, "Visible travel OK control is absent");
            const auto rect = screen().resolveHudLayoutElement("DialogueOkButton", width, height,
                pLayout->width, pLayout->height);
            require(rect && rect->width > 0 && rect->height > 0, "Travel OK has no actual hit area");
            const float x = rect->x + rect->width * 0.5f;
            const float y = rect->y + rect->height * 0.5f;
            trace << "{\"stage\":\"visible-travel-confirm\",\"source_map\":"
                << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"event\":" << eventId
                << ",\"destination\":" << std::quoted(destination) << ",\"xyz\":[" << move.x << ','
                << move.y << ',' << move.z << "],\"image_id\":" << imageId << ",\"text_id\":"
                << context.transitionTextId << ",\"label\":" << std::quoted(dialog.actions[0].label)
                << ",\"pixel\":[" << x << ',' << y << "]}\n";
            queueMouse(x, y, width, height, SDL_BUTTON_LEFT, true);
            queueMouse(x, y, width, height, SDL_BUTTON_LEFT, false);
            ++confirmations;
        };
        const auto settled = [&]()
        {
            const EventRuntimeState *pEvents = game.m_pMapSceneRuntime->eventRuntimeState();
            require(pEvents && !pEvents->pendingMapMove && !pEvents->pendingMovie
                && !pEvents->pendingDialogueContext && !game.m_gameSession.pendingMapMove()
                && !screen().activeEventDialog().isActive && !screen().heldInventoryItem().active,
                "Travel left pending map, movie, dialogue or held item state");
        };
        const auto report = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(12)
                << "{\"passed\":" << passed << ",\"checkpoint\":186,\"controlled\":true,"
                   "\"natural_campaign\":false,\"device_acceptance\":false,\"input_sha256\":"
                << std::quoted(sourceHash) << ",\"input_original_unchanged\":" << originalUnchanged
                << ",\"frames\":" << frames << ",\"phase\":" << int(phase) << ",\"captures\":" << captures
                << ",\"physical_world_clicks\":" << worldClicks << ",\"visible_confirmation_clicks\":"
                << confirmations << ",\"key_carrier_member_id\":" << carrierMemberId
                << ",\"key_grid\":[" << unsigned(keyFixture.gridX) << ',' << unsigned(keyFixture.gridY)
                << "],\"key_instance_id_available\":false,\"source_subcut_verified\":" << movieSourceVerified
                << ",\"movie_started\":" << movieStarted << ",\"movie_captured\":" << movieCaptured
                << ",\"movie_closed_by_escape\":" << movieClosed << ",\"movie_frames\":" << movieFrames
                << ",\"movie_seconds\":" << movieSeconds << ",\"movie_samples\":" << movieSamples
                << ",\"blank_movie_samples_discarded\":" << blankMovieSamples
                << ",\"actual_regna_landing_observed\":" << actualRegnaLanding
                << ",\"near_cave_pose_used\":" << nearCavePoseUsed << ",\"normal_frames\":" << normalFrames
                << ",\"normal_seconds\":" << normalSeconds << ",\"final_save_verified\":" << savedVerified
                << ",\"new_unit_tests\":0,\"cold_replays\":0,\"visual_review_required\":true,\"failure\":"
                << std::quoted(failure) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "source-input.oysav"),
                        "Cannot load the unchanged normal132 MM8 copy");
                    const MapStatsEntry *pMap = game.m_gameDataLoader.getMapStats().findByFileName("d06.blv");
                    require(pMap != nullptr, "Original d06 is absent from production MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump()
                        && game.m_gameSession.currentMapFileName() == "d06.blv", "Declared d06 map fixture failed");
                    Party &party = indoor().party();
                    verifyRoster(source->party, party.snapshot());
                    require(!party.hasQuestBit(214) && !party.hasItemAnywhere(619),
                        "Declared map fixture changed the initial submarine prerequisites");
                    const ItemTable &items = game.m_gameDataLoader.getItemTable();
                    require(items.get(619) != nullptr, "Original submarine key619 is absent from the real item table");
                    const InventoryItem key = ItemGenerator::makeInventoryItem(619, items, ItemGenerationMode::Generic);
                    for (size_t index = 0; index < party.memberCount(); ++index)
                    {
                        if (party.tryAutoPlaceItemInMemberInventory(index, key))
                        {
                            const Character &member = *party.member(index);
                            const auto found = std::find_if(member.inventory.begin(), member.inventory.end(),
                                [](const InventoryItem &item) { return item.objectDescriptionId == 619; });
                            require(found != member.inventory.end(), "Placed key has no real inventory cell");
                            carrierMemberId = member.memberId;
                            keyFixture = *found;
                            break;
                        }
                    }
                    require(carrierMemberId && party.inventoryItemCount(619) == 1 && !party.hasQuestBit(214),
                        "Cannot prepare exactly one real-table619 without changing Q214");
                    baseline = party.snapshot();
                    initialMinutes = game.m_gameSession.gameMinutes();
                    const auto &program = indoor().localEventProgram();
                    require(program && program->luaSourceText(), "Actual d06 Lua program is absent");
                    const std::string &lua = *program->luaSourceText();
                    require(lua.find(R"(evt.ShowMovie("\"Subcut\" ", true))") != std::string::npos
                        && lua.find("HasItem(619) or IsQBitSet(QBit(214))") != std::string::npos,
                        "Current program lacks the authenticated original submarine gate/movie overlay");
                    movieSourceVerified = true;
                    settled();
                    indoor().partyRuntime().teleportPartyPosition(7097, -1117, -639);
                    aimIndoor({7100, -736, -576});
                    record("declared-d06-hatch-pose-and-key619");
                    require(game.quickSaveToPath(output / "declared-fixture.oysav", "186潜艇受控前置"),
                        "Cannot save declared submarine fixture");
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 900 && SDL_GetTicks() - started < 90000,
                        "Submarine journey exceeded its finite App bound");
                    const bool movieBefore = dynamic_cast<CutsceneVideoScreen *>(
                        game.m_screenManager.activeScreen()) != nullptr;
                    const bool normal = phase == Phase::Continuing;
                    game.renderFrame(width, height, wheel, movieBefore || normal ? delta : 0.0f);
                    if (movieBefore)
                    {
                        ++movieFrames;
                        movieSeconds += delta;
                    }
                    if (normal && delta > 0)
                    {
                        ++normalFrames;
                        normalSeconds += delta;
                    }
                    auto *pMovie = dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen());
                    require(game.m_screenManager.activeScreen() == nullptr || (pMovie
                        && (phase == Phase::EntryClicked || phase == Phase::Movie || phase == Phase::MovieClosing)),
                        "Unexpected full-screen interruption");
                    require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->party().hasActableMember(),
                        "Submarine gameplay has no actable member");
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            const bool isMovie = pendingCapture == "subcut-movie";
                            size_t nonblack = 0;
                            if (isMovie)
                            {
                                require(pMovie != nullptr, "Movie closed before its actual screenshot was collected");
                                for (size_t pixel = 0; pixel + 3 < image->bgraPixels.size(); pixel += 4)
                                {
                                    if (image->bgraPixels[pixel] > 16 || image->bgraPixels[pixel + 1] > 16
                                        || image->bgraPixels[pixel + 2] > 16)
                                    {
                                        ++nonblack;
                                    }
                                }
                            }
                            if (isMovie && nonblack < size_t(image->width) * image->height / 100)
                            {
                                ++blankMovieSamples;
                                trace << "{\"stage\":\"blank-subcut-sample-discarded\",\"frame\":" << frames
                                    << ",\"nonblack_pixels\":" << nonblack << "}\n";
                                pendingCapture.clear();
                                nextMovieSample = frames + 15;
                            }
                            else
                            {
                                SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                    SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                                require(pSurface != nullptr, "Cannot wrap actual Metal pixels");
                                const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                                SDL_DestroySurface(pSurface);
                                require(saved, "Cannot save actual Metal image");
                                trace << "{\"stage\":\"capture-saved\",\"capture\":" << std::quoted(pendingCapture)
                                    << ",\"frame\":" << frames << ",\"nonblack_pixels\":" << nonblack << "}\n";
                                if (isMovie)
                                {
                                    movieCaptured = true;
                                }
                                pendingCapture.clear();
                                ++captures;
                            }
                        }
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        indoorClick(1625, 451, width, height);
                        advance(Phase::EntryClicked);
                    }
                    else if (phase == Phase::EntryClicked)
                    {
                        if (pMovie)
                        {
                            verifyParty(indoor().party().snapshot());
                            movieStarted = true;
                            movieStartedAt = SDL_GetTicks();
                            nextMovieSample = frames + 45;
                            record("original451-setQ214-retain619-subcut-started");
                            advance(Phase::Movie);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Original physical hatch did not start Subcut");
                        }
                    }
                    else if (phase == Phase::Movie)
                    {
                        require(pMovie && SDL_GetTicks() - movieStartedAt < 10000,
                            "Subcut ended or exceeded its bounded visual sample interval");
                        if (movieCaptured)
                        {
                            queueKey(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, true);
                            queueKey(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, false);
                            record("ordinary-subcut-escape-queued");
                            advance(Phase::MovieClosing);
                        }
                        else if (pendingCapture.empty() && frames >= nextMovieSample)
                        {
                            capture("subcut-movie");
                        }
                    }
                    else if (phase == Phase::MovieClosing)
                    {
                        if (!pMovie)
                        {
                            movieClosed = true;
                            record("subcut-closed-by-ordinary-escape");
                            advance(Phase::DockArrival);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Ordinary Escape did not close Subcut");
                        }
                    }
                    else if (phase == Phase::DockArrival)
                    {
                        if (game.m_gameSession.currentMapFileName() == "d34.blv")
                        {
                            settled();
                            verifyParty(indoor().party().snapshot());
                            const auto &pose = indoor().partyRuntime().movementState();
                            require(std::abs(pose.x + 2416) < 1 && std::abs(pose.y - 1850) < 1
                                && std::abs(pose.footZ + 687) <= 2,
                                "Actual submarine arrival differs from its original d34 landing");
                            record("actual-submarine-d34-arrival-before-pose-fixture");
                            indoor().partyRuntime().teleportPartyPosition(-28, -193, 57);
                            aimIndoor({96, -186, 298});
                            record("declared-d34-exit-pose");
                            advance(Phase::ExitAim);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Movie return did not complete the original d34 move");
                        }
                    }
                    else if (phase == Phase::ExitAim && frames - phaseFrame >= 6)
                    {
                        indoorClick(339, 501, width, height);
                        advance(Phase::ExitClicked);
                    }
                    else if (phase == Phase::ExitClicked)
                    {
                        if (screen().activeEventDialog().isActive && frames - phaseFrame >= 6)
                        {
                            record("original-d34-exit-confirmation-visible");
                            capture("original-exit-confirmation");
                            advance(Phase::ExitConfirmation);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Physical d34 exit501 did not present confirmation");
                        }
                    }
                    else if (phase == Phase::ExitConfirmation && pendingCapture.empty())
                    {
                        confirm("out13.odm", 1, 501, width, height);
                        advance(Phase::RegnaArrival);
                    }
                    else if (phase == Phase::RegnaArrival)
                    {
                        if (game.m_gameSession.currentMapFileName() == "out13.odm")
                        {
                            settled();
                            verifyParty(outdoor().party().snapshot());
                            const auto &pose = outdoor().partyRuntime().movementState();
                            require(std::abs(pose.x - 7425) < 1 && std::abs(pose.y + 2538) < 1
                                && std::abs(pose.footZ - 829) <= 8,
                                "Actual original exit differs from its Regna landing");
                            actualRegnaLanding = true;
                            record("actual-Regna-landing-before-any-outdoor-pose-fixture");
                            advance(Phase::RegnaCapture);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Visible exit confirmation did not load Regna");
                        }
                    }
                    else if (phase == Phase::RegnaCapture && frames - phaseFrame >= 6)
                    {
                        if (!regnaCaptureRequested)
                        {
                            // Cross-map loading submits its own frames; collect stable destination pixels.
                            capture("actual-Regna-landing");
                            regnaCaptureRequested = true;
                        }
                        else if (pendingCapture.empty())
                        {
                            aimCave();
                            advance(Phase::RegnaAim);
                        }
                    }
                    else if (phase == Phase::RegnaAim && frames - phaseFrame >= 6)
                    {
                        const auto point = cavePoint(width, height);
                        if (!point)
                        {
                            require(!nearCavePoseUsed, "Original Regna cave504 is unreachable at declared near pose");
                            nearCavePoseUsed = true;
                            outdoor().partyRuntime().teleportTo(7425, -2300, 829);
                            aimCave();
                            record("declared-essential-near-cave-pose-after-no-usable-arrival-pick");
                            phaseFrame = frames;
                        }
                        else
                        {
                            queueWorldClick((*point)[0], (*point)[1], width, height);
                            ++worldClicks;
                            record("physical-out13-model50-face22-event504-click-queued");
                            advance(Phase::ReturnClicked);
                        }
                    }
                    else if (phase == Phase::ReturnClicked)
                    {
                        if (screen().activeEventDialog().isActive && frames - phaseFrame >= 6)
                        {
                            confirm("d34.blv", 3, 504, width, height);
                            advance(Phase::ReturnArrival);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Physical Regna cave504 did not present confirmation");
                        }
                    }
                    else if (phase == Phase::ReturnArrival)
                    {
                        if (game.m_gameSession.currentMapFileName() == "d34.blv")
                        {
                            settled();
                            verifyParty(indoor().party().snapshot());
                            const auto &pose = indoor().partyRuntime().movementState();
                            require(std::abs(pose.x + 28) < 1 && std::abs(pose.y + 193) < 1
                                && std::abs(pose.footZ - 57) <= 2,
                                "Original Regna return differs from its d34 exit landing");
                            record("actual-Regna-return-d34-before-pose-fixture");
                            indoor().partyRuntime().teleportPartyPosition(-2416, 1850, -687);
                            aimIndoor({-2416, 2050, -680});
                            record("declared-d34-return-hatch-pose");
                            advance(Phase::HatchAim);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Visible cave confirmation did not return to d34");
                        }
                    }
                    else if (phase == Phase::HatchAim && frames - phaseFrame >= 6)
                    {
                        indoorClick(524, 451, width, height);
                        advance(Phase::HatchClicked);
                    }
                    else if (phase == Phase::HatchClicked)
                    {
                        if (screen().activeEventDialog().isActive && frames - phaseFrame >= 6)
                        {
                            confirm("d06.blv", 1, 451, width, height);
                            advance(Phase::FinalArrival);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Physical d34 hatch451 did not present confirmation");
                        }
                    }
                    else if (phase == Phase::FinalArrival)
                    {
                        if (game.m_gameSession.currentMapFileName() == "d06.blv")
                        {
                            settled();
                            verifyParty(indoor().party().snapshot());
                            const auto &pose = indoor().partyRuntime().movementState();
                            require(std::abs(pose.x - 7097) < 1 && std::abs(pose.y + 1117) < 1
                                && std::abs(pose.footZ + 639) <= 2,
                                "Original submarine return differs from its d06 landing");
                            record("actual-submarine-d06-return");
                            advance(Phase::Continuing);
                        }
                        else
                        {
                            require(frames - phaseFrame <= 60, "Visible hatch confirmation did not return to d06");
                        }
                    }
                    else if (phase == Phase::Continuing && normalFrames >= 30 && pendingCapture.empty())
                    {
                        settled();
                        verifyParty(indoor().party().snapshot());
                        require(game.m_gameSession.gameMinutes() >= initialMinutes
                            && game.m_gameSession.gameMinutes() - initialMinutes < 60,
                            "Ordinary scripted round trip advanced an unexpected game-time interval");
                        const auto livePose = indoor().partyRuntime().movementState();
                        const auto finalPath = output / "submarine-journey-complete.oysav";
                        require(game.quickSaveToPath(finalPath, "186潜艇雷格那往返后继续游戏"), "Cannot save final play");
                        const auto saved = loadGameDataFromPath(finalPath, error);
                        require(saved && saved->mapFileName == "d06.blv" && saved->hasIndoorSceneState
                            && saved->currentSceneKind == SceneKind::Indoor && !saved->heldInventoryItemActive
                            && saved->indoorSceneStates.contains("d34.blv")
                            && saved->outdoorWorldStates.contains("out13.odm"),
                            "Final save lacks ordinary current/cached journey maps: " + error);
                        verifyParty(saved->party);
                        const auto &savedPose = saved->indoorScene.partyRuntime.movementState;
                        require(savedPose.x == livePose.x && savedPose.y == livePose.y
                            && savedPose.footZ == livePose.footZ && saved->indoorScene.eventRuntimeState
                            && !saved->indoorScene.eventRuntimeState->pendingMovie
                            && !saved->indoorScene.eventRuntimeState->pendingMapMove
                            && !saved->indoorScene.eventRuntimeState->pendingDialogueContext
                            && std::abs(saved->savedGameMinutes - game.m_gameSession.gameMinutes()) < 0.000001,
                            "Final save changed actual return pose, time or settled event state");
                        originalUnchanged = fileHash(input) == sourceHash
                            && fileHash(output / "source-input.oysav") == sourceHash;
                        require(originalUnchanged, "Original or preserved seed changed during the App flow");
                        savedVerified = true;
                        record("final30-normal-frames-and-save-verified");
                        capture("continued-d06-game");
                        advance(Phase::Finished);
                    }
                    else if (phase == Phase::Finished && pendingCapture.empty())
                    {
                        require(worldClicks == 4 && confirmations == 3 && captures == 4
                            && movieSourceVerified && movieStarted && movieCaptured && movieClosed
                            && actualRegnaLanding && normalFrames >= 30 && savedVerified && originalUnchanged,
                            "Single submarine journey evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue ordinary App exit");
                    }
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the bounded submarine journey completed");
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
    if (argc != 4)
    {
        std::cerr << "submarine_journey_capture RUNTIME READONLY_MM8_SAVE NEW_OUTPUT\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Submarine journey capture: " << failure.what() << '\n';
        return 1;
    }
}
