#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "engine/TextTable.h"
#include "game/app/GameApplication.h"
#include "game/app/GameSettings.h"
#include "game/StringUtils.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/items/PriceCalculator.h"
#include "game/maps/MapDecorationDirection.h"
#include "game/maps/SaveGame.h"
#include "game/tables/HouseTable.h"
#include "game/tables/SpriteTables.h"
#include "game/outdoor/OutdoorMovementController.h"
#include "game/ui/SpellbookUiLayout.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint32_t TownPortalSpellId = 31;
constexpr uint32_t DimensionDoorScrollItemId = 190;
constexpr const char *MetVerdantGlobal = "MMerge.CrossContinents.MetVerdant";
constexpr const char *GotMainQuestGlobal = "MMerge.CrossContinents.GotMainQuest";
constexpr const char *GotFinalQuestGlobal = "MMerge.CrossContinents.GotFinalQuest";

enum class CaptureMode
{
    ToJadame,
    ToEnroth,
    ReloadOnly,
    ScrollLoop,
    ScrollReload,
};

struct Waypoint
{
    float x = 0.0f;
    float y = 0.0f;
    float tolerance = 80.0f;
};

struct CaptureState
{
    unsigned phase = 0;
    uint32_t phaseFrame = 0;
    size_t waypoint = 0;
    unsigned stalledFrames = 0;
    float walkedDistance = 0.0f;
    bool warmReloaded = false;
    bool saved = false;
    bool complete = false;
    size_t previewIndex = 0;
    double minutesBeforeWaiting = 0.0;
    double minutesAfterWaiting = 0.0;
    int foodBeforeWaiting = 0;
    int spellPointsBeforePortal = 0;
    float recoveryAtPortalRequest = 0.0f;
    double minutesBeforeHealing = 0.0;
    int goldBeforeHealing = 0;
    int healingPrice = 0;
    Party::Snapshot fixtureParty = {};
    Party::Snapshot stableParty = {};
    std::unordered_map<std::string, int32_t> stableGlobals;
    size_t loopLeg = 0;
    int scrollsBeforeLeg = 0;
    float legWalkedDistance = 0.0f;
    float legStartX = 0.0f;
    float legStartY = 0.0f;
    float legStartZ = 0.0f;
};

struct PendingCapture
{
    uint32_t requestedFrame = 0;
};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string captureModeName(CaptureMode mode)
{
    if (mode == CaptureMode::ToJadame) return "to-jadame";
    if (mode == CaptureMode::ToEnroth) return "to-enroth";
    if (mode == CaptureMode::ReloadOnly) return "reload-only";
    if (mode == CaptureMode::ScrollLoop) return "scroll-loop";
    return "scroll-reload";
}

bool isPhysicalInputEvent(const SDL_Event &event)
{
    return (event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_KEYMAP_CHANGED)
        || (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
        || (event.type >= SDL_EVENT_FINGER_DOWN && event.type <= SDL_EVENT_FINGER_CANCELED);
}

std::optional<size_t> findSorcerer(const Party::Snapshot &party)
{
    for (size_t index = 0; index < party.members.size(); ++index)
    {
        if (canonicalClassName(party.members[index].className) == "Sorcerer") return index;
    }
    return std::nullopt;
}

std::optional<GameSaveData> loadSave(const std::filesystem::path &path)
{
    std::string error;
    std::optional<GameSaveData> data = loadGameDataFromPath(path, error);
    require(data.has_value(), "Cannot read save " + path.string() + ": " + error);
    return data;
}

void writeSave(const std::filesystem::path &path, const GameSaveData &data)
{
    std::string error;
    require(saveGameDataToPath(path, data, error), "Cannot write save " + path.string() + ": " + error);
}

void requireControlledGlobals(const std::unordered_map<std::string, int32_t> &globals)
{
    const auto met = globals.find(MetVerdantGlobal);
    require(met != globals.end() && met->second == 1, "Controlled fixture lost MetVerdant=1");
    for (const char *name : {GotMainQuestGlobal, GotFinalQuestGlobal})
    {
        const auto iterator = globals.find(name);
        require(iterator == globals.end() || iterator->second == 0,
            std::string("Controlled fixture unexpectedly set ") + name);
    }
}

void requireExpectedQuestBits(const Party::Snapshot &party, bool afterJadame)
{
    std::unordered_set<uint32_t> expected = {1058, 1104, 1110};
    if (afterJadame)
    {
        expected.insert(185);
        expected.insert(226);
        expected.insert(401);
        expected.insert(407);
    }
    require(party.questBits == expected,
        afterJadame ? "Quest bits differ from the audited out01 first-load union"
                    : "Checkpoint-113 quest bits differ from the audited baseline");
    require(!party.questBits.contains(4) && !party.questBits.contains(309) && !party.questBits.contains(519),
        "A late-world Dimension Door landing gate changed during the boundary fixture");
}

std::filesystem::path makeControlledCasterFixture(
    const std::filesystem::path &source,
    const std::filesystem::path &output)
{
    std::optional<GameSaveData> loaded = loadSave(source);
    GameSaveData &data = *loaded;
    require(data.currentSceneKind == SceneKind::Outdoor && data.mapFileName == "oute3.odm",
        "The controlled caster fixture requires the accepted checkpoint-113 oute3 save");
    require(data.party.members.size() == 4, "The checkpoint-113 party must contain four members");
    requireExpectedQuestBits(data.party, false);
    require(data.namedGlobalVars.empty(), "The checkpoint-113 source unexpectedly has named globals");
    require(data.party.members[2].conditions.to_ullong() == 64,
        "The accepted Sorcerer condition baseline changed; the fixture must not clear it");
    require(data.party.members[3].conditions.to_ullong() == 64,
        "The accepted Archer condition baseline changed; the fixture must not clear it");

    const std::optional<size_t> sorcerer = findSorcerer(data.party);
    require(sorcerer.has_value() && *sorcerer == 2, "The accepted Sorcerer is not party member 2");
    Character &caster = data.party.members[*sorcerer];
    require(caster.maxSpellPoints >= 20, "The accepted Sorcerer's normalized maximum SP is below Town Portal cost");
    require(!caster.knowsSpell(TownPortalSpellId), "The source already knows Town Portal");
    require(caster.findSkillByCanonicalName("WaterMagic") == nullptr,
        "The source already has Water Magic");

    require(caster.learnSpell(TownPortalSpellId), "Cannot add Town Portal to the controlled caster");
    caster.skills.emplace("WaterMagic", CharacterSkill{"WaterMagic", 10, SkillMastery::Grandmaster});
    caster.spellPoints = 20;
    data.party.activeMemberIndex = *sorcerer;
    data.namedGlobalVars.emplace(MetVerdantGlobal, 1);

    std::filesystem::copy_file(source, output / "before-fixture.oysav");
    const std::filesystem::path fixture = output / "controlled-caster.oysav";
    writeSave(fixture, data);
    std::cout << "Controlled boundary fixture: active_member=" << *sorcerer
              << " spell=31 WaterMagic=10/Grandmaster SP=" << caster.spellPoints
              << " MetVerdant=1; no map, position, quest-bit, condition, immunity, or debug-mode edits.\n";
    return fixture;
}

int snapshotInventoryItemCount(const Party::Snapshot &party, uint32_t itemId)
{
    int count = 0;
    for (const Character &member : party.members)
    {
        for (const InventoryItem &item : member.inventory)
        {
            if (item.objectDescriptionId == itemId) count += static_cast<int>(item.quantity);
        }
    }
    return count;
}

std::filesystem::path makeScrollLoopFixture(
    const std::filesystem::path &source,
    const std::filesystem::path &output)
{
    std::optional<GameSaveData> loaded = loadSave(source);
    GameSaveData &data = *loaded;
    require(data.currentSceneKind == SceneKind::Outdoor && data.mapFileName == "oute3.odm",
        "The scroll-loop fixture requires the normal checkpoint-113 oute3 save");
    require(data.party.members.size() == 4, "The checkpoint-113 party must contain four members");
    requireExpectedQuestBits(data.party, false);
    require(data.namedGlobalVars.empty(), "The normal checkpoint-113 input unexpectedly has named globals");
    require(static_cast<uint32_t>(std::floor(data.savedGameMinutes / 1440.0f)) == 23,
        "The normal checkpoint-113 input is not on audited day 23");
    require(snapshotInventoryItemCount(data.party, DimensionDoorScrollItemId) == 0,
        "The normal checkpoint-113 input already contains item190");

    for (const Character &member : data.party.members)
    {
        require(!member.knowsSpell(TownPortalSpellId),
            "The normal checkpoint-113 input already knows Town Portal");
        require(member.findSkillByCanonicalName("WaterMagic") == nullptr,
            "The normal checkpoint-113 input already has Water Magic");
    }

    Character &member = data.party.members[0];
    require(member.health > 0 && member.conditions.none() && member.recoverySecondsRemaining == 0.0f,
        "The normal checkpoint-113 Knight is not already healthy enough and ready to act");
    require(member.spellPoints == 0, "The controlled scroll user must begin with zero SP");

    constexpr std::array<uint8_t, 3> scrollColumns = {4, 5, 6};
    for (const uint8_t gridX : scrollColumns)
    {
        require(member.inventoryItemAt(gridX, 0) == nullptr && member.inventoryItemAt(gridX, 1) == nullptr,
            "The audited item190 fixture cells are no longer empty");
        InventoryItem scroll = {};
        scroll.objectDescriptionId = DimensionDoorScrollItemId;
        scroll.quantity = 1;
        scroll.width = 1;
        scroll.height = 2;
        scroll.identified = true;
        require(member.addInventoryItemAt(scroll, gridX, 0),
            "Cannot place an independent item190 in the audited Knight backpack cell");
    }

    const float sourceMinutes = data.savedGameMinutes;
    data.savedGameMinutes += 1440.0f;
    data.party.activeMemberIndex = 0;
    data.party.everOwnedItemIds.insert(DimensionDoorScrollItemId);
    data.namedGlobalVars.emplace(MetVerdantGlobal, 1);

    std::filesystem::copy_file(source, output / "before-fixture.oysav");
    const std::filesystem::path fixture = output / "controlled-scroll-loop.oysav";
    writeSave(fixture, data);

    std::ofstream diff(output / "fixture-diff.tsv");
    diff.exceptions(std::ios::badbit | std::ios::failbit);
    diff << "field\tbefore\tafter\treason\n";
    diff << "saved_game_minutes\t" << sourceMinutes << '\t' << data.savedGameMinutes
         << "\tfixed day24 while preserving time-of-day\n";
    diff << "party.active_member\t3\t0\tpre-existing ready zero-SP Knight selected\n";
    diff << "named_global.MMerge.CrossContinents.MetVerdant\tabsent\t1\tcontrolled travel prerequisite\n";
    for (const uint8_t gridX : scrollColumns)
    {
        diff << "party.member[0].inventory[" << int(gridX)
             << ",0]\tabsent\titem190 quantity1 size1x2\tthree independent consumable scrolls\n";
    }
    diff << "party.ever_owned_item_ids[190]\tabsent\tpresent\tbookkeeping implied by controlled item addition\n";

    std::cout << "Controlled scroll-loop fixture: day="
              << static_cast<uint32_t>(std::floor(data.savedGameMinutes / 1440.0f))
              << " active_member=0 SP=0 MetVerdant=1 item190=3; no health, skill, spell, quest,"
                 " equipment, other inventory, map, position, or camera edits.\n";
    return fixture;
}

std::filesystem::path makeScrollDestinationFixture(
    const std::filesystem::path &source,
    const std::filesystem::path &output,
    uint32_t targetDay)
{
    std::optional<GameSaveData> loaded = loadSave(source);
    GameSaveData &data = *loaded;
    require(data.currentSceneKind == SceneKind::Outdoor
            && (data.mapFileName == "oute3.odm" || data.mapFileName == "outd3.odm"),
        "The scroll-destination fixture requires an accepted normal checkpoint-113 outdoor save");
    require(data.party.members.size() == 4, "The checkpoint-113 party must contain four members");
    if (data.mapFileName == "oute3.odm")
    {
        requireExpectedQuestBits(data.party, false);
    }
    else
    {
        require(data.party.questBits == std::unordered_set<uint32_t>{1104, 1110},
            "The alternate normal outd3 source no longer has its accepted quest-bit baseline");
    }
    require(data.namedGlobalVars.empty(), "The normal checkpoint-113 input unexpectedly has named globals");
    const uint32_t sourceDay = static_cast<uint32_t>(std::floor(data.savedGameMinutes / 1440.0f));
    require((data.mapFileName == "oute3.odm" && sourceDay == 23)
            || (data.mapFileName == "outd3.odm" && sourceDay == 2),
        "The normal checkpoint-113 input is not on its audited source day");
    require(targetDay <= 239, "The controlled destination day is outside the original 240-day cycle");
    require(snapshotInventoryItemCount(data.party, DimensionDoorScrollItemId) == 0,
        "The normal checkpoint-113 input already contains item190");

    for (const Character &candidate : data.party.members)
    {
        require(!candidate.knowsSpell(TownPortalSpellId),
            "The normal checkpoint-113 input already knows Town Portal");
        require(candidate.findSkillByCanonicalName("WaterMagic") == nullptr,
            "The normal checkpoint-113 input already has Water Magic");
    }

    Character &member = data.party.members[0];
    require(member.health > 0 && member.conditions.none() && member.recoverySecondsRemaining == 0.0f,
        "The normal checkpoint-113 Knight is not already healthy enough and ready to act");
    require(member.spellPoints == 0, "The controlled scroll user must begin with zero SP");
    InventoryItem scroll = {};
    scroll.objectDescriptionId = DimensionDoorScrollItemId;
    scroll.quantity = 1;
    scroll.width = 1;
    scroll.height = 2;
    scroll.identified = true;
    require(member.addInventoryItem(scroll),
        "Cannot place the independent item190 in the ready Knight backpack");
    const auto placedScroll = std::find_if(
        member.inventory.begin(), member.inventory.end(),
        [](const InventoryItem &item) { return item.objectDescriptionId == DimensionDoorScrollItemId; });
    require(placedScroll != member.inventory.end(), "Placed item190 is absent from the Knight backpack");
    const uint8_t scrollGridX = placedScroll->gridX;
    const uint8_t scrollGridY = placedScroll->gridY;

    const float sourceMinutes = data.savedGameMinutes;
    const size_t sourceActiveMember = data.party.activeMemberIndex;
    data.savedGameMinutes += (static_cast<float>(targetDay) - static_cast<float>(sourceDay)) * 1440.0f;
    require(data.savedGameMinutes >= 0.0f
            && static_cast<uint32_t>(std::floor(data.savedGameMinutes / 1440.0f)) == targetDay,
        "The controlled fixture could not preserve time-of-day on the requested representative day");
    data.party.activeMemberIndex = 0;
    data.party.everOwnedItemIds.insert(DimensionDoorScrollItemId);
    data.namedGlobalVars.emplace(MetVerdantGlobal, 1);

    std::filesystem::copy_file(source, output / "before-fixture.oysav");
    const std::filesystem::path fixture = output / "controlled-scroll-destination.oysav";
    writeSave(fixture, data);

    std::ofstream diff(output / "fixture-diff.tsv");
    diff.exceptions(std::ios::badbit | std::ios::failbit);
    diff << "field\tbefore\tafter\treason\n";
    diff << "saved_game_minutes\t" << sourceMinutes << '\t' << data.savedGameMinutes
         << "\trepresentative day " << targetDay << " while preserving time-of-day\n";
    diff << "party.active_member\t" << sourceActiveMember
         << "\t0\tpre-existing ready zero-SP Knight selected\n";
    diff << "named_global.MMerge.CrossContinents.MetVerdant\tabsent\t1\tcontrolled travel prerequisite\n";
    diff << "party.member[0].inventory[" << int(scrollGridX) << ',' << int(scrollGridY)
         << "]\tabsent\titem190 quantity1 size1x2"
            "\tone independent consumable scroll\n";
    diff << "party.ever_owned_item_ids[190]\tabsent\tpresent"
            "\tbookkeeping implied by controlled item addition\n";

    std::cout << "Controlled scroll-destination fixture: day=" << targetDay
              << " active_member=0 SP=0 MetVerdant=1 item190=1; no health, skill, spell, quest,"
                 " equipment, other inventory, map, position, or camera edits.\n";
    return fixture;
}

bool sameSkill(const CharacterSkill &left, const CharacterSkill &right)
{
    return left.name == right.name && left.level == right.level && left.mastery == right.mastery;
}

bool sameSkills(
    const std::unordered_map<std::string, CharacterSkill> &left,
    const std::unordered_map<std::string, CharacterSkill> &right)
{
    if (left.size() != right.size()) return false;
    for (const auto &[name, skill] : left)
    {
        const auto iterator = right.find(name);
        if (iterator == right.end() || !sameSkill(skill, iterator->second)) return false;
    }
    return true;
}

bool sameInventoryItem(const InventoryItem &left, const InventoryItem &right)
{
    return left.objectDescriptionId == right.objectDescriptionId
        && left.quantity == right.quantity
        && left.width == right.width && left.height == right.height
        && left.gridX == right.gridX && left.gridY == right.gridY
        && left.identified == right.identified && left.broken == right.broken
        && left.stolen == right.stolen
        && left.standardEnchantId == right.standardEnchantId
        && left.standardEnchantPower == right.standardEnchantPower
        && left.specialEnchantId == right.specialEnchantId
        && left.artifactId == right.artifactId && left.rarity == right.rarity
        && left.currentCharges == right.currentCharges && left.maxCharges == right.maxCharges
        && left.temporaryBonusRemainingSeconds == right.temporaryBonusRemainingSeconds;
}

bool sameInventory(const std::vector<InventoryItem> &left, const std::vector<InventoryItem> &right)
{
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), sameInventoryItem);
}

