#include "CombatResources.h"
#include "EnchantmentResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/AssetFileSystem.h"
#include "engine/TextTable.h"
#include "game/gameplay/MonsterSpellSupport.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MonsterProjectileTable.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/SpellTable.h"

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
using IndexedRows = std::map<uint32_t, Row>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t integer(const std::string &text, uint32_t limit = 65535)
{
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value <= limit,
        "Invalid combat table integer: " + text);
    return value;
}

std::string lower(std::string text)
{
    for (char &letter : text)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return text;
}

std::vector<uint8_t> readLoose(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Source file escapes root: " + name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size <= 16 * 1024 * 1024, "Combat table exceeds size limit: " + name);
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read combat source: " + name);
    return bytes;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid quoted combat text table");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (!row.empty() && !(row.size() == 1 && row[0].empty()))
        {
            rows.push_back(row);
        }
    }
    require(!rows.empty(), "Empty combat table");
    return rows;
}

IndexedRows indexed(const Rows &rows, size_t columns, const std::string &name)
{
    IndexedRows result;
    for (const Row &row : rows)
    {
        if (row[0].empty() || row[0].find_first_not_of("0123456789") != std::string::npos)
        {
            continue;
        }
        require(row.size() == columns || (name == "spells.txt" && row.size() == 5 && row[2] == "Unused!!!"),
            "Wrong column count in " + name + " at ID " + row[0]);
        require(result.emplace(integer(row[0]), row).second, "Duplicate ID in " + name + ": " + row[0]);
    }
    require(!result.empty(), "No numeric rows in " + name);
    return result;
}

Rows values(const IndexedRows &table)
{
    Rows rows;
    for (const auto &[id, row] : table)
    {
        rows.push_back(row);
    }
    return rows;
}

std::string jsonIds(const std::vector<uint32_t> &ids)
{
    std::string json = "[";
    for (size_t index = 0; index < ids.size(); ++index)
    {
        json += (index == 0 ? "" : ",") + std::to_string(ids[index]);
    }
    return json + "]";
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}

struct MissileBinding
{
    const char *pToken;
    int objectId;
    const char *pObjectName;
    int impactId;
};

// MM8 object identities checked against the owned ObjList. The supplied
// MM8patch's comparison hook at 0x452670 leaves FireAr as missile 2 and Rock as
// missile 6 (Earth), while Dagger resolves to zero. See COMBAT_IMPORT_PROFILE.md.
const std::vector<MissileBinding> MissileBindings = {
    {"Arrow", 545, "Arrow", 0}, {"ArrowF", 550, "FireArrow", 0}, {"FireAr", 550, "FireArrow", 0},
    {"Fire", 510, "Fire", 0}, {"Air", 500, "Air", 0}, {"Water", 515, "Water", 0},
    {"Earth", 505, "Earth", 0}, {"Rock", 505, "Earth", 0}, {"Spirit", 530, "Spirit", 0},
    {"Mind", 525, "Mind", 0}, {"Body", 520, "Body", 0}, {"Light", 535, "Light", 0},
    {"Dark", 540, "Dark", 0}, {"Ener", 555, "Laser", 556},
};
}

