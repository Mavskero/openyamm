#pragma once

#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct Character;
class CharacterInspectTable;
class UiTextTable;

struct CharacterSkillUiRow
{
    std::string canonicalName;
    std::string label;
    std::string level;
    bool upgradeable = false;
    bool interactive = false;
};

struct CharacterSkillUiData
{
    std::vector<CharacterSkillUiRow> weaponRows;
    std::vector<CharacterSkillUiRow> magicRows;
    std::vector<CharacterSkillUiRow> armorRows;
    std::vector<CharacterSkillUiRow> miscRows;
};

CharacterSkillUiData buildCharacterSkillUiData(
    const Character *pCharacter,
    const CharacterInspectTable *pTable,
    bool allowUpgradeInteraction = true,
    const UiTextTable *pUiTextTable = nullptr);
}
