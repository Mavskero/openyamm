#pragma once

#include "game/content/BreachMazeFloorDescriptor.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachMazePopulationVersion = 1;
inline constexpr uint32_t BreachMazePopulationContentFixVersion = 1; // Explicit trashheap -> trasheap.
inline constexpr uint32_t BreachMazePopulationMaximumLevel = BreachMazeMaximumPlannedLevel;
inline constexpr uint8_t BreachMazePopulationMaximumSide = BreachMazeTemplateSide;
inline constexpr size_t BreachMazePopulationMaximumActors = 1500; // Original RemoveMapMonstersLimits.lua.
inline constexpr uint8_t BreachMazeMaximumTreasureLevel = 6;
inline constexpr std::array<uint32_t, 11> BreachMazeTrapSpells = {2, 15, 24, 39, 18, 6, 7, 11, 32, 41, 93};

// Coordinates remain exact until a later checked native integer conversion. In particular,
// source y=-601.5 is -1203 here; floor(-601.5) would not be truncation toward zero.
struct BreachMazeHalfPoint
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    bool operator==(const BreachMazeHalfPoint &) const = default;
};

struct BreachMazeSourceFingerprint
{
    std::string name;
    std::string sha256;
    bool operator==(const BreachMazeSourceFingerprint &) const = default;
};

struct BreachMazeMonsterSource
{
    uint32_t id = 0;
    uint16_t level = 0;
    uint32_t baseHitPoints = 0; // Not a resolved combat cap.
    uint16_t height = 0;
    uint16_t radius = 0;
    uint16_t moveSpeed = 0;
    int16_t toHitRadius = 0;
    uint32_t tintArgb = 0;
    uint8_t baseKinds = 0; // Source Monster Kinds columns1..8, not native MonsterKind enum ordinals.
    uint8_t bolsterKinds = 0;
    std::array<uint16_t, 4> soundIds = {};
    std::array<std::string, 8> spriteGroups;
    // Original mechanics and resolver inputs are retained, never reconstructed from maze level.
    std::array<std::string, 39> monsterRow;
    std::array<std::string, 22> monListRow;
    std::optional<std::array<std::string, 17>> bolsterRow;
    bool operator==(const BreachMazeMonsterSource &) const = default;
};

struct BreachMazeDecorationSource
{
    uint16_t id = 0;
    std::string name;
    std::string spriteGroup;
    uint16_t height = 0;
    uint16_t radius = 0;
    uint16_t lightRadius = 0;
    uint16_t soundId = 0;
    bool noBlockMovement = false;
    std::array<std::string, 23> sourceRow;
    bool operator==(const BreachMazeDecorationSource &) const = default;
};

struct BreachMazeLightProperties
{
    int16_t radius = 0;
    std::array<uint8_t, 3> color = {};
    uint8_t type = 0;
    int16_t attributes = 0; // Includes all original bits; Off is supplied by each light plan.
    int16_t brightness = 0;
    int32_t id = 0;
    bool operator==(const BreachMazeLightProperties &) const = default;
};

struct BreachMazePopulationSourceSlot
{
    uint16_t decorationId = 0;
    BreachMazeHalfPoint position;
    int32_t facing = 0;
    uint16_t attributes = 0;
    BreachMazeLightProperties light;
    bool operator==(const BreachMazePopulationSourceSlot &) const = default;
};

struct BreachMazePopulationCatalog
{
    uint32_t version = BreachMazePopulationVersion;
    uint32_t contentFixVersion = BreachMazePopulationContentFixVersion;
    uint64_t templateFingerprint = 0;
    uint64_t contentFingerprint = 0;
    std::vector<BreachMazeSourceFingerprint> sources; // Sorted by source name, authenticated by importer.
    std::vector<BreachMazeMonsterSource> monsters; // Complete source IDs1..652, in order.
    std::vector<BreachMazeDecorationSource> decorations; // Complete original DecList, in ID order.
    std::array<BreachMazePopulationSourceSlot, BreachMazeTemplateCellCount> sourceSlots;
    std::vector<std::string> bolsterMapRow;
    std::vector<std::vector<std::string>> bolsterFormulaRows;
    bool operator==(const BreachMazePopulationCatalog &) const = default;
};

// Canonical integrity hashes, not authentication. Production catalogs must come from the SHA-gated
// source importer; recomputing a fingerprint is not permission to use an altered source profile.
uint64_t breachMazePopulationTemplateFingerprint(const BreachMazeTemplate &mazeTemplate);
uint64_t breachMazePopulationCatalogFingerprint(const BreachMazePopulationCatalog &catalog);

