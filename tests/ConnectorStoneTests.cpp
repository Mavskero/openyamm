#include "tests/MergeCrossContinentsHarness.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/data/GameDataRepository.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayItemService.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ConnectorStoneRuntime.h"
#include "game/maps/SaveGame.h"
#include "engine/AssetFileSystem.h"
#include <filesystem>
#include <fstream>

using namespace OpenYAMM::Game;
using OpenYAMM::Tests::MergeQuestFixture;
namespace
{
InventoryItem item(uint32_t id) { InventoryItem value; value.objectDescriptionId = id; return value; }
void full(Party &party)
{
    for (size_t member = 0; member < party.memberCount(); ++member)
    {
        party.member(member)->inventory.clear();
        for (size_t slot = 0; slot < 500; ++slot)
            if (!party.member(member)->addInventoryItem(item(1))) break;
        REQUIRE_FALSE(party.member(member)->addInventoryItem(item(1)));
    }
}
void prepareGrant(MergeQuestFixture &f)
{
    f.flag("GotMainQuest", 1);
    f.flag("MetVerdant", 1);
    f.flag("StartedAt", 3);
    f.setMapName("out03.odm");
    f.after();
    f.clearUiRequests();
}
struct DeliveryWorld : MergeQuestFixture
{
    bool flying = false;
    bool dropSuccess = false;
    std::vector<InventoryItem> drops;
    bool partyIsFlyingForEventChecks() const override { return flying; }
    std::optional<GameplayHeldItemDropRequest> buildHeldItemDropRequest() const override
    { return GameplayHeldItemDropRequest{}; }
    bool dropHeldItemToWorld(const GameplayHeldItemDropRequest &request) override
    { if (!dropSuccess) return false; drops.push_back(request.item); return true; }
};
struct ActualUi
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository data;
    GameSession session;
    ActualUi(DeliveryWorld &world)
    {
        const std::filesystem::path root = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
        REQUIRE(files.initialize(root, root, OpenYAMM::Engine::AssetScaleTier::X1, "mm8"));
        loader.setActiveWorldId("mm8");
        REQUIRE(loader.loadCommonForGameplay(files));
        data.bind(loader);
        session.bindDataRepository(&data);
        session.gameplayUiRuntime().bindAssetFileSystem(&files);
        session.bindActiveWorldRuntime(&world);
    }
};
GameSaveData pendingSave(DeliveryWorld &f, const InventoryItem &held)
{
    GameSaveData save;
    save.mapFileName = "out03.odm";
    save.party = f.party.snapshot();
    save.namedGlobalVars = f.state.namedGlobalVars;
    save.savedGameMinutes = f.minutes;
    save.heldInventoryItemActive = true;
    save.heldInventoryItem = held;
    save.saveName = "connector139-controlled";
    return save;
}
std::filesystem::path output(const char *name) { return std::filesystem::path(std::getenv("TMPDIR")) / name; }
std::vector<char> bytes(const std::filesystem::path &path)
{ std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
}

TEST_CASE("source stone grant gates, strict five-minute deadline and absolute fractional-entry lattice")
{
    for (bool main : {false, true}) for (uint32_t started : {1, 3})
    {
        MergeQuestFixture f;
        f.flag("GotMainQuest", main); f.flag("MetVerdant", 1); f.flag("StartedAt", started);
        f.setMapName("out03.odm"); f.after(); f.clearUiRequests();
        f.minutes = 106; f.timer(65511);
        CHECK_EQ(f.flag("GotConnectorStone"), main && started != 1);
    }
    DeliveryWorld f;
    prepareGrant(f);
    auto timers = reconcileScriptedEventTimers({}, f.global->timerTriggers(), 100.25, 0, false);
    auto callback = [&](const auto &definition) { f.timer(definition.eventId); return true; };
    // The source advances NextExact from the previous poll, so a jump with omitted polls creates
    // deliberate catch-up callbacks. Model ordinary raw-tick polling for the lattice assertion.
    for (int64_t poll = int64_t(100.25 * 256); poll <= int64_t(105.25 * 256); ++poll)
    {
        f.minutes = double(poll) / 256;
        updateScriptedEventTimers(timers, f.minutes, callback);
    }
    CHECK_EQ(f.flag("GotConnectorStone"), 0);
    f.minutes = 105.999;
    updateScriptedEventTimers(timers, f.minutes, callback);
    CHECK_EQ(f.flag("GotConnectorStone"), 0);
    f.minutes = 106;
    REQUIRE(updateScriptedEventTimers(timers, f.minutes, callback));
    CHECK_EQ(f.flag("GotConnectorStone"), 1);
    CHECK_EQ(f.party.inventoryItemCount(624, 0), 1);
    CHECK_EQ(f.state.namedGlobalVars.at("MMerge.LostItems.625"), 1);
    CHECK_EQ(f.party.snapshot().npcGreetingOverrides.at(803), 331);
    REQUIRE(f.state.pendingDialogueContext.has_value());
    CHECK(f.state.pendingDialogueContext->npcEnterPending);
    f.clearUiRequests(); f.timer(65511); f.after(); f.timer(65511);
    CHECK_EQ(f.party.inventoryItemCount(624), 1);
}

