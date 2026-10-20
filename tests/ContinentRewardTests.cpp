#include "tests/MergeCrossContinentsHarness.h"
#include "game/events/EventDialogContent.h"
#include "game/maps/SaveGame.h"
#include <algorithm>
#include <filesystem>

using namespace OpenYAMM::Game;
using OpenYAMM::Tests::MergeQuestFixture;
namespace
{
constexpr uint32_t Bits[] = {228,783,784};
struct RewardFixture : MergeQuestFixture
{
    using MergeQuestFixture::MergeQuestFixture;
    bool flying = false;
    bool partyIsFlyingForEventChecks() const override { return flying; }
};
std::string compact(std::string text)
{
    std::erase_if(text, [](unsigned char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; });
    return text;
}
std::string joined(const EventDialogContent &dialog)
{
    std::string text; for (const auto &line : dialog.lines) text += line;
    return compact(text);
}
InventoryItem item(uint32_t id) { InventoryItem v; v.objectDescriptionId = id; return v; }
GameSaveData saveOf(const MergeQuestFixture &f)
{
    GameSaveData save; save.saveName = "reward140-controlled"; save.mapFileName = "out03.odm";
    save.party = f.party.snapshot(); save.namedGlobalVars = f.state.namedGlobalVars; save.savedGameMinutes = f.minutes;
    return save;
}
std::filesystem::path path(const std::string &name)
{ return std::filesystem::path(std::getenv("TMPDIR")) / name; }
void restore(MergeQuestFixture &f,const GameSaveData &save)
{ f.party.restoreSnapshot(save.party); f.state.namedGlobalVars = save.namedGlobalVars; f.minutes = save.savedGameMinutes; }
EventDialogContent content(MergeQuestFixture &f)
{
    const auto &data = OpenYAMM::Tests::regressionGameData();
    auto dialog = buildEventDialogContent(f.state,0,true,&f.global,&data.houseTable,&data.classSkillTable,
        &data.npcDialogTable,&data.transitionTable,nullptr,nullptr,&f.party,&f,float(f.minutes));
    if (dialog.displayedNpcGreetingCount.has_value())
        f.party.recordNpcGreetingDisplay(dialog.sourceId,*dialog.displayedNpcGreetingCount);
    return dialog;
}
}

TEST_CASE("six source continent reward orders award two identified mouse owners and upgrade third exactly once")
{
    std::array<uint32_t,3> order{1,2,3}; int sequence = 0;
    do
    {
        RewardFixture f; // Intentionally no Main/Met/Stone prerequisite: source reward has none.
        int stage = 0;
        for (uint32_t continent : order)
        {
            f.state.activeHistoryContinentId = continent;
            f.party.setQuestBit(Bits[continent-1],true); f.after(); f.clearUiRequests();
            CHECK_EQ(f.party.snapshot().npcGreetingOverrides.at(803),328-continent);
            f.enter(67); CHECK_EQ(f.flag("GotReward."+std::to_string(continent)),0);
            f.state.normalGameplayScreen = false; f.red = true; f.flying = true;
            f.enter(); CHECK_EQ(f.flag("GotReward."+std::to_string(continent)),1);
            const size_t rings = size_t(std::min(stage+1,2));
            REQUIRE_EQ(f.party.pendingItemDeliveries().size(),rings);
            for (const auto &owner : f.party.pendingItemDeliveries())
            { CHECK_EQ(owner.item.objectDescriptionId,543); CHECK(owner.item.identified); CHECK_EQ(owner.recipientMemberIndex,-1); }
            CHECK_EQ(f.flag("ImporvedConnector"),stage==2);
            CHECK_EQ(f.flag("AllStoriesFinished"),stage==2); CHECK_EQ(f.flag("ShowInterlude"),stage==2);
            CHECK_EQ(f.flag("GotFinalQuest"),0); CHECK_EQ(f.flag("QuestFinished"),0);
            if (stage==2)
            {
                CHECK_EQ(f.ticks("MeetTime"),int64_t(f.minutes*256)+80640*256LL);
                CHECK_EQ(f.ticks("FQCatchTime"),0); // Initial third reward does not execute AfterLoad inline.
                CHECK_EQ(f.state.npcPrimaryGreetingTextOverrides.at(803),
                    OpenYAMM::Tests::regressionGameData().npcDialogTable.getGreeting(328-continent)->greetingPrimary
                    + " " + *OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2161));
            }
            f.enter(); CHECK_EQ(f.party.pendingItemDeliveries().size(),rings);
            std::string error; const auto save=saveOf(f);
            const auto filename=path("order-"+std::to_string(sequence)+"-"+std::to_string(stage)+".oysav");
            REQUIRE_MESSAGE(saveGameDataToPath(filename,save,error),error);
            const auto loaded=loadGameDataFromPath(filename,error); REQUIRE_MESSAGE(loaded.has_value(),error);
            RewardFixture cold(continent); restore(cold,*loaded); cold.after(); cold.enter();
            CHECK_EQ(cold.party.pendingItemDeliveries().size(),rings);
            CHECK_EQ(cold.flag("GotReward."+std::to_string(continent)),1);
            CHECK(cold.state.npcPrimaryGreetingTextOverrides.empty());
            ++stage;
        }
        ++sequence;
    } while (std::next_permutation(order.begin(),order.end()));
    CHECK_EQ(sequence,6);
}

