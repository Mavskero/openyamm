#include "doctest/doctest.h"
#include "game/FaceEnums.h"
#include "game/content/BreachMazeSceneBuilder.h"
#include "game/indoor/IndoorCollisionPrimitives.h"
#include "game/indoor/IndoorGeometryUtils.h"
#include "game/indoor/IndoorMovementController.h"
#include "game/indoor/IndoorPortalGraph.h"
#include "game/indoor/IndoorWorldRuntime.h"
#include "tools/mmerge_import/BreachMazeTemplateResources.h"
#include "tools/mmerge_import/LodResources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
std::filesystem::path originalSource()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

const BreachMazeTemplate &sourceTemplate()
{
    static const BreachMazeTemplateProfile profile = loadBreachMazeTemplateProfile(originalSource()).value();
    return profile.mazeTemplate;
}

const IndoorMapData &sourceMap()
{
    static const IndoorMapData map = IndoorMapDataLoader().loadFromBytes(
        readLodMap(originalSource() / "Data/breach.games.lod", "brbase.blv")).value();
    return map;
}

std::array<uint8_t, 16> palette(uint8_t selected = 1)
{
    std::array<uint8_t, 16> result;
    result.fill(selected);
    return result;
}

BreachMazeLayout layoutWithSide(uint8_t side)
{
    for (uint64_t seed = 0; seed < 512; ++seed)
    {
        BreachMazeLayout layout = generateBreachMazeLayout(sourceTemplate(), 16, seed);
        if (layout.side == side)
        {
            return layout;
        }
    }
    throw std::runtime_error("Controlled maze fixture could not find the requested size");
}

GameplayWorldPoint center(uint16_t cellId, float z = -51.0f)
{
    const BreachMazeTemplateCell &cell = sourceTemplate().cells.at(cellId - 1);
    return {float((cell.x - 1) * BreachMazeCellWidth), float(-(cell.y - 1) * BreachMazeCellWidth), z};
}

uint32_t wallFace(uint16_t cellId, BreachMazeSide direction)
{
    const BreachMazeTemplateCell &cell = sourceTemplate().cells.at(cellId - 1);
    for (uint32_t face : cell.wallFaceIndices)
    {
        if (sourceTemplate().faces[face].wallSide == direction)
        {
            return face;
        }
    }
    throw std::runtime_error("Controlled maze fixture is missing a main wall");
}

struct Adjacent
{
    uint16_t first;
    uint16_t second;
    BreachMazeSide direction;
    bool closed;
};

std::vector<Adjacent> adjacentCells(const BreachMazeLayout &layout)
{
    std::vector<Adjacent> result;
    for (uint8_t y = 1; y <= layout.side; ++y)
    {
        for (uint8_t x = 1; x <= layout.side; ++x)
        {
            const BreachMazeCell &cell = layout.cellAt(x, y);
            if (x < layout.side)
            {
                result.push_back({cell.templateCellId, layout.cellAt(x + 1, y).templateCellId,
                    BreachMazeSide::East, cell.walls[1]});
            }
            if (y < layout.side)
            {
                result.push_back({cell.templateCellId, layout.cellAt(x, y + 1).templateCellId,
                    BreachMazeSide::South, cell.walls[3]});
            }
        }
    }
    return result;
}

std::vector<uint16_t> routeBetweenEndpoints(const BreachMazeLayout &layout)
{
    const uint16_t first = layout.endpoints.entrance.templateCellId;
    const uint16_t last = layout.endpoints.exit.templateCellId;
    std::array<uint16_t, 257> parent = {};
    parent[first] = first;
    std::queue<uint16_t> pending;
    pending.push(first);
    const std::vector<Adjacent> edges = adjacentCells(layout);
    while (!pending.empty())
    {
        const uint16_t current = pending.front();
        pending.pop();
        for (const Adjacent &edge : edges)
        {
            if (edge.closed || (edge.first != current && edge.second != current))
            {
                continue;
            }
            const uint16_t next = edge.first == current ? edge.second : edge.first;
            if (parent[next] == 0)
            {
                parent[next] = current;
                pending.push(next);
            }
        }
    }
    REQUIRE(parent[last] != 0);
    std::vector<uint16_t> path = {last};
    while (path.back() != first)
    {
        path.push_back(parent[path.back()]);
    }
    std::reverse(path.begin(), path.end());
    return path;
}
}

