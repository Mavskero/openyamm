#include "game/gameplay/SourceMercenaryGenerationRuntime.h"

#include "game/gameplay/GameMechanics.h"
#include "game/items/ItemGenerator.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/SpellTable.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
constexpr size_t SourceSkillCount = 39;
constexpr uint32_t ExtraPotionPlayerBitBase = 200000; // InventoryItemUseRuntime's saved UsedPotions projection.
constexpr std::array<uint32_t, 4> SkillLevels{1, 4, 7, 10};
constexpr std::array<uint32_t, 4> SkillCosts{0, 9, 27, 54};
constexpr std::array<std::string_view, 7> StatNames{
    "Might", "Intellect", "Personality", "Endurance", "Accuracy", "Speed", "Luck"};
constexpr std::array<uint32_t Character::*, 7> PrimaryStats{
    &Character::might, &Character::intellect, &Character::personality, &Character::endurance,
    &Character::accuracy, &Character::speed, &Character::luck};
constexpr std::array<int CharacterStatBonuses::*, 7> PermanentStats{
    &CharacterStatBonuses::might, &CharacterStatBonuses::intellect, &CharacterStatBonuses::personality,
    &CharacterStatBonuses::endurance, &CharacterStatBonuses::accuracy, &CharacterStatBonuses::speed,
    &CharacterStatBonuses::luck};

struct EquipmentTarget
{
    uint32_t CharacterEquipment::*item;
    EquippedItemRuntimeState CharacterEquipmentRuntimeState::*state;
};

constexpr std::array<EquipmentTarget, 16> EquipmentTargets{{
    {&CharacterEquipment::offHand, &CharacterEquipmentRuntimeState::offHand},
    {&CharacterEquipment::mainHand, &CharacterEquipmentRuntimeState::mainHand},
    {&CharacterEquipment::bow, &CharacterEquipmentRuntimeState::bow},
    {&CharacterEquipment::armor, &CharacterEquipmentRuntimeState::armor},
    {&CharacterEquipment::helm, &CharacterEquipmentRuntimeState::helm},
    {&CharacterEquipment::belt, &CharacterEquipmentRuntimeState::belt},
    {&CharacterEquipment::cloak, &CharacterEquipmentRuntimeState::cloak},
    {&CharacterEquipment::gauntlets, &CharacterEquipmentRuntimeState::gauntlets},
    {&CharacterEquipment::boots, &CharacterEquipmentRuntimeState::boots},
    {&CharacterEquipment::amulet, &CharacterEquipmentRuntimeState::amulet},
    {&CharacterEquipment::ring1, &CharacterEquipmentRuntimeState::ring1},
    {&CharacterEquipment::ring2, &CharacterEquipmentRuntimeState::ring2},
    {&CharacterEquipment::ring3, &CharacterEquipmentRuntimeState::ring3},
    {&CharacterEquipment::ring4, &CharacterEquipmentRuntimeState::ring4},
    {&CharacterEquipment::ring5, &CharacterEquipmentRuntimeState::ring5},
    {&CharacterEquipment::ring6, &CharacterEquipmentRuntimeState::ring6},
}};

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::runtime_error(pMessage);
    }
}

std::vector<const ClassMultiplierEntry *> sourceClasses(const ClassMultiplierTable &table)
{
    std::vector<const ClassMultiplierEntry *> result;
    std::set<uint32_t> ids;
    for (const auto &[name, entry] : table.entries())
    {
        if (name.empty() || name == "n/u")
        {
            continue;
        }
        require(entry.classKind != 0 && ids.insert(entry.classId).second,
            "Invalid source mercenary class identity");
        result.push_back(&entry);
    }
    std::sort(result.begin(), result.end(), [](const auto *a, const auto *b) { return a->classId < b->classId; });
    require(!result.empty(), "Missing source mercenary classes");
    return result;
}

const ClassMultiplierEntry *classForId(
    const std::vector<const ClassMultiplierEntry *> &classes, uint32_t id)
{
    const auto found = std::find_if(classes.begin(), classes.end(),
        [id](const auto *pEntry) { return pEntry->classId == id; });
    return found == classes.end() ? nullptr : *found;
}

