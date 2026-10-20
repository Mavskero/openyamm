#pragma once

#include "CrossContinentsIntroSource.h"
#include "EventSupportSource.h"
#include "doctest/doctest.h"
#include "game/events/NamedGameTime.h"
#include "game/events/ISceneEventContext.h"
#include "tests/PartySpellTestHarness.h"
#include "tests/RegressionGameData.h"
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace OpenYAMM::Tests
{
class MergeQuestFixture : public PartySpellTestWorldRuntime, public Game::ISceneEventContext
{
public:
    static constexpr const char *Prefix = "MMerge.CrossContinents.";
    Game::Party party;
    Game::EventRuntimeState state;
    Game::EventRuntime events;
    std::optional<Game::ScriptedEventProgram> global;
    double minutes = 100.25;
    bool yellow = false;
    bool red = false;

    MergeQuestFixture(uint32_t continent = 1, bool includeLegacy = false, bool withOverlay = true)
        : events(&regressionGameData().houseTable, &regressionGameData().npcDialogTable)
    {
        REQUIRE_MESSAGE(regressionGameDataLoaded(), regressionGameDataFailure());
        std::string error;
        std::string source = std::string(OpenYAMM::Import::EventSupportSource) + "\n";
        if (includeLegacy)
        {
            const char *root = std::getenv("OPENYAMM_CC_EVENT_ROOT");
            REQUIRE(root != nullptr);
            std::ifstream file(std::string(root) + "/engine/events/Global.lua");
            REQUIRE(file.good());
            source.append(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            source += "\n";
        }
        if (withOverlay) source += OpenYAMM::Import::CrossContinentsIntroSource;
        global = Game::ScriptedEventProgram::loadFromLuaText(
            source,
            "@source-SHA-gated-cross-continents-controlled-fixture", Game::ScriptedEventScope::Global, error);
        REQUIRE_MESSAGE(global.has_value(), error);
        party.setItemTable(&regressionGameData().itemTable);
        party.setClassMultiplierTable(&regressionGameData().classMultiplierTable);
        party.seed(createSpellRegressionPartySeed());
        state.activeHistoryContinentId = continent;
        state.normalGameplayScreen = true;
        bindParty(&party);
        bindEventRuntimeState(&state);
        bindGlobalEventProgram(&global);
        setMapName("out01.odm");
    }
    float currentGameMinutes() const override { return float(minutes); }
    double currentGameMinutesPrecise() const override { return minutes; }
    const Game::MapDeltaData *mapDeltaData() const override { return nullptr; }
    Game::MapDeltaData *mapDeltaData() override { return nullptr; }
    bool setFacetBit(uint32_t, uint32_t, bool) override { return false; }
    bool castEventSpell(uint32_t spell, uint32_t skill, uint32_t mastery,
        int32_t x, int32_t y, int32_t z, int32_t tx, int32_t ty, int32_t tz) override
    {
        return PartySpellTestWorldRuntime::castEventSpell(spell, skill, mastery, x, y, z, tx, ty, tz);
    }
    bool summonMonsters(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t) override { return false; }
    bool summonEventItem(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool summonEventObject(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool checkMonstersKilled(uint32_t, uint32_t, uint32_t, bool) const override { return false; }
    double gameMinutes() const override { return minutes; }
    void advanceGameMinutes(double value) override { minutes += value; }
    size_t mapActorCount() const override { return 1; }
    bool actorRuntimeState(size_t index, Game::GameplayRuntimeActorState &actor) const override
    {
        if (index != 0) return false;
        actor = {};
        actor.hostileToParty = true;
        actor.hasDetectedParty = yellow || red;
        actor.combatTargetingParty = red;
        return true;
    }
    int32_t flag(const std::string &name) const
    {
        const auto found = state.namedGlobalVars.find(std::string(Prefix) + name);
        return found == state.namedGlobalVars.end() ? 0 : found->second;
    }
    void flag(const std::string &name, int32_t value)
    {
        state.namedGlobalVars[std::string(Prefix) + name] = value;
    }
    int64_t ticks(const std::string &name) const
    {
        int64_t value = 0;
        std::string error;
        REQUIRE_MESSAGE(Game::readNamedGameTime(state.namedGlobalVars, std::string(Prefix) + name,
            0, value, error), error);
        return value;
    }
    void ticks(const std::string &name, int64_t value)
    {
        std::string error;
        REQUIRE_MESSAGE(Game::writeNamedGameTime(state.namedGlobalVars, std::string(Prefix) + name,
            value, error), error);
    }
    void apply() { party.applyEventRuntimeState(state, false); }
    bool executeEventHooks(Game::EventRuntimeHookKind kind) override
    {
        const bool executed = events.executeHooks(std::nullopt, global, kind, state, &party, this);
        apply();
        return executed;
    }
    void talkContext(uint32_t npcId, uint32_t hostHouse = 0)
    {
        Game::EventRuntimeState::PendingDialogueContext pending;
        pending.kind = Game::DialogueContextKind::NpcTalk;
        pending.sourceId = npcId;
        pending.hostHouseId = hostHouse;
        state.pendingDialogueContext = pending;
    }
    void enter(uint32_t npcId = 803, uint32_t hostHouse = 0)
    {
        Game::EventRuntimeState::ActiveHookContext hook;
        hook.kind = Game::EventRuntimeHookKind::NpcEnter;
        hook.npcId = npcId;
        state.activeHookContext = hook;
        executeEventHooks(hook.kind);
        state.activeHookContext.reset();
        talkContext(npcId, hostHouse);
    }
    void topic(uint16_t id)
    {
        REQUIRE(state.pendingDialogueContext.has_value());
        const auto context = *state.pendingDialogueContext;
        state.messages.clear(); // Inspect this action's messages, as the UI clips at previousMessageCount.
        REQUIRE(events.executeNpcTopicEventById(std::nullopt, global, id, state, &party, this));
        apply();
        state.pendingDialogueContext = context; // Production dialogue controller keeps the current NPC open.
    }
    bool visible(uint16_t id) const
    {
        return events.canShowTopic(global, id, state, &party, this);
    }
    uint32_t slot(uint32_t index) const
    {
        return state.npcTopicOverrides.at(803).at(index);
    }
    void after()
    {
        REQUIRE(events.executeGlobalAfterLoadEvents(std::nullopt, global, state, &party, this));
        apply();
    }
    void leave()
    {
        REQUIRE(events.executeOnLeaveEvents(std::nullopt, global, state, &party, this));
        apply();
    }
    void timer(uint16_t id)
    {
        const bool executed = events.executeEventById(std::nullopt, global, id, state, &party, this,
            std::nullopt, true, Game::ScriptedEventScope::Global, true);
        REQUIRE_MESSAGE(executed, events.lastScriptError().value_or("No Lua handler/error"));
        apply();
    }
    void clearUiRequests() { state.pendingDialogueContext.reset(); state.messages.clear(); }
};
}
