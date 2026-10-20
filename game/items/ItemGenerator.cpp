#include "game/items/ItemGenerator.h"

#include "game/items/ItemEnchantRuntime.h"
#include "game/items/ItemEnchantTables.h"
#include "game/items/ItemRuntime.h"
#include "game/tables/ItemTable.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>

namespace OpenYAMM::Game
{
namespace
{
bool defaultIdentifiedState(const ItemDefinition &itemDefinition, ItemGenerationMode mode)
{
    if (mode == ItemGenerationMode::Shop)
    {
        return true;
    }

    // Loot identification is item-driven, not chest-driven:
    // items that do not require identification spawn identified,
    // while loot items that do require identification do not.
    if (!ItemRuntime::requiresIdentification(itemDefinition))
    {
        return true;
    }

    return false;
}

bool shouldTryGenerateRareItem(const ItemGenerationRequest &request)
{
    return request.allowRareItems
        && request.mode != ItemGenerationMode::Shop
        && std::clamp(request.treasureLevel, 1, 6) >= 6;
}

std::optional<uint32_t> chooseRandomRareItem(
    const ItemTable &itemTable,
    Party *pParty,
    std::mt19937 &rng,
    const std::function<bool(const ItemDefinition &)> &filter)
{
    std::vector<uint32_t> candidateIds;

    for (const ItemDefinition &entry : itemTable.entries())
    {
        if (entry.itemId == 0 || entry.hasContentFlag("NoRandom")
            || !ItemRuntime::isUniquelyGeneratedRareItem(entry))
        {
            continue;
        }

        if (filter && !filter(entry))
        {
            continue;
        }

        if (pParty != nullptr && pParty->hasFoundArtifactItem(entry.itemId))
        {
            continue;
        }

        candidateIds.push_back(entry.itemId);
    }

    if (candidateIds.empty())
    {
        return std::nullopt;
    }

    const size_t index = std::uniform_int_distribution<size_t>(0, candidateIds.size() - 1)(rng);
    return candidateIds[index];
}

int parsePositiveInteger(const std::string &value)
{
    int result = 0;
    bool hasDigit = false;

    for (char character : value)
    {
        if (!std::isdigit(static_cast<unsigned char>(character)))
        {
            break;
        }

        hasDigit = true;
        // Saturate before multiplying; malformed large mod2 values must not overflow int.
        result = std::min(0xFFFE, result * 10 + character - '0');
    }

    return hasDigit ? std::max(0, result) : 0;
}

uint16_t baseWandCharges(const ItemDefinition &itemDefinition)
{
    if (itemDefinition.equipStat != "WeaponW")
    {
        return 0;
    }

    return static_cast<uint16_t>(std::clamp(parsePositiveInteger(itemDefinition.mod2) + 1, 1, 0xFFFF));
}

void initializeWandCharges(InventoryItem &item, const ItemDefinition &itemDefinition, int randomBonus)
{
    const uint16_t baseCharges = baseWandCharges(itemDefinition);

    if (baseCharges == 0)
    {
        return;
    }

    const int maxCharges = std::clamp(static_cast<int>(baseCharges) + std::max(0, randomBonus), 1, 0xFFFF);
    item.currentCharges = static_cast<uint16_t>(maxCharges);
    item.maxCharges = static_cast<uint16_t>(maxCharges);
}

template <typename Random>
std::optional<uint32_t> chooseRandomBaseItemImpl(
    const ItemTable &itemTable,
    int treasureLevel,
    Random &rng,
    const std::function<bool(const ItemDefinition &)> &filter)
{
    const int clampedTreasureLevel = std::clamp(treasureLevel, 1, 6);
    const size_t weightIndex = static_cast<size_t>(clampedTreasureLevel - 1);
    std::vector<const ItemDefinition *> candidates;

    for (const ItemDefinition &entry : itemTable.entries())
    {
        if (entry.itemId == 0 || entry.hasContentFlag("NoRandom")
            || weightIndex >= entry.randomTreasureWeights.size())
        {
            continue;
        }

        if (ItemRuntime::isRareItem(entry))
        {
            continue;
        }

        if (filter && !filter(entry))
        {
            continue;
        }

        const int weight = std::max(0, entry.randomTreasureWeights[weightIndex]);

        if (weight <= 0)
        {
            continue;
        }

        candidates.push_back(&entry);
    }

    if (candidates.empty())
    {
        return std::nullopt;
    }

    if constexpr (std::is_same_v<Random, FixedItemGenerationRandom>)
    {
        std::sort(candidates.begin(), candidates.end(), [](const ItemDefinition *pLeft, const ItemDefinition *pRight)
        {
            return pLeft->itemId < pRight->itemId;
        });
        for (size_t index = 1; index < candidates.size(); ++index)
        {
            if (candidates[index - 1]->itemId == candidates[index]->itemId)
            {
                throw std::invalid_argument("Fixed item generation: duplicate item ID");
            }
        }
    }
    std::vector<int> weights;
    weights.reserve(candidates.size());
    for (const ItemDefinition *pCandidate : candidates)
    {
        weights.push_back(pCandidate->randomTreasureWeights[weightIndex]);
    }
    const std::optional<size_t> index = itemGenerationWeightedIndex(rng, weights);
    return index && *index < candidates.size()
        ? std::optional<uint32_t>(candidates[*index]->itemId) : std::nullopt;
}

template <typename Random>
std::optional<InventoryItem> generateOrdinaryInventoryItem(
    const ItemTable &itemTable,
    const StandardItemEnchantTable &standardItemEnchantTable,
    const SpecialItemEnchantTable &specialItemEnchantTable,
    const ItemGenerationRequest &request,
    Random &rng,
    const std::function<bool(const ItemDefinition &)> &filter)
{
    const std::optional<uint32_t> itemId = chooseRandomBaseItemImpl(itemTable, request.treasureLevel, rng, filter);

    if (!itemId)
    {
        return std::nullopt;
    }

    InventoryItem item = ItemGenerator::makeInventoryItem(*itemId, itemTable, request.mode);
    const ItemDefinition *pItemDefinition = itemTable.get(*itemId);

    if constexpr (std::is_same_v<Random, FixedItemGenerationRandom>)
    {
        if (pItemDefinition == nullptr)
        {
            throw std::invalid_argument("Fixed item generation: selected item is absent from the table");
        }
        if (pItemDefinition->equipStat == "Bottle" && *itemId != 220)
        {
            const int first = itemGenerationIntRange(rng, 1, 4);
            const int second = itemGenerationIntRange(rng, 1, 4);
            item.standardEnchantPower = uint16_t((first + second) * request.treasureLevel);
        }
    }

    if (pItemDefinition != nullptr && pItemDefinition->equipStat == "WeaponW")
    {
        initializeWandCharges(item, *pItemDefinition, itemGenerationIntRange(rng, 0, 5));
    }

    if (pItemDefinition == nullptr
        || ItemRuntime::isRareItem(*pItemDefinition)
        || !ItemEnchantRuntime::isEnchantable(*pItemDefinition))
    {
        return item;
    }

    const int clampedTreasureLevel = std::clamp(request.treasureLevel, 1, 6);
    const bool canHaveStandardEnchant = ItemEnchantRuntime::canHaveStandardEnchant(*pItemDefinition);
    const bool canHaveSpecialEnchant = ItemEnchantRuntime::canHaveSpecialEnchant(*pItemDefinition);
    const int standardChance =
        canHaveStandardEnchant ? ItemEnchantRuntime::standardEnchantChance(clampedTreasureLevel) : 0;
    const int specialChance =
        canHaveSpecialEnchant ? ItemEnchantRuntime::specialEnchantChance(*pItemDefinition, clampedTreasureLevel) : 0;

    if (!canHaveStandardEnchant)
    {
        if (specialChance <= 0)
        {
            return item;
        }

        const int roll = itemGenerationIntRange(rng, 0, 99);
        if (roll < specialChance || request.alwaysEnchant)
        {
            const std::optional<uint16_t> specialEnchantId =
                ItemEnchantRuntime::chooseSpecialEnchantId(
                    *pItemDefinition, specialItemEnchantTable, clampedTreasureLevel, rng);

            if (specialEnchantId)
            {
                item.specialEnchantId = *specialEnchantId;
            }
        }

        return item;
    }

    if (standardChance <= 0)
    {
        return item;
    }

    const int ordinaryRoll = itemGenerationIntRange(rng, 0, 99);
    const int roll = request.alwaysEnchant
        ? itemGenerationIntRange(rng, 0, standardChance + specialChance - 1) : ordinaryRoll;

    if (roll < standardChance)
    {
        const std::optional<uint16_t> standardEnchantId =
            ItemEnchantRuntime::chooseStandardEnchantId(*pItemDefinition, standardItemEnchantTable, rng);

        if (standardEnchantId)
        {
            item.standardEnchantId = *standardEnchantId;

            const StandardItemEnchantEntry *pEntry = standardItemEnchantTable.get(*standardEnchantId);

            if (pEntry != nullptr)
            {
                item.standardEnchantPower =
                    static_cast<uint16_t>(ItemEnchantRuntime::generateStandardEnchantPower(
                        pEntry->kind,
                        clampedTreasureLevel,
                        rng));
            }
        }

        return item;
    }

    if (roll < standardChance + specialChance)
    {
        const std::optional<uint16_t> specialEnchantId =
            ItemEnchantRuntime::chooseSpecialEnchantId(
                *pItemDefinition, specialItemEnchantTable, clampedTreasureLevel, rng);

        if (specialEnchantId)
        {
            item.specialEnchantId = *specialEnchantId;
        }
    }

    return item;
}
}

bool ItemGenerator::matchesLegacyTreasureType(const ItemDefinition &itemDefinition, uint32_t treasureType)
{
    const std::string &kind = itemDefinition.equipStat;
    switch (treasureType)
    {
        case 6: return kind == "Helm";
        case 7: return kind == "Belt";
        case 8: return kind == "Cloak";
        case 9: return kind == "Gauntlets";
        case 10: return kind == "Boots";
        case 11: return kind == "Ring";
        case 12: return kind == "Amulet";
        case 23: return canonicalSkillName(itemDefinition.skillGroup) == "Sword";
        case 24: return canonicalSkillName(itemDefinition.skillGroup) == "Dagger";
        case 25: return canonicalSkillName(itemDefinition.skillGroup) == "Axe";
        case 26: return canonicalSkillName(itemDefinition.skillGroup) == "Spear";
        case 27: return canonicalSkillName(itemDefinition.skillGroup) == "Bow";
        case 28: return canonicalSkillName(itemDefinition.skillGroup) == "Mace";
        case 29: return canonicalSkillName(itemDefinition.skillGroup) == "Club";
        case 30: return canonicalSkillName(itemDefinition.skillGroup) == "Staff";
        case 31: return canonicalSkillName(itemDefinition.skillGroup) == "LeatherArmor";
        case 32: return canonicalSkillName(itemDefinition.skillGroup) == "ChainArmor";
        case 33: return canonicalSkillName(itemDefinition.skillGroup) == "PlateArmor";
        case 16:
            return kind == "Sscroll";
        case 20:
            return kind == "Weapon" || kind == "Weapon1or2" || kind == "Weapon2" || kind == "Missile";
        case 21:
            return kind == "Armor" || kind == "Shield" || kind == "Helm" || kind == "Belt" || kind == "Cloak"
                || kind == "Gauntlets" || kind == "Boots" || kind == "Amulet";
        case 22:
            return kind == "Misc" || kind == "Bottle" || kind == "Reagent" || kind == "Gold" || kind == "Gem"
                || kind == "Message" || kind == "0" || kind == "N / A" || kind.empty();
        case 40:
            return kind == "Ring";
        case 43:
            return kind == "Sscroll" || kind == "Mscroll" || kind == "Book";
        case 45:
            return kind == "Reagent";
        default:
            return true;
    }
}

InventoryItem ItemGenerator::makeInventoryItem(
    uint32_t itemId,
    const ItemTable &itemTable,
    ItemGenerationMode mode)
{
    InventoryItem item = {};
    item.objectDescriptionId = itemId;
    item.quantity = 1;
    const ItemDefinition *pItemDefinition = itemTable.get(itemId);

    if (pItemDefinition != nullptr)
    {
        item.width = std::max<uint8_t>(1, pItemDefinition->inventoryWidth);
        item.height = std::max<uint8_t>(1, pItemDefinition->inventoryHeight);
        item.identified = defaultIdentifiedState(*pItemDefinition, mode);
        item.rarity = pItemDefinition->rarity;
        initializeWandCharges(item, *pItemDefinition, 0);

        if (ItemRuntime::isRareItem(*pItemDefinition))
        {
            item.artifactId = static_cast<uint16_t>(std::min<uint32_t>(pItemDefinition->itemId, 0xFFFFu));
        }
    }

    return item;
}

std::optional<InventoryItem> ItemGenerator::generateRandomInventoryItem(
    const ItemTable &itemTable,
    const StandardItemEnchantTable &standardItemEnchantTable,
    const SpecialItemEnchantTable &specialItemEnchantTable,
    const ItemGenerationRequest &request,
    Party *pParty,
    std::mt19937 &rng,
    const std::function<bool(const ItemDefinition &)> &filter)
{
    if (request.rareItemsOnly)
    {
        if (!shouldTryGenerateRareItem(request))
        {
            return std::nullopt;
        }

        const std::optional<uint32_t> rareItemId = chooseRandomRareItem(itemTable, pParty, rng, filter);

        if (!rareItemId)
        {
            return std::nullopt;
        }

        InventoryItem item = makeInventoryItem(*rareItemId, itemTable, request.mode);

        if (pParty != nullptr)
        {
            pParty->markArtifactItemFound(*rareItemId);
        }

        return item;
    }

    if (shouldTryGenerateRareItem(request) && std::uniform_int_distribution<int>(0, 99)(rng) < 5)
    {
        const std::optional<uint32_t> rareItemId = chooseRandomRareItem(itemTable, pParty, rng, filter);

        if (rareItemId)
        {
            InventoryItem item = makeInventoryItem(*rareItemId, itemTable, request.mode);

            if (pParty != nullptr)
            {
                pParty->markArtifactItemFound(*rareItemId);
            }

            return item;
        }
    }

    return generateOrdinaryInventoryItem(
        itemTable, standardItemEnchantTable, specialItemEnchantTable, request, rng, filter);
}

std::optional<uint32_t> ItemGenerator::chooseRandomBaseItem(
    const ItemTable &itemTable,
    int treasureLevel,
    std::mt19937 &rng,
    const std::function<bool(const ItemDefinition &)> &filter)
{
    return chooseRandomBaseItemImpl(itemTable, treasureLevel, rng, filter);
}

std::optional<FixedItemGenerationResult> ItemGenerator::generateFixedInventoryItem(
    const ItemTable &itemTable,
    const StandardItemEnchantTable &standardItemEnchantTable,
    const SpecialItemEnchantTable &specialItemEnchantTable,
    const ItemGenerationRequest &request,
    const FixedItemGenerationCursor &before,
    std::string &error,
    uint32_t treasureType)
{
    error.clear();
    if (request.mode != ItemGenerationMode::Generic || request.treasureLevel < 1 || request.treasureLevel > 6
        || request.allowRareItems || request.rareItemsOnly)
    {
        error = "Fixed item generation V1 requires Generic, strength1..6 and both rare flags disabled";
        return std::nullopt;
    }
    if (treasureType != 0 && treasureType != 16 && treasureType != 20 && treasureType != 21
        && treasureType != 22 && treasureType != 40 && treasureType != 43 && treasureType != 45
        && !(treasureType >= 6 && treasureType <= 12) && !(treasureType >= 23 && treasureType <= 33))
    {
        error = "Fixed item generation: unsupported legacy treasure type";
        return std::nullopt;
    }
    if (standardItemEnchantTable.entries().empty() || specialItemEnchantTable.entries().empty()
        || standardItemEnchantTable.entries().size() > std::numeric_limits<uint16_t>::max()
        || specialItemEnchantTable.entries().size() > std::numeric_limits<uint16_t>::max())
    {
        error = "Fixed item generation: missing or oversized enchant table";
        return std::nullopt;
    }
    try
    {
        FixedItemGenerationRandom rng(before);
        std::function<bool(const ItemDefinition &)> filter;
        if (treasureType != 0)
        {
            filter = [treasureType](const ItemDefinition &entry)
            {
                return matchesLegacyTreasureType(entry, treasureType);
            };
        }
        const std::optional<InventoryItem> item = generateOrdinaryInventoryItem(
            itemTable, standardItemEnchantTable, specialItemEnchantTable, request, rng, filter);
        if (!item)
        {
            error = "Fixed item generation: no eligible base item";
            return std::nullopt;
        }
        return FixedItemGenerationResult{*item, rng.cursor()};
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return std::nullopt;
    }
}
}
