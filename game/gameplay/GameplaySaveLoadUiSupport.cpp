#include "game/gameplay/GameplaySaveLoadUiSupport.h"

#include "game/gameplay/SavePreviewImage.h"
#include "game/maps/SaveGame.h"
#include "game/StringUtils.h"
#include "game/ui/SaveSlotText.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <system_error>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
std::string normalizeSaveGameConflictName(const std::string &value)
{
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return toLowerCopy(value.substr(begin, end - begin));
}

} // namespace

void clampJournalMapState(GameplayUiController::JournalScreenState &journalScreen)
{
    journalScreen.mapZoomStep = std::clamp(
        journalScreen.mapZoomStep,
        0,
        static_cast<int>(GameplayJournalMapZoomLevels.size()) - 1);

    const int zoom = GameplayJournalMapZoomLevels[journalScreen.mapZoomStep];
    const float zoomFactor = static_cast<float>(zoom) / 384.0f;
    const float visibleWorldHalfExtent = GameplayJournalMapWorldHalfExtent / zoomFactor;
    const float maxOffset = std::max(0.0f, GameplayJournalMapWorldHalfExtent - visibleWorldHalfExtent);
    journalScreen.mapCenterX = std::clamp(journalScreen.mapCenterX, -maxOffset, maxOffset);
    journalScreen.mapCenterY = std::clamp(journalScreen.mapCenterY, -maxOffset, maxOffset);
}

std::string resolveSaveLocationName(const std::vector<MapStatsEntry> &mapEntries, const std::string &mapFileName)
{
    for (const MapStatsEntry &entry : mapEntries)
    {
        if (toLowerCopy(entry.fileName) == toLowerCopy(mapFileName))
        {
            return entry.name;
        }
    }

    return mapFileName;
}

void refreshSaveGameSlots(
    GameplayUiController::SaveGameScreenState &saveGameScreen,
    const std::vector<MapStatsEntry> &mapEntries,
    const UiTextTable *pUiText)
{
    saveGameScreen.slots.clear();
    saveGameScreen.errorText.clear();
    std::error_code directoryError;
    std::filesystem::create_directories("saves", directoryError);
    if (directoryError)
    {
        saveGameScreen.errorText = "无法访问存档目录";
    }

    for (size_t index = 0; index < GameplaySaveGameSlotCount; ++index)
    {
        GameplayUiController::SaveSlotSummary slot = {};
        slot.path = std::filesystem::path("saves") / ("save" + std::to_string(index + 1) + ".oysav");

        std::error_code slotError;
        const bool exists = !directoryError && std::filesystem::exists(slot.path, slotError);
        if (directoryError || slotError)
        {
            slot.fileLabel = "不可用";
            slot.locationName = "无法访问存档目录";
            saveGameScreen.errorText = slot.locationName;
        }
        else if (exists)
        {
            std::string error;
            const bool regularFile = std::filesystem::is_regular_file(slot.path, slotError);
            const std::optional<GameSaveData> saveData = regularFile && !slotError
                ? loadGameDataFromPath(slot.path, error) : std::nullopt;

            if (saveData)
            {
                slot.populated = true;
                slot.fileLabel = !saveData->saveName.empty()
                    ? saveData->saveName : friendlySaveFileLabel(slot.path, pUiText);
                slot.locationName = resolveSaveLocationName(mapEntries, saveData->mapFileName);
                const SaveSlotTimeText timeText = formatSaveSlotTime(saveData->savedGameMinutes, pUiText);
                slot.weekdayClockText = timeText.clock;
                slot.dateText = timeText.date;

                if (!saveData->previewBmp.empty())
                {
                    SavePreviewImage::decodeBmpBytesToBgra(
                        saveData->previewBmp,
                        slot.previewWidth,
                        slot.previewHeight,
                        slot.previewPixelsBgra);
                }
            }
            else
            {
                // An unreadable/corrupt slot is not empty and must never be replaced implicitly.
                slot.fileLabel = "不可用";
                slot.locationName = "存档无法读取，原文件已保留";
                saveGameScreen.errorText = slot.locationName;
            }
        }

        if (slot.fileLabel.empty())
        {
            slot.fileLabel = pUiText != nullptr ? pUiText->text(UiTextTable::Message::EmptySlot) : "Empty";
        }

        saveGameScreen.slots.push_back(std::move(slot));
    }

    if (saveGameScreen.selectedIndex >= saveGameScreen.slots.size())
    {
        saveGameScreen.selectedIndex = 0;
    }

    saveGameScreen.scrollOffset = 0;
    saveGameScreen.editActive = false;
    saveGameScreen.editSlotIndex = 0;
    saveGameScreen.editBuffer.clear();
}

