#include "doctest/doctest.h"

#include "game/events/EvtEnums.h"
#include "game/gameplay/BountyHuntRuntime.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/gameplay/HouseServiceRuntime.h"
#include "game/gameplay/NpcFollowerRuntime.h"
#include "game/gameplay/ReputationRuntime.h"
#include "game/gameplay/StealingRuntime.h"
#include "game/items/InventoryItemUseRuntime.h"
#include "game/items/ItemRuntime.h"
#include "game/items/PriceCalculator.h"
#include "game/party/Party.h"
#include "game/tables/MergedBaseTables.h"

#include "tests/RegressionGameData.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>

using namespace OpenYAMM::Game;

namespace
{
std::vector<std::string> monsterStatsRow(int id, const std::string &name, int level)
{
    std::vector<std::string> row(38);
    row[0] = std::to_string(id);
    row[1] = name;
    row[2] = name;
    row[3] = std::to_string(level);
    row[4] = "20";
    row[5] = "5";
    row[10] = "Short";
    row[11] = "Normal";
    row[12] = "0";
    row[13] = "100";
    row[14] = "100";
    row[17] = "Phys";
    row[18] = "1d6";
    return row;
}

MonsterTable makeBountyMonsterTable()
{
    MonsterTable table;
    REQUIRE(table.loadStatsFromRows({
        monsterStatsRow(10, "Allowed", 12),
        monsterStatsRow(11, "Excluded", 8),
        monsterStatsRow(12, "Too High", 80),
    }));
    return table;
}

MergedBolsterMonsterTable makeBountyBolsterMonsterTable()
{
    MergedBolsterMonsterTable table;
    REQUIRE(table.loadFromRows({
        {
            "#",
            "Note",
            "Type",
            "ExtraType",
            "Creed",
            "Gender",
            "Style",
            "PrefMagic",
            "NoBounty",
            "Ranged",
            "Spells",
            "HPBySize",
            "Replicate",
            "Summons",
            "SummonId",
            "Extra",
            "MaxHP",
        },
        {"10", "Allowed", "Human", "", "Neutral", "", "", "", "-", "-", "-", "-", "-", "-", "", "", ""},
        {"11", "Excluded", "Human", "", "Neutral", "", "", "", "x", "-", "-", "-", "-", "-", "", "", ""},
        {"12", "Too High", "Human", "", "Neutral", "", "", "", "-", "-", "-", "-", "-", "-", "", "", ""},
    }));
    return table;
}

Party makeOneMemberParty()
{
    Character member = {};
    member.name = "Tester";
    member.might = 10;
    member.intellect = 10;
    member.personality = 10;
    member.endurance = 10;
    member.accuracy = 10;
    member.speed = 10;
    member.luck = 10;
    member.maxHealth = 50;
    member.health = 50;
    member.maxSpellPoints = 30;
    member.spellPoints = 30;

    PartySeed seed = {};
    seed.members = {member};

    Party party = {};
    party.seed(seed);
    return party;
}

uint32_t findFirstItemIdByEquipStat(const ItemTable &itemTable, const std::string &equipStat, bool needsIdentify)
{
    for (const ItemDefinition &entry : itemTable.entries())
    {
        if (entry.itemId != 0
            && entry.equipStat == equipStat
            && (!needsIdentify || ItemRuntime::requiresIdentification(entry)))
        {
            return entry.itemId;
        }
    }

    return 0;
}
}

TEST_CASE("MM7 bounty hunt runtime filters no-bounty monsters and claims monthly reward")
{
    MonsterTable monsterTable = makeBountyMonsterTable();
    MergedBolsterMonsterTable bolsterTable = makeBountyBolsterMonsterTable();

    const std::vector<int16_t> ids = collectBountyHuntMonsterIds(monsterTable, &bolsterTable, 30);
    REQUIRE_EQ(ids.size(), 1u);
    CHECK_EQ(ids.front(), 10);

    BountyHuntEntry entry = {};
    entry.month = 3;
    entry.monsterId = 10;

    CHECK(markBountyHuntMonsterKilled(entry, 10, 3));
    CHECK(entry.done);

    const BountyHuntClaimResult claim = claimBountyHuntReward(entry, monsterTable, 3, true);
    CHECK(claim.claimed);
    CHECK_EQ(claim.goldReward, 1200u);
    CHECK_EQ(claim.bountyTotalDelta, 1200u);
    CHECK_EQ(claim.reputationDelta, -1);
    CHECK(entry.claimed);
}

