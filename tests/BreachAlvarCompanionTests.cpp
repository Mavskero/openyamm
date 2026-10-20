#include "doctest/doctest.h"

#include "game/gameplay/BreachAlvarCompanionTransaction.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/RosterTable.h"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
CharacterDollTable dollTable(size_t count = 5)
{
    std::vector<std::vector<std::string>> rows(1, std::vector<std::string>(24));
    for (size_t index = 1; index <= count; ++index)
    {
        std::vector<std::string> row(24, "0");
        row[0] = std::to_string(index);
        row[22] = std::to_string(200 + index);
        rows.push_back(std::move(row));
    }
    CharacterDollTable dolls;
    REQUIRE(dolls.loadCharacterRows(rows));
    return dolls;
}

InventoryItem item(uint32_t id)
{
    InventoryItem value;
    value.objectDescriptionId = id;
    value.quantity = 1;
    value.width = 1;
    value.height = 1;
    value.stolen = true;
    value.identified = false;
    value.currentCharges = 3;
    value.maxCharges = 9;
    value.temporaryBonusRemainingSeconds = 63.5f;
    return value;
}

Party partyWithMembers(size_t count = 5)
{
    PartySeed seed;
    seed.members.resize(count);
    for (size_t index = 0; index < count; ++index)
    {
        seed.members[index].name = "Friend " + std::to_string(index);
        seed.members[index].className = "Knight";
        seed.members[index].characterDataId = uint32_t(index + 1);
    }
    Party party;
    party.seed(seed);
    Party::Snapshot snapshot = party.snapshot();
    snapshot.gold = 12345;
    snapshot.food = 13;
    snapshot.questBits.insert(123);
    for (size_t index = 0; index < count; ++index)
    {
        Character &member = snapshot.members[index];
        member.portraitPictureId = uint32_t(900 + index); // Deliberately not the NPC picture.
        member.level = uint32_t(10 + index);
        member.experience = uint32_t(1000 + index);
        member.health = int(17 + index);
        member.spellPoints = int(index);
        member.healthRegenAccumulator = 0.25f;
        member.spellRegenAccumulator = 0.75f;
        member.inventory = {item(uint32_t(601 + index))};
        member.awards = {42};
        snapshot.pendingItemDeliveries.push_back({item(uint32_t(620 + index)), int32_t(index), false});
    }
    snapshot.pendingItemDeliveries.push_back({item(630), -1, false});
    party.restoreSnapshot(snapshot);
    party.setHeldItemForQueries(item(631));
    return party;
}

void checkItem(const InventoryItem &a, const InventoryItem &b)
{
    CHECK(a.objectDescriptionId == b.objectDescriptionId);
    CHECK(a.quantity == b.quantity);
    CHECK(a.stolen == b.stolen);
    CHECK(a.identified == b.identified);
    CHECK(a.currentCharges == b.currentCharges);
    CHECK(a.maxCharges == b.maxCharges);
    CHECK(a.temporaryBonusRemainingSeconds == b.temporaryBonusRemainingSeconds);
}

void checkCharacter(const Character &a, const Character &b)
{
    CHECK(a.memberId == b.memberId);
    CHECK(a.rosterId == b.rosterId);
    CHECK(a.characterDataId == b.characterDataId);
    CHECK(a.name == b.name);
    CHECK(a.health == b.health);
    CHECK(a.spellPoints == b.spellPoints);
    CHECK(a.healthRegenAccumulator == b.healthRegenAccumulator);
    CHECK(a.spellRegenAccumulator == b.spellRegenAccumulator);
    CHECK(a.experience == b.experience);
    CHECK(a.awards == b.awards);
    CHECK(a.conditions == b.conditions);
    REQUIRE(a.inventory.size() == b.inventory.size());
    for (size_t index = 0; index < a.inventory.size(); ++index)
    {
        checkItem(a.inventory[index], b.inventory[index]);
    }
}

