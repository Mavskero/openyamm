#include "AppleStoryCheck.h"
#include "engine/AssetFileSystem.h"
#include "engine/TextTable.h"
#include "game/gameplay/JournalQuestRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/tables/JournalQuestTable.h"
#include "game/tables/NpcDialogTable.h"
#include "game/tables/RosterTable.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void checkLocalizedRecruitment()
{
    using namespace OpenYAMM::Game;
    std::vector<std::string> rosterRow(22, "0");
    rosterRow[0] = "1";
    rosterRow[1] = "Recruit";
    rosterRow[2] = "Knight";
    RosterTable roster;
    require(roster.loadFromRows({rosterRow}), "Recruitment fixture did not load");
    NpcDialogTable topics;
    require(topics.loadTextsFromRows({{"200", "一起冒险吧。"}, {"201", "队伍已满。"}}), "Missing invite texts");
    for (const std::string &label : {std::string("Roster Join Event"), std::string("Join")})
    {
        require(topics.loadTopicsFromRows({
            {"#", "Topic", "Requires", "Notes", "Text #", "Owner(s)", "Owner #", "Base Topic"},
            {"601", "加入队伍", "0", "", "200", "Recruit", "", label}}), "Localized topics did not load");
        topics.resolveSpecialTopics(roster);
        const std::optional<NpcDialogTable::RosterJoinOffer> offer = topics.getRosterJoinOfferForTopic(601);
        const std::optional<NpcDialogTable::ResolvedTopic> topic = topics.getTopicById(601);
        require(offer && offer->rosterId == 1 && topic && topic->topic == "加入队伍",
            "Localization changed roster recruitment behavior or display text");
    }
    require(topics.loadTopicsFromRows({{"601", "Roster Join Event", "0", "", "200", "Recruit", ""}}),
        "Legacy topics did not load");
    topics.resolveSpecialTopics(roster);
    require(topics.getRosterJoinOfferForTopic(601).has_value()
        && topics.getTopicById(601)->topic == "Join", "Legacy English recruitment changed");
    require(topics.loadTopicsFromRows({
        {"#", "Topic", "Requires", "Notes", "Text #", "Owner(s)", "Owner #", "Base Topic"},
        {"601", "Join", "0", "", "200", "Recruit", "", "Information"}}), "Non-join topic did not load");
    topics.resolveSpecialTopics(roster);
    require(!topics.getRosterJoinOfferForTopic(601), "Display text incorrectly created a recruitment action");
}
}

int runAppleStoryCheck(const std::filesystem::path &assets, const std::filesystem::path &newCheckDirectory)
{
    using namespace OpenYAMM;
    try
    {
        checkLocalizedRecruitment();
        Engine::AssetFileSystem files;
        require(files.initialize(assets, assets, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount story assets");
        const auto readRows = [&](const char *pName)
        {
            const std::optional<std::string> text = files.readTextFile(std::string("engine/data_tables/") + pName);
            const std::optional<Engine::TextTable> table = text
                ? Engine::TextTable::parseTabSeparated(*text) : std::nullopt;
            require(table.has_value(), std::string("Cannot read exported table: ") + pName);
            std::vector<std::vector<std::string>> rows;
            for (size_t i = 0; i < table->getRowCount(); ++i)
            {
                rows.push_back(table->getRow(i));
            }
            return rows;
        };
        Game::NpcDialogTable npcs;
        Game::JournalQuestTable quests;
        require(npcs.loadNpcRows(readRows("npc.txt")) && npcs.loadGreetingsFromRows(readRows("npc_greet.txt"))
            && npcs.loadTextsFromRows(readRows("npc_topic_text.txt"))
            && npcs.loadTopicsFromRows(readRows("npc_topic.txt"))
            && npcs.loadNewsFromRows(readRows("npc_news.txt"))
            && npcs.loadGroupNewsFromRows(readRows("english/npc_group.txt"))
            && quests.loadFromRows(readRows("english/quests.txt")), "Exported story tables failed to reload");
        require(!quests.entries().empty(), "No quest text loaded");
        const Game::JournalQuestEntry &quest = quests.entries().front();
        Game::Party party;
        party.seed(Game::Party::createDefaultSeed());
        party.setJournalQuestTable(&quests);
        party.setQuestBit(quest.qbitId, true);
        const std::vector<std::string> before = Game::buildCurrentQuestTexts(quests, party);
        require(!before.empty(), "Quest state did not expose journal text");
        Game::HiredNpcFollower follower;
        follower.npcId = npcs.entries().front().id;
        follower.name = npcs.entries().front().name;
        party.addHiredNpcFollower(follower);
        Game::GameSaveData save;
        save.mapFileName = "mm6:oute3.odm";
        save.saveName = "中文任务存档检查";
        save.party = party.snapshot();
        require(!std::filesystem::exists(newCheckDirectory)
            && std::filesystem::create_directories(newCheckDirectory), "Check directory must be new");
        const std::filesystem::path path = newCheckDirectory / "story-state-check.oysav";
        std::string error;
        const bool saved = Game::saveGameDataToPath(path, save, error);
        require(saved, "Saving failed: " + error);
        const std::optional<Game::GameSaveData> restored = Game::loadGameDataFromPath(path, error);
        require(restored.has_value(), "Reloading failed: " + error);
        Game::Party restoredParty;
        restoredParty.setJournalQuestTable(&quests);
        restoredParty.restoreSnapshot(restored->party);
        require(restored->saveName == save.saveName && restored->mapFileName == save.mapFileName
            && restoredParty.hasQuestBit(quest.qbitId) && Game::buildCurrentQuestTexts(quests, restoredParty) == before
            && restored->party.hiredNpcFollowers.size() == 1
            && restored->party.hiredNpcFollowers.front().name == follower.name,
            "Save/reload changed quest state, Chinese text or NPC identity");
        std::ofstream report(newCheckDirectory / "story-check.json");
        report << "{\"table_reload\":true,\"localized_recruitment\":true,\"legacy_recruitment\":true,"
            << "\"save_state_roundtrip\":true,\"full_gameplay_loop\":false,\"npcs\":" << npcs.entries().size()
            << ",\"quest_texts\":" << quests.entries().size() << ",\"checked_qbit\":" << quest.qbitId << "}\n";
        report.close();
        require(report.good(), "Could not write story check report");
        std::cout << "story_check npcs=" << npcs.entries().size() << " quest_texts=" << quests.entries().size()
            << " localization_recruitment=1 save_state_roundtrip=1 full_gameplay_loop=0" << std::endl;
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Story check failed: " << error.what() << std::endl;
        return 1;
    }
}
