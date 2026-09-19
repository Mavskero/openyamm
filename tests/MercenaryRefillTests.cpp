#include "doctest/doctest.h"

#include "game/gameplay/MercenaryRecruitmentRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/tables/HouseTable.h"

using namespace OpenYAMM::Game;

namespace
{
constexpr float MonthMinutes = 28.0f * 24.0f * 60.0f;

struct RecruitmentFixture
{
    HouseTable houses;
    MapStatsEntry map;
    Party party;
    EventRuntimeState state;

    RecruitmentFixture()
    {
        REQUIRE(houses.loadFromRows({
            {"239", "21", "Tavern", "24", "", "Test tavern"},
            {"240", "21", "Tavern", "25", "", "Another tavern"}}));
        map.id = 24;
        map.worldId = "mm7";
        map.fileName = "7out01.odm";
        map.mergedContinentId = 2;
        PartySeed seed;
        Character knight;
        knight.name = "Original knight";
        knight.className = "Knight";
        knight.health = knight.maxHealth = 35;
        seed.members.push_back(knight);
        party.seed(seed);
    }

    bool refresh(float minutes)
    {
        return refreshMercenaryRecruitmentForCurrentMap(
            map, party, state, MercenaryRecruitmentTables{.pHouseTable = &houses}, minutes);
    }

    void acceptFirstRecruit()
    {
        REQUIRE(state.generatedMercenaryRecruitsByNpcId.contains(303));
        REQUIRE(party.recruitCharacter(state.generatedMercenaryRecruitsByNpcId.at(303).character));
        state.generatedMercenaryRecruitsByNpcId.erase(303);
        state.unavailableNpcIds.insert(303);
        state.npcHouseOverrides.erase(303);
        party.setNpcUnavailable(303, true);
        party.clearNpcHouseOverride(303);
    }
};
}

TEST_CASE("hiring does not refill the tavern until the next original 28-day month")
{
    RecruitmentFixture f;
    REQUIRE(f.refresh(540.0f));
    f.acceptFirstRecruit();
    CHECK_FALSE(f.refresh(541.0f));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.empty());
    CHECK_FALSE(f.refresh(MonthMinutes - 1.0f));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.empty());
    REQUIRE(f.refresh(MonthMinutes));
    REQUIRE(f.state.generatedMercenaryRecruitsByNpcId.contains(304));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.at(304).rosterId == 43);
    CHECK_FALSE(f.refresh(MonthMinutes + 1.0f));
    CHECK(f.party.members().size() == 2);
}

TEST_CASE("an unhired candidate persists this month and can refresh in another month")
{
    RecruitmentFixture f;
    REQUIRE(f.refresh(540.0f));
    f.state.generatedMercenaryRecruitsByNpcId.at(303).character.name = "Existing candidate";
    CHECK_FALSE(f.refresh(600.0f));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.at(303).character.name == "Existing candidate");
    REQUIRE(f.refresh(MonthMinutes));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.at(303).character.name != "Existing candidate");
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.size() == 1);
}

TEST_CASE("mercenary refill is map local and excludes the original fourth continent")
{
    RecruitmentFixture f;
    REQUIRE(f.refresh(540.0f));
    EventRuntimeState anotherVisit;
    MapStatsEntry anotherMap = f.map;
    anotherMap.id = 25;
    anotherMap.fileName = "another.odm";
    CHECK(refreshMercenaryRecruitmentForCurrentMap(
        anotherMap, f.party, anotherVisit, MercenaryRecruitmentTables{.pHouseTable = &f.houses}, 540.0f));
    CHECK(anotherVisit.generatedMercenaryRecruitsByNpcId.at(303).houseId == 240);
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.at(303).houseId == 239);
    f.map.mergedContinentId = 4;
    EventRuntimeState excluded;
    CHECK_FALSE(refreshMercenaryRecruitmentForCurrentMap(
        f.map, f.party, excluded, MercenaryRecruitmentTables{.pHouseTable = &f.houses}, 540.0f));
    CHECK(excluded.generatedMercenaryRecruitsByNpcId.empty());
}

