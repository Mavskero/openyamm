#include "game/events/ScriptedEventTimer.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>

namespace OpenYAMM::Game
{
namespace
{
constexpr double LegacyTicksPerGameMinute = 256.0;
constexpr double GameMinutesPerDay = 24.0 * 60.0;
constexpr double GameMinutesPerWeek = 7.0 * GameMinutesPerDay;
constexpr double GameMinutesPerMonth = 28.0 * GameMinutesPerDay;
constexpr double GameMinutesPerYear = 12.0 * GameMinutesPerMonth;
constexpr double LegacyTimerPollDelayGameMinutes = 0.5;
constexpr double MinimumNativeTimerIntervalGameMinutes = 0.5;

double calendarPeriodGameMinutes(ScriptedEventTimerScheduleKind scheduleKind)
{
    switch (scheduleKind)
    {
        case ScriptedEventTimerScheduleKind::Daily: return GameMinutesPerDay;
        case ScriptedEventTimerScheduleKind::Weekly: return GameMinutesPerWeek;
        case ScriptedEventTimerScheduleKind::Monthly: return GameMinutesPerMonth;
        case ScriptedEventTimerScheduleKind::Yearly: return GameMinutesPerYear;
        case ScriptedEventTimerScheduleKind::Relative: return GameMinutesPerDay;
        case ScriptedEventTimerScheduleKind::Interval:
            return 0.0;
    }

    return 0.0;
}

double dailyStartOffsetGameMinutes(const ScriptedEventTimerDefinition &definition)
{
    return static_cast<double>(definition.startHour) * 60.0
        + static_cast<double>(definition.startMinute)
        + static_cast<double>(definition.startSecond) / 60.0;
}

double nextExactAlarmGameMinutes(double startGameMinutes, double periodGameMinutes, double currentGameMinutes)
{
    if (startGameMinutes > currentGameMinutes)
    {
        return startGameMinutes;
    }

    const double elapsedPeriods = std::floor((currentGameMinutes - startGameMinutes) / periodGameMinutes) + 1.0;
    return startGameMinutes + elapsedPeriods * periodGameMinutes;
}

double nextNativeExactAlarmGameMinutes(double alarmGameMinutes, double periodGameMinutes, int64_t lastPollTime)
{
    const double pollBoundary = legacyTimerGameMinutesFromTicks(lastPollTime) + 1.0 / LegacyTicksPerGameMinute;
    const double periods = std::max(1.0, std::floor((pollBoundary - alarmGameMinutes) / periodGameMinutes) + 1.0);
    return alarmGameMinutes + periods * periodGameMinutes;
}

ScriptedEventTimerState initializeTimer(
    const ScriptedEventTimerDefinition &definition,
    double currentGameMinutes,
    int64_t lastVisitTime)
{
    ScriptedEventTimerState state = {};
    state.definition = definition;

    if (definition.origin == ScriptedEventTimerOrigin::Native)
    {
        if (definition.scheduleKind == ScriptedEventTimerScheduleKind::Interval)
        {
            // Original registration and subsequent alarms use the preceding map poll tick.
            // A long absence may therefore cause catch-up callbacks on successive polls.
            const double previousVisit = legacyTimerGameMinutesFromTicks(lastVisitTime);
            state.lastNativePollTime = lastVisitTime == 0 ? -1 : lastVisitTime;
            const double start = dailyStartOffsetGameMinutes(definition);
            state.nextAlarmGameMinutes = start <= currentGameMinutes && start <= previousVisit
                ? nextNativeExactAlarmGameMinutes(start,
                    std::max(MinimumNativeTimerIntervalGameMinutes, definition.intervalGameMinutes),
                    state.lastNativePollTime)
                : start;
        }
        else
        {
            state.nextAlarmGameMinutes = currentGameMinutes + std::max(0.0, definition.initialDelayGameMinutes);
        }
        state.eligibleGameMinutes = currentGameMinutes;
        return state;
    }

    state.eligibleGameMinutes = currentGameMinutes + LegacyTimerPollDelayGameMinutes;

    if (definition.scheduleKind == ScriptedEventTimerScheduleKind::Interval)
    {
        const double intervalGameMinutes = static_cast<double>(definition.intervalHalfMinutes) * 0.5;
        state.nextAlarmGameMinutes = currentGameMinutes + intervalGameMinutes;
        return state;
    }

    const double periodGameMinutes = calendarPeriodGameMinutes(definition.scheduleKind);

    // Legacy OnTimer records are normal Timer calls. OnLongTimer records are refill timers and follow the
    // first-visit/last-visit path below.
    if (definition.triggerKind == ScriptedEventTimerTriggerKind::Timer)
    {
        if (definition.scheduleKind == ScriptedEventTimerScheduleKind::Daily)
        {
            state.nextAlarmGameMinutes = nextExactAlarmGameMinutes(
                dailyStartOffsetGameMinutes(definition),
                periodGameMinutes,
                currentGameMinutes);
            return state;
        }

        state.nextAlarmGameMinutes = currentGameMinutes + periodGameMinutes;
        return state;
    }

    const bool hasLastVisit = lastVisitTime != 0;
    const double lastVisitGameMinutes = legacyTimerGameMinutesFromTicks(lastVisitTime);
    state.nextAlarmGameMinutes = hasLastVisit
        ? lastVisitGameMinutes + periodGameMinutes
        : currentGameMinutes;
    state.initialCalendarFirePending = !hasLastVisit;
    return state;
}

void advanceTimerAfterFire(ScriptedEventTimerState &state, double currentGameMinutes)
{
    const ScriptedEventTimerDefinition &definition = state.definition;

    if (definition.origin == ScriptedEventTimerOrigin::Native)
    {
        if (definition.repeating)
        {
            const double intervalGameMinutes =
                std::max(MinimumNativeTimerIntervalGameMinutes, definition.intervalGameMinutes);
            state.nextAlarmGameMinutes = definition.scheduleKind == ScriptedEventTimerScheduleKind::Interval
                ? nextNativeExactAlarmGameMinutes(state.nextAlarmGameMinutes,
                    intervalGameMinutes, state.lastNativePollTime)
                : state.nextAlarmGameMinutes + intervalGameMinutes;
        }
        else
        {
            state.hasFired = true;
        }

        return;
    }

    if (definition.scheduleKind == ScriptedEventTimerScheduleKind::Interval)
    {
        state.nextAlarmGameMinutes = currentGameMinutes
            + static_cast<double>(definition.intervalHalfMinutes) * 0.5;
        state.initialCalendarFirePending = false;
        return;
    }

    const double periodGameMinutes = calendarPeriodGameMinutes(definition.scheduleKind);

    if (state.initialCalendarFirePending)
    {
        state.nextAlarmGameMinutes = currentGameMinutes + periodGameMinutes;
        state.initialCalendarFirePending = false;
        return;
    }

    while (state.nextAlarmGameMinutes <= currentGameMinutes)
    {
        state.nextAlarmGameMinutes += periodGameMinutes;
    }
}
}

double legacyTimerGameMinutesFromTicks(int64_t legacyTicks)
{
    return static_cast<double>(legacyTicks) / LegacyTicksPerGameMinute;
}

bool startDynamicScriptedEventTimer(
    DynamicScriptedEventTimers &timers, uint16_t eventId, double intervalGameMinutes, double currentGameMinutes,
    const DynamicScriptedEventTimerOptions &options)
{
    double alarm = currentGameMinutes + options.initialDelayGameMinutes.value_or(intervalGameMinutes);
    if (eventId == 0 || !std::isfinite(intervalGameMinutes) || intervalGameMinutes <= 0.0
        || !std::isfinite(currentGameMinutes) || currentGameMinutes < 0.0 || !std::isfinite(alarm)
        || timers.nextId == std::numeric_limits<uint64_t>::max()
        || (options.initialDelayGameMinutes && *options.initialDelayGameMinutes < 0.0))
    {
        return false;
    }
    if (!timers.lastPollGameMinutes)
    {
        timers.lastPollGameMinutes = options.lastVisitGameMinutes;
    }
    if (options.absoluteStartGameMinutes)
    {
        alarm = *options.absoluteStartGameMinutes;
        if (!std::isfinite(alarm) || alarm < 0.0)
        {
            return false;
        }
        const double last = *timers.lastPollGameMinutes;
        if (alarm <= currentGameMinutes && alarm <= last)
        {
            alarm = nextNativeExactAlarmGameMinutes(alarm, intervalGameMinutes,
                static_cast<int64_t>(std::llround(last * LegacyTicksPerGameMinute)));
        }
    }
    else if (options.pastAware)
    {
        alarm = currentGameMinutes;
    }
    DynamicScriptedEventTimer timer{timers.nextId++, eventId, intervalGameMinutes, alarm, false};
    timer.scope = options.scope;
    timer.exact = options.absoluteStartGameMinutes.has_value();
    timers.instances.push_back(timer);
    return true;
}

double scriptedEventRealTimeSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool delayDynamicScriptedEvent(
    DynamicScriptedEventTimers &timers, uint16_t eventId, double delaySeconds, ScriptedEventScope scope)
{
    if (eventId == 0 || !std::isfinite(delaySeconds) || delaySeconds < 0.0
        || timers.nextId == std::numeric_limits<uint64_t>::max())
    {
        return false;
    }
    DynamicScriptedEventTimer timer{timers.nextId++, eventId, 0.0,
        scriptedEventRealTimeSeconds() + delaySeconds, false};
    timer.scope = scope;
    timer.realTime = true;
    timers.instances.push_back(timer);
    return true;
}

void stopCurrentDynamicScriptedEventTimer(DynamicScriptedEventTimers &timers)
{
    for (DynamicScriptedEventTimer &timer : timers.instances)
    {
        if (timer.id == timers.executingId)
        {
            timer.removed = true;
            return;
        }
    }
}

bool updateDynamicScriptedEventTimers(
    DynamicScriptedEventTimers &timers, double currentGameMinutes,
    const std::function<bool(const ScriptedEventTimerDefinition &)> &executeTimer, double realTimeSeconds)
{
    bool executedAny = false;
    const size_t count = timers.instances.size();
    for (size_t index = 0; index < count; ++index)
    {
        // Copy before dispatch: callbacks can append new registrations and reallocate the vector.
        const DynamicScriptedEventTimer timer = timers.instances[index];
        if (timer.removed || (timer.realTime ? realTimeSeconds : currentGameMinutes) < timer.nextAlarmGameMinutes)
        {
            continue;
        }
        ScriptedEventTimerDefinition definition;
        definition.eventId = timer.eventId;
        definition.sourceEventId = timer.eventId;
        definition.scope = timer.scope;
        definition.repeating = !timer.realTime;
        definition.intervalGameMinutes = timer.intervalGameMinutes;
        const uint64_t previousExecutingId = timers.executingId;
        timers.executingId = timer.id;
        try
        {
            executedAny = executeTimer(definition) || executedAny;
        }
        catch (...)
        {
            timers.executingId = previousExecutingId;
            throw;
        }
        timers.executingId = previousExecutingId;
        if (timer.realTime)
        {
            timers.instances[index].removed = true;
        }
        else if (timer.exact)
        {
            const double last = timers.lastPollGameMinutes.value_or(currentGameMinutes);
            timers.instances[index].nextAlarmGameMinutes = nextNativeExactAlarmGameMinutes(
                timer.nextAlarmGameMinutes, timer.intervalGameMinutes,
                static_cast<int64_t>(std::llround(last * LegacyTicksPerGameMinute)));
        }
        else
        {
            // Source NextPeriod uses firing time, without catch-up callbacks after a slow frame.
            timers.instances[index].nextAlarmGameMinutes = currentGameMinutes + timer.intervalGameMinutes;
        }
    }
    timers.lastPollGameMinutes = currentGameMinutes;
    std::erase_if(timers.instances, [](const DynamicScriptedEventTimer &timer) { return timer.removed; });
    return executedAny;
}

int64_t legacyTimerTicksFromGameMinutes(double gameMinutes)
{
    const double nonnegativeGameMinutes = std::max(0.0, gameMinutes);
    const double legacyTicks = nonnegativeGameMinutes * LegacyTicksPerGameMinute;

    if (legacyTicks >= static_cast<double>(std::numeric_limits<int64_t>::max()))
    {
        return std::numeric_limits<int64_t>::max();
    }

    return static_cast<int64_t>(std::llround(legacyTicks));
}

bool sameScriptedEventTimerIdentity(
    const ScriptedEventTimerDefinition &left,
    const ScriptedEventTimerDefinition &right)
{
    if (left.scope != right.scope || left.origin != right.origin || left.sourceEventId != right.sourceEventId)
    {
        return false;
    }

    if (left.origin == ScriptedEventTimerOrigin::Legacy)
    {
        return left.triggerStep == right.triggerStep && left.triggerKind == right.triggerKind;
    }

    return left.registrationIndex == right.registrationIndex;
}

std::vector<ScriptedEventTimerState> reconcileScriptedEventTimers(
    const std::vector<ScriptedEventTimerState> &savedStates,
    const std::vector<ScriptedEventTimerDefinition> &definitions,
    double currentGameMinutes,
    int64_t lastVisitTime,
    bool resetLegacyTimers)
{
    std::vector<ScriptedEventTimerState> states;
    states.reserve(definitions.size());
    std::vector<bool> usedSavedStates(savedStates.size(), false);

    for (const ScriptedEventTimerDefinition &definition : definitions)
    {
        size_t matchedIndex = savedStates.size();

        const bool rebuildAnchoredNativeTimer = definition.origin == ScriptedEventTimerOrigin::Native
            && definition.scheduleKind == ScriptedEventTimerScheduleKind::Interval;
        if (!definition.temporaryMapLifecycle && !rebuildAnchoredNativeTimer
            && !(resetLegacyTimers && definition.origin == ScriptedEventTimerOrigin::Legacy))
        {
            for (size_t savedIndex = 0; savedIndex < savedStates.size(); ++savedIndex)
            {
                if (!usedSavedStates[savedIndex]
                    && sameScriptedEventTimerIdentity(savedStates[savedIndex].definition, definition))
                {
                    matchedIndex = savedIndex;
                    break;
                }
            }

            if (matchedIndex == savedStates.size())
            {
                for (size_t savedIndex = 0; savedIndex < savedStates.size(); ++savedIndex)
                {
                    if (!usedSavedStates[savedIndex]
                        && savedStates[savedIndex].migratedFromLegacySave
                        && savedStates[savedIndex].definition.eventId == definition.eventId)
                    {
                        matchedIndex = savedIndex;
                        break;
                    }
                }
            }
        }

        if (matchedIndex == savedStates.size())
        {
            states.push_back(initializeTimer(definition, currentGameMinutes, lastVisitTime));
            continue;
        }

        ScriptedEventTimerState state = savedStates[matchedIndex];
        state.definition = definition;
        if (state.migratedFromLegacySave)
        {
            state.nextAlarmGameMinutes =
                currentGameMinutes + state.migratedRemainingGameMinutes;
            state.eligibleGameMinutes = currentGameMinutes;
            state.migratedFromLegacySave = false;
            state.migratedRemainingGameMinutes = 0.0;
        }
        usedSavedStates[matchedIndex] = true;
        states.push_back(std::move(state));
    }

    return states;
}

bool updateScriptedEventTimers(
    std::vector<ScriptedEventTimerState> &states,
    double currentGameMinutes,
    const std::function<bool(const ScriptedEventTimerDefinition &)> &executeTimer)
{
    bool executedAny = false;

    for (ScriptedEventTimerState &state : states)
    {
        const bool anchoredNativeTimer = state.definition.origin == ScriptedEventTimerOrigin::Native
            && state.definition.scheduleKind == ScriptedEventTimerScheduleKind::Interval;
        // The native clock can lie between original ticks. Rounding up would consume a future poll
        // before its alarm is eligible (for example 2.999 followed by exactly 3 game minutes).
        const int64_t currentPollTime = anchoredNativeTimer
            ? legacyTimerTicksFromGameMinutes(std::floor(currentGameMinutes * LegacyTicksPerGameMinute)
                / LegacyTicksPerGameMinute)
            : 0;
        if (anchoredNativeTimer && currentPollTime == state.lastNativePollTime)
        {
            continue;
        }

        if (!(state.hasFired && !state.definition.repeating)
            && currentGameMinutes >= state.eligibleGameMinutes
            && currentGameMinutes >= state.nextAlarmGameMinutes)
        {
            executedAny = executeTimer(state.definition) || executedAny;
            advanceTimerAfterFire(state, currentGameMinutes);
        }
        if (anchoredNativeTimer)
        {
            state.lastNativePollTime = currentPollTime;
        }
    }

    return executedAny;
}
}