TEST_CASE("MMerge bounty hunt reward applies gold total bounty award and reputation")
{
    BountyHuntClaimResult claim = {};
    claim.claimed = true;
    claim.goldReward = 1200;
    claim.bountyTotalDelta = 1200;
    claim.reputationDelta = -1;

    Party party = makeOneMemberParty();
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    worldRuntime.setCurrentLocationReputation(0);

    applyBountyHuntClaimResult(worldRuntime, &party, claim);

    CHECK_EQ(party.gold(), 1200);
    CHECK_EQ(party.eventVariableValue(static_cast<uint16_t>(EvtVariable::NumBounties)), 1200);
    CHECK_EQ(party.eventVariableValue(static_cast<uint16_t>(EvtVariable::ArenaWinsPage)), 0);
    CHECK(party.hasAward(44));
    CHECK_EQ(worldRuntime.currentLocationReputation(), -1);
}

TEST_CASE("MMerge bounty hunt interaction creates map entry and hostile target")
{
    MonsterTable monsterTable = makeBountyMonsterTable();
    MergedBolsterMonsterTable bolsterTable = makeBountyBolsterMonsterTable();
    Party party = makeOneMemberParty();
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    EventRuntimeState state = {};
    worldRuntime.bindParty(&party);
    worldRuntime.bindEventRuntimeState(&state);
    worldRuntime.bindMonsterTable(&monsterTable);
    worldRuntime.bindMergedBolsterMonsterTable(&bolsterTable);
    worldRuntime.setMapName("new_sorpigal.odm");
    worldRuntime.setPartyPosition(100.0f, 200.0f, 300.0f);
    worldRuntime.setBountyHuntSpawnPoint(GameplayWorldPoint{444.0f, 555.0f, 666.0f});

    const BountyHuntInteractionResult result = performBountyHuntInteraction(worldRuntime, &party, true);

    REQUIRE(result.succeeded);
    REQUIRE_EQ(result.messages.size(), 1u);
    CHECK(result.messages.front().find("Allowed") != std::string::npos);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.Month"], 0);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.MonsterId"], 10);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.Done"], 0);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.Claimed"], 0);
    REQUIRE_EQ(worldRuntime.hostileSummonRequests().size(), 1u);
    CHECK_EQ(worldRuntime.hostileSummonRequests().front().monsterId, 10);
    CHECK_EQ(worldRuntime.hostileSummonRequests().front().group, 39u);
    CHECK_EQ(worldRuntime.hostileSummonRequests().front().x, 444.0f);
    CHECK_EQ(worldRuntime.hostileSummonRequests().front().y, 555.0f);
    CHECK_EQ(worldRuntime.hostileSummonRequests().front().z, 666.0f);
}

TEST_CASE("MMerge runtime bounty kill marker scans all active monthly entries")
{
    MonsterTable monsterTable = makeBountyMonsterTable();
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    EventRuntimeState state = {};
    worldRuntime.bindEventRuntimeState(&state);
    worldRuntime.setMapName("map_b.odm");
    worldRuntime.setCurrentLocationReputation(0);

    state.namedGlobalVars["MMerge.BountyHunt.map_a.odm.Month"] = 0;
    state.namedGlobalVars["MMerge.BountyHunt.map_a.odm.MonsterId"] = 10;
    state.namedGlobalVars["MMerge.BountyHunt.map_b.odm.Month"] = 0;
    state.namedGlobalVars["MMerge.BountyHunt.map_b.odm.MonsterId"] = 10;
    state.namedGlobalVars["MMerge.BountyHunt.claimed.odm.Month"] = 0;
    state.namedGlobalVars["MMerge.BountyHunt.claimed.odm.MonsterId"] = 10;
    state.namedGlobalVars["MMerge.BountyHunt.claimed.odm.Claimed"] = 1;
    state.namedGlobalVars["MMerge.BountyHunt.expired.odm.Month"] = 1;
    state.namedGlobalVars["MMerge.BountyHunt.expired.odm.MonsterId"] = 10;

    CHECK(markRuntimeBountyHuntMonsterKilled(worldRuntime, 10, &monsterTable));
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.map_a.odm.Done"], 1);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.map_b.odm.Done"], 1);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.claimed.odm.Done"], 0);
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.expired.odm.Done"], 0);
    CHECK_EQ(worldRuntime.currentLocationReputation(), -1);
}

