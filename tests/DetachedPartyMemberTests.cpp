#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "game/maps/SaveGame.h"
#include "game/party/Party.h"
#include "game/party/SpellIds.h"
#include "game/tables/HouseTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/RosterTable.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>

using namespace OpenYAMM::Game;

namespace
{
std::filesystem::path output(const std::string &name)
{
    const char *pRoot = std::getenv("TMPDIR");
    REQUIRE(pRoot != nullptr);
    REQUIRE(pRoot[0] != '\0');
    return std::filesystem::path(pRoot) / ("detached147-" + name + ".oysav");
}

std::vector<char> bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

InventoryItem complexItem(uint32_t id)
{
    InventoryItem item;
    item.objectDescriptionId = id;
    item.quantity = 2;
    item.width = 2;
    item.height = 3;
    item.gridX = 4;
    item.gridY = 5;
    item.identified = false;
    item.broken = true;
    item.stolen = true;
    item.standardEnchantId = 6;
    item.standardEnchantPower = 7;
    item.specialEnchantId = 8;
    item.artifactId = 9;
    item.rarity = ItemRarity::Relic;
    item.currentCharges = 10;
    item.maxCharges = 11;
    item.temporaryBonusRemainingSeconds = 1200.5f;
    return item;
}

void checkItem(const InventoryItem &a, const InventoryItem &b)
{
    CHECK(a.objectDescriptionId == b.objectDescriptionId);
    CHECK(a.quantity == b.quantity);
    CHECK(a.width == b.width);
    CHECK(a.height == b.height);
    CHECK(a.gridX == b.gridX);
    CHECK(a.gridY == b.gridY);
    CHECK(a.identified == b.identified);
    CHECK(a.broken == b.broken);
    CHECK(a.stolen == b.stolen);
    CHECK(a.standardEnchantId == b.standardEnchantId);
    CHECK(a.standardEnchantPower == b.standardEnchantPower);
    CHECK(a.specialEnchantId == b.specialEnchantId);
    CHECK(a.artifactId == b.artifactId);
    CHECK(a.rarity == b.rarity);
    CHECK(a.currentCharges == b.currentCharges);
    CHECK(a.maxCharges == b.maxCharges);
    CHECK(a.temporaryBonusRemainingSeconds == b.temporaryBonusRemainingSeconds);
}

void checkCharacter(const Character &a, const Character &b)
{
    CHECK(a.memberId == b.memberId);
    CHECK(a.name == b.name);
    CHECK(a.role == b.role);
    CHECK(a.className == b.className);
    CHECK(a.portraitTextureName == b.portraitTextureName);
    CHECK(a.portraitPictureId == b.portraitPictureId);
    CHECK(a.rosterId == b.rosterId);
    CHECK(a.characterDataId == b.characterDataId);
    CHECK(a.voiceId == b.voiceId);
    CHECK(a.birthYear == b.birthYear);
    CHECK(a.sexId == b.sexId);
    CHECK(a.raceId == b.raceId);
    CHECK(a.level == b.level);
    CHECK(a.experience == b.experience);
    CHECK(a.skillPoints == b.skillPoints);
    CHECK(a.might == b.might);
    CHECK(a.intellect == b.intellect);
    CHECK(a.personality == b.personality);
    CHECK(a.endurance == b.endurance);
    CHECK(a.accuracy == b.accuracy);
    CHECK(a.speed == b.speed);
    CHECK(a.luck == b.luck);
    CHECK(a.maxHealth == b.maxHealth);
    CHECK(a.health == b.health);
    CHECK(a.maxSpellPoints == b.maxSpellPoints);
    CHECK(a.spellPoints == b.spellPoints);
    CHECK(a.armageddonCastsToday == b.armageddonCastsToday);
    CHECK(a.conditions == b.conditions);
    CHECK(a.conditionStartGameMinutes == b.conditionStartGameMinutes);
    CHECK(a.permanentConditionImmunities == b.permanentConditionImmunities);
    CHECK(a.quickSpellName == b.quickSpellName);
    CHECK(a.attackSpellName == b.attackSpellName);
    CHECK(a.knownSpellIds == b.knownSpellIds);
    REQUIRE(a.skills.size() == b.skills.size());
    for (const auto &[name, skill] : b.skills)
    {
        REQUIRE(a.skills.contains(name));
        CHECK(a.skills.at(name).level == skill.level);
        CHECK(a.skills.at(name).mastery == skill.mastery);
    }
    CHECK(a.awards == b.awards);
    CHECK(a.eventVariables == b.eventVariables);
    CHECK(a.playerBits == b.playerBits);
    CHECK(a.npcs2 == b.npcs2);
    CHECK(a.recoverySecondsRemaining == b.recoverySecondsRemaining);
    CHECK(a.armorClassModifier == b.armorClassModifier);
    CHECK(a.levelModifier == b.levelModifier);
    CHECK(a.ageModifier == b.ageModifier);
    CHECK(a.healthRegenAccumulator == b.healthRegenAccumulator);
    CHECK(a.spellRegenAccumulator == b.spellRegenAccumulator);
    CHECK(a.baseResistances.fire == b.baseResistances.fire);
    CHECK(a.permanentBonuses.might == b.permanentBonuses.might);
    CHECK(a.temporaryEventBonuses.luck == b.temporaryEventBonuses.luck);
    CHECK(a.permanentImmunities.body == b.permanentImmunities.body);
    for (uint32_t CharacterEquipment::*slot : {
        &CharacterEquipment::offHand, &CharacterEquipment::mainHand, &CharacterEquipment::bow,
        &CharacterEquipment::armor, &CharacterEquipment::helm, &CharacterEquipment::belt,
        &CharacterEquipment::cloak, &CharacterEquipment::gauntlets, &CharacterEquipment::boots,
        &CharacterEquipment::amulet, &CharacterEquipment::ring1, &CharacterEquipment::ring2,
        &CharacterEquipment::ring3, &CharacterEquipment::ring4, &CharacterEquipment::ring5,
        &CharacterEquipment::ring6})
    {
        CHECK(a.equipment.*slot == b.equipment.*slot);
    }
    const EquippedItemRuntimeState &x = a.equipmentRuntime.mainHand;
    const EquippedItemRuntimeState &y = b.equipmentRuntime.mainHand;
    CHECK(x.identified == y.identified);
    CHECK(x.broken == y.broken);
    CHECK(x.stolen == y.stolen);
    CHECK(x.standardEnchantId == y.standardEnchantId);
    CHECK(x.standardEnchantPower == y.standardEnchantPower);
    CHECK(x.specialEnchantId == y.specialEnchantId);
    CHECK(x.artifactId == y.artifactId);
    CHECK(x.rarity == y.rarity);
    CHECK(x.currentCharges == y.currentCharges);
    CHECK(x.maxCharges == y.maxCharges);
    CHECK(x.temporaryBonusRemainingSeconds == y.temporaryBonusRemainingSeconds);
    REQUIRE(a.inventory.size() == b.inventory.size());
    for (size_t index = 0; index < a.inventory.size(); ++index)
    {
        checkItem(a.inventory[index], b.inventory[index]);
    }
    for (size_t index = 0; index < a.timedPrimaryStatBonuses.size(); ++index)
    {
        CHECK(a.timedPrimaryStatBonuses[index].power == b.timedPrimaryStatBonuses[index].power);
        CHECK(a.timedPrimaryStatBonuses[index].remainingSeconds == b.timedPrimaryStatBonuses[index].remainingSeconds);
    }
    for (size_t index = 0; index < a.lloydsBeacons.size(); ++index)
    {
        REQUIRE(a.lloydsBeacons[index].has_value() == b.lloydsBeacons[index].has_value());
        if (!a.lloydsBeacons[index])
        {
            continue;
        }
        const LloydBeacon &x = *a.lloydsBeacons[index];
        const LloydBeacon &y = *b.lloydsBeacons[index];
        CHECK(x.mapName == y.mapName);
        CHECK(x.locationName == y.locationName);
        CHECK(x.x == y.x);
        CHECK(x.y == y.y);
        CHECK(x.z == y.z);
        CHECK(x.directionDegrees == y.directionDegrees);
        CHECK(x.remainingSeconds == y.remainingSeconds);
        CHECK(x.previewWidth == y.previewWidth);
        CHECK(x.previewHeight == y.previewHeight);
        CHECK(x.previewPixelsBgra == y.previewPixelsBgra);
    }
}

Party makeParty(size_t count)
{
    PartySeed seed;
    seed.members.resize(count);
    for (size_t index = 0; index < count; ++index)
    {
        seed.members[index].name = "Detached member " + std::to_string(index);
        seed.members[index].className = "Knight";
        seed.members[index].characterDataId = 1; // A resolved portrait identity survives normal restore normalization.
    }
    Party party;
    party.seed(seed);
    for (size_t index = 0; index < count; ++index)
    {
        Character &c = *party.member(index);
        c.rosterId = index >= 3 ? uint32_t(38 + index) : 0;
        c.health = 17 + int(index);
        c.maxHealth = 90;
        c.spellPoints = 11 + int(index);
        c.maxSpellPoints = 60;
        c.level = 13;
        c.experience = 123456 + uint32_t(index);
        c.skillPoints = 37;
        c.armageddonCastsToday = 2;
        c.might = 21;
        c.intellect = 22;
        c.personality = 23;
        c.endurance = 24;
        c.accuracy = 25;
        c.speed = 26;
        c.luck = 27;
        c.voiceId = 9;
        c.birthYear = 1120;
        c.conditions.set(size_t(CharacterCondition::PoisonMedium));
        c.conditionStartGameMinutes[size_t(CharacterCondition::PoisonMedium)] = 1234.5f;
        c.permanentConditionImmunities.set(size_t(CharacterCondition::Fear));
        c.baseResistances.fire = 23;
        c.permanentBonuses.might = 4;
        c.temporaryEventBonuses.luck = 5;
        c.permanentImmunities.body = true;
        c.quickSpellName = "Fire Bolt";
        c.attackSpellName = "Fire Bolt";
        c.knownSpellIds = {1, 2, 3};
        c.awards = {4, 5};
        c.eventVariables[19] = -123;
        c.playerBits = {6, 7};
        c.npcs2 = 42;
        c.recoverySecondsRemaining = 1.5f;
        c.armorClassModifier = 3;
        c.levelModifier = 4;
        c.ageModifier = 5;
        c.healthRegenAccumulator = 0.25f;
        c.spellRegenAccumulator = 0.75f;
        c.inventory = {complexItem(624)};
        c.equipment.mainHand = 1;
        c.equipmentRuntime.mainHand = {false, true, true, 6, 7, 8, 9, ItemRarity::Artifact, 10, 11, 1200.5f};
        c.timedPrimaryStatBonuses[0] = {6, 1200.5f};
        LloydBeacon beacon;
        beacon.mapName = "out01.odm";
        beacon.locationName = "saved location";
        beacon.x = 1;
        beacon.y = 2;
        beacon.z = 3;
        beacon.directionDegrees = 123;
        beacon.remainingSeconds = 2400;
        beacon.previewWidth = 1;
        beacon.previewHeight = 1;
        beacon.previewPixelsBgra = {1, 2, 3, 255};
        c.lloydsBeacons[2] = std::move(beacon);
    }
    return party;
}

Party coldParty(const Party &party, const std::string &name)
{
    GameSaveData save;
    save.party = party.snapshot();
    save.mapFileName = "out01.odm";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(output(name), save, error), error);
    const auto loaded = loadGameDataFromPath(output(name), error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK(uint8_t(bytes(output(name))[8]) == 91);
    Party cold;
    cold.restoreSnapshot(loaded->party);
    return cold;
}

std::vector<Party::DetachmentAssignment> sourceAssignments(size_t count, size_t first)
{
    std::vector<Party::DetachmentAssignment> assignments;
    if (count == 1)
    {
        return assignments;
    }
    assignments.push_back({first, 772});
    uint32_t slot = 773;
    for (size_t index = 1; index < count; ++index)
    {
        if (index != first)
        {
            assignments.push_back({index, slot++});
        }
    }
    return assignments;
}
}