void checkUnchanged(const Party &party, const Party &before)
{
    const Party::Snapshot a = party.snapshot();
    const Party::Snapshot b = before.snapshot();
    CHECK(a.nextMemberId == b.nextMemberId);
    CHECK(a.activeMemberIndex == b.activeMemberIndex);
    CHECK(a.gold == b.gold);
    CHECK(a.food == b.food);
    CHECK(a.questBits == b.questBits);
    CHECK(a.npcTopicOverrides == b.npcTopicOverrides);
    CHECK(a.npcHouseOverrides == b.npcHouseOverrides);
    CHECK(a.npcGreetingOverrides == b.npcGreetingOverrides);
    CHECK(a.npcGreetingDisplayCounts == b.npcGreetingDisplayCounts);
    CHECK(a.completedDetachmentOwners == b.completedDetachmentOwners);
    CHECK(party.heldItemIdForQueries() == before.heldItemIdForQueries());
    REQUIRE(a.members.size() == b.members.size());
    REQUIRE(a.adventurersInnMembers.size() == b.adventurersInnMembers.size());
    REQUIRE(a.detachedMembers.size() == b.detachedMembers.size());
    for (size_t index = 0; index < a.members.size(); ++index)
    {
        checkCharacter(a.members[index], b.members[index]);
    }
    for (size_t index = 0; index < a.adventurersInnMembers.size(); ++index)
    {
        checkCharacter(a.adventurersInnMembers[index].character, b.adventurersInnMembers[index].character);
    }
    for (size_t index = 0; index < a.detachedMembers.size(); ++index)
    {
        CHECK(a.detachedMembers[index].ownerKey == b.detachedMembers[index].ownerKey);
        CHECK(a.detachedMembers[index].slotId == b.detachedMembers[index].slotId);
        REQUIRE(a.detachedMembers[index].pendingItemDeliveries.size()
            == b.detachedMembers[index].pendingItemDeliveries.size());
        for (size_t pending = 0; pending < a.detachedMembers[index].pendingItemDeliveries.size(); ++pending)
        {
            const auto &left = a.detachedMembers[index].pendingItemDeliveries[pending];
            const auto &right = b.detachedMembers[index].pendingItemDeliveries[pending];
            CHECK(left.recipientMemberIndex == right.recipientMemberIndex);
            CHECK(left.allowDropProtectedHeldItem == right.allowDropProtectedHeldItem);
            checkItem(left.item, right.item);
        }
        checkCharacter(a.detachedMembers[index].character, b.detachedMembers[index].character);
    }
    for (size_t member = 0; member < a.characterBuffs.size(); ++member)
    {
        for (size_t buff = 0; buff < CharacterBuffCount; ++buff)
        {
            const auto &left = a.characterBuffs[member][buff];
            const auto &right = b.characterBuffs[member][buff];
            CHECK(left.remainingSeconds == right.remainingSeconds);
            CHECK(left.periodicAccumulatorSeconds == right.periodicAccumulatorSeconds);
            CHECK(left.casterMemberId == right.casterMemberId);
            CHECK(left.casterMemberIndex == right.casterMemberIndex);
        }
    }
    for (size_t buff = 0; buff < PartyBuffCount; ++buff)
    {
        CHECK(a.partyBuffs[buff].remainingSeconds == b.partyBuffs[buff].remainingSeconds);
        CHECK(a.partyBuffs[buff].casterMemberIndex == b.partyBuffs[buff].casterMemberIndex);
    }
    REQUIRE(a.pendingItemDeliveries.size() == b.pendingItemDeliveries.size());
    for (size_t index = 0; index < a.pendingItemDeliveries.size(); ++index)
    {
        CHECK(a.pendingItemDeliveries[index].recipientMemberIndex
            == b.pendingItemDeliveries[index].recipientMemberIndex);
        checkItem(a.pendingItemDeliveries[index].item, b.pendingItemDeliveries[index].item);
    }
}

PreparedBreachAlvarCompanionMutation split(const Party &party, const CharacterDollTable &dolls, uint64_t seed = 0)
{
    BreachAlvarCompanionPreparation result = prepareBreachAlvarFiveMemberSplit(party, std::nullopt, dolls, {}, seed);
    REQUIRE_MESSAGE(result.code == BreachAlvarCompanionPreparationCode::Prepared, result.error);
    REQUIRE(result.mutation.has_value());
    REQUIRE(result.error.empty());
    return std::move(*result.mutation);
}

