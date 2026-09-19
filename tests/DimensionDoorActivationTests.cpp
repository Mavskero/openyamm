#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "engine/AssetFileSystem.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/events/EventRuntime.h"
#include "game/events/ISceneEventContext.h"
#include "game/events/ScriptedEventTimer.h"
#include "game/gameplay/DimensionDoorState.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/outdoor/OutdoorMapData.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
constexpr uint16_t DimensionDoorHookEventId = 65490;
constexpr const char *GotFinalQuestVar = "MMerge.CrossContinents.GotFinalQuest";
constexpr const char *QuestFinishedVar = "MMerge.CrossContinents.QuestFinished";

std::filesystem::path requiredEnvironmentPath(const char *pName)
{
    const char *pValue = std::getenv(pName);
    REQUIRE_MESSAGE(pValue != nullptr, (std::string("Missing required environment variable ") + pName).c_str());
    REQUIRE_MESSAGE(*pValue != '\0', (std::string("Empty required environment variable ") + pName).c_str());
    return pValue;
}

std::optional<ScriptedEventProgram> loadSupportProgram(
    ScriptedEventScope scope,
    const std::string &source)
{
    std::string error;
    std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + "\n" + source,
        scope == ScriptedEventScope::Map ? "@metadata_alias_map.lua" : "@metadata_alias_global.lua",
        scope,
        error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    return program;
}

struct PartySnapshot
{
    int gold = 0;
    int food = 0;
    uint32_t experience = 0;
    int health = 0;
    int spellPoints = 0;
    size_t itemCount = 0;
};

Party makeBoundaryParty()
{
    Character member;
    member.name = "Dimension Door boundary member";
    member.className = "Sorcerer";
    member.role = "Sorcerer";
    member.level = 10;
    member.experience = 987;
    member.maxHealth = 50;
    member.health = 41;
    member.maxSpellPoints = 80;
    member.spellPoints = 67;

    PartySeed seed;
    seed.gold = 1234;
    seed.food = 7;
    seed.members.push_back(member);

    Party party;
    party.seed(seed);
    return party;
}

PartySnapshot snapshotParty(const Party &party)
{
    const Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    return {
        party.gold(),
        party.food(),
        pMember->experience,
        pMember->health,
        pMember->spellPoints,
        party.inventoryItemCount(),
    };
}

void checkPartySnapshot(const Party &party, const PartySnapshot &expected)
{
    const PartySnapshot actual = snapshotParty(party);
    CHECK_EQ(actual.gold, expected.gold);
    CHECK_EQ(actual.food, expected.food);
    CHECK_EQ(actual.experience, expected.experience);
    CHECK_EQ(actual.health, expected.health);
    CHECK_EQ(actual.spellPoints, expected.spellPoints);
    CHECK_EQ(actual.itemCount, expected.itemCount);
}

