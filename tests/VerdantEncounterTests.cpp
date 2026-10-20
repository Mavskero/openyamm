#include "tests/MergeCrossContinentsHarness.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/maps/SaveGame.h"
#include <cstdlib>
#include <filesystem>

using namespace OpenYAMM::Game;
using OpenYAMM::Tests::MergeQuestFixture;

TEST_CASE("first catch uses 28-day months, strict deadline, two-day retry and source arena exclusions")
{
    MergeQuestFixture f;
    f.after();
    CHECK_EQ(f.ticks("MeetTime"), int64_t(100.25 * 256) + 8 * 28 * 1440 * 256LL);
    const int64_t deadline = f.ticks("MeetTime");
    f.minutes = double(deadline) / 256;
    f.after();
    f.timer(65506);
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    f.minutes += 1.0 / 256;
    f.after();
    CHECK_EQ(f.ticks("MeetTime"), deadline + 1 + 2 * 1440 * 256LL);
    f.timer(65506);
    REQUIRE(f.state.pendingDialogueContext.has_value());
    CHECK_EQ(f.state.pendingDialogueContext->sourceId, 803);
    f.clearUiRequests();
    f.timer(65506);
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    for (const char *map : {"d42.blv", "7d05.blv", "zarena.blv"})
    {
        MergeQuestFixture arena;
        arena.setMapName(map);
        arena.after();
        CHECK_EQ(arena.ticks("MeetTime"), 0);
        arena.ticks("MeetTime", 1);
        arena.after();
        arena.timer(65506);
        CHECK_FALSE(arena.state.pendingDialogueContext.has_value());
        CHECK_EQ(arena.ticks("MeetTime"), 1);
    }
}

TEST_CASE("first catch polls every ten game minutes, waits for normal screen and both enemy detectors")
{
    MergeQuestFixture f;
    f.ticks("MeetTime", 1);
    f.after();
    auto timers = reconcileScriptedEventTimers({}, f.global->timerTriggers(), f.minutes, 0, false);
    std::erase_if(timers, [](const auto &timer) { return timer.definition.eventId != 65506; });
    auto callback = [&](const auto &timer) { f.timer(timer.eventId); return true; };
    CHECK_FALSE(updateScriptedEventTimers(timers, 110.249, callback));
    f.state.normalGameplayScreen = false;
    REQUIRE(updateScriptedEventTimers(timers, 110.25, callback));
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    f.state.normalGameplayScreen = true;
    f.yellow = true;
    REQUIRE(updateScriptedEventTimers(timers, 120.25, callback));
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    f.yellow = false;
    f.red = true;
    REQUIRE(updateScriptedEventTimers(timers, 130.25, callback));
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    f.red = false;
    REQUIRE(updateScriptedEventTimers(timers, 140.25, callback));
    REQUIRE(f.state.pendingDialogueContext.has_value());
    CHECK_EQ(f.state.pendingDialogueContext->sourceId, 803);
    f.clearUiRequests();
    REQUIRE(updateScriptedEventTimers(timers, 150.25, callback));
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
}

TEST_CASE("pending first catch is map-local and is not restored as a saved armed closure")
{
    MergeQuestFixture f;
    f.ticks("MeetTime", 1);
    f.after();
    const auto flags = f.state.namedGlobalVars;
    f.leave();
    f.timer(65506);
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    f.after(); // The new two-day deadline prevents immediate rearming.
    f.timer(65506);
    CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    MergeQuestFixture cold;
    cold.state.namedGlobalVars = flags;
    cold.after();
    cold.timer(65506);
    CHECK_FALSE(cold.state.pendingDialogueContext.has_value());
    cold.minutes = double(cold.ticks("MeetTime") + 1) / 256;
    cold.after();
    cold.timer(65506);
    CHECK(cold.state.pendingDialogueContext.has_value());
}

