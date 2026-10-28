#pragma once

#include "game/gameplay/BreachAlvarCompanionTransaction.h"

namespace OpenYAMM::Game
{
class GameSession;
struct EventRuntimeState;

// Shared by the original Alvar dialogue and cross-map owned-mercenary return.
std::optional<BreachAlvarCompanionTopicFacts> resolveBreachAlvarCompanionTopicFacts(
    const GameSession &session, const EventRuntimeState &events, std::string &error);
void applyBreachAlvarNpcProjection(EventRuntimeState &events, const BreachAlvarCompanionNpcs &npcs, bool split);
}
