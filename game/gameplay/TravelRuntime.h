#pragma once

#include "game/events/EventRuntime.h"

#include <string>

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
class Party;

bool validateDeferredTravelPlan(const EventRuntimeState::DeferredTravelPlan &plan, std::string &error);
// The caller owns the snapshot and rollback if source events or map loading fail after these effects.
bool applyDeferredTravelPlan(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    const EventRuntimeState::DeferredTravelPlan &plan,
    std::string &error);

void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired);
}