bool sameEquipment(const CharacterEquipment &left, const CharacterEquipment &right)
{
    return left.offHand == right.offHand && left.mainHand == right.mainHand
        && left.bow == right.bow && left.armor == right.armor && left.helm == right.helm
        && left.belt == right.belt && left.cloak == right.cloak
        && left.gauntlets == right.gauntlets && left.boots == right.boots
        && left.amulet == right.amulet && left.ring1 == right.ring1
        && left.ring2 == right.ring2 && left.ring3 == right.ring3
        && left.ring4 == right.ring4 && left.ring5 == right.ring5
        && left.ring6 == right.ring6;
}

std::vector<InventoryItem> inventoryWithoutDimensionDoorScrolls(const std::vector<InventoryItem> &inventory)
{
    std::vector<InventoryItem> result;
    for (const InventoryItem &item : inventory)
    {
        if (item.objectDescriptionId != DimensionDoorScrollItemId) result.push_back(item);
    }
    return result;
}

void requireScrollPartyCore(
    const Party::Snapshot &before,
    const Party::Snapshot &after,
    int expectedScrollCount)
{
    require(before.members.size() == after.members.size(), "Scroll travel changed party size");
    require(before.activeMemberIndex == 0 && after.activeMemberIndex == 0,
        "Scroll travel changed the controlled zero-SP member");
    require(before.gold == after.gold && before.bankGold == after.bankGold && before.food == after.food
            && before.fineGold == after.fineGold,
        "Scroll travel changed party resources");
    require(snapshotInventoryItemCount(after, DimensionDoorScrollItemId) == expectedScrollCount,
        "Scroll travel did not preserve the exact remaining item190 count");

    for (size_t index = 0; index < before.members.size(); ++index)
    {
        const Character &left = before.members[index];
        const Character &right = after.members[index];
        require(left.name == right.name && left.role == right.role && left.className == right.className
                && left.rosterId == right.rosterId && left.characterDataId == right.characterDataId
                && left.raceId == right.raceId && left.sexId == right.sexId
                && left.level == right.level && left.experience == right.experience
                && left.skillPoints == right.skillPoints,
            "Scroll travel changed identity or progression for member " + std::to_string(index));
        require(left.might == right.might && left.intellect == right.intellect
                && left.personality == right.personality && left.endurance == right.endurance
                && left.accuracy == right.accuracy && left.speed == right.speed && left.luck == right.luck
                && left.maxHealth == right.maxHealth && left.maxSpellPoints == right.maxSpellPoints,
            "Scroll travel changed base attributes for member " + std::to_string(index));
        require(left.health == right.health && left.spellPoints == right.spellPoints
                && left.conditions == right.conditions
                && left.recoverySecondsRemaining == right.recoverySecondsRemaining,
            "Scroll travel changed HP, SP, conditions, or recovery for member " + std::to_string(index));
        require(left.knownSpellIds == right.knownSpellIds && sameSkills(left.skills, right.skills)
                && left.awards == right.awards && left.eventVariables == right.eventVariables
                && left.playerBits == right.playerBits,
            "Scroll travel changed learned or event state for member " + std::to_string(index));
        require(sameInventory(
                    inventoryWithoutDimensionDoorScrolls(left.inventory),
                    inventoryWithoutDimensionDoorScrolls(right.inventory))
                && sameEquipment(left.equipment, right.equipment),
            "Scroll travel changed non-scroll inventory or equipment for member " + std::to_string(index));
    }
}

void requireScrollReloadParty(
    const Party::Snapshot &before,
    const Party::Snapshot &after,
    int expectedScrollCount)
{
    requireScrollPartyCore(before, after, expectedScrollCount);
    require(before.questBits == after.questBits && before.eventVariables == after.eventVariables,
        "Independent scroll arrival reload changed quests or party event variables");
    require(before.foundArtifactItems == after.foundArtifactItems
            && before.everOwnedItemIds == after.everOwnedItemIds
            && before.continentReputations == after.continentReputations,
        "Independent scroll arrival reload changed artifact, ownership, or reputation state");
    require(before.npcTopicOverrides == after.npcTopicOverrides
            && before.npcGroupNews == after.npcGroupNews
            && before.npcGreetingOverrides == after.npcGreetingOverrides
            && before.npcGreetingDisplayCounts == after.npcGreetingDisplayCounts
            && before.npcHouseOverrides == after.npcHouseOverrides
            && before.npcItemOverrides == after.npcItemOverrides
            && before.unavailableNpcIds == after.unavailableNpcIds,
        "Independent scroll arrival reload changed NPC state");
}

void requireScrollFixtureLoaded(
    const GameSession &session,
    int expectedScrollCount,
    uint32_t expectedDay = 24)
{
    const Party *pParty = session.gameplayScreenRuntime().partyReadOnly();
    require(pParty != nullptr && pParty->members().size() == 4 && pParty->activeMemberIndex() == 0,
        "Rendered scroll fixture did not restore the controlled party/member");
    require(pParty->inventoryItemCount(DimensionDoorScrollItemId) == expectedScrollCount,
        "Rendered scroll fixture has the wrong item190 count");
    const Character &member = pParty->members()[0];
    require(member.health > 0 && member.conditions.none() && member.spellPoints == 0
            && member.recoverySecondsRemaining == 0.0f && GameMechanics::canTakeGameplayAction(member),
        "Rendered scroll user is not the ready zero-SP Knight");
    for (const Character &candidate : pParty->members())
    {
        require(!candidate.knowsSpell(TownPortalSpellId),
            "Rendered scroll fixture unexpectedly knows Town Portal");
        require(candidate.findSkillByCanonicalName("WaterMagic") == nullptr,
            "Rendered scroll fixture unexpectedly has Water Magic");
    }
    requireControlledGlobals(session.namedGlobalVars());
    const IGameplayWorldRuntime *world = session.activeWorldRuntime();
    require(world != nullptr
            && static_cast<uint32_t>(std::floor(world->gameMinutes() / 1440.0)) == expectedDay,
        "Rendered scroll fixture is not on the requested representative day");
}

void requireStablePartyCore(const Party::Snapshot &before, const Party::Snapshot &after, bool afterJadame)
{
    require(before.members.size() == after.members.size(), "Cross-world travel changed party size");
    require(before.gold == after.gold && before.bankGold == after.bankGold && before.food == after.food,
        "Cross-world travel changed party resources");
    requireExpectedQuestBits(after, afterJadame);
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        const Character &left = before.members[index];
        const Character &right = after.members[index];
        require(left.name == right.name && left.className == right.className
                && left.level == right.level && left.experience == right.experience
                && left.skillPoints == right.skillPoints,
            "Cross-world travel changed stable identity or progression for member " + std::to_string(index));
        require(left.health == right.health && left.conditions == right.conditions,
            "Outdoor boundary route changed HP or conditions for member " + std::to_string(index));
        require(left.knownSpellIds == right.knownSpellIds && sameSkills(left.skills, right.skills)
                && left.awards == right.awards && left.eventVariables == right.eventVariables,
            "Cross-world travel changed learned state for member " + std::to_string(index));
        require(sameInventory(left.inventory, right.inventory) && sameEquipment(left.equipment, right.equipment),
            "Cross-world travel changed inventory or equipped item IDs for member " + std::to_string(index));
    }
}

void requireStableParty(const Party::Snapshot &before, const Party::Snapshot &after, bool afterJadame)
{
    requireStablePartyCore(before, after, afterJadame);
    require(before.activeMemberIndex == after.activeMemberIndex,
        "Reload changed the active controlled member");
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        require(before.members[index].spellPoints == after.members[index].spellPoints
                && before.members[index].recoverySecondsRemaining
                    == after.members[index].recoverySecondsRemaining,
            "Reload changed SP or recovery for member " + std::to_string(index));
    }
}

void requireTownPortalCost(
    const Party::Snapshot &before,
    const Party::Snapshot &after,
    float recoveryAtRequest)
{
    requireStablePartyCore(before, after, true);
    require(before.activeMemberIndex == 2 && after.activeMemberIndex == 3,
        "Ordinary Town Portal did not advance from the recovering caster to the next ready member");
    require(before.members.size() > 2 && after.members.size() > 2,
        "Controlled caster is absent from transport comparison");
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        if (index == 2)
        {
            require(before.members[index].spellPoints == 20 && after.members[index].spellPoints == 0,
                "Ordinary Town Portal did not spend exactly 20 SP");
            require(recoveryAtRequest > 0.0f
                    && after.members[index].recoverySecondsRemaining >= 0.0f
                    && after.members[index].recoverySecondsRemaining <= recoveryAtRequest,
                "Saved arrival recovery is outside the request-to-load decay range");
        }
        else
        {
            require(before.members[index].spellPoints == after.members[index].spellPoints
                    && before.members[index].recoverySecondsRemaining
                        == after.members[index].recoverySecondsRemaining,
                "Dimension Door changed another member's SP or recovery");
        }
    }
}

void requireControlledCasterLoaded(
    const GameSession &session, int expectedSpellPoints, size_t expectedActiveMemberIndex)
{
    const Party *party = session.gameplayScreenRuntime().partyReadOnly();
    require(party != nullptr && party->members().size() == 4
            && party->activeMemberIndex() == expectedActiveMemberIndex,
        "Rendered fixture did not restore the expected active member");
    require(party->members()[2].conditions.to_ullong() == 64
            && party->members()[3].conditions.to_ullong() == 64,
        "Rendered fixture did not preserve the two accepted condition-64 members");
    const Character &caster = party->members()[2];
    const CharacterSkill *water = caster.findSkillByCanonicalName("WaterMagic");
    require(caster.knowsSpell(TownPortalSpellId) && water != nullptr && water->level == 10
            && water->mastery == SkillMastery::Grandmaster,
        "Rendered fixture did not restore Town Portal and Grandmaster Water Magic");
    require(caster.maxSpellPoints >= 20 && caster.spellPoints == expectedSpellPoints,
        "Controlled caster SP does not match this process boundary");
    if (expectedSpellPoints >= 20)
    {
        require(GameMechanics::canTakeGameplayAction(caster),
            "The controlled Sorcerer is not conscious and ready to cast");
    }
    requireControlledGlobals(session.namedGlobalVars());
}

void requireReturnedCasterLoaded(const GameSession &session)
{
    const Party *party = session.gameplayScreenRuntime().partyReadOnly();
    require(party != nullptr && party->members().size() == 4 && party->activeMemberIndex() == 3,
        "Returned save did not restore the expected ready member");
    const Character &caster = party->members()[2];
    const CharacterSkill *water = caster.findSkillByCanonicalName("WaterMagic");
    require(caster.knowsSpell(TownPortalSpellId) && water != nullptr && water->level == 10
            && water->mastery == SkillMastery::Grandmaster,
        "Returned save lost the controlled Town Portal skill");
    require(caster.health == Party::effectiveMaximumHealth(caster)
            && caster.spellPoints == Party::effectiveMaximumSpellPoints(caster) - 20
            && caster.conditions.none(),
        "Returned save lost the exact temple-healed and cast-cost state");
    require(party->members()[3].conditions.to_ullong() == 64,
        "Return route unexpectedly healed the Archer");
    requireControlledGlobals(session.namedGlobalVars());
}

void requireTempleHealingOnly(
    const Party::Snapshot &before,
    const Party::Snapshot &after,
    int price)
{
    require(before.members.size() > 2 && after.members.size() == before.members.size(),
        "Temple healing changed party size");
    require(before.activeMemberIndex == 2 && after.activeMemberIndex == 2,
        "Temple healing changed the selected patient");
    require(before.gold - after.gold == price && price == 10,
        "Temple healing did not charge the audited 10 gold");
    const Character &caster = after.members[2];
    require(caster.health == Party::effectiveMaximumHealth(caster)
            && caster.spellPoints == Party::effectiveMaximumSpellPoints(caster)
            && caster.conditions.none(),
        "Temple healing did not fully restore and cure the controlled Sorcerer");
    Party::Snapshot normalized = after;
    normalized.gold = before.gold;
    normalized.members[2].health = before.members[2].health;
    normalized.members[2].spellPoints = before.members[2].spellPoints;
    normalized.members[2].conditions = before.members[2].conditions;
    requireStableParty(before, normalized, true);
}

void requireReturnTownPortalCost(
    const Party::Snapshot &before,
    const Party::Snapshot &after,
    float recoveryAtRequest)
{
    requireStablePartyCore(before, after, true);
    require(before.activeMemberIndex == 2 && after.activeMemberIndex == 3,
        "Return Town Portal did not select the next ready member");
    require(before.members.size() > 2 && after.members.size() > 2,
        "Return transport lost the controlled caster");
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        if (index == 2)
        {
            require(before.members[index].spellPoints == 39 && after.members[index].spellPoints == 19,
                "Return Town Portal did not spend exactly 20 healed SP");
            require(recoveryAtRequest > 0.0f
                    && after.members[index].recoverySecondsRemaining >= 0.0f
                    && after.members[index].recoverySecondsRemaining <= recoveryAtRequest,
                "Return arrival recovery is outside the request-to-load decay range");
        }
        else
        {
            require(before.members[index].spellPoints == after.members[index].spellPoints
                    && before.members[index].recoverySecondsRemaining
                        == after.members[index].recoverySecondsRemaining,
                "Return Dimension Door changed another member's SP or recovery");
        }
    }
}

std::optional<EventRuntimeState> savedMapEventState(const GameSaveData &data, const std::string &mapName)
{
    const auto iterator = data.outdoorWorldStates.find(mapName);
    if (iterator != data.outdoorWorldStates.end()) return iterator->second.eventRuntimeState;
    if (data.mapFileName == mapName && data.hasOutdoorRuntimeState) return data.outdoorWorld.eventRuntimeState;
    return std::nullopt;
}

void requireEnrothMapVarUnchanged(const std::filesystem::path &savedPath)
{
    const std::optional<GameSaveData> data = loadSave(savedPath);
    const std::optional<EventRuntimeState> state = savedMapEventState(*data, "oute3.odm");
    require(state.has_value() && state->mapVars[50] == 0,
        "The boundary flow changed oute3 MapVar50 instead of using its zero-valued activation branch");
}
}

