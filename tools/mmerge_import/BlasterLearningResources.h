#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct BlasterLearningProfile
{
    std::string sourceSha256;
    std::string luaSource;
    std::string topic;
    std::string baseTopic;
    std::string globalTextSha256;
    std::string baseGlobalTextSha256;
};

// Only PromotionTopics.lua's basic Blaster topic 950 is translated; other promotions remain untouched.
// Missing source disables the projection. Changed source requires an explicit compatibility review.
std::optional<BlasterLearningProfile> loadBlasterLearningProfile(
    const std::filesystem::path &source, bool chineseText);
}
