#pragma once

#include "game/content/BreachMazeLayout.h"
#include "game/indoor/IndoorMapData.h"
#include "game/maps/MapDeltaData.h"

#include <array>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachMazeSceneVersion = 1;

struct BreachMazeSceneData
{
    IndoorMapData map;
    MapDeltaData delta;
    std::vector<std::string> requiredTextures;
};

// Builds a new immutable-at-use geometry instance. Both map and delta must be supplied to the
// same scene before renderer, collision, LOS and picking caches are initialized. Does not install it.
// Requires the original profile-verified BLV and a pristine version-1 generated layout. paletteByRoom
// is indexed by room - 1; every used room must explicitly choose original material set 1..4.
// Preserves all population, lights and non-face delta data. No travel, save or quest effects occur.
// Throws std::invalid_argument for mismatched geometry/metadata/layout, invalid palettes or revision overflow.
BreachMazeSceneData buildBreachMazeScene(
    const IndoorMapData &originalMap,
    const MapDeltaData &baseDelta,
    const BreachMazeTemplate &mazeTemplate,
    const BreachMazeLayout &layout,
    const std::array<uint8_t, 16> &paletteByRoom);
}
