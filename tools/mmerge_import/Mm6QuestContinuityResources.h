#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct Mm6QuestContinuityProfile
{
    std::string sewerSourceSha256;
    std::string followersSourceSha256;
    std::string followerApiSourceSha256;
    std::string keyChestSourceSha256;
    std::string sewerLuaSource;
    std::string globalLuaSource;
    std::string keyChestLuaSource;
};

// Translate only NPC802's three follower callbacks and the source 6d07 event 16 key/chest choice.
// Missing source omits the profile. Every available source must match the audited fingerprint.
std::optional<Mm6QuestContinuityProfile> loadMm6QuestContinuityProfile(const std::filesystem::path &source);
}
