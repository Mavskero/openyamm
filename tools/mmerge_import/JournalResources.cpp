#include "JournalResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/gameplay/AwardRuntime.h"
#include "game/party/Party.h"
#include "game/tables/AwardTable.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/tables/JournalHistoryTable.h"

#include <algorithm>
#include <charconv>
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

uint32_t integer(const std::string &text)
{
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value > 0 && value <= 65535,
        "Invalid journal ID/value: " + text);
    return value;
}

bool numeric(const std::string &text)
{
    return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
}

struct TableSpec
{
    const char *pSource;
    const char *pOutput;
    Row header;
    bool multiline;
    std::vector<size_t> displayColumns;
    size_t minimumColumns = 0;
};

const std::vector<TableSpec> JournalTables = {
    {"awards.txt", "awards.txt", {"A Bit", "Awards", "Sort", "Notes"}, false, {1}},
    {"autonote.txt", "autonote.txt", {"Note bit", "Autonote Text", "Category", "", ""}, true, {1}},
    {"history.txt", "history.txt", {"#", "Text", "Time", "Page Title"}, true, {1, 3}},
    {"mm7history.txt", "mm7_history.txt", {"#", "Text", "Time", "Page Title"}, true, {1, 3}},
};

IndexedRows readTable(const std::filesystem::path &archive, const TableSpec &spec, bool chinese,
    size_t &glyphs, size_t &continuations)
{
    const std::optional<Engine::TextTable> parsed =
        Engine::TextTable::parseTabSeparated(decodeLegacyText(readLodText(archive, spec.pSource), chinese, glyphs));
    require(parsed && parsed->getRowCount() > 0 && parsed->getRow(0) == spec.header,
        std::string("Unrecognized journal header/quoting: ") + spec.pSource);
    const size_t columns = spec.header.size();
    Rows rows;
    for (size_t index = 1; index < parsed->getRowCount(); ++index)
    {
        Row row = parsed->getRow(index);
        if (spec.minimumColumns > 0 && row.size() >= spec.minimumColumns && row.size() < columns
            && numeric(row[0]))
        {
            // The newer cross-continent quest rows omit their final Owner cell.
            // This is a declared optional field, not an incomplete text block.
            row.resize(columns);
        }
        const bool blank = row.empty() || (row.size() == 1 && row[0].empty());
        if (!rows.empty() && rows.back().size() < columns)
        {
            // Only an unfinished text cell may continue across physical lines.
            // Its final tabs supply the category/time/title; do not swallow the
            // next numeric record or append stray text to a complete entry.
            require(spec.multiline && rows.back().size() == 2 && (blank || !numeric(row[0])),
                std::string("Interrupted journal entry: ") + spec.pSource);
            rows.back()[1] += '\n';
            if (!blank)
            {
                rows.back()[1] += row[0];
                rows.back().insert(rows.back().end(), row.begin() + 1, row.end());
            }
            require(rows.back().size() <= columns, "Journal continuation has extra columns");
            ++continuations;
            continue;
        }
        if (blank)
        {
            continue;
        }
        require(numeric(row[0]) && (row.size() == columns || (spec.multiline && row.size() == 2)),
            std::string("Malformed journal row: ") + spec.pSource + " at " + std::to_string(index + 1));
        rows.push_back(row);
    }
    IndexedRows result;
    for (const Row &row : rows)
    {
        require(row.size() == columns, std::string("Unfinished journal text: ") + spec.pSource);
        require(result.emplace(integer(row[0]), row).second, "Duplicate journal ID: " + row[0]);
    }
    require(!result.empty(), "Empty journal table");
    return result;
}

Rows values(const IndexedRows &rows)
{
    Rows result;
    for (const auto &[id, row] : rows)
    {
        result.push_back(row);
    }
    return result;
}

std::string jsonIds(const std::vector<uint32_t> &ids)
{
    std::ostringstream text;
    text << '[';
    for (size_t index = 0; index < ids.size(); ++index)
    {
        text << (index == 0 ? "" : ",") << ids[index];
    }
    text << ']';
    return text.str();
}

