#include "doctest/doctest.h"

#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/data/GameDataRepository.h"
#include "game/gameplay/BreachAlvarCompanionTransaction.h"
#include "game/maps/SaveGame.h"
#include "game/maps/MapIdentity.h"
#include "tools/mmerge_import/BreachAlvarCompanionResources.h"
#include "game/tables/RosterTable.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>

using namespace OpenYAMM::Game;

namespace
{
std::filesystem::path output(const std::string &name)
{
    const char *pRoot = std::getenv("TMPDIR");
    REQUIRE(pRoot != nullptr);
    return std::filesystem::path(pRoot) / ("breach149-save-" + name + ".oysav");
}

std::vector<char> bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

const BreachAlvarFriendActorRules &rules()
{
    static const auto resources = OpenYAMM::Import::loadBreachAlvarCompanionResources(
        std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
            / "local-data/validation/mm8-runtime-076/source-001");
    REQUIRE(resources);
    return resources->profile.friendActor;
}

bool writeSave(const std::filesystem::path &path, const GameSaveData &data, std::string &error)
{
    return saveGameDataToPath(path, data, error, &rules());
}

void synchronizeWorldCopies(GameSaveData &save)
{
    save.outdoorWorld.eventRuntimeState->namedGlobalVars = save.namedGlobalVars;
    save.outdoorWorldStates["bralvar.odm"] = save.outdoorWorld;
    save.outdoorWorldStates[std::string(BreachAlvarCompanionMap)] = save.outdoorWorld;
    save.outdoorWorldStates[buildCanonicalMapId("mmmerge", "bralvar.odm")] = save.outdoorWorld;
}

// Session binds the legacy process-wide class table. Release that non-owning binding
// before this test's local loader is destroyed, including on an assertion exception.
struct SessionFixture
{
    GameDataLoader loader;
    GameDataRepository data;
    GameSession session;

    SessionFixture()
    {
        data.bind(loader);
        session.bindDataRepository(&data);
    }
    ~SessionFixture()
    {
        session.bindDataRepository(nullptr);
    }
};

CharacterDollTable dolls()
{
    std::vector<std::vector<std::string>> rows(1, std::vector<std::string>(24));
    for (uint32_t id = 1; id <= 5; ++id)
    {
        std::vector<std::string> row(24, "0");
        row[0] = std::to_string(id);
        row[22] = std::to_string(200 + id);
        rows.push_back(std::move(row));
    }
    CharacterDollTable table;
    REQUIRE(table.loadCharacterRows(rows));
    return table;
}

// Declared codec fixture. This is not a real actor spawn, renderer or Session split acceptance.
GameSaveData splitSave(const CharacterDollTable &table)
{
    PartySeed seed;
    seed.members.resize(5);
    for (size_t index = 0; index < seed.members.size(); ++index)
    {
        seed.members[index].name = "Persistent friend " + std::to_string(index);
        seed.members[index].className = "Knight";
        seed.members[index].characterDataId = uint32_t(index + 1);
    }
    Party party;
    party.seed(seed);
    Party::Snapshot before = party.snapshot();
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        InventoryItem item;
        item.objectDescriptionId = uint32_t(101 + index);
        item.quantity = 1;
        item.width = 1;
        item.height = 1;
        item.currentCharges = 3;
        item.maxCharges = 9;
        item.stolen = true;
        item.temporaryBonusRemainingSeconds = 42.5f;
        before.members[index].inventory.push_back(item);
        before.pendingItemDeliveries.push_back({item, int32_t(index), false});
    }
    party.restoreSnapshot(before);
    const auto prepared = prepareBreachAlvarFiveMemberSplit(party, std::nullopt, table, {}, 1234567);
    REQUIRE_MESSAGE(prepared.mutation, prepared.error);

