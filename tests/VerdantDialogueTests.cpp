#include "tests/MergeCrossContinentsHarness.h"
#include "game/gameplay/GameplayDialogController.h"
#include "game/maps/SaveGame.h"
#include <filesystem>

using namespace OpenYAMM::Game;
using OpenYAMM::Tests::MergeQuestFixture;

TEST_CASE("Verdant root topic gates and Enroth/Antagarich/Jadam text use physical continent")
{
    for (uint32_t continent : {1, 2, 3})
    {
        for (bool main : {false, true})
        {
            for (bool all : {false, true})
            {
                for (bool finished : {false, true})
                {
                    MergeQuestFixture f(continent, true);
                    f.flag("MetVerdant", 1);
                    f.flag("GotMainQuest", main);
                    f.flag("AllStoriesFinished", all);
                    f.flag("ContinentFinished." + std::to_string(continent), finished);
                    f.flag("QuestFinished", all);
                    f.enter();
                    const bool show = main && !all && !finished;
                    CHECK_EQ(f.visible(uint16_t(1781 + continent)), show);
                    CHECK_EQ(f.slot(0), show ? 1781 + continent : 0);
                    CHECK_EQ(f.slot(1), main ? 732 : (all ? 0 : 1779));
                    if (show)
                    {
                        f.topic(uint16_t(1781 + continent));
                        const auto expected = OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2153 + continent);
                        REQUIRE(expected.has_value());
                        REQUIRE_EQ(f.state.messages.size(), 1);
                        CHECK_EQ(f.state.messages.front(), *expected);
                    }
                    CHECK_EQ(f.visible(1781), false);
                    CHECK_EQ(f.slot(3), 0); // Unsupported Breach never opens.
                }
            }
        }
    }
}

TEST_CASE("Hints branch maps MM7 Q611 before Q612 and uses imported greeting and TTGuide")
{
    for (uint32_t continent : {1, 2, 3})
    {
        for (uint32_t bits : {0, 1, 2, 3})
        {
            MergeQuestFixture f(continent, true);
            f.flag("GotMainQuest", 1);
            f.flag("MetVerdant", 1);
            f.flag("GotConnectorStone", 1);
            f.party.setQuestBit(611, (bits & 1) != 0);
            f.party.setQuestBit(612, (bits & 2) != 0);
            f.enter();
            REQUIRE(f.visible(732));
            f.topic(732);
            CHECK_EQ(f.state.dialogueState.scriptedBranch, "Hints");
            CHECK_EQ(f.state.dialogueState.scriptedBranchNpcId, 803);
            REQUIRE_EQ(f.state.messages.size(), 1);
            const auto *greeting = OpenYAMM::Tests::regressionGameData().npcDialogTable.getGreeting(26);
            REQUIRE(greeting != nullptr);
            CHECK_NE(greeting->greetingPrimary, greeting->greetingSecondary);
            CHECK_EQ(f.state.messages.front(), greeting->greetingSecondary);
            const uint32_t hint = continent == 2 && (bits & 1) ? 894
                : continent == 2 && (bits & 2) ? 892 : std::vector<uint32_t>{732, 888, 1359}[continent - 1];
            CHECK_EQ(f.slot(0), 1781);
            CHECK_EQ(f.slot(1), hint);
            CHECK_EQ(f.slot(2), 1788); // Source ConStone replacement action is implemented in139.
            CHECK(f.visible(uint16_t(hint)));
            CHECK_FALSE(f.visible(uint16_t(1781 + continent)));
            f.topic(1781);
            REQUIRE_EQ(f.state.messages.size(), 1);
            CHECK_EQ(f.state.messages.front(), *OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2153));
        }
    }
}

TEST_CASE("dimension door ContTopic and Hint completion gates intentionally use different source switches")
{
    MergeQuestFixture f(2, true);
    f.flag("GotMainQuest", 1);
    f.flag("MetVerdant", 1);
    f.flag("ContinentFinished.2", 1);
    f.state.dimensionDoorActive = true;
    f.enter();
    CHECK(f.visible(1783)); // switch4 has no completion entry, but text remains physical MM7.
    f.topic(1783);
    CHECK_EQ(f.state.messages.front(), *OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2155));
    f.topic(732);
    CHECK(f.visible(888)); // Hint gate is min(4,3), not the completed MM7 entry.
    f.flag("ContinentFinished.3", 1);
    CHECK_FALSE(f.visible(888));
    f.enter();
    CHECK(f.visible(1783));
    f.flag("ContinentFinished.4", 1);
    CHECK_FALSE(f.visible(1783));
}