const CharacterDollEntry &choosePortrait(
    const SourceMercenaryGenerationTables &tables, const ClassMultiplierEntry &selected,
    const std::vector<const ClassMultiplierEntry *> &classes, uint32_t currentSwitch,
    std::vector<uint32_t> &history, FixedItemGenerationRandom &random)
{
    const auto &continents = tables.pCharacterSelectionTable->continents();
    const auto continent = std::find_if(continents.begin(), continents.end(),
        [currentSwitch](const auto &entry) { return entry.id == currentSwitch; });
    require(continent != continents.end(), "Missing source mercenary current-switch portrait exceptions");
    const ClassMultiplierEntry *pBase = nullptr;
    for (const ClassMultiplierEntry *pEntry : classes)
    {
        if (pEntry->classKind == selected.classKind && pEntry->promotionStep == 0)
        {
            require(pBase == nullptr, "Duplicate source mercenary base class");
            pBase = pEntry;
        }
    }
    // Some source kinds begin at Step1; ClassStages[kind][1] is nil and grants no race-based alternatives.
    std::vector<const CharacterDollEntry *> faces;
    for (const auto &[id, entry] : tables.pCharacterDollTable->characters())
    {
        require(id == entry.id && id != 0, "Invalid source mercenary portrait identity");
        if (entry.raceId == 10)
        {
            continue;
        }
        const ClassMultiplierEntry *pDefault = classForId(classes, entry.defaultClassId);
        bool compatible = pDefault != nullptr && pDefault->classKind == selected.classKind;
        if (entry.raceId >= 0)
        {
            const auto *pAllowed = tables.pCharacterSelectionTable->allowedClassesForRaceId(uint32_t(entry.raceId));
            compatible = compatible || (pBase != nullptr && pAllowed != nullptr
                && std::any_of(pAllowed->begin(), pAllowed->end(),
                [pBase](const std::string &name) { return canonicalClassName(name) == pBase->className; }));
        }
        const std::string face = std::to_string(id - 1);
        if (compatible && std::find(continent->portraitExceptions.begin(), continent->portraitExceptions.end(), face)
            == continent->portraitExceptions.end())
        {
            faces.push_back(&entry);
        }
    }
    std::sort(faces.begin(), faces.end(), [](const auto *a, const auto *b) { return a->id < b->id; });
    std::vector<const CharacterDollEntry *> unique;
    for (const auto *pFace : faces)
    {
        if (std::find(history.begin(), history.end(), pFace->id - 1) == history.end())
        {
            unique.push_back(pFace);
        }
    }
    if (unique.empty())
    {
        history.clear();
        unique = faces;
    }
    const CharacterDollEntry *pChosen = unique.empty()
        ? tables.pCharacterDollTable->getCharacter(uint32_t(random.bounded(12)) + 1)
        : unique[size_t(random.bounded(unique.size()))];
    require(pChosen != nullptr && pChosen->raceId >= 0, "Missing source mercenary selected portrait");
    history.push_back(pChosen->id - 1);
    return *pChosen;
}

void setSkill(Character &character, const std::string &name, uint32_t mastery)
{
    character.skills[name] = {name, SkillLevels[mastery - 1], static_cast<SkillMastery>(mastery)};
}

void generateSkills(Character &character, uint32_t level, const ClassSkillTable &table,
    const std::vector<std::string> &names, FixedItemGenerationRandom &random)
{
    character.skills.clear();
    for (const std::string &name : names)
    {
        if (table.getStartingSkillAvailability(character.className, name) == StartingSkillAvailability::HasByDefault)
        {
            setSkill(character, name, 1);
        }
    }
    struct LearnSkill
    {
        size_t index;
        uint32_t mastery;
    };
    std::vector<LearnSkill> remaining;
    const uint32_t maximum = (4 * level + 49) / 50;
    for (size_t index = 0; index < names.size(); ++index)
    {
        const uint32_t cap = uint32_t(table.getClassCap(character.className, names[index]));
        remaining.push_back({index, index == 7 ? 0 : std::min(maximum, cap)});
    }
    std::vector<LearnSkill> order;
    while (!remaining.empty())
    {
        const size_t selected = size_t(random.bounded(remaining.size()));
        order.push_back(remaining[selected]);
        remaining.erase(remaining.begin() + selected);
    }
    uint32_t budget = level * 5 - 5;
    uint32_t baseSkills = 0;
    uint32_t minimum = 1;
    for (LearnSkill skill : order)
    {
        if (skill.mastery == 1 && ++baseSkills >= 4)
        {
            minimum = 2;
        }
        while (skill.mastery >= minimum)
        {
            const uint32_t cost = SkillCosts[skill.mastery - 1];
            if (budget >= cost)
            {
                setSkill(character, names[skill.index], skill.mastery);
                budget -= cost;
                break;
            }
            --skill.mastery;
        }
        if (budget == 0)
        {
            break;
        }
    }
    // The actual MM8 source IDs21/22 are DarkElfAbility/VampireAbility, not Merchant/IdentifyItem.
    for (size_t index : {size_t(22), size_t(21)})
    {
        if (table.getEffectiveCap(character.className, character.raceId, names[index]) != SkillMastery::None
            && character.findSkill(names[index]) == nullptr)
        {
            setSkill(character, names[index], 1);
        }
    }
    character.skillPoints = budget;
}