TEST_CASE("Enroth letter requires meeting, physical switch three and absent Q1104, then grants once to player zero")
{
    for (uint32_t continent : {1, 2, 3})
    {
        for (bool met : {false, true})
        {
            for (bool door : {false, true})
            {
                MergeQuestFixture f(continent);
                f.flag("MetVerdant", met);
                f.state.dimensionDoorActive = door;
                f.state.normalGameplayScreen = false; // Letter has no additional screen or enemy gate.
                f.red = true;
                const bool eligible = met && continent == 3 && !door;
                f.after();
                CHECK_EQ(f.party.hasQuestBit(1104), eligible);
                CHECK_EQ(f.party.hasQuestBit(1105), eligible);
                CHECK_EQ(f.party.inventoryItemCount(2125, 0), eligible ? 1 : 0);
                CHECK_EQ(f.party.inventoryItemCount(2125, 1), 0);
                CHECK_EQ(f.state.pendingDialogueContext.has_value(), eligible);
                if (eligible) CHECK_EQ(f.party.snapshot().npcGreetingOverrides.at(803), 330);
                f.clearUiRequests();
                f.after();
                CHECK_FALSE(f.state.pendingDialogueContext.has_value());
                CHECK_EQ(f.party.inventoryItemCount(2125), eligible ? 1 : 0);
            }
        }
    }
    MergeQuestFixture prior(3);
    prior.flag("MetVerdant", 1);
    prior.party.setQuestBit(1104, true);
    prior.after();
    CHECK_FALSE(prior.party.hasQuestBit(1105));
    CHECK_EQ(prior.party.inventoryItemCount(2125), 0);
}

TEST_CASE("Verdant location follows physical continent, respects gates, and uses Party global NPC overrides")
{
    const uint32_t houses[] = {185, 641, 1195};
    MergeQuestFixture f;
    f.flag("MetVerdant", 1);
    f.party.setQuestBit(1104, true);
    for (uint32_t continent : {1, 2, 3})
    {
        f.state.activeHistoryContinentId = continent;
        f.after();
        CHECK_EQ(f.flag("VerdantAt"), int32_t(continent));
        CHECK_EQ(f.party.snapshot().npcHouseOverrides.at(803), houses[continent - 1]);
        EventRuntimeState cached;
        cached.npcHouseOverrides[803] = 185; // Old map retained a previous home.
        f.party.applyGlobalNpcStateTo(cached);
        CHECK_EQ(cached.npcHouseOverrides.at(803), houses[continent - 1]);
        Party restored;
        restored.restoreSnapshot(f.party.snapshot());
        restored.applyGlobalNpcStateTo(cached);
        CHECK_EQ(cached.npcHouseOverrides.at(803), houses[continent - 1]);
        f.party.setNpcHouseOverride(803, 777); // Same-continent AfterLoad must not move twice.
        f.after();
        CHECK_EQ(f.party.snapshot().npcHouseOverrides.at(803), 777);
    }
    for (const char *gate : {"MetVerdant", "HideVerdant", "QuestFinished"})
    {
        MergeQuestFixture blocked(2);
        blocked.flag("MetVerdant", std::string(gate) != "MetVerdant");
        if (std::string(gate) != "MetVerdant") blocked.flag(gate, 1);
        blocked.after();
        CHECK_FALSE(blocked.party.snapshot().npcHouseOverrides.contains(803));
        CHECK_EQ(blocked.flag("VerdantAt"), 0);
    }
    MergeQuestFixture door(3);
    door.flag("MetVerdant", 1);
    door.state.dimensionDoorActive = true;
    door.after();
    CHECK_FALSE(door.party.snapshot().npcHouseOverrides.contains(803));
}

TEST_CASE("letter, timing and latest NPC location persist through session save and cold Party restoration")
{
    MergeQuestFixture f(3);
    f.flag("MetVerdant", 1);
    f.ticks("MeetTime", 12345678901LL);
    f.after();
    GameSession session;
    OutdoorWorldRuntime::Snapshot world;
    world.gameMinutes = f.minutes;
    world.eventRuntimeState = f.state;
    session.captureOutdoorRuntimeState("oute3.odm", f.party, {}, world, 0, 0);
    auto save = session.buildSaveData();
    REQUIRE(save.has_value());
    const auto file = std::filesystem::path(std::getenv("TMPDIR")) / "verdant137-persist.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(file, *save, error), error);
    const auto loaded = loadGameDataFromPath(file, error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    MergeQuestFixture cold(3);
    cold.party.restoreSnapshot(loaded->party);
    cold.state = *loaded->outdoorWorld.eventRuntimeState;
    cold.state.namedGlobalVars = loaded->namedGlobalVars;
    clearTransientEventRuntimeState(cold.state);
    cold.party.applyGlobalNpcStateTo(cold.state);
    cold.after();
    CHECK_EQ(cold.flag("VerdantAt"), 3);
    CHECK_EQ(cold.ticks("MeetTime"), 12345678901LL);
    CHECK_EQ(cold.party.snapshot().npcHouseOverrides.at(803), 1195);
    CHECK_EQ(cold.party.snapshot().npcGreetingOverrides.at(803), 330);
    CHECK_EQ(cold.party.inventoryItemCount(2125), 1);
    CHECK(cold.party.hasQuestBit(1104));
    CHECK(cold.party.hasQuestBit(1105));
    CHECK_FALSE(cold.state.pendingDialogueContext.has_value());
}
