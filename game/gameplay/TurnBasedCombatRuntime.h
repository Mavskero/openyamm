#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace OpenYAMM::Game
{
class Party;
struct GameplayInputFrame;
class IGameplayWorldRuntime;
enum class ActorAiUpdateMode : uint8_t;

enum class TurnBasedCombatStage : uint8_t
{
    None,
    Wait,
    Attack,
    Movement,
};

class TurnBasedCombatRuntime
{
public:
    struct QueueEntry
    {
        enum class Kind : uint8_t
        {
            Character,
            Actor,
        };

        Kind kind = Kind::Character;
        size_t id = 0;
        int initiative = 0;
        float actionRecoverySeconds = 0.0f;
        bool operator==(const QueueEntry &) const = default;
    };

    struct Snapshot
    {
        bool active = false;
        TurnBasedCombatStage stage = TurnBasedCombatStage::None;
        std::vector<QueueEntry> queue;
        int turnInitiative = 100;
        float aiTurnTimerTicks = 0.0f;
        int movementActionPoints = 0;
        int pendingActions = 0;
        uint32_t turnCount = 0;
        float actorActionTimerTicks = 0.0f;
        bool movementFinished = false;
        bool waitingForActorActionResolution = false;
        float pendingGameTimeAdvanceMinutes = 0.0f;
        std::array<float, 5> memberTurnRecoverySeconds = {};
        bool operator==(const Snapshot &) const = default;
    };

    Snapshot snapshot() const;
    void restoreSnapshot(const Snapshot &snapshot);
    static bool isValidSnapshot(const Snapshot &snapshot);

    bool active() const;
    TurnBasedCombatStage stage() const;
    int movementActionPoints() const;
    int pendingActions() const;
    bool hasPendingActions() const;
    bool playerActionReady(const Party &party) const;
    bool shouldUpdateActorAi(const Party *pParty) const;
    ActorAiUpdateMode actorAiUpdateMode() const;
    bool frontQueueCharacter(size_t &memberIndex) const;
    const std::vector<QueueEntry> &queue() const;

    bool begin(Party &party, const IGameplayWorldRuntime *pWorldRuntime);
    void end(Party *pParty, bool preserveRecovery = true);
    void reset();

    bool canToggleOff(const Party &party) const;
    bool toggle(Party &party, const IGameplayWorldRuntime *pWorldRuntime);
    bool update(Party *pParty, IGameplayWorldRuntime *pWorldRuntime, float deltaSeconds);

    bool noteMovementInput(GameplayInputFrame &input);
    float movementDeltaSecondsForFrame(float fallbackDeltaSeconds) const;
    float consumePendingGameTimeAdvanceMinutes();
    void clearMovementInput(GameplayInputFrame &input) const;
    bool finishMovementPhase();

    bool canBeginPlayerAction(const Party &party) const;
    bool beginPlayerActionOrFinishMovement(Party &party);
    bool applyPlayerAction(Party &party, size_t memberIndex, float recoverySeconds);
    void storeMemberTurnRecovery(size_t memberIndex, float recoverySeconds);

    void registerPendingAction();
    void resolvePendingAction();

private:
    static constexpr size_t MaximumTrackedMembers = 5;

    void rebuildInitialQueue(Party &party, const IGameplayWorldRuntime *pWorldRuntime);
    void appendNearbyActors(const IGameplayWorldRuntime &worldRuntime);
    void removeInvalidActorEntries(const IGameplayWorldRuntime &worldRuntime);
    std::optional<size_t> readyCharacterQueueIndex(const Party *pParty) const;
    bool readyActorQueued() const;
    void sortQueue(Party *pParty);
    void selectFrontCharacter(Party *pParty);
    void removeInvalidEntries(const Party &party);
    void advanceAttackQueue(Party *pParty, bool resolveReadyActors);
    void enterWaitStage();
    void enterAttackStage(Party *pParty, const IGameplayWorldRuntime *pWorldRuntime);
    void enterMovementStage();
    bool consumeMovementActionPoint();
    void clearMovementLatches();

    bool m_active = false;
    TurnBasedCombatStage m_stage = TurnBasedCombatStage::None;
    std::vector<QueueEntry> m_queue;
    int m_turnInitiative = 100;
    float m_aiTurnTimerTicks = 0.0f;
    int m_movementActionPoints = 0;
    int m_pendingActions = 0;
    uint32_t m_turnCount = 0;
    float m_actorActionTimerTicks = 0.0f;
    bool m_movementFinished = false;
    bool m_movementStepThisFrame = false;
    bool m_waitingForActorActionResolution = false;
    float m_pendingGameTimeAdvanceMinutes = 0.0f;
    bool m_forwardLatch = false;
    bool m_backwardLatch = false;
    bool m_strafeLeftLatch = false;
    bool m_strafeRightLatch = false;
    std::optional<size_t> m_loggedReadyMemberIndex;
    float m_memberTurnRecoverySeconds[MaximumTrackedMembers] = {};
};
} // namespace OpenYAMM::Game