// A declared structural reference used to test the kernel. No actor is spawned or mocked as successful.
// Real actor existence, scene authorization and Session/Save publication are NOT tested by this fixture.
BreachAlvarCompanionState structuralState(const PreparedBreachAlvarCompanionMutation &mutation)
{
    BreachAlvarCompanionState state;
    state.receipt = mutation.receipt();
    state.friendActor = {std::string(BreachAlvarCompanionMap), 0, 261};
    return state;
}

void checkRejected(const BreachAlvarCompanionPreparation &result, BreachAlvarCompanionPreparationCode code)
{
    CHECK(result.code == code);
    CHECK_FALSE(result.mutation.has_value());
    CHECK_FALSE(result.error.empty());
}

const Character &originalById(const Party &original, uint64_t id)
{
    const auto found = std::find_if(original.members().begin(), original.members().end(),
        [id](const Character &member) { return member.memberId == id; });
    REQUIRE(found != original.members().end());
    return *found;
}
}

TEST_CASE("BrAlvar candidate split preserves five real identities and source order for all four selections")
{
    const CharacterDollTable dolls = dollTable();
    const Party original = partyWithMembers();
    std::set<uint64_t> selected;
    for (uint64_t seed = 0; seed < 64; ++seed)
    {
        const auto prepared = split(original, dolls, seed);
        CHECK(prepared.kind() == BreachAlvarCompanionMutationKind::Split);
        CHECK_FALSE(prepared.existingFriendActor().has_value());
        CHECK(prepared.receipt() == split(original, dolls, seed).receipt());
        const Party &candidate = prepared.party();
        REQUIRE(candidate.members().size() == 1);
        CHECK(candidate.member(0)->memberId == original.member(0)->memberId);
        CHECK(candidate.snapshot().nextMemberId == original.snapshot().nextMemberId);
        CHECK(candidate.hasQuestBit(1714));
        CHECK(candidate.hasQuestBit(123));
        CHECK_FALSE(original.hasQuestBit(1714));
        CHECK(candidate.heldItemIdForQueries() == 631);
        CHECK(candidate.gold() == original.gold());
        const uint64_t first = prepared.receipt().companions[0].memberId;
        selected.insert(first);
        std::vector<uint64_t> remaining;
        for (size_t slot = 1; slot < 5; ++slot)
        {
            if (original.member(slot)->memberId != first)
            {
                remaining.push_back(original.member(slot)->memberId);
            }
        }
        for (size_t index = 0; index < 4; ++index)
        {
            const uint32_t npcId = uint32_t(772 + index);
            const auto &entry = prepared.receipt().companions[index];
            const auto *pDetached = candidate.detachedMember(std::string(BreachAlvarSplitOwner), npcId);
            REQUIRE(pDetached != nullptr);
            checkCharacter(pDetached->character, originalById(original, entry.memberId));
            REQUIRE(pDetached->pendingItemDeliveries.size() == 1);
            CHECK(pDetached->pendingItemDeliveries[0].recipientMemberIndex == 0);
            CHECK_FALSE(pDetached->pendingItemDeliveries[0].allowDropProtectedHeldItem);
            CHECK(prepared.npcs()[index].pictureId == 200 + pDetached->character.characterDataId);
            CHECK(prepared.npcs()[index].name == pDetached->character.name);
            CHECK(prepared.npcs()[index].houseId == (index == 0 ? 0 : 704 + index));
            CHECK(prepared.npcs()[index].topicIds[0] == 1793);
            if (index != 0)
            {
                CHECK(entry.memberId == remaining[index - 1]);
            }
        }
        CHECK(candidate.pendingItemDeliveries().size() == 2); // leader plus mouse
        std::string error;
        CHECK(validBreachAlvarCompanionReceipt(prepared.receipt(), candidate.snapshot(), error));
        CHECK_FALSE(validBreachAlvarCompanionState(std::nullopt, candidate.snapshot(), error));
        BreachAlvarCompanionState incomplete;
        incomplete.receipt = prepared.receipt();
        CHECK_FALSE(validBreachAlvarCompanionState(incomplete, candidate.snapshot(), error));
    }
    CHECK(selected.size() == 4);
    CHECK(original.members().size() == 5);
    CHECK(original.detachedMembers().empty());
}