TEST_CASE("stone grant safety, arenas, leave cancellation and capacity failure do not commit flags")
{
    for (int gate = 0; gate < 4; ++gate)
    {
        DeliveryWorld f; prepareGrant(f); f.minutes = 106;
        f.state.normalGameplayScreen = gate != 0; f.yellow = gate == 1; f.red = gate == 2; f.flying = gate == 3;
        f.timer(65511); CHECK_EQ(f.flag("GotConnectorStone"), 0);
        f.state.normalGameplayScreen = true; f.yellow = false; f.red = false; f.flying = false;
        f.timer(65511); CHECK_EQ(f.flag("GotConnectorStone"), 1);
    }
    DeliveryWorld f; prepareGrant(f); full(f.party);
    for (size_t n = 0; n < Party::MaxPendingItemDeliveries; ++n) REQUIRE(f.party.grantOwnedQuestItem(item(543), -1));
    f.minutes = 106; f.timer(65511);
    CHECK_EQ(f.flag("GotConnectorStone"), 0);
    CHECK_FALSE(f.state.namedGlobalVars.contains("MMerge.LostItems.625"));
    CHECK_FALSE(f.party.ownsPendingItem(624));
    f.party.completeFirstPendingItemDelivery(); f.timer(65511);
    CHECK_EQ(f.flag("GotConnectorStone"), 1); CHECK(f.party.ownsPendingItem(624));
    DeliveryWorld cancelled; prepareGrant(cancelled); cancelled.leave(); cancelled.minutes = 106;
    cancelled.timer(65511); CHECK_EQ(cancelled.flag("GotConnectorStone"), 0);
    for (const char *map : {"out01.odm", "d42.blv", "7d05.blv", "zarena.blv"})
    {
        MergeQuestFixture rejected; rejected.flag("GotMainQuest",1); rejected.flag("MetVerdant",1);
        rejected.flag("StartedAt",3); rejected.setMapName(map); rejected.after(); rejected.minutes = 106;
        rejected.timer(65511); CHECK_EQ(rejected.flag("GotConnectorStone"),0);
    }
}

TEST_CASE("ConStone checks owned pending/inventory/mouse items and delivers once only in scoped Hints")
{
    for (uint32_t existing : {0, 624, 625}) for (bool pending : {false, true})
    {
        MergeQuestFixture f; f.flag("GotMainQuest",1); f.flag("GotConnectorStone",1); f.enter();
        CHECK_FALSE(f.visible(1788)); f.topic(732); REQUIRE(f.visible(1788));
        if (existing != 0)
        {
            if (pending) REQUIRE(f.party.grantOwnedQuestItem(item(existing), -1));
            else REQUIRE(f.party.tryGrantInventoryItemToMember(1, item(existing)));
        }
        f.topic(1788); f.topic(1788);
        CHECK_EQ(f.party.inventoryItemCount(624), existing == 625 || pending && existing != 0 ? 0 : 1);
        CHECK_EQ(f.party.pendingItemDeliveries().size(), pending && existing != 0 ? 1 : 0);
        REQUIRE_EQ(f.state.messages.size(),1);
        CHECK_EQ(f.state.messages.front(), *OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2160));
    }
    MergeQuestFixture held; held.flag("GotMainQuest",1); held.flag("GotConnectorStone",1);
    held.party.setHeldItemForQueries(item(625)); held.enter(); held.topic(732); held.topic(1788);
    CHECK_EQ(held.party.inventoryItemCount(624),0);
}