TEST_CASE("all five standard hints execute original dynamic global handlers rather than unrelated NPCText ids")
{
    for (uint16_t topic : {732, 888, 894, 892, 1359})
    {
        const uint32_t continent = topic == 732 ? 1 : topic == 1359 ? 3 : 2;
        MergeQuestFixture f(continent, true);
        MergeQuestFixture original(continent, true, false);
        f.flag("GotMainQuest", 1);
        f.flag("MetVerdant", 1);
        if (topic == 894) f.party.setQuestBit(611, true);
        if (topic == 892) f.party.setQuestBit(612, true);
        f.enter();
        f.topic(732);
        original.party.restoreSnapshot(f.party.snapshot());
        original.talkContext(803);
        original.topic(topic);
        f.topic(topic);
        CHECK_EQ(f.state.messages, original.state.messages);
        CHECK_EQ(f.party.snapshot().questBits, original.party.snapshot().questBits);
        CHECK_EQ(f.party.snapshot().eventVariables, original.party.snapshot().eventVariables);
        CHECK_EQ(f.state.variables, original.state.variables);
        CHECK_FALSE(f.state.messages.empty());
    }
}

TEST_CASE("scoped dispatcher preserves original handlers and predicates for other NPCs and new contexts")
{
    for (uint16_t topic : {732, 888, 894, 892, 1359})
    {
        MergeQuestFixture f(1, true);
        MergeQuestFixture original(1, true, false);
        f.enter(800);
        original.talkContext(800);
        CHECK_EQ(f.visible(topic), original.visible(topic));
        f.topic(topic);
        original.topic(topic);
        CHECK_EQ(f.state.messages, original.state.messages);
        CHECK_EQ(f.party.snapshot().questBits, original.party.snapshot().questBits);
        CHECK_FALSE(f.state.npcTopicOverrides.contains(803));
    }
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + R"lua(
RegisterGlobalEvent(60180, nil, function() evt.SetGlobalVar("Original", evt.GetGlobalVar("Original",0)+1) end)
RegisterCanShowTopic(60180, function() return evt.GetGlobalVar("OriginalVisible",0) ~= 0 end)
RegisterNpcScopedTopic(803, 60180, function() evt.SetGlobalVar("Scoped", 1) end, function() return true end)
)lua", "@scoped-predicate-controlled-fixture", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    MergeQuestFixture f;
    f.global = program;
    f.talkContext(800);
    CHECK_FALSE(f.visible(60180));
    f.state.namedGlobalVars["OriginalVisible"] = 1;
    CHECK(f.visible(60180));
    f.topic(60180);
    CHECK_EQ(f.state.namedGlobalVars.at("Original"), 1);
    f.talkContext(803);
    f.topic(60180);
    CHECK_EQ(f.state.namedGlobalVars.at("Scoped"), 1);
    f.talkContext(800); // A newly built execution context cannot retain803 identity.
    f.topic(60180);
    CHECK_EQ(f.state.namedGlobalVars.at("Original"), 2);
}

TEST_CASE("ordinary outdoor close113 returns Hints to root, whereas root and house dialogue close normally")
{
    MergeQuestFixture f(1, true);
    f.flag("GotMainQuest", 1);
    f.enter();
    f.topic(732);
    GameplayUiController ui;
    EventDialogContent active;
    active.isActive = true;
    active.sourceId = 803;
    size_t selection = 0;
    GameplayDialogController::Context context{ui, f.state, active, selection};
    context.pWorldRuntime = &f;
    context.pParty = &f.party;
    context.pGlobalEventProgram = &f.global;
    GameplayDialogController controller;
    const auto returned = controller.handleDialogueCloseRequest(context);
    CHECK(returned.shouldOpenPendingEventDialog);
    CHECK_FALSE(returned.shouldCloseActiveDialog);
    CHECK_EQ(f.state.dialogueState.scriptedBranch, "");
    CHECK_EQ(f.slot(0), 1782);
    CHECK_EQ(f.slot(1), 732);
    const auto root = controller.handleDialogueCloseRequest(context);
    CHECK(root.shouldCloseActiveDialog);
    f.enter(803, 185);
    f.topic(732);
    const auto house = controller.handleDialogueCloseRequest(context);
    CHECK(house.shouldCloseActiveDialog);
    f.enter(800);
    CHECK_EQ(f.state.pendingDialogueContext->sourceId, 800);
    CHECK_FALSE(controller.handleDialogueCloseRequest(context).shouldOpenPendingEventDialog);
}

