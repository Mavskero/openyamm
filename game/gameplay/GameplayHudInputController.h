#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;
struct GameplayHudPointerTarget;

struct GameplayPartyPortraitInputConfig
{
    int screenWidth = 0;
    int screenHeight = 0;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    bool allowInput = false;
    bool requireGameplayReady = false;
    bool hasActiveLootView = false;
    std::function<bool(size_t memberIndex)> onPortraitActivated;
};

struct GameplayHudButtonInputConfig
{
    int screenWidth = 0;
    int screenHeight = 0;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool leftButtonPressed = false;
    bool allowInput = false;
};

class GameplayHudInputController
{
public:
    static void activatePartyPortrait(
        GameplayScreenRuntime &context,
        size_t memberIndex,
        const GameplayPartyPortraitInputConfig &config);

    static void handlePartyPortraitInput(
        GameplayScreenRuntime &context,
        const GameplayPartyPortraitInputConfig &config);

    // Read the same target geometry as the HUD handler without arming or consuming input.
    static GameplayHudPointerTarget resolveGameplayHudButtonTarget(
        const GameplayScreenRuntime &context,
        const GameplayHudButtonInputConfig &config);

    static void handleGameplayHudButtonInput(
        GameplayScreenRuntime &context,
        const GameplayHudButtonInputConfig &config);
};
} // namespace OpenYAMM::Game
