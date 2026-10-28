#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/StringUtils.h"
#include "game/app/GameApplication.h"
#include "game/events/OutdoorLightTimeOverride.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/outdoor/OutdoorMechanismRuntime.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <CommonCrypto/CommonDigest.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr float FrameSeconds = 1.0f / 60.0f;
constexpr std::array<uint32_t, 3> TimeEvents = {101, 102, 103};
constexpr std::array<size_t, 3> TimeModels = {479, 318, 155};
constexpr const char *DayStateKey = "MMerge.BrAlvar.DayState";
constexpr const char *DaylightVersionKey = "MMerge.BrAlvar.DaylightVersion";

void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

std::string bytesDigest(const std::vector<uint8_t> &bytes)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash{};
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), hash.data());
    std::ostringstream result;
    for (unsigned char byte : hash) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string fileDigest(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot hash read-only input");
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    std::array<char, 65536> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
        CC_SHA256_Update(&context, buffer.data(), CC_LONG(input.gcount()));
    require(input.eof(), "Cannot finish input hash");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash{};
    CC_SHA256_Final(hash.data(), &context);
    std::ostringstream result;
    for (unsigned char byte : hash) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string itemDigest(const Character &member)
{
    Party::Snapshot comparison;
    Character carrier;
    carrier.inventory = member.inventory;
    carrier.equipment = member.equipment;
    carrier.equipmentRuntime = member.equipmentRuntime;
    comparison.members.push_back(std::move(carrier));
    return bytesDigest(encodePartySnapshotForComparison(comparison));
}

std::map<uint64_t, Character> ownedMembers(const Party::Snapshot &party)
{
    std::map<uint64_t, Character> members;
    const auto append = [&](const Character &member)
    {
        require(member.memberId != 0 && members.emplace(member.memberId, member).second,
            "Duplicate or absent Party member identity");
    };
    for (const Character &member : party.members) append(member);
    for (const auto &member : party.detachedMembers) append(member.character);
    for (const auto &member : party.adventurersInnMembers) append(member.character);
    return members;
}

void checkPartyCustody(const Party::Snapshot &before, const Party::Snapshot &after)
{
    require(before.members.size() == after.members.size()
        && before.detachedMembers.size() == after.detachedMembers.size()
        && before.adventurersInnMembers.size() == after.adventurersInnMembers.size()
        && before.nextMemberId == after.nextMemberId && before.questBits == after.questBits
        && before.gold == after.gold && before.bankGold == after.bankGold
        && before.food == after.food && before.fineGold == after.fineGold,
        "Party custody, quest bits or resources changed during daylight observation");
    require(before.pendingItemDeliveries.empty() && after.pendingItemDeliveries.empty(),
        "Use the accepted input without pending item deliveries");
    for (size_t index = 0; index < before.members.size(); ++index)
        require(before.members[index].memberId == after.members[index].memberId, "Active member ordering changed");
    const auto first = ownedMembers(before);
    const auto last = ownedMembers(after);
    require(first.size() == last.size(), "Owned character set changed");
    for (const auto &[id, member] : first)
    {
        const auto found = last.find(id);
        require(found != last.end(), "An owned character disappeared");
        const Character &actual = found->second;
        require(std::tie(member.rosterId, member.characterDataId, member.name, member.level, member.experience)
            == std::tie(actual.rosterId, actual.characterDataId, actual.name, actual.level, actual.experience),
            "Character identity, level or XP changed");
        require(itemDigest(member) == itemDigest(actual), "Owned inventory or equipment changed");
    }
    // HP/SP, conditions, recovery and buff durations remain under normal gameplay ownership.
}

int32_t mapValue(const EventRuntimeState &events, const char *key)
{
    const auto found = events.namedMapVars.find(key);
    return found == events.namedMapVars.end() ? 0 : found->second;
}

bool equalMotion(const RuntimeMechanismState &a, const RuntimeMechanismState &b)
{
    return std::tie(a.state, a.timeSinceTriggeredMs, a.currentDistance, a.rotationDirection, a.isMoving)
        == std::tie(b.state, b.timeSinceTriggeredMs, b.currentDistance, b.rotationDirection, b.isMoving);
}

struct VertexEvidence
{
    size_t face = 0;
    size_t vertex = 0;
    bx::Vec3 raw = {0.0f, 0.0f, 0.0f};
    bx::Vec3 live = {0.0f, 0.0f, 0.0f};
};

struct GeometryEvidence
{
    float minimumZ = std::numeric_limits<float>::max();
    float maximumZ = std::numeric_limits<float>::lowest();
    float minimumDeltaZ = std::numeric_limits<float>::max();
    float maximumDeltaZ = std::numeric_limits<float>::lowest();
    std::vector<VertexEvidence> vertices;
};

GeometryEvidence geometryEvidence(const OutdoorWorldRuntime &world, size_t modelIndex)
{
    const OutdoorMapData *pMap = world.mapData();
    require(pMap && modelIndex < pMap->bmodels.size(), "Missing original Time model");
    const OutdoorBModel &model = pMap->bmodels[modelIndex];
    std::vector<size_t> candidates;
    world.collectOutdoorFaceCandidates(1740.0f, -270.0f, 1820.0f, -70.0f, candidates);
    GeometryEvidence evidence;
    for (size_t index : candidates)
    {
        const OutdoorFaceGeometryData *pFace = world.outdoorFace(index);
        if (!pFace || pFace->bModelIndex != modelIndex) continue;
        require(pFace->faceIndex < model.faces.size(), "Live Time face lost its original face index");
        const OutdoorBModelFace &rawFace = model.faces[pFace->faceIndex];
        require(rawFace.vertexIndices.size() == pFace->vertices.size(), "Live Time face topology changed");
        for (size_t vertex = 0; vertex < pFace->vertices.size(); ++vertex)
        {
            require(rawFace.vertexIndices[vertex] < model.vertices.size(), "Invalid original Time vertex index");
            const bx::Vec3 raw = outdoorBModelVertexToWorld(model.vertices[rawFace.vertexIndices[vertex]]);
            const bx::Vec3 live = pFace->vertices[vertex];
            require(std::abs(live.x - raw.x) < 0.01f && std::abs(live.y - raw.y) < 0.01f,
                "Time model moved outside its original vertical motion");
            evidence.minimumZ = std::min(evidence.minimumZ, live.z);
            evidence.maximumZ = std::max(evidence.maximumZ, live.z);
            evidence.minimumDeltaZ = std::min(evidence.minimumDeltaZ, live.z - raw.z);
            evidence.maximumDeltaZ = std::max(evidence.maximumDeltaZ, live.z - raw.z);
            evidence.vertices.push_back({pFace->faceIndex, vertex, raw, live});
        }
    }
    require(!evidence.vertices.empty(), "Actual Time model geometry is absent from the live face index");
    return evidence;
}

void checkGeometry(const GeometryEvidence &geometry, float fraction, bool moving = false)
{
    // Collision vertices are integral and refreshed at30 Hz; settled endpoints remain exact.
    const float tolerance = moving ? 0.5f + 10.0f / 2.0f / 30.0f + 0.01f : 0.025f;
    require(std::abs(geometry.minimumDeltaZ + 10.0f * fraction) < tolerance
        && std::abs(geometry.maximumDeltaZ + 10.0f * fraction) < tolerance,
        "Actual transformed Time vertices disagree with the source dz=-10 motion");
}

void writeOverride(std::ostream &out, const std::optional<OutdoorLightTimeOverride> &value)
{
    if (!value)
    {
        out << "null";
        return;
    }
    out << "{\"hour\":" << value->hour << ",\"minute\":";
    if (value->minute) out << *value->minute;
    else out << "null";
    out << '}';
}

void checkDerivedLight(const OutdoorWorldRuntime &world, const EventRuntimeState &events)
{
    require(events.outdoorLightTimeOverride && validOutdoorLightTimeOverride(*events.outdoorLightTimeOverride),
        "Source typed light-time override is absent or invalid");
    const double expected = projectedOutdoorLightGameMinutes(world.gameMinutes(), events.outdoorLightTimeOverride);
    require(std::abs(world.lightGameMinutes() - expected) < 0.00001
        && std::abs(world.atmosphereState().lightMinutesOfDay - std::fmod(expected, 1440.0)) < 0.0001,
        "The actual atmosphere consumer did not project the source light time");
}

bool differentLight(const OutdoorWorldRuntime::AtmosphereState &a, const OutdoorWorldRuntime::AtmosphereState &b)
{
    return std::abs(a.ambientBrightness - b.ambientBrightness) > 0.001f
        || std::abs(a.darknessOverlayAlpha - b.darknessOverlayAlpha) > 0.001f
        || std::abs(a.sunDirectionX - b.sunDirectionX) > 0.001f
        || std::abs(a.sunDirectionZ - b.sunDirectionZ) > 0.001f;
}
}

