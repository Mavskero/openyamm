#include "tools/mmerge_import/SurfaceResources.h"
#include "tools/mmerge_import/SpriteFrameResources.h"
#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/maps/TerrainTileData.h"
#include "game/tables/TextureFrameTable.h"
#include "game/tables/SurfaceMaterialTable.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace OpenYAMM;
using namespace OpenYAMM::Game;
using Rows = std::vector<std::vector<std::string>>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void checkTerrainRows()
{
    const Rows source{{"#", "Name", "Id", "TileSet", "Section", "Burn", "Water", "Block", "Repulse", "Flat",
        "Wave", "NoDraw", "WaterTransition", "Transition", "ScrollDown", "ScrollUp", "ScrollLeft", "ScrollRight"},
        {"0", "Pending", "0", "255", "255", "-", "-", "-", "-", "-", "-", "x", "-", "-", "-", "-", "-", "-"},
        {"1", "WATER01", "19", "4", "2", "x", "X", "-", "-", "-", "-", "-", "x", "-", "-", "-", "-", "-"}};
    const Rows rows = Import::convertTerrainRows(source);
    require(rows.size() == 3 && rows[1][6] == "64"
        && rows[2] == Rows::value_type{"1", "water01", "19", "0", "4", "2", "259"},
        "Terrain conversion changed source IDs, names or flag bits");
    Rows withEmptySlot = source;
    withEmptySlot[2][1].clear();
    require(Import::convertTerrainRows(withEmptySlot)[2][1].empty(),
        "Empty legacy terrain slots must retain their indices without invented texture names");
    for (int fault = 0; fault < 5; ++fault)
    {
        Rows broken = source;
        switch (fault)
        {
        case 0: broken[0][1] = "unknown"; break;
        case 1: broken[2][0] = "3"; break;
        case 2: broken[2][1] = "../water"; break;
        case 3: broken[2][2] = "65536"; break;
        case 4: broken[2][5] = "yes"; break;
        }
        bool rejected = false;
        try
        {
            Import::convertTerrainRows(broken);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        require(rejected, "Malformed terrain table was accepted");
    }
    OutdoorMapData map;
    map.tileSetLookupIndices = {100, 200, 300, 400};
    for (uint8_t master = 0; master < 3; ++master)
    {
        map.masterTile = master;
        const int direct = master == 0 ? 1 : master == 1 ? 451 : 631;
        require(resolveTerrainDescriptorIndex(map, 1, true) == direct
            && resolveTerrainDescriptorIndex(map, 1, false) == 1
            && resolveTerrainDescriptorIndex(map, 0, true) == 0
            && resolveTerrainDescriptorIndex(map, 90, false) == 100
            && resolveTerrainDescriptorIndex(map, 233, true) == 435
            && !resolveTerrainDescriptorIndex(map, 234, true), "Terrain remapping differs across source tables");
    }
    std::cout << "Terrain indices/flags, three master-table mappings and five malformed inputs passed\n";
}

void checkEmptySpriteSelection()
{
    Rows rows(1, std::vector<std::string>(24, "-"));
    rows[0][0] = "0";
    rows[0][1] = "torch";
    rows[0][2] = "torch01";
    rows[0][3] = "65536";
    rows[0][5] = "x";
    rows[0][21] = "0";
    rows[0][22] = "1";
    rows[0][23] = "8";
    const std::set<std::string> empty;
    const Import::ConvertedSpriteFrames selected = Import::convertSpriteFrameRows(rows, &empty);
    SpriteFrameTable table;
    std::string error;
    require(selected.frameCount == 0 && selected.groupCount == 0 && selected.paletteIds.empty()
        && table.getFrame(0, 0) == nullptr,
        "A scene with no sprite references must not invent artwork");
    const Import::ConvertedSpriteFrames all = Import::convertSpriteFrameRows(rows);
    require(all.frameCount == 1 && all.groupCount == 1, "Empty selection changed full SFT conversion");
    require(table.loadFromYaml(all.yaml, error) && table.loadFromYaml(selected.yaml, error, true)
        && table.getFrame(0, 0) && table.getFrame(0, 0)->textureName == "torch01",
        "An empty scene contribution must load without removing the shared sprite table");
    rows[0][5] = "-";
    bool rejected = false;
    try
    {
        Import::convertSpriteFrameRows(rows, &empty);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    require(rejected, "Empty scene selection bypassed source SFT validation");
}

void checkSurfaceFrames()
{
    const Rows source{{"#", "Name", "Time", "GroupStart"}, {"0", "null", "0", "x"},
        {"1", "water1", "2", "x"}, {"2", "water2", "5", "-"}, {"3", "wall", "3", "x"}};
    TextureFrameTable table;
    require(table.loadRows(Import::convertTextureFrameRows(source)), "Cannot load converted TFT");
    const std::optional<SurfaceAnimationSequence> water = table.findAnimationSequenceByName("WATER1");
    const std::optional<SurfaceAnimationSequence> indexed = table.findAnimationSequenceByIndex(2);
    const std::optional<SurfaceAnimationSequence> wall = table.findAnimationSequenceByIndex(3);
    require(water && indexed && wall && water->frames.size() == 2 && water->animationLengthTicks == 56
        && water->frameIndexAtTicks(15) == 0 && water->frameIndexAtTicks(16) == 1
        && water->frameIndexAtTicks(55) == 1 && water->frameIndexAtTicks(56) == 0,
        "Imported animation lost source timing or group boundaries");
    require(indexed->frames.front().textureName == "water1" && wall->frames.size() == 1
        && wall->animationLengthTicks == 24 && table.findAnimationIdByName("water1") == 1,
        "Imported TFT shifted source indices or merged adjacent groups");
    for (int fault = 0; fault < 6; ++fault)
    {
        Rows damaged = source;
        switch (fault)
        {
        case 0: damaged[1][3] = "-"; break;
        case 1: damaged[2][0] = "4"; break;
        case 2: damaged[2][1] = "../wall"; break;
        case 3: damaged[2][2] = "32767"; break;
        case 4: damaged[3][3] = "unknown"; break;
        case 5: damaged[2].pop_back(); break;
        }
        bool rejected = false;
        try
        {
            Import::convertTextureFrameRows(damaged);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        require(rejected, "Malformed TFT was accepted");
    }
    std::cout << "TFT timing, index preservation and six malformed-input checks passed\n";
}

void checkHardwareWater()
{
    const Rows source{{"SW texture", "HW texture prefix"}, {"7wtrtyl", "7hdwtr"}, {"lavtyl", "hdlav"},
        {"oiltyl", "hwoil"}, {"tartyl", "hwoil"}};
    const std::string script = "-- Override water settings from mm8.ini\n"
        "\tGame.PatchOptions.HDWTRCount = 14\r\n\tGame.PatchOptions.HDWTRDelay = 15 -- source ticks\n";
    SurfaceMaterialTable materials;
    std::string error;
    require(materials.loadFromYaml(Import::convertHardwareWaterMaterials(source, script), error), error);
    const SurfaceMaterialDefinition *pWater = materials.findMatch("7WTRTYL", 0, true);
    const SurfaceMaterialDefinition *pLava = materials.findMatch("lavtyl", 0, false);
    const SurfaceMaterialDefinition *pTar = materials.findMatch("tartyl", 0, true);
    require(pWater && pWater->semantic == SurfaceMaterialSemantic::Water && pWater->terrainTransitionOverlay
        && pWater->animation.frames.size() == 14 && pWater->animation.frames.front().textureName == "7hdwtr000"
        && pWater->animation.frames.back().textureName == "7hdwtr013"
        && pWater->animation.animationLengthTicks == 210 && pWater->animation.frameIndexAtTicks(14) == 0
        && pWater->animation.frameIndexAtTicks(15) == 1 && pWater->animation.frameIndexAtTicks(209) == 13
        && pWater->animation.frameIndexAtTicks(210) == 0, "Hardware-water binding/timing changed");
    require(pLava && pLava->semantic == SurfaceMaterialSemantic::Lava
        && pTar && pTar->animation.frames.front().textureName == "hwoil000"
        && !materials.findMatch("wall", 0, true), "Hardware-water semantic/matching changed");
    for (int fault = 0; fault < 10; ++fault)
    {
        Rows damaged = source;
        std::string damagedScript = script;
        switch (fault)
        {
        case 0: damaged[0][0] = "unknown"; break;
        case 1: damaged.push_back(source[1]); break;
        case 2: damaged[1][1] = "../water"; break;
        case 3: damaged[1][0] = "unknown"; break;
        case 4: damaged[1].pop_back(); break;
        case 5: damagedScript.replace(damagedScript.find("14"), 2, "0"); break;
        case 6: damagedScript.replace(damagedScript.find("14"), 2, "1001"); break;
        case 7: damagedScript.replace(damagedScript.find("14"), 2, "7 + 7"); break;
        case 8: damagedScript += "Game.PatchOptions.HDWTRDelay = 20\n"; break;
        case 9: damagedScript.clear(); break;
        }
        bool rejected = false;
        try
        {
            Import::convertHardwareWaterMaterials(damaged, damagedScript);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        require(rejected, "Unsupported hardware-water data was accepted");
    }
    std::cout << "Hardware-water matching, frame timing and ten malformed-input checks passed\n";
}

std::optional<EventRuntimeState::PendingMapMove> mapMove(const EventRuntimeState &state)
{
    if (state.pendingMapMove)
    {
        return state.pendingMapMove;
    }
    return state.pendingDialogueContext ? state.pendingDialogueContext->transitionMapMove : std::nullopt;
}

void checkScenes(const std::filesystem::path &root)
{
    Engine::AssetFileSystem files;
    require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount runtime");
    GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    require(loader.loadCommonForGameplay(files), "Cannot load shared gameplay data");
    EventRuntime runtime(&loader.getHouseTable(), &loader.getNpcDialogTable());
    Party party;
    PartySeed seed;
    Character member;
    member.name = "地牢数据验证";
    member.health = member.maxHealth = 35;
    seed.members.push_back(member);
    party.seed(seed);
    party.setItemTable(&loader.getItemTable());
    struct Scene
    {
        const char *pFile;
        uint16_t entrance;
        uint16_t exit;
        uint16_t chest;
    };
    for (const Scene &scene : {Scene{"6d01.blv", 101, 51, 41}, Scene{"6d02.blv", 102, 50, 12},
            Scene{"6d18.blv", 103, 90, 44}})
    {
        require(loader.loadMapByFileNameForGameplay(files, "oute3.odm"), "Cannot load town");
        const MapAssetInfo &town = *loader.getSelectedMap();
        EventRuntimeState townState;
        runtime.initializeMapRuntimeState(town.outdoorMapDeltaData, townState);
        const auto enter = [&]
        {
            require(runtime.executeEventById(town.localEventProgram, town.globalEventProgram,
                scene.entrance, townState, &party), "Entrance event failed");
        };
        if (scene.entrance == 101)
        {
            enter();
            require(!mapMove(townState), "Locked Goblinwatch entrance allowed a party without its key");
            // A controlled fixture verifies the real key gate; it is not a claim of earning this quest item in play.
            require(party.tryGrantItem(2109, 1), "Cannot prepare key-gate fixture");
        }
        enter();
        const std::optional<EventRuntimeState::PendingMapMove> incoming = mapMove(townState);
        require(incoming && incoming->mapName == scene.pFile, "Entrance targeted the wrong dungeon");
        if (scene.entrance == 101)
        {
            require(party.hasQuestBit(1324) && party.inventoryItemCount(2109) == 0,
                "Entrance did not consume its real key");
        }
        require(loader.loadMapByFileNameForGameplay(files, *incoming->mapName), "Entrance target could not load");
        const MapAssetInfo &map = *loader.getSelectedMap();
        require(map.indoorMapData && map.indoorMapDeltaData && map.indoorTextureSet,
            "Dungeon is missing geometry, companion or surface textures");
        EventRuntimeBindingReport binding;
        require(runtime.validateProgramBindings(map.localEventProgram, map.globalEventProgram, binding),
            "Dungeon event bindings are incomplete");
        EventRuntimeState state;
        require(runtime.buildOnLoadState(map.localEventProgram, map.globalEventProgram, map.indoorMapDeltaData,
            state, &party), "Dungeon entry handlers failed");
        require(runtime.executeEventById(map.localEventProgram, map.globalEventProgram, 1, state, &party)
            && !state.lastAffectedMechanismIds.empty(), "Dungeon door did not address a mechanism");
        for (uint32_t id : state.lastAffectedMechanismIds)
        {
            require(state.mechanisms.contains(id), "Door event referenced an absent mechanism");
        }
        require(runtime.executeEventById(map.localEventProgram, map.globalEventProgram, scene.chest, state, &party)
            && state.openedChestIds == std::vector<uint32_t>{1} && map.indoorMapDeltaData->chests.size() > 1,
            "Chest event did not address its companion chest");
        const size_t surfaceCount = map.indoorTextureSet->textures.size();
        const size_t actorCount = map.indoorActorPreviewBillboardSet
            ? map.indoorActorPreviewBillboardSet->billboards.size() : 0;
        const size_t missingActors = map.indoorActorPreviewBillboardSet
            ? map.indoorActorPreviewBillboardSet->missingTextureActorCount : 0;
        require(surfaceCount > 0 && actorCount > 0 && missingActors == 0,
            "Incomplete dungeon presentation: " + std::string(scene.pFile) + ", surfaces="
                + std::to_string(surfaceCount) + ", actors=" + std::to_string(actorCount)
                + ", missing actors=" + std::to_string(missingActors));
        require(runtime.executeEventById(map.localEventProgram, map.globalEventProgram, scene.exit, state, &party),
            "Dungeon exit handler failed");
        const std::optional<EventRuntimeState::PendingMapMove> outgoing = mapMove(state);
        require(outgoing && outgoing->mapName == "oute3.odm", "Dungeon exit did not return to town");
        require(loader.loadMapByFileNameForGameplay(files, *outgoing->mapName), "Return target could not load");
        std::cout << scene.pFile << ": entrance/key gate, on-load, door, chest and return events passed; surfaces="
            << surfaceCount << ", actor previews=" << actorCount
            << ", missing actor previews=" << missingActors << '\n';
    }
    std::cout << "This checks imported data and shared event execution; "
        "it does not validate walking, rendering or combat.\n";
}
}

int main(int argc, char **argv)
{
    try
    {
        require(argc <= 2, "Usage: scene_import_runtime_check [RUNTIME_DIRECTORY]");
        checkTerrainRows();
        checkEmptySpriteSelection();
        checkSurfaceFrames();
        checkHardwareWater();
        if (argc == 2)
        {
            checkScenes(std::filesystem::canonical(argv[1]));
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
