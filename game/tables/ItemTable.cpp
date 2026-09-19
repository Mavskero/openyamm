#include "game/tables/ItemTable.h"
#include "engine/ImageAssetLoader.h"
#include "game/party/SkillData.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
std::string trimCopy(const std::string &value)
{
    size_t begin = 0;

    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    size_t end = value.size();

    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string getCell(const std::vector<std::string> &row, size_t index)
{
    if (index >= row.size())
    {
        return {};
    }

    return trimCopy(row[index]);
}

std::string canonicalBodyEquipmentType(const std::string &value)
{
    const std::string key = toLowerCopy(value);
    for (const char *pType : {"Armor", "Helm", "Belt", "Boots", "Cloak"})
    {
        if (key == toLowerCopy(pType))
        {
            return pType;
        }
    }
    return value;
}

std::vector<std::string> splitPipeSeparated(const std::string &value)
{
    std::vector<std::string> result;
    size_t begin = 0;

    while (begin <= value.size())
    {
        const size_t end = value.find('|', begin);
        const std::string token = trimCopy(value.substr(begin, end - begin));

        if (!token.empty())
        {
            result.push_back(token);
        }

        if (end == std::string::npos)
        {
            break;
        }

        begin = end + 1;
    }

    return result;
}

int parseInt(const std::string &value)
{
    if (value.empty())
    {
        return 0;
    }

    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return 0;
    }
}

float parseFloat(const std::string &value)
{
    if (value.empty())
    {
        return 0.0f;
    }

    try
    {
        return std::stof(value);
    }
    catch (...)
    {
        return 0.0f;
    }
}

bool isSupportedContentEffectFlag(const std::string &flag)
{
    static const std::unordered_set<std::string> SupportedFlags = {
        "AllConditionImmunity",
        "Amelioration",
        "BreakAfterFirstAttack",
        "BreakChance2SelfDamage",
        "BypassArmorChance20",
        "CarriedPartyEffect",
        "DisableDivineIntervention",
        "DiseaseWeaponDamage",
        "EncounterChancePlus15Percent",
        "EnrageOnHit5Grandmaster",
        "Everlasting",
        "ExperienceGainMinus15Percent",
        "FeatherFalling",
        "FearOnHit10",
        "FearImmunity",
        "HalveTargetArmor",
        "HalfMissileDamage",
        "KillOnHit10",
        "MagicArmor20",
        "MeleeArmor40",
        "MissileArmor30",
        "PartyEnduranceMinus40",
        "PerfectAccuracy",
        "ParalysisImmunity",
        "PhysicalDamageReduction3Percent",
        "PoisonImmunity",
        "PoisonOnHit40Grandmaster",
        "PoisonWeaponDamage",
        "RangedArmorMinus10",
        "ReelOnHit",
        "SelfKnockoutOnFailedStun70",
        "SleepImmunity",
        "StoneImmunity",
        "StunOnHit5",
        "TripleProjectile",
        "Vampiric",
        "WeeklyGold1000",
        "WeeklyHealthLossChance10",
        "WeeklyInsanityChance10",
        "WaterWalking",
        "DiseaseImmunity",
    };
    return SupportedFlags.contains(flag);
}