void convertCombatResources(const std::filesystem::path &source, const std::filesystem::path &coreAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path core = std::filesystem::canonical(coreAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, core})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && *relative.begin() == "..", "Combat output must be outside both input roots");
    }
    require(!std::filesystem::exists(output), "Combat destination already exists");
    size_t glyphs = 0;
    const std::filesystem::path baseArchive = root / "Data/mmmerge.T.lod";
    const std::filesystem::path localeArchive = root / "Data/10 LocZHCN.EnglishT.lod";
    const bool chinese = std::filesystem::exists(localeArchive);
    const auto archiveTable = [&](const std::string &name, bool localized = false)
    {
        return parse(decodeLegacyText(readLodText(localized ? localeArchive : baseArchive, name), localized, glyphs));
    };
    const auto looseTable = [&](const std::string &name, bool localized = false)
    {
        return parse(decodeLegacyText(readLoose(root, name), localized, glyphs));
    };
    const auto coreTable = [&](const std::string &name)
    {
        const std::vector<uint8_t> bytes = readLoose(core, "engine/data_tables/" + name);
        return parse(std::string(bytes.begin(), bytes.end()));
    };
    const Rows objectRows = coreTable("object_list.txt");
    Game::ObjectTable objects;
    require(objects.loadRows(objectRows), "Core object table failed to load");
    const Rows sourceObjects = looseTable("Data/Tables/ObjList.txt");
    require(sourceObjects.front().size() == 22 && sourceObjects.front()[0] == "Name"
        && sourceObjects.front()[1] == "Id" && sourceObjects.size() == objectRows.size() + 1,
        "Core/source object table schemas or coverage differ");
    for (size_t index = 0; index < objectRows.size(); ++index)
    {
        const Row &a = sourceObjects[index + 1];
        const Row &b = objectRows[index];
        require(a.size() == 22 && b.size() == 13 && a[0] == b[0] && a[1] == b[2]
            && a[2] == b[3] && a[3] == b[4] && a[16] == b[5] && a[17] == b[6]
            && lower(a[15]) == b[1], "Core object table is stale or belongs to a different source");
    }
    const IndexedRows sourceMonsters = indexed(archiveTable("monsters.txt"), 39, "monsters.txt");
    const IndexedRows coreMonsters = indexed(coreTable("monster_data.txt"), 39, "core monsters");
    require(sourceMonsters.size() == coreMonsters.size(), "Core monster coverage differs from source");
    for (const auto &[id, row] : sourceMonsters)
    {
        require(coreMonsters.contains(id)
            && Row(row.begin() + 2, row.end()) == Row(coreMonsters.at(id).begin() + 2, coreMonsters.at(id).end()),
            "Core monster mechanics differ from source");
    }
    Rows missiles;
    for (const MissileBinding &binding : MissileBindings)
    {
        const Game::ObjectEntry *pObject = objects.findByObjectId(binding.objectId);
        require(pObject && pObject->internalName == binding.pObjectName
            && pObject->speed > 0 && (pObject->flags & 0x54) == 0x54,
            std::string("Unsupported MM8 projectile object profile: ") + binding.pToken);
        if (binding.impactId != 0)
        {
            const Game::ObjectEntry *pImpact = objects.findByObjectId(binding.impactId);
            require(pImpact && pImpact->internalName == "explosion", "Missing projectile impact object");
        }
        missiles.push_back({binding.pToken, std::to_string(binding.objectId), std::to_string(binding.impactId)});
    }
    Game::MonsterProjectileTable projectileCheck;
    require(projectileCheck.loadFromRows(missiles), "Projectile mapping failed typed validation");
    std::vector<uint32_t> daggerMonsterIds;
    for (const auto &[id, row] : sourceMonsters)
    {
        for (size_t column : {size_t(19), size_t(23)})
        {
            const std::string token = lower(row[column]);
            if (token == "dagger")
            {
                daggerMonsterIds.push_back(id);
            }
            else
            {
                require(token.empty() || token == "0" || projectileCheck.findByToken(token),
                    "Unrecognized monster missile token: " + row[column]);
            }
        }
    }

    const IndexedRows spellSource = indexed(archiveTable("spells.txt"), 11, "spells.txt");
    const IndexedRows spellLocale = chinese
        ? indexed(archiveTable("spells.txt", true), 11, "localized spells") : IndexedRows{};
    const Rows spellStats = looseTable("Data/Tables/Spells2.txt");
    require(spellStats.front() == Row{"Spell", "SpellPointsNormal", "SpellPointsExpert", "SpellPointsMaster",
        "SpellPointsGM", "DelayNormal", "DelayExpert", "DelayMaster", "DelayGM", "DamageAdd", "DamageDiceSides"},
        "Unsupported Spells2 schema");
    require(spellSource.size() == 132 && spellStats.size() == 134, "Expected MM8's 132 fixed spell slots");
    Rows spells;
    std::vector<uint32_t> unusedSpellIds;
    std::vector<uint32_t> spellSlotsWithoutObjects;
    size_t translatedSpells = 0;
    size_t preservedSpellCells = 0;
    for (uint32_t id = 1; id <= 132; ++id)
    {
        require(spellSource.contains(id), "Noncontiguous MM8 spell slot IDs");
        Row row = spellSource.at(id);
        const Row &stats = spellStats[id + 1];
        require(stats.size() == 11 && row[2] == stats[0], "Spells2 slot/name association changed");
        for (size_t column = 1; column < stats.size(); ++column)
        {
            integer(stats[column], 32767);
        }
        if (row[2] == "Unused!!!")
        {
            unusedSpellIds.push_back(id);
            continue;
        }
        const std::string baseName = row[2];
        if (spellLocale.contains(id))
        {
            const Row &translation = spellLocale.at(id);
            for (size_t column : {size_t(1), size_t(3), size_t(10)})
            {
                preservedSpellCells += row[column] != translation[column] ? 1 : 0;
            }
            for (size_t column : {size_t(2), size_t(4), size_t(5), size_t(6), size_t(7), size_t(8), size_t(9)})
            {
                row[column] = translation[column];
            }
            ++translatedSpells;
        }
        row.insert(row.end(), stats.begin() + 1, stats.begin() + 9);
        // Verified fixed slot metadata in the supplied mm8.exe (data reads at
        // 0x4F4648 and 0x4FE12A), not guessed from translated spell names.
        const uint32_t objectId = 1000 + ((id - 1) / 11) * 1000 + ((id - 1) % 11) * 10;
        row.push_back(std::to_string(objectId + 9000));
        row.push_back(std::to_string(objectId));
        const Game::ObjectEntry *pImpact = objects.findByObjectId(objectId + 1);
        require(!pImpact || pImpact->internalName == "explosion", "Unexpected spell impact object layout");
        row.push_back(std::to_string(pImpact ? objectId + 1 : 0));
        row.push_back(stats[9]);
        row.push_back(stats[10]);
        row.push_back(baseName);
        if (!objects.findByObjectId(objectId))
        {
            // Many buffs and utility spells have reserved slots with no world
            // object. Preserve their fixed identity; never create fake objects.
            spellSlotsWithoutObjects.push_back(id);
        }
        spells.push_back(row);
    }
    Game::SpellTable spellCheck;
    require(spellCheck.loadFromRows(spells) && spellCheck.entries().size() == spells.size(),
        "Spell typed loader rejected converted rows");
    Rows englishSpells = spells;
    for (Row &row : englishSpells)
    {
        row[2] = row[24];
        row.resize(24);
    }
    Game::SpellTable englishSpellCheck;
    require(englishSpellCheck.loadFromRows(englishSpells), "Base spell lookup validation failed");
    for (const Row &row : spells)
    {
        const Game::SpellEntry *pSpell = spellCheck.findByName(row[24]);
        const Game::SpellEntry *pBaseSpell = englishSpellCheck.findByName(row[24]);
        const Game::SpellEntry *pById = spellCheck.findById(integer(row[0]));
        // Charm occupies both a school and a racial slot. Preserve the normal
        // English lookup result while retaining both spells by their own IDs.
        require(pSpell && pBaseSpell && pSpell->id == pBaseSpell->id && pById && pById->name == row[2],
            "Localized spell changed source-name lookup");
    }
    Game::MonsterTable monsterCheck;
    require(monsterCheck.loadStatsFromRows(values(coreMonsters)), "Monster ability validation failed");
    size_t monsterSpellReferences = 0;
    size_t monsterMissileReferences = 0;
    std::map<std::string, std::vector<uint32_t>> unsupportedMonsterSpells;
    for (const auto &[id, stats] : monsterCheck.statsEntries())
    {
        for (const auto &[active, name] : {std::pair{stats.hasSpell1, stats.spell1Name},
            std::pair{stats.hasSpell2, stats.spell2Name}})
        {
            if (active)
            {
                const Game::SpellEntry *pSpell = spellCheck.findByName(name);
                const bool knownUnsupported = Game::isKnownUnsupportedMonsterSpellName(name);
                require(pSpell || knownUnsupported,
                    "Unresolved monster spell at ID " + std::to_string(id) + ": " + name);
                if (knownUnsupported)
                {
                    // Preserve the descriptor, exposing the existing engine
                    // gap instead of replacing it with a different ability.
                    unsupportedMonsterSpells[name].push_back(id);
                }
                monsterSpellReferences += pSpell ? 1 : 0;
            }
        }
        for (const auto &[active, token] : {std::pair{stats.attack1HasMissile, stats.attack1MissileType},
            std::pair{stats.attack2HasMissile, stats.attack2MissileType}})
        {
            require(!active || projectileCheck.findByToken(token), "Unresolved active monster missile: " + token);
            require(lower(token) != "dagger" || !active, "Legacy Dagger unexpectedly became a ranged attack");
            monsterMissileReferences += active ? 1 : 0;
        }
    }

    IndexedRows items = indexed(archiveTable("items.txt"), 17, "items.txt");
    const IndexedRows randomItems = indexed(archiveTable("rnditems.txt"), 9, "rnditems.txt");
    size_t translatedItems = 0;
    size_t obsoleteRandomIconLabels = 0;
    if (chinese)
    {
        const Rows locale = looseTable("Data/Text localization/ZHCN_ItemsTxt.txt", true);
        require(locale.front() == Row{"Id", "Name", "NotIdentifiedName ", "Notes"},
            "Unrecognized item localization schema");
        for (const auto &[id, row] : indexed(locale, 4, "item localization"))
        {
            if (items.contains(id))
            {
                items.at(id)[2] = row[1];
                items.at(id)[10] = row[2];
                items.at(id)[16] = row[3];
                ++translatedItems;
            }
        }
    }
    for (const auto &[id, row] : items)
    {
        require(integer(row[11]) < objectRows.size(), "Item references an absent object-description slot");
        require(id == 0 || randomItems.contains(id), "Missing item treasure weights");
    }
    for (const auto &[id, row] : randomItems)
    {
        require(items.contains(id), "Random treasure references an absent item ID");
        for (size_t column = 2; column < 8; ++column)
        {
            integer(row[column], 32767);
        }
        // The source includes outdated icon labels. The loader joins by ID;
        // keep the original weights and label, do not use labels as foreign keys.
        obsoleteRandomIconLabels += row[1] != items.at(id)[1] ? 1 : 0;
    }
    Engine::AssetFileSystem files;
    require(files.initialize(core, core, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount core data for validation");
    Game::ItemTable itemCheck;
    require(itemCheck.load(files, values(items), values(randomItems)), "Item typed loader rejected converted rows");
    const EnchantmentResources enchants = convertEnchantmentTables(archiveTable("stditems.txt"),
        archiveTable("spcitems.txt"), chinese ? archiveTable("stditems.txt", true) : Rows{},
        chinese ? archiveTable("spcitems.txt", true) : Rows{});
    const std::map<std::string, Rows> tables = {{"monster_projectiles.txt", missiles}, {"spells.txt", spells},
        {"items.txt", values(items)}, {"random_items.txt", values(randomItems)},
        {"standard_item_enchants.txt", enchants.standard}, {"special_item_enchants.txt", enchants.special}};
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-combat-tables\",\"runtime_ready\":false,"
        << "\"typed_validation\":true,\"inventory_footprints_ready\":false,\"spell_profile\":\"mm8-132-slots\","
        << "\"projectile_bindings\":" << missiles.size() << ",\"legacy_dagger_nonprojectile_monsters\":"
        << jsonIds(daggerMonsterIds) << ",\"active_spells\":" << spells.size()
        << ",\"resolved_monster_spell_references\":" << monsterSpellReferences
        << ",\"resolved_monster_missile_references\":" << monsterMissileReferences
        << ",\"known_unsupported_monster_spells\":[";
    bool first = true;
    for (auto &[name, ids] : unsupportedMonsterSpells)
    {
        std::sort(ids.begin(), ids.end());
        manifest << (first ? "" : ",") << "{\"name\":" << std::quoted(name)
            << ",\"monster_ids\":" << jsonIds(ids) << "}";
        first = false;
    }
    manifest << "]"
        << ",\"unused_spell_ids\":" << jsonIds(unusedSpellIds)
        << ",\"reserved_spell_slots_without_objects\":" << jsonIds(spellSlotsWithoutObjects)
        << ",\"translated_spells\":" << translatedSpells << ",\"preserved_spell_mechanical_cells\":"
        << preservedSpellCells << ",\"item_rows_including_zero\":" << items.size()
        << ",\"translated_items\":" << translatedItems << ",\"treasure_weight_rows\":" << randomItems.size()
        << ",\"obsolete_random_icon_labels\":" << obsoleteRandomIconLabels << ",\"restored_glyphs\":" << glyphs
        << ",\"standard_enchants\":" << enchants.standard.size()
        << ",\"special_enchants\":" << enchants.special.size()
        << ",\"preserved_enchant_mechanical_cells\":" << enchants.preservedMechanicalCells
        << ",\"translated_enchants\":" << (chinese ? enchants.standard.size() + enchants.special.size() : 0)
        << ",\"enchant_profile\":\"mmerge-25-standard-73-special\",\"unsupported_standard_enchant_ids\":[25]"
        << ",\"unsupported_special_enchant_ids\":[73],\"unsupported_enchants_have_random_weight\":false"
        << ",\"enchant_treasure_ranges_imported\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh combat output");
    try
    {
        for (const auto &[name, rows] : tables)
        {
            writeText(output / "engine/data_tables" / name, serializeTextRows(rows));
        }
        writeText(output / "combat_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
