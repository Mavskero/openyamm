#include "ClassResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/gameplay/GameMechanics.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
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
constexpr size_t ClassSlots = 53;
constexpr size_t SkillSlots = 39;

// Source columns are positional pairs of class IDs. These labels are comments,
// not the Class Extra Kind IDs or reliable names of every class using the column.
const Row StartingHeader = {"", "Archer", "WarriorMage", "Cleric", "Priest", "DarkElf", "Dragon", "Druid",
    "GreatDruid", "Knight", "Cavalier", "Minotaur", "Monk", "Initiate", "Paladin", "Crusader", "Ranger",
    "Hunter", "Thief", "Rogue", "Troll", "Vampire", "Sorcerer", "Necromancer", "n/u", "Peasant",
    "High Priest / Master Wizard", "n/u", "n/u", "n/u", "n/u"};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t byteValue(const std::string &text)
{
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value <= 255,
        "Invalid class byte value: " + text);
    return value;
}

Rows readTable(const std::filesystem::path &root, const std::string &name, size_t rowCount, size_t columns)
{
    const std::filesystem::path path = std::filesystem::canonical(root / "Data/Tables" / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Class source escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 1024 * 1024, "Invalid class source file size");
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read class source: " + name);
    size_t glyphs = 0;
    const std::optional<Engine::TextTable> table =
        Engine::TextTable::parseTabSeparated(decodeLegacyText(bytes, false, glyphs));
    require(table.has_value() && glyphs == 0, "Invalid class mechanism table: " + name);
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 == table->getRowCount() && row == Row{""})
        {
            continue;
        }
        require(row.size() == columns, "Wrong class column count: " + name);
        rows.push_back(row);
    }
    require(rows.size() == rowCount, "Unsupported class row count: " + name);
    return rows;
}

