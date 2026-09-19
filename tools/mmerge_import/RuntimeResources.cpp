#include "RuntimeResources.h"
#include "RuntimeZip.h"
#include "ArcomageResources.h"
#include "BolsterResources.h"
#include "ClassResources.h"
#include "CombatResources.h"
#include "CoreResources.h"
#include "DecorationResources.h"
#include "DollResources.h"
#include "EquipmentPictureResources.h"
#include "EventResources.h"
#include "FrameResources.h"
#include "FxResources.h"
#include "InspectionResources.h"
#include "SceneResources.h"
#include "InventoryResources.h"
#include "JournalResources.h"
#include "MediaResources.h"
#include "MenuResources.h"
#include "NpcResources.h"
#include "ObjectPresentationResources.h"
#include "RaceResources.h"
#include "RosterResources.h"
#include "ServiceResources.h"
#include "SoundResources.h"
#include "SpeechResources.h"
#include "StoryResources.h"
#include "SupportResources.h"
#include "TeacherResources.h"
#include "TransitionResources.h"
#include "UiResources.h"
#include "VoiceResources.h"
#include "WorldRuleResources.h"
#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
using Path = std::filesystem::path;

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

bool equalFiles(const Path &left, const Path &right, const ImportCancelCheck &cancelled)
{
    if (std::filesystem::file_size(left) != std::filesystem::file_size(right))
    {
        return false;
    }
    std::ifstream a(left, std::ios::binary);
    std::ifstream b(right, std::ios::binary);
    require(a.good() && b.good(), "Cannot compare overlapping converted resources");
    std::array<char, 65536> first;
    std::array<char, 65536> second;
    while (a && b)
    {
        checkImportCancellation(cancelled);
        a.read(first.data(), first.size());
        b.read(second.data(), second.size());
        if (a.gcount() != b.gcount() || !std::equal(first.begin(), first.begin() + a.gcount(), second.begin()))
        {
            return false;
        }
    }
    require(a.eof() && b.eof(), "Cannot completely read overlapping converted resources");
    return true;
}

void mergeStage(const Path &stageRoot, const Path &assembled, const Path &reports, const std::string &stage,
    const ImportCancelCheck &cancelled)
{
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(stageRoot))
    {
        checkImportCancellation(cancelled);
        require(!entry.is_symlink() && (entry.is_directory() || entry.is_regular_file()),
            "Converter produced a nonregular resource");
        if (entry.is_directory())
        {
            continue;
        }
        const Path relative = entry.path().lexically_relative(stageRoot);
        const std::string name = relative.generic_string();
        const bool resource = *relative.begin() == "engine" || *relative.begin() == "worlds";
        const Path target = (resource ? assembled : reports / stage) / relative;
        std::filesystem::create_directories(target.parent_path());
        if (std::filesystem::exists(target))
        {
            if (equalFiles(entry.path(), target, cancelled))
            {
                continue;
            }
            // The selected interface replaces generic frame artwork. Full SFT data replaces the actor preview subset.
            const bool replacement = (stage == "ui" && name.starts_with("engine/icons/"))
                || (stage == "decoration" && name == "engine/rendering/sprite_frame_data_common.yml");
            require(replacement, "Conflicting converted resource in " + stage + ": " + name);
            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing);
        }
        else
        {
            std::filesystem::rename(entry.path(), target);
        }
    }
    std::filesystem::remove_all(stageRoot);
}
}

