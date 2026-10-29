#include "doctest/doctest.h"
#include "game/gameplay/KeyboardTurnRuntime.h"

#include <limits>

using namespace OpenYAMM::Game;

TEST_CASE("keyboard angle modes retain tap precision and refresh independent held turning")
{
    constexpr float Pi = 3.14159265358979323846f;
    const GameplayButtonInputState tap{false, true, false};
    const GameplayButtonInputState held{true, false, false};
    const GameplayButtonInputState none{};
    KeyboardTurnState state;
    CHECK(keyboardTurnDelta(state, TurnRateMode::X16, tap, none, 0.2f, true) == doctest::Approx(Pi / 8));
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, none, tap, 0.2f, true) == doctest::Approx(-Pi / 16));
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, none, none, 0.2f, true) == 0.0f);

    const auto heldTurn = [&](TurnRateMode mode, int framesPerSecond)
    {
        KeyboardTurnState run;
        double total = 0;
        for (int frame = 0; frame < framesPerSecond * 3; ++frame)
        {
            GameplayButtonInputState left = held;
            left.pressed = frame == 0;
            total += keyboardTurnDelta(run, mode, left, none, 1.0f / framesPerSecond, true);
        }
        return total;
    };
    for (const auto mode : {TurnRateMode::X16, TurnRateMode::X32, TurnRateMode::Smooth})
    {
        CHECK(heldTurn(mode, 10) == doctest::Approx(heldTurn(mode, 120)));
        CHECK(heldTurn(mode, 30) == doctest::Approx(heldTurn(mode, 120)));
    }
    CHECK(heldTurn(TurnRateMode::Smooth, 120) == doctest::Approx(1.75 * 3));
    CHECK(heldTurn(TurnRateMode::X16, 120) == doctest::Approx(14 * Pi / 8));
    CHECK(heldTurn(TurnRateMode::X32, 120) == doctest::Approx(27 * Pi / 16));

    CHECK(keyboardTurnDelta(state, TurnRateMode::X16, held, none, 0.05f, true) == doctest::Approx(Pi / 8));
    CHECK(keyboardTurnDelta(state, TurnRateMode::X16, held, none, 0.05f, true) == 0.0f);
    CHECK(keyboardTurnDelta(state, TurnRateMode::X16, held, none, 1.0f, false) == 0.0f);
    CHECK(state.direction == 0);
    CHECK(keyboardTurnDelta(state, TurnRateMode::X16, held, none, 0.01f, true) == doctest::Approx(Pi / 8));
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, held, none, 0.01f, true) == doctest::Approx(Pi / 16));
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, none, held, 0.01f, true) == doctest::Approx(-Pi / 16));
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, held, held, 0.01f, true) == 0.0f);
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, held, none, 0.0f, true) == 0.0f);
    CHECK(keyboardTurnDelta(state, TurnRateMode::X32, held, none,
        std::numeric_limits<float>::quiet_NaN(), true) == 0.0f);
}
