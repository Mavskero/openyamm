#pragma once

#include "game/content/BreachAlvarCompanionState.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class CharacterDollTable;
class GameSession;
class RosterTable;
struct SourceMercenaryGenerationTables;
struct BreachMercenaryProfile;
struct OwnedMercenaryJoinPreparation;

struct BreachAlvarSplitGenerationContext
{
    const RosterTable *pRosterTable = nullptr;
    const SourceMercenaryGenerationTables *pTables = nullptr;
    uint32_t currentSwitch = 0;
    std::vector<uint32_t> portraitHistory;
};

// Read-only inputs from the existing global quest variables; not a second saved hint registry.
struct BreachAlvarCompanionTopicFacts
{
    bool questFinished = false;
    std::array<uint32_t, 4> hintByNpc = {};
};

struct BreachAlvarCompanionNpcProjection
{
    uint32_t npcId = 0;
    uint64_t memberId = 0;
    uint32_t houseId = 0;
    uint32_t greetingId = 0;
    std::array<uint32_t, 6> topicIds = {};
    bool available = false;
    // Empty optionals mean erase appearance overrides after returning, not a new character template.
    std::optional<std::string> name;
    std::optional<uint32_t> pictureId;

    bool operator==(const BreachAlvarCompanionNpcProjection &) const = default;
};

using BreachAlvarCompanionNpcs = std::array<BreachAlvarCompanionNpcProjection, 4>;

enum class BreachAlvarCompanionPreparationCode
{
    Prepared,
    UnsupportedPartySize,
    InvalidParty,
    InvalidState,
    AlreadySplit,
    NotSplit,
    InvalidNpc,
    AlreadyReturned,
    PartyFull,
    InvalidPresentation,
    MutationRejected,
};

enum class BreachAlvarCompanionMutationKind
{
    Split,
    Return,
};

struct BreachAlvarCompanionPreparation;

// A disposable Party candidate, not a live operation or a persistable Session state.
// Only the later Session coordinator may publish it, after real scene/context authorization,
// actor prepare/publish, and stale Party/world/receipt checks. There is deliberately no public commit.
class PreparedBreachAlvarCompanionMutation
{
public:
    PreparedBreachAlvarCompanionMutation(const PreparedBreachAlvarCompanionMutation &) = default;
    PreparedBreachAlvarCompanionMutation(PreparedBreachAlvarCompanionMutation &&) = default;
    PreparedBreachAlvarCompanionMutation &operator=(const PreparedBreachAlvarCompanionMutation &) = default;
    PreparedBreachAlvarCompanionMutation &operator=(PreparedBreachAlvarCompanionMutation &&) = default;

    BreachAlvarCompanionMutationKind kind() const;
    uint32_t returningNpcId() const;
    const Party &party() const;
    const BreachAlvarCompanionReceipt &receipt() const;
    const BreachAlvarCompanionNpcs &npcs() const;
    const std::optional<BreachAlvarFriendActorBinding> &existingFriendActor() const;
    const std::vector<uint32_t> &portraitHistoryAfter() const;
    bool portraitHistoryChanged() const;

private:
    friend class GameSession;
    friend BreachAlvarCompanionPreparation prepareBreachAlvarFiveMemberSplit(
        const Party &, const std::optional<BreachAlvarCompanionState> &, const CharacterDollTable &,
        const BreachAlvarCompanionTopicFacts &, uint64_t);
    friend BreachAlvarCompanionPreparation prepareBreachAlvarSplit(
        const Party &, const std::optional<BreachAlvarCompanionState> &, const CharacterDollTable &,
        const BreachAlvarCompanionTopicFacts &, uint64_t, const BreachAlvarSplitGenerationContext &);
    friend BreachAlvarCompanionPreparation prepareBreachAlvarCompanionReturn(
        const Party &, const std::optional<BreachAlvarCompanionState> &, const CharacterDollTable &,
        const BreachAlvarCompanionTopicFacts &, uint32_t);
    friend OwnedMercenaryJoinPreparation prepareOwnedMercenaryJoin(
        const Party &, const OwnedMercenaryIdentity &, const BreachMercenaryProfile &,
        const std::optional<BreachAlvarCompanionState> &, const CharacterDollTable &,
        const BreachAlvarCompanionTopicFacts &);

    PreparedBreachAlvarCompanionMutation(
        BreachAlvarCompanionMutationKind kind, uint32_t returningNpcId, Party &&party,
        BreachAlvarCompanionReceipt receipt, BreachAlvarCompanionNpcs npcs,
        std::optional<BreachAlvarFriendActorBinding> existingFriendActor,
        std::vector<uint32_t> portraitHistoryAfter = {}, bool portraitHistoryChanged = false);

    BreachAlvarCompanionMutationKind m_kind;
    uint32_t m_returningNpcId = 0;
    Party m_party;
    BreachAlvarCompanionReceipt m_receipt;
    BreachAlvarCompanionNpcs m_npcs;
    std::optional<BreachAlvarFriendActorBinding> m_existingFriendActor;
    std::vector<uint32_t> m_portraitHistoryAfter;
    bool m_portraitHistoryChanged = false;
};

struct BreachAlvarCompanionPreparation
{
    BreachAlvarCompanionPreparationCode code = BreachAlvarCompanionPreparationCode::MutationRejected;
    std::string error;
    std::optional<PreparedBreachAlvarCompanionMutation> mutation;
};

std::optional<BreachAlvarCompanionNpcs> projectBreachAlvarCompanionNpcs(
    const Party &party, const BreachAlvarCompanionReceipt &receipt, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, std::string &error);

// Input Party, receipt and caller RNG are never modified. Prepared does not mean actor or Session success.
BreachAlvarCompanionPreparation prepareBreachAlvarSplit(
    const Party &party, const std::optional<BreachAlvarCompanionState> &state, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, uint64_t seed,
    const BreachAlvarSplitGenerationContext &context = {});
BreachAlvarCompanionPreparation prepareBreachAlvarFiveMemberSplit(
    const Party &party, const std::optional<BreachAlvarCompanionState> &state, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, uint64_t seed);
BreachAlvarCompanionPreparation prepareBreachAlvarCompanionReturn(
    const Party &party, const std::optional<BreachAlvarCompanionState> &state, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, uint32_t npcId);
}
