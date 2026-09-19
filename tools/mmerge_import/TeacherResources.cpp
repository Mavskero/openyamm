#include "TeacherResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/gameplay/MasteryTeacherDialog.h"
#include "game/party/Party.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/NpcDialogTable.h"

#include <charconv>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
const Row TopicHeader = {"TopicId", "Notes", "SkillId", "Mastery", "TextId", "Req. Gold", "Req. Skill"};
const Row NoteHeader = {"Topic Id", "Notes", "NPC Id", "Autonote Id", "NPC", "A", "NPC", "A"};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string trim(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \r\n\t");
    return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \r\n\t") - first + 1);
}

uint32_t number(const std::string &text, uint32_t maximum)
{
    const std::string value = trim(text);
    uint32_t parsedValue = 0;
    const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), parsedValue);
    require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && parsedValue <= maximum,
        "Invalid teacher numeric field: " + text);
    return parsedValue;
}

uint32_t optionalPositive(const std::string &text, uint32_t maximum)
{
    if (trim(text).empty())
    {
        return 0;
    }
    const uint32_t value = number(text, maximum);
    require(value > 0, "Explicit zero teacher override needs separate semantics");
    return value;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid teacher quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 == table->getRowCount() && row == Row{""})
        {
            continue;
        }
        rows.push_back(row);
    }
    return rows;
}

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Teacher input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 8 * 1024 * 1024, "Invalid teacher input size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read teacher input");
    if (legacy)
    {
        size_t glyphs = 0;
        text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
        require(glyphs == 0, "Unexpected localized teacher mechanics");
    }
    return parse(text);
}

void checkTrainingTransaction(const Game::NpcDialogTable &dialogs)
{
    using namespace Game;
    ClassSkillTable classes;
    require(classes.loadCapsFromRows({{"", "Knight"}, {"Staff", "G"}}), "Teacher fixture class failed");
    MergedTeacherTopicTable topics;
    require(topics.loadFromRows({TopicHeader, {"60000", "Staff Expert", "0", "1", "300", "", ""}}),
        "Teacher fixture topic failed");
    Character member;
    member.name = "训练验证";
    member.className = "Knight";
    member.health = member.maxHealth = 40;
    member.skills["Staff"] = {"Staff", 4, SkillMastery::Normal};
    PartySeed seed;
    seed.members = {member};
    seed.gold = 100000;
    Party party;
    party.seed(seed);
    const std::optional<MasteryTeacherEvaluation> evaluation =
        evaluateMasteryTeacherTopic(60000, party, classes, dialogs, &topics);
    require(evaluation && evaluation->approved && evaluation->cost > 0, "Blank teacher price became free");
    const int cost = evaluation->cost;
    std::string message;
    require(applyMasteryTeacherTopic(60000, party, classes, dialogs, &topics, message)
        && party.gold() == seed.gold - cost
        && party.activeMember()->findSkill("Staff")->mastery == SkillMastery::Expert,
        "Teacher payment or mastery update failed");
    require(!applyMasteryTeacherTopic(60000, party, classes, dialogs, &topics, message)
        && party.gold() == seed.gold - cost, "Already trained character was charged twice");
    seed.gold = 0;
    party.seed(seed);
    require(!applyMasteryTeacherTopic(60000, party, classes, dialogs, &topics, message)
        && party.gold() == 0 && party.activeMember()->findSkill("Staff")->mastery == SkillMastery::Normal,
        "Insufficient gold still trained a character");
    seed.gold = 100000;
    seed.members[0].skills["Staff"].level = 3;
    party.seed(seed);
    require(!applyMasteryTeacherTopic(60000, party, classes, dialogs, &topics, message)
        && party.gold() == seed.gold, "Insufficient skill still charged gold");
}
}

void convertTeacherResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Teacher output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Teacher output already exists");
    const Rows topics = readRows(root, "Data/Tables/Teacher topics.txt", true);
    const Rows notes = readRows(root, "Data/Tables/Teacher autonotes.txt", true);
    require(topics.size() > 1 && topics[0] == TopicHeader && notes.size() > 1 && notes[0] == NoteHeader,
        "Unsupported teacher headers");
    const auto assetRows = [&assets](const std::string &name)
    {
        return readRows(assets, "engine/data_tables/" + name, false);
    };
    Game::NpcDialogTable dialogs;
    require(dialogs.loadNpcRows(assetRows("npc.txt")) && dialogs.loadTopicsFromRows(assetRows("npc_topic.txt"))
        && dialogs.loadTextsFromRows(assetRows("npc_topic_text.txt")), "Invalid teacher NPC dependency");
    Game::JournalAutonoteTable journal;
    require(journal.loadFromRows(assetRows("english/autonote.txt")), "Invalid teacher journal dependency");
    std::set<uint32_t> noteIds;
    for (const Game::JournalAutonoteEntry &entry : journal.entries())
    {
        noteIds.insert(entry.noteBit >> 16);
    }
    Rows completeTopics = {{"#TeacherTopicMode", "Complete"}};
    completeTopics.insert(completeTopics.end(), topics.begin(), topics.end());
    const std::string topicText = serializeTextRows(completeTopics);
    const std::string noteText = serializeTextRows(notes);
    require(parse(topicText) == completeTopics && parse(noteText) == notes,
        "Teacher serialization changed source cells");
    Game::MergedTeacherTopicTable teacherTable;
    Game::MergedTeacherAutonoteTable noteTable;
    require(teacherTable.loadFromRows(parse(topicText)) && noteTable.loadFromRows(parse(noteText)),
        "Teacher typed loading failed");
    require(teacherTable.hasCompleteDefinitions(), "Teacher source authority was lost");
    std::set<uint32_t> topicIds;
    size_t priceOverrides = 0;
    size_t skillOverrides = 0;
    const std::vector<std::string> skillNames = Game::allCanonicalSkillNames();
    for (size_t index = 1; index < topics.size(); ++index)
    {
        const Row &row = topics[index];
        require(row.size() == 7, "Invalid teacher topic width");
        const uint32_t id = number(row[0], 65535);
        require(id > 0 && topicIds.insert(id).second, "Invalid/duplicate teacher topic ID");
        const uint32_t skill = number(row[2], 38);
        const uint32_t mastery = number(row[3], 3);
        const uint32_t textId = number(row[4], 65535);
        const uint32_t gold = optionalPositive(row[5], 0x7FFFFFFFu);
        const uint32_t level = optionalPositive(row[6], 65535);
        require(mastery > 0 && textId > 0, "Invalid teacher mastery or text ID");
        require(dialogs.getTopicById(id).has_value() && dialogs.getText(textId).has_value(),
            "Teacher topic or localized text is missing");
        const Game::MergedTeacherTopicEntry *pEntry = teacherTable.get(id);
        const std::optional<Game::MasteryTeacherTopicDefinition> definition =
            Game::resolveMasteryTeacherTopic(id, &teacherTable);
        require(pEntry && pEntry->note == trim(row[1]) && definition && definition->skillId == skill
            && definition->skillName == skillNames.at(skill) && definition->masteryRank == mastery
            && static_cast<uint32_t>(definition->targetMastery) == mastery + 1
            && definition->textId == textId && definition->requiredGold == gold && definition->requiredSkill == level,
            "Teacher topic semantics changed");
        priceOverrides += gold > 0;
        skillOverrides += level > 0;
    }
    require(teacherTable.entries().size() == topicIds.size(), "Teacher loader lost topics");
    size_t emptyPairs = 0;
    size_t inactiveBindings = 0;
    std::set<uint32_t> inactiveTopics;
    std::set<uint32_t> noteTopics;
    std::set<std::pair<uint32_t, uint32_t>> bindings;
    for (size_t index = 1; index < notes.size(); ++index)
    {
        const Row &row = notes[index];
        require(row.size() >= 4 && row.size() % 2 == 0, "Invalid teacher note pair width");
        const uint32_t topic = number(row[0], 65535);
        require(dialogs.getTopicById(topic).has_value() && noteTopics.insert(topic).second,
            "Invalid/duplicate teacher note topic");
        if (!topicIds.contains(topic))
        {
            require(!Game::resolveMasteryTeacherTopic(topic, &teacherTable), "Removed teacher topic was reactivated");
            inactiveTopics.insert(topic);
        }
        for (size_t column = 2; column < row.size(); column += 2)
        {
            if (trim(row[column]).empty() && trim(row[column + 1]).empty())
            {
                ++emptyPairs;
                continue;
            }
            const uint32_t npc = number(row[column], 65535);
            const uint32_t note = number(row[column + 1], 65535);
            require(npc > 0 && note > 0 && dialogs.getNpc(npc) && noteIds.contains(note),
                "Teacher note NPC or journal entry is missing");
            require(bindings.emplace(topic, npc).second, "Duplicate teacher NPC binding");
            require(noteTable.autonoteIdForTopicAndNpc(topic, npc) == note, "Teacher note identity changed");
            inactiveBindings += inactiveTopics.contains(topic);
        }
    }
    require(noteTable.mappingCount() == bindings.size(), "Teacher loader lost note bindings");
    checkTrainingTransaction(dialogs);
    Game::MergedTeacherTopicTable extension;
    require(extension.loadFromRows({TopicHeader, {"60000", "Extension", "0", "1", "300", "", ""}})
        && Game::resolveMasteryTeacherTopic(321, &extension).has_value()
        && Game::resolveMasteryTeacherTopic(321, nullptr).has_value(), "Legacy teacher fallback changed");
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"teachers\",\"runtime_ready\":false,\"topics\":" << topicIds.size()
        << ",\"note_bindings\":" << bindings.size() << ",\"empty_note_pairs\":" << emptyPairs
        << ",\"price_overrides\":" << priceOverrides << ",\"skill_overrides\":" << skillOverrides
        << ",\"inactive_note_bindings\":" << inactiveBindings << ",\"inactive_note_topic_ids\":[";
    for (uint32_t topic : inactiveTopics)
    {
        report << (topic == *inactiveTopics.begin() ? "" : ",") << topic;
    }
    report << "],\"complete_definitions\":true,\"typed_validation\":true,\"training_transaction_check\":true"
        << ",\"original_price_parity_validated\":false,\"rendered_training_validated\":false"
        << ",\"teacher_note_event_validated\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh teacher output");
    const auto write = [&output](const std::string &name, const std::string &text)
    {
        writeConvertedResource(output / name, std::vector<uint8_t>(text.begin(), text.end()));
    };
    try
    {
        write("engine/data_tables/teacher_topics.txt", topicText);
        write("engine/data_tables/teacher_autonotes.txt", noteText);
        write("teacher_conversion.json", report.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
