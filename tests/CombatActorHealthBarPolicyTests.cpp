#include "game/render/CombatActorHealthBarPolicy.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <limits>

namespace OpenYAMM::Game
{
TEST_CASE("combat actor health bars retain the eight nearest candidates within engagement range")
{
    CombatActorHealthBarSelection rangeSelection = {};
    considerCombatActorHealthBarCandidate(rangeSelection, 0, MaximumCombatActorHealthBarDistanceSquared);
    constexpr float BeyondMaximumDistance = MaximumCombatActorHealthBarDistance + 1.0f;
    considerCombatActorHealthBarCandidate(rangeSelection, 1, BeyondMaximumDistance * BeyondMaximumDistance);
    REQUIRE(rangeSelection.count == 1);
    CHECK(rangeSelection.candidates[0].drawItemIndex == 0);

    CombatActorHealthBarSelection selection = {};

    for (size_t index = 0; index < 10; ++index)
    {
        const float distance = static_cast<float>(10 - index) * 100.0f;
        considerCombatActorHealthBarCandidate(selection, index, distance * distance);
    }

    REQUIRE(selection.count == MaximumCombatActorHealthBars);
    std::array<size_t, MaximumCombatActorHealthBars> selectedIndices = {};

    for (size_t index = 0; index < selection.count; ++index)
    {
        selectedIndices[index] = selection.candidates[index].drawItemIndex;
    }

    std::sort(selectedIndices.begin(), selectedIndices.end());
    const std::array<size_t, MaximumCombatActorHealthBars> expectedIndices = {2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(selectedIndices == expectedIndices);
}

TEST_CASE("combat actor health bars shrink gradually across maximum engagement range")
{
    CHECK(combatActorHealthBarScale(0.0f) == doctest::Approx(1.0f));
    CHECK(
        combatActorHealthBarScale(
            MaximumCombatActorHealthBarDistanceSquared * 0.25f)
        == doctest::Approx(0.825f));
    CHECK(
        combatActorHealthBarScale(MaximumCombatActorHealthBarDistanceSquared)
        == doctest::Approx(MinimumCombatActorHealthBarScale));
}

TEST_CASE("projected health bars stay bounded across close distances and fields of view")
{
    for (const float depth : {0.11f, 1.0f, 25.0f, 100.0f, 500.0f, 2000.0f})
    {
        for (const float verticalFov : {30.0f, 60.0f, 100.0f})
        {
            const float projectionY = 1.0f / std::tan(verticalFov * 3.14159265f / 360.0f);
            const auto dimensions = combatActorHealthBarDimensions(1.0f, depth, projectionY, 0.5f);
            REQUIRE(dimensions.has_value());
            for (const float viewportHeight : {480.0f, 834.0f, 1660.0f, 2048.0f})
            {
                const float projectedHeight = dimensions->height * projectionY * viewportHeight / (2.0f * depth);
                CHECK(projectedHeight <= viewportHeight * 0.015f + 0.001f);
                CHECK(projectedHeight > 0.0f);
            }
            CHECK(dimensions->width / dimensions->height == doctest::Approx(92.0f / 11.0f));
        }
    }
}

TEST_CASE("small health bar frames contain their fill and retain accurate empty and full states")
{
    for (const float depth : {0.11f, 1.0f, 25.0f, 5000.0f})
    {
        for (const float health : {0.0f, 0.0001f, 0.25f, 0.5f, 1.0f})
        {
            const auto dimensions = combatActorHealthBarDimensions(0.9f, depth, 1.732f, health);
            REQUIRE(dimensions.has_value());
            CHECK(dimensions->innerWidth < dimensions->width);
            CHECK(dimensions->innerHeight < dimensions->height);
            CHECK(dimensions->fillWidth <= dimensions->innerWidth);
            if (health == 0.0f)
            {
                CHECK(dimensions->fillWidth == 0.0f);
            }
            else if (health == 1.0f)
            {
                CHECK(dimensions->fillWidth == dimensions->innerWidth);
            }
        }
    }
}

TEST_CASE("distant health bars keep their existing world dimensions")
{
    const auto dimensions = combatActorHealthBarDimensions(0.825f, 5000.0f, 1.732f, 0.5f);
    REQUIRE(dimensions.has_value());
    CHECK(dimensions->scale == doctest::Approx(0.825f));
    CHECK(dimensions->width == doctest::Approx(75.9f));
    CHECK(dimensions->height == doctest::Approx(9.075f));
    CHECK(dimensions->fillWidth == doctest::Approx(dimensions->innerWidth * 0.5f));
}

TEST_CASE("health bars behind the near plane or with invalid projection are not drawn")
{
    CHECK_FALSE(combatActorHealthBarDimensions(1.0f, -100.0f, 1.732f, 1.0f));
    CHECK_FALSE(combatActorHealthBarDimensions(1.0f, 0.0f, 1.732f, 1.0f));
    CHECK_FALSE(combatActorHealthBarDimensions(1.0f, 0.1f, 1.732f, 1.0f));
    CHECK_FALSE(combatActorHealthBarDimensions(1.0f, 100.0f, 0.0f, 1.0f));
    CHECK_FALSE(combatActorHealthBarDimensions(1.0f, std::numeric_limits<float>::infinity(), 1.732f, 1.0f));
    CHECK_FALSE(combatActorHealthBarDimensions(1.0f, 100.0f, 1.732f, std::numeric_limits<float>::quiet_NaN()));
}
} // namespace OpenYAMM::Game
