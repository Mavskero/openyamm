#include "doctest/doctest.h"

#include "game/maps/SaveGame.h"
#include "game/party/Party.h"

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace OpenYAMM::Game;

namespace
{
InventoryItem deliveryItem(uint32_t id = 624)
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
    item.currentCharges = 10;
    item.maxCharges = 11;
    item.temporaryBonusRemainingSeconds = 12.5f;
    return item;
}

void checkItem(const InventoryItem &actual, const InventoryItem &expected)
{
    CHECK_EQ(actual.objectDescriptionId, expected.objectDescriptionId);
    CHECK_EQ(actual.quantity, expected.quantity);
    CHECK_EQ(actual.width, expected.width);
    CHECK_EQ(actual.height, expected.height);
    CHECK_EQ(actual.gridX, expected.gridX);
    CHECK_EQ(actual.gridY, expected.gridY);
    CHECK_EQ(actual.identified, expected.identified);
    CHECK_EQ(actual.broken, expected.broken);
    CHECK_EQ(actual.stolen, expected.stolen);
    CHECK_EQ(actual.standardEnchantId, expected.standardEnchantId);
    CHECK_EQ(actual.standardEnchantPower, expected.standardEnchantPower);
    CHECK_EQ(actual.specialEnchantId, expected.specialEnchantId);
    CHECK_EQ(actual.artifactId, expected.artifactId);
    CHECK(actual.rarity == expected.rarity);
    CHECK_EQ(actual.currentCharges, expected.currentCharges);
    CHECK_EQ(actual.maxCharges, expected.maxCharges);
    CHECK_EQ(actual.temporaryBonusRemainingSeconds, expected.temporaryBonusRemainingSeconds);
}

Party fullDeliveryParty()
{
    PartySeed seed;
    for (size_t index = 0; index < 4; ++index)
    {
        Character member;
        member.name = "Recipient " + std::to_string(index);
        member.className = "Knight";
        member.maxHealth = 40;
        member.health = 40;
        seed.members.push_back(member);
    }
    Party party;
    party.seed(seed);
    InventoryItem filler;
    filler.objectDescriptionId = 1;
    for (size_t index = 0; index < party.memberCount(); ++index)
    {
        Character &member = *party.member(index);
        member.inventory.clear();
        for (int y = 0; y < Character::InventoryHeight; ++y)
        {
            for (int x = 0; x < Character::InventoryWidth; ++x)
            {
                REQUIRE(member.addInventoryItemAt(filler, uint8_t(x), uint8_t(y)));
            }
        }
        REQUIRE_FALSE(member.addInventoryItem(filler));
    }
    return party;
}

Party roundTripDeliveries(const Party &party, const std::string &name)
{
    const char *pTemporaryDirectory = std::getenv("TMPDIR");
    REQUIRE(pTemporaryDirectory != nullptr);
    REQUIRE(pTemporaryDirectory[0] != '\0');
    const std::filesystem::path path = std::filesystem::path(pTemporaryDirectory) / (name + ".oysav");
    GameSaveData save;
    save.mapFileName = "out03.odm";
    save.saveName = name;
    save.party = party.snapshot();
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const std::optional<GameSaveData> restored = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(restored.has_value(), error);
    Party cold;
    cold.restoreSnapshot(restored->party);
    REQUIRE_EQ(cold.pendingItemDeliveries().size(), party.pendingItemDeliveries().size());
    for (size_t index = 0; index < party.pendingItemDeliveries().size(); ++index)
    {
        const Party::PendingItemDelivery &expected = party.pendingItemDeliveries()[index];
        const Party::PendingItemDelivery &actual = cold.pendingItemDeliveries()[index];
        CHECK_EQ(actual.recipientMemberIndex, expected.recipientMemberIndex);
        CHECK_EQ(actual.allowDropProtectedHeldItem, expected.allowDropProtectedHeldItem);
        checkItem(actual.item, expected.item);
        CHECK(Party::validPendingItemDelivery(actual, cold.memberCount()));
    }
    return cold;
}
}

