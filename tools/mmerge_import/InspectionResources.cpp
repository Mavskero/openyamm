#include "InspectionResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/CharacterInspectTable.h"

#include <charconv>
#include <cctype>
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
const Row StatNames = {"Might", "Intellect", "Personality", "Endurance", "Accuracy", "Speed", "Luck",
    "Hit Points", "Armor Class", "Spell Points", "Condition", "Quick Spell", "Age", "Level", "Experience",
    "Attack Bonus", "Attack Damage", "Shoot Bonus", "Shoot Damage", "Fire", "Air", "Water", "Earth",
    "Mind", "Body", "Skill Points"};

// These source tables have positional identities and no numeric ID column.
// Check the supplied ZHCN profile's complete label order before overlaying text;
// equal row counts alone cannot detect shifted or swapped translations.
const Row ChineseStats = {"力量", "智力", "个性", "耐力", "精确", "速度", "运气", "生命值", "防御等级",
    "魔法值", "状态", "备用魔法", "年龄", "等级", "经验值", "攻击点数", "攻击杀伤力", "射击点数",
    "射击杀伤力", "火系抗性", "风系抗性", "水系抗性", "土系抗性", "心系抗性", "体系抗性", "技能点数"};
const Row ChineseSkills = {"手杖", "长剑", "匕首", "战斧", "长矛", "弓箭", "钉锤", "榴弹枪", "盾牌",
    "皮甲", "锁甲", "钢甲", "火系魔法", "空气魔法", "水系魔法", "土系魔法", "灵魂魔法", "心智魔法",
    "肢体魔法", "光明魔法", "暗黑魔法", "黑暗精灵能力", "吸血鬼能力", "飞龙能力", "物品鉴定术",
    "经商术", "修补术", "健身术", "冥想术", "洞察术", "再生术", "陷阱拆卸术", "闪躲术",
    "空手搏斗术", "怪物鉴定术", "武器使用术", "偷窃术", "炼金术", "学习术"};
const Row ChineseClasses = {"弓箭手", "高级射手", "神箭手", "狙击手", "牧师", "神甫", "光明主教",
    "黑暗主教", "黑暗精灵", "暗精尊者", "飞龙", "神龙", "德鲁伊", "高级德鲁伊", "邪术师", "大德鲁伊",
    "骑士", "豪侠", "黑骑士", "勇士", "牛头人", "牛头领主", "僧侣", "沙弥", "智者", "忍者",
    "圣武士", "十字军", "英雄", "魔头", "游骑兵", "猎手", "赏金猎人", "游骑领主", "盗贼", "神偷",
    "杀手", "密探", "巨魔", "战斗巨魔", "吸血鬼", "吸血圣王", "巫师", "法师", "死灵法师", "巫妖",
    "大法师", "死灵大法师", "农民", "n/u", "大主教", "魔法大师", "n/u"};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string trim(const std::string &text)
{
    size_t start = 0;
    size_t end = text.size();
    while (start < end && std::isspace(static_cast<unsigned char>(text[start])))
    {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        --end;
    }
    return text.substr(start, end - start);
}

uint32_t integer(const std::string &text)
{
    uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size() && value <= 255,
        "Invalid inspection metadata number");
    return value;
}

std::filesystem::path sourcePath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Inspection source escapes root");
    return path;
}

Rows parseTable(const std::string &text, const Row &header, size_t count, bool multiline, size_t &continuations)
{
    const std::optional<Engine::TextTable> parsed = Engine::TextTable::parseTabSeparated(text);
    require(parsed && parsed->getRowCount() > 0 && parsed->getRow(0) == header, "Invalid inspection header/quoting");
    Rows rows = {header};
    for (size_t index = 1; index < parsed->getRowCount(); ++index)
    {
        const Row &row = parsed->getRow(index);
        const bool blank = row.empty() || row == Row{""};
        if (rows.back().size() < header.size())
        {
            require(multiline && rows.back().size() == 2 && row.size() <= 2,
                "Interrupted inspection paragraph");
            rows.back()[1] += '\n';
            if (!blank)
            {
                rows.back()[1] += row[0];
                if (row.size() == 2)
                {
                    rows.back().push_back(row[1]);
                }
            }
            ++continuations;
            continue;
        }
        if (blank && index + 1 == parsed->getRowCount())
        {
            continue;
        }
        require(row.size() == header.size() || (multiline && row.size() == 2),
            "Malformed inspection row");
        rows.push_back(row);
    }
    require(rows.back().size() == header.size() && rows.size() == count + 1,
        "Inspection row count changed or paragraph unfinished");
    return rows;
}

