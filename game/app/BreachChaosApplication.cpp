#include "game/app/GameApplication.h"
#include "game/maps/MapIdentity.h"
#include "game/tables/PortraitEnums.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
using ChaosRequest = EventRuntimeState::PendingBreachAlvarChaosRequest;
constexpr uint32_t ChaosNpcId = 1092;
constexpr uint32_t ChaosHouseId = 712;

bool chaosFlag(const EventRuntimeState &events, const char *pName)
{
    const auto found = events.namedGlobalVars.find(std::string("MMerge.CrossContinents.") + pName);
    return found != events.namedGlobalVars.end() && found->second != 0;
}

bool chaosPartyMatches(const Party &party, const std::vector<uint64_t> &memberIds)
{
    if (party.memberCount() != memberIds.size() || memberIds.empty()) return false;
    for (size_t i = 0; i < memberIds.size(); ++i)
        if (memberIds[i] == 0 || party.members()[i].memberId != memberIds[i]) return false;
    return true;
}

bool hasChaosFollower(const std::vector<HiredNpcFollower> &followers)
{
    return std::any_of(followers.begin(), followers.end(),
        [](const HiredNpcFollower &follower) { return follower.npcId == ChaosNpcId; });
}
}

bool GameApplication::breachAlvarChaosRequestMatches(const ChaosRequest &request) const
{
    const auto &map = m_gameDataLoader.getSelectedMap();
    const EventRuntimeState *pEvents = m_gameplayController.eventRuntimeState();
    if (!map || !m_pMapSceneRuntime || !pEvents || m_pMapSceneRuntime->kind() != SceneKind::Outdoor
        || map->map.worldId != "mmmerge" || map->map.id != request.sourceMapId
        || map->map.canonicalId != request.sourceCanonicalMapId
        || normalizeMapFileStem(map->map.fileName) != "bralvar"
        || normalizeMapFileStem(request.sourceMapName) != "bralvar"
        || normalizeMapFileStem(m_gameSession.currentMapFileName()) != "bralvar"
        || !chaosPartyMatches(m_pMapSceneRuntime->party(), request.sourceMemberIds)
        || chaosFlag(*pEvents, "GotFQHint2") != request.expectedHint2
        || chaosFlag(*pEvents, "GotFQHint3") != request.expectedHint3
        || chaosFlag(*pEvents, "CoughtChaos") != request.expectedCaught) return false;
    if (request.kind == ChaosRequest::Kind::Return81)
    {
        return request.sourceEventId == 81 && request.expectedCaught
            && hasChaosFollower(pEvents->hiredNpcFollowers)
            && hasChaosFollower(m_pMapSceneRuntime->party().snapshot().hiredNpcFollowers);
    }
    if (request.sourceEventId != 5 || !m_pOutdoorWorldRuntime) return false;
    const double x = m_pOutdoorWorldRuntime->partyX();
    const double y = m_pOutdoorWorldRuntime->partyY();
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    const double key = std::ceil(std::round(x) / 500.0) + std::ceil(std::round(y) / 500.0);
    const auto found = pEvents->namedMapVars.find(BreachAlvarChaosDoorKeysPrefix + std::to_string(request.doorKey));
    const bool used = found != pEvents->namedMapVars.end() && found->second != 0;
    return key == request.doorKey && used == request.expectedDoorUsed;
}

