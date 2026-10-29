#include "game/app/GameSession.h"

#include "game/debug/GameplayDebugTrace.h"
#include "game/events/GameTime.h"
#include "game/maps/MapIdentity.h"
#include "game/StringUtils.h"
#include "game/gameplay/GameplayActionController.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayInteractionController.h"
#include "game/gameplay/GameplayScreenController.h"
#include "game/ui/GameplaySpellTargetingOverlayRenderer.h"

#include <cassert>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
bool isBreachMazeMap(const std::string &map)
{
    const std::string lower = toLowerCopy(map);
    return lower == "brbase.blv" || lower == "brbase"
        || lower == buildCanonicalMapId("mmmerge", "brbase.blv");
}

constexpr uint32_t BlazenStormlanceRosterUnlockQBit = 435;
constexpr uint32_t RescuedBlazenStormlanceQBit = 1542;

uint64_t averageNanoseconds(uint64_t totalNanoseconds, uint64_t count)
{
    return count != 0 ? totalNanoseconds / count : 0;
}

uint64_t nanosecondsToMicroseconds(uint64_t nanoseconds)
{
    return nanoseconds / 1000ULL;
}

const char *turnBasedStageName(TurnBasedCombatStage stage)
{
    switch (stage)
    {
        case TurnBasedCombatStage::None:
            return "none";
        case TurnBasedCombatStage::Wait:
            return "wait";
        case TurnBasedCombatStage::Attack:
            return "attack";
        case TurnBasedCombatStage::Movement:
            return "movement";
        default:
            return "unknown";
    }
}

void synchronizeTurnBasedPendingWorldActions(
    TurnBasedCombatRuntime &turnBasedCombatRuntime,
    IGameplayWorldRuntime *pWorldRuntime,
    size_t &syncedPendingWorldActions)
{
    if (!turnBasedCombatRuntime.active() || pWorldRuntime == nullptr)
    {
        syncedPendingWorldActions = 0;
        return;
    }

    const size_t pendingWorldActions = pWorldRuntime->turnBasedPendingWorldActionCount();

    while (syncedPendingWorldActions < pendingWorldActions)
    {
        turnBasedCombatRuntime.registerPendingAction();
        ++syncedPendingWorldActions;
    }

    while (syncedPendingWorldActions > pendingWorldActions)
    {
        turnBasedCombatRuntime.resolvePendingAction();
        --syncedPendingWorldActions;
    }
}

void pulseGameplayAction(GameplayInputFrame &input, KeyboardAction action)
{
    GameplayButtonInputState &state = input.actions[keyboardActionIndex(action)];
    state.held = true;
    state.pressed = true;
    state.released = false;
}

Party buildConfiguredParty(
    const Party::Snapshot &snapshot,
    const GameDataRepository &data)
{
    Party party = {};
    party.setItemTable(&data.itemTable());
    party.setJournalQuestTable(&data.journalQuestTable());
    party.setItemEnchantTables(&data.standardItemEnchantTable(), &data.specialItemEnchantTable());
    party.setClassMultiplierTable(&data.classMultiplierTable());
    party.setClassSkillTable(&data.classSkillTable());
    party.restoreSnapshot(snapshot);
    return party;
}

void migrateSavedRuntimeFollowersToParty(
    Party &party,
    const std::optional<EventRuntimeState> &eventRuntimeState)
{
    if (!eventRuntimeState)
    {
        return;
    }

    for (const HiredNpcFollower &savedFollower : eventRuntimeState->hiredNpcFollowers)
    {
        HiredNpcFollower follower = savedFollower;

        const std::unordered_map<uint32_t, std::string>::const_iterator nameIt =
            eventRuntimeState->npcNameOverrides.find(follower.npcId);
        if (follower.name.empty() && nameIt != eventRuntimeState->npcNameOverrides.end())
        {
            follower.name = nameIt->second;
        }

        const std::unordered_map<uint32_t, uint32_t>::const_iterator pictureIt =
            eventRuntimeState->npcPictureOverrides.find(follower.npcId);
        if (follower.pictureId == 0 && pictureIt != eventRuntimeState->npcPictureOverrides.end())
        {
            follower.pictureId = pictureIt->second;
        }

        party.addHiredNpcFollower(follower);
    }
}

void migrateBlazenRescueRosterUnlock(Party &party)
{
    if (!party.hasQuestBit(RescuedBlazenStormlanceQBit)
        || party.hasQuestBit(BlazenStormlanceRosterUnlockQBit))
    {
        return;
    }

    party.setQuestBit(BlazenStormlanceRosterUnlockQBit, true);
    GAMEPLAY_DEBUG_TRACE(
        "save_migration name=blazen_rescue_roster_unlock"
        " source_qbit=" + std::to_string(RescuedBlazenStormlanceQBit)
        + " restored_qbit=" + std::to_string(BlazenStormlanceRosterUnlockQBit));
}

void synchronizeGameplayActiveMemberToReadyMember(
    GameplayScreenRuntime &screenRuntime,
    const GameplayScreenState &screenState,
    const TurnBasedCombatRuntime &turnBasedCombatRuntime)
{
    if (screenRuntime.currentHudScreenState() != GameplayHudScreenState::Gameplay)
    {
        return;
    }

    if (turnBasedCombatRuntime.active())
    {
        return;
    }

    if (screenState.pendingSpellTarget().active || screenRuntime.heldInventoryItem().active)
    {
        return;
    }

    Party *pParty = screenRuntime.party();

    if (pParty == nullptr)
    {
        return;
    }

    const Character *pActiveMember = pParty->activeMember();

    if (pActiveMember == nullptr || !GameMechanics::canTakeGameplayAction(*pActiveMember))
    {
        pParty->switchToNextReadyMember();
    }
}
}

GameSession::GameSession()
    : m_gameplayItemService(*this)
    , m_gameplayFxService(*this)
    , m_gameplaySpellService(*this)
    , m_gameplayScreenRuntime(*this)
{
    m_gameplayUiController.bindExternalState(&m_gameplayScreenState.uiState());
}

void GameSession::bindDataRepository(const GameDataRepository *pDataRepository)
{
    m_pDataRepository = pDataRepository;
    m_mm9QuestMarkerCache.clear();
    ++m_mm9QuestMarkerMapRevision;
    m_gameplayUiRuntime.bindDataRepository(pDataRepository);
    GameMechanics::bindClassMultiplierTable(
        pDataRepository != nullptr && pDataRepository->isBound() ? &pDataRepository->classMultiplierTable() : nullptr);

    if (m_pDataRepository != nullptr && m_pDataRepository->isBound())
    {
        m_gameplayActorService.bindTables(&m_pDataRepository->monsterTable(), &m_pDataRepository->spellTable());
    }
    else
    {
        m_gameplayActorService.bindTables(nullptr, nullptr);
    }
}

bool GameSession::hasDataRepository() const
{
    return m_pDataRepository != nullptr;
}

const GameDataRepository &GameSession::data() const
{
    assert(m_pDataRepository != nullptr);
    return *m_pDataRepository;
}

void GameSession::clear()
{
    if (m_pActiveWorldRuntime != nullptr && m_pActiveWorldRuntime->boundGameSession() == this)
    {
        m_pActiveWorldRuntime->bindGameSession(nullptr);
    }
    m_partyState.reset();
    m_currentSceneKind = SceneKind::Outdoor;
    m_currentMapFileName.clear();
    m_gameplayScreenState.clear();
    m_gameplayCombatController.clear();
    m_gameplayProjectileService.clear();
    m_gameplayFxService.clear();
    m_turnBasedCombatRuntime.reset();
    m_turnBasedPendingWorldActions = 0;
    m_turnBasedFrameTraceState = {};
    m_gameplayUiRuntime.clear();
    m_gameplayScreenRuntime.clearTransientBindings();
    m_gameplayScreenRuntime.resetTownPortalContact();
    m_gameplayScreenRuntime.resetDimensionDoor();
    m_overlayInteractionState = {};
    m_previousKeyboardState.fill(0);
    m_pActiveWorldRuntime = nullptr;
    m_mm9QuestMarkerCache.clear();
    ++m_mm9QuestMarkerMapRevision;
    m_pCurrentGameplayInputFrame = nullptr;
    m_sharedInputFrameResult = {};
    m_sharedWorldInteractionBlockedThisFrame = false;
    m_relativeMouseMotionResetRequested = false;
    m_outdoorPartyState.reset();
    m_currentOutdoorWorldState.reset();
    m_outdoorWorldStates.clear();
    m_currentIndoorSceneState.reset();
    m_indoorSceneStates.clear();
    m_namedGlobalVars.clear();
    m_sessionSavedLocations.clear();
    m_breachAlvarCompanions.reset();
    m_breachMazeRunData.reset();
    m_historyEventTimesByContinent.clear();
    m_gameMinutes = 9.0f * 60.0f;
    m_outdoorCameraYawRadians = 0.0f;
    m_outdoorCameraPitchRadians = 0.0f;
    m_indoorCameraYawRadians = 0.0f;
    m_indoorCameraPitchRadians = 0.15f;
    m_currentSavePath.reset();
    m_pendingMapMove.reset();
    m_framePerformanceDiagnosticsEnabled = false;
    m_lastGameplayUpdateFramePerformanceDiagnostics = {};
    m_lastGameplayUiFramePerformanceDiagnostics = {};
}

const std::optional<Party> &GameSession::partyState() const
{
    return m_partyState;
}

std::optional<Party> &GameSession::partyState()
{
    return m_partyState;
}

void GameSession::setPartyState(const Party &party)
{
    m_partyState = party;
}

