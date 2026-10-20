#include "game/content/BreachMazeLayout.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::invalid_argument(std::string("Breach maze: ") + pMessage);
    }
}

size_t sideIndex(BreachMazeSide side)
{
    const size_t index = static_cast<size_t>(side);
    require(index < 4, "invalid wall side");
    return index;
}

uint16_t templateCellId(uint8_t x, uint8_t y)
{
    return uint16_t((y - 1) * BreachMazeTemplateSide + x);
}

class MazeRandom
{
public:
    explicit MazeRandom(uint64_t seed) : m_state(seed)
    {
    }

    uint32_t bounded(uint32_t bound)
    {
        // Unsigned wraparound and these fixed shifts define SplitMix64 on every supported platform.
        const uint64_t threshold = (uint64_t{0} - bound) % bound;
        uint64_t value;
        do
        {
            m_state += 0x9e3779b97f4a7c15ULL;
            ++m_draws;
            value = m_state;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            value ^= value >> 31;
        } while (value < threshold);
        return uint32_t(value % bound);
    }

    uint64_t state() const
    {
        return m_state;
    }

    uint64_t draws() const
    {
        return m_draws;
    }

private:
    uint64_t m_state;
    uint64_t m_draws = 0;
};

void validateTemplateWalls(const BreachMazeTemplate &mazeTemplate)
{
    require(mazeTemplate.version == BreachMazeTemplateVersion, "unsupported template metadata version");
    for (size_t index = 0; index < mazeTemplate.cells.size(); ++index)
    {
        const BreachMazeTemplateCell &cell = mazeTemplate.cells[index];
        require(cell.cellId == index + 1 && cell.x == index % 16 + 1 && cell.y == index / 16 + 1,
            "template cell identity mismatch");
        std::array<bool, 4> foundSides = {};
        uint32_t previousFace = 0;
        for (size_t wall = 0; wall < cell.wallFaceIndices.size(); ++wall)
        {
            const uint32_t faceIndex = cell.wallFaceIndices[wall];
            require(faceIndex < mazeTemplate.faces.size(), "template wall index out of range");
            require(wall == 0 || faceIndex > previousFace, "template walls lost source facet order");
            previousFace = faceIndex;
            const BreachMazeTemplateFace &face = mazeTemplate.faces[faceIndex];
            require(face.faceIndex == faceIndex && face.cellId == cell.cellId
                && face.role == BreachMazeFaceRole::Wall && face.wallSide, "invalid template wall ownership");
            const size_t direction = sideIndex(*face.wallSide);
            require(!foundSides[direction], "duplicate template wall direction");
            foundSides[direction] = true;
        }
    }
}

bool outsideWall(uint8_t x, uint8_t y, uint8_t side, BreachMazeSide direction)
{
    switch (direction)
    {
        case BreachMazeSide::West: return x == 1;
        case BreachMazeSide::East: return x == side;
        case BreachMazeSide::North: return y == 1;
        case BreachMazeSide::South: return y == side;
    }
    return false;
}

uint16_t findBoundaryDeadEnd(const BreachMazeLayout &layout, BreachMazeSide edge, uint8_t firstCoordinate)
{
    for (uint8_t coordinate = firstCoordinate; coordinate <= layout.side; ++coordinate)
    {
        const uint8_t x = edge == BreachMazeSide::West ? 1
            : edge == BreachMazeSide::East ? layout.side : coordinate;
        const uint8_t y = edge == BreachMazeSide::North ? 1
            : edge == BreachMazeSide::South ? layout.side : coordinate;
        const BreachMazeCell &cell = layout.cellAt(x, y);
        if (std::count(cell.walls.begin(), cell.walls.end(), false) == 1)
        {
            return cell.templateCellId;
        }
    }
    return 0;
}

BreachMazeEndpoint endpointForCell(const BreachMazeTemplate &mazeTemplate,
    const BreachMazeLayout &layout, uint16_t cellId)
{
    require(cellId > 0 && cellId <= mazeTemplate.cells.size(), "endpoint cell out of range");
    const BreachMazeTemplateCell &cell = mazeTemplate.cells[cellId - 1];
    const BreachMazeCell &mazeCell = layout.cellAt(cell.x, cell.y);
    for (uint32_t faceIndex : cell.wallFaceIndices)
    {
        const BreachMazeSide direction = *mazeTemplate.faces[faceIndex].wallSide;
        if (mazeCell.walls[sideIndex(direction)] && outsideWall(cell.x, cell.y, layout.side, direction))
        {
            return {cellId, faceIndex, direction};
        }
    }
    throw std::invalid_argument("Breach maze: endpoint has no closed outside wall");
}

