#pragma once

#include "game/content/BreachMazeItemResolver.h"
#include "game/content/BreachMazePopulation.h"
#include "game/gameplay/GameplayActorCombatState.h"
#include "game/gameplay/MergeMonsterSourceProfile.h"
#include "game/maps/MapAssetLoader.h"
#include "game/scene/IndoorSceneRuntime.h"

namespace OpenYAMM::Game
{
class MergedBolsterFormulaTable;

struct BreachMazeInstanceBuildInput
{
    const IndoorMapData &originalMap;
    const MapDeltaData &baseDelta;
    const BreachMazeTemplate &mazeTemplate;
    const BreachMazeFloorDescriptor &floor;
    const BreachMazePopulationPlan &population;
    const BreachMazePopulationCatalog &catalog;
    const MonsterTable &monsters;
    const MonsterProjectileTable &projectiles;
    const SpellTable &spells;
    const ObjectTable &objects;
    const ItemTable &items;
    const StandardItemEnchantTable &standardEnchants;
    const SpecialItemEnchantTable &specialEnchants;
    const MergedBolsterMonsterTable &bolsterMonsters;
    const MergedBolsterFormulaTable &bolsterFormulas;
    // Snapshot values only; effectivePartyLevel includes map LevelShift. Per-family seeds are
    // taken exclusively from the frozen population requests, never from combatContext.familySeed.
    MergeMonsterFamilyContext combatContext;
    std::string worldId = "mmmerge";
    std::string mapFileName = "brbase.blv";
    std::string contentPackageId = "engine";
    uint32_t contentPackageSchema = 1;
};

struct BreachMazePreparedInstance
{
    PreparedIndoorMapInstance instance;
    // Exact delta.spriteObjects index order. Receipts never authorize generating loot again.
    std::vector<BreachMazeItemResolutionReceipt> itemReceipts;
};

// Consumes an already frozen population plan and source-authenticated item/enchant tables.
// Existing source-profile gates validate original monster mechanics, descriptors and B2 formulas.
// Builds a wholly local candidate: no Party, scene publication, external RNG, fallback or reroll.
// Throws with the source failure; a caller must retain the failed plan instead of drawing another.
BreachMazePreparedInstance buildBreachMazeInstance(const BreachMazeInstanceBuildInput &input);

struct BreachMazeInstanceRestoreInput
{
    const IndoorMapData &originalMap;
    const MapDeltaData &baseDelta;
    const BreachMazeTemplate &mazeTemplate;
    const BreachMazeFloorDescriptor &floor;
    const BreachMazePopulationPlan &population;
    const BreachMazePopulationCatalog &catalog;
    const IndoorSceneRuntime::Snapshot &snapshot;
    const MergeMonsterSourceProfile &monsterSource;
    std::string worldId = "mmmerge";
    std::string mapFileName = "brbase.blv";
    std::string contentPackageId = "engine";
    uint32_t contentPackageSchema = 1;
};

// Reconstructs geometry from the saved layout and frozen population plan. The complete live delta,
// combat states and generator state come exclusively from snapshot; no population/item/actor is
// planned or resolved again. The caller must then restore this same snapshot before simulation.
// Original geometry/catalog/profile inputs must already have passed their source authentication.
PreparedIndoorMapInstance restoreBreachMazeInstance(const BreachMazeInstanceRestoreInput &input);
}
