#include "CrossContinentsIntroSource.h"
#include "EventSupportSource.h"
#include "doctest/doctest.h"

#include "engine/TextTable.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/events/EventRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/tables/NpcDialogTable.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
constexpr uint32_t VerdantNpcId = 803;
constexpr uint32_t IntroSlot = 1;
constexpr std::array<uint16_t, 3> IntroTopicIds = {1778, 1779, 1780};
constexpr std::array<uint32_t, 4> IntroTextIds = {2149, 2150, 2151, 2152};
constexpr std::array<const char *, 4> IntroGlobalNames = {
    "MMerge.CrossContinents.MetVerdant",
    "MMerge.CrossContinents.GotIntro1",
    "MMerge.CrossContinents.GotIntro2",
    "MMerge.CrossContinents.GotMainQuest",
};
constexpr std::array<const char *, 4> BoundaryFixtureMessages = {
    "维尔丹特引言：第一段",
    "维尔丹特引言：第二段",
    "维尔丹特引言：第三段",
    "维尔丹特引言：主任务",
};

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
};

std::string readTextFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE_MESSAGE(input.is_open(), ("Could not open " + path.string()).c_str());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::optional<ScriptedEventProgram> loadProgram(const std::string &source, std::string &error)
{
    return ScriptedEventProgram::loadFromLuaText(source, "@cross-continents-intro-boundary-fixture",
        ScriptedEventScope::Global, error);
}

std::optional<ScriptedEventProgram> loadCompiledIntroProgram(std::string &error)
{
    return loadProgram(std::string(OpenYAMM::Import::EventSupportSource) + "\n"
        + std::string(OpenYAMM::Import::CrossContinentsIntroSource), error);
}

NpcDialogTable makeBoundaryNpcDialogTable()
{
    std::vector<std::vector<std::string>> rows;
    for (size_t index = 0; index < IntroTextIds.size(); ++index)
    {
        rows.push_back({std::to_string(IntroTextIds[index]), BoundaryFixtureMessages[index]});
    }

    NpcDialogTable table;
    REQUIRE(table.loadTextsFromRows(rows));
    return table;
}

Party makeBoundaryParty()
{
    Character firstMember;
    firstMember.name = "Boundary fixture member one";
    firstMember.experience = 321;
    firstMember.maxHealth = 40;
    firstMember.health = 40;

    Character secondMember = firstMember;
    secondMember.name = "Boundary fixture member two";
    secondMember.experience = 654;

    PartySeed seed;
    seed.gold = 432;
    seed.food = 7;
    seed.members = {firstMember, secondMember};

    Party party;
    party.seed(seed);
    return party;
}

std::vector<uint32_t> memberExperiences(const Party &party)
{
    std::vector<uint32_t> experiences;
    experiences.reserve(party.memberCount());
    for (const Character &member : party.members())
    {
        experiences.push_back(member.experience);
    }
    return experiences;
}

int32_t globalValue(const EventRuntimeState &state, const char *pName)
{
    const std::unordered_map<std::string, int32_t>::const_iterator iterator = state.namedGlobalVars.find(pName);
    return iterator != state.namedGlobalVars.end() ? iterator->second : 0;
}

void executeNpcEnter(
    EventRuntime &runtime,
    const std::optional<ScriptedEventProgram> &program,
    EventRuntimeState &state,
    Party &party,
    uint32_t npcId)
{
    state.activeHookContext = EventRuntimeState::ActiveHookContext{};
    state.activeHookContext->kind = EventRuntimeHookKind::NpcEnter;
    state.activeHookContext->npcId = npcId;
    REQUIRE(runtime.executeHooks(std::nullopt, program, EventRuntimeHookKind::NpcEnter, state, &party));
}

void executeIntroTopic(
    EventRuntime &runtime,
    const std::optional<ScriptedEventProgram> &program,
    EventRuntimeState &state,
    Party &party,
    uint16_t topicId,
    const std::string &expectedMessage)
{
    const size_t previousMessageCount = state.messages.size();
    REQUIRE(runtime.executeNpcTopicEventById(std::nullopt, program, topicId, state, &party));
    REQUIRE_EQ(state.messages.size(), previousMessageCount + 1);
    CHECK_EQ(state.messages.back(), expectedMessage);
}

