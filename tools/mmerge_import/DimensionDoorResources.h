#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct DimensionDoorMapOverlay
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

struct DimensionDoorProfile
{
    std::string sourceSha256;
    std::string timerSourceSha256;
    std::vector<DimensionDoorMapOverlay> maps;
};

// Only the audited activation sites are translated. The original MMExtension scripts are never executed.
// A missing main travel script leaves the profile absent; changed recognized scripts require a compatibility review.
std::optional<DimensionDoorProfile> loadDimensionDoorProfile(const std::filesystem::path &source);
}
