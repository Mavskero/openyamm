#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct ArtifactMessengerProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

// Replaces the Tularean Forest messenger load event with the authenticated source gate and battle groups.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<ArtifactMessengerProfile> loadArtifactMessengerProfile(const std::filesystem::path &source);
}
