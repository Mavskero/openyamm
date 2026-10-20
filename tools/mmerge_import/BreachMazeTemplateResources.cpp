#include "BreachMazeTemplateResources.h"
#include "LodResources.h"
#include "game/BinaryReader.h"
#include "game/indoor/IndoorMapData.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
constexpr std::string_view ScriptSha256 =
    "58d0978f2d8683d3d94cfeb6048917c0cc61d9818a8f272ca1baca2e6005d60b";
constexpr std::string_view GeometrySha256 =
    "818920ea90ccbbb15330b2e8cb0cd458a53416297913d29ddb94b81f33bf2898";
constexpr size_t SourceFaceCount = 5632;
constexpr size_t SourceVertexCount = 4224;

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::runtime_error(std::string("BrBase template: ") + pMessage);
    }
}

std::string sha256(const uint8_t *pBytes, size_t size)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(pBytes, static_cast<CC_LONG>(size), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return digest.str();
}

std::filesystem::path containedFile(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relative);
    const std::filesystem::path resolvedRelative = path.lexically_relative(root);
    require(!resolvedRelative.empty() && !resolvedRelative.is_absolute() && *resolvedRelative.begin() != ".."
        && std::filesystem::is_regular_file(path), "source file escapes input root or is not a file");
    return path;
}

std::vector<uint8_t> readScript(const std::filesystem::path &path)
{
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 256 * 1024, "invalid source script size");
    std::vector<uint8_t> bytes(size);
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "cannot read source script");
    return bytes;
}

Game::BreachMazeFaceRole faceRole(std::string texture)
{
    std::transform(texture.begin(), texture.end(), texture.begin(),
        [](unsigned char value) { return char(std::tolower(value)); });
    if (texture == "rmzwal")
    {
        return Game::BreachMazeFaceRole::Wall;
    }
    if (texture == "rmzflr")
    {
        return Game::BreachMazeFaceRole::Floor;
    }
    if (texture == "rmzmsc")
    {
        return Game::BreachMazeFaceRole::Miscellaneous;
    }
    if (texture == "rmzdec")
    {
        return Game::BreachMazeFaceRole::Decoration;
    }
    // The audited template has no RmzPrt facets. Do not silently classify unknown materials as decoration.
    throw std::runtime_error("BrBase template: unrecognized service texture " + texture);
}

Game::BreachMazeSide wallSide(const std::array<float, 3> &normal)
{
    require(normal[2] == 0.0f, "main wall normal is not horizontal");
    if (normal[0] == 1.0f && normal[1] == 0.0f)
    {
        return Game::BreachMazeSide::West;
    }
    if (normal[0] == -1.0f && normal[1] == 0.0f)
    {
        return Game::BreachMazeSide::East;
    }
    if (normal[0] == 0.0f && normal[1] == -1.0f)
    {
        return Game::BreachMazeSide::North;
    }
    if (normal[0] == 0.0f && normal[1] == 1.0f)
    {
        return Game::BreachMazeSide::South;
    }
    throw std::runtime_error("BrBase template: main wall normal is not cardinal");
}
}