struct BreachMazeSkillView
{
    uint32_t skill = 0;
    uint8_t mastery = 0;
};

enum class BreachMazePopulationStream : uint8_t
{
    Material, Population, Decoration,
};
enum class BreachMazePopulationRoll : uint8_t
{
    Material, TreasureBudget, MonsterBudget, DeadEndBranch, UsefulDecoration,
    MonsterId, GroupSize, ActorX, ActorY, TorchWall, FloorTrap, PlaceholderItem,
    TreasureLevel, DecorationAttribute,
};

struct BreachMazePopulationDraw
{
    BreachMazePopulationStream stream = BreachMazePopulationStream::Population;
    BreachMazePopulationRoll purpose = BreachMazePopulationRoll::Material;
    uint16_t context = 0; // Room, source cell, or zero-based decoration slot, according to purpose.
    uint32_t minimum = 0;
    uint32_t maximum = 0;
    uint32_t value = 0;
    uint64_t rawDrawsAfter = 0; // Includes rejection draws, within the named stream/substream.
    bool operator==(const BreachMazePopulationDraw &) const = default;
};

struct BreachMazeActorRequest
{
    uint32_t ordinal = 0;
    uint16_t cellId = 0;
    uint32_t monsterId = 0;
    BreachMazeHalfPoint position;
    uint8_t hitPointMultiplier = 1; // Applied AFTER shared resolution, not to table baseHitPoints here.
    uint64_t familyResolveKey = 0;
    uint64_t actorResolveKey = 0;
    uint8_t hostilityType = 3;
    bool hostile = true;
    bool showOnMap = false;
    bool onAlertMap = true;
    uint16_t guardRadius = 10;
    uint16_t room = 0;
    uint8_t goldDiceCount = 0;
    uint8_t goldDiceSides = 0;
    bool rage = false;
    bool operator==(const BreachMazeActorRequest &) const = default;
};

struct BreachMazeItemRequest
{
    uint32_t ordinal = 0;
    uint16_t cellId = 0;
    BreachMazeHalfPoint position;
    uint32_t fixedItemId = 0; // 220 means the empty-bottle branch, with no randomization.
    uint32_t placeholderItemId = 0; // Consumed source177..186 draw; NOT the final object appearance.
    uint8_t treasureLevel = 0;
    uint8_t treasureType = 0;
    bool positiveBonusStrengthAtLeastOne = false;
    uint64_t resolveKey = 0;
    bool operator==(const BreachMazeItemRequest &) const = default;
};

// Changes to the frozen145 scene, in facet-ID order. An absent field preserves the existing value.
// No portal/visibility/collision geometry flags are changed by population, even on library openings.
struct BreachMazePopulationFacePatch
{
    uint32_t faceIndex = 0;
    std::optional<std::string> texture;
    std::optional<uint16_t> eventId;
    std::optional<bool> click;
    std::optional<bool> step;
    std::optional<bool> secret;
    bool operator==(const BreachMazePopulationFacePatch &) const = default;
};

enum class BreachMazeDecorationHandler : uint8_t
{
    None, Crystal, ReagentBucket, MiscBag, FoodSack, AttributeBarrel, ResistanceCauldron, TrashHeap, Challenge,
};

struct BreachMazeDecorationInitialState
{
    BreachMazeDecorationHandler handler = BreachMazeDecorationHandler::None;
    uint16_t eventVariable = 0;
    uint32_t fixedRewardItemId = 0;
    bool used = false;
    bool requiresGlobalChallengeLedger = false;
    uint16_t challengeThreshold = 0;
    uint16_t challengeSkillPointReward = 0;
    uint64_t randomState = 0; // Slot-specific interaction substream, after initialization draws.
    uint64_t randomDrawCount = 0;
    bool operator==(const BreachMazeDecorationInitialState &) const = default;
};

struct BreachMazeDecorationPlan
{
    uint16_t slot = 0;
    uint16_t cellId = 0; // Zero for untouched, hidden original slots.
    uint16_t decorationId = 0;
    std::string requestedName;
    std::string resolvedName;
    bool sourceNameCorrected = false;
    BreachMazeHalfPoint position;
    int32_t facing = 0;
    uint16_t sourceAttributes = 0;
    bool invisible = true;
    bool showOnMap = false;
    uint32_t eventId = 0;
    BreachMazeDecorationInitialState interaction;
    bool operator==(const BreachMazeDecorationPlan &) const = default;
};

struct BreachMazeLightPlan
{
    uint16_t slot = 0;
    BreachMazeHalfPoint position;
    BreachMazeLightProperties sourceProperties;
    bool off = true;
    bool operator==(const BreachMazeLightPlan &) const = default;
};