void generateTopology(BreachMazeLayout &layout, MazeRandom &random)
{
    const size_t side = layout.side;
    std::vector<uint16_t> labels(side);
    for (size_t x = 0; x < side; ++x)
    {
        labels[x] = uint16_t(x + 1);
    }
    std::vector<bool> eastWalls(side * side, true);
    std::vector<bool> southWalls(side * side, true);
    layout.cells.resize(side * side);

    for (size_t y = 0; y < side; ++y)
    {
        for (size_t x = 0; x < side; ++x)
        {
            // Preserve the source label at this point of row processing, before later set merges.
            BreachMazeCell &cell = layout.cells[y * side + x];
            cell.templateCellId = templateCellId(uint8_t(x + 1), uint8_t(y + 1));
            cell.room = labels[x];
            if (x + 1 < side && labels[x] != labels[x + 1]
                && (random.bounded(2) == 0 || y + 1 == side))
            {
                eastWalls[y * side + x] = false;
                const uint16_t oldLabel = labels[x + 1];
                const uint16_t mergedLabel = labels[x];
                std::replace(labels.begin(), labels.end(), oldLabel, mergedLabel);
            }
        }
        if (y + 1 == side)
        {
            break;
        }

        for (uint16_t label = 1; label <= side; ++label)
        {
            bool opened = false;
            std::optional<size_t> lastColumn;
            for (size_t x = 0; x < side; ++x)
            {
                if (labels[x] == label)
                {
                    lastColumn = x;
                    if (random.bounded(2) == 0)
                    {
                        southWalls[y * side + x] = false;
                        opened = true;
                    }
                }
            }
            if (!opened && lastColumn)
            {
                southWalls[y * side + *lastColumn] = false;
            }
        }

        for (size_t x = 0; x < side; ++x)
        {
            if (southWalls[y * side + x])
            {
                labels[x] = 0;
            }
        }
        std::vector<uint16_t> unusedLabels;
        for (uint16_t label = 1; label <= side; ++label)
        {
            if (std::find(labels.begin(), labels.end(), label) == labels.end())
            {
                unusedLabels.push_back(label);
            }
        }
        for (uint16_t &label : labels)
        {
            if (label == 0)
            {
                label = unusedLabels.back();
                unusedLabels.pop_back();
            }
        }
    }

    for (size_t y = 0; y < side; ++y)
    {
        for (size_t x = 0; x < side; ++x)
        {
            layout.cells[y * side + x].walls = {
                x == 0 || eastWalls[y * side + x - 1], eastWalls[y * side + x],
                y == 0 || southWalls[(y - 1) * side + x], southWalls[y * side + x]};
        }
    }
}
}

const BreachMazeCell &BreachMazeLayout::cellAt(uint8_t x, uint8_t y) const
{
    require(x > 0 && y > 0 && x <= side && y <= side, "active cell coordinate out of range");
    require(cells.size() == size_t(side) * side, "active grid size mismatch");
    return cells[(y - 1) * side + x - 1];
}

BreachMazeEndpoints selectBreachMazeEndpoints(const BreachMazeTemplate &mazeTemplate,
    const BreachMazeLayout &layout, BreachMazeSide entranceSide, BreachMazeSide exitSide)
{
    sideIndex(entranceSide);
    sideIndex(exitSide);
    require(entranceSide != exitSide, "entrance and exit searches require different sides");
    require(layout.side >= 4 && layout.side <= BreachMazeTemplateSide, "active side outside 4..16");
    validateTemplateWalls(mazeTemplate);
    for (uint8_t y = 1; y <= layout.side; ++y)
    {
        for (uint8_t x = 1; x <= layout.side; ++x)
        {
            require(layout.cellAt(x, y).templateCellId == templateCellId(x, y), "active cell identity mismatch");
        }
    }

    BreachMazeEndpoints endpoints;
    endpoints.entranceSearchSide = entranceSide;
    endpoints.exitSearchSide = exitSide;
    uint16_t entrance = findBoundaryDeadEnd(layout, entranceSide, 1);
    uint16_t exit = findBoundaryDeadEnd(layout, exitSide, 2);
    endpoints.entranceUsedFallback = entrance == 0;
    endpoints.exitUsedFallback = exit == 0;
    if (entrance == 0)
    {
        entrance = 1;
    }
    if (exit == 0)
    {
        exit = layout.side;
    }
    if (entrance == exit)
    {
        endpoints.duplicateCellFallback = true;
        entrance = 1;
        exit = layout.side;
    }
    endpoints.entrance = endpointForCell(mazeTemplate, layout, entrance);
    endpoints.exit = endpointForCell(mazeTemplate, layout, exit);
    return endpoints;
}

BreachMazeLayout generateBreachMazeLayout(const BreachMazeTemplate &mazeTemplate, uint32_t level, uint64_t seed)
{
    require(level > 0, "floor level must be positive");
    MazeRandom random(seed);
    BreachMazeLayout layout;
    layout.level = level;
    layout.seed = seed;
    const uint32_t maximumSide = std::clamp(level, 8u, 16u);
    layout.side = uint8_t(4 + random.bounded(maximumSide - 3));
    generateTopology(layout, random);
    const BreachMazeSide entranceSide = static_cast<BreachMazeSide>(random.bounded(4));
    BreachMazeSide exitSide;
    do
    {
        exitSide = static_cast<BreachMazeSide>(random.bounded(4));
    } while (exitSide == entranceSide);
    layout.endpoints = selectBreachMazeEndpoints(mazeTemplate, layout, entranceSide, exitSide);
    layout.finalRandomState = random.state();
    layout.randomDrawCount = random.draws();
    return layout;
}
}