TEST_CASE("BrAlvar preparation rejects unsupported counts and presentation failure without live changes")
{
    const CharacterDollTable dolls = dollTable();
    for (size_t count = 1; count < 5; ++count)
    {
        const Party party = partyWithMembers(count);
        const Party before = party;
        checkRejected(prepareBreachAlvarFiveMemberSplit(party, std::nullopt, dolls, {}, 0),
            BreachAlvarCompanionPreparationCode::UnsupportedPartySize);
        checkUnchanged(party, before);
    }
    const Party original = partyWithMembers();
    const Party before = original;
    const CharacterDollTable missingDolls;
    checkRejected(prepareBreachAlvarFiveMemberSplit(original, std::nullopt, missingDolls, {}, 8),
        BreachAlvarCompanionPreparationCode::InvalidPresentation);
    checkUnchanged(original, before);
    BreachAlvarCompanionTopicFacts invalidTopics;
    invalidTopics.hintByNpc = {1, 1, 0, 0};
    checkRejected(prepareBreachAlvarFiveMemberSplit(original, std::nullopt, dolls, invalidTopics, 8),
        BreachAlvarCompanionPreparationCode::InvalidPresentation);
    checkUnchanged(original, before);
    CHECK(split(original, dolls, 8).receipt() == split(before, dolls, 8).receipt());
}

TEST_CASE("BrAlvar 24 candidate return orders preserve identity pending and character buff caster mapping")
{
    const CharacterDollTable dolls = dollTable();
    Party original = partyWithMembers();
    original.applyPartyBuff(PartyBuffId::Fly, 3600, 0, 21, 10, SkillMastery::Grandmaster, 4);
    for (size_t target = 0; target < 5; ++target)
    {
        original.applyCharacterBuff(target, CharacterBuffId::Regeneration, 3600, 0, 71, 10,
            SkillMastery::Grandmaster, uint32_t((target + 1) % 5));
    }
    for (size_t index = 0; index < 5; ++index)
    {
        original.member(index)->healthRegenAccumulator = 0.25f;
        original.member(index)->spellRegenAccumulator = 0.75f;
    }
    const auto prepared = split(original, dolls);
    CHECK_FALSE(prepared.party().hasPartyBuff(PartyBuffId::Fly));
    const auto receiptBefore = prepared.receipt();
    std::array<uint32_t, 4> order = {772, 773, 774, 775};
    size_t permutations = 0;
    do
    {
        ++permutations;
        Party party = prepared.party();
        BreachAlvarCompanionState state = structuralState(prepared);
        for (uint32_t npc : order)
        {
            const Party before = party;
            const BreachAlvarCompanionState oldState = state;
            const size_t appendedSlot = party.members().size();
            BreachAlvarCompanionPreparation result =
                prepareBreachAlvarCompanionReturn(party, state, dolls, {}, npc);
            REQUIRE_MESSAGE(result.mutation.has_value(), result.error);
            CHECK(result.code == BreachAlvarCompanionPreparationCode::Prepared);
            CHECK(result.mutation->kind() == BreachAlvarCompanionMutationKind::Return);
            CHECK(result.mutation->returningNpcId() == npc);
            CHECK(result.mutation->existingFriendActor() == state.friendActor);
            checkUnchanged(party, before);
            CHECK(state == oldState);
            // Advance this unit's candidate chain only. This is not a production commit or scene test.
            party = result.mutation->party();
            state.receipt = result.mutation->receipt();
            CHECK(party.member(appendedSlot)->memberId == receiptBefore.companions[npc - 772].memberId);
            CHECK(result.mutation->npcs()[npc - 772].houseId == 0);
            CHECK_FALSE(result.mutation->npcs()[npc - 772].available);
            CHECK_FALSE(result.mutation->npcs()[npc - 772].name.has_value());
            CHECK(result.mutation->npcs()[npc - 772].topicIds[0] == 0);
            checkRejected(prepareBreachAlvarCompanionReturn(party, state, dolls, {}, npc),
                BreachAlvarCompanionPreparationCode::AlreadyReturned);
            std::string error;
            REQUIRE_MESSAGE(validBreachAlvarCompanionState(state, party.snapshot(), error), error);
            for (size_t slot = 0; slot < party.members().size(); ++slot)
            {
                const uint64_t id = party.member(slot)->memberId;
                checkCharacter(*party.member(slot), originalById(original, id));
                const Party::Snapshot current = party.snapshot();
                const CharacterBuffState &buff = current.characterBuffs[slot][size_t(CharacterBuffId::Regeneration)];
                const uint64_t casterId = id == 5 ? 1 : id + 1;
                CHECK(buff.casterMemberId == casterId);
                uint32_t expectedIndex = Party::NoActiveMember;
                for (size_t caster = 0; caster < party.members().size(); ++caster)
                {
                    if (party.member(caster)->memberId == casterId)
                    {
                        expectedIndex = uint32_t(caster);
                    }
                }
                CHECK(buff.casterMemberIndex == expectedIndex);
            }
            for (const auto &delivery : party.pendingItemDeliveries())
            {
                if (delivery.item.objectDescriptionId == 630)
                {
                    CHECK(delivery.recipientMemberIndex == -1);
                }
                else
                {
                    REQUIRE(delivery.recipientMemberIndex >= 0);
                    const uint64_t intendedId = delivery.item.objectDescriptionId - 619;
                    CHECK(party.member(size_t(delivery.recipientMemberIndex))->memberId == intendedId);
                }
            }
            // Real Party snapshot restoration tests identity custody; no new Session codec exists yet.
            Party restored;
            restored.restoreSnapshot(party.snapshot());
            restored.setHeldItemForQueries(item(631)); // Session/UI owns and restores the actual held item.
            party = std::move(restored);
            REQUIRE_MESSAGE(validBreachAlvarCompanionState(state, party.snapshot(), error), error);
        }
        CHECK(party.members().size() == 5);
        CHECK(party.detachedMembers().empty());
        CHECK(party.hasDetachedBatch(std::string(BreachAlvarSplitOwner)));
        CHECK(party.pendingItemDeliveries().size() == 6);
        CHECK(party.heldItemIdForQueries() == 631);
        CHECK(party.hasQuestBit(1714));
        CHECK(party.gold() == original.gold());
        CHECK_FALSE(party.hasPartyBuff(PartyBuffId::Fly));
        CHECK(state.receipt.seed == receiptBefore.seed);
        CHECK(state.receipt.finalRandomState == receiptBefore.finalRandomState);
        checkRejected(prepareBreachAlvarFiveMemberSplit(party, state, dolls, {}, 999),
            BreachAlvarCompanionPreparationCode::AlreadySplit);
    } while (std::next_permutation(order.begin(), order.end()));
    CHECK(permutations == 24);
}

