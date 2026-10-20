#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/ui/GameplayOverlayTypes.h"
#include "game/items/ItemEnchantTables.h"
#include "game/outdoor/OutdoorMovementDriver.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/tables/ItemTable.h"

#include <cmath>
#include <array>
#include <cstdlib>
#include <utility>

using namespace OpenYAMM::Game;

namespace
{
OutdoorMapData flatMap()
{
    OutdoorMapData map;
    map.heightMap.assign(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 0);
    map.attributeMap.assign(map.heightMap.size(), 0);
    return map;
}

void advance(OutdoorMovementDriver &driver, const OutdoorMovementInput &input, int steps)
{
    for (int step = 0; step < steps; ++step)
    {
        driver.update(input, 1.0f / 128.0f);
    }
}

void checkNoFlightOrDamage(const OutdoorMovementDriver &driver)
{
    CHECK_FALSE(driver.partyMovementState().flying);
    CHECK_FALSE(driver.partyMovementState().activelyFlying);
    CHECK_EQ(driver.pendingEffects().waterDamageTicks, 0);
    CHECK_EQ(driver.pendingEffects().burningDamageTicks, 0);
    CHECK_EQ(driver.pendingEffects().maxFallDamageDistance, 0.0f);
}

OutdoorBModelFace face(std::vector<uint16_t> vertices, uint8_t type)
{
    OutdoorBModelFace result;
    result.vertexIndices = std::move(vertices);
    result.polygonType = type;
    return result;
}
}

