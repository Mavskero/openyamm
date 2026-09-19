#include "doctest/doctest.h"
#include "game/events/EventDialogContent.h"
#include "game/events/ISceneEventContext.h"

#include <algorithm>

using namespace OpenYAMM::Game;

namespace
{
EventDialogContent travelDialog(const MapStatsEntry *pCurrentMap, bool positioned, const std::string &destination)
{
    EventRuntimeState state;
    EventRuntimeState::PendingDialogueContext context;
    context.kind = DialogueContextKind::MapTransition;
    context.sourceId = static_cast<uint32_t>(MapBoundaryEdge::East);
    if (positioned)
    {
        context.transitionMapMove.emplace();
        context.transitionMapMove->mapName = destination;
        context.transitionMapMove->x = -23076;
        context.transitionMapMove->y = -9472;
        context.transitionMapMove->z = 161;
        context.transitionMapMove->traceSourceKind = "map_boundary";
    }
    state.pendingDialogueContext = context;
    const std::vector<MapStatsEntry> maps = {{.name = "新索匹格镇", .fileName = "oute3.odm"}};
    return buildEventDialogContent(state, 0, false, nullptr, nullptr, nullptr, nullptr, nullptr,
        pCurrentMap, &maps, nullptr, nullptr, 0.0f);
}

bool hasDuration(const EventDialogContent &dialog)
{
    return std::any_of(dialog.lines.begin(), dialog.lines.end(), [](const std::string &line)
    {
        return line.starts_with("It will take ");
    });
}
}

TEST_CASE("positioned outdoor travel shows its charged five days before confirmation")
{
    MapStatsEntry current = {};
    current.fileName = "outd3.odm";
    current.eastTransition.emplace();
    current.eastTransition->destinationMapFileName = "oute3.odm";
    current.eastTransition->travelDays = 5;
    const EventDialogContent dialog = travelDialog(&current, true, "oute3.odm");
    REQUIRE(dialog.isActive);
    CHECK(dialog.title == "新索匹格镇");
    REQUIRE(dialog.lines.size() == 2);
    CHECK(dialog.lines[0] == "It will take 5 days to travel to 新索匹格镇.");
    CHECK(dialog.lines[1] == "Do you wish to enter 新索匹格镇?");
    REQUIRE(dialog.actions.size() == 2);
    CHECK(dialog.actions[0].kind == EventDialogActionKind::MapTransitionConfirm);
    CHECK(dialog.actions[1].kind == EventDialogActionKind::MapTransitionCancel);
}

TEST_CASE("positioned travel does not borrow a different edge route duration")
{
    MapStatsEntry current = {};
    current.fileName = "OUTD3.ODM";
    current.eastTransition.emplace();
    current.eastTransition->destinationMapFileName = "somewhere-else.odm";
    current.eastTransition->travelDays = 2;
    const EventDialogContent dialog = travelDialog(&current, true, "oute3.odm");
    REQUIRE(hasDuration(dialog));
    CHECK(dialog.lines[0] == "It will take 5 days to travel to 新索匹格镇.");
}

TEST_CASE("same map and dungeon moves do not claim multi day outdoor travel")
{
    MapStatsEntry current = {};
    current.fileName = "OUTE3.ODM";
    current.eastTransition.emplace();
    current.eastTransition->destinationMapFileName = "outd3.odm";
    current.eastTransition->travelDays = 5;
    CHECK_FALSE(hasDuration(travelDialog(&current, true, "oute3.odm")));
    CHECK_FALSE(hasDuration(travelDialog(&current, true, "6d02.blv")));
    CHECK_FALSE(hasDuration(travelDialog(&current, true, "")));
    CHECK_FALSE(hasDuration(travelDialog(nullptr, true, "oute3.odm")));
    current.fileName = "6d02.blv";
    CHECK_FALSE(hasDuration(travelDialog(&current, true, "oute3.odm")));
}

TEST_CASE("unpositioned edge travel retains its own one day and zero day prompts")
{
    MapStatsEntry current = {};
    current.fileName = "outd3.odm";
    current.eastTransition.emplace();
    current.eastTransition->destinationMapFileName = "oute3.odm";
    current.eastTransition->travelDays = 1;
    const EventDialogContent dialog = travelDialog(&current, false, "");
    REQUIRE(hasDuration(dialog));
    CHECK(dialog.lines[0] == "It will take 1 day to travel to 新索匹格镇.");
    current.eastTransition->travelDays = 0;
    CHECK_FALSE(hasDuration(travelDialog(&current, false, "")));
}
