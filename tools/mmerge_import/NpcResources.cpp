#include "NpcResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/events/EventDialogContent.h"
#include "game/gameplay/GenericActorDialog.h"
#include "game/gameplay/NpcFollowerRuntime.h"
#include "game/tables/MergedBaseTables.h"

#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
const Row ProfessionHeader = {"#", "Professions", "Prof name (Global.txt)", "Rarity", "Cost/w", "Personality",
    "Action Topic", "Joins", "Recruit", "Join Text", "Description"};
const Row BtbHeader = {"", "Beg", "Bribe", "Threat", "Creed", "Fame required", "Reputation required",
    "Rep ok, 1st greet", "Rep ok, 2nd greet", "Beg return", "Bribe return", "Threat return", "Fame too low",
    "Rep notorious, I'm good", "Rep notorious, I'm evil", "Rep saintly, I'm good", "Rep Saintly, I'm evil",
    "Rep below zero, 1st greet", "Rep above 10, 1st greet (and I'm evil)", "You aren't good enough for me. (1st time)",
    "You aren't bad enough for me.", "Rep below zero, 2nd greet", "Rep above 10, 2nd greet (and I'm evil)",
    "You still aren't good enough for me (2nd time)", "You still aren't bad enough for me (2nd time)",
    "I accept your beg", "I don't like begging", "Thanks for the bribe", "I don't like bribes",
    "I accept your threat", "I don't like threats"};

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

int64_t number(const std::string &text, int64_t maximum = 65535, bool optional = false, int64_t minimum = 0)
{
    const std::string value = trim(text);
    if (value.empty() && optional)
    {
        return 0;
    }
    int64_t result = 0;
    const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size()
        && result >= minimum && result <= maximum, "Invalid NPC numeric field: " + text);
    return result;
}

bool marker(const std::string &text)
{
    require(text.empty() || text == "x", "Unsupported NPC marker: " + text);
    return text == "x";
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "NPC input escapes root");
    return path;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid NPC quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 != table->getRowCount() || row != Row{""})
        {
            rows.push_back(row);
        }
    }
    return rows;
}

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy, bool chinese = false)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid NPC dependency size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read NPC dependency");
    if (legacy)
    {
        size_t glyphs = 0;
        text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), chinese, glyphs);
    }
    return parse(text);
}

std::map<uint32_t, std::string> globalTexts(const std::filesystem::path &root, bool chinese, size_t &glyphs)
{
    const std::string archive = chinese ? "10 LocZHCN.EnglishT.lod" : "mmmerge.T.lod";
    const Rows rows = parse(decodeLegacyText(readLodText(checkedPath(root, "Data/" + archive), "global.txt"),
        chinese, glyphs));
    require(rows.size() > 2 && rows[0] == Row{"Global Text", ""}, "Unsupported global text header");
    std::map<uint32_t, std::string> texts;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        if (row.empty() || (row.size() == 1 && row[0].empty()))
        {
            continue;
        }
        require(row.size() >= 2 && texts.emplace(number(row[0]), row[1]).second, "Invalid global text identity");
    }
    return texts;
}

void checkNewsNamespace()
{
    using namespace Game;
    NpcDialogTable dialogs;
    require(dialogs.loadNewsFromRows({{"123", "Wrong news registry", "Rumor"}})
        && dialogs.loadTextsFromRows({{"123", "正确的中文对话"}}), "News fixture failed");
    MergedNewsTopicTable area;
    require(area.loadFromRows({{"Area", "Topic Name", "Text"}, {"1", "NPCTopic 123", "NPCText 123"}}),
        "Explicit news reference failed");
    MapStatsEntry map = {};
    map.id = 1;
    EventRuntimeState state;
    const std::optional<GenericActorDialogResolution> resolved = resolveGenericActorDialog(
        "fixture.odm", "Fixture", 0, state, dialogs, nullptr, &map, &area);
    require(resolved && resolved->newsFromNpcText && resolved->newsId == 123,
        "Actor dialogue lost explicit text namespace");
    state.pendingDialogueContext = EventRuntimeState::PendingDialogueContext{};
    state.pendingDialogueContext->kind = DialogueContextKind::NpcNews;
    state.pendingDialogueContext->newsId = resolved->newsId;
    state.pendingDialogueContext->newsFromNpcText = resolved->newsFromNpcText;
    const EventDialogContent content = buildEventDialogContent(state, 0, true, nullptr, nullptr, nullptr,
        &dialogs, nullptr, &map, nullptr, nullptr, nullptr, 0);
    require(content.isActive && content.lines == std::vector<std::string>{"正确的中文对话"},
        "Rebuilt dialogue used a colliding news ID");
    require(dialogs.getNewsDialogText(123) == "Wrong news registry"
        && !dialogs.getNewsDialogText(999, true), "Legacy news or explicit missing text changed");
}
}

void convertNpcResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "NPC output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "NPC output already exists");
    const auto sourceRows = [&root](const std::string &name) { return readRows(root, "Data/Tables/" + name, true); };
    const auto assetRows = [&assets](const std::string &name)
    {
        return readRows(assets, "engine/data_tables/" + name, false);
    };
    Rows professions = sourceRows("NPC professions.txt");
    Rows names = sourceRows("NPC names.txt");
    const Rows btb = sourceRows("NPC BTB.txt");
    const Rows area = sourceRows("News topics - area.txt");
    const Rows continent = sourceRows("News topics - continent.txt");
    const Rows professionNews = sourceRows("News topics - profession.txt");
    require(professions.size() > 1 && professions[0] == ProfessionHeader && btb.size() > 1 && btb[0] == BtbHeader,
        "Unsupported NPC mechanics headers");
    require(names.size() > 1 && names[0] == Row{"Male", "Female"}, "Unsupported NPC names header");
    const bool localized = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    const std::string localizedNamesPath = "Data/Text localization/ZHCN_NPCNames.txt";
    const bool localizedNames = localized && std::filesystem::exists(root / localizedNamesPath);
    if (localizedNames)
    {
        const Rows translatedNames = readRows(root, localizedNamesPath, true, true);
        require(translatedNames.size() == names.size() && translatedNames[0] == names[0],
            "NPC name translation changes the source row count or header");
        for (size_t index = 1; index < names.size(); ++index)
        {
            require(!names[index].empty() && names[index].size() <= 2
                && !translatedNames[index].empty() && translatedNames[index].size() <= 2,
                "Invalid translated NPC name row");
            for (size_t sex = 0; sex < 2; ++sex)
            {
                const bool sourcePresent = names[index].size() > sex && !trim(names[index][sex]).empty();
                const bool translatedPresent = translatedNames[index].size() > sex
                    && !trim(translatedNames[index][sex]).empty();
                require(sourcePresent == translatedPresent, "NPC name translation changes pool membership");
            }
        }
        // LocalizeTables.lua replaces both pools in file order; repeated rows retain their random weight.
        names = translatedNames;
    }
    NpcDialogTable dialogs;
    MapStats maps;
    require(dialogs.loadNpcRows(assetRows("npc.txt")) && dialogs.loadTopicsFromRows(assetRows("npc_topic.txt"))
        && dialogs.loadTextsFromRows(assetRows("npc_topic_text.txt"))
        && dialogs.loadNewsFromRows(assetRows("npc_news.txt"))
        && maps.loadFromRows(assetRows("map_stats.txt")), "Invalid NPC registry dependency");
    size_t glyphs = 0;
    const std::map<uint32_t, std::string> baseGlobal = globalTexts(root, false, glyphs);
    const std::map<uint32_t, std::string> chineseGlobal = localized ? globalTexts(root, true, glyphs)
        : std::map<uint32_t, std::string>{};
    MergedNpcBtbTable btbTable;
    require(btbTable.loadFromRows(btb), "NPC personality loader failed");
    std::set<std::string> personalities;
    for (size_t index = 1; index < btb.size(); ++index)
    {
        const Row &row = btb[index];
        require(row.size() == 31 && !row[0].empty() && personalities.insert(row[0]).second,
            "Invalid/duplicate NPC personality");
        const MergedNpcBtbEntry *pEntry = btbTable.get(row[0]);
        require(pEntry && pEntry->acceptBeg == marker(row[1]) && pEntry->acceptBribe == marker(row[2])
            && pEntry->acceptThreat == marker(row[3]) && pEntry->creed == row[4]
            && (row[4] == "Neutral" || row[4] == "Light" || row[4] == "Dark")
            && pEntry->requiredFame == number(row[5], 0x7FFFFFFF)
            && pEntry->requiredReputation == number(row[6], 0x7FFFFFFF, false, -0x80000000LL),
            "NPC personality semantics changed");
        const std::array<uint32_t, 24> fields = {pEntry->reputationOkFirstTextId, pEntry->reputationOkSecondTextId,
            pEntry->begReturnTextId, pEntry->bribeReturnTextId, pEntry->threatReturnTextId, pEntry->fameTooLowTextId,
            pEntry->reputationNotoriousGoodTextId, pEntry->reputationNotoriousEvilTextId,
            pEntry->reputationSaintlyGoodTextId, pEntry->reputationSaintlyEvilTextId,
            pEntry->reputationBelowZeroFirstGoodTextId, pEntry->reputationAboveTenFirstEvilTextId,
            pEntry->lowReputationFirstGoodTextId, pEntry->lowReputationFirstEvilTextId,
            pEntry->reputationBelowZeroSecondGoodTextId, pEntry->reputationAboveTenSecondEvilTextId,
            pEntry->lowReputationSecondGoodTextId, pEntry->lowReputationSecondEvilTextId,
            pEntry->begSuccessTextId, pEntry->begFailTextId, pEntry->bribeSuccessTextId, pEntry->bribeFailTextId,
            pEntry->threatSuccessTextId, pEntry->threatFailTextId};
        for (size_t field = 0; field < fields.size(); ++field)
        {
            require(fields[field] == number(row[field + 7]) && dialogs.getText(fields[field]).has_value(),
                "NPC personality text identity changed or is missing");
        }
    }
    size_t translated = 0;
    for (size_t index = 1; index < professions.size(); ++index)
    {
        Row &row = professions[index];
        require(row.size() == 11 && number(row[0]) == index - 1, "NPC profession ID differs from source row order");
        const uint32_t globalId = number(row[2], 65535, true);
        std::string display = row[1];
        if (!row[2].empty())
        {
            require(baseGlobal.contains(globalId) && baseGlobal.at(globalId) == row[1],
                "NPC profession global name identity changed");
            const auto localizedIt = chineseGlobal.find(globalId);
            if (localizedIt != chineseGlobal.end() && !trim(localizedIt->second).empty())
            {
                display = localizedIt->second;
                ++translated;
            }
        }
        require(personalities.contains(row[5]), "Unknown NPC personality");
        const uint32_t action = number(row[6], 65535, true);
        require(action == 0 || dialogs.getTopicById(action).has_value(), "Missing NPC action topic");
        for (size_t field : {9u, 10u})
        {
            const uint32_t id = number(row[field], 65535, true);
            require(id == 0 || dialogs.getText(id).has_value(), "Missing NPC profession text");
        }
        row.push_back(display);
    }
    professions[0].push_back("Display Name");
    MergedNpcProfessionTable professionTable;
    require(professionTable.loadFromRows(professions), "NPC profession loader failed");
    for (size_t index = 1; index < professions.size(); ++index)
    {
        const Row &row = professions[index];
        const MergedNpcProfessionEntry *pEntry = professionTable.get(index - 1);
        require(pEntry && pEntry->profession == row[1] && pEntry->globalTextId == number(row[2], 65535, true)
            && pEntry->rarity == number(row[3], 0x7FFFFFFF) && pEntry->weeklyCost == number(row[4], 0x7FFFFFFF)
            && pEntry->personality == row[5] && pEntry->actionTopicId == number(row[6], 65535, true)
            && pEntry->joins == marker(row[7]) && pEntry->recruit == marker(row[8])
            && pEntry->joinTextId == number(row[9], 65535, true)
            && pEntry->descriptionTextId == number(row[10], 65535, true) && pEntry->displayName() == trim(row[11]),
            "NPC profession mechanics or display changed");
        if (pEntry->id != 0)
        {
            EventRuntimeState state;
            EventRuntimeState::HiredNpcFollower follower = {};
            follower.npcId = 1;
            follower.professionId = pEntry->id;
            follower.weeklyCost = pEntry->weeklyCost;
            state.hiredNpcFollowers.push_back(follower);
            const std::vector<HiredNpcFollowerView> views = buildHiredNpcFollowerViews(state, dialogs, professionTable);
            require(views.size() == 1 && views[0].profession == pEntry->displayName()
                && views[0].weeklyCost == pEntry->weeklyCost && views[0].professionId == pEntry->id,
                "Follower presentation changed localized name, profession ID or fee");
        }
    }
    MergedNpcNameTable nameTable;
    require(nameTable.loadFromRows(names), "NPC name loader failed");
    for (size_t sex = 0; sex < 2; ++sex)
    {
        std::vector<std::string> expected;
        for (size_t index = 1; index < names.size(); ++index)
        {
            require(!names[index].empty() && names[index].size() <= 2, "Invalid NPC name row");
            if (names[index].size() > sex && !trim(names[index][sex]).empty())
            {
                expected.push_back(trim(names[index][sex]));
            }
        }
        require(expected == (sex == 0 ? nameTable.maleNames() : nameTable.femaleNames()),
            "NPC name order or frequency changed");
    }
    std::set<uint32_t> continentIds;
    const Rows continents = sourceRows("Continent settings.txt");
    for (size_t index = 1; index < continents.size(); ++index)
    {
        require(!continents[index].empty(), "Empty continent definition");
        continentIds.insert(number(continents[index][0]));
    }
    size_t explicitTextReferences = 0;
    const auto checkNews = [&](const Rows &rows, bool mapNews)
    {
        require(rows.size() > 1 && rows[0] == Row{mapNews ? "Area" : "Continent", "Topic Name", "Text"},
            "Unsupported NPC news header");
        MergedNewsTopicTable table;
        require(table.loadFromRows(rows) && table.entries().size() == rows.size() - 1, "NPC news loader lost entries");
        for (size_t index = 1; index < rows.size(); ++index)
        {
            const Row &row = rows[index];
            require(row.size() == 3, "Invalid NPC news width");
            const MergedNewsTopicEntry &entry = table.entries()[index - 1];
            require(mapNews ? maps.findById(entry.ownerId) != nullptr : continentIds.contains(entry.ownerId),
                "NPC news owner is missing");
            const bool topicFromNpc = row[1].starts_with("NPCTopic ");
            const bool textFromNpc = row[2].starts_with("NPCText ");
            require(entry.ownerId == number(row[0]) && entry.topicFromNpcTable == topicFromNpc
                && entry.textFromNpcTable == textFromNpc
                && entry.topicTextId == number(topicFromNpc ? row[1].substr(9) : row[1])
                && entry.newsTextId == number(textFromNpc ? row[2].substr(8) : row[2]), "NPC news reference changed");
            require(topicFromNpc ? dialogs.getTopicById(entry.topicTextId).has_value()
                : dialogs.getNewsTopic(entry.topicTextId).has_value(), "Missing NPC news title");
            require(textFromNpc ? dialogs.getText(entry.newsTextId).has_value()
                : dialogs.getNewsText(entry.newsTextId).has_value(), "Missing NPC news body");
            explicitTextReferences += textFromNpc;
        }
    };
    checkNews(area, true);
    checkNews(continent, false);
    Row expectedNewsHeader = {"#", "Profession name"};
    for (const std::string day : {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"})
    {
        expectedNewsHeader.push_back(day + " topic");
        expectedNewsHeader.push_back(day + " text");
    }
    require(professionNews.size() > 1 && professionNews[0] == expectedNewsHeader,
        "Unsupported profession news header");
    MergedNewsProfessionTopicTable weeklyNews;
    require(weeklyNews.loadFromRows(professionNews), "Profession news loader failed");
    size_t professionLabelDifferences = 0;
    for (size_t index = 1; index < professionNews.size(); ++index)
    {
        const Row &row = professionNews[index];
        require(row.size() == 16 && number(row[0]) == index && professionTable.get(index),
            "Profession news identity differs from source row order");
        professionLabelDifferences += row[1] != professionTable.get(index)->profession;
        for (uint32_t day = 0; day < 7; ++day)
        {
            const MergedNewsProfessionDayTopic *pEntry = weeklyNews.get(index, day);
            require(pEntry && pEntry->topicTextId == number(row[2 + day * 2])
                && pEntry->newsTextId == number(row[3 + day * 2])
                && dialogs.getNewsTopic(pEntry->topicTextId).has_value()
                && dialogs.getNewsText(pEntry->newsTextId).has_value(), "Missing/changed weekday news reference");
        }
    }
    checkNewsNamespace();
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"npc_rules\",\"runtime_ready\":false,\"professions\":"
        << professionTable.entries().size() << ",\"translated_professions\":" << translated
        << ",\"global_archive_restored_glyphs\":" << glyphs << ",\"male_names\":" << nameTable.maleNameCount()
        << ",\"female_names\":" << nameTable.femaleNameCount() << ",\"personalities\":" << personalities.size()
        << ",\"area_news\":" << area.size() - 1 << ",\"continent_news\":" << continent.size() - 1
        << ",\"profession_day_news\":" << weeklyNews.topicCount()
        << ",\"explicit_npc_text_references\":" << explicitTextReferences
        << ",\"profession_news_label_differences\":" << professionLabelDifferences
        << ",\"typed_validation\":true,\"namespace_dialog_check\":true,\"follower_display_check\":true"
        << ",\"random_names_localized\":" << (localizedNames ? "true" : "false")
        << ",\"original_news_selection_parity\":false,\"btb_gameplay_validated\":false,\"full_gameplay_loop\":false}\n";
    const std::map<std::string, Rows> tables = {{"npc_professions.txt", professions}, {"npc_names.txt", names},
        {"npc_btb.txt", btb}, {"news_topics_area.txt", area}, {"news_topics_continent.txt", continent},
        {"news_topics_profession.txt", professionNews}};
    std::map<std::string, std::string> files;
    for (const auto &[name, rows] : tables)
    {
        const std::string text = serializeTextRows(rows);
        require(parse(text) == rows, "NPC serialization changed source cells");
        files.emplace("engine/data_tables/" + name, text);
    }
    files.emplace("npc_conversion.json", report.str());
    require(std::filesystem::create_directory(output), "Cannot create fresh NPC output");
    try
    {
        for (const auto &[name, text] : files)
        {
            writeConvertedResource(output / name, std::vector<uint8_t>(text.begin(), text.end()));
        }
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
