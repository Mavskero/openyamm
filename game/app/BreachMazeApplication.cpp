#include "game/app/GameApplication.h"
#include "game/FaceEnums.h"
#include "game/content/BreachMazeInstanceBuilder.h"
#include "game/content/BreachMazeLifecycle.h"
#include "game/gameplay/GameplayBolsterRuntime.h"
#include "game/gameplay/BreachMazeInteractionRuntime.h"
#include "game/maps/MapIdentity.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
using MazeRequest = EventRuntimeState::PendingBreachMazeTransitionRequest;

BreachMazeQuestView mazeQuestView(const GameSession &session, const EventRuntimeState *pEvents)
{
    const auto value = [&](const char *pName)
    {
        const std::string key = std::string("MMerge.CrossContinents.") + pName;
        if (pEvents)
        {
            const auto found = pEvents->namedGlobalVars.find(key);
            if (found != pEvents->namedGlobalVars.end()) return found->second != 0;
        }
        return session.namedGlobalVar(key) != 0;
    };
    // The authenticated Merge global source initializes Quest_CrossContinents to a table at startup.
    return {true, value("EnteredBasement"), value("QuestFinished")};
}

void stageMazeArrival(IndoorSceneRuntime &scene, const BreachMazeArrival &arrival)
{
    IndoorPartyRuntime::Snapshot placement = scene.partyRuntime().snapshot();
    placement.movementState = {};
    placement.movementState.x = arrival.x;
    placement.movementState.y = arrival.y;
    placement.movementState.footZ = arrival.footZ;
    placement.movementState.fallStartZ = arrival.footZ;
    placement.movementAccumulatorSeconds = 0;
    placement.jumpState = {};
    scene.partyRuntime().restoreSnapshot(placement);
}
}

bool GameApplication::breachMazeRequestMatches(const MazeRequest &request) const
{
    const auto &map = m_gameDataLoader.getSelectedMap();
    if (!m_pMapSceneRuntime || !map || map->map.worldId != "mmmerge"
        || map->map.id != request.sourceMapId || map->map.canonicalId != request.sourceCanonicalMapId
        || normalizeMapFileStem(map->map.fileName) != normalizeMapFileStem(request.sourceMapName)
        || normalizeMapFileStem(m_gameSession.currentMapFileName()) != normalizeMapFileStem(map->map.fileName))
        return false;
    const bool step = request.kind == MazeRequest::Kind::Up || request.kind == MazeRequest::Kind::Down;
    const uint16_t eventId = request.kind == MazeRequest::Kind::Up ? 1
        : request.kind == MazeRequest::Kind::Down ? 2 : request.kind == MazeRequest::Kind::BrAlvar53 ? 53 : 54;
    const std::string stem = step ? "brbase" : request.kind == MazeRequest::Kind::Breach54 ? "breach" : "bralvar";
    if (request.sourceEventId != eventId || normalizeMapFileStem(map->map.fileName) != stem
        || m_pMapSceneRuntime->kind() != (step ? SceneKind::Indoor : SceneKind::Outdoor)) return false;
    const auto &run = m_gameSession.breachMazeRunData();
    if (run.has_value() != request.expectedRunPresent) return false;
    if (!run) return !step;
    const auto floor = run->floors.find(run->currentLevel);
    const uint64_t serial = floor == run->floors.end() ? 0 : floor->second.descriptor.identity.generationSerial;
    return run->inside == step && run->inside == request.expectedInside && run->epoch == request.expectedEpoch
        && run->revision == request.expectedRevision && run->currentLevel == request.expectedLevel
        && serial == request.expectedGenerationSerial;
}

