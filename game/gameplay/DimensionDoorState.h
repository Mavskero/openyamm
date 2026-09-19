#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace OpenYAMM::Game
{
enum class DimensionDoorActivationSource
{
    NaturalLocation,
    ItemScroll
};

struct DimensionDoorReversionTimer
{
    double deadlineGameMinutes = 0.0;
    double intervalGameMinutes = 8.0;
};

struct DimensionDoorState
{
    bool active = false;
    uint32_t destinationDayIndex = 0;
    std::vector<DimensionDoorReversionTimer> reversionTimers;

    void activate(
        double gameMinutes,
        DimensionDoorActivationSource source = DimensionDoorActivationSource::NaturalLocation)
    {
        const double intervalGameMinutes = source == DimensionDoorActivationSource::ItemScroll ? 1.0 : 8.0;
        active = true;
        destinationDayIndex = static_cast<uint32_t>(std::floor(std::max(0.0, gameMinutes) / 1440.0));
        reversionTimers.push_back({gameMinutes + intervalGameMinutes, intervalGameMinutes});
    }

    void update(double gameMinutes, bool infoScreenOpen)
    {
        size_t index = 0;
        while (index < reversionTimers.size())
        {
            if (gameMinutes < reversionTimers[index].deadlineGameMinutes)
            {
                ++index;
            }
            else if (infoScreenOpen)
            {
                DimensionDoorReversionTimer &timer = reversionTimers[index++];
                timer.deadlineGameMinutes = gameMinutes + timer.intervalGameMinutes;
            }
            else
            {
                active = false;
                const DimensionDoorReversionTimer firingTimer = reversionTimers[index];
                // Original RemoveTimer(function) removes the first registered matching timer,
                // even if a later timer is the one currently firing.
                reversionTimers.erase(reversionTimers.begin());
                if (index > 0)
                {
                    reversionTimers[index - 1].deadlineGameMinutes =
                        gameMinutes + firingTimer.intervalGameMinutes;
                }
            }
        }
    }
};
} // namespace OpenYAMM::Game
