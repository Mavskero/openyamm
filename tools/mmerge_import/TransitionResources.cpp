#include "TransitionResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/events/EventDialogContent.h"
#include "game/tables/TransitionTable.h"

#include <algorithm>
#include <charconv>
#include <map>
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

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Transition source escapes root");
    return path;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid transition quoting");
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

Indexed read(const std::filesystem::path &root, const std::string &archive, bool chinese, size_t &glyphs)
{
    const Rows rows = parse(decodeLegacyText(readLodText(checkedPath(root, "Data/" + archive), "trans.txt"),
        chinese, glyphs));
    require(!rows.empty() && rows[0] == Row{"2D#", "Transition Description", "Note"},
        "Unsupported transition header");
    Indexed indexed;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(row.size() == 3, "Invalid transition row width");
        uint32_t id = 0;
        const std::from_chars_result parsed = std::from_chars(row[0].data(), row[0].data() + row[0].size(), id);
        require(parsed.ec == std::errc() && parsed.ptr == row[0].data() + row[0].size()
            && id > 0 && id <= 65535 && indexed.emplace(id, row).second, "Invalid/duplicate transition ID");
    }
    require(!indexed.empty(), "Empty transition source");
    return indexed;
}

bool placeholder(const std::string &text)
{
    return text.empty() || text == "Placeholder Text";
}

void checkDialog(const Game::TransitionTable &table, const Row &row)
{
    using namespace Game;
    const uint32_t id = std::stoul(row[0]);
    EventRuntimeState state;
    // This state tests content assembly only; no map is loaded or moved.
    EventRuntimeState::PendingDialogueContext context;
    context.kind = DialogueContextKind::MapTransition;
    context.transitionTextId = id;
    context.titleOverride = "场景切换";
    state.pendingDialogueContext = context;
    const EventDialogContent content = buildEventDialogContent(state, 0, false, nullptr, nullptr, nullptr,
        nullptr, &table, nullptr, nullptr, nullptr, nullptr, 0.0f);
    require(content.isActive && content.title == "场景切换"
        && std::find(content.lines.begin(), content.lines.end(), row[1]) != content.lines.end(),
        "Transition dialog changed imported text");
}

void checkNotePresentation()
{
    using namespace Game;
    TransitionTable table;
    require(table.loadFromRows({{"2D#", "Transition Description", "Note"},
        {"1", "进入洞穴？", "Dragon Cave (area 12)"},
        {"2", "进入洞穴？", "Ilsingore's Cave 58"},
        {"3", "进入洞穴？", "Passage Under Regna 44"},
        {"4", "进入洞穴？", "Dragon Cave (area unknown)"}}), "Transition note fixture failed");
    EventRuntimeState state;
    EventRuntimeState::PendingDialogueContext context;
    context.kind = DialogueContextKind::MapTransition;
    context.transitionTextId = 1;
    EventRuntimeState::PendingMapMove move;
    move.mapName = "test.blv";
    context.transitionMapMove = move;
    MapStatsEntry destination;
    destination.fileName = "test.blv";
    destination.name = "中文洞穴";
    const std::vector<MapStatsEntry> maps = {destination};
    const std::vector<std::string> videos = {"drgn_cav", "drgn_cav", "rsub", ""};
    for (uint32_t index = 0; index < videos.size(); ++index)
    {
        context.transitionTextId = index + 1;
        state.pendingDialogueContext = context;
        const EventDialogContent content = buildEventDialogContent(state, 0, false, nullptr, nullptr, nullptr,
            nullptr, &table, nullptr, &maps, nullptr, nullptr, 0.0f);
        require(content.title == "中文洞穴" && content.videoName == videos[index],
            "Source notes leaked into the title or lost video-reference behavior");
    }
}
}

void convertTransitionResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Transition output must be outside source");
    require(!std::filesystem::exists(output), "Transition output already exists");
    size_t glyphs = 0;
    const Indexed base = read(root, "mmmerge.T.lod", false, glyphs);
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    const Indexed localized = chinese ? read(root, "10 LocZHCN.EnglishT.lod", true, glyphs) : Indexed{};
    Rows rows = {{"2D#", "Transition Description", "Note"}};
    std::vector<uint32_t> inactive;
    std::vector<uint32_t> untranslated;
    size_t translated = 0;
    size_t changedNotes = 0;
    size_t extraTranslations = 0;
    for (const auto &[id, row] : localized)
    {
        extraTranslations += !base.contains(id);
    }
    for (const auto &[id, original] : base)
    {
        const bool usable = localized.contains(id) && !placeholder(localized.at(id)[1]);
        if (placeholder(original[1]))
        {
            require(!usable, "Localization activates a placeholder transition");
            inactive.push_back(id);
            continue;
        }
        Row row = original;
        if (usable)
        {
            require(localized.at(id)[2] == original[2], "Translation changes transition identity note");
            row[1] = localized.at(id)[1];
            ++translated;
        }
        else
        {
            untranslated.push_back(id);
        }
        if (localized.contains(id) && localized.at(id)[2] != original[2])
        {
            ++changedNotes;
        }
        rows.push_back(std::move(row));
    }
    require(rows.size() > 1, "No active transition text");
    const std::string serialized = serializeTextRows(rows);
    require(parse(serialized) == rows, "Transition serialization changed fields");
    Game::TransitionTable table;
    require(table.loadFromRows(parse(serialized)), "Transition typed loading failed");
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        const Game::TransitionEntry *pEntry = table.get(std::stoul(row[0]));
        require(pEntry && pEntry->description == row[1] && pEntry->sourceNote == row[2] && pEntry->title.empty(),
            "Transition note became a display title or text changed");
        checkDialog(table, row);
    }
    for (uint32_t id : inactive)
    {
        require(!table.get(id), "Placeholder transition became active");
    }
    Game::TransitionTable legacy;
    require(legacy.loadFromRows({{"1", "Original description", "Original title"}})
        && legacy.get(1)->title == "Original title" && legacy.get(1)->sourceNote.empty(),
        "Legacy transition title semantics changed");
    checkNotePresentation();

    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"transition-text\",\"runtime_ready\":false,\"source_records\":" << base.size()
        << ",\"active_transitions\":" << rows.size() - 1 << ",\"translated\":" << translated
        << ",\"restored_chinese_glyphs\":" << glyphs << ",\"ignored_extra_translations\":" << extraTranslations
        << ",\"preserved_note_differences\":" << changedNotes << ",\"inactive_ids\":[";
    for (size_t index = 0; index < inactive.size(); ++index)
    {
        manifest << (index ? "," : "") << inactive[index];
    }
    manifest << "],\"untranslated_ids\":[";
    for (size_t index = 0; index < untranslated.size(); ++index)
    {
        manifest << (index ? "," : "") << untranslated[index];
    }
    manifest << "],\"typed_validation\":true,\"dialog_content_checks\":" << rows.size() - 1
        << ",\"rendered_dialog_validated\":false,\"map_transfer_validated\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh transition output");
    try
    {
        writeConvertedResource(output / "engine/data_tables/english/trans.txt",
            std::vector<uint8_t>(serialized.begin(), serialized.end()));
        const std::string report = manifest.str();
        writeConvertedResource(output / "transition_conversion.json",
            std::vector<uint8_t>(report.begin(), report.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
