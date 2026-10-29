#pragma once

#include "game/maps/MapRuntimeRestrictions.h"
#include "game/maps/MapDeltaData.h"
#include "game/maps/MapItemSourceData.h"
#include "game/indoor/IndoorMapData.h"

#include <cstdint>
#include <optional>
#include <array>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct IndoorSceneEnvironment
{
    int64_t lastVisitTime = 0;
    std::string skyTexture;
    int32_t dayBitsRaw = 0;
    uint32_t mapExtraBitsRaw = 0;
    int32_t fogWeakDistance = 0;
    int32_t fogStrongDistance = 0;
    int32_t ceiling = 0;
    std::array<uint8_t, 24> mapExtraReserved = {};
};

struct IndoorSceneDecorationFlag
{
    size_t entityIndex = 0;
    uint16_t flag = 0;
};

struct IndoorSceneFaceAttributeOverride
{
    size_t faceIndex = 0;
    std::optional<uint32_t> legacyAttributes;
    std::optional<uint8_t> facetType;
    std::optional<std::string> textureName;
    std::optional<uint16_t> textureFrameTableCog;
    std::optional<int16_t> textureDeltaU;
    std::optional<int16_t> textureDeltaV;
    std::optional<std::vector<int16_t>> textureUs;
    std::optional<std::vector<int16_t>> textureVs;
    std::optional<uint16_t> cogNumber;
    std::optional<uint16_t> cogTriggered;
    std::optional<uint16_t> cogTriggerType;
};

struct IndoorSceneDoor
{
    size_t doorIndex = 0;
    MapDeltaDoor door = {};
};

struct IndoorSceneSpawn
{
    size_t spawnIndex = 0;
    IndoorSpawn spawn = {};
};

struct IndoorScenePartyStartPoint
{
    size_t startIndex = 0;
    std::string sourceName;
    IndoorPartyStartPoint startPoint = {};
};

struct IndoorSceneInitialState
{
    MapDeltaLocationInfo locationInfo = {};
    std::vector<uint8_t> visibleOutlines;
    std::vector<IndoorSceneFaceAttributeOverride> faceAttributeOverrides;
    std::vector<IndoorSceneDecorationFlag> decorationFlags;
    std::vector<MapDeltaActor> actors;
    std::vector<MapDeltaSpriteObject> spriteObjects;
    std::vector<MapDeltaChest> chests;
    std::vector<IndoorSceneDoor> doors;
    MapDeltaPersistentVariables eventVariables = {};
};

struct IndoorSceneData
{
    int formatVersion = 0;
    std::string geometryFile;
    std::optional<std::string> legacyCompanionFile;
    MapRuntimeRestrictions runtimeRestrictions = {};
    IndoorSceneEnvironment environment = {};
    bool hasSpawns = false;
    std::vector<IndoorSceneSpawn> spawns;
    std::vector<IndoorScenePartyStartPoint> partyStartPoints;
    IndoorSceneInitialState initialState = {};
    MapItemSourceData itemSources = {};
};

class IndoorSceneYmlLoader
{
public:
    std::optional<IndoorSceneData> loadFromText(const std::string &yamlText, std::string &errorMessage) const;
    bool applyOverlayFromText(
        IndoorSceneData &sceneData,
        const std::string &yamlText,
        std::string &errorMessage) const;
    // Augment an already loaded BLV/DLV without replacing its population or persistent variables.
    bool applyMapRulesFromText(
        IndoorMapData &indoorMapData,
        MapDeltaData &mapDeltaData,
        MapRuntimeRestrictions &runtimeRestrictions,
        const std::string &yamlText,
        const std::string &mapFileName,
        std::string &errorMessage) const;
};

const IndoorSceneFaceAttributeOverride *findIndoorSceneFaceOverride(const IndoorSceneData &sceneData, size_t faceIndex);
IndoorSceneFaceAttributeOverride *findIndoorSceneFaceOverride(IndoorSceneData &sceneData, size_t faceIndex);
void applyIndoorSceneFaceOverride(const IndoorSceneFaceAttributeOverride &overrideEntry, IndoorFace &face);

bool buildIndoorMapStateFromScene(
    const IndoorSceneData &sceneData,
    IndoorMapData &indoorMapData,
    MapDeltaData &mapDeltaData,
    std::string &errorMessage);
}
