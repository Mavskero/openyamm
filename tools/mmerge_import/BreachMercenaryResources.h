#pragma once

#include "game/content/BreachMercenaryProfile.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace OpenYAMM::Import
{
inline constexpr std::array<std::string_view, 3> BreachMercenarySourcePaths{{
    "Scripts/Maps/Breach.lua",
    "Scripts/General/NPCMercenaries.lua",
    "Scripts/General/MenuChooseCharacter.lua",
}};

struct BreachMercenaryResources
{
    Game::BreachMercenaryProfile profile;
};

// All three scripts absent means no profile; any present requires the complete exact source fingerprint set.
// chineseText selects the same global.txt archive as the normal NPC importer. Its raw payload SHA is retained.
// This bounded translation does not execute source Lua or authorize map admission/Character ownership.
std::optional<BreachMercenaryResources> loadBreachMercenaryResources(
    const std::filesystem::path &source, bool chineseText);
}
