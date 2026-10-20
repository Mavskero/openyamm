#include "game/content/BreachAlvarCompanionState.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
bool fail(std::string &error, const char *pMessage)
{
    error = std::string("Breach Alvar companion state: ") + pMessage;
    return false;
}
}

bool validBreachAlvarCompanionReceipt(
    const BreachAlvarCompanionReceipt &receipt, const Party::Snapshot &party, std::string &error)
{
    error.clear();
    if (!Party::validMemberOwnership(party, error))
    {
        return false;
    }
    if (receipt.profileVersion != BreachAlvarCompanionProfileVersion
        || receipt.planVersion != BreachAlvarSplitPlanVersion)
    {
        return fail(error, "unsupported profile or plan version");
    }
    std::array<std::optional<BreachAlvarGenerationRequest>, 4> generations = {};
    for (size_t index = 0; index < receipt.companions.size(); ++index)
    {
        const auto &generation = receipt.companions[index].generation;
        if (!generation) continue;
        generations[index] = generation->request;
        if (generation->randomDrawCount == 0 || generation->finalRandomState
            != generation->request.seed + generation->randomDrawCount * 0x9e3779b97f4a7c15ULL)
        {
            return fail(error, "invalid generated mercenary random stream receipt");
        }
    }
    if (!validateBreachAlvarSplitRandomReceipt(receipt.seed, receipt.initialActiveCount,
            receipt.leaderLevelAtSplit, generations, receipt.finalRandomState, receipt.randomDrawCount, error))
    {
        return false;
    }
    if (receipt.leaderMemberId == 0 || receipt.leaderMemberId >= party.nextMemberId)
    {
        return fail(error, "invalid historical leader identity");
    }
    if (std::find(party.completedDetachmentOwners.begin(), party.completedDetachmentOwners.end(),
            BreachAlvarSplitOwner) == party.completedDetachmentOwners.end())
    {
        return fail(error, "missing Party detachment receipt");
    }

    std::unordered_set<uint64_t> identities{receipt.leaderMemberId};
    for (size_t index = 0; index < receipt.companions.size(); ++index)
    {
        const BreachAlvarCompanionReceiptEntry &entry = receipt.companions[index];
        if (entry.memberId == 0 || entry.memberId >= party.nextMemberId || !identities.insert(entry.memberId).second)
        {
            return fail(error, "invalid or repeated companion identity");
        }
        const uint32_t npcId = uint32_t(772 + index);
        const auto found = std::find_if(party.detachedMembers.begin(), party.detachedMembers.end(),
            [npcId](const Party::DetachedPartyMember &member)
            { return member.ownerKey == BreachAlvarSplitOwner && member.slotId == npcId; });
        if (entry.returned)
        {
            if (found != party.detachedMembers.end())
            {
                return fail(error, "returned companion still occupies the split slot");
            }
            // A historical return survives ordinary dismissal, another lawful owner and replacement.
        }
        else if (found == party.detachedMembers.end() || found->character.memberId != entry.memberId)
        {
            return fail(error, "missing companion has no matching detached identity");
        }
        else if ((receipt.initialActiveCount == 1 || index >= receipt.initialActiveCount - 1)
            && found->character.rosterId != 38 + index)
        {
            return fail(error, "missing reserve companion has a different source roster");
        }
    }
    for (const Party::DetachedPartyMember &member : party.detachedMembers)
    {
        if (member.ownerKey == BreachAlvarSplitOwner && (member.slotId < 772 || member.slotId > 775))
        {
            return fail(error, "unexpected slot in the split batch");
        }
    }
    return true;
}

bool validBreachAlvarCompanionState(
    const std::optional<BreachAlvarCompanionState> &state, const Party::Snapshot &party, std::string &error)
{
    error.clear();
    if (!Party::validMemberOwnership(party, error))
    {
        return false;
    }
    if (!state)
    {
        const bool hasOwner = std::find(party.completedDetachmentOwners.begin(), party.completedDetachmentOwners.end(),
            BreachAlvarSplitOwner) != party.completedDetachmentOwners.end();
        const bool hasSlot = std::any_of(party.detachedMembers.begin(), party.detachedMembers.end(),
            [](const Party::DetachedPartyMember &member) { return member.ownerKey == BreachAlvarSplitOwner; });
        return (!hasOwner && !hasSlot) || fail(error, "Party split ownership is missing its Session state");
    }
    if (!validBreachAlvarCompanionReceipt(state->receipt, party, error))
    {
        return false;
    }
    if (state->friendActor.canonicalMapKey != BreachAlvarCompanionMap
        || state->friendActor.spawnMonsterId == 0
        || state->friendActor.spawnMonsterId > uint32_t(std::numeric_limits<int16_t>::max()))
    {
        return fail(error, "invalid persistent friend actor binding");
    }
    return true;
}
}
