#include "game/content/BreachMazeItemResolver.h"

#include "game/content/BreachMazePopulation.h"
#include "game/tables/ItemTable.h"

namespace OpenYAMM::Game
{
std::optional<BreachMazeResolvedItem> resolveBreachMazeItem(
    const BreachMazeItemRequest &request,
    const ItemTable &itemTable,
    const StandardItemEnchantTable &standardItemEnchantTable,
    const SpecialItemEnchantTable &specialItemEnchantTable,
    std::string &error)
{
    error.clear();
    if (request.cellId == 0 || request.cellId > BreachMazeTemplateCellCount || request.treasureType != 0)
    {
        error = "Breach Maze item: unsupported cell or treasure type";
        return std::nullopt;
    }

    BreachMazeResolvedItem result;
    result.receipt.ordinal = request.ordinal;
    result.receipt.resolveKey = request.resolveKey;
    result.receipt.after.state = request.resolveKey; // Zero is a valid, deterministic initial state.

    if (request.fixedItemId != 0)
    {
        if (request.fixedItemId != 220 || request.placeholderItemId != 0 || request.treasureLevel != 0
            || request.positiveBonusStrengthAtLeastOne)
        {
            error = "Breach Maze item: invalid fixed empty-bottle request";
            return std::nullopt;
        }
        const ItemDefinition *pBottle = itemTable.get(request.fixedItemId);
        if (pBottle == nullptr || pBottle->equipStat != "Bottle" || pBottle->rarity != ItemRarity::Common)
        {
            error = "Breach Maze item: missing or incompatible empty bottle220";
            return std::nullopt;
        }
        result.item = ItemGenerator::makeInventoryItem(220, itemTable, ItemGenerationMode::Generic);
        return result; // SummonItem only: no base-item, charge, power or enchant random draws.
    }

    if (request.placeholderItemId < 177 || request.placeholderItemId > 186
        || request.treasureLevel < 1 || request.treasureLevel > BreachMazeMaximumTreasureLevel
        || !request.positiveBonusStrengthAtLeastOne)
    {
        error = "Breach Maze item: unsupported random ground-item request";
        return std::nullopt;
    }

    ItemGenerationRequest generation;
    generation.treasureLevel = request.treasureLevel;
    generation.allowRareItems = false;
    generation.alwaysEnchant = true;
    const std::optional<FixedItemGenerationResult> generated = ItemGenerator::generateFixedInventoryItem(
        itemTable, standardItemEnchantTable, specialItemEnchantTable, generation, result.receipt.after, error);
    if (!generated)
    {
        return std::nullopt;
    }

    const ItemDefinition *pDefinition = itemTable.get(generated->item.objectDescriptionId);
    if (pDefinition == nullptr || pDefinition->value <= 0 || pDefinition->rarity != ItemRarity::Common)
    {
        // The authenticated low-level source pools have positive Value and common rarity. A changed
        // pool requires a new profile, not guessed native Material cleanup or a silent retry.
        error = "Breach Maze item: generated item is outside the V1 common positive-value profile";
        return std::nullopt;
    }

    result.item = generated->item;
    result.receipt.after = generated->after;
    return result;
}
}