uint32_t packedSkill(const Character &character, const std::string &name)
{
    const CharacterSkill *pSkill = character.findSkill(name);
    if (pSkill == nullptr || pSkill->level == 0 || pSkill->mastery == SkillMastery::None)
    {
        return 0;
    }
    constexpr std::array<uint32_t, 5> MasteryBits{0, 0, 64, 128, 256};
    return pSkill->level + MasteryBits[uint32_t(pSkill->mastery)];
}

void equip(Character &character, const InventoryItem &item, size_t slot)
{
    const EquipmentTarget &target = EquipmentTargets[slot];
    character.equipment.*target.item = item.objectDescriptionId;
    character.equipmentRuntime.*target.state = {
        item.identified, item.broken, item.stolen, item.standardEnchantId, item.standardEnchantPower,
        item.specialEnchantId, item.artifactId, item.rarity, item.currentCharges, item.maxCharges,
        item.temporaryBonusRemainingSeconds};
}

void generateEquipment(Character &character, uint32_t level, const CharacterDollTypeEntry &doll,
    const SourceMercenaryGenerationTables &tables, const std::vector<std::string> &names,
    FixedItemGenerationRandom &random)
{
    character.inventory.clear();
    character.equipment = {};
    character.equipmentRuntime = {};
    const uint32_t quality = std::min((level + 7) / 8 + uint32_t(random.bounded(2)), 5u);
    const ItemGenerationRequest request{.treasureLevel = int(quality), .mode = ItemGenerationMode::Generic,
        .allowRareItems = false, .rareItemsOnly = false};
    const auto itemByType = [&](uint32_t type, size_t slot)
    {
        std::string error;
        const auto generated = ItemGenerator::generateFixedInventoryItem(*tables.pItemTable,
            *tables.pStandardItemEnchantTable, *tables.pSpecialItemEnchantTable, request, random.cursor(), error, type);
        require(generated.has_value(), error.c_str());
        random = FixedItemGenerationRandom(generated->after);
        InventoryItem item = generated->item;
        item.identified = true;
        equip(character, item, slot);
    };
    constexpr std::array<uint32_t, 12> TypesBySkill{30, 23, 24, 25, 26, 27, 28, 11, 5, 31, 32, 33};
    const auto bySkills = [&](std::initializer_list<size_t> skills, size_t slot)
    {
        std::optional<size_t> selected;
        uint32_t highest = 0;
        for (size_t skill : skills)
        {
            const uint32_t packed = packedSkill(character, names[skill]);
            if (packed > highest)
            {
                highest = packed;
                selected = skill;
            }
        }
        if (selected)
        {
            itemByType(TypesBySkill[*selected], slot);
        }
    };
    if (doll.canEquipWeapon)
    {
        bySkills({0, 1, 2, 3, 4, 6}, 1);
    }
    if (doll.canEquipBow)
    {
        bySkills({5}, 2);
    }
    if (doll.canEquipArmor)
    {
        bySkills({9, 10, 11}, 3);
    }
    std::vector<uint32_t> apparel{6, 7, 8, 9, 10, 11, 12};
    constexpr std::array<size_t, 7> Slots{4, 5, 6, 7, 8, 10, 9};
    for (uint32_t index = 0; index < quality; ++index)
    {
        const size_t selected = size_t(random.bounded(apparel.size()));
        const uint32_t type = apparel[selected];
        const bool supported = type == 6 ? doll.canEquipHelm : type == 7 ? doll.canEquipBelt
            : type == 8 ? doll.canEquipCloak : type == 10 ? doll.canEquipBoots : true;
        if (supported)
        {
            itemByType(type, Slots[type - 6]);
        }
        apparel.erase(apparel.begin() + selected);
    }
}

