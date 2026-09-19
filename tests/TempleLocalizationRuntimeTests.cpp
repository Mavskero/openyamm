#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "engine/AssetScaleTier.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/gameplay/HouseServiceRuntime.h"
#include "game/items/ItemRuntime.h"
#include "game/items/PriceCalculator.h"
#include "game/party/Party.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
using namespace OpenYAMM::Game;

std::filesystem::path requiredRuntimeRoot()
{
    const char *pValue = std::getenv("OPENYAMM_TEMPLE_RUNTIME_ROOT");
    REQUIRE_MESSAGE(pValue != nullptr, "Missing OPENYAMM_TEMPLE_RUNTIME_ROOT");
    REQUIRE_MESSAGE(*pValue != '\0', "Empty OPENYAMM_TEMPLE_RUNTIME_ROOT");
    return pValue;
}

Party makeTempleParty(const GameDataLoader &loader, int gold, int health)
{
    Character member;
    member.name = "火焰法师";
    member.className = "Cleric";
    member.role = "Cleric";
    member.maxHealth = 50;
    member.health = health;
    member.maxSpellPoints = 30;
    member.spellPoints = 10;

    PartySeed seed;
    seed.gold = gold;
    seed.members.push_back(member);

    Party party;
    party.setItemTable(&loader.getItemTable());
    party.setItemEnchantTables(
        &loader.getStandardItemEnchantTable(),
        &loader.getSpecialItemEnchantTable());
    party.setClassSkillTable(&loader.getClassSkillTable());
    party.seed(seed);
    return party;
}

const HouseActionOption &actionById(
    const std::vector<HouseActionOption> &actions,
    HouseActionId id)
{
    const auto found = std::find_if(
        actions.begin(),
        actions.end(),
        [id](const HouseActionOption &action) { return action.id == id; });
    REQUIRE(found != actions.end());
    return *found;
}
}

