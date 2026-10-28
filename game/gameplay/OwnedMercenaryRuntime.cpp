#include "game/gameplay/OwnedMercenaryRuntime.h"

#include "game/content/BreachMercenaryProfile.h"

#include <algorithm>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
std::optional<uint32_t> sourceAlias(const Character &member, const BreachMercenaryProfile &profile)
{
    const std::optional<uint32_t> alias = member.sourceMercenaryRetention.sourceRosterId
        ? member.sourceMercenaryRetention.sourceRosterId
        : (member.rosterId != 0 ? std::optional<uint32_t>(member.rosterId) : std::nullopt);
    if (!alias || !breachMercenaryNpcId(profile, *alias)) return std::nullopt;
    return alias;
}

bool validOwner(const OwnedMercenaryOwner &owner)
{
    switch (owner.kind)
    {
        case OwnedMercenaryCustody::Active:
        case OwnedMercenaryCustody::AdventurersInn:
            return owner.ownerKey.empty() && owner.slotId == 0;
        case OwnedMercenaryCustody::Detached:
            return !owner.ownerKey.empty() && owner.slotId != 0;
    }
    return false;
}

OwnedMercenaryJoinPreparation fail(OwnedMercenaryJoinPreparationCode code, std::string error)
{
    return {code, std::move(error), std::nullopt};
}

OwnedMercenaryJoinPreparationCode preparationCode(OwnedMercenaryResolutionCode code)
{
    switch (code)
    {
        case OwnedMercenaryResolutionCode::AlreadyActive:
            return OwnedMercenaryJoinPreparationCode::AlreadyActive;
        case OwnedMercenaryResolutionCode::Released:
            return OwnedMercenaryJoinPreparationCode::Released;
        case OwnedMercenaryResolutionCode::NotRetained:
            return OwnedMercenaryJoinPreparationCode::NotRetained;
        case OwnedMercenaryResolutionCode::UnknownRoster:
            return OwnedMercenaryJoinPreparationCode::UnknownRoster;
        case OwnedMercenaryResolutionCode::AmbiguousRoster:
            return OwnedMercenaryJoinPreparationCode::AmbiguousRoster;
        case OwnedMercenaryResolutionCode::Stale:
            return OwnedMercenaryJoinPreparationCode::Stale;
        case OwnedMercenaryResolutionCode::UnsupportedOwner:
            return OwnedMercenaryJoinPreparationCode::UnsupportedOwner;
        case OwnedMercenaryResolutionCode::InvalidState:
        case OwnedMercenaryResolutionCode::Eligible:
            return OwnedMercenaryJoinPreparationCode::InvalidState;
    }
    return OwnedMercenaryJoinPreparationCode::InvalidState;
}
}

