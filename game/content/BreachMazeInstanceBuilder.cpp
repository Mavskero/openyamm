#include "game/content/BreachMazeInstanceBuilder.h"

#include "game/FaceEnums.h"
#include "game/SpriteObjectDefs.h"
#include "game/content/BreachMazeSceneBuilder.h"
#include "game/events/EvtEnums.h"
#include "game/indoor/IndoorActorActivation.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint16_t DecorationShowOnMap = 0x0008;
constexpr uint16_t DecorationInvisible = 0x0020;
constexpr uint16_t LightOff = 0x0008;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::invalid_argument("Breach maze instance: " + message);
    }
}

std::string sourceSha256(const BreachMazePopulationCatalog &catalog, std::string_view name)
{
    const auto found = std::find_if(catalog.sources.begin(), catalog.sources.end(),
        [name](const BreachMazeSourceFingerprint &source) { return source.name == name; });
    require(found != catalog.sources.end() && found->sha256.size() == 64
        && found->sha256.find_first_not_of("0123456789abcdef") == std::string::npos,
        "missing original source SHA256: " + std::string(name));
    return found->sha256;
}

int positionCoordinate(int32_t halfCoordinate)
{
    return halfCoordinate / 2; // Original signed field assignment truncates toward zero.
}

int16_t lightCoordinate(int32_t halfCoordinate)
{
    const int coordinate = positionCoordinate(halfCoordinate);
    require(coordinate >= std::numeric_limits<int16_t>::min()
        && coordinate <= std::numeric_limits<int16_t>::max(), "light coordinate exceeds original field");
    return static_cast<int16_t>(coordinate);
}

void setFaceFlag(uint32_t &attributes, FaceAttribute flag, const std::optional<bool> &enabled)
{
    if (enabled)
    {
        const uint32_t bit = faceAttributeBit(flag);
        attributes = *enabled ? attributes | bit : attributes & ~bit;
    }
}

void validatePopulationIdentity(const IndoorMapData &originalMap, const MapDeltaData &baseDelta,
    const BreachMazeTemplate &mazeTemplate, const BreachMazeFloorDescriptor &floor,
    const BreachMazePopulationPlan &population, const BreachMazePopulationCatalog &catalog,
    const std::string &worldId, const std::string &mapFileName,
    const std::string &contentPackageId, uint32_t contentPackageSchema)
{
    require(worldId == "mmmerge" && mapFileName == "brbase.blv", "unsupported map identity");
    require(!contentPackageId.empty() && contentPackageSchema != 0, "missing content package identity");
    require(population.version == BreachMazePopulationVersion
        && population.contentFixVersion == BreachMazePopulationContentFixVersion
        && catalog.version == BreachMazePopulationVersion
        && catalog.contentFixVersion == BreachMazePopulationContentFixVersion, "unsupported population version");
    require(floor.identity.level >= 1 && floor.identity.level <= BreachMazeMaximumPlannedLevel
        && floor.identity.epoch != 0 && floor.identity.generationSerial != 0, "unsupported floor identity");
    require(floor.layout.level == floor.identity.level && floor.layout.seed == floor.seeds.layout,
        "layout differs from floor identity or seed");
    require(population.identity == floor.identity && population.seeds == floor.seeds
        && population.layoutFinalRandomState == floor.layout.finalRandomState
        && population.layoutRandomDrawCount == floor.layout.randomDrawCount,
        "frozen plan does not belong to this floor");
    require(population.templateFingerprint == catalog.templateFingerprint
        && catalog.templateFingerprint == breachMazePopulationTemplateFingerprint(mazeTemplate)
        && population.catalogFingerprint == catalog.contentFingerprint
        && catalog.contentFingerprint == breachMazePopulationCatalogFingerprint(catalog),
        "frozen plan/catalog/template identity mismatch");
    require(catalog.monsters.size() == 652 && catalog.bolsterMapRow.size() >= 9,
        "incomplete original mechanical inputs");
    require(baseDelta.actors.empty() && baseDelta.spriteObjects.empty() && originalMap.spawns.empty(),
        "instance geometry requires an unpopulated original base");
    for (const BreachMazePopulationFacePatch &patch : population.facePatches)
    {
        require(patch.faceIndex != floor.layout.endpoints.entrance.faceIndex
            && patch.faceIndex != floor.layout.endpoints.exit.faceIndex,
            "population patch would overwrite a floor transition");
    }
}