struct GameApplicationTestAccess
{
    static int runLoot(std::filesystem::path root, std::filesystem::path input, std::filesystem::path output)
    {
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(root / "worlds/mmmerge.zip")
            && !std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/"),
            "Loot mode requires a complete runtime and new NVMe output directory");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->hasOutdoorRuntimeState && source->outdoorWorld.eventRuntimeState
            && toLowerCopy(source->mapFileName) == "bralvar.odm" && !source->party.members.empty(),
            "Loot input must be a prior actual BrAlvar arrival: " + error);
        const std::string inputHash = fileDigest(input);
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":176,\"mode\":\"loot\",\"controlled\":true,\"natural_campaign\":false,\"input\":"
            << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string()) << ",\"changes\":\"Member0 selected; RepairItem "
               "removed, conditions cleared, HP=maxHP and recovery0 once. No inventory reward/lock fixture. "
               "Existing held/pending items use normal inventory delivery and remain in the counted custody. "
               "One pose near actual decoration702/event30, then original model11/face18/event82 at "
               "(-258,-700,1992), with aimed cameras. One eventRNG=1 fixture before82 gives level3/type17. "
               "Three ordinary SDL clicks:30 once,82 twice without moving. No direct event dispatch, "
               "actor/AI edits, buff injection or old-route replay. Inspection, SDL inputs and captures use "
               "renderFrame delta0; normal AI is enabled but does not advance during that phase. Only the final "
               "30frames advance ordinary gameplay/AI at1/60 second, then write an explicit new save.\","
               "\"event_rng_fixture\":1,\"expected_treasure_level\":3,\"expected_item_type\":17,"
               "\"normal_ai\":true,\"new_unit_cases\":0,\"cold_replays\":0,\"visual_review_required\":true}\n";
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = settings.immortal = settings.unlimitedMana = settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM BrAlvar loot176";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(17);
        enum class Phase { Warmup, TrashReady, TrashClicked, LibraryPose, LibraryReady, LibraryClicked,
            RepeatReady, RepeatClicked, Continue, Done };
        Phase phase = Phase::Warmup;
        std::string stage = "initialize", pendingCapture, libraryKey;
        unsigned frames = 0, phaseFrame = 0, normalFrames = 0, captures = 0, clicks = 0;
        uint64_t started = 0;
        uint32_t bookId = 0, libraryRng = 0;
        bool complete = false, savedVerified = false, repeatedVerified = false;
        double simulationSeconds = 0;
        std::map<uint32_t, uint64_t> baseline, afterTrash, afterLibrary;
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm", "Loot left BrAlvar");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto events = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto itemCounts = [](const Party::Snapshot &party, bool held, const InventoryItem &heldItem)
        {
            std::map<uint32_t, uint64_t> counts;
            const auto add = [&](const InventoryItem &item)
                { if (item.objectDescriptionId) counts[item.objectDescriptionId] += std::max(1u, item.quantity); };
            for (const Character &member : party.members) for (const InventoryItem &item : member.inventory) add(item);
            for (const auto &delivery : party.pendingItemDeliveries) add(delivery.item);
            if (held) add(heldItem);
            return counts;
        };
        const auto liveCounts = [&]()
            { return itemCounts(scene().party().snapshot(), screen().heldInventoryItem().active,
                screen().heldInventoryItem().item); };
        const auto record = [&](const std::string &label)
        {
            const auto &pose = scene().partyRuntime().movementState();
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"xyz\":[" << pose.x << ',' << pose.y << ',' << pose.footZ
                << "],\"support\":[" << int(pose.supportKind) << ',' << pose.supportBModelIndex << ','
                << pose.supportFaceIndex << "],\"event_rng\":" << events().eventRandomState
                << ",\"library_key\":" << std::quoted(libraryKey) << ",\"library_lock\":"
                << (libraryKey.empty() ? 0 : mapValue(events(), libraryKey.c_str())) << ",\"disease_medium\":"
                << scene().party().members().at(0).conditions.test(size_t(CharacterCondition::DiseaseMedium))
                << ",\"member0_hp\":" << scene().party().members().at(0).health
                << ",\"member0_repair_present\":" << scene().party().members().at(0).skills.contains("RepairItem")
                << ",\"held_item\":" << (screen().heldInventoryItem().active
                    ? screen().heldInventoryItem().item.objectDescriptionId : 0)
                << ",\"pending_deliveries\":" << scene().party().pendingItemDeliveries().size()
                << ",\"actor_count\":" << scene().worldRuntime().mapActorCount() << ",\"items\":[";
            bool comma = false;
            for (const auto &[id, count] : liveCounts())
            {
                if (comma) trace << ',';
                comma = true;
                trace << '[' << id << ',' << count << ']';
            }
            trace << "]}\n";
            trace.flush();
        };
        const auto stowHeld = [&]()
        {
            if (!screen().heldInventoryItem().active) return;
            for (size_t member = 0; member < scene().party().members().size(); ++member)
                if (GameplayHeldItemController::tryAutoPlaceHeldInventoryItemOnPartyMember(
                        screen().heldInventoryItem(), scene().party(), member, error)) return;
            throw std::runtime_error("No inventory space for existing held item; do not drop or overwrite it");
        };
        const auto flushDelivery = [&]()
        {
            const auto before = liveCounts();
            stowHeld();
            GameplayHeldItemController::applyPendingQuestItemDeliveries(screen());
            stowHeld();
            require(scene().party().pendingItemDeliveries().empty() && liveCounts() == before,
                "Normal delivery lost custody or could not finish; no reward may be discarded");
        };
        const auto pose = [&](bool library)
        {
            record("before-declared-pose");
            const auto *pMap = scene().worldRuntime().mapData();
            require(pMap && pMap->entities.size() > 702 && pMap->bmodels.size() > 11,
                "Original loot geometry is missing");
            bx::Vec3 focus = {0.0f, 0.0f, 0.0f};
            if (library)
            {
                require(pMap->bmodels[11].faces.at(18).cogTriggeredNumber == 82,
                    "Original library model11/face18 changed");
                scene().partyRuntime().teleportTo(-258, -700, 1992);
                focus = {-258.5f, -889.0f, 2184.0f};
                require(!scene().partyRuntime().movementState().airborne,
                    "Original library pose has no supporting platform; stop without a click");
            }
            else
            {
                const auto &entity = pMap->entities[702];
                require(entity.scriptEventId() == 30 && entity.x == -6944 && entity.y == -13232,
                    "Original trash decoration702 changed");
                scene().partyRuntime().teleportTo(float(entity.x), float(entity.y + 192), float(entity.z + 1));
                focus = {float(entity.x), float(entity.y), float(entity.z + 32)};
            }
            const auto &position = scene().partyRuntime().movementState();
            game.m_outdoorGameView.setCameraAngles(std::atan2(focus.y - position.y, focus.x - position.x),
                std::atan2(focus.z - position.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                    std::hypot(focus.x - position.x, focus.y - position.y)));
            record(library ? "declared-library-pose" : "declared-trash-pose");
        };
        const auto click = [&](bool library, int width, int height)
        {
            auto &world = scene().worldRuntime();
            std::vector<bx::Vec3> points;
            if (library)
            {
                points.push_back({-258.5f, -889, 2184});
                const auto &model = world.mapData()->bmodels[11];
                for (uint16_t index : model.faces[18].vertexIndices)
                    points.push_back(bx::add(points[0], bx::mul(bx::sub(
                        outdoorBModelVertexToWorld(model.vertices[index]), points[0]), 0.65f)));
            }
            else for (float z : {16.0f, 32.0f, 48.0f, 64.0f}) points.push_back({-6944, -13232, 141 + z});
            const auto camera = world.buildWorldPickRequest({0, 0, width, height, false});
            float matrix[16];
            bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
            std::optional<std::array<float, 2>> pixel;
            for (const auto &point : points)
            {
                const auto projected = bx::mulH(point, matrix);
                const float x = (projected.x + 1) * width * 0.5f, y = (1 - projected.y) * height * 0.5f;
                if (x < 0 || y < 0 || x >= width || y >= height) continue;
                const auto hit = world.pickMouseInteractionTarget(
                    world.buildWorldPickRequest({x, y, width, height, true}));
                const uint16_t sourceEvent = hit.eventTarget
                    ? (library ? hit.eventTarget->triggeredEventId : hit.eventTarget->eventIdSecondary) : 0;
                const bool accepted = hit.eventTarget && sourceEvent == (library ? 82 : 30)
                    && (library ? hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                        && hit.eventTarget->targetIndex == 11 && hit.eventTarget->secondaryIndex == 18
                        : (hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Decoration
                            || hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Entity)
                            && hit.eventTarget->targetIndex == 702)
                    && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse);
                trace << "{\"event\":\"typed-pointer-probe\",\"source_event\":" << (library ? 82 : 30)
                    << ",\"pixel\":[" << x << ',' << y << "],\"accepted\":" << accepted
                    << ",\"hit_kind\":" << int(hit.kind) << ",\"hit_event\":"
                    << sourceEvent << "}\n";
                if (accepted) { pixel = std::array<float, 2>{x, y}; break; }
            }
            require(pixel.has_value(), "Original loot geometry is not reachable by actual picker; stop without retry");
            int count = 0;
            SDL_Window **pWindows = SDL_GetWindows(&count);
            SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
            SDL_free(pWindows);
            int pointWidth = 0, pointHeight = 0;
            require(pWindow && SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot find capture window");
            const auto edge = [&](uint8_t button, bool down)
            {
                SDL_Event event{};
                event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID = SDL_GetWindowID(pWindow);
                event.button.button = button;
                event.button.down = down;
                event.button.clicks = 1;
                event.button.x = (*pixel)[0] * float(pointWidth) / width;
                event.button.y = (*pixel)[1] * float(pointHeight) / height;
                require(SDL_PushEvent(&event), "Cannot queue ordinary SDL click");
            };
            edge(SDL_BUTTON_RIGHT, true); edge(SDL_BUTTON_LEFT, true);
            edge(SDL_BUTTON_LEFT, false); edge(SDL_BUTTON_RIGHT, false);
            ++clicks;
            phaseFrame = frames;
            record(library ? "queued-real82-click" : "queued-real30-click");
        };
        const auto capture = [&](const std::string &label)
        {
            require(pendingCapture.empty(), "Overlapping loot captures");
            pendingCapture = label;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, label.c_str());
        };
        const auto result = [&](bool passed, const std::string &message)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(17)
                << "{\"status\":" << std::quoted(passed ? "passed" : "failed")
                << ",\"checkpoint\":176,\"mode\":\"loot\",\"controlled\":true,\"natural_campaign\":false,\"stage\":"
                << std::quoted(stage) << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"SDL_clicks\":" << clicks << ",\"normal_frames\":" << normalFrames
                << ",\"simulation_seconds\":" << simulationSeconds << ",\"book_id\":" << bookId
                << ",\"library_key\":" << std::quoted(libraryKey) << ",\"repeat_no_reward_or_rng\":"
                << repeatedVerified << ",\"save_verified\":" << savedVerified
                << ",\"normal_ai\":true,\"inspection_input_delta_seconds\":0,"
                   "\"AI_advances_only_in_final_continuation\":true,\"continuation_delta_seconds\":" << FrameSeconds
                << ",\"new_unit_cases\":0,\"cold_replays\":0,\"error\":"
                << std::quoted(message) << ",\"input_sha256_before\":" << std::quoted(inputHash)
                << ",\"input_sha256_after\":" << std::quoted(fileDigest(input)) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal
                        && game.initializeRenderer() && game.loadSessionFromPath(input), "Actual loot input load failed");
                    screen().closeActiveEventDialog();
                    require(scene().localEventProgram() && scene().localEventProgram()->hasEvent(30)
                        && scene().localEventProgram()->hasEvent(82), "Fresh source30/82 handlers are absent");
                    record("loaded-input-before-fixtures");
                    auto candidate = scene().party().snapshot();
                    candidate.activeMemberIndex = 0;
                    auto &member = candidate.members[0];
                    member.skills.erase("RepairItem");
                    member.conditions.reset();
                    member.conditionStartGameMinutes.fill(0);
                    member.health = member.maxHealth;
                    member.recoverySecondsRemaining = 0;
                    scene().party().restoreSnapshot(candidate);
                    flushDelivery();
                    baseline = liveCounts();
                    pose(false);
                    stage = "before-trash";
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 240 && SDL_GetTicks() - started < 60000, "Bounded loot capture timed out");
                    const bool continuing = phase == Phase::Continue;
                    game.renderFrame(width, height, wheel, continuing ? FrameSeconds : 0.0f);
                    require(game.m_screenManager.activeScreen() == nullptr && !screen().activeEventDialog().isActive
                        && scene().party().hasActableMember(), "Normal loot flow was interrupted");
                    if (continuing) { ++normalFrames; simulationSeconds += FrameSeconds; }
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create loot image");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save loot image");
                            record("captured:" + pendingCapture);
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (!pendingCapture.empty()) return;
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        capture("before-trash"); phase = Phase::TrashReady;
                    }
                    else if (phase == Phase::TrashReady)
                    {
                        stage = "source30-SDL-click"; click(false, width, height); phase = Phase::TrashClicked;
                    }
                    else if (phase == Phase::TrashClicked && frames >= phaseFrame + 3)
                    {
                        auto expected = baseline; ++expected[315];
                        require(liveCounts() == expected && scene().party().members()[0].conditions.test(
                            size_t(CharacterCondition::DiseaseMedium)), "Real30 did not grant315 and yellow disease");
                        afterTrash = liveCounts();
                        record("source30-reward-and-disease");
                        capture("trash-reward-disease"); phase = Phase::LibraryPose;
                    }
                    else if (phase == Phase::LibraryPose)
                    {
                        flushDelivery(); pose(true);
                        const auto &position = scene().partyRuntime().movementState();
                        const int key = int(std::ceil(std::lround(position.x) / 200.0)
                            + std::ceil(std::lround(position.y) / 200.0) + std::ceil(std::lround(position.footZ) / 100.0));
                        libraryKey = "MMerge.BrAlvar.LibKeys." + std::to_string(key);
                        require(mapValue(events(), libraryKey.c_str()) == 0, "This source position was already looted");
                        trace << "{\"event\":\"declared-one-RNG-fixture\",\"before\":" << events().eventRandomState
                            << ",\"after\":1,\"first_draw\":1015568748,\"second_draw\":1586005467,"
                               "\"level\":3,\"type\":17}\n";
                        events().eventRandomState = 1;
                        phaseFrame = frames; phase = Phase::LibraryReady;
                    }
                    else if (phase == Phase::LibraryReady && frames >= phaseFrame + 3)
                    {
                        stage = "source82-SDL-click"; click(true, width, height); phase = Phase::LibraryClicked;
                    }
                    else if (phase == Phase::LibraryClicked && frames >= phaseFrame + 3)
                    {
                        afterLibrary = liveCounts();
                        auto expected = afterTrash;
                        for (const auto &[id, quantity] : afterLibrary)
                        {
                            const auto old = afterTrash.find(id);
                            if (quantity == (old == afterTrash.end() ? 0 : old->second) + 1)
                            {
                                require(bookId == 0, "More than one library reward appeared"); bookId = id;
                            }
                        }
                        const auto *pItem = game.m_gameSession.data().itemTable().get(bookId);
                        require(bookId && pItem && toLowerCopy(pItem->equipStat) == "book",
                            "Source type17 did not produce an actual spell book");
                        ++expected[bookId];
                        require(afterLibrary == expected && mapValue(events(), libraryKey.c_str()) == 1,
                            "Real82 reward or original position lock differs");
                        libraryRng = events().eventRandomState;
                        record("source82-book-and-original-sum-key");
                        capture("library-book"); phase = Phase::RepeatReady;
                    }
                    else if (phase == Phase::RepeatReady)
                    {
                        flushDelivery(); stage = "source82-repeat-same-position";
                        click(true, width, height); phase = Phase::RepeatClicked;
                    }
                    else if (phase == Phase::RepeatClicked && frames >= phaseFrame + 3)
                    {
                        require(liveCounts() == afterLibrary && events().eventRandomState == libraryRng
                            && mapValue(events(), libraryKey.c_str()) == 1, "Repeated82 duplicated loot or RNG");
                        repeatedVerified = true; record("repeat-no-reward-no-RNG");
                        stage = "normal30frames"; phase = Phase::Continue;
                    }
                    else if (phase == Phase::Continue && normalFrames >= 30)
                    {
                        const auto path = output / "loot-complete.oysav";
                        require(game.quickSaveToPath(path, "176原始垃圾堆与图书馆"), "Loot save failed");
                        const auto saved = loadGameDataFromPath(path, error);
                        require(saved && saved->outdoorWorld.eventRuntimeState
                            && itemCounts(saved->party, saved->heldInventoryItemActive, saved->heldInventoryItem)
                                == afterLibrary
                            && saved->party.members[0].conditions.test(size_t(CharacterCondition::DiseaseMedium))
                            && mapValue(*saved->outdoorWorld.eventRuntimeState, libraryKey.c_str()) == 1,
                            "Save lost source reward, disease or position lock: " + error);
                        savedVerified = true; record("saved-after30-normal-frames"); phase = Phase::Done;
                    }
                    else if (phase == Phase::Done)
                    {
                        require(captures == 3 && clicks == 3 && fileDigest(input) == inputHash,
                            "Loot evidence is incomplete or input changed");
                        complete = true; stage = "complete";
                        SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot quit completed loot capture");
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Loot capture ended before completion");
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            trace << "{\"event\":\"failure\",\"stage\":" << std::quoted(stage)
                << ",\"error\":" << std::quoted(failure.what()) << "}\n";
            result(false, failure.what());
            throw;
        }
    }

    static int runOrdinary(std::filesystem::path root, std::filesystem::path input, std::filesystem::path output)
    {
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(root / "worlds/mmmerge.zip")
            && !std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/"),
            "Ordinary mode requires a complete runtime and a new NVMe output directory");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->hasOutdoorRuntimeState && source->outdoorWorld.eventRuntimeState
            && toLowerCopy(source->mapFileName) == "bralvar.odm" && source->party.members.size() == 1
            && source->breachAlvarCompanions && source->party.npcHouseOverrides.contains(773)
            && source->party.npcHouseOverrides.at(773) == 705,
            "Ordinary input must be the accepted154 split arrival with Prince in705: " + error);
        // Check the one declared NPC-location fixture against the existing save contract before opening the App.
        GameSaveData fixtureCandidate = *source;
        fixtureCandidate.party.npcHouseOverrides[773] = 0;
        require(validateSavedBreachAlvarCompanions(fixtureCandidate, error),
            "NPC773 house0 fixture conflicts with companion save validation: " + error);
        const std::string inputHash = fileDigest(input);
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":178,\"mode\":\"ordinary\",\"controlled\":true,\"natural_campaign\":false,"
               "\"input\":" << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string())
            << ",\"inherited_fixtures\":\"154 actual004 continued arrival inherits GotFinalQuest=1,Q1713,"
               "member2 spell31/WaterMagic10 Grandmaster and the original out02 position fixture. Its actual "
               "TownPortal arrival and split receipt are reused without replay.\",\"changes\":\"Only after "
               "the Prince-in705 negative dispatch, Party NPC773 house becomes0 once and its normal NPC "
               "projection is published into live events. No receipt, QuestFinished, custody, pose, reward, "
               "RNG, health, buff or AI fixture. Original301,7,60 handlers use the actual Scene/EventRuntime "
               "dispatch API; no world mouse clicks or pressure-plate walking are claimed. Two SDL Escape "
               "key pairs close the house and message. Inspection/render/capture frames use delta0; only "
               "the final30 frames advance normal gameplay and AI at1/60 second.\",\"world_SDL_clicks\":0,"
               "\"normal_ai\":true,\"new_unit_cases\":0,\"cold_replays\":0,\"visual_review_required\":true}\n";
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = settings.immortal = settings.unlimitedMana = settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM BrAlvar ordinary178";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(17);
        constexpr const char *AlarmKey = "MMerge.BrAlvar.SwordsAggro";
        constexpr uint32_t Hostile = 0x01000000;
        enum class Phase { Warmup, Locked, House, CloseHouse, Alarm, CloseAlarm, Continue, Done };
        Phase phase = Phase::Warmup;
        std::string stage = "initialize", pendingCapture, alarmText;
        unsigned frames = 0, phaseFrame = 0, captures = 0, normalFrames = 0, dispatches = 0;
        uint64_t started = 0;
        uint32_t portraitSequence = 0;
        bool complete = false, lockedVerified = false, houseVerified = false, guardVerified = false;
        bool alarmVerified = false, repeatVerified = false, saveVerified = false;
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm", "Ordinary flow left BrAlvar");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto events = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto groupMask = [](const EventRuntimeState &state)
        {
            const auto found = state.actorGroupSetMasks.find(15);
            return found == state.actorGroupSetMasks.end() ? 0u : found->second;
        };
        const auto questFinished = [](const auto &variables) -> std::optional<int32_t>
        {
            const auto found = variables.find("MMerge.CrossContinents.QuestFinished");
            return found == variables.end() ? std::nullopt : std::optional<int32_t>(found->second);
        };
        const auto record = [&](const std::string &label)
        {
            const auto party = scene().party().snapshot();
            const auto &member = party.members.at(party.activeMemberIndex);
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"prince_house\":" << party.npcHouseOverrides.at(773)
                << ",\"alarm\":" << mapValue(events(), AlarmKey) << ",\"group15_set_mask\":" << groupMask(events())
                << ",\"portrait\":" << unsigned(member.portraitState)
                << ",\"portrait_sequence\":" << member.portraitSequenceCounter
                << ",\"event_rng\":" << events().eventRandomState << ",\"dialog_source\":"
                << screen().activeEventDialog().sourceId << ",\"dialog_active\":"
                << screen().activeEventDialog().isActive << "}\n";
            trace.flush();
        };
        const auto dispatch = [&](uint16_t id)
        {
            require(!screen().activeEventDialog().isActive, "Close the prior dialog before source dispatch");
            size_t previous = 0;
            require(scene().executeMapEventById(id, previous), "Source dispatch failed: " + std::to_string(id));
            ++dispatches;
            screen().presentPendingEventDialog(previous, false);
            record("native-source-dispatch:" + std::to_string(id));
        };
        const auto escape = [&]()
        {
            int count = 0;
            SDL_Window **pWindows = SDL_GetWindows(&count);
            SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
            SDL_free(pWindows);
            require(pWindow != nullptr, "Cannot find ordinary capture window");
            SDL_Event key{};
            key.type = SDL_EVENT_KEY_DOWN;
            key.key.windowID = SDL_GetWindowID(pWindow);
            key.key.key = SDLK_ESCAPE;
            key.key.scancode = SDL_SCANCODE_ESCAPE;
            key.key.down = true;
            require(SDL_PushEvent(&key), "Cannot queue Escape down");
            key.type = SDL_EVENT_KEY_UP;
            key.key.down = false;
            require(SDL_PushEvent(&key), "Cannot queue Escape up");
            phaseFrame = frames;
        };
        const auto capture = [&](const std::string &label)
        {
            require(pendingCapture.empty() && captures < 3, "Ordinary capture limit exceeded");
            pendingCapture = label;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, label.c_str());
        };
        const auto result = [&](bool passed, const std::string &message)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(17)
                << "{\"status\":" << std::quoted(passed ? "passed" : "failed")
                << ",\"checkpoint\":178,\"mode\":\"ordinary\",\"controlled\":true,\"natural_campaign\":false,"
                   "\"world_SDL_clicks\":0,\"pressure_plate_walk\":false,\"stage\":" << std::quoted(stage)
                << ",\"frames\":" << frames << ",\"captures\":" << captures << ",\"source_dispatches\":"
                << dispatches << ",\"DoorLocked_verified\":" << lockedVerified << ",\"house231_verified\":"
                << houseVerified << ",\"prince705_guard_verified\":" << guardVerified
                << ",\"alarm_verified\":" << alarmVerified << ",\"repeat_no_alarm\":" << repeatVerified
                << ",\"save_verified\":" << saveVerified << ",\"normal_frames\":" << normalFrames
                << ",\"simulation_seconds\":" << normalFrames * double(FrameSeconds)
                << ",\"normal_ai\":true,\"inspection_input_delta_seconds\":0,\"new_unit_cases\":0,"
                   "\"cold_replays\":0,\"error\":" << std::quoted(message)
                << ",\"input_sha256_before\":" << std::quoted(inputHash)
                << ",\"input_sha256_after\":" << std::quoted(fileDigest(input)) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal
                        && game.initializeRenderer() && game.loadSessionFromPath(input), "Ordinary input load failed");
                    screen().closeActiveEventDialog();
                    require(scene().localEventProgram() && scene().localEventProgram()->hasEvent(7)
                        && scene().localEventProgram()->hasEvent(301) && scene().localEventProgram()->hasEvent(60),
                        "Fresh source7/301/60 handlers are absent");
                    alarmText = scene().localEventProgram()->getLocalString(2).value_or("");
                    require(!alarmText.empty() && mapValue(events(), AlarmKey) == 0
                        && (groupMask(events()) & Hostile) == 0, "Input already consumed the source alarm");
                    record("loaded154-arrival-without-new-fixture");
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 180 && SDL_GetTicks() - started < 60000, "Bounded ordinary capture timed out");
                    const bool continuing = phase == Phase::Continue;
                    game.renderFrame(width, height, wheel, continuing ? FrameSeconds : 0.0f);
                    require(game.m_screenManager.activeScreen() == nullptr && scene().party().hasActableMember(),
                        "Ordinary flow was interrupted");
                    if (continuing) ++normalFrames;
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create ordinary image");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save ordinary image");
                            record("captured:" + pendingCapture);
                            pendingCapture.clear();
                            ++captures;
                        }
                        if (!pendingCapture.empty()) return;
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        stage = "source301-native-dispatch";
                        const auto before = scene().party().snapshot();
                        const uint32_t rng = events().eventRandomState;
                        portraitSequence = before.members.at(before.activeMemberIndex).portraitSequenceCounter;
                        dispatch(301);
                        const auto &audio = scene().party().pendingAudioRequests();
                        require(std::any_of(audio.begin(), audio.end(), [&](const auto &request)
                            { return request.kind == Party::PendingAudioRequest::Kind::Speech
                                && request.speechId == SpeechId::DoorLocked && request.memberIndex == before.activeMemberIndex; }),
                            "Source301 did not request the active member's DoorLocked reaction");
                        require(events().eventRandomState == rng && events().openedChestRequests.empty()
                            && events().grantedItems.empty() && events().grantedItemIds.empty()
                            && scene().party().snapshot().questBits == before.questBits
                            && !screen().activeEventDialog().isActive, "Locked source301 produced unexpected effects");
                        lockedVerified = true;
                        phaseFrame = frames;
                        phase = Phase::Locked;
                    }
                    else if (phase == Phase::Locked && frames >= phaseFrame + 2)
                    {
                        const auto &member = scene().party().members().at(scene().party().activeMemberIndex());
                        const auto *animation = game.m_gameSession.data().faceAnimationTable().find(FaceAnimationId::DoorLocked);
                        require(animation && member.portraitSequenceCounter > portraitSequence
                            && std::find(animation->portraitIds.begin(), animation->portraitIds.end(), member.portraitState)
                                != animation->portraitIds.end(), "DoorLocked did not reach its ordinary portrait presentation");
                        capture("locked-reaction");
                        phase = Phase::House;
                    }
                    else if (phase == Phase::House)
                    {
                        stage = "source7-native-dispatch";
                        dispatch(7);
                        const auto &dialog = screen().activeEventDialog();
                        require(dialog.isActive && dialog.isHouseDialog && dialog.sourceId == 231,
                            "Source7 did not present original house231");
                        houseVerified = true;
                        capture("tavern231");
                        phase = Phase::CloseHouse;
                        phaseFrame = 0;
                    }
                    else if (phase == Phase::CloseHouse)
                    {
                        if (phaseFrame == 0) { escape(); return; }
                        if (frames < phaseFrame + 3) return;
                        require(!screen().activeEventDialog().isActive, "Escape did not close house231");
                        stage = "source60-prince705-negative";
                        const auto beforeMessages = events().messages;
                        const auto beforeMask = groupMask(events());
                        dispatch(60);
                        require(events().messages == beforeMessages && !screen().activeEventDialog().isActive
                            && mapValue(events(), AlarmKey) == 0 && groupMask(events()) == beforeMask,
                            "Source60 alarm fired while the Prince remained in705");
                        guardVerified = true;
                        stage = "declared-party-NPC773-house0";
                        scene().party().setNpcHouseOverride(773, 0);
                        scene().party().applyGlobalNpcStateTo(events());
                        require(game.m_gameSession.breachAlvarCompanions() == source->breachAlvarCompanions
                            && questFinished(game.m_gameSession.namedGlobalVars()) == questFinished(source->namedGlobalVars),
                            "Location fixture changed receipt or QuestFinished");
                        record("one-NPC-house-fixture-published");
                        stage = "source60-first-alarm";
                        dispatch(60);
                        const auto &dialog = screen().activeEventDialog();
                        require(mapValue(events(), AlarmKey) == 1 && (groupMask(events()) & Hostile) != 0
                            && dialog.isActive && std::find(dialog.lines.begin(), dialog.lines.end(), alarmText)
                                != dialog.lines.end(), "Source60 did not display original str2 and set the one-time alarm");
                        size_t groupActors = 0;
                        for (size_t index = 0; index < scene().worldRuntime().mapActorCount(); ++index)
                        {
                            const auto *actor = scene().worldRuntime().mapActorState(index);
                            if (!actor || actor->group != 15) continue;
                            ++groupActors;
                            require(actor->hostileToParty, "Group15 hostility did not reach its actual actor consumer");
                        }
                        require(groupActors > 0, "Original group15 actors are absent");
                        alarmVerified = true;
                        phase = Phase::Alarm;
                    }
                    else if (phase == Phase::Alarm)
                    {
                        capture("prince-alarm-str2");
                        phase = Phase::CloseAlarm;
                        phaseFrame = 0;
                    }
                    else if (phase == Phase::CloseAlarm)
                    {
                        if (phaseFrame == 0) { escape(); return; }
                        if (frames < phaseFrame + 3) return;
                        require(!screen().activeEventDialog().isActive, "Escape did not close the alarm message");
                        stage = "source60-repeat";
                        const auto beforeMessages = events().messages;
                        const auto beforeMask = groupMask(events());
                        dispatch(60);
                        require(events().messages == beforeMessages && !screen().activeEventDialog().isActive
                            && mapValue(events(), AlarmKey) == 1 && groupMask(events()) == beforeMask,
                            "Repeated source60 produced a second alarm");
                        repeatVerified = true;
                        stage = "normal30frames";
                        phase = Phase::Continue;
                    }
                    else if (phase == Phase::Continue && normalFrames >= 30)
                    {
                        require(!screen().activeEventDialog().isActive, "Ordinary continuation opened another dialog");
                        const auto path = output / "ordinary-complete.oysav";
                        require(game.quickSaveToPath(path, "178布雷阿瓦尔普通交互"), "Ordinary save failed");
                        const auto saved = loadGameDataFromPath(path, error);
                        require(saved && saved->outdoorWorld.eventRuntimeState
                            && saved->breachAlvarCompanions == source->breachAlvarCompanions
                            && questFinished(saved->namedGlobalVars) == questFinished(source->namedGlobalVars)
                            && saved->party.questBits == source->party.questBits
                            && saved->party.nextMemberId == source->party.nextMemberId
                            && saved->party.npcHouseOverrides.at(773) == 0
                            && mapValue(*saved->outdoorWorld.eventRuntimeState, AlarmKey) == 1
                            && (groupMask(*saved->outdoorWorld.eventRuntimeState) & Hostile) != 0,
                            "Save lost alarm/location or changed split receipt/quest state: " + error);
                        saveVerified = true;
                        record("saved-after30-normal-frames");
                        phase = Phase::Done;
                    }
                    else if (phase == Phase::Done)
                    {
                        require(captures == 3 && dispatches == 5 && fileDigest(input) == inputHash,
                            "Ordinary evidence is incomplete or original input changed");
                        complete = true;
                        stage = "complete";
                        SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot quit completed ordinary capture");
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Ordinary capture ended before completion");
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            trace << "{\"event\":\"failure\",\"stage\":" << std::quoted(stage)
                << ",\"error\":" << std::quoted(failure.what()) << "}\n";
            result(false, failure.what());
            throw;
        }
    }

    static int runActorRules(std::filesystem::path root, std::filesystem::path input, std::filesystem::path output)
    {
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(root / "worlds/mmmerge.zip")
            && !std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/"),
            "Actor rules mode requires a complete runtime and a new NVMe output directory");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        constexpr const char *RulesKey = "MMerge.BrAlvar.ActorRulesVersion";
        constexpr uint32_t Hostile = 0x01000000;
        require(source && source->hasOutdoorRuntimeState && source->outdoorWorld.eventRuntimeState
            && toLowerCopy(source->mapFileName) == "bralvar.odm"
            && mapValue(*source->outdoorWorld.eventRuntimeState, RulesKey) == 0
            && source->outdoorWorld.eventRuntimeState->actorGroupSetMasks.contains(15)
            && (source->outdoorWorld.eventRuntimeState->actorGroupSetMasks.at(15) & Hostile) != 0,
            "Use the original178 save with its existing source alarm and no actor-rule migration: " + error);
        const std::string inputHash = fileDigest(input);
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":180,\"input\":" << std::quoted(input.string())
            << ",\"input_sha256\":" << std::quoted(inputHash) << ",\"runtime\":" << std::quoted(root.string())
            << ",\"controlled\":true,\"natural_campaign\":false,\"new_gameplay_fixtures\":false,"
               "\"inherited_fixtures\":\"178 ordinary-complete inherits154 controlled admission plus NPC773 house0.\","
               "\"scope\":\"Load old save through real App, inspect isolated AfterLoad migration, preserve prior alarm,"
               " render, advance30 normal frames, save and decode. No source dispatch, pose, AI, health or RNG injection.\","
               "\"subjective_audio\":false,\"new_unit_cases\":0,\"old_routes_replayed\":0}\n";
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = settings.immortal = settings.unlimitedMana = settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM BrAlvar actor rules180";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        unsigned frames = 0, normalFrames = 0, captures = 0;
        size_t wimps = 0, guards = 0, relations = 0;
        uint64_t started = 0;
        bool capturePending = false, complete = false;
        std::string stage = "initialize";
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm", "Actor rules flow left BrAlvar");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto inspect = [&](const EventRuntimeState &events, const auto &actors)
        {
            require(mapValue(events, RulesKey) == 1, "Source actor-rule migration was not committed");
            const auto relation = [&](uint32_t left, uint32_t right, int value)
            {
                const auto found = events.monsterRelationOverrides.find(EventRuntime::monsterRelationOverrideKey(left, right));
                require(found != events.monsterRelationOverrides.end() && found->second == value,
                    "Original directional relation is missing");
            };
            relation(649, 292, 4); relation(292, 649, 1);
            relation(475, 0, 0); relation(73, 0, 4); relation(0, 73, 4); relation(73, 75, 0);
            relations = events.monsterRelationOverrides.size();
            require(relations >= 198, "Source local relation expansion is incomplete");
            wimps = guards = 0;
            for (const auto &actor : actors)
            {
                if (actor.monsterId == 294)
                {
                    ++wimps;
                    require(actor.aiType == GameplayActorAiType::Wimp, "Original294 Wimp AI was lost");
                }
                if (actor.group == 15 && !actor.isDead && !actor.isSourceRemoved())
                {
                    ++guards;
                    require(actor.hostileToParty, "Local159 neutral default cancelled the existing source alarm");
                }
            }
            require(wimps == 9 && guards > 0, "Expected original294 and alarm guard actors");
            require((events.actorGroupSetMasks.at(15) & Hostile) != 0, "Source alarm mask was lost");
        };
        const auto result = [&](bool passed, const std::string &message)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"status\":" << std::quoted(passed ? "passed" : "failed")
                << ",\"checkpoint\":180,\"mode\":\"actor-rules\",\"stage\":" << std::quoted(stage)
                << ",\"controlled\":true,\"natural_campaign\":false,\"new_gameplay_fixtures\":false,"
                   "\"frames\":" << frames << ",\"normal_frames\":" << normalFrames
                << ",\"captures\":" << captures << ",\"wimp_actors\":" << wimps << ",\"hostile_guards\":"
                << guards << ",\"local_relations\":" << relations
                << ",\"subjective_audio\":false,\"source_dispatches\":0,\"new_unit_cases\":0,"
                   "\"error\":" << std::quoted(message) << ",\"input_sha256_before\":" << std::quoted(inputHash)
                << ",\"input_sha256_after\":" << std::quoted(fileDigest(input)) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal
                        && game.initializeRenderer() && game.loadSessionFromPath(input), "Actor rules load failed");
                    game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                    const auto restored = scene().worldRuntime().snapshot();
                    inspect(*scene().eventRuntimeState(), restored.mapActors);
                    checkPartyCustody(source->party, scene().party().snapshot());
                    require(game.m_gameSession.breachAlvarCompanions() == source->breachAlvarCompanions,
                        "Actor-rule migration changed the split receipt");
                    require(scene().worldRuntime().mapData()->textureFootstepSoundOverrides.size() == 8,
                        "Authenticated source texture footsteps were not loaded");
                    trace << "{\"event\":\"old-save-migrated-with-existing-alarm\"}\n";
                    stage = "render";
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 100 && SDL_GetTicks() - started < 60000, "Actor rules capture timed out");
                    const bool continuing = captures == 1;
                    game.renderFrame(width, height, wheel, continuing ? FrameSeconds : 0.0f);
                    require(game.m_screenManager.activeScreen() == nullptr && scene().party().hasActableMember(),
                        "Actor rules continuation was interrupted");
                    if (continuing) ++normalFrames;
                    if (capturePending)
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot("actor-rules"))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create actor rules image");
                            const bool saved = SDL_SaveBMP(pSurface, (output / "actor-rules.bmp").c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save actor rules image");
                            capturePending = false;
                            ++captures;
                            stage = "normal30frames";
                        }
                    }
                    else if (captures == 0 && frames >= 8)
                    {
                        capturePending = true;
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "actor-rules");
                    }
                    else if (normalFrames >= 30 && !complete)
                    {
                        stage = "save";
                        const auto path = output / "actor-rules-complete.oysav";
                        require(game.quickSaveToPath(path, "180布雷阿瓦尔生物规则"), "Actor rules save failed");
                        const auto saved = loadGameDataFromPath(path, error);
                        require(saved && saved->outdoorWorld.eventRuntimeState, "Cannot decode actor rules save: " + error);
                        inspect(*saved->outdoorWorld.eventRuntimeState, saved->outdoorWorld.mapActors);
                        require(saved->breachAlvarCompanions == source->breachAlvarCompanions
                            && fileDigest(input) == inputHash, "Receipt or original input changed");
                        trace << "{\"event\":\"saved-after30-normal-frames\"}\n";
                        complete = true;
                        stage = "complete";
                        SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot quit actor rules capture");
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Actor rules capture ended before completion");
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            result(false, failure.what());
            throw;
        }
    }

    static int run(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, const std::string &mode)
    {
        if (mode == "loot") return runLoot(std::move(root), std::move(input), std::move(output));
        if (mode == "ordinary") return runOrdinary(std::move(root), std::move(input), std::move(output));
        if (mode == "actor-rules") return runActorRules(std::move(root), std::move(input), std::move(output));
        const bool cold = mode == "cold";
        require(cold || mode == "actual", "Expected actual or cold mode");
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        require(std::filesystem::is_regular_file(root / "worlds/mmmerge.zip"), "Complete fresh runtime required");
        require(!std::filesystem::exists(output)
            && std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Use a new output directory on NVMeSSD");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        const std::string inputHash = fileDigest(input);
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->currentSceneKind == SceneKind::Outdoor && source->hasOutdoorRuntimeState
            && source->outdoorWorld.eventRuntimeState && toLowerCopy(source->mapFileName) == "bralvar.odm",
            "Expected the prior actual BrAlvar outdoor save: " + error);

        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"input\":" << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string()) << ",\"changes\":"
            << std::quoted(cold
                ? "Independent cold load of the completed daylight save. No position, camera, event, Party, HP, "
                  "quest, RNG or buff injection. Save and compare the restored state before0.5 normal seconds."
                : "One declared position fixture(1660,-174,3127.01) on original244/48 and fixed camera toward "
                  "the three Time models. Actual typed surface pointer clicks102 then103. Observe source default "
                  "4:12, real2-second interpolation,6:12 then11:actual-minute lighting. No direct event execution, "
                  "stairs, summons, quest, HP, Party, RNG or buff writes.")
            << ",\"normal_ai\":true,\"timing\":\"Gameplay including AI plus World interpolation uses1/60; "
               "render/UI delta0. Captures/save pause simulation. Actual clock must advance0.5game-minute/second.\","
               "\"bounds\":\"10 simulation seconds,500 frames,60 wall seconds after initialization\","
               "\"visual_review_required\":true}\n";

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM controlled BrAlvar daylight";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        std::ofstream pointers(output / "pointer-probes.jsonl");
        trace << std::setprecision(17) << std::boolalpha;
        pointers << std::setprecision(17) << std::boolalpha;
        std::string stage = "initialize";
        std::string failureDomain = "initialization";
        std::string captureLabel;
        unsigned captureFrame = 0;
        bool captureRequested = false;
        unsigned frames = 0;
        unsigned captures = 0;
        int appResult = -1;
        bool complete = false;
        bool coldChecked = false;
        std::array<bool, 2> intermediateObserved = {};
        bool inheritedMinuteChanged = false;
        bool naturalVitalsChanged = false;
        double initialGameMinutes = 0.0;
        double simulationSeconds = 0.0;
        double phaseStartSeconds = 0.0;
        int inheritedMinuteAtStart = 0;
        uint64_t readyWallTicks = 0;
        bool midpointCaptured = false;
        Party::Snapshot beforeParty;
        std::unordered_map<std::string, int32_t> beforeGlobals;
        OutdoorWorldRuntime::AtmosphereState defaultLight;
        OutdoorWorldRuntime::AtmosphereState secondLight;
        enum class Phase { Default, ClickSecond, Second, ClickThird, Third, ColdContinue, Done };
        Phase phase = cold ? Phase::ColdContinue : Phase::Default;

        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Daylight observation left its real outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto events = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto stationary = [&]()
        {
            return std::all_of(TimeEvents.begin(), TimeEvents.end(), [&](uint32_t eventId)
            {
                return !events().mechanisms.at(eventId).isMoving;
            });
        };
        const auto checkSelection = [&](int selection)
        {
            require(mapValue(events(), DaylightVersionKey) == 1 && mapValue(events(), DayStateKey) == selection,
                "Source daylight selection/version differs");
            const auto &light = events().outdoorLightTimeOverride;
            require(light && light->hour == (selection == 1 ? 4 : selection == 2 ? 6 : 11)
                && light->minute == (selection == 3 ? std::optional<int32_t>{} : std::optional<int32_t>{12}),
                "Source typed hour or fixed/inherited minute differs");
            for (size_t index = 0; index < TimeEvents.size(); ++index)
            {
                const RuntimeMechanismState &motion = events().mechanisms.at(TimeEvents[index]);
                const float expectedFraction = int(index) + 1 == selection ? 0.0f : 1.0f;
                require(!motion.isMoving
                    && motion.state == static_cast<uint16_t>(expectedFraction == 0.0f
                        ? EvtMechanismState::Closed : EvtMechanismState::Open)
                    && std::abs(outdoorMechanismOpenFraction(motion,
                        events().outdoorModelMechanisms.at(TimeEvents[index])) - expectedFraction) < 0.0001f,
                    "Source all-open/selected-closed geometry did not finish");
                checkGeometry(geometryEvidence(scene().worldRuntime(), TimeModels[index]), expectedFraction);
            }
            checkDerivedLight(scene().worldRuntime(), events());
        };
        const auto record = [&](const std::string &label, bool detailedGeometry = false)
        {
            auto &world = scene().worldRuntime();
            const auto &atmosphere = world.atmosphereState();
            const auto &pose = scene().partyRuntime().movementState();
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"simulation_seconds\":" << simulationSeconds
                << ",\"game_minutes\":" << world.gameMinutes() << ",\"light_game_minutes\":"
                << world.lightGameMinutes() << ",\"light_minutes_of_day\":" << atmosphere.lightMinutesOfDay
                << ",\"typed_override\":";
            writeOverride(trace, events().outdoorLightTimeOverride);
            trace << ",\"day_state\":" << mapValue(events(), DayStateKey)
                << ",\"feature_version\":" << mapValue(events(), DaylightVersionKey)
                << ",\"xyz\":[" << pose.x << ',' << pose.y << ',' << pose.footZ << "],\"support\":["
                << int(pose.supportKind) << ',' << pose.supportBModelIndex << ',' << pose.supportFaceIndex
                << "],\"sun_direction\":[" << atmosphere.sunDirectionX << ',' << atmosphere.sunDirectionY
                << ',' << atmosphere.sunDirectionZ << "],\"ambient\":" << atmosphere.ambientBrightness
                << ",\"darkness_alpha\":" << atmosphere.darknessOverlayAlpha
                << ",\"darkness_color_abgr\":" << atmosphere.darknessOverlayColorAbgr
                << ",\"is_night\":" << atmosphere.isNight << ",\"sky\":"
                << std::quoted(atmosphere.skyTextureName) << ",\"event_rng\":" << events().eventRandomState
                << ",\"actor_count\":" << world.mapActorCount() << ",\"camera\":["
                << game.m_outdoorGameView.cameraYawRadians() << ','
                << game.m_outdoorGameView.cameraPitchRadians() << "],\"mechanisms\":[";
            for (size_t index = 0; index < TimeEvents.size(); ++index)
            {
                if (index) trace << ',';
                const auto &definition = events().outdoorModelMechanisms.at(TimeEvents[index]);
                const auto &motion = events().mechanisms.at(TimeEvents[index]);
                const auto geometry = geometryEvidence(world, TimeModels[index]);
                trace << "{\"event\":" << TimeEvents[index] << ",\"model\":" << TimeModels[index]
                    << ",\"state\":" << motion.state << ",\"elapsed_ms\":" << motion.timeSinceTriggeredMs
                    << ",\"distance\":" << motion.currentDistance << ",\"rotation_direction\":"
                    << int(motion.rotationDirection) << ",\"moving\":" << motion.isMoving << ",\"fraction\":"
                    << outdoorMechanismOpenFraction(motion, definition) << ",\"live_z\":["
                    << geometry.minimumZ << ',' << geometry.maximumZ << "],\"raw_to_live_z_delta\":["
                    << geometry.minimumDeltaZ << ',' << geometry.maximumDeltaZ
                    << "],\"live_vertex_count\":" << geometry.vertices.size();
                if (detailedGeometry)
                {
                    trace << ",\"vertices\":[";
                    for (size_t vertex = 0; vertex < geometry.vertices.size(); ++vertex)
                    {
                        if (vertex) trace << ',';
                        const auto &point = geometry.vertices[vertex];
                        trace << '[' << point.face << ',' << point.vertex << ',' << point.raw.x << ','
                            << point.raw.y << ',' << point.raw.z << ',' << point.live.x << ',' << point.live.y
                            << ',' << point.live.z << ']';
                    }
                    trace << ']';
                }
                trace << '}';
            }
            trace << "],\"members\":[";
            bool separator = false;
            const auto baseline = ownedMembers(beforeParty);
            for (const auto &[id, member] : ownedMembers(scene().party().snapshot()))
            {
                if (separator) trace << ',';
                separator = true;
                const auto found = baseline.find(id);
                const bool changed = found != baseline.end() && (found->second.health != member.health
                    || found->second.spellPoints != member.spellPoints
                    || found->second.conditions != member.conditions);
                naturalVitalsChanged = naturalVitalsChanged || changed;
                trace << "{\"id\":" << id << ",\"hp\":" << member.health << ",\"sp\":" << member.spellPoints
                    << ",\"xp\":" << member.experience << ",\"natural_vitals_changed\":" << changed
                    << ",\"items_sha256\":" << std::quoted(itemDigest(member)) << '}';
            }
            trace << "]}\n";
            trace.flush();
        };
        const auto checkInvariants = [&]()
        {
            failureDomain = "party-custody-or-quest-change";
            require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm", "Unexpected map change");
            checkPartyCustody(beforeParty, scene().party().snapshot());
            for (const auto &[key, value] : beforeGlobals)
            {
                if (!key.starts_with("MMerge.CrossContinents.")) continue;
                const auto found = events().namedGlobalVars.find(key);
                require(found != events().namedGlobalVars.end() && found->second == value,
                    "A source quest/progression flag changed during daylight observation");
            }
            require(!game.m_gameSession.gameplayUiController().heldInventoryItem().active,
                "Unexpected held-item ownership change");
            failureDomain = "daylight-projection-or-geometry";
            checkDerivedLight(scene().worldRuntime(), events());
            for (size_t index = 0; index < TimeEvents.size(); ++index)
                checkGeometry(geometryEvidence(scene().worldRuntime(), TimeModels[index]),
                    outdoorMechanismOpenFraction(events().mechanisms.at(TimeEvents[index]),
                        events().outdoorModelMechanisms.at(TimeEvents[index])),
                    events().mechanisms.at(TimeEvents[index]).isMoving);
        };
        const auto save = [&](const std::string &label)
        {
            failureDomain = "daylight-save-persistence";
            const EventRuntimeState expected = events();
            const double expectedClock = scene().worldRuntime().gameMinutes();
            require(game.quickSaveToPath(output / (label + ".oysav"), "原始昼夜 " + label), "Save failed");
            const auto saved = loadGameDataFromPath(output / (label + ".oysav"), error);
            require(saved && saved->hasOutdoorRuntimeState && saved->outdoorWorld.eventRuntimeState,
                "Immediate save readback failed: " + error);
            const EventRuntimeState &actual = *saved->outdoorWorld.eventRuntimeState;
            require(actual.outdoorLightTimeOverride == expected.outdoorLightTimeOverride
                && mapValue(actual, DayStateKey) == mapValue(expected, DayStateKey)
                && mapValue(actual, DaylightVersionKey) == mapValue(expected, DaylightVersionKey)
                && actual.eventRandomState == expected.eventRandomState && saved->savedGameMinutes == expectedClock,
                "Save changed typed daylight, source selection, RNG or actual time");
            for (uint32_t eventId : TimeEvents)
                require(actual.mechanisms.contains(eventId)
                    && equalMotion(actual.mechanisms.at(eventId), expected.mechanisms.at(eventId)),
                    "Save changed complete Time mechanism motion state");
            require(std::abs(projectedOutdoorLightGameMinutes(saved->savedGameMinutes,
                actual.outdoorLightTimeOverride) - scene().worldRuntime().lightGameMinutes()) < 0.00001,
                "Saved typed values do not reconstruct the live light time");
            checkPartyCustody(scene().party().snapshot(), saved->party);
            record("saved:" + label, true);
        };
        const auto beginCapture = [&](const std::string &label)
        {
            require(captureLabel.empty(), "Overlapping captures");
            captureLabel = label;
            captureRequested = false;
            captureFrame = frames + 2;
        };
        const auto advance = [&](int width, int height)
        {
            failureDomain = "normal-world-interruption";
            require(game.m_screenManager.activeScreen() == nullptr
                && !game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive
                && !game.m_gameSession.turnBasedCombatRuntime().active(),
                "Normal observation encountered a modal screen or turn-based pause");
            GameplayInputFrame frame;
            frame.screenWidth = width;
            frame.screenHeight = height;
            game.m_gameSession.updateGameplay(frame, FrameSeconds);
            scene().worldRuntime().updateWorld(FrameSeconds);
            simulationSeconds += FrameSeconds;
            failureDomain = "actual-clock-rate";
            require(simulationSeconds <= 10.0
                && std::abs(scene().worldRuntime().gameMinutes() - initialGameMinutes
                    - simulationSeconds * 0.5) < 0.001,
                "Normal actual clock did not advance0.5game-minute/second within its bound");
            checkInvariants();
            if (mapValue(events(), DayStateKey) == 3)
                inheritedMinuteChanged = inheritedMinuteChanged
                    || int(std::floor(std::fmod(scene().worldRuntime().gameMinutes(), 60.0))) != inheritedMinuteAtStart;
            record("normal-update");
        };
        const auto click = [&](uint32_t eventId, int width, int height)
        {
            failureDomain = "typed-pointer-or-input";
            auto &world = scene().worldRuntime();
            const auto &definition = events().outdoorModelMechanisms.at(eventId);
            const auto camera = world.buildWorldPickRequest({width * 0.5f, height * 0.5f, width, height, true});
            float matrix[16];
            bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
            std::vector<size_t> faces;
            world.collectOutdoorFaceCandidates(1740.0f, -270.0f, 1820.0f, -70.0f, faces);
            std::optional<std::array<float, 2>> pixel;
            unsigned probes = 0;
            for (size_t faceIndex : faces)
            {
                const OutdoorFaceGeometryData *pFace = world.outdoorFace(faceIndex);
                if (!pFace || pFace->bModelIndex != definition.bmodelIndex || pFace->vertices.empty()) continue;
                bx::Vec3 center = {0.0f, 0.0f, 0.0f};
                for (const bx::Vec3 &vertex : pFace->vertices) center = bx::add(center, vertex);
                center = bx::mul(center, 1.0f / float(pFace->vertices.size()));
                std::vector<bx::Vec3> points{center};
                for (const bx::Vec3 &vertex : pFace->vertices)
                    points.push_back(bx::add(center, bx::mul(bx::sub(vertex, center), 0.65f)));
                for (const bx::Vec3 &point : points)
                {
                    require(++probes <= 128, "Bounded actual pointer probes exhausted");
                    const bx::Vec3 projected = bx::mulH(point, matrix);
                    const float x = (projected.x + 1.0f) * width * 0.5f;
                    const float y = (1.0f - projected.y) * height * 0.5f;
                    if (x < 0 || y < 0 || x >= width || y >= height) continue;
                    const auto hit = world.pickMouseInteractionTarget(
                        world.buildWorldPickRequest({x, y, width, height, true}));
                    pointers << "{\"event\":" << eventId << ",\"frame\":" << frames
                        << ",\"probe\":" << probes << ",\"pixel\":[" << x << ',' << y
                        << "],\"hit_kind\":" << int(hit.kind);
                    if (hit.eventTarget)
                        pointers << ",\"target_kind\":" << int(hit.eventTarget->targetKind)
                            << ",\"model\":" << hit.eventTarget->targetIndex << ",\"face\":"
                            << hit.eventTarget->secondaryIndex << ",\"triggered_event\":"
                            << hit.eventTarget->triggeredEventId << ",\"distance\":" << hit.eventTarget->distance;
                    if (hit.actor) pointers << ",\"occluding_actor\":" << hit.actor->actorIndex;
                    const bool accepted = hit.eventTarget
                        && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                        && hit.eventTarget->targetIndex == definition.bmodelIndex
                        && hit.eventTarget->triggeredEventId == eventId
                        && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse);
                    pointers << ",\"accepted\":" << accepted << "}\n";
                    pointers.flush();
                    if (accepted)
                    {
                        pixel = std::array<float, 2>{x, y};
                        break;
                    }
                }
                if (pixel) break;
            }
            require(pixel.has_value(), "Actual typed surface pointer cannot reach source" + std::to_string(eventId));
            const uint32_t rng = events().eventRandomState;
            GameplayInputFrame frame;
            frame.screenWidth = width;
            frame.screenHeight = height;
            frame.pointerX = (*pixel)[0];
            frame.pointerY = (*pixel)[1];
            frame.rightMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(frame, 0.0f);
            frame.rightMouseButton.pressed = false;
            frame.leftMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(frame, 0.0f);
            frame.leftMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(frame, 0.0f);
            frame.leftMouseButton = {};
            frame.rightMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(frame, 0.0f);
            require(events().eventRandomState == rng, "The source daylight click unexpectedly consumed event RNG");
            require(mapValue(events(), DayStateKey) == int32_t(eventId - 100),
                "Actual pointer input did not activate the intended source daylight handler");
            require(events().outdoorLightTimeOverride
                && events().outdoorLightTimeOverride->hour == (eventId == 102 ? 6 : 11)
                && events().outdoorLightTimeOverride->minute
                    == (eventId == 102 ? std::optional<int32_t>{12} : std::optional<int32_t>{}),
                "The real source click did not immediately publish its typed hour/minute selection");
            record("actual-pointer:" + std::to_string(eventId), true);
        };
        const auto writeResult = [&](bool passed, const std::string &message)
        {
            std::ofstream(output / "result.json") << std::setprecision(17) << std::boolalpha
                << "{\"status\":" << std::quoted(passed ? "passed" : "failed")
                << ",\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
                << ",\"stage\":" << std::quoted(stage) << ",\"complete\":" << complete
                << ",\"frames\":" << frames << ",\"captures\":" << captures << ",\"app_result\":" << appResult
                << ",\"simulation_seconds\":" << simulationSeconds << ",\"cold_state_equal\":" << coldChecked
                << ",\"source102_intermediate_geometry\":" << intermediateObserved[0]
                << ",\"source103_intermediate_geometry\":" << intermediateObserved[1]
                << ",\"inherited_actual_minute_changed\":" << inheritedMinuteChanged
                << ",\"natural_vitals_changed\":" << naturalVitalsChanged
                << ",\"normal_ai\":true,\"failure_domain\":" << std::quoted(failureDomain)
                << ",\"error\":" << std::quoted(message) << ",\"input_sha256_before\":" << std::quoted(inputHash)
                << ",\"input_sha256_after\":" << std::quoted(fileDigest(input))
                << ",\"visual_review_required\":true}\n";
        };

        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual capture requires Metal");
                    require(game.initializeRenderer() && game.loadSessionFromPath(input), "Actual input load failed");
                    game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                    beforeParty = source->party;
                    beforeGlobals = source->namedGlobalVars;
                    checkPartyCustody(beforeParty, scene().party().snapshot());
                    require(!game.m_gameSession.gameplayUiController().heldInventoryItem().active,
                        "Use the original empty-held input");
                    require(events().outdoorModelMechanisms.size() == 28 && scene().localEventProgram(),
                        "Fresh source28 definitions/program are required");
                    for (size_t index = 0; index < TimeEvents.size(); ++index)
                    {
                        const auto &definition = events().outdoorModelMechanisms.at(TimeEvents[index]);
                        require(definition.bmodelIndex == TimeModels[index]
                            && definition.modelName == "Time_" + std::to_string(index + 1)
                            && definition.interactionEventId == TimeEvents[index] && definition.dx == 0
                            && definition.dy == 0 && definition.dz == -10 && definition.moveTimeMs == 2000
                            && !definition.moveParty && scene().localEventProgram()->hasEvent(TimeEvents[index]),
                            "Fresh original Time model definition/handler differs");
                    }
                    initialGameMinutes = scene().worldRuntime().gameMinutes();
                    require(initialGameMinutes == source->savedGameMinutes, "Load changed the saved actual clock");
                    if (cold)
                    {
                        stage = "cold-before-continuation";
                        failureDomain = "daylight-cold-restoration";
                        const EventRuntimeState &saved = *source->outdoorWorld.eventRuntimeState;
                        require(saved.outdoorLightTimeOverride == events().outdoorLightTimeOverride
                            && saved.eventRandomState == events().eventRandomState
                            && mapValue(saved, DayStateKey) == mapValue(events(), DayStateKey)
                            && mapValue(saved, DaylightVersionKey) == mapValue(events(), DaylightVersionKey),
                            "Cold load changed typed daylight, source selection/version or RNG");
                        const auto restored = scene().partyRuntime().snapshot();
                        const auto &old = source->outdoorParty;
                        require(restored.jumpState == old.jumpState
                            && restored.movementAccumulatorSeconds == old.movementAccumulatorSeconds
                            && std::tie(restored.movementState.x, restored.movementState.y,
                                restored.movementState.footZ, restored.movementState.verticalVelocity,
                                restored.movementState.supportKind, restored.movementState.supportBModelIndex,
                                restored.movementState.supportFaceIndex, restored.movementState.airborne)
                                == std::tie(old.movementState.x, old.movementState.y, old.movementState.footZ,
                                    old.movementState.verticalVelocity, old.movementState.supportKind,
                                    old.movementState.supportBModelIndex, old.movementState.supportFaceIndex,
                                    old.movementState.airborne), "Cold load changed the saved Party pose/motion");
                        for (size_t index = 0; index < TimeEvents.size(); ++index)
                        {
                            require(saved.mechanisms.contains(TimeEvents[index])
                                && equalMotion(saved.mechanisms.at(TimeEvents[index]),
                                    events().mechanisms.at(TimeEvents[index])),
                                "Cold load replayed source initialization or changed Time motion state");
                            checkGeometry(geometryEvidence(scene().worldRuntime(), TimeModels[index]),
                                outdoorMechanismOpenFraction(saved.mechanisms.at(TimeEvents[index]),
                                    events().outdoorModelMechanisms.at(TimeEvents[index])),
                                saved.mechanisms.at(TimeEvents[index]).isMoving);
                        }
                        checkSelection(3);
                        require(std::abs(scene().worldRuntime().lightGameMinutes()
                            - projectedOutdoorLightGameMinutes(source->savedGameMinutes,
                                saved.outdoorLightTimeOverride)) < 0.00001,
                            "Cold consumer did not reconstruct light time from saved typed state");
                        // Atmosphere.lightMinutesOfDay is derived and deliberately absent from the save codec.
                        coldChecked = true;
                        inheritedMinuteAtStart = int(std::floor(std::fmod(initialGameMinutes, 60.0)));
                        record("cold-state-equality", true);
                        save("restored");
                        beginCapture("restored");
                    }
                    else
                    {
                        failureDomain = "declared-position-fixture";
                        require(mapValue(*source->outdoorWorld.eventRuntimeState, DaylightVersionKey) == 0,
                            "Actual mode expects the prior input before the daylight feature migration");
                        scene().partyRuntime().teleportTo(1660.0f, -174.0f, 3127.01f);
                        const auto &pose = scene().partyRuntime().movementState();
                        require(!pose.airborne && pose.supportKind == OutdoorSupportKind::BModelFace
                            && pose.supportBModelIndex == 244 && pose.supportFaceIndex == 48
                            && std::abs(pose.x - 1660.0f) < 0.01f && std::abs(pose.y + 174.0f) < 0.01f
                            && std::abs(pose.footZ - 3127.01f) < 0.1f,
                            "Declared pose did not resolve to the original244/48 support");
                        const bx::Vec3 eye = {pose.x, pose.y,
                            pose.footZ + game.m_outdoorGameView.m_cameraEyeHeight};
                        const bx::Vec3 focus = {1780.5f, -174.0f, 3211.5f};
                        game.m_outdoorGameView.setCameraAngles(std::atan2(focus.y - eye.y, focus.x - eye.x),
                            std::atan2(focus.z - eye.z, std::hypot(focus.x - eye.x, focus.y - eye.y)));
                        require(mapValue(events(), DaylightVersionKey) == 1 && mapValue(events(), DayStateKey) == 1,
                            "Old input did not receive the source default daylight migration");
                        stage = "source-default-settling";
                        record("single-supported-position-and-fixed-camera", true);
                    }
                    readyWallTicks = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event)
                {
                    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                        && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                        && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                        game.handleSdlEvent(event);
                },
                [&](int width, int height, float wheel, float)
                {
                    require(SDL_GetTicks() - readyWallTicks < 60000 && ++frames <= 500,
                        "Bounded daylight observation timed out at " + stage);
                    if (captureLabel.empty() && !complete)
                    {
                        if (phase == Phase::Default)
                        {
                            if (!stationary()) advance(width, height);
                            if (stationary())
                            {
                                checkSelection(1);
                                defaultLight = scene().worldRuntime().atmosphereState();
                                stage = "source-default-4-12";
                                record("source-default-settled", true);
                                phase = Phase::ClickSecond;
                                beginCapture("default-4-12");
                            }
                        }
                        else if (phase == Phase::ClickSecond || phase == Phase::ClickThird)
                        {
                            const bool second = phase == Phase::ClickSecond;
                            stage = second ? "source102-real-pointer" : "source103-real-pointer";
                            click(second ? 102 : 103, width, height);
                            phase = second ? Phase::Second : Phase::Third;
                            phaseStartSeconds = simulationSeconds;
                            midpointCaptured = false;
                            if (!second)
                                inheritedMinuteAtStart =
                                    int(std::floor(std::fmod(scene().worldRuntime().gameMinutes(), 60.0)));
                        }
                        else if (phase == Phase::Second || phase == Phase::Third)
                        {
                            const bool second = phase == Phase::Second;
                            stage = second ? "source102-interpolation" : "source103-interpolation";
                            advance(width, height);
                            const uint32_t selected = second ? 102 : 103;
                            const float fraction = outdoorMechanismOpenFraction(events().mechanisms.at(selected),
                                events().outdoorModelMechanisms.at(selected));
                            if (fraction > 0.05f && fraction < 0.95f)
                                intermediateObserved[second ? 0 : 1] = true;
                            if (!midpointCaptured && simulationSeconds - phaseStartSeconds >= 0.9 && !stationary())
                            {
                                record("intermediate-live-vertices", true);
                                beginCapture(second ? "source102-moving" : "source103-moving");
                                midpointCaptured = true;
                            }
                            else if (stationary())
                            {
                                require(simulationSeconds - phaseStartSeconds >= 1.9
                                    && simulationSeconds - phaseStartSeconds < 2.2
                                    && intermediateObserved[second ? 0 : 1],
                                    "Source2-second interpolation or intermediate actual geometry was skipped");
                                checkSelection(second ? 2 : 3);
                                const auto &light = scene().worldRuntime().atmosphereState();
                                require(differentLight(second ? defaultLight : secondLight, light),
                                    "Actual atmosphere sun/ambient/overlay did not change with the source selection");
                                if (second) secondLight = light;
                                stage = second ? "source102-settled-6-12" : "source103-settled-11-actual-minute";
                                record("source-selection-settled", true);
                                if (!second) save("daylight-complete");
                                beginCapture(second ? "source102-6-12" : "source103-11-actual-minute");
                                phase = second ? Phase::ClickThird : Phase::Done;
                            }
                        }
                        else if (phase == Phase::ColdContinue)
                        {
                            stage = "cold-normal-continuation";
                            advance(width, height);
                            checkSelection(3);
                            if (simulationSeconds >= 0.5)
                            {
                                save("continued");
                                beginCapture("continued");
                                phase = Phase::Done;
                            }
                        }
                        else
                        {
                            checkInvariants();
                            require(fileDigest(input) == inputHash && captures == (cold ? 2u : 5u),
                                "Read-only input changed or required actual images are absent");
                            complete = true;
                            stage = "complete";
                            record("complete");
                            SDL_Event quit{};
                            quit.type = SDL_EVENT_QUIT;
                            SDL_PushEvent(&quit);
                        }
                    }
                    game.renderFrame(width, height, wheel, 0.0f);
                    if (!captureLabel.empty() && frames >= captureFrame && !captureRequested)
                    {
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureLabel.c_str());
                        captureRequested = true;
                    }
                    if (captureRequested)
                    {
                        if (auto screenshot = Engine::BgfxContext::consumeScreenshot(captureLabel))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(screenshot->width),
                                int(screenshot->height), SDL_PIXELFORMAT_BGRA32, screenshot->bgraPixels.data(),
                                int(screenshot->width * 4));
                            require(surface != nullptr, "Cannot construct captured surface");
                            const bool saved =
                                SDL_SaveBMP(surface, (output / (captureLabel + ".bmp")).string().c_str());
                            SDL_DestroySurface(surface);
                            require(saved, "Cannot save actual daylight image");
                            trace << "{\"event\":\"rendered-image\",\"label\":" << std::quoted(captureLabel)
                                << ",\"frame\":" << frames << ",\"sha256\":"
                                << std::quoted(fileDigest(output / (captureLabel + ".bmp"))) << "}\n";
                            record("captured:" + captureLabel);
                            captureLabel.clear();
                            captureRequested = false;
                            ++captures;
                        }
                    }
                },
                [&]() { game.shutdownApplication(); });
            appResult = app.run();
            if (appResult != 0 || !complete)
            {
                failureDomain = appResult == 0 ? "external-interruption" : "application-exit";
                throw std::runtime_error("Daylight flow did not finish at " + stage
                    + "; app_result=" + std::to_string(appResult));
            }
            writeResult(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            trace << "{\"event\":\"failure\",\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"failure_domain\":" << std::quoted(failureDomain)
                << ",\"error\":" << std::quoted(failure.what()) << "}\n";
            trace.flush();
            writeResult(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "br_alvar_daylight_capture FRESH_RUNTIME READONLY_SAVE NEW_OUTPUT actual|cold|loot|ordinary|actor-rules\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "BrAlvar daylight capture: " << failure.what() << '\n';
        return 1;
    }
}
