#include "doctest/doctest.h"
#include "game/BinaryReader.h"
#include "game/content/BreachMazeLayout.h"
#include "tools/mmerge_import/BreachMazeTemplateResources.h"
#include "tools/mmerge_import/LodResources.h"

#include <zlib.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <queue>
#include <string>
#include <vector>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
constexpr size_t FaceHeaders = 0x8c + 4224 * 6 + 4;
constexpr size_t West = 0;
constexpr size_t East = 1;
constexpr size_t North = 2;
constexpr size_t South = 3;

std::filesystem::path originalSource()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

const BreachMazeTemplateProfile &profile()
{
    static const BreachMazeTemplateProfile value = loadBreachMazeTemplateProfile(originalSource()).value();
    return value;
}

const std::vector<uint8_t> &originalGeometry()
{
    static const std::vector<uint8_t> bytes = readLodMap(originalSource() / "Data/breach.games.lod", "brbase.blv");
    return bytes;
}

void putNumber(std::vector<uint8_t> &bytes, size_t offset, uint32_t value, size_t width = 4)
{
    for (size_t byte = 0; byte < width; ++byte)
    {
        bytes.at(offset + byte) = uint8_t(value >> (byte * 8));
    }
}

size_t textureOffset()
{
    uint32_t facetDataBytes = 0;
    REQUIRE(ByteReader(originalGeometry()).readUInt32(0x68, facetDataBytes));
    return FaceHeaders + 5632 * 0x60 + facetDataBytes;
}

// Test-owned candidate sources are retained under checkpoint 143, never in originals or sealed evidence.
std::filesystem::path candidateSource()
{
    const std::string nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path result = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/breach-maze-foundation-143" / ("source-fixture-" + nonce);
    REQUIRE(std::filesystem::create_directories(result / "Scripts/Maps"));
    REQUIRE(std::filesystem::create_directories(result / "Data"));
    return result;
}

void copyScript(const std::filesystem::path &source)
{
    REQUIRE(std::filesystem::copy_file(originalSource() / "Scripts/Maps/BrBase.lua",
        source / "Scripts/Maps/BrBase.lua"));
}

// Small, valid one-member test LOD lets the production importer reach its geometry SHA gate.
void writeGeometryArchive(const std::filesystem::path &path, const std::vector<uint8_t> &geometry)
{
    uLongf compressedSize = compressBound(uLong(geometry.size()));
    std::vector<uint8_t> compressed(compressedSize);
    REQUIRE_EQ(compress2(compressed.data(), &compressedSize, geometry.data(), uLong(geometry.size()),
        Z_BEST_SPEED), Z_OK);
    compressed.resize(compressedSize);
    std::vector<uint8_t> archive(288 + 32 + 16 + compressed.size());
    putNumber(archive, 0, 0x00444f4c);
    const std::string version = "GameMMVI";
    std::copy(version.begin(), version.end(), archive.begin() + 4);
    putNumber(archive, 272, 288);
    putNumber(archive, 276, uint32_t(archive.size() - 288));
    putNumber(archive, 284, 1);
    const std::string member = "brbase.blv";
    std::copy(member.begin(), member.end(), archive.begin() + 288);
    putNumber(archive, 288 + 16, 32);
    putNumber(archive, 288 + 20, uint32_t(compressed.size() + 16));
    putNumber(archive, 320 + 4, 0x6969766d);
    putNumber(archive, 320 + 8, uint32_t(compressed.size()));
    putNumber(archive, 320 + 12, uint32_t(geometry.size()));
    std::copy(compressed.begin(), compressed.end(), archive.begin() + 336);
    std::ofstream output(path, std::ios::binary);
    REQUIRE(bool(output.write(reinterpret_cast<const char *>(archive.data()), std::streamsize(archive.size()))));
}

