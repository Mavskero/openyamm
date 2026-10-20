#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace OpenYAMM::Import
{
struct RunawayChaosProfile
{
    std::string sourceSha256;
    std::string luaSource;
};

// Global dialogue only. This never enables any Merge map, travel, split party, reward or end card.
std::optional<RunawayChaosProfile> loadRunawayChaosProfile(
    const std::filesystem::path &source,
    const std::unordered_map<uint32_t, std::string> &npcNames,
    const std::unordered_map<uint32_t, std::string> &npcTopicNames,
    const std::unordered_map<uint32_t, std::string> &npcTexts);
}