TEST_CASE("detachment preserves original characters for every source party size and NPC772 choice")
{
    for (size_t count = 1; count <= 5; ++count)
    {
        for (size_t first = 1; first < std::max(size_t(2), count); ++first)
        {
            CAPTURE(count);
            CAPTURE(first);
            Party party = makeParty(count);
            const auto before = party.snapshot();
            const auto assignments = sourceAssignments(count, first);
            REQUIRE(party.detachMembers("MMerge.BreachSplit", assignments));
            CHECK(party.memberCount() == 1);
            CHECK(party.adventurersInnMembers().empty());
            CHECK(party.detachedMembers().size() == count - 1);
            CHECK_FALSE(party.detachMembers("MMerge.BreachSplit", assignments));
            Party cold = coldParty(party, "all-sizes-" + std::to_string(count) + "-" + std::to_string(first));
            CHECK(cold.hasDetachedBatch("MMerge.BreachSplit"));
            checkCharacter(*cold.member(0), before.members[0]);
            for (auto it = assignments.rbegin(); it != assignments.rend(); ++it)
            {
                const Party::DetachedPartyMember *stored = cold.detachedMember("MMerge.BreachSplit", it->slotId);
                REQUIRE(stored != nullptr);
                checkCharacter(stored->character, before.members[it->memberIndex]);
                REQUIRE(cold.joinDetachedMember("MMerge.BreachSplit", it->slotId));
                checkCharacter(*cold.member(cold.memberCount() - 1), before.members[it->memberIndex]);
                CHECK_FALSE(cold.joinDetachedMember("MMerge.BreachSplit", it->slotId));
            }
            CHECK(cold.detachedMembers().empty());
            CHECK(cold.hasDetachedBatch("MMerge.BreachSplit"));
            CHECK_FALSE(cold.detachMembers("MMerge.BreachSplit", {}));
            std::set<uint64_t> ids;
            for (const Character &member : cold.members())
            {
                ids.insert(member.memberId);
            }
            CHECK(ids.size() == count);
        }
    }
}

