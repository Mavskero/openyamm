#include "doctest/doctest.h"

#include "game/maps/MapRegistry.h"
#include "game/gameplay/GameplayDialogController.h"
#include "game/gameplay/TravelRuntime.h"
#include "game/mm9/Mm9RudeDialogue.h"
#include "game/mm9/Mm9TransportRoute.h"
#include "game/party/Party.h"
#include "game/ui/GameplayUiController.h"
#include "tests/PartySpellTestHarness.h"

#include <string>
#include <vector>

namespace
{
std::string rudeRow(uint32_t rudeId = 18, int requiredRawKey = 0, int forbiddenRawKey = 0)
{
    std::vector<std::string> columns(30, "0");
    columns[0] = std::to_string(rudeId);
    columns[1] = std::to_string(rudeId);
    columns[2] = "1";
    columns[3] = "Yes please.";
    columns[4] = "Great!";
    columns[5] = "-5";
    columns[6] = std::to_string(requiredRawKey);
    columns[20] = std::to_string(forbiddenRawKey);
    std::string result;
    for (size_t index = 0; index < columns.size(); ++index)
    {
        if (index != 0)
        {
            result += ',';
        }
        result += columns[index];
    }
    return result + '\n';
}

TEST_CASE("MM9 transport service keeps NPC 209 source quest gate")
{
    OpenYAMM::Game::Mm9RudeDialogueTable rudeTable;
    REQUIRE(rudeTable.loadDialogueText("NPC209.rude", rudeRow(209, 73)));
    OpenYAMM::Game::Party party;

    CHECK(rudeTable.visibleRows(209, 209, party).empty());
    party.setQuestBit(90073, true);
    const std::vector<const OpenYAMM::Game::Mm9RudeRow *> visibleRows =
        rudeTable.visibleRows(209, 209, party);
    REQUIRE_EQ(visibleRows.size(), 1);
    CHECK_EQ(visibleRows[0]->next, -5);
}

std::vector<std::string> header()
{
    return {
        "CanonicalId", "DockNpcId", "ShopId", "DockName", "ServiceClassification",
        "SourceRudeRowIndex", "SourceLine", "DestinationName", "DestinationMap", "DestinationMapFile",
        "DestinationStartPoint", "ArrivalX", "ArrivalY", "ArrivalZ", "FacingDegrees", "FacingYawUnits",
        "MovePlayerToFloor", "AvailableWeekday", "SourceDayIndex", "TravelDays", "BasePrice",
        "PriceMultiplier", "RequiredRawKeys", "ForbiddenRawKeys", "RouteOrder", "SourceShop", "SourceRude",
        "ScheduleSource", "ExecutableRva", "ReviewState", "Provenance",
    };
}

std::vector<std::string> row()
{
    return {
        "mm9:transport/18/monday", "18", "18", "Sea's Fang", "dock", "0", "1", "Sturmford",
        "world.mm9.map.sturmford", "sturmford.odm", "StartPoint0", "100", "200", "300", "90", "512",
        "1", "Monday", "2", "2", "20", "3", "73", "", "0", "MMIXSHOPS.txt", "NPC18.rude",
        "BOOK AND SCROLL TEXT.csv", "0x1002a260[2]", "executable_backed",
        "MMIXSHOPS+RUDE+BoatSchedule+cshell.dll",
    };
}

OpenYAMM::Game::MapRegistry mapRegistry()
{
    OpenYAMM::Game::MapStatsEntry target = {};
    target.id = 1;
    target.worldId = "mm9";
    target.canonicalId = "world.mm9.map.sturmford";
    target.fileName = "sturmford.odm";
    OpenYAMM::Game::MapRegistry result;
    result.initialize({target});
    return result;
}
}

TEST_CASE("MM9 transport route table validates RUDE ownership and weekday lookup")
{
    OpenYAMM::Game::Mm9RudeDialogueTable rudeTable;
    REQUIRE(rudeTable.loadDialogueText("NPC18.rude", rudeRow()));
    OpenYAMM::Game::Mm9TransportRouteTable table;
    REQUIRE(table.loadFromRows({header(), row()}, mapRegistry(), rudeTable));
    CHECK(table.errors().empty());

    const OpenYAMM::Game::Mm9TransportRoute *pRoute = table.findForServiceDay(18, 2);
    REQUIRE(pRoute != nullptr);
    CHECK_EQ(pRoute->travelDays, 2);
    CHECK_EQ(pRoute->basePrice, 20);
    CHECK_EQ(pRoute->priceMultiplier, doctest::Approx(3.0f));
    CHECK(table.findForServiceDay(18, 3) == nullptr);
    CHECK_EQ(table.forService(18).size(), 1);
}

