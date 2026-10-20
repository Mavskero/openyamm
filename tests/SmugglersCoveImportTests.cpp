#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "game/events/ISceneEventContext.h"
#include "game/events/ScriptedEventTimer.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/party/Party.h"
#include "tools/mmerge_import/SmugglersCoveResources.h"
#include "tools/mmerge_import/LodResources.h"
#include "tools/LegacyLuaExport.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
constexpr std::string_view RelativePath = "Scripts/Maps/d07.lua";
constexpr std::string_view ExpectedSha256 =
    "a2a6d4166098658ebe87d1923739f5b2f2d96a4fc932c7737aec06e080334a26";

std::filesystem::path originalSource()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

class SourceFixture
{
public:
    explicit SourceFixture(std::string_view name)
    {
        const char *pTempDirectory = std::getenv("TMPDIR");
        if (pTempDirectory == nullptr || *pTempDirectory == '\0')
        {
            throw std::runtime_error("TMPDIR must point at the external-storage test directory");
        }
        const uint64_t nonce = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        m_container = std::filesystem::path(pTempDirectory)
            / ("openyamm_smugglers_profile_" + std::string(name) + "_" + std::to_string(nonce));
        m_source = m_container / "source";
        std::filesystem::create_directories(m_source);
    }

    ~SourceFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_container, error);
    }

    const std::filesystem::path &source() const
    {
        return m_source;
    }

    std::filesystem::path copyOriginal()
    {
        const std::filesystem::path destination = m_source / RelativePath;
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(originalSource() / RelativePath, destination);
        return destination;
    }

    std::filesystem::path outside(std::string_view name) const
    {
        return m_container / name;
    }

private:
    std::filesystem::path m_container;
    std::filesystem::path m_source;
};

std::optional<ScriptedEventProgram> loadProfileProgram(const SmugglersCoveProfile &profile)
{
    const std::string generatedBase = R"OY_BASE(
SetMapMetadata({onLoad = {1}})
RegisterEvent(1, nil, function()
    SetQBit(QBit(999))
end)
)OY_BASE";
    std::string error;
    const std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(EventSupportSource) + "\n" + generatedBase + "\n" + profile.luaSource,
        "@d07.lua+smugglers-cove", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    return program;
}

class VisitContext : public ISceneEventContext
{
public:
    float minutes = 0;
    MapDeltaData delta;

    float currentGameMinutes() const override
    {
        return minutes;
    }
    const MapDeltaData *mapDeltaData() const override
    {
        return &delta;
    }
    MapDeltaData *mapDeltaData() override
    {
        return &delta;
    }
    bool setFacetBit(uint32_t, uint32_t, bool) override
    {
        return false;
    }
    bool castEventSpell(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        int32_t, int32_t, int32_t) override
    {
        return false;
    }
    bool summonMonsters(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        uint32_t, uint32_t) override
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
};
}

TEST_CASE("Smugglers Cove profile recognizes only the audited original script")
{
    const std::optional<SmugglersCoveProfile> profile = loadSmugglersCoveProfile(originalSource());
    REQUIRE(profile.has_value());
    CHECK_EQ(profile->mapStem, "d07");
    CHECK_EQ(profile->sourceSha256, ExpectedSha256);
    CHECK_FALSE(profile->luaSource.empty());
}

TEST_CASE("Smugglers Cove profile stays absent when its original map script is unavailable")
{
    SourceFixture fixture("absent");
    CHECK_FALSE(loadSmugglersCoveProfile(fixture.source()).has_value());
}

TEST_CASE("Smugglers Cove profile rejects modified or escaping source scripts")
{
    SUBCASE("modified script")
    {
        SourceFixture fixture("modified");
        const std::filesystem::path script = fixture.copyOriginal();
        std::ofstream(script, std::ios::binary | std::ios::app) << "\n-- modified\n";
        CHECK_THROWS_AS(loadSmugglersCoveProfile(fixture.source()), std::runtime_error);
    }

    SUBCASE("escaping symlink")
    {
        SourceFixture fixture("escaping");
        const std::filesystem::path outside = fixture.outside("outside_d07.lua");
        std::filesystem::copy_file(originalSource() / RelativePath, outside);
        const std::filesystem::path link = fixture.source() / RelativePath;
        std::filesystem::create_directories(link.parent_path());
        std::filesystem::create_symlink(outside, link);
        CHECK_THROWS_AS(loadSmugglersCoveProfile(fixture.source()), std::runtime_error);
    }
}