TEST_CASE("reward completion sync and arena switch4 gates; direct803 enters alone commit after owned capacity")
{
    for (int gate : {0,1,2})
    {
        RewardFixture f; f.party.setQuestBit(228,gate!=0);
        if (gate==1) f.setMapName("d42.blv");
        if (gate==2) f.state.dimensionDoorActive=true;
        f.after(); f.enter(); CHECK_EQ(f.flag("GotReward.1"),0);
        CHECK_EQ(f.flag("ContinentFinished.1"),gate!=0);
    }
    RewardFixture f;
    for (size_t n=0;n<Party::MaxPendingItemDeliveries;++n) REQUIRE(f.party.grantOwnedQuestItem(item(624),-1));
    f.party.setQuestBit(228,true); f.after(); f.enter();
    CHECK_EQ(f.flag("GotReward.1"),0); CHECK_EQ(f.party.pendingItemDeliveries().size(),128);
    f.party.completeFirstPendingItemDelivery(); f.enter();
    CHECK_EQ(f.flag("GotReward.1"),1); REQUIRE_EQ(f.party.pendingItemDeliveries().size(),128);
    CHECK_EQ(f.party.pendingItemDeliveries().back().item.objectDescriptionId,543);
    f.enter(); CHECK_EQ(f.party.pendingItemDeliveries().size(),128);
}

TEST_CASE("reward automated catch defers unsafe screen enemies or flying but ordinary803 enter is ungated")
{
    for(int gate=0;gate<4;++gate)
    {
        RewardFixture f; f.party.setQuestBit(228,true); f.after(); f.clearUiRequests();
        f.state.normalGameplayScreen=gate!=0; f.red=gate==1; f.yellow=gate==2; f.flying=gate==3;
        f.minutes+=10; f.timer(65515); CHECK_FALSE(f.state.pendingDialogueContext.has_value());
        CHECK_EQ(f.flag("GotReward.1"),0);
        f.state.normalGameplayScreen=true;f.red=false;f.yellow=false;f.flying=false;
        f.timer(65515); REQUIRE(f.state.pendingDialogueContext.has_value()); CHECK(f.state.pendingDialogueContext->npcEnterPending);
        CHECK_EQ(f.flag("GotReward.1"),0); f.enter(); CHECK_EQ(f.flag("GotReward.1"),1);
        f.clearUiRequests();f.timer(65515);CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    }
}

