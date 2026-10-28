#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct TempleBottleProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string itemSourceSha256;
};

std::optional<TempleBottleProfile> loadTempleBottleProfile(const std::filesystem::path &source);
std::string makeTempleBottleOverlay();
}