void GameSession::setPartyState(Party &&party)
{
    m_partyState = std::move(party);
}

SceneKind GameSession::currentSceneKind() const
{
    return m_currentSceneKind;
}

void GameSession::setCurrentSceneKind(SceneKind sceneKind)
{
    m_currentSceneKind = sceneKind;
}

bool GameSession::hasCurrentMapFileName() const
{
    return !m_currentMapFileName.empty();
}

const std::string &GameSession::currentMapFileName() const
{
    return m_currentMapFileName;
}

void GameSession::setCurrentMapFileName(const std::string &mapFileName)
{
    if (m_currentMapFileName != mapFileName)
    {
        m_gameplayScreenRuntime.resetDimensionDoor();
    }
    m_currentMapFileName = mapFileName;
    m_mm9QuestMarkerCache.clear();
    ++m_mm9QuestMarkerMapRevision;
}

void GameSession::setCurrentMapFileName(std::string &&mapFileName)
{
    if (m_currentMapFileName != mapFileName)
    {
        m_gameplayScreenRuntime.resetDimensionDoor();
    }
    m_currentMapFileName = std::move(mapFileName);
    m_mm9QuestMarkerCache.clear();
    ++m_mm9QuestMarkerMapRevision;
}

double GameSession::gameMinutes() const
{
    return m_gameMinutes;
}

void GameSession::setGameMinutes(double gameMinutes)
{
    if (!validGameMinutes(gameMinutes))
    {
        throw std::invalid_argument("Invalid authoritative game clock");
    }
    m_gameMinutes = gameMinutes;
}

GameplayUiController &GameSession::gameplayUiController()
{
    return m_gameplayUiController;
}

const GameplayUiController &GameSession::gameplayUiController() const
{
    return m_gameplayUiController;
}

GameplayScreenState &GameSession::gameplayScreenState()
{
    return m_gameplayScreenState;
}

const GameplayScreenState &GameSession::gameplayScreenState() const
{
    return m_gameplayScreenState;
}

GameplayUiRuntime &GameSession::gameplayUiRuntime()
{
    return m_gameplayUiRuntime;
}

const GameplayUiRuntime &GameSession::gameplayUiRuntime() const
{
    return m_gameplayUiRuntime;
}

GameplayActorService &GameSession::gameplayActorService()
{
    return m_gameplayActorService;
}

const GameplayActorService &GameSession::gameplayActorService() const
{
    return m_gameplayActorService;
}

GameplayCombatController &GameSession::gameplayCombatController()
{
    return m_gameplayCombatController;
}

const GameplayCombatController &GameSession::gameplayCombatController() const
{
    return m_gameplayCombatController;
}

GameplayItemService &GameSession::gameplayItemService()
{
    return m_gameplayItemService;
}

const GameplayItemService &GameSession::gameplayItemService() const
{
    return m_gameplayItemService;
}

GameplayProjectileService &GameSession::gameplayProjectileService()
{
    return m_gameplayProjectileService;
}

const GameplayProjectileService &GameSession::gameplayProjectileService() const
{
    return m_gameplayProjectileService;
}

GameplayFxService &GameSession::gameplayFxService()
{
    return m_gameplayFxService;
}

const GameplayFxService &GameSession::gameplayFxService() const
{
    return m_gameplayFxService;
}

GameplaySpellService &GameSession::gameplaySpellService()
{
    return m_gameplaySpellService;
}

const GameplaySpellService &GameSession::gameplaySpellService() const
{
    return m_gameplaySpellService;
}

TurnBasedCombatRuntime &GameSession::turnBasedCombatRuntime()
{
    return m_turnBasedCombatRuntime;
}

const TurnBasedCombatRuntime &GameSession::turnBasedCombatRuntime() const
{
    return m_turnBasedCombatRuntime;
}

GameplayScreenRuntime &GameSession::gameplayScreenRuntime()
{
    return m_gameplayScreenRuntime;
}

const GameplayScreenRuntime &GameSession::gameplayScreenRuntime() const
{
    return m_gameplayScreenRuntime;
}

GameplayDialogController &GameSession::gameplayDialogController()
{
    return m_gameplayDialogController;
}

const GameplayDialogController &GameSession::gameplayDialogController() const
{
    return m_gameplayDialogController;
}

GameplayOverlayInteractionState &GameSession::overlayInteractionState()
{
    return m_overlayInteractionState;
}

const GameplayOverlayInteractionState &GameSession::overlayInteractionState() const
{
    return m_overlayInteractionState;
}

std::array<uint8_t, SDL_SCANCODE_COUNT> &GameSession::previousKeyboardState()
{
    return m_previousKeyboardState;
}

const std::array<uint8_t, SDL_SCANCODE_COUNT> &GameSession::previousKeyboardState() const
{
    return m_previousKeyboardState;
}

IGameplayWorldRuntime *GameSession::activeWorldRuntime() const
{
    return m_pActiveWorldRuntime;
}

void GameSession::bindActiveWorldRuntime(IGameplayWorldRuntime *pWorldRuntime)
{
    if (m_pActiveWorldRuntime != pWorldRuntime)
    {
        m_turnBasedCombatRuntime.end(m_pActiveWorldRuntime != nullptr ? m_pActiveWorldRuntime->party() : nullptr);
        m_turnBasedFrameTraceState = {};
    }

    if (m_pActiveWorldRuntime != nullptr && m_pActiveWorldRuntime != pWorldRuntime
        && m_pActiveWorldRuntime->boundGameSession() == this)
    {
        m_pActiveWorldRuntime->bindGameSession(nullptr);
    }
    m_pActiveWorldRuntime = pWorldRuntime;
    if (pWorldRuntime != nullptr)
    {
        pWorldRuntime->bindGameSession(this);
    }
    if (pWorldRuntime != nullptr && pWorldRuntime->party() != nullptr)
    {
        const auto &heldItem = m_gameplayUiController.heldInventoryItem();
        if (heldItem.active) pWorldRuntime->party()->setHeldItemForQueries(heldItem.item);
        else pWorldRuntime->party()->clearHeldItemForQueries();
    }
    m_mm9QuestMarkerCache.clear();
    ++m_mm9QuestMarkerMapRevision;
}

Mm9QuestMarkerState GameSession::questMarkerForActor(size_t actorIndex) const
{
    return actorIndex <= static_cast<size_t>(std::numeric_limits<uint32_t>::max())
        ? m_mm9QuestMarkerCache.markerForActor(static_cast<uint32_t>(actorIndex))
        : Mm9QuestMarkerState::None;
}

const GameplayInputFrame *GameSession::currentGameplayInputFrame() const
{
    return m_pCurrentGameplayInputFrame;
}

void GameSession::bindCurrentGameplayInputFrame(const GameplayInputFrame *pInputFrame)
{
    m_pCurrentGameplayInputFrame = pInputFrame;
}

void GameSession::beginFramePerformanceDiagnostics(bool enabled)
{
    if (!enabled)
    {
        if (m_framePerformanceDiagnosticsEnabled)
        {
            m_framePerformanceDiagnosticsEnabled = false;
            m_lastGameplayUpdateFramePerformanceDiagnostics = {};
            m_lastGameplayUiFramePerformanceDiagnostics = {};
            m_gameplayUiRuntime.beginPerformanceFrame(false);
        }

        return;
    }

    m_framePerformanceDiagnosticsEnabled = true;
    m_lastGameplayUpdateFramePerformanceDiagnostics = {};
    m_lastGameplayUpdateFramePerformanceDiagnostics.collected = true;
    m_lastGameplayUiFramePerformanceDiagnostics = {};
    m_lastGameplayUiFramePerformanceDiagnostics.collected = true;
    m_gameplayUiRuntime.beginPerformanceFrame(true);
}

const GameplayUpdateFramePerformanceDiagnostics &
GameSession::lastGameplayUpdateFramePerformanceDiagnostics() const
{
    return m_lastGameplayUpdateFramePerformanceDiagnostics;
}

const GameplayUiFramePerformanceDiagnostics &GameSession::lastGameplayUiFramePerformanceDiagnostics() const
{
    return m_lastGameplayUiFramePerformanceDiagnostics;
}

bool GameSession::logGameplayUpdatePerformanceDiagnostics(uint32_t currentTick) const
{
    constexpr uint32_t LogIntervalMs = 1000;

    if (!m_gameplayUpdatePerformanceDiagnostics.hasActivity()
        || currentTick - m_lastGameplayUpdatePerformanceLogTick < LogIntervalMs)
    {
        return false;
    }

    m_lastGameplayUpdatePerformanceLogTick = currentTick;

    const GameplayUpdatePerformanceDiagnostics diagnostics = m_gameplayUpdatePerformanceDiagnostics;
    const uint64_t measuredNanoseconds =
        diagnostics.sharedFrameStateNanoseconds
        + diagnostics.worldInteractionStateNanoseconds
        + diagnostics.activeMemberSyncNanoseconds
        + diagnostics.sharedInputNanoseconds
        + diagnostics.worldMovementNanoseconds
        + diagnostics.actorAiNanoseconds
        + diagnostics.combatEventsNanoseconds
        + diagnostics.interactionFrameNanoseconds
        + diagnostics.projectileAndCooldownNanoseconds
        + diagnostics.performanceTraceLogNanoseconds;
    const uint64_t untrackedNanoseconds =
        diagnostics.totalNanoseconds > measuredNanoseconds
            ? diagnostics.totalNanoseconds - measuredNanoseconds
            : 0;

    std::cout << "[GameplayUpdatePerf]"
              << " frames=" << diagnostics.frames
              << " active_world_frames=" << diagnostics.activeWorldFrames
              << " actor_ai_frames=" << diagnostics.actorAiFrames
              << " avg_total_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.totalNanoseconds,
                  diagnostics.frames))
              << " avg_untracked_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  untrackedNanoseconds,
                  diagnostics.frames))
              << " avg_shared_frame_state_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.sharedFrameStateNanoseconds,
                  diagnostics.frames))
              << " avg_world_interaction_state_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.worldInteractionStateNanoseconds,
                  diagnostics.frames))
              << " avg_active_member_sync_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.activeMemberSyncNanoseconds,
                  diagnostics.frames))
              << " avg_shared_input_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.sharedInputNanoseconds,
                  diagnostics.frames))
              << " avg_world_movement_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.worldMovementNanoseconds,
                  diagnostics.frames))
              << " avg_actor_ai_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.actorAiNanoseconds,
                  diagnostics.frames))
              << " avg_combat_events_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.combatEventsNanoseconds,
                  diagnostics.frames))
              << " avg_interaction_frame_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.interactionFrameNanoseconds,
                  diagnostics.frames))
              << " avg_projectile_cooldown_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.projectileAndCooldownNanoseconds,
                  diagnostics.frames))
              << " avg_performance_trace_log_us=" << nanosecondsToMicroseconds(averageNanoseconds(
                  diagnostics.performanceTraceLogNanoseconds,
                  diagnostics.frames))
              << '\n';

    m_gameplayUpdatePerformanceDiagnostics = {};
    return true;
}