    GameSaveData save;
    save.party = prepared.mutation->party().snapshot();
    save.breachAlvarCompanions = BreachAlvarCompanionState{
        prepared.mutation->receipt(), {std::string(BreachAlvarCompanionMap), 0, 261}};
    save.namedGlobalVars["MMerge.CrossContinents.BreachSplit"] = 1;
    save.mapFileName = "bralvar.odm";
    save.hasOutdoorRuntimeState = true;
    save.savedGameMinutes = 69571.718750123;
    save.outdoorWorld.gameMinutes = save.savedGameMinutes;
    save.outdoorWorld.nextActorId = 1;
    save.outdoorWorld.eventRuntimeState.emplace();
    save.outdoorWorld.eventRuntimeState->mapFileName = save.mapFileName;
    save.outdoorWorld.eventRuntimeState->namedGlobalVars = save.namedGlobalVars;
    OutdoorWorldRuntime::MapActorState actor;
    actor.actorId = 0;
    actor.monsterId = 261;
    actor.npcId = 772;
    actor.spawnedAtRuntime = true;
    actor.group = 50;
    actor.ally = 9999;
    actor.hostileToParty = false;
    actor.currentHp = 555;
    actor.maxHp = 600;
    actor.breachAlvarCompanion = true;
    actor.breachAlvarNextMaintenanceGameMinute = save.savedGameMinutes + 0.75;
    save.outdoorWorld.mapActors.push_back(actor);
    synchronizeWorldCopies(save);
    save.heldInventoryItemActive = true;
    save.heldInventoryItem = before.members[0].inventory.front();
    save.heldInventoryItemGrabOffsetX = 3.25f;
    return save;
}

void checkPersisted(const GameSaveData &actual, const GameSaveData &expected)
{
    CHECK(actual.breachAlvarCompanions == expected.breachAlvarCompanions);
    CHECK(actual.party.nextMemberId == expected.party.nextMemberId);
    CHECK(actual.party.completedDetachmentOwners == expected.party.completedDetachmentOwners);
    CHECK(actual.party.questBits == expected.party.questBits);
    CHECK(actual.savedGameMinutes == expected.savedGameMinutes);
    CHECK(actual.heldInventoryItemActive == expected.heldInventoryItemActive);
    CHECK(actual.heldInventoryItem.objectDescriptionId == expected.heldInventoryItem.objectDescriptionId);
    CHECK(actual.heldInventoryItem.temporaryBonusRemainingSeconds == 42.5f);
    CHECK(actual.heldInventoryItemGrabOffsetX == 3.25f);
    REQUIRE(actual.party.members.size() == expected.party.members.size());
    REQUIRE(actual.party.detachedMembers.size() == expected.party.detachedMembers.size());
    for (size_t index = 0; index < actual.party.members.size(); ++index)
    {
        CHECK(actual.party.members[index].memberId == expected.party.members[index].memberId);
        CHECK(actual.party.members[index].name == expected.party.members[index].name);
    }
    for (size_t index = 0; index < actual.party.detachedMembers.size(); ++index)
    {
        const auto &a = actual.party.detachedMembers[index];
        const auto &b = expected.party.detachedMembers[index];
        CHECK(a.ownerKey == b.ownerKey);
        CHECK(a.slotId == b.slotId);
        CHECK(a.character.memberId == b.character.memberId);
        CHECK(a.character.name == b.character.name);
        REQUIRE(a.pendingItemDeliveries.size() == b.pendingItemDeliveries.size());
        REQUIRE(a.pendingItemDeliveries.size() == 1);
        CHECK_FALSE(a.pendingItemDeliveries[0].allowDropProtectedHeldItem);
        CHECK(a.pendingItemDeliveries[0].item.currentCharges == 3);
        CHECK(a.pendingItemDeliveries[0].item.temporaryBonusRemainingSeconds == 42.5f);
        CHECK(a.pendingItemDeliveries[0].item.stolen);
    }
    REQUIRE(actual.outdoorWorld.mapActors.size() == 1);
    const auto &actor = actual.outdoorWorld.mapActors[0];
    CHECK(actor.actorId == 0);
    CHECK(actor.breachAlvarCompanion);
    CHECK(actor.breachAlvarCompanionRemoved == expected.outdoorWorld.mapActors[0].breachAlvarCompanionRemoved);
    CHECK(actor.breachAlvarNextMaintenanceGameMinute
        == expected.outdoorWorld.mapActors[0].breachAlvarNextMaintenanceGameMinute);
    CHECK(actor.currentHp == 555);
    CHECK(actor.maxHp == 600);
}
}