TEST_CASE("MM9 transport route conditions use mounted raw-key QBits")
{
    OpenYAMM::Game::Mm9RudeDialogueTable rudeTable;
    REQUIRE(rudeTable.loadDialogueText("NPC18.rude", rudeRow()));
    OpenYAMM::Game::Mm9TransportRouteTable table;
    REQUIRE(table.loadFromRows({header(), row()}, mapRegistry(), rudeTable));
    const OpenYAMM::Game::Mm9TransportRoute *pRoute = table.findForServiceDay(18, 2);
    REQUIRE(pRoute != nullptr);
    OpenYAMM::Game::Party party;

    CHECK_FALSE(OpenYAMM::Game::mm9TransportRouteConditionsSatisfied(*pRoute, party));
    party.setQuestBit(90073, true);
    CHECK(OpenYAMM::Game::mm9TransportRouteConditionsSatisfied(*pRoute, party));
    CHECK_EQ(OpenYAMM::Game::mm9TransportDayIndex(2.0f * 24.0f * 60.0f), 2);
}

TEST_CASE("MM9 transport route table rejects duplicate service weekday ownership")
{
    OpenYAMM::Game::Mm9RudeDialogueTable rudeTable;
    REQUIRE(rudeTable.loadDialogueText("NPC18.rude", rudeRow()));
    std::vector<std::string> duplicate = row();
    duplicate[0] = "mm9:transport/18/monday-duplicate";
    OpenYAMM::Game::Mm9TransportRouteTable table;

    CHECK_FALSE(table.loadFromRows({header(), row(), duplicate}, mapRegistry(), rudeTable));
    REQUIRE_EQ(table.errors().size(), 1);
    CHECK(table.errors().front().find("duplicate service weekday ownership") != std::string::npos);
}

TEST_CASE("MM9 RUDE transport route queues an exact arrival before charging at travel commit")
{
    OpenYAMM::Game::Mm9RudeDialogueTable rudeTable;
    REQUIRE(rudeTable.loadDialogueText("NPC18.rude", rudeRow()));
    OpenYAMM::Game::Mm9TransportRouteTable routeTable;
    REQUIRE(routeTable.loadFromRows({header(), row()}, mapRegistry(), rudeTable));
    OpenYAMM::Game::Party party;
    party.seed(OpenYAMM::Tests::createSpellRegressionPartySeed());
    party.setQuestBit(90073, true);
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime;
    worldRuntime.bindParty(&party);
    worldRuntime.advanceGameMinutes(2.0f * 24.0f * 60.0f);
    OpenYAMM::Game::EventRuntimeState eventRuntimeState;
    worldRuntime.bindEventRuntimeState(&eventRuntimeState);
    OpenYAMM::Game::GameplayUiController uiController;
    OpenYAMM::Game::EventDialogContent dialog = {};
    size_t selectionIndex = 0;
    OpenYAMM::Game::GameplayDialogController::Context context = {
        .uiController = uiController,
        .eventRuntimeState = eventRuntimeState,
        .activeEventDialog = dialog,
        .selectionIndex = selectionIndex,
        .pParty = &party,
        .pWorldRuntime = &worldRuntime,
        .pMm9RudeDialogueTable = &rudeTable,
        .pMm9TransportRouteTable = &routeTable,
    };
    const OpenYAMM::Game::GameplayDialogController controller;
    controller.openMm9RudeDialogue(context, 18);
    REQUIRE_EQ(dialog.actions.size(), 1);

    controller.executeActiveDialogAction(context);
    REQUIRE_EQ(dialog.actions.size(), 2);
    CHECK(dialog.actions[0].kind == OpenYAMM::Game::EventDialogActionKind::Mm9RudeTransportRoute);
    const int goldBefore = party.gold();
    const float minutesBefore = worldRuntime.gameMinutes();

    const OpenYAMM::Game::GameplayDialogController::Result result =
        controller.executeActiveDialogAction(context);
    CHECK(result.shouldCloseActiveDialog);
    CHECK_EQ(party.gold(), goldBefore);
    CHECK_EQ(worldRuntime.gameMinutes(), minutesBefore);
    REQUIRE(eventRuntimeState.pendingMapMove.has_value());
    CHECK(eventRuntimeState.pendingMapMove->mapName == "sturmford.odm");
    CHECK_EQ(eventRuntimeState.pendingMapMove->x, 100);
    CHECK_EQ(eventRuntimeState.pendingMapMove->y, 200);
    CHECK_EQ(eventRuntimeState.pendingMapMove->z, 300);
    REQUIRE(eventRuntimeState.lastMapTransitionRequested.has_value());
    CHECK(eventRuntimeState.lastMapTransitionRequested->sourceKind == "mm9_boat_route");
    CHECK_EQ(eventRuntimeState.lastMapTransitionRequested->travelDays, 2);
    const OpenYAMM::Game::EventRuntimeState::DeferredTravelPlan &plan =
        eventRuntimeState.pendingMapMove->travelPlan;
    CHECK(plan.mode == OpenYAMM::Game::EventRuntimeState::TravelPlanMode::TravelDays);
    CHECK(plan.goldFare > 0);
    CHECK_EQ(plan.travelDays, 2);
    CHECK_EQ(plan.foodRequired, 0);
    CHECK_FALSE(plan.exitCallback.has_value());
    std::string error;
    REQUIRE(OpenYAMM::Game::applyDeferredTravelPlan(party, &worldRuntime, plan, error));
    CHECK(error.empty());
    CHECK_EQ(party.gold(), goldBefore - plan.goldFare);
    CHECK_EQ(worldRuntime.gameMinutes(), minutesBefore + 2.0f * 24.0f * 60.0f);
}