TEST_CASE("detached caster identities survive reorder and expire without healing their absent target")
{
    Party party = makeParty(4);
    party.applyPartyBuff(PartyBuffId::Fly, 120, 7, 21, 10, SkillMastery::Master, 2);
    party.applyPartyBuff(PartyBuffId::WizardEye, 120, 3, 2, 10, SkillMastery::Master, 0);
    party.applyCharacterBuff(1, CharacterBuffId::Bless, 1000, 7, 46, 10, SkillMastery::Master, 2);
    party.applyCharacterBuff(0, CharacterBuffId::Fate, 1000, 8, 47, 11, SkillMastery::Expert, 1);
    party.applyCharacterBuff(2, CharacterBuffId::Regeneration, 600, 9,
        spellIdValue(SpellId::Regeneration), 12, SkillMastery::Master, 2);
    const uint64_t firstId = party.member(1)->memberId;
    const uint64_t secondId = party.member(2)->memberId;
    const int secondHp = party.member(2)->health;
    REQUIRE(party.detachMembers("caster-test", {{2, 772}, {1, 773}}));
    CHECK_FALSE(party.hasPartyBuff(PartyBuffId::Fly));
    CHECK(party.hasPartyBuff(PartyBuffId::WizardEye));
    REQUIRE(party.characterBuff(0, CharacterBuffId::Fate) != nullptr);
    CHECK(party.characterBuff(0, CharacterBuffId::Fate)->casterMemberId == firstId);
    CHECK(party.characterBuff(0, CharacterBuffId::Fate)->casterMemberIndex == Party::NoActiveMember);
    Party cold = coldParty(party, "casters");
    const auto &bless = cold.detachedMember("caster-test", 773)->characterBuffs[size_t(CharacterBuffId::Bless)];
    CHECK(bless.casterMemberId == secondId);
    CHECK(bless.casterMemberIndex == Party::NoActiveMember);
    cold.advanceTimedStates(301); // Cross a regeneration tick while the absent character's buff is still active.
    CHECK(cold.detachedMember("caster-test", 772)->character.health == secondHp);
    CHECK(cold.detachedMember("caster-test", 772)->characterBuffs[
        size_t(CharacterBuffId::Regeneration)].remainingSeconds == 299);
    cold.advanceTimedStates(300);
    const auto &expired = cold.detachedMember("caster-test", 772)->characterBuffs;
    CHECK_FALSE(expired[size_t(CharacterBuffId::Regeneration)].active());
    REQUIRE(cold.joinDetachedMember("caster-test", 773));
    CHECK(cold.characterBuff(0, CharacterBuffId::Fate)->casterMemberIndex == 2);
    CHECK(cold.characterBuff(2, CharacterBuffId::Bless)->casterMemberIndex == Party::NoActiveMember);
    REQUIRE(cold.joinDetachedMember("caster-test", 772));
    CHECK(cold.characterBuff(2, CharacterBuffId::Bless)->casterMemberIndex == 3);
    CHECK(cold.characterBuff(2, CharacterBuffId::Bless)->remainingSeconds == 399);
    CHECK(cold.member(3)->health == secondHp);
    REQUIRE(cold.dismissMemberToAdventurersInn(1));
    CHECK(cold.characterBuff(1, CharacterBuffId::Bless)->casterMemberIndex == 2);
    REQUIRE(cold.dismissMemberToAdventurersInn(2));
    CHECK(cold.characterBuff(1, CharacterBuffId::Bless)->casterMemberIndex == Party::NoActiveMember);
    CHECK(cold.characterBuff(1, CharacterBuffId::Bless)->casterMemberId == secondId);
    REQUIRE(cold.hireAdventurersInnMember(1));
    CHECK(cold.characterBuff(1, CharacterBuffId::Bless)->casterMemberIndex == 2);
}

