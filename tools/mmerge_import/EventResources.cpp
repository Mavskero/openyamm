#include "EventResources.h"
#include "CrossContinentsResources.h"
#include "DimensionDoorResources.h"
#include "HarmondaleCastleResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "EventSupportSource.h"
#include "engine/TextTable.h"
#include "game/events/ScriptedEventProgram.h"
#include "tools/LegacyLuaExport.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace OpenYAMM::Import
{
namespace
{
using Rows = std::vector<std::vector<std::string>>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string lower(std::string text)
{
    for (char &character : text)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character += 'a' - 'A';
        }
    }
    return text;
}

bool safeName(const std::string &name)
{
    return !name.empty() && name != "." && name != ".."
        && name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-") == std::string::npos;
}

bool isOutside(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const std::filesystem::path relative = path.lexically_relative(root);
    return !relative.empty() && (relative.is_absolute() || *relative.begin() == "..");
}

std::optional<uint32_t> rowId(const std::vector<std::string> &row)
{
    if (row.empty())
    {
        return std::nullopt;
    }
    uint32_t id = 0;
    const auto parsed = std::from_chars(row[0].data(), row[0].data() + row[0].size(), id);
    return parsed.ec == std::errc() && parsed.ptr == row[0].data() + row[0].size()
        ? std::optional<uint32_t>(id) : std::nullopt;
}

Rows tableRows(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Missing converted lookup table: " + path.string());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid converted lookup table: " + path.string());
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (rowId(row))
        {
            rows.push_back(row);
        }
    }
    require(!rows.empty(), "Empty converted lookup table: " + path.string());
    return rows;
}

std::unordered_map<uint32_t, std::string> textLookup(const std::filesystem::path &path, size_t column)
{
    std::unordered_map<uint32_t, std::string> result;
    for (const std::vector<std::string> &row : tableRows(path))
    {
        require(row.size() > column, "Incomplete lookup row in " + path.string());
        require(result.emplace(*rowId(row), row[column]).second, "Duplicate lookup ID in " + path.string());
    }
    return result;
}

struct ArchiveMember
{
    std::filesystem::path archive;
    bool chinese = false;
};

using Members = std::map<std::string, ArchiveMember>;

Game::StrTable decodeStrings(const Members &members, const std::string &name, size_t &glyphs)
{
    Game::StrTable strings;
    const auto found = members.find(name);
    if (found == members.end())
    {
        return strings;
    }
    const std::vector<uint8_t> bytes = readLodText(found->second.archive, name);
    Game::StrTable encoded;
    require(encoded.loadFromNullSeparatedBytes(bytes), "Unterminated source STR: " + name);
    std::vector<uint8_t> decoded;
    for (const std::string &entry : encoded.getEntries())
    {
        const std::string text = decodeLegacyText({entry.begin(), entry.end()}, found->second.chinese, glyphs);
        decoded.insert(decoded.end(), text.begin(), text.end());
        decoded.push_back(0);
    }
    require(strings.loadFromNullSeparatedBytes(decoded), "Invalid decoded STR: " + name);
    return strings;
}

struct MapSource
{
    std::string world;
    std::string file;
    std::string name;
};

std::optional<uint16_t> dimensionDoorReplacementEventId(const std::string &mapStem)
{
    if (mapStem == "out07")
    {
        return 500;
    }
    if (mapStem == "outb3")
    {
        return 105;
    }
    if (mapStem == "7out04")
    {
        return 504;
    }
    if (mapStem == "out09")
    {
        return 6;
    }
    if (mapStem == "oute3")
    {
        return 140;
    }
    return std::nullopt;
}

