#pragma once

#include "game/party/Party.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class CharacterDollTable;
class ClassMultiplierTable;
class ClassSkillTable;
class ItemTable;
class MergedCharacterSelectionTable;
class MergedClassStartingStatTable;
class MergedNpcNameTable;
class SpecialItemEnchantTable;
class SpellTable;
class StandardItemEnchantTable;

struct SourceMercenaryGenerationTables
{
    const ClassMultiplierTable *pClassMultiplierTable = nullptr;
    const ClassSkillTable *pClassSkillTable = nullptr;
    const CharacterDollTable *pCharacterDollTable = nullptr;
    const MergedCharacterSelectionTable *pCharacterSelectionTable = nullptr;
    const MergedNpcNameTable *pNpcNameTable = nullptr;
    const MergedClassStartingStatTable *pClassStartingStatTable = nullptr;
    const ItemTable *pItemTable = nullptr;
    const StandardItemEnchantTable *pStandardItemEnchantTable = nullptr;
    const SpecialItemEnchantTable *pSpecialItemEnchantTable = nullptr;
    const SpellTable *pSpellTable = nullptr;
};

struct SourceMercenaryGenerationResult
{
    Character character;
    std::vector<uint32_t> portraitHistory; // Original zero-based Face IDs, in source insertion order.
    uint64_t finalRandomState = 0;
    uint64_t randomDrawCount = 0;
};

// Default GenerateMercenary path from the fingerprinted NPCMercenaries.lua, not monthly recruitment.
// baseline is updated field by field: owned memberId and source-unwritten state survive preparation.
// No live Party, external RNG or identity allocation. A failed result publishes no partial state.
// The caller supplies the initial history containing the original leader Face when no history exists.
std::optional<SourceMercenaryGenerationResult> prepareSourceMercenary(
    const Character &baseline, uint32_t rosterId, uint32_t level, uint64_t seed, uint32_t currentSwitch,
    const std::vector<uint32_t> &portraitHistory, const SourceMercenaryGenerationTables &tables, std::string &error);
}
