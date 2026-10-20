#include "EventSupportSource.h"
#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/data/GameDataRepository.h"
#include "game/events/EventRuntime.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/BreachAlvarCompanionTransaction.h"
#include "game/maps/MapIdentity.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <tuple>

using namespace OpenYAMM::Game;

namespace
{
std::filesystem::path output(const std::string &name)
{
    const char *pTemporaryDirectory = std::getenv("TMPDIR");
    REQUIRE(pTemporaryDirectory != nullptr);
    return std::filesystem::path(pTemporaryDirectory) / (name + ".oysav");
}

InventoryItem item(uint32_t id)
{
    InventoryItem value;
    value.objectDescriptionId = id;
    return value;
}

void checkSameItem(const InventoryItem &left, const InventoryItem &right)
{
    CHECK_EQ(left.objectDescriptionId, right.objectDescriptionId);
    CHECK_EQ(left.quantity, right.quantity);
    CHECK_EQ(left.width, right.width);
    CHECK_EQ(left.height, right.height);
    CHECK_EQ(left.gridX, right.gridX);
    CHECK_EQ(left.gridY, right.gridY);
    CHECK_EQ(left.identified, right.identified);
    CHECK_EQ(left.broken, right.broken);
    CHECK_EQ(left.stolen, right.stolen);
    CHECK_EQ(left.standardEnchantId, right.standardEnchantId);
    CHECK_EQ(left.standardEnchantPower, right.standardEnchantPower);
    CHECK_EQ(left.specialEnchantId, right.specialEnchantId);
    CHECK_EQ(left.artifactId, right.artifactId);
    CHECK(left.rarity == right.rarity);
    CHECK_EQ(left.currentCharges, right.currentCharges);
    CHECK_EQ(left.maxCharges, right.maxCharges);
    CHECK_EQ(left.temporaryBonusRemainingSeconds, right.temporaryBonusRemainingSeconds);
}

struct GrantFixture : OpenYAMM::Tests::PartySpellTestWorldRuntime
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository data;
    Party party;
    EventRuntimeState state;
    EventRuntime events;
    GameSession session;
    bool dropSuccess = false;
    std::vector<InventoryItem> droppedItems;

    GrantFixture()
    {
        REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(), OpenYAMM::Tests::regressionGameDataFailure());
        const char *pRoot = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
        REQUIRE(pRoot != nullptr);
        REQUIRE(files.initialize(pRoot, pRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mm7"));
        loader.setActiveWorldId("mm7");
        REQUIRE(loader.loadCommonForGameplay(files));
        data.bind(loader);
        party = OpenYAMM::Tests::makeSpellRegressionParty(OpenYAMM::Tests::regressionGameData());
        party.setItemTable(&data.itemTable());
        bindParty(&party);
        bindEventRuntimeState(&state);
        setMapName("7d37.blv");
        session.bindDataRepository(&data);
        session.gameplayUiRuntime().bindAssetFileSystem(&files);
        session.bindActiveWorldRuntime(this);
    }

    std::optional<GameplayHeldItemDropRequest> buildHeldItemDropRequest() const override
    {
        return GameplayHeldItemDropRequest{};
    }

    bool dropHeldItemToWorld(const GameplayHeldItemDropRequest &request) override
    {
        if (!dropSuccess)
        {
            return false;
        }
        droppedItems.push_back(request.item);
        return true;
    }

    GameplayScreenRuntime &screen() { return session.gameplayScreenRuntime(); }

    void fillPacks()
    {
        for (size_t index = 0; index < party.memberCount(); ++index)
        {
            Character &member = *party.member(index);
            member.inventory.clear();
            for (int y = 0; y < Character::InventoryHeight; ++y)
            {
                for (int x = 0; x < Character::InventoryWidth; ++x)
                {
                    REQUIRE(member.addInventoryItemAt(item(1), uint8_t(x), uint8_t(y)));
                }
            }
        }
    }

    void hold(uint32_t id)
    {
        GameplayHeldItemController::setHeldInventoryItem(screen().heldInventoryItem(), item(id));
        party.setHeldItemForQueries(screen().heldInventoryItem().item);
    }

    std::optional<ScriptedEventProgram> program(const std::string &source)
    {
        std::string error;
        std::optional<ScriptedEventProgram> result = ScriptedEventProgram::loadFromLuaText(
            std::string(OpenYAMM::Import::EventSupportSource) + "\n" + source,
            "@event-item-ownership142", ScriptedEventScope::Map, error);
        REQUIRE_MESSAGE(result.has_value(), error);
        return result;
    }

    std::optional<ScriptedEventProgram> portraitProgram()
    {
        const std::optional<std::string> source = files.readTextFile("Data/scripts/maps/7d37.lua");
        REQUIRE(source.has_value());
        REQUIRE(source->find("InventoryItem(1423)") != std::string::npos);
        return program(*source);
    }

    bool run(const std::optional<ScriptedEventProgram> &source, uint16_t event = 60100, bool expectSuccess = true)
    {
        const bool executed = events.executeEventById(source, std::nullopt, event, state, &party);
        if (expectSuccess && !executed)
        {
            REQUIRE_MESSAGE(executed, events.lastScriptError().value_or("Event handler not executed"));
        }
        return executed;
    }

    void deliver()
    {
        GameplayHeldItemController::applyGrantedEventItemsToHeldInventory(screen(), state, data.itemTable());
    }

    uint32_t commonItemInCategory(const std::string &equipStat) const
    {
        const std::vector<ItemDefinition> &entries = loader.getItemTable().entries();
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const ItemDefinition &definition)
        {
            return definition.itemId != 0 && definition.equipStat == equipStat
                && definition.rarity == ItemRarity::Common;
        });
        REQUIRE_MESSAGE(found != entries.end(), equipStat);
        return found->itemId;
    }

    void useOnlyRandomItems(const std::vector<uint32_t> &itemIds)
    {
        // Controlled table owned by this fixture. A singleton pool makes the
        // category oracle independent of any standard library distribution.
        for (const ItemDefinition &entry : loader.getItemTable().entries())
        {
            ItemDefinition &definition = const_cast<ItemDefinition &>(entry);
            std::erase(definition.contentFlags, std::string("NoRandom"));
            definition.randomTreasureWeights.fill(1);
            if (std::find(itemIds.begin(), itemIds.end(), entry.itemId) == itemIds.end())
            {
                definition.contentFlags.push_back("NoRandom");
            }
        }
    }

    GameSaveData saveData()
    {
        GameSaveData save;
        save.mapFileName = "7d37.blv";
        save.party = party.snapshot();
        save.heldInventoryItemActive = screen().heldInventoryItem().active;
        save.heldInventoryItem = screen().heldInventoryItem().item;
        return save;
    }
};

std::vector<char> bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
}