void validateInstruction(const Game::EvtInstruction &instruction, const std::string &context)
{
    const uint8_t opcode = static_cast<uint8_t>(instruction.opcode);
    require(opcode >= 1 && opcode <= 68 && opcode != 20 && opcode != 27 && opcode != 28,
        "Unimplemented opcode " + std::to_string(opcode) + " in " + context);
    // MM8 field widths, including terminators for the four inline-string commands.
    // MMerge's MM6/MM7 EVT archives already use this format; their IDs must not be remapped again.
    constexpr std::array<size_t, 69> minimumPayload = {
        0, 1, 4, 12, 1, 1, 28, 1, 2, 6, 2, 5, 3, 6, 7, 2,
        6, 6, 6, 23, 0, 27, 4, 9, 9, 6, 13, 0, 0, 4, 4, 10,
        5, 1, 22, 1, 1, 1, 10, 9, 8, 6, 4, 7, 7, 1, 1, 8,
        8, 9, 8, 8, 8, 1, 8, 8, 2, 9, 9, 2, 9, 8, 3, 4,
        7, 6, 8, 9, 5,
    };
    const std::vector<uint8_t> &payload = instruction.rawPayload;
    require(payload.size() >= minimumPayload[opcode],
        "Truncated opcode payload " + std::to_string(opcode) + " in " + context);
    if (opcode == 6 || opcode == 11 || opcode == 12 || opcode == 13)
    {
        const size_t stringOffset = minimumPayload[opcode] - 1;
        require(std::find(payload.begin() + stringOffset, payload.end(), 0) != payload.end(),
            "Unterminated opcode string payload in " + context);
    }
}
}

void convertEventResources(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::string &mapStem)
{
    convertEventResourcesForMaps(source, convertedBase, destination,
        mapStem.empty() ? std::vector<std::string>{} : std::vector<std::string>{mapStem});
}