TEST_CASE("MMerge extra potion drink effects apply and one-shot essence potions do not stack")
{
    REQUIRE_MESSAGE(
        OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());
    const OpenYAMM::Tests::RegressionGameData &gameData = OpenYAMM::Tests::regressionGameData();

    Party party = makeOneMemberParty();
    Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    InventoryItem essence = {};
    essence.objectDescriptionId = 272;
    essence.standardEnchantPower = 20;

    InventoryItemUseResult first = InventoryItemUseRuntime::useItemOnMember(
        party,
        0,
        essence,
        gameData.itemTable,
        &gameData.readableScrollTable,
        &gameData.mergedPotionSettingTable);
    CHECK(first.consumed);
    CHECK_EQ(pMember->might, 25u);
    CHECK_EQ(pMember->intellect, 5u);

    InventoryItemUseResult second = InventoryItemUseRuntime::useItemOnMember(
        party,
        0,
        essence,
        gameData.itemTable,
        &gameData.readableScrollTable,
        &gameData.mergedPotionSettingTable);
    CHECK_FALSE(second.consumed);
    CHECK_EQ(pMember->might, 25u);

    InventoryItem protection = {};
    protection.objectDescriptionId = 290;
    protection.standardEnchantPower = 2;
    InventoryItemUseResult protectionResult = InventoryItemUseRuntime::useItemOnMember(
        party,
        0,
        protection,
        gameData.itemTable,
        &gameData.readableScrollTable,
        &gameData.mergedPotionSettingTable);
    CHECK(protectionResult.consumed);
    CHECK(party.hasPartyBuff(PartyBuffId::ProtectionFromMagic));
}

TEST_CASE("MMerge stealing runtime resolves success, caught failure, and monster distance")
{
    Character thief = {};
    thief.speed = 50;
    thief.luck = 10;
    thief.skills["Stealing"] = {"Stealing", 10, SkillMastery::Master};

    CHECK_EQ(stealingTotalSkill(thief), 30);
    CHECK_EQ(stealingRecoveryTicks(thief), 95);

    StealingAttemptInput shop = {};
    shop.targetKind = StealingTargetKind::Shop;
    shop.itemValue = 500;
    shop.successRoll = 25;
    const StealingAttemptResult success = resolveStealingAttempt(thief, shop);
    CHECK(success.handled);
    CHECK(success.outcome == StealingOutcomeKind::Success);
    CHECK(success.stoleItem);

    shop.successRoll = 1;
    shop.caughtRoll = 0;
    const StealingAttemptResult caught = resolveStealingAttempt(thief, shop);
    CHECK(caught.outcome == StealingOutcomeKind::FailedCaught);
    CHECK(caught.breakInvisibility);
    CHECK_GT(caught.fineDelta, 0);

    StealingAttemptInput monster = {};
    monster.targetKind = StealingTargetKind::Monster;
    monster.monsterLevel = 20;
    monster.distanceSquared = 20000 * 9 + 1;
    const StealingAttemptResult tooFar = resolveStealingAttempt(thief, monster);
    CHECK(tooFar.outcome == StealingOutcomeKind::TooFar);

    monster.distanceSquared = 0;
    monster.successRoll = 20;
    monster.reputationSensitiveTarget = false;
    const StealingAttemptResult neutralMonster = resolveStealingAttempt(thief, monster);
    CHECK(neutralMonster.outcome == StealingOutcomeKind::Success);
    CHECK_EQ(neutralMonster.reputationDelta, 0);
}

TEST_CASE("MMerge runtime bounty kill marker persists to named globals")
{
    MonsterTable monsterTable = makeBountyMonsterTable();
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    EventRuntimeState state = {};
    worldRuntime.bindEventRuntimeState(&state);

    state.namedGlobalVars["MMerge.BountyHunt.spell_test.odm.Month"] = 0;
    state.namedGlobalVars["MMerge.BountyHunt.spell_test.odm.MonsterId"] = 10;
    worldRuntime.setCurrentLocationReputation(0);

    CHECK(markRuntimeBountyHuntMonsterKilled(worldRuntime, 10, &monsterTable));
    CHECK_EQ(state.namedGlobalVars["MMerge.BountyHunt.spell_test.odm.Done"], 1);
    CHECK_EQ(worldRuntime.currentLocationReputation(), -1);
}