TEST_CASE("BrAlvar full party return retains the friend and succeeds as a candidate after ordinary dismissal")
{
    const CharacterDollTable dolls = dollTable();
    const auto prepared = split(partyWithMembers(), dolls);
    Party party = prepared.party();
    BreachAlvarCompanionState state = structuralState(prepared);
    for (size_t index = 0; index < 4; ++index)
    {
        Character recruit;
        recruit.name = "Temporary recruit";
        recruit.className = "Knight";
        recruit.characterDataId = 1;
        REQUIRE(party.recruitCharacter(recruit));
    }
    const Party before = party;
    const BreachAlvarCompanionState stateBefore = state;
    checkRejected(prepareBreachAlvarCompanionReturn(party, state, dolls, {}, 772),
        BreachAlvarCompanionPreparationCode::PartyFull);
    checkUnchanged(party, before);
    CHECK(state == stateBefore);
    REQUIRE(party.dismissMemberToAdventurersInn(4));
    const auto result = prepareBreachAlvarCompanionReturn(party, state, dolls, {}, 772);
    REQUIRE(result.mutation.has_value());
    CHECK(result.mutation->party().members().size() == 5);
    CHECK(result.mutation->party().member(4)->memberId == state.receipt.companions[0].memberId);
    CHECK(party.detachedMember(std::string(BreachAlvarSplitOwner), 772) != nullptr);
    const CharacterDollTable missingDolls;
    checkRejected(prepareBreachAlvarCompanionReturn(party, state, missingDolls, {}, 772),
        BreachAlvarCompanionPreparationCode::InvalidPresentation);
    CHECK(party.detachedMember(std::string(BreachAlvarSplitOwner), 772) != nullptr);
}

