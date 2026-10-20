#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace OpenYAMM::Game
{
// Existing calendar/event consumers address integer minutes. Retain their range while preserving
// subminute precision in the authority and the original Merge 256-tick/minute deadlines.
constexpr double MaximumGameMinutes = std::numeric_limits<int32_t>::max();
constexpr int64_t GameTimeTicksPerMinute = 256;
constexpr int64_t MaximumGameTimeTicks = int64_t(std::numeric_limits<int32_t>::max()) * GameTimeTicksPerMinute;

inline bool validGameMinutes(double minutes)
{
    return std::isfinite(minutes) && minutes >= 0.0 && minutes <= MaximumGameMinutes;
}

inline std::optional<int64_t> gameTimeTicks(double minutes)
{
    if (!validGameMinutes(minutes))
    {
        return std::nullopt;
    }
    return int64_t(std::floor(minutes * GameTimeTicksPerMinute));
}
}