void convertEventResourcesForMaps(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::vector<std::string> &mapStems,
    const ImportCancelCheck &cancelled)
{
    checkImportCancellation(cancelled);
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path base = std::filesystem::canonical(convertedBase);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    require(!std::filesystem::exists(output), "Event destination already exists");
    require(isOutside(output, root) && isOutside(output, base), "Event output must be outside both input roots");
    std::set<std::string> requested;
    for (const std::string &stem : mapStems)
    {
        require(!stem.empty() && safeName(lower(stem)) && requested.insert(lower(stem)).second,
            "Invalid or duplicate requested map stem");
    }
    std::set<std::string> remaining = requested;

    Members members;
    Members baseMembers;
    for (const char *pName : {"mm6.T.lod", "mm7.T.lod", "mmmerge.T.lod", "10 LocZHCN.EnglishT.lod"})
    {
        const std::filesystem::path archive = root / "Data" / pName;
        if (!std::filesystem::exists(archive))
        {
            continue;
        }
        const bool chinese = std::string_view(pName) == "10 LocZHCN.EnglishT.lod";
        for (const std::string &member : listLodMembers(archive))
        {
            const std::string name = lower(member);
            if (name.ends_with(".evt") || name.ends_with(".str"))
            {
                require(safeName(name), "Unsafe event member name");
                require(!chinese || !name.ends_with(".evt"), "Language archive must not replace event mechanics");
                members[name] = {archive, chinese};
                if (!chinese)
                {
                    baseMembers[name] = {archive, false};
                }
            }
        }
    }
    require(members.contains("global.evt"), "Missing merged Global.evt");

    const std::filesystem::path tables = base / "engine/data_tables";
    Game::LegacyLuaExportLookups lookups;
    lookups.houseNames = textLookup(tables / "house_data.txt", 5);
    lookups.npcTexts = textLookup(tables / "npc_topic_text.txt", 1);
    lookups.npcNames = textLookup(tables / "npc.txt", 1);
    lookups.npcTopicNames = textLookup(tables / "npc_topic.txt", 1);
    lookups.questNotes = textLookup(tables / "english/quests.txt", 1);

    const std::optional<CrossContinentsIntroProfile> crossContinents = loadCrossContinentsIntroProfile(root);
    if (crossContinents)
    {
        require(lookups.npcNames.contains(803) && !lookups.npcNames.at(803).empty(),
            "Cross-continent introduction requires NPC 803");
        const std::unordered_map<uint32_t, std::string> greetings = textLookup(tables / "npc_greet.txt", 1);
        require(greetings.contains(328) && !greetings.at(328).empty(),
            "Missing cross-continent introduction completion greeting 328");
        for (uint32_t id : {1778u, 1779u, 1780u})
        {
            require(lookups.npcTopicNames.contains(id) && !lookups.npcTopicNames.at(id).empty(),
                "Missing cross-continent introduction topic " + std::to_string(id));
        }
        for (uint32_t id : {2149u, 2150u, 2151u, 2152u})
        {
            require(lookups.npcTexts.contains(id) && !lookups.npcTexts.at(id).empty(),
                "Missing cross-continent introduction text " + std::to_string(id));
        }
    }
    const std::optional<DimensionDoorProfile> dimensionDoor = loadDimensionDoorProfile(root);
    const std::optional<HarmondaleCastleProfile> harmondaleCastle = loadHarmondaleCastleProfile(root);

    std::vector<MapSource> maps = {{"", "global", ""}};
    for (const std::vector<std::string> &row : tableRows(tables / "map_stats.txt"))
    {
        require(row.size() >= 35, "Converted map registry must include explicit world IDs");
        const std::string file = lower(row[2]);
        const std::string world = lower(row[34]);
        require(safeName(file) && safeName(world), "Unsafe map registry identity");
        lookups.mapNamesByFile[file] = row[1];
        const std::string stem = std::filesystem::path(file).stem().string();
        lookups.mapNamesByFile[stem] = row[1];
        if (requested.empty() || requested.contains(stem))
        {
            require(members.contains(stem + ".evt"), "Missing map EVT: " + file);
            maps.push_back({world, file, row[1]});
            remaining.erase(stem);
        }
    }
    require(maps.size() > 1, "Requested map is absent from the converted registry");
    require(remaining.empty(), "One or more requested maps are absent from the converted registry");

    std::map<std::string, const DimensionDoorMapOverlay *> dimensionDoorOverlays;
    if (dimensionDoor)
    {
        for (const DimensionDoorMapOverlay &overlay : dimensionDoor->maps)
        {
            dimensionDoorOverlays.emplace(overlay.mapStem, &overlay);
        }
    }

    std::vector<std::pair<const MapSource *, const DimensionDoorMapOverlay *>> selectedDimensionDoorOverlays;
    for (const MapSource &map : maps)
    {
        const std::string stem = std::filesystem::path(map.file).stem().string();
        const auto overlay = dimensionDoorOverlays.find(stem);
        if (!map.world.empty() && overlay != dimensionDoorOverlays.end())
        {
            selectedDimensionDoorOverlays.emplace_back(&map, overlay->second);
        }
    }
    const auto selectedHarmondaleCastle = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return harmondaleCastle && map.world == "mm7"
            && std::filesystem::path(map.file).stem().string() == harmondaleCastle->mapStem;
    });

    std::filesystem::create_directories(output);
    try
    {
        writeConvertedResource(output / "engine/scripts/common/event_support.lua",
            {EventSupportSource.begin(), EventSupportSource.end()});
        std::ostringstream manifest;
        manifest << "{\"schema\":1,\"stage\":\"merged-events\",\"legacy_event_version\":8,"
            "\"runtime_ready\":false,\"mmextension_lua_overlays_applied\":false,\"map_filter\":"
            << std::quoted(requested.size() == 1 ? *requested.begin() : "") << ",\"map_filters\":[";
        size_t filterIndex = 0;
        for (const std::string &stem : requested)
        {
            manifest << (filterIndex++ ? "," : "") << std::quoted(stem);
        }
        manifest << "],\"selective_overlays\":[";
        bool firstSelectiveOverlay = true;
        if (crossContinents)
        {
            manifest << "{\"profile\":\"mmmerge-cross-continents-intro-v1\","
                "\"source\":\"Scripts/Global/Quest_CrossContinents.lua\",\"source_sha256\":"
                << std::quoted(crossContinents->sourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_cross_continents_intro.lua\","
                "\"scope\":\"Verdant introduction and NPC reentry only\",\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (dimensionDoor && !selectedDimensionDoorOverlays.empty())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-dimension-door-activation-v1\","
                "\"source\":\"Scripts/General/2_OutdoorTravels.lua\",\"source_sha256\":"
                << std::quoted(dimensionDoor->sourceSha256)
                << ",\"timer_source\":\"Scripts/Core/timers.lua\",\"timer_source_sha256\":"
                << std::quoted(dimensionDoor->timerSourceSha256)
                << ",\"scope\":\"seven audited map activation and final-range hooks only\","
                "\"complete_cross_continents\":false,\"map_overlays\":[";
            size_t overlayIndex = 0;
            for (const auto &[pMap, pOverlay] : selectedDimensionDoorOverlays)
            {
                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / pMap->world
                    / "events/maps" / (pOverlay->mapStem + "_mmmerge_dimension_door.lua");
                manifest << (overlayIndex++ ? "," : "") << "{\"map_stem\":" << std::quoted(pOverlay->mapStem)
                    << ",\"source\":" << std::quoted("Scripts/Maps/" + pOverlay->mapStem + ".lua")
                    << ",\"source_sha256\":" << std::quoted(pOverlay->sourceSha256)
                    << ",\"path\":" << std::quoted(overlayPath.generic_string()) << '}';
            }
            manifest << "]}";
            firstSelectiveOverlay = false;
        }
        if (harmondaleCastle && selectedHarmondaleCastle != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                / (harmondaleCastle->mapStem + "_mmmerge_castle_history.lua");
            manifest << "{\"profile\":\"mmmerge-harmondale-castle-entrance-v1\","
                "\"source\":\"Scripts/Maps/7out02.lua\",\"source_sha256\":"
                << std::quoted(harmondaleCastle->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"scope\":\"event301 entrance gate, initial history and castle transition only\"}";
            firstSelectiveOverlay = false;
        }
        manifest << "],\"programs\":[";
        bool first = true;
        for (const MapSource &map : maps)
        {
            checkImportCancellation(cancelled);
            const bool global = map.world.empty();
            const std::string stem = std::filesystem::path(map.file).stem().string();
            const std::string evtName = stem + ".evt";
            const std::string strName = stem + ".str";
            Game::EvtProgram evt;
            const std::vector<uint8_t> bytes = readLodText(members.at(evtName).archive, evtName);
            require(!bytes.empty() && evt.loadFromBytes(bytes), "Malformed EVT: " + evtName);
            for (const Game::EvtEvent &event : evt.getEvents())
            {
                for (const Game::EvtInstruction &instruction : event.instructions)
                {
                    validateInstruction(instruction, evtName + " event " + std::to_string(event.eventId)
                        + " step " + std::to_string(instruction.step));
                }
            }
            size_t glyphs = 0;
            const Game::StrTable strings = decodeStrings(members, strName, glyphs);
            if (members.contains(strName) && members.at(strName).chinese)
            {
                size_t unusedGlyphs = 0;
                const Game::StrTable originalStrings = decodeStrings(baseMembers, strName, unusedGlyphs);
                require(originalStrings.getEntries().size() == strings.getEntries().size(),
                    "Localized STR slot count differs from base: " + strName);
            }
            Game::LegacyLuaExportLookups mapLookups = lookups;
            if (!global)
            {
                mapLookups.mapName = map.name;
                mapLookups.sourceMapFile = map.file;
            }
            const std::string lua = Game::generateLegacyEventLuaChunk(evt, strings, mapLookups,
                global ? Game::LegacyLuaExportScope::Global : Game::LegacyLuaExportScope::Map,
                Game::LegacyEventVersion::Mm8);
            std::string error;
            const std::optional<Game::ScriptedEventProgram> program = Game::ScriptedEventProgram::loadFromLuaText(
                std::string(EventSupportSource) + "\n" + lua, "@" + evtName,
                global ? Game::ScriptedEventScope::Global : Game::ScriptedEventScope::Map, error);
            require(program.has_value(), "Generated Lua rejected for " + evtName + ": " + error);
            if (global && crossContinents)
            {
                require(!program->hasEvent(65500), "Cross-continent NPC-enter hook ID conflicts with Global.evt");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + crossContinents->luaSource,
                    "@Global.lua+cross-continent-intro", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Cross-continent introduction Lua rejected: " + error);
                for (uint16_t id : {1778, 1779, 1780, 65500})
                {
                    require(overlaid->hasEvent(id), "Cross-continent introduction handler was not registered");
                }
                writeConvertedResource(output / "engine/events/Global_mmmerge_cross_continents_intro.lua",
                    {crossContinents->luaSource.begin(), crossContinents->luaSource.end()});
            }
            const auto dimensionDoorOverlay = dimensionDoorOverlays.find(stem);
            if (!global && dimensionDoorOverlay != dimensionDoorOverlays.end())
            {
                const DimensionDoorMapOverlay &overlay = *dimensionDoorOverlay->second;
                const bool usesFirstReservedId = overlay.mapStem != "oute3";
                const bool usesSecondReservedId = overlay.mapStem == "out02";
                require(!usesFirstReservedId || !program->hasEvent(65490),
                    "Dimension Door hook ID 65490 conflicts with " + evtName);
                require(!usesSecondReservedId || !program->hasEvent(65491),
                    "Dimension Door hook ID 65491 conflicts with " + evtName);

                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay.luaSource,
                    "@" + evtName + "+dimension-door", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Dimension Door activation Lua rejected for " + evtName + ": " + error);

                if (usesFirstReservedId)
                {
                    require(overlaid->hasEvent(65490), "Dimension Door activation hook 65490 was not registered");
                }
                if (usesSecondReservedId)
                {
                    require(overlaid->hasEvent(65491), "Dimension Door Town Portal hook 65491 was not registered");
                }
                if (overlay.mapStem == "out01" || overlay.mapStem == "out07" || overlay.mapStem == "outb3"
                    || overlay.mapStem == "7out04" || overlay.mapStem == "out02")
                {
                    require(std::find(overlaid->outdoorStepHookEventIds().begin(),
                        overlaid->outdoorStepHookEventIds().end(), 65490)
                            != overlaid->outdoorStepHookEventIds().end(),
                        "Dimension Door outdoor-step hook was not registered for " + evtName);
                }
                if (overlay.mapStem == "out02")
                {
                    require(std::find(overlaid->townPortalHookEventIds().begin(),
                        overlaid->townPortalHookEventIds().end(), 65491)
                            != overlaid->townPortalHookEventIds().end(),
                        "Dimension Door Town Portal hook was not registered for " + evtName);
                }
                if (overlay.mapStem == "7out04")
                {
                    const auto modelEvent = overlaid->outdoorModelEvents().find("ClL1_W");
                    require(modelEvent != overlaid->outdoorModelEvents().end() && modelEvent->second == 504,
                        "Dimension Door model-face event was not registered for " + evtName);
                }
                if (overlay.mapStem == "out09")
                {
                    const auto timer = std::find_if(
                        overlaid->timerTriggers().begin(),
                        overlaid->timerTriggers().end(),
                        [](const Game::ScriptedEventProgram::TimerTrigger &trigger)
                        {
                            return trigger.eventId == 65490;
                        });
                    require(timer != overlaid->timerTriggers().end()
                            && timer->scheduleKind == Game::ScriptedEventTimerScheduleKind::Interval
                            && timer->repeating && timer->intervalGameMinutes == 1.0 && timer->startMinute == 3,
                        "Dimension Door exact interval timer was not registered for " + evtName);
                }

                if (const std::optional<uint16_t> eventId = dimensionDoorReplacementEventId(overlay.mapStem))
                {
                    require(overlaid->hasEvent(*eventId),
                        "Dimension Door replacement event was not registered for " + evtName);
                }

                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / map.world
                    / "events/maps" / (stem + "_mmmerge_dimension_door.lua");
                writeConvertedResource(output / overlayPath, {overlay.luaSource.begin(), overlay.luaSource.end()});
            }
            if (!global && harmondaleCastle && map.world == "mm7" && stem == harmondaleCastle->mapStem)
            {
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + harmondaleCastle->luaSource,
                    "@" + evtName + "+harmondale-castle", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Castle Harmondale entrance Lua rejected: " + error);
                require(overlaid->hasEvent(301), "Castle Harmondale entrance replacement was not registered");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / map.world
                    / "events/maps" / (stem + "_mmmerge_castle_history.lua");
                writeConvertedResource(output / overlayPath,
                    {harmondaleCastle->luaSource.begin(), harmondaleCastle->luaSource.end()});
            }
            for (const Game::EvtEvent &event : evt.getEvents())
            {
                require(program->hasEvent(event.eventId) || program->isHintOnlyEvent(event.eventId),
                    "Export dropped event " + std::to_string(event.eventId) + " in " + evtName);
            }
            const std::filesystem::path path = global ? std::filesystem::path("engine/events/Global.lua")
                : std::filesystem::path("worlds") / map.world / "events/maps" / (stem + ".lua");
            writeConvertedResource(output / path, {lua.begin(), lua.end()});
            if (!first)
            {
                manifest << ',';
            }
            first = false;
            manifest << "{\"path\":" << std::quoted(path.generic_string()) << ",\"evt_archive\":"
                << std::quoted(members.at(evtName).archive.filename().string()) << ",\"str_archive\":"
                << std::quoted(members.contains(strName) ? members.at(strName).archive.filename().string() : "")
                << ",\"source_events\":" << evt.getEvents().size() << ",\"source_instructions\":"
                << evt.getInstructionCount() << ",\"string_slots\":" << strings.getEntries().size()
                << ",\"restored_glyphs\":" << glyphs << ",\"lua_load_passed\":true}";
        }
        manifest << "]}\n";
        const std::string report = manifest.str();
        writeConvertedResource(output / "event_conversion.json", {report.begin(), report.end()});
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
