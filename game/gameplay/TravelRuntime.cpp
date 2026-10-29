#include "game/gameplay/TravelRuntime.h"

#include "game/debug/GameplayDebugTrace.h"
#include "game/events/GameTime.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/mm9/Mm9RudeDialogue.h"
#include "game/party/Party.h"

#include <algorithm>
#include <limits>

namespace OpenYAMM::Game
{
bool validateDeferredTravelPlan(const EventRuntimeState::DeferredTravelPlan &plan, std::string &error)
{
    error.clear();
    if (plan.goldFare < 0 || plan.travelDays < 0 || plan.foodRequired < 0
        || !validGameMinutes(double(plan.travelDays) * 1440.0))
    {
        error = "Invalid deferred travel cost or duration";
        return false;
    }

    switch (plan.mode)
    {
    case EventRuntimeState::TravelPlanMode::None:
        if (plan.goldFare != 0 || plan.travelDays != 0 || plan.foodRequired != 0 || plan.exitCallback)
        {
            error = "A travel plan without an action must not contain costs or a callback";
            return false;
        }
        return true;

    case EventRuntimeState::TravelPlanMode::Transport:
        if (plan.travelDays == 0 || plan.foodRequired != 0 || plan.exitCallback)
        {
            error = "Invalid transport travel plan";
            return false;
        }
        return true;

    case EventRuntimeState::TravelPlanMode::TravelDays:
        if (plan.exitCallback)
        {
            if (plan.exitCallback->sourceObjectIndex > std::numeric_limits<uint16_t>::max())
            {
                error = "Invalid deferred MM9 departure callback";
                return false;
            }
            const std::optional<uint16_t> expectedEvent =
                mm9RudeExitEventIdForSourceObject(plan.exitCallback->sourceObjectIndex);
            if (!expectedEvent || *expectedEvent != plan.exitCallback->eventId)
            {
                error = "Invalid deferred MM9 departure callback";
                return false;
            }
        }
        return true;
    }

    error = "Unknown deferred travel mode";
    return false;
}

bool applyDeferredTravelPlan(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    const EventRuntimeState::DeferredTravelPlan &plan,
    std::string &error)
{
    if (!validateDeferredTravelPlan(plan, error))
    {
        return false;
    }
    if (plan.mode == EventRuntimeState::TravelPlanMode::None)
    {
        return true;
    }
    if (pWorldRuntime == nullptr)
    {
        error = "Travel is unavailable right now";
        return false;
    }
    if (party.gold() < plan.goldFare)
    {
        error = "You don't have enough gold";
        return false;
    }

    const double travelMinutes = double(plan.travelDays) * 1440.0;
    if (!validGameMinutes(pWorldRuntime->gameMinutes())
        || !validGameMinutes(pWorldRuntime->gameMinutes() + travelMinutes))
    {
        error = "Invalid authoritative travel clock";
        return false;
    }
    if (plan.exitCallback)
    {
        const MapDeltaData *pMap = pWorldRuntime->mapDeltaData();
        if (pWorldRuntime->eventRuntimeState() == nullptr || pMap == nullptr
            || plan.exitCallback->actorIndex >= pMap->actors.size()
            || pMap->actors[plan.exitCallback->actorIndex].mm9SourceObjectIndex != plan.exitCallback->sourceObjectIndex)
        {
            error = "The deferred MM9 departure actor no longer matches its source";
            return false;
        }
    }

    party.addGold(-plan.goldFare);
    if (plan.mode == EventRuntimeState::TravelPlanMode::Transport)
    {
        // Native coach and boat services rest before their journey's timed states expire.
        party.restAndHealAll();
        pWorldRuntime->advanceGameMinutes(travelMinutes);
        party.advanceTimedStates(static_cast<float>(travelMinutes * 60.0));
    }
    else
    {
        applyTravelDaysSideEffects(party, pWorldRuntime, plan.travelDays, plan.foodRequired);
    }

    if (plan.exitCallback)
    {
        EventRuntimeState &events = *pWorldRuntime->eventRuntimeState();
        const std::optional<EventRuntimeState::ActiveHookContext> previousHook = events.activeHookContext;
        EventRuntimeState::ActiveHookContext hook;
        hook.actorIndex = plan.exitCallback->actorIndex;
        events.activeHookContext = std::move(hook);
        size_t previousMessageCount = events.messages.size();
        bool executed = false;
        try
        {
            executed = pWorldRuntime->executeMapEvent(plan.exitCallback->eventId, previousMessageCount);
        }
        catch (...)
        {
            events.activeHookContext = previousHook;
            throw;
        }
        events.activeHookContext = previousHook;
        GAMEPLAY_DEBUG_TRACE(
            "mm9_rude_exit_callback actor_index=" + std::to_string(plan.exitCallback->actorIndex)
            + " source_object_index=" + std::to_string(plan.exitCallback->sourceObjectIndex)
            + " event_id=" + std::to_string(plan.exitCallback->eventId)
            + " executed=" + std::string(executed ? "true" : "false"));
    }
    return true;
}

void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired)
{
    travelDays = std::max(0, travelDays);
    foodRequired = std::max(0, foodRequired);
    constexpr float MinutesPerDay = 24.0f * 60.0f;
    if (travelDays > 0 && pWorldRuntime != nullptr)
    {
        const float travelMinutes = static_cast<float>(travelDays) * MinutesPerDay;
        pWorldRuntime->advanceGameMinutes(travelMinutes);
        party.advanceTimedStates(travelMinutes * 60.0f);
    }
    if (travelDays == 0 && foodRequired == 0)
    {
        return;
    }

    party.restAndHealAll();
    const int availableFood = party.food();
    if (availableFood > 0)
    {
        party.addFood(-foodRequired);
    }
    if (availableFood >= foodRequired)
    {
        return;
    }

    const float gameMinutes = pWorldRuntime != nullptr ? pWorldRuntime->gameMinutes() : 0.0f;
    for (size_t memberIndex = 0; memberIndex < party.members().size(); ++memberIndex)
    {
        const Character *pMember = party.member(memberIndex);
        if (pMember != nullptr && pMember->health > 0)
        {
            party.applyMemberCondition(memberIndex, CharacterCondition::Weak, gameMinutes);
        }
    }
}
}