TEST_CASE("save91 keeps typed companion identities actor lifecycle and Session capture together")
{
    const auto table = dolls();
    const auto save = splitSave(table);
    std::string error;
    REQUIRE_MESSAGE(validateSavedBreachAlvarCompanions(save, error), error);
    REQUIRE_MESSAGE(writeSave(output("split"), save, error), error);
    const auto encoded = bytes(output("split"));
    REQUIRE(encoded.size() > 12);
    CHECK(uint8_t(encoded[8]) == 91);
    const auto loaded = loadGameDataFromPath(output("split"), error);
    REQUIRE_MESSAGE(loaded, error);
    checkPersisted(*loaded, save);

    SessionFixture fixture;
    GameSession &session = fixture.session;
    session.restoreFromSaveData(*loaded);
    CHECK(session.breachAlvarCompanions() == save.breachAlvarCompanions);
    const auto captured = session.buildSaveData();
    REQUIRE(captured);
    checkPersisted(*captured, save);
    REQUIRE_MESSAGE(writeSave(output("session"), *captured, error), error);
    session.clear();
    CHECK_FALSE(session.breachAlvarCompanions());
}

TEST_CASE("save91 refuses missing custody actor bindings and split provenance before replacing a file")
{
    const auto table = dolls();
    const auto save = splitSave(table);
    std::string error;
    const auto path = output("protected");
    REQUIRE_MESSAGE(writeSave(path, save, error), error);
    const auto original = bytes(path);
    const auto reject = [&](GameSaveData bad)
    {
        CHECK_FALSE(writeSave(path, bad, error));
        CHECK_FALSE(error.empty());
        CHECK(bytes(path) == original);
    };
    auto bad = save;
    bad.breachAlvarCompanions.reset();
    reject(bad);
    bad = save;
    bad.breachAlvarCompanions->friendActor.actorId = 999;
    reject(bad);
    bad = save;
    bad.breachAlvarCompanions->friendActor.spawnMonsterId = 262;
    reject(bad);
    bad = save;
    bad.breachAlvarCompanions->receipt.companions[0].returned = true;
    reject(bad);
    bad = save;
    bad.namedGlobalVars.clear();
    reject(bad);
    bad = save;
    bad.outdoorWorld.eventRuntimeState->namedGlobalVars["MMerge.CrossContinents.BreachSplit"] = 0;
    reject(bad);
    bad = save;
    bad.party.questBits.erase(1714);
    reject(bad);
    bad = save;
    bad.outdoorWorld.mapActors.push_back(bad.outdoorWorld.mapActors[0]);
    reject(bad);
    bad = save;
    bad.outdoorWorldStates.begin()->second.mapActors.clear();
    reject(bad);
    bad = save;
    bad.outdoorWorld.mapActors[0].breachAlvarCompanionRemoved = true;
    reject(bad);
    bad = save;
    bad.outdoorWorld.mapActors[0].breachAlvarNextMaintenanceGameMinute = std::numeric_limits<double>::quiet_NaN();
    reject(bad);
    bad = save;
    bad.mapFileName = "out01.odm";
    reject(bad);
    bad = save;
    bad.outdoorWorld.mapActors[0].npcId = 773;
    synchronizeWorldCopies(bad);
    CHECK_FALSE(validateSavedBreachAlvarCompanions(bad, error));
    reject(bad);
    bad = save;
    bad.outdoorWorld.mapActors[0].group = 51;
    synchronizeWorldCopies(bad);
    reject(bad);
    bad = save;
    bad.outdoorWorld.mapActors[0].ally = 0;
    synchronizeWorldCopies(bad);
    reject(bad);
    CHECK_FALSE(saveGameDataToPath(path, save, error));
    CHECK(bytes(path) == original);
}

TEST_CASE("save91 rejects stale map aliases while treating collection order as irrelevant")
{
    const auto table = dolls();
    auto save = splitSave(table);
    std::string error;
    const std::array<std::string, 3> aliases{
        "bralvar.odm", std::string(BreachAlvarCompanionMap), buildCanonicalMapId("mmmerge", "bralvar.odm")};
    for (const std::string &alias : aliases)
    {
        auto bad = save;
        bad.outdoorWorldStates.at(alias).mapActors[0].currentHp = 1;
        CHECK_FALSE(validateSavedBreachAlvarCompanions(bad, error));
        CHECK(error.find("conflicting") != std::string::npos);
    }
    auto &vars = save.outdoorWorld.eventRuntimeState->namedMapVars;
    for (int index = 0; index < 20; ++index)
    {
        vars["key" + std::to_string(index)] = index;
    }
    synchronizeWorldCopies(save);
    auto &reordered = save.outdoorWorldStates.at(aliases[2]).eventRuntimeState->namedMapVars;
    reordered.clear();
    reordered.rehash(97);
    for (int index = 19; index >= 0; --index)
    {
        reordered["key" + std::to_string(index)] = index;
    }
    CHECK_MESSAGE(validateSavedBreachAlvarCompanions(save, error), error);
}

