#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
inline constexpr uint16_t Mm6LostShieldHookEventId = 65476;

struct Mm6LostShieldProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string lostItemsSourceSha256;
    std::string luaSource;
};

// Translate outb2's chest1 shield rule using the source GotItem history, not current inventory absence.
// The caller must verify that converted item2119 has zero value, as required by the source LostItems tracker.
std::optional<Mm6LostShieldProfile> loadMm6LostShieldProfile(const std::filesystem::path &source);
}