TEST_CASE("pending item recipients survive dismissal before at and after their party position")
{
    for (size_t dismissed = 1; dismissed < 4; ++dismissed)
    {
        for (int32_t recipient = 0; recipient < 4; ++recipient)
        {
            CAPTURE(dismissed);
            CAPTURE(recipient);
            Party party = fullDeliveryParty();
            const std::string recipientName = party.member(size_t(recipient))->name;
            const InventoryItem item = deliveryItem();
            REQUIRE(party.grantOwnedQuestItem(item, recipient));
            REQUIRE(party.grantOwnedQuestItem(deliveryItem(543), -1));
            REQUIRE_EQ(party.pendingItemDeliveries().size(), 2u);
            REQUIRE(party.dismissMemberToAdventurersInn(dismissed));

            const int32_t expectedRecipient = recipient == int32_t(dismissed) ? -1
                : recipient > int32_t(dismissed) ? recipient - 1 : recipient;
            REQUIRE_EQ(party.pendingItemDeliveries().size(), 2u);
            CHECK_EQ(party.pendingItemDeliveries()[0].recipientMemberIndex, expectedRecipient);
            CHECK_EQ(party.pendingItemDeliveries()[1].recipientMemberIndex, -1);
            checkItem(party.pendingItemDeliveries()[0].item, item);
            CHECK_EQ(party.inventoryItemCount(item.objectDescriptionId), 0);
            CHECK(party.ownsPendingItem(item.objectDescriptionId));
            const Party::Snapshot snapshot = party.snapshot();
            REQUIRE_EQ(snapshot.adventurersInnMembers.size(), 1u);
            for (const InventoryItem &innItem : snapshot.adventurersInnMembers[0].character.inventory)
            {
                CHECK_NE(innItem.objectDescriptionId, item.objectDescriptionId);
            }

            Party cold = roundTripDeliveries(party,
                "pending-dismiss-142-" + std::to_string(dismissed) + "-" + std::to_string(recipient));
            if (expectedRecipient >= 0)
            {
                CHECK_EQ(cold.member(size_t(expectedRecipient))->name, recipientName);
                REQUIRE_FALSE(cold.tryGrantInventoryItemToMember(size_t(expectedRecipient), item));
                CHECK(cold.ownsPendingItem(item.objectDescriptionId));
                cold.member(size_t(expectedRecipient))->inventory.clear();
                REQUIRE(cold.tryGrantInventoryItemToMember(size_t(expectedRecipient), item));
                cold.completeFirstPendingItemDelivery();
                CHECK_EQ(cold.inventoryItemCount(item.objectDescriptionId, size_t(expectedRecipient)), 2);
                CHECK_FALSE(cold.ownsPendingItem(item.objectDescriptionId));
            }
            else
            {
                REQUIRE(cold.hireAdventurersInnMember(0));
                CHECK_EQ(cold.pendingItemDeliveries()[0].recipientMemberIndex, -1);
                CHECK_EQ(cold.inventoryItemCount(item.objectDescriptionId), 0);
            }
        }
    }
}

TEST_CASE("full pending item queue remains owned and serializable after repeated dismissals")
{
    Party party = fullDeliveryParty();
    for (size_t index = 0; index < Party::MaxPendingItemDeliveries; ++index)
    {
        REQUIRE(party.grantOwnedQuestItem(deliveryItem(), int32_t(index % 5) - 1));
    }
    REQUIRE_FALSE(party.grantOwnedQuestItem(deliveryItem(), -1));
    REQUIRE(party.dismissMemberToAdventurersInn(3));
    REQUIRE(party.dismissMemberToAdventurersInn(2));
    REQUIRE(party.dismissMemberToAdventurersInn(1));
    Party cold = roundTripDeliveries(party, "pending-dismiss-142-full-queue");
    CHECK_EQ(cold.memberCount(), 1u);
    REQUIRE_EQ(cold.pendingItemDeliveries().size(), Party::MaxPendingItemDeliveries);
    for (size_t index = 0; index < cold.pendingItemDeliveries().size(); ++index)
    {
        CHECK_EQ(cold.pendingItemDeliveries()[index].recipientMemberIndex, index % 5 == 1 ? 0 : -1);
    }
    CHECK_EQ(cold.inventoryItemCount(624), 0);
    CHECK(cold.ownsPendingItem(624));
    REQUIRE_FALSE(cold.grantOwnedQuestItem(deliveryItem(), -1));
}

TEST_CASE("refused party dismissals leave pending recipients and item state unchanged")
{
    Party party = fullDeliveryParty();
    const InventoryItem item = deliveryItem();
    REQUIRE(party.grantOwnedQuestItem(item, 3));
    REQUIRE_FALSE(party.dismissMemberToAdventurersInn(0));
    REQUIRE_FALSE(party.dismissMemberToAdventurersInn(4));
    CHECK_EQ(party.memberCount(), 4u);
    REQUIRE_EQ(party.pendingItemDeliveries().size(), 1u);
    CHECK_EQ(party.pendingItemDeliveries()[0].recipientMemberIndex, 3);
    checkItem(party.pendingItemDeliveries()[0].item, item);
    Party cold = roundTripDeliveries(party, "pending-dismiss-142-refused");
    CHECK_EQ(cold.pendingItemDeliveries()[0].recipientMemberIndex, 3);
}
