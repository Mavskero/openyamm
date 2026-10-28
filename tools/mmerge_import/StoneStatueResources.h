#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
struct StoneStatueProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

std::optional<StoneStatueProfile> loadStoneStatueProfile(const std::filesystem::path &source);
std::string makeStoneStatueOverlay(std::string_view statueHint);
}
