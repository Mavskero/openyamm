#pragma once

#include "game/content/BreachMazePopulation.h"
#include "game/items/ItemGenerationRandom.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace OpenYAMM::Game
{
struct EventRuntimeState;
class ISceneEventContext;
class Party;

inline constexpr uint32_t BreachMazeInteractionStateVersion = 1;

struct BreachMazeTrapState
{
    std::optional<uint32_t> spellId; // null: not selected yet; zero: permanently disarmed/exhausted.
    double nextShotGameMinute = 0.0;
    bool operator==(const BreachMazeTrapState &) const = default;
};

struct BreachMazeDecorationState
{
    bool used = false; // Challenges use the global member ledger instead.
    FixedItemGenerationCursor random;
    bool operator==(const BreachMazeDecorationState &) const = default;
};

// Mutable floor-owned state. The population plan remains the immutable generation baseline.
// Initialize once for a new floor, then persist this record in Save95 and retain it on floor changes.
struct BreachMazeInteractionState
{
    uint32_t version = BreachMazeInteractionStateVersion;
    BreachMazeFloorIdentity identity;
    FixedItemGenerationCursor random;
    std::array<BreachMazeTrapState, BreachMazeTemplateCellCount> traps;
    // Source records the Party's cell, which can differ from the activated face's cell.
    std::array<bool, BreachMazeTemplateCellCount> bookcaseUsed = {};
    std::array<BreachMazeDecorationState, BreachMazeTemplateCellCount> decorations;
    bool operator==(const BreachMazeInteractionState &) const = default;
};

struct BreachMazeInteractionRequest
{
    uint32_t eventId = 0;
    // null matches Game.CurrentPlayer == -1: no trap disarm attempt; decorations fall back to member zero.
    std::optional<size_t> currentMemberIndex;
};

struct BreachMazeInteractionContext
{
    Party &party;
    EventRuntimeState &events;
    ISceneEventContext &scene;
    float partyX = 0.0f;
    float partyY = 0.0f;
    float partyFootZ = 0.0f;
    double gameMinutes = 0.0;
};

enum class BreachMazeInteractionTextSource : uint8_t
{
    LocalString,
    GlobalText,
    NpcText,
};

struct BreachMazeInteractionMessage
{
    BreachMazeInteractionTextSource source = BreachMazeInteractionTextSource::LocalString;
    uint32_t id = 0;
    // Challenge win: original NPCText734 replaces " +3" with this argument (e.g. "+5").
    std::string formatArgument;
};

struct BreachMazeInteractionFaceAnimation
{
    size_t memberIndex = 0;
    uint32_t legacyAnimationId = 0;
};

struct BreachMazeInteractionResult
{
    bool handled = false;
    bool changed = false;
    std::string error;
    std::optional<BreachMazeInteractionMessage> message;
    std::optional<BreachMazeInteractionFaceAnimation> faceAnimation;
    // Caller applies these presentation/geometry projections after success. On restore, derive them
    // from spellId==0 and used on Crystal/MiscBag/FoodSack; never regenerate or consume another roll.
    std::optional<uint32_t> clearTrapFaceIndex;
    std::optional<uint16_t> hideDecorationSlot;
};

BreachMazeInteractionState makeBreachMazeInteractionState(const BreachMazePopulationPlan &population);
bool validateBreachMazeInteractionState(const BreachMazeInteractionState &state,
    const BreachMazePopulationPlan &population, std::string &error);

// Native original-source behavior for events 3, 4, and 20000+decoration slot. Unknown events are unhandled.
// Recognized invalid operations return handled+error without consuming RNG or changing Party/events/state.
// Rewards are prepared on disposable Party/event candidates; a projectile must spawn before committing a trap.
// The caller must merge events.namedGlobalVars into session globals and rebind globals on every floor entry.
// Challenge keys are Merge.WonChallenges.<stable memberId>.<eventVariable>, shared across all floors/maps.
BreachMazeInteractionResult activateBreachMazeInteraction(BreachMazeInteractionState &state,
    const BreachMazePopulationPlan &population, const BreachMazeTemplate &mazeTemplate,
    const BreachMazeInteractionRequest &request, BreachMazeInteractionContext context);
}