TEST_CASE("imported Haunted Mansion portrait remains owned after failed delivery events maps and cold save")
{
    std::optional<ScriptedEventProgram> source;
    std::optional<GameSaveData> saved;
    std::string error;
    {
        GrantFixture f;
        f.fillPacks();
        f.hold(2125);
        source = f.portraitProgram();
        REQUIRE(f.run(source, 376));
        CHECK(f.party.hasQuestBit(778));
        CHECK_EQ(f.party.pendingItemCount(1423), 1);
        CHECK(f.state.grantedItemIds.empty());
        CHECK(f.state.grantedItems.empty());
        f.deliver();
        CHECK_EQ(f.party.heldItemIdForQueries(), 2125u);
        CHECK_EQ(f.party.pendingItemCount(1423), 1);
        REQUIRE(f.run(f.program("RegisterEvent(60100, nil, function() end)")));
        clearTransientEventRuntimeState(f.state);
        f.setMapName("out01.odm");
        f.state = {};
        f.state.activeHistoryContinentId = 1;
        f.screen().clearUiControllerRuntimeState();
        CHECK_EQ(f.party.pendingItemCount(1423), 1);

        REQUIRE_MESSAGE(saveGameDataToPath(output("portrait142-pending"), f.saveData(), error), error);
        saved = loadGameDataFromPath(output("portrait142-pending"), error);
        REQUIRE_MESSAGE(saved.has_value(), error);
    }
    // AssetFileSystem owns process-wide PhysicsFS; destroy the first session before cold loading.
    GrantFixture cold;
    cold.party.restoreSnapshot(saved->party);
    cold.session.restoreHeldInventoryItemFromSaveData(*saved);
    REQUIRE(cold.run(source, 376));
    CHECK_EQ(cold.party.pendingItemCount(1423), 1);
    cold.dropSuccess = true;
    cold.deliver();
    CHECK_EQ(cold.party.heldItemIdForQueries(), 1423u);
    CHECK_FALSE(cold.party.ownsPendingItem(1423));
    REQUIRE_EQ(cold.droppedItems.size(), 1u);
    CHECK_EQ(cold.droppedItems[0].objectDescriptionId, 2125u);
    cold.deliver();
    REQUIRE(cold.run(source, 376));
    CHECK_EQ(cold.droppedItems.size(), 1u);
    CHECK_EQ(cold.party.heldItemIdForQueries(), 1423u);
    CHECK_FALSE(cold.party.ownsPendingItem(1423));
}

TEST_CASE("event pending capacity errors stop subsequent source quest bits for Add Set Give and SetHeld")
{
    for (const std::string command : {"AddValue(InventoryItem(1423),1423)",
             "SetValue(InventoryItem(1423),1)", "evt.GiveItem(1423)", "evt.SetHeldItem(1423)"})
    {
        CAPTURE(command);
        GrantFixture f;
        for (size_t index = 0; index < Party::MaxPendingItemDeliveries; ++index)
        {
            REQUIRE(f.party.grantOwnedQuestItem(item(543), -1));
        }
        const auto source = f.program("RegisterEvent(60100,nil,function() " + command
            + " SetQBit(QBit(778)) end)");
        CHECK_FALSE(f.run(source, 60100, false));
        REQUIRE(f.events.lastScriptError().has_value());
        CHECK(f.events.lastScriptError()->find("capacity exhausted") != std::string::npos);
        CHECK_FALSE(f.party.hasQuestBit(778));
        CHECK_FALSE(f.party.ownsPendingItem(1423));
        f.party.completeFirstPendingItemDelivery();
        REQUIRE(f.run(source));
        CHECK(f.party.hasQuestBit(778));
        CHECK_EQ(f.party.pendingItemCount(1423), 1);
    }
}

TEST_CASE("pending generic grants support party comparisons and consumption without duplicate inventory owners")
{
    GrantFixture f;
    REQUIRE(f.run(f.program(R"lua(
RegisterEvent(60100,nil,function()
    evt.ForPlayer(0)
    AddValue(InventoryItem(1423),1423)
    if evt.Cmp(InventoryItem(1423),1423) then error("mouse grant belongs to a fixed member") end
end)
)lua")));
    REQUIRE(f.run(f.program(R"lua(
RegisterEvent(60100,nil,function()
    if not evt.Cmp(InventoryItem(1423),1423) then error("missing owned pending item") end
    SubtractValue(InventoryItem(1423),1423)
    if evt.Cmp(InventoryItem(1423),1423) then error("consumed pending item still owned") end
end)
)lua")));
    CHECK_FALSE(f.party.ownsPendingItem(1423));
    CHECK_EQ(f.party.inventoryItemCount(1423), 0);
    f.deliver();
    CHECK_FALSE(f.screen().heldInventoryItem().active);
    REQUIRE(f.party.grantOwnedQuestItem(item(1423), 2));
    CHECK_EQ(f.party.inventoryItemCount(1423, 2), 1);
    REQUIRE(f.run(f.program("RegisterEvent(60100,nil,function() evt.GiveItem(1423) end)")));
    REQUIRE(f.party.removeItem(1423, 2));
    CHECK_FALSE(f.party.hasItemAnywhere(1423));
}

TEST_CASE("SetHeld after ClearHeld consumes old cursor before delivering new complete item fields")
{
    GrantFixture f;
    f.fillPacks();
    f.hold(2125);
    REQUIRE(f.run(f.program(R"lua(
RegisterEvent(60100,nil,function()
    evt.ClearHeldItem()
    evt.SetHeldItem(947,{identified=true,charges=13,maxCharges=17,bonus=19,enchantment=2,artifact=5})
end)
)lua")));
    REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 1u);
    const InventoryItem granted = f.party.pendingItemDeliveries()[0].item;
    CHECK_EQ(granted.currentCharges, 13);
    CHECK_EQ(granted.maxCharges, 17);
    CHECK_EQ(granted.standardEnchantPower, 19);
    CHECK_EQ(granted.standardEnchantId, 2);
    CHECK_EQ(granted.artifactId, 5);
    f.deliver();
    checkSameItem(f.screen().heldInventoryItem().item, granted);
    CHECK(f.droppedItems.empty());
    CHECK_EQ(f.party.inventoryItemCount(2125), 0);
    CHECK(f.party.pendingItemDeliveries().empty());
}

TEST_CASE("random GiveItem keeps generated fields through failed delivery and save89 without reroll")
{
    GrantFixture f;
    f.fillPacks();
    f.hold(2125);
    REQUIRE(f.run(f.program("RegisterEvent(60100,nil,function() evt.GiveItem(6,0,0) end)")));
    REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 1u);
    const InventoryItem granted = f.party.pendingItemDeliveries()[0].item;
    CHECK_NE(granted.objectDescriptionId, 0u);
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        f.deliver();
        REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 1u);
        checkSameItem(f.party.pendingItemDeliveries()[0].item, granted);
    }
    std::string error;
    REQUIRE(saveGameDataToPath(output("random142-pending"), f.saveData(), error));
    const auto saved = loadGameDataFromPath(output("random142-pending"), error);
    REQUIRE_MESSAGE(saved.has_value(), error);
    REQUIRE_EQ(saved->party.pendingItemDeliveries.size(), 1u);
    checkSameItem(saved->party.pendingItemDeliveries[0].item, granted);
    CHECK_FALSE(saved->party.pendingItemDeliveries[0].allowDropProtectedHeldItem);
}

TEST_CASE("random GiveItem source types 16 and 45 filter the shared generator at every treasure strength")
{
    GrantFixture f;
    const uint32_t scroll = f.commonItemInCategory("Sscroll");
    const uint32_t reagent = f.commonItemInCategory("Reagent");
    f.useOnlyRandomItems({scroll, reagent, f.commonItemInCategory("Book"), f.commonItemInCategory("Mscroll"),
                         f.commonItemInCategory("Bottle"), f.commonItemInCategory("Weapon")});
    struct Category
    {
        uint32_t type;
        uint32_t expectedItem;
    };
    const Category categories[] = {{16, scroll}, {45, reagent}};
    for (const Category &category : categories)
    {
        for (uint32_t strength = 1; strength <= 6; ++strength)
        {
            CAPTURE(category.type);
            CAPTURE(strength);
            const auto source = f.program("RegisterEvent(60100,nil,function() evt.GiveItem("
                + std::to_string(strength) + "," + std::to_string(category.type) + ",0) end)");
            for (int sample = 0; sample < 8; ++sample)
            {
                REQUIRE(f.run(source));
                REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 1u);
                CHECK_EQ(f.party.pendingItemDeliveries()[0].item.objectDescriptionId, category.expectedItem);
                CHECK(f.state.grantedItems.empty());
                f.party.completeFirstPendingItemDelivery();
            }
        }
    }
}

TEST_CASE("random GiveItem source categories reject incompatible singleton pools without a fallback")
{
    GrantFixture f;
    // ConstAndBits.lua distinguishes Scroll=16 (sscroll) from Book/MScroll;
    // Reagent_=45 is the original Bucket's reagent category, not a potion.
    const char *categories[] = {"Sscroll", "Reagent", "Book", "Mscroll", "Bottle", "Weapon"};
    for (const uint32_t requestedType : {16u, 45u})
    {
        for (const char *pCategory : categories)
        {
            if ((requestedType == 16 && std::string(pCategory) == "Sscroll")
                || (requestedType == 45 && std::string(pCategory) == "Reagent"))
            {
                continue;
            }
            CAPTURE(requestedType);
            CAPTURE(pCategory);
            f.useOnlyRandomItems({f.commonItemInCategory(pCategory)});
            REQUIRE(f.run(f.program("RegisterEvent(60100,nil,function() evt.GiveItem(6,"
                + std::to_string(requestedType) + ",0) end)")));
            CHECK(f.party.pendingItemDeliveries().empty());
            CHECK(f.state.grantedItems.empty());
        }
    }
}

TEST_CASE("GiveItem explicit IDs bypass random category filters and do not consume the random stream")
{
    GrantFixture f;
    const uint32_t weapon = f.commonItemInCategory("Weapon");
    f.useOnlyRandomItems({f.commonItemInCategory("Sscroll"), f.commonItemInCategory("Reagent")});
    f.state.eventRandomState = 0x14812345u;
    const std::string id = std::to_string(weapon);
    REQUIRE(f.run(f.program("RegisterEvent(60100,nil,function() evt.GiveItem(" + id
        + "); evt.GiveItem(6,16," + id + "); evt.GiveItem(1,45," + id + ") end)")));
    REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 3u);
    CHECK_EQ(f.party.pendingItemDeliveries()[0].item.objectDescriptionId, weapon);
    checkSameItem(f.party.pendingItemDeliveries()[0].item, f.party.pendingItemDeliveries()[1].item);
    checkSameItem(f.party.pendingItemDeliveries()[0].item, f.party.pendingItemDeliveries()[2].item);
    CHECK_EQ(f.state.eventRandomState, 0x14812345u);
}

