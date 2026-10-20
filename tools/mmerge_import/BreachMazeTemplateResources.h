#pragma once

#include "game/content/BreachMazeLayout.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct BreachMazeTemplateProfile
{
    std::string sourceScriptSha256;
    std::string geometrySha256;
    Game::BreachMazeTemplate mazeTemplate;
};

// Missing BrBase.lua leaves the profile absent. Changed scripts, changed geometry and escaping paths fail.
// Reads Data/breach.games.lod; only extracts metadata, with no event generation or gameplay admission.
std::optional<BreachMazeTemplateProfile> loadBreachMazeTemplateProfile(const std::filesystem::path &source);

// Structural analyzer for the original MM8 16x16 BLV template. Throws on malformed/non-template data.
// This diagnostic entry point is not a substitute for the fingerprint-gated import profile above.
Game::BreachMazeTemplate analyzeBreachMazeTemplate(const std::vector<uint8_t> &geometry);
}
