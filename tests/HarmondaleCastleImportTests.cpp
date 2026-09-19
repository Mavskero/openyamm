#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/party/Party.h"
#include "tools/mmerge_import/HarmondaleCastleResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/7out02.lua";
constexpr std::string_view ExpectedSha256 =
    "95ddcdcfbfa41519d072652e02e6c94bd5365bdbe5041eadc00052ec6d718957";

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
            / ("openyamm_harmondale_profile_" + std::string(name) + "_" + std::to_string(nonce));
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

std::optional<ScriptedEventProgram> loadProfileProgram(const HarmondaleCastleProfile &profile)
{
    const std::string generatedBase = R"OY_BASE(
RegisterEvent(301, "generated", function()
    AddValue(History(5), 0)
end, "generated")
)OY_BASE";
    std::string error;
    const std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(EventSupportSource) + "\n" + generatedBase + "\n" + profile.luaSource,
        "@7out02.lua+castle-history", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error.c_str());
    return program;
}

EventRuntimeState harmondaleState()
{
    EventRuntimeState state;
    setActiveHistoryContinent(state, 2);
    state.variables[uint32_t(EvtVariable::DayOfYear)] = 2;
    state.variables[uint32_t(EvtVariable::Hour)] = 10;
    return state;
}
}

TEST_CASE("Castle Harmondale profile recognizes only the audited original script")
{
    const std::optional<HarmondaleCastleProfile> profile = loadHarmondaleCastleProfile(originalSource());
    REQUIRE(profile.has_value());
    CHECK_EQ(profile->mapStem, "7out02");
    CHECK_EQ(profile->sourceSha256, ExpectedSha256);
    CHECK_FALSE(profile->luaSource.empty());
}

TEST_CASE("Castle Harmondale profile stays absent when its original map script is unavailable")
{
    SourceFixture fixture("absent");
    CHECK_FALSE(loadHarmondaleCastleProfile(fixture.source()).has_value());
}

TEST_CASE("Castle Harmondale profile rejects modified or escaping source scripts")
{
    SUBCASE("modified script")
    {
        SourceFixture fixture("modified");
        const std::filesystem::path script = fixture.copyOriginal();
        std::ofstream(script, std::ios::binary | std::ios::app) << "\n-- modified\n";
        CHECK_THROWS_AS(loadHarmondaleCastleProfile(fixture.source()), std::runtime_error);
    }

    SUBCASE("escaping symlink")
    {
        SourceFixture fixture("escaping");
        const std::filesystem::path outside = fixture.outside("outside_7out02.lua");
        std::filesystem::copy_file(originalSource() / RelativePath, outside);
        const std::filesystem::path link = fixture.source() / RelativePath;
        std::filesystem::create_directories(link.parent_path());
        std::filesystem::create_symlink(outside, link);
        CHECK_THROWS_AS(loadHarmondaleCastleProfile(fixture.source()), std::runtime_error);
    }
}

TEST_CASE("Castle Harmondale entrance preserves the MMerge gate history and repeat transitions")
{
    const HarmondaleCastleProfile profile = *loadHarmondaleCastleProfile(originalSource());
    const std::optional<ScriptedEventProgram> program = loadProfileProgram(profile);
    EventRuntime runtime;

    SUBCASE("Q519 is required")
    {
        Party party;
        EventRuntimeState state = harmondaleState();
        REQUIRE(runtime.executeEventById(program, std::nullopt, 301, state, &party));
        CHECK_FALSE(party.hasQuestBit(587));
        CHECK_FALSE(party.hasQuestBit(644));
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(3));
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(5));
        CHECK_FALSE(state.pendingMapMove.has_value());
    }

    SUBCASE("first entrance records history three and moves the butler once")
    {
        Party party;
        party.setQuestBit(519, true);
        EventRuntimeState state = harmondaleState();
        REQUIRE(runtime.executeEventById(program, std::nullopt, 301, state, &party));
        CHECK(party.hasQuestBit(587));
        CHECK(party.hasQuestBit(644));
        REQUIRE(state.historyEventTimesByContinent[2].contains(3));
        CHECK_EQ(state.historyEventTimesByContinent[2].at(3), 2040);
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(5));
        CHECK_EQ(state.npcHouseOverrides[397], 240u);
        REQUIRE(state.pendingDialogueContext.has_value());
        CHECK_EQ(state.pendingDialogueContext->sourceId, 397u);

        state.pendingDialogueContext.reset();
        REQUIRE(runtime.executeEventById(program, std::nullopt, 301, state, &party));
        CHECK_EQ(state.historyEventTimesByContinent[2].at(3), 2040);
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(5));
        REQUIRE(state.pendingDialogueContext.has_value());
        CHECK_EQ(state.pendingDialogueContext->transitionTextId, 390u);
        REQUIRE(state.pendingDialogueContext->transitionMapMove.has_value());
        CHECK_EQ(state.pendingDialogueContext->transitionMapMove->mapName,
            std::optional<std::string>("7d29.blv"));
    }

    SUBCASE("an existing arrival history keeps its first occurrence time")
    {
        Party party;
        party.setQuestBit(519, true);
        EventRuntimeState state = harmondaleState();
        state.historyEventTimesByContinent[2][3] = 1200;
        REQUIRE(runtime.executeEventById(program, std::nullopt, 301, state, &party));
        CHECK_EQ(state.historyEventTimesByContinent[2].at(3), 1200);
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(5));
    }

    SUBCASE("repaired castle uses the later original transition")
    {
        Party party;
        party.setQuestBit(519, true);
        party.setQuestBit(610, true);
        EventRuntimeState state = harmondaleState();
        REQUIRE(runtime.executeEventById(program, std::nullopt, 301, state, &party));
        REQUIRE(state.pendingDialogueContext.has_value());
        CHECK_EQ(state.pendingDialogueContext->transitionTextId, 382u);
        REQUIRE(state.pendingDialogueContext->transitionMapMove.has_value());
        CHECK_EQ(state.pendingDialogueContext->transitionMapMove->mapName,
            std::optional<std::string>("7d29.blv"));
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(3));
        CHECK_FALSE(state.historyEventTimesByContinent[2].contains(5));
    }
}
