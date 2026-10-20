#include "doctest/doctest.h"
#include "game/content/BreachMazeLifecycle.h"
#include "game/events/GameTime.h"
#include "tools/mmerge_import/BreachMazeTemplateResources.h"

#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
const BreachMazeTemplate &sourceTemplate()
{
    static const BreachMazeTemplateProfile profile = loadBreachMazeTemplateProfile(
        std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001").value();
    return profile.mazeTemplate;
}

// Controlled lifecycle payload, not a generated population or a runnable scene. Real source geometry
// metadata sizes are used, while actors/loot/AI are explicit sentinels for preservation and isolation.
BreachMazeFloorRuntime controlledRuntime(const BreachMazeArrival &arrival)
{
    BreachMazeFloorRuntime result;
    result.yawRadians = arrival.yawRadians;
    result.pitchRadians = arrival.pitchRadians;
    result.scene.mapDeltaData.emplace();
    MapDeltaData &delta = *result.scene.mapDeltaData;
    delta.locationInfo.totalFacesCount = uint32_t(sourceTemplate().faces.size());
    delta.locationInfo.decorationCount = sourceTemplate().decorationCount;
    delta.faceAttributes.resize(sourceTemplate().faces.size());
    delta.decorationFlags.resize(sourceTemplate().decorationCount);
    delta.surfaceRevision = 7;
    delta.actors.resize(3);
    delta.actors[0].hp = 150;
    delta.actors[1].hp = 0;
    delta.actors[2].hp = -10;
    result.actorMaximumHitPoints = {200, 400, 800};
    result.actorRage = {1, 0, 1};
    result.scene.worldRuntime.mapActorAiStates.resize(3);
    result.scene.worldRuntime.mapActorCorpseViews.resize(3);
    for (size_t index = 0; index < 3; ++index)
    {
        delta.actors[index].monsterId = int16_t(20 + index);
        result.scene.worldRuntime.mapActorAiStates[index].actorId = uint32_t(index);
        result.scene.worldRuntime.mapActorAiStates[index].monsterId = delta.actors[index].monsterId;
        result.scene.worldRuntime.mapActorAiStates[index].preciseX = 31.25f + float(index);
        result.scene.worldRuntime.mapActorAiStates[index].recoverySeconds = 1.25f;
    }
    IndoorMoveState &movement = result.scene.partyRuntime.movementState;
    movement.x = arrival.x;
    movement.y = arrival.y;
    movement.footZ = arrival.footZ;
    movement.verticalVelocity = -3.25f;
    result.scene.partyRuntime.jumpState.pendingSpecialJump = true;
    result.scene.partyRuntime.jumpState.pendingVelocityX = 2.5f;
    result.scene.partyRuntime.alwaysRunEnabled = false;
    result.scene.worldRuntime.gameMinutes = 6123.75;
    result.scene.worldRuntime.sessionChestSeed = 456;
    result.scene.worldRuntime.currentLocationReputation = 3;
    result.scene.worldRuntime.searchedLootPropSourceIds = {"controlled-used-bookcase"};
    result.scene.worldRuntime.projectileState.nextProjectileId = 29;
    result.scene.worldRuntime.projectileState.projectiles.emplace_back();
    result.scene.worldRuntime.projectileState.projectiles.back().projectileId = 28;
    result.scene.worldRuntime.projectileState.projectiles.back().x = 190.5f;
    IndoorWorldRuntime::BloodSplatState blood;
    blood.sourceActorId = 1;
    blood.x = 200.5f;
    blood.radius = 19.25f;
    blood.vertices.push_back({1.5f, 2.5f, -199.0f, 0.25f, 0.75f});
    result.scene.worldRuntime.bloodSplats.push_back(blood);
    result.scene.eventRuntimeState.emplace();
    result.scene.eventRuntimeState->mapFileName = "brbase.blv";
    result.scene.eventRuntimeState->namedMapVars["controlled-floor-marker"] = 77;
    result.scene.eventRuntimeState->namedGlobalVars["controlled-session-view"] = 91;
    result.scene.eventRuntimeState->eventRandomState = 17;
    result.scene.timers.emplace_back();
    result.scene.timers.back().nextAlarmGameMinutes = 6126.75;
    result.scene.timers.back().definition.eventId = 3;
    result.scene.lastProcessedPartyMoveStateForFaceTriggers = movement;
    result.scene.lastPartyFloorFaceForPressurePlateTriggers = 0;
    result.scene.mechanismAccumulatorMilliseconds = 7.5f;
    delta.spriteObjects.emplace_back();
    delta.spriteObjects.back().rawContainingItem = {0x14, 0x23, 0x00, 0xfe};
    delta.spriteObjects.back().semanticSourceId = "controlled-dropped-item";
    delta.eventVariables.mapVars[4] = 9;
    return result;
}

// Exercise the native snapshot producer, with no renderer/assets or generated population. A short
// corpse prefix is an explicit searched-loot fixture passed through the real native restore path.
BreachMazeFloorRuntime nativeCorpseRuntime(const BreachMazeArrival &arrival, size_t corpsePrefix)
{
    BreachMazeFloorRuntime result = controlledRuntime(arrival);
    const MapStatsEntry map{};
    const MonsterTable monsters;
    const ObjectTable objects;
    const ItemTable items;
    const ChestTable chests;
    IndoorWorldRuntime world;
    world.initialize(map, monsters, objects, items, chests, nullptr, nullptr,
        &result.scene.mapDeltaData, &result.scene.eventRuntimeState, nullptr);
    IndoorWorldRuntime::Snapshot native = world.snapshot();
    REQUIRE(native.mapActorAiStates.size() == result.scene.mapDeltaData->actors.size());
    REQUIRE(native.mapActorCorpseViews.empty());
    if (corpsePrefix > 0)
    {
        REQUIRE(corpsePrefix <= result.scene.mapDeltaData->actors.size());
        native.mapActorCorpseViews.resize(corpsePrefix);
        GameplayCorpseViewState corpse;
        corpse.sourceIndex = uint32_t(corpsePrefix - 1);
        corpse.title = "controlled searched corpse";
        corpse.items.emplace_back();
        corpse.items.back().isGold = true;
        corpse.items.back().goldAmount = 17;
        native.mapActorCorpseViews.back() = std::move(corpse);
        world.restoreSnapshot(native);
    }
    result.scene.worldRuntime = world.snapshot();
    return result;
}

BreachMazeReadyFloor readyFloor(const BreachMazePreparedTransition &prepared)
{
    REQUIRE(prepared.kind() == BreachMazePlanKind::NewFloor);
    REQUIRE(prepared.target() != nullptr);
    return {prepared.target()->identity, controlledRuntime(prepared.arrival())};
}

void commitPrepared(const BreachMazeLifecycle &lifecycle, BreachMazeState &state,
    BreachMazePreparedTransition &prepared, BreachMazeQuestView &quest)
{
    std::optional<BreachMazeReadyFloor> ready;
    if (prepared.kind() == BreachMazePlanKind::NewFloor)
    {
        ready = readyFloor(prepared);
    }
    REQUIRE(lifecycle.commit(state, prepared, true, quest, ready ? &*ready : nullptr)
        == BreachMazeCommitResult::Committed);
    if (prepared.questEffects().setEnteredBasement)
    {
        quest.enteredBasement = true;
    }
}

void enter(const BreachMazeLifecycle &lifecycle, BreachMazeState &state, BreachMazeQuestView &quest,
    BreachMazeEntry entry = BreachMazeEntry::BrAlvar53, uint64_t seed = UINT64_C(0x0123456789abcdef))
{
    BreachMazePreparedTransition prepared = lifecycle.prepareFreshEntry(state, entry, quest, seed);
    commitPrepared(lifecycle, state, prepared, quest);
}

void step(const BreachMazeLifecycle &lifecycle, BreachMazeState &state, BreachMazeQuestView &quest,
    BreachMazeStep direction)
{
    REQUIRE(state.activeFloor() != nullptr);
    BreachMazePreparedTransition prepared = lifecycle.prepareStep(
        state, direction, quest, state.activeFloor()->runtime);
    commitPrepared(lifecycle, state, prepared, quest);
}

std::vector<uint32_t> levels(const BreachMazeState &state)
{
    std::vector<uint32_t> result;
    for (const auto &[level, floor] : state.data().floors)
    {
        result.push_back(level);
    }
    return result;
}

void checkColdPayload(const BreachMazeFloorRuntime &runtime)
{
    REQUIRE(runtime.scene.mapDeltaData.has_value());
    CHECK(runtime.scene.mapDeltaData->actors[0].hp == 150);
    CHECK(runtime.scene.mapDeltaData->actors[1].hp == 0);
    CHECK(runtime.scene.mapDeltaData->actors[2].hp == -10);
    CHECK((runtime.actorMaximumHitPoints == std::vector<uint32_t>{200, 400, 800}));
    CHECK((runtime.actorRage == std::vector<uint8_t>{1, 0, 1}));
    CHECK(runtime.scene.worldRuntime.bloodSplats.size() == 1);
    CHECK(runtime.scene.worldRuntime.bloodSplats.at(0).x == 200.5f);
    CHECK(runtime.scene.worldRuntime.bloodSplats.at(0).vertices.at(0).v == 0.75f);
    CHECK(runtime.scene.worldRuntime.mapActorAiStates.at(0).preciseX == 31.25f);
    CHECK(runtime.scene.worldRuntime.mapActorAiStates.at(0).recoverySeconds == 1.25f);
    CHECK(runtime.scene.worldRuntime.projectileState.nextProjectileId == 29);
    CHECK(runtime.scene.worldRuntime.projectileState.projectiles.at(0).x == 190.5f);
    CHECK(runtime.scene.worldRuntime.searchedLootPropSourceIds.at(0) == "controlled-used-bookcase");
    CHECK(runtime.scene.worldRuntime.gameMinutes == 6123.75);
    CHECK(runtime.scene.worldRuntime.sessionChestSeed == 456);
    CHECK(runtime.scene.worldRuntime.currentLocationReputation == 3);
    CHECK(runtime.scene.mapDeltaData->surfaceRevision == 7);
    CHECK(runtime.scene.mapDeltaData->eventVariables.mapVars[4] == 9);
    CHECK((runtime.scene.mapDeltaData->spriteObjects.at(0).rawContainingItem
        == std::vector<uint8_t>{0x14, 0x23, 0x00, 0xfe}));
    CHECK(runtime.scene.mapDeltaData->spriteObjects.at(0).semanticSourceId == "controlled-dropped-item");
    REQUIRE(runtime.scene.eventRuntimeState.has_value());
    CHECK(runtime.scene.eventRuntimeState->namedMapVars.at("controlled-floor-marker") == 77);
    CHECK(runtime.scene.eventRuntimeState->namedGlobalVars.at("controlled-session-view") == 91);
    CHECK(runtime.scene.eventRuntimeState->eventRandomState == 17);
    CHECK(runtime.scene.timers.at(0).nextAlarmGameMinutes == 6126.75);
    CHECK(runtime.scene.timers.at(0).definition.eventId == 3);
    CHECK(runtime.scene.partyRuntime.movementState.verticalVelocity == -3.25f);
    CHECK(runtime.scene.partyRuntime.jumpState.pendingVelocityX == 2.5f);
    CHECK_FALSE(runtime.scene.partyRuntime.alwaysRunEnabled);
    CHECK(runtime.scene.lastPartyFloorFaceForPressurePlateTriggers == 0);
    CHECK(runtime.scene.mechanismAccumulatorMilliseconds == 7.5f);
}

BreachMazeRunData moveSingleFloorTo(const BreachMazeState &state, uint32_t level, uint64_t epoch)
{
    BreachMazeRunData data = state.data();
    BreachMazeFloorState floor = data.floors.at(data.currentLevel);
    floor.descriptor.identity.level = level;
    floor.descriptor.identity.epoch = epoch;
    floor.descriptor.seeds = deriveBreachMazeFloorSeeds(data.rootSeed, floor.descriptor.identity);
    floor.descriptor.layout = generateBreachMazeLayout(sourceTemplate(), level, floor.descriptor.seeds.layout);
    data.floors.clear();
    data.floors.emplace(level, std::move(floor));
    data.currentLevel = level;
    data.epoch = epoch;
    data.direction = BreachMazeDirection::Down;
    return data;
}
}