BreachMazeLayout closedFourByFour()
{
    BreachMazeLayout layout;
    layout.side = 4;
    layout.cells.resize(16);
    for (uint8_t y = 1; y <= 4; ++y)
    {
        for (uint8_t x = 1; x <= 4; ++x)
        {
            layout.cells[(y - 1) * 4 + x - 1].templateCellId = uint16_t((y - 1) * 16 + x);
        }
    }
    return layout;
}

void openEast(BreachMazeLayout &layout, size_t x, size_t y)
{
    layout.cells.at((y - 1) * 4 + x - 1).walls[East] = false;
    layout.cells.at((y - 1) * 4 + x).walls[West] = false;
}

void openSouth(BreachMazeLayout &layout, size_t x, size_t y)
{
    layout.cells.at((y - 1) * 4 + x - 1).walls[South] = false;
    layout.cells.at(y * 4 + x - 1).walls[North] = false;
}

// Independent graph oracle: closed boundary, reciprocal openings, one spanning tree across every cell.
void checkGraph(const BreachMazeLayout &layout)
{
    REQUIRE_EQ(layout.cells.size(), size_t(layout.side) * layout.side);
    size_t edges = 0;
    bool symmetric = true;
    bool closedBoundary = true;
    bool identities = true;
    bool roomLabels = true;
    for (uint8_t y = 1; y <= layout.side; ++y)
    {
        for (uint8_t x = 1; x <= layout.side; ++x)
        {
            const BreachMazeCell &cell = layout.cellAt(x, y);
            identities &= cell.templateCellId == (y - 1) * 16 + x;
            roomLabels &= cell.room >= 1 && cell.room <= layout.side;
            closedBoundary &= (x != 1 || cell.walls[West]) && (x != layout.side || cell.walls[East])
                && (y != 1 || cell.walls[North]) && (y != layout.side || cell.walls[South]);
            if (x < layout.side)
            {
                symmetric &= cell.walls[East] == layout.cellAt(x + 1, y).walls[West];
                edges += !cell.walls[East];
            }
            if (y < layout.side)
            {
                symmetric &= cell.walls[South] == layout.cellAt(x, y + 1).walls[North];
                edges += !cell.walls[South];
            }
        }
    }
    REQUIRE(closedBoundary);
    REQUIRE(symmetric);
    CHECK(identities);
    CHECK(roomLabels);
    CHECK_EQ(edges, layout.cells.size() - 1);
    std::vector<bool> visited(layout.cells.size(), false);
    std::queue<size_t> pending;
    pending.push(0);
    visited[0] = true;
    const std::array<int, 4> offsets = {-1, 1, -int(layout.side), int(layout.side)};
    while (!pending.empty())
    {
        const size_t cellIndex = pending.front();
        pending.pop();
        for (size_t direction = 0; direction < 4; ++direction)
        {
            if (!layout.cells[cellIndex].walls[direction])
            {
                const size_t next = size_t(int(cellIndex) + offsets[direction]);
                REQUIRE(next < visited.size());
                if (!visited[next])
                {
                    visited[next] = true;
                    pending.push(next);
                }
            }
        }
    }
    CHECK_EQ(std::count(visited.begin(), visited.end(), true), layout.cells.size());
}

void checkEndpoint(const BreachMazeLayout &layout, const BreachMazeEndpoint &endpoint)
{
    REQUIRE(endpoint.templateCellId >= 1);
    REQUIRE(endpoint.templateCellId <= 256);
    const BreachMazeTemplateCell &cell = profile().mazeTemplate.cells[endpoint.templateCellId - 1];
    REQUIRE(cell.x <= layout.side);
    REQUIRE(cell.y <= layout.side);
    CHECK(layout.cellAt(cell.x, cell.y).walls[static_cast<size_t>(endpoint.wallSide)]);
    CHECK(profile().mazeTemplate.faces[endpoint.faceIndex].wallSide == endpoint.wallSide);
    CHECK_EQ(profile().mazeTemplate.faces[endpoint.faceIndex].cellId, endpoint.templateCellId);
    const std::array<bool, 4> outer = {cell.x == 1, cell.x == layout.side, cell.y == 1, cell.y == layout.side};
    CHECK(outer[static_cast<size_t>(endpoint.wallSide)]);
    const auto firstOuter = std::find_if(cell.wallFaceIndices.begin(), cell.wallFaceIndices.end(),
        [&](uint32_t face)
        {
            return outer[static_cast<size_t>(*profile().mazeTemplate.faces[face].wallSide)];
        });
    REQUIRE(firstOuter != cell.wallFaceIndices.end());
    CHECK_EQ(endpoint.faceIndex, *firstOuter);
}