TEST_CASE("MMerge town hall fine payment clears party fines")
{
    Party party = makeOneMemberParty();
    party.addGold(200);
    party.addFineGold(150);

    HouseEntry townHall = {};
    townHall.type = "Town Hall";
    townHall.name = "Test Town Hall";

    const std::vector<HouseActionOption> options =
        buildHouseActionOptions(townHall, &party, nullptr, nullptr, 0.0f, DialogueMenuId::None);
    CHECK(std::any_of(options.begin(), options.end(), [](const HouseActionOption &option)
    {
        return option.id == HouseActionId::TownHallPayFine && option.enabled;
    }));

    HouseActionOption payFine = {};
    payFine.id = HouseActionId::TownHallPayFine;
    const HouseActionResult result = performHouseAction(payFine, townHall, party, nullptr, nullptr);
    CHECK(result.succeeded);
    CHECK_EQ(party.fineGold(), 0);
    CHECK_EQ(party.gold(), 50);
}

TEST_CASE("MMerge bad reputation toggles guard hostility requests")
{
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    EventRuntimeState state = {};
    worldRuntime.bindEventRuntimeState(&state);
    worldRuntime.setCurrentLocationReputation(19);

    addStoredCurrentLocationReputation(worldRuntime, 1);
    CHECK_EQ(state.actorGroupHostilityRequests[38], true);
    CHECK_EQ(state.actorGroupHostilityRequests[55], true);

    addStoredCurrentLocationReputation(worldRuntime, -1);
    CHECK_EQ(state.actorGroupHostilityRequests[38], false);
    CHECK_EQ(state.actorGroupHostilityRequests[55], false);
}

TEST_CASE("MMerge terrible reputation and theft bans disable shop service")
{
    Party party = makeOneMemberParty();
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    EventRuntimeState state = {};
    worldRuntime.bindEventRuntimeState(&state);
    worldRuntime.setCurrentLocationReputation(25);

    HouseEntry shop = {};
    shop.id = 1234;
    shop.type = "Weapon Shop";
    shop.name = "Test Weapon Shop";

    std::vector<HouseActionOption> options =
        buildHouseActionOptions(shop, &party, nullptr, &worldRuntime, 0.0f, DialogueMenuId::None);
    REQUIRE_FALSE(options.empty());
    CHECK(std::all_of(options.begin(), options.end(), [](const HouseActionOption &option)
    {
        return !option.enabled;
    }));

    worldRuntime.setCurrentLocationReputation(0);
    state.namedGlobalVars["MMerge.ShopBanUntil.1234"] = 1000;
    options = buildHouseActionOptions(shop, &party, nullptr, &worldRuntime, 10.0f, DialogueMenuId::None);
    REQUIRE_FALSE(options.empty());
    CHECK(std::all_of(options.begin(), options.end(), [](const HouseActionOption &option)
    {
        return !option.enabled;
    }));
}

