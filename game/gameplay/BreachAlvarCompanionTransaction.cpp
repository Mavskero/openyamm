#include "game/gameplay/BreachAlvarCompanionTransaction.h"

#include "game/gameplay/SourceMercenaryGenerationRuntime.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/RosterTable.h"

#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
BreachAlvarCompanionPreparation fail(BreachAlvarCompanionPreparationCode code, std::string error)
{
    return {code, std::move(error), std::nullopt};
}

BreachAlvarMemberView memberView(const Character &member)
{
    return {member.memberId, member.rosterId, member.experience, member.level};
}

void applyNpcCustodyProjection(Party &party, const BreachAlvarCompanionNpcs &npcs, bool initializeGreetings)
{
    for (const BreachAlvarCompanionNpcProjection &npc : npcs)
    {
        party.setNpcHouseOverride(npc.npcId, npc.houseId);
        if (initializeGreetings)
        {
            party.setNpcGreetingOverride(npc.npcId, npc.greetingId);
        }
        for (size_t slot = 0; slot < npc.topicIds.size(); ++slot)
        {
            party.setNpcTopicOverride(npc.npcId, uint32_t(slot), npc.topicIds[slot]);
        }
    }
}
}

PreparedBreachAlvarCompanionMutation::PreparedBreachAlvarCompanionMutation(
    BreachAlvarCompanionMutationKind kind, uint32_t returningNpcId, Party &&party,
    BreachAlvarCompanionReceipt receipt, BreachAlvarCompanionNpcs npcs,
    std::optional<BreachAlvarFriendActorBinding> existingFriendActor,
    std::vector<uint32_t> portraitHistoryAfter, bool portraitHistoryChanged)
    : m_kind(kind)
    , m_returningNpcId(returningNpcId)
    , m_party(std::move(party))
    , m_receipt(std::move(receipt))
    , m_npcs(std::move(npcs))
    , m_existingFriendActor(std::move(existingFriendActor))
    , m_portraitHistoryAfter(std::move(portraitHistoryAfter))
    , m_portraitHistoryChanged(portraitHistoryChanged)
{
}

BreachAlvarCompanionMutationKind PreparedBreachAlvarCompanionMutation::kind() const
{
    return m_kind;
}
uint32_t PreparedBreachAlvarCompanionMutation::returningNpcId() const
{
    return m_returningNpcId;
}
const Party &PreparedBreachAlvarCompanionMutation::party() const
{
    return m_party;
}
const BreachAlvarCompanionReceipt &PreparedBreachAlvarCompanionMutation::receipt() const
{
    return m_receipt;
}
const BreachAlvarCompanionNpcs &PreparedBreachAlvarCompanionMutation::npcs() const
{
    return m_npcs;
}
const std::optional<BreachAlvarFriendActorBinding> &PreparedBreachAlvarCompanionMutation::existingFriendActor() const
{
    return m_existingFriendActor;
}
const std::vector<uint32_t> &PreparedBreachAlvarCompanionMutation::portraitHistoryAfter() const
{
    return m_portraitHistoryAfter;
}
bool PreparedBreachAlvarCompanionMutation::portraitHistoryChanged() const
{
    return m_portraitHistoryChanged;
}

std::optional<BreachAlvarCompanionNpcs> projectBreachAlvarCompanionNpcs(
    const Party &party, const BreachAlvarCompanionReceipt &receipt, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, std::string &error)
{
    if (!validBreachAlvarCompanionReceipt(receipt, party.snapshot(), error))
    {
        return std::nullopt;
    }
    std::unordered_set<uint32_t> hints;
    for (uint32_t hint : topics.hintByNpc)
    {
        if (hint > 4 || (hint != 0 && !hints.insert(hint).second))
        {
            error = "Breach Alvar companion projection: invalid or duplicate hint assignment";
            return std::nullopt;
        }
    }

    BreachAlvarCompanionNpcs result = {};
    for (size_t index = 0; index < result.size(); ++index)
    {
        BreachAlvarCompanionNpcProjection &npc = result[index];
        const BreachAlvarCompanionReceiptEntry &entry = receipt.companions[index];
        npc.npcId = uint32_t(772 + index);
        npc.memberId = entry.memberId;
        if (entry.returned)
        {
            continue;
        }
        const Party::DetachedPartyMember *pDetached =
            party.detachedMember(std::string(BreachAlvarSplitOwner), npc.npcId);
        if (pDetached == nullptr || pDetached->character.memberId != entry.memberId)
        {
            error = "Breach Alvar companion projection: detached identity changed";
            return std::nullopt;
        }
        const CharacterDollEntry *pDoll = dolls.getCharacter(pDetached->character.characterDataId);
        if (pDoll == nullptr || pDoll->npcPictureId == 0)
        {
            error = "Breach Alvar companion projection: missing character doll NPC picture";
            return std::nullopt;
        }
        npc.name = pDetached->character.name;
        npc.pictureId = pDoll->npcPictureId;
        npc.houseId = index == 0 ? 0 : uint32_t(704 + index);
        npc.available = !topics.questFinished;
        npc.topicIds[0] = npc.available ? (topics.hintByNpc[index] == 0 ? 1793 : 1789) : 0;
    }
    return result;
}

