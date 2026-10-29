#pragma once

#include "game/events/GameTime.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>

namespace OpenYAMM::Game
{
struct SaveSlotTimeText
{
    std::string clock;
    std::string date;
};

inline SaveSlotTimeText formatSaveSlotTime(double gameMinutes)
{
    constexpr std::array<const char *, 7> Weekdays = {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    };
    constexpr std::array<const char *, 12> Months = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    const int64_t minutes = std::isfinite(gameMinutes)
        ? int64_t(std::floor(std::clamp(gameMinutes, 0.0, MaximumGameMinutes))) : 0;
    const int64_t days = minutes / 1440;
    const int hour24 = int(minutes / 60 % 24);
    const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
    const int minute = int(minutes % 60);
    SaveSlotTimeText result;
    result.clock = std::string(Weekdays[size_t(days % 7)]) + " " + std::to_string(hour12)
        + ":" + (minute < 10 ? "0" : "") + std::to_string(minute) + (hour24 >= 12 ? " PM" : " AM");
    result.date = std::to_string(1 + days % 28) + " " + Months[size_t(days / 28 % 12)]
        + " " + std::to_string(1168 + days / (12 * 28));
    return result;
}

inline std::string friendlySaveFileLabel(const std::filesystem::path &path)
{
    std::string label = path.stem().string();
    std::string normalized = label;
    for (char &character : normalized)
    {
        character = char(std::tolower(static_cast<unsigned char>(character)));
    }
    if (normalized == "autosave")
    {
        return "Autosave";
    }
    if (normalized == "quicksave")
    {
        return "Quicksave";
    }
    if (normalized.starts_with("save") && normalized.size() > 4
        && std::all_of(normalized.begin() + 4, normalized.end(), [](char character)
        {
            return character >= '0' && character <= '9';
        }))
    {
        // File names are unbounded decimal identifiers, not machine-sized slot numbers.
        const size_t firstDigit = normalized.find_first_not_of('0', 4);
        return "Save " + (firstDigit == std::string::npos ? "1" : normalized.substr(firstDigit));
    }
    if (!label.empty())
    {
        label[0] = char(std::toupper(static_cast<unsigned char>(label[0])));
    }
    return label;
}
}
