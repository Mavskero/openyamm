#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Import
{
struct Mm8TileSetFootstepRule
{
    uint16_t tileSet = 0;
    uint32_t walkSoundId = 0;
    uint32_t runSoundId = 0;
};

struct Mm8TerrainBitmapRule
{
    uint16_t firstSourceIndex = 0;
    uint16_t lastSourceIndex = 0;
    std::string sourceTexturePrefix;
    std::string textureName;
};

struct Mm8RemainingMapProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::vector<uint16_t> requiredEventIds;
    std::vector<uint16_t> requiredStringIds;
    std::vector<uint16_t> addedOnLoadEventIds;
    std::vector<uint16_t> addedAfterLoadEventIds;
    std::vector<Mm8TileSetFootstepRule> tileSetFootsteps;
    std::optional<Mm8TerrainBitmapRule> terrainBitmap;
    // Empty for installed event projections. d42 has no proven ordinary cross-world ingress/origin contract.
    std::string deferredReason;
};

// Each of the eight named 171 audit sources is independently authenticated. Missing scripts omit profiles;
// changed, oversized or escaping scripts are rejected rather than projected through an approximate fallback.
std::vector<Mm8RemainingMapProfile> loadMm8RemainingMapProfiles(const std::filesystem::path &source);

// out05 requires original localized STR70/71. Other event projections retain their exported hints.
// Terrain-only out04/out06 and deferred d42 return no Lua; their typed data remains available to the importer.
// out07 requires the shared read-only FindPartyMemberInventoryItem API before its overlay is installed.
std::string makeMm8RemainingMapOverlay(const Mm8RemainingMapProfile &profile,
    const std::unordered_map<uint16_t, std::string> &strings = {});
}
