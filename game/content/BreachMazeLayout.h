#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace OpenYAMM::Game
{
inline constexpr uint8_t BreachMazeTemplateSide = 16;
inline constexpr size_t BreachMazeTemplateCellCount = 256;
inline constexpr double BreachMazeCellWidth = 601.5;
inline constexpr uint32_t BreachMazeTemplateVersion = 1;
inline constexpr uint32_t BreachMazeAlgorithmVersion = 1;
inline constexpr std::string_view BreachMazePrng = "splitmix64-rejection-v1";

// The order matches the original GetFinalCell side numbers, minus one.
enum class BreachMazeSide : uint8_t
{
    West,
    East,
    North,
    South,
};

enum class BreachMazeFaceRole : uint8_t
{
    Wall,
    Floor,
    Miscellaneous,
    Decoration,
};

struct BreachMazeTemplateFace
{
    uint32_t faceIndex = 0; // Original zero-based BLV facet index, including facet 0.
    uint16_t cellId = 0; // Original one-based CellsInfo index: (y - 1) * 16 + x.
    BreachMazeFaceRole role = BreachMazeFaceRole::Decoration;
    std::optional<BreachMazeSide> wallSide;
    std::array<int16_t, 6> sourceBounds = {}; // minX, maxX, minY, maxY, minZ, maxZ; includes source padding.
    std::array<float, 3> sourceNormal = {};

    bool operator==(const BreachMazeTemplateFace &) const = default;
};

struct BreachMazeTemplateCell
{
    uint16_t cellId = 0;
    uint8_t x = 0; // One-based column and row, as in the source script.
    uint8_t y = 0;
    std::vector<uint32_t> faceIndices;
    std::array<uint32_t, 4> wallFaceIndices = {}; // Original facet traversal order, not cardinal order.
    uint32_t centralFloorFaceIndex = 0;

    bool operator==(const BreachMazeTemplateCell &) const = default;
};

struct BreachMazeTemplate
{
    uint32_t version = BreachMazeTemplateVersion;
    uint32_t vertexCount = 0;
    uint32_t decorationCount = 0;
    uint32_t lightCount = 0;
    std::vector<BreachMazeTemplateFace> faces;
    std::array<BreachMazeTemplateCell, BreachMazeTemplateCellCount> cells;

    bool operator==(const BreachMazeTemplate &) const = default;
};

struct BreachMazeCell
{
    uint16_t templateCellId = 0;
    uint16_t room = 0; // Source row-set label, retained for later room material selection.
    std::array<bool, 4> walls = {true, true, true, true};

    bool operator==(const BreachMazeCell &) const = default;
};

struct BreachMazeEndpoint
{
    uint16_t templateCellId = 0;
    uint32_t faceIndex = 0;
    BreachMazeSide wallSide = BreachMazeSide::West;

    bool operator==(const BreachMazeEndpoint &) const = default;
};

struct BreachMazeEndpoints
{
    BreachMazeSide entranceSearchSide = BreachMazeSide::West;
    BreachMazeSide exitSearchSide = BreachMazeSide::East;
    BreachMazeEndpoint entrance; // Source event 1 / RmzDrA: goes up, or leaves level 1.
    BreachMazeEndpoint exit; // Source event 2 / RmzDrB: goes down; locked before QuestFinished.
    bool entranceUsedFallback = false;
    bool exitUsedFallback = false;
    bool duplicateCellFallback = false;

    bool operator==(const BreachMazeEndpoints &) const = default;
};

struct BreachMazeLayout
{
    uint32_t algorithmVersion = BreachMazeAlgorithmVersion;
    uint64_t seed = 0;
    uint64_t finalRandomState = 0;
    uint64_t randomDrawCount = 0;
    uint32_t level = 0;
    uint8_t side = 0;
    std::vector<BreachMazeCell> cells; // Compact row-major active grid; IDs retain the template stride of 16.
    BreachMazeEndpoints endpoints;

    const BreachMazeCell &cellAt(uint8_t x, uint8_t y) const;
    bool operator==(const BreachMazeLayout &) const = default;
};

// Pure content generation. Does not modify scene geometry, travel, party, saves or quest flags.
// Stable ascending set/column traversal replaces unspecified Lua pairs order. The stream does not
// emulate math.random or consume the later material/population draws from the original script.
BreachMazeLayout generateBreachMazeLayout(const BreachMazeTemplate &mazeTemplate, uint32_t level, uint64_t seed);

// Source boundary scan: entrance starts at coordinate 1, exit at 2; missing candidates use cells 1/side.
// At corners the first closed outer wall in original facet order wins, independently of the search side.
BreachMazeEndpoints selectBreachMazeEndpoints(const BreachMazeTemplate &mazeTemplate,
    const BreachMazeLayout &layout, BreachMazeSide entranceSide, BreachMazeSide exitSide);
}