TEST_CASE("localized runtime keeps tavern training and transport service state changes")
{
    const std::filesystem::path runtimeRoot = requiredRuntimeRoot();
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        runtimeRoot,
        runtimeRoot,
        OpenYAMM::Engine::AssetScaleTier::X1,
        "mm6"));

    GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadCommonForGameplay(assetFileSystem));

    const HouseServiceTextTable &texts = loader.getHouseServiceTextTable();
    const HouseEntry *pTavern = loader.getHouseTable().get(228);
    const HouseEntry *pTraining = loader.getHouseTable().get(1564);
    const HouseEntry *pTransport = loader.getHouseTable().get(479);
    REQUIRE(pTavern != nullptr);
    REQUIRE(pTraining != nullptr);
    REQUIRE(pTransport != nullptr);

    SUBCASE("tavern provisions reject and succeed without changing original prices")
    {
        Party party = makeTempleParty(loader, 0, 50);
        const int initialFood = party.food();
        const int price = PriceCalculator::tavernFoodPrice(party.activeMember(), *pTavern, 0);
        const HouseActionOption action{HouseActionId::TavernBuyFood, {}, {}, true, {}};

        const HouseActionResult rejected = performHouseAction(
            action,
            *pTavern,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(rejected.succeeded);
        CHECK_EQ(party.gold(), 0);
        CHECK_EQ(party.food(), initialFood);
        CHECK(rejected.messages == std::vector<std::string>{
            "购买食物需要" + std::to_string(price) + "个金币。"});

        party.addGold(10000);
        const int fundedGold = party.gold();
        const HouseActionResult accepted = performHouseAction(
            action,
            *pTavern,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        REQUIRE(accepted.succeeded);
        CHECK_EQ(party.gold(), fundedGold - price);
        CHECK_GT(party.food(), initialFood);
        CHECK(accepted.messages == std::vector<std::string>{
            "旅店老板把你的食物补充到" + std::to_string(party.food()) + "天。"});
    }

    SUBCASE("training rejects insufficient gold and succeeds atomically")
    {
        Party party = makeTempleParty(loader, 0, 50);
        Character *pMember = party.activeMember();
        REQUIRE(pMember != nullptr);
        pMember->level = 4;
        pMember->experience = 50000;
        const uint32_t initialLevel = pMember->level;
        const uint32_t initialSkillPoints = pMember->skillPoints;
        const int price = PriceCalculator::trainingPrice(pMember, *pTraining);
        const HouseActionOption action{HouseActionId::TrainingTrainActiveMember, {}, {}, true, {}};

        EventRuntimeState state;
        OpenYAMM::Tests::PartySpellTestWorldRuntime world;
        world.bindParty(&party);
        world.bindEventRuntimeState(&state);
        world.advanceGameMinutes(12.0f * 60.0f);

        const HouseActionResult rejected = performHouseAction(
            action,
            *pTraining,
            party,
            &loader.getClassSkillTable(),
            &world,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(rejected.succeeded);
        CHECK_EQ(pMember->level, initialLevel);
        CHECK_EQ(pMember->skillPoints, initialSkillPoints);
        CHECK(rejected.messages == std::vector<std::string>{
            "训练需要" + std::to_string(price) + "个金币。"});

        party.addGold(10000);
        const int fundedGold = party.gold();
        const HouseActionResult accepted = performHouseAction(
            action,
            *pTraining,
            party,
            &loader.getClassSkillTable(),
            &world,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        REQUIRE(accepted.succeeded);
        CHECK_EQ(pMember->level, initialLevel + 1);
        CHECK_GT(pMember->skillPoints, initialSkillPoints);
        CHECK_EQ(party.gold(), fundedGold - price);
        REQUIRE_EQ(accepted.messages.size(), 1);
        CHECK(accepted.messages.front().starts_with("火焰法师已升至5级"));
    }

    SUBCASE("transport rejects insufficient gold and queues the same route after funding")
    {
        Party party = makeTempleParty(loader, 0, 50);
        EventRuntimeState state;
        OpenYAMM::Tests::PartySpellTestWorldRuntime world;
        world.bindParty(&party);
        world.bindEventRuntimeState(&state);
        world.advanceGameMinutes(12.0f * 60.0f);

        std::optional<HouseActionOption> routeAction;
        for (int day = 0; day < 7 && !routeAction.has_value(); ++day)
        {
            const std::vector<HouseActionOption> actions = buildHouseActionOptions(
                *pTransport,
                &party,
                &loader.getClassSkillTable(),
                &world,
                world.gameMinutes(),
                DialogueMenuId::None,
                &loader.getCharacterInspectTable(),
                &texts);
            const auto found = std::find_if(actions.begin(), actions.end(), [](const HouseActionOption &action)
            {
                return action.id == HouseActionId::TransportRoute && action.enabled;
            });
            if (found != actions.end())
            {
                routeAction = *found;
            }
            else
            {
                world.advanceGameMinutes(24.0f * 60.0f);
            }
        }
        REQUIRE(routeAction.has_value());
        CHECK(routeAction->label.find("旅费") != std::string::npos);

        const float rejectedMinutes = world.gameMinutes();
        const HouseActionResult rejected = performHouseAction(
            *routeAction,
            *pTransport,
            party,
            &loader.getClassSkillTable(),
            &world,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(rejected.succeeded);
        CHECK_EQ(party.gold(), 0);
        CHECK_EQ(world.gameMinutes(), doctest::Approx(rejectedMinutes));
        CHECK_FALSE(state.pendingMapMove.has_value());
        REQUIRE_EQ(rejected.messages.size(), 1);
        CHECK(rejected.messages.front().starts_with("旅行需要"));

        party.addGold(10000);
        const int fundedGold = party.gold();
        const HouseActionResult accepted = performHouseAction(
            *routeAction,
            *pTransport,
            party,
            &loader.getClassSkillTable(),
            &world,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        REQUIRE(accepted.succeeded);
        CHECK_LT(party.gold(), fundedGold);
        CHECK_GT(world.gameMinutes(), rejectedMinutes);
        CHECK(state.pendingMapMove.has_value());
        REQUIRE_EQ(accepted.messages.size(), 1);
        CHECK(accepted.messages.front().find("才能到达") != std::string::npos);
    }
}

TEST_CASE("localized runtime keeps shop buy sell identify and repair state changes")
{
    const std::filesystem::path runtimeRoot = requiredRuntimeRoot();
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        runtimeRoot,
        runtimeRoot,
        OpenYAMM::Engine::AssetScaleTier::X1,
        "mm6"));

    GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadCommonForGameplay(assetFileSystem));

    const HouseEntry *pWeaponShop = loader.getHouseTable().get(1);
    const HouseEntry *pMagicShop = loader.getHouseTable().get(97);
    REQUIRE(pWeaponShop != nullptr);
    REQUIRE(pMagicShop != nullptr);
    const HouseServiceTextTable &texts = loader.getHouseServiceTextTable();
    Party party = makeTempleParty(loader, 0, 50);

    const std::vector<InventoryItem> &stock = HouseServiceRuntime::ensureStock(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pWeaponShop,
        12.0f * 60.0f,
        HouseStockMode::ShopStandard);
    const auto stockIt = std::find_if(stock.begin(), stock.end(), [](const InventoryItem &item)
    {
        return item.objectDescriptionId != 0;
    });
    REQUIRE(stockIt != stock.end());
    const size_t slotIndex = static_cast<size_t>(std::distance(stock.begin(), stockIt));
    const size_t emptyInventoryCount = party.inventoryItemCount();
    std::string statusText;
    HouseServiceRuntime::ShopItemServiceResult result = HouseServiceRuntime::ShopItemServiceResult::None;

    CHECK_FALSE(HouseServiceRuntime::tryBuyStockItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pWeaponShop,
        12.0f * 60.0f,
        HouseStockMode::ShopStandard,
        slotIndex,
        statusText,
        &result,
        0,
        &texts));
    CHECK_EQ(result, HouseServiceRuntime::ShopItemServiceResult::NotEnoughGold);
    CHECK_EQ(statusText, "你的金币不够");
    CHECK_EQ(party.inventoryItemCount(), emptyInventoryCount);

    party.addGold(10000000);
    const int fundedGold = party.gold();
    REQUIRE(HouseServiceRuntime::tryBuyStockItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pWeaponShop,
        12.0f * 60.0f,
        HouseStockMode::ShopStandard,
        slotIndex,
        statusText,
        &result,
        0,
        &texts));
    CHECK_EQ(result, HouseServiceRuntime::ShopItemServiceResult::Success);
    CHECK(statusText.starts_with("购买了"));
    CHECK_LT(party.gold(), fundedGold);
    CHECK_GT(party.inventoryItemCount(), emptyInventoryCount);

    Character *pMember = party.activeMember();
    REQUIRE(pMember != nullptr);
    REQUIRE_FALSE(pMember->inventory.empty());
    const uint8_t boughtX = pMember->inventory.front().gridX;
    const uint8_t boughtY = pMember->inventory.front().gridY;
    const int beforeSellGold = party.gold();
    REQUIRE(HouseServiceRuntime::trySellInventoryItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pWeaponShop,
        party.activeMemberIndex(),
        boughtX,
        boughtY,
        statusText,
        &result,
        0,
        &texts));
    CHECK_EQ(result, HouseServiceRuntime::ShopItemServiceResult::Success);
    CHECK(statusText.starts_with("出售了"));
    CHECK_GT(party.gold(), beforeSellGold);

    const std::vector<ItemDefinition> &definitions = loader.getItemTable().entries();
    const auto definitionIt = std::find_if(definitions.begin(), definitions.end(), [](const ItemDefinition &definition)
    {
        return definition.itemId != 0
            && definition.value > 0
            && definition.equipStat == "Ring"
            && definition.rarity == ItemRarity::Common
            && ItemRuntime::requiresIdentification(definition);
    });
    REQUIRE(definitionIt != definitions.end());

    InventoryItem item;
    item.objectDescriptionId = definitionIt->itemId;
    item.quantity = 1;
    item.width = definitionIt->inventoryWidth;
    item.height = definitionIt->inventoryHeight;
    item.identified = false;
    REQUIRE(pMember->addInventoryItem(item));
    const auto inventoryIt = std::find_if(pMember->inventory.begin(), pMember->inventory.end(), [&item](const InventoryItem &candidate)
    {
        return candidate.objectDescriptionId == item.objectDescriptionId && !candidate.identified;
    });
    REQUIRE(inventoryIt != pMember->inventory.end());
    const uint8_t gridX = inventoryIt->gridX;
    const uint8_t gridY = inventoryIt->gridY;

    party.addGold(-party.gold());
    CHECK_FALSE(HouseServiceRuntime::tryIdentifyInventoryItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pMagicShop,
        party.activeMemberIndex(),
        gridX,
        gridY,
        statusText,
        &result,
        0,
        nullptr,
        &texts));
    CHECK_EQ(result, HouseServiceRuntime::ShopItemServiceResult::NotEnoughGold);
    CHECK_EQ(statusText, "你的金币不够");
    CHECK_FALSE(pMember->inventoryItemAt(gridX, gridY)->identified);

    party.addGold(10000000);
    const bool identified = HouseServiceRuntime::tryIdentifyInventoryItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pMagicShop,
        party.activeMemberIndex(),
        gridX,
        gridY,
        statusText,
        &result,
        0,
        nullptr,
        &texts);
    INFO("identify status: " << statusText << ", result: " << static_cast<int>(result));
    REQUIRE(identified);
    CHECK_EQ(statusText, texts.format(HouseServiceTextTable::Message::ShopIdentifySuccess));
    auto identifiedIt = std::find_if(pMember->inventory.begin(), pMember->inventory.end(), [gridX, gridY](const InventoryItem &candidate)
    {
        return candidate.gridX == gridX && candidate.gridY == gridY;
    });
    REQUIRE(identifiedIt != pMember->inventory.end());
    InventoryItem *pIdentified = &*identifiedIt;
    CHECK(pIdentified->identified);
    pIdentified->broken = true;

    party.addGold(-party.gold());
    CHECK_FALSE(HouseServiceRuntime::tryRepairInventoryItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pMagicShop,
        party.activeMemberIndex(),
        gridX,
        gridY,
        statusText,
        &result,
        0,
        nullptr,
        &texts));
    CHECK_EQ(result, HouseServiceRuntime::ShopItemServiceResult::NotEnoughGold);
    CHECK_EQ(statusText, "你的金币不够");
    CHECK(pIdentified->broken);

    party.addGold(10000000);
    REQUIRE(HouseServiceRuntime::tryRepairInventoryItem(
        party,
        loader.getItemTable(),
        loader.getStandardItemEnchantTable(),
        loader.getSpecialItemEnchantTable(),
        *pMagicShop,
        party.activeMemberIndex(),
        gridX,
        gridY,
        statusText,
        &result,
        0,
        nullptr,
        &texts));
    CHECK_EQ(statusText, texts.format(HouseServiceTextTable::Message::ShopRepairSuccess));
    CHECK_FALSE(pIdentified->broken);
}

