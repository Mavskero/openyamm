#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Import
{
inline constexpr uint16_t Mm6RemainingMapAfterLoadEventId = 65477;
inline constexpr uint16_t Mm6NicolaiKidnapEventId = 65478;
inline constexpr uint16_t Mm6NicolaiAfterLoadEventId = 65479;
inline constexpr uint16_t Mm6WinterSnowEventId = 65484;

struct Mm6TileSetFootstepRule
{
    uint16_t tileSet = 0;
    uint32_t walkSoundId = 0;
    uint32_t runSoundId = 0;
};

struct Mm6IndoorFacetTypeRule
{
    uint16_t faceIndex = 0;
    uint8_t facetType = 0;
};

struct Mm6RemainingMapProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::vector<uint16_t> requiredEventIds;
    std::vector<uint16_t> requiredStringIds;
    std::vector<uint16_t> addedEventIds;
    std::vector<uint16_t> addedAfterLoadEventIds;
    std::vector<uint16_t> removedLegacyTimerEventIds;
    std::vector<Mm6TileSetFootstepRule> tileSetFootsteps;
    std::vector<Mm6IndoorFacetTypeRule> indoorFacetTypes;
    std::vector<uint16_t> bolsterExcludedUniqueNameIds;
    // A source defect is recorded without replacing the base handler with invented behavior.
    std::string unresolvedSourceNote;
};

struct Mm6RemainingGlobalProfile
{
    std::string followersSourceSha256;
    std::string followerApiSourceSha256;
    std::string functionsSourceSha256;
    std::string luaSource;
    std::vector<uint16_t> addedEventIds;
    std::vector<uint16_t> addedAfterLoadEventIds;
};

// These twenty map sources are the finite MM6 remainder recorded by the 171 source audit.
// Each available source is authenticated; missing maps omit only their own profiles.
std::vector<Mm6RemainingMapProfile> loadMm6RemainingMapProfiles(const std::filesystem::path &source);
std::optional<Mm6RemainingGlobalProfile> loadMm6RemainingGlobalProfile(const std::filesystem::path &source);

// Localized STR IDs are declared by each profile. Existing unrelated events, hints and lifecycle lists remain.
// Facet/bolster and terrain-only profiles return no Lua; their typed data is consumed by the scene importer.
std::string makeMm6RemainingMapOverlay(const Mm6RemainingMapProfile &profile,
    const std::unordered_map<uint16_t, std::string> &strings = {});
}
