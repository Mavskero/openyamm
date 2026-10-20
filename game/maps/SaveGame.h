#pragma once

#include "game/content/BreachAlvarCompanionState.h"
#include "game/content/BreachMazeState.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/gameplay/TurnBasedCombatRuntime.h"
#include "game/scene/SceneKind.h"
#include "game/outdoor/OutdoorPartyRuntime.h"
#include "game/outdoor/OutdoorWorldRuntime.h"
#include "game/party/Party.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
class ItemTable;
class HouseTable;
struct BreachAlvarFriendActorRules;

struct GameSaveData
{
    SceneKind currentSceneKind = SceneKind::Outdoor;
    std::string mapFileName;
    Party::Snapshot party;
    std::unordered_map<std::string, int32_t> namedGlobalVars;
    bool hasOutdoorRuntimeState = false;
    OutdoorPartyRuntime::Snapshot outdoorParty;
    OutdoorWorldRuntime::Snapshot outdoorWorld;
    std::unordered_map<std::string, OutdoorWorldRuntime::Snapshot> outdoorWorldStates;
    bool hasIndoorSceneState = false;
    IndoorSceneRuntime::Snapshot indoorScene;
    std::unordered_map<std::string, IndoorSceneRuntime::Snapshot> indoorSceneStates;
    double savedGameMinutes = 0.0;
    float outdoorCameraYawRadians = 0.0f;
    float outdoorCameraPitchRadians = 0.0f;
    float indoorCameraYawRadians = 0.0f;
    float indoorCameraPitchRadians = 0.15f;
    bool heldInventoryItemActive = false;
    InventoryItem heldInventoryItem = {};
    uint8_t heldInventoryItemGrabCellOffsetX = 0;
    uint8_t heldInventoryItemGrabCellOffsetY = 0;
    float heldInventoryItemGrabOffsetX = 0.0f;
    float heldInventoryItemGrabOffsetY = 0.0f;
    std::unordered_map<std::string, uint32_t> requiredContentPackages;
    std::string saveName;
    std::vector<uint8_t> previewBmp;
    TurnBasedCombatRuntime::Snapshot turnBasedCombat;
    std::optional<BreachAlvarCompanionState> breachAlvarCompanions;
    std::optional<BreachMazeRunData> breachMaze;
};

bool validateSavedBreachMaze(const GameSaveData &data, std::string &error);
bool validateSavedActorCombat(const GameSaveData &data, std::string &error);
bool validateSavedBreachAlvarCompanions(
    const GameSaveData &data, std::string &error, const BreachAlvarFriendActorRules *pRules = nullptr);
// In-memory, same-build baselines for synchronous transactions; not a portable hash or saved payload.
std::vector<uint8_t> encodePartySnapshotForComparison(const Party::Snapshot &snapshot);
std::vector<uint8_t> encodeInventoryItemForComparison(const InventoryItem &item);
void migrateLegacyConsumedCorpseMarkers(GameSaveData &data, uint32_t sourceSaveVersion);
void migrateLegacyPartyMemberIdentities(GameSaveData &data, uint32_t sourceSaveVersion);
bool saveGameDataToPath(const std::filesystem::path &path, const GameSaveData &data, std::string &error,
    const BreachAlvarFriendActorRules *pCompanionRules = nullptr);
// Save96 adds source reserve generation receipts and Character biography. Save91–95 companion receipts
// migrate only from their strict five-member form; original files remain unchanged.
// Save94 playable maze v2 is explicitly migrated to v3 interaction records on load. Save95 requires
// its persisted v3 records; missing/corrupt current records are rejected, never regenerated.
std::optional<GameSaveData> loadGameDataFromPath(const std::filesystem::path &path, std::string &error);
std::unordered_map<std::string, uint32_t> collectRequiredContentPackages(
    const GameSaveData &data,
    const ItemTable &itemTable,
    const HouseTable &houseTable,
    const std::unordered_map<std::string, uint32_t> &loadedPackageSchemas);
bool validateRequiredContentPackages(
    const GameSaveData &data,
    const ItemTable &itemTable,
    const HouseTable &houseTable,
    const std::unordered_map<std::string, uint32_t> &loadedPackageSchemas,
    std::string &error);
bool compareSavePathsForDisplay(const std::filesystem::path &left, const std::filesystem::path &right);
}