TEST_CASE("Breach lifecycle seed domains have fixed version-one integer goldens")
{
    CHECK(BreachMazeSeedVersion == 1);
    struct Vector
    {
        uint64_t rootSeed;
        BreachMazeFloorIdentity identity;
        BreachMazeFloorSeeds expected;
    };
    // Independent Python integer oracle recorded in checkpoint148/implementation-001/seed-goldens.json.
    const Vector vectors[] = {
        {0, {1, 3, 1}, {UINT64_C(0x9e215b82540ed06a), UINT64_C(0x9cacebaf6ffd1cb4),
            UINT64_C(0xb2475ed0edf0d2f5), UINT64_C(0x53ee3ee951393838)}},
        {UINT64_C(0x0123456789abcdef), {1, 1, 1}, {UINT64_C(0x4c445113e2e2df04), UINT64_C(0x4735039432eb21c9),
            UINT64_C(0x326c36c8802e908e), UINT64_C(0x92069363a9d6f477)}},
        {UINT64_MAX, {UINT64_MAX - 1, BreachMazeMaximumPlannedLevel, UINT64_MAX - 1},
            {UINT64_C(0xd46131f02e75db11), UINT64_C(0x44bbad29f6c19f48),
                UINT64_C(0x9017a849b8b0f337), UINT64_C(0xaf5acfcb699c77ad)}},
        {UINT64_C(0x0123456789abcdef), {2, 3, 9}, {UINT64_C(0x4eeaea8a8439e8ac), UINT64_C(0x4eeb0effcd2234b6),
            UINT64_C(0x8d4c4c5e9085e876), UINT64_C(0xcfb71a96db2b9776)}},
    };
    for (const Vector &vector : vectors)
    {
        const BreachMazeFloorSeeds actual = deriveBreachMazeFloorSeeds(vector.rootSeed, vector.identity);
        CHECK(actual == vector.expected);
        CHECK((std::set<uint64_t>{actual.layout, actual.material, actual.population, actual.interaction}.size() == 4));
    }
    CHECK_THROWS_AS(deriveBreachMazeFloorSeeds(0, {0, 1, 1}), std::invalid_argument);
    CHECK_THROWS_AS(deriveBreachMazeFloorSeeds(0, {1, 0, 1}), std::invalid_argument);
    CHECK_THROWS_AS(deriveBreachMazeFloorSeeds(0, {1, 1, 0}), std::invalid_argument);
    CHECK_THROWS_AS(deriveBreachMazeFloorSeeds(0, {1, UINT32_MAX, 1}), std::invalid_argument);
}

