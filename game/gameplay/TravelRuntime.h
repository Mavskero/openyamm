#pragma once

#include <string>

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
class Party;

int outdoorMapMoveTravelDays(const std::string &currentMapFileName, const std::string &destinationMapFileName);

void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired);
}
