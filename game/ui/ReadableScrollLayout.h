#pragma once

#include "game/ui/GameplayOverlayTypes.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;

struct ReadableScrollLayout
{
    GameplayResolvedHudLayoutElement rootRect = {};
    GameplayResolvedHudLayoutElement titleRect = {};
    // Text origin and available page area, already inset by the authored padding.
    GameplayResolvedHudLayoutElement bodyRect = {};
    std::vector<std::string> titleLines;
    std::vector<std::string> bodyLines;
    float titleLineHeight = 0.0f;
    float lineHeight = 0.0f;
    size_t linesPerPage = 1;
    size_t pageCount = 1;
    // Hidden controls have zero width and height. Recomputed for the current viewport.
    GameplayResolvedHudLayoutElement previousButton = {};
    GameplayResolvedHudLayoutElement nextButton = {};
    GameplayResolvedHudLayoutElement closeButton = {};
};

std::optional<ReadableScrollLayout> resolveReadableScrollLayout(
    GameplayScreenRuntime &context,
    int width,
    int height);
}