struct ApplicationCaptureTestAccess
{
    static void pressAction(GameSession &session, int width, int height, KeyboardAction action, float delta = 0.0f)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.actions[keyboardActionIndex(action)] = {true, true, false};
        session.updateGameplay(input, delta);
        input.actions[keyboardActionIndex(action)] = {false, false, true};
        session.updateGameplay(input, 0.0f);
    }

    static void clickPoint(GameSession &session, int width, int height, float x, float y)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = x;
        input.pointerY = y;
        input.leftMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.leftMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
    }

    static void clickLayout(GameSession &session, int width, int height, const std::string &layoutId)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto *layout = runtime.findHudLayoutElement(layoutId);
        require(layout != nullptr && layout->visible, "Visible HUD layout is absent: " + layoutId);
        const auto resolved = runtime.resolveHudLayoutElement(
            layoutId, width, height, layout->width, layout->height);
        require(resolved.has_value(), "HUD layout cannot be resolved: " + layoutId);
        clickPoint(session, width, height,
            resolved->x + resolved->width * 0.5f,
            resolved->y + resolved->height * 0.5f);
        std::cout << "UI click: " << layoutId << " at "
                  << resolved->x + resolved->width * 0.5f << ','
                  << resolved->y + resolved->height * 0.5f << '\n';
    }

    static void clickDestination(
        GameSession &session, int width, int height, const std::string &destinationId)
    {
        const std::array<float, 2> point = destinationSafePoint(session, width, height, destinationId);
        clickPoint(session, width, height, point[0], point[1]);
        std::cout << "Destination click: " << destinationId << " at " << point[0] << ',' << point[1] << '\n';
    }

    static void selectPartyMember(GameSession &session, int width, int height, size_t memberIndex)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
        require(portrait.has_value(), "Controlled party portrait is absent");
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = portrait->x + portrait->width * 0.5f;
        input.pointerY = portrait->y + portrait->height * 0.5f;
        input.rightMouseButton.held = true;
        input.leftMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.leftMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
        require(runtime.partyReadOnly() != nullptr
                && runtime.partyReadOnly()->activeMemberIndex() == memberIndex
                && !runtime.characterScreen().open,
            "Normal portrait input did not select the controlled Sorcerer");
    }

    static void openMemberInventory(GameSession &session, int width, int height, size_t memberIndex)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
        require(portrait.has_value(), "Controlled party portrait is absent");
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = portrait->x + portrait->width * 0.5f;
        input.pointerY = portrait->y + portrait->height * 0.5f;
        input.rightMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.rightMouseButton.pressed = false;
        for (unsigned click = 0; click < 2; ++click)
        {
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        }
        input.rightMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
        require(runtime.characterScreen().open
                && runtime.selectedCharacterScreenSourceIndex() == memberIndex,
            "Normal portrait double-click did not open the controlled inventory");
    }

    static void pickInventoryItem(
        GameSession &session,
        int width,
        int height,
        size_t memberIndex,
        uint32_t itemId)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        require(pParty != nullptr && runtime.characterScreen().open
                && runtime.selectedCharacterScreenSourceIndex() == memberIndex,
            "Normal inventory item selection has no matching member screen");
        const auto *pLayout = runtime.findHudLayoutElement("CharacterInventoryGrid");
        require(pLayout != nullptr && pLayout->visible, "Original inventory grid is absent");
        const auto grid = runtime.resolveHudLayoutElement(
            pLayout->id, width, height, pLayout->width, pLayout->height);
        require(grid.has_value(), "Original inventory grid cannot be placed");

        bool picked = false;
        for (const GameplayRenderedInspectableHudItem &image : runtime.renderedInspectableHudItems())
        {
            if (image.objectDescriptionId != itemId || image.sourceMemberIndex != memberIndex
                || image.sourceType != GameplayUiController::ItemInspectSourceType::Inventory) continue;
            for (float y = std::max(grid->y + 1.0f, image.y + 2.0f);
                 y < image.y + image.height && !picked;
                 y += 3.0f)
            {
                for (float x = std::max(grid->x + 1.0f, image.x + 2.0f);
                     x < image.x + image.width;
                     x += 3.0f)
                {
                    if (x >= grid->x + grid->width || y >= grid->y + grid->height) continue;
                    const auto *pItem = pParty->members()[memberIndex].inventoryItemAt(
                        uint8_t((x - grid->x) * Character::InventoryWidth / grid->width),
                        uint8_t((y - grid->y) * Character::InventoryHeight / grid->height));
                    if (pItem == nullptr || pItem->objectDescriptionId != itemId
                        || !runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                    clickPoint(session, width, height, x, y);
                    picked = true;
                    break;
                }
            }
            if (picked) break;
        }
        require(picked && runtime.heldInventoryItem().active
                && runtime.heldInventoryItem().item.objectDescriptionId == itemId,
            "Normal inventory pointer click did not pick item190");
    }

    static void useHeldItemOnDoll(GameSession &session, int width, int height)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        require(runtime.heldInventoryItem().active, "Character doll use has no held item");
        const auto body = runtime.resolveHudLayoutElement("CharacterDollPanel", width, height, 175, 378);
        require(body.has_value(), "Original character doll panel is absent");
        clickPoint(session, width, height,
            body->x + body->width * 0.35f,
            body->y + body->height * 0.5f);
        require(!runtime.heldInventoryItem().active,
            "Normal character doll pointer click did not consume the held item190");
    }

    static void selectDialogAction(
        GameplayScreenRuntime &runtime, EventDialogActionKind kind, uint32_t id)
    {
        const EventDialogContent &dialog = runtime.activeEventDialog();
        require(dialog.isActive, "Expected house dialog is not active");
        const auto action = std::find_if(
            dialog.actions.begin(), dialog.actions.end(),
            [&](const EventDialogAction &candidate) { return candidate.kind == kind && candidate.id == id; });
        if (action == dialog.actions.end())
        {
            for (const EventDialogAction &candidate : dialog.actions)
            {
                std::cout << "Available house action: kind=" << int(candidate.kind)
                          << " id=" << candidate.id << " label=" << candidate.label << '\n';
            }
            throw std::runtime_error("Required house action is absent: " + std::to_string(id));
        }
        require(action->enabled, "Required house action is disabled");
        std::cout << "House action: kind=" << int(kind) << " id=" << id
                  << " label=" << action->label << '\n';
        runtime.eventDialogSelectionIndex() = size_t(action - dialog.actions.begin());
        runtime.executeActiveDialogAction();
    }

    static void pressEscape(GameSession &session, int width, int height)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
        input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
    }

    static void hoverDestination(
        GameApplication &game, int width, int height, const std::string &destinationId)
    {
        GameSession &session = game.m_gameSession;
        const std::array<float, 2> point = destinationSafePoint(session, width, height, destinationId);
        int windowCount = 0;
        SDL_Window **pWindows = SDL_GetWindows(&windowCount);
        SDL_Window *pWindow = pWindows && windowCount == 1 ? pWindows[0] : nullptr;
        SDL_free(pWindows);
        require(pWindow != nullptr, "Dimension Door preview needs the single capture window");
        int pointWidth = 0;
        int pointHeight = 0;
        require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight)
                && pointWidth > 0 && pointHeight > 0 && width > 0 && height > 0,
            "Dimension Door preview cannot resolve window point scaling");
        const float windowX = point[0] * static_cast<float>(pointWidth) / static_cast<float>(width);
        const float windowY = point[1] * static_cast<float>(pointHeight) / static_cast<float>(height);
        SDL_WarpMouseInWindow(pWindow, windowX, windowY);
        SDL_Event event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(pWindow);
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = false;
        event.button.x = windowX;
        event.button.y = windowY;
        game.m_gameInputSystem.handleSdlEvent(event);
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = point[0];
        input.pointerY = point[1];
        session.updateGameplay(input, 0.0f);
        std::cout << "Destination hover: " << destinationId << " at " << point[0] << ',' << point[1]
                  << " window=" << event.button.windowID << ':' << windowX << ',' << windowY << '\n';
    }

    static std::array<float, 2> destinationSafePoint(
        GameSession &session, int width, int height, const std::string &destinationId)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto destination = std::find_if(
            runtime.townPortalDestinations().begin(), runtime.townPortalDestinations().end(),
            [&](const GameplayTownPortalDestination &candidate) { return candidate.id == destinationId; });
        require(destination != runtime.townPortalDestinations().end(),
            "Dimension Door destination is absent: " + destinationId);
        const auto *layout = runtime.findHudLayoutElement("TownPortalBackground");
        require(layout != nullptr && layout->visible, "Town Portal background is absent");
        const auto background = runtime.resolveHudLayoutElement(
            "TownPortalBackground", width, height, layout->width, layout->height);
        require(background.has_value(), "Town Portal background cannot be resolved");
        const float scale = background->width / 640.0f;
        std::array<float, 2> logicalPoint = {};
        if (destinationId == "TPGlobal:0") logicalPoint = {37.0f, 100.0f};
        else if (destinationId == "TPGlobal:2") logicalPoint = {592.0f, 100.0f};
        else if (destinationId == "TPGlobal:4") logicalPoint = {310.0f, 24.0f};
        else throw std::runtime_error("No audited original safe point for " + destinationId);
        const float x = background->x + logicalPoint[0] * scale;
        const float y = background->y + logicalPoint[1] * scale;
        return {x, y};
    }

    static bool walkRoute(
        GameApplication &game,
        int width,
        int height,
        CaptureState &state,
        const std::vector<Waypoint> &route,
        std::ofstream &steps)
    {
        require(game.m_gameSession.activeWorldRuntime() != nullptr, "Walking route has no active world runtime");
        if (state.waypoint >= route.size()) return true;
        IGameplayWorldRuntime &world = *game.m_gameSession.activeWorldRuntime();
        const Waypoint &target = route[state.waypoint];
        const float beforeX = world.partyX();
        const float beforeY = world.partyY();
        const float dx = target.x - beforeX;
        const float dy = target.y - beforeY;
        const float remaining = std::hypot(dx, dy);
        if (remaining <= target.tolerance)
        {
            steps << "waypoint\t" << state.waypoint << '\t' << beforeX << '\t' << beforeY
                  << '\t' << world.partyFootZ() << "\treached\n";
            ++state.waypoint;
            state.stalledFrames = 0;
            return state.waypoint >= route.size();
        }
        game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
        const float movementSeconds = std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f);
        pressAction(game.m_gameSession, width, height, KeyboardAction::Forward, movementSeconds);
        const float moved = std::hypot(world.partyX() - beforeX, world.partyY() - beforeY);
        state.walkedDistance += moved;
        if (moved < 0.25f) ++state.stalledFrames;
        else state.stalledFrames = 0;
        if (state.stalledFrames >= 240)
        {
            game.quickSaveToPath(
                std::filesystem::current_path() / "route-stall-state.oysav",
                "跨洲普通路线停滞现场",
                {});
            steps << "stall\t" << state.waypoint << '\t' << world.partyX() << '\t'
                  << world.partyY() << '\t' << world.partyFootZ() << "\tordinary-forward\n";
            throw std::runtime_error(
                "Ordinary Forward route stalled at waypoint " + std::to_string(state.waypoint));
        }
        if (state.phaseFrame % 30 == 0)
        {
            steps << "step\t" << state.waypoint << '\t' << world.partyX() << '\t' << world.partyY()
                  << '\t' << world.partyFootZ() << "\tremaining=" << remaining << '\n';
        }
        return false;
    }

    static void advancePhase(CaptureState &state, unsigned next)
    {
        state.phase = next;
        state.phaseFrame = 0;
    }

    static void requireDimensionDoor(GameplayScreenRuntime &runtime, const IGameplayWorldRuntime &world)
    {
        const auto &overlay = runtime.utilitySpellOverlayReadOnly();
        require(overlay.active
                && overlay.mode == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor,
            "Standard Town Portal cast did not open Dimension Door");
        require(runtime.ensureTownPortalDestinationsLoaded(), "Dimension Door destinations did not load");
        const auto &destinations = runtime.townPortalDestinations();
        require(destinations.size() == 3, "Dimension Door did not expose exactly three current native choices");
        std::set<std::string> ids;
        for (const GameplayTownPortalDestination &destination : destinations)
        {
            ids.insert(destination.id);
            std::cout << "Dimension Door choice: id=" << destination.id
                      << " label=" << destination.label << " map=" << destination.mapName << '\n';
        }
        require(ids == std::set<std::string>{"TPGlobal:0", "TPGlobal:2", "TPGlobal:4"},
            "Dimension Door destination IDs differ from the audited current profile");
        require(static_cast<uint32_t>(std::floor(world.gameMinutes() / 1440.0)) == 24,
            "Dimension Door was not activated on the naturally reached day 24");

        struct ExpectedDestination
        {
            const char *id;
            const char *mapName;
            const char *iconName;
        };
        constexpr std::array<ExpectedDestination, 3> expected = {{
            {"TPGlobal:0", "out01.odm", "tpjadam1"},
            {"TPGlobal:2", "out11.odm", "tpantag5"},
            {"TPGlobal:4", "outd2.odm", "tpenroth5"},
        }};
        for (const ExpectedDestination &entry : expected)
        {
            const auto iterator = std::find_if(
                destinations.begin(), destinations.end(),
                [&](const GameplayTownPortalDestination &destination) { return destination.id == entry.id; });
            require(iterator != destinations.end(), std::string("Missing day-24 destination ") + entry.id);
            require(iterator->mapName == entry.mapName && iterator->iconTextureName == entry.iconName,
                std::string("Day-24 destination map/icon mismatch for ") + entry.id);
            require(iterator->x == 0 && iterator->y == 0 && iterator->z == 0,
                std::string("Day-24 destination lost its authored zero coordinates for ") + entry.id);
            require(iterator->directionDegrees.has_value() && *iterator->directionDegrees == 0,
                std::string("Day-24 destination lost its authored zero direction for ") + entry.id);
            std::cout << "Day-24 destination: id=" << entry.id << " map=" << iterator->mapName
                      << " icon=" << iterator->iconTextureName << '\n';
        }
    }

    static void requireDimensionDoorTarget(
        GameplayScreenRuntime &runtime,
        const IGameplayWorldRuntime &world,
        uint32_t expectedDay,
        const std::string &destinationId,
        const std::string &expectedMap,
        const std::string &expectedIcon)
    {
        const auto &overlay = runtime.utilitySpellOverlayReadOnly();
        require(overlay.active
                && overlay.mode == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor
                && !overlay.spendMana && !overlay.applyRecovery,
            "Held item190 did not open the free Dimension Door selection page");
        require(runtime.ensureTownPortalDestinationsLoaded(), "Dimension Door destinations did not load");
        const auto &destinations = runtime.townPortalDestinations();
        require(destinations.size() == 3, "Dimension Door did not expose exactly three current native choices");
        std::set<std::string> ids;
        for (const GameplayTownPortalDestination &destination : destinations) ids.insert(destination.id);
        require(ids == std::set<std::string>{"TPGlobal:0", "TPGlobal:2", "TPGlobal:4"},
            "Dimension Door destination IDs differ from the audited current profile");
        require(static_cast<uint32_t>(std::floor(world.gameMinutes() / 1440.0)) == expectedDay,
            "Dimension Door was not activated on the requested representative day");
        const auto destination = std::find_if(
            destinations.begin(), destinations.end(),
            [&](const GameplayTownPortalDestination &candidate)
            {
                return candidate.id == destinationId;
            });
        require(destination != destinations.end(), "Requested Dimension Door destination is absent");
        require(destination->mapName == expectedMap && destination->iconTextureName == expectedIcon,
            "Requested Dimension Door destination differs from the checkpoint-121 map/icon row");
        require(destination->x == 0 && destination->y == 0 && destination->z == 0,
            "Requested Dimension Door destination lost its authored zero coordinates");
        require(destination->directionDegrees.has_value() && *destination->directionDegrees == 0,
            "Requested Dimension Door destination lost its authored zero direction");
        std::cout << "Representative-day destination: day=" << expectedDay
                  << " id=" << destinationId << " map=" << destination->mapName
                  << " icon=" << destination->iconTextureName << '\n';
    }

    static void requireAuthoredOutdoorStart(GameApplication &game, std::ofstream &steps)
    {
        const auto &selectedMap = game.m_gameDataLoader.getSelectedMap();
        require(selectedMap.has_value() && selectedMap->map.fileName == "outd2.odm"
                && selectedMap->outdoorMapData.has_value(),
            "Enroth arrival did not retain the selected original outdoor map data");
        const auto start = std::find_if(
            selectedMap->outdoorMapData->entities.begin(),
            selectedMap->outdoorMapData->entities.end(),
            [](const OutdoorEntity &entity)
            {
                return entity.name == "Party Start" || entity.name == "party start";
            });
        require(start != selectedMap->outdoorMapData->entities.end(),
            "Original outd2 map has no authored Party Start entity");
        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
        require(world != nullptr, "Enroth arrival lost the outdoor world runtime");
        const float expectedYaw = mapDecorationFacingDegrees(start->facing, start->variableSecondary)
            * 3.14159265358979323846f / 180.0f;
        const float yawDistance = std::abs(std::atan2(
            std::sin(game.m_outdoorGameView.cameraYawRadians() - expectedYaw),
            std::cos(game.m_outdoorGameView.cameraYawRadians() - expectedYaw)));
        require(std::abs(world->partyX() - static_cast<float>(start->x)) < 0.1f
                && std::abs(world->partyY() - static_cast<float>(start->y)) < 0.1f
                && std::abs(world->partyFootZ() - static_cast<float>(start->z + 1)) < 0.1f
                && yawDistance < 0.0001f
                && std::abs(game.m_outdoorGameView.cameraPitchRadians() - (-0.15f)) < 0.0001f,
            "Enroth arrival differs from the original outd2 Party Start entity");
        steps << "authored-map-start\t0\t" << world->partyX() << '\t' << world->partyY()
              << '\t' << world->partyFootZ() << "\tentity=" << start->x << ',' << start->y << ','
              << start->z << " facing=" << start->facing << " yaw="
              << game.m_outdoorGameView.cameraYawRadians() << " pitch="
              << game.m_outdoorGameView.cameraPitchRadians() << '\n';
    }

    static void requireAuthoredOutdoorStart(
        GameApplication &game,
        const std::string &expectedMap,
        size_t leg,
        std::ofstream &steps)
    {
        const auto &selectedMap = game.m_gameDataLoader.getSelectedMap();
        require(selectedMap.has_value() && selectedMap->map.fileName == expectedMap
                && selectedMap->outdoorMapData.has_value(),
            "Scroll arrival did not retain the selected original outdoor map data");
        require(game.m_pAssetFileSystem != nullptr, "Scroll MapStart audit lost the active asset filesystem");
        const std::optional<std::string> decorationText =
            game.m_pAssetFileSystem->readTextFile("engine/data_tables/decoration_data.txt");
        require(decorationText.has_value(), "Scroll MapStart audit cannot read decoration_data.txt");
        const std::optional<Engine::TextTable> parsed = Engine::TextTable::parseTabSeparated(*decorationText);
        require(parsed.has_value(), "Scroll MapStart audit cannot parse decoration_data.txt");
        std::vector<std::vector<std::string>> rows;
        rows.reserve(parsed->getRowCount());
        for (size_t row = 0; row < parsed->getRowCount(); ++row) rows.push_back(parsed->getRow(row));
        DecorationTable decorationTable;
        require(decorationTable.loadRows(rows), "Scroll MapStart audit cannot load decoration_data.txt");

        const OutdoorEntity *pStart = nullptr;
        for (const OutdoorEntity &entity : selectedMap->outdoorMapData->entities)
        {
            std::string name = toLowerCopy(entity.name);
            if (name.empty())
            {
                if (const DecorationEntry *pDecoration = decorationTable.get(entity.decorationListId))
                    name = pDecoration->internalName;
            }
            if (name == "party start")
            {
                pStart = &entity;
                break;
            }
        }
        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
        require(world != nullptr, "Scroll arrival lost the outdoor world runtime");
        const int centerGridX = OutdoorMapData::TerrainWidth / 2;
        const int centerGridY = OutdoorMapData::TerrainHeight / 2;
        const size_t centerSampleIndex = static_cast<size_t>(
            centerGridY * OutdoorMapData::TerrainWidth + centerGridX);
        const float authoredX = pStart != nullptr ? static_cast<float>(pStart->x) : 0.0f;
        const float authoredY = pStart != nullptr ? static_cast<float>(pStart->y) : 0.0f;
        const float authoredZHint = pStart != nullptr
            ? static_cast<float>(pStart->z)
            : static_cast<float>(selectedMap->outdoorMapData->heightMap[centerSampleIndex]
                * OutdoorMapData::TerrainHeightScale);
        const float collisionRadius = game.m_activeWorldManifest.partyMovement.declared
            ? game.m_activeWorldManifest.partyMovement.collisionRadius
            : 37.0f;
        const OutdoorMovementController startResolver(
            *selectedMap->outdoorMapData,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt);
        const OutdoorMoveState resolvedStart = startResolver.initializeStateForBody(
            authoredX, authoredY, authoredZHint, collisionRadius);
        const float expectedX = resolvedStart.x;
        const float expectedY = resolvedStart.y;
        const float expectedZ = resolvedStart.footZ;
        const float expectedYaw = pStart != nullptr
            ? mapDecorationFacingDegrees(pStart->facing, pStart->variableSecondary)
                * 3.14159265358979323846f / 180.0f
            : -3.14159265358979323846f * 0.5f;
        const float yawDistance = std::abs(std::atan2(
            std::sin(game.m_outdoorGameView.cameraYawRadians() - expectedYaw),
            std::cos(game.m_outdoorGameView.cameraYawRadians() - expectedYaw)));
        const bool exactStart = std::abs(world->partyX() - expectedX) < 0.1f
            && std::abs(world->partyY() - expectedY) < 0.1f
            && std::abs(world->partyFootZ() - expectedZ) < 0.1f
            && yawDistance < 0.0001f
            && std::abs(game.m_outdoorGameView.cameraPitchRadians() - (-0.15f)) < 0.0001f;
        if (!exactStart)
        {
            throw std::runtime_error(
                "Scroll arrival differs from the engine-resolved original MapStart: map=" + expectedMap
                + " source=" + (pStart != nullptr ? "party-start-decoration" : "center-height-fallback")
                + " expected=" + std::to_string(expectedX) + ',' + std::to_string(expectedY) + ','
                + std::to_string(expectedZ) + ',' + std::to_string(expectedYaw) + ",-0.15"
                + " actual=" + std::to_string(world->partyX()) + ',' + std::to_string(world->partyY())
                + ',' + std::to_string(world->partyFootZ()) + ','
                + std::to_string(game.m_outdoorGameView.cameraYawRadians()) + ','
                + std::to_string(game.m_outdoorGameView.cameraPitchRadians()));
        }
        steps << "authored-map-start\t" << leg << '\t' << world->partyX() << '\t'
              << world->partyY() << '\t' << world->partyFootZ()
              << "\tmap=" << expectedMap << " world=" << selectedMap->map.worldId
              << " source=" << (pStart != nullptr ? "party-start-decoration" : "center-height-fallback")
              << " authored_hint=" << authoredX << ',' << authoredY << ',' << authoredZHint
              << " collision_radius=" << collisionRadius
              << " expected=" << expectedX << ',' << expectedY << ',' << expectedZ
              << " support_kind=" << static_cast<int>(resolvedStart.supportKind)
              << " facing=" << (pStart != nullptr ? std::to_string(pStart->facing) : "default")
              << " yaw="
              << game.m_outdoorGameView.cameraYawRadians() << " pitch="
              << game.m_outdoorGameView.cameraPitchRadians() << '\n';
    }

    static void openDimensionDoor(
        GameApplication &game,
        int width,
        int height,
        CaptureState &state,
        unsigned firstPhase = 7)
    {
        GameplayScreenRuntime &runtime = game.m_gameSession.gameplayScreenRuntime();
        switch (state.phase - firstPhase)
        {
        case 0:
            pressAction(game.m_gameSession, width, height, KeyboardAction::Cast);
            advancePhase(state, firstPhase + 1);
            break;
        case 1:
            if (state.phaseFrame >= 4)
            {
                require(runtime.spellbookReadOnly().active, "Normal Cast action did not open the spellbook");
                clickLayout(game.m_gameSession, width, height, "SpellbookSchoolButtonWater");
                advancePhase(state, firstPhase + 2);
            }
            break;
        case 2:
            if (state.phaseFrame >= 4)
            {
                clickLayout(game.m_gameSession, width, height,
                    spellbookSpellLayoutId(GameplayUiController::SpellbookSchool::Water, 9));
                advancePhase(state, firstPhase + 3);
            }
            break;
        case 3:
            if (state.phaseFrame >= 4)
            {
                clickLayout(game.m_gameSession, width, height, "SpellbookCastButton");
                advancePhase(state, firstPhase + 4);
            }
            break;
        default:
            break;
        }
    }

    static int runScrollLoop(
        const std::filesystem::path &runtimeRoot,
        const std::filesystem::path &sourceSave,
        const std::filesystem::path &output)
    {
        struct ScrollLeg
        {
            const char *label;
            const char *destinationId;
            const char *mapName;
            const char *worldId;
            const char *iconName;
        };
        static constexpr std::array<ScrollLeg, 3> legs = {{
            {"mm7", "TPGlobal:2", "out11.odm", "mm7", "tpantag5"},
            {"mm8", "TPGlobal:0", "out01.odm", "mm8", "tpjadam1"},
            {"mm6", "TPGlobal:4", "outd2.odm", "mm6", "tpenroth5"},
        }};

        require(std::filesystem::exists(runtimeRoot), "Runtime root does not exist");
        require(std::filesystem::is_regular_file(sourceSave), "Input save does not exist");
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        std::filesystem::create_directories(output);
        const std::filesystem::path absoluteOutput = std::filesystem::canonical(output);
        const std::filesystem::path absoluteSource = std::filesystem::canonical(sourceSave);
        const std::filesystem::path input = makeScrollLoopFixture(absoluteSource, absoluteOutput);
        const std::optional<GameSaveData> inputData = loadSave(input);

        std::filesystem::current_path(absoluteOutput);
        std::ofstream steps(absoluteOutput / "steps.tsv");
        steps.exceptions(std::ios::badbit | std::ios::failbit);
        steps << "kind\tindex\tx\ty\tz\tdetail\n";

        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.gameplayTrace = true;
        settings.gameplayTraceAppend = false;
        settings.gameplayTraceFile = (absoluteOutput / "gameplay.trace").string();
        settings.assetRoot = runtimeRoot.string();
        settings.windowMode = WindowMode::Windowed;
        std::string settingsError;
        require(saveGameSettings(absoluteOutput / "settings.ini", settings, settingsError),
            "Cannot write scroll-loop settings: " + settingsError);

        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM three-continent scroll validation";
        config.assetRoot = runtimeRoot.string();
        config.activeWorldId = "mm6";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;

        GameApplication game(config);
        CaptureState state;
        uint32_t frames = 0;
        uint64_t startedTicks = 0;
        std::map<std::string, PendingCapture> pendingCaptures;
        std::set<std::string> captured;
        std::set<std::string> expectedCaptures;
        for (size_t leg = 0; leg < legs.size(); ++leg)
        {
            const std::string prefix = "leg" + std::to_string(leg + 1) + '-' + legs[leg].label;
            expectedCaptures.insert(prefix + "-inventory");
            expectedCaptures.insert(prefix + "-held-scroll");
            expectedCaptures.insert(prefix + "-selection");
            expectedCaptures.insert(prefix + "-arrived");
        }

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                return game.loadGameData(files);
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal,
                    "Three-continent scroll capture requires the Metal backend");
                if (!game.initializeRenderer() || !game.loadSessionFromPath(input)) return false;
                require(game.m_gameSession.currentMapFileName() == "oute3.odm",
                    "Rendered scroll fixture did not begin on the normal MM6 source map");
                requireScrollFixtureLoaded(game.m_gameSession, 3);
                require(game.quickSaveToPath(
                        absoluteOutput / "loaded-before-loop.oysav", "三大陆卷轴稳定前置", {}),
                    "Cannot preserve the loaded scroll-loop boundary");
                state.stableParty = game.m_gameSession.gameplayScreenRuntime().partyReadOnly()->snapshot();
                state.stableGlobals = game.m_gameSession.namedGlobalVars();
                steps << "loaded-fixture\t0\t"
                      << game.m_gameSession.activeWorldRuntime()->partyX() << '\t'
                      << game.m_gameSession.activeWorldRuntime()->partyY() << '\t'
                      << game.m_gameSession.activeWorldRuntime()->partyFootZ()
                      << "\tmap=oute3.odm world=mm6 day=24 item190=3 sp=0 recovery=0\n";
                startedTicks = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (!isPhysicalInputEvent(event)) game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                ++frames;
                ++state.phaseFrame;
                std::optional<std::string> captureRequest;
                GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                Party *pParty = screen.party();
                require(pParty != nullptr, "Scroll loop lost the party");
                const ScrollLeg &leg = legs[state.loopLeg];
                const std::string prefix = "leg" + std::to_string(state.loopLeg + 1) + '-' + leg.label;
                const int expectedBefore = 3 - static_cast<int>(state.loopLeg);
                const int expectedAfter = expectedBefore - 1;

                if (!state.complete)
                {
                    if (state.phase == 0 && state.phaseFrame >= (state.loopLeg == 0 ? 30u : 15u))
                    {
                        requireScrollFixtureLoaded(game.m_gameSession, expectedBefore);
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), expectedBefore);
                        state.scrollsBeforeLeg = pParty->inventoryItemCount(DimensionDoorScrollItemId);
                        require(state.scrollsBeforeLeg == expectedBefore,
                            "Scroll count changed before the next ordinary item use");
                        require(game.quickSaveToPath(
                                absoluteOutput / (prefix + "-before-use.oysav"),
                                "三大陆卷轴使用前", {}),
                            "Cannot save the pre-scroll-use boundary");
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Scroll use lost the active outdoor runtime");
                        steps << "scroll-before-use\t" << state.loopLeg + 1 << '\t'
                              << world->partyX() << '\t' << world->partyY() << '\t' << world->partyFootZ()
                              << "\tmap=" << game.m_gameSession.currentMapFileName()
                              << " item190=" << state.scrollsBeforeLeg << " sp=" << pParty->members()[0].spellPoints
                              << " recovery=" << pParty->members()[0].recoverySecondsRemaining << '\n';
                        openMemberInventory(game.m_gameSession, width, height, 0);
                        advancePhase(state, 1);
                    }
                    else if (state.phase == 1 && state.phaseFrame >= 8)
                    {
                        require(screen.characterScreen().open
                                && screen.selectedCharacterScreenSourceIndex() == 0,
                            "Controlled inventory closed before item190 pointer selection");
                        captureRequest = prefix + "-inventory";
                        advancePhase(state, 2);
                    }
                    else if (state.phase == 2 && captured.contains(prefix + "-inventory"))
                    {
                        pickInventoryItem(
                            game.m_gameSession, width, height, 0, DimensionDoorScrollItemId);
                        require(pParty->inventoryItemCount(DimensionDoorScrollItemId) == expectedAfter
                                && screen.heldInventoryItem().active,
                            "Picking item190 did not move exactly one independent scroll to the hand");
                        steps << "scroll-held\t" << state.loopLeg + 1 << "\t0\t0\t0\tinventory="
                              << expectedAfter << " held=190\n";
                        captureRequest = prefix + "-held-scroll";
                        advancePhase(state, 3);
                    }
                    else if (state.phase == 3 && captured.contains(prefix + "-held-scroll"))
                    {
                        useHeldItemOnDoll(game.m_gameSession, width, height);
                        require(pParty->inventoryItemCount(DimensionDoorScrollItemId) == expectedAfter,
                            "Ordinary item190 use did not consume exactly one scroll");
                        require(pParty->members()[0].spellPoints == 0
                                && pParty->members()[0].recoverySecondsRemaining == 0.0f,
                            "Held item190 use changed SP or recovery");
                        require(screen.utilitySpellOverlayReadOnly().active
                                && screen.utilitySpellOverlayReadOnly().mode
                                    == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor
                                && !screen.utilitySpellOverlayReadOnly().spendMana
                                && !screen.utilitySpellOverlayReadOnly().applyRecovery,
                            "Held item190 did not open the free Dimension Door selection page");
                        steps << "scroll-used\t" << state.loopLeg + 1 << "\t0\t0\t0\titem190="
                              << expectedAfter << " sp=0 recovery=0 ordinary_doll_pointer=1\n";
                        advancePhase(state, 4);
                    }
                    else if (state.phase == 4 && state.phaseFrame >= 4)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Dimension Door selection lost the active world");
                        requireDimensionDoor(screen, *world);
                        const auto destination = std::find_if(
                            screen.townPortalDestinations().begin(), screen.townPortalDestinations().end(),
                            [&](const GameplayTownPortalDestination &candidate)
                            {
                                return candidate.id == leg.destinationId;
                            });
                        require(destination != screen.townPortalDestinations().end()
                                && destination->mapName == leg.mapName
                                && destination->iconTextureName == leg.iconName,
                            "Selected scroll destination differs from the audited day-24 map/icon");
                        hoverDestination(game, width, height, leg.destinationId);
                        captureRequest = prefix + "-selection";
                        advancePhase(state, 5);
                    }
                    else if (state.phase == 5 && captured.contains(prefix + "-selection"))
                    {
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), expectedAfter);
                        require(game.quickSaveToPath(
                                absoluteOutput / (prefix + "-before-selection.oysav"),
                                "三大陆卷轴选择前", {}),
                            "Cannot save the destination-selection boundary");
                        clickDestination(game.m_gameSession, width, height, leg.destinationId);
                        require(pParty->members()[0].spellPoints == 0
                                && pParty->members()[0].recoverySecondsRemaining == 0.0f,
                            "Scroll destination pointer selection changed SP or recovery");
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr && world->eventRuntimeState() != nullptr,
                            "Scroll destination request lost the active event runtime");
                        const auto &move = world->eventRuntimeState()->pendingMapMove;
                        require(move.has_value() && move->mapName.has_value()
                                && *move->mapName == leg.mapName
                                && move->x == 0 && move->y == 0 && move->z == 0
                                && move->useMapStartPosition
                                && !move->directionDegrees.has_value()
                                && !move->lookAngleDegrees.has_value(),
                            "Scroll destination request lost the zero/map-start cross-world contract");
                        steps << "portal-request\t" << state.loopLeg + 1
                              << "\t0\t0\t0\tdestination=" << leg.destinationId
                              << " map=" << leg.mapName << " world=" << leg.worldId
                              << " icon=" << leg.iconName << " map_start=1 sp=0 recovery=0\n";
                        advancePhase(state, 6);
                    }
                    else if (state.phase == 6
                             && game.m_gameSession.currentMapFileName() == leg.mapName)
                    {
                        const auto &selectedMap = game.m_gameDataLoader.getSelectedMap();
                        require(selectedMap.has_value() && selectedMap->map.worldId == leg.worldId,
                            "Actual scroll arrival world differs from the audited target world");
                        requireAuthoredOutdoorStart(game, leg.mapName, state.loopLeg + 1, steps);
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), expectedAfter);
                        require(game.quickSaveToPath(
                                absoluteOutput / (prefix + "-map-start.oysav"),
                                "三大陆原始落点", {}),
                            "Cannot save the authored map-start boundary");
                        advancePhase(state, 7);
                    }
                    else if (state.phase == 7 && state.phaseFrame >= 4)
                    {
                        require(game.m_gameSession.currentMapFileName() == leg.mapName,
                            "Scroll arrival changed map before ordinary movement");
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), expectedAfter);
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Normal arrival movement lost the active world");
                        state.legStartX = world->partyX();
                        state.legStartY = world->partyY();
                        state.legStartZ = world->partyFootZ();
                        state.legWalkedDistance = 0.0f;
                        advancePhase(state, 9);
                    }
                    else if (state.phase == 9)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Normal arrival movement lost the active world");
                        const float x = world->partyX();
                        const float y = world->partyY();
                        pressAction(game.m_gameSession, width, height, KeyboardAction::Forward, 0.025f);
                        state.legWalkedDistance += std::hypot(world->partyX() - x, world->partyY() - y);
                        if (state.phaseFrame >= 8) advancePhase(state, 10);
                    }
                    else if (state.phase == 10 && state.phaseFrame >= 8)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr && state.legWalkedDistance >= 8.0f,
                            "Bounded ordinary Forward input did not prove the arrival world is operable");
                        require(game.m_gameSession.currentMapFileName() == leg.mapName,
                            "Normal arrival movement left the expected map");
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), expectedAfter);
                        steps << "ordinary-movement\t" << state.loopLeg + 1 << '\t'
                              << world->partyX() << '\t' << world->partyY() << '\t' << world->partyFootZ()
                              << "\tfrom=" << state.legStartX << ',' << state.legStartY << ',' << state.legStartZ
                              << " distance=" << state.legWalkedDistance << " input=Forward\n";
                        require(game.quickSaveToPath(
                                absoluteOutput / (std::string(leg.label) + "-arrived.oysav"),
                                "三大陆卷轴抵达", {}),
                            "Cannot save the operable scroll arrival");
                        captureRequest = prefix + "-arrived";
                        state.saved = true;
                        advancePhase(state, 11);
                    }
                    else if (state.phase == 11 && captured.contains(prefix + "-arrived"))
                    {
                        if (state.loopLeg + 1 == legs.size())
                        {
                            state.complete = true;
                        }
                        else
                        {
                            ++state.loopLeg;
                            advancePhase(state, 0);
                        }
                    }
                }

                game.renderFrame(width, height, wheel, delta);

                if (captureRequest.has_value())
                {
                    require(!pendingCaptures.contains(*captureRequest) && !captured.contains(*captureRequest),
                        "Screenshot token was requested more than once: " + *captureRequest);
                    pendingCaptures.emplace(*captureRequest, PendingCapture{frames});
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureRequest->c_str());
                }
                for (auto iterator = pendingCaptures.begin(); iterator != pendingCaptures.end();)
                {
                    std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                        Engine::BgfxContext::consumeScreenshot(iterator->first);
                    if (!capture.has_value())
                    {
                        require(frames - iterator->second.requestedFrame <= 300,
                            "Metal screenshot readback timed out: " + iterator->first);
                        ++iterator;
                        continue;
                    }
                    SDL_Surface *surface = SDL_CreateSurfaceFrom(
                        int(capture->width), int(capture->height), SDL_PIXELFORMAT_BGRA32,
                        capture->bgraPixels.data(), int(capture->width * 4));
                    require(surface != nullptr, "Cannot wrap Metal screenshot pixels");
                    const bool written = SDL_SaveBMP(
                        surface, (absoluteOutput / (iterator->first + ".bmp")).c_str());
                    SDL_DestroySurface(surface);
                    require(written, "Cannot write Metal screenshot: " + iterator->first);
                    std::cout << "Captured " << iterator->first << ": "
                              << capture->width << 'x' << capture->height << '\n';
                    captured.insert(iterator->first);
                    iterator = pendingCaptures.erase(iterator);
                }

                const bool timedOut = frames >= 18000 || SDL_GetTicks() - startedTicks >= 300000;
                if (state.complete || timedOut)
                {
                    if (timedOut && !state.complete)
                        game.quickSaveToPath(
                            absoluteOutput / "timeout-state.oysav", "三大陆卷轴超时现场", {});
                    require(state.complete, "Three-continent scroll capture exceeded its bounded timeout");
                    require(captured == expectedCaptures,
                        "Three-continent scroll capture did not receive every screenshot acknowledgement");
                    require(state.saved, "Three-continent scroll capture did not write its final save");
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                game.shutdownApplication();
            },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });

        const int result = application.run();
        require(result == 0 && state.complete && state.saved && captured == expectedCaptures,
            "Three-continent scroll capture process did not complete");
        std::cout << "Three-continent scroll capture passed: MM6->MM7->MM8->MM6, frames="
                  << frames << " captures=" << captured.size()
                  << "; one Metal process, three ordinary held-item/doll uses, three pointer selections.\n";
        return 0;
    }

    static int runScrollDestination(
        const std::filesystem::path &runtimeRoot,
        const std::filesystem::path &sourceSave,
        const std::filesystem::path &output,
        uint32_t targetDay,
        const std::string &destinationId,
        const std::string &expectedMap,
        const std::string &expectedWorld,
        const std::string &expectedIcon)
    {
        require(std::filesystem::exists(runtimeRoot), "Runtime root does not exist");
        require(std::filesystem::is_regular_file(sourceSave), "Input save does not exist");
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        require(!destinationId.empty() && !expectedMap.empty()
                && !expectedWorld.empty() && !expectedIcon.empty(),
            "Scroll destination specification contains an empty field");
        std::filesystem::create_directories(output);
        const std::filesystem::path absoluteOutput = std::filesystem::canonical(output);
        const std::filesystem::path absoluteSource = std::filesystem::canonical(sourceSave);
        const std::filesystem::path input =
            makeScrollDestinationFixture(absoluteSource, absoluteOutput, targetDay);
        const std::optional<GameSaveData> inputData = loadSave(input);
        require(inputData->mapFileName != expectedMap,
            "Representative destination validation requires a different source map to prove an actual load");
        const std::string sourceMap = inputData->mapFileName;

        std::filesystem::current_path(absoluteOutput);
        std::ofstream steps(absoluteOutput / "steps.tsv");
        steps.exceptions(std::ios::badbit | std::ios::failbit);
        steps << "kind\tindex\tx\ty\tz\tdetail\n";

        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.gameplayTrace = true;
        settings.gameplayTraceAppend = false;
        settings.gameplayTraceFile = (absoluteOutput / "gameplay.trace").string();
        settings.assetRoot = runtimeRoot.string();
        settings.windowMode = WindowMode::Windowed;
        std::string settingsError;
        require(saveGameSettings(absoluteOutput / "settings.ini", settings, settingsError),
            "Cannot write scroll-destination settings: " + settingsError);

        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM representative scroll destination validation";
        config.assetRoot = runtimeRoot.string();
        config.activeWorldId = "mm6";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;

        GameApplication game(config);
        CaptureState state;
        uint32_t frames = 0;
        uint64_t startedTicks = 0;
        std::map<std::string, PendingCapture> pendingCaptures;
        std::set<std::string> captured;
        const std::set<std::string> expectedCaptures = {"selection", "arrived"};

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                return game.loadGameData(files);
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal,
                    "Representative scroll capture requires the Metal backend");
                if (!game.initializeRenderer() || !game.loadSessionFromPath(input)) return false;
                require(game.m_gameSession.currentMapFileName() == sourceMap,
                    "Rendered scroll fixture did not begin on the selected normal MM6 source map");
                requireScrollFixtureLoaded(game.m_gameSession, 1, targetDay);
                require(game.quickSaveToPath(
                        absoluteOutput / "loaded-before-use.oysav", "单目的地卷轴稳定前置", {}),
                    "Cannot preserve the loaded single-scroll boundary");
                state.stableParty = game.m_gameSession.gameplayScreenRuntime().partyReadOnly()->snapshot();
                state.stableGlobals = game.m_gameSession.namedGlobalVars();
                IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                require(world != nullptr, "Loaded single-scroll fixture has no outdoor world");
                steps << "loaded-fixture\t0\t" << world->partyX() << '\t' << world->partyY() << '\t'
                      << world->partyFootZ() << "\tmap=" << sourceMap << " world=mm6 day=" << targetDay
                      << " item190=1 sp=0 recovery=0\n";
                startedTicks = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (!isPhysicalInputEvent(event)) game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                ++frames;
                ++state.phaseFrame;
                std::optional<std::string> captureRequest;
                GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                Party *pParty = screen.party();
                require(pParty != nullptr, "Representative scroll capture lost the party");

                if (!state.complete)
                {
                    if (state.phase == 0 && state.phaseFrame >= 30)
                    {
                        requireScrollFixtureLoaded(game.m_gameSession, 1, targetDay);
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), 1);
                        require(game.quickSaveToPath(
                                absoluteOutput / "before-use.oysav", "单目的地卷轴使用前", {}),
                            "Cannot save the pre-scroll-use boundary");
                        openMemberInventory(game.m_gameSession, width, height, 0);
                        advancePhase(state, 1);
                    }
                    else if (state.phase == 1 && state.phaseFrame >= 8)
                    {
                        require(screen.characterScreen().open
                                && screen.selectedCharacterScreenSourceIndex() == 0,
                            "Controlled inventory closed before item190 pointer selection");
                        pickInventoryItem(
                            game.m_gameSession, width, height, 0, DimensionDoorScrollItemId);
                        require(pParty->inventoryItemCount(DimensionDoorScrollItemId) == 0
                                && screen.heldInventoryItem().active,
                            "Picking item190 did not move the independent scroll to the hand");
                        steps << "scroll-held\t0\t0\t0\t0\tinventory=0 held=190\n";
                        advancePhase(state, 2);
                    }
                    else if (state.phase == 2 && state.phaseFrame >= 2)
                    {
                        useHeldItemOnDoll(game.m_gameSession, width, height);
                        require(pParty->inventoryItemCount(DimensionDoorScrollItemId) == 0,
                            "Ordinary item190 use did not consume exactly one scroll");
                        require(pParty->members()[0].spellPoints == 0
                                && pParty->members()[0].recoverySecondsRemaining == 0.0f,
                            "Held item190 use changed SP or recovery");
                        require(screen.utilitySpellOverlayReadOnly().active
                                && screen.utilitySpellOverlayReadOnly().mode
                                    == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor
                                && !screen.utilitySpellOverlayReadOnly().spendMana
                                && !screen.utilitySpellOverlayReadOnly().applyRecovery,
                            "Held item190 did not open the free Dimension Door selection page");
                        steps << "scroll-used\t0\t0\t0\t0\titem190=0 sp=0 recovery=0"
                                 " ordinary_doll_pointer=1\n";
                        advancePhase(state, 3);
                    }
                    else if (state.phase == 3 && state.phaseFrame >= 4)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Dimension Door selection lost the active world");
                        requireDimensionDoorTarget(
                            screen, *world, targetDay, destinationId, expectedMap, expectedIcon);
                        hoverDestination(game, width, height, destinationId);
                        captureRequest = "selection";
                        advancePhase(state, 4);
                    }
                    else if (state.phase == 4 && captured.contains("selection"))
                    {
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), 0);
                        require(game.quickSaveToPath(
                                absoluteOutput / "before-selection.oysav", "单目的地卷轴选择前", {}),
                            "Cannot save the destination-selection boundary");
                        clickDestination(game.m_gameSession, width, height, destinationId);
                        require(pParty->members()[0].spellPoints == 0
                                && pParty->members()[0].recoverySecondsRemaining == 0.0f,
                            "Scroll destination pointer selection changed SP or recovery");
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr && world->eventRuntimeState() != nullptr,
                            "Scroll destination request lost the active event runtime");
                        const auto &move = world->eventRuntimeState()->pendingMapMove;
                        require(move.has_value() && move->mapName.has_value()
                                && *move->mapName == expectedMap
                                && move->x == 0 && move->y == 0 && move->z == 0
                                && move->useMapStartPosition
                                && !move->directionDegrees.has_value()
                                && !move->lookAngleDegrees.has_value(),
                            "Scroll destination request lost the zero/map-start cross-world contract");
                        steps << "portal-request\t0\t0\t0\t0\tdestination=" << destinationId
                              << " map=" << expectedMap << " world=" << expectedWorld
                              << " icon=" << expectedIcon << " map_start=1 sp=0 recovery=0\n";
                        advancePhase(state, 5);
                    }
                    else if (state.phase == 5
                             && game.m_gameSession.currentMapFileName() == expectedMap)
                    {
                        const auto &selectedMap = game.m_gameDataLoader.getSelectedMap();
                        require(selectedMap.has_value() && selectedMap->map.worldId == expectedWorld,
                            "Actual scroll arrival world differs from the checkpoint-121 target row");
                        requireAuthoredOutdoorStart(game, expectedMap, 0, steps);
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), 0);
                        require(game.quickSaveToPath(
                                absoluteOutput / "map-start.oysav", "单目的地原始落点", {}),
                            "Cannot save the engine-resolved map-start boundary");
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Scroll arrival lost the outdoor world runtime");
                        state.legStartX = world->partyX();
                        state.legStartY = world->partyY();
                        state.legStartZ = world->partyFootZ();
                        advancePhase(state, 6);
                    }
                    else if (state.phase == 6 && state.phaseFrame >= 8)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr
                                && std::isfinite(world->partyX()) && std::isfinite(world->partyY())
                                && std::isfinite(world->partyFootZ()),
                            "Normal landing/collision frames produced a non-finite party position");
                        steps << "landing-collision\t0\t" << world->partyX() << '\t' << world->partyY()
                              << '\t' << world->partyFootZ() << "\tmap_start=" << state.legStartX << ','
                              << state.legStartY << ',' << state.legStartZ
                              << " vertical_delta=" << world->partyFootZ() - state.legStartZ
                              << " ordinary_gravity_collision_frames=8\n";
                        selectPartyMember(game.m_gameSession, width, height, 1);
                        require(pParty->activeMemberIndex() == 1,
                            "Ordinary party portrait selection did not select member 1");
                        selectPartyMember(game.m_gameSession, width, height, 0);
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), 0);
                        steps << "basic-selection\t0\t0\t0\t0\tportrait_member1_then_member0=1\n";
                        state.legStartX = world->partyX();
                        state.legStartY = world->partyY();
                        state.legStartZ = world->partyFootZ();
                        state.legWalkedDistance = 0.0f;
                        advancePhase(state, 7);
                    }
                    else if (state.phase == 7)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Normal arrival movement lost the active world");
                        const float x = world->partyX();
                        const float y = world->partyY();
                        pressAction(game.m_gameSession, width, height, KeyboardAction::Forward, 0.025f);
                        state.legWalkedDistance += std::hypot(world->partyX() - x, world->partyY() - y);
                        if (state.phaseFrame >= 8) advancePhase(state, 8);
                    }
                    else if (state.phase == 8 && state.phaseFrame >= 8)
                    {
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr && state.legWalkedDistance >= 8.0f,
                            "Bounded ordinary Forward input did not prove the arrival world is operable");
                        require(game.m_gameSession.currentMapFileName() == expectedMap,
                            "Normal arrival movement left the expected map");
                        requireScrollPartyCore(state.stableParty, pParty->snapshot(), 0);
                        require(game.m_gameSession.namedGlobalVars() == state.stableGlobals,
                            "Representative scroll travel changed named globals");
                        steps << "ordinary-movement\t0\t" << world->partyX() << '\t' << world->partyY()
                              << '\t' << world->partyFootZ() << "\tfrom=" << state.legStartX << ','
                              << state.legStartY << ',' << state.legStartZ
                              << " distance=" << state.legWalkedDistance << " input=Forward\n";
                        require(game.quickSaveToPath(
                                absoluteOutput / "arrived.oysav", "单目的地卷轴抵达", {}),
                            "Cannot save the operable scroll arrival");
                        captureRequest = "arrived";
                        state.saved = true;
                        advancePhase(state, 9);
                    }
                    else if (state.phase == 9 && captured.contains("arrived"))
                    {
                        state.complete = true;
                    }
                }

                game.renderFrame(width, height, wheel, delta);
                if (captureRequest.has_value())
                {
                    require(!pendingCaptures.contains(*captureRequest) && !captured.contains(*captureRequest),
                        "Screenshot token was requested more than once: " + *captureRequest);
                    pendingCaptures.emplace(*captureRequest, PendingCapture{frames});
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureRequest->c_str());
                }
                for (auto iterator = pendingCaptures.begin(); iterator != pendingCaptures.end();)
                {
                    std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                        Engine::BgfxContext::consumeScreenshot(iterator->first);
                    if (!capture.has_value())
                    {
                        require(frames - iterator->second.requestedFrame <= 300,
                            "Metal screenshot readback timed out: " + iterator->first);
                        ++iterator;
                        continue;
                    }
                    SDL_Surface *surface = SDL_CreateSurfaceFrom(
                        int(capture->width), int(capture->height), SDL_PIXELFORMAT_BGRA32,
                        capture->bgraPixels.data(), int(capture->width * 4));
                    require(surface != nullptr, "Cannot wrap Metal screenshot pixels");
                    const bool written = SDL_SaveBMP(
                        surface, (absoluteOutput / (iterator->first + ".bmp")).c_str());
                    SDL_DestroySurface(surface);
                    require(written, "Cannot write Metal screenshot: " + iterator->first);
                    captured.insert(iterator->first);
                    std::cout << "Captured " << iterator->first << ": "
                              << capture->width << 'x' << capture->height << '\n';
                    iterator = pendingCaptures.erase(iterator);
                }

                const bool timedOut = frames >= 6000 || SDL_GetTicks() - startedTicks >= 180000;
                if (state.complete || timedOut)
                {
                    if (timedOut && !state.complete)
                        game.quickSaveToPath(
                            absoluteOutput / "timeout-state.oysav", "单目的地卷轴超时现场", {});
                    require(state.complete, "Representative scroll capture exceeded its bounded timeout");
                    require(captured == expectedCaptures,
                        "Representative scroll capture did not receive every screenshot acknowledgement");
                    require(state.saved, "Representative scroll capture did not write its arrival save");
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                game.shutdownApplication();
            },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });

        const int result = application.run();
        require(result == 0 && state.complete && state.saved && captured == expectedCaptures,
            "Representative scroll capture process did not complete");
        std::cout << "Representative scroll destination passed: day=" << targetDay
                  << " id=" << destinationId << " map=" << expectedMap
                  << " world=" << expectedWorld << " icon=" << expectedIcon
                  << " frames=" << frames << " captures=" << captured.size()
                  << "; ordinary held-item/doll use, pointer selection, collision frames,"
                     " portrait selection, movement, and save.\n";
        return 0;
    }

    static int runScrollReload(
        const std::filesystem::path &runtimeRoot,
        const std::filesystem::path &sourceSave,
        const std::filesystem::path &output)
    {
        require(std::filesystem::exists(runtimeRoot), "Runtime root does not exist");
        require(std::filesystem::is_regular_file(sourceSave), "Input save does not exist");
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        std::filesystem::create_directories(output);
        const std::filesystem::path absoluteOutput = std::filesystem::canonical(output);
        const std::filesystem::path input = std::filesystem::canonical(sourceSave);
        const std::optional<GameSaveData> inputData = loadSave(input);
        require(inputData->currentSceneKind == SceneKind::Outdoor && !inputData->mapFileName.empty(),
            "Scroll reload input is not an outdoor arrival");
        const int expectedScrollCount = snapshotInventoryItemCount(inputData->party, DimensionDoorScrollItemId);
        const uint32_t expectedDay =
            static_cast<uint32_t>(std::floor(inputData->savedGameMinutes / 1440.0f));
        const std::string mapLabel = std::filesystem::path(inputData->mapFileName).stem().string();

        std::filesystem::current_path(absoluteOutput);
        std::ofstream steps(absoluteOutput / "steps.tsv");
        steps.exceptions(std::ios::badbit | std::ios::failbit);
        steps << "kind\tindex\tx\ty\tz\tdetail\n";

        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.gameplayTrace = true;
        settings.gameplayTraceAppend = false;
        settings.gameplayTraceFile = (absoluteOutput / "gameplay.trace").string();
        settings.assetRoot = runtimeRoot.string();
        settings.windowMode = WindowMode::Windowed;
        std::string settingsError;
        require(saveGameSettings(absoluteOutput / "settings.ini", settings, settingsError),
            "Cannot write scroll-reload settings: " + settingsError);

        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM scroll arrival reload validation";
        config.assetRoot = runtimeRoot.string();
        config.activeWorldId = "mm6";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;

        GameApplication game(config);
        CaptureState state;
        uint32_t frames = 0;
        uint64_t startedTicks = 0;
        std::map<std::string, PendingCapture> pendingCaptures;
        std::set<std::string> captured;
        const std::set<std::string> expectedCaptures = {
            mapLabel + "-cold", mapLabel + "-warm", mapLabel + "-final"};

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                return game.loadGameData(files);
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal,
                    "Scroll reload capture requires the Metal backend");
                if (!game.initializeRenderer() || !game.loadSessionFromPath(input)) return false;
                require(game.m_gameSession.currentMapFileName() == inputData->mapFileName,
                    "Independent cold load restored a different scroll arrival map");
                requireScrollFixtureLoaded(game.m_gameSession, expectedScrollCount, expectedDay);
                state.fixtureParty = game.m_gameSession.gameplayScreenRuntime().partyReadOnly()->snapshot();
                state.stableGlobals = game.m_gameSession.namedGlobalVars();
                require(game.quickSaveToPath(
                        absoluteOutput / "cold-restored.oysav", "三大陆独立冷读档", {}),
                    "Cannot preserve the independent cold-restored state");
                startedTicks = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (!isPhysicalInputEvent(event)) game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                ++frames;
                ++state.phaseFrame;
                std::optional<std::string> captureRequest;
                GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                Party *pParty = screen.party();
                require(pParty != nullptr, "Independent scroll reload lost the party");

                if (!state.complete)
                {
                    if (state.phase == 0 && state.phaseFrame >= 30)
                    {
                        captureRequest = mapLabel + "-cold";
                        advancePhase(state, 1);
                    }
                    else if (state.phase == 1 && captured.contains(mapLabel + "-cold"))
                    {
                        require(game.quickLoadFromPath(input, true),
                            "Independent in-session warm reload failed");
                        advancePhase(state, 2);
                    }
                    else if (state.phase == 2 && state.phaseFrame >= 4)
                    {
                        require(game.m_gameSession.currentMapFileName() == inputData->mapFileName,
                            "Independent warm reload changed the arrival map");
                        requireScrollFixtureLoaded(game.m_gameSession, expectedScrollCount, expectedDay);
                        requireScrollReloadParty(state.fixtureParty, pParty->snapshot(), expectedScrollCount);
                        require(game.m_gameSession.namedGlobalVars() == state.stableGlobals,
                            "Independent warm reload changed named globals");
                        require(game.quickSaveToPath(
                                absoluteOutput / "warm-restored.oysav", "三大陆同会话回读", {}),
                            "Cannot preserve the warm-restored state");
                        captureRequest = mapLabel + "-warm";
                        advancePhase(state, 3);
                    }
                    else if (state.phase == 3 && captured.contains(mapLabel + "-warm")
                             && state.phaseFrame >= 1)
                    {
                        // Renderer production and screenshot acknowledgement run ordinary frames.
                        // Compare on the first callback after acknowledgement so this remains a
                        // persistence check rather than an unbounded live-simulation exercise.
                        requireScrollReloadParty(state.fixtureParty, pParty->snapshot(), expectedScrollCount);
                        require(game.m_gameSession.namedGlobalVars() == state.stableGlobals,
                            "Stable independent runtime changed named globals");
                        require(game.quickSaveToPath(
                                absoluteOutput / "final-readback.oysav", "三大陆最终回读", {}),
                            "Cannot write the final independent readback save");
                        IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                        require(world != nullptr, "Final independent reload lost its world");
                        steps << "reload-final\t0\t" << world->partyX() << '\t' << world->partyY()
                              << '\t' << world->partyFootZ() << "\tmap=" << inputData->mapFileName
                              << " item190=" << expectedScrollCount << " sp=0 recovery=0\n";
                        captureRequest = mapLabel + "-final";
                        state.saved = true;
                        advancePhase(state, 4);
                    }
                    else if (state.phase == 4 && captured.contains(mapLabel + "-final"))
                    {
                        state.complete = true;
                    }
                }

                game.renderFrame(width, height, wheel, delta);
                if (captureRequest.has_value())
                {
                    require(!pendingCaptures.contains(*captureRequest) && !captured.contains(*captureRequest),
                        "Screenshot token was requested more than once: " + *captureRequest);
                    pendingCaptures.emplace(*captureRequest, PendingCapture{frames});
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureRequest->c_str());
                }
                for (auto iterator = pendingCaptures.begin(); iterator != pendingCaptures.end();)
                {
                    std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                        Engine::BgfxContext::consumeScreenshot(iterator->first);
                    if (!capture.has_value())
                    {
                        require(frames - iterator->second.requestedFrame <= 300,
                            "Metal screenshot readback timed out: " + iterator->first);
                        ++iterator;
                        continue;
                    }
                    SDL_Surface *surface = SDL_CreateSurfaceFrom(
                        int(capture->width), int(capture->height), SDL_PIXELFORMAT_BGRA32,
                        capture->bgraPixels.data(), int(capture->width * 4));
                    require(surface != nullptr, "Cannot wrap Metal screenshot pixels");
                    const bool written = SDL_SaveBMP(
                        surface, (absoluteOutput / (iterator->first + ".bmp")).c_str());
                    SDL_DestroySurface(surface);
                    require(written, "Cannot write Metal screenshot: " + iterator->first);
                    captured.insert(iterator->first);
                    std::cout << "Captured " << iterator->first << ": "
                              << capture->width << 'x' << capture->height << '\n';
                    iterator = pendingCaptures.erase(iterator);
                }

                const bool timedOut = frames >= 3600 || SDL_GetTicks() - startedTicks >= 120000;
                if (state.complete || timedOut)
                {
                    if (timedOut && !state.complete)
                        game.quickSaveToPath(
                            absoluteOutput / "timeout-state.oysav", "三大陆独立重载超时现场", {});
                    require(state.complete, "Independent scroll reload exceeded its bounded timeout");
                    require(captured == expectedCaptures,
                        "Independent scroll reload did not receive every screenshot acknowledgement");
                    require(state.saved, "Independent scroll reload did not write its final readback");
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                game.shutdownApplication();
            },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });

        const int result = application.run();
        require(result == 0 && state.complete && state.saved && captured == expectedCaptures,
            "Independent scroll reload process did not complete");
        std::cout << "Independent scroll arrival reload passed: map=" << inputData->mapFileName
                  << " captures=" << captured.size() << " frames=" << frames << '\n';
        return 0;
    }

    static int run(
        const std::filesystem::path &runtimeRoot,
        const std::filesystem::path &sourceSave,
        const std::filesystem::path &output,
        CaptureMode mode)
    {
        if (mode == CaptureMode::ScrollLoop)
            return runScrollLoop(runtimeRoot, sourceSave, output);
        if (mode == CaptureMode::ScrollReload)
            return runScrollReload(runtimeRoot, sourceSave, output);
        require(std::filesystem::exists(runtimeRoot), "Runtime root does not exist");
        require(std::filesystem::is_regular_file(sourceSave), "Input save does not exist");
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        std::filesystem::create_directories(output);
        const std::filesystem::path absoluteOutput = std::filesystem::canonical(output);
        const std::filesystem::path absoluteSource = std::filesystem::canonical(sourceSave);

        std::filesystem::path input = absoluteSource;
        if (mode == CaptureMode::ToJadame)
        {
            input = makeControlledCasterFixture(absoluteSource, absoluteOutput);
        }
        const std::optional<GameSaveData> inputData = loadSave(input);
        requireControlledGlobals(inputData->namedGlobalVars);
        requireExpectedQuestBits(inputData->party, mode != CaptureMode::ToJadame);
        const bool reloadFromEnroth = mode == CaptureMode::ReloadOnly && inputData->mapFileName == "outd2.odm";
        const bool inputMapMatches =
            (mode == CaptureMode::ToJadame && inputData->mapFileName == "oute3.odm")
            || (mode == CaptureMode::ToEnroth && inputData->mapFileName == "out01.odm")
            || (mode == CaptureMode::ReloadOnly
                && (inputData->mapFileName == "out01.odm" || reloadFromEnroth));
        require(inputMapMatches, "Input save map does not match the selected process mode");

        std::filesystem::current_path(absoluteOutput);
        std::ofstream steps(absoluteOutput / "steps.tsv");
        steps.exceptions(std::ios::badbit | std::ios::failbit);
        steps << "kind\tindex\tx\ty\tz\tdetail\n";

        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.gameplayTrace = true;
        settings.gameplayTraceAppend = false;
        settings.gameplayTraceFile = (absoluteOutput / "gameplay.trace").string();
        settings.assetRoot = runtimeRoot.string();
        settings.windowMode = WindowMode::Windowed;
        std::string settingsError;
        require(saveGameSettings(absoluteOutput / "settings.ini", settings, settingsError),
            "Cannot write capture settings: " + settingsError);

        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM cross-world travel validation";
        config.assetRoot = runtimeRoot.string();
        config.activeWorldId = "mm6";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;

        static const std::vector<Waypoint> enrothFountainRoute = {
            {-9020.0f, -8828.0f}, {-9406.0f, -8450.0f}, {-9791.0f, -8065.0f},
            {-10304.0f, -7552.0f}, {-10813.0f, -7043.0f}, {-11196.0f, -7040.0f},
            {-10945.0f, -7039.89f}, {-10816.0f, -6912.24f}, {-10816.0f, -6788.49f},
            {-10816.0f, -6659.11f}, {-10816.0f, -6529.74f}, {-10816.0f, -6400.36f},
            {-10816.0f, -6276.61f}, {-10816.0f, -6147.24f}, {-10941.2f, -6017.02f},
            {-11069.4f, -5890.59f}, {-11196.7f, -5763.33f}, {-11326.4f, -5633.56f},
            {-11453.7f, -5506.27f}, {-11328.7f, -5376.75f, 32.0f},
        };
        static const std::vector<Waypoint> jadameTempleRoute = {
            {4413.0f, 7805.0f}, {4925.0f, 8189.0f}, {6463.0f, 8575.0f},
            {7231.0f, 8831.0f}, {7744.0f, 9088.0f}, {8125.0f, 9341.0f},
            {8256.0f, 9472.0f}, {8894.0f, 9472.0f}, {9298.0f, 9450.0f, 32.0f},
        };
        static const std::vector<Waypoint> jadameDimensionDoorRoute = {
            {8894.0f, 9472.0f}, {8256.0f, 9472.0f}, {8125.0f, 9341.0f},
            {7744.0f, 9088.0f}, {7231.0f, 8831.0f}, {6463.0f, 8575.0f},
            {4925.0f, 8189.0f}, {4413.0f, 7805.0f}, {3560.0f, 7696.0f},
            {3776.0f, 6784.0f}, {3264.0f, 6016.0f}, {2240.0f, 4864.0f},
            {1600.0f, 4224.0f}, {960.0f, 3584.0f}, {320.0f, 2944.0f},
            {-256.0f, 2304.0f, 32.0f},
        };
        static constexpr std::array<const char *, 3> previewIds = {
            "TPGlobal:0", "TPGlobal:2", "TPGlobal:4"};
        static constexpr std::array<const char *, 3> previewTokens = {
            "dimension-door-preview-jadame",
            "dimension-door-preview-antagarich",
            "dimension-door-preview-enroth"};

        GameApplication game(config);
        CaptureState state;
        uint32_t frames = 0;
        uint64_t startedTicks = 0;
        std::map<std::string, PendingCapture> pendingCaptures;
        std::set<std::string> captured;
        const std::set<std::string> expectedCaptures = mode == CaptureMode::ToJadame
            ? std::set<std::string>{
                "fountain-activated",
                "dimension-door-preview-jadame",
                "dimension-door-preview-antagarich",
                "dimension-door-preview-enroth",
                "jadame-arrived"}
            : mode == CaptureMode::ToEnroth
                ? std::set<std::string>{
                    "temple-before-heal",
                    "temple-after-heal",
                    "dimension-door-tile-activated",
                    "dimension-door-return-preview",
                    "enroth-arrived"}
                : std::set<std::string>{reloadFromEnroth ? "outd2-cold-reload" : "out01-cold-reload"};

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                return game.loadGameData(files);
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal,
                    "Cross-world capture requires the Metal backend");
                if (!game.initializeRenderer() || !game.loadSessionFromPath(input)) return false;
                require(game.m_gameSession.currentMapFileName() == inputData->mapFileName,
                    "Rendered application loaded a different input map");
                if (reloadFromEnroth) requireReturnedCasterLoaded(game.m_gameSession);
                else
                {
                    requireControlledCasterLoaded(
                        game.m_gameSession,
                        mode == CaptureMode::ToJadame ? 20 : 0,
                        mode == CaptureMode::ToJadame ? 2 : 3);
                }
                require(game.quickSaveToPath(absoluteOutput / "cold-restored.oysav", "跨洲冷读档", {}),
                    "Cannot preserve cold-restored state");
                if (mode != CaptureMode::ReloadOnly)
                {
                    require(game.quickLoadFromPath(input, true), "Controlled caster warm reload failed");
                    if (mode == CaptureMode::ToJadame)
                        requireControlledCasterLoaded(game.m_gameSession, 20, 2);
                    else
                        requireControlledCasterLoaded(game.m_gameSession, 0, 3);
                    require(game.quickSaveToPath(absoluteOutput / "warm-restored.oysav", "跨洲同会话读档", {}),
                        "Cannot preserve warm-restored controlled caster");
                    state.warmReloaded = true;
                    state.fixtureParty = game.m_gameSession.gameplayScreenRuntime().partyReadOnly()->snapshot();
                    state.stableGlobals = game.m_gameSession.namedGlobalVars();
                }
                startedTicks = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (!isPhysicalInputEvent(event)) game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                ++frames;
                ++state.phaseFrame;
                std::optional<std::string> captureRequest;
                GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();

                if (!state.complete)
                {
                    if (mode == CaptureMode::ReloadOnly && !state.warmReloaded)
                    {
                        const std::string token = reloadFromEnroth ? "outd2-cold-reload" : "out01-cold-reload";
                        if (state.phase == 0 && state.phaseFrame >= 30)
                        {
                            captureRequest = token;
                            advancePhase(state, 1);
                        }
                        else if (state.phase == 1 && captured.contains(token))
                        {
                            require(game.quickLoadFromPath(input, true), "Independent in-session warm reload failed");
                            if (reloadFromEnroth) requireReturnedCasterLoaded(game.m_gameSession);
                            else requireControlledCasterLoaded(game.m_gameSession, 0, 3);
                            require(game.quickSaveToPath(absoluteOutput / "warm-restored.oysav", "跨洲同会话读档", {}),
                                "Cannot preserve warm-restored state");
                            state.warmReloaded = true;
                            state.stableParty = screen.partyReadOnly()->snapshot();
                            state.stableGlobals = game.m_gameSession.namedGlobalVars();
                            require(game.quickSaveToPath(absoluteOutput / "final-readback.oysav", "跨洲最终回读", {}),
                                "Cannot write final readback save");
                            requireStableParty(inputData->party, screen.partyReadOnly()->snapshot(), true);
                            require(game.m_gameSession.namedGlobalVars() == inputData->namedGlobalVars,
                                "Final independent reload changed named globals");
                            require(game.m_gameSession.currentMapFileName() == inputData->mapFileName,
                                "Independent cross-world reload changed maps");
                            requireEnrothMapVarUnchanged(absoluteOutput / "final-readback.oysav");
                            state.saved = true;
                            state.complete = true;
                        }
                    }
                    else if (mode == CaptureMode::ToJadame)
                    {
                        if (state.phase == 0 && state.phaseFrame >= 20)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr && static_cast<uint32_t>(std::floor(world->gameMinutes() / 1440.0)) == 23,
                                "Checkpoint-113 fixture is not on the audited day 23 before normal waiting");
                            state.minutesBeforeWaiting = world->gameMinutes();
                            state.foodBeforeWaiting = screen.partyReadOnly()->food();
                            require(game.quickSaveToPath(absoluteOutput / "before-dawn-wait.oysav", "跨洲黎明等待前", {}),
                                "Cannot save the pre-wait boundary");
                            pressAction(game.m_gameSession, width, height, KeyboardAction::Rest);
                            require(screen.restScreenState().active,
                                "Normal Rest action did not open the rest screen");
                            advancePhase(state, 1);
                        }
                        else if (state.phase == 1 && state.phaseFrame >= 4)
                        {
                            clickLayout(game.m_gameSession, width, height, "RestButtonUntilDawn");
                            require(screen.restScreenState().mode == GameplayUiController::RestMode::Wait,
                                "Normal wait-until-dawn button did not begin waiting");
                            advancePhase(state, 2);
                        }
                        else if (state.phase == 2)
                        {
                            require(state.phaseFrame < 1800, "Normal wait until day-24 dawn did not finish");
                            if (screen.restScreenState().mode != GameplayUiController::RestMode::None)
                            {
                                // The ordinary render loop advances the wait below.
                            }
                            else
                            {
                                IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                                require(world != nullptr, "Normal dawn wait lost the active outdoor world");
                                state.minutesAfterWaiting = world->gameMinutes();
                                require(state.minutesAfterWaiting > state.minutesBeforeWaiting
                                        && static_cast<uint32_t>(std::floor(state.minutesAfterWaiting / 1440.0)) == 24,
                                    "Normal dawn wait did not naturally reach day 24");
                                require(screen.partyReadOnly()->food() == state.foodBeforeWaiting,
                                    "Wait until dawn unexpectedly consumed food");
                                if (screen.restScreenState().active)
                                {
                                    clickLayout(game.m_gameSession, width, height, "RestButtonExit");
                                }
                                advancePhase(state, 3);
                            }
                        }
                        else if (state.phase == 3 && state.phaseFrame >= 4)
                        {
                            require(!screen.restScreenState().active, "Rest screen remained open after normal dawn wait");
                            requireControlledCasterLoaded(game.m_gameSession, 20, 2);
                            require(game.quickSaveToPath(absoluteOutput / "after-dawn-wait.oysav", "跨洲黎明等待后", {}),
                                "Cannot save the post-wait boundary");
                            state.stableParty = screen.partyReadOnly()->snapshot();
                            state.stableGlobals = game.m_gameSession.namedGlobalVars();
                            std::cout << "Normal dawn wait: minutes=" << state.minutesBeforeWaiting << "->"
                                      << state.minutesAfterWaiting << " day=24 food=" << state.foodBeforeWaiting << '\n';
                            for (size_t index = 0; index < state.fixtureParty.members.size(); ++index)
                            {
                                std::cout << "Wait member " << index << ": HP="
                                          << state.fixtureParty.members[index].health << "->"
                                          << state.stableParty.members[index].health << " conditions="
                                          << state.fixtureParty.members[index].conditions.to_ullong() << "->"
                                          << state.stableParty.members[index].conditions.to_ullong() << '\n';
                            }
                            advancePhase(state, 4);
                        }
                        else if (state.phase == 4 && state.phaseFrame >= 20)
                        {
                            require(game.m_gameSession.currentMapFileName() == "oute3.odm",
                                "Fountain route left oute3 before activation");
                            if (walkRoute(game, width, height, state, enrothFountainRoute, steps))
                            {
                                advancePhase(state, 5);
                            }
                        }
                        else if (state.phase == 5 && state.phaseFrame >= 4)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr, "Fountain Use lost the active outdoor world");
                            constexpr float fountainX = -11168.0f;
                            constexpr float fountainY = -5376.0f;
                            constexpr float fountainZ = 192.0f;
                            const float dx = fountainX - world->partyX();
                            const float dy = fountainY - world->partyY();
                            const auto &movement = game.m_activeWorldManifest.partyMovement;
                            const float eyeHeight = movement.declared ? movement.eyeHeight : 160.0f;
                            const float yaw = std::atan2(dy, dx);
                            const float pitch = std::atan2(
                                fountainZ - world->partyFootZ() - eyeHeight,
                                std::hypot(dx, dy));
                            game.m_outdoorGameView.setCameraAngles(yaw, pitch);
                            steps << "fountain-aim\t0\t" << world->partyX() << '\t' << world->partyY()
                                  << '\t' << world->partyFootZ() << "\tyaw=" << yaw
                                  << " pitch=" << pitch << '\n';
                            pressAction(game.m_gameSession, width, height, KeyboardAction::Use);
                            std::cout << "Fountain Use: status=\"" << screen.statusBarEventText() << "\"\n";
                            advancePhase(state, 6);
                        }
                        else if (state.phase == 6 && state.phaseFrame >= 3)
                        {
                            require(screen.shouldTownPortalCastOpenDimensionDoor(),
                                "Normal Use on oute3 model 18 face 24 did not activate Dimension Door");
                            captureRequest = "fountain-activated";
                            advancePhase(state, 7);
                        }
                        else if (state.phase >= 7 && state.phase <= 10)
                        {
                            openDimensionDoor(game, width, height, state);
                        }
                        else if (state.phase == 11 && state.phaseFrame >= 4)
                        {
                            require(game.m_gameSession.activeWorldRuntime() != nullptr,
                                "Dimension Door lost the active outdoor world");
                            requireDimensionDoor(screen, *game.m_gameSession.activeWorldRuntime());
                            advancePhase(state, 12);
                        }
                        else if (state.phase == 12 && state.phaseFrame >= 4)
                        {
                            if (state.previewIndex == previewIds.size())
                            {
                                if (captured.contains(previewTokens.back())) advancePhase(state, 13);
                            }
                            else if (state.previewIndex == 0
                                || captured.contains(previewTokens[state.previewIndex - 1]))
                            {
                                hoverDestination(game, width, height, previewIds[state.previewIndex]);
                                captureRequest = previewTokens[state.previewIndex];
                                ++state.previewIndex;
                                state.phaseFrame = 0;
                            }
                        }
                        else if (state.phase == 13 && state.phaseFrame >= 4)
                        {
                            const Party::Snapshot beforeSelection = screen.partyReadOnly()->snapshot();
                            requireStableParty(state.stableParty, beforeSelection, false);
                            const Character &beforeCaster = beforeSelection.members[2];
                            require(beforeCaster.spellPoints == 20
                                    && beforeCaster.recoverySecondsRemaining == 0.0f,
                                "Controlled caster was not at the audited selection boundary");
                            require(game.quickSaveToPath(
                                    absoluteOutput / "before-portal-selection.oysav", "跨洲选择目的地前", {}),
                                "Cannot save the pre-selection boundary");
                            state.spellPointsBeforePortal = beforeCaster.spellPoints;
                            clickDestination(game.m_gameSession, width, height, "TPGlobal:0");
                            const Character &afterCaster = screen.partyReadOnly()->members()[2];
                            require(afterCaster.spellPoints == state.spellPointsBeforePortal - 20,
                                "Ordinary destination selection did not spend 20 SP");
                            require(afterCaster.recoverySecondsRemaining > 0.0f,
                                "Ordinary destination selection did not start recovery");
                            state.recoveryAtPortalRequest = afterCaster.recoverySecondsRemaining;
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr && world->eventRuntimeState() != nullptr,
                                "Jadame request lost the active event runtime");
                            const auto &pendingMove = world->eventRuntimeState()->pendingMapMove;
                            std::cout << "World pending move: present=" << pendingMove.has_value();
                            if (pendingMove)
                            {
                                std::cout << " map=" << pendingMove->mapName.value_or("<none>")
                                          << " xyz=" << pendingMove->x << ',' << pendingMove->y << ',' << pendingMove->z
                                          << " map_start=" << pendingMove->useMapStartPosition
                                          << " direction=";
                                if (pendingMove->directionDegrees) std::cout << *pendingMove->directionDegrees;
                                else std::cout << "<none>";
                                std::cout << " look=";
                                if (pendingMove->lookAngleDegrees) std::cout << *pendingMove->lookAngleDegrees;
                                else std::cout << "<none>";
                            }
                            std::cout << '\n';
                            require(pendingMove.has_value() && pendingMove->mapName.has_value()
                                    && *pendingMove->mapName == "out01.odm"
                                    && pendingMove->x == 0 && pendingMove->y == 0 && pendingMove->z == 0
                                    && pendingMove->useMapStartPosition
                                    && !pendingMove->directionDegrees.has_value()
                                    && !pendingMove->lookAngleDegrees.has_value(),
                                "Jadame request did not preserve the cross-map zero/map-start contract");
                            steps << "portal-request\t0\t0\t0\t0\tmap=out01.odm map_start=1 sp="
                                  << afterCaster.spellPoints << " recovery="
                                  << state.recoveryAtPortalRequest << '\n';
                            advancePhase(state, 14);
                        }
                        else if (state.phase == 14 && game.m_gameSession.currentMapFileName() == "out01.odm")
                        {
                            advancePhase(state, 15);
                        }
                        else if (state.phase == 15 && state.phaseFrame >= 60)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr && std::hypot(world->partyX() - 3560.0f, world->partyY() - 7696.0f) < 8.0f,
                                "Jadame arrival did not use the actual out01 map start");
                            requireTownPortalCost(
                                state.stableParty,
                                screen.partyReadOnly()->snapshot(),
                                state.recoveryAtPortalRequest);
                            require(game.m_gameSession.namedGlobalVars() == state.stableGlobals,
                                "Jadame arrival changed named globals");
                            require(game.quickSaveToPath(absoluteOutput / "jadame-arrived.oysav", "跨洲抵达匕伤岛", {}),
                                "Cannot save Jadame arrival");
                            requireEnrothMapVarUnchanged(absoluteOutput / "jadame-arrived.oysav");
                            captureRequest = "jadame-arrived";
                            state.saved = true;
                            advancePhase(state, 16);
                        }
                        else if (state.phase == 16 && captured.contains("jadame-arrived"))
                        {
                            state.complete = true;
                        }
                    }
                    else if (mode == CaptureMode::ToEnroth)
                    {
                        if (state.phase == 0 && state.phaseFrame >= 20)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr
                                    && static_cast<uint32_t>(std::floor(world->gameMinutes() / 1440.0)) == 24,
                                "Jadame return input is not on the audited day 24");
                            require(game.m_gameSession.currentMapFileName() == "out01.odm",
                                "Return route did not begin at the accepted out01 arrival");
                            const Character &caster = screen.partyReadOnly()->members()[2];
                            require(state.phaseFrame < 600,
                                "Returned caster recovery did not expire during ordinary gameplay frames");
                            if (caster.recoverySecondsRemaining > 0.0f)
                            {
                                // The outbound cast save legitimately carries a short recovery. Let the ordinary
                                // application frame loop make the member selectable before clicking the portrait.
                                game.renderFrame(width, height, wheel, delta);
                                return;
                            }
                            selectPartyMember(game.m_gameSession, width, height, 2);
                            state.fixtureParty = screen.partyReadOnly()->snapshot();
                            require(state.fixtureParty.members[2].spellPoints == 0
                                    && state.fixtureParty.members[2].conditions.to_ullong() == 64,
                                "Return route did not begin with the depleted, poisoned Sorcerer");
                            steps << "portrait-select\t2\t" << world->partyX() << '\t' << world->partyY()
                                  << '\t' << world->partyFootZ() << "\tactive=2\n";
                            advancePhase(state, 1);
                        }
                        else if (state.phase == 1 && state.phaseFrame >= 4)
                        {
                            if (walkRoute(game, width, height, state, jadameTempleRoute, steps))
                            {
                                advancePhase(state, 2);
                            }
                        }
                        else if (state.phase == 2 && state.phaseFrame >= 4)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr, "Temple interaction lost the active outdoor world");
                            constexpr float templeX = 9516.0f;
                            constexpr float templeY = 9450.0f;
                            constexpr float templeZ = 1920.0f;
                            const float dx = templeX - world->partyX();
                            const float dy = templeY - world->partyY();
                            const auto &movement = game.m_activeWorldManifest.partyMovement;
                            const float eyeHeight = movement.declared ? movement.eyeHeight : 160.0f;
                            const float yaw = std::atan2(dy, dx);
                            const float pitch = std::atan2(
                                templeZ - world->partyFootZ() - eyeHeight,
                                std::hypot(dx, dy));
                            game.m_outdoorGameView.setCameraAngles(yaw, pitch);
                            steps << "temple-aim\t185\t" << world->partyX() << '\t' << world->partyY()
                                  << '\t' << world->partyFootZ() << "\tyaw=" << yaw
                                  << " pitch=" << pitch << '\n';
                            pressAction(game.m_gameSession, width, height, KeyboardAction::Use);
                            advancePhase(state, 3);
                        }
                        else if (state.phase == 3 && state.phaseFrame >= 4)
                        {
                            const EventDialogContent &dialog = screen.activeEventDialog();
                            require(dialog.isActive && dialog.isHouseDialog && dialog.sourceId == 303,
                                "Normal Use did not enter original out01 house 303");
                            const auto proprietor = std::find_if(
                                dialog.actions.begin(), dialog.actions.end(),
                                [](const EventDialogAction &action)
                                {
                                    return action.kind == EventDialogActionKind::HouseProprietor
                                        && action.id == 303;
                                });
                            if (proprietor != dialog.actions.end())
                            {
                                selectDialogAction(screen, EventDialogActionKind::HouseProprietor, 303);
                            }
                            advancePhase(state, 4);
                        }
                        else if (state.phase == 4 && state.phaseFrame >= 4)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            const HouseEntry *pHouse = screen.houseTable() != nullptr
                                ? screen.houseTable()->get(303)
                                : nullptr;
                            require(world != nullptr && pHouse != nullptr && pHouse->templeCanHeal,
                                "Original house 303 is not available as a healing temple");
                            const EventDialogContent &dialog = screen.activeEventDialog();
                            require(dialog.isActive && dialog.sourceId == 303,
                                "Original house 303 dialogue closed before healing");
                            state.healingPrice = PriceCalculator::templeHealPrice(
                                screen.partyReadOnly()->activeMember(), *pHouse, world->gameMinutes());
                            require(state.healingPrice == 10,
                                "Original house 303 did not quote the audited 10-gold healing price");
                            state.minutesBeforeHealing = world->gameMinutes();
                            state.goldBeforeHealing = screen.partyReadOnly()->gold();
                            state.fixtureParty = screen.partyReadOnly()->snapshot();
                            require(game.quickSaveToPath(
                                    absoluteOutput / "before-temple-heal.oysav", "跨洲神殿治疗前", {}),
                                "Cannot save the pre-healing boundary");
                            captureRequest = "temple-before-heal";
                            advancePhase(state, 5);
                        }
                        else if (state.phase == 5 && captured.contains("temple-before-heal"))
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr, "Temple healing lost the active outdoor world");
                            selectDialogAction(
                                screen, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TempleHeal));
                            state.stableParty = screen.partyReadOnly()->snapshot();
                            require(world->gameMinutes() == state.minutesBeforeHealing,
                                "Original temple healing unexpectedly advanced game time");
                            require(screen.partyReadOnly()->gold() == state.goldBeforeHealing - state.healingPrice,
                                "Original temple healing charged a different amount than quoted");
                            requireTempleHealingOnly(state.fixtureParty, state.stableParty, state.healingPrice);
                            require(game.quickSaveToPath(
                                    absoluteOutput / "after-temple-heal.oysav", "跨洲神殿治疗后", {}),
                                "Cannot save the post-healing boundary");
                            std::cout << "Original temple heal: member=2 price=" << state.healingPrice
                                      << " gold=" << state.goldBeforeHealing << "->"
                                      << state.stableParty.gold << " minutes=" << state.minutesBeforeHealing << '\n';
                            captureRequest = "temple-after-heal";
                            advancePhase(state, 6);
                        }
                        else if (state.phase == 6 && captured.contains("temple-after-heal"))
                        {
                            pressEscape(game.m_gameSession, width, height);
                            advancePhase(state, 7);
                        }
                        else if (state.phase == 7 && state.phaseFrame >= 4)
                        {
                            if (screen.activeEventDialog().isActive)
                            {
                                pressEscape(game.m_gameSession, width, height);
                                state.phaseFrame = 0;
                            }
                            else
                            {
                                state.waypoint = 0;
                                state.stalledFrames = 0;
                                advancePhase(state, 8);
                            }
                        }
                        else if (state.phase == 8 && state.phaseFrame >= 4)
                        {
                            if (walkRoute(game, width, height, state, jadameDimensionDoorRoute, steps))
                            {
                                advancePhase(state, 9);
                            }
                        }
                        else if (state.phase == 9 && state.phaseFrame >= 3)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr && world->partyX() >= -512.0f && world->partyX() < 0.0f
                                    && world->partyY() > 2048.0f && world->partyY() <= 2560.0f,
                                "Ordinary return walk did not enter exact terrain tile 63,59");
                            require(static_cast<uint32_t>(std::floor(world->gameMinutes() / 1440.0)) == 24,
                                "Return route crossed out of the audited day-24 profile");
                            require(screen.shouldTownPortalCastOpenDimensionDoor(),
                                "Actual movement onto out01 tile 63,59 did not activate Dimension Door");
                            require(game.quickSaveToPath(
                                    absoluteOutput / "tile-activated.oysav", "跨洲返程地块激活", {}),
                                "Cannot save the tile-activation boundary");
                            steps << "tile-activated\t63,59\t" << world->partyX() << '\t' << world->partyY()
                                  << '\t' << world->partyFootZ() << "\tday=24\n";
                            captureRequest = "dimension-door-tile-activated";
                            advancePhase(state, 10);
                        }
                        else if (state.phase >= 10 && state.phase <= 13
                            && captured.contains("dimension-door-tile-activated"))
                        {
                            openDimensionDoor(game, width, height, state, 10);
                        }
                        else if (state.phase == 14 && state.phaseFrame >= 4)
                        {
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr, "Return Dimension Door lost the outdoor world");
                            requireDimensionDoor(screen, *world);
                            require(game.quickSaveToPath(
                                    absoluteOutput / "before-enroth-selection.oysav", "跨洲返程选择前", {}),
                                "Cannot save the Enroth selection boundary");
                            hoverDestination(game, width, height, "TPGlobal:4");
                            captureRequest = "dimension-door-return-preview";
                            advancePhase(state, 15);
                        }
                        else if (state.phase == 15 && captured.contains("dimension-door-return-preview"))
                        {
                            const Party::Snapshot beforeSelection = screen.partyReadOnly()->snapshot();
                            requireStableParty(state.stableParty, beforeSelection, true);
                            const Character &beforeCaster = beforeSelection.members[2];
                            require(beforeCaster.spellPoints == 39
                                    && beforeCaster.recoverySecondsRemaining == 0.0f,
                                "Temple-healed caster was not at the audited return-cast boundary");
                            state.spellPointsBeforePortal = beforeCaster.spellPoints;
                            clickDestination(game.m_gameSession, width, height, "TPGlobal:4");
                            const Character &afterCaster = screen.partyReadOnly()->members()[2];
                            require(afterCaster.spellPoints == state.spellPointsBeforePortal - 20,
                                "Ordinary Enroth destination selection did not spend 20 SP");
                            require(afterCaster.recoverySecondsRemaining > 0.0f,
                                "Ordinary Enroth destination selection did not start recovery");
                            state.recoveryAtPortalRequest = afterCaster.recoverySecondsRemaining;
                            IGameplayWorldRuntime *world = game.m_gameSession.activeWorldRuntime();
                            require(world != nullptr && world->eventRuntimeState() != nullptr,
                                "Enroth request lost the active event runtime");
                            const auto &pendingMove = world->eventRuntimeState()->pendingMapMove;
                            require(pendingMove.has_value() && pendingMove->mapName.has_value()
                                    && *pendingMove->mapName == "outd2.odm"
                                    && pendingMove->x == 0 && pendingMove->y == 0 && pendingMove->z == 0
                                    && pendingMove->useMapStartPosition
                                    && !pendingMove->directionDegrees.has_value()
                                    && !pendingMove->lookAngleDegrees.has_value(),
                                "Enroth request did not preserve the cross-map zero/map-start contract");
                            steps << "portal-request\t4\t0\t0\t0\tmap=outd2.odm map_start=1 sp="
                                  << afterCaster.spellPoints << " recovery="
                                  << state.recoveryAtPortalRequest << '\n';
                            advancePhase(state, 16);
                        }
                        else if (state.phase == 16
                            && game.m_gameSession.currentMapFileName() == "outd2.odm")
                        {
                            advancePhase(state, 17);
                        }
                        else if (state.phase == 17 && state.phaseFrame >= 60)
                        {
                            requireAuthoredOutdoorStart(game, steps);
                            requireReturnTownPortalCost(
                                state.stableParty,
                                screen.partyReadOnly()->snapshot(),
                                state.recoveryAtPortalRequest);
                            require(game.m_gameSession.namedGlobalVars() == state.stableGlobals,
                                "Enroth return changed named globals");
                            require(game.quickSaveToPath(
                                    absoluteOutput / "enroth-arrived.oysav", "跨洲返回恩洛斯", {}),
                                "Cannot save the Enroth arrival");
                            requireEnrothMapVarUnchanged(absoluteOutput / "enroth-arrived.oysav");
                            captureRequest = "enroth-arrived";
                            state.saved = true;
                            advancePhase(state, 18);
                        }
                        else if (state.phase == 18 && captured.contains("enroth-arrived"))
                        {
                            state.complete = true;
                        }
                    }
                }

                game.renderFrame(width, height, wheel, delta);

                if (captureRequest.has_value())
                {
                    require(!pendingCaptures.contains(*captureRequest) && !captured.contains(*captureRequest),
                        "Screenshot token was requested more than once: " + *captureRequest);
                    pendingCaptures.emplace(*captureRequest, PendingCapture{frames});
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureRequest->c_str());
                }
                for (auto iterator = pendingCaptures.begin(); iterator != pendingCaptures.end();)
                {
                    std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                        Engine::BgfxContext::consumeScreenshot(iterator->first);
                    if (!capture.has_value())
                    {
                        require(frames - iterator->second.requestedFrame <= 300,
                            "Metal screenshot readback timed out: " + iterator->first);
                        ++iterator;
                        continue;
                    }
                    SDL_Surface *surface = SDL_CreateSurfaceFrom(
                        int(capture->width), int(capture->height), SDL_PIXELFORMAT_BGRA32,
                        capture->bgraPixels.data(), int(capture->width * 4));
                    require(surface != nullptr, "Cannot wrap Metal screenshot pixels");
                    const bool written = SDL_SaveBMP(surface,
                        (absoluteOutput / (iterator->first + ".bmp")).c_str());
                    SDL_DestroySurface(surface);
                    require(written, "Cannot write Metal screenshot: " + iterator->first);
                    std::cout << "Captured " << iterator->first << ": "
                              << capture->width << 'x' << capture->height << '\n';
                    captured.insert(iterator->first);
                    iterator = pendingCaptures.erase(iterator);
                }

                const bool timedOut = frames >= 18000 || SDL_GetTicks() - startedTicks >= 300000;
                if (state.complete || timedOut)
                {
                    if (timedOut && !state.complete)
                    {
                        game.quickSaveToPath(absoluteOutput / "timeout-state.oysav", "跨洲边界超时现场", {});
                    }
                    require(state.complete, "Cross-world capture exceeded its frame or wall-time bound");
                    require(captured == expectedCaptures, "Cross-world capture did not receive every screenshot acknowledgement");
                    require(state.saved, "Cross-world capture did not write its final process save");
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                game.shutdownApplication();
            },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });

        const int result = application.run();
        require(result == 0 && state.complete && state.saved && captured == expectedCaptures,
            "Cross-world capture process did not complete");
        std::cout << "Cross-world boundary capture passed: mode=" << captureModeName(mode)
                  << " frames=" << frames << " captures=" << captured.size()
                  << " walked=" << state.walkedDistance
                  << "; controlled caster fixture, day-24 original destination profile, Mac Metal only.\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc >= 5 && std::string(argv[4]) == "--scroll-destination")
        {
            if (argc != 10)
            {
                throw std::runtime_error(
                    "Usage: cross_world_travel_capture RUNTIME INPUT_SAVE NEW_OUTPUT "
                    "--scroll-destination DAY DESTINATION_ID MAP WORLD ICON");
            }
            const unsigned long parsedDay = std::stoul(argv[5]);
            if (parsedDay > 239) throw std::runtime_error("Scroll destination DAY must be 0 through 239");
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runScrollDestination(
                std::filesystem::canonical(argv[1]),
                std::filesystem::canonical(argv[2]),
                std::filesystem::absolute(argv[3]),
                static_cast<uint32_t>(parsedDay),
                argv[6], argv[7], argv[8], argv[9]);
        }
        if (argc != 5)
        {
            throw std::runtime_error(
                "Usage: cross_world_travel_capture RUNTIME INPUT_SAVE NEW_OUTPUT "
                "--to-jadame|--to-enroth|--reload-only|--scroll-loop|--scroll-reload, or "
                "--scroll-destination DAY DESTINATION_ID MAP WORLD ICON");
        }
        const std::string option = argv[4];
        OpenYAMM::Game::CaptureMode mode;
        if (option == "--to-jadame") mode = OpenYAMM::Game::CaptureMode::ToJadame;
        else if (option == "--to-enroth") mode = OpenYAMM::Game::CaptureMode::ToEnroth;
        else if (option == "--reload-only") mode = OpenYAMM::Game::CaptureMode::ReloadOnly;
        else if (option == "--scroll-loop") mode = OpenYAMM::Game::CaptureMode::ScrollLoop;
        else if (option == "--scroll-reload") mode = OpenYAMM::Game::CaptureMode::ScrollReload;
        else throw std::runtime_error("Unsupported cross-world capture mode: " + option);
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(
            std::filesystem::canonical(argv[1]),
            std::filesystem::canonical(argv[2]),
            std::filesystem::absolute(argv[3]),
            mode);
    }
    catch (const std::exception &error)
    {
        std::cerr << "Cross-world capture failed: " << error.what() << '\n';
        return 1;
    }
}