void validateMapRules(const BreachMazePopulationCatalog &catalog, const MergeBolsterMapRules &map)
{
    MergedBolsterMapTable sourceMaps;
    require(sourceMaps.loadFromRows({{}, catalog.bolsterMapRow}), "invalid source map rules");
    const MergedBolsterMapEntry *pMap = sourceMaps.findById(206);
    require(pMap != nullptr && pMap->bolsterKind == "LowerToEqual" && map.mapId == pMap->id
        && map.kind == MergeBolsterMapKind::LowerToEqual && map.spells == pMap->spells
        && map.summons == pMap->summons && pMap->bolsterExtra <= 65535
        && map.levelShift == static_cast<int32_t>(pMap->bolsterExtra), "map rules differ from original BrBase");
}

void validateInput(const BreachMazeInstanceBuildInput &input)
{
    validatePopulationIdentity(input.originalMap, input.baseDelta, input.mazeTemplate, input.floor,
        input.population, input.catalog, input.worldId, input.mapFileName,
        input.contentPackageId, input.contentPackageSchema);
    validateMapRules(input.catalog, input.combatContext.map);
}

void applyPopulationGeometry(BreachMazeSceneData &scene, const BreachMazePopulationPlan &population,
    const BreachMazePopulationCatalog &catalog)
{
    uint32_t previousFace = 0;
    bool firstFace = true;
    for (const BreachMazePopulationFacePatch &patch : population.facePatches)
    {
        require(patch.faceIndex < scene.map.faces.size() && (firstFace || patch.faceIndex > previousFace),
            "invalid or duplicate population face patch");
        firstFace = false;
        previousFace = patch.faceIndex;
        IndoorFace &face = scene.map.faces[patch.faceIndex];
        if (patch.texture)
        {
            require(!patch.texture->empty(), "empty population texture");
            face.textureName = *patch.texture;
        }
        if (patch.eventId)
        {
            face.cogTriggered = *patch.eventId;
        }
        setFaceFlag(face.attributes, FaceAttribute::Clickable, patch.click);
        setFaceFlag(face.attributes, FaceAttribute::PressurePlate, patch.step);
        setFaceFlag(face.attributes, FaceAttribute::IsSecret, patch.secret);
        scene.delta.faceAttributes[patch.faceIndex] = face.attributes;
    }

    require(scene.map.entities.size() == population.decorations.size()
        && scene.map.lights.size() == population.lights.size() && scene.map.sectors.size() == 2,
        "source decoration/light/sector slots do not match population");
    scene.delta.decorationFlags.resize(population.decorations.size());
    for (size_t index = 0; index < population.decorations.size(); ++index)
    {
        const BreachMazeDecorationPlan &decoration = population.decorations[index];
        require(decoration.slot == index && decoration.decorationId < catalog.decorations.size()
            && decoration.eventId <= std::numeric_limits<uint16_t>::max(), "invalid decoration slot or event");
        const BreachMazeDecorationSource &source = catalog.decorations[decoration.decorationId];
        require(source.name == decoration.resolvedName, "decoration identity differs from source catalog");
        IndoorEntity &entity = scene.map.entities[index];
        entity.decorationListId = decoration.decorationId;
        entity.name = decoration.resolvedName;
        entity.x = positionCoordinate(decoration.position.x);
        entity.y = positionCoordinate(decoration.position.y);
        entity.z = positionCoordinate(decoration.position.z);
        entity.facing = decoration.facing;
        entity.eventIdSecondary = static_cast<uint16_t>(decoration.eventId);
        entity.specialTrigger = decoration.interaction.eventVariable;
        entity.aiAttributes = decoration.sourceAttributes & ~(DecorationInvisible | DecorationShowOnMap);
        if (decoration.invisible) entity.aiAttributes |= DecorationInvisible;
        if (decoration.showOnMap) entity.aiAttributes |= DecorationShowOnMap;
        scene.delta.decorationFlags[index] = entity.aiAttributes;

        const BreachMazeLightPlan &light = population.lights[index];
        require(light.slot == index, "light slot differs from plan order");
        IndoorLight &target = scene.map.lights[index];
        target.x = lightCoordinate(light.position.x);
        target.y = lightCoordinate(light.position.y);
        target.z = lightCoordinate(light.position.z);
        target.radius = light.sourceProperties.radius;
        target.red = light.sourceProperties.color[0];
        target.green = light.sourceProperties.color[1];
        target.blue = light.sourceProperties.color[2];
        target.type = light.sourceProperties.type;
        const uint16_t attributes = static_cast<uint16_t>(light.sourceProperties.attributes) & ~LightOff;
        target.attributes = static_cast<int16_t>(attributes | (light.off ? LightOff : 0));
        target.brightness = light.sourceProperties.brightness;
        target.id = light.sourceProperties.id;
    }
    scene.map.sectors[1].minAmbientLightLevel = population.sectorOneDarkness;
}

