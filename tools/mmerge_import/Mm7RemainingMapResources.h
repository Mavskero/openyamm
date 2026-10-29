#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Import
{
struct Mm7RemainingMapProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string followerApiSourceSha256;
    std::vector<uint16_t> requiredEventIds;
    std::vector<uint16_t> requiredStringIds;
    std::vector<uint16_t> requiredGlobalTextIds;
    // All new handlers, including callbacks which have no static timer registration.
    std::vector<uint16_t> addedEventIds;
    std::vector<uint16_t> addedOnLoadEventIds;
    std::vector<uint16_t> addedAfterLoadEventIds;
    std::vector<uint16_t> addedOnLeaveEventIds;
    std::vector<uint16_t> removedOnLoadEventIds;
    std::vector<uint16_t> removedLegacyTimerEventIds;
    std::vector<uint16_t> addedNpcExitHookEventIds;
    std::vector<uint16_t> addedRestFoodCostHookEventIds;
    std::vector<uint16_t> addedHouseTopicFilterHookEventIds;
    std::vector<uint16_t> addedGameplayActionHookEventIds;
    std::unordered_map<uint16_t, std::string> globalTexts;
    std::string globalTextSha256;
    std::string baseGlobalTextSha256;
};

struct Mm7RemainingGlobalProfile
{
    std::string sourcePath;
    std::string sourceSha256;
    std::string followerApiSourceSha256;
    std::vector<uint16_t> requiredEventIds;
    std::vector<uint16_t> addedNpcEnterHookEventIds;
    std::string luaSource;
};

// Only the finite MM7 map differences recorded by audit171 are projected. Missing source scripts omit
// profiles; changed, oversized or escaping source is rejected. Arena static rules are a scene sidecar.
std::vector<Mm7RemainingMapProfile> loadMm7RemainingMapProfiles(
    const std::filesystem::path &source, bool chineseText);

std::string makeMm7RemainingMapOverlay(const Mm7RemainingMapProfile &profile,
    const std::unordered_map<uint16_t, std::string> &strings = {});

// Appends only the MM7 follower tails from StdQuestsFollowers and PromotionTopics.
std::vector<Mm7RemainingGlobalProfile> loadMm7RemainingGlobalProfiles(const std::filesystem::path &source);
}
