#include "StoryResources.h"
#include "BlasterLearningResources.h"
#include "CrossContinentsResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/JournalQuestTable.h"
#include "game/tables/NpcDialogTable.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Rows = std::vector<std::vector<std::string>>;

struct TableSpec
{
    const char *pSource;
    const char *pOutput;
    size_t minimumColumns;
    std::vector<size_t> displayColumns;
};

const std::vector<TableSpec> StoryTables = {
    {"npcdata.txt", "npc.txt", 16, {1}},
    {"npcgreet.txt", "npc_greet.txt", 5, {1, 2}},
    {"npctext.txt", "npc_topic_text.txt", 2, {1}},
    {"npctopic.txt", "npc_topic.txt", 6, {1}},
    {"npcnews.txt", "npc_news.txt", 2, {1, 2}},
    {"npcgroup.txt", "english/npc_group.txt", 2, {}},
    {"quests.txt", "english/quests.txt", 2, {1}},
};

struct CreditsText
{
    std::string text;
    size_t sourceLines = 0;
    size_t outputLines = 0;
    size_t nonemptyLines = 0;
    size_t headingMarkers = 0;
    size_t layoutControls = 0;
};

CreditsText normalizeCreditsText(const std::string &decoded)
{
    CreditsText credits;
    std::istringstream input(decoded);
    std::string sourceLine;
    bool pendingBlankLine = false;
    while (std::getline(input, sourceLine))
    {
        ++credits.sourceLines;
        std::string line;
        for (unsigned char character : sourceLine)
        {
            if (character == '\t')
            {
                line += ' ';
            }
            else if (character >= 32 && character != 127)
            {
                line += static_cast<char>(character);
            }
            else if (character != '\r')
            {
                ++credits.layoutControls;
            }
        }
        const size_t first = line.find_first_not_of(' ');
        line = first == std::string::npos
            ? "" : line.substr(first, line.find_last_not_of(' ') - first + 1);
        if (!line.empty() && line.front() == '_')
        {
            line.erase(0, 1);
            ++credits.headingMarkers;
        }
        if (line.empty())
        {
            pendingBlankLine = !credits.text.empty();
            continue;
        }
        if (pendingBlankLine)
        {
            credits.text += '\n';
            ++credits.outputLines;
            pendingBlankLine = false;
        }
        credits.text += line + '\n';
        ++credits.outputLines;
        ++credits.nonemptyLines;
    }
    if (credits.nonemptyLines == 0)
    {
        throw std::runtime_error("Original credits text is empty");
    }
    return credits;
}

std::optional<uint32_t> rowId(const std::vector<std::string> &row)
{
    if (row.empty() || row[0].empty())
    {
        return std::nullopt;
    }
    uint32_t id = 0;
    const std::string &cell = row[0];
    const std::from_chars_result result = std::from_chars(cell.data(), cell.data() + cell.size(), id);
    return result.ec == std::errc() && result.ptr == cell.data() + cell.size()
        ? std::optional<uint32_t>(id) : std::nullopt;
}