void GameSession::logTurnBasedFrameTraceIfNeeded(
    bool actorAiUpdate,
    bool gameplayPaused,
    bool allowMoveInput,
    bool movementStep,
    bool cursorMode,
    bool modalBlocked,
    bool pendingSpellTarget,
    bool sharedWorldBlocked,
    float movementDeltaSeconds,
    float frameDeltaSeconds)
{
    constexpr uint32_t LogIntervalMs = 1000;

    if (!m_turnBasedCombatRuntime.active())
    {
        m_turnBasedFrameTraceState = {};
        return;
    }

    const uint32_t currentTick = SDL_GetTicks();
    const TurnBasedCombatStage stage = m_turnBasedCombatRuntime.stage();
    const int movementActionPoints = m_turnBasedCombatRuntime.movementActionPoints();
    const int pendingActions = m_turnBasedCombatRuntime.pendingActions();

    const bool stateChanged =
        !m_turnBasedFrameTraceState.active
        || m_turnBasedFrameTraceState.stage != stage
        || m_turnBasedFrameTraceState.actorAiUpdate != actorAiUpdate
        || m_turnBasedFrameTraceState.gameplayPaused != gameplayPaused
        || m_turnBasedFrameTraceState.allowMoveInput != allowMoveInput
        || m_turnBasedFrameTraceState.cursorMode != cursorMode
        || m_turnBasedFrameTraceState.modalBlocked != modalBlocked
        || m_turnBasedFrameTraceState.pendingSpellTarget != pendingSpellTarget
        || m_turnBasedFrameTraceState.sharedWorldBlocked != sharedWorldBlocked
        || m_turnBasedFrameTraceState.movementActionPoints != movementActionPoints
        || m_turnBasedFrameTraceState.pendingActions != pendingActions;
    const bool periodic =
        !m_turnBasedFrameTraceState.active
        || currentTick - m_turnBasedFrameTraceState.lastLogTick >= LogIntervalMs;

    if (stateChanged || movementStep || periodic)
    {
        const char *reason = movementStep ? "movement_step" : (stateChanged ? "state" : "periodic");
        GAMEPLAY_DEBUG_TRACE(
            "turn_based_frame reason=" + std::string(reason)
            + " stage=" + std::string(turnBasedStageName(stage))
            + " actor_ai_update=" + (actorAiUpdate ? "true" : "false")
            + " gameplay_paused=" + (gameplayPaused ? "true" : "false")
            + " allow_move_input=" + (allowMoveInput ? "true" : "false")
            + " cursor_mode=" + (cursorMode ? "true" : "false")
            + " modal_blocked=" + (modalBlocked ? "true" : "false")
            + " pending_spell_target=" + (pendingSpellTarget ? "true" : "false")
            + " shared_world_blocked=" + (sharedWorldBlocked ? "true" : "false")
            + " movement_dt=" + std::to_string(movementDeltaSeconds)
            + " frame_dt=" + std::to_string(frameDeltaSeconds)
            + " movement_ap=" + std::to_string(movementActionPoints)
            + " pending=" + std::to_string(pendingActions));
        m_turnBasedFrameTraceState.lastLogTick = currentTick;
    }

    m_turnBasedFrameTraceState.active = true;
    m_turnBasedFrameTraceState.stage = stage;
    m_turnBasedFrameTraceState.actorAiUpdate = actorAiUpdate;
    m_turnBasedFrameTraceState.gameplayPaused = gameplayPaused;
    m_turnBasedFrameTraceState.allowMoveInput = allowMoveInput;
    m_turnBasedFrameTraceState.cursorMode = cursorMode;
    m_turnBasedFrameTraceState.modalBlocked = modalBlocked;
    m_turnBasedFrameTraceState.pendingSpellTarget = pendingSpellTarget;
    m_turnBasedFrameTraceState.sharedWorldBlocked = sharedWorldBlocked;
    m_turnBasedFrameTraceState.movementActionPoints = movementActionPoints;
    m_turnBasedFrameTraceState.pendingActions = pendingActions;
}

