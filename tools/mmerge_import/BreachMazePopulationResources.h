#pragma once

#include "game/content/BreachMazePopulation.h"

#include <filesystem>
#include <optional>

namespace OpenYAMM::Import
{
struct BreachMazePopulationProfile
{
    Game::BreachMazeTemplate mazeTemplate;
    Game::BreachMazePopulationCatalog catalog;
};

// Missing BrBase.lua means no profile. All present inputs must match the fixed original source hashes.
// Reads actual merged monsters, kind/bolster/MonList/DecList tables and BLV slots. No chosen-ID list,
// package generation, renderer loading or normal-map admission is performed. Source is read-only.
// Content fix v1 explicitly resolves source "trashheap" to original "trasheap", keeping both names.
std::optional<BreachMazePopulationProfile> loadBreachMazePopulationProfile(const std::filesystem::path &source);
}
