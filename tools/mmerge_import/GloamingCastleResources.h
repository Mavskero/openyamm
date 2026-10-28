#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
struct GloamingCastleProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

// Only event 5 is translated. Existing map-load and continent reward hooks retain their ownership.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<GloamingCastleProfile> loadGloamingCastleProfile(const std::filesystem::path &source);

// Text comes from the selected, decoded d03.str slots 20 and 21, preserving source localization.
std::string makeGloamingCastleOverlay(std::string_view entranceHint, std::string_view lockedText);
}