TEST_CASE("upgrade direct88 runs before house/enemy gates and preserves buff recovery age and ledger")
{
    for (bool house : {false,true}) for (bool enemy : {false,true})
    {
        MergeQuestFixture f; f.flag("ImporvedConnector",1);
        auto *member = f.party.member(0); member->health = 1; member->spellPoints = 0;
        member->ageModifier = 12; member->recoverySecondsRemaining = 27;
        member->conditions.set(size_t(CharacterCondition::Asleep));
        member->conditionStartGameMinutes[size_t(CharacterCondition::Asleep)] = 99;
        f.party.applyPartyBuff(PartyBuffId::Haste, 500, 5, 86, 5, SkillMastery::Master, 0);
        auto stone = item(624);
        auto result = useConnectorStone(f.party,f.state,stone,100.25,house,enemy,ConnectorMinimumRechargeTicks);
        CHECK_EQ(result, house ? ConnectorUseResult::HouseScreen : enemy ? ConnectorUseResult::Enemies : ConnectorUseResult::Called);
        CHECK_EQ(member->health,member->maxHealth); CHECK_EQ(member->spellPoints,member->maxSpellPoints);
        CHECK(member->conditions.none()); CHECK_EQ(member->conditionStartGameMinutes[size_t(CharacterCondition::Asleep)],0);
        CHECK_EQ(member->ageModifier,12); CHECK_EQ(member->recoverySecondsRemaining,27);
        CHECK(f.party.hasPartyBuff(PartyBuffId::Haste)); CHECK_EQ(f.flag("HiddenCaster49DivineInterventionCasts"),1);
        member->health = 1; stone = item(624);
        useConnectorStone(f.party,f.state,stone,99999,true,true,ConnectorMinimumRechargeTicks);
        CHECK_EQ(member->health,1); // elapsed days do not reset hidden49.
    }
}

TEST_CASE("raw-tick recharge inclusive endpoints and source leave strict boundary preserve payload and held stone")
{
    for (int32_t delay : {ConnectorMinimumRechargeTicks, ConnectorMaximumRechargeTicks})
    {
        MergeQuestFixture f; auto stone = item(624); stone.stolen = true; stone.standardEnchantPower = 9;
        REQUIRE_EQ(useConnectorStone(f.party,f.state,stone,100.25,false,false,delay),ConnectorUseResult::Called);
        CHECK_EQ(f.ticks("NextConRecharge"),int64_t(100.25*256)+delay);
        REQUIRE(f.party.tryGrantInventoryItemToMember(0,stone));
        f.party.setHeldItemForQueries(stone);
        f.minutes = double(f.ticks("NextConRecharge"))/256; f.leave(); CHECK_EQ(f.party.inventoryItemCount(625),1);
        f.minutes += 1.0/256; f.leave(); CHECK_EQ(f.party.inventoryItemCount(624),1);
        CHECK(f.party.member(0)->inventory.back().stolen); CHECK_EQ(f.party.member(0)->inventory.back().standardEnchantPower,9);
        CHECK_EQ(f.party.heldItemIdForQueries(),625);
        auto discharged = item(625);
        CHECK_EQ(useConnectorStone(f.party,f.state,discharged,999999,false,false,delay),ConnectorUseResult::Discharged);
        CHECK_EQ(discharged.objectDescriptionId,625);
    }
    MergeQuestFixture migration;
    migration.flag("ImprovedConnector",1); migration.flag("NextConnectorRecharge",12345);
    migration.flag("DivineInterventionLastUsed",999);
    std::string error; REQUIRE(prepareConnectorState(migration.state,error));
    CHECK_EQ(migration.flag("ImporvedConnector"),1); CHECK_EQ(migration.ticks("NextConRecharge"),12345*256LL);
    CHECK_FALSE(migration.state.namedGlobalVars.contains(HiddenCaster49CounterName));
    migration.flag("HiddenCaster49DivineInterventionCasts",2); CHECK_FALSE(prepareConnectorState(migration.state,error));
}