void GameApplication::processPendingBreachMazeTransitionRequest()
{
    EventRuntimeState *pEvents = m_gameplayController.eventRuntimeState();
    if (!pEvents || !pEvents->pendingBreachMazeTransitionRequest) return;
    const MazeRequest request = *pEvents->pendingBreachMazeTransitionRequest;
    pEvents->pendingBreachMazeTransitionRequest.reset();
    if (!breachMazeRequestMatches(request) || m_screenManager.activeScreen()
        || m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive) return;
    const BreachMazeQuestView quest = mazeQuestView(m_gameSession, pEvents);
    if (request.kind == MazeRequest::Kind::Down && quest.present && !quest.questFinished)
    {
        reportQuickSaveStatus("门锁着。");
        return;
    }
    m_pendingBreachMazeTransition = request;
    m_pendingBreachMazeQuest = quest;
    EventRuntimeState::PendingMapMove move;
    move.mapName = "brbase.blv";
    move.traceSourceKind = "breach_maze";
    move.traceEventId = request.sourceEventId;
    move.traceDestinationName = "brbase.blv";
    EventRuntimeState::PendingDialogueContext dialog;
    dialog.kind = DialogueContextKind::MapTransition;
    dialog.sourceId = request.sourceEventId;
    dialog.transitionMapMove = std::move(move);
    dialog.transitionTextId = request.kind == MazeRequest::Kind::BrAlvar53 ? 710
        : request.kind == MazeRequest::Kind::BrAlvar54 || request.kind == MazeRequest::Kind::Breach54 ? 711 : 0;
    dialog.transitionImageId = request.kind == MazeRequest::Kind::BrAlvar53 ? 3
        : request.kind == MazeRequest::Kind::BrAlvar54 || request.kind == MazeRequest::Kind::Breach54 ? 2 : 1;
    const size_t messages = pEvents->messages.size();
    pEvents->pendingDialogueContext = std::move(dialog);
    m_gameSession.gameplayScreenRuntime().presentPendingEventDialog(messages, true);
}

