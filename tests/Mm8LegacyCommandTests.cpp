#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "tools/LegacyLuaExport.h"
#include "tools/mmerge_import/LodResources.h"

#include <filesystem>
#include <stdexcept>

using namespace OpenYAMM::Game;

namespace
{
std::string originalMapLua(const char *name)
{
    const std::filesystem::path archive = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001/Data/mm6.T.lod";
    EvtProgram program;
    REQUIRE(program.loadFromBytes(OpenYAMM::Import::readLodText(archive, name)));
    return generateLegacyEventLuaChunk(program, {}, {}, LegacyLuaExportScope::Map, LegacyEventVersion::Mm8);
}

std::optional<ScriptedEventProgram> load(const std::string &lua)
{
    std::string error;
    std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + "\n" + lua,
        "@original-mm8-legacy-command", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    return program;
}
}

TEST_CASE("Original 6d09 command27 continues to close door21 and command28 changes nothing")
{
    const std::string lua = originalMapLua("6d09.evt");
    CHECK(lua.find("do end -- Original MM8 ignores command 27; payload 01 01") != std::string::npos);
    CHECK(lua.find("do end -- Original MM8 ignores command 28; payload 01") != std::string::npos);
    const std::optional<ScriptedEventProgram> program = load(lua);
    EventRuntime runtime;
    EventRuntimeState state;
    state.mechanisms[21].state = uint16_t(EvtMechanismState::Open);
    state.mechanisms[22].state = uint16_t(EvtMechanismState::Open);
    REQUIRE(runtime.executeEventById(program, std::nullopt, 21, state));
    CHECK_EQ(state.mechanisms.at(21).state, uint16_t(EvtMechanismState::Closing));
    CHECK_EQ(state.mechanisms.at(22).state, uint16_t(EvtMechanismState::Open));
    REQUIRE(runtime.executeEventById(program, std::nullopt, 39, state));
    CHECK_EQ(state.mechanisms.size(), 2);
    CHECK_EQ(state.mechanisms.at(21).state, uint16_t(EvtMechanismState::Closing));
    CHECK_EQ(state.mechanisms.at(22).state, uint16_t(EvtMechanismState::Open));
}

TEST_CASE("Original znwc event71 retains seven explicit MM8 no-ops without party or map effects")
{
    const std::string lua = originalMapLua("znwc.evt");
    size_t count = 0;
    size_t offset = 0;
    const std::string marker = "do end -- Original MM8 ignores command 20; payload";
    while ((offset = lua.find(marker, offset)) != std::string::npos)
    {
        ++count;
        offset += marker.size();
    }
    CHECK_EQ(count, 7);
    const std::optional<ScriptedEventProgram> program = load(lua);
    EventRuntime runtime;
    EventRuntimeState state;
    state.variables[123] = 456;
    state.mapVars[9] = 17;
    Party party;
    party.addGold(1234);
    REQUIRE(runtime.executeEventById(program, std::nullopt, 71, state, &party));
    CHECK_EQ(party.gold(), 1234);
    CHECK_EQ(state.variables.size(), 1);
    CHECK_EQ(state.variables.at(123), 456);
    CHECK_EQ(state.mapVars[9], 17);
    CHECK(state.mechanisms.empty());
    CHECK(state.grantedItemIds.empty());
    CHECK(state.actorSetMasks.empty());
    CHECK(state.actorClearMasks.empty());
}

TEST_CASE("A branch into an MM8 no-op still executes its following reward exactly once")
{
    const std::vector<uint8_t> bytes = {
        5, 1, 0, 0, 36, 2,
        10, 1, 0, 1, 16, 21, 0, 100, 0, 0, 0,
        6, 1, 0, 2, 27, 1, 1,
        10, 1, 0, 3, 16, 21, 0, 7, 0, 0, 0,
        5, 1, 0, 4, 1, 0};
    EvtProgram source;
    REQUIRE(source.loadFromBytes(bytes));
    const std::string lua = generateLegacyEventLuaChunk(source, {}, {},
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm8);
    const std::optional<ScriptedEventProgram> program = load(lua);
    EventRuntime runtime;
    EventRuntimeState state;
    Party party;
    REQUIRE(runtime.executeEventById(program, std::nullopt, 1, state, &party));
    CHECK_EQ(party.gold(), 7);
    for (const LegacyEventVersion version : {LegacyEventVersion::Mm6, LegacyEventVersion::Mm7})
    {
        CHECK_THROWS_AS(generateLegacyEventLuaChunk(source, {}, {}, LegacyLuaExportScope::Map, version),
            std::runtime_error);
    }
}