TEST_CASE("an exhausted mercenary roster does not refill again within the checked month")
{
    RecruitmentFixture f;
    for (uint32_t npcId = 303; npcId <= 310; ++npcId) f.state.unavailableNpcIds.insert(npcId);
    CHECK_FALSE(f.refresh(540.0f));
    f.state.unavailableNpcIds.erase(303);
    CHECK_FALSE(f.refresh(600.0f));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.empty());
    CHECK(f.refresh(MonthMinutes));
}

TEST_CASE("old saved tavern states acquire the saved month without spawning a replacement")
{
    RecruitmentFixture f;
    REQUIRE(f.refresh(540.0f));
    f.acceptFirstRecruit();
    // Legacy saves lacked the refill marker, including after an ordinary recruitment.
    f.state.namedMapVars.erase("LastMercsRefill");
    GameSaveData saved;
    saved.hasOutdoorRuntimeState = true;
    saved.outdoorWorld.gameMinutes = 540.0f;
    saved.outdoorWorld.eventRuntimeState = f.state;
    saved.outdoorWorldStates["7out01.odm"] = saved.outdoorWorld;
    saved.hasIndoorSceneState = true;
    saved.indoorScene.worldRuntime.gameMinutes = MonthMinutes + 600.0f;
    saved.indoorScene.eventRuntimeState = f.state;
    saved.indoorSceneStates["test.blv"] = saved.indoorScene;
    const auto path = std::filesystem::temp_directory_path() / "openyamm-mercenary-refill-check.oysav";
    std::string error;
    REQUIRE(saveGameDataToPath(path, saved, error));
    const auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE(loaded.has_value());
    const auto &outdoor = *loaded->outdoorWorld.eventRuntimeState;
    const auto &indoor = *loaded->indoorScene.eventRuntimeState;
    REQUIRE(outdoor.namedMapVars.contains("LastMercsRefill"));
    REQUIRE(indoor.namedMapVars.contains("LastMercsRefill"));
    CHECK(outdoor.namedMapVars.at("LastMercsRefill") == 0);
    CHECK(indoor.namedMapVars.at("LastMercsRefill") == 1);
    CHECK(loaded->outdoorWorldStates.at("7out01.odm").eventRuntimeState->namedMapVars.at("LastMercsRefill") == 0);
    CHECK(loaded->indoorSceneStates.at("test.blv").eventRuntimeState->namedMapVars.at("LastMercsRefill") == 1);
    f.state = outdoor;
    CHECK_FALSE(f.refresh(600.0f));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.empty());
}

TEST_CASE("legacy migration preserves candidates and leaves an unvisited state ready to generate")
{
    RecruitmentFixture f;
    EventRuntimeState empty;
    migrateLegacyMercenaryRefillState(empty, MonthMinutes);
    CHECK(empty.namedMapVars.empty());
    CHECK(refreshMercenaryRecruitmentForCurrentMap(
        f.map, f.party, empty, MercenaryRecruitmentTables{.pHouseTable = &f.houses}, MonthMinutes));
    REQUIRE(f.refresh(540.0f));
    f.state.generatedMercenaryRecruitsByNpcId.at(303).character.name = "Original saved candidate";
    f.state.namedMapVars.erase("LastMercsRefill");
    migrateLegacyMercenaryRefillState(f.state, 600.0f);
    CHECK_FALSE(f.refresh(601.0f));
    CHECK(f.state.generatedMercenaryRecruitsByNpcId.at(303).character.name == "Original saved candidate");
    migrateLegacyMercenaryRefillState(f.state, MonthMinutes);
    CHECK(f.state.namedMapVars.at("LastMercsRefill") == 0);
    CHECK(f.refresh(MonthMinutes));
}

TEST_CASE("refill follows the original calendar month across the year boundary")
{
    RecruitmentFixture f;
    REQUIRE(f.refresh(12.0f * MonthMinutes - 1.0f));
    f.acceptFirstRecruit();
    CHECK_FALSE(f.refresh(12.0f * MonthMinutes - 0.5f));
    REQUIRE(f.refresh(12.0f * MonthMinutes));
    CHECK(f.state.namedMapVars.at("LastMercsRefill") == 0);
    CHECK_FALSE(f.refresh(12.0f * MonthMinutes + 540.0f));
}