TEST_CASE("source ending can clear Q1714 while retaining historical companion custody")
{
    const auto table = dolls();
    auto save = splitSave(table);
    save.party.questBits.erase(1714);
    std::string error;
    CHECK_FALSE(validateSavedBreachAlvarCompanions(save, error));
    save.namedGlobalVars["MMerge.CrossContinents.QuestFinished"] = 1;
    synchronizeWorldCopies(save);
    REQUIRE_MESSAGE(writeSave(output("ending-history"), save, error), error);
    const auto loaded = loadGameDataFromPath(output("ending-history"), error);
    REQUIRE_MESSAGE(loaded, error);
    CHECK_FALSE(loaded->party.questBits.contains(1714));
    CHECK(loaded->breachAlvarCompanions == save.breachAlvarCompanions);
    CHECK(loaded->party.detachedMembers.size() == 4);
}

TEST_CASE("save91 preserves all return orders without reviving street actors or duplicating custody")
{
    const auto table = dolls();
    std::array<uint32_t, 4> order{772, 773, 774, 775};
    size_t permutation = 0;
    do
    {
        auto save = splitSave(table);
        for (uint32_t npcId : order)
        {
            Party party;
            party.restoreSnapshot(save.party);
            const auto prepared = prepareBreachAlvarCompanionReturn(
                party, save.breachAlvarCompanions, table, {false, {1, 2, 3, 4}}, npcId);
            REQUIRE_MESSAGE(prepared.mutation, prepared.error);
            save.party = prepared.mutation->party().snapshot();
            save.breachAlvarCompanions->receipt = prepared.mutation->receipt();
            if (npcId == 772)
            {
                auto &actor = save.outdoorWorld.mapActors[0];
                actor.breachAlvarCompanionRemoved = true;
                actor.isInvisible = true;
                actor.canReceiveDamage = false;
            }
            synchronizeWorldCopies(save);
            const auto path = output("return-" + std::to_string(permutation) + "-" + std::to_string(npcId));
            std::string error;
            REQUIRE_MESSAGE(writeSave(path, save, error), error);
            const auto loaded = loadGameDataFromPath(path, error);
            REQUIRE_MESSAGE(loaded, error);
            checkPersisted(*loaded, save);
            save = *loaded;
        }
        CHECK(save.party.members.size() == 5);
        CHECK(save.party.detachedMembers.empty());
        CHECK(save.party.pendingItemDeliveries.size() == 5);
        CHECK(save.outdoorWorld.mapActors[0].breachAlvarCompanionRemoved);
        ++permutation;
    } while (std::next_permutation(order.begin(), order.end()));
    CHECK(permutation == 24);
}

TEST_CASE("save91 rejects decoded companion corruption and truncated lifecycle records")
{
    const auto table = dolls();
    const auto save = splitSave(table);
    std::string error;
    const auto path = output("corrupt");
    REQUIRE_MESSAGE(writeSave(path, save, error), error);
    const auto encoded = bytes(path);
    const auto decodeBytes = [&](const std::vector<char> &payload)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(payload.data(), std::streamsize(payload.size()));
        file.close();
        CHECK_FALSE(loadGameDataFromPath(path, error));
        CHECK_FALSE(error.empty());
    };
    const std::string marker(BreachAlvarCompanionMap);
    auto bad = encoded;
    auto found = std::find_end(bad.begin(), bad.end(), marker.begin(), marker.end());
    REQUIRE(found != bad.end());
    REQUIRE(std::search(found + marker.size(), bad.end(), marker.begin(), marker.end()) == bad.end());
    *found = 'x';
    decodeBytes(bad);
    bad = encoded;
    found = std::find_end(bad.begin(), bad.end(), marker.begin(), marker.end());
    const size_t actorOffset = size_t(found - bad.begin()) + marker.size();
    REQUIRE(actorOffset + 8 == bad.size());
    bad[actorOffset] = 99;
    decodeBytes(bad);
    for (size_t cut : {size_t(1), size_t(8), size_t(32)})
    {
        bad.assign(encoded.begin(), encoded.end() - cut);
        decodeBytes(bad);
    }
}

