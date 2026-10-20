#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct SmugglersCoveProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

// Translates the audited original map entry and monster-killed rules. Missing source leaves
// the profile absent; changed source must be reviewed before enabling the selective overlay.
std::optional<SmugglersCoveProfile> loadSmugglersCoveProfile(
    const std::filesystem::path &source);
}