void checkOnlyTopicVisible(
    EventRuntime &runtime,
    const std::optional<ScriptedEventProgram> &program,
    const EventRuntimeState &state,
    const Party &party,
    uint16_t expectedTopicId)
{
    for (uint16_t topicId : IntroTopicIds)
    {
        CHECK_EQ(runtime.canShowTopic(program, topicId, state, &party), topicId == expectedTopicId);
    }
}

void captureOutdoor(GameSession &session, const std::string &map, const Party &party, const EventRuntimeState &state)
{
    OutdoorWorldRuntime::Snapshot world;
    world.eventRuntimeState = state;
    session.captureOutdoorRuntimeState(map, party, OutdoorPartyRuntime::Snapshot{}, world, 0.0f, 0.0f);
}

void checkGlobals(
    const std::unordered_map<std::string, int32_t> &globals,
    const std::array<int32_t, 4> &expected)
{
    for (size_t index = 0; index < IntroGlobalNames.size(); ++index)
    {
        const std::unordered_map<std::string, int32_t>::const_iterator iterator =
            globals.find(IntroGlobalNames[index]);
        const int32_t actual = iterator != globals.end() ? iterator->second : 0;
        CHECK_EQ(actual, expected[index]);
    }
}

void checkSavedRuntimeGlobals(const GameSaveData &saved, const std::array<int32_t, 4> &expected)
{
    checkGlobals(saved.namedGlobalVars, expected);
    for (const auto &[map, snapshot] : saved.outdoorWorldStates)
    {
        INFO(map);
        REQUIRE(snapshot.eventRuntimeState.has_value());
        checkGlobals(snapshot.eventRuntimeState->namedGlobalVars, expected);
    }
}

std::filesystem::path fixturePath(const char *pName)
{
    const char *pTempDirectory = std::getenv("TMPDIR");
    REQUIRE_MESSAGE(pTempDirectory != nullptr, "TMPDIR must point at the external-storage test directory");
    return std::filesystem::path(pTempDirectory) / pName;
}

void saveAndReload(const std::filesystem::path &path, const GameSaveData &saved, GameSaveData &loaded)
{
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, saved, error), error.c_str());
    const std::optional<GameSaveData> result = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(result.has_value(), error.c_str());
    loaded = *result;
}

std::vector<std::vector<std::string>> parseTextTableRows(const std::filesystem::path &path)
{
    const std::optional<OpenYAMM::Engine::TextTable> table =
        OpenYAMM::Engine::TextTable::parseTabSeparated(readTextFile(path));
    REQUIRE(table.has_value());

    std::vector<std::vector<std::string>> rows;
    rows.reserve(table->getRowCount());
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    return rows;
}
}

