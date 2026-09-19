#include "InventoryResources.h"
#include "ItemIconResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/AssetFileSystem.h"
#include "engine/TextTable.h"
#include "game/items/InventoryItemMixingRuntime.h"
#include "game/items/InventoryItemUseRuntime.h"
#include "game/party/Party.h"
#include "game/tables/ItemTable.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/PotionMixingTable.h"
#include "game/tables/PotionNoteTable.h"
#include "game/tables/ReadableScrollTable.h"

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
using Indexed = std::map<uint32_t, Row>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool numeric(const std::string &text)
{
    return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
}

uint32_t number(const std::string &text, uint32_t maximum = 65535)
{
    uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size() && value <= maximum,
        "Invalid inventory table number: " + text);
    return value;
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Inventory input escapes root");
    return path;
}

std::string readLoose(const std::filesystem::path &root, const std::string &name, bool legacy = false)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid inventory table size");
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read inventory table");
    size_t glyphs = 0;
    return legacy ? decodeLegacyText(bytes, false, glyphs) : std::string(bytes.begin(), bytes.end());
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid inventory table quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    return rows;
}

Indexed indexed(const Rows &rows, size_t columns)
{
    Indexed result;
    for (const Row &row : rows)
    {
        if (!row.empty() && numeric(row[0]))
        {
            require(row.size() == columns && result.emplace(number(row[0]), row).second,
                "Invalid inventory registry shape or duplicate ID");
        }
    }
    require(!result.empty(), "Empty inventory registry");
    return result;
}

Row splitLine(const std::string &line)
{
    Row row;
    size_t start = 0;
    while (true)
    {
        const size_t end = line.find('\t', start);
        row.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos)
        {
            return row;
        }
        start = end + 1;
    }
}

Indexed scrollRows(const std::string &text, size_t &continuations, size_t &quoteRepairs)
{
    // Five supplied item-name comments have a truncated opening quote. Parse
    // physical records first so these comments cannot swallow subsequent IDs.
    const std::map<uint32_t, std::string> truncatedComments = {
        {1520, "\"Journal Entry, 23rd Apri"}, {1521, "\"Journal Entry, 30th June"},
        {1524, "\"Journal Entry, 11th Dece"}, {1526, "\"Journal Entry, 2nd Janua"},
        {1527, "\"Journal Entry, 19th Sept"}};
    std::istringstream stream(text);
    std::string line;
    Rows rows;
    bool header = true;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        Row row = splitLine(line);
        if (header)
        {
            require(row == Row{"Item#", "message text", "Location", "Item Name"}, "Invalid scroll header");
            header = false;
        }
        else if (!rows.empty() && rows.back().size() == 2)
        {
            require((row.size() == 1 || row.size() == 3) && !(row.size() > 1 && numeric(row[0])),
                "Interrupted scroll paragraph");
            rows.back()[1] += '\n' + row[0];
            rows.back().insert(rows.back().end(), row.begin() + 1, row.end());
            ++continuations;
        }
        else if (std::any_of(row.begin(), row.end(), [](const std::string &cell) { return !cell.empty(); }))
        {
            require((row.size() == 2 || row.size() == 4) && numeric(row[0]), "Invalid scroll record");
            rows.push_back(std::move(row));
        }
    }
    Indexed result;
    for (Row &row : rows)
    {
        require(row.size() == 4, "Unfinished scroll paragraph");
        const uint32_t id = number(row[0]);
        require(id > 0, "Invalid scroll item ID");
        for (size_t column = 1; column < 4; ++column)
        {
            std::string &cell = row[column];
            if (!cell.empty() && cell.front() == '"')
            {
                if (cell.size() >= 2 && cell.back() == '"')
                {
                    cell = cell.substr(1, cell.size() - 2);
                    for (size_t position = 0; (position = cell.find("\"\"", position)) != std::string::npos; ++position)
                    {
                        cell.erase(position, 1);
                    }
                }
                else
                {
                    require(column == 3 && truncatedComments.contains(id) && cell == truncatedComments.at(id),
                        "Unsupported truncated scroll quote");
                    cell.erase(0, 1);
                    ++quoteRepairs;
                }
            }
        }
        require(result.emplace(id, row).second, "Duplicate scroll item ID");
    }
    require(!result.empty(), "Empty scroll table");
    return result;
}

