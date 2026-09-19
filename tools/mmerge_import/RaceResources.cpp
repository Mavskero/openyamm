#include "RaceResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/RaceStartingStatsTable.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
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
const Row RaceNames = {"Human", "Vampire", "DarkElf", "Minotaur", "Troll", "Dragon", "Undead", "Elf",
    "Goblin", "Dwarf", "Zombie"};
const Row StatHeaders = {"Human (0)", "Vampire (1)", "Dark elf (2)", "Minotaur (3)", "Troll (4)", "Dragon (5)",
    "Undead (6)", "Elf (7)", "Goblin (8)", "Dwarf (9)", "Zombie (10)"};
const Row FamilyNames = {"Archer", "Cleric", "Dark Elf", "Dragon", "Druid", "Knight", "Minotaur", "Monk",
    "Paladin", "Ranger", "Thief", "Troll", "Vampire", "Sorcerer", "Necromancer", "Peasant"};
const Row StatNames = {"Might", "Intellect", "Personality", "Endurance", "Accuracy", "Speed", "Luck"};
const Row ContinentNames = {"Jadam", "Antagarich", "Enroth", "Between time"};
const Row ContinentKeys = {"jadame", "antagarich", "enroth", "between_time"};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int number(const std::string &text, int maximum = 255)
{
    int value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value >= 0 && value <= maximum,
        "Invalid race table number: " + text);
    return value;
}

Row split(const std::string &text)
{
    Row parts;
    size_t start = 0;
    while (true)
    {
        const size_t end = text.find('/', start);
        parts.push_back(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos)
        {
            return parts;
        }
        start = end + 1;
    }
}

Rows read(const std::filesystem::path &root, const std::string &name, bool legacy = true)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Race source escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 4 * 1024 * 1024, "Invalid race table size");
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read race table");
    size_t glyphs = 0;
    const std::string text = legacy ? decodeLegacyText(bytes, false, glyphs) : std::string(bytes.begin(), bytes.end());
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value() && glyphs == 0, "Invalid race mechanism table");
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

void shape(const Rows &rows, size_t count, size_t columns)
{
    require(rows.size() == count, "Unsupported race table row count");
    for (const Row &row : rows)
    {
        require(row.size() == columns, "Unsupported race table column count");
    }
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}

struct Rule
{
    size_t race = 0;
    size_t kind = 0;
    size_t skill = 0;
    int minimum = 0;
    int add = 0;
    int exception = 0;
    std::string raw;
};

bool applies(const Rule &rule, size_t kind, int manaMode)
{
    if (rule.kind != 0)
    {
        // The authored class-specific pool is unconditional; only general
        // racial pools use the exception field in RacialSkills.lua.
        return rule.kind == kind;
    }
    return rule.exception == 0 || !(rule.exception == static_cast<int>(kind)
        || (rule.exception == -4 && manaMode == 0) || (rule.exception == -5 && manaMode > 0)
        || -rule.exception == manaMode);
}
}

