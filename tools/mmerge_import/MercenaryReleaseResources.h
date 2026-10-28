#pragma once

#include "game/content/MercenaryReleaseProfile.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace OpenYAMM::Import
{
inline constexpr std::string_view MercenaryReleaseSourcePath = "Scripts/General/NPCMercenaries.lua";

struct MercenaryReleaseResources
{
    Game::MercenaryReleaseProfile profile;
};

// Absent source means no policy. Present source and the selected language table must match the authored inputs.
// This imports release rules only; it does not release a Character or execute source Lua.
std::optional<MercenaryReleaseResources> loadMercenaryReleaseResources(
    const std::filesystem::path &source, bool chineseText);
}
