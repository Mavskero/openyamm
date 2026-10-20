#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct Mm6HiveProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

// Applies only to the audited Merge hive script, after the map's exported EVT.
// Missing source leaves the profile absent; changed or escaping source is rejected.
std::optional<Mm6HiveProfile> loadMm6HiveProfile(const std::filesystem::path &source);
}