TEST_CASE("typed random GiveItem payloads survive failed delivery and cold save without reroll")
{
    GrantFixture f;
    f.fillPacks();
    f.hold(2125);
    const uint32_t scroll = f.commonItemInCategory("Sscroll");
    const uint32_t reagent = f.commonItemInCategory("Reagent");
    f.useOnlyRandomItems({scroll, reagent});
    REQUIRE(f.run(f.program(
        "RegisterEvent(60100,nil,function() evt.GiveItem(1,16,0); evt.GiveItem(1,45,0) end)")));
    REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 2u);
    const InventoryItem first = f.party.pendingItemDeliveries()[0].item;
    const InventoryItem second = f.party.pendingItemDeliveries()[1].item;
    CHECK_EQ(first.objectDescriptionId, scroll);
    CHECK_EQ(second.objectDescriptionId, reagent);
    const uint32_t randomStateAfterGeneration = f.state.eventRandomState;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        f.deliver();
        REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 2u);
        checkSameItem(f.party.pendingItemDeliveries()[0].item, first);
        checkSameItem(f.party.pendingItemDeliveries()[1].item, second);
        CHECK_EQ(f.state.eventRandomState, randomStateAfterGeneration);
    }
    std::string error;
    REQUIRE(saveGameDataToPath(output("typed-random148-pending"), f.saveData(), error));
    const auto saved = loadGameDataFromPath(output("typed-random148-pending"), error);
    REQUIRE_MESSAGE(saved.has_value(), error);
    REQUIRE_EQ(saved->party.pendingItemDeliveries.size(), 2u);
    checkSameItem(saved->party.pendingItemDeliveries[0].item, first);
    checkSameItem(saved->party.pendingItemDeliveries[1].item, second);
    for (const Party::PendingItemDelivery &delivery : saved->party.pendingItemDeliveries)
    {
        CHECK_FALSE(delivery.allowDropProtectedHeldItem);
        CHECK_EQ(delivery.recipientMemberIndex, -1);
    }
}

TEST_CASE("generic pending delivery honors NoDrop and retains sole owner until pack space becomes available")
{
    GrantFixture f;
    f.fillPacks();
    f.hold(2125);
    // The fixture owns this loader and table; add a controlled mod flag without editing global tables or assets.
    ItemDefinition *pProtected = const_cast<ItemDefinition *>(f.loader.getItemTable().get(2125));
    REQUIRE(pProtected != nullptr);
    pProtected->contentFlags.push_back("NoDrop");
    f.dropSuccess = true;
    REQUIRE(f.run(f.program("RegisterEvent(60100,nil,function() evt.GiveItem(1423) end)")));
    f.deliver();
    CHECK_EQ(f.party.heldItemIdForQueries(), 2125u);
    CHECK_EQ(f.party.pendingItemCount(1423), 1);
    CHECK(f.droppedItems.empty());
    f.party.member(0)->inventory.clear();
    f.deliver();
    CHECK_EQ(f.party.inventoryItemCount(2125), 1);
    CHECK_EQ(f.party.heldItemIdForQueries(), 1423u);
    CHECK_FALSE(f.party.ownsPendingItem(1423));
    CHECK(f.droppedItems.empty());
}

