#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
inline constexpr uint16_t RegnaCannonMigrationHookEventId = 65474;

struct RegnaCannonProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

// Translates the repeatable source cannon sequence and explicitly migrates an unfinished legacy MapVar41 phase.
// Timer instances belong to the current map visit; source named flags retain their existing save ownership.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<RegnaCannonProfile> loadRegnaCannonProfile(const std::filesystem::path &source);

// Text comes from decoded out13.str slots 13, 11 and 18, retaining the selected source localization.
std::string makeRegnaCannonOverlay(std::string_view cannonHint, std::string_view firingText,
    std::string_view noAmmoText);
}