void convertRaceResources(const std::filesystem::path &source, const std::filesystem::path &classAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path classRoot = std::filesystem::canonical(classAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, classRoot})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Race output must be outside input roots");
    }
    require(!std::filesystem::exists(output), "Race output already exists");

    const Rows sourceStats = read(root, "Data/Tables/Class Starting Stats.txt");
    const Rows sourceSkills = read(root, "Data/Tables/Race Skills.txt");
    const Rows selection = read(root, "Data/Tables/Character selection.txt");
    const Rows metadata = read(root, "Data/Tables/Class Extra.txt");
    const Rows sourceHp = read(root, "Data/Tables/Class HP SP.txt");
    const Rows sourceCaps = read(root, "Data/Tables/Class Skills.txt");
    const Rows sourceStarting = read(root, "Data/Tables/Class Starting Skills.txt");
    const Rows continents = read(root, "Data/Tables/Continent settings.txt");
    shape(sourceStats, 15, 19);
    shape(sourceSkills, 40, 188);
    shape(metadata, 54, 4);
    shape(sourceHp, 54, 6);
    shape(sourceCaps, 40, 54);
    shape(sourceStarting, 40, 31);
    require(selection.size() == 25 && selection[0].size() == 54 && selection[0][0] == "Race/Class"
        && selection[12] == Row{"", "Continents:"} && continents.size() == 5, "Invalid race selection structure");
    require(sourceStats[0][0] == "Stat" && sourceSkills[0][0].empty()
        && metadata[0] == Row{"#", "Kind", "Step", "Note"}, "Invalid race source header");

    const Rows extra = read(classRoot, "engine/data_tables/class_extra.txt", false);
    const Rows hp = read(classRoot, "engine/data_tables/class_multipliers.txt", false);
    const Rows caps = read(classRoot, "engine/data_tables/class_skills.txt", false);
    const Rows starting = read(classRoot, "engine/data_tables/class_starting_skills.txt", false);
    shape(extra, 52, 4);
    shape(hp, 52, 6);
    shape(caps, 40, 52);
    shape(starting, 52, 40);
    Game::ClassMultiplierTable multipliers;
    Game::ClassSkillTable skills;
    require(multipliers.loadFromRows(hp) && multipliers.applyClassExtraRows(extra)
        && skills.loadCapsFromRows(caps) && skills.loadStartingSkillsFromRows(starting)
        && skills.loadClassMetadataFromRows(extra) && skills.loadClassSpellPointMetadataFromRows(hp),
        "Cannot load class input for race validation");
    const Row skillNames = Game::allCanonicalSkillNames();
    require(skillNames.size() >= 39, "Shared skill registry incomplete");
    const std::map<std::string, int> manaCodes = {{"-", 0}, {"I", 1}, {"P", 2}, {"IP", 3}, {"PI", 3}};
    std::map<size_t, std::string> classNames;
    for (size_t id = 0, activeIndex = 1; id < 53; ++id)
    {
        require(number(metadata[id + 1][0]) == static_cast<int>(id), "Class source ID changed");
        if (id == 49 || id == 52)
        {
            require(metadata[id + 1][1] == "0" && metadata[id + 1][3] == "n/u"
                && selection[0][id + 1] == "n/u", "Unused class selection slot changed");
            continue;
        }
        const std::string name = Game::canonicalClassName(metadata[id + 1][3]);
        const Game::ClassMultiplierEntry *pClass = multipliers.get(name);
        require(extra[activeIndex] == metadata[id + 1] && pClass && pClass->classId == id
            && skills.classIdForName(name) == id && Game::canonicalClassName(selection[0][id + 1]) == name
            && Game::canonicalClassName(sourceHp[id + 1][0]) == name
            && Game::canonicalClassName(sourceCaps[0][id + 1]) == name, "Stale class identity input");
        require(pClass->baseHealth == number(sourceHp[id + 1][1])
            && pClass->healthPerLevel == number(sourceHp[id + 1][2])
            && pClass->baseMana == number(sourceHp[id + 1][3])
            && pClass->manaPerLevel == number(sourceHp[id + 1][4])
            && manaCodes.contains(sourceHp[id + 1][5])
            && static_cast<int>(pClass->manaMode) == manaCodes.at(sourceHp[id + 1][5]), "Stale class HP/SP input");
        for (size_t skill = 0; skill < 39; ++skill)
        {
            require(std::set<std::string>{"-", "B", "E", "M", "G"}.contains(sourceCaps[skill + 1][id + 1])
                && std::set<std::string>{"-", "C", "F"}.contains(sourceStarting[skill + 1][id / 2 + 1]),
                "Invalid source class skill token");
            require(Game::canonicalSkillName(sourceCaps[skill + 1][0]) == skillNames[skill]
                && Game::canonicalSkillName(sourceStarting[skill + 1][0]) == skillNames[skill]
                && caps[skill + 1][activeIndex] == sourceCaps[skill + 1][id + 1]
                && starting[activeIndex][skill + 1] == sourceStarting[skill + 1][id / 2 + 1],
                "Stale class skill input");
        }
        classNames.emplace(id, name);
        ++activeIndex;
    }
    for (size_t family = 0; family < FamilyNames.size(); ++family)
    {
        const Game::ClassMultiplierEntry *pClass = multipliers.get(FamilyNames[family]);
        require(pClass && pClass->classKind == family + 1, "Race skill family index changed");
    }

    Rows stats = sourceStats;
    for (size_t column = 1; column < 19; ++column)
    {
        require(sourceStats[0][column] == (column <= 11 ? StatHeaders[column - 1] : "n/u"),
            "Race stat ID/header changed");
        for (size_t stat = 0; stat < 7; ++stat)
        {
            require(sourceStats[stat * 2 + 1][0] == StatNames[stat]
                && sourceStats[stat * 2 + 2][0] == "+ add", "Race stat row order changed");
            const Row baseMaximum = split(sourceStats[stat * 2 + 1][column]);
            Row delta = split(sourceStats[stat * 2 + 2][column]);
            require(baseMaximum.size() == 2 && (delta.size() == 1 || delta.size() == 2), "Invalid race stat pair");
            require(number(baseMaximum[0]) > 0 && number(baseMaximum[1]) >= number(baseMaximum[0]),
                "Invalid race stat range");
            if (delta.size() == 1)
            {
                delta.push_back("1"); // DataTables.StartingStats explicitly defaults the denominator to one.
            }
            require(number(delta[0]) > 0 && number(delta[1]) > 0, "Invalid race stat step/cost");
            stats[stat * 2 + 2][column] = delta[0] + "/" + delta[1];
        }
    }
    for (Row &row : stats)
    {
        row.resize(12); // Seven n/u columns have no active race identity.
    }
    Game::RaceStartingStatsTable statCheck;
    Game::MergedClassStartingStatTable mergedStatCheck;
    require(statCheck.loadFromRows(stats) && mergedStatCheck.loadFromRows(stats)
        && mergedStatCheck.entries().size() == 77 && !statCheck.get("n/u"), "Race stat typed load failed");
    for (const Game::MergedClassStartingStatEntry &entry : mergedStatCheck.entries())
    {
        require(entry.raceId && *entry.raceId < 11, "Race stat ID lost");
        const size_t race = *entry.raceId;
        const Game::RaceStartingStatsTable::Entry *pRace = statCheck.get(RaceNames[race]);
        const size_t stat = std::find(StatNames.begin(), StatNames.end(), entry.statName) - StatNames.begin();
        require(pRace && stat < 7 && pRace->stats[stat] == static_cast<int>(entry.baseValue)
            && pRace->maximumStats[stat] == static_cast<int>(entry.maxValue)
            && pRace->addSteps[stat] == static_cast<int>(entry.addNumerator)
            && pRace->droppedSteps[stat] == static_cast<int>(entry.addDenominator), "Race stat interpretation changed");
    }

    std::vector<Rule> rules;
    YAML::Node ruleDocument;
    ruleDocument["rules"] = YAML::Node(YAML::NodeType::Sequence);
    size_t blankCells = 0;
    size_t numericZeroRules = 0;
    const std::map<std::string, int> masteryCodes = {{"0", 0}, {"B", 1}, {"E", 2}, {"M", 3}, {"G", 4}};
    const std::map<std::string, int> exceptions = {{"I", -1}, {"P", -2}, {"IP", -3}, {"S", -4}, {"W", -5}};
    for (size_t column = 1; column < 188; ++column)
    {
        const size_t race = (column - 1) % 11;
        const size_t kind = (column - 1) / 11;
        const std::string expected = kind == 0 ? RaceNames[race] + "(" + std::to_string(race) + ")"
            : RaceNames[race] + " - " + FamilyNames[kind - 1];
        require(sourceSkills[0][column] == expected, "Race skill target ID/order changed");
        for (size_t skill = 0; skill < 39; ++skill)
        {
            require(Game::canonicalSkillName(sourceSkills[skill + 1][0]) == skillNames[skill],
                "Race skill row order changed");
            const std::string &raw = sourceSkills[skill + 1][column];
            if (raw.empty() || raw == "-")
            {
                blankCells += raw.empty() ? 1 : 0;
                continue;
            }
            const Row parts = split(raw);
            require(parts.size() == 3 && masteryCodes.contains(parts[0]), "Invalid race skill token");
            const int exception = exceptions.contains(parts[2]) ? exceptions.at(parts[2]) : number(parts[2], 16);
            const Rule rule = {race, kind, skill, masteryCodes.at(parts[0]), number(parts[1], 4), exception, raw};
            numericZeroRules += parts[0] == "0" ? 1 : 0;
            rules.push_back(rule);
            YAML::Node node;
            node["race"] = RaceNames[race];
            node["race_id"] = race;
            node["skill"] = skillNames[skill];
            node["raw_token"] = raw;
            if (kind != 0)
            {
                node["class_kind"] = std::to_string(kind);
            }
            ruleDocument["rules"].push_back(node);
        }
    }
    const std::string ruleYaml = YAML::Dump(ruleDocument) + "\n";
    Game::MergedRaceSkillTable ruleCheck;
    std::string error;
    require(ruleCheck.loadFromYaml(ruleYaml, error) && ruleCheck.overrides().size() == rules.size()
        && skills.applyRaceSkillOverrides(ruleCheck), "Race skill YAML load failed: " + error);
    for (size_t index = 0; index < rules.size(); ++index)
    {
        const Rule &rule = rules[index];
        const Game::MergedRaceSkillOverride &entry = ruleCheck.overrides()[index];
        require(entry.raceId == rule.race && entry.skillName == skillNames[rule.skill]
            && entry.classKind == (rule.kind == 0 ? "" : std::to_string(rule.kind))
            && entry.minMastery == rule.minimum && entry.add == rule.add && entry.exceptionCode == rule.exception
            && entry.rawToken == rule.raw, "Race skill rule changed");
    }

    size_t effectiveChecks = 0;
    size_t suppressedDefaults = 0;
    for (const auto &[id, name] : classNames)
    {
        const size_t kind = number(metadata[id + 1][1]);
        const int mana = manaCodes.at(sourceHp[id + 1][5]);
        for (size_t race = 0; race < 11; ++race)
        {
            std::set<std::string> defaults;
            for (size_t skill = 0; skill < 39; ++skill)
            {
                const int baseCap = static_cast<int>(Game::parseSkillMasteryToken(sourceCaps[skill + 1][id + 1]));
                int minimum = 0;
                int add = 0;
                for (const Rule &rule : rules)
                {
                    if (rule.race == race && rule.skill == skill && applies(rule, kind, mana))
                    {
                        minimum = std::max(minimum, rule.minimum);
                        add = std::max(add, rule.add);
                    }
                }
                const int cap = std::clamp(std::max(baseCap, minimum) + (baseCap > 0 ? add : 0), 0, 4);
                const std::string &initial = sourceStarting[skill + 1][id / 2 + 1];
                Game::StartingSkillAvailability available = Game::StartingSkillAvailability::None;
                if (cap > 0)
                {
                    available = initial != "-" ? Game::parseStartingSkillAvailabilityToken(initial)
                        : (baseCap == 0 && minimum > 0 ? Game::StartingSkillAvailability::CanLearn
                            : Game::StartingSkillAvailability::None);
                }
                require(static_cast<int>(skills.getEffectiveCap(name, static_cast<uint32_t>(race), skillNames[skill]))
                        == cap
                    && skills.getEffectiveStartingSkillAvailability(
                        name, static_cast<uint32_t>(race), skillNames[skill]) == available,
                    "Effective race/class skill behavior changed");
                if (available == Game::StartingSkillAvailability::HasByDefault)
                {
                    defaults.insert(skillNames[skill]);
                }
                suppressedDefaults += initial == "F" && cap == 0 ? 1 : 0;
                ++effectiveChecks;
            }
            const std::vector<Game::CharacterSkill> actual =
                skills.getDefaultSkillsForCharacter(name, static_cast<uint32_t>(race));
            require(actual.size() == defaults.size(), "Effective starting defaults changed");
            for (const Game::CharacterSkill &skill : actual)
            {
                require(defaults.erase(skill.name) == 1 && skill.level == 1
                    && skill.mastery == Game::SkillMastery::Normal, "Effective default skill value changed");
            }
        }
    }

    YAML::Node selectionDocument;
    selectionDocument["race_class_availability"] = YAML::Node(YAML::NodeType::Sequence);
    selectionDocument["new_game_continents"] = YAML::Node(YAML::NodeType::Sequence);
    size_t selectedPairs = 0;
    for (size_t race = 0; race < 11; ++race)
    {
        const Row &row = selection[race + 1];
        require(row.size() == 54 && number(row[0], 10) == static_cast<int>(race), "Selection race row changed");
        YAML::Node node;
        node["race"] = RaceNames[race];
        node["race_id"] = race;
        node["classes"] = YAML::Node(YAML::NodeType::Sequence);
        for (size_t id = 0; id < 53; ++id)
        {
            require(row[id + 1] == "x" || row[id + 1] == "-", "Invalid selection marker");
            if (row[id + 1] == "x")
            {
                require(classNames.contains(id), "Inactive class selected");
                node["classes"].push_back(classNames.at(id));
                ++selectedPairs;
            }
        }
        selectionDocument["race_class_availability"].push_back(node);
    }
    for (size_t index = 0; index < 4; ++index)
    {
        const Row &name = selection[13 + index * 3];
        const Row &classes = selection[14 + index * 3];
        const Row &races = selection[15 + index * 3];
        require(name.size() >= 2 && name[0] == ContinentNames[index] && name[1] == "Portraits exceptions:"
            && continents[index + 1].size() >= 2 && number(continents[index + 1][0]) == static_cast<int>(index + 1)
            && continents[index + 1][1] == name[0], "Selection continent identity changed");
        require(classes.size() > 2 && classes[0].empty() && classes[1] == "Available classes:"
            && races.size() > 2 && races[0].empty() && races[1] == "Available races:", "Invalid selection lists");
        YAML::Node node;
        node["id"] = index + 1;
        node["key"] = ContinentKeys[index];
        node["name"] = name[0];
        node["portrait_exceptions"] = Row(name.begin() + 2, name.end());
        std::set<int> seenClasses;
        std::set<int> seenRaces;
        for (size_t column = 2; column < classes.size(); ++column)
        {
            const int id = number(classes[column], 52);
            require(classNames.contains(id) && seenClasses.insert(id).second, "Invalid continent class ID");
            node["available_class_ids"].push_back(id);
        }
        for (size_t column = 2; column < races.size(); ++column)
        {
            const int id = number(races[column], 10);
            require(seenRaces.insert(id).second, "Duplicate continent race ID");
            node["available_race_ids"].push_back(id);
        }
        selectionDocument["new_game_continents"].push_back(node);
    }
    const std::string selectionYaml = YAML::Dump(selectionDocument) + "\n";
    Game::MergedCharacterSelectionTable selectionCheck;
    require(selectionCheck.loadFromYaml(selectionYaml, error) && selectionCheck.continents().size() == 4,
        "Selection YAML load failed: " + error);
    for (size_t race = 0; race < 11; ++race)
    {
        const std::vector<std::string> *pAllowed = selectionCheck.allowedClassesForRaceId(static_cast<uint32_t>(race));
        Row expected;
        for (const auto &[id, name] : classNames)
        {
            if (selection[race + 1][id + 1] == "x")
            {
                expected.push_back(name);
            }
        }
        require(pAllowed && *pAllowed == expected
            && selectionCheck.raceNameForId(static_cast<uint32_t>(race)) == RaceNames[race],
            "Selection classes changed");
    }
    for (size_t index = 0; index < 4; ++index)
    {
        const Game::MergedCharacterSelectionContinent &entry = selectionCheck.continents()[index];
        require(entry.id == index + 1 && entry.key == ContinentKeys[index] && entry.name == ContinentNames[index]
            && entry.portraitExceptions == Row(selection[13 + index * 3].begin() + 2, selection[13 + index * 3].end()),
            "Selection continent metadata changed");
        std::vector<uint32_t> expectedClasses;
        std::vector<uint32_t> expectedRaces;
        const Row &classRow = selection[14 + index * 3];
        const Row &raceRow = selection[15 + index * 3];
        for (size_t column = 2; column < classRow.size(); ++column)
        {
            expectedClasses.push_back(number(classRow[column]));
        }
        for (size_t column = 2; column < raceRow.size(); ++column)
        {
            expectedRaces.push_back(number(raceRow[column]));
        }
        require(entry.availableClassIds == expectedClasses && entry.availableRaceIds == expectedRaces,
            "Continent selection IDs changed");
    }
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-race-and-selection\",\"runtime_ready\":false,"
        << "\"races\":11,\"stat_entries\":77,\"inactive_race_ids\":[11,12,13,14,15,16,17],"
        << "\"race_skill_rules\":" << rules.size() << ",\"blank_skill_cells\":" << blankCells
        << ",\"numeric_zero_rules_requiring_legacy_parser_review\":" << numericZeroRules
        << ",\"race_class_skill_checks\":" << effectiveChecks << ",\"suppressed_default_assignments\":"
        << suppressedDefaults << ",\"selected_race_class_pairs\":" << selectedPairs
        << ",\"continents\":4,\"typed_validation\":true,\"legacy_lua_parity\":false,"
        << "\"portraits_validated\":false,\"character_creation_ready\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh race output");
    try
    {
        writeText(output / "engine/data_tables/class_starting_stats.txt", serializeTextRows(stats));
        writeText(output / "engine/data_tables/race_skills.yml", ruleYaml);
        writeText(output / "engine/data_tables/character_selection.yml", selectionYaml);
        writeText(output / "race_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