void GameSession::updateGameplay(
    const GameplayInputFrame &input,
    float deltaSeconds,
    bool collectPerformanceDiagnostics)
{
    const uint64_t totalBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;

    if (collectPerformanceDiagnostics)
    {
        ++m_gameplayUpdatePerformanceDiagnostics.frames;
    }

    const auto recordDiagnostics =
        [&](uint64_t &aggregateField, uint64_t &frameField, uint64_t beginTickCount)
    {
        if (collectPerformanceDiagnostics)
        {
            const uint64_t elapsedNanoseconds = SDL_GetTicksNS() - beginTickCount;
            aggregateField += elapsedNanoseconds;
            frameField += elapsedNanoseconds;
        }
    };

    m_gameplayScreenRuntime.updateDimensionDoor();
    bindCurrentGameplayInputFrame(&input);
    m_sharedInputFrameResult = {};

    const uint64_t sharedFrameStateBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
    GameplayScreenFrameUpdateConfig frameUpdateConfig = {};
    frameUpdateConfig.updateBuffInspectOverlay =
        m_currentSceneKind == SceneKind::Outdoor || m_currentSceneKind == SceneKind::Indoor;
    GameplayScreenController::updateSharedFrameState(
        m_gameplayScreenRuntime,
        input.screenWidth,
        input.screenHeight,
        deltaSeconds,
        frameUpdateConfig);
    recordDiagnostics(
        m_gameplayUpdatePerformanceDiagnostics.sharedFrameStateNanoseconds,
        m_lastGameplayUpdateFramePerformanceDiagnostics.sharedFrameStateNanoseconds,
        sharedFrameStateBeginTickCount);

    IGameplayWorldRuntime *pWorldRuntime = activeWorldRuntime();
    const uint64_t worldInteractionStateBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
    const bool hasActiveLootView =
        pWorldRuntime != nullptr
        && (pWorldRuntime->activeChestView() != nullptr || pWorldRuntime->activeCorpseView() != nullptr);
    const GameplayStandardWorldInteractionFrameState worldInteractionFrameState =
        GameplayScreenController::captureStandardWorldInteractionFrameState(m_gameplayScreenRuntime);
    m_sharedWorldInteractionBlockedThisFrame =
        GameplayScreenController::isStandardWorldInteractionBlockedForFrame(
            m_gameplayScreenRuntime,
            GameplayStandardWorldInteractionFrameGateConfig{
                .state = worldInteractionFrameState,
                .hasActiveLootView = hasActiveLootView,
            });
    recordDiagnostics(
        m_gameplayUpdatePerformanceDiagnostics.worldInteractionStateNanoseconds,
        m_lastGameplayUpdateFramePerformanceDiagnostics.worldInteractionStateNanoseconds,
        worldInteractionStateBeginTickCount);

    if (pWorldRuntime != nullptr)
    {
        if (collectPerformanceDiagnostics)
        {
            ++m_gameplayUpdatePerformanceDiagnostics.activeWorldFrames;
        }

        const uint64_t activeMemberSyncBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
        synchronizeGameplayActiveMemberToReadyMember(
            m_gameplayScreenRuntime,
            m_gameplayScreenState,
            m_turnBasedCombatRuntime);
        recordDiagnostics(
            m_gameplayUpdatePerformanceDiagnostics.activeMemberSyncNanoseconds,
            m_lastGameplayUpdateFramePerformanceDiagnostics.activeMemberSyncNanoseconds,
            activeMemberSyncBeginTickCount);

        const Party *pParty = pWorldRuntime->party();
        const bool hasReadyMember = pParty != nullptr && pParty->hasSelectableMemberInGameplay();
        const bool isUtilitySpellModalActive =
            m_gameplayScreenRuntime.utilitySpellOverlayReadOnly().active
            && m_gameplayScreenRuntime.utilitySpellOverlayReadOnly().mode
                != GameplayUiController::UtilitySpellOverlayMode::InventoryTarget;
        const bool isReadableScrollOverlayActive =
            m_gameplayScreenRuntime.readableScrollOverlayReadOnly().active;

        const uint64_t sharedInputBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
        m_sharedInputFrameResult =
            GameplayInputController::updateSharedGameplayInputFrame(
                m_gameplayScreenState,
                m_gameplayScreenRuntime,
                m_gameplaySpellService,
                GameplaySharedInputFrameConfig{
                    .pKeyboardState = input.keyboardState(),
                    .pInputFrame = &input,
                    .mouseWheelDelta = input.mouseWheelDelta,
                    .screenWidth = input.screenWidth,
                    .screenHeight = input.screenHeight,
                    .pointerX = input.pointerX,
                    .pointerY = input.pointerY,
                    .leftButtonPressed = input.leftMouseButton.held,
                    .rightButtonPressed = input.rightMouseButton.held,
                    .hasReadyMember = hasReadyMember,
                    .canBeginQuickCast = true,
                    .isUtilitySpellModalActive = isUtilitySpellModalActive,
                    .isReadableScrollOverlayActive = isReadableScrollOverlayActive,
                    .processSharedGameplayHotkeys = true,
                    .processQuickCast = true,
                });
        recordDiagnostics(
            m_gameplayUpdatePerformanceDiagnostics.sharedInputNanoseconds,
            m_lastGameplayUpdateFramePerformanceDiagnostics.sharedInputNanoseconds,
            sharedInputBeginTickCount);

        GameplayInputFrame worldInput = input;

        if (m_overlayInteractionState.gameplayHudAttackRequested)
        {
            pulseGameplayAction(worldInput, KeyboardAction::Attack);
            m_overlayInteractionState.gameplayHudAttackRequested = false;
        }

        if (m_overlayInteractionState.gameplayHudTriggerRequested)
        {
            pulseGameplayAction(worldInput, KeyboardAction::Trigger);
            m_overlayInteractionState.gameplayHudTriggerRequested = false;
        }

        const bool gameplayHudPointerActive =
            m_overlayInteractionState.gameplayHudClickLatch
            && m_overlayInteractionState.gameplayHudPressedTarget.type != GameplayHudPointerTargetType::None;
        const bool quickCastHudPointerActive =
            gameplayHudPointerActive
            && m_overlayInteractionState.gameplayHudPressedTarget.type
                == GameplayHudPointerTargetType::QuickCastButton;
        const bool gameplayCursorModeActive = m_sharedInputFrameResult.mouseLookPolicy.cursorModeActive;
        const bool pendingSpellTargetActive = m_gameplayScreenState.pendingSpellTarget().active;
        const bool gameplayHudPointerBlocksWorldInput =
            gameplayHudPointerActive
            && (!quickCastHudPointerActive || pendingSpellTargetActive);
        const bool rightMouseInspectPauseActive = input.rightMouseButton.held;
        const bool modalWorldInputBlocked =
            m_sharedInputFrameResult.journalInputConsumed
            || m_sharedInputFrameResult.worldInputBlocked
            || gameplayHudPointerBlocksWorldInput;
        const bool standardWorldInputBlocked =
            gameplayCursorModeActive
            || modalWorldInputBlocked;
        const bool allowWorldMovementInput =
            !standardWorldInputBlocked
            && !pendingSpellTargetActive
            && !rightMouseInspectPauseActive;
        float worldMovementDeltaSeconds = rightMouseInspectPauseActive ? 0.0f : deltaSeconds;
        bool turnBasedMovementStep = false;
        if (m_turnBasedCombatRuntime.active() && !rightMouseInspectPauseActive)
        {
            // Charge a step only when the same input is allowed to move the party. Menus,
            // journals and targeting overlays must not spend movement points while paused.
            turnBasedMovementStep = allowWorldMovementInput && m_turnBasedCombatRuntime.noteMovementInput(worldInput);
            worldMovementDeltaSeconds = allowWorldMovementInput
                ? m_turnBasedCombatRuntime.movementDeltaSecondsForFrame(deltaSeconds) : 0.0f;
            if (allowWorldMovementInput && !turnBasedMovementStep && pWorldRuntime->partyNeedsTurnBasedPhysicsUpdate())
            {
                worldInput.turnBasedPhysicsStep = true;
                worldMovementDeltaSeconds = deltaSeconds;
            }
        }

        const uint64_t worldMovementBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
        pWorldRuntime->updateWorldMovement(worldInput, worldMovementDeltaSeconds, allowWorldMovementInput);
        recordDiagnostics(
            m_gameplayUpdatePerformanceDiagnostics.worldMovementNanoseconds,
            m_lastGameplayUpdateFramePerformanceDiagnostics.worldMovementNanoseconds,
            worldMovementBeginTickCount);

        if (collectPerformanceDiagnostics)
        {
            const GameplayWorldMovementFrameDiagnostics *pWorldMovementDiagnostics =
                pWorldRuntime->lastWorldMovementFrameDiagnostics();

            if (pWorldMovementDiagnostics != nullptr)
            {
                m_lastGameplayUpdateFramePerformanceDiagnostics.worldMovement = *pWorldMovementDiagnostics;
            }
        }

        const bool gameplayWorldPaused =
            modalWorldInputBlocked
            || pendingSpellTargetActive
            || m_sharedWorldInteractionBlockedThisFrame
            || rightMouseInspectPauseActive;

        if (EventRuntimeState *pEventState = pWorldRuntime->eventRuntimeState())
        {
            pEventState->normalGameplayScreen = m_gameplayScreenRuntime.isNormalGameplayScreen();
        }

        const Party *pTurnBasedParty = pWorldRuntime->party();
        const bool turnBasedWorldPaused =
            m_turnBasedCombatRuntime.active()
            && !m_turnBasedCombatRuntime.shouldUpdateActorAi(pTurnBasedParty);

        synchronizeTurnBasedPendingWorldActions(
            m_turnBasedCombatRuntime,
            pWorldRuntime,
            m_turnBasedPendingWorldActions);

        logTurnBasedFrameTraceIfNeeded(
            !turnBasedWorldPaused,
            gameplayWorldPaused,
            allowWorldMovementInput,
            turnBasedMovementStep,
            gameplayCursorModeActive,
            modalWorldInputBlocked,
            pendingSpellTargetActive,
            m_sharedWorldInteractionBlockedThisFrame,
            worldMovementDeltaSeconds,
            deltaSeconds);

        if (!gameplayWorldPaused && !turnBasedWorldPaused)
        {
            if (collectPerformanceDiagnostics)
            {
                ++m_gameplayUpdatePerformanceDiagnostics.actorAiFrames;
            }

            const uint64_t actorAiBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
            pWorldRuntime->updateActorAi(deltaSeconds, m_turnBasedCombatRuntime.actorAiUpdateMode());
            recordDiagnostics(
                m_gameplayUpdatePerformanceDiagnostics.actorAiNanoseconds,
                m_lastGameplayUpdateFramePerformanceDiagnostics.actorAiNanoseconds,
                actorAiBeginTickCount);
        }
        else if (turnBasedWorldPaused)
        {
            pWorldRuntime->updateTurnBasedPausedActorAnimations(deltaSeconds);
        }

        synchronizeTurnBasedPendingWorldActions(
            m_turnBasedCombatRuntime,
            pWorldRuntime,
            m_turnBasedPendingWorldActions);

        if (!gameplayWorldPaused)
        {
            m_turnBasedCombatRuntime.update(pWorldRuntime->party(), pWorldRuntime, deltaSeconds);
        }

        const uint64_t combatEventsBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
        Party *pMutableParty = pWorldRuntime->party();
        if (pMutableParty != nullptr && !m_gameplayCombatController.pendingCombatEvents().empty())
        {
            GameplayCombatController::PendingCombatEventContext combatEventContext{
                .party = *pMutableParty,
                .pWorldRuntime = pWorldRuntime,
                .pRuntime = &m_gameplayScreenRuntime,
            };
            m_gameplayCombatController.handleAndClearPendingCombatEvents(combatEventContext);
        }
        recordDiagnostics(
            m_gameplayUpdatePerformanceDiagnostics.combatEventsNanoseconds,
            m_lastGameplayUpdateFramePerformanceDiagnostics.combatEventsNanoseconds,
            combatEventsBeginTickCount);

        const bool worldInputBlocked = modalWorldInputBlocked || m_sharedWorldInteractionBlockedThisFrame;
        const uint64_t interactionFrameBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
        GameplayInteractionController::updateWorldInteractionFrame(
            m_gameplayScreenState,
            m_overlayInteractionState,
            m_gameplayScreenRuntime,
            m_gameplaySpellService,
            worldInput,
            m_sharedInputFrameResult,
            worldInputBlocked);
        recordDiagnostics(
            m_gameplayUpdatePerformanceDiagnostics.interactionFrameNanoseconds,
            m_lastGameplayUpdateFramePerformanceDiagnostics.interactionFrameNanoseconds,
            interactionFrameBeginTickCount);
    }

    updateMm9QuestMarkerCache();

    if (deltaSeconds > 0.0f && !input.rightMouseButton.held)
    {
        const uint64_t projectileCooldownBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
        m_gameplayProjectileService.updateProjectileImpactPresentation(deltaSeconds);
        GameplayActionController::updateCooldowns(m_gameplayScreenState, deltaSeconds);
        recordDiagnostics(
            m_gameplayUpdatePerformanceDiagnostics.projectileAndCooldownNanoseconds,
            m_lastGameplayUpdateFramePerformanceDiagnostics.projectileAndCooldownNanoseconds,
            projectileCooldownBeginTickCount);
    }

    if (collectPerformanceDiagnostics)
    {
        const uint64_t totalNanoseconds = SDL_GetTicksNS() - totalBeginTickCount;
        m_gameplayUpdatePerformanceDiagnostics.totalNanoseconds += totalNanoseconds;
        m_lastGameplayUpdateFramePerformanceDiagnostics.totalNanoseconds += totalNanoseconds;
        const uint64_t performanceLogBeginTickCount = SDL_GetTicksNS();
        const bool loggedPerformanceDiagnostics = logGameplayUpdatePerformanceDiagnostics(SDL_GetTicks());
        const uint64_t performanceLogNanoseconds = SDL_GetTicksNS() - performanceLogBeginTickCount;
        m_lastGameplayUpdateFramePerformanceDiagnostics.performanceTraceLogNanoseconds +=
            performanceLogNanoseconds;
        m_lastGameplayUpdateFramePerformanceDiagnostics.totalNanoseconds += performanceLogNanoseconds;

        if (!loggedPerformanceDiagnostics)
        {
            m_gameplayUpdatePerformanceDiagnostics.performanceTraceLogNanoseconds +=
                performanceLogNanoseconds;
            m_gameplayUpdatePerformanceDiagnostics.totalNanoseconds += performanceLogNanoseconds;
        }
    }
}