class DimensionDoorTestWorld :
    public OpenYAMM::Tests::PartySpellTestWorldRuntime,
    public ISceneEventContext
{
public:
    void bindPrograms(
        Party *pParty,
        EventRuntimeState *pState,
        const std::optional<ScriptedEventProgram> *pLocalProgram,
        const std::optional<ScriptedEventProgram> *pGlobalProgram)
    {
        m_pParty = pParty;
        m_pState = pState;
        m_pLocalProgram = pLocalProgram;
        m_pGlobalProgram = pGlobalProgram;
        bindParty(pParty);
        bindEventRuntimeState(pState);
        bindGlobalEventProgram(pGlobalProgram);
    }

    bool executeEventHooks(EventRuntimeHookKind kind) override
    {
        if (m_pState == nullptr)
        {
            return false;
        }
        EventRuntime runtime;
        const bool executed = runtime.executeHooks(
            localProgram(), globalProgramValue(), kind, *m_pState, m_pParty, this);
        applyPartyState(executed);
        return executed;
    }

    bool executeMapEvent(
        uint16_t eventId,
        size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        if (m_pState == nullptr)
        {
            return false;
        }
        previousMessageCount = m_pState->messages.size();
        EventRuntime runtime;
        const bool executed = runtime.executeEventById(
            localProgram(),
            globalProgramValue(),
            eventId,
            *m_pState,
            m_pParty,
            this,
            continueStep);
        applyPartyState(executed);
        return executed;
    }

    bool executeTimerEvent(const ScriptedEventTimerDefinition &definition)
    {
        if (m_pState == nullptr)
        {
            return false;
        }
        EventRuntime runtime;
        const bool executed = runtime.executeEventById(
            localProgram(),
            globalProgramValue(),
            definition.eventId,
            *m_pState,
            m_pParty,
            this,
            std::nullopt,
            true,
            definition.scope,
            true);
        applyPartyState(executed);
        return executed;
    }

    float currentGameMinutes() const override
    {
        return gameMinutes();
    }

    const MapDeltaData *mapDeltaData() const override
    {
        return nullptr;
    }

    MapDeltaData *mapDeltaData() override
    {
        return nullptr;
    }

    bool setFacetBit(uint32_t, uint32_t, bool) override
    {
        return false;
    }

    bool castEventSpell(
        uint32_t spellId,
        uint32_t skillLevel,
        uint32_t skillMastery,
        int32_t fromX,
        int32_t fromY,
        int32_t fromZ,
        int32_t toX,
        int32_t toY,
        int32_t toZ) override
    {
        return OpenYAMM::Tests::PartySpellTestWorldRuntime::castEventSpell(
            spellId, skillLevel, skillMastery, fromX, fromY, fromZ, toX, toY, toZ);
    }

    bool summonMonsters(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t) override
    {
        return false;
    }

    bool summonEventItem(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override
    {
        return false;
    }

    bool summonEventObject(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override
    {
        return false;
    }

    bool checkMonstersKilled(uint32_t, uint32_t, uint32_t, bool) const override
    {
        return false;
    }

private:
    const std::optional<ScriptedEventProgram> &localProgram() const
    {
        static const std::optional<ScriptedEventProgram> Empty;
        return m_pLocalProgram != nullptr ? *m_pLocalProgram : Empty;
    }

    const std::optional<ScriptedEventProgram> &globalProgramValue() const
    {
        static const std::optional<ScriptedEventProgram> Empty;
        return m_pGlobalProgram != nullptr ? *m_pGlobalProgram : Empty;
    }

    void applyPartyState(bool executed)
    {
        if (executed && m_pParty != nullptr)
        {
            m_pParty->applyEventRuntimeState(*m_pState);
        }
    }

    Party *m_pParty = nullptr;
    EventRuntimeState *m_pState = nullptr;
    const std::optional<ScriptedEventProgram> *m_pLocalProgram = nullptr;
    const std::optional<ScriptedEventProgram> *m_pGlobalProgram = nullptr;
};

struct ActualDimensionDoorFixture
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository repository;
    GameSession session;
    Party party = makeBoundaryParty();
    EventRuntimeState eventState;
    DimensionDoorTestWorld world;

    explicit ActualDimensionDoorFixture(const std::string &mapFileName, float initialGameMinutes = 0.0f)
    {
        const std::filesystem::path runtimeRoot = requiredEnvironmentPath("OPENYAMM_CC_RUNTIME_ROOT");
        const std::filesystem::path eventRoot = requiredEnvironmentPath("OPENYAMM_CC_EVENT_ROOT");

        REQUIRE(files.initialize(runtimeRoot, runtimeRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
        REQUIRE(files.mountDevelopmentRoot(eventRoot));
        loader.setActiveWorldId("mm6");
        REQUIRE(loader.loadCommonForGameplay(files));
        loader.setActiveWorldId("mm6");
        REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(files, mapFileName));
        REQUIRE(loader.getSelectedMap().has_value());
        REQUIRE(loader.getSelectedMap()->localEventProgram.has_value());

        repository.bind(loader);
        session.bindDataRepository(&repository);
        session.setCurrentMapFileName(mapFileName);
        eventState.mapFileName = mapFileName;
        world.bindPrograms(
            &party,
            &eventState,
            &loader.getSelectedMap()->localEventProgram,
            &loader.getSelectedMap()->globalEventProgram);
        world.bindMonsterTable(&loader.getMonsterTable());
        world.bindMergedBolsterMonsterTable(&loader.getMergedBolsterMonsterTable());
        world.setMapName(mapFileName);
        world.advanceGameMinutes(initialGameMinutes);
        session.bindActiveWorldRuntime(&world);
    }

    GameplayScreenRuntime &runtime()
    {
        return session.gameplayScreenRuntime();
    }

    const std::optional<ScriptedEventProgram> &localProgram() const
    {
        return loader.getSelectedMap()->localEventProgram;
    }

    const std::optional<ScriptedEventProgram> &globalProgram() const
    {
        return loader.getSelectedMap()->globalEventProgram;
    }

    bool runMapEvent(uint16_t eventId)
    {
        size_t previousMessageCount = eventState.messages.size();
        return world.executeMapEvent(eventId, previousMessageCount);
    }

    void rebindAfterSessionReset(const std::string &mapFileName)
    {
        session.bindDataRepository(&repository);
        session.setCurrentMapFileName(mapFileName);
        session.bindActiveWorldRuntime(&world);
    }
};
}

TEST_CASE("Dimension Door timer keeps the original strict eight-minute and duplicate removal boundaries")
{
    DimensionDoorState state;
    state.activate(2881.0);
    CHECK(state.active);
    CHECK_EQ(state.destinationDayIndex, 2u);
    REQUIRE_EQ(state.reversionTimers.size(), 1u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(2889.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(8.0));

    state.update(2888.999, false);
    CHECK(state.active);
    state.update(2889.0, false);
    CHECK_FALSE(state.active);
    CHECK(state.reversionTimers.empty());

    state = {};
    state.activate(100.0);
    state.activate(107.0);
    REQUIRE_EQ(state.reversionTimers.size(), 2u);
    state.update(108.0, false);
    CHECK_FALSE(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 1u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(115.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(8.0));
}

TEST_CASE("Dimension Door deferral preserves original first-matching timer removal after list shifting")
{
    DimensionDoorState state;
    state.activate(100.0);
    state.activate(107.0);

    state.update(108.0, true);
    CHECK(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 2u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(116.0));
    CHECK_EQ(state.reversionTimers[1].deadlineGameMinutes, doctest::Approx(115.0));

    state.update(115.0, false);
    CHECK_FALSE(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 1u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(123.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(8.0));
    state.update(123.0, false);
    CHECK(state.reversionTimers.empty());
}

TEST_CASE("Dimension Door scroll and natural activations keep independent timer intervals")
{
    DimensionDoorState state;
    state.activate(100.0, DimensionDoorActivationSource::ItemScroll);
    state.activate(100.0, DimensionDoorActivationSource::NaturalLocation);
    REQUIRE_EQ(state.reversionTimers.size(), 2u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(101.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(1.0));
    CHECK_EQ(state.reversionTimers[1].deadlineGameMinutes, doctest::Approx(108.0));
    CHECK_EQ(state.reversionTimers[1].intervalGameMinutes, doctest::Approx(8.0));

    state.update(100.999, false);
    CHECK(state.active);
    state.update(101.0, false);
    CHECK_FALSE(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 1u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(108.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(8.0));
}

TEST_CASE("later scroll firing first removes earlier natural callback and retains its one-minute cadence")
{
    DimensionDoorState state;
    state.activate(100.0, DimensionDoorActivationSource::NaturalLocation);
    state.activate(101.0, DimensionDoorActivationSource::ItemScroll);
    REQUIRE_EQ(state.reversionTimers.size(), 2u);

    state.update(101.999, false);
    CHECK(state.active);
    state.update(102.0, false);
    CHECK_FALSE(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 1u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(103.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(1.0));
    state.update(103.0, false);
    CHECK(state.reversionTimers.empty());
}

TEST_CASE("screen three defers each mixed timer by its own interval")
{
    DimensionDoorState state;
    state.activate(100.0, DimensionDoorActivationSource::NaturalLocation);
    state.activate(101.0, DimensionDoorActivationSource::ItemScroll);

    state.update(102.0, true);
    CHECK(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 2u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(108.0));
    CHECK_EQ(state.reversionTimers[1].deadlineGameMinutes, doctest::Approx(103.0));
    CHECK_EQ(state.reversionTimers[1].intervalGameMinutes, doctest::Approx(1.0));

    state.update(102.5, false);
    CHECK(state.active);
    state.update(103.0, false);
    CHECK_FALSE(state.active);
    REQUIRE_EQ(state.reversionTimers.size(), 1u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(104.0));
    CHECK_EQ(state.reversionTimers[0].intervalGameMinutes, doctest::Approx(1.0));
}

TEST_CASE("mixed timers sharing a deadline both follow original same-frame removal")
{
    DimensionDoorState state;
    state.activate(100.0, DimensionDoorActivationSource::NaturalLocation);
    state.activate(107.0, DimensionDoorActivationSource::ItemScroll);
    REQUIRE_EQ(state.reversionTimers.size(), 2u);
    CHECK_EQ(state.reversionTimers[0].deadlineGameMinutes, doctest::Approx(108.0));
    CHECK_EQ(state.reversionTimers[1].deadlineGameMinutes, doctest::Approx(108.0));

    state.update(108.0, false);
    CHECK_FALSE(state.active);
    CHECK(state.reversionTimers.empty());
}

TEST_CASE("event support metadata updates preserve map and global lifecycle arrays exactly once")
{
    const std::optional<ScriptedEventProgram> mapProgram = loadSupportProgram(
        ScriptedEventScope::Map,
        R"lua(
RegisterEvent(10, nil, function() end)
RegisterEvent(11, nil, function() end)
RegisterEvent(12, nil, function() end)
RegisterEvent(20, nil, function() end)
RegisterEvent(21, nil, function() end)
RegisterEvent(22, nil, function() end)
SetMapMetadata({
    onLoad = {10},
    onLeave = {11},
    timers = {{
        eventId = 12,
        sourceEventId = 12,
        origin = "native",
        scheduleKind = "relative",
        repeating = true,
        intervalGameMinutes = 1,
        initialDelayGameMinutes = 1,
    }},
})
-- This partial update aliases the existing lifecycle arrays and must not append them to themselves.
SetMapMetadata({outdoorModelEvents = {AliasProbe = 20}})
SetMapMetadata({
    onLoad = {20},
    onLeave = {21},
    timers = {{
        eventId = 22,
        sourceEventId = 22,
        origin = "native",
        scheduleKind = "relative",
        repeating = true,
        intervalGameMinutes = 2,
        initialDelayGameMinutes = 2,
    }},
})
)lua");
    REQUIRE(mapProgram.has_value());
    REQUIRE_EQ(mapProgram->onLoadEventIds().size(), 2u);
    CHECK_EQ(mapProgram->onLoadEventIds()[0], 20u);
    CHECK_EQ(mapProgram->onLoadEventIds()[1], 10u);
    REQUIRE_EQ(mapProgram->onLeaveEventIds().size(), 2u);
    CHECK_EQ(mapProgram->onLeaveEventIds()[0], 21u);
    CHECK_EQ(mapProgram->onLeaveEventIds()[1], 11u);
    REQUIRE_EQ(mapProgram->timerTriggers().size(), 2u);
    CHECK_EQ(mapProgram->timerTriggers()[0].eventId, 22u);
    CHECK_EQ(mapProgram->timerTriggers()[1].eventId, 12u);
    const auto modelEvent = mapProgram->outdoorModelEvents().find("AliasProbe");
    REQUIRE(modelEvent != mapProgram->outdoorModelEvents().end());
    CHECK_EQ(modelEvent->second, 20u);

    const std::optional<ScriptedEventProgram> globalProgram = loadSupportProgram(
        ScriptedEventScope::Global,
        R"lua(
RegisterGlobalEvent(30, nil, function() end)
RegisterGlobalEvent(31, nil, function() end)
RegisterGlobalEvent(32, nil, function() end)
RegisterGlobalEvent(40, nil, function() end)
RegisterGlobalEvent(41, nil, function() end)
RegisterGlobalEvent(42, nil, function() end)
SetGlobalMetadata({
    onLoad = {30},
    onLeave = {31},
    timers = {{
        eventId = 32,
        sourceEventId = 32,
        origin = "native",
        scheduleKind = "relative",
        repeating = true,
        intervalGameMinutes = 1,
        initialDelayGameMinutes = 1,
    }},
})
-- Exercise the same alias path for the global scope without replacing any lifecycle array.
SetGlobalMetadata({aliasProbe = true})
SetGlobalMetadata({
    onLoad = {40},
    onLeave = {41},
    timers = {{
        eventId = 42,
        sourceEventId = 42,
        origin = "native",
        scheduleKind = "relative",
        repeating = true,
        intervalGameMinutes = 2,
        initialDelayGameMinutes = 2,
    }},
})
)lua");
    REQUIRE(globalProgram.has_value());
    REQUIRE_EQ(globalProgram->onLoadEventIds().size(), 2u);
    CHECK_EQ(globalProgram->onLoadEventIds()[0], 40u);
    CHECK_EQ(globalProgram->onLoadEventIds()[1], 30u);
    REQUIRE_EQ(globalProgram->onLeaveEventIds().size(), 2u);
    CHECK_EQ(globalProgram->onLeaveEventIds()[0], 41u);
    CHECK_EQ(globalProgram->onLeaveEventIds()[1], 31u);
    REQUIRE_EQ(globalProgram->timerTriggers().size(), 2u);
    CHECK_EQ(globalProgram->timerTriggers()[0].eventId, 42u);
    CHECK_EQ(globalProgram->timerTriggers()[1].eventId, 32u);
}

TEST_CASE("actual map overlays activate Dimension Door only on the four audited terrain tiles")
{
    struct TileCase
    {
        const char *pMap;
        int tileX;
        int tileY;
    };
    constexpr std::array<TileCase, 4> Cases = {{
        {"out01.odm", 63, 59},
        {"out07.odm", 43, 98},
        {"outb3.odm", 89, 47},
        {"7out04.odm", 84, 94},
    }};

    for (const TileCase &testCase : Cases)
    {
        CAPTURE(testCase.pMap);
        CAPTURE(testCase.tileX);
        CAPTURE(testCase.tileY);
        ActualDimensionDoorFixture fixture(testCase.pMap, 100.0f);
        fixture.runtime().dispatchOutdoorStep(testCase.tileX + 1, testCase.tileY);
        CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

        fixture.runtime().dispatchOutdoorStep(testCase.tileX, testCase.tileY);
        CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
        CHECK_EQ(fixture.eventState.pendingDimensionDoorActivations, 0u);
        CHECK_FALSE(fixture.eventState.pendingDimensionDoorOverlay);
    }
}

TEST_CASE("actual replacement events activate without retaining unrelated base rewards")
{
    struct EventCase
    {
        const char *pMap;
        uint16_t eventId;
    };
    constexpr std::array<EventCase, 5> Cases = {{
        {"out07.odm", 500},
        {"out09.odm", 6},
        {"outb3.odm", 105},
        {"oute3.odm", 140},
        {"7out04.odm", 504},
    }};

    for (const EventCase &testCase : Cases)
    {
        CAPTURE(testCase.pMap);
        CAPTURE(testCase.eventId);
        ActualDimensionDoorFixture fixture(testCase.pMap, 200.0f);
        const PartySnapshot before = snapshotParty(fixture.party);
        REQUIRE(fixture.runMapEvent(testCase.eventId));
        fixture.runtime().updateDimensionDoor();
        CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
        CHECK_EQ(fixture.eventState.pendingDimensionDoorActivations, 0u);
        CHECK_FALSE(fixture.eventState.pendingDimensionDoorOverlay);
        checkPartySnapshot(fixture.party, before);
    }

    for (uint32_t guardedValue : {1u, 2u})
    {
        CAPTURE(guardedValue);
        ActualDimensionDoorFixture guarded("oute3.odm", 200.0f);
        guarded.eventState.mapVars[50] = guardedValue;
        const PartySnapshot before = snapshotParty(guarded.party);
        REQUIRE(guarded.runMapEvent(140));
        guarded.runtime().updateDimensionDoor();
        CHECK_FALSE(guarded.runtime().shouldTownPortalCastOpenDimensionDoor());
        CHECK_FALSE(guarded.eventState.pendingDimensionDoorOverlay);
        checkPartySnapshot(guarded.party, before);
    }
}

TEST_CASE("actual out09 timer starts at minute three, repeats each minute, and uses a strict radius")
{
    ActualDimensionDoorFixture fixture("out09.odm");
    const std::vector<ScriptedEventTimerDefinition> allDefinitions =
        scriptedEventTimerDefinitionsFromPrograms(fixture.localProgram(), fixture.globalProgram());
    std::vector<ScriptedEventTimerDefinition> definitions;
    std::copy_if(
        allDefinitions.begin(),
        allDefinitions.end(),
        std::back_inserter(definitions),
        [](const ScriptedEventTimerDefinition &definition)
        {
            return definition.origin == ScriptedEventTimerOrigin::Native
                && definition.eventId == DimensionDoorHookEventId;
        });
    REQUIRE_EQ(definitions.size(), 1u);
    CHECK(definitions[0].scheduleKind == ScriptedEventTimerScheduleKind::Interval);
    CHECK(definitions[0].repeating);
    CHECK_EQ(definitions[0].intervalGameMinutes, doctest::Approx(1.0));
    CHECK_EQ(definitions[0].startMinute, 3);

    std::vector<ScriptedEventTimerState> timers =
        reconcileScriptedEventTimers({}, definitions, 0.0, 0, false);
    REQUIRE_EQ(timers.size(), 1u);
    CHECK_EQ(timers[0].nextAlarmGameMinutes, doctest::Approx(3.0));

    fixture.world.setPartyPosition(-3621.0f, 98.0f, 0.0f);
    fixture.world.advanceGameMinutes(2.99f);
    CHECK_FALSE(updateScriptedEventTimers(
        timers,
        2.99,
        [&fixture](const ScriptedEventTimerDefinition &definition)
        {
            return fixture.world.executeTimerEvent(definition);
        }));
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    fixture.world.advanceGameMinutes(0.01f);
    REQUIRE(updateScriptedEventTimers(
        timers,
        3.0,
        [&fixture](const ScriptedEventTimerDefinition &definition)
        {
            return fixture.world.executeTimerEvent(definition);
        }));
    fixture.runtime().updateDimensionDoor();
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    CHECK_EQ(timers[0].nextAlarmGameMinutes, doctest::Approx(4.0));

    fixture.world.setPartyPosition(-3621.25f, 98.0f, 0.0f);
    fixture.world.advanceGameMinutes(1.0f);
    REQUIRE(updateScriptedEventTimers(
        timers,
        4.0,
        [&fixture](const ScriptedEventTimerDefinition &definition)
        {
            return fixture.world.executeTimerEvent(definition);
        }));
    fixture.runtime().updateDimensionDoor();
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    const int64_t lastVisit = legacyTimerTicksFromGameMinutes(7.5);
    std::vector<ScriptedEventTimerState> catchup =
        reconcileScriptedEventTimers({}, definitions, 10.0, lastVisit, false);
    REQUIRE_EQ(catchup.size(), 1u);
    CHECK_EQ(catchup[0].nextAlarmGameMinutes, doctest::Approx(8.0));

    int catchupExecutions = 0;
    REQUIRE(updateScriptedEventTimers(
        catchup,
        10.0,
        [&catchupExecutions](const ScriptedEventTimerDefinition &)
        {
            ++catchupExecutions;
            return true;
        }));
    CHECK_EQ(catchupExecutions, 1);
    CHECK_EQ(catchup[0].nextAlarmGameMinutes, doctest::Approx(9.0));
    CHECK_FALSE(updateScriptedEventTimers(
        catchup,
        10.0,
        [&catchupExecutions](const ScriptedEventTimerDefinition &)
        {
            ++catchupExecutions;
            return true;
        }));
    CHECK_EQ(catchupExecutions, 1);
    REQUIRE(updateScriptedEventTimers(
        catchup,
        10.01,
        [&catchupExecutions](const ScriptedEventTimerDefinition &)
        {
            ++catchupExecutions;
            return true;
        }));
    CHECK_EQ(catchupExecutions, 2);
    CHECK_EQ(catchup[0].nextAlarmGameMinutes, doctest::Approx(11.0));

    const int64_t justBeforeMinuteEight = legacyTimerTicksFromGameMinutes(8.0) - 1;
    const std::vector<ScriptedEventTimerState> plusOneTick =
        reconcileScriptedEventTimers({}, definitions, 10.0, justBeforeMinuteEight, false);
    REQUIRE_EQ(plusOneTick.size(), 1u);
    CHECK_EQ(plusOneTick[0].nextAlarmGameMinutes, doctest::Approx(9.0));
}

TEST_CASE("actual 7out04 model metadata routes every ClL1_W face to event 504")
{
    ActualDimensionDoorFixture fixture("7out04.odm");
    REQUIRE(fixture.loader.getSelectedMap()->outdoorMapData.has_value());
    const std::vector<OutdoorBModel> &models = fixture.loader.getSelectedMap()->outdoorMapData->bmodels;
    const auto iterator = std::find_if(models.begin(), models.end(), [](const OutdoorBModel &model)
    {
        return model.name == "ClL1_W";
    });
    REQUIRE(iterator != models.end());
    REQUIRE_FALSE(iterator->faces.empty());
    for (const OutdoorBModelFace &face : iterator->faces)
    {
        CHECK_EQ(face.cogTriggeredNumber, 504u);
    }
}

TEST_CASE("actual out02 final gate uses strict radius and routes Town Portal by quest completion")
{
    ActualDimensionDoorFixture fixture("out02.odm", 300.0f);
    fixture.world.setPartyPosition(15103.0f, -9759.0f, 0.0f);

    fixture.runtime().dispatchOutdoorStep(0, 0);
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    CHECK_FALSE(fixture.runtime().tryRunTownPortalHook());
    CHECK_FALSE(fixture.eventState.pendingMapMove.has_value());

    fixture.eventState.namedGlobalVars[GotFinalQuestVar] = 1;
    fixture.world.setPartyPosition(15703.0f, -9759.0f, 0.0f);
    fixture.runtime().dispatchOutdoorStep(0, 0);
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    CHECK_FALSE(fixture.runtime().tryRunTownPortalHook());
    CHECK_FALSE(fixture.eventState.pendingMapMove.has_value());

    fixture.world.setPartyPosition(15702.75f, -9759.0f, 0.0f);
    fixture.runtime().dispatchOutdoorStep(0, 0);
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    REQUIRE(fixture.runtime().tryRunTownPortalHook());
    REQUIRE(fixture.eventState.pendingMapMove.has_value());
    REQUIRE(fixture.eventState.pendingMapMove->mapName.has_value());
    CHECK_EQ(*fixture.eventState.pendingMapMove->mapName, "BrAlvar.odm");

    fixture.eventState.pendingMapMove.reset();
    fixture.eventState.namedGlobalVars[QuestFinishedVar] = 1;
    REQUIRE(fixture.runtime().tryRunTownPortalHook());
    REQUIRE(fixture.eventState.pendingMapMove.has_value());
    REQUIRE(fixture.eventState.pendingMapMove->mapName.has_value());
    CHECK_EQ(*fixture.eventState.pendingMapMove->mapName, "Breach.odm");
}

TEST_CASE("runtime defers reversion for Lloyd and journal Info screens until the next eight-minute alarm")
{
    ActualDimensionDoorFixture fixture("out01.odm", 100.0f);
    fixture.runtime().dispatchOutdoorStep(63, 59);
    REQUIRE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    GameplayUiController::UtilitySpellOverlayState &overlay = fixture.runtime().utilitySpellOverlay();
    overlay.active = true;
    overlay.mode = GameplayUiController::UtilitySpellOverlayMode::LloydsBeacon;
    fixture.world.advanceGameMinutes(8.0f);
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    overlay = {};
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    fixture.world.advanceGameMinutes(8.0f);
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    fixture.runtime().dispatchOutdoorStep(63, 59);
    REQUIRE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    fixture.runtime().journalScreenState().active = true;
    fixture.world.advanceGameMinutes(8.0f);
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    fixture.runtime().journalScreenState().active = false;
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    fixture.world.advanceGameMinutes(8.0f);
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
}

TEST_CASE("session and map lifetime reset Dimension Door while a same-map bind preserves it")
{
    ActualDimensionDoorFixture fixture("out01.odm", 400.0f);
    fixture.runtime().dispatchOutdoorStep(63, 59);
    REQUIRE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    fixture.session.setCurrentMapFileName("out01.odm");
    fixture.session.bindActiveWorldRuntime(&fixture.world);
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    fixture.session.setCurrentMapFileName("out07.odm");
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    fixture.session.setCurrentMapFileName("out01.odm");
    fixture.runtime().dispatchOutdoorStep(63, 59);
    REQUIRE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    fixture.session.clear();
    fixture.rebindAfterSessionReset("out01.odm");
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());

    fixture.runtime().dispatchOutdoorStep(63, 59);
    REQUIRE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    GameSaveData save;
    save.currentSceneKind = SceneKind::Outdoor;
    save.mapFileName = "out01.odm";
    save.savedGameMinutes = fixture.world.gameMinutes();
    save.party = fixture.party.snapshot();
    fixture.session.restoreFromSaveData(save);
    fixture.rebindAfterSessionReset("out01.odm");
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
}
