#pragma once

#include "game/app/KeyboardBindings.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>

namespace OpenYAMM::Game
{
struct GameplayButtonInputState
{
    bool held = false;
    bool pressed = false;
    bool released = false;
};

struct GameplayInputFrame
{
    int screenWidth = 0;
    int screenHeight = 0;
    float mouseWheelDelta = 0.0f;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool pointerFromTouch = false;
    float relativeMouseX = 0.0f;
    float relativeMouseY = 0.0f;
    std::string textInput;
    bool mobileJoystickActive = false;
    bool turnBasedMovementStep = false;
    bool turnBasedPhysicsStep = false;
    float movementSpeedScale = 1.0f;
    float mobileJoystickBaseX = 0.0f;
    float mobileJoystickBaseY = 0.0f;
    float mobileJoystickKnobX = 0.0f;
    float mobileJoystickKnobY = 0.0f;
    bool mobileTouchDragStarted = false;
    bool mobileTouchDragActive = false;
    bool mobileTouchDragReleased = false;
    float mobileTouchDragStartX = 0.0f;
    float mobileTouchDragStartY = 0.0f;
    GameplayButtonInputState leftMouseButton = {};
    GameplayButtonInputState rightMouseButton = {};
    GameplayButtonInputState middleMouseButton = {};
    std::array<bool, SDL_SCANCODE_COUNT> keyboardHeld = {};
    std::array<uint16_t, SDL_SCANCODE_COUNT> keyboardPressCounts = {};
    std::array<GameplayButtonInputState, KeyboardActionCount> actions = {};
    // Nonzero IDs identify one input update, including copies dispatched to different UI layers.
    // Zero preserves held-key semantics for callers that construct input without a frame clock.
    uint64_t frameId = 0;

    const bool *keyboardState() const
    {
        return keyboardHeld.data();
    }

    bool isScancodeHeld(SDL_Scancode scancode) const
    {
        return scancode > SDL_SCANCODE_UNKNOWN
            && scancode < SDL_SCANCODE_COUNT
            && keyboardHeld[scancode];
    }

    uint16_t scancodePressCount(SDL_Scancode scancode) const
    {
        return scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT
            ? keyboardPressCounts[scancode]
            : 0;
    }

    const GameplayButtonInputState &action(KeyboardAction keyboardAction) const
    {
        return actions[keyboardActionIndex(keyboardAction)];
    }

    bool isActionActive(KeyboardAction keyboardAction) const
    {
        // A tap completed between frames still contributes one frame of input.
        const GameplayButtonInputState &state = action(keyboardAction);
        return state.held || state.pressed;
    }
};
} // namespace OpenYAMM::Game