TEST_CASE("Breach lifecycle entry53 starts level3 at exit while entry54 never resets the quest flag")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    for (BreachMazeEntry entry : {BreachMazeEntry::BrAlvar53, BreachMazeEntry::BrAlvar54, BreachMazeEntry::Breach54})
    {
        for (bool present : {false, true})
        {
            for (bool entered : {false, true})
            {
                for (bool finished : {false, true})
                {
                    BreachMazeState state;
                    BreachMazeQuestView quest{present, entered, finished};
                    const BreachMazeRunData *pBefore = &state.data();
                    BreachMazePreparedTransition plan = lifecycle.prepareFreshEntry(state, entry, quest, 0);
                    const bool first = present && !finished && (entry == BreachMazeEntry::BrAlvar53 || !entered);
                    REQUIRE(plan.target() != nullptr);
                    CHECK(plan.target()->identity.level == (first ? 3 : 1));
                    CHECK(plan.arrival().kind == (first
                        ? BreachMazeArrivalKind::Exit : BreachMazeArrivalKind::Entrance));
                    const uint16_t cellId = first ? plan.target()->layout.endpoints.exit.templateCellId
                        : plan.target()->layout.endpoints.entrance.templateCellId;
                    const BreachMazeTemplateCell &cell = sourceTemplate().cells.at(cellId - 1);
                    CHECK(plan.arrival().x == float((cell.x - 1) * 601.5));
                    CHECK(plan.arrival().y == float(-(cell.y - 1) * 601.5));
                    CHECK(plan.arrival().footZ == -200.0f);
                    CHECK(plan.questEffects().setEnteredBasement == present);
                    CHECK_FALSE(plan.questEffects().setEnteredBreach);
                    CHECK(plan.requiresSourceEntryReset());
                    CHECK(&state.data() == pBefore);
                    commitPrepared(lifecycle, state, plan, quest);
                    CHECK(state.data().epoch == 1);
                    CHECK(state.data().rootSeed == 0);
                    CHECK(state.data().nextGenerationSerial == 2);
                    CHECK(quest.questFinished == finished);
                    CHECK(quest.enteredBasement == (present || entered));
                    CHECK_THROWS_AS(lifecycle.prepareFreshEntry(state, entry, quest, 1), std::invalid_argument);
                }
            }
        }
    }
    // Fractional centers are preserved; no conversion through PendingMapMove's integral coordinates.
    bool foundHalfUnit = false;
    for (uint64_t seed = 0; seed < 32; ++seed)
    {
        const BreachMazePreparedTransition plan = lifecycle.prepareFreshEntry(
            BreachMazeState(), BreachMazeEntry::BrAlvar53, {true, false, false}, seed);
        foundHalfUnit = foundHalfUnit || std::floor(plan.arrival().x) != plan.arrival().x
            || std::floor(plan.arrival().y) != plan.arrival().y;
    }
    CHECK(foundHalfUnit);
}

TEST_CASE("Breach lifecycle Down lock precedes capture and never prompts or mutates")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, false, false};
    enter(lifecycle, state, quest);
    const BreachMazeRunData *pBefore = &state.data();
    BreachMazeFloorRuntime deliberatelyMissingDelta;
    BreachMazePreparedTransition locked = lifecycle.prepareStep(
        state, BreachMazeStep::Down, quest, deliberatelyMissingDelta);
    CHECK(locked.kind() == BreachMazePlanKind::DownLocked);
    CHECK(locked.target() == nullptr);
    CHECK_FALSE(locked.departure().has_value());
    CHECK_FALSE(locked.requiresSourceEntryReset());
    CHECK_FALSE(locked.questEffects().setEnteredBasement);
    CHECK(lifecycle.commit(state, locked, true, quest) == BreachMazeCommitResult::DownLocked);
    CHECK(&state.data() == pBefore);
    CHECK(state.data().nextGenerationSerial == 2);
    CHECK(state.data().currentLevel == 3);
}