TEST_CASE("third upgrade primary is803 local map-session only; secondary otherNPC leave and cold preserve original")
{
    for (uint32_t continent : {1,2,3})
    {
        RewardFixture f(continent); f.party.setQuestBit(Bits[continent-1],true);
        for(uint32_t c=1;c<=3;++c) if(c!=continent)f.flag("GotReward."+std::to_string(c),1);
        f.after();f.enter(); const auto expected=f.state.npcPrimaryGreetingTextOverrides.at(803);
        CHECK_EQ(expected,OpenYAMM::Tests::regressionGameData().npcDialogTable.getGreeting(328-continent)->greetingPrimary
            +" "+*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2161));
        const auto first=content(f); CHECK(first.isActive);CHECK_EQ(first.sourceId,803);CHECK_FALSE(first.lines.empty());
        CHECK_NE(joined(first).find(compact(expected)),std::string::npos);
        f.enter(); CHECK_EQ(f.state.npcPrimaryGreetingTextOverrides.at(803),expected);
        const auto second=content(f);CHECK(second.isActive);
        const auto *originalGreeting=OpenYAMM::Tests::regressionGameData().npcDialogTable.getGreeting(328-continent);
        if (!originalGreeting->greetingSecondary.empty())
        {
            CHECK_NE(joined(second).find(compact(originalGreeting->greetingSecondary)),std::string::npos);
            CHECK_EQ(joined(second).find(compact(*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2161))),std::string::npos);
        }
        f.party.setNpcGreetingOverride(67,328-continent); f.enter(67); const auto other=content(f);
        CHECK_EQ(other.sourceId,67);
        CHECK_NE(joined(other).find(compact(originalGreeting->greetingPrimary)),std::string::npos);
        CHECK_EQ(joined(other).find(compact(*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2161))),std::string::npos);
        f.enter();
        const auto *greeting=OpenYAMM::Tests::regressionGameData().npcDialogTable.getGreeting(328-continent);
        CHECK_FALSE(greeting->greetingPrimary.ends_with(*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2161)));
        CHECK_FALSE(greeting->greetingSecondary.ends_with(*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2161)));
        CHECK_EQ(f.state.npcPrimaryGreetingTextOverrides.size(),1);
        f.topic(732);CHECK_EQ(f.state.npcPrimaryGreetingTextOverrides.at(803),expected);
        f.leave();CHECK(f.state.npcPrimaryGreetingTextOverrides.empty());
    }
}

TEST_CASE("final week initialization is later AfterLoad than third reward and >= starts ten-minute safe catch even arena")
{
    for (uint32_t continent : {1,2,3}) for (int gate=0;gate<4;++gate)
    {
        RewardFixture f(continent);f.flag("AllStoriesFinished",1);f.flag("ShowInterlude",1);
        f.ticks("MeetTime",int64_t(f.minutes*256)+80640*256LL);f.setMapName("d42.blv");
        f.after();CHECK_EQ(f.flag("HideVerdant"),1);CHECK_EQ(f.party.snapshot().npcHouseOverrides.at(803),0);
        const int64_t deadline=f.ticks("FQCatchTime");CHECK_EQ(deadline,int64_t(f.minutes*256)+10080*256LL);
        CHECK_NE(deadline,f.ticks("MeetTime"));
        f.minutes=double(deadline-1)/256;f.after();f.clearUiRequests();f.timer(65515);CHECK_EQ(f.flag("GotFinalQuest"),0);
        f.minutes=double(deadline)/256;f.after();f.clearUiRequests();
        std::vector<ScriptedEventTimerDefinition> definitions;
        for(const auto &definition:f.global->timerTriggers())if(definition.eventId==65515)definitions.push_back(definition);
        auto timers=reconcileScriptedEventTimers({},definitions,f.minutes,0,false);
        auto callback=[&](const auto &definition){f.timer(definition.eventId);return true;};
        CHECK_FALSE(updateScriptedEventTimers(timers,f.minutes+9.999,callback));CHECK_EQ(f.flag("GotFinalQuest"),0);
        f.state.normalGameplayScreen=gate!=0;f.red=gate==1;f.yellow=gate==2;f.flying=gate==3;
        f.minutes+=10;updateScriptedEventTimers(timers,f.minutes,callback);CHECK_EQ(f.flag("GotFinalQuest"),0);
        f.state.normalGameplayScreen=true;f.red=false;f.yellow=false;f.flying=false;
        f.minutes+=10;updateScriptedEventTimers(timers,f.minutes,callback);
        CHECK_EQ(f.flag("GotFinalQuest"),1);CHECK_EQ(f.flag("ShowInterlude"),0);CHECK(f.party.hasQuestBit(1713));
        CHECK_EQ(f.party.snapshot().npcGreetingOverrides.at(803),324);CHECK_EQ(f.flag("QuestFinished"),0);
        f.clearUiRequests();f.timer(65515);CHECK_FALSE(f.state.pendingDialogueContext.has_value());
    }
}