TEST_CASE("Verdant intro boundary follows the original four-click dialogue and completes once")
{
    std::string error;
    const std::optional<ScriptedEventProgram> program = loadCompiledIntroProgram(error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    NpcDialogTable dialogs = makeBoundaryNpcDialogTable();
    EventRuntime runtime(nullptr, &dialogs);
    EventRuntimeState state;
    Party party = makeBoundaryParty();
    REQUIRE(party.member(0) != nullptr);
    const std::vector<uint32_t> initialExperiences = memberExperiences(party);
    const int initialGold = party.gold();
    const int initialFood = party.food();
    const size_t initialInventoryCount = party.inventoryItemCount();

    executeNpcEnter(runtime, program, state, party, VerdantNpcId);
    REQUIRE(state.npcTopicOverrides.contains(VerdantNpcId));
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1778);
    checkOnlyTopicVisible(runtime, program, state, party, 1778);

    executeIntroTopic(runtime, program, state, party, 1778, BoundaryFixtureMessages[0]);
    CHECK_EQ(globalValue(state, IntroGlobalNames[0]), 1);
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1779);
    checkOnlyTopicVisible(runtime, program, state, party, 1779);

    executeIntroTopic(runtime, program, state, party, 1779, BoundaryFixtureMessages[1]);
    CHECK_EQ(globalValue(state, IntroGlobalNames[1]), 1);
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1779);

    executeIntroTopic(runtime, program, state, party, 1779, BoundaryFixtureMessages[2]);
    CHECK_EQ(globalValue(state, IntroGlobalNames[2]), 1);
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1780);
    checkOnlyTopicVisible(runtime, program, state, party, 1780);

    executeIntroTopic(runtime, program, state, party, 1780, BoundaryFixtureMessages[3]);
    checkGlobals(state.namedGlobalVars, {1, 1, 1, 1});
    CHECK_EQ(state.npcGreetingOverrides.at(VerdantNpcId), 328);
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 0);
    for (uint16_t topicId : IntroTopicIds)
    {
        CHECK_FALSE(runtime.canShowTopic(program, topicId, state, &party));
    }
    CHECK_EQ(memberExperiences(party), initialExperiences);
    CHECK_EQ(party.gold(), initialGold);
    CHECK_EQ(party.food(), initialFood);
    CHECK_EQ(party.inventoryItemCount(), initialInventoryCount);
    CHECK_FALSE(party.hasQuestBit(1104));
    CHECK_FALSE(party.hasQuestBit(1105));
    CHECK(state.npcHouseOverrides.empty());
    CHECK(state.grantedItems.empty());
    CHECK(state.grantedItemIds.empty());

    const std::unordered_map<std::string, int32_t> completedGlobals = state.namedGlobalVars;
    const size_t completedMessageCount = state.messages.size();
    for (uint16_t topicId : IntroTopicIds)
    {
        REQUIRE(runtime.executeNpcTopicEventById(std::nullopt, program, topicId, state, &party));
        CHECK_EQ(state.messages.size(), completedMessageCount);
    }
    executeNpcEnter(runtime, program, state, party, VerdantNpcId);
    CHECK_EQ(state.namedGlobalVars, completedGlobals);
    CHECK_EQ(state.npcGreetingOverrides.at(VerdantNpcId), 328);
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 0);
    CHECK_EQ(memberExperiences(party), initialExperiences);
    CHECK_EQ(party.gold(), initialGold);
    CHECK_EQ(party.food(), initialFood);
    CHECK_EQ(party.inventoryItemCount(), initialInventoryCount);
    CHECK_FALSE(party.hasQuestBit(1104));
    CHECK_FALSE(party.hasQuestBit(1105));
    CHECK(state.npcHouseOverrides.empty());
}

TEST_CASE("Verdant intro boundary resets only its partial steps on Verdant reentry")
{
    std::string error;
    const std::optional<ScriptedEventProgram> program = loadCompiledIntroProgram(error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    NpcDialogTable dialogs = makeBoundaryNpcDialogTable();
    EventRuntime runtime(nullptr, &dialogs);
    EventRuntimeState state;
    Party party = makeBoundaryParty();

    executeNpcEnter(runtime, program, state, party, VerdantNpcId);
    executeIntroTopic(runtime, program, state, party, 1778, BoundaryFixtureMessages[0]);
    executeIntroTopic(runtime, program, state, party, 1779, BoundaryFixtureMessages[1]);
    executeIntroTopic(runtime, program, state, party, 1779, BoundaryFixtureMessages[2]);
    checkGlobals(state.namedGlobalVars, {1, 1, 1, 0});

    state.npcTopicOverrides[804] = {{0, 901}, {1, 902}, {2, 903}};
    const std::unordered_map<uint32_t, uint32_t> otherNpcTopics = state.npcTopicOverrides.at(804);
    executeNpcEnter(runtime, program, state, party, 804);
    checkGlobals(state.namedGlobalVars, {1, 1, 1, 0});
    CHECK_EQ(state.npcTopicOverrides.at(804), otherNpcTopics);

    executeNpcEnter(runtime, program, state, party, VerdantNpcId);
    checkGlobals(state.namedGlobalVars, {1, 0, 0, 0});
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1779);
    CHECK_EQ(state.npcTopicOverrides.at(804), otherNpcTopics);
    executeIntroTopic(runtime, program, state, party, 1779, BoundaryFixtureMessages[1]);
    checkGlobals(state.namedGlobalVars, {1, 1, 0, 0});
}

