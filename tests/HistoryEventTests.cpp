#include "doctest/doctest.h"
#include "game/events/EventRuntime.h"
#include "game/party/Party.h"

TEST_CASE("history Add records the original zero-operand departure events once per continent")
{
    using namespace OpenYAMM::Game;
    const uint32_t historyThree = uint32_t(EvtVariable::HistoryBegin) + 2;
    const uint32_t historyFour = historyThree + 1;
    EventRuntimeState state;
    state.variables[uint32_t(EvtVariable::DayOfYear)] = 2;
    state.variables[uint32_t(EvtVariable::Hour)] = 10;
    for (uint32_t continent : {2u, 1u, 3u})
    {
        setActiveHistoryContinent(state, continent);
        CHECK_FALSE(state.historyEventTimesByContinent[continent].contains(3));
        CHECK_FALSE(state.historyEventTimesByContinent[continent].contains(4));
        EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(historyThree), 0, nullptr, {});
        EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(historyFour), 0, nullptr, {});
        REQUIRE(state.historyEventTimesByContinent[continent].contains(3));
        REQUIRE(state.historyEventTimesByContinent[continent].contains(4));
        CHECK(state.historyEventTimesByContinent[continent].at(3) == 2040);
        CHECK(state.historyEventTimesByContinent[continent].at(4) == 2040);
    }
    state.variables[uint32_t(EvtVariable::DayOfYear)] = 3;
    for (uint32_t continent : {2u, 1u, 3u})
    {
        setActiveHistoryContinent(state, continent);
        EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(historyThree), 0, nullptr, {});
        EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(historyFour), 0, nullptr, {});
        CHECK(state.historyEventTimesByContinent[continent].at(3) == 2040);
        CHECK(state.historyEventTimesByContinent[continent].at(4) == 2040);
    }
    CHECK(state.historyEventTimes.at(3) == 2040);
    CHECK(state.historyEventTimes.at(4) == 2040);
}

TEST_CASE("history Add keeps the first occurrence time for nonzero operands too")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    setActiveHistoryContinent(state, 2);
    state.variables[uint32_t(EvtVariable::DayOfYear)] = 2;
    state.variables[uint32_t(EvtVariable::Hour)] = 10;
    const auto history = EventRuntime::decodeVariable(uint32_t(EvtVariable::HistoryBegin) + 6);
    EventRuntime::addVariableValue(state, history, 1, nullptr, {});
    REQUIRE(state.historyEventTimesByContinent[2].at(7) == 2040);
    state.variables[uint32_t(EvtVariable::Hour)] = 11;
    EventRuntime::addVariableValue(state, history, 123, nullptr, {});
    CHECK(state.historyEventTimesByContinent[2].at(7) == 2040);
    CHECK(EventRuntime::getVariableValue(state, history, nullptr) == 1);
}

TEST_CASE("history Add does not change explicit Set clearing or ordinary numeric addition")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    setActiveHistoryContinent(state, 2);
    const auto history = EventRuntime::decodeVariable(uint32_t(EvtVariable::HistoryBegin) + 6);
    EventRuntime::addVariableValue(state, history, 0, nullptr, {});
    REQUIRE(state.historyEventTimesByContinent[2].contains(7));
    EventRuntime::setVariableValue(state, history, 0, nullptr, {});
    CHECK_FALSE(state.historyEventTimesByContinent[2].contains(7));
    CHECK(EventRuntime::getVariableValue(state, history, nullptr) == 0);
    const auto gold = EventRuntime::decodeVariable(uint32_t(EvtVariable::Gold));
    Party party;
    party.addGold(100);
    EventRuntime::addVariableValue(state, gold, 0, &party, {});
    CHECK(party.gold() == 100);
}