Rows readTable(const std::filesystem::path &archive, const TableSpec &spec, bool chinese, size_t &glyphs,
    std::vector<uint32_t> &emptyTopicIds, std::vector<uint32_t> &weekdayNewsIds)
{
    const std::optional<Engine::TextTable> parsed =
        Engine::TextTable::parseTabSeparated(decodeLegacyText(readLodText(archive, spec.pSource), chinese, glyphs));
    if (!parsed)
    {
        throw std::runtime_error(std::string("Invalid quoted story table: ") + spec.pSource);
    }
    Rows rows;
    std::map<uint32_t, bool> ids;
    for (size_t index = 0; index < parsed->getRowCount(); ++index)
    {
        std::vector<std::string> row = parsed->getRow(index);
        if (std::string_view(spec.pSource) == "npcnews.txt" && !row.empty())
        {
            const size_t separator = row[0].find(' ');
            if (separator != std::string::npos)
            {
                const std::string suffix = row[0].substr(separator + 1);
                const std::vector<std::string> prefix = {row[0].substr(0, separator)};
                const std::optional<uint32_t> numericId = rowId(prefix);
                if (numericId && (suffix == "M" || suffix == "T" || suffix == "W" || suffix == "F"
                    || suffix == "St" || suffix == "Sn"))
                {
                    row[0] = prefix[0];
                    weekdayNewsIds.push_back(*numericId);
                }
            }
        }
        const std::optional<uint32_t> id = rowId(row);
        if (!id)
        {
            if (row.size() == 1 && row[0].empty() && !rows.empty())
            {
                const size_t lastColumn = rows.back().size() - 1;
                if (std::find(spec.displayColumns.begin(), spec.displayColumns.end(), lastColumn)
                    != spec.displayColumns.end())
                {
                    rows.back().back() += '\n';
                }
            }
            if (row.empty() || (row.size() == 1 && row[0].empty())
                || row[0].starts_with('#') || row[0] == "Q Bit" || row[0] == "NPC Data (Special)"
                || row[0] == "News # " || row[0] == "Group #")
            {
                continue;
            }
            // MMerge T archives also contain unquoted multiline cells. As in
            // the runtime greeting loader, the first continuation cell finishes
            // the preceding last cell; following tabs start subsequent fields.
            if (rows.empty() || (std::string_view(spec.pSource) != "npcgreet.txt"
                && std::string_view(spec.pSource) != "npctext.txt"
                && std::string_view(spec.pSource) != "npctopic.txt"
                && std::string_view(spec.pSource) != "npcnews.txt"))
            {
                throw std::runtime_error(std::string("Unrecognized story row in ") + spec.pSource
                    + " at " + std::to_string(index + 1));
            }
            std::vector<std::string> &previous = rows.back();
            previous.back() += '\n';
            previous.back() += row.front();
            previous.insert(previous.end(), row.begin() + 1, row.end());
            continue;
        }
        if (!ids.emplace(*id, true).second)
        {
            throw std::runtime_error(std::string("Invalid or duplicate story ID in ") + spec.pSource
                + ": " + std::to_string(*id));
        }
        rows.push_back(row);
    }
    std::erase_if(rows, [&](const std::vector<std::string> &row)
    {
        if (std::string_view(spec.pSource) == "npctopic.txt" && row.size() == 1)
        {
            // An ID-only row is an unused topic slot, also skipped by the game
            // loader. Record it explicitly instead of inventing a topic body.
            emptyTopicIds.push_back(*rowId(row));
            return true;
        }
        return false;
    });
    for (const std::vector<std::string> &row : rows)
    {
        if (row.size() < spec.minimumColumns)
        {
            throw std::runtime_error(std::string("Incomplete story row in ") + spec.pSource + ": " + row[0]);
        }
    }
    if (rows.empty())
    {
        throw std::runtime_error(std::string("Empty story table: ") + spec.pSource);
    }
    return rows;
}