void validateAwardVisibility(const Game::AwardTable &awards)
{
    Game::Party party;
    Game::Character character;
    require(Game::buildVisibleAwards(awards, character, party, {}).empty(), "Awards visible before unlocking");
    for (const Game::AwardDisplayEntry &entry : awards.entries())
    {
        if (entry.stateKind == Game::AwardStateKind::CharacterAward)
        {
            character.awards.insert(entry.stateId);
        }
        else if (entry.stateKind == Game::AwardStateKind::PartyQBit)
        {
            party.setQuestBit(entry.stateId, true);
        }
        const auto lookup = [&](uint32_t id)
        {
            return entry.stateKind == Game::AwardStateKind::PartyAutonote && id == entry.stateId;
        };
        const std::vector<Game::VisibleAward> visible = Game::buildVisibleAwards(awards, character, party, lookup);
        require(visible.size() == 1 && visible.front().key == entry.key
            && visible.front().priority == entry.priority && !visible.front().text.empty(),
            "Award state selected the wrong display entry");
        std::string expectedText = entry.text;
        if (entry.stateKind == Game::AwardStateKind::CharacterAward)
        {
            const size_t longMarker = expectedText.find("%lu");
            const size_t marker = longMarker != std::string::npos ? longMarker : expectedText.find("%u");
            if (marker != std::string::npos)
            {
                expectedText.replace(marker, longMarker != std::string::npos ? 3 : 2, "0");
            }
        }
        require(visible.front().text == expectedText, "Award display/counter substitution changed");
        if (entry.stateKind != Game::AwardStateKind::CharacterAward)
        {
            require(character.awards.empty() && visible.front().text == entry.text,
                "Party award projection changed character awards or text");
        }
        character.awards.clear();
        if (entry.stateKind == Game::AwardStateKind::PartyQBit)
        {
            party.setQuestBit(entry.stateId, false);
        }
        require(Game::buildVisibleAwards(awards, character, party, {}).empty(), "Award remained after clearing state");
    }
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertJournalResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Journal output must be outside source");
    require(!std::filesystem::exists(output), "Journal output already exists");
    const auto archivePath = [&](const std::string &name)
    {
        const std::filesystem::path path = std::filesystem::canonical(root / "Data" / name);
        const std::filesystem::path archiveRelative = path.lexically_relative(root);
        require(!archiveRelative.empty() && !archiveRelative.is_absolute() && *archiveRelative.begin() != "..",
            "Journal source archive escapes root");
        return path;
    };
    const std::filesystem::path base = archivePath("mmmerge.T.lod");
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    const std::filesystem::path locale = chinese ? archivePath("10 LocZHCN.EnglishT.lod") : base;
    size_t glyphs = 0;
    size_t continuations = 0;
    std::map<std::string, IndexedRows> baseTables;
    std::map<std::string, IndexedRows> tables;
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-journal-tables\",\"runtime_ready\":false,\"tables\":[";
    for (const TableSpec &spec : JournalTables)
    {
        IndexedRows rows = readTable(base, spec, false, glyphs, continuations);
        baseTables.emplace(spec.pOutput, rows);
        IndexedRows translated = chinese ? readTable(locale, spec, true, glyphs, continuations) : IndexedRows{};
        size_t localizedRows = 0;
        size_t preservedCells = 0;
        std::vector<uint32_t> inactiveIds;
        std::vector<uint32_t> untranslatedIds;
        for (auto &[id, row] : rows)
        {
            if (row[1].empty())
            {
                inactiveIds.push_back(id);
                translated.erase(id);
                continue;
            }
            if (!translated.contains(id))
            {
                if (chinese)
                {
                    untranslatedIds.push_back(id);
                }
                continue;
            }
            const Row &translation = translated.at(id);
            require(!translation[1].empty(), "Translation blanks active journal entry: " + row[0]);
            for (size_t column = 1; column < row.size(); ++column)
            {
                if (std::find(spec.displayColumns.begin(), spec.displayColumns.end(), column)
                    != spec.displayColumns.end())
                {
                    row[column] = translation[column];
                }
                else
                {
                    preservedCells += row[column] != translation[column] ? 1 : 0;
                }
            }
            ++localizedRows;
            translated.erase(id);
        }
        std::vector<uint32_t> ignoredIds;
        for (const auto &[id, row] : translated)
        {
            ignoredIds.push_back(id);
        }
        manifest << (tables.empty() ? "" : ",") << "{\"source\":" << std::quoted(spec.pSource)
            << ",\"rows\":" << rows.size() << ",\"localized_rows\":" << localizedRows
            << ",\"inactive_ids\":" << jsonIds(inactiveIds)
            << ",\"untranslated_ids\":" << jsonIds(untranslatedIds)
            << ",\"ignored_localization_ids\":" << jsonIds(ignoredIds)
            << ",\"preserved_mechanical_cells\":" << preservedCells << "}";
        tables.emplace(spec.pOutput, std::move(rows));
    }

    const TableSpec questSpec = {"quests.txt", "", {"Q Bit", "Quest Note Text", "Notes", "Owner"}, false, {}, 3};
    const IndexedRows quests = readTable(base, questSpec, false, glyphs, continuations);
    const IndexedRows &baseNotes = baseTables.at("autonote.txt");
    const IndexedRows &notes = tables.at("autonote.txt");
    Rows aliases;
    // The shared award regression contract covers 13 displaced MM8 autonotes
    // and 18 QBits. Source note/quest equality validates the QBit text join.
    // Use the actual state ID in new display keys; do not invent old award IDs.
    for (uint32_t id = 492; id <= 504; ++id)
    {
        require(notes.contains(id) && !notes.at(id)[1].empty(), "Missing MM8 award autonote");
        aliases.push_back({"mm8.award.autonote." + std::to_string(id), "party_autonote",
            std::to_string(id), notes.at(id)[1], "3", "MMerge autonote award"});
    }
    for (uint32_t id = 1537; id <= 1554; ++id)
    {
        const uint32_t noteId = id - 1032;
        require(quests.contains(id) && baseNotes.contains(noteId) && !quests.at(id)[2].empty()
            && quests.at(id)[2] == baseNotes.at(noteId)[1] && quests.at(id)[1].empty(),
            "MM8 award QBit/note profile mismatch at " + std::to_string(id));
        aliases.push_back({"mm8.award.qbit." + std::to_string(id), "party_qbit", std::to_string(id),
            notes.at(noteId)[1], "3", "Display text from autonote " + std::to_string(noteId)});
    }
    Game::AwardTable awardCheck;
    require(awardCheck.loadFromRows(values(tables.at("awards.txt")), aliases), "Award typed validation failed");
    require(awardCheck.entries().size() == tables.at("awards.txt").size() + aliases.size(),
        "Award typed loader changed coverage");
    for (const auto &[id, row] : baseTables.at("awards.txt"))
    {
        require(integer(row[2]) <= 5, "Unsupported award sort group");
    }
    validateAwardVisibility(awardCheck);
    Game::JournalHistoryTable historyCheck;
    require(historyCheck.loadFromRows(values(tables.at("history.txt")))
        && historyCheck.loadFromRowsForContinent(2, values(tables.at("mm7_history.txt"))),
        "History typed validation failed");
    for (const auto &[file, continent] : std::map<std::string, uint32_t>{{"history.txt", 1}, {"mm7_history.txt", 2}})
    {
        size_t activeRows = 0;
        for (const auto &[id, row] : tables.at(file))
        {
            const Game::JournalHistoryEntry *pEntry = historyCheck.getForContinent(continent, id);
            require(row[1].empty() ? pEntry == nullptr : pEntry && pEntry->text == row[1]
                && pEntry->pageTitle == row[3] && pEntry->timeToken == baseTables.at(file).at(id)[2],
                "History continent, trigger or display text changed");
            activeRows += !row[1].empty() ? 1 : 0;
        }
        require(historyCheck.entriesForContinent(continent).size() == activeRows, "History coverage changed");
    }
    Game::JournalAutonoteTable baseNoteCheck;
    Game::JournalAutonoteTable noteCheck;
    require(baseNoteCheck.loadFromRows(values(baseNotes)) && noteCheck.loadFromRows(values(notes)),
        "Autonote typed validation failed");
    require(baseNoteCheck.entries().size() == noteCheck.entries().size(), "Autonote coverage changed");
    std::vector<uint32_t> awardCategoryIds;
    for (size_t index = 0; index < noteCheck.entries().size(); ++index)
    {
        const Game::JournalAutonoteEntry &entry = noteCheck.entries()[index];
        const Game::JournalAutonoteEntry &original = baseNoteCheck.entries()[index];
        const uint32_t id = entry.noteBit >> 16;
        require(entry.noteBit == ((id << 16) | 0xE1u) && entry.noteBit == original.noteBit
            && entry.category == original.category && entry.text == notes.at(id)[1], "Autonote identity changed");
        std::string category = baseNotes.at(id)[2];
        std::transform(category.begin(), category.end(), category.begin(), [](unsigned char c)
        {
            return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        });
        require(std::set<std::string>{"potion", "stat", "obelisk", "seer", "teacher", "misc", "award"}
            .contains(category), "Unknown autonote category: " + category);
        if (category == "award")
        {
            awardCategoryIds.push_back(id);
        }
    }
    manifest << "],\"character_awards\":" << tables.at("awards.txt").size()
        << ",\"party_award_aliases\":" << aliases.size() << ",\"visible_autonotes\":" << noteCheck.entries().size()
        << ",\"mm8_history_entries\":" << historyCheck.entries().size()
        << ",\"mm7_history_entries\":" << historyCheck.entriesForContinent(2).size()
        << ",\"award_category_using_existing_misc_ids\":" << jsonIds(awardCategoryIds)
        << ",\"restored_glyphs\":" << glyphs << ",\"unquoted_continuation_lines\":" << continuations
        << ",\"typed_validation\":true,\"award_state_visibility_validated\":true,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh journal output");
    try
    {
        for (const auto &[name, rows] : tables)
        {
            writeText(output / "engine/data_tables/english" / name, serializeTextRows(values(rows)));
        }
        writeText(output / "engine/data_tables/english/award_display_aliases.txt", serializeTextRows(aliases));
        writeText(output / "journal_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