bool GameApplication::commitBreachMazeTransition(const EventRuntimeState::PendingMapMove &move, bool initializeView)
{
    if (!m_pendingBreachMazeTransition) return false;
    const MazeRequest request = *m_pendingBreachMazeTransition;
    m_pendingBreachMazeTransition.reset();
    EventRuntimeState *pEvents = m_gameplayController.eventRuntimeState();
    if (move.traceEventId != request.sourceEventId || !breachMazeRequestMatches(request)
        || mazeQuestView(m_gameSession, pEvents) != m_pendingBreachMazeQuest) return false;
    const BreachMazeQuestView quest = m_pendingBreachMazeQuest;
    synchronizeSessionFromRuntime();
    const auto backup = m_gameSession.buildSaveData();
    if (!backup) return false;
    try
    {
        const auto &profile = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem);
        const BreachMazeLifecycle lifecycle(profile.mazeTemplate);
        BreachMazeState state = m_gameSession.breachMazeRunData()
            ? lifecycle.restore(*m_gameSession.breachMazeRunData()) : BreachMazeState::playable();
        const bool step = request.kind == MazeRequest::Kind::Up || request.kind == MazeRequest::Kind::Down;
        BreachMazePreparedTransition transition = step
            ? lifecycle.prepareStep(state, request.kind == MazeRequest::Kind::Up ? BreachMazeStep::Up
                : BreachMazeStep::Down, quest, state.activeFloor()->runtime)
            : lifecycle.prepareFreshEntry(state, request.kind == MazeRequest::Kind::BrAlvar53
                ? BreachMazeEntry::BrAlvar53 : request.kind == MazeRequest::Kind::BrAlvar54
                ? BreachMazeEntry::BrAlvar54 : BreachMazeEntry::Breach54, quest, SDL_GetTicksNS());
        if (transition.kind() == BreachMazePlanKind::DownLocked) return false;
        const BreachMazeQuestEffects effects = transition.questEffects();
        const BreachMazeArrival arrival = transition.arrival();
        const auto departure = transition.departure();
        closeTransientGameplayUiForMapMove();
        beginLoadingOverlay(LoadingOverlayScreen::Presentation::DungeonTransition);
        renderLoadingOverlayProgress(15);
        if (transition.kind() != BreachMazePlanKind::LeaveMaze)
        {
            const BreachMazeFloorDescriptor target = *transition.target();
            const bool retained = transition.kind() == BreachMazePlanKind::RetainedFloor;
            std::optional<BreachMazeFloorState> saved;
            if (retained) saved = *transition.retainedFloor();
            shutdownRenderer();
            if (!activateWorldForMapFileName("brbase.blv")
                || !m_gameDataLoader.loadMapByFileNameForHeadlessGameplay(*m_pAssetFileSystem, "brbase.blv"))
                throw std::runtime_error("Cannot load maze base");
            const auto &base = m_gameDataLoader.getSelectedMap();
            if (!base || !base->indoorMapData || !base->indoorMapDeltaData)
                throw std::runtime_error("Incomplete maze base");
            // World switches can reload loader-owned profiles, so reacquire after activation.
            const auto &currentProfile = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem);
            std::vector<BreachMazeSkillView> perception;
            for (const Character &member : ensureSessionPartyState().members())
            {
                const CharacterSkill *pSkill = member.findSkillByCanonicalName("Perception");
                perception.push_back(pSkill ? BreachMazeSkillView{pSkill->level, uint8_t(pSkill->mastery)}
                    : BreachMazeSkillView{});
            }
            BreachMazePopulationPlan population = retained ? *saved->population
                : planBreachMazePopulation(currentProfile.catalog, currentProfile.mazeTemplate, target, perception);
            PreparedIndoorMapInstance instance;
            if (retained)
            {
                instance = restoreBreachMazeInstance({*base->indoorMapData, *base->indoorMapDeltaData,
                    currentProfile.mazeTemplate, target, population, currentProfile.catalog, saved->runtime.scene,
                    m_gameDataLoader.getOrCreateMergeMonsterSourceProfile(*m_pAssetFileSystem)});
            }
            else
            {
                const auto *pMap = m_gameDataLoader.getMergedBolsterMapTable().findById(206);
                if (!pMap) throw std::runtime_error("Missing maze bolster map rules");
                MergeMonsterFamilyContext context;
                context.map = {206, MergeBolsterMapKind::LowerToEqual, pMap->spells, pMap->summons,
                    int32_t(pMap->bolsterExtra)};
                context.effectivePartyLevel = gameplayBolsterAveragePartyLevel(&ensureSessionPartyState())
                    + context.map.levelShift;
                context.enabled = m_settings.bolsterMonsters;
                instance = buildBreachMazeInstance({*base->indoorMapData, *base->indoorMapDeltaData,
                    currentProfile.mazeTemplate, target, population, currentProfile.catalog,
                    m_gameDataLoader.getMonsterTable(), m_gameDataLoader.getMonsterProjectileTable(),
                    m_gameDataLoader.getSpellTable(), m_gameDataLoader.getObjectTable(),
                    m_gameDataLoader.getItemTable(),
                    m_gameDataLoader.getStandardItemEnchantTable(), m_gameDataLoader.getSpecialItemEnchantTable(),
                    m_gameDataLoader.getMergedBolsterMonsterTable(), m_gameDataLoader.getMergedBolsterFormulaTable(),
                    context}).instance;
            }
            if (!m_gameDataLoader.loadPreparedIndoorMapForGameplay(*m_pAssetFileSystem, instance))
                throw std::runtime_error("Cannot prepare maze resources");
            m_gameSession.setBreachMazeRunData(std::nullopt);
            m_gameSession.setCurrentMapFileName("brbase.blv");
            if (!initializeSelectedMapRuntime(initializeView)) throw std::runtime_error("Cannot initialize maze");
            auto &scene = *static_cast<IndoorSceneRuntime *>(m_pMapSceneRuntime.get());
            if (retained)
            {
                saved->runtime.scene.worldRuntime.gameMinutes = m_gameSession.gameMinutes();
                if (saved->runtime.scene.eventRuntimeState)
                {
                    saved->runtime.scene.eventRuntimeState->namedGlobalVars = m_gameSession.namedGlobalVars();
                    m_gameSession.applyHistoryToRuntime(*saved->runtime.scene.eventRuntimeState);
                }
                saved->runtime.scene.worldRuntime.bloodSplats.clear();
                scene.restoreSnapshot(saved->runtime.scene);
                const bool canSeeTraps = std::any_of(perception.begin(), perception.end(),
                    [&](const BreachMazeSkillView &view)
                    {
                        return view.mastery == 4 || (view.mastery > 0
                            && view.skill > (target.identity.level + view.mastery - 1) / view.mastery);
                    });
                MapDeltaData *pDelta = scene.worldRuntime().mapDeltaData();
                for (size_t cell = 0; cell < BreachMazeTemplateCellCount; ++cell)
                {
                    if (!population.traps[cell].placed) continue;
                    const uint32_t face = currentProfile.mazeTemplate.cells[cell].centralFloorFaceIndex;
                    const bool active = saved->runtime.interaction->traps[cell].spellId != 0;
                    if (canSeeTraps && active)
                        pDelta->faceAttributes[face] |= faceAttributeBit(FaceAttribute::IsSecret);
                    else pDelta->faceAttributes[face] &= ~faceAttributeBit(FaceAttribute::IsSecret);
                }
            }
            stageMazeArrival(scene, arrival);
            m_indoorRenderer.setCameraAngles(arrival.yawRadians, arrival.pitchRadians);
            BreachMazeReadyFloor ready{target.identity, {}, population};
            ready.runtime.scene = scene.snapshot();
            ready.runtime.yawRadians = arrival.yawRadians;
            ready.runtime.pitchRadians = arrival.pitchRadians;
            ready.runtime.actorRage.resize(ready.runtime.scene.worldRuntime.mapActorAiStates.size(), 0);
            ready.runtime.interaction = retained ? saved->runtime.interaction
                : std::optional(makeBreachMazeInteractionState(population));
            if (lifecycle.commit(state, transition, true,
                    mazeQuestView(m_gameSession, m_gameplayController.eventRuntimeState()), retained ? nullptr : &ready)
                != BreachMazeCommitResult::Committed) throw std::runtime_error("Maze transition became stale");
            if (retained) lifecycle.captureCurrentFloor(state, ready.runtime);
            m_gameSession.setBreachMazeRunData(state.data());
            if (!bindBreachMazeInteractions()) throw std::runtime_error("Cannot bind maze interactions");
        }
        else
        {
            if (!departure || lifecycle.commit(state, transition, true, mazeQuestView(m_gameSession, pEvents))
                != BreachMazeCommitResult::Committed)
                throw std::runtime_error("Maze departure became stale");
            m_gameSession.setBreachMazeRunData(state.data());
            m_gameSession.setCurrentMapFileName(std::string(departure->mapFileName));
            if (!loadCurrentSessionMap(initializeView)) throw std::runtime_error("Cannot load maze departure");
            if (!m_pOutdoorPartyRuntime) throw std::runtime_error("Maze departure is not outdoors");
            m_pOutdoorPartyRuntime->teleportTo(float(departure->x), float(departure->y), float(departure->z));
            m_outdoorGameView.setCameraAngles(float(departure->yawUnits) * 6.28318530718f / 2048.0f, 0);
        }
        pEvents = m_gameplayController.eventRuntimeState();
        const auto applyEffect = [&](const char *pName, bool enabled)
        {
            if (!enabled) return;
            const std::string key = std::string("MMerge.CrossContinents.") + pName;
            m_gameSession.setNamedGlobalVar(key, 1);
            if (pEvents) pEvents->namedGlobalVars[key] = 1;
        };
        applyEffect("EnteredBasement", effects.setEnteredBasement);
        applyEffect("EnteredBreach", effects.setEnteredBreach);
        synchronizeSessionFromRuntime();
        completeLoadingOverlay();
        return true;
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Breach maze transition failed: " << failure.what() << '\n';
        shutdownRenderer();
        m_gameSession.restoreFromSaveData(*backup);
        const bool previousLoading = m_loadingSavedGameRuntime;
        m_loadingSavedGameRuntime = true;
        bool restored = loadCurrentSessionMap(initializeView) && applyCurrentSessionToRuntime();
        if (restored) restored = m_gameSession.restoreTurnBasedCombatFromSaveData(*backup);
        if (restored) m_gameSession.restoreHeldInventoryItemFromSaveData(*backup);
        m_loadingSavedGameRuntime = previousLoading;
        if (!restored) std::cerr << "Breach maze rollback could not restore scene\n";
        cancelLoadingOverlay();
        reportQuickSaveStatus("无法通过通道，请重试。");
        return false;
    }
}

