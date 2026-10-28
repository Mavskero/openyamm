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

// Only the source temple transaction (19) and Adventurer's Inn entrance (140) are translated.
// Missing source leaves the profile absent; changed source requires an explicit compatibility review.
std::optional<FreeHavenTempleProfile> loadFreeHavenTempleProfile(const std::filesystem::path &source);

// Temple messages use the selected, decoded outc2.str slots 29 and 30. The existing temple hint
// and House1607's localized name supply the two entrance hints.
std::string makeFreeHavenTempleOverlay(std::string_view templeHint, std::string_view repairedText,
    std::string_view chaliceText, std::string_view innHint);
}
