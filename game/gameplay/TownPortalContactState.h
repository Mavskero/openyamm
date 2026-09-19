#pragma once

namespace OpenYAMM::Game
{
// The original first-contact hook keeps a transient four-minute delay across map changes.
// Even a cast away from a Dimension Door trigger postpones the next possible interruption.
struct TownPortalContactState
{
    double interruptAfterGameMinutes = 0.0;

    bool onTownPortalAttempt(double gameMinutes, bool dimensionDoorEligible, bool metVerdant)
    {
        if (metVerdant)
        {
            return false;
        }

        const bool shouldInterrupt = dimensionDoorEligible && gameMinutes > interruptAfterGameMinutes;
        interruptAfterGameMinutes = gameMinutes + 4.0;
        return shouldInterrupt;
    }
};
} // namespace OpenYAMM::Game
