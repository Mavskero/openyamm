#include "game/content/BreachMazeSceneBuilder.h"

#include "game/FaceEnums.h"
#include "game/indoor/IndoorGeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
// Typed geometry identity v1, independently derived from the SHA-gated source BLV in checkpoint 145.
// This is a corruption/mismatched-input check, not a replacement for the importer's SHA trust boundary.
constexpr uint64_t OriginalGeometryIdentity = 0x2ad661497f2ffc97ULL;
constexpr std::array<std::array<std::string_view, 4>, 4> Materials = {{
    {"7cdb", "7c1fl", "7cdb", "7c1fl"},
    {"cwb", "7c1fl", "cwb", "cwb1"},
    {"7t1s2bs4", "7tfb1", "7trim9_32", "7trimd"},
    {"t65c02", "t65b03", "t65b10", "t65a03"},
}};
constexpr uint32_t OpenWallAttributes = faceAttributeBit(FaceAttribute::IsPortal)
    | faceAttributeBit(FaceAttribute::Invisible) | faceAttributeBit(FaceAttribute::Untouchable);

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::invalid_argument(std::string("Breach maze scene: ") + pMessage);
    }
}

class GeometryIdentity
{
public:
    void byte(uint8_t value)
    {
        m_value = (m_value ^ value) * 1099511628211ULL;
    }

    void number(uint32_t value)
    {
        for (size_t shift = 0; shift < 32; shift += 8)
        {
            byte(uint8_t(value >> shift));
        }
    }

    template <typename... Values>
    void numbers(Values... values)
    {
        (number(uint32_t(values)), ...);
    }

    template <typename Value>
    void sequence(const std::vector<Value> &values)
    {
        number(uint32_t(values.size()));
        for (Value value : values)
        {
            number(uint32_t(value));
        }
    }

    void string(std::string_view value)
    {
        number(uint32_t(value.size()));
        for (unsigned char character : value)
        {
            byte(character);
        }
    }

    uint64_t value() const
    {
        return m_value;
    }

private:
    uint64_t m_value = 14695981039346656037ULL;
};

uint64_t geometryIdentity(const IndoorMapData &map)
{
    GeometryIdentity hash;
    hash.number(uint32_t(map.vertices.size()));
    for (const IndoorVertex &vertex : map.vertices)
    {
        hash.numbers(vertex.x, vertex.y, vertex.z);
    }
    hash.number(uint32_t(map.faces.size()));
    for (const IndoorFace &face : map.faces)
    {
        hash.number(face.attributes);
        hash.sequence(face.vertexIndices);
        hash.sequence(face.textureUs);
        hash.sequence(face.textureVs);
        hash.numbers(face.textureDeltaU, face.textureDeltaV, face.textureFrameTableIndex, face.textureFrameTableCog,
            face.cogNumber, face.cogTriggered, face.cogTriggerType, face.lightLevel,
            face.roomNumber, face.roomBehindNumber, face.facetType);
        hash.string(face.textureName);
        hash.number(face.isPortal);
    }
    hash.number(uint32_t(map.sectors.size()));
    for (const IndoorSector &sector : map.sectors)
    {
        hash.numbers(sector.flags, sector.unknown0, sector.floorCount, sector.wallCount, sector.ceilingCount,
            sector.fluidCount, sector.portalCount, sector.faceCount, sector.nonBspFaceCount, sector.cylinderFaceCount,
            sector.cogCount, sector.decorationCount, sector.markerCount, sector.lightCount,
            sector.waterLevel, sector.mistLevel, sector.lightDistanceMultiplier, sector.minAmbientLightLevel,
            sector.firstBspNode, sector.exitTag, sector.minX, sector.maxX, sector.minY, sector.maxY,
            sector.minZ, sector.maxZ);
        hash.sequence(sector.floorFaceIds);
        hash.sequence(sector.wallFaceIds);
        hash.sequence(sector.ceilingFaceIds);
        hash.sequence(sector.faceIds);
        hash.sequence(sector.nonBspFaceIds);
        hash.sequence(sector.cylinderFaceIds);
        hash.sequence(sector.lightIds);
        hash.sequence(sector.decorationIds);
        hash.sequence(sector.portalFaceIds);
    }
    hash.sequence(map.sectorData);
    hash.sequence(map.sectorLightData);
    hash.number(uint32_t(map.bspNodes.size()));
    for (const IndoorBspNode &node : map.bspNodes)
    {
        hash.numbers(node.front, node.back, node.faceIdOffset, node.faceCount);
    }
    hash.number(uint32_t(map.outlines.size()));
    for (const IndoorOutline &outline : map.outlines)
    {
        hash.numbers(outline.vertex1Id, outline.vertex2Id, outline.face1Id, outline.face2Id, outline.z, outline.flags);
    }
    return hash.value();
}

