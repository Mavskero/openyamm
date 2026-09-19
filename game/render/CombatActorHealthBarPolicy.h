#pragma once

#include "game/gameplay/GameplayActorService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace OpenYAMM::Game
{
constexpr size_t MaximumCombatActorHealthBars = 8;
constexpr float MinimumCombatActorHealthBarScale = 0.65f;
constexpr float MaximumCombatActorHealthBarDistance = GameplayActorService::MaximumPartyEngagementRange;
constexpr float MaximumCombatActorHealthBarDistanceSquared =
    MaximumCombatActorHealthBarDistance * MaximumCombatActorHealthBarDistance;
constexpr float MaximumCombatActorHealthBarViewportHeight = 0.015f;

struct CombatActorHealthBarDimensions
{
    float scale = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float innerWidth = 0.0f;
    float innerHeight = 0.0f;
    float fillWidth = 0.0f;
};

inline std::optional<CombatActorHealthBarDimensions> combatActorHealthBarDimensions(
    float distanceScale, float viewDepth, float projectionYScale, float healthRatio)
{
    if (!std::isfinite(distanceScale) || distanceScale <= 0.0f
        || !std::isfinite(viewDepth) || viewDepth <= 0.1f
        || !std::isfinite(projectionYScale) || projectionYScale == 0.0f
        || !std::isfinite(healthRatio))
    {
        return std::nullopt;
    }

    // Perspective projects a world-space height to height * projectionYScale / (2 * depth)
    // of the viewport. Cap the complete bar, retaining its original size at greater distances.
    const float maximumScale = 2.0f * viewDepth * MaximumCombatActorHealthBarViewportHeight
        / (11.0f * std::abs(projectionYScale));
    CombatActorHealthBarDimensions result;
    result.scale = std::min(distanceScale, maximumScale);
    result.width = 92.0f * result.scale;
    result.height = 11.0f * result.scale;
    result.innerWidth = 86.0f * result.scale;
    result.innerHeight = 7.0f * result.scale;
    result.fillWidth = healthRatio <= 0.0f ? 0.0f
        : std::max(result.scale, result.innerWidth * std::min(healthRatio, 1.0f));
    return result;
}

struct CombatActorHealthBarCandidate
{
    size_t drawItemIndex = 0;
    float distanceSquared = 0.0f;
};

struct CombatActorHealthBarSelection
{
    std::array<CombatActorHealthBarCandidate, MaximumCombatActorHealthBars> candidates = {};
    size_t count = 0;
};

inline void considerCombatActorHealthBarCandidate(
    CombatActorHealthBarSelection &selection,
    size_t drawItemIndex,
    float distanceSquared)
{
    if (distanceSquared > MaximumCombatActorHealthBarDistanceSquared)
    {
        return;
    }

    if (selection.count < selection.candidates.size())
    {
        selection.candidates[selection.count++] = {drawItemIndex, distanceSquared};
        return;
    }

    size_t farthestCandidateIndex = 0;

    for (size_t index = 1; index < selection.candidates.size(); ++index)
    {
        if (selection.candidates[index].distanceSquared
            > selection.candidates[farthestCandidateIndex].distanceSquared)
        {
            farthestCandidateIndex = index;
        }
    }

    if (distanceSquared < selection.candidates[farthestCandidateIndex].distanceSquared)
    {
        selection.candidates[farthestCandidateIndex] = {drawItemIndex, distanceSquared};
    }
}

inline float combatActorHealthBarScale(float distanceSquared)
{
    const float distance = std::sqrt(std::clamp(distanceSquared, 0.0f, MaximumCombatActorHealthBarDistanceSquared));
    const float distanceRatio = distance / MaximumCombatActorHealthBarDistance;
    return 1.0f - distanceRatio * (1.0f - MinimumCombatActorHealthBarScale);
}
} // namespace OpenYAMM::Game
