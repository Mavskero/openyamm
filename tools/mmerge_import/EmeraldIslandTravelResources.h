#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
struct EmeraldIslandTravelProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

// Only the Q519-dependent boat route is translated. Missing 7out03.lua leaves the profile absent;
// otherwise its fingerprint and the relevant original transport table records must match.
std::optional<EmeraldIslandTravelProfile> loadEmeraldIslandTravelProfile(const std::filesystem::path &source);

// The caller supplies the localized destination name from the selected source MapStats entry for 7out01.odm.
std::string makeEmeraldIslandTravelOverlay(std::string_view destinationName);
}
