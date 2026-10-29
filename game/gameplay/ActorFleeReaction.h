#pragma once

#include "game/gameplay/GameplayRuntimeInterfaces.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace OpenYAMM::Game
{
inline bool isWithinPartyYellRange(const GameplayWorldPoint &actor, const GameplayWorldPoint &party)
{
    const float dx = party.x - actor.x;
    const float dy = party.y - actor.y;
    const float dz = party.z - actor.z;
    return dx * dx + dy * dy + dz * dz < 512.0f * 512.0f;
}

struct ActorFleeReaction
{
    float actionSeconds = 0.0f;
    float yawRadians = 0.0f;
    float moveDirectionX = 0.0f;
    float moveDirectionY = 0.0f;
};

inline ActorFleeReaction partyYellFleeReaction(
    const GameplayWorldPoint &actor,
    const GameplayWorldPoint &party,
    uint16_t moveSpeed,
    uint32_t randomValue)
{
    constexpr float Pi = 3.14159265358979323846f;
    constexpr int TicksPerSecond = 128;
    const float dx = party.x - actor.x;
    const float dy = party.y - actor.y;
    const float horizontalDistance = std::hypot(dx, dy);
    const int actionTicks = moveSpeed != 0
        ? std::min(256, static_cast<int>(horizontalDistance * TicksPerSecond) / moveSpeed)
        : 0;

    ActorFleeReaction reaction;
    reaction.actionSeconds = static_cast<float>(actionTicks) / TicksPerSecond;
    // The sampled half circle faces away from the party, from 90 through 270 degrees.
    reaction.yawRadians = std::fmod(
        std::atan2(dy, dx) + Pi * 0.5f + static_cast<float>(randomValue % 1024u) * Pi / 1024.0f
            + Pi * 2.0f,
        Pi * 2.0f);
    reaction.moveDirectionX = std::cos(reaction.yawRadians);
    reaction.moveDirectionY = std::sin(reaction.yawRadians);
    return reaction;
}
}