TEST_CASE("BrBase scene keeps immutable geometry identity and synchronizes all effective face attributes")
{
    const std::optional<std::vector<uint8_t>> originalBytes = IndoorMapDataWriter().buildBytes(sourceMap());
    REQUIRE(originalBytes.has_value());
    const std::array<std::array<std::string, 4>, 4> expectedMaterials = {{
        {"7cdb", "7c1fl", "7cdb", "7c1fl"}, {"cwb", "7c1fl", "cwb", "cwb1"},
        {"7t1s2bs4", "7tfb1", "7trim9_32", "7trimd"}, {"t65c02", "t65b03", "t65b10", "t65a03"}}};
    MapDeltaData base;
    base.surfaceRevision = 19;
    base.faceAttributes.assign(5632, faceAttributeBit(FaceAttribute::Invisible));
    base.locationInfo.reputation = 17;
    base.locationInfo.totalFacesCount = 5632;
    base.locationTime.lastVisitTime = 12345;
    base.eventVariables.mapVars[2] = 71;
    base.eventVariables.decorVars[3] = 82;
    base.decorationFlags = {9, 8, 7};
    base.visibleOutlines = {1, 2};
    base.actors.resize(1);
    base.actors[0].hp = 197;
    base.actors[0].name = "Preserve controlled sentinel";
    base.spriteObjects.resize(1);
    base.spriteObjects[0].x = 4321;
    for (uint8_t side : {4, 8, 16})
    {
        const BreachMazeLayout layout = layoutWithSide(side);
        for (uint8_t selected = 1; selected <= 4; ++selected)
        {
            CAPTURE(side);
            CAPTURE(selected);
            const BreachMazeSceneData scene = buildBreachMazeScene(
                sourceMap(), base, sourceTemplate(), layout, palette(selected));
            CHECK_EQ(scene.delta.surfaceRevision, 20);
            CHECK_EQ(scene.delta.locationInfo.reputation, 17);
            CHECK_EQ(scene.delta.locationTime.lastVisitTime, 12345);
            CHECK_EQ(scene.delta.eventVariables.mapVars[2], 71);
            CHECK_EQ(scene.delta.eventVariables.decorVars[3], 82);
            CHECK(scene.delta.decorationFlags == base.decorationFlags);
            CHECK(scene.delta.visibleOutlines == base.visibleOutlines);
            REQUIRE_EQ(scene.delta.actors.size(), 1);
            CHECK_EQ(scene.delta.actors[0].hp, 197);
            CHECK_EQ(scene.delta.actors[0].name, base.actors[0].name);
            REQUIRE_EQ(scene.delta.spriteObjects.size(), 1);
            CHECK_EQ(scene.delta.spriteObjects[0].x, 4321);
            CHECK_EQ(scene.map.vertices.size(), 4224);
            REQUIRE_EQ(scene.map.faces.size(), 5632);
            REQUIRE_EQ(scene.delta.faceAttributes.size(), 5632);
            CHECK_EQ(scene.map.rawFaceCount, 5632);
            CHECK(scene.map.sectorData == sourceMap().sectorData);
            CHECK(scene.map.sectorLightData == sourceMap().sectorLightData);
            CHECK_EQ(scene.map.bspNodes.size(), 1123);
            CHECK_EQ(scene.map.outlines.size(), 2792);
            REQUIRE_EQ(scene.map.sectors.size(), 2);
            CHECK(scene.map.sectors[0].faceIds.empty());
            CHECK(scene.map.sectors[1].faceIds == sourceMap().sectors[1].faceIds);
            CHECK(scene.map.sectors[1].portalFaceIds.empty());
            CHECK(buildIndoorPortalGraph(scene.map, &scene.delta).portals.empty());
            size_t eventFaces = 0;
            std::set<std::string> visibleMaterials;
            for (size_t index = 0; index < scene.map.faces.size(); ++index)
            {
                const IndoorFace &face = scene.map.faces[index];
                const IndoorFace &original = sourceMap().faces[index];
                const BreachMazeTemplateFace &metadata = sourceTemplate().faces[index];
                const BreachMazeTemplateCell &cell = sourceTemplate().cells[metadata.cellId - 1];
                const bool active = cell.x <= side && cell.y <= side;
                CHECK_EQ(scene.delta.faceAttributes[index], face.attributes);
                CHECK(face.vertexIndices == original.vertexIndices);
                CHECK(face.textureUs == original.textureUs);
                CHECK(face.textureVs == original.textureVs);
                CHECK_EQ(face.roomNumber, original.roomNumber);
                CHECK_EQ(face.roomBehindNumber, original.roomBehindNumber);
                CHECK_EQ(face.cogNumber, original.cogNumber);
                if (!active)
                {
                    CHECK(hasFaceAttribute(face.attributes, FaceAttribute::Invisible));
                    CHECK_EQ(face.textureName, original.textureName);
                    CHECK_EQ(face.isPortal, original.isPortal);
                }
                else
                {
                    const BreachMazeCell &mazeCell = layout.cellAt(cell.x, cell.y);
                    const bool open = metadata.wallSide && !mazeCell.walls[static_cast<size_t>(*metadata.wallSide)];
                    CHECK_EQ(face.isPortal, open);
                    CHECK_EQ(hasFaceAttribute(face.attributes, FaceAttribute::Invisible), open);
                    if (metadata.wallSide)
                    {
                        CHECK_EQ(hasFaceAttribute(face.attributes, FaceAttribute::IsPortal), open);
                        CHECK_EQ(hasFaceAttribute(face.attributes, FaceAttribute::Untouchable), open);
                    }
                    if (face.cogTriggered == 0)
                    {
                        CHECK_EQ(face.textureName, open ? original.textureName
                            : expectedMaterials[selected - 1][static_cast<size_t>(metadata.role)]);
                    }
                }
                if (face.cogTriggered != 0)
                {
                    ++eventFaces;
                    REQUIRE(face.cogTriggered <= 2);
                    const BreachMazeEndpoint &endpoint = face.cogTriggered == 1
                        ? layout.endpoints.entrance : layout.endpoints.exit;
                    CHECK_EQ(index, endpoint.faceIndex);
                    CHECK(hasFaceAttribute(face.attributes, FaceAttribute::Clickable));
                    CHECK_FALSE(face.isPortal);
                    CHECK_FALSE(hasFaceAttribute(face.attributes, FaceAttribute::Untouchable));
                    CHECK_EQ(face.textureName, face.cogTriggered == 1 ? "rmzdra" : "rmzdrb");
                }
                if (!hasFaceAttribute(face.attributes, FaceAttribute::Invisible) && !face.isPortal)
                {
                    visibleMaterials.insert(face.textureName);
                }
            }
            CHECK_EQ(eventFaces, 2);
            CHECK(scene.requiredTextures == std::vector<std::string>(visibleMaterials.begin(), visibleMaterials.end()));
            CHECK_EQ(hasFaceAttribute(scene.map.faces[0].attributes, FaceAttribute::Invisible), side < 11);
            const BreachMazeSceneData repeated = buildBreachMazeScene(
                sourceMap(), base, sourceTemplate(), layout, palette(selected));
            CHECK(IndoorMapDataWriter().buildBytes(scene.map) == IndoorMapDataWriter().buildBytes(repeated.map));
            CHECK(scene.delta.faceAttributes == repeated.delta.faceAttributes);
            // Reset only the fields owned by this builder: all other map data must serialize identically.
            IndoorMapData restored = scene.map;
            restored.faces = sourceMap().faces;
            CHECK(IndoorMapDataWriter().buildBytes(restored) == originalBytes);
        }
    }
    CHECK(IndoorMapDataWriter().buildBytes(sourceMap()) == originalBytes);
    CHECK_EQ(base.surfaceRevision, 19);
    CHECK(std::all_of(base.faceAttributes.begin(), base.faceAttributes.end(), [](uint32_t attributes)
    {
        return attributes == faceAttributeBit(FaceAttribute::Invisible);
    }));
}