void verifyLocalizedRows(const Rows &rows, const Row &names)
{
    require(rows.size() == names.size() + 1, "Localized inspection coverage changed");
    for (size_t index = 0; index < names.size(); ++index)
    {
        require(rows[index + 1][0] == names[index], "Localized inspection label/order changed");
    }
}

Rows reload(const Rows &rows)
{
    const std::optional<Engine::TextTable> parsed = Engine::TextTable::parseTabSeparated(serializeTextRows(rows));
    require(parsed && parsed->getRowCount() >= rows.size(), "Cannot reload inspection output");
    Rows result;
    for (size_t index = 0; index < rows.size(); ++index)
    {
        result.push_back(parsed->getRow(index));
    }
    require(result == rows, "Inspection serialization changed cells");
    return result;
}

void verifyLegacyLoading()
{
    Game::CharacterInspectTable table;
    require(table.loadStatRows({{"Might", "Old stat"}})
        && table.loadSkillRows({{"Skill", "Description", "Normal", "Expert", "Master", "GrandMaster"},
            {"Leather", "Old skill", "B", "E", "M", "G"}})
        && table.loadClassRows({{"Class name", "Class description", "Class kind"},
            {"Warrior Mage", "Old class", "Archer"}}), "Legacy inspection load failed");
    require(table.getStat("Might") && table.getStat("Might")->name == "Might"
        && table.getSkill("LeatherArmor") && table.getSkill("LeatherArmor")->name == "Leather Armor"
        && table.getClass("WarriorMage") && table.getClass("WarriorMage")->name == "Warrior Mage"
        && !table.getSkill("Skill") && !table.getClass("Class name"), "Legacy display/header behavior changed");
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertInspectionResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Inspection output must be outside source");
    require(!std::filesystem::exists(output), "Inspection output already exists");
    const std::filesystem::path base = sourcePath(root, "Data/mmmerge.T.lod");
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    const std::filesystem::path locale = chinese ? sourcePath(root, "Data/10 LocZHCN.EnglishT.lod") : base;
    size_t glyphs = 0;
    size_t continuations = 0;
    const auto read = [&](const std::filesystem::path &archive, const char *pName, const Row &header,
        size_t count, bool multiline, bool localized)
    {
        return parseTable(decodeLegacyText(readLodText(archive, pName), localized, glyphs),
            header, count, multiline, continuations);
    };
    const Rows stats = read(base, "stats.txt", {"Stats Descriptions", "Description"}, 26, false, false);
    const Rows skills = read(base, "skilldes.txt", {"Skill", "Description", "Normal", "Expert", "Master",
        "GrandMaster"}, 39, false, false);
    const Rows classes = read(base, "class.txt", {"Class name", "Class description", "Class kind"}, 53, true, false);
    const Rows localizedStats = chinese ? read(locale, "stats.txt", stats[0], 26, false, true) : stats;
    const Rows localizedSkills = chinese ? read(locale, "skilldes.txt", skills[0], 39, false, true) : skills;
    const Rows localizedClasses = chinese ? read(locale, "class.txt", classes[0], 53, true, true) : classes;
    if (chinese)
    {
        verifyLocalizedRows(localizedStats, ChineseStats);
        verifyLocalizedRows(localizedSkills, ChineseSkills);
        verifyLocalizedRows(localizedClasses, ChineseClasses);
    }

    const std::filesystem::path metadataPath = sourcePath(root, "Data/Tables/Class Extra.txt");
    const uintmax_t size = std::filesystem::file_size(metadataPath);
    require(size > 0 && size <= 1024 * 1024, "Invalid class metadata size");
    std::ifstream stream(metadataPath, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read class metadata");
    const Rows metadata = parseTable(decodeLegacyText(bytes, false, glyphs), {"#", "Kind", "Step", "Note"},
        53, false, continuations);

    Rows statsOutput = {{"Stats Descriptions", "Description", "Display Name"}};
    Rows skillsOutput = {skills[0]};
    skillsOutput[0].push_back("Display Name");
    Rows classesOutput = {{"Class name", "Class description", "Class kind", "Display Name", "Class ID"}};
    for (size_t index = 1; index < stats.size(); ++index)
    {
        require(stats[index][0] == StatNames[index - 1] && !trim(localizedStats[index][1]).empty(),
            "Stat identity changed or description empty");
        statsOutput.push_back({stats[index][0], localizedStats[index][1], localizedStats[index][0]});
    }
    const Row skillNames = Game::allCanonicalSkillNames();
    require(skillNames.size() >= 39, "Shared skill registry incomplete");
    for (size_t index = 1; index < skills.size(); ++index)
    {
        require(Game::canonicalSkillName(skills[index][0]) == skillNames[index - 1], "Skill description order changed");
        Row row = localizedSkills[index];
        for (size_t column = 1; column < row.size(); ++column)
        {
            require(!trim(row[column]).empty(), "Empty skill description");
        }
        row.push_back(row[0]);
        row[0] = skills[index][0];
        skillsOutput.push_back(row);
    }
    std::set<std::string> classKeys;
    for (size_t index = 1; index < classes.size(); ++index)
    {
        const size_t id = index - 1;
        require(integer(metadata[index][0]) == id, "Inspection class ID/order changed");
        const uint32_t kind = integer(metadata[index][1]);
        const uint32_t step = integer(metadata[index][2]);
        require(classes[index][2] == localizedClasses[index][2], "Localized class family/order changed");
        if (id == 49 || id == 52)
        {
            require(kind == 0 && step == 0 && metadata[index][3] == "n/u" && classes[index][0] == "n/u"
                && localizedClasses[index][0] == "n/u", "Inactive inspection class changed");
            continue;
        }
        const std::string key = Game::canonicalClassName(metadata[index][3]);
        if (id == 47)
        {
            require(key == "DarkArchMage" && classes[index][0] == "Master Necromancer"
                && classes[index][2] == "Wiz", "Class 47 display alias changed");
        }
        else
        {
            require(Game::canonicalClassName(classes[index][0]) == key,
                "Inspection class name association changed");
        }
        require(kind > 0 && !key.empty() && classKeys.insert(key).second
            && !trim(localizedClasses[index][1]).empty(), "Invalid active inspection class");
        classesOutput.push_back({key, localizedClasses[index][1], classes[index][2], localizedClasses[index][0],
            std::to_string(id)});
    }

    Game::CharacterInspectTable check;
    require(check.loadStatRows(reload(statsOutput)) && check.loadSkillRows(reload(skillsOutput))
        && check.loadClassRows(reload(classesOutput)), "Typed inspection load failed");
    for (size_t index = 1; index < statsOutput.size(); ++index)
    {
        const Row &row = statsOutput[index];
        const Game::StatInspectEntry *pEntry = check.getStat(row[0]);
        require(pEntry && pEntry->name == trim(row[2]) && pEntry->description == trim(row[1]),
            "Stat display/lookup changed");
        require(!chinese || check.getStat(row[2]) == nullptr, "Localized stat label became a mechanism key");
    }
    for (size_t index = 1; index < skillsOutput.size(); ++index)
    {
        const Row &row = skillsOutput[index];
        const Game::SkillInspectEntry *pEntry = check.getSkill(skillNames[index - 1]);
        require(pEntry && pEntry->name == trim(row[6]) && pEntry->description == trim(row[1])
            && pEntry->normalDescription == trim(row[2]) && pEntry->expertDescription == trim(row[3])
            && pEntry->masterDescription == trim(row[4]) && pEntry->grandmasterDescription == trim(row[5]),
            "Skill display/lookup changed");
        require(!chinese || check.getSkill(row[6]) == nullptr, "Localized skill label became a mechanism key");
    }
    for (size_t index = 1; index < classesOutput.size(); ++index)
    {
        const Row &row = classesOutput[index];
        const Game::ClassInspectEntry *pEntry = check.getClass(row[0]);
        require(pEntry && pEntry->name == trim(row[3]) && pEntry->description == trim(row[1]),
            "Class display/lookup changed");
        require(!chinese || check.getClass(row[3]) == nullptr, "Localized class label became a mechanism key");
    }
    require(!check.getSkill("Skill") && !check.getClass("Class name") && !check.getClass("n/u")
        && !check.getClass("Master Necromancer"), "Phantom inspection key became active");
    verifyLegacyLoading();
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-character-inspection\",\"runtime_ready\":false,"
        << "\"stats\":26,\"skills\":39,\"classes\":51,\"inactive_class_ids\":[49,52],"
        << "\"localized_entries\":" << (chinese ? 116 : 0) << ",\"restored_glyphs\":" << glyphs
        << ",\"unquoted_continuation_lines\":" << continuations
        << ",\"class_display_alias_ids\":[47],\"typed_validation\":true,\"legacy_tables_validated\":true,"
        << "\"descriptions_match_mechanics_validated\":false,\"full_gameplay_ui\":false,"
        << "\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh inspection output");
    try
    {
        writeText(output / "engine/data_tables/english/stats.txt", serializeTextRows(statsOutput));
        writeText(output / "engine/data_tables/english/skill_des.txt", serializeTextRows(skillsOutput));
        writeText(output / "engine/data_tables/english/class.txt", serializeTextRows(classesOutput));
        writeText(output / "inspection_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