MapDeltaSpriteObject materializeItem(const BreachMazeResolvedItem &resolved,
    const BreachMazeItemRequest &request, const ItemTable &items, const ObjectTable &objects)
{
    const ItemDefinition *pDefinition = items.get(resolved.item.objectDescriptionId);
    require(pDefinition != nullptr && pDefinition->spriteIndex > 0
        && pDefinition->spriteIndex <= std::numeric_limits<int16_t>::max(),
        "resolved item lacks a representable source object appearance");
    const std::optional<uint16_t> description = objects.findDescriptionIdByObjectId(
        static_cast<int16_t>(pDefinition->spriteIndex));
    require(description.has_value(), "resolved item object is absent; cannot reroll item");
    const ObjectEntry *pObject = objects.get(*description);
    require(pObject != nullptr && (pObject->flags & ObjectDescNoSprite) == 0 && pObject->spriteId != 0,
        "resolved item has no drawable object sprite; cannot reroll item");
    MapDeltaSpriteObject result;
    result.objectDescriptionId = *description;
    result.spriteId = pObject->spriteId;
    result.x = positionCoordinate(request.position.x);
    result.y = positionCoordinate(request.position.y);
    result.z = positionCoordinate(request.position.z);
    result.initialX = result.x;
    result.initialY = result.y;
    result.initialZ = result.z;
    result.sectorId = 1;
    result.containingItem = resolved.item;
    result.containingGoldAmount = 0; // V1's fixed/random ground requests never request a gold amount.
    return result;
}
}