TEST_CASE("MMerge follower bonuses expose skill, gold, food, and item-service effects")
{
    EventRuntimeState state = {};
    state.hiredNpcFollowers.push_back({1001, 14, 150});
    state.hiredNpcFollowers.push_back({1002, 30, 250});
    state.hiredNpcFollowers.push_back({1003, 32, 300});
    state.hiredNpcFollowers.push_back({1004, 4, 100});

    CHECK_EQ(hiredNpcSkillBonus(state, "Learning"), 20);
    CHECK_EQ(hiredNpcRestFoodReduction(state), 2);
    CHECK_EQ(hiredNpcGoldFindBonusPercent(state), 20u);
    CHECK_EQ(totalHiredNpcFollowerFeePercent(state), 7u);
    CHECK_EQ(hiredNpcGoldAfterBonusAndFees(1000, state), 1116u);
    CHECK(hiredNpcCanIdentifyItemKind(state, "Ring"));
    CHECK_FALSE(hiredNpcCanRepairItemKind(state, "Armor"));

    REQUIRE(OpenYAMM::Tests::regressionGameDataLoaded());
    const auto &items = OpenYAMM::Tests::regressionGameData().itemTable;
    const auto mace = std::find_if(items.entries().begin(), items.entries().end(), [](const ItemDefinition &item)
    {
        return item.skillGroup == "Mace" && item.equipStat == "Weapon" && item.rarity == ItemRarity::Common;
    });
    const auto bow = std::find_if(items.entries().begin(), items.entries().end(), [](const ItemDefinition &item)
    {
        return item.skillGroup == "Bow" && item.equipStat == "Missile" && item.rarity == ItemRarity::Common;
    });
    REQUIRE(mace != items.entries().end());
    REQUIRE(bow != items.entries().end());

    Party party = makeOneMemberParty();
    Character *member = party.activeMember();
    REQUIRE(member != nullptr);
    member->skills["Mace"] = {"Mace", 4, SkillMastery::Expert};
    member->skills["Bow"] = {"Bow", 0, SkillMastery::None};
    member->equipment.mainHand = mace->itemId;
    member->equipment.bow = bow->itemId;
    party.setItemTable(&items);
    const auto baseline = GameMechanics::buildCharacterAttackProfile(*party.activeMember(), &items, nullptr);
    const auto itemBonuses = party.activeMember()->itemSkillBonuses;
    REQUIRE(baseline.hasBow);
    CHECK_EQ(baseline.meleeSkillLevel, 4u);

    party.addHiredNpcFollower({1005, 16, 400});
    const auto boosted = GameMechanics::buildCharacterAttackProfile(*party.activeMember(), &items, nullptr);
    CHECK_EQ(boosted.meleeSkillLevel, 7u);
    CHECK_EQ(boosted.meleeAttackBonus, baseline.meleeAttackBonus + 3);
    CHECK_EQ(boosted.rangedSkillLevel, 3u);
    CHECK_EQ(boosted.rangedSkillMastery, static_cast<uint32_t>(SkillMastery::Normal));
    CHECK_EQ(boosted.rangedAttackBonus.value_or(0), baseline.rangedAttackBonus.value_or(0) + 3);
    CHECK_EQ(party.activeMember()->findSkill("Mace")->level, 4u);
    CHECK(party.activeMember()->findSkill("Mace")->mastery == SkillMastery::Expert);
    CHECK(party.activeMember()->findSkill("Bow")->mastery == SkillMastery::None);
    CHECK(party.activeMember()->itemSkillBonuses == itemBonuses);

    party.setItemTable(&items);
    CHECK_EQ(GameMechanics::buildCharacterAttackProfile(*party.activeMember(), &items, nullptr).meleeSkillLevel, 7u);
    Party restored = {};
    restored.setItemTable(&items);
    restored.restoreSnapshot(party.snapshot());
    CHECK_EQ(GameMechanics::buildCharacterAttackProfile(*restored.activeMember(), &items, nullptr).meleeSkillLevel, 7u);
    restored.removeHiredNpcFollower(1005);
    const auto dismissed = GameMechanics::buildCharacterAttackProfile(*restored.activeMember(), &items, nullptr);
    CHECK_EQ(dismissed.meleeSkillLevel, baseline.meleeSkillLevel);
    CHECK_EQ(dismissed.meleeAttackBonus, baseline.meleeAttackBonus);
    CHECK_EQ(dismissed.rangedSkillLevel, 0u);

    Party utility = makeOneMemberParty();
    Character *learner = utility.activeMember();
    REQUIRE(learner != nullptr);
    learner->skills["Learning"] = {"Learning", 1, SkillMastery::Normal};
    learner->skills["Merchant"] = {"Merchant", 1, SkillMastery::Normal};
    learner->skills["Perception"] = {"Perception", 1, SkillMastery::Normal};
    learner->skills["DisarmTraps"] = {"DisarmTraps", 1, SkillMastery::Normal};
    learner->skills["LeatherArmor"] = {"LeatherArmor", 1, SkillMastery::Normal};
    const int originalPrice = PriceCalculator::itemBuyingPrice(learner, 1000, 2.0f);
    utility.addHiredNpcFollower({2001, 14, 150});
    utility.addHiredNpcFollower({2002, 21, 400});
    utility.addHiredNpcFollower({2003, 26, 600});
    utility.addHiredNpcFollower({2004, 22, 300});
    utility.grantSharedExperience(100);
    CHECK_EQ(learner->experience, 125u);
    CHECK_EQ(learner->findSkill("Learning")->level, 1u);
    CHECK(PriceCalculator::itemBuyingPrice(learner, 1000, 2.0f) < originalPrice);
    CHECK_EQ(GameMechanics::resolveCharacterPerceptionValue(*learner), 7);
    CHECK_EQ(GameMechanics::resolveCharacterDisarmTrapValue(*learner), 7);

    const auto leather = std::find_if(items.entries().begin(), items.entries().end(), [](const ItemDefinition &item)
    {
        return canonicalSkillName(item.skillGroup) == "LeatherArmor" && item.rarity == ItemRarity::Common;
    });
    REQUIRE(leather != items.entries().end());
    learner->equipment.armor = leather->itemId;
    utility.setItemTable(&items);
    const int armorBefore = GameMechanics::buildCharacterSheetSummary(*learner, &items).armorClass.actual;
    utility.removeHiredNpcFollower(2001);
    utility.addHiredNpcFollower({2005, 46, 400});
    CHECK_EQ(GameMechanics::buildCharacterSheetSummary(*learner, &items).armorClass.actual, armorBefore + 2);
    utility.removeHiredNpcFollower(2005);
    CHECK_EQ(GameMechanics::buildCharacterSheetSummary(*learner, &items).armorClass.actual, armorBefore);
}