TEST_CASE("Verdant intro boundary survives cross-world cache and binary save boundaries")
{
    std::string error;
    const std::optional<ScriptedEventProgram> program = loadCompiledIntroProgram(error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    NpcDialogTable dialogs = makeBoundaryNpcDialogTable();
    EventRuntime runtime(nullptr, &dialogs);
    Party party = makeBoundaryParty();
    EventRuntimeState enroth;

    executeNpcEnter(runtime, program, enroth, party, VerdantNpcId);
    executeIntroTopic(runtime, program, enroth, party, 1778, BoundaryFixtureMessages[0]);
    executeIntroTopic(runtime, program, enroth, party, 1779, BoundaryFixtureMessages[1]);
    SessionFixture firstSession;
    captureOutdoor(firstSession.session, "oute3.odm", party, enroth);

    EventRuntimeState staleErathia;
    staleErathia.namedGlobalVars[IntroGlobalNames[1]] = 0;
    staleErathia.namedGlobalVars[IntroGlobalNames[2]] = 0;
    firstSession.session.applyNamedGlobalVarsToRuntime(staleErathia);
    captureOutdoor(firstSession.session, "7out02.odm", party, staleErathia);
    const std::optional<GameSaveData> partialSaved = firstSession.session.buildSaveData();
    REQUIRE(partialSaved.has_value());
    checkSavedRuntimeGlobals(*partialSaved, {1, 1, 0, 0});

    GameSaveData partialLoaded;
    saveAndReload(fixturePath("cross-continents-intro-partial-boundary.oysav"), *partialSaved, partialLoaded);
    checkSavedRuntimeGlobals(partialLoaded, {1, 1, 0, 0});
    SessionFixture secondSession;
    secondSession.session.restoreFromSaveData(partialLoaded);
    checkGlobals(secondSession.session.namedGlobalVars(), {1, 1, 0, 0});

    REQUIRE(secondSession.session.partyState().has_value());
    Party continuedParty = *secondSession.session.partyState();
    EventRuntimeState jadame;
    secondSession.session.applyNamedGlobalVarsToRuntime(jadame);
    executeNpcEnter(runtime, program, jadame, continuedParty, VerdantNpcId);
    checkGlobals(jadame.namedGlobalVars, {1, 0, 0, 0});
    executeIntroTopic(runtime, program, jadame, continuedParty, 1779, BoundaryFixtureMessages[1]);
    executeIntroTopic(runtime, program, jadame, continuedParty, 1779, BoundaryFixtureMessages[2]);
    executeIntroTopic(runtime, program, jadame, continuedParty, 1780, BoundaryFixtureMessages[3]);
    captureOutdoor(secondSession.session, "out01.odm", continuedParty, jadame);

    EventRuntimeState staleEnroth = enroth;
    staleEnroth.namedGlobalVars[IntroGlobalNames[1]] = 0;
    staleEnroth.namedGlobalVars[IntroGlobalNames[2]] = 0;
    secondSession.session.applyNamedGlobalVarsToRuntime(staleEnroth);
    captureOutdoor(secondSession.session, "oute3.odm", continuedParty, staleEnroth);
    const std::optional<GameSaveData> completedSaved = secondSession.session.buildSaveData();
    REQUIRE(completedSaved.has_value());
    checkSavedRuntimeGlobals(*completedSaved, {1, 1, 1, 1});

    GameSaveData completedLoaded;
    saveAndReload(fixturePath("cross-continents-intro-complete-boundary.oysav"), *completedSaved, completedLoaded);
    checkSavedRuntimeGlobals(completedLoaded, {1, 1, 1, 1});
    SessionFixture finalSession;
    finalSession.session.restoreFromSaveData(completedLoaded);
    checkGlobals(finalSession.session.namedGlobalVars(), {1, 1, 1, 1});

    EventRuntimeState postLoadStaleState;
    postLoadStaleState.namedGlobalVars[IntroGlobalNames[1]] = 0;
    postLoadStaleState.namedGlobalVars[IntroGlobalNames[2]] = 0;
    finalSession.session.applyNamedGlobalVarsToRuntime(postLoadStaleState);
    executeNpcEnter(runtime, program, postLoadStaleState, continuedParty, VerdantNpcId);
    checkGlobals(postLoadStaleState.namedGlobalVars, {1, 1, 1, 1});
    CHECK_EQ(postLoadStaleState.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 0);
    const size_t completedMessageCount = postLoadStaleState.messages.size();
    REQUIRE(runtime.executeNpcTopicEventById(std::nullopt, program, 1780, postLoadStaleState, &continuedParty));
    CHECK_EQ(postLoadStaleState.messages.size(), completedMessageCount);
}

TEST_CASE("actual converted Verdant intro boundary uses imported Chinese NPC text when requested")
{
    const char *pEventRoot = std::getenv("OPENYAMM_CC_EVENT_ROOT");
    const char *pBaseRoot = std::getenv("OPENYAMM_CC_BASE_ROOT");
    if (pEventRoot == nullptr && pBaseRoot == nullptr)
    {
        INFO("Set OPENYAMM_CC_EVENT_ROOT and OPENYAMM_CC_BASE_ROOT for converted-resource acceptance");
        return;
    }
    REQUIRE(pEventRoot != nullptr);
    REQUIRE(pBaseRoot != nullptr);

    const std::filesystem::path eventRoot(pEventRoot);
    const std::filesystem::path baseRoot(pBaseRoot);
    std::string error;
    const std::optional<ScriptedEventProgram> program = loadProgram(
        readTextFile(eventRoot / "engine/scripts/common/event_support.lua") + "\n"
            + readTextFile(eventRoot / "engine/events/Global.lua") + "\n"
            + readTextFile(eventRoot / "engine/events/Global_mmmerge_cross_continents_intro.lua"),
        error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());

    NpcDialogTable dialogs;
    REQUIRE(dialogs.loadTextsFromRows(
        parseTextTableRows(baseRoot / "engine/data_tables/npc_topic_text.txt")));
    std::array<std::string, 4> importedMessages;
    for (size_t index = 0; index < IntroTextIds.size(); ++index)
    {
        const std::optional<std::string> text = dialogs.getText(IntroTextIds[index]);
        REQUIRE(text.has_value());
        REQUIRE_FALSE(text->empty());
        importedMessages[index] = *text;
    }

    EventRuntime runtime(nullptr, &dialogs);
    EventRuntimeState state;
    Party party = makeBoundaryParty();
    executeNpcEnter(runtime, program, state, party, VerdantNpcId);
    executeIntroTopic(runtime, program, state, party, 1778, importedMessages[0]);
    executeIntroTopic(runtime, program, state, party, 1779, importedMessages[1]);
    executeIntroTopic(runtime, program, state, party, 1779, importedMessages[2]);
    executeIntroTopic(runtime, program, state, party, 1780, importedMessages[3]);
    checkGlobals(state.namedGlobalVars, {1, 1, 1, 1});
    CHECK_EQ(state.npcGreetingOverrides.at(VerdantNpcId), 328);
    CHECK_EQ(state.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 0);

    const char *pRuntimeRoot = std::getenv("OPENYAMM_CC_RUNTIME_ROOT");
    if (pRuntimeRoot == nullptr)
    {
        INFO("Set OPENYAMM_CC_RUNTIME_ROOT for GameDataLoader overlay-discovery acceptance");
        return;
    }

    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    const std::filesystem::path runtimeRoot(pRuntimeRoot);
    REQUIRE(assetFileSystem.initialize(
        runtimeRoot, runtimeRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
    REQUIRE(assetFileSystem.mountDevelopmentRoot(eventRoot));

    GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadCommonForGameplay(assetFileSystem));
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(assetFileSystem, "oute3.odm"));
    const std::optional<MapAssetInfo> &selectedMap = loader.getSelectedMap();
    REQUIRE(selectedMap.has_value());
    REQUIRE(selectedMap->globalEventProgram.has_value());
    REQUIRE(selectedMap->globalEventProgram->hasEvent(1778));
    REQUIRE(selectedMap->globalEventProgram->hasEvent(1779));
    REQUIRE(selectedMap->globalEventProgram->hasEvent(1780));
    REQUIRE(selectedMap->globalEventProgram->luaSourceName().has_value());
    CHECK(selectedMap->globalEventProgram->luaSourceName()->find(
        "events/Global_mmmerge_cross_continents_intro.lua") != std::string::npos);

    EventRuntime loadedRuntime(nullptr, &loader.getNpcDialogTable());
    EventRuntimeState loadedState;
    Party loadedParty = makeBoundaryParty();
    executeNpcEnter(loadedRuntime, selectedMap->globalEventProgram, loadedState, loadedParty, VerdantNpcId);
    CHECK_EQ(loadedState.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1778);
    const std::optional<std::string> loadedMessage = loader.getNpcDialogTable().getText(2149);
    REQUIRE(loadedMessage.has_value());
    executeIntroTopic(
        loadedRuntime, selectedMap->globalEventProgram, loadedState, loadedParty, 1778, *loadedMessage);
    CHECK_EQ(globalValue(loadedState, IntroGlobalNames[0]), 1);
    CHECK_EQ(loadedState.npcTopicOverrides.at(VerdantNpcId).at(IntroSlot), 1779);
}