TEST_CASE("MM9 RUDE transport rejects unavailable and unaffordable routes without mutation")
{
    OpenYAMM::Game::Mm9RudeDialogueTable rudeTable;
    REQUIRE(rudeTable.loadDialogueText("NPC18.rude", rudeRow()));
    OpenYAMM::Game::Mm9TransportRouteTable routeTable;
    REQUIRE(routeTable.loadFromRows({header(), row()}, mapRegistry(), rudeTable));
    OpenYAMM::Game::Party party;
    party.seed(OpenYAMM::Tests::createSpellRegressionPartySeed());
    party.setQuestBit(90073, true);
    OpenYAMM::Tests::PartySpellTestWorldRuntime worldRuntime;
    worldRuntime.bindParty(&party);
    OpenYAMM::Game::EventRuntimeState eventRuntimeState;
    worldRuntime.bindEventRuntimeState(&eventRuntimeState);
    OpenYAMM::Game::GameplayUiController uiController;
    OpenYAMM::Game::EventDialogContent dialog = {};
    size_t selectionIndex = 0;
    OpenYAMM::Game::GameplayDialogController::Context context = {
        .uiController = uiController,
        .eventRuntimeState = eventRuntimeState,
        .activeEventDialog = dialog,
        .selectionIndex = selectionIndex,
        .pParty = &party,
        .pWorldRuntime = &worldRuntime,
        .pMm9RudeDialogueTable = &rudeTable,
        .pMm9TransportRouteTable = &routeTable,
    };
    const OpenYAMM::Game::GameplayDialogController controller;
    controller.openMm9RudeDialogue(context, 18);
    controller.executeActiveDialogAction(context);
    REQUIRE_EQ(dialog.actions.size(), 1);
    CHECK(dialog.actions[0].kind == OpenYAMM::Game::EventDialogActionKind::Mm9RudeTransportBack);
    CHECK_FALSE(eventRuntimeState.pendingMapMove.has_value());

    worldRuntime.advanceGameMinutes(2.0f * 24.0f * 60.0f);
    selectionIndex = 0;
    controller.executeActiveDialogAction(context);
    REQUIRE_EQ(dialog.actions.size(), 1);
    selectionIndex = 0;
    controller.executeActiveDialogAction(context);
    REQUIRE_EQ(dialog.actions.size(), 2);
    party.addGold(-party.gold());
    const float minutesBefore = worldRuntime.gameMinutes();
    selectionIndex = 0;
    controller.executeActiveDialogAction(context);
    CHECK_EQ(party.gold(), 0);
    CHECK_EQ(worldRuntime.gameMinutes(), minutesBefore);
    CHECK_FALSE(eventRuntimeState.pendingMapMove.has_value());
}