TEST_CASE("real item wrapper distinguishes inventory7 house13 and resident house context and clears inventory on success")
{
    DeliveryWorld f; ActualUi ui(f); auto &runtime = ui.session.gameplayScreenRuntime();
    auto &held = ui.session.gameplayUiController().heldInventoryItem();
    for (int screen : {0,1,2,3})
    {
        GameplayHeldItemController::setHeldInventoryItem(held,item(624));
        f.party.setHeldItemForQueries(held.item);
        auto &dialog = ui.session.gameplayUiController().eventDialog().content;
        dialog = {}; dialog.isActive = screen != 0; dialog.isHouseDialog = screen == 1;
        f.talkContext(803, screen >= 2 ? 185 : 0);
        runtime.characterScreen().open = screen == 0 || screen == 3;
        runtime.characterScreen().dollJewelryOverlayOpen = true;
        REQUIRE(ui.session.gameplayItemService().tryUseHeldItemOnPartyMember(runtime,0,true));
        const bool allowed = screen == 0 || screen == 3;
        CHECK_EQ(held.item.objectDescriptionId,allowed ? 625 : 624);
        if (allowed)
        {
            CHECK_FALSE(runtime.characterScreen().open); CHECK_FALSE(runtime.characterScreen().dollJewelryOverlayOpen);
            REQUIRE(f.state.pendingDialogueContext.has_value()); CHECK_EQ(f.state.pendingDialogueContext->sourceId,803);
            CHECK(f.state.pendingDialogueContext->npcEnterPending);
        }
    }
}

TEST_CASE("old prototype625 migrates at map entry and leave before strict future deadline checks")
{
    MergeQuestFixture f; f.flag("MetVerdant",1); f.flag("NextConnectorRecharge",200);
    REQUIRE(f.party.tryGrantInventoryItemToMember(0,item(625)));
    f.after(); CHECK_EQ(f.ticks("NextConRecharge"),200*256);
    f.minutes = 199; f.leave(); CHECK_EQ(f.party.inventoryItemCount(625),1);
    f.minutes = 200; f.leave(); CHECK_EQ(f.party.inventoryItemCount(625),1);
    f.minutes += 1.0/256; f.leave(); CHECK_EQ(f.party.inventoryItemCount(624),1);
    MergeQuestFixture directLeave; directLeave.flag("NextConnectorRecharge",200);
    REQUIRE(directLeave.party.tryGrantInventoryItemToMember(0,item(625)));
    directLeave.minutes = 150; directLeave.leave(); CHECK_EQ(directLeave.party.inventoryItemCount(625),1);
}

TEST_CASE("letter fullpack ownership reserves before QBits and capacity refusal preserves one-time state")
{
    MergeQuestFixture f(3); f.flag("MetVerdant",1); full(f.party);
    for (size_t n = 0; n < Party::MaxPendingItemDeliveries; ++n) REQUIRE(f.party.grantOwnedQuestItem(item(543),-1));
    f.after(); CHECK_FALSE(f.party.hasQuestBit(1104)); CHECK_FALSE(f.party.hasQuestBit(1105));
    CHECK_FALSE(f.party.ownsPendingItem(2125));
    f.party.completeFirstPendingItemDelivery(); f.after();
    CHECK(f.party.hasQuestBit(1104)); CHECK(f.party.hasQuestBit(1105)); CHECK(f.party.ownsPendingItem(2125));
    f.after(); CHECK_EQ(f.party.pendingItemDeliveries().size(),128);
    CHECK_EQ(f.party.inventoryItemCount(2125),0); // pending is deliberately outside legacy inventory queries.
}

TEST_CASE("source switch4 remains eligible for grant LostItems and immediate leave recharge")
{
    DeliveryWorld f;
    f.flag("GotMainQuest",1); f.flag("MetVerdant",1); f.flag("StartedAt",1);
    f.setMapName("out03.odm"); f.state.dimensionDoorActive = true; f.after(); f.clearUiRequests();
    f.minutes = 106; f.timer(65511);
    CHECK_EQ(f.flag("GotConnectorStone"),1); CHECK_EQ(f.state.namedGlobalVars.at("MMerge.LostItems.625"),4);
    REQUIRE(f.party.tryGrantInventoryItemToMember(0,item(625))); f.ticks("NextConRecharge",999999999);
    f.leave(); CHECK_EQ(f.party.inventoryItemCount(625),0); CHECK_EQ(f.party.inventoryItemCount(624),2);
}