BreachMazePreparedInstance buildBreachMazeInstance(const BreachMazeInstanceBuildInput &input)
{
    validateInput(input);
    std::vector<std::vector<std::string>> monsterRows;
    std::vector<std::vector<std::string>> bolsterRows(1); // Existing TXT loader skips its header row.
    monsterRows.reserve(input.catalog.monsters.size());
    for (const BreachMazeMonsterSource &monster : input.catalog.monsters)
    {
        monsterRows.emplace_back(monster.monsterRow.begin(), monster.monsterRow.end());
        if (monster.bolsterRow)
        {
            bolsterRows.emplace_back(monster.bolsterRow->begin(), monster.bolsterRow->end());
        }
    }
    const MergeMonsterSourceProfile source = MergeMonsterSourceProfile::fromRows(
        monsterRows, input.monsters, input.projectiles, input.spells);
    const MergeBolsterProfile bolster = MergeBolsterProfile::fromFormulas(input.bolsterFormulas.entries());
    MergedBolsterFormulaTable sourceFormulas;
    require(sourceFormulas.loadFromRows(input.catalog.bolsterFormulaRows), "missing source formulas");
    (void)MergeBolsterProfile::fromFormulas(sourceFormulas.entries());
    MergedBolsterMonsterTable sourceSettings;
    require(sourceSettings.loadFromRows(bolsterRows), "missing source monster settings");
    const std::string catalogSha = sourceSha256(input.catalog, "Data/mmmerge.T.lod::monsters.txt");
    require(catalogSha == MergeMonsterSourceMemberSha256, "monster source SHA256 differs from validated profile");
    const std::string profileSha = sourceSha256(input.catalog, "Data/Tables/Bolster - formulas.txt");

    BreachMazeSceneData scene = buildBreachMazeScene(input.originalMap, input.baseDelta, input.mazeTemplate,
        input.floor.layout, input.population.paletteByRoom);
    applyPopulationGeometry(scene, input.population, input.catalog);
    // The original empty BrBase DLV has a zeroed location header. A generated floor owns
    // its populated geometry and must persist the actual counts with its live snapshot.
    scene.delta.locationInfo.totalFacesCount = static_cast<uint32_t>(scene.map.faces.size());
    scene.delta.locationInfo.decorationCount = static_cast<uint32_t>(scene.map.entities.size());
    scene.delta.locationInfo.bmodelCount = 0;
    BreachMazePreparedInstance result;
    result.instance.actorCombat.reserve(input.population.actors.size());
    scene.delta.actors.reserve(input.population.actors.size());
    const MergeMonsterGenerationServices generationServices{
        source, bolster, input.bolsterMonsters, input.projectiles, input.spells};
    result.instance.monsterGeneration = makeMergeMonsterGenerationState(generationServices,
        {input.contentPackageId, input.contentPackageSchema, input.combatContext, input.floor.seeds.population,
            input.floor.identity.epoch, input.floor.identity.level, input.floor.identity.generationSerial});
    MergeMonsterGenerationState &generation = *result.instance.monsterGeneration;
    require(generation.catalogSha256 == catalogSha && generation.profileSha256 == profileSha,
        "generation profile differs from authenticated source catalog");
    for (const BreachMazeActorRequest &request : input.population.actors)
    {
        require(request.ordinal == scene.delta.actors.size() && request.monsterId > 0 && request.monsterId <= 651
            && (request.hitPointMultiplier == 1 || request.hitPointMultiplier == 2), "invalid actor request");
        const uint16_t id = static_cast<uint16_t>(request.monsterId);
        const uint16_t familyId = static_cast<uint16_t>((id + 2) / 3);
        require(mergeMonsterGenerationFamilySeed(generation, familyId) == request.familyResolveKey
            && mergeMonsterGenerationActorSeed(generation, generation.nextActorOrdinal) == request.actorResolveKey,
            "generation keys differ from frozen population requests");
        MergeMonsterFamilyContext context = input.combatContext;
        context.familySeed = request.familyResolveKey;
        require(source.familyInput(id, input.bolsterMonsters, context) == source.familyInput(id, sourceSettings, context),
            "loaded monster settings differ from the frozen source catalog");
        const GameplayActorCombatState generated = prepareMergeMonsterGenerationActor(
            generation, generationServices, id, MergeMonsterGenerationMode::LuaSummon);
        require(generated.resolved() != nullptr, "source generation did not produce a resolved actor");
        GameplayMergeResolvedActorCombat resolved = *generated.resolved();
        const int64_t finalHitPoints = int64_t(resolved.combat.fullHitPoints) * request.hitPointMultiplier;
        require(finalHitPoints > 0 && finalHitPoints <= std::numeric_limits<int32_t>::max(),
            "post-bolster HP multiplier exceeds the persistent actor range");
        resolved.combat.fullHitPoints = static_cast<int32_t>(finalHitPoints);
        if (request.hitPointMultiplier == 2) resolved.appliedModifiers |= GameplayActorMazeDeadEndDoubleHp;
        resolved.combat.hostileType = request.hostilityType;
        resolved.combat.rewards.goldDiceCount = request.goldDiceCount;
        resolved.combat.rewards.goldDiceSides = request.goldDiceSides;
        source.validateResolvedResources(resolved.combat, input.projectiles, input.spells);
        const MonsterEntry *pDescriptor = input.monsters.findById(static_cast<int16_t>(id));
        const MonsterTable::MonsterStatsEntry *pStats = input.monsters.findStatsById(static_cast<int16_t>(id));
        require(pDescriptor != nullptr && pStats != nullptr, "resolved monster lacks its native render descriptor");
        require(resolved.combat.moveSpeed <= std::numeric_limits<uint16_t>::max(), "actor speed exceeds delta field");
        MapDeltaActor actor;
        actor.diagnosticSourceActorIndex = request.ordinal;
        actor.name = pStats->name;
        actor.monsterId = static_cast<int16_t>(id);
        actor.monsterInfoId = actor.monsterId;
        actor.hp = resolved.combat.fullHitPoints;
        actor.attributes = indoorGeneratedActorAttributes(request.hostile);
        if (request.showOnMap) actor.attributes |= static_cast<uint32_t>(EvtActorAttribute::Nearby);
        if (request.onAlertMap) actor.attributes |= static_cast<uint32_t>(EvtActorAttribute::AlertStatus);
        actor.hostilityType = request.hostilityType;
        actor.radius = pDescriptor->radius;
        actor.height = pDescriptor->height;
        actor.moveSpeed = static_cast<uint16_t>(resolved.combat.moveSpeed);
        actor.x = positionCoordinate(request.position.x);
        actor.y = positionCoordinate(request.position.y);
        actor.z = positionCoordinate(request.position.z);
        actor.sectorId = 1;
        GameplayActorCombatState combat(std::move(resolved));
        std::string error;
        require(validateGameplayActorCombatState(combat, actor.hp, error), error);
        scene.delta.actors.push_back(std::move(actor));
        result.instance.actorCombat.push_back(std::move(combat));
    }

    result.itemReceipts.reserve(input.population.items.size());
    scene.delta.spriteObjects.reserve(input.population.items.size());
    for (const BreachMazeItemRequest &request : input.population.items)
    {
        require(request.ordinal == scene.delta.spriteObjects.size(), "item ordinal differs from frozen plan order");
        std::string error;
        const std::optional<BreachMazeResolvedItem> resolved = resolveBreachMazeItem(
            request, input.items, input.standardEnchants, input.specialEnchants, error);
        require(resolved.has_value(), error);
        scene.delta.spriteObjects.push_back(materializeItem(*resolved, request, input.items, input.objects));
        result.itemReceipts.push_back(resolved->receipt);
    }
    result.instance.worldId = input.worldId;
    result.instance.mapFileName = input.mapFileName;
    result.instance.runtimeRestrictions = MapRuntimeRestrictions{};
    result.instance.runtimeRestrictions->allowLloydsBeacon = false;
    result.instance.runtimeRestrictions->townPortalUsesDimensionDoor = true;
    result.instance.geometry = std::move(scene.map);
    result.instance.delta = std::move(scene.delta);
    result.instance.itemSources = MapItemSourceData{};
    return result;
}

