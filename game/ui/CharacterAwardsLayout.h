#pragma once

#include "game/ui/GameplayOverlayTypes.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;

struct CharacterAwardLayoutRow
{
    std::vector<std::string> lines;
    float height = 0.0f;
    int priority = 0;
};

struct CharacterAwardsLayout
{
    GameplayHudFontHandle font = {};
    GameplayResolvedHudLayoutElement listRect = {};
    std::optional<GameplayResolvedHudLayoutElement> trackRect;
    std::optional<GameplayResolvedHudLayoutElement> thumbRect;
    std::vector<CharacterAwardLayoutRow> rows;
    float fontScale = 1.0f;
    float awardGap = 0.0f;
    float textX = 0.0f;
    float textY = 0.0f;
    float thumbTrackTop = 0.0f;
    float thumbTravel = 0.0f;
    size_t maximumScrollOffset = 0;

    size_t scrollOffsetAt(float pointerY, float thumbGrabOffset) const;
};

// Uses the same wrapped entries and thumb geometry for rendering and pointer input.
std::optional<CharacterAwardsLayout> layoutCharacterAwards(
    GameplayScreenRuntime &context,
    const Character *pCharacter,
    const Party *pParty,
    int width,
    int height);
}