Rows matrixRows(const Rows &source, size_t &continuations, size_t &omittedRows)
{
    require(!source.empty() && source[0].size() >= 4
        && Row(source[0].begin(), source[0].begin() + 4) == Row{"", "Name", "Description", "Effect"},
        "Invalid potion matrix header");
    Indexed records;
    for (size_t index = 1; index < source.size(); ++index)
    {
        Row row = source[index];
        if (row.empty() || !numeric(row[0]))
        {
            // Authored reagent descriptions/footer labels have no matrix IDs.
            require(row.empty() || std::all_of(row.begin() + std::min<size_t>(row.size(), 4), row.end(),
                [](const std::string &cell) { return cell.empty(); }), "Unrecognized potion matrix footer");
            ++omittedRows;
            continue;
        }
        const uint32_t id = number(row[0]);
        require((id >= 200 && id <= 290) || id == 806, "Unknown potion matrix row ID");
        if (row.size() == 4 && (id == 282 || id == 283))
        {
            require(index + 1 < source.size() && source[index + 1].size() == 74 && source[index + 1][0].empty(),
                "Interrupted potion effect continuation");
            const Row &tail = source[++index];
            row.insert(row.end(), tail.begin() + 1, tail.end());
            ++continuations;
        }
        require(row.size() >= 77 && std::all_of(row.begin() + 77, row.end(),
            [](const std::string &cell) { return cell.empty(); }), "Potion matrix width or padding changed");
        row.resize(77);
        require(records.emplace(id, row).second, "Duplicate potion matrix ID");
    }
    require(records.size() == 92 && records.contains(806), "Incomplete potion source matrix");
    const Row &header = records.at(220);
    for (uint32_t id = 222; id <= 290; ++id)
    {
        require(header[id - 215] == std::to_string(id), "Potion matrix column ID/order changed");
    }
    require(header[76] == "806" && records.at(806)[1] == "Strange potion", "Legacy potion slot changed");
    Rows result;
    Row outputHeader(header.begin(), header.begin() + 76);
    result.push_back(std::move(outputHeader));
    for (uint32_t id = 222; id <= 290; ++id)
    {
        const Row &row = records.at(id);
        result.emplace_back(row.begin(), row.begin() + 76);
    }
    return result;
}

std::string jsonIds(const std::vector<uint32_t> &ids)
{
    std::string result = "[";
    for (uint32_t id : ids)
    {
        result += (result.size() == 1 ? "" : ",") + std::to_string(id);
    }
    return result + "]";
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}

void verifyMix(const Game::ItemTable &items, const Game::PotionMixingTable &mixing,
    const Game::MergedPotionSettingTable &settings, const Game::PotionNoteTable &notes,
    uint32_t heldId, uint32_t targetId, bool rejected)
{
    Game::Party party;
    party.setItemTable(&items);
    Game::PartySeed seed;
    Game::Character member;
    member.name = "Mix check";
    member.className = "Knight";
    member.health = member.maxHealth = 40;
    member.skills["Alchemy"] = {"Alchemy", 10, Game::SkillMastery::Grandmaster};
    seed.members.push_back(member);
    party.seed(seed);
    Game::InventoryItem target;
    target.objectDescriptionId = targetId;
    target.width = target.height = target.quantity = 1;
    target.standardEnchantPower = 40;
    require(party.tryAutoPlaceItemInMemberInventory(0, target), "Cannot prepare diagnostic potion target");
    Game::InventoryItem held;
    held.objectDescriptionId = heldId;
    held.quantity = held.width = held.height = 1;
    held.standardEnchantPower = 20;
    Game::MergedReagentSettingTable reagents;
    const Game::InventoryItemMixResult result = Game::InventoryItemMixingRuntime::tryApplyHeldItemToInventoryItem(
        party, 0, held, 0, 0, items, mixing, settings, reagents, &notes);
    const Game::InventoryItem *pAfter = party.memberInventoryItemMutable(0, 0, 0);
    require(held.objectDescriptionId == heldId && held.standardEnchantPower == 20 && held.quantity == 1 && pAfter,
        "Diagnostic mixing changed held item or removed target");
    if (rejected)
    {
        require(!result.handled && !result.success && !result.heldItemConsumed && !result.targetItemChanged
            && !result.targetItemRemoved && !result.heldItemReplacement && result.unlockedAutonoteId == 0
            && pAfter->objectDescriptionId == targetId && pAfter->standardEnchantPower == 40,
            "Invalid potion result consumed or changed inputs");
    }
    else
    {
        const std::optional<Game::PotionMixingTable::PotionCombination> combination =
            mixing.potionCombination(heldId, targetId);
        require(result.success && result.heldItemConsumed && result.targetItemChanged && combination
            && pAfter->objectDescriptionId == combination->resultItemId && pAfter->standardEnchantPower == 30
            && result.unlockedAutonoteId == notes.autonoteIdForMix(targetId, heldId).value_or(0),
            "Valid diagnostic potion mixing changed");
    }
}
}