PreparedIndoorMapInstance restoreBreachMazeInstance(const BreachMazeInstanceRestoreInput &input)
{
    validatePopulationIdentity(input.originalMap, input.baseDelta, input.mazeTemplate, input.floor,
        input.population, input.catalog, input.worldId, input.mapFileName,
        input.contentPackageId, input.contentPackageSchema);
    require(input.snapshot.mapDeltaData.has_value(), "saved floor lacks its authoritative delta");
    const MapDeltaData &delta = *input.snapshot.mapDeltaData;
    const IndoorWorldRuntime::Snapshot &world = input.snapshot.worldRuntime;
    require(world.mergeMonsterGeneration.has_value(), "saved floor lacks its monster generation state");
    const MergeMonsterGenerationState &generation = *world.mergeMonsterGeneration;
    std::string error;
    require(validateMergeMonsterGenerationState(generation, error), error);
    require(generation.generationEpoch == input.floor.identity.epoch
        && generation.generationLevel == input.floor.identity.level
        && generation.generationSerial == input.floor.identity.generationSerial
        && generation.populationSeed == input.floor.seeds.population,
        "saved monster generation belongs to a different floor");
    require(generation.contentPackageId == input.contentPackageId
        && generation.contentPackageSchema == input.contentPackageSchema
        && generation.catalogSha256 == sourceSha256(input.catalog, "Data/mmmerge.T.lod::monsters.txt")
        && generation.profileSha256 == sourceSha256(input.catalog, "Data/Tables/Bolster - formulas.txt"),
        "saved generation content differs from the authenticated catalog");
    const MergeMonsterSourceEntry *pSource = input.monsterSource.findById(1);
    require(pSource != nullptr
        && pSource->identity.parserVersion == generation.sourceProfileVersion
        && pSource->identity.contentFixVersion == generation.sourceContentFixVersion
        && pSource->identity.sourceMemberSha256 == generation.catalogSha256
        && pSource->identity.mechanicalFingerprint == generation.mechanicalFingerprint
        && pSource->identity.descriptorFingerprint == generation.descriptorFingerprint,
        "saved generation differs from the loaded original monster profile");
    validateMapRules(input.catalog, generation.context.map);
    require(delta.faceAttributes.size() == input.mazeTemplate.faces.size()
        && delta.decorationFlags.size() == input.mazeTemplate.decorationCount
        && delta.locationInfo.totalFacesCount == input.mazeTemplate.faces.size()
        && delta.locationInfo.decorationCount == input.mazeTemplate.decorationCount,
        "saved delta geometry metadata differs from the authenticated template");
    require(world.mapActorAiStates.size() == delta.actors.size()
        && delta.actors.size() >= input.population.actors.size()
        && generation.nextActorOrdinal == delta.actors.size(),
        "saved actor order/count differs from the population or generator cursor");

    PreparedIndoorMapInstance result;
    result.runtimeRestrictions = MapRuntimeRestrictions{};
    result.runtimeRestrictions->allowLloydsBeacon = false;
    result.runtimeRestrictions->townPortalUsesDimensionDoor = true;
    result.actorCombat.reserve(delta.actors.size());
    for (size_t index = 0; index < delta.actors.size(); ++index)
    {
        const MapDeltaActor &actor = delta.actors[index];
        const IndoorWorldRuntime::MapActorAiState &ai = world.mapActorAiStates[index];
        require(validateGameplayActorCombatState(ai.combatState, actor.hp, error), error);
        require(validateGameplayActorSpecialRuntimeState(ai.specialState, ai.combatState, error), error);
        const GameplayMergeResolvedActorCombat *pResolved = ai.combatState.resolved();
        require(pResolved != nullptr, "saved maze actor lacks authoritative resolved combat");
        const GameplayMergeResolvedActorCombat &resolved = *pResolved;
        require(resolved.generationEpoch == generation.generationEpoch
            && resolved.generationLevel == generation.generationLevel
            && resolved.generationSerial == generation.generationSerial
            && resolved.contentPackageId == generation.contentPackageId
            && resolved.contentPackageSchema == generation.contentPackageSchema
            && resolved.catalogSha256 == generation.catalogSha256
            && resolved.profileSha256 == generation.profileSha256,
            "saved actor belongs to different generation content");
        require(actor.monsterId == resolved.combat.id && actor.monsterInfoId == resolved.combat.id
            && input.monsterSource.findById(resolved.combat.id) != nullptr,
            "saved delta actor identity differs from resolved combat");
        const uint16_t familyId = static_cast<uint16_t>((resolved.combat.id + 2) / 3);
        require(resolved.actorSeed == mergeMonsterGenerationActorSeed(generation, index)
            && resolved.familySeed == mergeMonsterGenerationFamilySeed(generation, familyId),
            "saved actor seeds differ from their frozen generation order");
        require(std::any_of(generation.preparedFamilies.begin(), generation.preparedFamilies.end(),
            [familyId](const MergeMonsterPreparedFamily &family) { return family.familyId == familyId; }),
            "saved actor family is absent from the frozen generator");
        if (index < input.population.actors.size())
        {
            const BreachMazeActorRequest &request = input.population.actors[index];
            require(request.ordinal == index && request.monsterId == resolved.combat.id
                && request.actorResolveKey == resolved.actorSeed && request.familyResolveKey == resolved.familySeed
                && (request.hitPointMultiplier == 1 || request.hitPointMultiplier == 2)
                && ((resolved.appliedModifiers & GameplayActorMazeDeadEndDoubleHp) != 0)
                    == (request.hitPointMultiplier == 2),
                "saved population actor differs from its frozen request");
        }
        result.actorCombat.push_back(ai.combatState);
    }

    BreachMazeSceneData scene = buildBreachMazeScene(input.originalMap, input.baseDelta, input.mazeTemplate,
        input.floor.layout, input.population.paletteByRoom);
    applyPopulationGeometry(scene, input.population, input.catalog);
    result.worldId = input.worldId;
    result.mapFileName = input.mapFileName;
    result.geometry = std::move(scene.map);
    // Discard every generated delta field, including surfaceRevision and initial decoration flags.
    // The snapshot also retains removed actors, picked-up items, current HP, mechanisms and all timers.
    result.delta = delta;
    result.monsterGeneration = generation;
    result.itemSources = MapItemSourceData{};
    return result;
}

}
