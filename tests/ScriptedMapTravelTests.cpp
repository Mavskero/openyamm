#include "doctest/doctest.h"
#include "tests/HouseDialogueTestHarness.h"
#include "game/gameplay/TravelRuntime.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Tests;

namespace
{
const RegressionGameData &data()
{
    REQUIRE_MESSAGE(regressionGameDataLoaded(), regressionGameDataFailure());
    return regressionGameData();
}

void choose(HouseDialogueTestHarness &harness, EventDialogActionKind kind)
{
    const EventDialogContent &dialog = harness.presentPendingDialog(0, true);
    REQUIRE(dialog.isActive);
    const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(),
        [kind](const EventDialogAction &action) { return action.kind == kind; });
    REQUIRE(found != dialog.actions.end());
    const size_t index = size_t(found - dialog.actions.begin());
    CHECK(harness.executeActiveDialogAction(index).shouldCloseActiveDialog);
}
}

TEST_CASE("scripted outdoor confirmation and cancellation do not charge implicit travel or heal")
{
    // Original Icon8 MoveToMap is distinct from table-backed WalkToMap. The third pair makes sure
    // the shared rule is about scripted moves, rather than a hardcoded Shoals filename exception.
    const std::array<std::pair<std::string, std::string>, 3> routes = {{{"out14.odm", "7out15.odm"},
        {"7out15.odm", "out14.odm"}, {"Out02.odm", "Out06.odm"}}};
    for (const auto &[from, to] : routes)
    {
        for (bool confirm : {false, true})
        {
            for (int food : {0, 10})
            {
                CAPTURE(from);
                CAPTURE(to);
                CAPTURE(confirm);
                CAPTURE(food);
                HouseDialogueTestHarness harness(data());
                MapStatsEntry map = {};
                map.fileName = from;
                map.name = "Source map";
                // Even a matching normal edge route must not lend its cost to a positioned script.
                map.westTransition.emplace();
                map.westTransition->destinationMapFileName = to;
                map.westTransition->travelDays = 1;
                harness.setCurrentMap(map);
                harness.party().addFood(food - harness.party().food());
                harness.party().applyPartyBuff(PartyBuffId::TorchLight, 600.0f, 1, 0, 0, SkillMastery::None, 0);
                Character *pMember = harness.party().activeMember();
                REQUIRE(pMember != nullptr);
                pMember->health = 1;
                pMember->spellPoints = 0;
                pMember->levelModifier = 3;
                pMember->armorClassModifier = 4;
                pMember->conditions.reset();
                const Party::Snapshot before = harness.party().snapshot();
                const double clock = harness.worldRuntime().gameMinutes();

                EventRuntimeState::PendingDialogueContext prompt;
                prompt.kind = DialogueContextKind::MapTransition;
                prompt.sourceId = uint32_t(MapBoundaryEdge::West);
                prompt.transitionImageId = 8;
                prompt.transitionMapMove.emplace();
                prompt.transitionMapMove->mapName = to;
                prompt.transitionMapMove->x = 100;
                prompt.transitionMapMove->y = 200;
                prompt.transitionMapMove->z = 300;
                harness.eventRuntimeState().pendingDialogueContext = prompt;
                choose(harness, confirm ? EventDialogActionKind::MapTransitionConfirm
                    : EventDialogActionKind::MapTransitionCancel);

                CHECK(harness.worldRuntime().gameMinutes() == clock);
                CHECK(harness.party().food() == food);
                const Party::Snapshot after = harness.party().snapshot();
                REQUIRE(after.members.size() == before.members.size());
                for (size_t index = 0; index < before.members.size(); ++index)
                {
                    CHECK(after.members[index].health == before.members[index].health);
                    CHECK(after.members[index].spellPoints == before.members[index].spellPoints);
                    CHECK(after.members[index].conditions == before.members[index].conditions);
                    CHECK(after.members[index].levelModifier == before.members[index].levelModifier);
                    CHECK(after.members[index].armorClassModifier == before.members[index].armorClassModifier);
                }
                CHECK(after.partyBuffs[size_t(PartyBuffId::TorchLight)].remainingSeconds
                    == before.partyBuffs[size_t(PartyBuffId::TorchLight)].remainingSeconds);
                const auto &move = harness.eventRuntimeState().pendingMapMove;
                CHECK(move.has_value() == confirm);
                if (confirm)
                {
                    REQUIRE(move.has_value());
                    CHECK(move->mapName == to);
                    CHECK(move->x == 100);
                    CHECK(move->y == 200);
                    CHECK(move->z == 300);
                }
            }
        }
    }
}

