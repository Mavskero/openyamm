#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/StringUtils.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/PartyJumpState.h"
#include "game/maps/SaveGame.h"
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
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr float FrameSeconds = 1.0f / 60.0f;
constexpr float PhysicsStepSeconds = 1.0f / 128.0f;

void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

std::string digest(const std::vector<uint8_t> &bytes)
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
    require(bool(input), "Cannot read input hash");
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
    return digest(encodePartySnapshotForComparison(comparison));
}

std::map<uint64_t, Character> ownedMembers(const Party::Snapshot &party)
{
    std::map<uint64_t, Character> members;
    const auto append = [&](const Character &member)
    {
        require(member.memberId != 0 && members.emplace(member.memberId, member).second,
            "Duplicate or missing member identity");
    };
    for (const Character &member : party.members) append(member);
    for (const auto &member : party.detachedMembers) append(member.character);
    for (const auto &member : party.adventurersInnMembers) append(member.character);
    return members;
}

void checkParty(const Party::Snapshot &before, const Party::Snapshot &after)
{
    require(before.members.size() == after.members.size()
        && before.detachedMembers.size() == after.detachedMembers.size()
        && before.adventurersInnMembers.size() == after.adventurersInnMembers.size()
        && before.nextMemberId == after.nextMemberId && before.questBits == after.questBits
        && before.gold == after.gold && before.bankGold == after.bankGold && before.food == after.food,
        "Jump changed Party custody, quest bits or resources");
    require(before.pendingItemDeliveries.empty() && after.pendingItemDeliveries.empty(),
        "This bounded input must have no pending Party deliveries");
    for (size_t index = 0; index < before.members.size(); ++index)
        require(before.members[index].memberId == after.members[index].memberId,
            "Jump changed active member ordering");
    const auto first = ownedMembers(before);
    const auto last = ownedMembers(after);
    require(first.size() == last.size(), "Jump changed the owned member set");
    for (const auto &[id, member] : first)
    {
        const auto found = last.find(id);
        require(found != last.end(), "Jump lost an owned identity");
        const Character &actual = found->second;
        require(std::tie(member.rosterId, member.characterDataId, member.name, member.level, member.experience,
            member.health, member.spellPoints, member.conditions)
            == std::tie(actual.rosterId, actual.characterDataId, actual.name, actual.level, actual.experience,
                actual.health, actual.spellPoints, actual.conditions), "Jump changed character identity, HP/SP or XP");
        require(itemDigest(member) == itemDigest(actual), "Jump changed an owned inventory/equipment payload");
    }
}

bool equalMovement(const OutdoorMoveState &a, const OutdoorMoveState &b)
{
    return std::tie(a.x, a.y, a.footZ, a.verticalVelocity, a.supportKind, a.supportBModelIndex, a.supportFaceIndex,
        a.supportIsFluid, a.supportOnWater, a.supportOnBurning, a.airborne, a.landedThisFrame,
        a.fallStartZ, a.fallDistance)
        == std::tie(b.x, b.y, b.footZ, b.verticalVelocity, b.supportKind, b.supportBModelIndex, b.supportFaceIndex,
            b.supportIsFluid, b.supportOnWater, b.supportOnBurning, b.airborne, b.landedThisFrame,
            b.fallStartZ, b.fallDistance);
}

void checkSnapshot(const OutdoorPartyRuntime::Snapshot &a, const OutdoorPartyRuntime::Snapshot &b)
{
    require(a.jumpState == b.jumpState && a.movementAccumulatorSeconds == b.movementAccumulatorSeconds
        && equalMovement(a.movementState, b.movementState)
        && std::tie(a.partyMovementState.running, a.partyMovementState.flying, a.partyMovementState.activelyFlying,
            a.partyMovementState.featherFall, a.partyMovementState.waterWalk)
            == std::tie(b.partyMovementState.running, b.partyMovementState.flying, b.partyMovementState.activelyFlying,
                b.partyMovementState.featherFall, b.partyMovementState.waterWalk),
        "Saved/cold jump DTO, accumulator, movement or movement mode differs");
}

