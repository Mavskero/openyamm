#pragma once

#include "game/events/GameTime.h"

#include <bit>
#include <string>
#include <unordered_map>

namespace OpenYAMM::Game
{
using NamedGameVars = std::unordered_map<std::string, int32_t>;
constexpr const char *GameTimeHighSuffix = ".TicksHi";
constexpr const char *GameTimeLowSuffix = ".TicksLo";

inline bool validGameTimeName(const std::string &name)
{
    return !name.empty() && name.size() <= 96
        && name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.") == std::string::npos
        && !name.ends_with(GameTimeHighSuffix) && !name.ends_with(GameTimeLowSuffix);
}

inline bool readNamedGameTime(const NamedGameVars &vars, const std::string &name, int64_t fallback,
    int64_t &ticks, std::string &error)
{
    if (!validGameTimeName(name) || fallback < 0 || fallback > MaximumGameTimeTicks)
    {
        error = "Invalid named game time name or fallback";
        return false;
    }
    const auto hi = vars.find(name + GameTimeHighSuffix);
    const auto lo = vars.find(name + GameTimeLowSuffix);
    if (hi == vars.end() && lo == vars.end())
    {
        ticks = fallback;
        return true;
    }
    if (hi == vars.end() || lo == vars.end() || hi->second < 0
        || hi->second > MaximumGameTimeTicks / (int64_t(1) << 32))
    {
        error = "Incomplete or invalid named game time fields";
        return false;
    }
    ticks = (int64_t(hi->second) << 32) | std::bit_cast<uint32_t>(lo->second);
    if (ticks > MaximumGameTimeTicks)
    {
        error = "Named game time exceeds supported calendar range";
        return false;
    }
    return true;
}

inline bool writeNamedGameTime(NamedGameVars &vars, const std::string &name, int64_t ticks, std::string &error)
{
    if (!validGameTimeName(name) || ticks < 0 || ticks > MaximumGameTimeTicks)
    {
        error = "Invalid named game time name or value";
        return false;
    }
    vars[name + GameTimeHighSuffix] = int32_t(ticks >> 32);
    vars[name + GameTimeLowSuffix] = std::bit_cast<int32_t>(uint32_t(ticks));
    return true;
}

inline bool validateNamedGameTimes(const NamedGameVars &vars, std::string &error)
{
    for (const auto &[name, value] : vars)
    {
        (void)value;
        const std::string suffix = name.ends_with(GameTimeHighSuffix) ? GameTimeHighSuffix
            : name.ends_with(GameTimeLowSuffix) ? GameTimeLowSuffix : "";
        if (!suffix.empty())
        {
            int64_t ticks = 0;
            if (!readNamedGameTime(vars, name.substr(0, name.size() - suffix.size()), 0, ticks, error))
            {
                return false;
            }
        }
    }
    return true;
}
}