TEST_CASE("Breach lifecycle prunes old Cur plus-minus two and returns to saved pose before rerolling evicted floors")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, false, false};
    enter(lifecycle, state, quest);
    const BreachMazeFloorDescriptor originalThree = state.activeFloor()->descriptor;
    BreachMazePreparedTransition up = lifecycle.prepareStep(
        state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
    CHECK(up.arrival().kind == BreachMazeArrivalKind::Exit);
    commitPrepared(lifecycle, state, up, quest);
    BreachMazeFloorRuntime second = state.activeFloor()->runtime;
    second.scene.partyRuntime.movementState.x = 222.5f;
    second.scene.partyRuntime.movementState.y = -333.25f;
    second.scene.partyRuntime.movementState.footZ = -187.5f;
    second.yawRadians = 1.125f;
    second.pitchRadians = -0.375f;
    second.scene.mapDeltaData->actors[0].hp = 49;
    second.scene.eventRuntimeState->namedMapVars["controlled-floor-marker"] = 2;
    lifecycle.captureCurrentFloor(state, second);
    step(lifecycle, state, quest, BreachMazeStep::Up);
    CHECK((levels(state) == std::vector<uint32_t>{1, 2, 3}));
    CHECK(state.data().currentLevel == 1);
    CHECK(state.data().nextGenerationSerial == 4);
    const BreachMazeState roundTrip = lifecycle.restore(state.data());
    CHECK((levels(roundTrip) == std::vector<uint32_t>{1, 2, 3}));
    CHECK(roundTrip.data().direction == BreachMazeDirection::Up);
    CHECK(roundTrip.data().floors.at(2).runtime.scene.mapDeltaData->actors[0].hp == 49);
    CHECK(roundTrip.data().floors.at(2).runtime.yawRadians == 1.125f);
    CHECK(roundTrip.data().floors.at(3).descriptor.seeds == originalThree.seeds);
    CHECK(lifecycle.coldRestore(roundTrip).descriptor.identity.level == 1);

    // Explicit postQuest planning fixture; no product quest reward or final lifecycle is invoked.
    quest.questFinished = true;
    const BreachMazeRunData *pBefore = &state.data();
    BreachMazePreparedTransition down = lifecycle.prepareStep(
        state, BreachMazeStep::Down, quest, state.activeFloor()->runtime);
    CHECK(&state.data() == pBefore);
    CHECK(state.data().floors.contains(3));
    REQUIRE(down.retainedFloor() != nullptr);
    CHECK(down.arrival().kind == BreachMazeArrivalKind::SavedPose);
    CHECK(down.arrival().x == 222.5f);
    CHECK(down.arrival().y == -333.25f);
    CHECK(down.arrival().footZ == -187.5f);
    CHECK(down.arrival().yawRadians == 0.0f); // Source TriggerReload, not a cold load.
    CHECK(down.arrival().pitchRadians == 0.0f);
    CHECK(state.data().floors.at(2).runtime.yawRadians == 1.125f); // Preparation has not changed live state.
    CHECK(state.data().floors.at(2).runtime.pitchRadians == -0.375f);
    CHECK(down.retainedFloor()->runtime.scene.mapDeltaData->actors[0].hp == 49);
    CHECK(down.retainedFloor()->runtime.scene.eventRuntimeState->namedMapVars.at("controlled-floor-marker") == 2);
    commitPrepared(lifecycle, state, down, quest);
    CHECK((levels(state) == std::vector<uint32_t>{1, 2}));
    CHECK(state.data().nextGenerationSerial == 4);
    CHECK(state.activeFloor()->runtime.yawRadians == 0.0f);
    CHECK(state.activeFloor()->runtime.pitchRadians == 0.0f);

    BreachMazePreparedTransition regenerate = lifecycle.prepareStep(
        state, BreachMazeStep::Down, quest, state.activeFloor()->runtime);
    CHECK(regenerate.kind() == BreachMazePlanKind::NewFloor);
    CHECK(regenerate.arrival().kind == BreachMazeArrivalKind::Entrance);
    REQUIRE(regenerate.target() != nullptr);
    CHECK(regenerate.target()->identity.level == 3);
    CHECK(regenerate.target()->identity.generationSerial == 4);
    CHECK(regenerate.target()->seeds != originalThree.seeds);
    commitPrepared(lifecycle, state, regenerate, quest);
    CHECK((levels(state) == std::vector<uint32_t>{1, 2, 3}));
    CHECK(state.data().nextGenerationSerial == 5);
    CHECK(state.data().floors.at(2).runtime.scene.mapDeltaData->actors[0].hp == 49);
    CHECK(state.data().floors.at(3).runtime.scene.mapDeltaData->actors[0].hp == 150);
    step(lifecycle, state, quest, BreachMazeStep::Down);
    CHECK((levels(state) == std::vector<uint32_t>{2, 3, 4}));
}