void generateStats(Character &character, uint32_t level, const ClassMultiplierEntry &selected,
    const SourceMercenaryGenerationTables &tables, FixedItemGenerationRandom &random)
{
    std::array<const MergedClassStartingStatEntry *, 7> stats{};
    for (const MergedClassStartingStatEntry &entry : tables.pClassStartingStatTable->entries())
    {
        if (!entry.raceId || *entry.raceId != character.raceId)
        {
            continue;
        }
        const auto found = std::find(StatNames.begin(), StatNames.end(), entry.statName);
        if (found == StatNames.end())
        {
            continue;
        }
        const size_t index = size_t(found - StatNames.begin());
        require(stats[index] == nullptr && entry.addDenominator != 0, "Invalid source mercenary stat identity");
        stats[index] = &entry;
    }
    for (size_t index = 0; index < stats.size(); ++index)
    {
        require(stats[index] != nullptr, "Missing source mercenary race starting stat");
        const auto &stat = *stats[index];
        // DataTablesSupport parses "+ add" A/S as two struct fields Add=A, Spend=S, not a ratio.
        const uint64_t value = stat.addNumerator == 1
            ? (uint64_t(stat.baseValue) + stat.maxValue + 1) / 2
            : stat.addNumerator < 1 ? stat.baseValue : stat.maxValue;
        require(value <= std::numeric_limits<uint32_t>::max(), "Source mercenary starting stat overflow");
        character.*PrimaryStats[index] = uint32_t(value);
        character.permanentBonuses.*PermanentStats[index] = 0;
    }
    const auto adjust = [&](size_t index, bool add)
    {
        const auto &stat = *stats[index];
        const int64_t amount = int64_t(stat.addNumerator) * 2;
        const int64_t value = int64_t(character.*PrimaryStats[index]) + (add ? amount : -amount);
        require(value >= 0 && value <= std::numeric_limits<uint32_t>::max(), "Source mercenary SP stat overflow");
        character.*PrimaryStats[index] = uint32_t(value);
    };
    // Source has `or SPStats == 0`, an unassigned global. Mixed mode does not enter either branch.
    if (selected.manaMode == ClassManaMode::Intellect)
    {
        adjust(2, false);
        adjust(3, true);
    }
    if (selected.manaMode == ClassManaMode::Personality)
    {
        adjust(1, false);
        adjust(5, true);
    }
    int64_t pool = (uint64_t(level) * 3 + 1) / 2;
    while (pool > 0)
    {
        for (const auto stat : PrimaryStats)
        {
            const uint32_t bonus = uint32_t(random.bounded(5));
            require(character.*stat <= std::numeric_limits<uint32_t>::max() - bonus,
                "Source mercenary bonus stat overflow");
            character.*stat += bonus;
            pool -= bonus; // The original completes all seven draws even after exhausting this round's pool.
        }
    }
    character.ageModifier = 0;
    // Only native potion-use markers are cleared; unrelated PlayerBits remain untouched.
    for (const ItemDefinition &item : tables.pItemTable->entries())
    {
        if (item.equipStat == "Bottle"
            && item.itemId <= std::numeric_limits<uint32_t>::max() - ExtraPotionPlayerBitBase)
        {
            character.playerBits.erase(ExtraPotionPlayerBitBase + item.itemId);
        }
    }
    for (uint32_t item = 264; item <= 270; ++item)
    {
        character.playerBits.erase(ExtraPotionPlayerBitBase + item);
    }
}

void generateSpells(Character &character, const std::vector<std::string> &names, const SpellTable &table)
{
    character.knownSpellIds.clear();
    for (uint32_t school = 0; school < 12; ++school)
    {
        const CharacterSkill *pSkill = character.findSkill(names[school + 12]);
        if (pSkill == nullptr)
        {
            continue;
        }
        const uint32_t count = school < 9 ? pSkill->level : uint32_t(pSkill->mastery);
        for (uint32_t offset = 0; offset < count; ++offset)
        {
            const uint32_t id = school * 11 + offset + 1;
            require(table.findById(int(id)) != nullptr, "Missing source mercenary granted spell");
            character.knownSpellIds.insert(id);
        }
    }
}
}

