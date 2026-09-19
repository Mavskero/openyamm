#include "game/ui/GameplayDialogueRenderer.h"

#include <doctest/doctest.h>

using OpenYAMM::Game::GameplayDialogueRenderer;
using OpenYAMM::Game::GameplayUiViewportRect;

TEST_CASE("dialogue blackout has no rectangles at exact 4:3")
{
    const auto rects = GameplayDialogueRenderer::blackoutBackdropRects(
        1024, 768, GameplayUiViewportRect{0.0f, 0.0f, 1024.0f, 768.0f});
    CHECK(rects.empty());
}

TEST_CASE("dialogue blackout covers only the two wide-screen side gutters")
{
    const auto rects = GameplayDialogueRenderer::blackoutBackdropRects(
        1210, 834, GameplayUiViewportRect{49.0f, 0.0f, 1112.0f, 834.0f});
    REQUIRE(rects.size() == 2);
    CHECK(rects[0].x == doctest::Approx(0.0f));
    CHECK(rects[0].y == doctest::Approx(0.0f));
    CHECK(rects[0].width == doctest::Approx(49.0f));
    CHECK(rects[0].height == doctest::Approx(834.0f));
    CHECK(rects[1].x == doctest::Approx(1161.0f));
    CHECK(rects[1].y == doctest::Approx(0.0f));
    CHECK(rects[1].width == doctest::Approx(49.0f));
    CHECK(rects[1].height == doctest::Approx(834.0f));
}

TEST_CASE("dialogue blackout covers all four non-overlapping perimeter gutters")
{
    const auto rects = GameplayDialogueRenderer::blackoutBackdropRects(
        1200, 800, GameplayUiViewportRect{100.0f, 50.0f, 1000.0f, 700.0f});
    REQUIRE(rects.size() == 4);
    CHECK(rects[0].x == doctest::Approx(0.0f));
    CHECK(rects[0].y == doctest::Approx(0.0f));
    CHECK(rects[0].width == doctest::Approx(1200.0f));
    CHECK(rects[0].height == doctest::Approx(50.0f));
    CHECK(rects[1].x == doctest::Approx(0.0f));
    CHECK(rects[1].y == doctest::Approx(750.0f));
    CHECK(rects[1].width == doctest::Approx(1200.0f));
    CHECK(rects[1].height == doctest::Approx(50.0f));
    CHECK(rects[2].x == doctest::Approx(0.0f));
    CHECK(rects[2].y == doctest::Approx(50.0f));
    CHECK(rects[2].width == doctest::Approx(100.0f));
    CHECK(rects[2].height == doctest::Approx(700.0f));
    CHECK(rects[3].x == doctest::Approx(1100.0f));
    CHECK(rects[3].y == doctest::Approx(50.0f));
    CHECK(rects[3].width == doctest::Approx(100.0f));
    CHECK(rects[3].height == doctest::Approx(700.0f));
}
