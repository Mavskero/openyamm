#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "tools/LegacyLuaExport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace OpenYAMM::Game;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::optional<ScriptedEventProgram> loadProgram(const std::string &source, ScriptedEventScope scope)
{
    std::string error;
    std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + "\n" + source, "@event-check", scope, error);
    require(program.has_value(), "Cannot load event program: " + error);
    return program;
}

std::string readText(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot read " + path.string());
    return {(std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()};
}

Party createParty()
{
    Character member;
    member.rosterId = 17;
    member.health = 100;
    member.maxHealth = 100;
    PartySeed seed;
    seed.members.push_back(member);
    Party party;
    party.seed(seed);
    return party;
}

void checkCanPlayerAct()
{
    // EVT 0x44 asks whether roster member 17 can act. Gold makes both branch outcomes observable.
    std::vector<uint8_t> bytes;
    const auto record = [&](uint8_t step, uint8_t opcode, std::initializer_list<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {static_cast<uint8_t>(4 + payload.size()), 77, 0, step, opcode});
        bytes.insert(bytes.end(), payload);
    };
    record(0, 68, {17, 0, 0, 0, 3});
    record(1, 16, {21, 0, 1, 0, 0, 0});
    record(2, 1, {0});
    record(3, 16, {21, 0, 10, 0, 0, 0});
    record(4, 1, {0});
    EvtProgram evt;
    require(evt.loadFromBytes(bytes), "Cannot parse CanPlayerAct fixture");
    const std::optional<ScriptedEventProgram> program = loadProgram(
        generateLegacyEventLuaChunk(evt, {}, {}, LegacyLuaExportScope::Map, LegacyEventVersion::Mm8),
        ScriptedEventScope::Map);
    EventRuntime runtime;
    EventRuntimeState state;
    Party party = createParty();
    const auto expectGold = [&](int expected)
    {
        const int before = party.gold();
        require(runtime.executeEventById(program, std::nullopt, 77, state, &party), "Branch execution failed");
        require(party.gold() - before == expected, "CanPlayerAct chose the wrong branch");
    };
    expectGold(10);
    party.member(0)->conditions.set(static_cast<size_t>(CharacterCondition::Unconscious));
    expectGold(1);
    party.member(0)->conditions.reset();
    party.member(0)->health = 0;
    expectGold(1);
    party.member(0)->health = 100;
    party.member(0)->rosterId = 18;
    HiredNpcFollower follower;
    follower.npcId = 17;
    party.addHiredNpcFollower(follower);
    state.hiredNpcFollowers.push_back(follower);
    expectGold(1);
    party.member(0)->rosterId = 17;
    expectGold(10);
}

void checkImportedStart(const std::filesystem::path &root)
{
    const std::optional<ScriptedEventProgram> global = loadProgram(
        readText(root / "engine/events/Global.lua"), ScriptedEventScope::Global);
    const std::optional<ScriptedEventProgram> local = loadProgram(
        readText(root / "worlds/mm6/events/maps/oute3.lua"), ScriptedEventScope::Map);
    EventRuntime runtime;
    EventRuntimeBindingReport report;
    require(runtime.validateProgramBindings(local, global, report),
        "Runtime bindings rejected: " + report.errorMessage.value_or("missing handlers"));
    require(std::find(local->onLoadEventIds().begin(), local->onLoadEventIds().end(), 100)
        != local->onLoadEventIds().end(), "Starting quest is not registered for map load");
    EventRuntimeState state;
    Party party = createParty();
    require(runtime.executeEventById(local, global, 100, state, &party), "Starting quest execution failed");
    require(party.hasQuestBit(1104) && party.hasQuestBit(1105), "Starting quest bits were not granted");
    require(state.grantedItemIds == std::vector<uint32_t>{2125}, "Starting quest must request exactly one letter");
    state.grantedItemIds.clear();
    require(runtime.executeEventById(local, global, 100, state, &party), "Repeated starting quest execution failed");
    require(state.grantedItemIds.empty(), "Starting quest granted a duplicate letter");
    Party freshParty = createParty();
    EventRuntimeState freshState;
    require(runtime.executeOnLoadEvents(local, global, freshState, &freshParty),
        "Imported map-entry event chain failed");
    require(freshParty.hasQuestBit(1104) && freshParty.hasQuestBit(1105)
        && freshState.grantedItemIds == std::vector<uint32_t>{2125},
        "Later imported map-entry handlers erased the initial letter reward");
    std::cout << ",\"local_handlers\":" << report.localHandlerCount
        << ",\"global_handlers\":" << report.globalHandlerCount
        << ",\"topic_handlers\":" << report.canShowTopicHandlerCount
        << ",\"start_quest_idempotent\":true,\"full_entry_chain_retains_letter\":true";
}

void checkEntryBatchOutputs()
{
    const std::optional<ScriptedEventProgram> program = loadProgram(R"(
SetMapMetadata({onLoad = {1, 2}})
RegisterEvent(1, nil, function() AddValue(InventoryItem(101), 101) end)
RegisterEvent(2, nil, function() AddValue(InventoryItem(102), 102) end)
RegisterEvent(3, nil, function() end)
)", ScriptedEventScope::Map);
    EventRuntime runtime;
    EventRuntimeState state;
    Party party = createParty();
    require(runtime.executeOnLoadEvents(program, std::nullopt, state, &party), "Map-entry batch failed");
    require(state.grantedItemIds == std::vector<uint32_t>({101, 102}),
        "Map-entry batch lost or reordered item rewards");
    require(runtime.executeEventById(program, std::nullopt, 3, state, &party), "Independent event failed");
    require(state.grantedItemIds.empty(), "Independent event retained outputs from a previous execution");
}
}

int main(int argc, char **argv)
{
    try
    {
        require(argc <= 2, "Usage: event_import_runtime_check [CONVERTED_EVENT_ROOT]");
        checkCanPlayerAct();
        checkEntryBatchOutputs();
        std::cout << "{\"can_player_act_cases\":5,\"entry_batch_and_independent_outputs\":true";
        if (argc == 2)
        {
            checkImportedStart(argv[1]);
        }
        std::cout << ",\"passed\":true}\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
