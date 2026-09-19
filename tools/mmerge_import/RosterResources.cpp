#include "RosterResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/party/Party.h"
#include "game/party/SpellSchool.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/NpcDialogTable.h"
#include "game/tables/RosterTable.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <regex>
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

uint32_t number(const std::string &text, uint32_t maximum, bool allowEmpty = false)
{
    const std::string value = trim(text);
    if (value.empty() && allowEmpty)
    {
        return 0;
    }
    uint32_t result = 0;
    const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && result <= maximum,
        "Invalid roster numeric value: " + text);
    return result;
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Roster input escapes root");
    return path;
}

std::string readLoose(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid roster dependency size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read roster dependency");
    return text;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid roster quoting");
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

Row header()
{
    Row result = {"#", "Name", "Class", "Birthdate", "Picture", "Voice", "Experience", "Level", "Might",
        "Intelligence", "Personality", "Endurance", "Speed", "Accuracy", "Luck", "Fire resist", "Air Resist",
        "Water Resist", "Earth resist", "Mind Resist", "Body resist", "Skillpoints"};
    const Row skills = {"Staff", "Sword", "Dagger", "Axe", "Spear", "Bow", "Mace", "Blaster", "Shield",
        "Leather", "Chain", "Plate", "Fire", "Air", "Water", "Earth", "Spirit", "Mind", "Body", "Light", "Dark",
        "DarkElf", "Vampire", "Dragon", "ID Item", "Merchant", "Repair", "B Building", "Meditate", "Perception",
        "Regeneration", "Disarm Trap", "Dodging", "Unarmed", "Ident Mon", "Armsmaster", "Stealing", "Alchemy",
        "Learning"};
    for (const std::string &skill : skills)
    {
        result.push_back(skill);
        result.emplace_back();
    }
    result.insert(result.end(), skills.begin() + 12, skills.begin() + 24);
    result.push_back("QuestBit");
    result.push_back("Equipment - 0 is not a valid item!!");
    result.resize(123);
    result.insert(result.end(), {"Blurb", "End"});
    return result;
}

Rows readRoster(const std::filesystem::path &root, const std::string &archive, bool chinese, size_t &glyphs)
{
    Rows rows = parse(decodeLegacyText(readLodText(checkedPath(root, "Data/" + archive), "roster.txt"),
        chinese, glyphs));
    require(rows.size() > 2 && rows[0].size() == 125 && rows[1] == header(), "Unsupported roster header");
    std::set<uint32_t> ids;
    for (size_t index = 2; index < rows.size(); ++index)
    {
        require(rows[index].size() == 125, "Invalid roster row width");
        require(ids.insert(number(rows[index][0], 65535)).second, "Duplicate roster ID");
    }
    return rows;
}

std::map<std::string, uint32_t> classAliases(const std::filesystem::path &root)
{
    const std::string script = readLoose(root, "Scripts/General/MiscTweaks.lua");
    const std::string marker = "local ClassConns = {";
    const size_t start = script.find(marker);
    require(start != std::string::npos && script.find(marker, start + marker.size()) == std::string::npos,
        "Missing/ambiguous roster class mapping");
    const size_t end = script.find('}', start + marker.size());
    require(end != std::string::npos, "Unclosed roster class mapping");
    std::istringstream lines(script.substr(start + marker.size(), end - start - marker.size()));
    const std::regex assignment(R"(^([A-Za-z][A-Za-z0-9_]*)\s*=\s*([0-9]+)\s*,?$)");
    std::map<std::string, uint32_t> aliases;
    std::string line;
    while (std::getline(lines, line))
    {
        line = trim(line.substr(0, line.find("--")));
        if (line.empty())
        {
            continue;
        }
        std::smatch match;
        require(std::regex_match(line, match, assignment), "Unsupported executable roster class mapping");
        require(aliases.emplace(match[1].str(), number(match[2].str(), 255)).second,
            "Duplicate roster class alias");
    }
    require(!aliases.empty(), "Empty roster class mapping");
    return aliases;
}

uint32_t resolveClass(const std::string &token, const std::map<std::string, uint32_t> &aliases,
    const Game::ClassSkillTable &classes)
{
    uint32_t id = 0;
    if (aliases.contains(token))
    {
        id = aliases.at(token);
    }
    else if (!token.empty() && std::all_of(token.begin(), token.end(), [](char c) { return c >= '0' && c <= '9'; }))
    {
        id = number(token, 255);
    }
    else
    {
        const std::optional<uint32_t> named = classes.classIdForName(token);
        require(named.has_value(), "Unknown roster class: " + token);
        id = *named;
    }
    const std::optional<std::string> name = classes.classNameForId(id);
    require(name.has_value() && classes.hasClass(*name), "Roster class ID is unavailable");
    return id;
}

void validateMechanics(const Row &row, const Game::ItemTable &items, const std::set<uint32_t> &portraits)
{
    for (size_t column = 3; column <= 21; ++column)
    {
        number(row[column], column == 6 ? 0xFFFFFFFFu : 65535, true);
    }
    require(!trim(row[1]).empty() && number(row[7], 65535) > 0, "Empty roster name or level");
    require(portraits.contains(number(row[4], 65535)), "Roster portrait metadata is missing");
    for (size_t column = 22; column < 100; column += 2)
    {
        const std::string mastery = trim(row[column]);
        require(mastery.empty() || mastery == "N" || mastery == "B" || mastery == "E" || mastery == "M"
            || mastery == "G", "Unknown roster mastery token");
        const uint32_t level = number(row[column + 1], 65535, true);
        require(mastery.empty() || mastery == "N" || level > 0, "Active roster skill has no level");
    }
    for (size_t column = 100; column < 112; ++column)
    {
        number(row[column], column < 109 ? 11 : 4, true);
    }
    number(row[112], 65535, true);
    for (size_t column = 113; column < 123; ++column)
    {
        const uint32_t raw = number(row[column], 6999, true);
        if (raw != 0)
        {
            require(raw % 1000 != 0 && items.get(raw % 1000), "Roster item ID is unavailable");
        }
    }
}

void checkTypedFields(const Row &row, const Game::RosterEntry &entry)
{
    const std::vector<uint32_t> values = {entry.birthYear, entry.pictureId, static_cast<uint32_t>(entry.voiceId),
        entry.experience, entry.level, entry.might, entry.intellect, entry.personality, entry.endurance, entry.speed,
        entry.accuracy, entry.luck, static_cast<uint32_t>(entry.baseResistances.fire),
        static_cast<uint32_t>(entry.baseResistances.air), static_cast<uint32_t>(entry.baseResistances.water),
        static_cast<uint32_t>(entry.baseResistances.earth), static_cast<uint32_t>(entry.baseResistances.mind),
        static_cast<uint32_t>(entry.baseResistances.body), entry.skillPoints};
    for (size_t index = 0; index < values.size(); ++index)
    {
        const uint32_t expected = index == 2 && trim(row[index + 3]).empty()
            ? 0xFFFFFFFFu : number(row[index + 3], 0xFFFFFFFFu, true);
        require(values[index] == expected, "Roster loader changed a numeric field");
    }
    require(entry.unlockQuestBitId == number(row[112], 65535, true), "Roster loader changed unlock bit");
    const Row columns = header();
    size_t activeSkills = 0;
    for (size_t column = 22; column < 100; column += 2)
    {
        const uint32_t level = number(row[column + 1], 65535, true);
        if (level == 0)
        {
            continue;
        }
        const std::string token = trim(row[column]);
        const Game::SkillMastery mastery = token == "G" ? Game::SkillMastery::Grandmaster
            : token == "M" ? Game::SkillMastery::Master
            : token == "E" ? Game::SkillMastery::Expert : Game::SkillMastery::Normal;
        const auto found = entry.skills.find(Game::canonicalSkillName(columns[column]));
        require(found != entry.skills.end() && found->second.mastery == mastery
            && found->second.level == level, "Roster loader changed a skill");
        ++activeSkills;
    }
    require(entry.skills.size() == activeSkills, "Roster loader created extra skills");
    size_t schools = 0;
    for (size_t column = 100; column < 112; ++column)
    {
        const uint32_t count = number(row[column], 11, true);
        if (count == 0)
        {
            continue;
        }
        const auto found = entry.knownSpellCounts.find(Game::canonicalSkillName(columns[column]));
        require(found != entry.knownSpellCounts.end() && found->second == count, "Roster loader changed spell count");
        ++schools;
    }
    require(entry.knownSpellCounts.size() == schools, "Roster loader created extra spell schools");
}

void checkRecruitment(const Game::RosterEntry &entry, const Game::ItemTable &items,
    const Game::ClassMultiplierTable &multipliers)
{
    Game::Party party;
    party.setItemTable(&items);
    party.setClassMultiplierTable(&multipliers);
    require(party.recruitRosterMember(entry), "Roster recruitment failed");
    const Game::Character &member = party.members().back();
    require(member.name == entry.name && member.className == entry.className && member.rosterId == entry.id
        && member.level == entry.level && member.experience == entry.experience && member.might == entry.might
        && member.intellect == entry.intellect && member.personality == entry.personality
        && member.endurance == entry.endurance && member.speed == entry.speed && member.accuracy == entry.accuracy
        && member.luck == entry.luck && member.portraitPictureId == entry.pictureId && member.voiceId == entry.voiceId,
        "Recruitment changed roster identity or base stats");
    std::map<uint32_t, int> equipmentCounts;
    for (const Game::RosterEntry::StartingItem &item : entry.startingItems)
    {
        require(item.itemId == item.rawValue % 1000 && item.enchantmentLevel == item.rawValue / 1000,
            "Roster equipment encoding changed");
        ++equipmentCounts[item.itemId];
    }
    for (const auto &[itemId, count] : equipmentCounts)
    {
        require(party.inventoryItemCount(itemId) == count, "Recruitment lost or duplicated starting equipment");
    }
    for (const auto &[name, skill] : entry.skills)
    {
        const auto found = member.skills.find(name);
        require(found != member.skills.end() && found->second.level == skill.level
            && found->second.mastery == skill.mastery, "Recruitment changed a roster skill");
    }
    for (const auto &[skill, count] : entry.knownSpellCounts)
    {
        const std::optional<std::pair<uint32_t, uint32_t>> range = Game::spellIdRangeForMagicSkill(skill);
        require(range.has_value() && count <= range->second - range->first + 1, "Roster spell school is unavailable");
        for (uint32_t offset = 0; offset < count; ++offset)
        {
            require(member.knowsSpell(range->first + offset), "Recruitment lost a roster spell");
        }
    }
}
}

void convertRosterResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Roster output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Roster output already exists");
    const auto assetRows = [&](const std::string &name)
    {
        return parse(readLoose(assets, "engine/data_tables/" + name));
    };
    Game::ClassSkillTable classes;
    require(classes.loadCapsFromRows(assetRows("class_skills.txt"))
        && classes.loadClassMetadataFromRows(assetRows("class_extra.txt")), "Invalid roster class dependency");
    Game::ClassMultiplierTable multipliers;
    require(multipliers.loadFromRows(assetRows("class_multipliers.txt"))
        && multipliers.applyClassExtraRows(assetRows("class_extra.txt")), "Invalid roster multiplier dependency");
    Engine::AssetFileSystem fileSystem;
    Game::ItemTable items;
    require(items.load(fileSystem, assetRows("items.txt"), {}), "Invalid roster item dependency");
    const std::map<std::string, uint32_t> aliases = classAliases(root);
    size_t ignoredGlyphs = 0;
    const std::string portraitText = readLoose(root, "Data/Tables/Character portraits.txt");
    const Rows portraitRows = parse(decodeLegacyText(std::vector<uint8_t>(portraitText.begin(), portraitText.end()),
        false, ignoredGlyphs));
    std::set<uint32_t> portraits;
    require(!portraitRows.empty() && portraitRows[0].size() == 27 && portraitRows[0][0] == "#",
        "Unsupported roster portrait metadata");
    for (size_t index = 1; index < portraitRows.size(); ++index)
    {
        require(portraitRows[index].size() == 27 && portraits.insert(number(portraitRows[index][0], 65535)).second,
            "Invalid roster portrait identity");
    }
    size_t glyphs = 0;
    const Rows base = readRoster(root, "mmmerge.T.lod", false, glyphs);
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    const Rows localized = chinese ? readRoster(root, "10 LocZHCN.EnglishT.lod", true, glyphs) : Rows{};
    std::map<uint32_t, Row> translations;
    for (size_t index = 2; index < localized.size(); ++index)
    {
        translations.emplace(number(localized[index][0], 65535), localized[index]);
    }
    Rows rows = {base[0], base[1]};
    size_t names = 0;
    size_t blurbs = 0;
    size_t ignoredMechanics = 0;
    size_t startingItems = 0;
    std::vector<uint32_t> inactive;
    for (size_t index = 2; index < base.size(); ++index)
    {
        Row row = base[index];
        const uint32_t id = number(row[0], 65535);
        if (id == 0 || row[1] == "Placeholder")
        {
            require(id == 0 && row[1] == "Placeholder" && row[2] == "Placeholder",
                "Unknown inactive roster row");
            inactive.push_back(id);
            translations.erase(id);
            continue;
        }
        validateMechanics(row, items, portraits);
        const uint32_t classId = resolveClass(row[2], aliases, classes);
        if (translations.contains(id))
        {
            const Row &translation = translations.at(id);
            for (size_t column = 1; column < row.size(); ++column)
            {
                if (column != 1 && column != 123)
                {
                    ignoredMechanics += row[column] != translation[column];
                }
            }
            if (!trim(translation[1]).empty() && translation[1] != "Placeholder")
            {
                row[1] = translation[1];
                ++names;
            }
            if (!trim(translation[123]).empty())
            {
                row[123] = translation[123];
                ++blurbs;
            }
            translations.erase(id);
        }
        row[2] = std::to_string(classId);
        rows.push_back(std::move(row));
    }
    require(rows.size() > 2, "No active roster records");
    const std::string serialized = serializeTextRows(rows);
    require(parse(serialized) == rows, "Roster serialization changed fields");
    Game::RosterTable roster;
    require(roster.loadFromRows(parse(serialized), &classes), "Roster typed loading failed");
    require(roster.getEntriesSortedById().size() == rows.size() - 2, "Roster typed loading lost entries");
    for (size_t index = 2; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        const Game::RosterEntry *pEntry = roster.get(number(row[0], 65535));
        require(pEntry && pEntry->name == row[1] && pEntry->blurb == row[123]
            && pEntry->className == classes.classNameForId(number(row[2], 255)), "Roster text or class changed");
        checkTypedFields(row, *pEntry);
        checkRecruitment(*pEntry, items, multipliers);
        startingItems += pEntry->startingItems.size();
    }
    Game::NpcDialogTable dialogs;
    require(dialogs.loadTopicsFromRows(assetRows("npc_topic.txt"))
        && dialogs.loadTextsFromRows(assetRows("npc_topic_text.txt")), "Invalid roster dialog dependency");
    dialogs.resolveSpecialTopics(roster);
    size_t joinOffers = 0;
    for (uint32_t topic = 601; topic <= 649; ++topic)
    {
        const std::optional<Game::NpcDialogTable::RosterJoinOffer> offer = dialogs.getRosterJoinOfferForTopic(topic);
        if (offer)
        {
            require(offer->rosterId == topic - 600 && roster.get(offer->rosterId), "Roster join identity changed");
            ++joinOffers;
        }
    }
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"roster\",\"runtime_ready\":false,\"source_records\":" << base.size() - 2
        << ",\"active_roster\":" << rows.size() - 2 << ",\"translated_names\":" << names
        << ",\"translated_blurbs\":" << blurbs << ",\"restored_chinese_glyphs\":" << glyphs
        << ",\"ignored_translation_mechanics\":" << ignoredMechanics
        << ",\"ignored_extra_translations\":" << translations.size() << ",\"starting_items\":" << startingItems
        << ",\"resolved_join_offers\":" << joinOffers << ",\"inactive_ids\":[";
    for (size_t index = 0; index < inactive.size(); ++index)
    {
        report << (index ? "," : "") << inactive[index];
    }
    report << "],\"typed_validation\":true,\"recruitment_checks\":" << rows.size() - 2
        << ",\"portrait_art_validated\":false,\"original_enchantment_rolls_validated\":false"
        << ",\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh roster output");
    try
    {
        writeConvertedResource(output / "engine/data_tables/roster.txt",
            std::vector<uint8_t>(serialized.begin(), serialized.end()));
        const std::string manifest = report.str();
        writeConvertedResource(output / "roster_conversion.json",
            std::vector<uint8_t>(manifest.begin(), manifest.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