void GameApplication::captureBreachMazeRuntime(
    const IndoorSceneRuntime::Snapshot &snapshot, float yaw, float pitch)
{
    const auto &run = m_gameSession.breachMazeRunData();
    if (!run || !run->inside || normalizeMapFileStem(m_gameSession.currentMapFileName()) != "brbase") return;
    const auto &profile = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem);
    const BreachMazeLifecycle lifecycle(profile.mazeTemplate);
    BreachMazeState state = lifecycle.restore(*run);
    BreachMazeFloorRuntime runtime = state.activeFloor()->runtime;
    runtime.scene = snapshot;
    runtime.yawRadians = yaw;
    runtime.pitchRadians = pitch;
    const auto &scene = *static_cast<IndoorSceneRuntime *>(m_pMapSceneRuntime.get());
    const std::span<const uint8_t> rage = scene.worldRuntime().breachMazeActorRage();
    runtime.actorRage.assign(rage.begin(), rage.end());
    // Native Special may append actors during this frame; newly appended actors start without rage.
    runtime.actorRage.resize(snapshot.worldRuntime.mapActorAiStates.size(), 0);
    lifecycle.captureCurrentFloor(state, runtime);
    m_gameSession.setBreachMazeRunData(state.data());
}

bool GameApplication::commitBreachMazePortalDeparture(
    const EventRuntimeState::PendingMapMove &move, bool initializeView)
{
    const auto &run = m_gameSession.breachMazeRunData();
    const auto &map = m_gameDataLoader.getSelectedMap();
    if (!run || !run->inside || !map || map->map.worldId != "mmmerge"
        || normalizeMapFileStem(map->map.fileName) != "brbase" || !m_pMapSceneRuntime
        || m_pMapSceneRuntime->kind() != SceneKind::Indoor || !move.mapName || move.mapName->empty()
        || *move.mapName == "0" || *move.mapName == "0." || normalizeMapFileStem(*move.mapName) == "brbase"
        || !m_gameSession.activeWorldRuntime() || !m_gameSession.activeWorldRuntime()->townPortalUsesDimensionDoor())
        return false;
    synchronizeSessionFromRuntime();
    const auto backup = m_gameSession.buildSaveData();
    if (!backup) return false;
    try
    {
        const auto &profile = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem);
        const BreachMazeLifecycle lifecycle(profile.mazeTemplate);
        BreachMazeState state = lifecycle.restore(*m_gameSession.breachMazeRunData());
        const BreachMazeQuestView quest = mazeQuestView(m_gameSession, m_gameplayController.eventRuntimeState());
        auto departure = lifecycle.preparePortalDeparture(state, quest, state.activeFloor()->runtime);
        if (departure.kind() != BreachMazePlanKind::PortalDeparture
            || lifecycle.commit(state, departure, true,
                mazeQuestView(m_gameSession, m_gameplayController.eventRuntimeState()))
                != BreachMazeCommitResult::Committed)
            throw std::runtime_error("Maze portal departure became stale");
        // Stage the candidate while loading. The existing shared travel path owns map resources,
        // spell placement, camera, on-leave outputs and arrival. A failure restores the full source save.
        m_gameSession.setBreachMazeRunData(state.data());
        if (!applyMapMove(move, initializeView)) throw std::runtime_error("Cannot load maze portal destination");
        return true;
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Breach maze portal failed: " << failure.what() << '\n';
        shutdownRenderer();
        m_gameSession.restoreFromSaveData(*backup);
        const bool previousLoading = m_loadingSavedGameRuntime;
        m_loadingSavedGameRuntime = true;
        bool restored = loadCurrentSessionMap(initializeView) && applyCurrentSessionToRuntime();
        if (restored) restored = m_gameSession.restoreTurnBasedCombatFromSaveData(*backup);
        if (restored) m_gameSession.restoreHeldInventoryItemFromSaveData(*backup);
        m_loadingSavedGameRuntime = previousLoading;
        if (!restored) std::cerr << "Breach maze portal rollback could not restore scene\n";
        cancelLoadingOverlay();
        reportQuickSaveStatus("无法通过传送门，请重试。");
        return false;
    }
}