TEST_CASE("BrAlvar returned historical identities may move to inn another owner or be replaced")
{
    const CharacterDollTable dolls = dollTable();
    const auto prepared = split(partyWithMembers(), dolls);
    Party party = prepared.party();
    BreachAlvarCompanionState state = structuralState(prepared);
    const auto joined = prepareBreachAlvarCompanionReturn(party, state, dolls, {}, 772);
    REQUIRE(joined.mutation.has_value());
    party = joined.mutation->party();
    state.receipt = joined.mutation->receipt();
    const uint64_t originalId = state.receipt.companions[0].memberId;
    std::string error;
    REQUIRE(party.dismissMemberToAdventurersInn(1));
    CHECK(validBreachAlvarCompanionState(state, party.snapshot(), error));
    REQUIRE(party.hireAdventurersInnMember(0));
    REQUIRE(party.detachMembers("Another lawful owner", {{1, 99}}));
    CHECK(validBreachAlvarCompanionState(state, party.snapshot(), error));
    REQUIRE(party.joinDetachedMember("Another lawful owner", 99));
    RosterEntry replacement;
    replacement.id = 10;
    replacement.name = "Replacement";
    replacement.className = "Knight";
    REQUIRE(party.replaceMemberWithRosterEntry(1, replacement));
    CHECK(party.member(1)->memberId != originalId);
    CHECK(validBreachAlvarCompanionState(state, party.snapshot(), error));
    checkRejected(prepareBreachAlvarCompanionReturn(party, state, dolls, {}, 772),
        BreachAlvarCompanionPreparationCode::AlreadyReturned);
    const auto npcs = projectBreachAlvarCompanionNpcs(party, state.receipt, dolls, {}, error);
    REQUIRE(npcs.has_value());
    CHECK_FALSE((*npcs)[0].available);
    CHECK((*npcs)[0].topicIds[0] == 0);
}

TEST_CASE("BrAlvar receipt validation rejects incomplete duplicate mismatched and cross custody identities")
{
    const CharacterDollTable dolls = dollTable();
    const auto prepared = split(partyWithMembers(), dolls);
    const Party::Snapshot snapshot = prepared.party().snapshot();
    const BreachAlvarCompanionState state = structuralState(prepared);
    std::string error;
    REQUIRE(validBreachAlvarCompanionState(state, snapshot, error));
    for (size_t mutation = 0; mutation < 9; ++mutation)
    {
        BreachAlvarCompanionState bad = state;
        switch (mutation)
        {
            case 0: bad.receipt.companions[0].memberId = 0; break;
            case 1: bad.receipt.companions[0].memberId = bad.receipt.companions[1].memberId; break;
            case 2: std::swap(bad.receipt.companions[0], bad.receipt.companions[1]); break;
            case 3: bad.receipt.companions[0].returned = true; break;
            case 4: bad.receipt.profileVersion++; break;
            case 5: bad.receipt.finalRandomState++; break;
            case 6: bad.friendActor.canonicalMapKey = "mm8:out01.odm"; break;
            case 7: bad.friendActor.spawnMonsterId = 0; break;
            case 8: bad.receipt.leaderMemberId = bad.receipt.companions[0].memberId; break;
        }
        CHECK_FALSE(validBreachAlvarCompanionState(bad, snapshot, error));
        CHECK_FALSE(error.empty());
    }
    Party::Snapshot conflict = snapshot;
    conflict.adventurersInnMembers.push_back({conflict.detachedMembers[0].character, 1});
    CHECK_FALSE(validBreachAlvarCompanionState(state, conflict, error));
    conflict = snapshot;
    conflict.detachedMembers[0].slotId = 999;
    CHECK_FALSE(validBreachAlvarCompanionState(state, conflict, error));
    conflict = snapshot;
    conflict.completedDetachmentOwners.clear();
    CHECK_FALSE(validBreachAlvarCompanionState(state, conflict, error));
    conflict = snapshot;
    conflict.detachedMembers.erase(conflict.detachedMembers.begin());
    CHECK_FALSE(validBreachAlvarCompanionState(state, conflict, error));
    CHECK_FALSE(validBreachAlvarCompanionState(std::nullopt, snapshot, error));
    checkRejected(prepareBreachAlvarCompanionReturn(partyWithMembers(), std::nullopt, dolls, {}, 772),
        BreachAlvarCompanionPreparationCode::NotSplit);
    checkRejected(prepareBreachAlvarCompanionReturn(prepared.party(), state, dolls, {}, 771),
        BreachAlvarCompanionPreparationCode::InvalidNpc);
}