BreachMazeFaceRole sourceRole(std::string texture)
{
    for (char &character : texture)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character += 'a' - 'A';
        }
    }
    const std::array<std::string_view, 4> roles = {"rmzwal", "rmzflr", "rmzmsc", "rmzdec"};
    const auto found = std::find(roles.begin(), roles.end(), texture);
    require(found != roles.end(), "unexpected original service material");
    return static_cast<BreachMazeFaceRole>(found - roles.begin());
}

void validateTemplate(const IndoorMapData &originalMap, const BreachMazeTemplate &mazeTemplate)
{
    require(originalMap.version == 8 && originalMap.vertices.size() == 4224 && originalMap.faces.size() == 5632
        && originalMap.rawFaceCount == 5632 && originalMap.sectors.size() == 2 && originalMap.sectorCount == 2
        && originalMap.entities.size() == 256 && originalMap.lights.size() == 256 && originalMap.doorCount == 0,
        "input is not the original BrBase geometry");
    require(geometryIdentity(originalMap) == OriginalGeometryIdentity, "original geometry identity mismatch");

    BreachMazeTemplate expected;
    expected.vertexCount = uint32_t(originalMap.vertices.size());
    expected.decorationCount = uint32_t(originalMap.entities.size());
    expected.lightCount = uint32_t(originalMap.lights.size());
    expected.faces.reserve(originalMap.faces.size());
    for (size_t index = 0; index < expected.cells.size(); ++index)
    {
        BreachMazeTemplateCell &cell = expected.cells[index];
        cell.cellId = uint16_t(index + 1);
        cell.x = uint8_t(index % 16 + 1);
        cell.y = uint8_t(index / 16 + 1);
    }
    std::array<size_t, 256> wallsPerCell = {};
    for (size_t faceIndex = 0; faceIndex < originalMap.faces.size(); ++faceIndex)
    {
        IndoorFaceGeometryData geometry;
        require(buildIndoorFaceGeometry(originalMap, originalMap.vertices, faceIndex, geometry),
            "invalid original face geometry");
        BreachMazeTemplateFace face;
        face.faceIndex = uint32_t(faceIndex);
        face.role = sourceRole(originalMap.faces[faceIndex].textureName);
        // The audited original's stored bounds have exactly five units of padding on every axis.
        face.sourceBounds = {int16_t(geometry.minX - 5), int16_t(geometry.maxX + 5),
            int16_t(geometry.minY - 5), int16_t(geometry.maxY + 5),
            int16_t(geometry.minZ - 5), int16_t(geometry.maxZ + 5)};
        face.sourceNormal = {geometry.normal.x, geometry.normal.y, geometry.normal.z};
        for (float &axis : face.sourceNormal)
        {
            const float rounded = std::round(axis);
            require(std::abs(axis - rounded) < 0.00001f, "original template normal is not axial");
            axis = rounded;
        }
        const int x = int(std::ceil((face.sourceBounds[1] + 290.75) / BreachMazeCellWidth));
        const int y = int(std::ceil((-face.sourceBounds[2] + 290.75) / BreachMazeCellWidth));
        require(x >= 1 && x <= 16 && y >= 1 && y <= 16, "original facet outside template grid");
        face.cellId = uint16_t((y - 1) * 16 + x);
        BreachMazeTemplateCell &cell = expected.cells[face.cellId - 1];
        cell.faceIndices.push_back(uint32_t(faceIndex));
        if (face.role == BreachMazeFaceRole::Wall)
        {
            const std::array<std::array<float, 3>, 4> normals = {{
                {1, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 1, 0}}};
            const auto found = std::find(normals.begin(), normals.end(), face.sourceNormal);
            require(found != normals.end(), "original wall has invalid direction");
            face.wallSide = static_cast<BreachMazeSide>(found - normals.begin());
            const size_t wall = wallsPerCell[face.cellId - 1]++;
            require(wall < 4, "original cell contains too many walls");
            cell.wallFaceIndices[wall] = uint32_t(faceIndex);
        }
        else if (face.role == BreachMazeFaceRole::Floor
            && std::abs((face.sourceBounds[0] + face.sourceBounds[1]) / 2.0 - (x - 1) * BreachMazeCellWidth) < 100
            && std::abs((face.sourceBounds[2] + face.sourceBounds[3]) / 2.0 + (y - 1) * BreachMazeCellWidth) < 100)
        {
            cell.centralFloorFaceIndex = uint32_t(faceIndex);
        }
        expected.faces.push_back(face);
    }
    require(mazeTemplate == expected, "template metadata does not match original geometry");
}
}

