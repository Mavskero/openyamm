#include "CrossContinentsIntroSource.h"
#include "EventSupportSource.h"
#include "doctest/doctest.h"

#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/events/GameTime.h"
#include "game/events/NamedGameTime.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/indoor/IndoorWorldRuntime.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/maps/SaveGame.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <limits>

using namespace OpenYAMM::Game;

namespace
{
std::optional<ScriptedEventProgram> program(const std::string &text, ScriptedEventScope scope)
{
    std::string error;
    auto result = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + "\n" + text,
        "@merge-lifecycle-controlled-fixture", scope, error);
    REQUIRE_MESSAGE(result.has_value(), error);
    return result;
}

std::filesystem::path output(const std::string &name)
{
    const char *root = std::getenv("TMPDIR");
    REQUIRE(root != nullptr);
    return std::filesystem::path(root) / name;
}

class PreciseWorld : public IndoorWorldRuntime
{
public:
    Party *party() override { return &p; }
    const Party *party() const override { return &p; }
    Party p;
};

void setClock(IndoorWorldRuntime &world, double value)
{
    auto snapshot = world.snapshot();
    snapshot.gameMinutes = value;
    world.restoreSnapshot(snapshot);
}
}

TEST_CASE("production world clock advances at eight months and survives map/session/save transfer")
{
    const double start = 356496.59375;
    const float frameMinutes = (1.0f / 60.0f) * 0.5f;
    PreciseWorld indoor;
    setClock(indoor, start);
    float oldClock = float(start);
    for (unsigned frame = 0; frame < 600; ++frame)
    {
        oldClock += frameMinutes;
        indoor.advanceGameMinutes(frameMinutes);
    }
    CHECK_EQ(oldClock, float(start));
    const double expected = start + 600.0 * frameMinutes;
    CHECK(std::abs(indoor.gameMinutes() - expected) < 0.000001);
    CHECK(indoor.gameMinutes() > start + 4.99);

    OutdoorWorldRuntime outdoor;
    auto outdoorSnapshot = outdoor.snapshot();
    outdoorSnapshot.gameMinutes = start;
    outdoorSnapshot.eventRuntimeState = EventRuntimeState{};
    outdoor.restoreSnapshot(outdoorSnapshot);
    EventRuntime runtime;
    for (unsigned frame = 0; frame < 600; ++frame)
        outdoor.updateTimers(1.0f / 60.0f, runtime, std::nullopt, std::nullopt);
    CHECK(std::abs(outdoor.gameMinutes() - expected) < 0.000001);

    GameSession session;
    IndoorSceneRuntime::Snapshot indoorSnapshot;
    indoorSnapshot.worldRuntime = indoor.snapshot();
    session.captureIndoorRuntimeState("d07.blv", indoor.p, indoorSnapshot, 0, 0);
    CHECK_EQ(session.gameMinutes(), indoor.gameMinutes());
    outdoorSnapshot = outdoor.snapshot();
    outdoorSnapshot.gameMinutes = session.gameMinutes();
    session.captureOutdoorRuntimeState("7out02.odm", indoor.p, {}, outdoorSnapshot, 0, 0);
    CHECK_EQ(session.gameMinutes(), indoor.gameMinutes());
    std::string error;
    const auto saved = session.buildSaveData();
    REQUIRE(saved.has_value());
    REQUIRE_MESSAGE(saveGameDataToPath(output("precision87.oysav"), *saved, error), error);
    const auto loaded = loadGameDataFromPath(output("precision87.oysav"), error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK_EQ(loaded->savedGameMinutes, indoor.gameMinutes());
    CHECK_EQ(loaded->outdoorWorld.gameMinutes, indoor.gameMinutes());
    CHECK_EQ(loaded->indoorSceneStates.at("d07.blv").worldRuntime.gameMinutes, indoor.gameMinutes());
    GameSession restored;
    GameDataLoader loader;
    GameDataRepository data;
    data.bind(loader);
    restored.bindDataRepository(&data);
    restored.restoreFromSaveData(*loaded);
    CHECK_EQ(restored.gameMinutes(), indoor.gameMinutes());
    std::cout << std::setprecision(17) << "clock600 start=" << start << " old_float_delta=" << double(oldClock) - start
              << " indoor_delta=" << indoor.gameMinutes() - start
              << " outdoor_delta=" << outdoor.gameMinutes() - start << '\n';
}

TEST_CASE("raw ticks retain subminute deadlines beyond signed 32-bit ticks and integer minute boundaries")
{
    PreciseWorld world;
    const auto global = program(R"lua(
ReplaceGlobalEvent(60100, nil, function()
    evt.SetGlobalTimeTicks("Fixture.Deadline", evt.GameTimeTicks() + 1)
    evt.SetGlobalVar("Fixture.Minute", evt.CurrentGameMinutes())
end)
ReplaceGlobalEvent(60101, nil, function()
    evt.SetGlobalVar("Fixture.Strict", evt.GameTimeTicks() > evt.GetGlobalTimeTicks("Fixture.Deadline", 0) and 1 or 0)
    evt.SetGlobalVar("Fixture.Inclusive", evt.GameTimeTicks() >= evt.GetGlobalTimeTicks("Fixture.Deadline", 0) and 1 or 0)
end)
)lua", ScriptedEventScope::Global);
    EventRuntime runtime;
    EventRuntimeState state;
    setClock(world, 356496.999);
    REQUIRE(runtime.executeEventById(std::nullopt, global, 60100, state, nullptr, &world));
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Minute"), 356496);
    setClock(world, 25165824.00390625); // hi=1, lo has its sign bit set after multiplying by256.
    REQUIRE(runtime.executeEventById(std::nullopt, global, 60100, state, nullptr, &world));
    int64_t deadline = 0;
    std::string error;
    REQUIRE(readNamedGameTime(state.namedGlobalVars, "Fixture.Deadline", 0, deadline, error));
    CHECK(deadline > std::numeric_limits<int32_t>::max());
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Deadline.TicksHi"), 1);
    CHECK(state.namedGlobalVars.at("Fixture.Deadline.TicksLo") < 0);
    CHECK_EQ(deadline, 6442450946LL);
    setClock(world, double(deadline - 1) / 256);
    REQUIRE(runtime.executeEventById(std::nullopt, global, 60101, state, nullptr, &world));
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Strict"), 0);
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Inclusive"), 0);
    setClock(world, double(deadline) / 256);
    REQUIRE(runtime.executeEventById(std::nullopt, global, 60101, state, nullptr, &world));
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Strict"), 0);
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Inclusive"), 1);
    setClock(world, double(deadline + 1) / 256);
    REQUIRE(runtime.executeEventById(std::nullopt, global, 60101, state, nullptr, &world));
    CHECK_EQ(state.namedGlobalVars.at("Fixture.Strict"), 1);
    REQUIRE(writeNamedGameTime(state.namedGlobalVars, "Fixture.Maximum", MaximumGameTimeTicks, error));
    REQUIRE(readNamedGameTime(state.namedGlobalVars, "Fixture.Maximum", 0, deadline, error));
    CHECK_EQ(deadline, MaximumGameTimeTicks);
    CHECK_FALSE(writeNamedGameTime(state.namedGlobalVars, "Fixture.Overflow", MaximumGameTimeTicks + 1, error));
    state.namedGlobalVars.erase("Fixture.Deadline.TicksLo");
    CHECK_FALSE(validateNamedGameTimes(state.namedGlobalVars, error));
    CHECK_FALSE(runtime.executeEventById(std::nullopt, global, 60101, state, nullptr, &world));
}

TEST_CASE("global and local hooks have distinct load commit and leave phases even without local program")
{
    const auto global = program(R"lua(
local function add(n) evt.SetGlobalVar("Order", evt.GetGlobalVar("Order", 0) * 10 + n) end
RegisterGlobalOnLoadEvent(60110, nil, function() add(1) end)
RegisterGlobalAfterLoadEvent(60111, nil, function() add(3) end)
RegisterGlobalOnLeaveEvent(60112, nil, function() add(4) end)
SetGlobalMetadata({onLoad={}, onLeave={}, afterLoad={}})
)lua", ScriptedEventScope::Global);
    const auto local = program(R"lua(
local function add(n) evt.SetGlobalVar("Order", evt.GetGlobalVar("Order", 0) * 10 + n) end
RegisterMapOnLoadEvent(60113, nil, function() add(2) end)
RegisterMapOnLeaveEvent(60114, nil, function() add(5) end)
)lua", ScriptedEventScope::Map);
    EventRuntime runtime;
    EventRuntimeState state;
    REQUIRE(runtime.executeOnLoadEvents(local, global, state));
    CHECK_EQ(state.namedGlobalVars.at("Order"), 12);
    REQUIRE(runtime.executeGlobalAfterLoadEvents(local, global, state));
    CHECK_EQ(state.namedGlobalVars.at("Order"), 123);
    REQUIRE(runtime.executeOnLeaveEvents(local, global, state));
    CHECK_EQ(state.namedGlobalVars.at("Order"), 12345);
    EventRuntimeState globalOnly;
    REQUIRE(runtime.executeGlobalAfterLoadEvents(std::nullopt, global, globalOnly));
    REQUIRE(runtime.executeOnLeaveEvents(std::nullopt, global, globalOnly));
    CHECK_EQ(globalOnly.namedGlobalVars.at("Order"), 34);
}

TEST_CASE("temporary timers rearm from committed entry instead of restoring a saved countdown")
{
    const auto global = program(R"lua(
RegisterGlobalTemporaryTimerEvent(60120, 600, function()
    if evt.IsNormalGameplayScreen() then
        evt.SetGlobalVar("Catch", evt.GetGlobalVar("Catch", 0) + 1)
    end
end)
)lua", ScriptedEventScope::Global);
    REQUIRE_EQ(global->timerTriggers().size(), 1);
    const auto definitions = global->timerTriggers();
    CHECK(definitions[0].temporaryMapLifecycle);
    auto timers = reconcileScriptedEventTimers({}, definitions, 100, 0, false);
    EventRuntime runtime;
    EventRuntimeState state;
    auto callback = [&](const auto &definition) {
        return runtime.executeEventById(std::nullopt, global, definition.eventId, state,
            nullptr, nullptr, std::nullopt, true, ScriptedEventScope::Global, true);
    };
    CHECK_FALSE(updateScriptedEventTimers(timers, 109.999, callback));
    REQUIRE(updateScriptedEventTimers(timers, 110, callback));
    CHECK_FALSE(state.namedGlobalVars.contains("Catch")); // Unsafe interface at the first poll.
    state.normalGameplayScreen = true;
    REQUIRE(updateScriptedEventTimers(timers, 120, callback));
    CHECK_EQ(state.namedGlobalVars.at("Catch"), 1);
    CHECK_FALSE(updateScriptedEventTimers(timers, 120, callback));
    CHECK_EQ(state.namedGlobalVars.at("Catch"), 1);
    timers = reconcileScriptedEventTimers(timers, definitions, 125, 0, false);
    CHECK_EQ(timers[0].nextAlarmGameMinutes, 135);
    OutdoorWorldRuntime outdoor;
    auto snapshot = outdoor.snapshot();
    snapshot.timers = timers;
    outdoor.restoreSnapshot(snapshot);
    CHECK(outdoor.snapshot().timers.empty());
}

TEST_CASE("screen safety reflects actual modal screens while preserving held cursor item semantics")
{
    GameSession session;
    PreciseWorld world;
    session.bindActiveWorldRuntime(&world);
    auto &screen = session.gameplayScreenRuntime();
    REQUIRE(screen.isNormalGameplayScreen());
    screen.heldInventoryItem().active = true;
    CHECK(screen.isNormalGameplayScreen());
    screen.characterScreen().open = true;
    CHECK_FALSE(screen.isNormalGameplayScreen());
    screen.characterScreen().open = false;
    screen.journalScreenState().active = true;
    CHECK_FALSE(screen.isNormalGameplayScreen());
    screen.journalScreenState().active = false;
    screen.utilitySpellOverlay().active = true;
    CHECK_FALSE(screen.isNormalGameplayScreen());
    screen.utilitySpellOverlay().active = false;
    screen.activeEventDialog().isActive = true;
    CHECK_FALSE(screen.isNormalGameplayScreen());
    screen.activeEventDialog().isActive = false;
    CHECK(screen.isNormalGameplayScreen());
}

TEST_CASE("scene owner afterload shares its armed Lua closure with timers and commits only once")
{
    const auto global = program(R"lua(
local armed = false
RegisterGlobalAfterLoadEvent(60130, nil, function()
    armed = true
    evt.SetGlobalVar("Committed", evt.GetGlobalVar("Committed", 0) + 1)
end)
RegisterGlobalTemporaryTimerEvent(60131, 600, function()
    if armed then evt.SetGlobalVar("Fired", evt.GetGlobalVar("Fired", 0) + 1) end
end)
RegisterGlobalOnLeaveEvent(60132, nil, function() armed = false end)
)lua", ScriptedEventScope::Global);
    MapStatsEntry map;
    IndoorMapData geometry;
    MonsterTable monsters;
    ObjectTable objects;
    ItemTable items;
    ChestTable chests;
    Party party;
    std::optional<EventRuntimeState> state = EventRuntimeState{};
    IndoorSceneRuntime scene("fixture.blv", map, geometry, monsters, objects, items, chests,
        party, std::nullopt, state, std::nullopt, global, nullptr);
    scene.prepareTimers();
    EventRuntime separate;
    REQUIRE(separate.executeGlobalAfterLoadEvents(std::nullopt, global,
        *scene.eventRuntimeState(), &party, scene.sceneEventContext()));
    scene.advanceTurnBasedGameMinutes(10);
    CHECK_FALSE(scene.eventRuntimeState()->namedGlobalVars.contains("Fired"));
    REQUIRE(scene.executeGlobalAfterLoadEvents());
    CHECK_EQ(scene.eventRuntimeState()->namedGlobalVars.at("Committed"), 2);
    REQUIRE(scene.executeGlobalAfterLoadEvents());
    CHECK_EQ(scene.eventRuntimeState()->namedGlobalVars.at("Committed"), 2);
    scene.advanceTurnBasedGameMinutes(10);
    CHECK_EQ(scene.eventRuntimeState()->namedGlobalVars.at("Fired"), 1);
    REQUIRE(scene.executeOnLeaveEvents());
    scene.advanceTurnBasedGameMinutes(10);
    CHECK_EQ(scene.eventRuntimeState()->namedGlobalVars.at("Fired"), 1);
    OutdoorMapData outdoorGeometry;
    OutdoorMovementDriver movement(outdoorGeometry, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    OutdoorPartyRuntime outdoorParty(std::move(movement), items);
    OutdoorWorldRuntime outdoorWorld;
    auto snapshot = outdoorWorld.snapshot();
    snapshot.eventRuntimeState = EventRuntimeState{};
    outdoorWorld.restoreSnapshot(snapshot);
    outdoorWorld.prepareTimers(std::nullopt, global);
    OutdoorSceneRuntime outdoorScene("fixture.odm", map, outdoorParty, outdoorWorld, std::nullopt, global);
    REQUIRE(outdoorScene.executeGlobalAfterLoadEvents());
    REQUIRE(outdoorScene.executeGlobalAfterLoadEvents());
    CHECK_EQ(outdoorScene.eventRuntimeState()->namedGlobalVars.at("Committed"), 1);
    outdoorScene.advanceTurnBasedGameMinutes(10);
    CHECK_EQ(outdoorScene.eventRuntimeState()->namedGlobalVars.at("Fired"), 1);
    REQUIRE(outdoorScene.executeOnLeaveEvents());
    outdoorScene.advanceTurnBasedGameMinutes(10);
    CHECK_EQ(outdoorScene.eventRuntimeState()->namedGlobalVars.at("Fired"), 1);
}

TEST_CASE("StartedAt migration explicitly preserves values and infers only unfinished old stone grants")
{
    auto global = program(std::string(OpenYAMM::Import::CrossContinentsIntroSource), ScriptedEventScope::Global);
    EventRuntime runtime;
    for (uint32_t continent : {1u, 2u, 3u})
    {
        EventRuntimeState state;
        state.activeHistoryContinentId = continent;
        state.namedGlobalVars["MMerge.CrossContinents.GotMainQuest"] = 1;
        REQUIRE(runtime.executeGlobalAfterLoadEvents(std::nullopt, global, state));
        CHECK_EQ(state.namedGlobalVars.at("MMerge.CrossContinents.StartedAt"), continent);
        CHECK_EQ(state.namedGlobalVars.at("MMerge.CrossContinents.StartedAtInferred"), 1);
        state.activeHistoryContinentId = 4;
        REQUIRE(runtime.executeGlobalAfterLoadEvents(std::nullopt, global, state));
        CHECK_EQ(state.namedGlobalVars.at("MMerge.CrossContinents.StartedAt"), continent);
    }
    EventRuntimeState existing;
    existing.namedGlobalVars["MMerge.CrossContinents.GotMainQuest"] = 1;
    existing.namedGlobalVars["MMerge.CrossContinents.StartedAt"] = 2;
    REQUIRE(runtime.executeGlobalAfterLoadEvents(std::nullopt, global, existing));
    CHECK_EQ(existing.namedGlobalVars.at("MMerge.CrossContinents.StartedAt"), 2);
    CHECK_FALSE(existing.namedGlobalVars.contains("MMerge.CrossContinents.StartedAtInferred"));
    existing.namedGlobalVars["MMerge.CrossContinents.StartedAt"] = 4;
    CHECK_FALSE(runtime.executeGlobalAfterLoadEvents(std::nullopt, global, existing));
    existing.namedGlobalVars.erase("MMerge.CrossContinents.StartedAt");
    existing.namedGlobalVars["MMerge.CrossContinents.GotConnectorStone"] = 1;
    REQUIRE(runtime.executeGlobalAfterLoadEvents(std::nullopt, global, existing));
    CHECK_FALSE(existing.namedGlobalVars.contains("MMerge.CrossContinents.StartedAt"));
}

TEST_CASE("current schema rejects invalid clocks and incomplete precise deadline fields")
{
    GameSaveData save;
    save.savedGameMinutes = 543210.23456789;
    std::string error;
    const auto path = output("schema87.oysav");
    REQUIRE(saveGameDataToPath(path, save, error));
    std::ifstream input(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    uint32_t version = 0;
    std::memcpy(&version, bytes.data() + 8, sizeof(version));
    CHECK_EQ(version, 90);
    std::vector<size_t> positions;
    for (size_t offset = 0; offset + sizeof(double) <= bytes.size(); ++offset)
        if (std::memcmp(bytes.data() + offset, &save.savedGameMinutes, sizeof(double)) == 0)
            positions.push_back(offset);
    REQUIRE_EQ(positions.size(), 1);
    unsigned index = 0;
    for (double invalid : {-1.0, MaximumGameMinutes + 1,
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        auto corrupt = bytes;
        std::memcpy(corrupt.data() + positions[0], &invalid, sizeof(invalid));
        const auto corruptPath = output("invalid87-" + std::to_string(index++) + ".oysav");
        std::ofstream file(corruptPath, std::ios::binary);
        file.write(corrupt.data(), corrupt.size()); file.close();
        CHECK_FALSE(loadGameDataFromPath(corruptPath, error).has_value());
        save.savedGameMinutes = invalid;
        CHECK_FALSE(saveGameDataToPath(output("must-not-write.oysav"), save, error));
    }
    save.savedGameMinutes = 100;
    REQUIRE(writeNamedGameTime(save.namedGlobalVars, "ReaderPair", 6442450946LL, error));
    REQUIRE(saveGameDataToPath(output("valid-pair87.oysav"), save, error));
    std::ifstream pairInput(output("valid-pair87.oysav"), std::ios::binary);
    std::string pairBytes{std::istreambuf_iterator<char>(pairInput), std::istreambuf_iterator<char>()};
    const auto lowOffset = pairBytes.find("ReaderPair.TicksLo");
    REQUIRE(lowOffset != std::string::npos);
    pairBytes.replace(lowOffset, 18, "ReaderPair.TicksXX");
    std::ofstream brokenPair(output("incomplete-pair87.oysav"), std::ios::binary);
    brokenPair.write(pairBytes.data(), pairBytes.size()); brokenPair.close();
    CHECK_FALSE(loadGameDataFromPath(output("incomplete-pair87.oysav"), error));
    save.namedGlobalVars.clear();
    save.outdoorWorldStates["bad.odm"].gameMinutes = -1;
    CHECK_FALSE(saveGameDataToPath(output("must-not-write.oysav"), save, error));
    save.outdoorWorldStates.clear();
    save.indoorScene.eventRuntimeState = EventRuntimeState{};
    save.indoorScene.eventRuntimeState->namedGlobalVars["Incomplete.TicksHi"] = 1;
    CHECK_FALSE(saveGameDataToPath(output("must-not-write.oysav"), save, error));
    GameSession session;
    CHECK_THROWS_AS(session.setGameMinutes(-1), std::invalid_argument);
}

TEST_CASE("accepted old128129130 saves read unchanged and upgrade explicitly to schema88")
{
    const char *paths = std::getenv("OPENYAMM_LEGACY_SAVE_PATHS");
    REQUIRE_MESSAGE(paths != nullptr, "Supply original sealed 128/129/130 save paths separated by semicolons");
    std::string all(paths);
    unsigned count = 0;
    while (!all.empty())
    {
        const size_t delimiter = all.find(';');
        const std::filesystem::path path = all.substr(0, delimiter);
        all = delimiter == std::string::npos ? "" : all.substr(delimiter + 1);
        std::string error;
        const auto old = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(old.has_value(), error);
        const auto upgradedPath = output("old-upgraded-" + std::to_string(count++) + ".oysav");
        REQUIRE_MESSAGE(saveGameDataToPath(upgradedPath, *old, error), error);
        const auto upgraded = loadGameDataFromPath(upgradedPath, error);
        REQUIRE_MESSAGE(upgraded.has_value(), error);
        CHECK_EQ(upgraded->savedGameMinutes, old->savedGameMinutes);
        CHECK_EQ(upgraded->outdoorWorld.gameMinutes, old->outdoorWorld.gameMinutes);
        CHECK_EQ(upgraded->indoorScene.worldRuntime.gameMinutes, old->indoorScene.worldRuntime.gameMinutes);
        CHECK_EQ(upgraded->namedGlobalVars, old->namedGlobalVars);
        CHECK_EQ(upgraded->mapFileName, old->mapFileName);
        CHECK_EQ(upgraded->heldInventoryItemActive, old->heldInventoryItemActive);
        CHECK_EQ(upgraded->outdoorWorldStates.size(), old->outdoorWorldStates.size());
        CHECK_EQ(upgraded->indoorSceneStates.size(), old->indoorSceneStates.size());
    }
    CHECK_EQ(count, 3);
}