TEST_CASE("Breach lifecycle level1 exit uses exact source destinations and fresh reentry starts a new epoch")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    for (BreachMazeQuestView quest : {BreachMazeQuestView{}, BreachMazeQuestView{true, true, false},
             BreachMazeQuestView{true, true, true}})
    {
        BreachMazeState state;
        enter(lifecycle, state, quest, BreachMazeEntry::BrAlvar54);
        REQUIRE(state.data().currentLevel == 1);
        BreachMazePreparedTransition exit = lifecycle.prepareStep(
            state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
        CHECK(exit.kind() == BreachMazePlanKind::LeaveMaze);
        CHECK(exit.target() == nullptr);
        CHECK_FALSE(exit.requiresSourceEntryReset());
        REQUIRE(exit.departure().has_value());
        CHECK(exit.departure()->mapFileName == (!quest.present ? "out02.odm"
            : quest.questFinished ? "Breach.odm" : "BrAlvar.odm"));
        CHECK(exit.departure()->x == (quest.present ? 1578 : 20487));
        CHECK(exit.departure()->y == (quest.present ? 899 : -4443));
        CHECK(exit.departure()->z == (quest.present ? 847 : 2));
        CHECK(exit.departure()->yawUnits == (quest.present ? 1160 : 1167));
        CHECK(exit.questEffects().setEnteredBreach == quest.present);
        const bool finished = quest.questFinished;
        commitPrepared(lifecycle, state, exit, quest);
        CHECK_FALSE(state.data().inside);
        CHECK(state.activeFloor() == nullptr);
        CHECK(quest.questFinished == finished);
        CHECK_THROWS_AS(lifecycle.coldRestore(state), std::invalid_argument);
        const BreachMazeFloorSeeds oldSeeds = state.data().floors.at(1).descriptor.seeds;
        const BreachMazeRunData *pExited = &state.data();
        BreachMazePreparedTransition fresh = lifecycle.prepareFreshEntry(
            state, BreachMazeEntry::BrAlvar54, quest, UINT64_C(0x0123456789abcdef));
        REQUIRE(fresh.target() != nullptr);
        CHECK(fresh.target()->identity.epoch == 2);
        CHECK(fresh.target()->identity.generationSerial == 1);
        CHECK(fresh.target()->seeds != oldSeeds);
        CHECK(&state.data() == pExited);
        commitPrepared(lifecycle, state, fresh, quest);
        CHECK(state.data().epoch == 2);
        CHECK(state.data().floors.size() == 1);
        CHECK(state.data().nextGenerationSerial == 2);
    }
}

TEST_CASE("Breach lifecycle cold restore preserves Fresh floor combat ceilings rage blood and exact pose")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, false, false};
    enter(lifecycle, state, quest);
    REQUIRE(state.data().direction == BreachMazeDirection::Fresh);
    BreachMazeFloorRuntime live = state.activeFloor()->runtime;
    live.scene.partyRuntime.movementState.x = 1301.25f;
    live.scene.partyRuntime.movementState.y = -1987.5f;
    live.scene.partyRuntime.movementState.footZ = -190.75f;
    live.yawRadians = 2.75f;
    live.pitchRadians = -0.3125f;
    lifecycle.captureCurrentFloor(state, live);
    const BreachMazeRunData *pBefore = &state.data();
    const BreachMazeFloorState &warm = lifecycle.coldRestore(state);
    CHECK(&warm == state.activeFloor());
    CHECK(&state.data() == pBefore);
    const BreachMazeState restored = lifecycle.restore(state.data()); // Codec-shaped value round trip, not file I/O.
    const BreachMazeFloorState &cold = lifecycle.coldRestore(restored);
    CHECK(restored.data().revision == state.data().revision);
    CHECK(restored.data().nextGenerationSerial == state.data().nextGenerationSerial);
    CHECK(cold.descriptor.identity == warm.descriptor.identity);
    CHECK(cold.descriptor.seeds == warm.descriptor.seeds);
    CHECK(cold.descriptor.layout == warm.descriptor.layout);
    CHECK(cold.runtime.scene.partyRuntime.movementState.x == 1301.25f);
    CHECK(cold.runtime.scene.partyRuntime.movementState.y == -1987.5f);
    CHECK(cold.runtime.scene.partyRuntime.movementState.footZ == -190.75f);
    CHECK(cold.runtime.yawRadians == 2.75f);
    CHECK(cold.runtime.pitchRadians == -0.3125f);
    checkColdPayload(cold.runtime);
    // No shared mutable snapshot storage and no healing of the dead/negative-HP actors.
    live.scene.mapDeltaData->actors[0].hp = 1;
    live.actorMaximumHitPoints[0] = 1;
    live.actorRage.clear();
    live.scene.worldRuntime.bloodSplats.clear();
    checkColdPayload(cold.runtime);
    CHECK_FALSE(quest.questFinished);
}