TEST_CASE("190 real wrapper clears hidden49 on overlay open and cancellation; natural DD and active88 preserve ledger")
{
    DeliveryWorld f; ActualUi ui(f); auto &runtime = ui.session.gameplayScreenRuntime();
    f.flag("MetVerdant",1); f.flag("HiddenCaster49DivineInterventionCasts",1);
    runtime.activateDimensionDoor(DimensionDoorActivationSource::NaturalLocation);
    CHECK_EQ(f.flag("HiddenCaster49DivineInterventionCasts"),1);
    REQUIRE(runtime.openDimensionDoorOverlay(0,31)); ui.session.gameplayUiController().closeUtilitySpellOverlay();
    CHECK_EQ(f.flag("HiddenCaster49DivineInterventionCasts"),1);
    GameplayHeldItemController::setHeldInventoryItem(runtime.heldInventoryItem(),item(190));
    f.party.setHeldItemForQueries(runtime.heldInventoryItem().item);
    REQUIRE(ui.session.gameplayItemService().tryUseHeldItemOnPartyMember(runtime,0,true));
    CHECK(runtime.utilitySpellOverlay().active); CHECK_EQ(f.flag("HiddenCaster49DivineInterventionCasts"),0);
    ui.session.gameplayUiController().closeUtilitySpellOverlay(); CHECK_EQ(f.flag("HiddenCaster49DivineInterventionCasts"),0);
    f.flag("HiddenCaster49DivineInterventionCasts",1); f.party.member(0)->health = 1;
    PartySpellCastRequest request; request.spellId = 88; request.skillLevelOverride = 10;
    request.skillMasteryOverride = SkillMastery::Grandmaster; request.spendMana = false; request.applyRecovery = false;
    request.bypassRequiredMastery = true; request.bypassGameplayCasterValidation = true;
    const auto result = PartySpellSystem::castSpell(f.party,f,OpenYAMM::Tests::regressionGameData().spellTable,request);
    REQUIRE(result.succeeded()); CHECK_EQ(f.party.member(0)->health,f.party.member(0)->maxHealth);
    CHECK_EQ(f.flag("HiddenCaster49DivineInterventionCasts"),1);
}

TEST_CASE("durable quest delivery retains fullpack old held on failed drop across events and delivers multiple owners once")
{
    DeliveryWorld f; ActualUi ui(f); auto &runtime = ui.session.gameplayScreenRuntime(); full(f.party);
    auto old = item(2125); old.quantity = 2; old.stolen = true; old.standardEnchantPower = 31;
    GameplayHeldItemController::setHeldInventoryItem(runtime.heldInventoryItem(),old); f.party.setHeldItemForQueries(old);
    REQUIRE(f.party.grantOwnedQuestItem(item(624),0)); REQUIRE(f.party.grantOwnedQuestItem(item(543),-1));
    GameplayHeldItemController::applyPendingQuestItemDeliveries(runtime);
    CHECK_EQ(runtime.heldInventoryItem().item.objectDescriptionId,2125); CHECK_EQ(f.party.pendingItemDeliveries().size(),2);
    f.after(); f.enter(); f.topic(732); f.leave(); clearTransientEventRuntimeState(f.state);
    CHECK_EQ(f.party.pendingItemDeliveries().size(),2);
    auto save = pendingSave(f,old); std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(output("pending139.oysav"),save,error),error);
    const auto loaded = loadGameDataFromPath(output("pending139.oysav"),error); REQUIRE_MESSAGE(loaded.has_value(),error);
    CHECK_EQ(loaded->party.pendingItemDeliveries.size(),2); CHECK_EQ(loaded->heldInventoryItem.quantity,2);
    f.party.restoreSnapshot(loaded->party); f.state.namedGlobalVars = loaded->namedGlobalVars;
    f.dropSuccess = true; GameplayHeldItemController::applyPendingQuestItemDeliveries(runtime);
    REQUIRE_EQ(f.drops.size(),2); CHECK_EQ(f.drops[0].objectDescriptionId,2125); CHECK_EQ(f.drops[0].quantity,2);
    CHECK(f.drops[0].stolen); CHECK_EQ(f.drops[0].standardEnchantPower,31); CHECK_EQ(f.drops[1].objectDescriptionId,624);
    CHECK_EQ(runtime.heldInventoryItem().item.objectDescriptionId,543); CHECK(f.party.pendingItemDeliveries().empty());
    GameplayHeldItemController::applyPendingQuestItemDeliveries(runtime); CHECK_EQ(f.drops.size(),2);
}

