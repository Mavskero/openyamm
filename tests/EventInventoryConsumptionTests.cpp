#include "EventSupportSource.h"
#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/data/GameDataRepository.h"
#include "game/events/EventRuntime.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace OpenYAMM::Game;

namespace
{
// Original MM8 opcode 0x11 / Inventory dispatch is special: 0x444658..0x4447f8.
// Source hashes, exact branch addresses and the original out02 event75 are recorded in
// local-data/validation/mm8-endgame-146/heart-consumption-review-001/REVIEW.md.
// These bounded event fixtures do not claim natural item acquisition or campaign acceptance.
struct ConsumptionFixture : OpenYAMM::Tests::PartySpellTestWorldRuntime
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository data;
    Party party;
    EventRuntimeState state;
    EventRuntime events;
    GameSession session;

    ConsumptionFixture()
    {
        REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(), OpenYAMM::Tests::regressionGameDataFailure());
        const char *pRoot = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
        REQUIRE(pRoot != nullptr);
        REQUIRE(files.initialize(pRoot, pRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mm8"));
        loader.setActiveWorldId("mm8");
        REQUIRE(loader.loadCommonForGameplay(files));
        data.bind(loader);
        party = OpenYAMM::Tests::makeSpellRegressionParty(OpenYAMM::Tests::regressionGameData());
        party.setItemTable(&data.itemTable());
        for (size_t index = 0; index < party.memberCount(); ++index)
        {
            Character &member = *party.member(index);
            member.inventory.clear();
            member.equipment = {};
            member.equipmentRuntime = {};
        }
        party.member(0)->skills["Sword"] = {"Sword", 1, SkillMastery::Normal};
        party.member(0)->skills["Shield"] = {"Shield", 1, SkillMastery::Normal};
        party.refreshDerivedState();
        REQUIRE(party.setActiveMemberIndex(3));
        bindParty(&party);
        bindEventRuntimeState(&state);
        setMapName("out02.odm");
        session.bindDataRepository(&data);
        session.gameplayUiRuntime().bindAssetFileSystem(&files);
        session.bindActiveWorldRuntime(this);
    }

    InventoryItem item(uint32_t id, uint32_t quantity = 1)
    {
        REQUIRE(data.itemTable().get(id) != nullptr);
        InventoryItem value = ItemGenerator::makeInventoryItem(id, data.itemTable(), ItemGenerationMode::Generic);
        value.quantity = quantity;
        return value;
    }

    void give(size_t member, uint32_t id, uint32_t quantity = 1)
    {
        REQUIRE(party.member(member) != nullptr);
        REQUIRE(party.member(member)->addInventoryItem(item(id, quantity)));
    }

    void hold(uint32_t id)
    {
        GameplayHeldItemController::setHeldInventoryItem(session.gameplayScreenRuntime().heldInventoryItem(), item(id));
        party.setHeldItemForQueries(session.gameplayScreenRuntime().heldInventoryItem().item);
    }

    void run(const std::string &body)
    {
        std::string error;
        const std::optional<ScriptedEventProgram> source = ScriptedEventProgram::loadFromLuaText(
            std::string(OpenYAMM::Import::EventSupportSource)
                + "\nRegisterEvent(60146,nil,function()\n" + body + "\nend)\n",
            "@inventory-consumption146", ScriptedEventScope::Map, error);
        REQUIRE_MESSAGE(source.has_value(), error);
        REQUIRE_MESSAGE(events.executeEventById(source, std::nullopt, 60146, state, &party),
            events.lastScriptError().value_or("Inventory event was not executed"));
    }

    void subtract(uint32_t id, const std::string &selector = "Players.Current")
    {
        run("evt.ForPlayer(" + selector + ") RemoveItem(" + std::to_string(id) + ")");
    }

    void deliver()
    {
        GameplayHeldItemController::applyGrantedEventItemsToHeldInventory(
            session.gameplayScreenRuntime(), state, data.itemTable());
    }

    uint32_t ordinaryItemFor(EquipmentSlot slot)
    {
        for (const ItemDefinition &definition : data.itemTable().entries())
        {
            if (definition.itemId == 0 || definition.itemId >= 500 || definition.rarity != ItemRarity::Common)
            {
                continue;
            }
            const std::optional<CharacterEquipPlan> plan = GameMechanics::resolveCharacterEquipPlan(
                *party.member(0), definition, &data.itemTable(), nullptr,
                slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand
                    ? std::nullopt : std::optional<EquipmentSlot>(slot),
                slot == EquipmentSlot::OffHand);
            if (plan && plan->targetSlot == slot)
            {
                return definition.itemId;
            }
        }
        FAIL("No ordinary, legal equipment fixture found for this slot");
        return 0;
    }

    void equip(size_t member, EquipmentSlot slot, InventoryItem value)
    {
        const ItemDefinition *pDefinition = data.itemTable().get(value.objectDescriptionId);
        REQUIRE(pDefinition != nullptr);
        const std::optional<CharacterEquipPlan> plan = GameMechanics::resolveCharacterEquipPlan(
            *party.member(member), *pDefinition, &data.itemTable(), nullptr,
            slot == EquipmentSlot::MainHand || slot == EquipmentSlot::OffHand
                ? std::nullopt : std::optional<EquipmentSlot>(slot),
            slot == EquipmentSlot::OffHand);
        REQUIRE(plan.has_value());
        REQUIRE(plan->targetSlot == slot);
        std::optional<InventoryItem> displaced;
        REQUIRE(party.tryEquipItemOnMember(member, slot, value, plan->displacedSlot,
            plan->autoStoreDisplacedItem, displaced));
        REQUIRE_FALSE(displaced.has_value());
    }
};

int countStored(const Character &member, uint32_t id)
{
    int count = 0;
    for (const InventoryItem &item : member.inventory)
    {
        if (item.objectDescriptionId == id)
        {
            count += int(item.quantity);
        }
    }
    return count;
}
}

