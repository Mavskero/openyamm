#include "SceneResources.h"
#include "BreachAlvarCompanionResources.h"
#include "BreachMazeEventSource.h"
#include "LodResources.h"
#include "SpriteFrameResources.h"
#include "SurfaceResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/audio/SoundCatalog.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/maps/MapDeltaData.h"
#include "game/maps/TerrainTileData.h"
#include "game/tables/HouseTable.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/SpriteTables.h"
#include "game/tables/SurfaceMaterialTable.h"
#include "game/tables/TextureFrameTable.h"
#include "tools/legacy_events/StrTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Path = std::filesystem::path;
using Rows = std::vector<std::vector<std::string>>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool outside(const Path &path, const Path &root)
{
    const Path relative = path.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() == "..";
}

std::string readText(const Path &path)
{
    require(std::filesystem::file_size(path) <= 16 * 1024 * 1024, "Scene table exceeds import limit");
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "Cannot read scene table: " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

Rows readRows(const Path &path)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(readText(path));
    require(table && table->getRowCount() != 0, "Invalid scene table: " + path.string());
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    return rows;
}

std::string lower(std::string name)
{
    for (char &letter : name)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return name;
}

void writeText(const Path &path, const std::string &text)
{
    writeConvertedResource(path, {text.begin(), text.end()});
}

std::string luaText(const std::string &text)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char byte : text)
    {
        if (byte == '"' || byte == '\\') out << '\\' << char(byte);
        else if (byte < 32 || byte == 127) out << '\\' << std::setw(3) << std::setfill('0') << uint32_t(byte);
        else out << char(byte);
    }
    out << '"';
    return out.str();
}

std::string breachNativeTexts(const Path &root, bool chinese, const std::string &stem,
    size_t minimumEntries, std::optional<uint32_t> globalTextId = std::nullopt)
{
    const Path archive = root / "Data" / (chinese ? "10 LocZHCN.EnglishT.lod" : "mmmerge.T.lod");
    std::vector<uint8_t> stringBytes = readLodText(archive, stem + ".str");
    require(!stringBytes.empty(), "Empty original " + stem + " STR");
    // Some original STRs end their final entry at the LOD payload boundary without a trailing NUL.
    // Supply that boundary to the strict shared parser; the read-only source remains untouched.
    if (stringBytes.back() != 0) stringBytes.push_back(0);
    Game::StrTable strings;
    require(strings.loadFromNullSeparatedBytes(stringBytes)
        && strings.getEntries().size() >= minimumEntries, "Incomplete original " + stem + " STR");
    size_t restoredGlyphs = 0;
    std::ostringstream source;
    source << "-- Native text projection: original " << stem << ".str, same active language.\n"
        << "evt.strings = {\n";
    for (size_t index = 0; index < strings.getEntries().size(); ++index)
    {
        const std::string &raw = strings.getEntries()[index];
        source << "  [" << index << "] = "
            << luaText(decodeLegacyText({raw.begin(), raw.end()}, chinese, restoredGlyphs)) << ",\n";
    }
    source << "}\nevt.importedGlobalText = {";
    if (globalTextId)
    {
        const auto global = Engine::TextTable::parseTabSeparated(
            decodeLegacyText(readLodText(archive, "global.txt"), chinese, restoredGlyphs));
        require(global.has_value(), "Invalid original GlobalTxt table");
        std::optional<std::string> value;
        for (size_t index = 0; index < global->getRowCount(); ++index)
        {
            const auto &row = global->getRow(index);
            if (row.size() >= 2 && row[0] == std::to_string(*globalTextId))
            {
                require(!value, "Duplicate GlobalTxt" + std::to_string(*globalTextId));
                value = row[1];
            }
        }
        require(value && !value->empty(), "Missing original GlobalTxt" + std::to_string(*globalTextId));
        source << '[' << *globalTextId << "] = " << luaText(*value);
    }
    source << "}\n";
    return source.str();
}

std::string minimapArrowPrefix(const std::string &worldId)
{
    if (worldId == "mm6")
    {
        return "6mapdir";
    }
    if (worldId == "mm7")
    {
        return "7mapdir";
    }
    require(worldId == "mm8", "Unsupported minimap arrow source world");
    return "mapdir";
}

struct MergeSceneProfile
{
    std::string sourceScript;
    std::string sha256;
    std::set<std::string> textures;
};

// These scenes have Map Lua, not EVT binaries. Only explicit source projections execute; importing
// their remaining original scripts as provenance does not provide general Merge Lua compatibility.
MergeSceneProfile mergeSceneProfile(const Path &root, const std::string &file)
{
    const std::map<std::string, MergeSceneProfile> profiles = {
        {"breach.odm", {"Breach", "e3ef00b2de86c3f795fe18739adc978b41d8c7bd3c7aac08bafd90f4116a9eac", {}}},
        {"bralvar.odm", {"BrAlvar", "947567325601e0e120c03125b5f3087c15d311391c80c1949312e7a6feadc7a3", {}}},
        {"brbase.blv", {"BrBase", "58d0978f2d8683d3d94cfeb6048917c0cc61d9818a8f272ca1baca2e6005d60b",
            {"7c1fl", "7cdb", "7t1s2bs4", "7tfb1", "7trim9_32", "7trimd", "rmzdra", "rmzdrb",
                "bookcas1", "cwb", "cwb1", "t65a03", "t65b03", "t65b10", "t65c02"}}},
    };
    const auto found = profiles.find(file);
    require(found != profiles.end(), "Merge scene is outside the audited source profile: " + file);
    MergeSceneProfile profile = found->second;
    profile.sourceScript = "Scripts/Maps/" + profile.sourceScript + ".lua";
    const Path path = std::filesystem::canonical(root / profile.sourceScript);
    const Path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Merge scene source script escapes input root");
    const std::string script = readText(path);
    require(!script.empty(), "Empty Merge scene source script");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    require(digest.str() == profile.sha256, "Unsupported Merge scene source fingerprint: " + file);
    return profile;
}

