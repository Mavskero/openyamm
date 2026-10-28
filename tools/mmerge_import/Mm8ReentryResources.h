#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct Mm8ReentryProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

// Authenticate out02.lua and pbp.lua independently. Missing scripts omit their profiles;
// present but invalid or changed scripts require a compatibility review.
std::vector<Mm8ReentryProfile> loadMm8ReentryProfiles(const std::filesystem::path &source);

// Only the crystal entrance (out02/504) and four prison entrances (pbp/502-505) are translated.
// out02 retains its exported hint. pbp requires decoded source STR slots 51-54 in that order.
// Opened flags belong to the map snapshot and reset naturally on a full map refill.
std::string makeMm8ReentryOverlay(const Mm8ReentryProfile &profile,
    const std::array<std::string, 4> &prisonHints = {});
}