TEST_CASE("localized runtime loads Chinese temple text without changing stable actions or deductions")
{
    const std::filesystem::path runtimeRoot = requiredRuntimeRoot();
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        runtimeRoot,
        runtimeRoot,
        OpenYAMM::Engine::AssetScaleTier::X1,
        "mm6"));

    GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadCommonForGameplay(assetFileSystem));

    const HouseEntry *pTemple = loader.getHouseTable().get(303);
    REQUIRE(pTemple != nullptr);
    const HouseServiceTextTable &texts = loader.getHouseServiceTextTable();
    const std::vector<HouseActionOption> rootActions = buildHouseActionOptions(
        *pTemple,
        nullptr,
        &loader.getClassSkillTable(),
        nullptr,
        0.0f,
        DialogueMenuId::None,
        &loader.getCharacterInspectTable(),
        &texts);

    const HouseActionOption &heal = actionById(rootActions, HouseActionId::TempleHeal);
    const HouseActionOption &donate = actionById(rootActions, HouseActionId::TempleDonate);
    const HouseActionOption &learn = actionById(rootActions, HouseActionId::OpenLearnSkillsMenu);
    CHECK(heal.label.starts_with("治疗（"));
    CHECK(donate.label.starts_with("捐赠（"));
    CHECK_EQ(learn.label, "学习技能");

    const SkillInspectEntry *pMerchant = loader.getCharacterInspectTable().getSkill("Merchant");
    REQUIRE(pMerchant != nullptr);
    REQUIRE_FALSE(pMerchant->name.empty());
    CHECK_NE(pMerchant->name, "Merchant");
    CHECK(texts.format(
        HouseServiceTextTable::Message::TempleLearnSkillAction,
        {},
        pMerchant->name,
        250).starts_with("学习"));

    SUBCASE("already healthy does not deduct gold")
    {
        Party party = makeTempleParty(loader, 1000, 50);
        Character *pMember = party.activeMember();
        REQUIRE(pMember != nullptr);
        pMember->spellPoints = pMember->maxSpellPoints;
        const int initialGold = party.gold();
        const HouseActionResult result = performHouseAction(
            HouseActionOption{HouseActionId::TempleHeal, {}, {}, true, {}},
            *pTemple,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(result.succeeded);
        REQUIRE_EQ(result.messages.size(), 1);
        CHECK_EQ(result.messages[0], "神殿人员说火焰法师已经很健康。");
        CHECK_EQ(party.gold(), initialGold);
    }

    SUBCASE("insufficient healing gold does not deduct or heal")
    {
        Party party = makeTempleParty(loader, 0, 9);
        const int initialGold = party.gold();
        const int initialHealth = party.activeMember()->health;
        const int price = PriceCalculator::templeHealPrice(party.activeMember(), *pTemple, 0.0f);
        const HouseActionResult result = performHouseAction(
            HouseActionOption{HouseActionId::TempleHeal, {}, {}, true, {}},
            *pTemple,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(result.succeeded);
        CHECK(result.messages == std::vector<std::string>{"治疗需要" + std::to_string(price) + "金币。"});
        CHECK_EQ(party.gold(), initialGold);
        CHECK_EQ(party.activeMember()->health, initialHealth);
    }

    SUBCASE("unsupported condition does not deduct or clear the condition")
    {
        HouseEntry limitedTemple = *pTemple;
        limitedTemple.templeHealingTier = 2.0f;
        Party party = makeTempleParty(loader, 1000, 50);
        Character *pMember = party.activeMember();
        REQUIRE(pMember != nullptr);
        pMember->spellPoints = pMember->maxSpellPoints;
        const size_t conditionIndex = static_cast<size_t>(CharacterCondition::Eradicated);
        pMember->conditions.set(conditionIndex);
        const int initialGold = party.gold();
        const HouseActionResult result = performHouseAction(
            HouseActionOption{HouseActionId::TempleHeal, {}, {}, true, {}},
            limitedTemple,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(result.succeeded);
        CHECK_EQ(party.gold(), initialGold);
        CHECK(party.activeMember()->conditions.test(conditionIndex));
        CHECK(result.messages == std::vector<std::string>{
            "神殿人员无法治疗火焰法师的当前状态。"});
    }

    SUBCASE("successful healing preserves price and restores original resources")
    {
        Party party = makeTempleParty(loader, 1000, 9);
        const int price = PriceCalculator::templeHealPrice(party.activeMember(), *pTemple, 0.0f);
        const int initialGold = party.gold();
        const HouseActionResult result = performHouseAction(
            HouseActionOption{HouseActionId::TempleHeal, {}, {}, true, {}},
            *pTemple,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        REQUIRE(result.succeeded);
        CHECK_EQ(party.gold(), initialGold - price);
        CHECK_EQ(party.activeMember()->health, 50);
        CHECK_EQ(party.activeMember()->spellPoints, 30);
        CHECK(result.messages == std::vector<std::string>{
            "神殿已为火焰法师恢复健康，花费" + std::to_string(price) + "金币。"});
    }

    SUBCASE("insufficient donation gold preserves gold")
    {
        Party party = makeTempleParty(loader, 0, 50);
        const HouseActionResult result = performHouseAction(
            HouseActionOption{HouseActionId::TempleDonate, {}, {}, true, {}},
            *pTemple,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        CHECK_FALSE(result.succeeded);
        CHECK_EQ(party.gold(), 0);
        REQUIRE_EQ(result.messages.size(), 1);
        CHECK(result.messages[0].starts_with("在这里捐赠需要"));
    }

    SUBCASE("successful donation keeps original price and localized result")
    {
        Party party = makeTempleParty(loader, 1000, 50);
        const int price = std::max(1, static_cast<int>(std::round(pTemple->priceMultiplier)));
        const int initialGold = party.gold();
        const HouseActionResult result = performHouseAction(
            HouseActionOption{HouseActionId::TempleDonate, {}, {}, true, {}},
            *pTemple,
            party,
            &loader.getClassSkillTable(),
            nullptr,
            nullptr,
            &loader.getCharacterInspectTable(),
            &texts);
        REQUIRE(result.succeeded);
        CHECK_EQ(party.gold(), initialGold - price);
        CHECK(result.messages == std::vector<std::string>{"谢谢你！"});
    }

    SUBCASE("donation reputation counter and deterministic buffs retain original rules")
    {
        Party party = makeTempleParty(loader, 5000, 50);
        EventRuntimeState eventRuntimeState;
        OpenYAMM::Tests::PartySpellTestWorldRuntime world;
        world.bindParty(&party);
        world.bindEventRuntimeState(&eventRuntimeState);
        world.advanceGameMinutes(24.0f * 60.0f);
        world.setCurrentLocationReputation(-25);
        const int price = std::max(1, static_cast<int>(std::round(pTemple->priceMultiplier)));
        const int initialGold = party.gold();

        for (int donationIndex = 0; donationIndex < 3; ++donationIndex)
        {
            const HouseActionResult result = performHouseAction(
                HouseActionOption{HouseActionId::TempleDonate, {}, {}, true, {}},
                *pTemple,
                party,
                &loader.getClassSkillTable(),
                &world,
                nullptr,
                &loader.getCharacterInspectTable(),
                &texts);
            REQUIRE(result.succeeded);
            CHECK(result.messages == std::vector<std::string>{"谢谢你！"});
        }

        CHECK_EQ(party.gold(), initialGold - price * 3);
        CHECK_EQ(world.currentLocationReputation(), -25);
        CHECK_EQ(
            eventRuntimeState.dialogueState.templeDonationCounters[party.activeMemberIndex()],
            3);
        const PartyBuffState *pWizardEye = party.partyBuff(PartyBuffId::WizardEye);
        const PartyBuffState *pProtectionFromMagic = party.partyBuff(PartyBuffId::ProtectionFromMagic);
        REQUIRE(pWizardEye != nullptr);
        REQUIRE(pProtectionFromMagic != nullptr);
        CHECK(pWizardEye->active());
        CHECK_EQ(pWizardEye->remainingSeconds, doctest::Approx(43200.0f));
        CHECK(pProtectionFromMagic->active());
        CHECK_EQ(pProtectionFromMagic->remainingSeconds, doctest::Approx(10800.0f));
        CHECK_EQ(pProtectionFromMagic->power, 3);
    }
}
