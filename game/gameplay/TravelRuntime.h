#pragma once

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
class Party;

void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired);
}