uint64_t layoutFingerprint(const BreachMazeLayout &layout)
{
    uint64_t digest = 14695981039346656037ULL;
    const auto append = [&](uint64_t value, size_t bytes)
    {
        for (size_t index = 0; index < bytes; ++index)
        {
            digest = (digest ^ uint8_t(value >> (index * 8))) * 1099511628211ULL;
        }
    };
    append(layout.side, 1);
    append(layout.level, 4);
    for (const BreachMazeCell &cell : layout.cells)
    {
        append(cell.templateCellId, 2);
        append(cell.room, 2);
        uint8_t wallMask = 0;
        for (size_t direction = 0; direction < 4; ++direction)
        {
            wallMask |= uint8_t(cell.walls[direction]) << direction;
        }
        append(wallMask, 1);
    }
    for (const BreachMazeEndpoint &endpoint : {layout.endpoints.entrance, layout.endpoints.exit})
    {
        append(endpoint.templateCellId, 2);
        append(endpoint.faceIndex, 4);
        append(static_cast<uint8_t>(endpoint.wallSide), 1);
    }
    append(static_cast<uint8_t>(layout.endpoints.entranceSearchSide), 1);
    append(static_cast<uint8_t>(layout.endpoints.exitSearchSide), 1);
    append(layout.endpoints.entranceUsedFallback, 1);
    append(layout.endpoints.exitUsedFallback, 1);
    append(layout.endpoints.duplicateCellFallback, 1);
    return digest;
}
}

TEST_CASE("BrBase original template maps all zero-based facets into 256 source cells")
{
    const BreachMazeTemplate &maze = profile().mazeTemplate;
    CHECK_EQ(profile().sourceScriptSha256, "58d0978f2d8683d3d94cfeb6048917c0cc61d9818a8f272ca1baca2e6005d60b");
    CHECK_EQ(profile().geometrySha256, "818920ea90ccbbb15330b2e8cb0cd458a53416297913d29ddb94b81f33bf2898");
    CHECK_EQ(maze.version, 1);
    CHECK_EQ(maze.vertexCount, 4224);
    CHECK_EQ(maze.decorationCount, 256);
    CHECK_EQ(maze.lightCount, 256);
    REQUIRE_EQ(maze.faces.size(), 5632);
    std::vector<uint8_t> ownership(maze.faces.size(), 0);
    for (const BreachMazeTemplateCell &cell : maze.cells)
    {
        CHECK_EQ(cell.cellId, (cell.y - 1) * 16 + cell.x);
        REQUIRE_EQ(cell.faceIndices.size(), 22);
        CHECK(std::is_sorted(cell.faceIndices.begin(), cell.faceIndices.end()));
        std::array<size_t, 4> roleCounts = {};
        for (uint32_t faceIndex : cell.faceIndices)
        {
            REQUIRE(faceIndex < maze.faces.size());
            const BreachMazeTemplateFace &face = maze.faces[faceIndex];
            CHECK_EQ(face.faceIndex, faceIndex);
            CHECK_EQ(face.cellId, cell.cellId);
            ++ownership[faceIndex];
            ++roleCounts[static_cast<size_t>(face.role)];
        }
        CHECK(roleCounts == std::array<size_t, 4>{4, 5, 8, 5});
        CHECK(maze.faces[cell.centralFloorFaceIndex].role == BreachMazeFaceRole::Floor);
    }
    CHECK(std::all_of(ownership.begin(), ownership.end(), [](uint8_t owners) { return owners == 1; }));
    CHECK_EQ(maze.faces[0].cellId, 166);
    CHECK(maze.faces[0].role == BreachMazeFaceRole::Decoration);
    CHECK(maze.faces[0].sourceBounds == std::array<int16_t, 6>{2762, 3253, -5779, -5709, 206, 216});
    CHECK(maze.cells[0].wallFaceIndices == std::array<uint32_t, 4>{5253, 5461, 5505, 5507});
    CHECK_EQ(maze.cells[0].centralFloorFaceIndex, 2422);
    CHECK(analyzeBreachMazeTemplate(originalGeometry()) == maze);
}