OwnedMercenaryResolution resolveOwnedMercenary(
    const Party &party, uint64_t memberId, const BreachMercenaryProfile &profile)
{
    OwnedMercenaryResolution result;
    if (!validBreachMercenaryProfile(profile, result.error)
        || !Party::validMemberOwnership(party.snapshot(), result.error)) return result;
    const auto consider = [&](const Character &member, OwnedMercenaryOwner owner)
    {
        if (member.memberId != memberId) return;
        result.pCharacter = &member;
        result.owner = std::move(owner);
    };
    for (const Character &member : party.members())
        consider(member, {OwnedMercenaryCustody::Active, {}, 0});
    for (const AdventurersInnMember &member : party.adventurersInnMembers())
        consider(member.character, {OwnedMercenaryCustody::AdventurersInn, {}, 0});
    for (const Party::DetachedPartyMember &member : party.detachedMembers())
        consider(member.character, {OwnedMercenaryCustody::Detached, member.ownerKey, member.slotId});
    if (result.pCharacter == nullptr)
    {
        result.code = OwnedMercenaryResolutionCode::Stale;
        result.error = "Owned mercenary identity no longer exists";
        return result;
    }

    const Character &member = *result.pCharacter;
    result.sourceRosterId = sourceAlias(member, profile);
    if (result.sourceRosterId) result.npcId = breachMercenaryNpcId(profile, *result.sourceRosterId);
    if (result.owner.kind == OwnedMercenaryCustody::Active)
    {
        result.code = OwnedMercenaryResolutionCode::AlreadyActive;
        result.error = "Owned mercenary is already active";
        return result;
    }
    if (member.sourceMercenaryRetention.kind == SourceMercenaryRetentionKind::Released)
    {
        result.code = OwnedMercenaryResolutionCode::Released;
        result.error = "Owned mercenary was explicitly released";
        return result;
    }
    if (member.sourceMercenaryRetention.kind != SourceMercenaryRetentionKind::Retained)
    {
        result.code = OwnedMercenaryResolutionCode::NotRetained;
        result.error = "Owned mercenary has no proved source retention";
        return result;
    }
    if (!result.sourceRosterId || !result.npcId)
    {
        result.code = OwnedMercenaryResolutionCode::UnknownRoster;
        result.error = "Retained mercenary has no authenticated source roster alias";
        return result;
    }

    bool duplicate = false;
    const auto checkAlias = [&](const Character &other)
    {
        if (other.memberId != memberId && sourceAlias(other, profile) == result.sourceRosterId) duplicate = true;
    };
    for (const Character &other : party.members()) checkAlias(other);
    for (const AdventurersInnMember &other : party.adventurersInnMembers()) checkAlias(other.character);
    for (const Party::DetachedPartyMember &other : party.detachedMembers()) checkAlias(other.character);
    if (duplicate)
    {
        result.code = OwnedMercenaryResolutionCode::AmbiguousRoster;
        result.error = "Owned mercenary source roster alias is shared by another owned Character";
        return result;
    }
    if (result.owner.kind == OwnedMercenaryCustody::Detached
        && (result.owner.ownerKey != BreachAlvarSplitOwner || result.owner.slotId < 772 || result.owner.slotId > 775))
    {
        result.code = OwnedMercenaryResolutionCode::UnsupportedOwner;
        result.error = "Owned mercenary detached owner has no supported return receipt";
        return result;
    }
    result.code = OwnedMercenaryResolutionCode::Eligible;
    return result;
}

PreparedOwnedMercenaryJoin::PreparedOwnedMercenaryJoin(Party &&party, OwnedMercenaryIdentity identity,
    uint32_t npcId, std::optional<OwnedMercenaryCompanionReturn> companionReturn)
    : m_party(std::move(party)), m_identity(std::move(identity)), m_npcId(npcId)
    , m_companionReturn(std::move(companionReturn))
{
}

const Party &PreparedOwnedMercenaryJoin::party() const
{
    return m_party;
}

const OwnedMercenaryIdentity &PreparedOwnedMercenaryJoin::identity() const
{
    return m_identity;
}

uint32_t PreparedOwnedMercenaryJoin::npcId() const
{
    return m_npcId;
}

const std::optional<OwnedMercenaryCompanionReturn> &PreparedOwnedMercenaryJoin::companionReturn() const
{
    return m_companionReturn;
}