bool parseContentEffect(
    const std::vector<std::string> &row,
    size_t startColumn,
    ItemContentEffect &effect,
    std::string &errorMessage)
{
    effect = {};
    effect.might = parseInt(getCell(row, startColumn));
    effect.intellect = parseInt(getCell(row, startColumn + 1));
    effect.personality = parseInt(getCell(row, startColumn + 2));
    effect.endurance = parseInt(getCell(row, startColumn + 3));
    effect.speed = parseInt(getCell(row, startColumn + 4));
    effect.accuracy = parseInt(getCell(row, startColumn + 5));
    effect.luck = parseInt(getCell(row, startColumn + 6));
    effect.maxHealth = parseInt(getCell(row, startColumn + 7));
    effect.maxSpellPoints = parseInt(getCell(row, startColumn + 8));
    effect.armorClass = parseInt(getCell(row, startColumn + 9));
    effect.meleeAttack = parseInt(getCell(row, startColumn + 10));
    effect.rangedAttack = parseInt(getCell(row, startColumn + 11));
    effect.meleeDamage = parseInt(getCell(row, startColumn + 12));
    effect.rangedDamage = parseInt(getCell(row, startColumn + 13));
    effect.allResistances = parseInt(getCell(row, startColumn + 14));
    effect.fireResistance = parseInt(getCell(row, startColumn + 15));
    effect.airResistance = parseInt(getCell(row, startColumn + 16));
    effect.waterResistance = parseInt(getCell(row, startColumn + 17));
    effect.earthResistance = parseInt(getCell(row, startColumn + 18));
    effect.mindResistance = parseInt(getCell(row, startColumn + 19));
    effect.bodyResistance = parseInt(getCell(row, startColumn + 20));
    effect.spiritResistance = parseInt(getCell(row, startColumn + 21));

    for (const std::string &token : splitPipeSeparated(getCell(row, startColumn + 22)))
    {
        const size_t separator = token.rfind(':');

        if (separator == std::string::npos)
        {
            errorMessage = "invalid skill bonus token '" + token + "'";
            return false;
        }

        const std::string skillName = canonicalSkillName(token.substr(0, separator));
        const int amount = parseInt(trimCopy(token.substr(separator + 1)));

        if (skillName.empty() || amount == 0)
        {
            errorMessage = "invalid skill bonus token '" + token + "'";
            return false;
        }

        effect.skillBonuses[skillName] += amount;
    }

    effect.weaponDamage = parseInt(getCell(row, startColumn + 23));
    effect.recoveryReductionTicks = parseInt(getCell(row, startColumn + 24));
    effect.healthRegenPerSecond = parseFloat(getCell(row, startColumn + 25));
    effect.spellRegenPerSecond = parseFloat(getCell(row, startColumn + 26));
    effect.flags = splitPipeSeparated(getCell(row, startColumn + 27));

    for (const std::string &flag : effect.flags)
    {
        if (!isSupportedContentEffectFlag(flag))
        {
            errorMessage = "unsupported content effect flag '" + flag + "'";
            return false;
        }
    }
    return true;
}

uint32_t parseUint32(const std::string &value)
{
    if (value.empty())
    {
        return 0;
    }

    try
    {
        return static_cast<uint32_t>(std::stoul(value));
    }
    catch (...)
    {
        return 0;
    }
}

ItemRarity parseItemRarity(const std::string &value)
{
    const std::string normalized = toLowerCopy(trimCopy(value));

    if (normalized == "artifact")
    {
        return ItemRarity::Artifact;
    }

    if (normalized == "relic")
    {
        return ItemRarity::Relic;
    }

    if (normalized == "special")
    {
        return ItemRarity::Special;
    }

    return ItemRarity::Common;
}