void GameSession::updateMm9QuestMarkerCache()
{
    if (m_pDataRepository == nullptr
        || !m_pDataRepository->isBound()
        || m_pActiveWorldRuntime == nullptr
        || data().mm9QuestInteractionTable().interactions().empty())
    {
        if (m_mm9QuestMarkerCache.active())
        {
            m_mm9QuestMarkerCache.clear();
        }
        return;
    }

    Party *pParty = m_pActiveWorldRuntime->party();
    if (pParty == nullptr)
    {
        m_mm9QuestMarkerCache.clear();
        return;
    }

    std::string mapId = std::filesystem::path(m_currentMapFileName).stem().string();
    std::transform(
        mapId.begin(),
        mapId.end(),
        mapId.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    std::vector<Mm9LoadedQuestMarkerActor> actors;
    uint64_t actorEligibilitySignature = 1469598103934665603ULL;
    const auto addSignatureValue = [&actorEligibilitySignature](uint64_t value)
    {
        actorEligibilitySignature ^= value;
        actorEligibilitySignature *= 1099511628211ULL;
    };
    for (size_t actorIndex = 0; actorIndex < m_pActiveWorldRuntime->mapActorCount(); ++actorIndex)
    {
        GameplayRuntimeActorState state = {};
        if (!m_pActiveWorldRuntime->actorRuntimeState(actorIndex, state) || state.mm9RudeId <= 0)
        {
            continue;
        }
        const bool interactable = !state.isDead && !state.isInvisible && !state.hostileToParty;
        actors.push_back({
            static_cast<uint32_t>(actorIndex),
            {
                mapId,
                state.mm9SourceObjectIndex,
                static_cast<uint32_t>(state.mm9RudeId),
                interactable,
            },
        });
        addSignatureValue(actorIndex);
        addSignatureValue(state.mm9SourceObjectIndex);
        addSignatureValue(static_cast<uint32_t>(state.mm9RudeId));
        addSignatureValue(interactable ? 1 : 0);
    }

    const Mm9QuestEligibilityStamp stamp = {
        pParty->dialogueEligibilityRevision(),
        m_mm9QuestMarkerMapRevision ^ actorEligibilitySignature,
    };
    if (!m_mm9QuestMarkerCache.active())
    {
        m_mm9QuestMarkerCache.loadLocation(
            true,
            actors,
            stamp,
            *pParty,
            data().journalQuestTable(),
            data().mm9RudeDialogueTable(),
            data().mm9QuestInteractionTable());
        return;
    }
    m_mm9QuestMarkerCache.update(
        actors,
        stamp,
        *pParty,
        data().journalQuestTable(),
        data().mm9RudeDialogueTable(),
        data().mm9QuestInteractionTable());
}

void GameSession::clearSharedInputFrameResult()
{
    m_sharedInputFrameResult = {};
    m_sharedWorldInteractionBlockedThisFrame = false;
    m_gameplayScreenState.gameplayMouseLookState().clear();
}

void GameSession::consumePendingGameplayAudioRequests()
{
    m_gameplayScreenRuntime.updateDimensionDoor();
    m_gameplayScreenRuntime.consumePendingEventRuntimeAudioRequests();
}

void GameSession::renderGameplayUi(int width, int height)
{
    const bool collectPerformanceDiagnostics = m_framePerformanceDiagnosticsEnabled;
    const uint64_t totalBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
    const auto recordDiagnostics =
        [&](uint64_t &field, uint64_t beginTickCount)
        {
            if (collectPerformanceDiagnostics)
            {
                field += SDL_GetTicksNS() - beginTickCount;
            }
        };
    const auto finishDiagnostics =
        [&]()
        {
            if (!collectPerformanceDiagnostics)
            {
                return;
            }

            m_lastGameplayUiFramePerformanceDiagnostics.totalNanoseconds =
                SDL_GetTicksNS() - totalBeginTickCount;
            m_lastGameplayUiFramePerformanceDiagnostics.overlays =
                m_gameplayUiRuntime.lastOverlayFramePerformanceDiagnostics();
        };

    IGameplayWorldRuntime *pWorldRuntime = activeWorldRuntime();

    if (pWorldRuntime == nullptr)
    {
        finishDiagnostics();
        return;
    }

    const uint64_t uiRenderStateBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
    const GameplayWorldUiRenderState uiRenderState = pWorldRuntime->gameplayUiRenderState(width, height);
    recordDiagnostics(
        m_lastGameplayUiFramePerformanceDiagnostics.worldUiRenderStateNanoseconds,
        uiRenderStateBeginTickCount);

    if (!uiRenderState.renderGameplayHud)
    {
        finishDiagnostics();
        return;
    }

    const GameplayScreenState::GameplayMouseLookState &gameplayMouseLookState =
        m_gameplayScreenState.gameplayMouseLookState();
    const GameplayScreenState::PendingSpellTargetState &pendingSpellCast =
        m_gameplayScreenState.pendingSpellTarget();

    const uint64_t standardUiBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
    GameplayScreenController::renderStandardUi(
        m_gameplayScreenRuntime,
        width,
        height,
        GameplayStandardUiRenderConfig{
            .canRenderHudOverlays = uiRenderState.canRenderHudOverlays,
            .renderGameplayHud = uiRenderState.renderGameplayHud,
            .renderGameplayMouseLookOverlay =
                gameplayMouseLookState.mouseLookActive
                    && !gameplayMouseLookState.cursorModeActive
                    && !pendingSpellCast.active,
            .renderActorInspectOverlay = uiRenderState.renderActorInspectOverlay,
            .renderDebugFallbacks = uiRenderState.renderDebugFallbacks,
        });
    recordDiagnostics(
        m_lastGameplayUiFramePerformanceDiagnostics.standardUiNanoseconds,
        standardUiBeginTickCount);

    const GameplayInputFrame *pInputFrame = currentGameplayInputFrame();

    if (pInputFrame == nullptr)
    {
        finishDiagnostics();
        return;
    }

    const uint64_t pendingSpellOverlayBeginTickCount = collectPerformanceDiagnostics ? SDL_GetTicksNS() : 0;
    if (pendingSpellCast.active)
    {
        GameplaySpellTargetingOverlayRenderer::renderPendingSpellTargetingOverlay(
            m_gameplayScreenRuntime,
            m_gameplaySpellService,
            pendingSpellCast,
            width,
            height,
            pInputFrame->pointerX,
            pInputFrame->pointerY);
    }
    recordDiagnostics(
        m_lastGameplayUiFramePerformanceDiagnostics.pendingSpellOverlayNanoseconds,
        pendingSpellOverlayBeginTickCount);
    finishDiagnostics();
}

const GameplaySharedInputFrameResult &GameSession::sharedInputFrameResult() const
{
    return m_sharedInputFrameResult;
}

bool GameSession::sharedWorldInteractionBlockedThisFrame() const
{
    return m_sharedWorldInteractionBlockedThisFrame;
}

void GameSession::requestRelativeMouseMotionReset()
{
    m_relativeMouseMotionResetRequested = true;
}

bool GameSession::consumeRelativeMouseMotionResetRequest()
{
    const bool requested = m_relativeMouseMotionResetRequested;
    m_relativeMouseMotionResetRequested = false;
    return requested;
}

const std::optional<OutdoorPartyRuntime::Snapshot> &GameSession::outdoorPartyState() const
{
    return m_outdoorPartyState;
}

void GameSession::setOutdoorPartyState(const OutdoorPartyRuntime::Snapshot &snapshot)
{
    m_outdoorPartyState = snapshot;
}

void GameSession::setOutdoorPartyState(OutdoorPartyRuntime::Snapshot &&snapshot)
{
    m_outdoorPartyState = std::move(snapshot);
}

const std::optional<OutdoorWorldRuntime::Snapshot> &GameSession::currentOutdoorWorldState() const
{
    return m_currentOutdoorWorldState;
}

void GameSession::setCurrentOutdoorWorldState(const OutdoorWorldRuntime::Snapshot &snapshot)
{
    m_currentOutdoorWorldState = snapshot;
    m_currentOutdoorWorldState->gameMinutes = m_gameMinutes;
    if (m_currentOutdoorWorldState->eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        captureHistoryFromRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        applyHistoryToRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
    }
}

void GameSession::setCurrentOutdoorWorldState(OutdoorWorldRuntime::Snapshot &&snapshot)
{
    m_currentOutdoorWorldState = std::move(snapshot);
    m_currentOutdoorWorldState->gameMinutes = m_gameMinutes;
    if (m_currentOutdoorWorldState->eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        captureHistoryFromRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        applyHistoryToRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
    }
}

const std::unordered_map<std::string, OutdoorWorldRuntime::Snapshot> &GameSession::outdoorWorldStates() const
{
    return m_outdoorWorldStates;
}

std::unordered_map<std::string, OutdoorWorldRuntime::Snapshot> &GameSession::outdoorWorldStates()
{
    return m_outdoorWorldStates;
}

void GameSession::storeOutdoorWorldState(const std::string &mapFileName, const OutdoorWorldRuntime::Snapshot &snapshot)
{
    OutdoorWorldRuntime::Snapshot normalizedSnapshot = snapshot;
    normalizedSnapshot.gameMinutes = m_gameMinutes;
    if (normalizedSnapshot.eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*normalizedSnapshot.eventRuntimeState);
        captureHistoryFromRuntime(*normalizedSnapshot.eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*normalizedSnapshot.eventRuntimeState);
        applyHistoryToRuntime(*normalizedSnapshot.eventRuntimeState);
    }
    m_outdoorWorldStates[mapFileName] = normalizedSnapshot;
    if (m_breachAlvarCompanions)
    {
        const std::string canonical = buildCanonicalMapId("mmmerge", "bralvar.odm");
        const auto isCompanionMap = [&](const std::string &name)
        {
            const std::string lower = toLowerCopy(name);
            return lower == "bralvar.odm" || lower == BreachAlvarCompanionMap || lower == canonical;
        };
        if (isCompanionMap(mapFileName))
        {
            // These aliases refer to one live map. Each capture must advance them together.
            for (auto &[key, cached] : m_outdoorWorldStates)
            {
                if (isCompanionMap(key)) cached = normalizedSnapshot;
            }
            if (isCompanionMap(m_currentMapFileName)) m_currentOutdoorWorldState = normalizedSnapshot;
        }
    }
}