bool GameApplication::restoreBreachMazeMapAssets()
{
    try
    {
        const auto &run = m_gameSession.breachMazeRunData();
        if (!run || !run->inside) throw std::invalid_argument("No active maze floor to restore");
        const auto &profile = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem);
        const BreachMazeLifecycle lifecycle(profile.mazeTemplate);
        const BreachMazeState state = lifecycle.restore(*run);
        const BreachMazeFloorState &floor = lifecycle.coldRestore(state);
        if (!m_gameDataLoader.loadMapByFileNameForHeadlessGameplay(*m_pAssetFileSystem, "brbase.blv"))
            throw std::invalid_argument("Cannot read original maze geometry");
        const auto &base = m_gameDataLoader.getSelectedMap();
        if (!base || base->map.worldId != "mmmerge" || !base->indoorMapData || !base->indoorMapDeltaData)
            throw std::invalid_argument("Maze base is not a complete indoor map");
        const auto &source = m_gameDataLoader.getOrCreateMergeMonsterSourceProfile(*m_pAssetFileSystem);
        const BreachMazeInstanceRestoreInput input{*base->indoorMapData, *base->indoorMapDeltaData,
            profile.mazeTemplate, floor.descriptor, *floor.population, profile.catalog, floor.runtime.scene, source};
        const PreparedIndoorMapInstance prepared = restoreBreachMazeInstance(input);
        return m_gameDataLoader.loadPreparedIndoorMapForGameplay(*m_pAssetFileSystem, prepared,
            [this]() { pumpLoadingOverlayAnimation(); });
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Cannot restore Breach maze: " << failure.what() << '\n';
        return false;
    }
}