TEST_CASE("BrBase profile rejects changed scripts geometry and paths escaping its source")
{
    const std::filesystem::path source = candidateSource();
    CHECK_FALSE(loadBreachMazeTemplateProfile(source).has_value());
    SUBCASE("modified script")
    {
        copyScript(source);
        std::ofstream(source / "Scripts/Maps/BrBase.lua", std::ios::app) << "\n-- modified\n";
        CHECK_THROWS_WITH(loadBreachMazeTemplateProfile(source), "BrBase template: unsupported BrBase.lua fingerprint");
    }
    SUBCASE("escaping script")
    {
        std::filesystem::create_symlink(originalSource() / "Scripts/Maps/BrBase.lua",
            source / "Scripts/Maps/BrBase.lua");
        CHECK_THROWS_WITH(loadBreachMazeTemplateProfile(source),
            "BrBase template: source file escapes input root or is not a file");
    }
    SUBCASE("escaping archive")
    {
        copyScript(source);
        std::filesystem::create_symlink(originalSource() / "Data/breach.games.lod", source / "Data/breach.games.lod");
        CHECK_THROWS_WITH(loadBreachMazeTemplateProfile(source),
            "BrBase template: source file escapes input root or is not a file");
    }
    SUBCASE("independent geometry fingerprint even when structure remains valid")
    {
        copyScript(source);
        writeGeometryArchive(source / "Data/breach.games.lod", originalGeometry());
        REQUIRE(loadBreachMazeTemplateProfile(source).has_value());
        std::vector<uint8_t> changed = originalGeometry();
        changed.at(textureOffset()) ^= 0x20; // Texture letter case is structurally equivalent.
        CHECK(analyzeBreachMazeTemplate(changed) == profile().mazeTemplate);
        writeGeometryArchive(source / "Data/breach.games.lod", changed);
        CHECK_THROWS_WITH(loadBreachMazeTemplateProfile(source), "BrBase template: unsupported BrBase.blv fingerprint");
    }
}

TEST_CASE("BrBase template analyzer rejects malformed geometry rather than dropping or guessing faces")
{
    std::vector<uint8_t> changed = originalGeometry();
    SUBCASE("truncated BLV")
    {
        changed.resize(FaceHeaders + 10);
        CHECK_THROWS(analyzeBreachMazeTemplate(changed));
    }
    SUBCASE("unknown service material on facet zero")
    {
        changed.at(textureOffset()) = 'x';
        CHECK_THROWS_WITH(analyzeBreachMazeTemplate(changed), "BrBase template: unrecognized service texture xmzdec");
    }
    SUBCASE("non finite source normal")
    {
        putNumber(changed, FaceHeaders, 0x7fc00000);
        CHECK_THROWS_WITH(analyzeBreachMazeTemplate(changed), "BrBase template: non-finite facet normal");
    }
    SUBCASE("source padded bounds outside grid")
    {
        putNumber(changed, FaceHeaders + 0x50, 30000, 2);
        putNumber(changed, FaceHeaders + 0x52, 30001, 2);
        CHECK_THROWS_WITH(analyzeBreachMazeTemplate(changed),
            "BrBase template: facet outside the original 16x16 cell grid");
    }
    SUBCASE("reversed bounds")
    {
        putNumber(changed, FaceHeaders + 0x50, 3200, 2);
        putNumber(changed, FaceHeaders + 0x52, 3000, 2);
        CHECK_THROWS_WITH(analyzeBreachMazeTemplate(changed), "BrBase template: reversed facet bounds");
    }
    SUBCASE("duplicate cardinal wall direction")
    {
        putNumber(changed, FaceHeaders + 5253 * 0x60, std::bit_cast<uint32_t>(-1.0f));
        CHECK_THROWS_WITH(analyzeBreachMazeTemplate(changed),
            "BrBase template: cell has duplicate main wall directions");
    }
}

