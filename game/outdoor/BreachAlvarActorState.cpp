#include "game/outdoor/OutdoorWorldRuntime.h"

#include "game/events/GameTime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace OpenYAMM::Game
{
bool OutdoorWorldRuntime::validBreachAlvarActorRules(const BreachAlvarFriendActorRules &rules)
{
    // Content/source equality is checked by the profile codec. These are local arithmetic prerequisites.
    return rules.npcId > 0 && rules.npcId <= uint32_t(std::numeric_limits<int16_t>::max())
        && rules.fullHealthMultiplier > 0 && rules.restoreIntervalGameMinutes > 0 && rules.restoreHealth > 0
        && !rules.initiallyHostile && rules.restoreClearsHostile && rules.restoreSkipsRemoved
        && rules.restoreDeadToStand;
}

bool OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(
    const Snapshot &snapshot, const BreachAlvarFriendActorBinding &binding, bool returned,
    std::string &error, const BreachAlvarFriendActorRules *pRules)
{
    error.clear();
    const auto fail = [&error](const char *message)
    {
        error = std::string("Breach Alvar actor binding: ") + message;
        return false;
    };
    if (!validGameMinutes(snapshot.gameMinutes) || binding.canonicalMapKey != BreachAlvarCompanionMap
        || binding.spawnMonsterId == 0 || binding.spawnMonsterId > uint32_t(std::numeric_limits<int16_t>::max())
        || (pRules != nullptr && !validBreachAlvarActorRules(*pRules)))
    {
        return fail("invalid clock, map, monster or rules");
    }
    const MapActorState *pFriend = nullptr;
    std::unordered_set<uint32_t> identities;
    for (const MapActorState &actor : snapshot.mapActors)
    {
        if (!identities.insert(actor.actorId).second || actor.actorId >= snapshot.nextActorId)
        {
            return fail("duplicate actor identity or invalid next identity");
        }
        if (!actor.breachAlvarCompanion)
        {
            if (actor.breachAlvarCompanionRemoved || actor.breachAlvarNextMaintenanceGameMinute != 0.0)
            {
                return fail("companion lifecycle without its role");
            }
            continue;
        }
        if (pFriend != nullptr)
        {
            return fail("multiple companion actors");
        }
        pFriend = &actor;
    }
    if (pFriend == nullptr || pFriend->actorId != binding.actorId || pFriend->npcId <= 0
        || pFriend->monsterId != int16_t(binding.spawnMonsterId) || !pFriend->spawnedAtRuntime
        || pFriend->fromSpawnPoint || pFriend->combatState.maximumHitPoints() <= 0 || pFriend->currentHp < 0
        || pFriend->currentHp > pFriend->combatState.maximumHitPoints() || pFriend->breachAlvarCompanionRemoved != returned
        || !std::isfinite(pFriend->breachAlvarNextMaintenanceGameMinute)
        || pFriend->breachAlvarNextMaintenanceGameMinute <= 0.0
        || pFriend->breachAlvarNextMaintenanceGameMinute
            > MaximumGameMinutes + double(std::numeric_limits<uint32_t>::max()))
    {
        return fail("missing actor or incompatible identity, health or lifecycle");
    }
    if (std::count_if(snapshot.mapActors.begin(), snapshot.mapActors.end(),
            [pFriend](const MapActorState &actor) { return actor.npcId == pFriend->npcId; }) != 1)
    {
        return fail("multiple actors share the companion NPC");
    }
    if (returned && (pFriend->hostileToParty || pFriend->controlMode != ActorControlMode::None
        || pFriend->controlRemainingSeconds != 0.0f || !pFriend->isInvisible || pFriend->canReceiveDamage
        || pFriend->actionSeconds != 0.0f || pFriend->attackImpactTriggered
        || pFriend->velocityX != 0.0f || pFriend->velocityY != 0.0f || pFriend->velocityZ != 0.0f))
    {
        return fail("companion removal gates are inconsistent");
    }
    if (pRules != nullptr && (uint32_t(pFriend->npcId) != pRules->npcId || pFriend->group != pRules->group
        || pFriend->ally != pRules->ally))
    {
        return fail("actor differs from source content rules");
    }
    return true;
}
}