TEST_CASE("BrBase scene rejects content corruption even when input counts remain unchanged")
{
    IndoorMapData map = sourceMap();
    MapDeltaData delta;
    BreachMazeTemplate metadata = sourceTemplate();
    BreachMazeLayout layout = layoutWithSide(4);
    std::array<uint8_t, 16> materials = palette();
    SUBCASE("moved original vertex")
    {
        ++map.vertices[0].x;
    }
    SUBCASE("changed source face zero UV")
    {
        ++map.faces[0].textureUs[0];
    }
    SUBCASE("changed source vertex reference")
    {
        map.faces[0].vertexIndices[0] ^= 1;
    }
    SUBCASE("changed source texture")
    {
        map.faces[0].textureName = "other";
    }
    SUBCASE("changed source event")
    {
        map.faces[0].cogTriggered = 17;
    }
    SUBCASE("changed source bool without attribute")
    {
        map.faces[0].isPortal = true;
    }
    SUBCASE("changed source sector face ordering")
    {
        std::swap(map.sectors[1].faceIds[0], map.sectors[1].faceIds[1]);
    }
    SUBCASE("changed source BSP")
    {
        ++map.bspNodes[0].faceIdOffset;
    }
    SUBCASE("changed outline geometry")
    {
        map.outlines[0].vertex1Id ^= 1;
    }
    SUBCASE("metadata owner")
    {
        metadata.faces[0].cellId = 167;
    }
    SUBCASE("metadata normal")
    {
        metadata.faces[0].sourceNormal[2] = 1;
    }
    SUBCASE("metadata bounds")
    {
        ++metadata.faces[0].sourceBounds[0];
    }
    SUBCASE("metadata role")
    {
        metadata.faces[0].role = BreachMazeFaceRole::Floor;
    }
    SUBCASE("metadata cell face duplicate")
    {
        metadata.cells[0].faceIndices[0] = metadata.cells[0].faceIndices[1];
    }
    SUBCASE("metadata center floor")
    {
        metadata.cells[0].centralFloorFaceIndex = 1313;
    }
    SUBCASE("metadata wall order")
    {
        std::swap(metadata.cells[0].wallFaceIndices[0], metadata.cells[0].wallFaceIndices[1]);
    }
    SUBCASE("layout version")
    {
        layout.algorithmVersion = 2;
    }
    SUBCASE("layout asymmetry")
    {
        layout.cells[0].walls[1] = !layout.cells[0].walls[1];
    }
    SUBCASE("layout opened outer boundary")
    {
        layout.cells[0].walls[0] = false;
    }
    SUBCASE("layout relabeled room")
    {
        layout.cells[0].room = layout.cells[0].room == 1 ? 2 : 1;
    }
    SUBCASE("layout forged endpoint")
    {
        layout.endpoints.entrance.faceIndex = 0;
    }
    SUBCASE("layout changed PRNG count")
    {
        ++layout.randomDrawCount;
    }
    SUBCASE("missing used palette")
    {
        materials[layout.cells[0].room - 1] = 0;
    }
    SUBCASE("unknown used palette")
    {
        materials[layout.cells[0].room - 1] = 5;
    }
    SUBCASE("delta truncated face array")
    {
        delta.faceAttributes.resize(5631);
    }
    SUBCASE("revision overflow")
    {
        delta.surfaceRevision = std::numeric_limits<uint64_t>::max();
    }
    CHECK_THROWS_AS(buildBreachMazeScene(map, delta, metadata, layout, materials), std::invalid_argument);
}

