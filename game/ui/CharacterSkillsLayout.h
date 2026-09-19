#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include "game/ui/GameplayHudCommon.h"

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;

struct CharacterSkillLayout
{
    float rowHeight = 18;
    float scrollOffset = 0;
    float maximumScroll = 0;
    GameplayResolvedHudLayoutElement viewport = {};

    // Translate a content rectangle, then accept only fully visible rows/headers.
    bool placeVisible(GameplayResolvedHudLayoutElement &rect) const;
    bool contains(float x, float y) const;
};

bool isCharacterSkillContentHeader(std::string_view normalizedId);

// Applies readable spacing to rendering, upgrade clicks and inspect popups.
// Counts are weapons, magic, armor and miscellaneous, in that order.
CharacterSkillLayout layoutCharacterSkillRows(
    GameplayScreenRuntime &context,
    int width,
    int height,
    float naturalRowHeight,
    const std::array<size_t, 4> &rowCounts);
}