TEST_CASE("BrBase Eller layouts are connected acyclic symmetric and bounded across floors and seeds")
{
    std::array<bool, 17> generatedSides = {};
    for (uint32_t level : {1u, 8u, 9u, 16u, 50u, std::numeric_limits<uint32_t>::max()})
    {
        for (uint64_t seed = 0; seed < 128; ++seed)
        {
            CAPTURE(level);
            CAPTURE(seed);
            const BreachMazeLayout layout = generateBreachMazeLayout(profile().mazeTemplate, level, seed);
            CHECK_EQ(layout.algorithmVersion, 1);
            CHECK_EQ(layout.level, level);
            CHECK_EQ(layout.seed, seed);
            REQUIRE(layout.side >= 4);
            REQUIRE(layout.side <= std::clamp(level, 8u, 16u));
            generatedSides[layout.side] = true;
            CHECK_EQ(layout.finalRandomState, seed + 0x9e3779b97f4a7c15ULL * layout.randomDrawCount);
            CHECK(layout.randomDrawCount > size_t(layout.side) * layout.side);
            checkGraph(layout);
            CHECK(layout.endpoints.entranceSearchSide != layout.endpoints.exitSearchSide);
            CHECK(layout.endpoints.entrance.templateCellId != layout.endpoints.exit.templateCellId);
            checkEndpoint(layout, layout.endpoints.entrance);
            checkEndpoint(layout, layout.endpoints.exit);
            if (seed < 4)
            {
                CHECK(generateBreachMazeLayout(profile().mazeTemplate, level, seed) == layout);
            }
        }
    }
    for (size_t side = 4; side <= 16; ++side)
    {
        CAPTURE(side);
        CHECK(generatedSides[side]);
    }
    CHECK_EQ(BreachMazePrng, "splitmix64-rejection-v1");
}

TEST_CASE("BrBase version one layouts retain independent golden room walls endpoints and PRNG state")
{
    // Set-based Python oracle in checkpoint 143; versioned port behavior, not a Lua random stream assertion.
    struct Golden
    {
        uint32_t level;
        uint64_t seed;
        uint8_t side;
        uint64_t draws;
        uint64_t state;
        uint64_t fingerprint;
    };
    const std::array<Golden, 4> cases = {{
        {1, 0, 4, 27, 12671394942373533239ULL, 1328818004051597915ULL},
        {9, 1, 9, 138, 5325398801289503571ULL, 9774627637333329740ULL},
        {16, 42, 13, 289, 11286137664104174559ULL, 8190702190044333918ULL},
        {50, std::numeric_limits<uint64_t>::max(), 8, 110, 18146777187011875077ULL, 4156872653702695169ULL},
    }};
    for (const Golden &expected : cases)
    {
        CAPTURE(expected.level);
        CAPTURE(expected.seed);
        const BreachMazeLayout layout = generateBreachMazeLayout(profile().mazeTemplate, expected.level, expected.seed);
        CHECK_EQ(layout.side, expected.side);
        CHECK_EQ(layout.randomDrawCount, expected.draws);
        CHECK_EQ(layout.finalRandomState, expected.state);
        CHECK_EQ(layoutFingerprint(layout), expected.fingerprint);
    }
}

