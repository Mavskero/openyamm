#include "game/ui/CharacterStatLayout.h"

#include "game/tables/CharacterInspectTable.h"

namespace OpenYAMM::Game
{
namespace
{
constexpr std::array<CharacterStatRowDefinition, 26> CharacterStatRows = {{
    {"SkillPoints", "characterstatsskillpointstextlabel", "characterstatsskillpointsvalue"},
    {"Might", "characterstatmightlabel", "characterstatmightvalue"},
    {"Intellect", "characterstatintellectlabel", "characterstatintellectvalue"},
    {"Personality", "characterstatpersonalitylabel", "characterstatpersonalityvalue"},
    {"Endurance", "characterstatendurancelabel", "characterstatendurancevalue"},
    {"Accuracy", "characterstataccuracylabel", "characterstataccuracyvalue"},
    {"Speed", "characterstatspeedlabel", "characterstatspeedvalue"},
    {"Luck", "characterstatlucklabel", "characterstatluckvalue"},
    {"HitPoints", "characterstathitpointslabel", "characterstathitpointsvalue"},
    {"SpellPoints", "characterstatspellpointslabel", "characterstatspellpointsvalue"},
    {"ArmorClass", "characterstatarmorclasslabel", "characterstatarmorclassvalue"},
    {"Condition", "characterstatconditionlabel", "characterstatconditionvalue"},
    {"QuickSpell", "characterstatquickspelllabel", "characterstatquickspellvalue"},
    {"Age", "characterstatagelabel", "characterstatagevalue"},
    {"Level", "characterstatlevellabel", "characterstatlevelvalue"},
    {"Experience", "characterstatexperiencelabel", "characterstatexperiencevalue"},
    {"Attack", "characterstatattacklabel", "characterstatattackvalue"},
    {"MeleeDamage", "characterstatmeleedamagelabel", "characterstatmeleedamagevalue"},
    {"Shoot", "characterstatshootlabel", "characterstatshootvalue"},
    {"RangedDamage", "characterstatrangeddamagelabel", "characterstatrangeddamagevalue"},
    {"FireResistance", "characterstatfireresistancelabel", "characterstatfireresistancevalue"},
    {"AirResistance", "characterstatairresistancelabel", "characterstatairresistancevalue"},
    {"WaterResistance", "characterstatwaterresistancelabel", "characterstatwaterresistancevalue"},
    {"EarthResistance", "characterstatearthresistancelabel", "characterstatearthresistancevalue"},
    {"MindResistance", "characterstatmindresistancelabel", "characterstatmindresistancevalue"},
    {"BodyResistance", "characterstatbodyresistancelabel", "characterstatbodyresistancevalue"},
}};

}

const std::array<CharacterStatRowDefinition, 26> &characterStatRows()
{
    return CharacterStatRows;
}

std::optional<std::string> characterStatDisplayLabel(
    const CharacterInspectTable *pTable,
    std::string_view normalizedLayoutId)
{
    if (pTable == nullptr)
    {
        return std::nullopt;
    }
    if (normalizedLayoutId == "characterskillsskillpointstextlabel")
    {
        normalizedLayoutId = "characterstatsskillpointstextlabel";
    }
    for (const CharacterStatRowDefinition &row : CharacterStatRows)
    {
        if (normalizedLayoutId != row.pLabelId)
        {
            continue;
        }
        const StatInspectEntry *pEntry = pTable->getStat(row.pStatName);
        if (pEntry == nullptr || pEntry->name.empty())
        {
            return std::nullopt;
        }
        return pEntry->name + (normalizedLayoutId == "characterstatsskillpointstextlabel" ? ":" : "");
    }
    return std::nullopt;
}
}