Rows roundTrip(const Rows &rows)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(serializeTextRows(rows));
    require(table.has_value(), "Cannot reload exported class table");
    Rows result;
    for (size_t index = 0; index < rows.size(); ++index)
    {
        result.push_back(table->getRow(index));
    }
    require(result == rows, "Exported class cells changed");
    return result;
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertClassResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Class output must be outside source");
    require(!std::filesystem::exists(output), "Class output already exists");

    const Rows hp = readTable(root, "Class HP SP.txt", ClassSlots + 1, 6);
    const Rows extra = readTable(root, "Class Extra.txt", ClassSlots + 1, 4);
    const Rows caps = readTable(root, "Class Skills.txt", SkillSlots + 1, ClassSlots + 1);
    const Rows starting = readTable(root, "Class Starting Skills.txt", SkillSlots + 1, StartingHeader.size());
    require(hp[0] == Row{"", "HPBase", "HPFactor", "SPBase", "SPFactor", "SPStats"}
        && extra[0] == Row{"#", "Kind", "Step", "Note"} && caps[0][0].empty()
        && starting[0] == StartingHeader, "Unsupported class source headers");
    const std::map<std::string, std::pair<std::string, Game::ClassManaMode>> manaModes = {
        {"-", {"None", Game::ClassManaMode::None}},
        {"I", {"Intellect", Game::ClassManaMode::Intellect}},
        {"P", {"Personality", Game::ClassManaMode::Personality}},
        {"IP", {"Mixed", Game::ClassManaMode::Mixed}},
        {"PI", {"Mixed", Game::ClassManaMode::Mixed}},
    };
    std::vector<size_t> activeIds;
    std::set<std::string> names;
    Rows hpOutput = {hp[0]};
    Rows extraOutput = {extra[0]};
    Rows capsOutput(SkillSlots + 1, Row{caps[0][0]});
    Row skillNames = Game::allCanonicalSkillNames();
    require(skillNames.size() >= SkillSlots, "Shared skill registry is incomplete");
    skillNames.resize(SkillSlots);
    Row startingOutputHeader = {"Class"};
    startingOutputHeader.insert(startingOutputHeader.end(), skillNames.begin(), skillNames.end());
    Rows startingOutput = {startingOutputHeader};
    for (size_t skill = 0; skill < SkillSlots; ++skill)
    {
        require(Game::canonicalSkillName(caps[skill + 1][0]) == skillNames[skill]
            && Game::canonicalSkillName(starting[skill + 1][0]) == skillNames[skill], "Class skill order changed");
        capsOutput[skill + 1][0] = caps[skill + 1][0];
        for (size_t column = 1; column < caps[skill + 1].size(); ++column)
        {
            require(std::set<std::string>{"-", "B", "E", "M", "G"}.contains(caps[skill + 1][column]),
                "Invalid class mastery token");
        }
        for (size_t column = 1; column < starting[skill + 1].size(); ++column)
        {
            require(std::set<std::string>{"-", "C", "F"}.contains(starting[skill + 1][column]),
                "Invalid starting skill token");
        }
    }

    for (size_t id = 0; id < ClassSlots; ++id)
    {
        const Row &metadata = extra[id + 1];
        require(byteValue(metadata[0]) == id, "Class Extra ID/order changed");
        const uint32_t kind = byteValue(metadata[1]);
        const uint32_t step = byteValue(metadata[2]);
        for (size_t column = 1; column <= 4; ++column)
        {
            byteValue(hp[id + 1][column]);
        }
        require(manaModes.contains(hp[id + 1][5]), "Unknown source SPStats token");
        if (id == 49 || id == 52)
        {
            require(kind == 0 && step == 0 && metadata[3] == "n/u" && hp[id + 1][0] == "n/u"
                && caps[0][id + 1] == "n/u", "Reserved class slot was activated; profile requires review");
            continue;
        }
        const std::string name = Game::canonicalClassName(metadata[3]);
        require(!name.empty() && name != "n/u" && Game::canonicalClassName(hp[id + 1][0]) == name
            && Game::canonicalClassName(caps[0][id + 1]) == name, "Class ID/name association changed");
        require(kind > 0 && names.insert(name).second, "Missing class kind or duplicate active class");
        activeIds.push_back(id);
        Row multiplier = hp[id + 1];
        multiplier[5] = manaModes.at(multiplier[5]).first;
        hpOutput.push_back(multiplier);
        extraOutput.push_back(metadata);
        capsOutput[0].push_back(caps[0][id + 1]);
        Row initial = {name};
        for (size_t skill = 0; skill < SkillSlots; ++skill)
        {
            capsOutput[skill + 1].push_back(caps[skill + 1][id + 1]);
            // Both MenuChooseCharacter and NPCMercenaries use floor(classId / 2).
            initial.push_back(starting[skill + 1][id / 2 + 1]);
        }
        startingOutput.push_back(initial);
    }

    Game::ClassMultiplierTable multipliers;
    Game::ClassSkillTable skills;
    require(multipliers.loadFromRows(roundTrip(hpOutput))
        && multipliers.applyClassExtraRows(roundTrip(extraOutput))
        && multipliers.entries().size() == activeIds.size()
        && skills.loadCapsFromRows(roundTrip(capsOutput))
        && skills.loadStartingSkillsFromRows(roundTrip(startingOutput))
        && skills.loadClassMetadataFromRows(roundTrip(extraOutput))
        && skills.loadClassSpellPointMetadataFromRows(roundTrip(hpOutput)), "Typed class table load failed");

    // The shared promotion traversal uses legacy edges only when no metadata
    // successor exists. Reject a stale profile that could make those edges cycle
    // or cross class families before invoking that traversal.
    for (const auto &[name, entry] : multipliers.entries())
    {
        const bool hasNext = std::any_of(multipliers.entries().begin(), multipliers.entries().end(),
            [&](const auto &candidate)
            {
                return candidate.second.classKind == entry.classKind
                    && candidate.second.promotionStep == entry.promotionStep + 1;
            });
        if (!hasNext)
        {
            for (const std::string &next : Game::promotionClassNames(name))
            {
                const Game::ClassMultiplierEntry *pNext = multipliers.get(next);
                require(pNext && pNext->classKind == entry.classKind && pNext->promotionStep > entry.promotionStep,
                    "Unsafe legacy promotion edge in class profile");
            }
        }
    }

    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-class-tables\",\"runtime_ready\":false,"
        << "\"source_class_slots\":53,\"active_classes\":" << activeIds.size()
        << ",\"inactive_class_ids\":[49,52],\"skills\":39,\"starting_source_groups\":30,"
        << "\"unused_starting_group_indices\":[26,27,28,29],\"starting_group_by_class\":[";
    size_t defaultCount = 0;
    std::vector<Row> startingWithoutCaps;
    for (size_t index = 0; index < activeIds.size(); ++index)
    {
        const size_t id = activeIds[index];
        const std::string name = Game::canonicalClassName(extra[id + 1][3]);
        const Game::ClassMultiplierEntry *pEntry = multipliers.get(name);
        require(pEntry && pEntry->classId == id && pEntry->classKind == byteValue(extra[id + 1][1])
            && pEntry->promotionStep == byteValue(extra[id + 1][2])
            && pEntry->baseHealth == byteValue(hp[id + 1][1])
            && pEntry->healthPerLevel == byteValue(hp[id + 1][2])
            && pEntry->baseMana == byteValue(hp[id + 1][3]) && pEntry->manaPerLevel == byteValue(hp[id + 1][4])
            && pEntry->manaMode == manaModes.at(hp[id + 1][5]).second
            && skills.classIdForName(name) == id && skills.classNameForId(static_cast<uint32_t>(id)) == name,
            "Typed class identity or resource values changed");
        manifest << (index == 0 ? "" : ",") << "{\"class_id\":" << id << ",\"group_index\":" << id / 2 << "}";
        std::set<std::string> expectedDefaults;
        for (size_t skill = 0; skill < SkillSlots; ++skill)
        {
            const std::string &skillName = skillNames[skill];
            const Game::SkillMastery cap = Game::parseSkillMasteryToken(caps[skill + 1][id + 1]);
            const std::string &initial = starting[skill + 1][id / 2 + 1];
            require(skills.getClassCap(name, skillName) == cap
                && skills.getStartingSkillAvailability(name, skillName)
                    == Game::parseStartingSkillAvailabilityToken(initial), "Class skill cell changed");
            if (initial == "F")
            {
                expectedDefaults.insert(skillName);
            }
            if (initial != "-" && cap == Game::SkillMastery::None)
            {
                startingWithoutCaps.push_back({std::to_string(id), skillName, initial});
            }
            Game::SkillMastery highest = Game::SkillMastery::None;
            for (size_t candidate : activeIds)
            {
                if (extra[candidate + 1][1] == extra[id + 1][1]
                    && byteValue(extra[candidate + 1][2]) > pEntry->promotionStep)
                {
                    highest = std::max(highest, Game::parseSkillMasteryToken(caps[skill + 1][candidate + 1]));
                }
            }
            require(skills.getHighestPromotionCap(name, skillName) == highest, "Promotion cap association changed");
        }
        const std::vector<Game::CharacterSkill> defaults = skills.getDefaultSkillsForClass(name);
        require(defaults.size() == expectedDefaults.size(), "Default skill coverage changed");
        for (const Game::CharacterSkill &skill : defaults)
        {
            require(expectedDefaults.erase(skill.name) == 1 && skill.level == 1
                && skill.mastery == Game::SkillMastery::Normal, "Default skill level/mastery changed");
        }
        defaultCount += defaults.size();
        // Probe the production HP/SP formulas without equipment or skills. The
        // fixed attributes have known bonuses: endurance 14 -> 0, intellect
        // 25 -> 5, personality 30 -> 6. Pass this table explicitly, without globals.
        Game::Character character = {};
        character.className = name;
        character.endurance = 14;
        character.intellect = 25;
        character.personality = 30;
        const std::string &sourceMode = hp[id + 1][5];
        const int manaBonus = sourceMode == "I" ? 5 : (sourceMode == "P" ? 6 : 11);
        for (uint32_t level : {1u, 10u, 25u})
        {
            character.level = level;
            const int expectedHp = pEntry->baseHealth + pEntry->healthPerLevel * static_cast<int>(level);
            const int expectedSp = sourceMode == "-" ? 0
                : pEntry->baseMana + pEntry->manaPerLevel * (static_cast<int>(level) + manaBonus);
            require(Game::GameMechanics::calculateBaseCharacterMaxHealth(character, &multipliers) == expectedHp
                && Game::GameMechanics::calculateBaseCharacterMaxSpellPoints(character, &multipliers) == expectedSp,
                "Class HP/SP progression changed");
        }
        for (size_t candidate : activeIds)
        {
            const bool atLeast = id == candidate || (extra[id + 1][1] == extra[candidate + 1][1]
                && pEntry->promotionStep >= byteValue(extra[candidate + 1][2]));
            require(skills.classIdIsAtLeast(static_cast<uint32_t>(id), static_cast<uint32_t>(candidate)) == atLeast,
                "Class promotion family changed");
        }
    }
    require(!skills.classNameForId(49) && !skills.classNameForId(52) && multipliers.get("n/u") == nullptr,
        "Unused class became active");
    manifest << "],\"default_skill_assignments\":" << defaultCount << ",\"starting_without_class_cap\":[";
    for (size_t index = 0; index < startingWithoutCaps.size(); ++index)
    {
        const Row &entry = startingWithoutCaps[index];
        manifest << (index == 0 ? "" : ",") << "{\"class_id\":" << entry[0]
            << ",\"skill\":" << std::quoted(entry[1]) << ",\"availability\":" << std::quoted(entry[2]) << "}";
    }
    manifest << "],\"typed_validation\":true,\"promotion_validation\":true,\"hp_sp_progression_validated\":true,"
        << "\"race_overrides_ready\":false,\"localized_class_ui_ready\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh class output");
    try
    {
        writeText(output / "engine/data_tables/class_multipliers.txt", serializeTextRows(hpOutput));
        writeText(output / "engine/data_tables/class_extra.txt", serializeTextRows(extraOutput));
        writeText(output / "engine/data_tables/class_skills.txt", serializeTextRows(capsOutput));
        writeText(output / "engine/data_tables/class_starting_skills.txt", serializeTextRows(startingOutput));
        writeText(output / "class_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
