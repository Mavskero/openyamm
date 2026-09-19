#include "game/ui/CharacterSkillRows.h"

#include "game/party/Party.h"
#include "game/tables/CharacterInspectTable.h"

#include <algorithm>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *WeaponSkillNames[] = {
    "Axe",
    "Bow",
    "Throwing",
    "Dagger",
    "Mace",
    "Spear",
    "Staff",
    "Sword",
    "Unarmed",
    "Blaster",
};

constexpr const char *MagicSkillNames[] = {
    "FireMagic",
    "AirMagic",
    "WaterMagic",
    "EarthMagic",
    "SpiritMagic",
    "MindMagic",
    "BodyMagic",
    "LightMagic",
    "DarkMagic",
};

constexpr const char *ArmorSkillNames[] = {
    "LeatherArmor",
    "ChainArmor",
    "PlateArmor",
    "Shield",
    "Dodging",
};

constexpr const char *MiscSkillNames[] = {
    "Alchemy",
    "Armsmaster",
    "Bodybuilding",
    "IdentifyItem",
    "IdentifyMonster",
    "Learning",
    "DisarmTraps",
    "Meditation",
    "Merchant",
    "Perception",
    "RepairItem",
    "Stealing",
};

CharacterSkillUiRow makeRow(
    const Character &character,
    const CharacterSkill &skill,
    const std::string &canonicalName,
    const CharacterInspectTable *pTable,
    bool allowUpgradeInteraction)
{
    CharacterSkillUiRow row = {};
    row.canonicalName = canonicalName;
    const SkillInspectEntry *pEntry = pTable != nullptr ? pTable->getSkill(canonicalName) : nullptr;
    row.label = pEntry != nullptr && !pEntry->name.empty() ? pEntry->name : displaySkillName(skill.name);
    if (skill.mastery != SkillMastery::None && skill.mastery != SkillMastery::Normal)
    {
        const std::string mastery = masteryDisplayName(skill.mastery);
        row.label += " " + (mastery == "Grandmaster" ? std::string("Grand") : mastery);
    }
    row.level = std::to_string(skill.level);
    row.upgradeable = character.skillPoints > skill.level;
    row.interactive = row.upgradeable && allowUpgradeInteraction;
    return row;
}
}

CharacterSkillUiData buildCharacterSkillUiData(
    const Character *pCharacter,
    const CharacterInspectTable *pTable,
    bool allowUpgradeInteraction)
{
    CharacterSkillUiData data = {};
    if (pCharacter == nullptr)
    {
        return data;
    }
    std::unordered_set<std::string> shownSkillNames;
    const auto appendGroup = [&](std::vector<CharacterSkillUiRow> &rows, const auto &names)
    {
        for (const char *pName : names)
        {
            const CharacterSkill *pSkill = pCharacter->findSkillByCanonicalName(pName);
            if (pSkill != nullptr)
            {
                rows.push_back(makeRow(*pCharacter, *pSkill, pName, pTable, allowUpgradeInteraction));
                shownSkillNames.insert(pName);
            }
        }
    };
    appendGroup(data.weaponRows, WeaponSkillNames);
    appendGroup(data.magicRows, MagicSkillNames);
    appendGroup(data.armorRows, ArmorSkillNames);
    appendGroup(data.miscRows, MiscSkillNames);

    std::vector<CharacterSkillUiRow> extraRows;
    for (const auto &[canonicalName, skill] : pCharacter->skills)
    {
        if (!shownSkillNames.contains(canonicalName))
        {
            extraRows.push_back(makeRow(*pCharacter, skill, canonicalName, pTable, allowUpgradeInteraction));
        }
    }
    // Keep extra-skill ordering stable when display language changes.
    std::sort(extraRows.begin(), extraRows.end(), [](const CharacterSkillUiRow &left, const CharacterSkillUiRow &right)
    {
        return left.canonicalName < right.canonicalName;
    });
    data.miscRows.insert(data.miscRows.end(), extraRows.begin(), extraRows.end());
    return data;
}
}