void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertStoryResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path sourceRoot = std::filesystem::canonical(source);
    const std::filesystem::path outputRoot = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = outputRoot.lexically_relative(sourceRoot);
    if (relative.empty() || (!relative.is_absolute() && *relative.begin() != ".."))
    {
        throw std::runtime_error("Story output must be outside the original source");
    }
    if (std::filesystem::exists(destination))
    {
        throw std::runtime_error("Story destination already exists");
    }
    const std::filesystem::path base = sourceRoot / "Data/mmmerge.T.lod";
    const std::filesystem::path localized = sourceRoot / "Data/10 LocZHCN.EnglishT.lod";
    const bool chinese = std::filesystem::exists(localized);
    const std::filesystem::path creditsArchive = chinese ? localized : base;
    size_t creditsGlyphs = 0;
    const CreditsText credits = normalizeCreditsText(
        decodeLegacyText(readLodText(creditsArchive, "Credits.txt"), chinese, creditsGlyphs));
    const std::optional<CrossContinentsIntroProfile> crossContinents = loadCrossContinentsIntroProfile(sourceRoot);
    const std::optional<BlasterLearningProfile> blasterLearning = loadBlasterLearningProfile(sourceRoot, chinese);
    std::map<std::string, Rows> tables;
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-story-tables\",\"runtime_ready\":false,"
        << "\"base_archive\":\"mmmerge.T.lod\",\"language\":\"" << (chinese ? "zh-CN" : "en")
        << "\",\"localization_archive\":" << (chinese ? "\"10 LocZHCN.EnglishT.lod\"" : "null")
        << ",\"tables\":[\n";
    for (const TableSpec &spec : StoryTables)
    {
        size_t glyphs = 0;
        std::vector<uint32_t> emptyBaseTopics;
        std::vector<uint32_t> emptyLocalizedTopics;
        std::vector<uint32_t> baseWeekdayIds;
        std::vector<uint32_t> localizedWeekdayIds;
        Rows rows = readTable(base, spec, false, glyphs, emptyBaseTopics, baseWeekdayIds);
        std::map<uint32_t, std::vector<std::string>> translated;
        if (chinese && !spec.displayColumns.empty())
        {
            for (const std::vector<std::string> &row :
                readTable(localized, spec, true, glyphs, emptyLocalizedTopics, localizedWeekdayIds))
            {
                translated.emplace(*rowId(row), row);
            }
        }
        size_t applied = 0;
        size_t untranslated = 0;
        size_t preservedDifferences = 0;
        for (std::vector<std::string> &row : rows)
        {
            const std::string baseTopic = std::string_view(spec.pSource) == "npctopic.txt" ? row[1] : "";
            const auto match = translated.find(*rowId(row));
            if (match != translated.end())
            {
                const std::vector<std::string> &overlay = match->second;
                for (size_t column = 1; column < std::max(row.size(), overlay.size()); ++column)
                {
                    const bool display = std::find(spec.displayColumns.begin(), spec.displayColumns.end(), column)
                        != spec.displayColumns.end();
                    if (display && column < overlay.size())
                    {
                        if (row.size() <= column)
                        {
                            row.resize(column + 1);
                        }
                        row[column] = overlay[column];
                    }
                    else if (!display && (column < row.size() ? row[column] : "")
                        != (column < overlay.size() ? overlay[column] : ""))
                    {
                        ++preservedDifferences;
                    }
                }
                ++applied;
                translated.erase(match);
            }
            else if (!spec.displayColumns.empty())
            {
                ++untranslated;
            }
            if (std::string_view(spec.pSource) == "npctopic.txt")
            {
                if (row.size() > 7
                    && std::any_of(row.begin() + 7, row.end(), [](const std::string &cell) { return !cell.empty(); }))
                {
                    throw std::runtime_error("Unexpected NPC topic extension schema");
                }
                row.resize(7);
                row.push_back(baseTopic);
            }
        }
        if (blasterLearning && std::string_view(spec.pSource) == "npctopic.txt")
        {
            const auto topic = std::find_if(rows.begin(), rows.end(), [](const std::vector<std::string> &row)
                { return rowId(row) == 950; });
            if (topic == rows.end() || topic->size() != 8)
            {
                throw std::runtime_error("Missing or malformed original Blaster learning topic 950");
            }
            (*topic)[1] = blasterLearning->topic;
            (*topic)[7] = blasterLearning->baseTopic;
        }
        if (crossContinents && std::string_view(spec.pSource) == "npctopic.txt")
        {
            const CrossContinentsBreachTravelTopic &topic = crossContinents->breachTravelTopic;
            const bool collision = std::any_of(rows.begin(), rows.end(), [&](const std::vector<std::string> &row)
                { return rowId(row) == topic.id; })
                || translated.contains(topic.id)
                || std::find(emptyBaseTopics.begin(), emptyBaseTopics.end(), topic.id) != emptyBaseTopics.end()
                || std::find(emptyLocalizedTopics.begin(), emptyLocalizedTopics.end(), topic.id)
                    != emptyLocalizedTopics.end();
            if (collision)
            {
                throw std::runtime_error("BreachTravel synthetic topic collides with an original NPC topic");
            }
            // This is an ordinary global topic row. The handler displays NPCTopic186 explicitly,
            // so its Text # is empty rather than silently changing that source namespace to NPCText.
            rows.push_back({std::to_string(topic.id), topic.topic, "",
                "Verdant - BreachTravel; Quest_CrossContinents.lua447-463; Hints slot3", "", "803", "803",
                topic.baseTopic});
        }
        manifest << (tables.empty() ? "" : ",\n") << "{\"source\":" << std::quoted(spec.pSource)
            << ",\"output\":" << std::quoted(std::string("engine/data_tables/") + spec.pOutput)
            << ",\"rows\":" << rows.size() << ",\"localized_rows\":" << applied
            << ",\"untranslated_rows\":" << untranslated << ",\"restored_glyphs\":" << glyphs
            << ",\"preserved_non_display_differences\":" << preservedDifferences
            << ",\"ignored_localization_ids\":[";
        bool first = true;
        for (const auto &[id, row] : translated)
        {
            manifest << (first ? "" : ",") << id;
            first = false;
        }
        manifest << "],\"empty_base_topic_ids\":[";
        for (size_t i = 0; i < emptyBaseTopics.size(); ++i)
        {
            manifest << (i == 0 ? "" : ",") << emptyBaseTopics[i];
        }
        manifest << "],\"empty_localized_topic_ids\":[";
        for (size_t i = 0; i < emptyLocalizedTopics.size(); ++i)
        {
            manifest << (i == 0 ? "" : ",") << emptyLocalizedTopics[i];
        }
        manifest << "],\"normalized_base_weekday_news_ids\":[";
        for (size_t i = 0; i < baseWeekdayIds.size(); ++i)
        {
            manifest << (i == 0 ? "" : ",") << baseWeekdayIds[i];
        }
        manifest << "],\"normalized_localized_weekday_news_ids\":[";
        for (size_t i = 0; i < localizedWeekdayIds.size(); ++i)
        {
            manifest << (i == 0 ? "" : ",") << localizedWeekdayIds[i];
        }
        manifest << "]";
        if ((crossContinents || blasterLearning) && std::string_view(spec.pSource) == "npctopic.txt")
        {
            manifest << ",\"projected_topics\":[";
            if (crossContinents)
            {
                manifest << "{\"id\":" << crossContinents->breachTravelTopic.id
                    << ",\"source\":\"Scripts/Global/Quest_CrossContinents.lua:447-463\",\"source_sha256\":"
                    << std::quoted(crossContinents->sourceSha256)
                    << ",\"label_source\":\"Game.MapStats[204].Name\",\"message_source\":\"Game.NPCTopic[186]\","
                    << "\"npc\":803,\"branch\":\"Hints\",\"slot\":3}";
            }
            if (blasterLearning)
            {
                manifest << (crossContinents ? "," : "") << "{\"id\":950,"
                    "\"source\":\"Scripts/Global/PromotionTopics.lua:4-14\",\"source_sha256\":"
                    << std::quoted(blasterLearning->sourceSha256)
                    << ",\"label_source\":" << std::quoted(chinese
                        ? "Data/10 LocZHCN.EnglishT.lod::global.txt:278" : "Data/mmmerge.T.lod::global.txt:278")
                    << ",\"global_text_sha256\":" << std::quoted(blasterLearning->globalTextSha256)
                    << ",\"base_label_source\":\"Data/mmmerge.T.lod::global.txt:278\",\"base_global_text_sha256\":"
                    << std::quoted(blasterLearning->baseGlobalTextSha256)
                    << ",\"display_columns\":[1,7],\"npcs\":[422,426]}";
            }
            manifest << "]";
        }
        manifest << "}";
        if (std::string_view(spec.pSource) == "npctopic.txt")
        {
            rows.insert(rows.begin(), {"#", "Topic", "Requires", "Notes", "Text #", "Owner(s)",
                "Owner #", "Base Topic"});
        }
        tables.emplace(spec.pOutput, std::move(rows));
    }
    Game::NpcDialogTable npcs;
    Game::JournalQuestTable quests;
    if (!npcs.loadNpcRows(tables.at("npc.txt")) || !npcs.loadGreetingsFromRows(tables.at("npc_greet.txt"))
        || !npcs.loadTextsFromRows(tables.at("npc_topic_text.txt"))
        || !npcs.loadTopicsFromRows(tables.at("npc_topic.txt"))
        || !npcs.loadNewsFromRows(tables.at("npc_news.txt"))
        || !npcs.loadGroupNewsFromRows(tables.at("english/npc_group.txt"))
        || !quests.loadFromRows(tables.at("english/quests.txt")))
    {
        throw std::runtime_error("Converted story tables failed production loader validation");
    }
    manifest << "\n],\"validated_npcs\":" << npcs.entries().size()
        << ",\"visible_quest_texts\":" << quests.entries().size()
        << ",\"table_loaders_validated\":true,\"credits\":{\"source_archive\":"
        << std::quoted(creditsArchive.filename().string())
        << ",\"source\":\"Credits.txt\",\"output\":\"engine/data_tables/credits.txt\""
        << ",\"source_lines\":" << credits.sourceLines << ",\"output_lines\":" << credits.outputLines
        << ",\"nonempty_lines\":" << credits.nonemptyLines << ",\"restored_glyphs\":" << creditsGlyphs
        << ",\"removed_heading_markers\":" << credits.headingMarkers
        << ",\"removed_layout_controls\":" << credits.layoutControls << "}}\n";
    if (!std::filesystem::create_directories(destination))
    {
        throw std::runtime_error("Story destination was created by another operation");
    }
    try
    {
        for (const auto &[name, rows] : tables)
        {
            writeText(destination / "engine/data_tables" / name, serializeTextRows(rows));
        }
        writeText(destination / "engine/data_tables/credits.txt", credits.text);
        writeText(destination / "story_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(destination);
        throw;
    }
}
}
