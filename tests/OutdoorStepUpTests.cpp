#include "doctest/doctest.h"
#include "game/outdoor/OutdoorMovementController.h"

#include <utility>

using namespace OpenYAMM::Game;

namespace
{
OutdoorBModelFace stepFace(std::vector<uint16_t> indices, uint8_t type)
{
    OutdoorBModelFace face = {};
    face.vertexIndices = std::move(indices);
    face.polygonType = type;
    return face;
}

OutdoorMapData stairMap(int height = 32, int count = 1)
{
    OutdoorMapData map = {};
    map.heightMap.assign(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 0);
    map.attributeMap.assign(map.heightMap.size(), 0);
    OutdoorBModel model = {};
    model.name = "terrain_stairs";
    for (int index = 0; index < count; ++index)
    {
        const int x = index * 128;
        const int z = index * height;
        const uint16_t first = static_cast<uint16_t>(model.vertices.size());
        model.vertices.insert(model.vertices.end(), {
            {x, -384, z}, {x, -384, z + height}, {x, 384, z + height}, {x, 384, z},
            {x + 128, -384, z + height}, {x + 128, 384, z + height}});
        model.faces.push_back(stepFace({first, uint16_t(first + 1), uint16_t(first + 2), uint16_t(first + 3)}, 1));
        model.faces.push_back(stepFace({uint16_t(first + 1), uint16_t(first + 4),
            uint16_t(first + 5), uint16_t(first + 2)}, 3));
    }
    map.bmodels.push_back(std::move(model));
    return map;
}

OutdoorMoveState advance(const OutdoorMovementController &controller, OutdoorMoveState state,
    float speed = 384.0f, int frames = 1)
{
    for (int frame = 0; frame < frames; ++frame)
    {
        state = controller.resolveMoveForBody(state, OutdoorBodyDimensions{}, speed, 0.0f, 0.0f,
            false, false, false, false, false, 480.0f, 1536.0f, 4000.0f, 1.0f / 128.0f);
    }
    return state;
}
}

TEST_CASE("party can ascend the gentle triangle beside one high terrain corner")
{
    OutdoorMapData map = stairMap();
    map.bmodels.clear();
    const size_t topLeft = 64 * OutdoorMapData::TerrainWidth + 64;
    SUBCASE("high bottom-right corner does not block the opposite gentle triangle")
    {
        map.heightMap[topLeft + 1] = 4;
        map.heightMap[topLeft + OutdoorMapData::TerrainWidth + 1] = 32;
        const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        OutdoorMoveState state = controller.initializeState(128.0f, -128.0f, 33.0f);
        REQUIRE(state.footZ == doctest::Approx(33.0f));
        state = advance(controller, state, 384.0f, 32);
        CHECK_GT(state.x, 210.0f);
        CHECK(state.footZ == doctest::Approx(57.0f));
        CHECK_FALSE(state.airborne);
    }
    SUBCASE("high top-left corner does not block the opposite gentle triangle")
    {
        map.heightMap[topLeft] = 32;
        map.heightMap[topLeft + 1] = 4;
        map.heightMap[topLeft + OutdoorMapData::TerrainWidth + 1] = 4;
        const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        OutdoorMoveState state = controller.initializeState(288.0f, -384.0f, 73.0f);
        REQUIRE(state.footZ == doctest::Approx(73.0f));
        state = advance(controller, state, 384.0f, 32);
        CHECK_GT(state.x, 370.0f);
        CHECK(state.footZ == doctest::Approx(97.0f));
        CHECK_FALSE(state.airborne);
    }
}

TEST_CASE("party still cannot walk uphill on a genuinely steep terrain triangle")
{
    OutdoorMapData map = stairMap();
    map.bmodels.clear();
    const size_t topLeft = 64 * OutdoorMapData::TerrainWidth + 64;
    map.heightMap[topLeft + 1] = 32;
    map.heightMap[topLeft + OutdoorMapData::TerrainWidth + 1] = 32;
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    OutdoorMoveState state = controller.initializeState(128.0f, -128.0f, 257.0f);
    state = advance(controller, state, 384.0f, 32);
    CHECK_LE(state.x, 130.0f);
    CHECK_LE(state.footZ, 257.0f);
}

