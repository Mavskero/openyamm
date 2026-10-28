#pragma once

#include "game/events/EventRuntime.h"
#include "game/party/Party.h"
#include "game/tables/MapStats.h"

#include <cstdint>
#include <optional>

namespace OpenYAMM::Game
{
class CharacterDollTable;
class ClassMultiplierTable;
class ClassSkillTable;
class HouseTable;
class ItemTable;
class MergedCharacterSelectionTable;
class MergedNpcNameTable;
class RaceStartingStatsTable;
class StandardItemEnchantTable;
class SpecialItemEnchantTable;
class SpellTable;

struct MercenaryRecruitmentTables
{
    const HouseTable *pHouseTable = nullptr;
    const MergedNpcNameTable *pNpcNameTable = nullptr;
    const MergedCharacterSelectionTable *pCharacterSelectionTable = nullptr;
    const CharacterDollTable *pCharacterDollTable = nullptr;
    const ClassSkillTable *pClassSkillTable = nullptr;
    const ClassMultiplierTable *pClassMultiplierTable = nullptr;
    const RaceStartingStatsTable *pRaceStartingStatsTable = nullptr;
    const ItemTable *pItemTable = nullptr;
    const StandardItemEnchantTable *pStandardItemEnchantTable = nullptr;
    const SpecialItemEnchantTable *pSpecialItemEnchantTable = nullptr;
    const SpellTable *pSpellTable = nullptr;
};

bool isGeneratedMercenaryNpcId(uint32_t npcId);
uint32_t generatedMercenaryRosterIdForNpcId(uint32_t npcId);
uint32_t generatedMercenaryNpcIdForRosterId(uint32_t rosterId);
// Explicit source releases permanently transfer these slots from their original templates to the random pool.
bool isReusableMercenaryRosterId(const Party &party, uint32_t rosterId);
bool isReusableMercenaryNpcId(const Party &party, uint32_t npcId);
bool isAvailableGeneratedMercenaryRecruit(
    const Party &party,
    const EventRuntimeState &runtimeState,
    uint32_t npcId);
std::optional<uint32_t> findMercenaryRecruitmentHouseId(
    const MapStatsEntry &map,
    const HouseTable &houseTable);
void migrateLegacyMercenaryRefillState(EventRuntimeState &runtimeState, float savedGameMinutes);
bool refreshMercenaryRecruitmentForCurrentMap(
    const MapStatsEntry &map,
    Party &party,
    EventRuntimeState &runtimeState,
    const MercenaryRecruitmentTables &tables,
    float gameMinutes);
}