void refreshLoadGameSlots(
    GameplayUiController::LoadGameScreenState &loadGameScreen,
    const std::vector<MapStatsEntry> &mapEntries,
    const UiTextTable *pUiText)
{
    loadGameScreen.slots.clear();
    loadGameScreen.scrollOffset = 0;
    std::error_code directoryError;
    std::filesystem::create_directories("saves", directoryError);
    if (directoryError)
    {
        loadGameScreen.selectedIndex = 0;
        return;
    }

    const std::filesystem::directory_iterator end;
    std::filesystem::directory_iterator entries("saves", directoryError);
    while (!directoryError && entries != end)
    {
        const std::filesystem::directory_entry entry = *entries;
        std::error_code entryError;
        const bool regularFile = entry.is_regular_file(entryError);
        entries.increment(directoryError);
        if (entryError)
        {
            directoryError = entryError;
            break;
        }
        if (!regularFile || toLowerCopy(entry.path().extension().string()) != ".oysav")
        {
            continue;
        }

        std::string error;
        const std::optional<GameSaveData> saveData = loadGameDataFromPath(entry.path(), error);

        if (!saveData)
        {
            continue;
        }

        GameplayUiController::SaveSlotSummary slot = {};
        slot.path = entry.path();
        slot.fileLabel = !saveData->saveName.empty()
            ? saveData->saveName : friendlySaveFileLabel(entry.path(), pUiText);
        slot.locationName = resolveSaveLocationName(mapEntries, saveData->mapFileName);
        slot.populated = true;
        const SaveSlotTimeText timeText = formatSaveSlotTime(saveData->savedGameMinutes, pUiText);
        slot.weekdayClockText = timeText.clock;
        slot.dateText = timeText.date;

        if (!saveData->previewBmp.empty())
        {
            SavePreviewImage::decodeBmpBytesToBgra(
                saveData->previewBmp,
                slot.previewWidth,
                slot.previewHeight,
                slot.previewPixelsBgra);
        }

        loadGameScreen.slots.push_back(std::move(slot));
    }
    if (directoryError)
    {
        loadGameScreen.slots.clear();
        loadGameScreen.selectedIndex = 0;
        return;
    }

    std::sort(
        loadGameScreen.slots.begin(),
        loadGameScreen.slots.end(),
        [](const GameplayUiController::SaveSlotSummary &left, const GameplayUiController::SaveSlotSummary &right)
        {
            return compareSavePathsForDisplay(left.path, right.path);
        });

    if (loadGameScreen.selectedIndex >= loadGameScreen.slots.size())
    {
        loadGameScreen.selectedIndex = 0;
    }

    loadGameScreen.scrollOffset = 0;
}

bool saveGameNameConflictsWithExistingSlot(
    const GameplayUiController::SaveGameScreenState &saveGameScreen,
    const std::string &saveName,
    size_t targetSlotIndex)
{
    const std::string normalizedSaveName = normalizeSaveGameConflictName(saveName);

    if (normalizedSaveName.empty())
    {
        return false;
    }

    for (size_t slotIndex = 0; slotIndex < saveGameScreen.slots.size(); ++slotIndex)
    {
        if (slotIndex == targetSlotIndex)
        {
            continue;
        }

        const GameplayUiController::SaveSlotSummary &slot = saveGameScreen.slots[slotIndex];

        if (!slot.populated)
        {
            continue;
        }

        if (normalizeSaveGameConflictName(slot.fileLabel) == normalizedSaveName)
        {
            return true;
        }
    }

    return false;
}
} // namespace OpenYAMM::Game