TEST_CASE("detachment failures commit neither ownership nor once-only receipt and full joins remain recoverable")
{
    Party party = makeParty(3);
    const auto before = party.snapshot();
    for (const auto &bad : std::vector<std::vector<Party::DetachmentAssignment>>{
        {{0, 772}}, {{3, 772}}, {{1, 772}, {1, 773}}, {{1, 772}, {2, 772}}, {{1, 0}}})
    {
        CHECK_FALSE(party.detachMembers("attempt", bad));
        CHECK_FALSE(party.hasDetachedBatch("attempt"));
        CHECK(party.detachedMembers().empty());
        REQUIRE(party.memberCount() == before.members.size());
        for (size_t index = 0; index < party.memberCount(); ++index)
        {
            checkCharacter(*party.member(index), before.members[index]);
        }
        CHECK(party.snapshot().nextMemberId == before.nextMemberId);
    }
    party.member(1)->rosterId = 41;
    REQUIRE(party.detachMembers("attempt", {{1, 772}}));
    CHECK(party.hasRosterMember(41));
    CHECK(party.adventurersInnMembers().empty());
    const Character absent = party.detachedMember("attempt", 772)->character;
    CHECK_FALSE(party.recruitCharacter(absent));
    CHECK_FALSE(party.addAdventurersInnMember(absent, 1));
    RosterEntry entry;
    entry.id = 41;
    CHECK_FALSE(party.recruitRosterMember(entry));
    CHECK_FALSE(party.replaceMemberWithRosterEntry(1, entry));
    while (!party.isFull())
    {
        Character extra;
        extra.name = "Temporary recruit";
        extra.className = "Knight";
        REQUIRE(party.recruitCharacter(extra));
    }
    const uint64_t next = party.snapshot().nextMemberId;
    CHECK_FALSE(party.joinDetachedMember("attempt", 772));
    checkCharacter(party.detachedMember("attempt", 772)->character, absent);
    CHECK(party.snapshot().nextMemberId == next);
    REQUIRE(party.dismissMemberToAdventurersInn(4));
    REQUIRE(party.joinDetachedMember("attempt", 772));
    checkCharacter(*party.member(4), absent);
}