std::string breachTowerMechanisms(const Game::OutdoorMapData &map, const Path &base,
    const std::string &file, const MergeSceneProfile &profile)
{
    require(file == "bralvar.odm" || file == "breach.odm", "Unsupported Breach tower source: " + file);
    const bool isAlvar = file == "bralvar.odm";
    struct ModelRule
    {
        std::string name;
        uint16_t eventId;
        int32_t dz;
        uint32_t moveTimeMs;
        uint32_t startSoundId;
        uint32_t stopSoundId;
    };
    // Source const.Minute advances in two real seconds at the ordinary outdoor clock rate.
    // Closed models are stored at their source position; opening moves them by the source dz.
    std::array<ModelRule, 28> rules = {{
        {"Time_1", 101, -10, 2000, 0, 0},
        {"Time_2", 102, -10, 2000, 0, 0},
        {"Time_3", 103, -10, 2000, 0, 0},
        {"Elev_1_Door", 104, 250, 2000, 185, 178},
        {"Elev_2_Door", 105, 250, 2000, 185, 178},
        {"Elev_3_Door", 106, 250, 2000, 185, 178},
        {"SubTower_Door_1", 107, 250, 2000, 185, 178},
        {"SubTower_Door_2", 108, 250, 2000, 185, 178},
        {"SubTower_Door_3", 109, 250, 2000, 185, 178},
        {"LibDoor_1", 110, 300, 2000, 177, 168},
        {"LibDoor_2", 111, 300, 2000, 177, 168},
        {"LibDoor_3", 112, 300, 2000, 177, 168},
        {"LibDoor_4", 113, 300, 2000, 177, 168},
        {"Mus_Door_2", 114, 300, 2000, 177, 178},
        {"Mus_Door_3", 115, 300, 2000, 177, 178},
        {"Mus_Door_4", 116, 300, 2000, 177, 178},
        {"Mus_Door_1", 117, 300, 2000, 185, 178},
        {"RoomDoor_1", 118, 200, 1000, 0, 170},
        {"RoomDoor_2", 119, 200, 1000, 0, 170},
        {"RoomDoor_3", 120, 200, 1000, 0, 170},
        {"RoomDoor_4", 121, 200, 1000, 0, 170},
        {"RoomDoor_5", 122, 200, 1000, 0, 170},
        {"RoomDoor_6", 123, 200, 1000, 0, 170},
        {"RoomDoor_7", 124, 200, 1000, 0, 170},
        {"RoomElev", 125, 455, 4000, 177, 170},
        {"Elev_1_Button", 126, 5, 500, 177, 0},
        {"Elev_2_Button", 127, 5, 500, 177, 0},
        {"Elev_3_Button", 128, 5, 500, 177, 0},
    }};
    if (!isAlvar)
    {
        // Breach.lua 268-284 uses Mus_Door_1..4 in order, all with StartSound177.
        for (size_t index = 0; index < 4; ++index)
        {
            rules[13 + index].name = "Mus_Door_" + std::to_string(index + 1);
            rules[13 + index].startSoundId = 177;
        }
    }
    Game::SoundCatalog sounds;
    std::string error;
    require(sounds.loadFromScopedRows(readRows(base / "engine/data_tables/sounds.txt"), {}, error),
        "Cannot read shared tower sound catalog: " + error);
    const auto soundName = [&sounds](uint32_t id)
    {
        const Game::SoundCatalogEntry *pSound = sounds.findById(id);
        require(pSound && !pSound->name.empty(), "Missing original tower sound ID " + std::to_string(id));
        return lower(pSound->name) + ".wav";
    };

    std::ostringstream yaml;
    yaml << "# " << profile.sourceScript
        << "; original model positions and raw ODM/DDM are retained.\n"
        << "format_version: 1\nkind: outdoor_mechanisms\nmap: " << file << "\nsource:\n"
        << "  script: " << profile.sourceScript << "\n  sha256: " << std::quoted(profile.sha256)
        << "\nruntime_restrictions:\n";
    if (isAlvar)
    {
        yaml << "  allow_lloyds_beacon: false\n";
    }
    // Breach.lua 355-358 forces Dimension Door but does not prohibit Lloyd's Beacon.
    yaml << "  town_portal_uses_dimension_door: true\nmechanisms:\n";
    for (const ModelRule &rule : rules)
    {
        std::optional<size_t> modelIndex;
        for (size_t index = 0; index < map.bmodels.size(); ++index)
        {
            if (map.bmodels[index].name == rule.name)
            {
                require(!modelIndex, "Ambiguous original tower model: " + rule.name);
                modelIndex = index;
            }
        }
        require(modelIndex && !map.bmodels[*modelIndex].faces.empty(),
            "Missing original tower model: " + rule.name);
        yaml << "  - mechanism_id: " << rule.eventId << "\n    event_id: " << rule.eventId
            << "\n    source_object_index: " << *modelIndex
            << "\n    source_class: OutdoorAnimObjects.SetDoor\n    source_name: " << std::quoted(rule.name)
            << "\n    kind: " << (rule.eventId <= 103 || rule.dz == 5 ? "linear_button" : "linear_door")
            << "\n    binding:\n      target_kind: odm_bmodel\n      bmodel_index: " << *modelIndex
            << "\n      bmodel_name: " << std::quoted(rule.name)
            << "\n      confidence: exact_source_model_name\n    motion:\n      move_time_ms: " << rule.moveTimeMs
            << "\n      linear:\n        delta_openyamm: {x: 0, y: 0, z: " << rule.dz << "}\n"
            << "    activation:\n      start_open: false\n      move_party: false\n";
        if (rule.startSoundId != 0 || rule.stopSoundId != 0)
        {
            yaml << "    sounds:\n";
        }
        if (rule.startSoundId != 0)
        {
            const std::string startSound = soundName(rule.startSoundId);
            yaml << "      open_start: " << std::quoted(startSound)
                << "\n      close_start: " << std::quoted(startSound) << '\n';
        }
        if (rule.stopSoundId != 0)
        {
            const std::string stopSound = soundName(rule.stopSoundId);
            yaml << "      open_stop: " << std::quoted(stopSound)
                << "\n      close_stop: " << std::quoted(stopSound) << '\n';
        }
    }
    return yaml.str();
}

