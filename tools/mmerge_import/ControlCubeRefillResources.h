#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct ControlCubeRefillProfile
{
    std::string mapStem;
    std::string sourceSha256;
    uint16_t hookEventId = 0;
    uint32_t refillIntervalDays = 0;
    std::string luaSource;
};

// The source refill hook resets items on existing actors 0/1; the original Q759 entry event is retained.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<ControlCubeRefillProfile> loadControlCubeRefillProfile(const std::filesystem::path &source);
}