TEST_CASE("detached pending recipients reserve all 128 delivery slots and rejoin only their original owner")
{
    Party party = makeParty(5);
    for (size_t index = 0; index < party.memberCount(); ++index)
    {
        Character &member = *party.member(index);
        member.inventory.clear();
        InventoryItem filler;
        filler.objectDescriptionId = 1;
        for (int y = 0; y < Character::InventoryHeight; ++y)
        {
            for (int x = 0; x < Character::InventoryWidth; ++x)
            {
                REQUIRE(member.addInventoryItemAt(filler, uint8_t(x), uint8_t(y)));
            }
        }
    }
    for (size_t index = 0; index < Party::MaxPendingItemDeliveries; ++index)
    {
        REQUIRE(party.grantOwnedQuestItem(complexItem(624), int32_t(index % 6) - 1, index % 2 == 0));
    }
    const auto before = party.snapshot();
    REQUIRE(party.detachMembers("pending", {{3, 772}, {1, 773}}));
    CHECK_FALSE(party.grantOwnedQuestItem(complexItem(543), -1, false));
    while (!party.isFull())
    {
        Character extra;
        extra.className = "Knight";
        REQUIRE(party.recruitCharacter(extra));
    }
    const size_t storedCount = party.detachedMember("pending", 773)->pendingItemDeliveries.size();
    CHECK_FALSE(party.joinDetachedMember("pending", 773));
    CHECK(party.detachedMember("pending", 773)->pendingItemDeliveries.size() == storedCount);
    REQUIRE(party.dismissMemberToAdventurersInn(4));
    REQUIRE(party.dismissMemberToAdventurersInn(3));
    size_t count = party.pendingItemDeliveries().size();
    for (const auto &member : party.detachedMembers())
    {
        count += member.pendingItemDeliveries.size();
    }
    CHECK(count == 128);
    for (const auto &member : party.detachedMembers())
    {
        for (const auto &delivery : member.pendingItemDeliveries)
        {
            CHECK(delivery.recipientMemberIndex == 0);
        }
    }
    Party cold = coldParty(party, "pending-capacity");
    REQUIRE(cold.joinDetachedMember("pending", 773));
    REQUIRE(cold.joinDetachedMember("pending", 772));
    REQUIRE(cold.pendingItemDeliveries().size() == before.pendingItemDeliveries.size());
    std::array<size_t, 6> actualCounts = {};
    std::array<size_t, 6> expectedCounts = {};
    for (const auto &delivery : before.pendingItemDeliveries)
    {
        ++expectedCounts[size_t(delivery.recipientMemberIndex + 1)];
    }
    for (const auto &delivery : cold.pendingItemDeliveries())
    {
        checkItem(delivery.item, complexItem(624));
        if (delivery.recipientMemberIndex == -1)
        {
            ++actualCounts[0];
            CHECK(delivery.allowDropProtectedHeldItem);
        }
        else
        {
            const uint64_t id = cold.member(size_t(delivery.recipientMemberIndex))->memberId;
            const auto found = std::find_if(before.members.begin(), before.members.end(),
                [id](const Character &member) { return member.memberId == id; });
            REQUIRE(found != before.members.end());
            const size_t oldIndex = size_t(found - before.members.begin());
            ++actualCounts[oldIndex + 1];
            CHECK(delivery.allowDropProtectedHeldItem == ((oldIndex + 1) % 2 == 0));
        }
    }
    CHECK(actualCounts == expectedCounts);
    CHECK_FALSE(cold.grantOwnedQuestItem(complexItem(543), -1));
}