TEST_CASE("MMerge follower luck and resistance bonuses are visible in character summaries")
{
    Character member = {};
    member.luck = 10;
    member.baseResistances.fire = 5;
    member.baseResistances.air = 6;
    member.baseResistances.water = 7;
    member.baseResistances.earth = 8;

    EventRuntimeState state = {};
    state.hiredNpcFollowers.push_back({1001, 27, 100});
    state.hiredNpcFollowers.push_back({1002, 37, 100});

    const CharacterSheetSummary summary =
        GameMechanics::buildCharacterSheetSummary(member, nullptr, nullptr, nullptr, &state);
    CHECK_EQ(summary.luck.actual, 20);
    CHECK_EQ(summary.luck.base, 10);
    CHECK_EQ(summary.fireResistance.actual, 25);
    CHECK_EQ(summary.airResistance.actual, 26);
    CHECK_EQ(summary.waterResistance.actual, 27);
    CHECK_EQ(summary.earthResistance.actual, 28);

    PartySeed seed = {};
    seed.members.push_back(member);
    Party party = {};
    party.seed(seed);
    party.addHiredNpcFollower({1001, 27, 100});
    party.addHiredNpcFollower({1002, 37, 100});
    const auto projected = GameMechanics::buildCharacterSheetSummary(*party.activeMember(), nullptr);
    const auto explicitEvent = GameMechanics::buildCharacterSheetSummary(
        *party.activeMember(), nullptr, nullptr, nullptr, &state);
    CHECK_EQ(projected.luck.actual, summary.luck.actual);
    CHECK_EQ(GameMechanics::resolveCharacterDisplayedActualPrimaryStat(*party.activeMember(), 0x2D, nullptr), 20);
    CHECK_EQ(GameMechanics::resolveCharacterDisplayedActualPrimaryStat(*party.activeMember(), 0x26, nullptr), 20);
    CHECK_EQ(GameMechanics::resolveCharacterDisplayedBasePrimaryStat(*party.activeMember(), 0x26, nullptr), 10);
    Character unprojected = *party.activeMember();
    unprojected.hasHiredNpcDefenseProjection = false;
    CHECK_EQ(GameMechanics::resolveCharacterDisplayedActualPrimaryStat(unprojected, 0x2D, nullptr), 10);
    CHECK_EQ(projected.fireResistance.actual, summary.fireResistance.actual);
    CHECK_EQ(explicitEvent.luck.actual, projected.luck.actual);
    CHECK_EQ(explicitEvent.fireResistance.actual, projected.fireResistance.actual);
    party.addHiredNpcFollower({1003, 37, 1000});
    const auto stacked = GameMechanics::buildCharacterSheetSummary(*party.activeMember(), nullptr);
    CHECK_EQ(stacked.fireResistance.actual, 45);
    std::mt19937 plainRolls(0), boostedRolls(0);
    const int plainDamage = GameMechanics::resolveCharacterIncomingDamage(
        member, nullptr, nullptr, nullptr, 80, CombatDamageType::Fire, plainRolls);
    const int boostedDamage = GameMechanics::resolveCharacterIncomingDamage(
        *party.activeMember(), nullptr, nullptr, nullptr, 80, CombatDamageType::Fire, boostedRolls);
    CHECK(boostedDamage < plainDamage);
    party.removeHiredNpcFollower(1001);
    party.removeHiredNpcFollower(1002);
    party.removeHiredNpcFollower(1003);
    const auto staleEvent = GameMechanics::buildCharacterSheetSummary(
        *party.activeMember(), nullptr, nullptr, nullptr, &state);
    CHECK_EQ(staleEvent.luck.actual, 10);
    CHECK_EQ(GameMechanics::resolveCharacterDisplayedActualPrimaryStat(*party.activeMember(), 0x2D, nullptr), 10);
    CHECK_EQ(staleEvent.fireResistance.actual, 5);
}