TEST_CASE("BrBase scene applies explicit per-room palettes and leaves unused room slots optional")
{
    const BreachMazeLayout layout = layoutWithSide(8);
    std::array<uint8_t, 16> materials = {};
    for (const BreachMazeCell &cell : layout.cells)
    {
        materials[cell.room - 1] = uint8_t(1 + (cell.room - 1) % 4);
    }
    const BreachMazeSceneData scene = buildBreachMazeScene(sourceMap(), {}, sourceTemplate(), layout, materials);
    const std::array<std::string, 4> floors = {"7c1fl", "7c1fl", "7tfb1", "t65b03"};
    const std::array<std::string, 4> details = {"7c1fl", "cwb1", "7trimd", "t65a03"};
    for (const BreachMazeCell &cell : layout.cells)
    {
        const BreachMazeTemplateCell &metadata = sourceTemplate().cells[cell.templateCellId - 1];
        CHECK_EQ(scene.map.faces[metadata.centralFloorFaceIndex].textureName, floors[materials[cell.room - 1] - 1]);
        for (uint32_t face : metadata.faceIndices)
        {
            if (sourceTemplate().faces[face].role == BreachMazeFaceRole::Decoration)
            {
                CHECK_EQ(scene.map.faces[face].textureName, details[materials[cell.room - 1] - 1]);
            }
        }
    }
    CHECK_EQ(BreachMazeSceneVersion, 1);
}

