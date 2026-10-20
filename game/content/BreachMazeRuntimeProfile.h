#pragma once

#include "game/content/BreachMazePopulation.h"

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachMazeRuntimeCodecVersion = 1;
inline constexpr size_t BreachMazeRuntimeProfileMaximumBytes = 8 * 1024 * 1024;
inline constexpr size_t BreachMazePopulationPlanMaximumBytes = 2 * 1024 * 1024;
inline constexpr std::string_view BreachMazeRuntimeProfileAsset = "content/breach-maze-profile.bin";

struct BreachMazeRuntimeProfile
{
    BreachMazeTemplate mazeTemplate;
    BreachMazePopulationCatalog catalog;
    bool operator==(const BreachMazeRuntimeProfile &) const = default;
};

// Versioned bounded little-endian value codecs. Invalid, truncated, oversized, unknown-version,
// inconsistent-fingerprint or trailing input throws std::invalid_argument. Identity is checked,
// never repaired. The profile originates exclusively from the SHA-gated importer.
std::vector<uint8_t> encodeBreachMazeRuntimeProfile(const BreachMazeRuntimeProfile &profile);
BreachMazeRuntimeProfile decodeBreachMazeRuntimeProfile(std::span<const uint8_t> bytes);
std::vector<uint8_t> encodeBreachMazePopulationPlan(const BreachMazePopulationPlan &plan);
BreachMazePopulationPlan decodeBreachMazePopulationPlan(std::span<const uint8_t> bytes);
}
