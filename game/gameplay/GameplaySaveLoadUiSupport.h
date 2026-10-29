#pragma once

#include "game/tables/MapStats.h"
#include "game/ui/GameplayUiController.h"

#include <array>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class UiTextTable;

constexpr size_t GameplaySaveLoadVisibleSlotCount = 10;
constexpr size_t GameplaySaveGameSlotCount = 100;
constexpr std::array<int, 3> GameplayJournalMapZoomLevels = {384, 768, 1536};
constexpr float GameplayJournalMapWorldHalfExtent = 32768.0f;

void clampJournalMapState(GameplayUiController::JournalScreenState &journalScreen);
std::string resolveSaveLocationName(const std::vector<MapStatsEntry> &mapEntries, const std::string &mapFileName);
void refreshSaveGameSlots(
    GameplayUiController::SaveGameScreenState &saveGameScreen,
    const std::vector<MapStatsEntry> &mapEntries,
    const UiTextTable *pUiText = nullptr);
void refreshLoadGameSlots(
    GameplayUiController::LoadGameScreenState &loadGameScreen,
    const std::vector<MapStatsEntry> &mapEntries,
    const UiTextTable *pUiText = nullptr);
bool saveGameNameConflictsWithExistingSlot(
    const GameplayUiController::SaveGameScreenState &saveGameScreen,
    const std::string &saveName,
    size_t targetSlotIndex);
} // namespace OpenYAMM::Game