BreachAlvarCompanionPreparation prepareBreachAlvarSplit(
    const Party &party, const std::optional<BreachAlvarCompanionState> &state, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, uint64_t seed,
    const BreachAlvarSplitGenerationContext &context)
{
    const Party::Snapshot before = party.snapshot();
    std::string error;
    if (!Party::validMemberOwnership(before, error))
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidParty, std::move(error));
    }
    if (!validBreachAlvarCompanionState(state, before, error))
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidState, std::move(error));
    }
    if (state)
    {
        return fail(BreachAlvarCompanionPreparationCode::AlreadySplit, "Breach Alvar companions were already split");
    }
    if (before.members.empty() || before.members.size() > 5)
    {
        return fail(BreachAlvarCompanionPreparationCode::UnsupportedPartySize,
            "Breach Alvar companion preparation requires one to five active members");
    }

    BreachAlvarSplitInput input;
    input.nextMemberId = before.nextMemberId;
    std::array<const Character *, 4> reserveMembers = {};
    const auto rememberReserve = [&reserveMembers](const Character &member)
    {
        if (member.rosterId < 38 || member.rosterId > 41) return true;
        const size_t slot = member.rosterId - 38;
        if (reserveMembers[slot] != nullptr) return false;
        reserveMembers[slot] = &member;
        return true;
    };
    for (const Character &member : before.members)
    {
        input.activeMembers.push_back(memberView(member));
        if (!rememberReserve(member))
            return fail(BreachAlvarCompanionPreparationCode::InvalidParty, "Duplicate reserve roster ownership");
    }
    for (const AdventurersInnMember &member : before.adventurersInnMembers)
    {
        input.innMembers.push_back(memberView(member.character));
        if (!rememberReserve(member.character))
            return fail(BreachAlvarCompanionPreparationCode::InvalidParty, "Duplicate reserve roster ownership");
    }
    for (const Party::DetachedPartyMember &member : before.detachedMembers)
    {
        input.detachedMembers.push_back({member.ownerKey, member.slotId, memberView(member.character)});
        if (!rememberReserve(member.character))
            return fail(BreachAlvarCompanionPreparationCode::InvalidParty, "Duplicate reserve roster ownership");
    }
    for (size_t slot = 0; slot < input.reserves.size(); ++slot)
    {
        if (before.members.size() > 1 && slot < before.members.size() - 1) continue;
        BreachAlvarReserveEvidence &evidence = input.reserves[slot];
        if (reserveMembers[slot] != nullptr)
        {
            evidence.kind = BreachAlvarReserveKind::OwnedMember;
            evidence.memberId = reserveMembers[slot]->memberId;
        }
        else
        {
            const RosterEntry *pRoster = context.pRosterTable != nullptr
                ? context.pRosterTable->get(uint32_t(38 + slot)) : nullptr;
            if (pRoster == nullptr || pRoster->id != 38 + slot || pRoster->level == 0)
            {
                return fail(BreachAlvarCompanionPreparationCode::InvalidParty,
                    "Required unowned reserve has no verified source roster template");
            }
            evidence.kind = BreachAlvarReserveKind::UninstantiatedTemplate;
            evidence.templateExperience = pRoster->experience;
            evidence.templateLevel = pRoster->level;
        }
    }
    const std::optional<BreachAlvarSplitPlan> plan = planBreachAlvarSplit(input, seed, error);
    if (!plan)
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidParty, std::move(error));
    }

    BreachAlvarCompanionReceipt receipt;
    receipt.seed = plan->seed;
    receipt.finalRandomState = plan->finalRandomState;
    receipt.randomDrawCount = plan->randomDrawCount;
    receipt.leaderMemberId = plan->leaderMemberId;
    receipt.initialActiveCount = uint32_t(before.members.size());
    receipt.leaderLevelAtSplit = before.members.front().level;
    std::vector<Party::DetachmentAssignment> assignments;
    std::vector<Party::ReservedDetachment> reserves;
    std::vector<uint32_t> portraitHistory = context.portraitHistory;
    uint64_t newMemberCount = 0;
    for (size_t index = 0; index < plan->companions.size(); ++index)
    {
        const BreachAlvarCompanionPlan &companion = plan->companions[index];
        const BreachAlvarTransferSource &source = companion.source;
        if (companion.npcId != 772 + index)
        {
            return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
                "Split plan changed the source NPC order");
        }
        if (source.kind == BreachAlvarTransferKind::ActiveMember)
        {
            if (companion.generation || !source.activeMemberIndex || *source.activeMemberIndex == 0
                || *source.activeMemberIndex >= before.members.size()
                || before.members[*source.activeMemberIndex].memberId != source.memberId
                || before.members[*source.activeMemberIndex].rosterId != source.rosterId)
            {
                return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
                    "Split plan changed an active member identity");
            }
            assignments.push_back({*source.activeMemberIndex, companion.npcId});
            continue;
        }

        const Character *pBaseline = nullptr;
        std::optional<Character> templateCharacter;
        switch (source.kind)
        {
            case BreachAlvarTransferKind::InnMember:
                if (source.innMemberIndex && *source.innMemberIndex < before.adventurersInnMembers.size())
                    pBaseline = &before.adventurersInnMembers[*source.innMemberIndex].character;
                break;
            case BreachAlvarTransferKind::DetachedMember:
                for (const Party::DetachedPartyMember &member : before.detachedMembers)
                {
                    if (member.ownerKey == source.detachmentOwnerKey && member.slotId == source.detachmentSlotId)
                    {
                        pBaseline = &member.character;
                        break;
                    }
                }
                break;
            case BreachAlvarTransferKind::UninstantiatedTemplate:
            {
                const RosterEntry *pRoster = context.pRosterTable != nullptr
                    ? context.pRosterTable->get(source.rosterId) : nullptr;
                if (pRoster != nullptr && source.memberId == 0 && pRoster->id == 38 + index)
                {
                    templateCharacter = party.makeRosterCharacter(*pRoster);
                    pBaseline = &*templateCharacter;
                    ++newMemberCount;
                }
                break;
            }
            case BreachAlvarTransferKind::ActiveMember:
                break;
        }
        if (pBaseline == nullptr || pBaseline->memberId != source.memberId
            || pBaseline->rosterId != source.rosterId || source.rosterId != 38 + index
            || (templateCharacter && (pBaseline->experience != input.reserves[index].templateExperience
                || pBaseline->level != input.reserves[index].templateLevel)))
        {
            return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
                "Split reserve no longer matches its full Party ownership source");
        }

        Party::ReservedDetachment reserve;
        reserve.slotId = companion.npcId;
        reserve.memberId = source.memberId;
        reserve.generated = companion.generation.has_value();
        if (companion.generation)
        {
            if (context.pTables == nullptr)
            {
                return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
                    "Required reserve generation tables are absent");
            }
            if (portraitHistory.empty())
            {
                const uint32_t leaderDollId = before.members.front().characterDataId;
                if (leaderDollId == 0 || dolls.getCharacter(leaderDollId) == nullptr)
                {
                    return fail(BreachAlvarCompanionPreparationCode::InvalidPresentation,
                        "Source mercenary portrait history has no valid leader Face");
                }
                portraitHistory.push_back(leaderDollId - 1);
            }
            const BreachAlvarGenerationRequest &generation = *companion.generation;
            std::optional<SourceMercenaryGenerationResult> generated = prepareSourceMercenary(
                *pBaseline, generation.rosterId, generation.level, generation.seed, context.currentSwitch,
                portraitHistory, *context.pTables, error);
            if (!generated)
                return fail(BreachAlvarCompanionPreparationCode::MutationRejected, std::move(error));
            if (generated->character.memberId != source.memberId
                || generated->character.rosterId != source.rosterId || generated->character.level != generation.level)
            {
                return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
                    "Source mercenary generation changed its requested identity or level");
            }
            receipt.companions[index].generation = BreachAlvarCompanionGenerationReceipt{
                generation, generated->finalRandomState, generated->randomDrawCount};
            portraitHistory = std::move(generated->portraitHistory);
            reserve.replacement = std::move(generated->character);
        }
        else if (templateCharacter)
        {
            reserve.replacement = std::move(*templateCharacter);
        }
        reserves.push_back(std::move(reserve));
    }
    Party candidate = party;
    if (!candidate.detachMembersWithReserves(std::string(BreachAlvarSplitOwner), assignments, reserves))
    {
        return fail(BreachAlvarCompanionPreparationCode::MutationRejected, "Party rejected companion detachment");
    }
    const Party::Snapshot after = candidate.snapshot();
    if (after.members.size() != 1 || after.members.front().memberId != receipt.leaderMemberId
        || before.nextMemberId > std::numeric_limits<uint64_t>::max() - newMemberCount
        || after.nextMemberId != before.nextMemberId + newMemberCount)
    {
        return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
            "Split candidate changed the leader or allocated unexpected identities");
    }
    for (size_t index = 0; index < receipt.companions.size(); ++index)
    {
        const Party::DetachedPartyMember *pDetached =
            candidate.detachedMember(std::string(BreachAlvarSplitOwner), uint32_t(772 + index));
        const uint64_t sourceMemberId = plan->companions[index].source.memberId;
        if (pDetached == nullptr || (sourceMemberId != 0 && pDetached->character.memberId != sourceMemberId)
            || (sourceMemberId == 0 && pDetached->character.memberId < before.nextMemberId))
        {
            return fail(BreachAlvarCompanionPreparationCode::MutationRejected,
                "Split candidate did not retain the planned companion identity");
        }
        receipt.companions[index].memberId = pDetached->character.memberId;
    }
    const std::optional<BreachAlvarCompanionNpcs> npcs =
        projectBreachAlvarCompanionNpcs(candidate, receipt, dolls, topics, error);
    if (!npcs)
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidPresentation, std::move(error));
    }
    applyNpcCustodyProjection(candidate, *npcs, true);
    if (plan->setMissingFriendsQuestBit) candidate.setQuestBit(1714, true);
    const bool historyChanged = portraitHistory != context.portraitHistory;
    BreachAlvarCompanionPreparation result;
    result.code = BreachAlvarCompanionPreparationCode::Prepared;
    result.mutation = PreparedBreachAlvarCompanionMutation(
        BreachAlvarCompanionMutationKind::Split, 0, std::move(candidate), std::move(receipt), *npcs, std::nullopt,
        std::move(portraitHistory), historyChanged);
    return result;
}