TEST_CASE("interlude first instructions and repeat instructions use original source topics and never complete quest")
{
    RewardFixture f;f.flag("GotMainQuest",1);f.flag("GotConnectorStone",1);f.flag("AllStoriesFinished",1);f.flag("ShowInterlude",1);
    f.enter();CHECK_EQ(f.slot(0),1787);CHECK(f.visible(1787));CHECK_FALSE(f.visible(1785));f.topic(1787);
    REQUIRE_EQ(f.state.messages.size(),1);CHECK_EQ(f.state.messages[0],*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2159));
    CHECK_EQ(f.flag("GotInstructions"),0);f.flag("GotFinalQuest",1);f.flag("ShowInterlude",0);f.enter();
    CHECK_EQ(f.slot(0),1785);CHECK(f.visible(1785));CHECK_FALSE(f.visible(1786));f.topic(1785);
    CHECK_EQ(f.flag("GotInstructions"),1);CHECK_EQ(f.slot(0),1786);CHECK_FALSE(f.party.hasQuestBit(1713));
    CHECK_EQ(f.state.messages[0],*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2157));
    CHECK_FALSE(f.visible(1785));CHECK(f.visible(1786));f.topic(1786);CHECK(f.party.hasQuestBit(1713));
    CHECK_EQ(f.state.messages[0],*OpenYAMM::Tests::regressionGameData().npcDialogTable.getText(2158));
    CHECK_EQ(f.flag("QuestFinished"),0);f.flag("QuestFinished",1);f.enter();CHECK_EQ(f.slot(0),0);
    CHECK_FALSE(f.visible(1785));CHECK_FALSE(f.visible(1786));CHECK_FALSE(f.visible(1787));
}

TEST_CASE("Party greeting receipt survives map cache cold restore and explicit greeting reset")
{
    RewardFixture first(3);first.flag("GotMainQuest",1);first.flag("MetVerdant",1);first.flag("GotFinalQuest",1);
    first.party.setQuestBit(1104,true);first.party.setQuestBit(1105,true);
    first.party.setNpcGreetingOverride(803,325);first.enter();const auto primary=content(first);
    REQUIRE(primary.displayedNpcGreetingCount.has_value());CHECK_EQ(*primary.displayedNpcGreetingCount,1);
    auto snapshot=saveOf(first);
    RewardFixture another(2);restore(another,snapshot);another.state.npcGreetingDisplayCounts[803]=0;
    another.enter();const auto secondary=content(another);
    const auto *greeting=OpenYAMM::Tests::regressionGameData().npcDialogTable.getGreeting(325);
    REQUIRE_FALSE(greeting->greetingSecondary.empty());
    CHECK_NE(joined(secondary).find(compact(greeting->greetingSecondary)),std::string::npos);
    CHECK_EQ(*secondary.displayedNpcGreetingCount,2);
    std::string error;REQUIRE(saveGameDataToPath(path("greeting-count140.oysav"),saveOf(another),error));
    auto loaded=loadGameDataFromPath(path("greeting-count140.oysav"),error);REQUIRE(loaded.has_value());
    RewardFixture cold(3);restore(cold,*loaded);cold.after();cold.enter();const auto again=content(cold);
    CHECK_EQ(*again.displayedNpcGreetingCount,3);
    CHECK_NE(joined(again).find(compact(greeting->greetingSecondary)),std::string::npos);
    cold.party.setNpcGreetingOverride(803,325);cold.enter();const auto reset=content(cold);
    CHECK_EQ(*reset.displayedNpcGreetingCount,1);
    CHECK_NE(joined(reset).find(compact(greeting->greetingPrimary)),std::string::npos);
}
