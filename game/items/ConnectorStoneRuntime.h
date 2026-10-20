#pragma once

#include "game/events/EventRuntime.h"
#include "game/events/NamedGameTime.h"
#include "game/party/PartySpellSystem.h"

namespace OpenYAMM::Game
{
inline constexpr const char *ConnectorRechargeName = "MMerge.CrossContinents.NextConRecharge";
inline constexpr const char *ConnectorImprovedName = "MMerge.CrossContinents.ImporvedConnector";
inline constexpr const char *HiddenCaster49CounterName = "MMerge.CrossContinents.HiddenCaster49DivineInterventionCasts";
inline constexpr int32_t ConnectorMinimumRechargeTicks = 360 * 256;
inline constexpr int32_t ConnectorMaximumRechargeTicks = 2880 * 256;

// Explicit migration of the earlier port prototype, not an inference of original hidden-caster history.
inline bool prepareConnectorState(EventRuntimeState &state, std::string &error)
{
    auto &vars = state.namedGlobalVars;
    const auto oldImproved = vars.find("MMerge.CrossContinents.ImprovedConnector");
    if (!vars.contains(ConnectorImprovedName) && oldImproved != vars.end())
        vars[ConnectorImprovedName] = oldImproved->second;
    const auto oldRecharge = vars.find("MMerge.CrossContinents.NextConnectorRecharge");
    if (!vars.contains(std::string(ConnectorRechargeName) + GameTimeHighSuffix)
        && !vars.contains(std::string(ConnectorRechargeName) + GameTimeLowSuffix)
        && oldRecharge != vars.end())
    {
        if (oldRecharge->second < 0) { error = "Invalid prototype connector deadline"; return false; }
        if (!writeNamedGameTime(vars, ConnectorRechargeName, int64_t(oldRecharge->second) * 256, error)) return false;
    }
    const auto counter = vars.find(HiddenCaster49CounterName);
    if (counter != vars.end() && (counter->second < 0 || counter->second > 1))
    { error = "Invalid scripted hidden49 counter"; return false; }
    int64_t deadline = 0;
    return readNamedGameTime(vars, ConnectorRechargeName, 0, deadline, error);
}

enum class ConnectorUseResult { Unrelated, Discharged, HouseScreen, Enemies, Called, InvalidState };

inline ConnectorUseResult useConnectorStone(Party &party, EventRuntimeState &state, InventoryItem &item,
    double minutes, bool houseScreen, bool detectedEnemies, int32_t rechargeDelayTicks)
{
    if (item.objectDescriptionId != 624 && item.objectDescriptionId != 625) return ConnectorUseResult::Unrelated;
    if (item.objectDescriptionId == 625) return ConnectorUseResult::Discharged;
    std::string error;
    if (!prepareConnectorState(state, error)) return ConnectorUseResult::InvalidState;
    auto &vars = state.namedGlobalVars;
    const auto improved = vars.find(ConnectorImprovedName);
    const auto counter = vars.find(HiddenCaster49CounterName);
    if (improved != vars.end() && improved->second != 0 && (counter == vars.end() || counter->second == 0))
    {
        for (const Character &member : party.members())
        {
            if (member.health < member.maxHealth || member.spellPoints < member.maxSpellPoints)
            {
                // Original direct helper clears hidden49 before88, then88 increments it to1.
                PartySpellSystem::applyDivineIntervention(party);
                vars[HiddenCaster49CounterName] = 1;
                break;
            }
        }
    }
    // Source orders DI before screen13 and enemy checks.
    if (houseScreen) return ConnectorUseResult::HouseScreen;
    if (detectedEnemies) return ConnectorUseResult::Enemies;
    const auto now = gameTimeTicks(minutes);
    if (!now || rechargeDelayTicks < ConnectorMinimumRechargeTicks || rechargeDelayTicks > ConnectorMaximumRechargeTicks
        || *now > MaximumGameTimeTicks - rechargeDelayTicks
        || !writeNamedGameTime(vars, ConnectorRechargeName, *now + rechargeDelayTicks, error))
        return ConnectorUseResult::InvalidState;
    item.objectDescriptionId = 625;
    return ConnectorUseResult::Called;
}
}
