#pragma once

#include "game/app/GameSettings.h"
#include "game/app/MobileJumpDoubleTapGesture.h"
#include "game/gameplay/GameplayInputFrame.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class GameInputSystem
{
public:
    void handleSdlEvent(const SDL_Event &event);

    void updateFromEngineInput(
        int screenWidth,
        int screenHeight,
        float mouseWheelDelta,
        const GameSettings &settings,
        bool blockGameplayInput = false,
        bool mobileGameplayTouchControlsEnabled = true,
        bool mobileJumpGestureEnabled = true,
        bool mobileFlightControlsEnabled = false,
        bool mobileInspectControlEnabled = true,
        std::optional<SDL_FRect> mobileInspectButtonRect = std::nullopt);

    const GameplayInputFrame &frame() const;
    bool consumeMobileDebugConsoleToggleRequested();
    void resetRelativeMouseMotion();
    void suppressMouseButtonsUntilReleased();
    void clearTextComposition();

private:
    enum class MobileTouchRole
    {
        None,
        Movement,
        Camera,
        FlyUp,
        FlyDown,
        InspectModifier,
        InspectTarget,
        Hud,
        DebugConsoleGesture
    };

    struct MobileTouchPath
    {
        std::vector<SDL_FPoint> points;
        bool incomplete = false;
    };

    struct MobileTouchPoint
    {
        bool active = false;
        SDL_FingerID fingerId = 0;
        uint64_t gestureId = 0;
        MobileTouchRole role = MobileTouchRole::None;
        float startX = 0.0f;
        float startY = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        bool dragThresholdExceeded = false;
        bool dragStartDelivered = false;
        bool hudPressDelivered = false;
        bool debugConsoleGestureCandidate = false;
        bool debugConsoleGestureTriggered = false;
        MobileTouchPath hudPath;
    };

    struct MobileHudTap
    {
        float startX = 0.0f;
        float startY = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        uint64_t gestureId = 0;
        MobileTouchPath path;
    };

    struct MobileInspectTarget
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    static void appendMobileTouchPath(MobileTouchPath &path, float x, float y);
    void deliverMobileTouchPath(MobileTouchPath &path, int screenWidth, int screenHeight);

    GameplayInputFrame m_frame = {};
    uint64_t m_inputFrameId = 0;
    uint64_t m_keyboardFocusGeneration = 0;
    bool m_keyboardFocusActive = true;
    std::string m_pendingTextInput;
    std::vector<GameplayTextEditOperation> m_pendingTextEdits;
    std::array<uint16_t, SDL_SCANCODE_COUNT> m_pendingKeyboardPressCounts = {};
    std::deque<SDL_MouseButtonEvent> m_pendingMouseButtons;
    SDL_MouseButtonFlags m_deliveredMouseButtons = 0;
    bool m_mouseInputCanceled = false;
    std::array<bool, SDL_SCANCODE_COUNT> m_previousKeyboardHeld = {};
    std::array<bool, KeyboardActionCount> m_previousActionHeld = {};
    std::array<MobileTouchPoint, 8> m_mobileTouches = {};
    uint64_t m_mobileTouchGestureSequence = 0;
    uint64_t m_mobileDeliveredHudGestureId = 0;
    std::optional<MobileInspectTarget> m_mobileInspectTarget;
    std::deque<MobileHudTap> m_mobilePendingHudTaps;
    bool m_mobilePendingHudRelease = false;
    float m_mobilePendingHudReleaseX = 0.0f;
    float m_mobilePendingHudReleaseY = 0.0f;
    uint64_t m_mobilePendingHudReleaseGestureId = 0;
    MobileTouchPath m_mobilePendingHudReleasePath;
    bool m_mobilePendingHudDragRelease = false;
    bool m_mobilePendingHudDragStartDelivered = false;
    float m_mobilePendingHudDragStartX = 0.0f;
    float m_mobilePendingHudDragStartY = 0.0f;
    float m_mobilePendingHudDragReleaseX = 0.0f;
    float m_mobilePendingHudDragReleaseY = 0.0f;
    uint64_t m_mobilePendingHudDragReleaseGestureId = 0;
    MobileTouchPath m_mobilePendingHudDragReleasePath;
    bool m_mobilePendingCameraTap = false;
    uint64_t m_mobilePendingCameraTapTimestampNanoseconds = 0;
    float m_mobilePendingCameraTapX = 0.0f;
    float m_mobilePendingCameraTapY = 0.0f;
    MobileJumpDoubleTapGesture m_mobileJumpDoubleTapGesture;
    bool m_mobileDebugConsoleToggleRequested = false;
    bool m_previousLeftMouseButtonHeld = false;
    bool m_previousRightMouseButtonHeld = false;
    bool m_previousMiddleMouseButtonHeld = false;
    bool m_suppressLeftMouseButtonUntilReleased = false;
    bool m_suppressRightMouseButtonUntilReleased = false;
    bool m_suppressMiddleMouseButtonUntilReleased = false;
};
} // namespace OpenYAMM::Game