const std::optional<IndoorSceneRuntime::Snapshot> &GameSession::currentIndoorSceneState() const
{
    return m_currentIndoorSceneState;
}

void GameSession::setCurrentIndoorSceneState(const IndoorSceneRuntime::Snapshot &snapshot)
{
    m_currentIndoorSceneState = snapshot;
    m_currentIndoorSceneState->worldRuntime.gameMinutes = m_gameMinutes;
    if (m_currentIndoorSceneState->eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        captureHistoryFromRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        applyHistoryToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
    }
}

void GameSession::setCurrentIndoorSceneState(IndoorSceneRuntime::Snapshot &&snapshot)
{
    m_currentIndoorSceneState = std::move(snapshot);
    m_currentIndoorSceneState->worldRuntime.gameMinutes = m_gameMinutes;
    if (m_currentIndoorSceneState->eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        captureHistoryFromRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        applyHistoryToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
    }
}

const std::unordered_map<std::string, IndoorSceneRuntime::Snapshot> &GameSession::indoorSceneStates() const
{
    return m_indoorSceneStates;
}

std::unordered_map<std::string, IndoorSceneRuntime::Snapshot> &GameSession::indoorSceneStates()
{
    return m_indoorSceneStates;
}

void GameSession::storeIndoorSceneState(const std::string &mapFileName, const IndoorSceneRuntime::Snapshot &snapshot)
{
    IndoorSceneRuntime::Snapshot normalizedSnapshot = snapshot;
    normalizedSnapshot.worldRuntime.gameMinutes = m_gameMinutes;
    if (normalizedSnapshot.eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*normalizedSnapshot.eventRuntimeState);
        captureHistoryFromRuntime(*normalizedSnapshot.eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*normalizedSnapshot.eventRuntimeState);
        applyHistoryToRuntime(*normalizedSnapshot.eventRuntimeState);
    }
    if (!m_breachMazeRunData || !isBreachMazeMap(mapFileName))
    {
        m_indoorSceneStates[mapFileName] = normalizedSnapshot;
    }
}

const std::optional<BreachMazeRunData> &GameSession::breachMazeRunData() const
{
    return m_breachMazeRunData;
}

void GameSession::setBreachMazeRunData(std::optional<BreachMazeRunData> data)
{
    m_breachMazeRunData = std::move(data);
    if (m_breachMazeRunData)
    {
        std::erase_if(m_indoorSceneStates, [](const auto &entry) { return isBreachMazeMap(entry.first); });
    }
}

const std::optional<BreachAlvarCompanionState> &GameSession::breachAlvarCompanions() const
{
    return m_breachAlvarCompanions;
}

const std::unordered_map<std::string, EventRuntimeState::SavedLocation> &GameSession::sessionSavedLocations() const
{
    return m_sessionSavedLocations;
}

void GameSession::setSessionSavedLocation(const std::string &name, EventRuntimeState::SavedLocation location)
{
    if (!name.empty() && !location.mapName.empty())
    {
        m_sessionSavedLocations[name] = std::move(location);
    }
}

void GameSession::clearSessionSavedLocation(const std::string &name)
{
    m_sessionSavedLocations.erase(name);
}

const std::unordered_map<std::string, int32_t> &GameSession::namedGlobalVars() const
{
    return m_namedGlobalVars;
}

int32_t GameSession::namedGlobalVar(const std::string &name, int32_t defaultValue) const
{
    const std::unordered_map<std::string, int32_t>::const_iterator it = m_namedGlobalVars.find(name);
    return it != m_namedGlobalVars.end() ? it->second : defaultValue;
}

void GameSession::setNamedGlobalVar(const std::string &name, int32_t value)
{
    if (name.empty())
    {
        return;
    }

    m_namedGlobalVars[name] = value;
}

void GameSession::clearNamedGlobalVar(const std::string &name)
{
    m_namedGlobalVars.erase(name);
}

void GameSession::mergeNamedGlobalVarsFromRuntime(const EventRuntimeState &runtimeState)
{
    for (const auto &[name, value] : runtimeState.namedGlobalVars)
    {
        if (!name.empty())
        {
            m_namedGlobalVars[name] = value;
        }
    }
}

void GameSession::applyNamedGlobalVarsToRuntime(EventRuntimeState &runtimeState) const
{
    for (const auto &[name, value] : m_namedGlobalVars)
    {
        runtimeState.namedGlobalVars[name] = value;
    }
}

void GameSession::captureHistoryFromRuntime(const EventRuntimeState &runtimeState)
{
    // A live runtime receives the session history before its map scripts run. Replace each
    // represented continent so explicit History clears survive subsequent map visits.
    for (const auto &[continent, events] : runtimeState.historyEventTimesByContinent)
    {
        m_historyEventTimesByContinent[continent] = events;
    }
    if (!runtimeState.historyEventTimesByContinent.contains(1) && !runtimeState.historyEventTimes.empty())
    {
        m_historyEventTimesByContinent[1] = runtimeState.historyEventTimes;
    }
}

void GameSession::applyHistoryToRuntime(EventRuntimeState &runtimeState) const
{
    for (const auto &[continent, events] : m_historyEventTimesByContinent)
    {
        runtimeState.historyEventTimesByContinent[continent] = events;
    }
    const auto legacy = runtimeState.historyEventTimesByContinent.find(1);
    runtimeState.historyEventTimes = legacy != runtimeState.historyEventTimesByContinent.end()
        ? legacy->second : std::unordered_map<uint32_t, int32_t>{};
}

void GameSession::restoreHistoryFromSaveData(const GameSaveData &saveData)
{
    m_historyEventTimesByContinent.clear();
    // Older saves stored history separately in each visited map. Migrate the saved
    // occurrences once, retaining their earliest recorded dates. New saves carry the
    // same session history in every snapshot, including any explicitly cleared entries.
    const auto collectEvents = [this](uint32_t continent, const auto &events)
    {
        auto &destination = m_historyEventTimesByContinent[normalizedHistoryContinentId(continent)];
        for (const auto &[event, time] : events)
        {
            const auto [entry, inserted] = destination.try_emplace(event, time);
            if (!inserted)
            {
                entry->second = std::min(entry->second, time);
            }
        }
    };
    const auto collectSnapshot = [&](const auto &snapshot)
    {
        if (!snapshot.eventRuntimeState)
        {
            return;
        }
        const EventRuntimeState &state = *snapshot.eventRuntimeState;
        for (const auto &[continent, events] : state.historyEventTimesByContinent)
        {
            collectEvents(continent, events);
        }
        if (!state.historyEventTimesByContinent.contains(1))
        {
            collectEvents(1, state.historyEventTimes);
        }
    };
    if (saveData.hasOutdoorRuntimeState)
    {
        collectSnapshot(saveData.outdoorWorld);
    }
    if (saveData.hasIndoorSceneState)
    {
        collectSnapshot(saveData.indoorScene);
    }
    for (const auto &[map, snapshot] : saveData.outdoorWorldStates)
    {
        collectSnapshot(snapshot);
    }
    for (const auto &[map, snapshot] : saveData.indoorSceneStates)
    {
        collectSnapshot(snapshot);
    }
}

void GameSession::setOutdoorCameraAngles(float yawRadians, float pitchRadians)
{
    m_outdoorCameraYawRadians = yawRadians;
    m_outdoorCameraPitchRadians = pitchRadians;
}

float GameSession::outdoorCameraYawRadians() const
{
    return m_outdoorCameraYawRadians;
}

float GameSession::outdoorCameraPitchRadians() const
{
    return m_outdoorCameraPitchRadians;
}

float GameSession::indoorCameraYawRadians() const
{
    return m_indoorCameraYawRadians;
}

float GameSession::indoorCameraPitchRadians() const
{
    return m_indoorCameraPitchRadians;
}

const std::optional<std::filesystem::path> &GameSession::currentSavePath() const
{
    return m_currentSavePath;
}

void GameSession::setCurrentSavePath(const std::filesystem::path &path)
{
    m_currentSavePath = path;
}

void GameSession::clearCurrentSavePath()
{
    m_currentSavePath.reset();
}