TEST_CASE("Hints branch is absent from save and NPC enter rebuilds old topic overrides")
{
    MergeQuestFixture f(2, true);
    f.flag("GotMainQuest", 1);
    f.enter();
    f.topic(732);
    CHECK_EQ(f.slot(0), 1781);
    GameSaveData save;
    save.party = f.party.snapshot();
    save.currentSceneKind = SceneKind::Outdoor;
    save.mapFileName = "7out02.odm";
    save.hasOutdoorRuntimeState = true;
    save.outdoorWorld.eventRuntimeState = f.state;
    save.namedGlobalVars = f.state.namedGlobalVars;
    const auto path = std::filesystem::path(std::getenv("TMPDIR")) / "dialogue138.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK_EQ(loaded->outdoorWorld.eventRuntimeState->dialogueState.scriptedBranch, "");
    MergeQuestFixture restored(2, true);
    restored.party.restoreSnapshot(loaded->party);
    restored.state = *loaded->outdoorWorld.eventRuntimeState;
    restored.state.namedGlobalVars = loaded->namedGlobalVars;
    restored.enter();
    CHECK_EQ(restored.slot(0), 1783);
    CHECK_EQ(restored.slot(1), 732);
    CHECK_EQ(restored.slot(2), 0);
    CHECK_EQ(restored.state.dialogueState.scriptedBranch, "");
    restored.flag("GotMainQuest", 0);
    restored.flag("MetVerdant", 1);
    restored.flag("GotIntro1", 1);
    restored.flag("GotIntro2", 1);
    restored.enter();
    CHECK_EQ(restored.flag("GotIntro1"), 0);
    CHECK_EQ(restored.flag("GotIntro2"), 0);
    CHECK_EQ(restored.slot(0), 0);
    CHECK_EQ(restored.slot(1), 1779);
}

TEST_CASE("Lua Speak dispatches NPC enter once when presented, while topic refresh never reenters")
{
    MergeQuestFixture f;
    std::string error;
    f.global = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + "\n"
        + std::string(OpenYAMM::Import::CrossContinentsIntroSource) + R"lua(
RegisterGlobalNpcEnterHook(60181, nil, function(c)
    if c.npcId == 803 then
        evt.SetGlobalVar("EnterCount",evt.GetGlobalVar("EnterCount",0)+1)
        evt.SimpleMessage("enter output")
        if evt.GetGlobalVar("Redirect",0) ~= 0 then evt.SpeakNPC(800) end
    end
end)
RegisterGlobalEvent(60182,nil,function() evt.GiveItem(2125) evt.SpeakNPC(803) end)
)lua", "@controlled-source-Speak-presentation", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(f.global.has_value(), error);
    f.flag("GotMainQuest", 1);
    f.flag("MetVerdant", 1);
    f.timer(60182);
    REQUIRE(f.state.pendingDialogueContext->npcEnterPending);
    GameplayUiController ui;
    EventDialogContent &active = ui.eventDialog().content;
    size_t selection = 0;
    GameplayDialogController::Context context{ui, f.state, active, selection};
    context.pWorldRuntime = &f;
    context.pParty = &f.party;
    context.pGlobalEventProgram = &f.global;
    context.pNpcDialogTable = &OpenYAMM::Tests::regressionGameData().npcDialogTable;
    GameplayDialogController controller;
    CHECK(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened);
    CHECK_EQ(f.state.namedGlobalVars.at("EnterCount"), 1);
    CHECK(f.party.ownsPendingItem(2125));
    CHECK_EQ(f.state.messages.back(), "enter output");
    CHECK_EQ(f.slot(0), 1782);
    f.topic(732);
    CHECK(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened);
    CHECK_EQ(f.state.namedGlobalVars.at("EnterCount"), 1);
    CHECK_EQ(f.state.dialogueState.scriptedBranch, "Hints");
    f.timer(60182);
    CHECK(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened);
    CHECK_EQ(f.state.namedGlobalVars.at("EnterCount"), 2);
    CHECK_EQ(f.state.dialogueState.scriptedBranch, "");
    controller.openNpcDialogue(context, 803);
    CHECK_EQ(f.state.namedGlobalVars.at("EnterCount"), 3);
    CHECK(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened);
    CHECK_EQ(f.state.namedGlobalVars.at("EnterCount"), 3);
    f.state.namedGlobalVars["Redirect"] = 1;
    f.timer(60182);
    CHECK_FALSE(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened);
    CHECK_EQ(f.state.pendingDialogueContext->sourceId, 800);
    CHECK(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened);
    CHECK_EQ(f.state.pendingDialogueContext->sourceId, 800);
    CHECK_EQ(f.state.namedGlobalVars.at("EnterCount"), 4);
}