std::optional<uint32_t> parseItemId(const std::vector<std::string> &row)
{
    const std::string rawItemId = getCell(row, 0);

    if (rawItemId.empty())
    {
        return std::nullopt;
    }

    for (char character : rawItemId)
    {
        if (!std::isdigit(static_cast<unsigned char>(character)))
        {
            return std::nullopt;
        }
    }

    try
    {
        return static_cast<uint32_t>(std::stoul(rawItemId));
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool readInt32LittleEndian(const std::vector<uint8_t> &bytes, size_t offset, int32_t &value)
{
    if (offset + sizeof(value) > bytes.size())
    {
        return false;
    }

    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return true;
}

bool loadBitmapDimensions(
    const Engine::AssetFileSystem &assetFileSystem,
    const std::unordered_map<std::string, std::string> &iconPaths,
    const std::string &iconName,
    int &width,
    int &height,
    std::string &virtualPath
)
{
    if (iconName.empty())
    {
        return false;
    }

    const auto foundPath = iconPaths.find(toLowerCopy(iconName));

    if (foundPath == iconPaths.end())
    {
        return false;
    }

    const std::optional<std::vector<uint8_t>> imageBytes = assetFileSystem.readBinaryFile(foundPath->second);

    if (!imageBytes || imageBytes->empty())
    {
        return false;
    }

    int32_t bitmapWidth = 0;
    int32_t bitmapHeight = 0;

    if (imageBytes->size() >= 26
        && (*imageBytes)[0] == 'B'
        && (*imageBytes)[1] == 'M'
        && readInt32LittleEndian(*imageBytes, 18, bitmapWidth)
        && readInt32LittleEndian(*imageBytes, 22, bitmapHeight)
        && bitmapWidth > 0
        && bitmapHeight != 0)
    {
        width = bitmapWidth;
        height = std::abs(bitmapHeight);
    }
    else
    {
        const std::optional<Engine::ImagePixelsBgra> decodedImage =
            Engine::decodeImagePixelsBgra(*imageBytes, foundPath->second);

        if (!decodedImage || decodedImage->width <= 0 || decodedImage->height <= 0)
        {
            return false;
        }

        width = decodedImage->width;
        height = decodedImage->height;
    }

    width = Engine::scalePhysicalPixelsToLogical(width, assetFileSystem.getAssetScaleTier());
    height = Engine::scalePhysicalPixelsToLogical(height, assetFileSystem.getAssetScaleTier());
    virtualPath = foundPath->second;
    return true;
}

uint8_t inventorySlotsFromPixels(int pixelSize)
{
    const int clampedPixels = std::max(14, pixelSize);
    const int slots = 1 + (clampedPixels - 14) / 32;
    return static_cast<uint8_t>(std::clamp(slots, 1, 14));
}

std::unordered_map<std::string, std::string> buildIconPathMap(const Engine::AssetFileSystem &assetFileSystem)
{
    std::unordered_map<std::string, std::string> iconPaths;
    const std::vector<std::string> entries = assetFileSystem.enumerate("Data/icons");

    for (const std::string &entry : entries)
    {
        const std::string lowerEntry = toLowerCopy(entry);

        if (lowerEntry.size() <= 4)
        {
            continue;
        }

        if (lowerEntry.substr(lowerEntry.size() - 4) == ".png")
        {
            const std::string iconStem = lowerEntry.substr(0, lowerEntry.size() - 4);
            iconPaths[iconStem] = "Data/icons/" + entry;
        }
        else if (lowerEntry.substr(lowerEntry.size() - 4) == ".bmp")
        {
            const std::string iconStem = lowerEntry.substr(0, lowerEntry.size() - 4);

            if (!iconPaths.contains(iconStem))
            {
                iconPaths[iconStem] = "Data/icons/" + entry;
            }
        }
    }

    return iconPaths;
}
}

bool ItemTable::load(
    const Engine::AssetFileSystem &assetFileSystem,
    const std::vector<std::vector<std::string>> &itemRows,
    const std::vector<std::vector<std::string>> &randomItemRows
)
{
    const std::unordered_map<std::string, std::string> iconPaths = buildIconPathMap(assetFileSystem);
    uint32_t maxItemId = 0;

    for (const std::vector<std::string> &row : itemRows)
    {
        const std::optional<uint32_t> itemId = parseItemId(row);

        if (!itemId)
        {
            continue;
        }

        maxItemId = std::max(maxItemId, *itemId);
    }

    if (maxItemId == 0)
    {
        return false;
    }

    m_entries.clear();
    m_entries.resize(static_cast<size_t>(maxItemId) + 1);

    for (size_t itemIndex = 0; itemIndex < m_entries.size(); ++itemIndex)
    {
        m_entries[itemIndex].itemId = static_cast<uint32_t>(itemIndex);
    }

    for (const std::vector<std::string> &row : itemRows)
    {
        const std::optional<uint32_t> itemId = parseItemId(row);

        if (!itemId || *itemId >= m_entries.size())
        {
            continue;
        }

        ItemDefinition &entry = m_entries[*itemId];
        entry.itemId = *itemId;
        entry.packageId = getCell(row, 17);
        entry.canonicalId = getCell(row, 18);
        entry.sourceItemId = parseUint32(getCell(row, 19));

        if (entry.packageId.empty())
        {
            entry.packageId = "engine";
        }

        if (entry.canonicalId.empty())
        {
            entry.canonicalId = "engine:item/" + std::to_string(*itemId);
        }

        if (entry.sourceItemId == 0)
        {
            entry.sourceItemId = *itemId;
        }

        entry.iconName = getCell(row, 1);
        entry.name = getCell(row, 2);
        entry.value = parseInt(getCell(row, 3));
        entry.equipStat = canonicalBodyEquipmentType(getCell(row, 4));
        entry.skillGroup = getCell(row, 5);
        entry.mod1 = getCell(row, 6);
        entry.mod2 = getCell(row, 7);
        entry.material = parseInt(getCell(row, 8));
        entry.materialName = getCell(row, 8);
        entry.rarity = parseItemRarity(entry.materialName);
        entry.idRepSt = getCell(row, 9);
        entry.identifyRepairDifficulty = parseInt(entry.idRepSt);
        entry.unidentifiedName = getCell(row, 10);
        entry.spriteIndex = static_cast<uint16_t>(std::max(0, parseInt(getCell(row, 11))));
        entry.varA = parseInt(getCell(row, 12));
        entry.varB = parseInt(getCell(row, 13));
        entry.equipX = parseInt(getCell(row, 14));
        entry.equipY = parseInt(getCell(row, 15));
        entry.notes = getCell(row, 16);
        entry.contentFlags = splitPipeSeparated(getCell(row, 20));
        entry.normalizedTier = static_cast<uint8_t>(std::clamp(parseInt(getCell(row, 21)), 0, 7));
        entry.useAction = getCell(row, 22);
        entry.useTarget = getCell(row, 23);
        entry.setId = getCell(row, 24);
        entry.effectProfile = getCell(row, 25);
        entry.allowedClassRoots = splitPipeSeparated(getCell(row, 26));
        entry.requiredMight = std::max(0, parseInt(getCell(row, 27)));
        entry.requiredAccuracy = std::max(0, parseInt(getCell(row, 28)));
        const int authoredInventoryWidth = parseInt(getCell(row, 29));
        const int authoredInventoryHeight = parseInt(getCell(row, 30));
        entry.visualStatus = getCell(row, 31);
        entry.sourceEquipType = getCell(row, 34);
        entry.sourceSkill = getCell(row, 35);
        entry.sourceIconPath = getCell(row, 36);
        entry.importStatus = getCell(row, 43);
        entry.balanceProfile = getCell(row, 44);

        int iconWidth = 0;
        int iconHeight = 0;

        if (loadBitmapDimensions(assetFileSystem, iconPaths, entry.iconName, iconWidth, iconHeight, entry.iconVirtualPath))
        {
            entry.inventoryWidth = inventorySlotsFromPixels(iconWidth);
            entry.inventoryHeight = inventorySlotsFromPixels(iconHeight);
        }
        else
        {
            entry.inventoryWidth = 1;
            entry.inventoryHeight = 1;
        }

        if (authoredInventoryWidth > 0 && authoredInventoryHeight > 0)
        {
            entry.inventoryWidth = static_cast<uint8_t>(std::clamp(authoredInventoryWidth, 1, 14));
            entry.inventoryHeight = static_cast<uint8_t>(std::clamp(authoredInventoryHeight, 1, 9));
        }
    }

    for (const std::vector<std::string> &row : randomItemRows)
    {
        const std::optional<uint32_t> itemId = parseItemId(row);

        if (!itemId || *itemId >= m_entries.size())
        {
            continue;
        }

        ItemDefinition &entry = m_entries[*itemId];

        for (size_t weightIndex = 0; weightIndex < entry.randomTreasureWeights.size(); ++weightIndex)
        {
            entry.randomTreasureWeights[weightIndex] = parseInt(getCell(row, 2 + weightIndex));
        }
    }

    return true;
}

bool ItemDefinition::hasContentFlag(const std::string &flag) const
{
    const std::string normalizedFlag = toLowerCopy(trimCopy(flag));

    return std::any_of(
        contentFlags.begin(),
        contentFlags.end(),
        [&normalizedFlag](const std::string &candidate)
        {
            return toLowerCopy(candidate) == normalizedFlag;
        });
}

const ItemDefinition *ItemTable::get(uint32_t itemId) const
{
    if (itemId >= m_entries.size())
    {
        return nullptr;
    }

    const ItemDefinition &entry = m_entries[itemId];

    if (entry.itemId == 0 && itemId != 0)
    {
        return nullptr;
    }

    if (itemId != 0 && entry.name.empty() && entry.iconName.empty())
    {
        return nullptr;
    }

    return &entry;
}

const ItemDefinition *ItemTable::findBySpriteIndex(uint16_t spriteIndex) const
{
    if (spriteIndex == 0)
    {
        return nullptr;
    }

    for (const ItemDefinition &entry : m_entries)
    {
        if (entry.itemId != 0 && entry.spriteIndex == spriteIndex)
        {
            return &entry;
        }
    }

    return nullptr;
}

const std::vector<ItemDefinition> &ItemTable::entries() const
{
    return m_entries;
}

bool ItemTable::loadContentEffectRows(
    const std::vector<std::vector<std::string>> &rows,
    std::string &errorMessage)
{
    std::unordered_map<std::string, uint32_t> profileOwners;

    for (const std::vector<std::string> &row : rows)
    {
        const std::optional<uint32_t> itemId = parseItemId(row);

        if (!itemId)
        {
            continue;
        }

        ItemDefinition *pItem = *itemId < m_entries.size() ? &m_entries[*itemId] : nullptr;
        const std::string profile = getCell(row, 1);

        if (pItem == nullptr || get(*itemId) == nullptr || profile.empty() || pItem->effectProfile != profile)
        {
            errorMessage = "item effect row does not match item " + std::to_string(*itemId) + " profile";
            return false;
        }

        if (!profileOwners.emplace(toLowerCopy(profile), *itemId).second)
        {
            errorMessage = "duplicate item effect profile '" + profile + "'";
            return false;
        }

        if (!parseContentEffect(row, 2, pItem->contentEffect, errorMessage))
        {
            errorMessage = "item " + std::to_string(*itemId) + ": " + errorMessage;
            return false;
        }
    }

    return true;
}

bool ItemTable::loadItemSetRows(
    const std::vector<std::vector<std::string>> &rows,
    std::string &errorMessage)
{
    m_setBonuses.clear();
    std::unordered_map<std::string, size_t> setThresholds;

    for (const std::vector<std::string> &row : rows)
    {
        const std::string setId = getCell(row, 0);

        if (setId.empty() || toLowerCopy(setId) == "set_id")
        {
            continue;
        }

        const int requiredPieceCount = parseInt(getCell(row, 1));
        const std::string profile = getCell(row, 2);

        if (requiredPieceCount <= 0 || profile.empty())
        {
            errorMessage = "invalid item set bonus for '" + setId + "'";
            return false;
        }

        const std::string thresholdKey = toLowerCopy(setId) + ":" + std::to_string(requiredPieceCount);

        if (!setThresholds.emplace(thresholdKey, static_cast<size_t>(requiredPieceCount)).second)
        {
            errorMessage = "duplicate item set threshold '" + thresholdKey + "'";
            return false;
        }

        ItemSetBonusDefinition definition = {};
        definition.setId = setId;
        definition.requiredPieceCount = static_cast<size_t>(requiredPieceCount);
        definition.effectProfile = profile;

        if (!parseContentEffect(row, 3, definition.effect, errorMessage))
        {
            errorMessage = "item set '" + setId + "': " + errorMessage;
            return false;
        }

        m_setBonuses.push_back(std::move(definition));
    }

    return true;
}

const std::vector<ItemSetBonusDefinition> &ItemTable::setBonuses() const
{
    return m_setBonuses;
}

bool ItemTable::loadSourceAliasRows(
    const std::vector<std::vector<std::string>> &rows,
    std::string &errorMessage)
{
    m_sourceAliases.clear();

    for (const std::vector<std::string> &row : rows)
    {
        const std::string packageId = toLowerCopy(getCell(row, 0));
        const uint32_t sourceItemId = parseUint32(getCell(row, 1));
        const uint32_t itemId = parseUint32(getCell(row, 2));

        if (packageId.empty() || packageId == "package_id")
        {
            continue;
        }

        const ItemDefinition *pTarget = get(itemId);
        const std::string key = packageId + ":" + std::to_string(sourceItemId);

        if (sourceItemId == 0 || pTarget == nullptr || !m_sourceAliases.emplace(key, itemId).second)
        {
            errorMessage = "invalid or duplicate source item alias '" + key + "'";
            return false;
        }

        if (toLowerCopy(pTarget->packageId) == packageId && pTarget->sourceItemId != sourceItemId)
        {
            errorMessage = "source item alias '" + key + "' does not match its package definition";
            return false;
        }
    }

    return true;
}

const ItemDefinition *ItemTable::resolvePackageSourceItemId(
    const std::string &packageId,
    uint32_t sourceItemId) const
{
    const std::string key = toLowerCopy(trimCopy(packageId)) + ":" + std::to_string(sourceItemId);
    const auto foundAlias = m_sourceAliases.find(key);
    return foundAlias != m_sourceAliases.end() ? get(foundAlias->second) : nullptr;
}
}