TEST_CASE("MM8 inventory subtraction ignores selectors and consumes exactly one active roster item")
{
    for (const std::string selector : {"Players.Current", "Players.All", "0", "2", "4"})
    {
        CAPTURE(selector);
        ConsumptionFixture f;
        f.give(1, 606, 2);
        f.give(2, 606);
        f.subtract(606, selector);
        CHECK(f.party.inventoryItemCount(606, 1) == 1);
        CHECK(f.party.inventoryItemCount(606, 2) == 1);
        CHECK(f.party.inventoryItemCount(606) == 2);
        CHECK(f.state.removedItemIds.empty()); // Already consumed; no second removal during output application.
        f.party.applyEventRuntimeState(f.state);
        CHECK(f.party.inventoryItemCount(606) == 2);
    }
}

TEST_CASE("MM8 inventory subtraction prefers the actual active member over selector and roster order")
{
    ConsumptionFixture f;
    f.give(0, 606);
    f.give(1, 606);
    f.give(3, 606);
    f.subtract(606, "0");
    CHECK(f.party.inventoryItemCount(606, 3) == 0);
    CHECK(f.party.inventoryItemCount(606, 0) == 1);
    CHECK(f.party.inventoryItemCount(606, 1) == 1);
    f.subtract(606, "Players.All");
    CHECK(f.party.inventoryItemCount(606, 0) == 0);
    CHECK(f.party.inventoryItemCount(606, 1) == 1);
}

TEST_CASE("MM8 held consumption updates queries immediately and removes one duplicate per command")
{
    ConsumptionFixture f;
    f.hold(606);
    f.give(3, 606, 2);
    f.give(0, 606);
    f.run(R"lua(
        evt.ForPlayer(0)
        RemoveItem(606)
        if evt.GetHeldItemId() == 0 then SetQBit(QBit(30000)) end
        RemoveItem(606)
    )lua");
    CHECK(f.party.hasQuestBit(30000));
    CHECK(f.party.heldItemIdForQueries() == 0);
    CHECK(f.state.clearHeldItemRequest);
    CHECK(f.party.inventoryItemCount(606, 3) == 1);
    CHECK(f.party.inventoryItemCount(606, 0) == 1);
    f.deliver();
    CHECK_FALSE(f.session.gameplayScreenRuntime().heldInventoryItem().active);
    CHECK_FALSE(f.state.clearHeldItemRequest);
    f.deliver();
    CHECK(f.party.inventoryItemCount(606) == 2);
}

TEST_CASE("held consumption survives a separate event until the real held item controller applies it")
{
    ConsumptionFixture f;
    f.hold(606);
    f.give(0, 606);
    f.subtract(606);
    CHECK(f.party.heldItemIdForQueries() == 0);
    REQUIRE(f.state.clearHeldItemRequest);
    REQUIRE(f.session.gameplayScreenRuntime().heldInventoryItem().active);
    f.run("SetQBit(QBit(30001))");
    CHECK(f.party.hasQuestBit(30001));
    CHECK(f.party.heldItemIdForQueries() == 0);
    CHECK(f.state.clearHeldItemRequest);
    CHECK(f.party.inventoryItemCount(606, 0) == 1);
    f.deliver();
    CHECK_FALSE(f.session.gameplayScreenRuntime().heldInventoryItem().active);
    CHECK_FALSE(f.state.clearHeldItemRequest);
    CHECK(f.party.inventoryItemCount(606, 0) == 1);
}