const std::optional<EventRuntimeState::PendingMapMove> &GameSession::pendingMapMove() const
{
    return m_pendingMapMove;
}

void GameSession::setPendingMapMove(const EventRuntimeState::PendingMapMove &pendingMapMove)
{
    m_pendingMapMove = pendingMapMove;
}

void GameSession::setPendingMapMove(EventRuntimeState::PendingMapMove &&pendingMapMove)
{
    m_pendingMapMove = std::move(pendingMapMove);
}

std::optional<EventRuntimeState::PendingMapMove> GameSession::consumePendingMapMove()
{
    std::optional<EventRuntimeState::PendingMapMove> pendingMapMove = std::move(m_pendingMapMove);
    m_pendingMapMove.reset();
    return pendingMapMove;
}

void GameSession::clearPendingMapMove()
{
    m_pendingMapMove.reset();
}

void GameSession::setSaveGameToPathCallback(SaveGameToPathCallback callback)
{
    m_saveGameToPathCallback = std::move(callback);
}

bool GameSession::canSaveGameToPath() const
{
    return static_cast<bool>(m_saveGameToPathCallback);
}

bool GameSession::saveGameToPath(
    const std::filesystem::path &path,
    const std::string &saveName,
    const std::vector<uint8_t> &previewBmp,
    std::string &error) const
{
    return m_saveGameToPathCallback && m_saveGameToPathCallback(path, saveName, previewBmp, error);
}

void GameSession::setSettingsChangedCallback(SettingsChangedCallback callback)
{
    m_settingsChangedCallback = std::move(callback);
}

void GameSession::notifySettingsChanged(const GameSettings &settings) const
{
    if (m_settingsChangedCallback)
    {
        m_settingsChangedCallback(settings);
    }
}

void GameSession::requestOpenNewGameScreen()
{
    m_gameplayScreenState.requestOpenNewGameScreen();
}

void GameSession::requestOpenLoadGameScreen()
{
    m_gameplayScreenState.requestOpenLoadGameScreen();
}

bool GameSession::consumePendingOpenNewGameScreenRequest()
{
    return m_gameplayScreenState.consumePendingOpenNewGameScreenRequest();
}

bool GameSession::consumePendingOpenLoadGameScreenRequest()
{
    return m_gameplayScreenState.consumePendingOpenLoadGameScreenRequest();
}

void GameSession::captureOutdoorRuntimeState(
    const std::string &mapFileName,
    const Party &party,
    const OutdoorPartyRuntime::Snapshot &partySnapshot,
    const OutdoorWorldRuntime::Snapshot &worldSnapshot,
    float yawRadians,
    float pitchRadians)
{
    m_currentSceneKind = SceneKind::Outdoor;
    m_currentMapFileName = mapFileName;
    m_partyState = party;
    m_outdoorPartyState = partySnapshot;
    setGameMinutes(worldSnapshot.gameMinutes);

    OutdoorWorldRuntime::Snapshot normalizedWorldSnapshot = worldSnapshot;
    normalizedWorldSnapshot.gameMinutes = m_gameMinutes;
    if (normalizedWorldSnapshot.eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*normalizedWorldSnapshot.eventRuntimeState);
        captureHistoryFromRuntime(*normalizedWorldSnapshot.eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*normalizedWorldSnapshot.eventRuntimeState);
        applyHistoryToRuntime(*normalizedWorldSnapshot.eventRuntimeState);
    }
    m_currentOutdoorWorldState = normalizedWorldSnapshot;
    storeOutdoorWorldState(mapFileName, normalizedWorldSnapshot);
    m_outdoorCameraYawRadians = yawRadians;
    m_outdoorCameraPitchRadians = pitchRadians;
}

void GameSession::captureIndoorRuntimeState(
    const std::string &mapFileName,
    const Party &party,
    const IndoorSceneRuntime::Snapshot &snapshot,
    float yawRadians,
    float pitchRadians)
{
    m_currentSceneKind = SceneKind::Indoor;
    m_currentMapFileName = mapFileName;
    m_partyState = party;
    setGameMinutes(snapshot.worldRuntime.gameMinutes);

    IndoorSceneRuntime::Snapshot normalizedSnapshot = snapshot;
    normalizedSnapshot.worldRuntime.gameMinutes = m_gameMinutes;
    if (normalizedSnapshot.eventRuntimeState)
    {
        mergeNamedGlobalVarsFromRuntime(*normalizedSnapshot.eventRuntimeState);
        captureHistoryFromRuntime(*normalizedSnapshot.eventRuntimeState);
        applyNamedGlobalVarsToRuntime(*normalizedSnapshot.eventRuntimeState);
        applyHistoryToRuntime(*normalizedSnapshot.eventRuntimeState);
    }
    m_currentIndoorSceneState = normalizedSnapshot;
    if (!m_breachMazeRunData || !isBreachMazeMap(mapFileName))
    {
        m_indoorSceneStates[mapFileName] = normalizedSnapshot;
    }
    m_indoorCameraYawRadians = yawRadians;
    m_indoorCameraPitchRadians = pitchRadians;
}

std::optional<GameSaveData> GameSession::buildSaveData() const
{
    if (!m_partyState || m_currentMapFileName.empty())
    {
        return std::nullopt;
    }

    if (m_currentSceneKind == SceneKind::Outdoor && (!m_outdoorPartyState || !m_currentOutdoorWorldState))
    {
        return std::nullopt;
    }

    if (m_currentSceneKind == SceneKind::Indoor && !m_currentIndoorSceneState)
    {
        return std::nullopt;
    }

    GameSaveData saveData = {};
    saveData.currentSceneKind = m_currentSceneKind;
    saveData.mapFileName = m_currentMapFileName;
    saveData.party = m_partyState->snapshot();
    saveData.namedGlobalVars = m_namedGlobalVars;
    saveData.sessionSavedLocations = m_sessionSavedLocations;
    saveData.pendingMapMove = m_pendingMapMove;
    saveData.breachAlvarCompanions = m_breachAlvarCompanions;
    saveData.breachMaze = m_breachMazeRunData;
    saveData.savedGameMinutes = m_gameMinutes;
    saveData.turnBasedCombat = m_turnBasedCombatRuntime.snapshot();
    if (saveData.turnBasedCombat.active && m_pActiveWorldRuntime != nullptr)
    {
        // A save may occur between projectile creation and the next pending-action synchronization.
        saveData.turnBasedCombat.pendingActions =
            static_cast<int>(m_pActiveWorldRuntime->turnBasedPendingWorldActionCount());
    }

    if (m_outdoorPartyState && m_currentOutdoorWorldState)
    {
        saveData.hasOutdoorRuntimeState = true;
        saveData.outdoorParty = *m_outdoorPartyState;
        saveData.outdoorWorld = *m_currentOutdoorWorldState;
        saveData.outdoorWorld.gameMinutes = m_gameMinutes;
        if (saveData.outdoorWorld.eventRuntimeState)
        {
            applyNamedGlobalVarsToRuntime(*saveData.outdoorWorld.eventRuntimeState);
            applyHistoryToRuntime(*saveData.outdoorWorld.eventRuntimeState);
        }
    }

    saveData.outdoorWorldStates = m_outdoorWorldStates;
    for (auto &[mapFileName, worldState] : saveData.outdoorWorldStates)
    {
        worldState.gameMinutes = m_gameMinutes;
        if (worldState.eventRuntimeState)
        {
            applyNamedGlobalVarsToRuntime(*worldState.eventRuntimeState);
            applyHistoryToRuntime(*worldState.eventRuntimeState);
        }
    }

    if (m_currentIndoorSceneState)
    {
        saveData.hasIndoorSceneState = true;
        saveData.indoorScene = *m_currentIndoorSceneState;
        saveData.indoorScene.worldRuntime.gameMinutes = m_gameMinutes;
        if (saveData.indoorScene.eventRuntimeState)
        {
            applyNamedGlobalVarsToRuntime(*saveData.indoorScene.eventRuntimeState);
            applyHistoryToRuntime(*saveData.indoorScene.eventRuntimeState);
        }
    }

    saveData.indoorSceneStates = m_indoorSceneStates;
    for (auto &[mapFileName, sceneState] : saveData.indoorSceneStates)
    {
        sceneState.worldRuntime.gameMinutes = m_gameMinutes;
        if (sceneState.eventRuntimeState)
        {
            applyNamedGlobalVarsToRuntime(*sceneState.eventRuntimeState);
            applyHistoryToRuntime(*sceneState.eventRuntimeState);
        }
    }

    if (saveData.breachMaze)
    {
        std::erase_if(saveData.indoorSceneStates, [](const auto &entry) { return isBreachMazeMap(entry.first); });
        for (auto &[level, floor] : saveData.breachMaze->floors)
        {
            if (floor.runtime.scene.eventRuntimeState)
            {
                applyNamedGlobalVarsToRuntime(*floor.runtime.scene.eventRuntimeState);
                applyHistoryToRuntime(*floor.runtime.scene.eventRuntimeState);
            }
        }
        const bool genericMazeScene = saveData.indoorScene.eventRuntimeState
            && isBreachMazeMap(saveData.indoorScene.eventRuntimeState->mapFileName);
        if ((saveData.breachMaze->inside && isBreachMazeMap(m_currentMapFileName)) || genericMazeScene)
        {
            // Maze scenes remain owned by their floor after departure as well as while active.
            saveData.hasIndoorSceneState = false;
            saveData.indoorScene = {};
        }
    }

    saveData.outdoorCameraYawRadians = m_outdoorCameraYawRadians;
    saveData.outdoorCameraPitchRadians = m_outdoorCameraPitchRadians;
    saveData.indoorCameraYawRadians = m_indoorCameraYawRadians;
    saveData.indoorCameraPitchRadians = m_indoorCameraPitchRadians;

    const GameplayUiController::HeldInventoryItemState &heldItem = m_gameplayUiController.heldInventoryItem();
    saveData.heldInventoryItemActive = heldItem.active;
    if (heldItem.active)
    {
        saveData.heldInventoryItem = heldItem.item;
        saveData.heldInventoryItemGrabCellOffsetX = heldItem.grabCellOffsetX;
        saveData.heldInventoryItemGrabCellOffsetY = heldItem.grabCellOffsetY;
        saveData.heldInventoryItemGrabOffsetX = heldItem.grabOffsetX;
        saveData.heldInventoryItemGrabOffsetY = heldItem.grabOffsetY;
    }

    return saveData;
}

