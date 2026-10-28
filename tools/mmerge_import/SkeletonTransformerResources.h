#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
struct SkeletonTransformerProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

// Only d19 events 15 and 131 are translated; existing mechanisms and lifecycle hooks remain authoritative.
// A missing source leaves the profile absent; a changed source requires a compatibility review.
std::optional<SkeletonTransformerProfile> loadSkeletonTransformerProfile(const std::filesystem::path &source);

// The entrance hint comes from the selected d19.str slot 2. Source Map.Vars[19] is generated MapVar(21).
std::string makeSkeletonTransformerOverlay(std::string_view entranceHint);
}
