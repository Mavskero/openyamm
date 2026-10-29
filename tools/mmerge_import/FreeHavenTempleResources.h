#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
struct FreeHavenTempleProfile
{
    std::string mapStem;
    std::string sourceSha256;
};

struct FreeHavenCouncilProfile
{
    std::string mapSourceSha256;
    std::string globalSourceSha256;
    std::string mapLuaSource;
    std::string globalLuaSource;
};

// The temple profile translates only transaction 19 and Adventurer's Inn entrance 140.
// Missing source leaves the profile absent; changed source requires an explicit compatibility review.
std::optional<FreeHavenTempleProfile> loadFreeHavenTempleProfile(const std::filesystem::path &source);

// Audited outc2 entrances 49/50 and the StdQuestsFunctions after-event 1693 movie/house refresh.
// Both source files must be present and authenticated before enabling this combined projection.
std::optional<FreeHavenCouncilProfile> loadFreeHavenCouncilProfile(const std::filesystem::path &source);

// Temple messages use the selected, decoded outc2.str slots 29 and 30. The existing temple hint
// and House1607's localized name supply the two entrance hints.
std::string makeFreeHavenTempleOverlay(std::string_view templeHint, std::string_view repairedText,
    std::string_view chaliceText, std::string_view innHint);
}