TEST_CASE("save88 queue rejects bounds recipient empty item and quantity; old87 defaults empty")
{
    DeliveryWorld f; full(f.party); REQUIRE(f.party.grantOwnedQuestItem(item(624),0));
    auto save = pendingSave(f,item(2125)); std::string error;
    REQUIRE(saveGameDataToPath(output("valid139.oysav"),save,error));
    auto empty = save; empty.party.pendingItemDeliveries.clear();
    REQUIRE(saveGameDataToPath(output("empty139.oysav"),empty,error));
    auto data = bytes(output("valid139.oysav")); const auto emptyBytes = bytes(output("empty139.oysav"));
    size_t offset = 0; while (offset < emptyBytes.size() && data[offset] == emptyBytes[offset]) ++offset;
    REQUIRE(offset + 12 < data.size());
    for (int invalid : {0,1,2,3})
    {
        auto corrupted = data;
        const size_t at = offset + (invalid == 0 ? 0 : invalid == 1 ? 4 : invalid == 2 ? 8 : 36);
        const uint32_t value = invalid == 0 ? 129 : invalid == 3 ? 999 : 0;
        for (int byte = 0; byte < 4; ++byte) corrupted[at+byte] = char(value >> (byte*8));
        const auto path = output(("invalid139-"+std::to_string(invalid)+".oysav").c_str());
        std::ofstream out(path,std::ios::binary); out.write(corrupted.data(),corrupted.size()); out.close();
        CHECK_FALSE(loadGameDataFromPath(path,error).has_value());
    }
    for (int recipient : {-2,999})
    {
        auto invalid = save; invalid.party.pendingItemDeliveries[0].recipientMemberIndex = recipient;
        CHECK_FALSE(saveGameDataToPath(output("invalid-write139.oysav"),invalid,error));
    }
    auto invalid = save; invalid.party.pendingItemDeliveries.resize(129,invalid.party.pendingItemDeliveries.front());
    CHECK_FALSE(saveGameDataToPath(output("invalid-write139.oysav"),invalid,error));
    const char *old = std::getenv("OPENYAMM_OLD87_SAVE"); REQUIRE(old != nullptr);
    auto legacy = loadGameDataFromPath(old,error); REQUIRE_MESSAGE(legacy.has_value(),error);
    CHECK(legacy->party.pendingItemDeliveries.empty());
}

TEST_CASE("read-only old128129130 preserve semantic state through save88 with empty pending owner")
{
    const auto root = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path();
    const std::vector<std::filesystem::path> originals{
        root / "local-data/validation/mm7-castle-clear-128/astra-actual-008/mm7-castle-clear-complete.oysav",
        root / "local-data/validation/mm8-blackmail-129/astra-reload-001/mm8-blackmail-complete.oysav",
        root / "local-data/validation/merge-invitation-130/reload-002/merge-invitation-reload-complete.oysav"};
    for (size_t index = 0; index < originals.size(); ++index)
    {
        std::string error; const auto original = loadGameDataFromPath(originals[index],error);
        REQUIRE_MESSAGE(original.has_value(),error); CHECK(original->party.pendingItemDeliveries.empty());
        const auto path = output(("old-upgraded-"+std::to_string(index)+".oysav").c_str());
        REQUIRE_MESSAGE(saveGameDataToPath(path,*original,error),error);
        const auto upgraded = loadGameDataFromPath(path,error); REQUIRE_MESSAGE(upgraded.has_value(),error);
        CHECK(upgraded->party.pendingItemDeliveries.empty()); CHECK_EQ(upgraded->party.questBits,original->party.questBits);
        CHECK_EQ(upgraded->namedGlobalVars,original->namedGlobalVars); CHECK_EQ(upgraded->savedGameMinutes,original->savedGameMinutes);
        CHECK_EQ(upgraded->party.gold,original->party.gold); CHECK_EQ(upgraded->party.food,original->party.food);
        CHECK_EQ(upgraded->party.eventVariables,original->party.eventVariables);
    }
}

