#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace OpenYAMM::Game
{
struct OutdoorLightTimeOverride
{
    int32_t hour = 0;
    std::optional<int32_t> minute;

    bool operator==(const OutdoorLightTimeOverride &) const = default;
};

inline bool validOutdoorLightTimeOverride(const OutdoorLightTimeOverride &value)
{
    return value.hour >= 0 && value.hour < 24
        && (!value.minute || (*value.minute >= 0 && *value.minute < 60));
}

inline double projectedOutdoorLightGameMinutes(
    double actualGameMinutes, const std::optional<OutdoorLightTimeOverride> &lightTimeOverride) noexcept
{
    if (!lightTimeOverride)
    {
        return actualGameMinutes;
    }

    // The source light hook receives integer Hour/Minute registers, separate from the real clock.
    const double dayStart = std::floor(actualGameMinutes / 1440.0) * 1440.0;
    const double minute = lightTimeOverride->minute
        ? double(*lightTimeOverride->minute) : std::floor(std::fmod(actualGameMinutes, 60.0));
    return dayStart + double(lightTimeOverride->hour) * 60.0 + minute;
}
}