TEST_CASE("BrAlvar NPC projection follows existing hint order and cannot reactivate returned friends")
{
    const CharacterDollTable dolls = dollTable();
    const auto prepared = split(partyWithMembers(), dolls);
    BreachAlvarCompanionTopicFacts topics;
    topics.hintByNpc = {0, 2, 0, 1};
    std::string error;
    const auto projected = projectBreachAlvarCompanionNpcs(prepared.party(), prepared.receipt(), dolls, topics, error);
    REQUIRE(projected.has_value());
    CHECK((*projected)[0].topicIds[0] == 1793);
    CHECK((*projected)[1].topicIds[0] == 1789);
    CHECK((*projected)[2].topicIds[0] == 1793);
    CHECK((*projected)[3].topicIds[0] == 1789);
    topics.questFinished = true;
    const auto finished = projectBreachAlvarCompanionNpcs(prepared.party(), prepared.receipt(), dolls, topics, error);
    REQUIRE(finished.has_value());
    for (const auto &npc : *finished)
    {
        CHECK_FALSE(npc.available);
        CHECK(npc.topicIds[0] == 0);
    }
    topics.hintByNpc[0] = 5;
    CHECK_FALSE(projectBreachAlvarCompanionNpcs(
        prepared.party(), prepared.receipt(), dolls, topics, error).has_value());
}

TEST_CASE("BrAlvar candidates retain the total pending limit and reject malformed original ownership")
{
    const CharacterDollTable dolls = dollTable();
    Party party = partyWithMembers();
    Party::Snapshot snapshot = party.snapshot();
    snapshot.pendingItemDeliveries.clear();
    for (size_t index = 0; index < Party::MaxPendingItemDeliveries; ++index)
    {
        snapshot.pendingItemDeliveries.push_back({item(620), int32_t(index % 5), false});
    }
    party.restoreSnapshot(snapshot);
    const auto prepared = split(party, dolls, std::numeric_limits<uint64_t>::max());
    size_t total = prepared.party().pendingItemDeliveries().size();
    for (const auto &member : prepared.party().detachedMembers())
    {
        total += member.pendingItemDeliveries.size();
    }
    CHECK(total == Party::MaxPendingItemDeliveries);
    CHECK(prepared.receipt().finalRandomState == 11400714819323198484ULL);
    BreachAlvarCompanionState state = structuralState(prepared);
    const auto joined = prepareBreachAlvarCompanionReturn(prepared.party(), state, dolls, {}, 772);
    REQUIRE(joined.mutation.has_value());
    total = joined.mutation->party().pendingItemDeliveries().size();
    for (const auto &member : joined.mutation->party().detachedMembers())
    {
        total += member.pendingItemDeliveries.size();
    }
    CHECK(total == Party::MaxPendingItemDeliveries);
    snapshot.pendingItemDeliveries.push_back({item(620), 0, false});
    party.restoreSnapshot(snapshot);
    const Party overCapacity = party;
    checkRejected(prepareBreachAlvarFiveMemberSplit(party, std::nullopt, dolls, {}, 7),
        BreachAlvarCompanionPreparationCode::InvalidParty);
    checkUnchanged(party, overCapacity);
    party = partyWithMembers();
    party.member(1)->memberId = party.member(0)->memberId;
    const Party duplicate = party;
    checkRejected(prepareBreachAlvarFiveMemberSplit(party, std::nullopt, dolls, {}, 7),
        BreachAlvarCompanionPreparationCode::InvalidParty);
    checkUnchanged(party, duplicate);
}