void GameSession::restoreFromSaveData(const GameSaveData &saveData)
{
    m_gameplayScreenRuntime.resetTownPortalContact();
    m_gameplayScreenRuntime.resetDimensionDoor();
    m_turnBasedCombatRuntime.reset();
    m_turnBasedPendingWorldActions = 0;
    m_partyState = buildConfiguredParty(saveData.party, data());
    restoreHeldInventoryItemFromSaveData(saveData);

    if (m_partyState)
    {
        migrateBlazenRescueRosterUnlock(*m_partyState);

        if (saveData.currentSceneKind == SceneKind::Outdoor && saveData.hasOutdoorRuntimeState)
        {
            migrateSavedRuntimeFollowersToParty(*m_partyState, saveData.outdoorWorld.eventRuntimeState);
        }
        else if (saveData.currentSceneKind == SceneKind::Indoor && saveData.hasIndoorSceneState)
        {
            migrateSavedRuntimeFollowersToParty(*m_partyState, saveData.indoorScene.eventRuntimeState);
        }
    }

    m_currentSceneKind = saveData.currentSceneKind;
    m_currentMapFileName = saveData.mapFileName;
    setGameMinutes(saveData.savedGameMinutes);
    m_namedGlobalVars = saveData.namedGlobalVars;
    m_sessionSavedLocations = saveData.sessionSavedLocations;
    m_breachAlvarCompanions = saveData.breachAlvarCompanions;
    m_breachMazeRunData = saveData.breachMaze;
    const bool collectLegacyNamedGlobalVars = m_namedGlobalVars.empty();
    restoreHistoryFromSaveData(saveData);

    if (m_gameMinutes <= 0.0f)
    {
        if (saveData.currentSceneKind == SceneKind::Outdoor && saveData.hasOutdoorRuntimeState)
        {
            setGameMinutes(saveData.outdoorWorld.gameMinutes);
        }
        else if (saveData.currentSceneKind == SceneKind::Indoor && saveData.hasIndoorSceneState)
        {
            setGameMinutes(saveData.indoorScene.worldRuntime.gameMinutes);
        }
    }

    m_outdoorPartyState = saveData.hasOutdoorRuntimeState
        ? std::optional<OutdoorPartyRuntime::Snapshot>(saveData.outdoorParty)
        : std::nullopt;
    m_currentOutdoorWorldState = saveData.hasOutdoorRuntimeState
        ? std::optional<OutdoorWorldRuntime::Snapshot>(saveData.outdoorWorld)
        : std::nullopt;
    if (m_currentOutdoorWorldState)
    {
        m_currentOutdoorWorldState->gameMinutes = m_gameMinutes;
        if (m_currentOutdoorWorldState->eventRuntimeState)
        {
            if (collectLegacyNamedGlobalVars)
            {
                mergeNamedGlobalVarsFromRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
            }
            applyNamedGlobalVarsToRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
            applyHistoryToRuntime(*m_currentOutdoorWorldState->eventRuntimeState);
        }
    }

    m_outdoorWorldStates = saveData.outdoorWorldStates;
    for (auto &[mapFileName, worldState] : m_outdoorWorldStates)
    {
        worldState.gameMinutes = m_gameMinutes;
        if (worldState.eventRuntimeState)
        {
            if (collectLegacyNamedGlobalVars)
            {
                mergeNamedGlobalVarsFromRuntime(*worldState.eventRuntimeState);
            }
            applyNamedGlobalVarsToRuntime(*worldState.eventRuntimeState);
            applyHistoryToRuntime(*worldState.eventRuntimeState);
        }
    }

    if (saveData.hasOutdoorRuntimeState && m_currentSceneKind == SceneKind::Outdoor)
    {
        m_outdoorWorldStates[m_currentMapFileName] = *m_currentOutdoorWorldState;
    }

    m_currentIndoorSceneState = saveData.hasIndoorSceneState
        ? std::optional<IndoorSceneRuntime::Snapshot>(saveData.indoorScene)
        : std::nullopt;
    if (m_currentIndoorSceneState)
    {
        m_currentIndoorSceneState->worldRuntime.gameMinutes = m_gameMinutes;
        if (m_currentIndoorSceneState->eventRuntimeState)
        {
            if (collectLegacyNamedGlobalVars)
            {
                mergeNamedGlobalVarsFromRuntime(*m_currentIndoorSceneState->eventRuntimeState);
            }
            applyNamedGlobalVarsToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
            applyHistoryToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
        }
    }

    m_indoorSceneStates = saveData.indoorSceneStates;
    for (auto &[mapFileName, sceneState] : m_indoorSceneStates)
    {
        sceneState.worldRuntime.gameMinutes = m_gameMinutes;
        if (sceneState.eventRuntimeState)
        {
            if (collectLegacyNamedGlobalVars)
            {
                mergeNamedGlobalVarsFromRuntime(*sceneState.eventRuntimeState);
            }
            applyNamedGlobalVarsToRuntime(*sceneState.eventRuntimeState);
            applyHistoryToRuntime(*sceneState.eventRuntimeState);
        }
    }

    if (saveData.hasIndoorSceneState && m_currentSceneKind == SceneKind::Indoor)
    {
        m_indoorSceneStates[m_currentMapFileName] = *m_currentIndoorSceneState;
    }

    if (m_breachMazeRunData)
    {
        std::erase_if(m_indoorSceneStates, [](const auto &entry) { return isBreachMazeMap(entry.first); });
        if (m_breachMazeRunData->inside && isBreachMazeMap(m_currentMapFileName))
        {
            const auto &floor = m_breachMazeRunData->floors.at(m_breachMazeRunData->currentLevel);
            m_currentIndoorSceneState = floor.runtime.scene;
            m_currentIndoorSceneState->worldRuntime.gameMinutes = m_gameMinutes;
            if (m_currentIndoorSceneState->eventRuntimeState)
            {
                applyNamedGlobalVarsToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
                applyHistoryToRuntime(*m_currentIndoorSceneState->eventRuntimeState);
            }
        }
    }

    m_outdoorCameraYawRadians = saveData.outdoorCameraYawRadians;
    m_outdoorCameraPitchRadians = saveData.outdoorCameraPitchRadians;
    m_indoorCameraYawRadians = saveData.indoorCameraYawRadians;
    m_indoorCameraPitchRadians = saveData.indoorCameraPitchRadians;
    m_pendingMapMove = saveData.pendingMapMove;
}

bool GameSession::restoreTurnBasedCombatFromSaveData(const GameSaveData &saveData)
{
    // Rebind only after the world and projectiles exist: switching scenes ends the old turn runtime.
    const TurnBasedCombatRuntime::Snapshot &turn = saveData.turnBasedCombat;
    if (!TurnBasedCombatRuntime::isValidSnapshot(turn))
    {
        return false;
    }
    const size_t pending = turn.active && m_pActiveWorldRuntime != nullptr
        ? m_pActiveWorldRuntime->turnBasedPendingWorldActionCount() : 0;
    if ((turn.active && m_pActiveWorldRuntime == nullptr) || pending != static_cast<size_t>(turn.pendingActions))
    {
        return false;
    }
    m_turnBasedCombatRuntime.restoreSnapshot(turn);
    m_turnBasedPendingWorldActions = pending;
    m_turnBasedFrameTraceState = {};
    return true;
}

void GameSession::restoreHeldInventoryItemFromSaveData(const GameSaveData &saveData)
{
    GameplayUiController::HeldInventoryItemState &heldItem = m_gameplayUiController.heldInventoryItem();
    heldItem = {};
    if (saveData.heldInventoryItemActive && saveData.heldInventoryItem.objectDescriptionId != 0)
    {
        heldItem.active = true;
        heldItem.item = saveData.heldInventoryItem;
        heldItem.grabCellOffsetX = saveData.heldInventoryItemGrabCellOffsetX;
        heldItem.grabCellOffsetY = saveData.heldInventoryItemGrabCellOffsetY;
        heldItem.grabOffsetX = saveData.heldInventoryItemGrabOffsetX;
        heldItem.grabOffsetY = saveData.heldInventoryItemGrabOffsetY;
    }

    auto applyHeldItemQueryState =
        [&heldItem](Party &party)
        {
            if (heldItem.active)
            {
                party.setHeldItemForQueries(heldItem.item);
            }
            else
            {
                party.clearHeldItemForQueries();
            }
        };

    if (m_partyState)
    {
        applyHeldItemQueryState(*m_partyState);
    }

    Party *pRuntimeParty = m_pActiveWorldRuntime != nullptr ? m_pActiveWorldRuntime->party() : nullptr;
    if (pRuntimeParty != nullptr && (!m_partyState || pRuntimeParty != &*m_partyState))
    {
        applyHeldItemQueryState(*pRuntimeParty);
    }
}
}
