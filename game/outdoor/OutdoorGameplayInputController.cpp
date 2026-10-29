#include "game/outdoor/OutdoorGameplayInputController.h"

#include "game/app/GameSettings.h"
#include "game/app/GameSession.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/KeyboardTurnRuntime.h"
#include "game/outdoor/OutdoorGameView.h"
#include "game/outdoor/OutdoorWorldRuntime.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace OpenYAMM::Game
{
namespace
{
constexpr float Pi = 3.14159265358979323846f;
} // namespace

void OutdoorGameplayInputController::updateCameraFromInput(
    OutdoorGameView &view,
    const GameplayInputFrame &input,
    float deltaSeconds,
    GameplayWorldMovementFrameDiagnostics *pPerformanceDiagnostics)
{
    const float keyboardTurnDeltaSeconds = deltaSeconds;
    const float displayDeltaSeconds = std::max(deltaSeconds, 0.000001f);
    const float instantaneousFramesPerSecond = 1.0f / displayDeltaSeconds;
    view.m_framesPerSecond = (view.m_framesPerSecond == 0.0f)
        ? instantaneousFramesPerSecond
        : (view.m_framesPerSecond * 0.9f + instantaneousFramesPerSecond * 0.1f);
    if (!input.turnBasedMovementStep)
    {
        deltaSeconds = std::min(deltaSeconds, 0.05f);
    }

    const bool *pKeyboardState = input.keyboardState();

    GameplayScreenState &gameplayScreenState = view.m_gameSession.gameplayScreenState();
    GameplayScreenState::PendingSpellTargetState &pendingSpellCast = gameplayScreenState.pendingSpellTarget();
    GameplayScreenState::GameplayMouseLookState &gameplayMouseLookState =
        gameplayScreenState.gameplayMouseLookState();
    GameplayScreenRuntime &overlayContext = view.m_gameSession.gameplayScreenRuntime();
    const bool hasPendingSpellCast = pendingSpellCast.active;
    const bool hasActiveLootView =
        view.m_pOutdoorWorldRuntime != nullptr
        && (view.m_pOutdoorWorldRuntime->activeChestView() != nullptr
            || view.m_pOutdoorWorldRuntime->activeCorpseView() != nullptr);
    SDL_Window *pWindow = SDL_GetMouseFocus();

    if (pWindow == nullptr)
    {
        pWindow = SDL_GetKeyboardFocus();
    }

    const GameplaySharedInputFrameResult &sharedInputFrameResult = view.m_gameSession.sharedInputFrameResult();
    view.syncGameplayMouseLookMode(pWindow, sharedInputFrameResult.mouseLookPolicy.mouseLookActive);

    if (sharedInputFrameResult.journalInputConsumed)
    {
        view.m_keyboardTurnState = {};
        return;
    }

    if (sharedInputFrameResult.worldInputBlocked)
    {
        view.m_keyboardTurnState = {};
        return;
    }

    const bool runWalkModifier = pKeyboardState[SDL_SCANCODE_LSHIFT] || pKeyboardState[SDL_SCANCODE_RSHIFT];
    const bool turboSpeed = pKeyboardState[SDL_SCANCODE_LCTRL] || pKeyboardState[SDL_SCANCODE_RCTRL];
    const bool blockCameraRotation =
        overlayContext.buffInspectOverlayReadOnly().active
        || overlayContext.characterDetailOverlayReadOnly().active;
    const bool wasRotatingCamera = view.m_isRotatingCamera;
    const bool classicControls = overlayContext.settingsSnapshot().controlScheme == ControlScheme::Classic;

    if (gameplayMouseLookState.mouseLookActive && !hasPendingSpellCast && !blockCameraRotation)
    {
        const float deltaMouseX = input.relativeMouseX;
        const float deltaMouseY = input.relativeMouseY;

        if (wasRotatingCamera && (deltaMouseX != 0.0f || deltaMouseY != 0.0f))
        {
            view.m_cameraYawRadians -= deltaMouseX * view.m_mouseRotateSpeed;
            view.m_cameraPitchRadians -= deltaMouseY * view.m_mouseRotateSpeed;
        }

        view.m_isRotatingCamera = true;
    }
    else
    {
        view.m_isRotatingCamera = false;
        view.m_lastMouseX = input.pointerX;
        view.m_lastMouseY = input.pointerY;
    }

    const bool moveForwardPressed = input.isActionActive(KeyboardAction::Forward);
    const bool moveBackwardPressed = input.isActionActive(KeyboardAction::Backward);
    const bool leftPressed = input.isActionActive(KeyboardAction::Left);
    const bool rightPressed = input.isActionActive(KeyboardAction::Right);
    const bool strafeLeftPressed = !classicControls && leftPressed;
    const bool strafeRightPressed = !classicControls && rightPressed;
    const bool jumpPressed = input.isActionActive(KeyboardAction::Jump);
    const bool flyUpPressed = input.isActionActive(KeyboardAction::FlyUp);
    const bool flyDownPressed = input.isActionActive(KeyboardAction::FlyDown);
    const bool lookUpPressed = input.isActionActive(KeyboardAction::LookUp);
    const bool lookDownPressed = input.isActionActive(KeyboardAction::LookDown);
    const bool centerViewPressed = input.isActionActive(KeyboardAction::CenterView);

    const bool allowWorldSimulation = !hasActiveLootView && !hasPendingSpellCast && !input.rightMouseButton.held;
    const bool allowCameraMovementInput = allowWorldSimulation && !gameplayMouseLookState.cursorModeActive;
    view.m_cameraYawRadians += keyboardTurnDelta(
        view.m_keyboardTurnState,
        overlayContext.settingsSnapshot().turnRate,
        input.action(KeyboardAction::Left),
        input.action(KeyboardAction::Right),
        keyboardTurnDeltaSeconds,
        classicControls && allowCameraMovementInput);

    const float cosYaw = std::cos(view.m_cameraYawRadians);
    const float sinYaw = std::sin(view.m_cameraYawRadians);
    const bx::Vec3 forward = {
        cosYaw,
        sinYaw,
        0.0f
    };
    const bx::Vec3 right = {
        sinYaw,
        -cosYaw,
        0.0f
    };

    if (view.m_pOutdoorMapData)
    {
        if (view.m_pOutdoorPartyRuntime)
        {
            if (allowWorldSimulation)
            {
                const OutdoorMovementInput movementInput = {
                    allowCameraMovementInput && moveForwardPressed,
                    allowCameraMovementInput && moveBackwardPressed,
                    allowCameraMovementInput && strafeLeftPressed,
                    allowCameraMovementInput && strafeRightPressed,
                    allowCameraMovementInput && jumpPressed,
                    allowCameraMovementInput && flyUpPressed,
                    allowCameraMovementInput && flyDownPressed,
                    runWalkModifier,
                    turboSpeed,
                    view.m_cameraYawRadians,
                    view.m_cameraPitchRadians,
                    !classicControls,
                    input.turnBasedMovementStep,
                    input.turnBasedPhysicsStep,
                    input.movementSpeedScale
                };
                if (view.m_pOutdoorSceneRuntime != nullptr)
                {
                    const OutdoorSceneRuntime::AdvanceFrameResult frameAdvanceResult =
                        view.m_pOutdoorSceneRuntime->advanceFrame(
                            movementInput,
                            deltaSeconds,
                            pPerformanceDiagnostics);

                    EventRuntimeState *pEventRuntimeState =
                        view.m_pOutdoorWorldRuntime != nullptr
                            ? view.m_pOutdoorWorldRuntime->eventRuntimeState()
                            : nullptr;

                    if (pEventRuntimeState != nullptr)
                    {
                        for (const std::string &statusMessage : pEventRuntimeState->statusMessages)
                        {
                            view.setStatusBarEvent(
                                statusMessage,
                                2.0f,
                                GameplayUiController::StatusBarEventPriority::High);
                        }

                        pEventRuntimeState->statusMessages.clear();
                    }

                    if (view.m_pOutdoorWorldRuntime != nullptr)
                    {
                        view.m_pOutdoorWorldRuntime->applyGrantedEventItemsToHeldInventory();
                    }

                    if (!view.m_pOutdoorPartyRuntime->movementStatusText().empty())
                    {
                        const std::string &localizedStatusText = view.m_gameSession.gameplayUiRuntime()
                            .uiTextTable().movementStatusText(view.m_pOutdoorPartyRuntime->movementStatus());
                        view.setStatusBarEvent(localizedStatusText.empty()
                            ? view.m_pOutdoorPartyRuntime->movementStatusText() : localizedStatusText);
                    }

                    if (frameAdvanceResult.shouldOpenEventDialog)
                    {
                        if (view.m_pOutdoorWorldRuntime != nullptr)
                        {
                            view.m_pOutdoorWorldRuntime->presentPendingEventDialog(
                                frameAdvanceResult.previousMessageCount,
                                true);
                        }
                    }
                }
            }

            view.m_cameraTargetX = view.m_pOutdoorPartyRuntime->movementState().x;
            view.m_cameraTargetY = view.m_pOutdoorPartyRuntime->movementState().y;
            view.m_cameraTargetZ = view.m_pOutdoorPartyRuntime->movementState().footZ + view.m_cameraEyeHeight;
        }
    }
    else
    {
        float moveVelocityX = 0.0f;
        float moveVelocityY = 0.0f;
        const float freeMoveSpeed = turboSpeed ? 4000.0f : 576.0f;

        if (!gameplayMouseLookState.cursorModeActive && strafeLeftPressed)
        {
            moveVelocityX -= right.x * freeMoveSpeed;
            moveVelocityY -= right.y * freeMoveSpeed;
        }

        if (!gameplayMouseLookState.cursorModeActive && strafeRightPressed)
        {
            moveVelocityX += right.x * freeMoveSpeed;
            moveVelocityY += right.y * freeMoveSpeed;
        }

        if (!gameplayMouseLookState.cursorModeActive && moveForwardPressed)
        {
            moveVelocityX += forward.x * freeMoveSpeed;
            moveVelocityY += forward.y * freeMoveSpeed;
        }

        if (!gameplayMouseLookState.cursorModeActive && moveBackwardPressed)
        {
            moveVelocityX -= forward.x * freeMoveSpeed;
            moveVelocityY -= forward.y * freeMoveSpeed;
        }

        view.m_cameraTargetX += moveVelocityX * deltaSeconds;
        view.m_cameraTargetY += moveVelocityY * deltaSeconds;
    }

    const float keyboardPitchSpeed = 1.25f;

    if (!gameplayMouseLookState.cursorModeActive && lookUpPressed)
    {
        view.m_cameraPitchRadians -= keyboardPitchSpeed * deltaSeconds;
    }

    if (!gameplayMouseLookState.cursorModeActive && lookDownPressed)
    {
        view.m_cameraPitchRadians += keyboardPitchSpeed * deltaSeconds;
    }

    if (!gameplayMouseLookState.cursorModeActive && centerViewPressed)
    {
        view.m_cameraPitchRadians *= std::max(0.0f, 1.0f - deltaSeconds * 8.0f);
    }

    if (view.m_pOutdoorPartyRuntime)
    {
        if (input.action(KeyboardAction::Land).pressed && view.m_pOutdoorPartyRuntime->partyMovementState().flying)
        {
            view.m_pOutdoorPartyRuntime->toggleFlying();
        }
    }

    if (view.m_cameraYawRadians > Pi)
    {
        view.m_cameraYawRadians -= Pi * 2.0f;
    }
    else if (view.m_cameraYawRadians < -Pi)
    {
        view.m_cameraYawRadians += Pi * 2.0f;
    }

    view.m_cameraPitchRadians = std::clamp(view.m_cameraPitchRadians, -1.55f, 1.55f);
    view.m_cameraTargetZ = std::clamp(view.m_cameraTargetZ, -2000.0f, 30000.0f);
    view.m_cameraOrthoScale = std::clamp(view.m_cameraOrthoScale, 0.05f, 3.5f);
}
} // namespace OpenYAMM::Game