std::string presentationWorld(const Game::MapStatsEntry &map, SceneImportMode mode)
{
    if (mode == SceneImportMode::ResourceInspection || mode == SceneImportMode::NativeMergeGameplay)
    {
        require(map.worldId == "mmmerge", "Merge source profile requires original mmmerge ownership");
        return "mm8";
    }
    require(map.worldId == "mm6" || map.worldId == "mm7" || map.worldId == "mm8",
        "Scene has no supported gameplay world: " + map.fileName);
    return map.worldId;
}

struct Archives
{
    std::vector<Path> paths;
    std::set<std::string> members;

    Archives(const Path &source, std::initializer_list<const char *> names)
    {
        for (const char *pName : names)
        {
            if (!std::filesystem::exists(source / "Data" / pName))
            {
                continue;
            }
            const Path path = std::filesystem::canonical(source / "Data" / pName);
            const Path relative = path.lexically_relative(source);
            require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
                "Scene archive escapes source");
            paths.push_back(path);
            for (const std::string &member : listLodMembers(path))
            {
                members.insert(lower(member));
            }
        }
    }
};

void stringArray(std::ostream &out, const std::set<std::string> &values)
{
    out << '[';
    size_t index = 0;
    for (const std::string &value : values)
    {
        out << (index++ ? "," : "") << std::quoted(value);
    }
    out << ']';
}
}

SceneMapSelection discoverSceneMapFiles(const Path &source, const Path &convertedBase, const std::string &worldId)
{
    require(worldId == "mm6" || worldId == "mm7" || worldId == "mm8", "Unsupported scene world");
    Game::MapStats registry;
    require(registry.loadFromRows(readRows(convertedBase / "engine/data_tables/map_stats.txt")),
        "Cannot read shared map registry");
    std::set<std::string> members;
    for (const std::string &name : {worldId + ".games.lod", std::string("patch.games.lod")})
    {
        if (std::filesystem::exists(source / "Data" / name))
        {
            for (const std::string &member : listLodMembers(source / "Data" / name))
            {
                members.insert(lower(member));
            }
        }
    }
    SceneMapSelection result;
    for (const Game::MapStatsEntry &map : registry.getEntries())
    {
        if (map.worldId != worldId)
        {
            continue;
        }
        const std::string file = lower(map.fileName);
        const std::string companion = Path(file).stem().string() + (file.ends_with(".odm") ? ".ddm" : ".dlv");
        (members.contains(file) && members.contains(companion) ? result.available : result.missing).push_back(file);
    }
    return result;
}