void GameApplication::projectBreachMazeInteractions(IndoorSceneRuntime &scene, const BreachMazeFloorState &floor)
{
    if (!floor.population || !floor.runtime.interaction) throw std::invalid_argument("Missing maze interaction state");
    EventRuntimeState *pEvents = scene.eventRuntimeState();
    MapDeltaData *pDelta = scene.worldRuntime().mapDeltaData();
    const auto &map = m_gameDataLoader.getSelectedMap();
    if (!pEvents || !pDelta || !map || !map->indoorMapData)
        throw std::invalid_argument("Missing maze interaction geometry");
    const auto &maze = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem).mazeTemplate;
    const BreachMazeInteractionState &interaction = *floor.runtime.interaction;
    for (size_t slot = 0; slot < BreachMazeTemplateCellCount; ++slot)
    {
        if (interaction.traps[slot].spellId == 0)
        {
            const uint32_t face = maze.cells[slot].centralFloorFaceIndex;
            if (face >= pDelta->faceAttributes.size()) throw std::invalid_argument("Missing maze trap face");
            // Source clears Event and IsSecret. Disabling both live activation paths projects Event=0
            // without modifying the immutable source BLV or rebuilding geometry during movement.
            pDelta->faceAttributes[face] &= ~(faceAttributeBit(FaceAttribute::IsSecret)
                | faceAttributeBit(FaceAttribute::PressurePlate) | faceAttributeBit(FaceAttribute::Clickable));
        }
        const auto handler = floor.population->decorations[slot].interaction.handler;
        const bool hidden = interaction.decorations[slot].used
            && (handler == BreachMazeDecorationHandler::Crystal || handler == BreachMazeDecorationHandler::MiscBag
                || handler == BreachMazeDecorationHandler::FoodSack);
        if (hidden)
        {
            if (slot >= map->indoorMapData->entities.size() || slot >= pDelta->decorationFlags.size())
                throw std::invalid_argument("Missing maze decoration");
            pEvents->spriteOverrides[map->indoorMapData->entities[slot].spriteOverrideKey(slot)].hidden = true;
            pDelta->decorationFlags[slot] |= 0x20; // Original LevelDecorationInvisible.
        }
    }
    scene.worldRuntime().applyEventRuntimeState();
}

bool GameApplication::bindBreachMazeInteractions()
{
    if (!m_pMapSceneRuntime || m_pMapSceneRuntime->kind() != SceneKind::Indoor) return true;
    auto &scene = *static_cast<IndoorSceneRuntime *>(m_pMapSceneRuntime.get());
    const auto &map = m_gameDataLoader.getSelectedMap();
    if (!map || map->map.worldId != "mmmerge" || normalizeMapFileStem(map->map.fileName) != "brbase"
        || map->authoredCompanionSource != AuthoredCompanionSource::PreparedInstance)
    {
        scene.worldRuntime().setNativeEventHandler({});
        scene.worldRuntime().clearBreachMazeCombatProfile();
        return true;
    }
    try
    {
        if (!map->localEventProgram || !map->localEventProgram->getLocalString(4)
            || !map->localEventProgram->getLocalString(5) || !map->localEventProgram->getLocalString(6)
            || !map->localEventProgram->getImportedGlobalText(521))
            throw std::invalid_argument("Maze native messages were not imported");
        scene.worldRuntime().setNativeEventHandler(
            [this](uint16_t eventId, const std::string &sourceKind, size_t sourceIndex)
            {
                return activateBreachMazeNativeEvent(eventId, sourceKind, sourceIndex);
            });
        const auto &run = m_gameSession.breachMazeRunData();
        if (run && run->inside)
        {
            const BreachMazeFloorState &floor = run->floors.at(run->currentLevel);
            projectBreachMazeInteractions(scene, floor);
            std::string error;
            if (!scene.worldRuntime().installBreachMazeCombatProfile(
                    floor.descriptor.layout, floor.runtime.actorRage, error))
                throw std::invalid_argument(error);
        }
        return true;
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Cannot bind maze interactions: " << failure.what() << '\n';
        return false;
    }
}