void writeJump(std::ostream &out, const PartyJumpState &state)
{
    out << std::boolalpha << "{\"pendingJumpRequested\":" << state.pendingJumpRequested
        << ",\"pendingJumpVelocity\":";
    if (state.pendingJumpVelocity) out << *state.pendingJumpVelocity;
    else out << "null";
    out << ",\"pendingJumpLift\":" << state.pendingJumpLift
        << ",\"specialJumpActive\":" << state.specialJumpActive
        << ",\"velocityX\":" << state.velocityX << ",\"velocityY\":" << state.velocityY
        << ",\"pendingSpecialJump\":" << state.pendingSpecialJump
        << ",\"pendingVelocityX\":" << state.pendingVelocityX
        << ",\"pendingVelocityY\":" << state.pendingVelocityY
        << ",\"pendingVelocityZ\":" << state.pendingVelocityZ << '}';
}
}

// Existing friend: discard only scheduled combat AI; keep actors, their collisions and source event state.
struct BreachAlvarWorldActorTestAccess
{
    static void discardQueuedAi(OutdoorWorldRuntime &world)
    {
        world.m_actorAiUpdateQueued = false;
        world.m_queuedActorAiDeltaSeconds = 0.0f;
        world.m_queuedActorAiPartyX = 0.0f;
        world.m_queuedActorAiPartyY = 0.0f;
        world.m_queuedActorAiPartyZ = 0.0f;
    }
};

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, const std::string &mode)
    {
        const bool cold = mode == "bridge-cold";
        require(cold || mode == "bridge", "Expected bridge or bridge-cold mode");
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        require(std::filesystem::is_regular_file(root / "worlds/mmmerge.zip"), "Complete controlled runtime required");
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
            && toLowerCopy(source->mapFileName) == "bralvar.odm", "Expected the real BrAlvar outdoor input: " + error);

        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"input\":" << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string()) << ",\"changes\":"
            << std::quoted(cold
                ? "Independent cold process restores a real pending/airborne source16 save exactly. No pose, "
                  "Party, quest, RNG or jump-request injection and no deliberate source re-trigger. Snapshot equality "
                  "and restored save precede continuation. Any unexpected pressure-plate repeat is recorded."
                : "The sole position fixture is BrAlvar(5888,8400,201.43), guarded by real ramp96/21 with event0. "
                  "Normal northward walk enters the original pressure plate96/50/event16; source Lua chooses its "
                  "own random128..256 elevation and1800 speed. No direct event execution, RNG, Party, quest, "
                  "buff, HP or item writes. The input carries real source128 Feather Fall from the prior actual run.")
            << ",\"physics\":\"Scene movement and World interpolation use1/60; render/UI delta0. One positive "
               "dt smaller than the remaining1/128 step checks pending retention. Save/capture pauses simulation. "
               "Only the existing friend discards transient actor-AI queues; actors and collisions remain.\","
               "\"bounds\":\"10 simulation seconds,60 wall seconds after init, "
               "no navigation or combat campaign claim\"}\n";

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
        config.appName = "OpenYAMM controlled original source jump";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::setprecision(17) << std::boolalpha;
        std::string stage = "initialize";
        std::string failureDomain = "initialization";
        std::string captureLabel;
        unsigned requestCaptureFrame = 0;
        bool captureRequested = false;
        unsigned frames = 0;
        unsigned captures = 0;
        int appResult = -1;
        bool complete = false;
        bool pendingObserved = false;
        bool shortStepChecked = false;
        bool airborneObserved = false;
        bool airborneSaved = false;
        bool retriggerObserved = false;
        bool activeObserved = false;
        unsigned activeMovingUpdates = 0;
        double simulationSeconds = 0.0;
        double launchSimulationSeconds = 0.0;
        double initialGameMinutes = 0.0;
        uint64_t readyWallTicks = 0;
        bx::Vec3 launchPose = {0.0f, 0.0f, 0.0f};
        Party::Snapshot beforeParty;
        std::unordered_map<std::string, int32_t> beforeGlobals;
        std::vector<uint64_t> actorIds;
        uint64_t nextActorId = 0;
        uint32_t sourceRandomState = 0;
        unsigned source16Observations = 0;
        std::string inputJumpKind = "none";
        enum class Phase { Approach, PendingProbe, Flight, Done };
        Phase phase = Phase::Approach;

        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Source jump left the actual outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto events = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto record = [&](const std::string &event)
        {
            const auto snapshot = scene().partyRuntime().snapshot();
            const auto &pose = snapshot.movementState;
            const auto party = scene().party().snapshot();
            trace << "{\"event\":" << std::quoted(event) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"simulation_seconds\":" << simulationSeconds
                << ",\"game_minutes\":" << scene().worldRuntime().gameMinutes()
                << ",\"map\":" << std::quoted(game.m_gameSession.currentMapFileName())
                << ",\"xyz\":[" << pose.x << ',' << pose.y << ',' << pose.footZ << "],\"vz\":"
                << pose.verticalVelocity << ",\"airborne\":" << pose.airborne
                << ",\"support\":[" << int(pose.supportKind) << ',' << pose.supportBModelIndex << ','
                << pose.supportFaceIndex << "],\"movementAccumulatorSeconds\":" << snapshot.movementAccumulatorSeconds
                << ",\"jumpState\":";
            writeJump(trace, snapshot.jumpState);
            trace << ",\"event_random_state\":" << events().eventRandomState
                << ",\"named_globals_preserved\":" << (events().namedGlobalVars == beforeGlobals)
                << ",\"quest_bits_preserved\":" << (party.questBits == beforeParty.questBits)
                << ",\"actor_count\":" << scene().worldRuntime().mapActorCount()
                << ",\"feather_fall_seconds\":" << party.partyBuffs[size_t(PartyBuffId::FeatherFall)].remainingSeconds
                << ",\"source_retrigger_observed\":" << retriggerObserved << ",\"members\":[";
            bool separator = false;
            for (const auto &[id, member] : ownedMembers(party))
            {
                if (separator) trace << ',';
                separator = true;
                trace << "{\"id\":" << id << ",\"hp\":" << member.health << ",\"sp\":" << member.spellPoints
                    << ",\"xp\":" << member.experience << ",\"items_sha256\":"
                    << std::quoted(itemDigest(member)) << '}';
            }
            trace << "]";
            if (events().lastPressurePlateTrigger)
            {
                const auto &trigger = *events().lastPressurePlateTrigger;
                trace << ",\"last_pressure_plate\":[" << trigger.eventId << ',' << trigger.bmodelIndex
                    << ',' << trigger.faceIndex << ']';
            }
            trace << "}\n";
            trace.flush();
        };
        const auto checkInvariants = [&]()
        {
            require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm",
                "Bridge jump changed maps");
            checkParty(beforeParty, scene().party().snapshot());
            require(events().namedGlobalVars == beforeGlobals, "Bridge jump changed named global/quest state");
            const auto world = scene().worldRuntime().snapshot();
            require(world.mapActors.size() == actorIds.size() && world.nextActorId == nextActorId,
                "Bridge jump created or erased an actor identity");
            for (size_t index = 0; index < actorIds.size(); ++index)
                require(world.mapActors[index].actorId == actorIds[index], "Bridge jump reordered an actor ID");
            const auto held = game.m_gameSession.gameplayUiController().heldInventoryItem();
            require(!held.active && !scene().partyRuntime().partyMovementState().flying,
                "Bridge jump used a held-item or Fly fixture");
            const auto party = scene().party().snapshot();
            const auto &initial = beforeParty.partyBuffs[size_t(PartyBuffId::FeatherFall)];
            const auto &actual = party.partyBuffs[size_t(PartyBuffId::FeatherFall)];
            const float expected = std::max(0.0f, initial.remainingSeconds - float(simulationSeconds * 30.0));
            require(std::abs(actual.remainingSeconds - expected) < 0.02f,
                "Feather Fall did not follow the real source game clock");
            if (expected > 0.0f)
                require(std::tie(initial.spellId, initial.skillLevel, initial.skillMastery,
                    initial.power, initial.casterMemberIndex)
                    == std::tie(actual.spellId, actual.skillLevel, actual.skillMastery,
                        actual.power, actual.casterMemberIndex), "Jump changed Feather Fall metadata");
            require(validPartyJumpState(scene().partyRuntime().snapshot().jumpState), "Invalid live PartyJumpState");
        };
        const auto save = [&](const std::string &name)
        {
            const auto expected = scene().partyRuntime().snapshot();
            require(game.quickSaveToPath(output / (name + ".oysav"), "原始跳跃 " + name), "Save failed: " + name);
            const auto restored = loadGameDataFromPath(output / (name + ".oysav"), error);
            require(restored.has_value(), "Immediate save readback failed: " + error);
            checkSnapshot(expected, restored->outdoorParty);
            checkParty(scene().party().snapshot(), restored->party);
            require(restored->savedGameMinutes == scene().worldRuntime().gameMinutes()
                && restored->namedGlobalVars == beforeGlobals && restored->outdoorWorld.eventRuntimeState
                && restored->outdoorWorld.eventRuntimeState->eventRandomState == events().eventRandomState,
                "Save changed clock, globals or source RNG state");
            const auto liveBuff = scene().party().snapshot().partyBuffs[size_t(PartyBuffId::FeatherFall)];
            const auto &savedBuff = restored->party.partyBuffs[size_t(PartyBuffId::FeatherFall)];
            require(std::tie(liveBuff.remainingSeconds, liveBuff.spellId, liveBuff.skillLevel, liveBuff.skillMastery,
                liveBuff.power, liveBuff.casterMemberIndex)
                == std::tie(savedBuff.remainingSeconds, savedBuff.spellId, savedBuff.skillLevel, savedBuff.skillMastery,
                    savedBuff.power, savedBuff.casterMemberIndex), "Save changed exact Feather Fall state");
            record("saved:" + name);
        };
        const auto beginCapture = [&](const std::string &name)
        {
            require(captureLabel.empty(), "Overlapping screenshots");
            captureLabel = name;
            captureRequested = false;
            requestCaptureFrame = frames + 2;
        };
        const auto advance = [&](GameplayInputFrame inputFrame, float dt, int width, int height)
        {
            failureDomain = "source-jump-physics-or-persistence";
            require(game.m_screenManager.activeScreen() == nullptr
                && !game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive,
                "Source jump encountered a modal screen");
            const uint32_t rngBefore = events().eventRandomState;
            auto &world = scene().worldRuntime();
            inputFrame.screenWidth = width;
            inputFrame.screenHeight = height;
            world.updateWorldMovement(inputFrame, dt, true);
            BreachAlvarWorldActorTestAccess::discardQueuedAi(world);
            world.updateWorld(dt);
            BreachAlvarWorldActorTestAccess::discardQueuedAi(world);
            simulationSeconds += dt;
            require(simulationSeconds <= 10.0
                && std::abs(world.gameMinutes() - initialGameMinutes - simulationSeconds * 0.5) < 0.001,
                "Jump exceeded its simulation bound or changed the source clock rate");
            if (events().eventRandomState != rngBefore && events().lastPressurePlateTrigger
                && events().lastPressurePlateTrigger->eventId == 16)
            {
                ++source16Observations;
                if (cold || source16Observations > 1) retriggerObserved = true;
                record("source16-rng-advance");
            }
            checkInvariants();
            record("physics-update");
        };
        const auto result = [&](bool passed, const std::string &reason)
        {
            const std::string status = passed ? "passed" : failureDomain == "external-interruption"
                ? "interrupted" : "failed";
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(17)
                << "{\"status\":" << std::quoted(status) << ",\"controlled\":true,\"mode\":" << std::quoted(mode)
                << ",\"stage\":" << std::quoted(stage) << ",\"complete\":" << complete
                << ",\"frames\":" << frames << ",\"captures\":" << captures << ",\"app_result\":" << appResult
                << ",\"simulation_seconds\":" << simulationSeconds << ",\"input_jump_kind\":"
                << std::quoted(inputJumpKind) << ",\"pending_observed\":" << pendingObserved
                << ",\"pending_short_step_preserved\":" << shortStepChecked
                << ",\"airborne_observed\":" << airborneObserved << ",\"active_observed\":" << activeObserved
                << ",\"active_moving_updates\":" << activeMovingUpdates
                << ",\"source16_observations\":" << source16Observations
                << ",\"source_retrigger_observed\":" << retriggerObserved
                << ",\"failure_domain\":" << std::quoted(failureDomain) << ",\"error\":" << std::quoted(reason)
                << ",\"input_sha256_before\":" << std::quoted(inputHash)
                << ",\"input_sha256_after\":" << std::quoted(fileDigest(input))
                << ",\"natural_campaign\":false,\"visual_review_required\":true}\n";
        };

        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(input), "Actual seed load failed");
                    game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                    beforeParty = source->party;
                    checkParty(beforeParty, scene().party().snapshot());
                    beforeGlobals = source->namedGlobalVars;
                    require(events().namedGlobalVars == beforeGlobals, "Cold load changed source globals");
                    require(!game.m_gameSession.gameplayUiController().heldInventoryItem().active,
                        "Use the original empty-held input");
                    const auto world = scene().worldRuntime().snapshot();
                    require(world.mapActors.size() == source->outdoorWorld.mapActors.size()
                        && world.nextActorId == source->outdoorWorld.nextActorId, "Cold actor allocation changed");
                    for (size_t index = 0; index < world.mapActors.size(); ++index)
                    {
                        require(world.mapActors[index].actorId == source->outdoorWorld.mapActors[index].actorId,
                            "Cold actor identity/order changed");
                        actorIds.push_back(world.mapActors[index].actorId);
                    }
                    nextActorId = world.nextActorId;
                    sourceRandomState = events().eventRandomState;
                    require(source->outdoorWorld.eventRuntimeState
                        && sourceRandomState == source->outdoorWorld.eventRuntimeState->eventRandomState,
                        "Cold load changed the saved source RNG");
                    initialGameMinutes = scene().worldRuntime().gameMinutes();
                    require(initialGameMinutes == source->savedGameMinutes, "Cold load changed saved game time");
                    checkSnapshot(source->outdoorParty, scene().partyRuntime().snapshot());
                    if (cold)
                    {
                        const auto &jump = source->outdoorParty.jumpState;
                        require(validPartyJumpState(jump)
                            && (jump.pendingSpecialJump || (jump.specialJumpActive
                                && source->outdoorParty.movementState.airborne)), "Cold input is not pending/airborne");
                        inputJumpKind = jump.pendingSpecialJump ? "pending" : "airborne";
                        pendingObserved = jump.pendingSpecialJump;
                        airborneObserved = source->outdoorParty.movementState.airborne;
                        activeObserved = jump.specialJumpActive;
                        phase = jump.pendingSpecialJump ? Phase::PendingProbe : Phase::Flight;
                        const auto &pose = source->outdoorParty.movementState;
                        launchPose = {pose.x, pose.y, pose.footZ};
                        stage = "cold-snapshot-equality";
                        record("cold-exact-snapshot-preserved");
                        save("restored");
                        beginCapture("restored");
                    }
                    else
                    {
                        require(!source->outdoorParty.jumpState.pendingSpecialJump
                            && !source->outdoorParty.jumpState.specialJumpActive
                            && !source->outdoorParty.movementState.airborne,
                            "Bridge input must be the actual supported prior landing");
                        require(beforeParty.partyBuffs[size_t(PartyBuffId::FeatherFall)].remainingSeconds > 0.0f,
                            "Use the prior actual source128 Feather Fall input; do not inject a buff");
                        scene().partyRuntime().teleportTo(5888.0f, 8400.0f, 201.43f);
                        const auto &pose = scene().partyRuntime().movementState();
                        require(!pose.airborne && pose.supportKind == OutdoorSupportKind::BModelFace
                            && pose.supportBModelIndex == 96 && pose.supportFaceIndex == 21
                            && std::abs(pose.x - 5888.0f) < 0.01f && std::abs(pose.y - 8400.0f) < 0.01f,
                            "Initial navigation fixture did not resolve to the original untriggered bridge ramp");
                        const auto *map = scene().worldRuntime().mapData();
                        require(map && map->bmodels.at(96).faces.at(21).cogTriggeredNumber == 0
                            && map->bmodels.at(96).faces.at(50).cogTriggeredNumber == 16
                            && hasFaceAttribute(map->bmodels.at(96).faces.at(50).attributes,
                                FaceAttribute::PressurePlate)
                            && scene().localEventProgram() && scene().localEventProgram()->hasEvent(16),
                            "Original bridge/ramp source bindings are unavailable");
                        game.m_outdoorGameView.setCameraAngles(std::numbers::pi_v<float> / 2.0f, 0.0f);
                        stage = "original-bridge-approach";
                        record("single-ramp-navigation-fixture");
                    }
                    BreachAlvarWorldActorTestAccess::discardQueuedAi(scene().worldRuntime());
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
                    require(SDL_GetTicks() - readyWallTicks < 60000, "Bounded jump capture timed out at " + stage);
                    ++frames;
                    if (captureLabel.empty() && !complete)
                    {
                        if (phase == Phase::Approach)
                        {
                            require(simulationSeconds < 3.0, "Normal northward walk did not enter the real bridge16");
                            GameplayInputFrame inputFrame;
                            inputFrame.actions[keyboardActionIndex(KeyboardAction::Forward)].held = true;
                            inputFrame.keyboardHeld[SDL_SCANCODE_W] = true;
                            inputFrame.keyboardHeld[SDL_SCANCODE_LSHIFT] =
                                scene().partyRuntime().partyMovementState().running;
                            advance(inputFrame, FrameSeconds, width, height);
                            const auto snapshot = scene().partyRuntime().snapshot();
                            if (snapshot.jumpState.pendingSpecialJump)
                            {
                                const auto &trigger = events().lastPressurePlateTrigger;
                                require(trigger && trigger->world == "outdoor" && trigger->eventId == 16
                                    && trigger->bmodelIndex == 96 && trigger->faceIndex == 50,
                                    "Pending jump did not originate from the actual original pressure plate16");
                                const auto &jump = snapshot.jumpState;
                                require(jump.pendingVelocityY < -500.0f && jump.pendingVelocityZ > 0.0f,
                                    "Original source16 did not request a horizontal southward launch");
                                const double horizontal = std::hypot(jump.pendingVelocityX, jump.pendingVelocityY);
                                trace << "{\"event\":\"observed-source-launch\",\"source_speed\":"
                                    << std::hypot(horizontal, double(jump.pendingVelocityZ)) / std::sqrt(1280.0 / 800.0)
                                    << ",\"observed_source_z_angle\":"
                                    << std::atan2(double(jump.pendingVelocityZ), horizontal) * 1024.0 / std::numbers::pi
                                    << ",\"rng_before\":" << sourceRandomState
                                    << ",\"rng_after\":" << events().eventRandomState << "}\n";
                                pendingObserved = true;
                                stage = "source16-pending";
                                const auto &pose = snapshot.movementState;
                                launchPose = {pose.x, pose.y, pose.footZ};
                                launchSimulationSeconds = simulationSeconds;
                                save("pending");
                                phase = Phase::PendingProbe;
                            }
                        }
                        else if (phase == Phase::PendingProbe)
                        {
                            stage = "pending-substep-retention";
                            const auto before = scene().partyRuntime().snapshot();
                            require(before.jumpState.pendingSpecialJump
                                && before.movementAccumulatorSeconds >= 0.0f
                                && before.movementAccumulatorSeconds < PhysicsStepSeconds,
                                "Pending input has an invalid unconsumed accumulator");
                            const float dt = (PhysicsStepSeconds - before.movementAccumulatorSeconds) * 0.25f;
                            advance({}, dt, width, height);
                            const auto after = scene().partyRuntime().snapshot();
                            require(before.jumpState == after.jumpState
                                && equalMovement(before.movementState, after.movementState)
                                && std::abs(after.movementAccumulatorSeconds
                                    - before.movementAccumulatorSeconds - dt) < 0.00000001f,
                                "A sub-1/128 step consumed pending jump data or changed physical movement");
                            shortStepChecked = true;
                            record("pending-short-step-preserved");
                            phase = Phase::Flight;
                        }
                        else if (phase == Phase::Flight)
                        {
                            stage = "unsteered-source-flight";
                            const auto before = scene().partyRuntime().snapshot();
                            advance({}, FrameSeconds, width, height);
                            const auto after = scene().partyRuntime().snapshot();
                            const auto &pose = after.movementState;
                            activeObserved = activeObserved || after.jumpState.specialJumpActive;
                            airborneObserved = airborneObserved || pose.airborne;
                            if (before.jumpState.specialJumpActive && after.jumpState.specialJumpActive
                                && std::hypot(pose.x - before.movementState.x, pose.y - before.movementState.y) > 0.5f)
                                ++activeMovingUpdates;
                            if (!cold && !airborneSaved && simulationSeconds - launchSimulationSeconds >= 0.2
                                && pose.airborne && pose.verticalVelocity > 0.0f)
                            {
                                stage = "source16-airborne";
                                save("airborne");
                                airborneSaved = true;
                                beginCapture("airborne");
                            }
                            if (airborneObserved && !pose.airborne && !after.jumpState.specialJumpActive
                                && !after.jumpState.pendingSpecialJump && pose.supportKind != OutdoorSupportKind::None)
                            {
                                stage = "source16-landed";
                                save("landed");
                                require(activeObserved && activeMovingUpdates >= 5
                                    && std::hypot(pose.x - launchPose.x, pose.y - launchPose.y) > 100.0f,
                                    "Horizontal jump did not persist across multiple actual updates");
                                require(cold || (pendingObserved && shortStepChecked && airborneSaved),
                                    "Source bridge run missed pending/substep/airborne evidence");
                                require(!retriggerObserved, "Source16 unexpectedly re-triggered during continuation");
                                phase = Phase::Done;
                                beginCapture("landed");
                            }
                        }
                        else
                        {
                            require(captures == 2 && fileDigest(input) == inputHash,
                                "Jump evidence is incomplete or changed its read-only input");
                            checkInvariants();
                            complete = true;
                            record("complete");
                            SDL_Event quit{};
                            quit.type = SDL_EVENT_QUIT;
                            SDL_PushEvent(&quit);
                        }
                    }
                    game.renderFrame(width, height, wheel, 0.0f);
                    if (!captureLabel.empty() && frames >= requestCaptureFrame && !captureRequested)
                    {
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureLabel.c_str());
                        captureRequested = true;
                    }
                    if (captureRequested)
                    {
                        if (auto screenshot = Engine::BgfxContext::consumeScreenshot(captureLabel))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(
                                int(screenshot->width), int(screenshot->height),
                                SDL_PIXELFORMAT_BGRA32, screenshot->bgraPixels.data(), int(screenshot->width * 4));
                            require(surface != nullptr, "Cannot construct captured surface");
                            const bool written =
                                SDL_SaveBMP(surface, (output / (captureLabel + ".bmp")).string().c_str());
                            SDL_DestroySurface(surface);
                            require(written, "Cannot save captured image");
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
                throw std::runtime_error("Source jump did not finish at " + stage + "; app_result="
                    + std::to_string(appResult) + "; frames=" + std::to_string(frames));
            }
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            trace << "{\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"failure_domain\":" << std::quoted(failureDomain)
                << ",\"error\":" << std::quoted(failure.what()) << "}\n";
            trace.flush();
            result(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "source_jump_capture CONTROLLED_RUNTIME READONLY_SAVE NEW_OUTPUT bridge|bridge-cold\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "source jump capture: " << failure.what() << '\n';
        return 1;
    }
}