TEST_CASE("MMerge shop stealing integrates with stock and reputation")
{
    REQUIRE(OpenYAMM::Tests::regressionGameDataLoaded());
    const OpenYAMM::Tests::RegressionGameData &gameData = OpenYAMM::Tests::regressionGameData();
    const HouseEntry *pHouseEntry = gameData.houseTable.get(1);
    REQUIRE(pHouseEntry != nullptr);

    Party party = makeOneMemberParty();
    party.setItemTable(&gameData.itemTable);
    party.setItemEnchantTables(&gameData.standardItemEnchantTable, &gameData.specialItemEnchantTable);
    party.addGold(1000);

    Character *pThief = party.activeMember();
    REQUIRE(pThief != nullptr);
    pThief->skills["Stealing"] = {"Stealing", 30, SkillMastery::Grandmaster};

    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    worldRuntime.bindParty(&party);

    const std::vector<InventoryItem> &stock = HouseServiceRuntime::ensureStock(
        party,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        *pHouseEntry,
        worldRuntime.gameMinutes(),
        HouseStockMode::ShopStandard);
    const auto stockIt = std::find_if(stock.begin(), stock.end(), [](const InventoryItem &item)
    {
        return item.objectDescriptionId != 0;
    });
    REQUIRE(stockIt != stock.end());

    const size_t slotIndex = static_cast<size_t>(std::distance(stock.begin(), stockIt));
    const size_t initialInventoryCount = party.inventoryItemCount();
    std::string statusText;
    HouseServiceRuntime::ShopItemServiceResult serviceResult = HouseServiceRuntime::ShopItemServiceResult::None;

    const bool stoleItem = HouseServiceRuntime::tryStealStockItem(
        party,
        worldRuntime,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        *pHouseEntry,
        worldRuntime.gameMinutes(),
        HouseStockMode::ShopStandard,
        slotIndex,
        150u,
        1000000u,
        statusText,
        &serviceResult);

    INFO(statusText);
    INFO(static_cast<int>(serviceResult));
    REQUIRE(stoleItem);

    CHECK(serviceResult == HouseServiceRuntime::ShopItemServiceResult::Stolen);
    CHECK_EQ(party.inventoryItemCount(), initialInventoryCount + 1u);
    CHECK_EQ(worldRuntime.currentLocationReputation(), 1);
    const std::vector<InventoryItem> &remainingStock = HouseServiceRuntime::ensureStock(
        party,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        *pHouseEntry,
        worldRuntime.gameMinutes(),
        HouseStockMode::ShopStandard);
    REQUIRE(slotIndex < remainingStock.size());
    CHECK_EQ(remainingStock[slotIndex].objectDescriptionId, 0u);
}