Game::BreachMazeTemplate analyzeBreachMazeTemplate(const std::vector<uint8_t> &geometry)
{
    require(geometry.size() >= 0x8c && geometry.size() <= 4 * 1024 * 1024, "invalid BLV byte size");
    const std::optional<Game::IndoorMapData> map = Game::IndoorMapDataLoader().loadFromBytes(geometry);
    require(map.has_value() && map->version == 8, "invalid MM8 BLV geometry");
    require(map->vertices.size() == SourceVertexCount && map->faces.size() == SourceFaceCount
        && map->rawFaceCount == SourceFaceCount, "unexpected template vertex/facet count");
    require(map->sectors.size() == 2 && map->sectors[0].faceIds.empty()
        && map->sectors[1].faceIds.size() == SourceFaceCount && map->sectors[1].portalFaceIds.empty(),
        "template must retain its single populated sector");
    require(map->entities.size() == 256 && map->lights.size() == 256 && map->doorCount == 0,
        "unexpected template decoration/light/door count");

    Game::BreachMazeTemplate result;
    result.vertexCount = uint32_t(map->vertices.size());
    result.decorationCount = uint32_t(map->entities.size());
    result.lightCount = uint32_t(map->lights.size());
    result.faces.reserve(SourceFaceCount);
    for (size_t index = 0; index < result.cells.size(); ++index)
    {
        Game::BreachMazeTemplateCell &cell = result.cells[index];
        cell.cellId = uint16_t(index + 1);
        cell.x = uint8_t(index % Game::BreachMazeTemplateSide + 1);
        cell.y = uint8_t(index / Game::BreachMazeTemplateSide + 1);
    }

    // IndoorMapDataLoader validates the full BLV. Only the source-padded bounds and floating normals
    // absent from IndoorFace are read here; positions/indices/textures remain owned by that loader.
    const Game::ByteReader reader(geometry);
    const size_t headersOffset = 0x8c + SourceVertexCount * 6 + 4;
    std::array<std::array<size_t, 4>, Game::BreachMazeTemplateCellCount> roleCounts = {};
    std::array<std::array<bool, 4>, Game::BreachMazeTemplateCellCount> directions = {};
    std::array<size_t, Game::BreachMazeTemplateCellCount> centralFloorCounts = {};
    for (size_t faceIndex = 0; faceIndex < map->faces.size(); ++faceIndex)
    {
        const Game::IndoorFace &face = map->faces[faceIndex];
        require(face.roomNumber == 1 && face.roomBehindNumber == 0 && !face.isPortal
            && face.cogNumber == 0 && face.cogTriggered == 0, "unexpected original facet room/event ownership");
        const size_t header = headersOffset + faceIndex * 0x60;
        Game::BreachMazeTemplateFace record;
        record.faceIndex = uint32_t(faceIndex);
        record.role = faceRole(face.textureName);
        for (size_t axis = 0; axis < 3; ++axis)
        {
            uint32_t bits = 0;
            require(reader.readUInt32(header + axis * 4, bits), "truncated facet normal");
            record.sourceNormal[axis] = std::bit_cast<float>(bits);
            require(std::isfinite(record.sourceNormal[axis]), "non-finite facet normal");
            require(reader.readInt16(header + 0x50 + axis * 4, record.sourceBounds[axis * 2])
                && reader.readInt16(header + 0x52 + axis * 4, record.sourceBounds[axis * 2 + 1]),
                "truncated facet bounds");
            require(record.sourceBounds[axis * 2] <= record.sourceBounds[axis * 2 + 1], "reversed facet bounds");
        }
        constexpr double Radius = Game::BreachMazeCellWidth / 2 - 10;
        const int x = int(std::ceil((record.sourceBounds[1] + Radius) / Game::BreachMazeCellWidth));
        const int y = int(std::ceil((-record.sourceBounds[2] + Radius) / Game::BreachMazeCellWidth));
        require(x >= 1 && x <= 16 && y >= 1 && y <= 16, "facet outside the original 16x16 cell grid");
        record.cellId = uint16_t((y - 1) * 16 + x);
        const size_t cellIndex = record.cellId - 1;
        Game::BreachMazeTemplateCell &cell = result.cells[cellIndex];
        cell.faceIndices.push_back(uint32_t(faceIndex));
        const size_t roleIndex = static_cast<size_t>(record.role);
        const size_t roleCount = roleCounts[cellIndex][roleIndex]++;
        if (record.role == Game::BreachMazeFaceRole::Wall)
        {
            require(roleCount < cell.wallFaceIndices.size(), "cell has more than four main walls");
            record.wallSide = wallSide(record.sourceNormal);
            const size_t direction = static_cast<size_t>(*record.wallSide);
            require(!directions[cellIndex][direction], "cell has duplicate main wall directions");
            directions[cellIndex][direction] = true;
            cell.wallFaceIndices[roleCount] = uint32_t(faceIndex);
        }
        else if (record.role == Game::BreachMazeFaceRole::Floor)
        {
            const double midpointX = (record.sourceBounds[0] + record.sourceBounds[1]) / 2.0;
            const double midpointY = (record.sourceBounds[2] + record.sourceBounds[3]) / 2.0;
            if (std::abs(midpointX - Game::BreachMazeCellWidth * (x - 1)) < 100
                && std::abs(midpointY + Game::BreachMazeCellWidth * (y - 1)) < 100)
            {
                require(centralFloorCounts[cellIndex]++ == 0, "cell has multiple central floors");
                cell.centralFloorFaceIndex = uint32_t(faceIndex);
            }
        }
        result.faces.push_back(record);
    }

    for (size_t index = 0; index < result.cells.size(); ++index)
    {
        require(result.cells[index].faceIndices.size() == 22
            && roleCounts[index] == std::array<size_t, 4>{4, 5, 8, 5} && centralFloorCounts[index] == 1,
            "cell does not contain the audited 22-facet wall/floor/detail arrangement");
    }
    return result;
}

std::optional<BreachMazeTemplateProfile> loadBreachMazeTemplateProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::exists(root / "Scripts/Maps/BrBase.lua"))
    {
        return std::nullopt;
    }
    const std::vector<uint8_t> script = readScript(containedFile(root, "Scripts/Maps/BrBase.lua"));
    const std::string scriptHash = sha256(script.data(), script.size());
    require(scriptHash == ScriptSha256, "unsupported BrBase.lua fingerprint");
    const std::filesystem::path archive = containedFile(root, "Data/breach.games.lod");
    const std::vector<uint8_t> geometry = readLodMap(archive, "brbase.blv");
    require(geometry.size() <= 4 * 1024 * 1024, "oversized BrBase BLV");
    const std::string geometryHash = sha256(geometry.data(), geometry.size());
    require(geometryHash == GeometrySha256, "unsupported BrBase.blv fingerprint");
    return BreachMazeTemplateProfile{scriptHash, geometryHash, analyzeBreachMazeTemplate(geometry)};
}
}
