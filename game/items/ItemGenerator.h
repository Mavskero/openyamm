#pragma once

#include "game/items/ItemGenerationRandom.h"
#include "game/party/Party.h"

#include <functional>
#include <optional>
#include <random>
#include <string>

namespace OpenYAMM::Game
{
class ItemTable;
class StandardItemEnchantTable;
class SpecialItemEnchantTable;
struct ItemDefinition;

enum class ItemGenerationMode : uint8_t
{
    Generic = 0,
    Shop,
    MonsterLoot,
    ChestLoot,
};

struct ItemGenerationRequest
{
    int treasureLevel = 1;
    ItemGenerationMode mode = ItemGenerationMode::Generic;
    bool allowRareItems = true;
    bool rareItemsOnly = false;
    bool alwaysEnchant = false;
};

struct FixedItemGenerationResult
{
    InventoryItem item;
    FixedItemGenerationCursor after;
};

class ItemGenerator
{
public:
    // Shared legacy GiveItem type semantics. Unknown types retain the legacy unrestricted behavior;
    // fixed generation accepts only the explicitly supported types listed below.
    static bool matchesLegacyTreasureType(const ItemDefinition &itemDefinition, uint32_t treasureType);
    static InventoryItem makeInventoryItem(
        uint32_t itemId,
        const ItemTable &itemTable,
        ItemGenerationMode mode);
    static std::optional<InventoryItem> generateRandomInventoryItem(
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const ItemGenerationRequest &request,
        Party *pParty,
        std::mt19937 &rng,
        const std::function<bool(const ItemDefinition &)> &filter = {});
    static std::optional<uint32_t> chooseRandomBaseItem(
        const ItemTable &itemTable,
        int treasureLevel,
        std::mt19937 &rng,
        const std::function<bool(const ItemDefinition &)> &filter = {});
    // V1: Generic, strength1..6, both rare flags false. No Party, external filter or live cursor.
    // Types: 0=Any, 16=Sscroll, 20=Weapon_, 21=Armor_, 22=Misc, 40=Ring_, 43=Scroll_, 45=Reagent_.
    // Default GenerateMercenary also uses exact equip types6..12 and skill types23..33.
    // Default Any preserves the original V1 draw schedule, including potion strength.
    static std::optional<FixedItemGenerationResult> generateFixedInventoryItem(
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const ItemGenerationRequest &request,
        const FixedItemGenerationCursor &before,
        std::string &error,
        uint32_t treasureType = 0);
};
}