TEST_CASE("explicit outdoor walking route still charges its declared time and food and rests")
{
    // Exercise the two actual walking confirmation forms, including their deferred settlement.
    for (bool positionedBoundary : {false, true})
    {
        CAPTURE(positionedBoundary);
        HouseDialogueTestHarness harness(data());
        MapStatsEntry map = {};
        map.fileName = "Out02.odm";
        map.name = "Source map";
        map.eastTransition.emplace();
        map.eastTransition->destinationMapFileName = "Out06.odm";
        map.eastTransition->travelDays = 5;
        harness.setCurrentMap(map);
        harness.eventRuntimeState().mapFileName = map.fileName;
        // Leave Event followers empty: the current Party owns hired followers for both quote/plan.
        harness.party().addHiredNpcFollower({2001, 5, 100});
        harness.party().addFood(10 - harness.party().food());
        Character *pMember = harness.party().activeMember();
        REQUIRE(pMember != nullptr);
        pMember->health = 1;
        pMember->spellPoints = 0;
        const double clock = harness.worldRuntime().gameMinutes();
        EventRuntimeState::PendingDialogueContext prompt;
        prompt.kind = DialogueContextKind::MapTransition;
        prompt.sourceId = uint32_t(MapBoundaryEdge::East);
        if (positionedBoundary)
        {
            prompt.transitionMapMove.emplace();
            prompt.transitionMapMove->mapName = "Out06.odm";
            prompt.transitionMapMove->traceSourceKind = "map_boundary";
            prompt.transitionMapMove->traceSourceId = prompt.sourceId;
        }
        harness.eventRuntimeState().pendingDialogueContext = prompt;
        const EventDialogContent quote = buildEventDialogContent(
            harness.eventRuntimeState(), 0, false, nullptr, nullptr, nullptr, nullptr, nullptr,
            &map, nullptr, &harness.party(), &harness.worldRuntime(), float(clock));
        REQUIRE_FALSE(quote.lines.empty());
        CHECK(quote.lines[0] == "It will take 4 days to travel to Out06.odm.");
        choose(harness, EventDialogActionKind::MapTransitionConfirm);
        REQUIRE(harness.eventRuntimeState().pendingMapMove.has_value());
        CHECK(harness.eventRuntimeState().pendingMapMove->mapName == "Out06.odm");
        const auto plan = harness.eventRuntimeState().pendingMapMove->travelPlan;
        CHECK(plan.mode == EventRuntimeState::TravelPlanMode::TravelDays);
        CHECK(plan.travelDays == 4);
        CHECK(plan.foodRequired == 5);
        CHECK(harness.worldRuntime().gameMinutes() == clock);
        CHECK(harness.party().food() == 10);
        std::string error;
        REQUIRE_MESSAGE(applyDeferredTravelPlan(harness.party(), &harness.worldRuntime(), plan, error), error);
        CHECK(harness.worldRuntime().gameMinutes() == clock + 4 * 24 * 60);
        CHECK(harness.party().food() == 5);
        CHECK(harness.party().activeMember()->health == harness.party().activeMember()->maxHealth);
        CHECK(harness.party().activeMember()->spellPoints == harness.party().activeMember()->maxSpellPoints);
    }
}