TEST_CASE("save90 retains detached content dependencies and rejects corrupt ownership metadata")
{
    Party party = makeParty(2);
    for (size_t index = 0; index < 2; ++index)
    {
        party.member(index)->inventory.clear();
        party.member(index)->equipment = {};
    }
    party.member(1)->inventory = {complexItem(101)};
    party.member(1)->equipment.mainHand = 102;
    auto snap = party.snapshot();
    snap.pendingItemDeliveries.push_back({complexItem(103), 1, false});
    party.restoreSnapshot(snap);
    REQUIRE(party.detachMembers("packages", {{1, 772}}));
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    std::vector<std::vector<std::string>> rows;
    for (int id = 101; id <= 103; ++id)
    {
        std::vector<std::string> row(20);
        row[0] = std::to_string(id);
        row[2] = "Detached dependency";
        row[17] = "detached" + std::to_string(id);
        rows.push_back(std::move(row));
    }
    REQUIRE(items.load(assets, rows, {}));
    HouseTable houses;
    const std::unordered_map<std::string, uint32_t> schemas = {
        {"detached101", 1}, {"detached102", 2}, {"detached103", 3}};
    GameSaveData save;
    save.party = party.snapshot();
    save.requiredContentPackages = collectRequiredContentPackages(save, items, houses, schemas);
    CHECK(save.requiredContentPackages == schemas);
    std::string error;
    CHECK(validateRequiredContentPackages(save, items, houses, schemas, error));
    CHECK_FALSE(validateRequiredContentPackages(save, items, houses, {{"detached101", 1}}, error));
    REQUIRE(saveGameDataToPath(output("valid90"), save, error));
    const auto validReload = loadGameDataFromPath(output("valid90"), error);
    REQUIRE_MESSAGE(validReload.has_value(), error);
    REQUIRE(validReload->party.detachedMembers.size() == 1);
    CHECK(validReload->party.completedDetachmentOwners == save.party.completedDetachmentOwners);
    CHECK(validReload->party.nextMemberId == save.party.nextMemberId);
    CHECK(validReload->requiredContentPackages == schemas);
    checkCharacter(validReload->party.detachedMembers[0].character, save.party.detachedMembers[0].character);
    const auto validBytes = bytes(output("valid90"));
    auto bad = save;
    bad.party.detachedMembers[0].character.memberId = bad.party.members[0].memberId;
    CHECK_FALSE(saveGameDataToPath(output("invalid90"), bad, error));
    bad = save;
    bad.party.nextMemberId = bad.party.detachedMembers[0].character.memberId;
    CHECK_FALSE(saveGameDataToPath(output("invalid90"), bad, error));
    bad = save;
    bad.party.detachedMembers[0].pendingItemDeliveries[0].recipientMemberIndex = -1;
    CHECK_FALSE(saveGameDataToPath(output("invalid90"), bad, error));
    bad = save;
    bad.party.completedDetachmentOwners.push_back("packages");
    CHECK_FALSE(saveGameDataToPath(output("invalid90"), bad, error));
    CHECK(bytes(output("valid90")) == validBytes);
    save.party.members[0].memberId = 0x1122334455667788ULL;
    save.party.nextMemberId = save.party.members[0].memberId + 1;
    REQUIRE(saveGameDataToPath(output("corrupt90"), save, error));
    auto corrupted = bytes(output("corrupt90"));
    const std::array<char, 8> marker = {char(0x88), 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    const auto position = std::search(corrupted.begin(), corrupted.end(), marker.begin(), marker.end());
    REQUIRE(position != corrupted.end());
    std::fill(position, position + 8, 0);
    std::ofstream file(output("corrupt90"), std::ios::binary);
    file.write(corrupted.data(), std::streamsize(corrupted.size()));
    file.close();
    CHECK_FALSE(loadGameDataFromPath(output("corrupt90"), error).has_value());
}

TEST_CASE("sealed save88 and save89 migrate deterministic identities without rewriting their inputs")
{
    const char *pRoot = std::getenv("OPENYAMM_LEGACY_SAVE_ROOT");
    const std::filesystem::path root = pRoot != nullptr ? std::filesystem::path(pRoot)
        : std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
            / "local-data/validation/campaign-readiness-142/event_item_ownership_check-output-002";
    for (const std::string name : {"policy142-version88-controlled", "policy142-version89"})
    {
        const auto path = root / (name + ".oysav");
        const auto original = bytes(path);
        const uint8_t version = uint8_t(original.at(8));
        REQUIRE((version == 88 || version == 89));
        std::string error;
        const auto loaded = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(loaded.has_value(), error);
        CHECK(loaded->party.detachedMembers.empty());
        CHECK(loaded->party.completedDetachmentOwners.empty());
        uint64_t next = 1;
        for (const Character &member : loaded->party.members)
        {
            CHECK(member.memberId == next++);
            CHECK(member.armageddonCastsToday == 0);
        }
        for (const auto &member : loaded->party.adventurersInnMembers)
        {
            CHECK(member.character.memberId == next++);
        }
        CHECK(loaded->party.nextMemberId == next);
        REQUIRE(loaded->party.pendingItemDeliveries.size() == 1);
        CHECK(loaded->party.pendingItemDeliveries[0].allowDropProtectedHeldItem == (version == 88));
        REQUIRE(saveGameDataToPath(output(name + "-upgraded"), *loaded, error));
        const auto roundTrip = loadGameDataFromPath(output(name + "-upgraded"), error);
        REQUIRE_MESSAGE(roundTrip.has_value(), error);
        CHECK(roundTrip->party.nextMemberId == loaded->party.nextMemberId);
        for (size_t index = 0; index < loaded->party.members.size(); ++index)
        {
            checkCharacter(roundTrip->party.members[index], loaded->party.members[index]);
        }
        CHECK(bytes(path) == original);
    }
    GameSaveData legacy;
    legacy.party.members.resize(2);
    legacy.party.adventurersInnMembers.resize(1);
    legacy.party.characterBuffs[0][0].remainingSeconds = 10;
    legacy.party.characterBuffs[0][0].casterMemberIndex = 1;
    migrateLegacyPartyMemberIdentities(legacy, 89);
    CHECK(legacy.party.members[0].memberId == 1);
    CHECK(legacy.party.members[1].memberId == 2);
    CHECK(legacy.party.adventurersInnMembers[0].character.memberId == 3);
    CHECK(legacy.party.characterBuffs[0][0].casterMemberId == 2);
    CHECK(legacy.party.nextMemberId == 4);
    migrateLegacyPartyMemberIdentities(legacy, 89);
    CHECK(legacy.party.nextMemberId == 4);
}

TEST_CASE("save90 accepts empty parties and inn custody while rejecting reused identities")
{
    Party party;
    std::string error;
    CHECK(Party::validMemberOwnership(party.snapshot(), error));
    Party emptyCold = coldParty(party, "empty-party");
    CHECK(emptyCold.memberCount() == 0);
    CHECK(emptyCold.snapshot().nextMemberId == 1);

    Character newcomer;
    newcomer.name = "Inn custody";
    newcomer.className = "Knight";
    newcomer.characterDataId = 1;
    newcomer.health = 17;
    newcomer.maxHealth = 90;
    newcomer.armageddonCastsToday = 2;
    REQUIRE(party.addAdventurersInnMember(newcomer, 0));
    REQUIRE(party.adventurersInnMembers().size() == 1);
    CHECK(newcomer.memberId == 0);
    const Character stored = party.adventurersInnMembers()[0].character;
    CHECK(stored.memberId == 1);
    CHECK(Party::validMemberOwnership(party.snapshot(), error));
    CHECK_FALSE(party.addAdventurersInnMember(stored, 0));
    CHECK_FALSE(party.recruitCharacter(stored));
    CHECK(party.snapshot().nextMemberId == 2);

    Party cold = coldParty(party, "inn-only");
    CHECK(cold.memberCount() == 0);
    REQUIRE(cold.adventurersInnMembers().size() == 1);
    checkCharacter(cold.adventurersInnMembers()[0].character, stored);
    REQUIRE(cold.hireAdventurersInnMember(0));
    CHECK(cold.adventurersInnMembers().empty());
    checkCharacter(*cold.member(0), stored);
    CHECK(cold.snapshot().nextMemberId == 2);
    REQUIRE(cold.recruitCharacter(newcomer));
    CHECK(cold.member(1)->memberId == 2);
    REQUIRE(cold.dismissMemberToAdventurersInn(1));
    CHECK(cold.adventurersInnMembers()[0].character.memberId == 2);
    REQUIRE(cold.hireAdventurersInnMember(0));
    CHECK(cold.member(1)->memberId == 2);
    CHECK(cold.snapshot().nextMemberId == 3);
    CHECK(Party::validMemberOwnership(cold.snapshot(), error));

    auto duplicate = cold.snapshot();
    duplicate.adventurersInnMembers.push_back({duplicate.members[0], 0});
    CHECK_FALSE(Party::validMemberOwnership(duplicate, error));
    auto invalid = cold.snapshot();
    invalid.members[0].memberId = 0;
    CHECK_FALSE(Party::validMemberOwnership(invalid, error));

    GameSaveData legacyInn;
    legacyInn.party.adventurersInnMembers.resize(1);
    migrateLegacyPartyMemberIdentities(legacyInn, 88);
    CHECK(legacyInn.party.adventurersInnMembers[0].character.memberId == 1);
    CHECK(legacyInn.party.nextMemberId == 2);
    CHECK(Party::validMemberOwnership(legacyInn.party, error));
}

TEST_CASE("detached timer expiry cannot reset active fractional regeneration progress")
{
    Party party = makeParty(2);
    party.member(0)->healthRegenAccumulator = 0.75f;
    party.member(0)->spellRegenAccumulator = 0.5f;
    party.member(1)->timedPrimaryStatBonuses[0] = {6, 1.0f};
    party.member(1)->inventory[0].temporaryBonusRemainingSeconds = 1.0f;
    party.member(1)->equipmentRuntime.mainHand.temporaryBonusRemainingSeconds = 1.0f;
    REQUIRE(party.detachMembers("expiry", {{1, 772}}));
    party.advanceTimedStates(2.0f);
    CHECK(party.member(0)->healthRegenAccumulator == 0.75f);
    CHECK(party.member(0)->spellRegenAccumulator == 0.5f);
    const auto *pStored = party.detachedMember("expiry", 772);
    REQUIRE(pStored != nullptr);
    CHECK(pStored->character.timedPrimaryStatBonuses[0].power == 0);
    CHECK(pStored->character.timedPrimaryStatBonuses[0].remainingSeconds == 0.0f);
    CHECK(pStored->character.inventory[0].temporaryBonusRemainingSeconds == 0.0f);
    CHECK(pStored->character.inventory[0].standardEnchantId == 0);
    CHECK(pStored->character.inventory[0].specialEnchantId == 0);
    CHECK(pStored->character.equipmentRuntime.mainHand.temporaryBonusRemainingSeconds == 0.0f);
    CHECK(pStored->character.equipmentRuntime.mainHand.standardEnchantId == 0);
    CHECK(pStored->character.equipmentRuntime.mainHand.specialEnchantId == 0);
    REQUIRE(party.joinDetachedMember("expiry", 772));
    CHECK(party.member(0)->healthRegenAccumulator == 0.75f);
    CHECK(party.member(0)->spellRegenAccumulator == 0.5f);
}