TEST_CASE("BrBase endpoint scan keeps source shift fallback and original corner facet ordering")
{
    BreachMazeLayout comb = closedFourByFour();
    for (size_t x = 1; x <= 4; ++x)
    {
        if (x < 4)
        {
            openEast(comb, x, 1);
        }
        for (size_t y = 1; y < 4; ++y)
        {
            openSouth(comb, x, y);
        }
    }
    const BreachMazeTemplate &maze = profile().mazeTemplate;
    const BreachMazeEndpoints sides = selectBreachMazeEndpoints(maze, comb, BreachMazeSide::West, BreachMazeSide::East);
    CHECK_EQ(sides.entrance.templateCellId, 49);
    CHECK_EQ(sides.exit.templateCellId, 52);
    CHECK_FALSE(sides.entranceUsedFallback);
    CHECK_FALSE(sides.exitUsedFallback);
    CHECK_FALSE(sides.duplicateCellFallback);
    const BreachMazeEndpoints fallback = selectBreachMazeEndpoints(maze, comb,
        BreachMazeSide::North, BreachMazeSide::South);
    CHECK(fallback.entranceUsedFallback);
    CHECK_EQ(fallback.entrance.templateCellId, 1);
    CHECK_EQ(fallback.exit.templateCellId, 50); // Exit starts at coordinate 2, not 1.
    CHECK(fallback.entrance.wallSide == BreachMazeSide::West); // Source order overrides searched North at corner.
    CHECK_EQ(fallback.entrance.faceIndex, 5253);
    const BreachMazeEndpoints duplicate = selectBreachMazeEndpoints(maze, comb,
        BreachMazeSide::South, BreachMazeSide::West);
    CHECK(duplicate.duplicateCellFallback);
    CHECK_EQ(duplicate.entrance.templateCellId, 1);
    CHECK_EQ(duplicate.exit.templateCellId, 4);

    BreachMazeLayout snake = closedFourByFour();
    for (size_t y = 1; y <= 4; ++y)
    {
        for (size_t x = 1; x < 4; ++x)
        {
            openEast(snake, x, y);
        }
        if (y < 4)
        {
            openSouth(snake, y % 2 == 1 ? 4 : 1, y);
        }
    }
    const BreachMazeEndpoints skippedCorner = selectBreachMazeEndpoints(maze, snake,
        BreachMazeSide::West, BreachMazeSide::North);
    CHECK_EQ(skippedCorner.entrance.templateCellId, 1);
    CHECK(skippedCorner.exitUsedFallback);
    CHECK_EQ(skippedCorner.exit.templateCellId, 4);
}

TEST_CASE("BrBase layout rejects invalid template metadata level and active cell references")
{
    const BreachMazeTemplate &maze = profile().mazeTemplate;
    CHECK_THROWS(generateBreachMazeLayout(maze, 0, 0));
    BreachMazeTemplate invalid = maze;
    invalid.version = 2;
    CHECK_THROWS(generateBreachMazeLayout(invalid, 1, 0));
    invalid = maze;
    std::swap(invalid.cells[0].wallFaceIndices[0], invalid.cells[0].wallFaceIndices[1]);
    CHECK_THROWS(generateBreachMazeLayout(invalid, 1, 0));
    BreachMazeLayout layout = generateBreachMazeLayout(maze, 1, 0);
    CHECK_THROWS(layout.cellAt(0, 1));
    CHECK_THROWS(layout.cellAt(layout.side + 1, 1));
    CHECK_THROWS(selectBreachMazeEndpoints(maze, layout, BreachMazeSide::West, BreachMazeSide::West));
    CHECK_THROWS(selectBreachMazeEndpoints(maze, layout, static_cast<BreachMazeSide>(4), BreachMazeSide::West));
    layout.cells[0].templateCellId = 256;
    CHECK_THROWS(selectBreachMazeEndpoints(maze, layout, BreachMazeSide::West, BreachMazeSide::East));
    layout.cells.pop_back();
    CHECK_THROWS(layout.cellAt(1, 1));
}
