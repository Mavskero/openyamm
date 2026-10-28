#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct SciFiProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

// Only basic Blaster learning is appended to event 61; its existing handler and learned skills are preserved.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<SciFiProfile> loadSciFiProfile(const std::filesystem::path &source);
}