TEST_CASE("Breach lifecycle prepare cancel stale confirmation and bad staged payloads leave state untouched")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, false, false};
    const BreachMazeRunData *pEmpty = &state.data();
    BreachMazePreparedTransition cancelled = lifecycle.prepareFreshEntry(state, BreachMazeEntry::BrAlvar53, quest, 41);
    const BreachMazeFloorSeeds cancelledSeeds = cancelled.target()->seeds;
    CHECK(lifecycle.commit(state, cancelled, false, quest) == BreachMazeCommitResult::Cancelled);
    CHECK(&state.data() == pEmpty);
    CHECK(lifecycle.commit(state, cancelled, true, quest) == BreachMazeCommitResult::Consumed);
    BreachMazePreparedTransition retry = lifecycle.prepareFreshEntry(state, BreachMazeEntry::BrAlvar53, quest, 41);
    CHECK(retry.target()->seeds == cancelledSeeds);
    BreachMazeReadyFloor ready = readyFloor(retry);
    CHECK_THROWS_AS(lifecycle.commit(state, retry, true, quest), std::invalid_argument);
    CHECK(&state.data() == pEmpty);
    ++ready.identity.generationSerial;
    CHECK_THROWS_AS(lifecycle.commit(state, retry, true, quest, &ready), std::invalid_argument);
    --ready.identity.generationSerial;
    ready.runtime.scene.partyRuntime.movementState.x += 0.5f;
    CHECK_THROWS_AS(lifecycle.commit(state, retry, true, quest, &ready), std::invalid_argument);
    ready = readyFloor(retry);
    ready.runtime.actorMaximumHitPoints[0] = 99;
    CHECK_THROWS_AS(lifecycle.commit(state, retry, true, quest, &ready), std::invalid_argument);
    CHECK(&state.data() == pEmpty);
    CHECK(lifecycle.commit(state, retry, true, {true, false, true}, &ready) == BreachMazeCommitResult::StaleQuest);
    CHECK(&state.data() == pEmpty);
    ready = readyFloor(retry);
    REQUIRE(lifecycle.commit(state, retry, true, quest, &ready) == BreachMazeCommitResult::Committed);
    CHECK(lifecycle.commit(state, retry, true, quest, &ready) == BreachMazeCommitResult::Consumed);
    quest.enteredBasement = true;
    BreachMazePreparedTransition first = lifecycle.prepareStep(
        state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
    BreachMazePreparedTransition second = lifecycle.prepareStep(
        state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
    CHECK(first.target()->layout == second.target()->layout);
    commitPrepared(lifecycle, state, first, quest);
    const BreachMazeRunData *pCommitted = &state.data();
    CHECK(lifecycle.commit(state, second, true, quest) == BreachMazeCommitResult::StaleState);
    CHECK(&state.data() == pCommitted);
    BreachMazePreparedTransition beforeCapture = lifecycle.prepareStep(
        state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
    lifecycle.captureCurrentFloor(state, state.activeFloor()->runtime);
    const BreachMazeRunData *pCaptured = &state.data();
    CHECK(lifecycle.commit(state, beforeCapture, true, quest) == BreachMazeCommitResult::StaleState);
    CHECK(&state.data() == pCaptured);
    BreachMazePreparedTransition beforeMove = lifecycle.prepareStep(
        state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
    BreachMazePreparedTransition moved = std::move(beforeMove);
    CHECK(lifecycle.commit(state, beforeMove, true, quest) == BreachMazeCommitResult::Consumed);
    commitPrepared(lifecycle, state, moved, quest);
}

TEST_CASE("Breach lifecycle failed generation or cancellation cannot prematurely prune a retained floor")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, false, false};
    enter(lifecycle, state, quest);
    step(lifecycle, state, quest, BreachMazeStep::Up);
    step(lifecycle, state, quest, BreachMazeStep::Up);
    quest.questFinished = true; // Controlled postQuest planning only.
    const BreachMazeRunData *pAllThree = &state.data();
    BreachMazePreparedTransition cancelled = lifecycle.prepareStep(
        state, BreachMazeStep::Down, quest, state.activeFloor()->runtime);
    CHECK(lifecycle.commit(state, cancelled, false, quest) == BreachMazeCommitResult::Cancelled);
    CHECK(&state.data() == pAllThree);
    CHECK((levels(state) == std::vector<uint32_t>{1, 2, 3}));
    step(lifecycle, state, quest, BreachMazeStep::Down);
    step(lifecycle, state, quest, BreachMazeStep::Down);
    const BreachMazeRunData *pBeforeNewFour = &state.data();
    BreachMazePreparedTransition newFour = lifecycle.prepareStep(
        state, BreachMazeStep::Down, quest, state.activeFloor()->runtime);
    REQUIRE(newFour.kind() == BreachMazePlanKind::NewFloor);
    CHECK_THROWS_AS(lifecycle.commit(state, newFour, true, quest), std::invalid_argument);
    CHECK(&state.data() == pBeforeNewFour);
    CHECK(state.data().floors.contains(1));
    CHECK(state.data().nextGenerationSerial == 5);
    commitPrepared(lifecycle, state, newFour, quest);
    CHECK((levels(state) == std::vector<uint32_t>{2, 3, 4}));
    CHECK(state.data().nextGenerationSerial == 6);
}

TEST_CASE("Breach lifecycle rejects corrupt restore payloads without silently regenerating them")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, false, false};
    enter(lifecycle, state, quest);
    const BreachMazeRunData *pBefore = &state.data();
    const std::vector<std::function<void(BreachMazeRunData &)>> corruptions = {
        [](BreachMazeRunData &data) { ++data.version; },
        [](BreachMazeRunData &data) { ++data.seedVersion; },
        [](BreachMazeRunData &data) { data.templateFingerprint ^= 1; },
        [](BreachMazeRunData &data) { data.revision = 0; },
        [](BreachMazeRunData &data) { data.epoch = 0; },
        [](BreachMazeRunData &data) { data.nextGenerationSerial = 1; },
        [](BreachMazeRunData &data) { data.currentLevel = 0; },
        [](BreachMazeRunData &data) { data.direction = BreachMazeDirection::Exited; },
        [](BreachMazeRunData &data) { data.floors.clear(); },
        [](BreachMazeRunData &data) { data.floors.at(3).descriptor.identity.epoch += 1; },
        [](BreachMazeRunData &data) { data.floors.at(3).descriptor.identity.level = 2; },
        [](BreachMazeRunData &data) { data.floors.at(3).descriptor.identity.generationSerial = 0; },
        [](BreachMazeRunData &data) { data.floors.at(3).descriptor.seeds.material ^= 1; },
        [](BreachMazeRunData &data) { data.floors.at(3).descriptor.layout.cells.at(0).walls[0] = false; },
        [](BreachMazeRunData &data) { ++data.floors.at(3).descriptor.layout.randomDrawCount; },
        [](BreachMazeRunData &data) { ++data.floors.at(3).descriptor.layout.endpoints.exit.faceIndex; },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.scene.mapDeltaData.reset(); },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.scene.mapDeltaData->faceAttributes.pop_back(); },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.scene.mapDeltaData->decorationFlags.clear(); },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.scene.mapDeltaData->locationInfo.totalFacesCount = 1; },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.actorMaximumHitPoints.pop_back(); },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.actorMaximumHitPoints[0] = 149; },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.actorMaximumHitPoints[1] = 0; },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.actorRage[0] = 2; },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.actorRage.clear(); },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.scene.worldRuntime.mapActorAiStates.clear(); },
        [](BreachMazeRunData &data)
        {
            BreachMazeFloorRuntime &runtime = data.floors.at(3).runtime;
            runtime.scene.worldRuntime.mapActorCorpseViews.resize(runtime.scene.mapDeltaData->actors.size() + 1);
        },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.yawRadians = std::numeric_limits<float>::infinity(); },
        [](BreachMazeRunData &data)
        {
            data.floors.at(3).runtime.scene.partyRuntime.movementState.x = std::numeric_limits<float>::quiet_NaN();
        },
        [](BreachMazeRunData &data)
        {
            data.floors.at(3).runtime.scene.partyRuntime.movementState.supportFaceIndex = sourceTemplate().faces.size();
        },
        [](BreachMazeRunData &data) { data.floors.at(3).runtime.scene.worldRuntime.gameMinutes = -1.0; },
        [](BreachMazeRunData &data)
        {
            data.floors.at(3).runtime.scene.timers.at(0).nextAlarmGameMinutes = std::numeric_limits<double>::infinity();
        },
        [](BreachMazeRunData &data)
        {
            data.floors.at(3).runtime.scene.worldRuntime.bloodSplats.at(0).vertices.at(0).x
                = std::numeric_limits<float>::quiet_NaN();
        },
    };
    for (size_t index = 0; index < corruptions.size(); ++index)
    {
        CAPTURE(index);
        BreachMazeRunData corrupt = state.data();
        corruptions[index](corrupt);
        CHECK_THROWS_AS(lifecycle.restore(std::move(corrupt)), std::invalid_argument);
        CHECK(&state.data() == pBefore);
    }
    BreachMazeFloorRuntime badCapture = state.activeFloor()->runtime;
    badCapture.actorMaximumHitPoints.clear();
    CHECK_THROWS_AS(lifecycle.captureCurrentFloor(state, badCapture), std::invalid_argument);
    CHECK(&state.data() == pBefore);
    BreachMazeRunData empty;
    empty.rootSeed = 1;
    CHECK_THROWS_AS(lifecycle.restore(std::move(empty)), std::invalid_argument);
    BreachMazeTemplate changedTemplate = sourceTemplate();
    ++changedTemplate.vertexCount; // Same wall topology; full metadata identity must still differ.
    const BreachMazeLifecycle otherTemplate(std::move(changedTemplate));
    CHECK_THROWS_AS(otherTemplate.restore(state.data()), std::invalid_argument);
    CHECK_THROWS_AS(otherTemplate.coldRestore(state), std::invalid_argument);
    CHECK(&state.data() == pBefore);

    step(lifecycle, state, quest, BreachMazeStep::Up);
    BreachMazeRunData duplicate = state.data();
    duplicate.floors.at(2).descriptor.identity.generationSerial =
        duplicate.floors.at(3).descriptor.identity.generationSerial;
    duplicate.floors.at(2).descriptor.seeds = deriveBreachMazeFloorSeeds(duplicate.rootSeed,
        duplicate.floors.at(2).descriptor.identity);
    duplicate.floors.at(2).descriptor.layout = generateBreachMazeLayout(sourceTemplate(), 2,
        duplicate.floors.at(2).descriptor.seeds.layout);
    CHECK_THROWS_AS(lifecycle.restore(std::move(duplicate)), std::invalid_argument);
}