void convertInventoryResources(const std::filesystem::path &source, const std::filesystem::path &gameAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(gameAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Inventory output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Inventory output already exists");
    const std::filesystem::path base = checkedPath(root, "Data/mmmerge.T.lod");
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    size_t glyphs = 0;
    const auto baseText = [&](const std::string &name)
    {
        return decodeLegacyText(readLodText(base, name), false, glyphs);
    };
    const Rows itemRows = parse(readLoose(assets, "engine/data_tables/items.txt"));
    const Indexed items = indexed(itemRows, 17);
    const Indexed sourceItems = indexed(parse(baseText("items.txt")), 17);
    require(items.size() == sourceItems.size(), "Stale item registry coverage");
    for (const auto &[id, row] : sourceItems)
    {
        require(items.contains(id), "Stale item registry ID");
        for (size_t column = 0; column < 17; ++column)
        {
            require(column == 2 || column == 10 || column == 16 || row[column] == items.at(id)[column],
                "Stale item registry mechanics");
        }
    }
    Engine::AssetFileSystem fileSystem;
    Game::ItemTable itemCheck;
    require(itemCheck.load(fileSystem, itemRows, {}), "Item typed validation failed");
    Game::JournalAutonoteTable noteCheck;
    require(noteCheck.loadFromRows(parse(readLoose(assets, "engine/data_tables/english/autonote.txt"))),
        "Autonote typed validation failed");
    std::set<uint32_t> potionNotes;
    for (const Game::JournalAutonoteEntry &entry : noteCheck.entries())
    {
        if (entry.category == Game::JournalAutonoteCategory::Potion)
        {
            potionNotes.insert(entry.noteBit >> 16);
        }
    }

    size_t scrollContinuations = 0;
    size_t quoteRepairs = 0;
    Indexed scrolls = scrollRows(baseText("scroll.txt"), scrollContinuations, quoteRepairs);
    Indexed locale;
    if (chinese)
    {
        locale = scrollRows(decodeLegacyText(readLodText(checkedPath(root, "Data/10 LocZHCN.EnglishT.lod"),
            "scroll.txt"), true, glyphs), scrollContinuations, quoteRepairs);
    }
    std::vector<uint32_t> inactive;
    std::vector<uint32_t> untranslated;
    std::vector<uint32_t> ignoredLocale;
    size_t localized = 0;
    Rows scrollOutput;
    for (auto &[id, row] : scrolls)
    {
        require(itemCheck.get(id) != nullptr, "Scroll references absent item");
        if (row[1].empty())
        {
            inactive.push_back(id);
        }
        else
        {
            require(sourceItems.at(id)[4] == "Mscroll", "Readable item is not a message scroll");
            if (locale.contains(id) && !locale.at(id)[1].empty())
            {
                const Row &translated = locale.at(id);
                require(row[2] == translated[2] && row[3] == translated[3], "Scroll translation identity changed");
                row[1] = translated[1];
                ++localized;
            }
            else if (chinese)
            {
                untranslated.push_back(id); // Old ZHCN 1574 is an empty placeholder; preserve the newer hint.
            }
        }
        locale.erase(id);
        scrollOutput.push_back(row);
    }
    for (const auto &[id, row] : locale)
    {
        ignoredLocale.push_back(id);
    }
    const std::string scrollText = serializeTextRows(scrollOutput);
    Game::ReadableScrollTable scrollCheck;
    require(scrollCheck.loadFromRows(parse(scrollText)), "Readable scroll typed load failed");
    Game::Party party;
    Game::PartySeed seed;
    Game::Character member;
    member.name = "Import check";
    member.className = "Knight";
    member.level = 1;
    member.health = member.maxHealth = 40;
    seed.members.push_back(member);
    party.seed(seed);
    size_t readChecks = 0;
    for (const Row &row : scrollOutput)
    {
        const uint32_t id = number(row[0]);
        const Game::ReadableScrollEntry *pEntry = scrollCheck.get(id);
        require(pEntry && pEntry->text == row[1] && pEntry->location == row[2] && pEntry->itemName == row[3],
            "Readable scroll content changed");
        if (!row[1].empty())
        {
            Game::InventoryItem item;
            item.objectDescriptionId = id;
            const Game::InventoryItemUseResult result =
                Game::InventoryItemUseRuntime::useItemOnMember(party, 0, item, itemCheck, &scrollCheck);
            require(result.handled && !result.consumed && result.readableBody == row[1]
                && result.readableTitle == itemCheck.get(id)->name, "Scroll item-use behavior changed");
            ++readChecks;
        }
    }

    const Rows settings = parse(readLoose(root, "Data/Tables/Potion settings.txt", true));
    const Indexed settingIds = indexed(settings, 6);
    require(settings[0] == Row{"Pot Id", "Item Id", "Required mastery", "Drinkable", "Usable", "Notes"}
        && settingIds.size() == 71, "Unsupported potion settings profile");
    Game::MergedPotionSettingTable settingCheck;
    require(settingCheck.loadFromRows(settings), "Potion settings typed load failed");
    for (uint32_t id = 0; id <= 70; ++id)
    {
        require(settingIds.contains(id), "Missing potion settings ID");
        const Row &row = settingIds.at(id);
        require(number(row[1]) == id + 220 && sourceItems.contains(id + 220)
            && sourceItems.at(id + 220)[4] == "Bottle", "Potion item identity changed");
        require(id == 0 ? row[2] == "N/A" && row[3] == "N/A" && row[4] == "N/A"
            : numeric(row[2]) && number(row[2], 4) <= 4 && (row[3] == "x" || row[3] == "-")
                && (row[4] == "x" || row[4] == "-"), "Invalid potion settings fields");
        const Game::MergedPotionSettingEntry *pEntry = settingCheck.getByItemId(id + 220);
        const size_t noteStart = row[5].find_first_not_of(" \t\r\n");
        const size_t noteEnd = row[5].find_last_not_of(" \t\r\n");
        const std::string expectedNote = noteStart == std::string::npos ? ""
            : row[5].substr(noteStart, noteEnd - noteStart + 1);
        require(pEntry && pEntry->potionId == id && pEntry->drinkable == (row[3] == "x")
            && pEntry->usable == (row[4] == "x") && pEntry->note == expectedNote
            && (id == 0 ? !pEntry->requiredMastery : pEntry->requiredMastery == number(row[2])),
            "Potion settings changed at ID " + std::to_string(id));
    }
    // 806 is an obsolete Strange potion matrix slot, absent from the authoritative
    // settings and now a Weapon item. Do not make Graceful Sword a potion operand.
    require(sourceItems.contains(806) && sourceItems.at(806)[4] == "Weapon"
        && !settingCheck.getByItemId(806), "Stale potion exclusion no longer applies");
    size_t matrixContinuations = 0;
    size_t omittedRows = 0;
    const Rows mixing = matrixRows(parse(baseText("potion.txt")), matrixContinuations, omittedRows);
    const Rows notes = matrixRows(parse(baseText("potnotes.txt")), matrixContinuations, omittedRows);
    Game::PotionMixingTable mixingCheck;
    Game::PotionNoteTable potionNoteCheck;
    const std::string mixingText = serializeTextRows(mixing);
    const std::string noteText = serializeTextRows(notes);
    require(mixingCheck.loadFromRows(parse(mixingText)) && potionNoteCheck.loadFromRows(parse(noteText)),
        "Potion matrices typed load failed");
    size_t noteLinks = 0;
    size_t asymmetricMixes = 0;
    size_t asymmetricNotes = 0;
    size_t unsupportedResults = 0;
    for (uint32_t row = 1; row <= 69; ++row)
    {
        for (uint32_t column = 7; column < 76; ++column)
        {
            const uint32_t held = row + 221;
            const uint32_t target = column + 215;
            const std::string &cell = mixing[row][column];
            const std::optional<Game::PotionMixingTable::PotionCombination> actual =
                mixingCheck.potionCombination(held, target);
            require(actual.has_value(), "Missing potion combination");
            const bool noMix = cell == "no";
            const bool failure = !cell.empty() && cell[0] == 'E';
            const uint32_t damage = failure ? number(cell.substr(1), 4) : 0;
            const uint32_t result = noMix || failure ? 0 : number(cell);
            require(!failure || damage > 0, "Zero potion failure level");
            const bool staleResult = held == 262 && target == 223 && result == 806;
            require(result == 0 || (result >= 221 && result <= 290) || staleResult,
                "Mix creates unregistered potion");
            unsupportedResults += staleResult ? 1 : 0;
            require((noMix || failure || result > 0) && actual->noMix == noMix
                && actual->failureDamageLevel == damage && actual->resultItemId == result,
                "Potion combination changed");
            const std::string &note = notes[row][column];
            const uint32_t noteId = note == "no" || note == "0" ? 0 : number(note);
            const std::optional<uint32_t> actualNote = potionNoteCheck.autonoteIdForMix(held, target);
            require(noteId == 0 ? !actualNote : actualNote == noteId && potionNotes.contains(noteId),
                "Potion note does not resolve to an active potion autonote");
            noteLinks += noteId > 0 ? 1 : 0;
            asymmetricMixes += cell != mixing[column - 6][row + 6] ? 1 : 0;
            asymmetricNotes += note != notes[column - 6][row + 6] ? 1 : 0;
        }
    }
    require(mixingCheck.combinationCount() == 69 * 69 && potionNoteCheck.entryCount() == noteLinks
        && !mixingCheck.potionCombination(806, 222) && !mixingCheck.potionCombination(222, 806),
        "Potion matrix coverage changed");
    verifyMix(itemCheck, mixingCheck, settingCheck, potionNoteCheck, 222, 223, false);
    if (unsupportedResults != 0)
    {
        verifyMix(itemCheck, mixingCheck, settingCheck, potionNoteCheck, 262, 223, true);
    }
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-inventory-data\",\"runtime_ready\":false,\"scroll_records\":"
        << scrollOutput.size() << ",\"readable_item_use_checks\":" << readChecks
        << ",\"localized_scrolls\":" << localized
        << ",\"inactive_scroll_ids\":" << jsonIds(inactive) << ",\"untranslated_scroll_ids\":" << jsonIds(untranslated)
        << ",\"ignored_localization_ids\":" << jsonIds(ignoredLocale)
        << ",\"repaired_metadata_quotes\":" << quoteRepairs
        << ",\"scroll_continuation_lines\":" << scrollContinuations << ",\"restored_glyphs\":" << glyphs
        << ",\"potion_settings\":71,\"potion_combinations\":4761,\"potion_note_links\":" << noteLinks
        << ",\"asymmetric_mix_cells\":" << asymmetricMixes << ",\"asymmetric_note_cells\":" << asymmetricNotes
        << ",\"omitted_matrix_operand_ids\":[806],\"matrix_continuation_lines\":" << matrixContinuations
        << ",\"unsupported_result_rules\":" << (unsupportedResults ? "[[262,223,806]]" : "[]")
        << ",\"mix_runtime_checks\":" << 1 + unsupportedResults
        << ",\"matrix_footer_or_blank_rows\":" << omittedRows
        << ",\"typed_validation\":true,\"alchemy_gameplay_validated\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh inventory output");
    try
    {
        writeText(output / "engine/data_tables/english/scroll.txt", scrollText);
        writeText(output / "engine/data_tables/english/potion.txt", mixingText);
        writeText(output / "engine/data_tables/english/potnotes.txt", noteText);
        writeText(output / "engine/data_tables/potion_settings.txt", serializeTextRows(settings));
        convertItemIconResources(root, itemCheck, output);
        writeText(output / "inventory_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
