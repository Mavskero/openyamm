#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace OpenYAMM::Import
{
inline constexpr std::string_view ClankerPostQuestSourcePath = "Scripts/Maps/7out04.lua";
inline constexpr std::string_view ClankerPostQuestSourceSha256 =
    "a3c2c6008c3cd49ee99548b1adae5af8f792e83d6cfb77adc63815a1e9311cb3";
inline constexpr uint16_t PostQuestTritonHideHookEventId = 65475;

struct PostQuestVisitProfile
{
    std::string world;
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
    uint16_t afterLoadEventId = 0;
};

// Projects only authenticated post-alliance Triton hiding and Clanker's post-quest house visit.
// Source house395 exit data is projected separately by the service/house importer.
std::vector<PostQuestVisitProfile> loadPostQuestVisitProfiles(const std::filesystem::path &source);
}