TEST_CASE("Breach lifecycle accepts native empty and sparse corpse snapshots without materializing loot")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    const BreachMazeQuestView quest{true, false, false};
    for (size_t prefix : {size_t(0), size_t(2)})
    {
        CAPTURE(prefix);
        BreachMazeState state;
        BreachMazePreparedTransition prepared = lifecycle.prepareFreshEntry(
            state, BreachMazeEntry::BrAlvar53, quest, 41);
        BreachMazeReadyFloor ready{prepared.target()->identity, nativeCorpseRuntime(prepared.arrival(), prefix)};
        REQUIRE(lifecycle.commit(state, prepared, true, quest, &ready) == BreachMazeCommitResult::Committed);
        lifecycle.captureCurrentFloor(state, ready.runtime);
        const BreachMazeState restored = lifecycle.restore(state.data());
        const BreachMazeFloorRuntime &cold = lifecycle.coldRestore(restored).runtime;
        REQUIRE(cold.scene.worldRuntime.mapActorCorpseViews.size() == prefix);
        CHECK(cold.scene.worldRuntime.mapActorAiStates.size() == 3);
        CHECK(cold.scene.mapDeltaData->actors[0].hp == 150);
        CHECK(cold.scene.mapDeltaData->actors[1].hp == 0);
        CHECK(cold.scene.mapDeltaData->actors[2].hp == -10);
        if (prefix > 0)
        {
            CHECK_FALSE(cold.scene.worldRuntime.mapActorCorpseViews.front().has_value());
            REQUIRE(cold.scene.worldRuntime.mapActorCorpseViews.back().has_value());
            const GameplayCorpseViewState &corpse = *cold.scene.worldRuntime.mapActorCorpseViews.back();
            CHECK(corpse.sourceIndex == prefix - 1);
            CHECK(corpse.title == "controlled searched corpse");
            REQUIRE(corpse.items.size() == 1);
            CHECK(corpse.items.front().isGold);
            CHECK(corpse.items.front().goldAmount == 17);
        }
        // A bad cache must not commit or consume a new generation token, and cannot enter via
        // capture or a codec-shaped restore either. Neither case requires generating any loot.
        for (bool wrongSourceIndex : {false, true})
        {
            CAPTURE(wrongSourceIndex);
            BreachMazePreparedTransition next = lifecycle.prepareStep(
                state, BreachMazeStep::Up, quest, state.activeFloor()->runtime);
            BreachMazeReadyFloor bad = readyFloor(next);
            bad.runtime.scene.worldRuntime.mapActorCorpseViews.resize(4);
            if (wrongSourceIndex)
            {
                bad.runtime.scene.worldRuntime.mapActorCorpseViews.resize(2);
                bad.runtime.scene.worldRuntime.mapActorCorpseViews[1].emplace();
                bad.runtime.scene.worldRuntime.mapActorCorpseViews[1]->sourceIndex = 0;
            }
            const BreachMazeRunData *pBefore = &state.data();
            CHECK_THROWS_AS(lifecycle.commit(state, next, true, quest, &bad), std::invalid_argument);
            CHECK(&state.data() == pBefore);
            CHECK_THROWS_AS(lifecycle.captureCurrentFloor(state, bad.runtime), std::invalid_argument);
            CHECK(&state.data() == pBefore);
            BreachMazeRunData invalid = state.data();
            invalid.floors.at(invalid.currentLevel).runtime = bad.runtime;
            CHECK_THROWS_AS(lifecycle.restore(std::move(invalid)), std::invalid_argument);
            CHECK(&state.data() == pBefore);
            bad = readyFloor(next);
            REQUIRE(lifecycle.commit(state, next, true, quest, &bad) == BreachMazeCommitResult::Committed);
        }
    }
}