void convertRuntimeResources(const Path &source, const Path &layoutDirectory, const Path &destination,
    const RuntimeImportProgress &progress, const ImportCancelCheck &cancelled)
{
    checkImportCancellation(cancelled);
    const Path root = std::filesystem::canonical(source);
    const Path layouts = std::filesystem::canonical(layoutDirectory);
    const Path output = std::filesystem::weakly_canonical(destination);
    require(std::filesystem::is_directory(root) && std::filesystem::is_directory(layouts),
        "Source and gameplay layout inputs must be directories");
    require(!std::filesystem::exists(output) && outside(output, root) && outside(output, layouts),
        "Runtime conversion requires a new destination outside both inputs");
    const Path work = output / ".work";
    const Path assembled = work / "assembled";
    const Path stagesRoot = work / "stages";
    struct Stage
    {
        std::string name;
        std::function<void(const Path &)> convert;
    };
    SceneMapSelection sceneMaps;
    const std::vector<std::string> sceneWorlds{"mm6", "mm7", "mm8"};
    std::map<std::string, std::string> mapWorlds;
    // Known legacy commands have no implemented semantics. Keep these scenes out of the supported
    // import profile until their commands are ported; never strip instructions to make a map load.
    const std::map<std::string, std::string> unsupportedEventMaps = {
        {"6d09.blv", "Legacy commands 27/28 in events 21/39 are not implemented"},
        {"znwc.blv", "Legacy command 20 in event 71 is not implemented"}};
    std::vector<std::string> excludedScenes;
    const std::vector<Stage> stages = {
        {"menu", [&](const Path &p) { convertMenuResources(root, p); }},
        {"core", [&](const Path &p) { convertCoreResources(root, p); }},
        {"story", [&](const Path &p) { convertStoryResources(root, p); }},
        {"combat", [&](const Path &p) { convertCombatResources(root, assembled, p); }},
        {"service", [&](const Path &p) { convertServiceResources(root, p); }},
        {"journal", [&](const Path &p) { convertJournalResources(root, p); }},
        {"class", [&](const Path &p) { convertClassResources(root, p); }},
        {"inspection", [&](const Path &p) { convertInspectionResources(root, p); }},
        {"race", [&](const Path &p) { convertRaceResources(root, assembled, p); }},
        {"inventory", [&](const Path &p) { convertInventoryResources(root, assembled, p); }},
        {"frame", [&](const Path &p) { convertFrameResources(root, p); }},
        {"fx", [&](const Path &p) { convertFxResources(root, assembled, p); }},
        {"transition", [&](const Path &p) { convertTransitionResources(root, p); }},
        {"roster", [&](const Path &p) { convertRosterResources(root, assembled, p); }},
        {"doll", [&](const Path &p) { convertDollResources(root, assembled, p); }},
        {"teacher", [&](const Path &p) { convertTeacherResources(root, assembled, p); }},
        {"npc", [&](const Path &p) { convertNpcResources(root, assembled, p); }},
        {"support", [&](const Path &p) { convertSupportResources(root, assembled, p); }},
        {"bolster", [&](const Path &p) { convertBolsterResources(root, assembled, p); }},
        {"voice", [&](const Path &p) { convertVoiceResources(root, assembled, p); }},
        {"equipment_picture", [&](const Path &p) { convertEquipmentPictureResources(root, assembled, p); }},
        {"world_rule", [&](const Path &p) { convertWorldRuleResources(root, assembled, p); }},
        {"arcomage", [&](const Path &p) { convertArcomageResources(root, assembled, p); }},
        {"sound", [&](const Path &p) { convertSoundResources(root, assembled, p); }},
        {"speech", [&](const Path &p) { convertSpeechResources(root, assembled, p); }},
        {"ui", [&](const Path &p) { convertUiResources(root, layouts, 3, p); }},
        {"decoration", [&](const Path &p) { convertDecorationResources(root, p); }},
        {"object_presentation", [&](const Path &p)
        {
            convertObjectPresentationResources(root, assembled, p, cancelled);
        }},
        {"event", [&](const Path &p)
        {
            for (const std::string &world : sceneWorlds)
            {
                const SceneMapSelection selection = discoverSceneMapFiles(root, assembled, world);
                for (const std::vector<std::string> *pFiles : {&selection.available, &selection.missing})
                {
                    for (const std::string &file : *pFiles)
                    {
                        require(mapWorlds.emplace(file, world).second, "Ambiguous scene registry identity: " + file);
                    }
                }
                sceneMaps.available.insert(sceneMaps.available.end(),
                    selection.available.begin(), selection.available.end());
                sceneMaps.missing.insert(sceneMaps.missing.end(), selection.missing.begin(), selection.missing.end());
            }
            require(std::find(sceneMaps.available.begin(), sceneMaps.available.end(), "oute3.odm")
                != sceneMaps.available.end(), "The MM6 starting scene is missing");
            require(std::find(sceneMaps.available.begin(), sceneMaps.available.end(), "7out01.odm")
                != sceneMaps.available.end(), "The MM7 starting scene is missing");
            require(std::find(sceneMaps.available.begin(), sceneMaps.available.end(), "out01.odm")
                != sceneMaps.available.end(), "The MM8 starting scene is missing");
            std::erase_if(sceneMaps.available, [&](const std::string &file)
            {
                if (!unsupportedEventMaps.contains(file))
                {
                    return false;
                }
                excludedScenes.push_back(file);
                return true;
            });
            std::vector<std::string> stems;
            for (const std::string &file : sceneMaps.available)
            {
                stems.push_back(Path(file).stem().string());
            }
            convertEventResourcesForMaps(root, assembled, p, stems, cancelled);
        }},
        {"scene", [&](const Path &p) { convertSceneResources(root, assembled, p, sceneMaps.available, cancelled); }},
        {"house", [&](const Path &p) { convertHousePresentationResources(root, assembled, p); }},
        {"media", [&](const Path &p) { convertMediaResources(root, p); }},
    };
    require(std::filesystem::create_directories(output), "Conversion destination was created by another operation");
    std::string activeStage;
    try
    {
        std::filesystem::create_directories(assembled);
        std::filesystem::create_directories(stagesRoot);
        for (size_t index = 0; index < stages.size(); ++index)
        {
            checkImportCancellation(cancelled);
            activeStage = stages[index].name;
            if (progress)
            {
                progress(activeStage, index + 1, stages.size() + 2);
            }
            checkImportCancellation(cancelled);
            const Path stageRoot = stagesRoot / activeStage;
            stages[index].convert(stageRoot);
            mergeStage(stageRoot, assembled, output / "reports", activeStage, cancelled);
        }
        activeStage = "package";
        if (progress)
        {
            progress(activeStage, stages.size() + 1, stages.size() + 2);
        }
        writeRuntimeZip(assembled / "engine", output / "engine.zip", cancelled);
        for (const char *pWorld : {"mm6", "mm7", "mm8"})
        {
            writeRuntimeZip(assembled / "worlds" / pWorld,
                output / "worlds" / (std::string(pWorld) + ".zip"), cancelled);
        }
        activeStage = "validate";
        if (progress)
        {
            progress(activeStage, stages.size() + 2, stages.size() + 2);
        }
        {
            checkImportCancellation(cancelled);
            Engine::AssetFileSystem files;
            require(files.initialize(output, output, Engine::AssetScaleTier::X1, "mm6"),
                "Cannot mount converted runtime packages");
            Game::GameDataLoader loader;
            std::string activeWorld;
            for (const std::string &file : sceneMaps.available)
            {
                checkImportCancellation(cancelled);
                const std::string &world = mapWorlds.at(file);
                if (world != activeWorld)
                {
                    require(files.switchActiveWorld(world), "Cannot switch converted world: " + world);
                    loader.setActiveWorldId(world);
                    require(loader.loadCommonForGameplay(files), "Converted common data could not load in " + world);
                    activeWorld = world;
                }
                require(loader.loadMapByFileNameForGameplay(files, file), "Converted map could not load: " + file);
                const std::optional<Game::MapAssetInfo> &map = loader.getSelectedMap();
                require(map && map->map.worldId == world && map->localEventProgram,
                    "Converted scene has incorrect world ownership or missing events: " + file);
                if (file.ends_with(".odm"))
                {
                    require(map->outdoorMapData && map->outdoorMapDeltaData && map->outdoorTerrainTextureAtlas
                        && map->outdoorActorPreviewBillboardSet,
                        "Converted outdoor geometry, companion or presentation could not load: " + file);
                    const bool needsModelTextures = std::any_of(map->outdoorMapData->bmodels.begin(),
                        map->outdoorMapData->bmodels.end(), [](const Game::OutdoorBModel &model)
                        {
                            return std::any_of(model.faces.begin(), model.faces.end(),
                                [](const Game::OutdoorBModelFace &face) { return !face.textureName.empty(); });
                        });
                    // Maps without textured building faces legitimately have no building texture set.
                    require(!needsModelTextures || map->outdoorBModelTextureSet,
                        "Converted outdoor model textures could not load: " + file);
                }
                else
                {
                    require(map->indoorMapData && map->indoorMapDeltaData && map->indoorTextureSet
                        && map->indoorActorPreviewBillboardSet,
                        "Converted indoor geometry, companion or presentation could not load: " + file);
                }
            }
        }
        checkImportCancellation(cancelled);
        std::ostringstream manifest;
        manifest << "{\"schema\":1,\"stage\":\"native-runtime-assembly\",\"runtime_ready\":false,"
            "\"common_data_validated\":true,\"starting_map_load_validated\":true,"
            "\"converted_maps\":[";
        for (size_t index = 0; index < sceneMaps.available.size(); ++index)
        {
            const std::string &file = sceneMaps.available[index];
            manifest << (index ? "," : "") << std::quoted(mapWorlds.at(file) + ":" + file);
        }
        manifest << "],\"unavailable_source_maps\":[";
        for (size_t index = 0; index < sceneMaps.missing.size(); ++index)
        {
            const std::string &file = sceneMaps.missing[index];
            manifest << (index ? "," : "") << std::quoted(mapWorlds.at(file) + ":" + file);
        }
        manifest << "],\"unsupported_event_maps\":[";
        for (size_t index = 0; index < excludedScenes.size(); ++index)
        {
            const std::string &file = excludedScenes[index];
            manifest << (index ? "," : "") << "{\"map\":" << std::quoted(mapWorlds.at(file) + ":" + file)
                << ",\"reason\":" << std::quoted(unsupportedEventMaps.at(file)) << '}';
        }
        manifest << "],\"converted_worlds\":[";
        for (size_t index = 0; index < sceneWorlds.size(); ++index)
        {
            manifest << (index ? "," : "") << std::quoted(sceneWorlds[index]);
        }
        manifest << "],\"default_world\":\"mm6\",\"converted_maps_load_validated\":true,"
            "\"full_gameplay_validated\":false,\"resource_audit\":{\"schema\":1,"
            "\"base_archives_checked\":true,\"chest_backgrounds_checked\":true,"
            "\"missing_mm8_base_archives\":[";
        bool firstMissing = true;
        for (const char *pName : {"Data/bitmaps.lod", "Data/sprites.lod", "Data/icons.lod"})
        {
            if (!std::filesystem::is_regular_file(root / pName))
            {
                manifest << (firstMissing ? "" : ",") << std::quoted(pName);
                firstMissing = false;
            }
        }
        manifest << "],\"missing_chest_backgrounds\":[";
        firstMissing = true;
        for (unsigned index = 1; index <= 8; ++index)
        {
            const std::string name = "icons/chest0" + std::to_string(index) + ".bmp";
            if (!std::filesystem::is_regular_file(assembled / "engine" / name))
            {
                manifest << (firstMissing ? "" : ",") << std::quoted(name);
                firstMissing = false;
            }
        }
        manifest << "]},\"stages\":[";
        for (size_t index = 0; index < stages.size(); ++index)
        {
            manifest << (index == 0 ? "" : ",") << std::quoted(stages[index].name);
        }
        manifest << "],\"limits\":[\"MM6/MM7/MM8 scenes with supported event commands are converted; "
            "Breach scene expansion remains pending\","
            "\"Missing original artwork/audio and MMExtension overlays remain unresolved\","
            "\"A successful conversion does not establish complete MM6/7/8 gameplay\"]}\n";
        std::ofstream report(output / "conversion.json", std::ios::binary);
        report.exceptions(std::ios::badbit | std::ios::failbit);
        report << manifest.str();
        report.close();
        std::filesystem::remove_all(work);
    }
    catch (const ImportCancelled &)
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(output, cleanupError);
        throw;
    }
    catch (const std::exception &error)
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(output, cleanupError);
        throw std::runtime_error("Runtime conversion failed at " + activeStage + ": " + error.what());
    }
    catch (...)
    {
        std::error_code cleanupError;
        std::filesystem::remove_all(output, cleanupError);
        throw;
    }
}
}