TEST_CASE("save91 reads sealed save90 without inventing a split and retains the old input bytes")
{
    const std::filesystem::path root = OPENYAMM_SOURCE_DIR;
    const auto path = root.parent_path()
        / "local-data/validation/mm8-endgame-146/lords-ending-001/mm8-ending-complete.oysav";
    const auto original = bytes(path);
    REQUIRE(uint8_t(original.at(8)) == 90);
    std::string error;
    const auto old = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(old, error);
    CHECK_FALSE(old->breachAlvarCompanions);
    REQUIRE_MESSAGE(writeSave(output("upgrade90"), *old, error), error);
    const auto upgraded = loadGameDataFromPath(output("upgrade90"), error);
    REQUIRE_MESSAGE(upgraded, error);
    CHECK_FALSE(upgraded->breachAlvarCompanions);
    CHECK(upgraded->party.questBits == old->party.questBits);
    CHECK(upgraded->party.nextMemberId == old->party.nextMemberId);
    CHECK(upgraded->savedGameMinutes == old->savedGameMinutes);
    CHECK(upgraded->namedGlobalVars == old->namedGlobalVars);
    CHECK(bytes(path) == original);
}

TEST_CASE("transaction byte baselines detect gameplay changes beyond party member identities")
{
    const auto table = dolls();
    const auto save = splitSave(table);
    const auto baseline = encodePartySnapshotForComparison(save.party);
    CHECK(encodePartySnapshotForComparison(save.party) == baseline);
    auto changed = save.party;
    --changed.members[0].health;
    CHECK(encodePartySnapshotForComparison(changed) != baseline);
    changed = save.party;
    ++changed.members[0].spellPoints;
    CHECK(encodePartySnapshotForComparison(changed) != baseline);
    changed = save.party;
    ++changed.detachedMembers[0].character.inventory.front().currentCharges;
    CHECK(encodePartySnapshotForComparison(changed) != baseline);
    changed = save.party;
    changed.detachedMembers[0].pendingItemDeliveries[0].allowDropProtectedHeldItem = true;
    CHECK(encodePartySnapshotForComparison(changed) != baseline);
    changed = save.party;
    changed.characterBuffs[0][0].remainingSeconds = 1.25f;
    CHECK(encodePartySnapshotForComparison(changed) != baseline);
    changed = save.party;
    changed.questBits.insert(7777);
    CHECK(encodePartySnapshotForComparison(changed) != baseline);
    changed = save.party;
    changed.npcTopicOverrides.rehash(89);
    CHECK(encodePartySnapshotForComparison(changed) == baseline);
    auto item = save.heldInventoryItem;
    const auto held = encodeInventoryItemForComparison(item);
    item.temporaryBonusRemainingSeconds -= 0.5f;
    CHECK(encodeInventoryItemForComparison(item) != held);
}

TEST_CASE("save91 live capture advances every companion map alias before saving")
{
    const auto save = splitSave(dolls());
    SessionFixture fixture;
    GameSession &session = fixture.session;
    session.restoreFromSaveData(save);
    REQUIRE(session.partyState());
    Party party = *session.partyState();
    auto world = save.outdoorWorld;
    world.gameMinutes += 0.5;
    world.mapActors[0].currentHp -= 17;
    session.captureOutdoorRuntimeState("bralvar.odm", party, save.outdoorParty, world, 0, 0);
    std::string error;
    auto captured = session.buildSaveData();
    REQUIRE(captured);
    REQUIRE_MESSAGE(validateSavedBreachAlvarCompanions(*captured, error, &rules()), error);
    for (const auto &[key, cached] : captured->outdoorWorldStates)
    {
        CAPTURE(key);
        CHECK(cached.mapActors[0].currentHp == world.mapActors[0].currentHp);
        CHECK(cached.gameMinutes == world.gameMinutes);
    }
    world.mapActors[0].currentHp -= 11;
    session.storeOutdoorWorldState(buildCanonicalMapId("mmmerge", "bralvar.odm"), world);
    captured = session.buildSaveData();
    REQUIRE(captured);
    REQUIRE_MESSAGE(writeSave(output("live-alias-capture"), *captured, error), error);
    const auto loaded = loadGameDataFromPath(output("live-alias-capture"), error);
    REQUIRE_MESSAGE(loaded, error);
    CHECK(loaded->outdoorWorld.mapActors[0].currentHp == 527);
    for (const auto &[key, cached] : loaded->outdoorWorldStates)
    {
        CAPTURE(key);
        CHECK(cached.mapActors[0].currentHp == 527);
    }
}
