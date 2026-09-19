#include "SceneResources.h"
#include "LodResources.h"
#include "SpriteFrameResources.h"
#include "SurfaceResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/maps/MapDeltaData.h"
#include "game/maps/TerrainTileData.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/SpriteTables.h"
#include "game/tables/SurfaceMaterialTable.h"
#include "game/tables/TextureFrameTable.h"

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
    const std::vector<std::string> &mapFiles, const ImportCancelCheck &cancelled)
{
    checkImportCancellation(cancelled);
    using namespace Game;
    const Path root = std::filesystem::canonical(source);
    const Path base = std::filesystem::canonical(convertedBase);
    const Path output = std::filesystem::weakly_canonical(destination);
    require(!std::filesystem::exists(output) && outside(output, root) && outside(output, base),
        "Scene output must be new and outside both inputs");
    require(!mapFiles.empty(), "No scene maps requested");
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
        require(pMap && terrainRows.contains(pMap->worldId), "Scene has no supported registered world: " + file);
        if (lower(Path(file).extension().string()) == ".odm")
        {
            minimapNames.insert(lower(Path(file).stem().string()));
            if (pMap->worldId == "mm8")
            {
                mm8MinimapNames.insert(lower(Path(file).stem().string()));
            }
        }
        for (int direction = 1; direction <= 8; ++direction)
        {
            minimapNames.insert(minimapArrowPrefix(pMap->worldId) + std::to_string(direction));
            if (pMap->worldId == "mm8")
            {
                mm8MinimapNames.insert(minimapArrowPrefix(pMap->worldId) + std::to_string(direction));
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
        writeText(output / "engine/data_tables/texture_frame_data.txt", serializeTextRows(textureRows));
        writeText(output / "engine/rendering/surface_materials.yml", waterYaml);
        for (const auto &[world, rows] : terrainRows)
        {
            const std::string suffix = world == "mm6" ? "_3" : world == "mm7" ? "_2" : "";
            writeText(output / "engine/data_tables" / ("terrain_tile_data" + suffix + ".txt"), serializeTextRows(rows));
        }
        std::map<std::string, std::set<std::string>> mapMembers;
        for (const char *pArchive : {"mm6.games.lod", "mm7.games.lod", "mm8.games.lod", "patch.games.lod"})
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
        report << "{\"schema\":1,\"stage\":\"registered-scenes\",\"runtime_ready\":false,"
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
            require(pMap && terrainRows.contains(pMap->worldId), "Scene has no supported registered world: " + file);
            const auto sourceArchive = [&](const std::string &member)
            {
                const std::string name = mapMembers["patch.games.lod"].contains(member)
                    ? "patch.games.lod" : pMap->worldId + ".games.lod";
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
            const std::string eventSource = readText(base / "engine/scripts/common/event_support.lua") + "\n"
                + readText(base / "worlds" / pMap->worldId / "events/maps" / (Path(file).stem().string() + ".lua"));
            const std::optional<ScriptedEventProgram> events = ScriptedEventProgram::loadFromLuaText(
                eventSource, "@" + file, ScriptedEventScope::Map, error);
            require(events.has_value(), "Cannot read converted scene event metadata: " + error);
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
            for (const std::string &name : events->textureNames())
            {
                addSurface(name, 0);
            }
            if (outdoorMap)
            {
                const Rows &tiles = terrainRows.at(pMap->worldId);
                require(outdoorMap->masterTile <= 2, "Unsupported master terrain table in " + file);
                // Collect the same complete local palette that the runtime resolves, including seasonal variants.
                for (int tile = 0; tile < 234; ++tile)
                {
                    const std::optional<int> index = resolveTerrainDescriptorIndex(*outdoorMap, tile,
                        pMap->worldId == "mm8");
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
            for (const std::string &name : events->spriteNames())
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
            const Path world = output / "worlds" / pMap->worldId;
            if (arrowWorlds.insert(pMap->worldId).second)
            {
                const std::string prefix = minimapArrowPrefix(pMap->worldId);
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
                convertLodImages(bitmaps.paths, present, output / "worlds" / world / "textures");
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