TEST_CASE("save88 pending owners retain source TakeOrDrop policy across current schema migration")
{
    // Read an actual checkpoint-142 version-88 fixture. Altering the current
    // writer's version word is not a valid old-format migration test.
    const std::filesystem::path legacyPath = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/campaign-readiness-142/event_item_ownership_check-output-002"
        / "policy142-version88-controlled.oysav";
    const std::vector<char> legacyBytes = bytes(legacyPath);
    REQUIRE_GT(legacyBytes.size(), 12u);
    CHECK_EQ(uint8_t(legacyBytes[8]), 88);
    CHECK_EQ(legacyBytes[9], 0);
    CHECK_EQ(legacyBytes[10], 0);
    CHECK_EQ(legacyBytes[11], 0);
    std::string error;
    const auto migrated = loadGameDataFromPath(legacyPath, error);
    REQUIRE_MESSAGE(migrated.has_value(), error);
    REQUIRE_EQ(migrated->party.pendingItemDeliveries.size(), 1u);
    CHECK(migrated->party.pendingItemDeliveries[0].allowDropProtectedHeldItem);
    CHECK_EQ(migrated->party.pendingItemDeliveries[0].item.objectDescriptionId, 1423u);
    REQUIRE(saveGameDataToPath(output("policy-legacy-migrated"), *migrated, error));
    const auto migratedAgain = loadGameDataFromPath(output("policy-legacy-migrated"), error);
    REQUIRE_MESSAGE(migratedAgain.has_value(), error);
    REQUIRE_EQ(migratedAgain->party.pendingItemDeliveries.size(), 1u);
    CHECK(migratedAgain->party.pendingItemDeliveries[0].allowDropProtectedHeldItem);

    GrantFixture f;
    REQUIRE(f.party.grantOwnedQuestItem(item(1423), -1, false));
    REQUIRE(saveGameDataToPath(output("policy-current"), f.saveData(), error));
    const auto modern = loadGameDataFromPath(output("policy-current"), error);
    REQUIRE_MESSAGE(modern.has_value(), error);
    REQUIRE_EQ(modern->party.pendingItemDeliveries.size(), 1u);
    CHECK_FALSE(modern->party.pendingItemDeliveries[0].allowDropProtectedHeldItem);
}

namespace
{
struct BreachBridgeDialogueWorld : OutdoorWorldRuntime
{
    // Headless equivalent of the view's forwarding call. The real controller and Lua still execute;
    // this seam supplies no dialogue context, request, Party mutation or success result of its own.
    std::function<bool(uint16_t, size_t &, std::optional<uint8_t>)> dispatchNpcTopic;

    bool executeNpcTopicEvent(uint16_t id, size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        return dispatchNpcTopic && dispatchNpcTopic(id, previousMessageCount, continueStep);
    }
};

// Controlled bridge fixture: real OutdoorWorldRuntime/Session and source profile, flat geometry,
// hand-authored actor snapshot plus a pure prepared receipt. No GPU spawn or live split is asserted.
struct BreachBridgeFixture
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository data;
    GameSession session;
    BreachBridgeDialogueWorld world;
    EventRuntime events;
    std::optional<OutdoorMapData> geometry = OutdoorMapData{};
    MapStatsEntry map;
    MonsterProjectileTable projectiles;
    ObjectTable objects;
    GameSaveData baseline;

