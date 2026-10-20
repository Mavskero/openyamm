#pragma once

#include "game/items/ItemGenerator.h"

namespace OpenYAMM::Game
{
struct BreachMazeItemRequest;

inline constexpr uint32_t BreachMazeItemResolutionVersion = 1;

// Audit metadata only. The InventoryItem, once materialized, is the sole item payload;
// this receipt never authorizes regenerating an item after pickup or during load.
struct BreachMazeItemResolutionReceipt
{
    uint32_t resolutionVersion = BreachMazeItemResolutionVersion;
    uint32_t ordinal = 0;
    uint64_t resolveKey = 0;
    FixedItemGenerationCursor after;
    bool operator==(const BreachMazeItemResolutionReceipt &) const = default;
};

struct BreachMazeResolvedItem
{
    InventoryItem item;
    BreachMazeItemResolutionReceipt receipt;
};

// Resolves frozen B1 ground requests: fixed empty bottle220 or Any strength1..3.
// V1 binds Randomize(...,0,0)'s Lua-truthy third argument to alwaysEnchant=true.
// The caller supplies source-authenticated mechanical tables and persists the source profile
// and floor/plan identities with the receipt. This pure prepare call does not authenticate
// a content package, run Lua hooks, resolve render resources, publish loot, or mutate a Party.
std::optional<BreachMazeResolvedItem> resolveBreachMazeItem(
    const BreachMazeItemRequest &request,
    const ItemTable &itemTable,
    const StandardItemEnchantTable &standardItemEnchantTable,
    const SpecialItemEnchantTable &specialItemEnchantTable,
    std::string &error);
}