TEST_CASE("inventory subtraction preserves subsequent selector and noninventory member operations")
{
    ConsumptionFixture f;
    f.give(1, 606);
    std::array<uint32_t, 4> before = {};
    for (size_t index = 0; index < before.size(); ++index)
    {
        before[index] = f.party.member(index)->skillPoints;
    }
    f.run("evt.ForPlayer(2) RemoveItem(606) AddValue(SkillPoints,7) SubtractValue(SkillPoints,2)");
    CHECK(f.party.inventoryItemCount(606) == 0);
    for (size_t index = 0; index < before.size(); ++index)
    {
        CHECK(f.party.member(index)->skillPoints == before[index] + (index == 2 ? 5u : 0u));
    }
}

TEST_CASE("MM8 inventory subtraction checks each member backpack before equipment and active before roster")
{
    ConsumptionFixture f;
    REQUIRE(f.party.setActiveMemberIndex(0));
    const uint32_t id = f.ordinaryItemFor(EquipmentSlot::MainHand);
    f.equip(0, EquipmentSlot::MainHand, f.item(id));
    f.give(0, id);
    f.give(1, id);
    f.subtract(id, "Players.All");
    CHECK(countStored(*f.party.member(0), id) == 0);
    CHECK(f.party.equippedItemId(0, EquipmentSlot::MainHand) == id);
    CHECK(f.party.inventoryItemCount(id, 1) == 1);
    f.subtract(id, "1");
    CHECK(f.party.equippedItemId(0, EquipmentSlot::MainHand) == 0);
    CHECK(f.party.inventoryItemCount(id, 1) == 1);
}

TEST_CASE("MM8 roster fallback checks each member equipment before moving to the next member")
{
    ConsumptionFixture f;
    f.equip(0, EquipmentSlot::Ring6, f.item(137));
    f.give(1, 137);
    REQUIRE(f.party.activeMemberIndex() == 3);
    f.subtract(137, "1");
    CHECK(f.party.equippedItemId(0, EquipmentSlot::Ring6) == 0);
    CHECK(f.party.inventoryItemCount(137, 1) == 1);
    f.subtract(137);
    CHECK(f.party.inventoryItemCount(137) == 0);
}

TEST_CASE("MM8 inventory subtraction removes legal equipment and its runtime state from ordered slots")
{
    for (const EquipmentSlot slot : {EquipmentSlot::OffHand, EquipmentSlot::MainHand, EquipmentSlot::Ring6})
    {
        CAPTURE(int(slot));
        ConsumptionFixture f;
        REQUIRE(f.party.setActiveMemberIndex(0));
        const uint32_t id = f.ordinaryItemFor(slot);
        InventoryItem equipped = f.item(id);
        equipped.standardEnchantId = 1;
        equipped.standardEnchantPower = 7;
        equipped.temporaryBonusRemainingSeconds = 60.0f;
        f.equip(0, slot, equipped);
        f.subtract(id, "3");
        CHECK(f.party.equippedItemId(0, slot) == 0);
        REQUIRE(f.party.equippedItemRuntime(0, slot) != nullptr);
        const EquippedItemRuntimeState &cleared = *f.party.equippedItemRuntime(0, slot);
        CHECK(cleared.standardEnchantId == 0);
        CHECK(cleared.standardEnchantPower == 0);
        CHECK(cleared.specialEnchantId == 0);
        CHECK(cleared.artifactId == 0);
        CHECK(cleared.currentCharges == 0);
        CHECK(cleared.maxCharges == 0);
        CHECK(cleared.temporaryBonusRemainingSeconds == 0.0f);
        CHECK_FALSE(f.party.equippedItem(0, slot).has_value());
    }
    ConsumptionFixture rings;
    REQUIRE(rings.party.setActiveMemberIndex(0));
    rings.equip(0, EquipmentSlot::Ring1, rings.item(137));
    rings.equip(0, EquipmentSlot::Ring6, rings.item(137));
    rings.subtract(137);
    CHECK(rings.party.equippedItemId(0, EquipmentSlot::Ring1) == 0);
    CHECK(rings.party.equippedItemId(0, EquipmentSlot::Ring6) == 137);
}

TEST_CASE("consumed equipment loses derived bonuses and stays absent across save serialization")
{
    ConsumptionFixture f;
    REQUIRE(f.party.setActiveMemberIndex(0));
    const int before = f.party.member(0)->magicalBonuses.resistances.fire;
    f.equip(0, EquipmentSlot::Ring6, f.item(519)); // Original Ring of Planes, legitimate equipment.
    REQUIRE(f.party.member(0)->magicalBonuses.resistances.fire > before);
    f.subtract(519);
    CHECK(f.party.member(0)->magicalBonuses.resistances.fire == before);
    GameSaveData save;
    save.party = f.party.snapshot();
    save.mapFileName = "out02.odm";
    const char *pTemporaryDirectory = std::getenv("TMPDIR");
    REQUIRE(pTemporaryDirectory != nullptr);
    const std::filesystem::path path = std::filesystem::path(pTemporaryDirectory) / "inventory146-equipped.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const std::optional<GameSaveData> cold = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(cold.has_value(), error);
    Party restored;
    restored.setItemTable(&f.data.itemTable());
    restored.restoreSnapshot(cold->party);
    CHECK(restored.equippedItemId(0, EquipmentSlot::Ring6) == 0);
    CHECK(restored.member(0)->magicalBonuses.resistances.fire == before);
}

