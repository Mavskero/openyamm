#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct CrossContinentsBreachTravelTopic
{
    uint32_t id = 65517;
    std::string topic;
    std::string baseTopic;
    std::string message;
};

struct CrossContinentsIntroProfile
{
    std::string sourceSha256;
    std::string luaSource;
    CrossContinentsBreachTravelTopic breachTravelTopic;
};

// Audited quest hooks and Verdant dialogue are translated; the original MMExtension script is never executed.
// Missing scripts leave the profile absent; changed scripts require an explicit compatibility review.
std::optional<CrossContinentsIntroProfile> loadCrossContinentsIntroProfile(const std::filesystem::path &source);
}
