#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct LichPromotionProfile
{
    std::string sourceSha256;
    std::string miscTweaksSha256;
    std::string luaSource;
};

// Projects Lich topics 738/846/847, the topic 89 tail, and MiscTweaks' source-managed class effects.
// Missing PromotionTopics disables the projection; otherwise both source scripts are required and authenticated.
std::optional<LichPromotionProfile> loadLichPromotionProfile(const std::filesystem::path &source);
}
