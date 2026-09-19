#include "BolsterResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "engine/scripting/LuaStateOwner.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/MonsterTable.h"

#include <algorithm>
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

int32_t number(const std::string &text, int32_t minimum = 0, int32_t maximum = 65535)
{
    const std::string value = trim(text);
    int32_t result = 0;
    const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size()
        && result >= minimum && result <= maximum, "Invalid bolster numeric field: " + text);
    return result;
}

bool marker(const std::string &text)
{
    require(text.empty() || text == "-" || text == "x", "Unsupported bolster marker");
    return text == "x";
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Bolster input escapes root");
    return path;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid bolster quoting");
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

Rows parseSource(const std::string &text)
{
    Rows rows;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        Row row;
        size_t start = 0;
        while (true)
        {
            const size_t end = line.find('\t', start);
            row.push_back(line.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid bolster dependency size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read bolster dependency");
    if (legacy)
    {
        size_t glyphs = 0;
        text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
    }
    return legacy ? parseSource(text) : parse(text);
}

std::optional<uint32_t> optionalNumber(const std::string &text)
{
    return trim(text).empty() ? std::nullopt : std::optional<uint32_t>(number(text));
}

Row tokens(const std::string &text)
{
    Row result;
    if (text.empty())
    {
        return result;
    }
    require(text.back() != ',', "Empty bolster taxonomy token");
    std::istringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        token = trim(token);
        require(!token.empty(), "Empty bolster taxonomy token");
        result.push_back(token);
    }
    return result;
}

const std::set<std::string> Types = {"0", "Unknown", "Undead", "Dragon", "Swimmer", "Immobile", "Peasant",
    "NoArena", "Ogre", "Elemental", "Demon", "Titan", "Elf", "Goblin", "Dwarf", "Human", "DarkElf", "Lizardman",
    "Minotaur", "Troll", "Creature", "Construct"};
const std::map<std::string, Game::MonsterKind> KindFlags = {{"Undead", Game::MonsterKind::Undead},
    {"Dragon", Game::MonsterKind::Dragon}, {"Swimmer", Game::MonsterKind::Swimmer},
    {"Immobile", Game::MonsterKind::Immobile}, {"Peasant", Game::MonsterKind::Peasant},
    {"NoArena", Game::MonsterKind::NoArena}, {"Ogre", Game::MonsterKind::Ogre},
    {"Elemental", Game::MonsterKind::Elemental}, {"Demon", Game::MonsterKind::Demon},
    {"Titan", Game::MonsterKind::Titan}, {"Elf", Game::MonsterKind::Elf}};
}

void convertBolsterResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
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
            "Bolster output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Bolster output already exists");
    const auto sourceRows = [&root](const std::string &name) { return readRows(root, "Data/Tables/" + name, true); };
    const auto assetRows = [&assets](const std::string &name)
    {
        return readRows(assets, "engine/data_tables/" + name, false);
    };
    const Rows formulas = sourceRows("Bolster - formulas.txt");
    const Rows mapRows = sourceRows("Bolster - maps.txt");
    const Rows monsterRows = sourceRows("Bolster - monsters.txt");
    require(formulas.size() > 1 && formulas[0] == Row{"Target", "Stat", "Formula", "Notes"},
        "Unsupported bolster formula header");
    require(mapRows.size() > 1 && mapRows[0] == Row{"#", "Note", "Continent", "Bolster kind", "Spells", "Summons",
        "Weather", "Bolster extra", "Profs max rarity", "Custom sky"}, "Unsupported bolster map header");
    require(monsterRows.size() > 1 && monsterRows[0] == Row{"#", "Note", "Type", "ExtraType", "Creed", "Gender",
        "Style", "Pref magic", "No bounty hunt", "New ranged attacks", "New spells", "Size affects HP",
        "Replicate", "New summons", "Summon Id", "Extra points", "Max HP Boost (%)"},
        "Unsupported bolster monster header");
    MapStats maps;
    MonsterTable monsters;
    require(maps.loadFromRows(assetRows("map_stats.txt"))
        && monsters.loadEntriesFromRows(assetRows("monster_descriptors.txt"))
        && monsters.loadStatsFromRows(assetRows("monster_data.txt")), "Invalid bolster registry dependency");
    MergedBolsterMapTable mapTable;
    MergedBolsterMonsterTable monsterTable;
    MergedBolsterFormulaTable formulaTable;
    require(mapTable.loadFromRows(mapRows) && monsterTable.loadFromRows(monsterRows)
        && formulaTable.loadFromRows(formulas), "Bolster typed loading failed");
    const Rows continents = sourceRows("Continent settings.txt");
    std::set<uint32_t> continentIds;
    for (size_t index = 1; index < continents.size(); ++index)
    {
        require(!continents[index].empty(), "Empty continent definition");
        continentIds.insert(number(continents[index][0], 1));
    }
    const std::set<std::string> mapKinds = {"0", "NoBolster", "OriginalStats", "LowerToEqual", "AllToEqual"};
    size_t professionMaps = 0;
    for (size_t index = 1; index < mapRows.size(); ++index)
    {
        const Row &row = mapRows[index];
        require(row.size() == 10 && number(row[0]) == index - 1, "Invalid/duplicate bolster map ID");
        const uint32_t id = index - 1;
        const MergedBolsterMapEntry *pEntry = mapTable.findById(id);
        const std::optional<uint32_t> continent = optionalNumber(row[2]);
        require(id == 0 || (maps.findById(id) && continent && continentIds.contains(*continent)),
            "Missing bolster map or continent");
        require(pEntry && pEntry->note == trim(row[1]) && pEntry->continent == continent.value_or(0)
            && mapKinds.contains(row[3]) && pEntry->bolsterKind == row[3]
            && pEntry->spells == marker(row[4]) && pEntry->summons == marker(row[5])
            && pEntry->weather == marker(row[6]) && pEntry->bolsterExtra == number(row[7])
            && pEntry->professionMaxRarity == optionalNumber(row[8]) && pEntry->customSky == trim(row[9]),
            "Bolster map semantics changed");
        professionMaps += pEntry->professionMaxRarity.value_or(0) > 0;
    }
    const std::set<std::string> creeds = {"0", "Neutral", "Light", "Dark", "Peasant"};
    const std::set<std::string> styles = {"0", "Strength", "Endurance", "Speed", "Magic", "Wimpy"};
    const std::set<std::string> magic = {"0", "Any", "Fire", "Air", "Water", "Earth", "Spirit", "Mind", "Body",
        "Light", "Dark", "Self", "Elemental"};
    std::map<uint32_t, uint32_t> expectedKindFlags;
    std::set<std::string> typesWithoutKindFlag;
    size_t summonReferences = 0;
    for (size_t index = 1; index < monsterRows.size(); ++index)
    {
        const Row &row = monsterRows[index];
        require(row.size() == 17 && number(row[0], 0, 32767) == index - 1,
            "Invalid/duplicate bolster monster ID");
        const uint32_t id = index - 1;
        const MergedBolsterMonsterEntry *pEntry = monsterTable.findById(id);
        require(id == 0 || monsters.findStatsById(id), "Missing bolster monster");
        require(pEntry && pEntry->note == trim(row[1]) && Types.contains(row[2]) && pEntry->type == row[2]
            && pEntry->extraTypes == tokens(row[3]) && creeds.contains(row[4]) && pEntry->creed == row[4]
            && (row[5].empty() || row[5] == "M" || row[5] == "F") && pEntry->gender == row[5]
            && styles.contains(row[6]) && pEntry->style == row[6]
            && magic.contains(row[7]) && pEntry->preferredMagic == row[7]
            && pEntry->noBountyHunt == marker(row[8]) && pEntry->newRangedAttacks == marker(row[9])
            && pEntry->newSpells == marker(row[10]) && pEntry->sizeAffectsHp == marker(row[11])
            && pEntry->replicate == marker(row[12]) && pEntry->newSummons == marker(row[13])
            && pEntry->summonId == optionalNumber(row[14]) && pEntry->extraPoints == optionalNumber(row[15])
            && pEntry->maxHpBoostPercent == optionalNumber(row[16]), "Bolster monster semantics changed");
        if (pEntry->summonId.value_or(0) != 0)
        {
            require(*pEntry->summonId <= 32767 && monsters.findStatsById(*pEntry->summonId),
                "Missing bolster summon monster");
            ++summonReferences;
        }
        Row allTypes = pEntry->extraTypes;
        allTypes.push_back(pEntry->type);
        uint32_t flags = 0;
        for (const std::string &type : allTypes)
        {
            require(Types.contains(type), "Unknown extra monster type");
            const auto found = KindFlags.find(type);
            if (found != KindFlags.end())
            {
                flags |= monsterKindFlag(found->second);
            }
            else if (type != "0" && type != "Unknown")
            {
                typesWithoutKindFlag.insert(type);
            }
        }
        expectedKindFlags[id] = flags;
    }
    require(monsters.applyKindFlagsFromBolsterMonsterTable(monsterTable) && maps.applyMergedBolsterMaps(mapTable),
        "Bolster runtime registry application failed");
    for (const MapStatsEntry &map : maps.getEntries())
    {
        const MergedBolsterMapEntry *pEntry = mapTable.findById(map.id);
        require(pEntry && map.mergedContinentId == pEntry->continent, "Map continent assignment changed");
    }
    std::vector<uint32_t> monstersWithoutSettings;
    for (const auto &[id, stats] : monsters.statsEntries())
    {
        if (!expectedKindFlags.contains(id))
        {
            monstersWithoutSettings.push_back(id);
        }
        else
        {
            require(stats.kindFlags == expectedKindFlags.at(id), "Monster category flags changed");
        }
    }
    std::sort(monstersWithoutSettings.begin(), monstersWithoutSettings.end());
    const std::set<std::string> formulaStats = {"HP", "AC", "MoveSpeed", "MinDamage", "MaxDamage", "SpellSkill",
        "SpellMastery", "AllowNewSpell", "AllowSummons", "AllowReplicate", "PlayerAC"};
    Engine::LuaStateOwner syntaxState;
    require(syntaxState.isValid(), "Cannot create formula syntax checker");
    std::set<std::pair<std::string, std::string>> formulaKeys;
    size_t commentRows = 0;
    for (size_t index = 1; index < formulas.size(); ++index)
    {
        const Row &row = formulas[index];
        require(row.size() == 4, "Invalid bolster formula width");
        if (row[0].empty() && row[1].empty() && row[2].empty())
        {
            ++commentRows;
            continue;
        }
        require((row[0] == "def" || (number(row[0], 1, 10922) * 3 <= monsterRows.size() - 2))
            && formulaStats.contains(row[1]) && !row[2].empty() && row[2].size() <= 8192
            && formulaKeys.emplace(row[0], row[1]).second, "Invalid/duplicate bolster formula identity");
        // Match source chunk construction, but compile only. No libraries are opened and no chunk executes.
        const std::string chunk = row[2].find("return") == std::string::npos ? "return " + row[2] : row[2];
        const int status = luaL_loadbufferx(syntaxState.state(), chunk.data(), chunk.size(), "bolster-formula", "t");
        require(status == LUA_OK, "Invalid bolster Lua formula syntax");
        lua_pop(syntaxState.state(), 1);
        const MergedBolsterFormulaEntry &entry = formulaTable.entries().at(formulaKeys.size() - 1);
        require(entry.target == row[0] && entry.stat == row[1] && entry.formula == trim(row[2])
            && entry.notes == trim(row[3]), "Bolster formula or note changed");
    }
    require(formulaTable.entries().size() == formulaKeys.size() && mapTable.entries().size() == mapRows.size() - 1
        && monsterTable.entries().size() == monsterRows.size() - 1, "Bolster loader lost records");
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"bolster_tables\",\"runtime_ready\":false,\"formulas\":"
        << formulaKeys.size() << ",\"formula_comment_rows\":" << commentRows
        << ",\"map_settings\":" << mapTable.entries().size()
        << ",\"monster_settings\":" << monsterTable.entries().size()
        << ",\"maps_with_random_professions\":" << professionMaps << ",\"summon_references\":" << summonReferences
        << ",\"monster_ids_without_source_settings\":[";
    for (size_t index = 0; index < monstersWithoutSettings.size(); ++index)
    {
        report << (index == 0 ? "" : ",") << monstersWithoutSettings[index];
    }
    report << "],\"source_taxonomy_without_runtime_kind_flag\":[";
    for (const std::string &type : typesWithoutKindFlag)
    {
        report << (type == *typesWithoutKindFlag.begin() ? "" : ",") << '"' << type << '"';
    }
    report << "],\"typed_validation\":true,\"formula_syntax_checked\":true,\"formulas_executed\":false"
        << ",\"map_continents_applied\":true,\"existing_monster_kind_flags_applied\":true"
        << ",\"runtime_uses_imported_formulas\":false,\"original_bolster_parity\":false"
        << ",\"full_gameplay_loop\":false}\n";
    const std::map<std::string, Rows> tables = {{"bolster_formulas.txt", formulas},
        {"bolster_maps.txt", mapRows}, {"bolster_monsters.txt", monsterRows}};
    std::map<std::string, std::string> files;
    for (const auto &[name, rows] : tables)
    {
        const std::string text = serializeTextRows(rows);
        require(parse(text) == rows, "Bolster serialization changed source cells");
        files.emplace("engine/data_tables/" + name, text);
    }
    files.emplace("bolster_conversion.json", report.str());
    require(std::filesystem::create_directory(output), "Cannot create fresh bolster output");
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