    explicit BreachBridgeFixture(bool split = true, uint32_t returnedNpc = 0)
    {
        REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(), OpenYAMM::Tests::regressionGameDataFailure());
        const char *pRoot = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
        REQUIRE(pRoot != nullptr);
        REQUIRE(files.initialize(pRoot, pRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mmmerge"));
        const char *pInspection = std::getenv("OPENYAMM_BREACH_ALVAR_INSPECTION_ROOT");
        REQUIRE(pInspection != nullptr);
        REQUIRE(files.mountDevelopmentRoot(pInspection));
        loader.setActiveWorldId("mmmerge");
        REQUIRE(loader.loadCommonForGameplay(files));
        data.bind(loader);
        REQUIRE_MESSAGE(data.breachAlvarCompanionProfile(), "Bridge149 requires an imported149 source profile");
        const auto found = std::find_if(data.mapEntries().begin(), data.mapEntries().end(), [](const auto &entry)
        {
            return entry.canonicalId == buildCanonicalMapId("mmmerge", "bralvar.odm");
        });
        REQUIRE(found != data.mapEntries().end());
        map = *found;
        geometry->heightMap.assign(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 151);
        geometry->attributeMap.assign(geometry->heightMap.size(), 0);
        events = EventRuntime(&data.houseTable(), &data.npcDialogTable());
        session.bindDataRepository(&data);
        Party source = OpenYAMM::Tests::makeSpellRegressionParty(OpenYAMM::Tests::regressionGameData());
        PartySeed seed = OpenYAMM::Tests::createSpellRegressionPartySeed();
        seed.members.push_back(seed.members.front());
        seed.members.back().name = "Fifth bridge fixture member";
        source.seed(seed);
        baseline.party = source.snapshot();
        baseline.currentSceneKind = SceneKind::Outdoor;
        baseline.mapFileName = "bralvar.odm";
        baseline.savedGameMinutes = 120.0;
        baseline.hasOutdoorRuntimeState = true;
        baseline.outdoorWorld.gameMinutes = 120.0;
        baseline.outdoorWorld.eventRuntimeState.emplace();
        baseline.outdoorWorld.eventRuntimeState->mapFileName = baseline.mapFileName;
        if (split)
        {
            const auto prepared = prepareBreachAlvarFiveMemberSplit(source, std::nullopt,
                data.characterDollTable(), {}, 43129);
            REQUIRE_MESSAGE(prepared.mutation, prepared.error);
            baseline.party = prepared.mutation->party().snapshot();
            baseline.breachAlvarCompanions = BreachAlvarCompanionState{
                prepared.mutation->receipt(), {std::string(BreachAlvarCompanionMap), 0, 417}};
            baseline.namedGlobalVars["MMerge.CrossContinents.BreachSplit"] = 1;
            OutdoorWorldRuntime::MapActorState actor;
            actor.actorId = 0; // Stable actor zero is valid; optional absence is distinct.
            actor.monsterId = 417;
            actor.npcId = 772;
            actor.spawnedAtRuntime = true;
            actor.group = 50;
            actor.ally = 9999;
            actor.currentHp = 500;
            actor.maxHp = 600;
            actor.breachAlvarCompanion = true;
            actor.breachAlvarNextMaintenanceGameMinute = 121.0;
            baseline.outdoorWorld.mapActors.push_back(actor);
            baseline.outdoorWorld.nextActorId = 1;
            if (returnedNpc != 0)
            {
                const auto returned = prepareBreachAlvarCompanionReturn(prepared.mutation->party(),
                    baseline.breachAlvarCompanions, data.characterDollTable(), {}, returnedNpc);
                REQUIRE_MESSAGE(returned.mutation, returned.error);
                baseline.party = returned.mutation->party().snapshot();
                baseline.breachAlvarCompanions->receipt = returned.mutation->receipt();
                if (returnedNpc == 772)
                {
                    auto &removed = baseline.outdoorWorld.mapActors[0];
                    removed.breachAlvarCompanionRemoved = true;
                    removed.isInvisible = true;
                    removed.canReceiveDamage = false;
                }
            }
        }
        restore(baseline);
    }

    ~BreachBridgeFixture() { session.bindActiveWorldRuntime(nullptr); }

    Party &party() { return *session.partyState(); }
    EventRuntimeState &state() { return *world.eventRuntimeState(); }

    void restore(GameSaveData save)
    {
        session.bindActiveWorldRuntime(nullptr);
        save.outdoorWorld.eventRuntimeState->namedGlobalVars = save.namedGlobalVars;
        session.restoreFromSaveData(save);
        world.initialize(map, data.monsterTable(), projectiles, objects, data.spellTable(), data.itemTable(),
            &party(), nullptr, data.standardItemEnchantTable(), data.specialItemEnchantTable(), nullptr,
            geometry, std::nullopt, std::nullopt, save.outdoorWorld.eventRuntimeState);
        world.restoreSnapshot(save.outdoorWorld);
        session.bindActiveWorldRuntime(&world);
        session.applyNamedGlobalVarsToRuntime(state());
    }

    static std::string overlay(const std::string &name)
    {
        const auto path = std::filesystem::path(OPENYAMM_SOURCE_DIR) / "tools/mmerge_import/overlays" / name;
        std::ifstream input(path);
        REQUIRE_MESSAGE(input.good(), path.string());
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    std::optional<ScriptedEventProgram> program(const std::string &source, ScriptedEventScope scope)
    {
        std::string error;
        const auto value = ScriptedEventProgram::loadFromLuaText(
            std::string(OpenYAMM::Import::EventSupportSource) + "\n" + source, "@controlled-breach-bridge149",
            scope, error);
        REQUIRE_MESSAGE(value, error);
        return value;
    }

    void dialogue(uint32_t npc, uint32_t house, std::optional<uint32_t> actor = std::nullopt)
    {
        EventRuntimeState::PendingDialogueContext context;
        context.kind = DialogueContextKind::NpcTalk;
        context.sourceId = npc;
        context.hostHouseId = house;
        context.sourceActorIndex = actor;
        state().pendingDialogueContext = context;
        state().dialogueState.hostHouseId = house;
    }

    void topic(const std::optional<ScriptedEventProgram> &global, uint16_t id = 1789)
    {
        REQUIRE_MESSAGE(events.executeNpcTopicEventById(std::nullopt, global, id, state(), &party(), &world),
            events.lastScriptError().value_or("topic did not run"));
    }
};
}

TEST_CASE("Breach149 Lua split only queues from source MapOnLoad and survives later event outputs")
{
    BreachBridgeFixture f(false);
    const auto local = f.program(BreachBridgeFixture::overlay("Map_mmmerge_br_alvar_companions.lua"),
        ScriptedEventScope::Map);
    const auto global = f.program(R"lua(
        RegisterGlobalAfterLoadEvent(65001, nil, function() evt.SimpleMessage("after load") end)
        RegisterGlobalMapRefillHook(65002, nil, function() evt.SimpleMessage("refill") end)
        RegisterGlobalEvent(65003, nil, function() evt.SimpleMessage("next event") end)
    )lua", ScriptedEventScope::Global);
    const auto before = encodePartySnapshotForComparison(f.party().snapshot());
    // An ordinary dispatch of the same event ID is not a map load.
    REQUIRE(f.events.executeEventById(local, global, 65469, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    REQUIRE(f.events.executeOnLoadEvents(local, global, f.state(), &f.party(), &f.world));
    REQUIRE(f.state().pendingBreachAlvarCompanionRequest);
    const auto request = f.state().pendingBreachAlvarCompanionRequest;
    CHECK(request->kind == EventRuntimeState::PendingBreachAlvarCompanionRequest::Kind::Split);
    CHECK(request->sourceEventId == 65469);
    CHECK_FALSE(request->executingGlobalHandler);
    // Requests retain the registry spelling (BrAlvar.odm); admission uses the canonical identity separately.
    CHECK(request->sourceMapName == f.map.fileName);
    CHECK(request->dialogueKind == DialogueContextKind::None);
    REQUIRE(f.events.executeOnLoadEvents(local, global, f.state(), &f.party(), &f.world));
    REQUIRE(f.events.executeGlobalAfterLoadEvents(local, global, f.state(), &f.party(), &f.world));
    EventRuntimeState::ActiveHookContext hook;
    hook.kind = EventRuntimeHookKind::MapRefill;
    f.state().activeHookContext = hook;
    REQUIRE(f.events.executeHooks(local, global, hook.kind, f.state(), &f.party(), &f.world));
    f.state().activeHookContext.reset();
    REQUIRE(f.events.executeEventById(local, global, 65003, f.state(), &f.party(), &f.world));
    CHECK(f.state().pendingBreachAlvarCompanionRequest == request);
    CHECK(encodePartySnapshotForComparison(f.party().snapshot()) == before);
    CHECK_FALSE(f.session.breachAlvarCompanions());
    CHECK_FALSE(f.party().hasQuestBit(1714));
    CHECK_FALSE(f.state().namedGlobalVars.contains("MMerge.CrossContinents.BreachSplit"));
    CHECK(f.world.snapshot().mapActors.empty());
    clearTransientEventRuntimeState(f.state());
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
}

TEST_CASE("Breach149 Lua rejects forged split scope event map scene profile and failed handlers")
{
    BreachBridgeFixture f(false);
    const auto good = f.program("RegisterMapOnLoadEvent(65469, nil, function() "
        "evt.RequestBreachAlvarSplit() end)", ScriptedEventScope::Map);
    const auto wrongEvent = f.program("RegisterMapOnLoadEvent(65468, nil, function() "
        "evt._BeginEvent(65469); assert(not evt.RequestBreachAlvarSplit()) end)", ScriptedEventScope::Map);
    const auto global = f.program("RegisterGlobalOnLoadEvent(65469, nil, function() "
        "assert(not evt.RequestBreachAlvarSplit()) end)", ScriptedEventScope::Global);
    REQUIRE(f.events.executeOnLoadEvents(wrongEvent, global, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.state().mapFileName = "out01.odm";
    REQUIRE(f.events.executeOnLoadEvents(good, std::nullopt, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.state().mapFileName = "bralvar.odm";
    REQUIRE(f.events.executeOnLoadEvents(good, std::nullopt, f.state(), &f.party(), nullptr));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    GameDataLoader emptyLoader;
    GameDataRepository missingProfile;
    missingProfile.bind(emptyLoader);
    f.session.bindDataRepository(&missingProfile);
    REQUIRE(f.events.executeOnLoadEvents(good, std::nullopt, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.session.bindDataRepository(&f.data);
    f.dialogue(773, 705);
    REQUIRE(f.events.executeOnLoadEvents(good, std::nullopt, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.state().pendingDialogueContext.reset();
    const auto failure = f.program("RegisterMapOnLoadEvent(65469, nil, function() "
        "assert(evt.RequestBreachAlvarSplit()); error('controlled queue rollback') end)", ScriptedEventScope::Map);
    f.events.executeOnLoadEvents(failure, std::nullopt, f.state(), &f.party(), &f.world);
    REQUIRE(f.events.lastScriptError());
    CHECK(f.events.lastScriptError()->find("controlled queue rollback") != std::string::npos);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    CHECK_FALSE(f.session.breachAlvarCompanions());
    CHECK_FALSE(f.party().hasQuestBit(1714));
}

TEST_CASE("Breach149 Lua return captures cleared NPC house and stable actor zero without returning anyone")
{
    BreachBridgeFixture f;
    const auto global = f.program(BreachBridgeFixture::overlay("Global_mmmerge_br_alvar_companions.lua"),
        ScriptedEventScope::Global);
    const auto before = encodePartySnapshotForComparison(f.party().snapshot());
    const auto receipt = f.session.breachAlvarCompanions();
    f.dialogue(772, 0, 0);
    f.topic(global);
    CHECK_FALSE(f.state().pendingDialogueContext); // _BeginEvent really cleared the original context.
    REQUIRE(f.state().pendingBreachAlvarCompanionRequest);
    auto request = *f.state().pendingBreachAlvarCompanionRequest;
    CHECK(request.npcId == 772);
    CHECK(request.sourceNpcId == 772);
    CHECK(request.sourceActorIndex == std::optional<uint32_t>(0));
    CHECK(request.stableActorId == std::optional<uint32_t>(0));
    CHECK(request.dialogueKind == DialogueContextKind::NpcTalk);
    CHECK(request.hostHouseId == 0);
    CHECK(request.sourceEventId == 1789);
    CHECK(request.executingGlobalHandler);
    f.dialogue(772, 0, 0);
    f.topic(global);
    CHECK(*f.state().pendingBreachAlvarCompanionRequest == request); // Same request coalesces.
    f.dialogue(773, 705);
    f.topic(global);
    CHECK(*f.state().pendingBreachAlvarCompanionRequest == request); // Another NPC cannot overwrite it.
    f.state().pendingBreachAlvarCompanionRequest.reset(); // Simulated App consumption, not a commit.
    f.dialogue(773, 705);
    f.topic(global);
    REQUIRE(f.state().pendingBreachAlvarCompanionRequest);
    CHECK(f.state().pendingBreachAlvarCompanionRequest->npcId == 773);
    CHECK(f.state().pendingBreachAlvarCompanionRequest->hostHouseId == 705);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest->sourceActorIndex);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest->stableActorId);
    CHECK(encodePartySnapshotForComparison(f.party().snapshot()) == before);
    CHECK(f.session.breachAlvarCompanions() == receipt);
    CHECK_FALSE(f.world.mapActorState(0)->breachAlvarCompanionRemoved);
}

TEST_CASE("Breach149 Lua return rejects wrong NPC scope hook house and stale actor identities")
{
    BreachBridgeFixture f;
    const auto global = f.program(R"lua(
        RegisterGlobalEvent(1789, nil, function() assert(not evt.RequestBreachAlvarCompanionReturn(772)) end)
        RegisterGlobalEvent(1791, nil, function()
            evt._BeginEvent(1789); assert(not evt.RequestBreachAlvarCompanionReturn(772))
        end)
        RegisterGlobalNpcEnterHook(1789, nil, function()
            assert(not evt.RequestBreachAlvarCompanionReturn(772))
        end)
    )lua", ScriptedEventScope::Global);
    // Hook registration replaces the same global handler; use a separate hook program below.
    const auto calls = f.program(R"lua(
        RegisterGlobalEvent(1789, nil, function() assert(not evt.RequestBreachAlvarCompanionReturn(772)) end)
        RegisterGlobalEvent(1791, nil, function()
            evt._BeginEvent(1789); assert(not evt.RequestBreachAlvarCompanionReturn(772))
        end)
    )lua", ScriptedEventScope::Global);
    for (const auto &[npc, house, actor] : std::vector<std::tuple<uint32_t, uint32_t, std::optional<uint32_t>>>{
            {773, 705, std::nullopt}, {772, 705, 0}, {772, 0, std::nullopt}, {772, 0, 99}})
    {
        f.dialogue(npc, house, actor);
        f.topic(calls);
        CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    }
    f.dialogue(772, 0, 0);
    f.topic(calls, 1791); // _BeginEvent cannot turn another event into the actual Join dispatch.
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.dialogue(772, 0, 0);
    REQUIRE(f.events.executeEventById(std::nullopt, calls, 1789, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest); // Generic event is not an NPC UI action.
    const auto local = f.program("RegisterEvent(1789, nil, function() "
        "assert(not evt.RequestBreachAlvarCompanionReturn(772)) end)", ScriptedEventScope::Map);
    f.dialogue(772, 0, 0);
    REQUIRE(f.events.executeNpcTopicEventById(local, std::nullopt, 1789, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.dialogue(772, 0, 0);
    EventRuntimeState::ActiveHookContext hook;
    hook.kind = EventRuntimeHookKind::NpcEnter;
    hook.npcId = 772;
    f.state().activeHookContext = hook;
    REQUIRE(f.events.executeHooks(std::nullopt, global, hook.kind, f.state(), &f.party(), &f.world));
    f.state().activeHookContext.reset();
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    auto stale = f.baseline;
    stale.outdoorWorld.mapActors[0].actorId = 8;
    stale.outdoorWorld.nextActorId = 9;
    f.restore(stale); // Receipt still binds stable actor zero, regardless of vector index zero.
    f.dialogue(772, 0, 0);
    f.topic(calls);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.restore(f.baseline);
    const auto house = f.program("RegisterGlobalEvent(1789, nil, function() "
        "assert(not evt.RequestBreachAlvarCompanionReturn(773)) end)", ScriptedEventScope::Global);
    f.dialogue(773, 706);
    f.topic(house);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
}

TEST_CASE("Breach149 availability is readonly outside current NPC and readonly requests cannot queue")
{
    BreachBridgeFixture f;
    const auto global = f.program(R"lua(
        RegisterCanShowTopic(1789, function()
            assert(evt.IsBreachAlvarCompanionAvailable(772))
            assert(evt.IsBreachAlvarCompanionAvailable(774))
            assert(not evt.IsBreachAlvarCompanionAvailable(771))
            assert(not evt.IsBreachAlvarCompanionAvailable(4294968068)) -- Must not wrap to NPC772.
            assert(not evt.RequestBreachAlvarSplit())
            assert(not evt.RequestBreachAlvarCompanionReturn(773))
            return true
        end)
    )lua", ScriptedEventScope::Global);
    const auto before = encodePartySnapshotForComparison(f.party().snapshot());
    const auto receipt = f.session.breachAlvarCompanions();
    const auto globals = f.state().namedGlobalVars;
    const auto topics = f.state().npcTopicOverrides;
    CHECK(f.events.canShowTopic(global, 1789, f.state(), &f.party(), &f.world));
    f.dialogue(773, 705);
    CHECK(f.events.canShowTopic(global, 1789, f.state(), &f.party(), &f.world));
    // The actual dialogue builder projects Party NPC overrides into a readonly state copy.
    EventRuntimeState displayState = f.state();
    f.party().applyGlobalNpcStateTo(displayState);
    CHECK(f.events.canShowTopic(global, 1789, displayState, &f.party(), &f.world));
    CHECK_FALSE(displayState.pendingBreachAlvarCompanionRequest);
    CHECK(f.state().pendingDialogueContext->sourceId == 773);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    CHECK(f.state().namedGlobalVars == globals);
    CHECK(f.state().npcTopicOverrides == topics);
    CHECK(f.session.breachAlvarCompanions() == receipt);
    CHECK(encodePartySnapshotForComparison(f.party().snapshot()) == before);
}

TEST_CASE("Breach149 readonly display copies query live custody and require the real Party and world")
{
    BreachBridgeFixture f(true, 773);
    const auto global = f.program(R"lua(
        RegisterCanShowTopic(1789, function()
            return evt.IsBreachAlvarCompanionAvailable(772)
                and not evt.IsBreachAlvarCompanionAvailable(773)
        end)
    )lua", ScriptedEventScope::Global);
    EventRuntimeState displayState = f.state();
    f.party().applyGlobalNpcStateTo(displayState);
    // Deliberately stale display facts do not replace the Session's receipt or live quest state.
    displayState.namedGlobalVars["MMerge.CrossContinents.QuestFinished"] = 1;
    displayState.namedGlobalVars["MMerge.CrossContinents.GotFQHints"] = 99;
    const auto before = encodePartySnapshotForComparison(f.party().snapshot());
    const auto receipt = f.session.breachAlvarCompanions();
    CHECK(f.events.canShowTopic(global, 1789, displayState, &f.party(), &f.world));
    Party copiedParty = f.party();
    CHECK_FALSE(f.events.canShowTopic(global, 1789, displayState, &copiedParty, &f.world));
    CHECK_FALSE(f.events.canShowTopic(global, 1789, displayState, &f.party(), nullptr));
    const std::string liveMap = f.state().mapFileName;
    f.state().mapFileName = "out01.odm";
    CHECK_FALSE(f.events.canShowTopic(global, 1789, displayState, &f.party(), &f.world));
    f.state().mapFileName = liveMap;
    displayState.mapFileName = "out01.odm";
    CHECK_FALSE(f.events.canShowTopic(global, 1789, displayState, &f.party(), &f.world));
    displayState.mapFileName = liveMap;
    GameDataLoader emptyLoader;
    GameDataRepository missingProfile;
    missingProfile.bind(emptyLoader);
    f.session.bindDataRepository(&missingProfile);
    CHECK_FALSE(f.events.canShowTopic(global, 1789, displayState, &f.party(), &f.world));
    f.session.bindDataRepository(&f.data);
    CHECK(f.events.canShowTopic(global, 1789, displayState, &f.party(), &f.world));
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    CHECK_FALSE(displayState.pendingBreachAlvarCompanionRequest);
    CHECK(f.session.breachAlvarCompanions() == receipt);
    CHECK(encodePartySnapshotForComparison(f.party().snapshot()) == before);
}

TEST_CASE("Breach149 writable state copies cannot issue source dispatch split or return requests")
{
    {
        BreachBridgeFixture f(false);
        EventRuntimeState copiedState = f.state();
        const auto local = f.program("RegisterMapOnLoadEvent(65469, nil, function() "
            "assert(not evt.RequestBreachAlvarSplit()) end)", ScriptedEventScope::Map);
        const auto before = encodePartySnapshotForComparison(f.party().snapshot());
        REQUIRE(f.events.executeOnLoadEvents(local, std::nullopt, copiedState, &f.party(), &f.world));
        CHECK_FALSE(copiedState.pendingBreachAlvarCompanionRequest);
        CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
        CHECK_FALSE(f.session.breachAlvarCompanions());
        CHECK(encodePartySnapshotForComparison(f.party().snapshot()) == before);
    }
    {
        BreachBridgeFixture f;
        f.dialogue(772, 0, 0);
        EventRuntimeState copiedState = f.state();
        const auto global = f.program("RegisterGlobalEvent(1789, nil, function() "
            "assert(not evt.RequestBreachAlvarCompanionReturn(772)) end)", ScriptedEventScope::Global);
        const auto before = encodePartySnapshotForComparison(f.party().snapshot());
        const auto receipt = f.session.breachAlvarCompanions();
        REQUIRE(f.events.executeNpcTopicEventById(std::nullopt, global, 1789, copiedState, &f.party(), &f.world));
        CHECK_FALSE(copiedState.pendingBreachAlvarCompanionRequest);
        CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
        CHECK(f.session.breachAlvarCompanions() == receipt);
        CHECK(encodePartySnapshotForComparison(f.party().snapshot()) == before);
    }
}

TEST_CASE("Breach149 Runaway refresh and Hint1793 never reopen returned receipt or postquest topics")
{
    BreachBridgeFixture f(true, 773);
    const auto global = f.program(BreachBridgeFixture::overlay("Global_mmmerge_runaway_chaos.lua") + "\n"
        + BreachBridgeFixture::overlay("Global_mmmerge_br_alvar_companions.lua"), ScriptedEventScope::Global);
    REQUIRE(f.events.executeGlobalAfterLoadEvents(std::nullopt, global, f.state(), &f.party(), &f.world));
    CHECK(f.state().npcTopicOverrides.at(773).at(0) == 0);
    CHECK(f.state().npcTopicOverrides.at(774).at(0) == 1793);
    f.dialogue(773, 705);
    CHECK_FALSE(f.events.canShowTopic(global, 1793, f.state(), &f.party(), &f.world));
    CHECK_FALSE(f.events.canShowTopic(global, 1789, f.state(), &f.party(), &f.world));
    const auto before = f.state().namedGlobalVars;
    f.topic(global, 1793);
    CHECK(f.state().namedGlobalVars == before);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
    f.dialogue(773, 705);
    EventRuntimeState::ActiveHookContext hook;
    hook.kind = EventRuntimeHookKind::NpcEnter;
    hook.npcId = 773;
    f.state().activeHookContext = hook;
    REQUIRE(f.events.executeHooks(std::nullopt, global, hook.kind, f.state(), &f.party(), &f.world));
    f.state().activeHookContext.reset();
    CHECK(f.state().npcTopicOverrides.at(773).at(0) == 0);
    // An unreturned friend still grants its source encounter-order hint.
    f.dialogue(774, 706);
    f.topic(global, 1793);
    CHECK(f.state().namedGlobalVars.at("MMerge.CrossContinents.GotFQHints") == 1);
    CHECK(f.state().namedGlobalVars.at("MMerge.CrossContinents.HintByNPC.774") == 1);
    CHECK(f.state().npcTopicOverrides.at(774).at(0) == 1789);
    const auto once = f.state().namedGlobalVars;
    f.dialogue(774, 706);
    f.topic(global, 1793);
    CHECK(f.state().namedGlobalVars == once);
    // Source completion can leave companions detached. This fixture does not earn completion.
    f.state().namedGlobalVars["MMerge.CrossContinents.QuestFinished"] = 1;
    f.party().setQuestBit(1714, false);
    REQUIRE(f.events.executeGlobalAfterLoadEvents(std::nullopt, global, f.state(), &f.party(), &f.world));
    for (uint32_t npc = 772; npc <= 775; ++npc) CHECK(f.state().npcTopicOverrides.at(npc).at(0) == 0);
    CHECK(f.party().snapshot().detachedMembers.size() == 3);
    CHECK(f.session.breachAlvarCompanions()->receipt.companions[1].returned);
}

TEST_CASE("Breach149 Runaway unsplit hints remain compatible and missing typed scene cannot grant new hints")
{
    BreachBridgeFixture f(false);
    const auto global = f.program(BreachBridgeFixture::overlay("Global_mmmerge_runaway_chaos.lua"),
        ScriptedEventScope::Global);
    f.dialogue(772, 0);
    f.topic(global, 1793);
    CHECK(f.state().namedGlobalVars.at("MMerge.CrossContinents.GotFQHints") == 1);
    CHECK(f.state().npcTopicOverrides.at(772).at(0) == 1789);
    // Compatibility flag without its typed receipt is a failed state, never a fallback source roster.
    f.state().namedGlobalVars["MMerge.CrossContinents.BreachSplit"] = 1;
    const auto before = f.state().namedGlobalVars;
    f.dialogue(773, 705);
    f.topic(global, 1793);
    CHECK(f.state().namedGlobalVars == before);
    REQUIRE(f.events.executeGlobalAfterLoadEvents(std::nullopt, global, f.state(), &f.party(), &f.world));
    CHECK(f.state().npcTopicOverrides.at(773).at(0) == 0);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
}

TEST_CASE("Breach149 actual NPC topic controller preserves actor identity through source Hint then Join")
{
    // Each iteration enters through the real shared dialogue controller, then selects its generated
    // actions. The headless world forwards events to production Lua instead of requiring a GPU view.
    for (uint32_t npc : {772u, 773u})
    {
        CAPTURE(npc);
        BreachBridgeFixture f;
        const auto global = f.program(BreachBridgeFixture::overlay("Global_mmmerge_runaway_chaos.lua") + "\n"
            + BreachBridgeFixture::overlay("Global_mmmerge_br_alvar_companions.lua"), ScriptedEventScope::Global);
        f.world.bindGlobalEventProgram(&global);
        f.world.dispatchNpcTopic = [&](uint16_t id, size_t &previous, std::optional<uint8_t> step)
        {
            previous = f.state().messages.size();
            return f.events.executeNpcTopicEventById(std::nullopt, global, id, f.state(), &f.party(), &f.world, step);
        };
        REQUIRE(f.events.executeGlobalAfterLoadEvents(std::nullopt, global, f.state(), &f.party(), &f.world));
        auto &screen = f.session.gameplayScreenRuntime();
        GameplayDialogController::Context context{f.session.gameplayUiController(), f.state(),
            screen.activeEventDialog(), screen.eventDialogSelectionIndex()};
        context.pParty = &f.party();
        context.pWorldRuntime = &f.world;
        context.pGlobalEventProgram = &global;
        context.pHouseTable = &f.data.houseTable();
        context.pNpcDialogTable = &f.data.npcDialogTable();
        context.pCurrentMap = &f.map;
        context.pMapEntries = &f.data.mapEntries();
        const uint32_t house = npc == 772 ? 0 : 705;
        const std::optional<uint32_t> actor = npc == 772 ? std::optional<uint32_t>(0) : std::nullopt;
        GameplayDialogController controller;
        const auto opened = controller.openNpcDialogue(context, npc, house, actor);
        REQUIRE(opened.shouldOpenPendingEventDialog);
        REQUIRE(controller.presentPendingEventDialog(context, opened.previousMessageCount, true, false).dialogOpened);
        REQUIRE(screen.activeEventDialog().sourceActorIndex == actor);
        for (uint32_t topic : {1793u, 1789u})
        {
            CAPTURE(topic);
            const auto &actions = screen.activeEventDialog().actions;
            const auto action = std::find_if(actions.begin(), actions.end(), [topic](const auto &entry)
            {
                return entry.kind == EventDialogActionKind::NpcTopic && entry.id == topic
                    && entry.enabled && !entry.textOnly;
            });
            REQUIRE(action != actions.end());
            screen.eventDialogSelectionIndex() = size_t(action - actions.begin());
            const auto result = controller.executeActiveDialogAction(context);
            REQUIRE(result.shouldOpenPendingEventDialog);
            CHECK_FALSE(result.shouldCloseActiveDialog);
            REQUIRE(f.state().pendingDialogueContext);
            CHECK(f.state().pendingDialogueContext->sourceId == npc);
            CHECK(f.state().pendingDialogueContext->sourceActorIndex == actor);
            CHECK(f.state().pendingDialogueContext->hostHouseId == house);
            REQUIRE(controller.presentPendingEventDialog(context, result.previousMessageCount, true, false).dialogOpened);
            CHECK(screen.activeEventDialog().sourceActorIndex == actor);
            CHECK(screen.activeEventDialog().sourceId == npc);
        }
        REQUIRE(f.state().pendingBreachAlvarCompanionRequest);
        const auto &request = *f.state().pendingBreachAlvarCompanionRequest;
        CHECK(request.sourceActorIndex == actor);
        CHECK(request.stableActorId == actor); // This controlled street actor has both index/id zero.
        CHECK(request.npcId == npc);
        CHECK(request.hostHouseId == house);
        CHECK_FALSE(f.session.breachAlvarCompanions()->receipt.companions[npc - 772].returned);
    }
}

TEST_CASE("shared NPC topic controller honors explicit Lua NPC replacement without copying old actor")
{
    BreachBridgeFixture f;
    const auto global = f.program(R"lua(
        RegisterGlobalEvent(60107, nil, function() evt.SpeakNPC(773) end)
    )lua", ScriptedEventScope::Global);
    f.world.bindGlobalEventProgram(&global);
    f.world.dispatchNpcTopic = [&](uint16_t id, size_t &previous, std::optional<uint8_t> step)
    {
        previous = f.state().messages.size();
        return f.events.executeNpcTopicEventById(std::nullopt, global, id, f.state(), &f.party(), &f.world, step);
    };
    auto &screen = f.session.gameplayScreenRuntime();
    GameplayDialogController::Context context{f.session.gameplayUiController(), f.state(),
        screen.activeEventDialog(), screen.eventDialogSelectionIndex()};
    context.pParty = &f.party();
    context.pWorldRuntime = &f.world;
    context.pGlobalEventProgram = &global;
    context.pNpcDialogTable = &f.data.npcDialogTable();
    GameplayDialogController controller;
    const auto opened = controller.openNpcDialogue(context, 772, 0, 0);
    REQUIRE(opened.shouldOpenPendingEventDialog);
    REQUIRE(controller.presentPendingEventDialog(context, opened.previousMessageCount, true, false).dialogOpened);
    EventDialogAction action;
    action.kind = EventDialogActionKind::NpcTopic;
    action.id = 60107;
    action.enabled = true;
    screen.activeEventDialog().actions = {action}; // Declared replacement topic, executed by real Lua.
    screen.eventDialogSelectionIndex() = 0;
    const auto result = controller.executeActiveDialogAction(context);
    REQUIRE(result.shouldOpenPendingEventDialog);
    REQUIRE(f.state().pendingDialogueContext);
    CHECK(f.state().pendingDialogueContext->sourceId == 773);
    CHECK_FALSE(f.state().pendingDialogueContext->sourceActorIndex);
    REQUIRE(controller.presentPendingEventDialog(context, result.previousMessageCount, true, false).dialogOpened);
    CHECK(screen.activeEventDialog().sourceId == 773);
    CHECK_FALSE(screen.activeEventDialog().sourceActorIndex);
    CHECK_FALSE(f.state().pendingBreachAlvarCompanionRequest);
}