void convertSceneResources(const Path &source, const Path &convertedBase, const Path &destination,
    const std::vector<std::string> &mapFiles, const ImportCancelCheck &cancelled, SceneImportMode mode, bool chinese)
{
    checkImportCancellation(cancelled);
    using namespace Game;
    const Path root = std::filesystem::canonical(source);
    const Path base = std::filesystem::canonical(convertedBase);
    const Path output = std::filesystem::weakly_canonical(destination);
    require(!std::filesystem::exists(output) && outside(output, root) && outside(output, base),
        "Scene output must be new and outside both inputs");
    require(!mapFiles.empty(), "No scene maps requested");
    const bool nativeMergeSource = mode == SceneImportMode::ResourceInspection
        || mode == SceneImportMode::NativeMergeGameplay;
    const Path resources = mode == SceneImportMode::ResourceInspection
        ? output / "inspection/merge-scenes" : output;
    const Path tables = base / "engine/data_tables";
    MapStats registry;
    MonsterTable monsters;
    DecorationTable decorations;
    ObjectTable objects;
    MergedContinentSettingTable continents;
    MergedBolsterMapTable mapRules;
    SpriteFrameTable sprites;
    std::string error;
    require(registry.loadFromRows(readRows(tables / "map_stats.txt")), "Cannot read shared map registry");
    require(monsters.loadEntriesFromRows(readRows(tables / "monster_descriptors.txt")), "Cannot read monsters");
    require(monsters.loadStatsFromRows(readRows(tables / "monster_data.txt")), "Cannot read monster statistics");
    require(decorations.loadRows(readRows(tables / "decoration_data.txt")), "Cannot read decoration registry");
    require(objects.loadRows(readRows(tables / "object_list.txt")), "Cannot read object registry");
    require(continents.loadFromRows(readRows(tables / "continent_settings.txt"))
        && mapRules.loadFromRows(readRows(tables / "bolster_maps.txt")), "Cannot read scene sky settings");
    require(sprites.loadFromYaml(readText(base / "engine/rendering/sprite_frame_data_common.yml"), error),
        "Cannot read complete shared sprite frames: " + error);
    std::map<std::string, Rows> terrainRows;
    for (const auto &[world, name] : std::map<std::string, std::string>{
            {"mm6", "Tile3.txt"}, {"mm7", "Tile2.txt"}, {"mm8", "Tile.txt"}})
    {
        terrainRows[world] = convertTerrainRows(readRows(root / "Data/Tables" / name));
    }
    const Rows textureRows = convertTextureFrameRows(readRows(root / "Data/Tables/TFT.txt"));
    TextureFrameTable textureFrames;
    require(textureFrames.loadRows(textureRows), "Converted TFT could not load");
    const Rows waterRows = readRows(root / "Data/Tables/HW water textures.txt");
    const std::string waterYaml = convertHardwareWaterMaterials(waterRows,
        readText(root / "Scripts/General/MiscTweaks.lua"));
    SurfaceMaterialTable waterMaterials;
    require(waterMaterials.loadFromYaml(waterYaml, error), "Converted water materials could not load: " + error);
    Rows spriteRows = readRows(root / "Data/Tables/SFT.txt");
    require(spriteRows.front().size() == 24 && spriteRows.front()[1] == "GroupName", "Unknown SFT schema");
    spriteRows.erase(spriteRows.begin());
    const Archives bitmaps(root, {"bitmaps.lod", "mm6.bitmaps.lod", "mm7.bitmaps.lod", "mm8.bitmaps.lod",
        "mmmerge.bitmaps.lod", "00 patch.bitmaps.lod"});
    const Archives spriteArchives(root, {"sprites.lod", "mm6.sprites.lod", "mm7.sprites.lod", "mmmerge.sprites.lod"});
    const Archives iconArchives(root, {"icons.lod", "mm6.icons.lod", "mm7.icons.lod", "mm8.icons.lod",
        "mmmerge.icons.lod", "00 patch.icons.lod"});
    std::set<std::string> minimapNames;
    std::set<std::string> mm8MinimapNames;
    for (const std::string &file : mapFiles)
    {
        const MapStatsEntry *pMap = registry.findByFileName(lower(file));
        require(pMap != nullptr, "Scene has no registered world: " + file);
        const std::string formatWorld = presentationWorld(*pMap, mode);
        if (nativeMergeSource)
        {
            mergeSceneProfile(root, lower(file));
        }
        if (lower(Path(file).extension().string()) == ".odm")
        {
            minimapNames.insert(lower(Path(file).stem().string()));
            if (formatWorld == "mm8")
            {
                mm8MinimapNames.insert(lower(Path(file).stem().string()));
            }
        }
        for (int direction = 1; direction <= 8; ++direction)
        {
            minimapNames.insert(minimapArrowPrefix(formatWorld) + std::to_string(direction));
            if (formatWorld == "mm8")
            {
                mm8MinimapNames.insert(minimapArrowPrefix(formatWorld) + std::to_string(direction));
            }
        }
    }
    std::map<std::string, Path> minimapSources;
    for (const Path &path : iconArchives.paths)
    {
        for (const std::string &member : listLodMembers(path))
        {
            const std::string name = lower(member);
            if (minimapNames.contains(name))
            {
                const auto [owner, inserted] = minimapSources.emplace(name, path);
                if (!inserted)
                {
                    // Merge supplies revised MM8 town maps above the original base icon archive.
                    const std::string layer = lower(path.filename().string());
                    require(mm8MinimapNames.contains(name) && lower(owner->second.filename().string()) == "icons.lod"
                        && (layer == "mm8.icons.lod" || layer == "mmmerge.icons.lod" || layer == "00 patch.icons.lod"),
                        "Ambiguous minimap ownership requires an explicit override profile: " + name);
                    owner->second = path;
                }
            }
        }
    }
    require(std::filesystem::create_directories(output), "Cannot create scene staging output");
    try
    {
        writeText(resources / "engine/data_tables/texture_frame_data.txt", serializeTextRows(textureRows));
        writeText(resources / "engine/rendering/surface_materials.yml", waterYaml);
        for (const auto &[world, rows] : terrainRows)
        {
            const std::string suffix = world == "mm6" ? "_3" : world == "mm7" ? "_2" : "";
            writeText(resources / "engine/data_tables" / ("terrain_tile_data" + suffix + ".txt"),
                serializeTextRows(rows));
        }
        std::map<std::string, std::set<std::string>> mapMembers;
        for (const char *pArchive : {"mm6.games.lod", "mm7.games.lod", "mm8.games.lod", "patch.games.lod",
                "breach.games.lod"})
        {
            if (std::filesystem::exists(root / "Data" / pArchive))
            {
                for (const std::string &name : listLodMembers(root / "Data" / pArchive))
                {
                    mapMembers[pArchive].insert(lower(name));
                }
            }
        }
        std::map<std::string, std::set<std::string>> worldSkies;
        std::set<std::string> arrowWorlds;
        std::ostringstream arrowReport;
        std::ostringstream report;
        report << "{\"schema\":1,\"stage\":" << std::quoted(mode == SceneImportMode::ResourceInspection
            ? "inspection-scenes" : mode == SceneImportMode::NativeMergeGameplay
                ? "native-merge-gameplay-scenes" : "registered-scenes") << ",\"runtime_ready\":false,"
            "\"tft_frames\":" << textureRows.size() << ",\"hardware_water_materials\":" << waterRows.size() - 1
            << ",\"maps\":[";
        std::set<std::string> seen;
        for (const std::string &requested : mapFiles)
        {
            checkImportCancellation(cancelled);
            const std::string file = lower(requested);
            const bool outdoor = Path(file).extension() == ".odm";
            require(Path(file).filename() == file && (outdoor || Path(file).extension() == ".blv")
                && file.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-") == std::string::npos
                && seen.insert(file).second, "Invalid or duplicate scene map name");
            const MapStatsEntry *pMap = registry.findByFileName(file);
            require(pMap != nullptr, "Scene has no registered world: " + file);
            const std::string formatWorld = presentationWorld(*pMap, mode);
            const std::optional<MergeSceneProfile> profile = nativeMergeSource
                ? std::optional<MergeSceneProfile>(mergeSceneProfile(root, file)) : std::nullopt;
            const auto sourceArchive = [&](const std::string &member)
            {
                const std::string name = profile ? "breach.games.lod"
                    : mapMembers["patch.games.lod"].contains(member) ? "patch.games.lod" : pMap->worldId + ".games.lod";
                require(mapMembers[name].contains(member), "Missing scene member: " + member);
                return root / "Data" / name;
            };
            const Path archive = sourceArchive(file);
            const std::vector<uint8_t> geometry = readLodMap(archive, file);
            std::optional<IndoorMapData> indoorMap;
            std::optional<OutdoorMapData> outdoorMap;
            if (outdoor)
            {
                outdoorMap = OutdoorMapDataLoader().loadFromBytes(geometry);
            }
            else
            {
                indoorMap = IndoorMapDataLoader().loadFromBytes(geometry);
            }
            require(indoorMap.has_value() || outdoorMap.has_value(), "Cannot decode scene geometry: " + file);
            const std::string companionName = Path(file).stem().string() + (outdoor ? ".ddm" : ".dlv");
            const Path companionArchive = sourceArchive(companionName);
            const std::vector<uint8_t> companion = readLodMap(companionArchive, companionName);
            const std::optional<MapDeltaData> delta = outdoor
                ? MapDeltaDataLoader().loadOutdoorFromBytes(companion, *outdoorMap)
                : MapDeltaDataLoader().loadIndoorFromBytes(companion, *indoorMap);
            require(delta.has_value(), "Cannot decode scene companion: " + file);
            std::optional<ScriptedEventProgram> events;
            if (!profile)
            {
                const std::string eventSource = readText(base / "engine/scripts/common/event_support.lua") + "\n"
                    + readText(base / "worlds" / pMap->worldId / "events/maps" / (Path(file).stem().string() + ".lua"));
                events = ScriptedEventProgram::loadFromLuaText(eventSource, "@" + file, ScriptedEventScope::Map, error);
                require(events.has_value(), "Cannot read converted scene event metadata: " + error);
            }
            std::set<std::string> textures;
            std::set<std::string> groups;
            std::set<std::string> unresolved;
            std::set<std::string> unresolvedEncounters;
            size_t faceCount = 0;
            const auto addSurface = [&](const std::string &name, int frameIndex)
            {
                if (name.empty())
                {
                    return;
                }
                textures.insert(lower(name));
                if (const SurfaceMaterialDefinition *pWater = waterMaterials.findMatch(name, 0, true))
                {
                    for (const SurfaceAnimationFrame &frame : pWater->animation.frames)
                    {
                        textures.insert(lower(frame.textureName));
                    }
                }
                std::optional<SurfaceAnimationSequence> animation = textureFrames.findAnimationSequenceByName(name);
                if (!animation && frameIndex > 0)
                {
                    animation = textureFrames.findAnimationSequenceByIndex(frameIndex);
                }
                if (animation)
                {
                    for (const SurfaceAnimationFrame &frame : animation->frames)
                    {
                        textures.insert(lower(frame.textureName));
                    }
                }
            };
            for (const std::string &name : events ? events->textureNames() : std::vector<std::string>{})
            {
                addSurface(name, 0);
            }
            if (profile)
            {
                for (const std::string &name : profile->textures)
                {
                    addSurface(name, 0);
                }
            }
            if (outdoorMap)
            {
                const Rows &tiles = terrainRows.at(formatWorld);
                require(outdoorMap->masterTile <= 2, "Unsupported master terrain table in " + file);
                // Collect the same complete local palette that the runtime resolves, including seasonal variants.
                for (int tile = 0; tile < 234; ++tile)
                {
                    const std::optional<int> index = resolveTerrainDescriptorIndex(*outdoorMap, tile,
                        formatWorld == "mm8");
                    require(index && *index >= 0 && size_t(*index) + 1 < tiles.size(),
                        "Scene terrain lookup outside source table: " + file);
                    const std::string &name = tiles[*index + 1][1];
                    if (name != "pending")
                    {
                        addSurface(name, 0);
                    }
                }
                for (const OutdoorBModel &model : outdoorMap->bmodels)
                {
                    for (const OutdoorBModelFace &face : model.faces)
                    {
                        ++faceCount;
                        addSurface(face.textureName, 0);
                    }
                }
            }
            else
            {
                faceCount = indoorMap->faces.size();
                for (const IndoorFace &face : indoorMap->faces)
                {
                    if (!face.isPortal)
                    {
                        addSurface(face.textureName, face.textureFrameTableIndex);
                    }
                }
            }
            const auto addSprite = [&](uint16_t id)
            {
                if (id != 0)
                {
                    const SpriteFrameEntry *pFrame = sprites.getFrame(id, 0);
                    require(pFrame != nullptr, "Unknown scene SFT group: " + std::to_string(id));
                    groups.insert(lower(pFrame->spriteName));
                }
            };
            const auto addMonster = [&](const MonsterEntry *pMonster)
            {
                require(pMonster != nullptr, "Scene references an unknown monster in " + file);
                for (const std::string &name : pMonster->spriteNames)
                {
                    if (!name.empty())
                    {
                        groups.insert(lower(name));
                    }
                }
            };
            for (const MapDeltaActor &actor : delta->actors)
            {
                addMonster(monsters.findById(actor.monsterInfoId));
            }
            // Population generation and difficulty can choose any of the three encounter tiers.
            for (const MapEncounterInfo *pEncounter : {&pMap->encounter1, &pMap->encounter2, &pMap->encounter3})
            {
                if (!pEncounter->monsterName.empty() && pEncounter->monsterName != "0")
                {
                    for (const char *pTier : {" A", " B", " C"})
                    {
                        const std::string picture = (pEncounter->pictureName.empty()
                            ? pEncounter->monsterName : pEncounter->pictureName) + pTier;
                        const MonsterTable::MonsterStatsEntry *pStats = monsters.findStatsByPictureName(picture);
                        if (pStats)
                        {
                            addMonster(monsters.findById(static_cast<int16_t>(pStats->id)));
                        }
                        else
                        {
                            // Preserve unresolved source encounter declarations in the report. Do not invent
                            // tier variants for a unique monster or claim that its population can spawn.
                            unresolvedEncounters.insert(picture);
                        }
                    }
                }
            }
            const auto addDecoration = [&](const auto &entity)
            {
                const DecorationLookupResult resolved =
                    decorations.resolveMapDecoration(entity.decorationListId, entity.name);
                if (!resolved.pEntry)
                {
                    unresolved.insert(entity.name);
                }
                else if (!hasDecorationFlag(resolved.pEntry->flags, DecorationDescFlag::DontDraw))
                {
                    addSprite(resolved.pEntry->spriteId);
                }
            };
            if (outdoorMap)
            {
                for (const OutdoorEntity &entity : outdoorMap->entities)
                {
                    addDecoration(entity);
                }
            }
            else
            {
                for (const IndoorEntity &entity : indoorMap->entities)
                {
                    addDecoration(entity);
                }
            }
            for (const std::string &name : events ? events->spriteNames() : std::vector<std::string>{})
            {
                if (const DecorationEntry *pDecoration = decorations.findByInternalName(name))
                {
                    addSprite(pDecoration->spriteId);
                }
                else
                {
                    unresolved.insert(name);
                }
            }
            for (const MapDeltaSpriteObject &object : delta->spriteObjects)
            {
                if (const ObjectEntry *pObject = objects.get(object.objectDescriptionId))
                {
                    addSprite(pObject->spriteId);
                }
            }
            // Frame zero is the original empty sprite sentinel, with zero scale, palette and duration.
            // It does not refer to eight missing "null" images.
            if (const SpriteFrameEntry *pNull = sprites.getFrame(0, 0);
                pNull && pNull->spriteName == "null" && pNull->scale == 0.0f && pNull->paletteId == 0)
            {
                groups.erase("null");
            }
            const ConvertedSpriteFrames selected = convertSpriteFrameRows(spriteRows, &groups);
            std::set<std::string> wantedSprites;
            for (const std::string &group : groups)
            {
                const std::optional<uint16_t> id = sprites.findFrameIndexBySpriteName(group);
                require(id.has_value(), "Unknown scene sprite group: " + group);
                const std::vector<std::string> names = sprites.collectTextureNames(*id);
                wantedSprites.insert(names.begin(), names.end());
            }
            std::vector<std::string> presentTextures;
            std::vector<std::string> presentSprites;
            std::vector<int16_t> presentPalettes;
            std::set<std::string> missingTextures;
            std::set<std::string> missingSprites;
            std::set<std::string> missingPalettes;
            for (const std::string &name : textures)
            {
                checkImportCancellation(cancelled);
                if (bitmaps.members.contains(name))
                {
                    presentTextures.push_back(name);
                }
                else
                {
                    missingTextures.insert(name);
                }
            }
            for (const std::string &name : wantedSprites)
            {
                checkImportCancellation(cancelled);
                if (spriteArchives.members.contains(name))
                {
                    presentSprites.push_back(name);
                }
                else
                {
                    missingSprites.insert(name);
                }
            }
            for (int16_t id : selected.paletteIds)
            {
                std::ostringstream name;
                name << "pal" << std::setw(3) << std::setfill('0') << id;
                if (bitmaps.members.contains(name.str()))
                {
                    presentPalettes.push_back(id);
                }
                else
                {
                    missingPalettes.insert(name.str());
                }
            }
            const Path world = resources / "worlds" / pMap->worldId;
            if (profile)
            {
                require(missingTextures.empty() && missingSprites.empty() && missingPalettes.empty(),
                    "Merge inspection resources are incomplete: " + file);
                const std::string stem = Path(file).stem().string();
                std::string mapLuaSource;
                std::string mechanismsYaml;
                if (file == "bralvar.odm")
                {
                    // BrAlvar.lua hides these exact entity slots, not their shared primary10 sprite key.
                    for (size_t entityIndex : {381u, 676u, 792u})
                    {
                        require(entityIndex < outdoorMap->entities.size()
                            && outdoorMap->entities[entityIndex].eventIdPrimary == 10
                            && outdoorMap->entities[entityIndex].eventIdSecondary == 0,
                            "BrAlvar source hidden decoration identity changed");
                    }
                    const auto companions = loadBreachAlvarCompanionResources(root);
                    require(companions.has_value(), "Missing Breach Alvar companion source profile");
                    mapLuaSource = breachNativeTexts(root, chinese, stem, 5) + companions->mapLuaSource
                        + "\n" + std::string(BreachMazeBrAlvarMapSource);
                    mechanismsYaml = breachTowerMechanisms(*outdoorMap, base, file, *profile);
                }
                else if (file == "breach.odm")
                {
                    require(delta->chests.size() > 15, "Breach source chest indices1..15 are unavailable");
                    Game::HouseTable houses;
                    require(houses.loadFromRows(readRows(base / "engine/data_tables/house_data.txt")),
                        "Cannot read Breach source Inn dependency");
                    const Game::HouseEntry *pInn = houses.get(1607);
                    require(pInn && pInn->isAdventurersInn(), "Breach source house1607 is not an Adventurer's Inn");
                    bool telelocatorFacetsPresent = false;
                    for (const Game::OutdoorBModel &model : outdoorMap->bmodels)
                    {
                        for (const Game::OutdoorBModelFace &face : model.faces)
                        {
                            telelocatorFacetsPresent = telelocatorFacetsPresent || face.cogNumber == 400;
                        }
                    }
                    require(telelocatorFacetsPresent, "Breach source Telelocator facet group400 is unavailable");
                    std::optional<std::string> telelocatorName;
                    for (const auto &row : readRows(base / "engine/data_tables/items.txt"))
                    {
                        if (row.size() >= 3 && row[0] == "666")
                        {
                            require(!telelocatorName && !row[2].empty(), "Invalid Telelocator item666 name");
                            telelocatorName = row[2];
                        }
                    }
                    require(telelocatorName.has_value(), "Missing source Telelocator item666");
                    mapLuaSource = breachNativeTexts(root, chinese, stem, 6, 676)
                        + "evt.importedItemNames = {[666] = " + luaText(*telelocatorName) + "}\n"
                        + std::string(BreachMazeBreachMapSource);
                    mechanismsYaml = breachTowerMechanisms(*outdoorMap, base, file, *profile);
                    std::vector<Path> endCardArchives = {root / "Data/mmmerge.D.lod"};
                    const Path localizedEndCards = root / "Data/10 LocZHCN.EnglishD.lod";
                    if (chinese && std::filesystem::exists(localizedEndCards))
                    {
                        endCardArchives.push_back(localizedEndCards);
                    }
                    convertLodImages(endCardArchives, {"winbg2.pcx"}, resources / "engine/icons");
                }
                else if (file == "brbase.blv")
                {
                    mapLuaSource = breachNativeTexts(root, chinese, stem, 7, 521)
                        + std::string(BreachMazeBrBaseMapSource);
                }
                const std::string eventSource = readText(base / "engine/scripts/common/event_support.lua")
                    + "\n" + mapLuaSource;
                const auto program = ScriptedEventProgram::loadFromLuaText(
                    eventSource, "@" + stem + "-native-breach", ScriptedEventScope::Map, error);
                require(program && (file == "brbase.blv" ? program->hasEvent(1) && program->hasEvent(2)
                    : file == "bralvar.odm" ? program->hasEvent(5) && program->hasEvent(50)
                        && program->hasEvent(51) && program->hasEvent(52) && program->hasEvent(53)
                        && program->hasEvent(54) && program->hasEvent(81) && program->hasEvent(65458)
                        && program->hasEvent(65459) && program->hasEvent(65460)
                        && program->hasEvent(65461) && program->hasEvent(65469)
                    : program->hasEvent(54) && program->hasEvent(65462) && program->hasEvent(65463)),
                    "Native Breach map request Lua rejected: " + error);
                if (file == "bralvar.odm")
                {
                    require(program->hasEvent(15) && program->hasEvent(16), "Missing native BrAlvar jump handlers");
                    for (uint16_t eventId = 65454; eventId <= 65457; ++eventId)
                    {
                        require(program->hasEvent(eventId),
                            "Missing native BrAlvar source journey handler " + std::to_string(eventId));
                    }
                }
                else if (file == "breach.odm")
                {
                    for (uint16_t eventId : {81, 82, 401, 402, 403, 900, 65464, 65465, 65466, 65467})
                    {
                        require(program->hasEvent(eventId),
                            "Missing native Breach source handler " + std::to_string(eventId));
                    }
                    for (uint16_t eventId = 301; eventId <= 315; ++eventId)
                    {
                        require(program->hasEvent(eventId),
                            "Missing native Breach chest handler " + std::to_string(eventId));
                    }
                }
                if (!mechanismsYaml.empty())
                {
                    for (uint16_t eventId = 101; eventId <= 128; ++eventId)
                    {
                        require(program->hasEvent(eventId),
                            "Missing native " + stem + " tower handler " + std::to_string(eventId));
                    }
                    writeText(world / "maps" / (stem + ".mechanisms.yml"), mechanismsYaml);
                }
                // Executable content is the authenticated projection; original Map Lua remains provenance only.
                writeText(world / "events/maps" / (stem + ".lua"), mapLuaSource);
                // Provenance assets are not placed in events/ and are never registered as executable handlers.
                writeText(world / "source/maps" / (stem + ".original.lua"), readText(root / profile->sourceScript));
                for (const auto &[language, archiveName] : std::map<std::string, std::string>{
                        {"english", "mmmerge.T.lod"}, {"zh_CN", "10 LocZHCN.EnglishT.lod"}})
                {
                    writeConvertedResource(world / "source/maps" / language / (stem + ".str"),
                        readLodText(root / "Data" / archiveName, stem + ".str"));
                }
            }
            if (arrowWorlds.insert(pMap->worldId).second)
            {
                const std::string prefix = minimapArrowPrefix(formatWorld);
                std::set<std::string> missingArrows;
                std::ostringstream bindings;
                bindings << "Direction\tTexture\n";
                arrowReport << (arrowWorlds.size() > 1 ? "," : "")
                    << "{\"world\":" << std::quoted(pMap->worldId) << ",\"images\":[";
                size_t arrowCount = 0;
                for (int direction = 1; direction <= 8; ++direction)
                {
                    const std::string sourceName = prefix + std::to_string(direction);
                    bindings << direction << '\t' << sourceName << ".bmp\n";
                    if (!minimapSources.contains(sourceName))
                    {
                        missingArrows.insert(sourceName);
                        continue;
                    }
                    const ConvertedLodImage image =
                        convertLodImages({minimapSources.at(sourceName)}, {sourceName}, world / "icons").front();
                    arrowReport << (arrowCount++ ? "," : "") << "{\"name\":" << std::quoted(image.name)
                        << ",\"source_name\":" << std::quoted(sourceName)
                        << ",\"archive\":" << std::quoted(image.archive)
                        << ",\"width\":" << image.width << ",\"height\":" << image.height << '}';
                }
                arrowReport << "],\"missing_images\":";
                stringArray(arrowReport, missingArrows);
                arrowReport << '}';
                writeText(world / "ui/minimap_arrows.txt", bindings.str());
            }
            const std::string minimapName = Path(file).stem().string();
            // Geographic art stays world-local. The shared HUD already resolves this original map stem.
            const std::vector<ConvertedLodImage> minimapImages = outdoor && minimapSources.contains(minimapName)
                ? convertLodImages({minimapSources.at(minimapName)}, {minimapName}, world / "icons")
                : std::vector<ConvertedLodImage>{};
            if (outdoor)
            {
                std::set<std::string> &skies = worldSkies[pMap->worldId];
                if (skies.empty())
                {
                    // Preserve the existing legacy sky families, plus the runtime's dawn/night
                    // variants and Dagger Wound Island's fixed sunset-cloud background.
                    skies.insert({"sunsetclouds", "sky6pm", "sky05"});
                    for (const std::string &name : bitmaps.members)
                    {
                        if (name.starts_with("6sky") || name.starts_with("7sky") || name.starts_with("sky")
                            || name.starts_with("plansky") || name.starts_with("7plansky"))
                        {
                            skies.insert(name);
                        }
                    }
                }
                const auto addSky = [&](const std::string &name)
                {
                    if (!name.empty())
                    {
                        require(name.size() <= 64 && name.find_first_not_of(
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos,
                            "Unsafe scene sky texture name");
                        skies.insert(lower(name));
                    }
                };
                addSky(outdoorMap->skyTexture);
                addSky(delta->locationTime.skyTextureName);
                if (const MergedBolsterMapEntry *pRule = mapRules.findById(uint32_t(pMap->id)))
                {
                    addSky(pRule->customSky);
                    if (const MergedContinentSettingEntry *pContinent = continents.findById(pRule->continent))
                    {
                        for (const std::string &name : pContinent->skies)
                        {
                            addSky(name);
                        }
                    }
                }
            }
            writeConvertedResource(world / "maps" / file, geometry);
            writeConvertedResource(world / "maps" / companionName, companion);
            const std::vector<ConvertedLodImage> images = convertLodImages(bitmaps.paths, presentTextures,
                world / "textures");
            const std::vector<ConvertedLodImage> spriteImages = convertLodSprites(spriteArchives.paths, presentSprites,
                world / "sprites");
            convertLodPalettes(bitmaps.paths, presentPalettes, world / "textures");
            report << (seen.size() > 1 ? "," : "") << "{\"map\":" << std::quoted(pMap->worldId + ":" + file)
                << ",\"presentation_format\":" << std::quoted(formatWorld)
                << ",\"normal_enterable\":" << (mode == SceneImportMode::ResourceInspection ? "false" : "true")
                << ",\"script_execution\":" << std::quoted(profile ? "native-source-overlay" : "converted-evt")
                << ",\"geometry_archive\":" << std::quoted(archive.filename().string())
                << ",\"companion_archive\":" << std::quoted(companionArchive.filename().string())
                << ",\"faces\":" << faceCount << ",\"actors\":" << delta->actors.size()
                << ",\"spawns\":" << (outdoorMap ? outdoorMap->spawns.size() : indoorMap->spawns.size())
                << ",\"chests\":" << delta->chests.size()
                << ",\"doors\":" << delta->doors.size() << ",\"textures\":" << images.size()
                << ",\"sprites\":" << spriteImages.size() << ",\"palettes\":" << presentPalettes.size()
                << ",\"minimap\":";
            if (!outdoor)
            {
                report << "null";
            }
            else
            {
                report << "{\"name\":" << std::quoted(minimapName)
                    << ",\"available\":" << (minimapImages.empty() ? "false" : "true");
                if (!minimapImages.empty())
                {
                    const ConvertedLodImage &image = minimapImages.front();
                    report << ",\"archive\":" << std::quoted(image.archive)
                        << ",\"width\":" << image.width << ",\"height\":" << image.height;
                }
                report << '}';
            }
            if (profile)
            {
                report << ",\"map_lua_source\":" << std::quoted(profile->sourceScript)
                    << ",\"map_lua_sha256\":" << std::quoted(profile->sha256)
                    << ",\"evt_present\":false,\"dynamic_script_limit\":"
                    << "\"Only explicit source overlays execute; "
                        "unprojected dynamic Merge behavior remains unsupported\"";
                if (file == "bralvar.odm" || file == "breach.odm")
                {
                    report << ",\"mechanisms_companion\":"
                        << std::quoted(Path(file).stem().string() + ".mechanisms.yml")
                        << ",\"tower_mechanisms\":28";
                }
            }
            report << ",\"missing_textures\":";
            stringArray(report, missingTextures);
            report << ",\"missing_sprites\":";
            stringArray(report, missingSprites);
            report << ",\"missing_palettes\":";
            stringArray(report, missingPalettes);
            report << ",\"unresolved_encounters\":";
            stringArray(report, unresolvedEncounters);
            report << ",\"unresolved_decorations\":";
            stringArray(report, unresolved);
            report << ",\"image_owners\":[";
            size_t imageIndex = 0;
            for (const std::vector<ConvertedLodImage> *pImages : {&images, &spriteImages, &minimapImages})
            {
                for (const ConvertedLodImage &image : *pImages)
                {
                    checkImportCancellation(cancelled);
                    report << (imageIndex++ ? "," : "") << "{\"name\":" << std::quoted(image.name)
                        << ",\"archive\":" << std::quoted(image.archive) << '}';
                }
            }
            report << "]}";
        }
        report << "],\"minimap_arrows\":[" << arrowReport.str() << "],\"skies\":[";
        size_t skyWorldIndex = 0;
        for (const auto &[world, skies] : worldSkies)
        {
            checkImportCancellation(cancelled);
            std::vector<std::string> present;
            std::set<std::string> missing;
            for (const std::string &name : skies)
            {
                if (bitmaps.members.contains(name))
                {
                    present.push_back(name);
                }
                else
                {
                    missing.insert(name);
                }
            }
            const std::vector<ConvertedLodImage> images =
                convertLodImages(bitmaps.paths, present, resources / "worlds" / world / "textures");
            // sky_textures is the runtime's scale-category alias for physical textures[_xN],
            // not a separate package directory. Both sky existence checks and rendering use it.
            report << (skyWorldIndex++ ? "," : "") << "{\"world\":" << std::quoted(world)
                << ",\"directory\":\"textures\",\"images\":[";
            for (size_t index = 0; index < images.size(); ++index)
            {
                const ConvertedLodImage &image = images[index];
                report << (index ? "," : "") << "{\"name\":" << std::quoted(image.name)
                    << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                    << ",\"height\":" << image.height << '}';
            }
            report << "],\"missing_images\":";
            stringArray(report, missing);
            report << '}';
        }
        report << "]}\n";
        writeText(output / "scene_conversion.json", report.str());
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove_all(output, ignored);
        throw;
    }
}
}