struct BreachMazeTrapInitialState
{
    bool placed = false;
    bool visible = false;
    std::optional<uint32_t> spellId; // null=not drawn yet; 0=disarmed. Initial plans never draw the spell.
    uint64_t nextShotGameMinute = 0;
    bool operator==(const BreachMazeTrapInitialState &) const = default;
};

struct BreachMazePopulationInteractionRules
{
    std::vector<uint32_t> trapSpellChoices; // Source prefix min(level,11), selected lazily.
    uint8_t trapSpellSkillLevel = 7;
    uint8_t trapSpellMastery = 3;
    uint8_t trapCooldownGameMinutes = 3;
    // Effective tier bounds after source min(roll,6). The raw roll uses the floor's uint32 level,
    // preserving this codec's byte fields and the exact old L1..3 range and cursor consumption.
    uint8_t bookcaseTreasureMinimum = 0;
    uint8_t bookcaseTreasureMaximum = 0;
    uint8_t bookcaseTreasureType = 16; // Strict Sscroll, not Book or broad Scroll_.
    uint8_t bookcaseRecipientPartyIndex = 0; // Resolve to a stable recipient when the interaction commits.
    bool interactionsUsePartyPositionCell = true;
    bool operator==(const BreachMazePopulationInteractionRules &) const = default;
};

struct BreachMazePopulationResources
{
    std::vector<std::string> textures;
    std::vector<std::string> actorSpriteGroups;
    std::vector<std::string> decorationSpriteGroups;
    std::vector<uint16_t> soundIds;
    std::vector<uint32_t> monsterIds;
    std::vector<uint32_t> fixedItemIds;
    std::vector<uint32_t> trapSpellIds;
    // B1 does not resolve combat spell generation, summons or random InventoryItem appearance.
    bool requiresResolvedActorResources = true;
    bool requiresResolvedItemResources = false;
    bool operator==(const BreachMazePopulationResources &) const = default;
};

struct BreachMazePopulationPlan
{
    uint32_t version = BreachMazePopulationVersion;
    uint32_t contentFixVersion = BreachMazePopulationContentFixVersion;
    BreachMazeFloorIdentity identity;
    BreachMazeFloorSeeds seeds;
    uint64_t templateFingerprint = 0;
    uint64_t catalogFingerprint = 0;
    uint64_t layoutFinalRandomState = 0;
    uint64_t layoutRandomDrawCount = 0;
    std::array<uint8_t, 16> paletteByRoom = {};
    uint32_t treasureLimit = 0;
    int32_t initialMonsterBudget = 0;
    int32_t remainingMonsterBudget = 0;
    uint32_t placedDecorationCount = 0;
    uint32_t torchCount = 0;
    bool canSeeTrapsAtCreation = false;
    uint8_t sectorOneDarkness = 26; // Source D3D branch, explicitly selected for native Metal.
    uint64_t materialRandomState = 0;
    uint64_t materialRandomDrawCount = 0;
    uint64_t populationRandomState = 0;
    uint64_t populationRandomDrawCount = 0;
    uint64_t interactionRandomState = 0; // Unconsumed dynamic trap/bookcase stream.
    std::vector<uint32_t> candidateMonsterIds;
    std::vector<BreachMazeActorRequest> actors;
    std::vector<BreachMazeItemRequest> items;
    std::vector<BreachMazePopulationFacePatch> facePatches;
    std::array<BreachMazeDecorationPlan, BreachMazeTemplateCellCount> decorations;
    std::array<BreachMazeLightPlan, BreachMazeTemplateCellCount> lights;
    std::array<BreachMazeTrapInitialState, BreachMazeTemplateCellCount> traps;
    std::array<bool, BreachMazeTemplateCellCount> bookcaseCells = {};
    std::array<bool, BreachMazeTemplateCellCount> bookcaseUsed = {};
    BreachMazePopulationInteractionRules interactionRules;
    BreachMazePopulationResources resources;
    std::vector<BreachMazePopulationDraw> draws;
    bool operator==(const BreachMazePopulationPlan &) const = default;
};

// Pure source-profile plan. Requires pristine versioned143 layout and authenticated original catalog.
// Throws on invalid identity, unrepresentable source budgets, corrupt metadata/layout or an empty pool.
// No entropy, materialization, HP resolution, inventory award, scene mutation, travel or save effects.
BreachMazePopulationPlan planBreachMazePopulation(const BreachMazePopulationCatalog &catalog,
    const BreachMazeTemplate &mazeTemplate, const BreachMazeFloorDescriptor &floor,
    std::span<const BreachMazeSkillView> perceptionAtCreation = {});
}