BreachMazeSceneData buildBreachMazeScene(
    const IndoorMapData &originalMap,
    const MapDeltaData &baseDelta,
    const BreachMazeTemplate &mazeTemplate,
    const BreachMazeLayout &layout,
    const std::array<uint8_t, 16> &paletteByRoom)
{
    validateTemplate(originalMap, mazeTemplate);
    require(layout.algorithmVersion == BreachMazeAlgorithmVersion, "unsupported layout algorithm version");
    // Exact regeneration also validates room-label timing, endpoints, PRNG metadata and the entire graph.
    require(layout == generateBreachMazeLayout(mazeTemplate, layout.level, layout.seed),
        "layout does not match its versioned seed and template");
    require(baseDelta.surfaceRevision != std::numeric_limits<uint64_t>::max(), "surface revision overflow");
    require(baseDelta.faceAttributes.empty() || baseDelta.faceAttributes.size() == originalMap.faces.size(),
        "base delta face count mismatch");
    for (const BreachMazeCell &cell : layout.cells)
    {
        require(paletteByRoom[cell.room - 1] >= 1 && paletteByRoom[cell.room - 1] <= Materials.size(),
            "used room requires explicit original palette 1..4");
    }

    BreachMazeSceneData result{originalMap, baseDelta, {}};
    for (const BreachMazeTemplateCell &cell : mazeTemplate.cells)
    {
        const bool active = cell.x <= layout.side && cell.y <= layout.side;
        for (uint32_t faceIndex : cell.faceIndices)
        {
            IndoorFace &face = result.map.faces[faceIndex];
            face.cogTriggered = 0;
            face.attributes &= ~faceAttributeBit(FaceAttribute::Clickable);
            if (!active)
            {
                face.attributes |= faceAttributeBit(FaceAttribute::Invisible);
                continue;
            }
            face.attributes &= ~faceAttributeBit(FaceAttribute::Invisible);
            const BreachMazeCell &mazeCell = layout.cellAt(cell.x, cell.y);
            const BreachMazeTemplateFace &templateFace = mazeTemplate.faces[faceIndex];
            const size_t role = static_cast<size_t>(templateFace.role);
            const bool openWall = templateFace.role == BreachMazeFaceRole::Wall
                && !mazeCell.walls[static_cast<size_t>(*templateFace.wallSide)];
            if (templateFace.role == BreachMazeFaceRole::Wall)
            {
                face.isPortal = openWall;
                face.attributes = (face.attributes & ~OpenWallAttributes) | (openWall ? OpenWallAttributes : 0);
            }
            // As in ProcessFacet, the original service texture remains on an invisible open main wall.
            if (!openWall)
            {
                face.textureName = Materials[paletteByRoom[mazeCell.room - 1] - 1][role];
            }
        }
    }
    const std::array<BreachMazeEndpoint, 2> endpoints = {layout.endpoints.entrance, layout.endpoints.exit};
    for (size_t index = 0; index < endpoints.size(); ++index)
    {
        IndoorFace &face = result.map.faces[endpoints[index].faceIndex];
        face.cogTriggered = uint16_t(index + 1);
        face.attributes |= faceAttributeBit(FaceAttribute::Clickable);
        face.textureName = index == 0 ? "rmzdra" : "rmzdrb";
    }
    std::set<std::string> textures;
    result.delta.faceAttributes.resize(result.map.faces.size());
    for (size_t index = 0; index < result.map.faces.size(); ++index)
    {
        const IndoorFace &face = result.map.faces[index];
        result.delta.faceAttributes[index] = face.attributes;
        if (!face.isPortal && !hasFaceAttribute(face.attributes, FaceAttribute::Invisible))
        {
            textures.insert(face.textureName);
        }
    }
    ++result.delta.surfaceRevision;
    result.requiredTextures.assign(textures.begin(), textures.end());
    return result;
}
}
