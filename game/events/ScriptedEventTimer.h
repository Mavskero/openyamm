#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace OpenYAMM::Game
{
enum class ScriptedEventScope : uint8_t
{
    Map,
    Global,
};

enum class ScriptedEventTimerOrigin : uint8_t
{
    Native,
    Legacy,
};

enum class ScriptedEventTimerTriggerKind : uint8_t
{
    None,
    Timer,
    LongTimer,
};

enum class ScriptedEventTimerScheduleKind : uint8_t
{
    Relative,
    Interval,
    Daily,
    Weekly,
    Monthly,
    Yearly,
};

struct ScriptedEventTimerDefinition
{
    ScriptedEventScope scope = ScriptedEventScope::Map;
    ScriptedEventTimerOrigin origin = ScriptedEventTimerOrigin::Native;
    ScriptedEventTimerTriggerKind triggerKind = ScriptedEventTimerTriggerKind::None;
    ScriptedEventTimerScheduleKind scheduleKind = ScriptedEventTimerScheduleKind::Relative;
    uint16_t eventId = 0;
    uint16_t sourceEventId = 0;
    uint8_t triggerStep = 0;
    uint32_t registrationIndex = 0;
    bool repeating = false;
    // Derived from Lua metadata, never persisted: callbacks are re-created for each map entry.
    bool temporaryMapLifecycle = false;
    uint16_t intervalHalfMinutes = 0;
    int startHour = 0;
    int startMinute = 0;
    int startSecond = 0;
    double intervalGameMinutes = 0.0;
    double initialDelayGameMinutes = 0.0;
};

struct ScriptedEventTimerState
{
    ScriptedEventTimerDefinition definition;
    double nextAlarmGameMinutes = 0.0;
    double eligibleGameMinutes = 0.0;
    bool hasFired = false;
    bool initialCalendarFirePending = false;
    bool migratedFromLegacySave = false;
    double migratedRemainingGameMinutes = 0.0;
    // Anchored native timers are re-registered on map load, so their poll cursor is transient.
    int64_t lastNativePollTime = -1;
};

// Source Timer(callback, period) registrations exist only for the current map visit.
// Repeated registrations of the same callback are independent; they are never saved.
struct DynamicScriptedEventTimer
{
    uint64_t id = 0;
    uint16_t eventId = 0;
    double intervalGameMinutes = 0.0;
    double nextAlarmGameMinutes = 0.0;
    bool removed = false;
    ScriptedEventScope scope = ScriptedEventScope::Map;
    bool exact = false;
    bool realTime = false;
};

struct DynamicScriptedEventTimers
{
    std::vector<DynamicScriptedEventTimer> instances;
    uint64_t nextId = 1;
    uint64_t executingId = 0;
    std::optional<double> lastPollGameMinutes;
};

struct DynamicScriptedEventTimerOptions
{
    ScriptedEventScope scope = ScriptedEventScope::Map;
    std::optional<double> absoluteStartGameMinutes;
    std::optional<double> initialDelayGameMinutes;
    double lastVisitGameMinutes = -1.0 / 256.0;
    bool pastAware = false;
};

bool startDynamicScriptedEventTimer(
    DynamicScriptedEventTimers &timers, uint16_t eventId, double intervalGameMinutes, double currentGameMinutes,
    const DynamicScriptedEventTimerOptions &options = {});
double scriptedEventRealTimeSeconds();
bool delayDynamicScriptedEvent(
    DynamicScriptedEventTimers &timers, uint16_t eventId, double delaySeconds, ScriptedEventScope scope);
void stopCurrentDynamicScriptedEventTimer(DynamicScriptedEventTimers &timers);
bool updateDynamicScriptedEventTimers(
    DynamicScriptedEventTimers &timers, double currentGameMinutes,
    const std::function<bool(const ScriptedEventTimerDefinition &)> &executeTimer,
    double realTimeSeconds = scriptedEventRealTimeSeconds());

double legacyTimerGameMinutesFromTicks(int64_t legacyTicks);
int64_t legacyTimerTicksFromGameMinutes(double gameMinutes);

bool sameScriptedEventTimerIdentity(
    const ScriptedEventTimerDefinition &left,
    const ScriptedEventTimerDefinition &right);

std::vector<ScriptedEventTimerState> reconcileScriptedEventTimers(
    const std::vector<ScriptedEventTimerState> &savedStates,
    const std::vector<ScriptedEventTimerDefinition> &definitions,
    double currentGameMinutes,
    int64_t lastVisitTime,
    bool resetLegacyTimers);

bool updateScriptedEventTimers(
    std::vector<ScriptedEventTimerState> &states,
    double currentGameMinutes,
    const std::function<bool(const ScriptedEventTimerDefinition &)> &executeTimer);
}