TEST_CASE("MM8 inventory subtraction never reaches inn or detached owners and missing IDs are harmless")
{
    ConsumptionFixture f;
    f.give(1, 606);
    f.give(2, 606);
    REQUIRE(f.party.detachMembers("inventory146", {{1, 772}}));
    REQUIRE(f.party.dismissMemberToAdventurersInn(1));
    REQUIRE(f.party.setActiveMemberIndex(0));
    const size_t activeCount = f.party.memberCount();
    f.subtract(606, "Players.All");
    CHECK(f.party.memberCount() == activeCount);
    CHECK(f.party.inventoryItemCount(606) == 0);
    REQUIRE(f.party.detachedMember("inventory146", 772) != nullptr);
    CHECK(countStored(f.party.detachedMember("inventory146", 772)->character, 606) == 1);
    REQUIRE(f.party.snapshot().adventurersInnMembers.size() == 1);
    CHECK(countStored(f.party.snapshot().adventurersInnMembers[0].character, 606) == 1);
    f.hold(607);
    f.subtract(606);
    CHECK(f.party.heldItemIdForQueries() == 607);
    CHECK_FALSE(f.state.clearHeldItemRequest);
    CHECK(f.state.removedItemIds.empty());
}

TEST_CASE("OpenYAMM extension consumes pending FIFO only after all physical active party copies")
{
    ConsumptionFixture f;
    InventoryItem first = f.item(606, 2);
    first.standardEnchantPower = 11;
    InventoryItem second = f.item(606);
    second.standardEnchantPower = 22;
    REQUIRE(f.party.grantOwnedQuestItem(first, -1));
    REQUIRE(f.party.grantOwnedQuestItem(second, -1));
    f.give(1, 606);
    f.subtract(606, "0");
    CHECK(f.party.inventoryItemCount(606) == 0);
    CHECK(f.party.pendingItemCount(606) == 3);
    f.subtract(606, "2");
    REQUIRE(f.party.pendingItemDeliveries().size() == 2);
    CHECK(f.party.pendingItemDeliveries()[0].item.quantity == 1);
    CHECK(f.party.pendingItemDeliveries()[1].item.quantity == 1);
    CHECK(f.party.pendingItemDeliveries()[0].item.standardEnchantPower == 11);
    CHECK(f.party.pendingItemDeliveries()[1].item.standardEnchantPower == 22);
    f.subtract(606);
    REQUIRE(f.party.pendingItemDeliveries().size() == 1);
    CHECK(f.party.pendingItemCount(606) == 1);
    CHECK(f.party.pendingItemDeliveries()[0].item.standardEnchantPower == 22);
    f.subtract(606);
    CHECK(f.party.pendingItemCount(606) == 0);
    CHECK(f.party.pendingItemDeliveries().empty());
}

TEST_CASE("OpenYAMM extension respects fixed pending recipients without consuming detached pending owners")
{
    ConsumptionFixture f;
    // Controlled full pack makes a grant to member1 durable pending rather than immediately placed.
    Character &member = *f.party.member(1);
    for (uint8_t y = 0; y < Character::InventoryHeight; ++y)
    {
        for (uint8_t x = 0; x < Character::InventoryWidth; ++x)
        {
            InventoryItem cell;
            cell.objectDescriptionId = 1;
            REQUIRE(member.addInventoryItemAt(cell, x, y));
        }
    }
    REQUIRE(f.party.grantOwnedQuestItem(f.item(606), 1));
    REQUIRE(f.party.pendingItemCount(606, 1) == 1);
    f.subtract(606, "0");
    CHECK(f.party.pendingItemCount(606) == 0);
    REQUIRE(f.party.grantOwnedQuestItem(f.item(606), 1));
    REQUIRE(f.party.detachMembers("pending146", {{1, 772}}));
    f.subtract(606);
    const Party::DetachedPartyMember *pDetached = f.party.detachedMember("pending146", 772);
    REQUIRE(pDetached != nullptr);
    REQUIRE(pDetached->pendingItemDeliveries.size() == 1);
    CHECK(pDetached->pendingItemDeliveries[0].item.objectDescriptionId == 606);
    CHECK(pDetached->pendingItemDeliveries[0].item.quantity == 1);
}
