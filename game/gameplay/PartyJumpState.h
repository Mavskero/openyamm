#pragma once

#include <cmath>
#include <optional>

namespace OpenYAMM::Game
{
// Party-only requests and special-jump momentum. Vertical motion remains in the world's MoveState.
struct PartyJumpState
{
    bool pendingJumpRequested = false;
    std::optional<float> pendingJumpVelocity;
    float pendingJumpLift = 0.0f;
    bool specialJumpActive = false;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    bool pendingSpecialJump = false;
    float pendingVelocityX = 0.0f;
    float pendingVelocityY = 0.0f;
    float pendingVelocityZ = 0.0f;

    bool operator==(const PartyJumpState &) const = default;
};

inline bool validPartyJumpState(const PartyJumpState &state)
{
    return (!state.pendingJumpVelocity || std::isfinite(*state.pendingJumpVelocity))
        && std::isfinite(state.pendingJumpLift) && state.pendingJumpLift >= 0.0f
        && std::isfinite(state.velocityX) && std::isfinite(state.velocityY)
        && std::isfinite(state.pendingVelocityX) && std::isfinite(state.pendingVelocityY)
        && std::isfinite(state.pendingVelocityZ)
        && (state.specialJumpActive || (state.velocityX == 0.0f && state.velocityY == 0.0f))
        && (state.pendingSpecialJump
            || (state.pendingVelocityX == 0.0f && state.pendingVelocityY == 0.0f && state.pendingVelocityZ == 0.0f));
}
}