OwnedMercenaryJoinPreparation prepareOwnedMercenaryJoin(
    const Party &party, const OwnedMercenaryIdentity &expected, const BreachMercenaryProfile &profile,
    const std::optional<BreachAlvarCompanionState> &companions, const CharacterDollTable &dolls,
    const BreachAlvarCompanionTopicFacts &topics)
{
    if (expected.memberId == 0 || !validOwner(expected.owner))
        return fail(OwnedMercenaryJoinPreparationCode::InvalidState, "Invalid expected owned mercenary identity");
    const OwnedMercenaryResolution owned = resolveOwnedMercenary(party, expected.memberId, profile);
    if (owned.code != OwnedMercenaryResolutionCode::Eligible)
        return fail(preparationCode(owned.code), owned.error);
    if (owned.owner != expected.owner || owned.sourceRosterId != expected.sourceRosterId)
        return fail(OwnedMercenaryJoinPreparationCode::Stale, "Owned mercenary alias or custody changed since offer");

    const Party::Snapshot before = party.snapshot();
    std::string error;
    if (!validBreachAlvarCompanionState(companions, before, error))
        return fail(OwnedMercenaryJoinPreparationCode::InvalidState, std::move(error));
    if (party.isFull() || party.memberCount() >= profile.partyCapacity)
        return fail(OwnedMercenaryJoinPreparationCode::Full, "The party is full");

    std::optional<Party> candidate;
    std::optional<OwnedMercenaryCompanionReturn> companionReturn;
    if (owned.owner.kind == OwnedMercenaryCustody::AdventurersInn)
    {
        const auto &inn = party.adventurersInnMembers();
        const auto found = std::find_if(inn.begin(), inn.end(), [&](const AdventurersInnMember &entry)
            { return entry.character.memberId == expected.memberId; });
        if (found == inn.end() || found->character.rosterId != expected.sourceRosterId)
            return fail(OwnedMercenaryJoinPreparationCode::Stale, "Owned mercenary Inn identity changed");
        candidate.emplace(party);
        if (!candidate->hireAdventurersInnMember(size_t(found - inn.begin())))
            return fail(OwnedMercenaryJoinPreparationCode::MutationRejected, "Party rejected owned Inn return");
    }
    else if (owned.owner.kind == OwnedMercenaryCustody::Detached)
    {
        if (!companions || owned.owner.ownerKey != BreachAlvarSplitOwner
            || owned.owner.slotId < 772 || owned.owner.slotId > 775)
            return fail(OwnedMercenaryJoinPreparationCode::UnsupportedOwner, "Unsupported owned mercenary return");
        const BreachAlvarCompanionReceiptEntry &entry = companions->receipt.companions[owned.owner.slotId - 772];
        if (entry.returned || entry.memberId != expected.memberId)
            return fail(OwnedMercenaryJoinPreparationCode::Stale, "Owned mercenary companion receipt changed");
        BreachAlvarCompanionPreparation returned = prepareBreachAlvarCompanionReturn(
            party, companions, dolls, topics, owned.owner.slotId);
        if (returned.code != BreachAlvarCompanionPreparationCode::Prepared || !returned.mutation)
            return fail(OwnedMercenaryJoinPreparationCode::MutationRejected, std::move(returned.error));
        PreparedBreachAlvarCompanionMutation &mutation = *returned.mutation;
        if (mutation.m_kind != BreachAlvarCompanionMutationKind::Return || !mutation.m_existingFriendActor)
            return fail(OwnedMercenaryJoinPreparationCode::InvalidState, "Incomplete companion return preparation");
        candidate.emplace(std::move(mutation.m_party));
        companionReturn = OwnedMercenaryCompanionReturn{mutation.m_returningNpcId, std::move(mutation.m_receipt),
            std::move(mutation.m_npcs), std::move(*mutation.m_existingFriendActor)};
    }
    else
    {
        return fail(OwnedMercenaryJoinPreparationCode::AlreadyActive, "Owned mercenary is already active");
    }

    candidate->setNpcHouseOverride(*owned.npcId, 0);
    candidate->removeHiredNpcFollower(*owned.npcId);
    const Party::Snapshot after = candidate->snapshot();
    std::optional<BreachAlvarCompanionState> afterCompanions = companions;
    if (companionReturn) afterCompanions->receipt = companionReturn->receipt;
    if (!Party::validMemberOwnership(after, error)
        || !validBreachAlvarCompanionState(afterCompanions, after, error))
        return fail(OwnedMercenaryJoinPreparationCode::InvalidState, std::move(error));
    const Character *pJoined = candidate->member(before.members.size());
    const Character &original = *owned.pCharacter;
    if (after.nextMemberId != before.nextMemberId || after.members.size() != before.members.size() + 1
        || pJoined == nullptr || pJoined->memberId != expected.memberId || pJoined->rosterId != expected.sourceRosterId
        || pJoined->sourceMercenaryRetention != original.sourceMercenaryRetention
        || pJoined->name != original.name || pJoined->biography != original.biography
        || pJoined->experience != original.experience || pJoined->level != original.level
        || pJoined->skillPoints != original.skillPoints || pJoined->knownSpellIds != original.knownSpellIds
        || pJoined->health != original.health || pJoined->spellPoints != original.spellPoints)
        return fail(OwnedMercenaryJoinPreparationCode::MutationRejected,
            "Owned mercenary return changed identity, retention, resources or progression");

    OwnedMercenaryJoinPreparation result;
    result.code = OwnedMercenaryJoinPreparationCode::Prepared;
    result.mutation = PreparedOwnedMercenaryJoin(
        std::move(*candidate), expected, *owned.npcId, std::move(companionReturn));
    return result;
}
}
