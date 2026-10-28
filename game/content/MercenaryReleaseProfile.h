#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
inline constexpr uint32_t MercenaryReleaseContentVersion = 1;
inline constexpr std::string_view MercenaryReleaseContentPath = "mercenary_release.txt";
inline constexpr std::string_view MercenaryReleaseSourceSha256 =
    "95a53fa79282b211907ff33ca5509ac892283d111fd6a5a4f171acc75cb7384b";
inline constexpr std::string_view MercenaryReleaseChineseGlobalTextSha256 =
    "b2ea93cf0d823b60aa9d0b2aa5ded4df93d39fc724048d3618609dacfae1e376";
inline constexpr std::string_view MercenaryReleaseEnglishGlobalTextSha256 =
    "6616594c1061155f58d6d8210ff8744b6ec34e5eb0f9f92e793fd96b0a17eb96";

struct MercenaryReleaseProtection
{
    uint32_t sourceRosterId = 0;
    // Empty means permanent protection; otherwise any set bit permits release.
    std::vector<uint32_t> unlockQuestBits;
    bool operator==(const MercenaryReleaseProtection &) const = default;
};

// Global authored source policy, independent of Breach presentation or Character ownership.
// The supported roster range ends at 49 because the original Join path rejects higher slots.
struct MercenaryReleaseProfile
{
    uint32_t version = MercenaryReleaseContentVersion;
    std::string sourceSha256;
    uint32_t minimumSourceRosterId = 0;
    uint32_t maximumSourceRosterId = 0;
    uint32_t npcRosterOffset = 0;
    uint32_t rosterQuestBitOffset = 0;
    uint32_t confirmationWindowMilliseconds = 0;
    std::array<MercenaryReleaseProtection, 3> protections{};
    uint32_t confirmationGlobalTextId = 0;
    uint32_t confirmationSoundId = 0;
    uint32_t rejectionSoundId = 0;
    bool chineseText = false;
    std::string globalTextSha256;
    std::string confirmationText;
    bool operator==(const MercenaryReleaseProfile &) const = default;
};

bool validMercenaryReleaseProfile(const MercenaryReleaseProfile &profile, std::string &error);
std::string serializeMercenaryReleaseProfile(const MercenaryReleaseProfile &profile);
std::optional<MercenaryReleaseProfile> loadMercenaryReleaseProfileText(
    const std::string &text, std::string &error);

// Invalid policy and unsupported aliases fail closed. Ownership and confirmation are checked by the caller.
bool sourceMercenaryReleaseProtected(const MercenaryReleaseProfile &profile, uint32_t sourceRosterId,
    const std::unordered_set<uint32_t> &questBits);
}
