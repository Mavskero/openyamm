#pragma once

#include "game/content/BreachMazeState.h"

#include <string_view>

namespace OpenYAMM::Game
{
struct BreachMazeQuestView
{
    bool present = false;
    bool enteredBasement = false;
    bool questFinished = false;

    bool operator==(const BreachMazeQuestView &) const = default;
};

struct BreachMazeQuestEffects
{
    bool setEnteredBasement = false;
    bool setEnteredBreach = false;
    // Deliberately no QuestFinished effect: only the actual Breach lifecycle owns that transition.
};

enum class BreachMazeEntry : uint8_t
{
    BrAlvar53,
    BrAlvar54,
    Breach54,
};

enum class BreachMazeStep : uint8_t
{
    Up, // Source event 1: entrance door.
    Down, // Source event 2: exit door; locked for an unfinished QSet.
};

enum class BreachMazePlanKind : uint8_t
{
    NewFloor,
    RetainedFloor,
    LeaveMaze,
    DownLocked,
    PortalDeparture,
};

enum class BreachMazeArrivalKind : uint8_t
{
    Entrance,
    Exit,
    SavedPose, // Saved XYZ; real TriggerReload resets yaw/pitch to zero, unlike ColdRestore.
};

struct BreachMazeArrival
{
    BreachMazeArrivalKind kind = BreachMazeArrivalKind::Entrance;
    float x = 0.0f;
    float y = 0.0f;
    float footZ = -200.0f;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

struct BreachMazeDeparture
{
    std::string_view mapFileName;
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    int32_t yawUnits = 0;
};

struct BreachMazeReadyFloor
{
    BreachMazeFloorIdentity identity;
    BreachMazeFloorRuntime runtime;
    std::optional<BreachMazePopulationPlan> population;
};

enum class BreachMazeCommitResult : uint8_t
{
    Committed,
    Cancelled,
    DownLocked,
    StaleState,
    StaleQuest,
    Consumed,
};

// Move-only token. Callers can inspect but cannot edit its candidate or substitute a target descriptor.
class BreachMazePreparedTransition
{
public:
    BreachMazePreparedTransition(BreachMazePreparedTransition &&) noexcept = default;
    BreachMazePreparedTransition &operator=(BreachMazePreparedTransition &&) noexcept = default;
    BreachMazePreparedTransition(const BreachMazePreparedTransition &) = delete;
    BreachMazePreparedTransition &operator=(const BreachMazePreparedTransition &) = delete;

    BreachMazePlanKind kind() const;
    const BreachMazeFloorDescriptor *target() const;
    const BreachMazeFloorState *retainedFloor() const;
    const BreachMazeArrival &arrival() const;
    const std::optional<BreachMazeDeparture> &departure() const;
    const BreachMazeQuestEffects &questEffects() const;
    // Source OnLoad clears rage/blood on real entry or floor changes. A later controller must
    // perform that reset on its staged scene; this pure state module does not pretend to run it.
    bool requiresSourceEntryReset() const;

private:
    friend class BreachMazeLifecycle;
    BreachMazePreparedTransition() = default;
    std::shared_ptr<const BreachMazeRunData> m_expected;
    std::optional<BreachMazeRunData> m_candidate;
    BreachMazeQuestView m_quest;
    BreachMazePlanKind m_kind = BreachMazePlanKind::DownLocked;
    std::optional<BreachMazeFloorDescriptor> m_newFloor;
    BreachMazeArrival m_arrival;
    std::optional<BreachMazeDeparture> m_departure;
    BreachMazeQuestEffects m_effects;
    bool m_consumed = false;
};

// Pure lifecycle over an already validated 143 template. No map admission, entropy, saves or travel.
class BreachMazeLifecycle
{
public:
    explicit BreachMazeLifecycle(BreachMazeTemplate mazeTemplate);

    // Strict restore: never rerolls a corrupt record. Cold load preserves the complete native
    // snapshot and persisted interactions, even when direction is Fresh. Legacy playable v2 must
    // pass the explicit Save94 migration first; restore never initializes missing interactions.
    BreachMazeState restore(BreachMazeRunData data) const;
    const BreachMazeFloorState &coldRestore(const BreachMazeState &state) const;

    BreachMazePreparedTransition prepareFreshEntry(const BreachMazeState &state, BreachMazeEntry entry,
        BreachMazeQuestView quest, uint64_t rootSeed) const;
    BreachMazePreparedTransition prepareStep(const BreachMazeState &state, BreachMazeStep step,
        BreachMazeQuestView quest, const BreachMazeFloorRuntime &leavingRuntime) const;
    // An already selected normal Town Portal destination belongs to App's map-move transaction.
    // Retain the actual current floor and its interactions, with no stair-exit quest effects.
    // PortalDeparture has neither target() nor departure(); only playable v3 may prepare it.
    BreachMazePreparedTransition preparePortalDeparture(const BreachMazeState &state,
        BreachMazeQuestView quest, const BreachMazeFloorRuntime &leavingRuntime) const;

    // Native policy: caller has already received the real confirmation and staged all required
    // scene/resources. New floors require a matching ready snapshot; retained floors use their
    // stored snapshot. Apply questEffects ONLY after Committed, together with the scene switch.
    // Invalid payloads throw without changing state or consuming the token; cancel consumes it.
    BreachMazeCommitResult commit(BreachMazeState &state, BreachMazePreparedTransition &prepared,
        bool confirmed, BreachMazeQuestView currentQuest, const BreachMazeReadyFloor *pReadyFloor = nullptr) const;

    // Save capture is not travel and needs no confirmation. Preserve the live interaction record;
    // capture never reconstructs it from the initial population. Invalid captures leave state unchanged.
    void captureCurrentFloor(BreachMazeState &state, const BreachMazeFloorRuntime &runtime) const;

private:
    uint64_t templateFingerprintForVersion(uint32_t version) const;
    void validate(const BreachMazeRunData &data) const;
    void validateRuntime(const BreachMazeFloorRuntime &runtime, uint32_t version) const;
    void selectTarget(BreachMazePreparedTransition &prepared, bool useExit) const;
    BreachMazeTemplate m_mazeTemplate;
    uint64_t m_templateFingerprint = 0;
    uint64_t m_playableTemplateFingerprint = 0;
};
}