TEST_CASE("underwater movement supports ascent descent and hovering without granting Fly")
{
    const OutdoorMapData map = flatMap();
    OutdoorMovementDriver driver(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    driver.initialize(0.0f, 0.0f, 1.0f);
    OutdoorMovementInput up;
    up.flyUp = true;
    advance(driver, up, 32);
    CHECK(driver.state().footZ == doctest::Approx(1.0f));
    checkNoFlightOrDamage(driver);

    driver.setUnderwater(true);
    advance(driver, up, 64);
    REQUIRE_GT(driver.state().footZ, 500.0f);
    const float raisedZ = driver.state().footZ;
    advance(driver, {}, 256);
    CHECK(driver.state().footZ == doctest::Approx(raisedZ));
    checkNoFlightOrDamage(driver);

    OutdoorMovementInput down;
    down.flyDown = true;
    advance(driver, down, 32);
    CHECK_LT(driver.state().footZ, raisedZ - 250.0f);
    CHECK_GT(driver.state().footZ, 1.0f);
    advance(driver, down, 256);
    CHECK(driver.state().footZ == doctest::Approx(1.0f));
    checkNoFlightOrDamage(driver);

    advance(driver, up, 512);
    CHECK_GT(driver.state().footZ, 3900.0f);
    CHECK_LE(driver.state().footZ, driver.tuning().maxFlightHeight);
    checkNoFlightOrDamage(driver);
}

TEST_CASE("underwater pitch movement and scene restoration preserve swimming without carrying it onto land")
{
    const OutdoorMapData map = flatMap();
    OutdoorMovementDriver driver(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    driver.initialize(0.0f, 0.0f, 1.0f);
    driver.setUnderwater(true);
    OutdoorMovementInput forwardUp;
    forwardUp.forward = true;
    forwardUp.pitchRadians = 0.5f;
    advance(driver, forwardUp, 64);
    REQUIRE_GT(driver.state().x, 400.0f);
    REQUIRE_GT(driver.state().footZ, 250.0f);

    OutdoorMovementDriver restored(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    restored.initialize(0.0f, 0.0f, 1.0f);
    restored.setUnderwater(true);
    restored.restoreState(driver.state(), driver.partyMovementState());
    const float savedZ = restored.state().footZ;
    advance(restored, {}, 128);
    CHECK(restored.state().footZ == doctest::Approx(savedZ));
    checkNoFlightOrDamage(restored);

    restored.setUnderwater(false);
    OutdoorMovementInput up;
    up.flyUp = true;
    advance(restored, up, 256);
    CHECK(restored.state().footZ == doctest::Approx(1.0f));
    CHECK_FALSE(restored.partyMovementState().flying);

    // A same-map position initialization does not discard the map environment.
    driver.initialize(0.0f, 0.0f, 1.0f);
    advance(driver, up, 32);
    CHECK_GT(driver.state().footZ, 300.0f);
    checkNoFlightOrDamage(driver);
}

TEST_CASE("underwater movement keeps swept body wall ceiling and floor collisions")
{
    OutdoorMapData map = flatMap();
    OutdoorBModel model;
    model.name = "underwater_collision_fixture";
    model.vertices = {{256, -1024, 0}, {256, -1024, 2048}, {256, 1024, 2048}, {256, 1024, 0},
        {-1024, -1024, 512}, {-1024, 1024, 512}, {1024, 1024, 512}, {1024, -1024, 512}};
    model.faces = {face({0, 1, 2, 3}, 1), face({4, 5, 6, 7}, 5)};
    map.bmodels.push_back(std::move(model));
    OutdoorMovementDriver driver(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    driver.initialize(0.0f, 0.0f, 1.0f);
    driver.setUnderwater(true);
    OutdoorMovementInput diagonal;
    diagonal.forward = true;
    diagonal.flyUp = true;
    advance(driver, diagonal, 128);
    CHECK_GT(driver.state().x, 100.0f);
    CHECK_LT(driver.state().x, 256.0f);
    CHECK_GT(driver.state().footZ, 100.0f);
    CHECK_LT(driver.state().footZ, 512.0f - 100.0f);
    advance(driver, diagonal, 128);
    CHECK_LT(driver.state().x, 256.0f);
    CHECK_LT(driver.state().footZ, 512.0f - 100.0f);
    OutdoorMovementInput down;
    down.flyDown = true;
    advance(driver, down, 128);
    CHECK(driver.state().footZ == doctest::Approx(1.0f));
    checkNoFlightOrDamage(driver);
}

TEST_CASE("outdoor boundary collision reports west even without a table destination")
{
    const OutdoorMapData map = flatMap();
    MapBounds bounds;
    bounds.enabled = true;
    bounds.minX = -512;
    bounds.maxX = 512;
    bounds.minY = -512;
    bounds.maxY = 512;
    OutdoorMovementDriver driver(map, bounds, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    driver.initialize(-450.0f, 0.0f, 1.0f);
    OutdoorMovementInput west;
    west.backward = true;
    advance(driver, west, 64);
    REQUIRE(driver.lastEvents().blockedBoundaryEdge);
    CHECK(*driver.lastEvents().blockedBoundaryEdge == MapBoundaryEdge::West);
    CHECK_GE(driver.state().x, -512.0f);
    const float contactX = driver.state().x;

    // A render frame may have no fixed movement step. Its absent boundary event
    // is not evidence that the party moved away from the contact point.
    driver.update(west, 1.0f / 512.0f);
    CHECK_FALSE(driver.lastEvents().blockedBoundaryEdge);
    CHECK_EQ(driver.state().x, contactX);
    OutdoorMovementInput east;
    east.forward = true;
    advance(driver, east, 16);
    CHECK_GT(driver.state().x, contactX + 8.0f);
    CHECK_FALSE(driver.lastEvents().blockedBoundaryEdge);
    advance(driver, east, 256);
    REQUIRE(driver.lastEvents().blockedBoundaryEdge);
    CHECK(*driver.lastEvents().blockedBoundaryEdge == MapBoundaryEdge::East);
}

TEST_CASE("production outdoor scene dispatches boundary context before table fallback and rearms after moving away")
{
    const std::optional<OutdoorMapData> map = flatMap();
    MapStatsEntry entry = {};
    entry.fileName = "boundary-fixture.odm";
    entry.outdoorBounds = {true, -512, 512, -512, 512};
    entry.eastTransition = MapEdgeTransition{};
    entry.eastTransition->destinationMapFileName = "east-fixture.odm";
    const ItemTable items;
    const MonsterTable monsters;
    const MonsterProjectileTable projectiles;
    const ObjectTable objects;
    const SpellTable spells;
    const StandardItemEnchantTable standardEnchants;
    const SpecialItemEnchantTable specialEnchants;
    OutdoorPartyRuntime party(OutdoorMovementDriver(*map, entry.outdoorBounds,
        std::nullopt, std::nullopt, std::nullopt, std::nullopt), items);
    party.initialize(-450.0f, 0.0f, 1.0f);
    PartySeed seed;
    seed.members.resize(1);
    seed.members[0].health = seed.members[0].maxHealth = 100;
    party.party().seed(seed);
    std::optional<EventRuntimeState> initial = EventRuntimeState{};
    initial->mapFileName = entry.fileName;
    initial->normalGameplayScreen = true;
    OutdoorWorldRuntime world;
    world.initialize(entry, monsters, projectiles, objects, spells, items, &party.party(), &party,
        standardEnchants, specialEnchants, nullptr, map, std::nullopt, std::nullopt, initial);
    std::string error;
    const std::optional<ScriptedEventProgram> script = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + R"lua(
RegisterMapBoundaryHook(65470, nil, function(context)
    evt.SetMapVar("Count", evt.GetMapVar("Count", 0) + 1)
    local expected = context.boundaryEdge == 3 and "" or "east-fixture.odm"
    evt.SetMapVar("DestinationMatches", context.destinationMapName == expected and 1 or 0)
    evt.SetHookBlocked(true)
    if evt.GetMapVar("Offer", 0) == 1 then
        evt.MoveToMap(11, 22, 33, 0, 0, 0, 0, 8, "script-fixture.odm")
    else
        evt.StatusText("boundary fixture")
    end
end)
)lua", "@boundary-production-fixture", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(script, error);
    OutdoorSceneRuntime scene(entry.fileName, entry, party, world, script, std::nullopt);
    EventRuntimeState &state = *scene.eventRuntimeState();
    const auto move = [&](bool west, int frames)
    {
        OutdoorMovementInput input;
        input.backward = west;
        input.forward = !west;
        for (int frame = 0; frame < frames; ++frame)
        {
            scene.advanceFrame(input, 1.0f / 512.0f);
        }
    };

    move(true, 256);
    CHECK_EQ(state.namedMapVars.at("Count"), 1);
    CHECK_EQ(state.namedMapVars.at("DestinationMatches"), 1);
    REQUIRE_EQ(state.statusMessages.size(), 1u);
    CHECK_EQ(state.statusMessages[0], "boundary fixture");
    CHECK_FALSE(state.pendingDialogueContext);
    CHECK_FALSE(state.activeHookContext);
    move(true, 256);
    CHECK_EQ(state.namedMapVars.at("Count"), 1);

    move(false, 64);
    move(true, 256);
    CHECK_EQ(state.namedMapVars.at("Count"), 2);
    REQUIRE_EQ(state.statusMessages.size(), 2u);
    state.namedMapVars["Offer"] = 1;
    move(false, 64);
    move(true, 256);
    CHECK_EQ(state.namedMapVars.at("Count"), 3);
    REQUIRE(state.pendingDialogueContext);
    REQUIRE(state.pendingDialogueContext->transitionMapMove);
    CHECK(state.pendingDialogueContext->transitionMapMove->mapName == "script-fixture.odm");
    // Model cancellation at the state boundary; the actual driver separately
    // exercises the normal dialog input. Holding contact must not reopen it.
    state.pendingDialogueContext.reset();
    move(true, 256);
    CHECK_EQ(state.namedMapVars.at("Count"), 3);
    CHECK_FALSE(state.pendingDialogueContext);
    move(false, 64);
    move(true, 256);
    CHECK_EQ(state.namedMapVars.at("Count"), 4);
    REQUIRE(state.pendingDialogueContext);
    state.pendingDialogueContext.reset();
    state.namedMapVars["Offer"] = 0;
    move(false, 2048);
    CHECK_EQ(state.namedMapVars.at("Count"), 5);
    CHECK_EQ(state.namedMapVars.at("DestinationMatches"), 1);
    CHECK_FALSE(state.pendingDialogueContext);
}

TEST_CASE("original Merge MapStats underwater environment reaches the production map profile")
{
    const char *pRoot = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
    REQUIRE(pRoot != nullptr);
    struct MapCase
    {
        const char *pWorld;
        const char *pMap;
        bool underwater;
        const char *pEnvironment;
    };
    // Original Merge applies its EAX22 rule to both maps, including Tutorial.
    const std::array<MapCase, 4> cases = {{{"mm7", "7out15.odm", true, "UNDERWATER"},
        {"mm7", "out14.odm", false, "FOREST"}, {"mm8", "out15.odm", true, "UNDERWATER"},
        {"mm8", "elemw.odm", true, "MOUNTAIN"}}};
    for (const MapCase &test : cases)
    {
        CAPTURE(test.pWorld);
        CAPTURE(test.pMap);
        OpenYAMM::Engine::AssetFileSystem files;
        REQUIRE(files.initialize(pRoot, pRoot, OpenYAMM::Engine::AssetScaleTier::X1, test.pWorld));
        GameDataLoader loader;
        loader.setActiveWorldId(test.pWorld);
        REQUIRE(loader.loadCommonForGameplay(files));
        REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(files, test.pMap));
        REQUIRE(loader.getSelectedMap());
        const MapAssetInfo &map = *loader.getSelectedMap();
        REQUIRE(map.outdoorWeatherProfile);
        CHECK_EQ(map.map.environmentName, test.pEnvironment);
        CHECK_EQ(map.outdoorWeatherProfile->underwater, test.underwater);
    }
}