std::optional<SourceMercenaryGenerationResult> prepareSourceMercenary(
    const Character &baseline, uint32_t rosterId, uint32_t level, uint64_t seed, uint32_t currentSwitch,
    const std::vector<uint32_t> &portraitHistory, const SourceMercenaryGenerationTables &tables, std::string &error)
{
    error.clear();
    try
    {
        require(rosterId >= 38 && rosterId <= 41 && baseline.rosterId == rosterId && level > 0,
            "Invalid source mercenary reserve or level");
        const uint64_t levelProduct = uint64_t(level) * (level - 1);
        require(levelProduct <= std::numeric_limits<uint32_t>::max() / 500,
            "Source mercenary experience overflow");
        const uint64_t experience = levelProduct * 500;
        require(tables.pClassMultiplierTable && tables.pClassSkillTable && tables.pCharacterDollTable
            && tables.pCharacterSelectionTable && tables.pNpcNameTable && tables.pClassStartingStatTable
            && tables.pItemTable && tables.pStandardItemEnchantTable && tables.pSpecialItemEnchantTable
            && tables.pSpellTable, "Missing source mercenary generation table");
        std::vector<std::string> names = allCanonicalSkillNames();
        require(names.size() >= SourceSkillCount, "Missing source mercenary skill identity");
        names.resize(SourceSkillCount);
        const auto classes = sourceClasses(*tables.pClassMultiplierTable);
        FixedItemGenerationRandom random({.state = seed});
        const ClassMultiplierEntry &selected = *classes[size_t(random.bounded(classes.size()))];
        require(tables.pClassSkillTable->hasClass(selected.className), "Missing source mercenary class skills");
        SourceMercenaryGenerationResult result;
        result.character = baseline;
        result.portraitHistory = portraitHistory;
        const CharacterDollEntry &portrait = choosePortrait(
            tables, selected, classes, currentSwitch, result.portraitHistory, random);
        const CharacterDollTypeEntry *pDoll = tables.pCharacterDollTable->getDollType(portrait.dollTypeId);
        require(pDoll != nullptr, "Missing source mercenary selected doll type");
        const auto &pool = portrait.defaultSex == 0 ? tables.pNpcNameTable->maleNames()
            : tables.pNpcNameTable->femaleNames();
        require(!pool.empty(), "Missing source mercenary names");
        Character &character = result.character;
        character.characterDataId = portrait.id;
        character.portraitPictureId = portrait.id - 1;
        character.portraitTextureName = portrait.facePicturesPrefix + "01";
        if (portrait.facePicturesPrefix.empty())
        {
            char texture[24]{};
            std::snprintf(texture, sizeof(texture), "PC%02u-01", portrait.id);
            character.portraitTextureName = texture;
        }
        character.voiceId = int32_t(portrait.defaultVoiceId);
        character.sexId = portrait.defaultSex;
        character.raceId = uint32_t(portrait.raceId);
        character.className = selected.className;
        character.role = selected.className;
        character.level = level;
        character.experience = uint32_t(experience);
        character.name = pool[size_t(random.bounded(pool.size()))];
        generateSkills(character, level, *tables.pClassSkillTable, names, random);
        generateEquipment(character, level, *pDoll, tables, names, random);
        generateStats(character, level, selected, tables, random);
        generateSpells(character, names, *tables.pSpellTable);
        character.baseResistances = {};
        character.permanentBonuses.resistances = {};
        if (selected.classId == 45)
        {
            character.baseResistances.mind = 65000;
            character.baseResistances.body = 65000;
        }
        if (selected.classId == 40 || selected.classId == 41)
        {
            character.baseResistances.mind = 65000;
        }
        character.awards.clear();
        character.birthYear = uint32_t(random.intRange(1132, 1154));
        character.biography = character.name + " - "
            + (selected.displayName.empty() ? selected.className : selected.displayName);
        GameMechanics::refreshCharacterBaseResources(character, true, tables.pClassMultiplierTable);
        character.conditions.reset();
        character.conditionStartGameMinutes = {};
        result.finalRandomState = random.cursor().state;
        result.randomDrawCount = random.cursor().rawDrawCount;
        return result;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return std::nullopt;
    }
}
}