BreachAlvarCompanionPreparation prepareBreachAlvarFiveMemberSplit(
    const Party &party, const std::optional<BreachAlvarCompanionState> &state, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, uint64_t seed)
{
    if (party.memberCount() != 5)
    {
        const Party::Snapshot before = party.snapshot();
        std::string error;
        if (!Party::validMemberOwnership(before, error))
            return fail(BreachAlvarCompanionPreparationCode::InvalidParty, std::move(error));
        if (!validBreachAlvarCompanionState(state, before, error))
            return fail(BreachAlvarCompanionPreparationCode::InvalidState, std::move(error));
        if (state)
            return fail(BreachAlvarCompanionPreparationCode::AlreadySplit,
                "Breach Alvar companions were already split");
        return fail(BreachAlvarCompanionPreparationCode::UnsupportedPartySize,
            "Five-member split wrapper requires five existing active members");
    }
    return prepareBreachAlvarSplit(party, state, dolls, topics, seed);
}

BreachAlvarCompanionPreparation prepareBreachAlvarCompanionReturn(
    const Party &party, const std::optional<BreachAlvarCompanionState> &state, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics, uint32_t npcId)
{
    std::string error;
    if (!Party::validMemberOwnership(party.snapshot(), error))
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidParty, std::move(error));
    }
    if (!validBreachAlvarCompanionState(state, party.snapshot(), error))
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidState, std::move(error));
    }
    if (!state)
    {
        return fail(BreachAlvarCompanionPreparationCode::NotSplit, "No Breach Alvar companion state");
    }
    if (npcId < 772 || npcId > 775)
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidNpc, "Not a Breach Alvar companion NPC");
    }
    if (state->receipt.companions[npcId - 772].returned)
    {
        return fail(BreachAlvarCompanionPreparationCode::AlreadyReturned, "Companion already returned");
    }
    if (party.isFull())
    {
        return fail(BreachAlvarCompanionPreparationCode::PartyFull, "The party is full");
    }
    Party candidate = party;
    BreachAlvarCompanionReceipt receipt = state->receipt;
    if (!candidate.joinDetachedMember(std::string(BreachAlvarSplitOwner), npcId))
    {
        return fail(BreachAlvarCompanionPreparationCode::MutationRejected, "Party rejected companion return");
    }
    receipt.companions[npcId - 772].returned = true;
    const std::optional<BreachAlvarCompanionNpcs> npcs =
        projectBreachAlvarCompanionNpcs(candidate, receipt, dolls, topics, error);
    if (!npcs)
    {
        return fail(BreachAlvarCompanionPreparationCode::InvalidPresentation, std::move(error));
    }
    applyNpcCustodyProjection(candidate, *npcs, false);
    BreachAlvarCompanionPreparation result;
    result.code = BreachAlvarCompanionPreparationCode::Prepared;
    result.mutation = PreparedBreachAlvarCompanionMutation(BreachAlvarCompanionMutationKind::Return, npcId,
        std::move(candidate), std::move(receipt), *npcs, state->friendActor);
    return result;
}
}