TEST_CASE("BrBase real shared geometry and swept body collision agree on every adjacent cell wall")
{
    for (uint8_t side : {4, 8, 16})
    {
        const BreachMazeLayout layout = layoutWithSide(side);
        const BreachMazeSceneData scene = buildBreachMazeScene(sourceMap(), {}, sourceTemplate(), layout, palette());
        for (const Adjacent &edge : adjacentCells(layout))
        {
            CAPTURE(side);
            CAPTURE(edge.first);
            CAPTURE(edge.second);
            const GameplayWorldPoint start = center(edge.first);
            const bx::Vec3 direction = edge.direction == BreachMazeSide::East
                ? bx::Vec3{1, 0, 0} : bx::Vec3{0, -1, 0};
            const BreachMazeSide opposite = edge.direction == BreachMazeSide::East
                ? BreachMazeSide::West : BreachMazeSide::North;
            IndoorSweptBody body;
            body.lowSphere = {{start.x, start.y, -174}, 37, 37};
            body.highSphere = {{start.x, start.y, -88}, 37, 123};
            for (uint32_t face : {wallFace(edge.first, edge.direction), wallFace(edge.second, opposite)})
            {
                IndoorFaceGeometryData geometry;
                REQUIRE(buildIndoorFaceGeometry(scene.map, scene.map.vertices, face, geometry, &scene.delta));
                CHECK_EQ(geometry.attributes, scene.delta.faceAttributes[face]);
                CHECK_EQ(geometry.isPortal, !edge.closed);
                const std::optional<IndoorSweptFaceHit> hit =
                    sweepIndoorBodyAgainstFace(body, direction, float(BreachMazeCellWidth), geometry);
                CHECK_EQ(hit.has_value(), edge.closed);
            }
        }
    }
}