void GameApplication::processPendingBreachAlvarChaosRequest()
{
    EventRuntimeState *pEvents = m_gameplayController.eventRuntimeState();
    if (!pEvents || !pEvents->pendingBreachAlvarChaosRequest) return;
    const ChaosRequest request = *pEvents->pendingBreachAlvarChaosRequest;
    pEvents->pendingBreachAlvarChaosRequest.reset();
    if (!breachAlvarChaosRequestMatches(request) || m_screenManager.activeScreen()
        || m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive
        || pEvents->pendingMapMove || pEvents->pendingDialogueContext || pEvents->pendingMovie
        || pEvents->pendingWinGame || pEvents->pendingInputPrompt || m_gameSession.pendingMapMove()) return;
    if (request.kind == ChaosRequest::Kind::Return81)
    {
        m_pendingBreachAlvarChaosReturn = request;
        EventRuntimeState::PendingMapMove move;
        move.mapName = "Breach.odm";
        move.x = -841;
        move.y = -475;
        move.z = 4501;
        move.directionDegrees = 176.0f * 360.0f / 2048.0f;
        move.lookAngleDegrees = 0;
        move.traceSourceKind = "breach_alvar_chaos_return";
        move.traceEventId = 81;
        move.traceDestinationName = "Breach.odm";
        pEvents->pendingMapMove = std::move(move);
        return;
    }

    // Prepare the entire source event before publishing any RNG, NPC or door-key change.
    EventRuntimeState candidate = *pEvents;
    const auto draw = [&](uint32_t minimum, uint32_t maximum)
    {
        return minimum + EventRuntime::randomValue(candidate, 5) % (maximum - minimum + 1);
    };
    if (!request.expectedHint3)
    {
        const auto &names = m_gameDataLoader.getMergedNpcNameTable().maleNames();
        const auto &professions = m_gameDataLoader.getMergedNpcProfessionTable().entries();
        if (names.empty() || professions.size() < 3) return;
        for (size_t i = 0; i < professions.size(); ++i)
            if (professions[i].id != i) return;
        candidate.npcPictureOverrides[ChaosNpcId] = draw(68, 438);
        candidate.npcNameOverrides[ChaosNpcId] = names[draw(0, uint32_t(names.size() - 1))];
        // Source NPCProfessions is a plain Lua table indexed 0..N; random upper bound is #table - 1.
        candidate.npcProfessionOverrides[ChaosNpcId] = draw(1, uint32_t(professions.size() - 2));
    }
    const bool locked = !request.expectedHint2 || request.expectedCaught || request.expectedDoorUsed;
    uint32_t lockedReaction = 0;
    if (locked)
    {
        lockedReaction = draw(1, 3);
        if (lockedReaction != 3)
        {
            EventRuntimeState::PendingSound sound;
            sound.soundId = lockedReaction == 1 ? 420 : 438;
            candidate.pendingSounds.push_back(std::move(sound));
        }
    }
    else
    {
        candidate.namedMapVars[BreachAlvarChaosDoorKeysPrefix + std::to_string(request.doorKey)] = 1;
        candidate.npcHouseOverrides[ChaosNpcId] = ChaosHouseId;
        candidate.messages.clear();
        candidate.dialogueState.hostHouseId = ChaosHouseId;
        EventRuntimeState::PendingDialogueContext dialog;
        dialog.kind = DialogueContextKind::HouseService;
        dialog.sourceId = ChaosHouseId;
        dialog.hostHouseId = ChaosHouseId;
        dialog.mapNoteSourcePoint = candidate.activeEventMapNoteSourcePoint;
        candidate.pendingDialogueContext = std::move(dialog);
    }
    const EventRuntimeState before = *pEvents;
    Party &party = m_pMapSceneRuntime->party();
    const Party::Snapshot partyBefore = party.snapshot();
    try
    {
        *pEvents = std::move(candidate);
        if (!locked) party.setNpcHouseOverride(ChaosNpcId, ChaosHouseId);
        if (IGameplayWorldRuntime *pWorld = m_gameSession.activeWorldRuntime()) pWorld->applyEventRuntimeState(true);
        if (lockedReaction == 3)
            m_gameSession.gameplayScreenRuntime().triggerPortraitFaceAnimation(
                party.activeMemberIndex(), FaceAnimationId::DoorLocked);
        if (!locked) m_gameSession.gameplayScreenRuntime().presentPendingEventDialog(before.messages.size(), true);
        synchronizeSessionFromRuntime();
    }
    catch (const std::exception &failure)
    {
        *pEvents = before;
        party.restoreSnapshot(partyBefore);
        if (IGameplayWorldRuntime *pWorld = m_gameSession.activeWorldRuntime()) pWorld->applyEventRuntimeState(true);
        std::cerr << "Chaos door transaction failed: " << failure.what() << '\n';
        reportQuickSaveStatus("无法进入房屋，请重试。");
    }
}

bool GameApplication::commitBreachAlvarChaosReturn(
    const EventRuntimeState::PendingMapMove &move, bool initializeView)
{
    if (!m_pendingBreachAlvarChaosReturn) return false;
    const ChaosRequest request = *m_pendingBreachAlvarChaosReturn;
    m_pendingBreachAlvarChaosReturn.reset();
    if (request.kind != ChaosRequest::Kind::Return81 || move.traceEventId != 81
        || !breachAlvarChaosRequestMatches(request)) return false;
    synchronizeSessionFromRuntime();
    const auto backup = m_gameSession.buildSaveData();
    if (!backup) return false;
    try
    {
        const bool arrived = applyMapMove(move, initializeView, [&]()
        {
            const auto &map = m_gameDataLoader.getSelectedMap();
            EventRuntimeState *pArrivingEvents = m_gameplayController.eventRuntimeState();
            if (!map || map->map.worldId != "mmmerge" || normalizeMapFileStem(map->map.fileName) != "breach"
                || !m_pMapSceneRuntime || !pArrivingEvents
                || !chaosPartyMatches(m_pMapSceneRuntime->party(), request.sourceMemberIds)
                || !chaosFlag(*pArrivingEvents, "CoughtChaos")
                || !hasChaosFollower(m_pMapSceneRuntime->party().snapshot().hiredNpcFollowers))
                throw std::runtime_error("Chaos return lost its source party or follower");
            Party &party = m_pMapSceneRuntime->party();
            party.removeHiredNpcFollower(ChaosNpcId);
            party.setNpcUnavailable(ChaosNpcId, false);
            std::erase_if(pArrivingEvents->hiredNpcFollowers,
                [](const HiredNpcFollower &follower) { return follower.npcId == ChaosNpcId; });
            pArrivingEvents->unavailableNpcIds.erase(ChaosNpcId);
            for (size_t i = 0; i < party.memberCount(); ++i) party.addExperienceToMember(i, 100000);
        });
        if (!arrived) throw std::runtime_error("Cannot load the Breach return map");
        return true;
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Chaos return transaction failed: " << failure.what() << '\n';
        shutdownRenderer();
        m_gameSession.restoreFromSaveData(*backup);
        const bool previousLoading = m_loadingSavedGameRuntime;
        m_loadingSavedGameRuntime = true;
        bool restored = loadCurrentSessionMap(initializeView) && applyCurrentSessionToRuntime();
        if (restored) restored = m_gameSession.restoreTurnBasedCombatFromSaveData(*backup);
        if (restored) m_gameSession.restoreHeldInventoryItemFromSaveData(*backup);
        m_loadingSavedGameRuntime = previousLoading;
        if (!restored) std::cerr << "Chaos return rollback could not restore scene\n";
        cancelLoadingOverlay();
        reportQuickSaveStatus("无法返回裂隙，请重试。");
        return false;
    }
}
}