TEST_CASE("party walks from terrain up five 32-unit steps without jumping")
{
    const OutdoorMapData map = stairMap(32, 5);
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    OutdoorMoveState state = controller.initializeState(-100.0f, 0.0f, 1.0f);
    state = advance(controller, state, 384.0f, 240);
    INFO("position=" << state.x << "," << state.footZ);
    CHECK_GT(state.x, 520.0f);
    CHECK_LT(state.x, 640.0f);
    CHECK(state.footZ == doctest::Approx(161.0f));
    CHECK(state.supportKind == OutdoorSupportKind::BModelFace);
    CHECK_FALSE(state.airborne);
    const OutdoorMoveState idle = advance(controller, state, 0.0f, 128);
    CHECK(idle.footZ == doctest::Approx(state.footZ));
    const OutdoorMoveState returned = advance(controller, idle, -384.0f, 240);
    CHECK_LT(returned.x, -60.0f);
    CHECK(advance(controller, returned, 0.0f, 128).footZ == doctest::Approx(1.0f));
}

TEST_CASE("party can stop immediately after stepping at the edge")
{
    const OutdoorMapData map = stairMap();
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    OutdoorMoveState state = controller.initializeState(-100.0f, 0.0f, 1.0f);
    for (int frame = 0; frame < 60 && state.footZ < 32.0f; ++frame)
    {
        state = advance(controller, state);
    }
    REQUIRE(state.footZ == doctest::Approx(33.0f));
    CHECK(state.supportKind == OutdoorSupportKind::BModelFace);
    CHECK_FALSE(state.airborne);
    state = advance(controller, state, 0.0f, 128);
    CHECK(state.footZ == doctest::Approx(33.0f));
    CHECK_FALSE(state.airborne);
}

TEST_CASE("step probes preserve height limits and non-step faces")
{
    OutdoorMapData map = stairMap();
    SUBCASE("too high") { map = stairMap(160); }
    SUBCASE("not a step") { map.bmodels[0].faces[1].reserved = 1; }
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    const OutdoorMoveState state = advance(controller, controller.initializeState(-100.0f, 0.0f, 1.0f), 384.0f, 100);
    CHECK_LT(state.x, 0.0f);
    CHECK_LT(state.footZ, 32.0f);
}

TEST_CASE("step up requires headroom for the whole party body")
{
    OutdoorMapData map = stairMap();
    OutdoorBModel &model = map.bmodels[0];
    model.vertices.insert(model.vertices.end(), {{-100, -384, 208}, {128, -384, 208},
        {128, 384, 208}, {-100, 384, 208}});
    model.faces.push_back(stepFace({6, 9, 8, 7}, 5));
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    const OutdoorMoveState state = advance(controller, controller.initializeState(-100.0f, 0.0f, 1.0f), 384.0f, 100);
    CHECK_LT(state.x, 0.0f);
    CHECK_LT(state.footZ, 32.0f);
}

TEST_CASE("idle terrain party does not snap up to a nearby stair")
{
    const OutdoorMapData map = stairMap();
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    OutdoorMoveState state = controller.initializeState(-30.0f, 0.0f, 1.0f);
    state = advance(controller, state, 0.0f, 128);
    CHECK(state.footZ == doctest::Approx(1.0f));
    CHECK(state.supportKind == OutdoorSupportKind::Terrain);
}

TEST_CASE("a low tread does not let the party pass through a taller wall")
{
    OutdoorMapData map = stairMap();
    OutdoorBModel &model = map.bmodels[0];
    model.vertices.insert(model.vertices.end(), {{0, -384, 32}, {0, -384, 320},
        {0, 384, 320}, {0, 384, 32}});
    model.faces.push_back(stepFace({6, 7, 8, 9}, 1));
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    const OutdoorMoveState state = advance(controller, controller.initializeState(-100.0f, 0.0f, 1.0f), 384.0f, 100);
    CHECK_LT(state.x, 0.0f);
    CHECK_LT(state.footZ, 34.0f);
}

TEST_CASE("party loses step support after walking beyond the footprint")
{
    const OutdoorMapData map = stairMap();
    const OutdoorMovementController controller(map, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    OutdoorMoveState state = controller.initializeState(-100.0f, 0.0f, 1.0f);
    state = advance(controller, state, 384.0f, 160);
    REQUIRE_GT(state.x, 300.0f);
    state = advance(controller, state, 0.0f, 128);
    CHECK(state.footZ == doctest::Approx(1.0f));
    CHECK(state.supportKind == OutdoorSupportKind::Terrain);
}
