#pragma once

#include "game/content/BreachMazeLayout.h"

#include <limits>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachMazeSeedVersion = 1;
// A planning bound, not a promise that population/resources support this many levels.
inline constexpr uint32_t BreachMazeMaximumPlannedLevel = std::numeric_limits<uint32_t>::max() - 2;

struct BreachMazeFloorIdentity
{
    uint64_t epoch = 0;
    uint32_t level = 0;
    uint64_t generationSerial = 0;

    bool operator==(const BreachMazeFloorIdentity &) const = default;
};

struct BreachMazeFloorSeeds
{
    uint64_t layout = 0;
    uint64_t material = 0;
    uint64_t population = 0;
    uint64_t interaction = 0;

    bool operator==(const BreachMazeFloorSeeds &) const = default;
};

// Version 1 uses only specified uint64_t arithmetic; it never consumes the 143 layout stream.
BreachMazeFloorSeeds deriveBreachMazeFloorSeeds(uint64_t rootSeed, const BreachMazeFloorIdentity &identity);

struct BreachMazeFloorDescriptor
{
    BreachMazeFloorIdentity identity;
    BreachMazeFloorSeeds seeds;
    BreachMazeLayout layout;
};

}
