#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
struct StoneCityProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

// Only event 416 is translated. Existing map-load and continent reward hooks retain their ownership.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<StoneCityProfile> loadStoneCityProfile(const std::filesystem::path &source);

// Text comes from the selected, decoded 7d24.str slots 20 and 19, preserving source localization.
std::string makeStoneCityOverlay(std::string_view entranceHint, std::string_view lockedText);
}
