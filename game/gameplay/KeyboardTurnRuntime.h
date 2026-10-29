#pragma once

#include "game/app/GameSettings.h"
#include "game/gameplay/GameplayInputFrame.h"

#include <cmath>

namespace OpenYAMM::Game
{
struct KeyboardTurnState
{
    TurnRateMode mode = TurnRateMode::Smooth;
    int direction = 0;
    double heldSeconds = 0.0;
};

inline float keyboardTurnDelta(
    KeyboardTurnState &state,
    TurnRateMode mode,
    const GameplayButtonInputState &left,
    const GameplayButtonInputState &right,
    float deltaSeconds,
    bool enabled)
{
    constexpr double Pi = 3.14159265358979323846;
    constexpr double RadiansPerSecond = 1.75;
    const int direction = int(left.held || left.pressed) - int(right.held || right.pressed);
    if (!enabled || direction == 0 || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
    {
        state = {};
        return 0.0f;
    }

    if (mode == TurnRateMode::Smooth)
    {
        state = {};
        return static_cast<float>(std::remainder(direction * RadiansPerSecond * deltaSeconds, 2.0 * Pi));
    }

    const auto &key = direction > 0 ? left : right;
    const double step = Pi / (mode == TurnRateMode::X16 ? 8.0 : 16.0);
    const bool newPress = key.pressed || state.direction != direction || state.mode != mode;
    if (newPress)
    {
        state = {mode, direction, 0.0};
    }
    double angle = newPress ? step : 0.0;

    // Original discrete modes advanced once per input update. Retain their angle
    // steps, but repeat held keys at the existing smooth angular speed, independent
    // of display refresh rate. A tap completed between updates gets exactly one step.
    if (key.held)
    {
        const double repeatSeconds = step / RadiansPerSecond;
        state.heldSeconds += deltaSeconds;
        const double repeats = std::floor((state.heldSeconds + 1.0e-8) / repeatSeconds);
        state.heldSeconds -= repeats * repeatSeconds;
        angle += repeats * step;
    }
    else
    {
        state = {};
    }

    return static_cast<float>(std::remainder(direction * angle, 2.0 * Pi));
}
}