TEST_CASE("late source poll catch-up and prototype deadline persist through new runtime save reload")
{
    DeliveryWorld late;
    prepareGrant(late);
    auto timers = reconcileScriptedEventTimers({}, late.global->timerTriggers(), 100.25, 0, false);
    auto callback = [&](const auto &definition) { late.timer(definition.eventId); return true; };
    late.minutes = 105.25;
    updateScriptedEventTimers(timers, late.minutes, callback);
    CHECK_EQ(late.flag("GotConnectorStone"), 0);
    late.minutes = 105.999;
    updateScriptedEventTimers(timers, late.minutes, callback);
    CHECK_EQ(late.flag("GotConnectorStone"), 1); // Source NextExact uses preceding poll.

    DeliveryWorld original;
    original.flag("MetVerdant", 1);
    original.flag("NextConnectorRecharge", 200);
    REQUIRE(original.party.tryGrantInventoryItemToMember(0, item(625)));
    original.after();
    auto save = pendingSave(original, item(2125));
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(output("prototype-future139.oysav"), save, error), error);
    for (int boundary = 0; boundary < 3; ++boundary)
    {
        const auto loaded = loadGameDataFromPath(output("prototype-future139.oysav"), error);
        REQUIRE_MESSAGE(loaded.has_value(), error);
        DeliveryWorld restored;
        restored.party.restoreSnapshot(loaded->party);
        restored.state.namedGlobalVars = loaded->namedGlobalVars;
        restored.minutes = boundary == 0 ? 199 : boundary == 1 ? 200 : 200 + 1.0 / 256;
        restored.after();
        CHECK_EQ(restored.ticks("NextConRecharge"), 200 * 256);
        restored.leave();
        CHECK_EQ(restored.party.inventoryItemCount(625), boundary < 2 ? 1 : 0);
        CHECK_EQ(restored.party.inventoryItemCount(624), boundary == 2 ? 1 : 0);
    }
}

TEST_CASE("map UI resets preserve full cursor grab state and pending owners; new session and inactive save clear it")
{
    DeliveryWorld f;ActualUi ui(f);auto &screen=ui.session.gameplayScreenRuntime();full(f.party);
    auto old=item(2125);old.quantity=2;old.stolen=true;old.standardEnchantPower=31;
    auto &held=screen.heldInventoryItem();GameplayHeldItemController::setHeldInventoryItem(held,old);
    held.grabCellOffsetX=1;held.grabCellOffsetY=2;held.grabOffsetX=13;held.grabOffsetY=17;
    REQUIRE(f.party.grantOwnedQuestItem(item(624),0));
    screen.characterScreen().open=true;screen.clearUiControllerRuntimeState();
    CHECK_FALSE(screen.characterScreen().open);CHECK(held.active);CHECK_EQ(held.item.objectDescriptionId,2125);
    CHECK_EQ(held.item.quantity,2);CHECK(held.item.stolen);CHECK_EQ(held.item.standardEnchantPower,31);
    CHECK_EQ(held.grabCellOffsetX,1);CHECK_EQ(held.grabCellOffsetY,2);CHECK_EQ(held.grabOffsetX,13);CHECK_EQ(held.grabOffsetY,17);
    CHECK_EQ(f.party.pendingItemDeliveries().size(),1);CHECK_EQ(f.party.heldItemIdForQueries(),2125);
    DeliveryWorld next;next.party.restoreSnapshot(f.party.snapshot());ui.session.bindActiveWorldRuntime(&next);
    screen.clearUiControllerRuntimeState();CHECK_EQ(next.party.heldItemIdForQueries(),2125);
    CHECK_EQ(next.party.pendingItemDeliveries().size(),1);
    GameSaveData inactive;ui.session.restoreHeldInventoryItemFromSaveData(inactive);
    CHECK_FALSE(held.active);CHECK_EQ(next.party.heldItemIdForQueries(),0);
    GameplayHeldItemController::setHeldInventoryItem(held,old);ui.session.clear();CHECK_FALSE(held.active);
}