TEST_CASE("Smugglers Cove original entry states use completion anger and last visit")
{
    const SmugglersCoveProfile profile = *loadSmugglersCoveProfile(originalSource());
    const std::optional<ScriptedEventProgram> program = loadProfileProgram(profile);
    REQUIRE_FALSE(program->hasEvent(1));
    EventRuntime runtime;
    const uint32_t hostile = uint32_t(EvtActorAttribute::Hostile);
    const uint32_t invisible = uint32_t(EvtActorAttribute::Invisible);

    // A kill's age is deliberately irrelevant: expiry is measured from the last map visit.
    for (const bool completed : {false, true})
    {
        for (const bool angry : {false, true})
        {
            for (const float elapsed : {0.0f, 1439.5f, 1440.0f, 1440.5f})
            {
                CAPTURE(completed);
                CAPTURE(angry);
                CAPTURE(elapsed);
                Party party;
                party.setQuestBit(10, completed);
                EventRuntimeState state;
                state.namedMapVars["WereratsMad"] = angry ? 1 : 0;
                state.mapVars[9] = 7;
                state.mapVars[11] = 6;
                VisitContext context;
                context.delta.locationTime.lastVisitTime = legacyTimerTicksFromGameMinutes(100.0);
                context.minutes = 100.0f + elapsed;
                REQUIRE(runtime.executeOnLoadEvents(program, std::nullopt, state, &party, &context));
                const bool remainsAngry = angry && elapsed <= 1440.0f;
                const bool peaceful = completed && !remainsAngry;
                CHECK_EQ(state.namedMapVars["WereratsMad"], remainsAngry ? 1 : 0);
                for (const uint32_t group : {8u, 10u, 11u})
                {
                    CHECK_EQ(state.actorGroupSetMasks[group] & hostile, peaceful ? 0u : hostile);
                    CHECK_EQ(state.actorGroupClearMasks[group] & hostile, peaceful ? hostile : 0u);
                }
                CHECK_EQ(state.actorGroupSetMasks[8] & invisible, peaceful ? invisible : 0u);
                CHECK_EQ(state.actorGroupClearMasks[8] & invisible, peaceful ? 0u : invisible);
                CHECK_EQ(state.actorGroupSetMasks[11] & invisible, peaceful ? 0u : invisible);
                CHECK_EQ(state.actorGroupClearMasks[11] & invisible, peaceful ? invisible : 0u);
                // The current EVT runtime maps raw MM8 0x86 to its canonical slot11.
                CHECK_EQ(state.mapVars[9], 7);
                CHECK_EQ(state.mapVars[11], peaceful ? 6 : 2);
                CHECK_FALSE(party.hasQuestBit(999));
                CHECK_EQ(party.hasQuestBit(10), completed);
            }
        }
    }
}

TEST_CASE("Smugglers Cove any monster kill persists anger across entry and extends from latest visit")
{
    const SmugglersCoveProfile profile = *loadSmugglersCoveProfile(originalSource());
    const std::optional<ScriptedEventProgram> program = loadProfileProgram(profile);
    EventRuntime runtime;
    Party party;
    party.setQuestBit(10, true);
    EventRuntimeState state;
    VisitContext context;
    context.minutes = 100;
    EventRuntimeState::ActiveHookContext killedContext;
    killedContext.kind = EventRuntimeHookKind::MonsterKilled;
    killedContext.actorIndex = 23;
    killedContext.monsterId = 101;
    state.activeHookContext = killedContext;
    REQUIRE(runtime.executeHooks(program, std::nullopt, EventRuntimeHookKind::MonsterKilled,
        state, &party, &context));
    CHECK_EQ(state.namedMapVars["WereratsMad"], 1);

    // Leave much later than the kill, then reenter exactly one day later: still angry.
    context.delta.locationTime.lastVisitTime = legacyTimerTicksFromGameMinutes(2000.0);
    context.minutes = 3440;
    REQUIRE(runtime.executeOnLoadEvents(program, std::nullopt, state, &party, &context));
    CHECK_EQ(state.namedMapVars["WereratsMad"], 1);
    context.minutes += 0.5f;
    REQUIRE(runtime.executeOnLoadEvents(program, std::nullopt, state, &party, &context));
    CHECK_EQ(state.namedMapVars["WereratsMad"], 0);
    CHECK_EQ(state.actorGroupClearMasks[11] & uint32_t(EvtActorAttribute::Hostile),
        uint32_t(EvtActorAttribute::Hostile));

    state.activeHookContext = killedContext;
    REQUIRE(runtime.executeHooks(program, std::nullopt, EventRuntimeHookKind::MonsterKilled,
        state, &party, &context));
    context.delta.locationTime.lastVisitTime = legacyTimerTicksFromGameMinutes(context.minutes);
    REQUIRE(runtime.executeOnLoadEvents(program, std::nullopt, state, &party, &context));
    CHECK_EQ(state.namedMapVars["WereratsMad"], 1);
    CHECK_EQ(state.actorGroupSetMasks[11] & uint32_t(EvtActorAttribute::Hostile),
        uint32_t(EvtActorAttribute::Hostile));
}

TEST_CASE("Smugglers Cove Merge entry shares the original legacy EVT variable selector")
{
    EvtProgram evt;
    REQUIRE(evt.loadFromBytes(readLodText(originalSource() / "Data/mmmerge.T.lod", "d07.evt")));
    const std::string generated = generateLegacyEventLuaChunk(
        evt, {}, {}, LegacyLuaExportScope::Map, LegacyEventVersion::Mm8);
    const SmugglersCoveProfile profile = *loadSmugglersCoveProfile(originalSource());
    std::string error;
    const std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(EventSupportSource) + "\n" + generated + "\n" + profile.luaSource,
        "@original-d07+merge-overlay", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    EventRuntime runtime;
    EventRuntimeState state;
    Party party;
    VisitContext context;
    context.minutes = 100;
    REQUIRE(runtime.executeOnLoadEvents(program, std::nullopt, state, &party, &context));
    REQUIRE(runtime.executeEventById(program, std::nullopt, 451, state, &party, &context));
    CHECK_FALSE(state.pendingDialogueContext.has_value());

    // Original room event clears the same variable; the warning can now appear.
    REQUIRE(runtime.executeEventById(program, std::nullopt, 201, state, &party, &context));
    REQUIRE(runtime.executeEventById(program, std::nullopt, 451, state, &party, &context));
    REQUIRE(state.pendingDialogueContext.has_value());
    CHECK_EQ(state.pendingDialogueContext->sourceId, 37u);
}
