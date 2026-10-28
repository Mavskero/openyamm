#pragma once

#include "game/gameplay/BreachAlvarCompanionTransaction.h"

#include <cstdint>
#include <optional>
#include <string>

namespace OpenYAMM::Game
{
struct BreachMercenaryProfile;
class GameSession;

enum class OwnedMercenaryResolutionCode : uint8_t
{
    Eligible,
    AlreadyActive,
    Released,
    NotRetained,
    UnknownRoster,
    AmbiguousRoster,
    Stale,
    UnsupportedOwner,
    InvalidState,
};

// A borrowed view valid only while the supplied Party is unchanged. Even a non-eligible saved world binding
// remains an owned-mercenary binding; callers must never fall back to template recruitment on these results.
struct OwnedMercenaryResolution
{
    OwnedMercenaryResolutionCode code = OwnedMercenaryResolutionCode::InvalidState;
    std::string error;
    const Character *pCharacter = nullptr;
    OwnedMercenaryOwner owner;
    std::optional<uint32_t> sourceRosterId;
    std::optional<uint32_t> npcId;
};

OwnedMercenaryResolution resolveOwnedMercenary(
    const Party &party, uint64_t memberId, const BreachMercenaryProfile &profile);

enum class OwnedMercenaryJoinPreparationCode : uint8_t
{
    Prepared,
    Full,
    AlreadyActive,
    Released,
    NotRetained,
    UnknownRoster,
    AmbiguousRoster,
    Stale,
    UnsupportedOwner,
    InvalidState,
    MutationRejected,
};

struct OwnedMercenaryCompanionReturn
{
    uint32_t npcId = 0; // Custom split NPC772..775, distinct from the mercenary's source roster+261 NPC.
    BreachAlvarCompanionReceipt receipt;
    BreachAlvarCompanionNpcs npcs;
    BreachAlvarFriendActorBinding existingFriendActor;
};

struct OwnedMercenaryJoinPreparation;

// Exactly one disposable Party candidate. Only Session may publish it together with the prepared world/event
// changes after rechecking its offer, scene generation, source actor and complete Party/receipt baselines.
class PreparedOwnedMercenaryJoin
{
public:
    PreparedOwnedMercenaryJoin(const PreparedOwnedMercenaryJoin &) = delete;
    PreparedOwnedMercenaryJoin &operator=(const PreparedOwnedMercenaryJoin &) = delete;
    PreparedOwnedMercenaryJoin(PreparedOwnedMercenaryJoin &&) = default;
    PreparedOwnedMercenaryJoin &operator=(PreparedOwnedMercenaryJoin &&) = default;

    const Party &party() const;
    const OwnedMercenaryIdentity &identity() const;
    uint32_t npcId() const;
    const std::optional<OwnedMercenaryCompanionReturn> &companionReturn() const;

private:
    friend class GameSession;
    friend OwnedMercenaryJoinPreparation prepareOwnedMercenaryJoin(
        const Party &, const OwnedMercenaryIdentity &, const BreachMercenaryProfile &,
        const std::optional<BreachAlvarCompanionState> &, const CharacterDollTable &,
        const BreachAlvarCompanionTopicFacts &);

    PreparedOwnedMercenaryJoin(Party &&party, OwnedMercenaryIdentity identity, uint32_t npcId,
        std::optional<OwnedMercenaryCompanionReturn> companionReturn);

    Party m_party;
    OwnedMercenaryIdentity m_identity;
    uint32_t m_npcId = 0;
    std::optional<OwnedMercenaryCompanionReturn> m_companionReturn;
};

struct OwnedMercenaryJoinPreparation
{
    OwnedMercenaryJoinPreparationCode code = OwnedMercenaryJoinPreparationCode::MutationRejected;
    std::string error;
    std::optional<PreparedOwnedMercenaryJoin> mutation;
};

// Does not publish, generate a Character, allocate memberId, heal, refill, grant XP or change retention.
OwnedMercenaryJoinPreparation prepareOwnedMercenaryJoin(
    const Party &party, const OwnedMercenaryIdentity &expected, const BreachMercenaryProfile &profile,
    const std::optional<BreachAlvarCompanionState> &companions, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics);
}
