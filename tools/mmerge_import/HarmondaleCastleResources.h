#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct HarmondaleCastleProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

// Only the audited Castle Harmondale entrance and Judge Grey succession trigger are translated.
// The original MMExtension map script is never executed. A missing script leaves the profile absent;
// a changed script requires an explicit compatibility review before this selective overlay is enabled.
std::optional<HarmondaleCastleProfile> loadHarmondaleCastleProfile(
    const std::filesystem::path &source);
}