std::optional<bool> GameApplication::activateBreachMazeNativeEvent(
    uint16_t eventId, const std::string &sourceKind, size_t sourceIndex)
{
    if (eventId != 3 && eventId != 4
        && (eventId < 20000 || eventId >= 20000 + BreachMazeTemplateCellCount)) return std::nullopt;
    const auto &run = m_gameSession.breachMazeRunData();
    const auto &map = m_gameDataLoader.getSelectedMap();
    if (!run || !run->inside || !m_pMapSceneRuntime || m_pMapSceneRuntime->kind() != SceneKind::Indoor
        || !map || map->map.worldId != "mmmerge" || normalizeMapFileStem(map->map.fileName) != "brbase")
        return false;
    auto &scene = *static_cast<IndoorSceneRuntime *>(m_pMapSceneRuntime.get());
    EventRuntimeState *pEvents = scene.eventRuntimeState();
    if (!pEvents) return false;
    if (run->revision == std::numeric_limits<uint64_t>::max()) return false;
    BreachMazeRunData candidate = *run;
    BreachMazeFloorState &floor = candidate.floors.at(candidate.currentLevel);
    if (!floor.population || !floor.runtime.interaction) return false;
    const auto &maze = m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*m_pAssetFileSystem).mazeTemplate;
    Party &party = scene.party();
    const std::optional<size_t> current = party.activeMemberIndex() < party.memberCount()
        ? std::optional(party.activeMemberIndex()) : std::nullopt;
    const BreachMazeInteractionResult result = activateBreachMazeInteraction(*floor.runtime.interaction,
        *floor.population, maze, {eventId, current}, {party, *pEvents, scene.worldRuntime(),
            scene.partyRuntime().partyX(), scene.partyRuntime().partyY(), scene.partyRuntime().partyFootZ(),
            scene.worldRuntime().currentGameMinutesPrecise()});
    if (!result.handled) return std::nullopt;
    if (!result.error.empty())
    {
        std::cerr << "Maze interaction " << eventId << " failed: " << result.error << '\n';
        pEvents->statusMessages.push_back("无法使用这个物件。");
        return false;
    }
    projectBreachMazeInteractions(scene, floor);
    if (result.message)
    {
        const BreachMazeInteractionMessage &message = *result.message;
        std::optional<std::string> text;
        switch (message.source)
        {
            case BreachMazeInteractionTextSource::LocalString:
                text = map->localEventProgram->getLocalString(message.id);
                break;
            case BreachMazeInteractionTextSource::GlobalText:
                text = map->localEventProgram->getImportedGlobalText(message.id);
                break;
            case BreachMazeInteractionTextSource::NpcText:
                text = m_gameSession.data().npcDialogTable().getText(message.id);
                break;
        }
        if (text && !message.formatArgument.empty())
        {
            const size_t marker = text->find(" +3");
            if (marker != std::string::npos) text->replace(marker, 3, message.formatArgument);
        }
        if (text) pEvents->statusMessages.push_back(*text);
    }
    if (result.faceAnimation)
    {
        const auto &animation = *result.faceAnimation;
        m_gameSession.gameplayScreenRuntime().triggerPortraitFaceAnimation(
            animation.memberIndex, static_cast<FaceAnimationId>(animation.legacyAnimationId));
        // The shared interaction already enqueues speech for raw4/5/53.
        if (animation.legacyAnimationId == 36) party.requestSpeech(animation.memberIndex, SpeechId::PotionSuccess);
    }
    pEvents->lastActivationResult = "native maze event " + std::to_string(eventId)
        + " from " + sourceKind + ":" + std::to_string(sourceIndex);
    if (result.changed) ++candidate.revision;
    m_gameSession.setBreachMazeRunData(std::move(candidate));
    // Do not restore a scene from inside a movement callback. The normal capture path saves
    // the current scene and the mutable floor record together at its next safe point.
    return true;
}
}