TEST_CASE("Breach lifecycle uses native world clock boundaries and rejects invalid clocks atomically")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    const BreachMazeQuestView quest{true, false, false};
    for (double minutes : {0.0, MaximumGameMinutes})
    {
        CAPTURE(minutes);
        BreachMazeState state;
        BreachMazePreparedTransition prepared = lifecycle.prepareFreshEntry(
            state, BreachMazeEntry::BrAlvar53, quest, 41);
        BreachMazeReadyFloor ready = readyFloor(prepared);
        ready.runtime.scene.worldRuntime.gameMinutes = minutes;
        IndoorWorldRuntime world;
        REQUIRE_NOTHROW(world.restoreSnapshot(ready.runtime.scene.worldRuntime));
        CHECK(world.snapshot().gameMinutes == minutes);
        REQUIRE(lifecycle.commit(state, prepared, true, quest, &ready) == BreachMazeCommitResult::Committed);
        lifecycle.captureCurrentFloor(state, ready.runtime);
        const BreachMazeState restored = lifecycle.restore(state.data());
        CHECK(lifecycle.coldRestore(restored).runtime.scene.worldRuntime.gameMinutes == minutes);
    }
    for (double minutes : {-1.0, MaximumGameMinutes + 1.0,
        std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        CAPTURE(minutes);
        BreachMazeState state;
        BreachMazePreparedTransition prepared = lifecycle.prepareFreshEntry(
            state, BreachMazeEntry::BrAlvar53, quest, 41);
        BreachMazeReadyFloor ready = readyFloor(prepared);
        ready.runtime.scene.worldRuntime.gameMinutes = minutes;
        IndoorWorldRuntime world;
        CHECK_THROWS_AS(world.restoreSnapshot(ready.runtime.scene.worldRuntime), std::invalid_argument);
        const BreachMazeRunData *pEmpty = &state.data();
        CHECK_THROWS_AS(lifecycle.commit(state, prepared, true, quest, &ready), std::invalid_argument);
        CHECK(&state.data() == pEmpty);
        CHECK(state.data().nextGenerationSerial == 1);
        ready.runtime.scene.worldRuntime.gameMinutes = 0.0;
        REQUIRE(lifecycle.commit(state, prepared, true, quest, &ready) == BreachMazeCommitResult::Committed);
        CHECK(state.data().nextGenerationSerial == 2);
        const BreachMazeRunData *pBefore = &state.data();
        ready.runtime.scene.worldRuntime.gameMinutes = minutes;
        CHECK_THROWS_AS(lifecycle.captureCurrentFloor(state, ready.runtime), std::invalid_argument);
        CHECK(&state.data() == pBefore);
        CHECK_THROWS_AS(lifecycle.prepareStep(state, BreachMazeStep::Up, quest, ready.runtime),
            std::invalid_argument);
        CHECK(&state.data() == pBefore);
        BreachMazeRunData invalid = state.data();
        invalid.floors.at(invalid.currentLevel).runtime.scene.worldRuntime.gameMinutes = minutes;
        CHECK_THROWS_AS(lifecycle.restore(std::move(invalid)), std::invalid_argument);
        CHECK(&state.data() == pBefore);
        CHECK(state.activeFloor()->runtime.scene.worldRuntime.gameMinutes == 0.0);
    }
}

TEST_CASE("Breach lifecycle planning level revision epoch and serial overflow fail without mutation")
{
    const BreachMazeLifecycle lifecycle(sourceTemplate());
    BreachMazeState state;
    BreachMazeQuestView quest{true, true, true}; // Explicit postQuest planner fixture, not normal admission.
    enter(lifecycle, state, quest, BreachMazeEntry::BrAlvar54);
    BreachMazeState maximum = lifecycle.restore(moveSingleFloorTo(state, BreachMazeMaximumPlannedLevel, 1));
    const BreachMazeRunData *pMaximum = &maximum.data();
    CHECK_THROWS_AS(lifecycle.prepareStep(maximum, BreachMazeStep::Down, quest, maximum.activeFloor()->runtime),
        std::invalid_argument);
    CHECK(&maximum.data() == pMaximum);
    BreachMazePreparedTransition up = lifecycle.prepareStep(
        maximum, BreachMazeStep::Up, quest, maximum.activeFloor()->runtime);
    CHECK(up.target()->identity.level == BreachMazeMaximumPlannedLevel - 1);
    CHECK(up.target()->layout.side >= 4);
    CHECK(up.target()->layout.side <= 16);

    BreachMazeRunData revisionData = state.data();
    revisionData.revision = UINT64_MAX;
    BreachMazeState revision = lifecycle.restore(std::move(revisionData));
    const BreachMazeRunData *pRevision = &revision.data();
    CHECK_THROWS_AS(lifecycle.prepareStep(revision, BreachMazeStep::Down, quest, revision.activeFloor()->runtime),
        std::invalid_argument);
    CHECK_THROWS_AS(lifecycle.captureCurrentFloor(revision, revision.activeFloor()->runtime), std::invalid_argument);
    CHECK(&revision.data() == pRevision);
    CHECK(&lifecycle.coldRestore(revision) == revision.activeFloor());

    BreachMazeRunData serialData = state.data();
    serialData.nextGenerationSerial = UINT64_MAX;
    BreachMazeState serial = lifecycle.restore(std::move(serialData));
    const BreachMazeRunData *pSerial = &serial.data();
    CHECK_THROWS_AS(lifecycle.prepareStep(serial, BreachMazeStep::Down, quest, serial.activeFloor()->runtime),
        std::invalid_argument);
    CHECK(&serial.data() == pSerial);

    BreachMazeState epoch = lifecycle.restore(moveSingleFloorTo(state, 1, UINT64_MAX));
    step(lifecycle, epoch, quest, BreachMazeStep::Up);
    const BreachMazeRunData *pEpoch = &epoch.data();
    CHECK_THROWS_AS(lifecycle.prepareFreshEntry(epoch, BreachMazeEntry::BrAlvar53, quest, 7), std::invalid_argument);
    CHECK(&epoch.data() == pEpoch);
}