TEST_CASE("MMerge caught shop stealing records a house ban")
{
    REQUIRE(OpenYAMM::Tests::regressionGameDataLoaded());
    const OpenYAMM::Tests::RegressionGameData &gameData = OpenYAMM::Tests::regressionGameData();
    const HouseEntry *pHouseEntry = gameData.houseTable.get(1);
    REQUIRE(pHouseEntry != nullptr);

    Party party = makeOneMemberParty();
    party.setItemTable(&gameData.itemTable);
    party.setItemEnchantTables(&gameData.standardItemEnchantTable, &gameData.specialItemEnchantTable);

    Character *pThief = party.activeMember();
    REQUIRE(pThief != nullptr);
    pThief->skills["Stealing"] = {"Stealing", 1, SkillMastery::Normal};

    EventRuntimeState runtimeState = {};
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime = {};
    worldRuntime.bindParty(&party);
    worldRuntime.bindEventRuntimeState(&runtimeState);

    const std::vector<InventoryItem> &stock = HouseServiceRuntime::ensureStock(
        party,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        *pHouseEntry,
        worldRuntime.gameMinutes(),
        HouseStockMode::ShopStandard);
    const auto stockIt = std::find_if(stock.begin(), stock.end(), [](const InventoryItem &item)
    {
        return item.objectDescriptionId != 0;
    });
    REQUIRE(stockIt != stock.end());

    const size_t slotIndex = static_cast<size_t>(std::distance(stock.begin(), stockIt));
    std::string statusText;
    HouseServiceRuntime::ShopItemServiceResult serviceResult = HouseServiceRuntime::ShopItemServiceResult::None;

    CHECK_FALSE(HouseServiceRuntime::tryStealStockItem(
        party,
        worldRuntime,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        *pHouseEntry,
        worldRuntime.gameMinutes(),
        HouseStockMode::ShopStandard,
        slotIndex,
        0u,
        0u,
        statusText,
        &serviceResult));

    CHECK(serviceResult == HouseServiceRuntime::ShopItemServiceResult::TheftCaught);
    const std::string banVar = "MMerge.ShopBanUntil." + std::to_string(pHouseEntry->id);
    REQUIRE(runtimeState.namedGlobalVars.contains(banVar));
    CHECK_GT(runtimeState.namedGlobalVars[banVar], 0);
}

TEST_CASE("MMerge follower identify and repair helpers are shop service consumers")
{
    REQUIRE(OpenYAMM::Tests::regressionGameDataLoaded());
    const OpenYAMM::Tests::RegressionGameData &gameData = OpenYAMM::Tests::regressionGameData();
    const uint32_t ringId = findFirstItemIdByEquipStat(gameData.itemTable, "Ring", true);
    const uint32_t armorId = findFirstItemIdByEquipStat(gameData.itemTable, "Armor", false);
    REQUIRE(ringId != 0);
    REQUIRE(armorId != 0);

    HouseEntry houseEntry = {};
    houseEntry.type = "Weapon Shop";
    houseEntry.priceMultiplier = 1.0f;

    Party party = makeOneMemberParty();
    party.setItemTable(&gameData.itemTable);
    party.setItemEnchantTables(&gameData.standardItemEnchantTable, &gameData.specialItemEnchantTable);
    party.addGold(10000);

    InventoryItem ring = {};
    ring.objectDescriptionId = ringId;
    ring.width = 1;
    ring.height = 1;
    ring.identified = false;
    REQUIRE(party.member(0)->addInventoryItemAt(ring, 0, 0));

    InventoryItem armor = {};
    armor.objectDescriptionId = armorId;
    armor.width = 2;
    armor.height = 3;
    armor.broken = true;
    REQUIRE(party.member(0)->addInventoryItemAt(armor, 2, 0));

    EventRuntimeState runtimeState = {};
    runtimeState.hiredNpcFollowers.push_back({1001, 4, 100});
    runtimeState.hiredNpcFollowers.push_back({1002, 2, 100});

    std::string statusText;
    HouseServiceRuntime::ShopItemServiceResult serviceResult = HouseServiceRuntime::ShopItemServiceResult::None;

    CHECK(HouseServiceRuntime::tryIdentifyInventoryItem(
        party,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        houseEntry,
        0,
        0,
        0,
        statusText,
        &serviceResult,
        0,
        &runtimeState));
    CHECK(serviceResult == HouseServiceRuntime::ShopItemServiceResult::Success);

    CHECK(HouseServiceRuntime::tryRepairInventoryItem(
        party,
        gameData.itemTable,
        gameData.standardItemEnchantTable,
        gameData.specialItemEnchantTable,
        houseEntry,
        0,
        2,
        0,
        statusText,
        &serviceResult,
        0,
        &runtimeState));
    CHECK(serviceResult == HouseServiceRuntime::ShopItemServiceResult::Success);
}
