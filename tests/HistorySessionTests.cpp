#include "doctest/doctest.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"

using namespace OpenYAMM::Game;

namespace
{
EventRuntimeState historyState(uint32_t continent, int32_t day = 2)
{
    EventRuntimeState state;
    setActiveHistoryContinent(state, continent);
    state.variables[uint32_t(EvtVariable::DayOfYear)] = day;
    state.variables[uint32_t(EvtVariable::Hour)] = 10;
    return state;
}

void recordHistory(EventRuntimeState &state, uint32_t event)
{
    EventRuntime::addVariableValue(state,
        EventRuntime::decodeVariable(uint32_t(EvtVariable::HistoryBegin) + event - 1), 0, nullptr, {});
}

void captureOutdoor(GameSession &session, const std::string &map, const EventRuntimeState &state)
{
    OutdoorWorldRuntime::Snapshot world;
    world.eventRuntimeState = state;
    session.captureOutdoorRuntimeState(map, Party{}, OutdoorPartyRuntime::Snapshot{}, world, 0, 0);
}

struct SessionFixture
{
    GameDataLoader loader;
    GameDataRepository data;
    GameSession session;

    SessionFixture()
    {
        data.bind(loader);
        session.bindDataRepository(&data);
    }
};
}

TEST_CASE("history follows outdoor indoor and continent transitions with its original dates")
{
    SessionFixture fixture;
    GameSession &session = fixture.session;
    EventRuntimeState emerald = historyState(2);
    recordHistory(emerald, 3);
    recordHistory(emerald, 4);
    captureOutdoor(session, "7out01.odm", emerald);

    EventRuntimeState harmondale = historyState(2, 3);
    session.applyHistoryToRuntime(harmondale);
    REQUIRE(harmondale.historyEventTimesByContinent.at(2).contains(3));
    CHECK(harmondale.historyEventTimesByContinent.at(2).at(3) == 2040);
    recordHistory(harmondale, 3);
    captureOutdoor(session, "7out02.odm", harmondale);

    IndoorSceneRuntime::Snapshot indoor;
    indoor.eventRuntimeState = historyState(2, 4);
    session.applyHistoryToRuntime(*indoor.eventRuntimeState);
    recordHistory(*indoor.eventRuntimeState, 5);
    session.captureIndoorRuntimeState("7d29.blv", Party{}, indoor, 0, 0);

    EventRuntimeState jadame = historyState(1, 5);
    session.applyHistoryToRuntime(jadame);
    recordHistory(jadame, 3);
    captureOutdoor(session, "out01.odm", jadame);
    const auto saved = session.buildSaveData();
    REQUIRE(saved);
    const auto &history = saved->outdoorWorld.eventRuntimeState->historyEventTimesByContinent;
    CHECK(history.at(2).at(3) == 2040);
    CHECK(history.at(2).at(4) == 2040);
    CHECK(history.at(2).at(5) == 4920);
    CHECK(history.at(1).at(3) == 6360);
    CHECK(saved->outdoorWorld.eventRuntimeState->historyEventTimes.at(3) == 6360);
    for (const auto &[map, snapshot] : saved->outdoorWorldStates)
    {
        CHECK(snapshot.eventRuntimeState->historyEventTimesByContinent == history);
    }
    CHECK(saved->indoorScene.eventRuntimeState->historyEventTimesByContinent == history);
    CHECK(saved->indoorSceneStates.at("7d29.blv").eventRuntimeState->historyEventTimesByContinent == history);

    session.restoreFromSaveData(*saved);
    EventRuntimeState backToEmerald = emerald;
    session.applyHistoryToRuntime(backToEmerald);
    CHECK(backToEmerald.historyEventTimesByContinent == history);
}

TEST_CASE("cleared history stays cleared after stale map reentry save and restore")
{
    SessionFixture fixture;
    GameSession &session = fixture.session;
    EventRuntimeState original = historyState(2);
    recordHistory(original, 3);
    recordHistory(original, 4);
    captureOutdoor(session, "7out01.odm", original);

    EventRuntimeState destination = historyState(2, 3);
    session.applyHistoryToRuntime(destination);
    EventRuntime::setVariableValue(destination,
        EventRuntime::decodeVariable(uint32_t(EvtVariable::HistoryBegin) + 2), 0, nullptr, {});
    captureOutdoor(session, "7out02.odm", destination);
    EventRuntimeState cached = original;
    session.applyHistoryToRuntime(cached);
    CHECK_FALSE(cached.historyEventTimesByContinent.at(2).contains(3));
    CHECK(cached.historyEventTimesByContinent.at(2).at(4) == 2040);
    const auto saved = session.buildSaveData();
    REQUIRE(saved);
    CHECK_FALSE(saved->outdoorWorldStates.at("7out01.odm")
        .eventRuntimeState->historyEventTimesByContinent.at(2).contains(3));
    session.restoreFromSaveData(*saved);
    session.applyHistoryToRuntime(cached);
    CHECK_FALSE(cached.historyEventTimesByContinent.at(2).contains(3));
    session.clear();
    EventRuntimeState fresh = historyState(2);
    session.applyHistoryToRuntime(fresh);
    CHECK_FALSE(fresh.historyEventTimesByContinent.at(2).contains(4));
}

TEST_CASE("legacy map local histories migrate once without inventing or replacing first dates")
{
    SessionFixture fixture;
    GameSaveData saved;
    saved.currentSceneKind = SceneKind::Outdoor;
    saved.mapFileName = "7out02.odm";
    saved.hasOutdoorRuntimeState = true;
    saved.outdoorWorld.eventRuntimeState = historyState(2);
    OutdoorWorldRuntime::Snapshot emerald;
    emerald.eventRuntimeState = historyState(2);
    recordHistory(*emerald.eventRuntimeState, 3);
    recordHistory(*emerald.eventRuntimeState, 4);
    saved.outdoorWorldStates["7out01.odm"] = emerald;
    emerald.eventRuntimeState->historyEventTimesByContinent[2][3] = 9999;
    saved.outdoorWorldStates["world.mm7.map.7out01"] = emerald;
    IndoorSceneRuntime::Snapshot legacy;
    legacy.eventRuntimeState.emplace();
    legacy.eventRuntimeState->historyEventTimes[7] = 300;
    saved.indoorSceneStates["legacy.blv"] = legacy;

    fixture.session.restoreFromSaveData(saved);
    const auto migrated = fixture.session.buildSaveData();
    REQUIRE(migrated);
    const auto &state = *migrated->outdoorWorld.eventRuntimeState;
    CHECK(state.historyEventTimesByContinent.at(2).at(3) == 2040);
    CHECK(state.historyEventTimesByContinent.at(2).at(4) == 2040);
    CHECK_FALSE(state.historyEventTimesByContinent.at(2).contains(5));
    CHECK(state.historyEventTimesByContinent.at(1).at(7) == 300);
    CHECK(state.historyEventTimes.at(7) == 300);
    fixture.session.restoreFromSaveData(*migrated);
    const auto repeated = fixture.session.buildSaveData();
    REQUIRE(repeated);
    CHECK(repeated->outdoorWorld.eventRuntimeState->historyEventTimesByContinent
        == state.historyEventTimesByContinent);
}
