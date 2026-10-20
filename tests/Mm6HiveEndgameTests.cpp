#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "game/events/ISceneEventContext.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"
#include "tools/LegacyLuaExport.h"
#include "tools/mmerge_import/LodResources.h"
#include "tools/mmerge_import/Mm6HiveResources.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
constexpr const char *SuppressionKey = "OpenYAMM.SuppressDeathMovie";

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

std::filesystem::path outputPath(const std::string &name)
{
    const char *pRoot = std::getenv("TMPDIR");
    REQUIRE_MESSAGE(pRoot != nullptr, "Hive tests require the external-storage TMPDIR");
    return std::filesystem::path(pRoot) / name;
}

std::string originalScript()
{
    std::ifstream input(sourceRoot() / "Scripts/Maps/hive.lua", std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string baseProgram()
{
    EvtProgram evt;
    REQUIRE(evt.loadFromBytes(readLodText(sourceRoot() / "Data/mm6.T.lod", "hive.evt")));
    StrTable strings;
    REQUIRE(strings.loadFromNullSeparatedBytes(readLodText(sourceRoot() / "Data/mmmerge.T.lod", "hive.str")));
    return generateLegacyEventLuaChunk(evt, strings, {}, LegacyLuaExportScope::Map, LegacyEventVersion::Mm8);
}

std::optional<ScriptedEventProgram> hiveProgram()
{
    const std::optional<Mm6HiveProfile> profile = loadMm6HiveProfile(sourceRoot());
    REQUIRE(profile.has_value());
    std::string error;
    std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(EventSupportSource) + "\n" + baseProgram() + "\n" + profile->luaSource,
        "@original-hive-evt+source-verified-ending", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    return program;
}

class HiveFixture : public OpenYAMM::Tests::PartySpellTestWorldRuntime, public ISceneEventContext
{
public:
    Party party;
    EventRuntimeState state;
    EventRuntime events;
    std::optional<ScriptedEventProgram> local = hiveProgram();
    std::vector<std::array<int32_t, 8>> summons;

    HiveFixture()
    {
        PartySeed seed = OpenYAMM::Tests::createSpellRegressionPartySeed();
        seed.members.push_back(OpenYAMM::Tests::makeSpellRegressionPartyMember("Fifth", "Knight", "PC01-01", 8));
        party.seed(seed);
        state.mapFileName = "hive.blv";
        state.activeHistoryContinentId = 3;
        bindParty(&party);
        bindEventRuntimeState(&state);
        setMapName("hive.blv");
        setPartyPosition(4000, 24000, -2256);
        for (uint32_t id : {28, 30, 51, 52, 53})
        {
            state.mechanisms[id].state = id == 30 || id == 53 ? 0 : 2;
        }
    }

    float currentGameMinutes() const override { return 600; }
    const MapDeltaData *mapDeltaData() const override { return nullptr; }
    MapDeltaData *mapDeltaData() override { return nullptr; }
    bool setFacetBit(uint32_t, uint32_t, bool) override { return false; }
    bool castEventSpell(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        int32_t, int32_t, int32_t) override { return false; }
    bool summonMonsters(uint32_t type, uint32_t level, uint32_t count, int32_t x, int32_t y,
        int32_t z, uint32_t group, uint32_t uniqueName) override
    {
        summons.push_back({int32_t(type), int32_t(level), int32_t(count), x, y, z,
            int32_t(group), int32_t(uniqueName)});
        return true;
    }
    bool summonEventItem(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool summonEventObject(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool checkMonstersKilled(uint32_t, uint32_t, uint32_t, bool) const override { return false; }

    void scroll(size_t member = 4)
    {
        InventoryItem item;
        item.objectDescriptionId = 2164;
        REQUIRE(party.member(member) != nullptr);
        REQUIRE(party.member(member)->addInventoryItem(item));
    }

    void killed(uint32_t monsterId)
    {
        EventRuntimeState::ActiveHookContext hook;
        hook.kind = EventRuntimeHookKind::MonsterKilled;
        hook.monsterId = monsterId;
        hook.actorIndex = 0;
        state.activeHookContext = hook;
        REQUIRE_MESSAGE(events.executeHooks(local, std::nullopt, hook.kind, state, &party, this),
            events.lastScriptError().value_or("Hive kill hook missing"));
        state.activeHookContext.reset();
        party.applyEventRuntimeState(state, false);
    }

    void leave()
    {
        REQUIRE_MESSAGE(events.executeOnLeaveEvents(local, std::nullopt, state, &party, this),
            events.lastScriptError().value_or("Hive leave hook missing"));
        party.applyEventRuntimeState(state, false);
    }

    void exit()
    {
        REQUIRE_MESSAGE(events.executeEventById(local, std::nullopt, 60, state, &party, this),
            events.lastScriptError().value_or("Hive exit missing"));
        party.applyEventRuntimeState(state, false);
    }

    int32_t flag(const std::string &name) const
    {
        const auto found = state.namedMapVars.find(name);
        return found == state.namedMapVars.end() ? 0 : found->second;
    }

    void coldReload(const std::string &name)
    {
        GameSaveData save;
        save.currentSceneKind = SceneKind::Indoor;
        save.mapFileName = "hive.blv";
        save.party = party.snapshot();
        save.namedGlobalVars = state.namedGlobalVars;
        save.hasIndoorSceneState = true;
        save.indoorScene.eventRuntimeState = state;
        save.indoorScene.worldRuntime.gameMinutes = 600;
        save.savedGameMinutes = 600;
        save.indoorSceneStates["hive.blv"] = save.indoorScene;
        std::string error;
        const std::filesystem::path path = outputPath(name);
        REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
        const std::optional<GameSaveData> loaded = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(loaded.has_value(), error);
        REQUIRE(loaded->indoorScene.eventRuntimeState.has_value());
        CHECK_EQ(loaded->indoorSceneStates.at("hive.blv").eventRuntimeState->namedMapVars, state.namedMapVars);
        CHECK_EQ(loaded->party.questBits, save.party.questBits);
        party.restoreSnapshot(loaded->party);
        state = *loaded->indoorScene.eventRuntimeState;
        local = hiveProgram();
        // Loading a save must not replay the kill handler or its sixteen summons.
    }
};
}

TEST_CASE("MM6 Hive profile accepts audited source and rejects changed or escaping scripts")
{
    const std::optional<Mm6HiveProfile> profile = loadMm6HiveProfile(sourceRoot());
    REQUIRE(profile.has_value());
    CHECK_EQ(profile->mapStem, "hive");
    CHECK_EQ(profile->sourceSha256, "17cba593aec07d22b9611acfa68374427d6bfacde3aa36efe82b7cb09e825b03");
    const std::string nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path fixture = outputPath("hive-profile-" + nonce);
    std::filesystem::create_directories(fixture / "Scripts/Maps");
    CHECK_FALSE(loadMm6HiveProfile(fixture).has_value());
    const std::filesystem::path candidate = fixture / "Scripts/Maps/hive.lua";
    std::ofstream(candidate, std::ios::binary) << originalScript() << "\n-- unreviewed change";
    CHECK_THROWS_AS(loadMm6HiveProfile(fixture), std::runtime_error);
    std::filesystem::remove(candidate);
    std::filesystem::create_symlink(sourceRoot() / "Scripts/Maps/hive.lua", candidate);
    CHECK_THROWS_AS(loadMm6HiveProfile(fixture), std::runtime_error);
}

TEST_CASE("MM6 Hive ending hooks supplement original EVT without replacing ordinary mechanisms")
{
    HiveFixture f;
    REQUIRE_EQ(f.local->monsterKilledHookEventIds(), std::vector<uint16_t>{65481});
    REQUIRE_EQ(f.local->monsterDamageHookEventIds(), std::vector<uint16_t>{65480});
    CHECK(f.local->hasEvent(1));
    CHECK(f.local->hasEvent(48));
    CHECK(f.local->hasEvent(100));
    CHECK_EQ(f.local->onLeaveEventIds(), std::vector<uint16_t>{65482});
    CHECK_EQ(f.local->timerTriggers().size(), 3);
    f.exit();
    CHECK_FALSE(f.state.statusMessages.empty());
    CHECK_FALSE(f.state.pendingMapMove);
    CHECK_FALSE(f.state.pendingDialogueContext);
    CHECK_FALSE(f.party.hasQuestBit(784));
    f.leave();
    CHECK_FALSE(f.state.pendingMovie);
    CHECK_FALSE(f.state.pendingReturnToMainMenu);
}

TEST_CASE("MM6 Hive queen before reactor and unrelated kills cannot open the ending")
{
    for (uint32_t monsterId : {646, 645, 649})
    {
        HiveFixture f;
        f.scroll();
        f.killed(monsterId);
        CHECK_EQ(f.flag("ReactorKilled"), 0);
        CHECK_EQ(f.flag("QueenKilled"), 0);
        CHECK_FALSE(f.party.hasQuestBit(1226));
        CHECK_FALSE(f.party.hasQuestBit(784));
        CHECK(f.summons.empty());
        f.leave();
        CHECK_FALSE(f.state.pendingMovie);
        CHECK_FALSE(f.state.pendingReturnToMainMenu);
    }
}

TEST_CASE("MM6 Hive either reactor without the scroll fails without granting completion")
{
    for (uint32_t reactor : {647, 648})
    {
        HiveFixture f;
        f.killed(reactor);
        REQUIRE(f.state.pendingMovie);
        CHECK_EQ(f.state.pendingMovie->movieName, "mm6end2");
        CHECK(f.state.pendingReturnToMainMenu);
        CHECK_EQ(f.flag("ReactorKilled"), 0);
        CHECK_FALSE(f.party.hasQuestBit(1226));
        CHECK_FALSE(f.party.hasQuestBit(784));
        CHECK(f.summons.empty());
        REQUIRE_EQ(f.state.pendingSounds.size(), 1);
        CHECK_EQ(f.state.pendingSounds.front().soundId, 130);
    }
}

TEST_CASE("MM6 Hive reactor uses fifth-member scroll and preserves every source summon and teleport field")
{
    const std::string source = originalScript();
    const std::regex summon(R"(evt\.SummonMonsters\((\d+), (\d+), (\d+), (-?\d+), (-?\d+), (-?\d+)\))");
    std::vector<std::array<int32_t, 8>> expected;
    for (std::sregex_iterator it(source.begin(), source.end(), summon), end; it != end; ++it)
    {
        std::array<int32_t, 8> entry = {};
        for (size_t index = 0; index < 6; ++index)
        {
            entry[index] = std::stoi((*it)[index + 1].str());
        }
        expected.push_back(entry);
    }
    REQUIRE_EQ(expected.size(), 16);
    for (uint32_t reactor : {647, 648})
    {
        HiveFixture f;
        f.scroll();
        f.killed(reactor);
        CHECK_EQ(f.flag("ReactorKilled"), 1);
        CHECK_EQ(f.flag(SuppressionKey), 1);
        CHECK_EQ(f.flag("QueenKilled"), 0);
        CHECK_FALSE(f.party.hasQuestBit(784));
        CHECK_FALSE(f.party.hasQuestBit(1226));
        CHECK(f.party.hasItemAnywhere(2164));
        CHECK_EQ(f.summons, expected);
        REQUIRE(f.state.pendingMapMove);
        const EventRuntimeState::PendingMapMove &move = *f.state.pendingMapMove;
        CHECK_FALSE(move.mapName);
        CHECK_FALSE(move.useMapStartPosition);
        CHECK(move.x >= 3208);
        CHECK(move.x <= 3448);
        CHECK(move.y >= 25800);
        CHECK(move.y <= 26040);
        CHECK_EQ(move.z, -2256);
        CHECK_EQ(move.directionDegrees, std::optional<int32_t>(90));
        CHECK_FALSE(f.state.pendingMovie);
        CHECK_FALSE(f.state.pendingReturnToMainMenu);
        CHECK_EQ(f.state.mechanisms.at(28).state, 3);
        CHECK_EQ(f.state.mechanisms.at(51).state, 3);
        CHECK_EQ(f.state.mechanisms.at(52).state, 3);
        CHECK_EQ(f.state.mechanisms.at(30).state, 1);
        CHECK_EQ(f.state.mechanisms.at(53).state, 1);
    }
}

TEST_CASE("MM6 Hive reactor rest clears source conditions but preserves death eradication and zombie")
{
    HiveFixture f;
    f.scroll();
    for (size_t index = 0; index < f.party.memberCount(); ++index)
    {
        Character *pMember = f.party.member(index);
        REQUIRE(pMember != nullptr);
        for (size_t condition = 0; condition <= size_t(CharacterCondition::Unconscious); ++condition)
        {
            pMember->conditions.set(condition);
            pMember->conditionStartGameMinutes[condition] = 10;
        }
        pMember->conditions.set(size_t(CharacterCondition::Petrified));
        pMember->health = 1;
        pMember->spellPoints = 1;
        pMember->recoverySecondsRemaining = 9;
    }
    f.party.member(1)->conditions.set(size_t(CharacterCondition::Dead));
    f.party.member(2)->conditions.set(size_t(CharacterCondition::Eradicated));
    f.party.member(3)->conditions.set(size_t(CharacterCondition::Zombie));
    f.party.applyPartyBuff(PartyBuffId::Haste, 100, 1, 5, 1, SkillMastery::Normal, 0);
    f.killed(647);
    CHECK_FALSE(f.party.hasPartyBuff(PartyBuffId::Haste));
    for (size_t index = 0; index < f.party.memberCount(); ++index)
    {
        const Character *pMember = f.party.member(index);
        for (size_t condition = 0; condition <= size_t(CharacterCondition::Unconscious); ++condition)
        {
            CHECK_FALSE(pMember->conditions.test(condition));
            CHECK_EQ(pMember->conditionStartGameMinutes[condition], 0);
        }
        CHECK_FALSE(pMember->conditions.test(size_t(CharacterCondition::Petrified)));
    }
    CHECK(f.party.member(1)->conditions.test(size_t(CharacterCondition::Dead)));
    CHECK(f.party.member(2)->conditions.test(size_t(CharacterCondition::Eradicated)));
    CHECK(f.party.member(3)->conditions.test(size_t(CharacterCondition::Zombie)));
    CHECK_EQ(f.party.member(1)->health, 1);
    CHECK_EQ(f.party.member(2)->health, 1);
    CHECK_EQ(f.party.member(3)->health, f.party.member(3)->maxHealth / 2);
    CHECK_EQ(f.party.member(3)->spellPoints, 0);
    CHECK_EQ(f.party.member(4)->health, f.party.member(4)->maxHealth);
    CHECK_EQ(f.party.member(4)->spellPoints, f.party.member(4)->maxSpellPoints);
    CHECK_EQ(f.party.member(4)->recoverySecondsRemaining, 0);
}

TEST_CASE("MM6 Hive physical reactor damage alone is replaced by the original 2 through 40 roll")
{
    HiveFixture f;
    for (uint32_t monster : {646, 647, 648, 649})
    {
        for (uint32_t damageType : {0, 4, 10})
        {
            EventRuntimeState::ActiveHookContext hook;
            hook.kind = EventRuntimeHookKind::MonsterDamage;
            hook.monsterId = monster;
            hook.damageType = damageType;
            hook.damage = 1000;
            f.state.activeHookContext = hook;
            REQUIRE(f.events.executeHooks(f.local, std::nullopt, hook.kind, f.state, &f.party, &f));
            const std::optional<int32_t> result = f.state.activeHookContext->damageOverride;
            if ((monster == 647 || monster == 648) && damageType == 4)
            {
                REQUIRE(result);
                CHECK(*result >= 2);
                CHECK(*result <= 40);
            }
            else
            {
                CHECK_FALSE(result);
            }
        }
    }
}

TEST_CASE("MM6 Hive leaving between reactor and queen loses after a cold save reload")
{
    HiveFixture f;
    f.scroll();
    f.party.setQuestBit(1222, true);
    f.killed(647);
    f.state.pendingMapMove.reset();
    f.coldReload("hive-reactor-before-queen.oysav");
    CHECK_EQ(f.summons.size(), 16);
    CHECK_EQ(f.flag(SuppressionKey), 1);
    f.leave();
    REQUIRE(f.state.pendingMovie);
    CHECK_EQ(f.state.pendingMovie->movieName, "mm6end2");
    CHECK(f.state.pendingReturnToMainMenu);
    CHECK_FALSE(f.party.hasQuestBit(784));
    CHECK(f.party.hasQuestBit(1222));
    CHECK(f.party.hasItemAnywhere(2164));
}

TEST_CASE("MM6 Hive victory requires ordered kills and leaving and remains playable after persistence")
{
    HiveFixture f;
    f.scroll();
    f.party.setQuestBit(1222, true);
    f.killed(647);
    f.state.pendingMapMove.reset();
    f.killed(646);
    CHECK_EQ(f.flag("QueenKilled"), 1);
    CHECK_EQ(f.flag(SuppressionKey), 0);
    CHECK(f.party.hasQuestBit(1226));
    CHECK_FALSE(f.party.hasQuestBit(784));
    CHECK(f.party.hasItemAnywhere(2164));
    f.coldReload("hive-queen-before-exit.oysav");
    f.exit();
    REQUIRE(f.state.pendingMapMove);
    CHECK_EQ(f.state.pendingMapMove->mapName, std::optional<std::string>("oute3.odm"));
    CHECK(f.state.pendingMapMove->useMapStartPosition);
    CHECK_FALSE(f.state.pendingDialogueContext);
    CHECK_FALSE(f.party.hasQuestBit(784));
    f.leave();
    CHECK(f.party.hasQuestBit(784));
    CHECK(f.party.hasQuestBit(1226));
    CHECK_FALSE(f.party.hasQuestBit(1222));
    CHECK_FALSE(f.party.hasItemAnywhere(2164));
    CHECK_EQ(f.state.namedGlobalVars.at("MMerge.CrossContinents.ContinentFinished.3"), 1);
    for (size_t index = 0; index < f.party.memberCount(); ++index)
    {
        CHECK(f.party.hasAward(index, 78));
    }
    REQUIRE(f.state.pendingMovie);
    CHECK_EQ(f.state.pendingMovie->movieName, "mm6end1");
    CHECK_FALSE(f.state.pendingReturnToMainMenu);
    CHECK_FALSE(f.state.pendingWinGame);
    CHECK_FALSE(f.party.hasQuestBit(1261)); // Stale EVT ending side effect is replaced, not composed.
    f.state.pendingMapMove.reset();
    f.state.pendingMovie.reset();
    f.coldReload("hive-completed.oysav");
    CHECK(f.party.hasQuestBit(784));
    CHECK_FALSE(f.party.hasItemAnywhere(2164));
    CHECK_FALSE(f.state.pendingReturnToMainMenu);
    CHECK_EQ(f.summons.size(), 16);
}

TEST_CASE("MM6 Hive forged exit qbit alone cannot substitute for reactor and queen history")
{
    HiveFixture f;
    f.scroll();
    f.party.setQuestBit(1226, true);
    f.exit();
    REQUIRE(f.state.pendingMapMove);
    f.leave();
    CHECK_FALSE(f.party.hasQuestBit(784));
    CHECK_FALSE(f.state.pendingMovie);
    CHECK(f.party.hasItemAnywhere(2164));
}

TEST_CASE("MM6 Hive map load restores suppression without replaying the reactor reaction")
{
    for (bool reactor : {false, true})
    {
        for (bool queen : {false, true})
        {
            HiveFixture f;
            f.state.namedMapVars["ReactorKilled"] = reactor;
            f.state.namedMapVars["QueenKilled"] = queen;
            REQUIRE(f.events.executeOnLoadEvents(f.local, std::nullopt, f.state, &f.party, &f));
            CHECK_EQ(f.flag(SuppressionKey), reactor && !queen);
            CHECK(f.summons.empty());
            CHECK_FALSE(f.state.pendingMapMove);
            CHECK_FALSE(f.party.hasQuestBit(784));
        }
    }
}