TEST_CASE("BrBase production movement follows a real generated route and is stopped by a closed wall")
{
    const BreachMazeLayout layout = layoutWithSide(4);
    const BreachMazeSceneData scene = buildBreachMazeScene(sourceMap(), {}, sourceTemplate(), layout, palette());
    const std::optional<MapDeltaData> delta = scene.delta;
    const std::optional<EventRuntimeState> events = EventRuntimeState{};
    const IndoorMovementController movement(scene.map, &delta, &events);
    const IndoorBodyDimensions body;
    const std::vector<uint16_t> route = routeBetweenEndpoints(layout);
    REQUIRE(route.size() > 1);
    const GameplayWorldPoint start = center(route.front());
    IndoorMoveState state = movement.initializeStateFromEyePosition(start.x, start.y, start.z, body);
    REQUIRE(state.grounded);
    CHECK(state.footZ == doctest::Approx(-211).epsilon(0.0001));
    for (size_t index = 1; index < route.size(); ++index)
    {
        CAPTURE(index);
        CAPTURE(route[index]);
        const GameplayWorldPoint target = center(route[index]);
        for (size_t step = 0; step < 150; ++step)
        {
            const float dx = target.x - state.x;
            const float dy = target.y - state.y;
            const float distance = std::hypot(dx, dy);
            if (distance < 0.5f)
            {
                break;
            }
            const float speed = std::min(300.0f, distance / 0.02f);
            state = movement.resolveMove(state, body, speed * dx / distance, speed * dy / distance, false, 0.02f);
        }
        REQUIRE(std::hypot(target.x - state.x, target.y - state.y) < 1.0f);
        CHECK(state.grounded);
        CHECK(state.footZ == doctest::Approx(-211).epsilon(0.0001));
        CHECK_EQ(state.sectorId, 1);
    }
    const std::vector<Adjacent> edges = adjacentCells(layout);
    const auto closed = std::find_if(edges.begin(), edges.end(), [](const Adjacent &edge) { return edge.closed; });
    REQUIRE(closed != edges.end());
    const GameplayWorldPoint before = center(closed->first);
    const GameplayWorldPoint beyond = center(closed->second);
    state = movement.initializeStateFromEyePosition(before.x, before.y, before.z, body);
    const float velocityX = closed->direction == BreachMazeSide::East ? 300 : 0;
    const float velocityY = closed->direction == BreachMazeSide::South ? -300 : 0;
    for (size_t step = 0; step < 120; ++step)
    {
        state = movement.resolveMove(state, body, velocityX, velocityY, false, 0.02f);
    }
    CHECK(std::hypot(state.x - before.x, state.y - before.y) > 200);
    CHECK(std::hypot(state.x - before.x, state.y - before.y) < 300);
    CHECK(std::hypot(state.x - beyond.x, state.y - beyond.y) > 300);
}

TEST_CASE("BrBase real runtime combat line of sight uses the same open and closed geometry")
{
    const MonsterTable monsters{};
    const ObjectTable objects{};
    const ItemTable items{};
    const ChestTable chests{};
    MapStatsEntry map;
    map.fileName = "brbase.blv";
    map.name = "Controlled BrBase geometry without population";
    for (uint8_t side : {4, 8, 16})
    {
        CAPTURE(side);
        const BreachMazeLayout layout = layoutWithSide(side);
        const BreachMazeSceneData scene = buildBreachMazeScene(sourceMap(), {}, sourceTemplate(), layout, palette());
        std::optional<MapDeltaData> delta = scene.delta;
        std::optional<EventRuntimeState> events = EventRuntimeState{};
        IndoorWorldRuntime world;
        world.initialize(map, monsters, objects, items, chests, nullptr, nullptr, &delta, &events,
            nullptr, nullptr, &scene.map);
        std::array<size_t, 2> checked = {};
        for (const Adjacent &edge : adjacentCells(layout))
        {
            if (checked[edge.closed] >= 4)
            {
                continue;
            }
            ++checked[edge.closed];
            CAPTURE(edge.first);
            CAPTURE(edge.second);
            CHECK_EQ(world.hasIndoorCombatLineOfSight(center(edge.first), 1, center(edge.second), 1), !edge.closed);
            CHECK_EQ(world.hasIndoorCombatLineOfSight(center(edge.second), 1, center(edge.first), 1), !edge.closed);
        }
        CHECK_EQ(checked[0], 4);
        CHECK_EQ(checked[1], 4);
    }
}
