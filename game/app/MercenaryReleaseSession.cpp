#include "game/app/GameSession.h"

#include <exception>
#include <type_traits>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
void clearReleasedRecruit(EventRuntimeState &events, uint32_t npcId, uint32_t rosterId)
{
    events.generatedMercenaryRecruitsByNpcId.erase(npcId);
    events.npcHouseOverrides[npcId] = 0;
    events.unavailableNpcIds.erase(npcId);
    events.npcNameOverrides.erase(npcId);
    events.npcPictureOverrides.erase(npcId);
    events.npcProfessionOverrides.erase(npcId);
    if (events.dialogueState.currentOffer
        && (events.dialogueState.currentOffer->npcId == npcId
            || events.dialogueState.currentOffer->rosterId == rosterId))
        events.dialogueState.currentOffer.reset();
    if (events.dialogueState.ownedMercenaryOffer
        && events.dialogueState.ownedMercenaryOffer->binding.npcId == npcId)
        events.dialogueState.ownedMercenaryOffer.reset();
    if (events.dialogueState.pendingOwnedMercenaryJoin
        && events.dialogueState.pendingOwnedMercenaryJoin->binding.npcId == npcId)
        events.dialogueState.pendingOwnedMercenaryJoin.reset();
    if (events.pendingDialogueContext && events.pendingDialogueContext->sourceId == npcId)
        events.pendingDialogueContext.reset();
    // Map refill timing and weak Breach actor bindings deliberately survive this operation.
}
}

bool GameSession::releaseAdventurersInnMember(uint64_t memberId, uint32_t sourceRosterId, std::string &error)
{
    error.clear();
    try
    {
        IGameplayWorldRuntime *pWorld = activeWorldRuntime();
        if (!hasDataRepository() || !data().mercenaryReleaseProfile() || pWorld == nullptr
            || pWorld->boundGameSession() != this || pWorld->party() == nullptr
            || pWorld->eventRuntimeState() == nullptr)
        {
            error = "Permanent release requires the current world and authenticated source policy";
            return false;
        }
        Party &party = *pWorld->party();
        EventRuntimeState &events = *pWorld->eventRuntimeState();
        const auto identity = m_gameplayScreenRuntime.selectedAdventurersInnReleaseIdentity();
        if (!identity || identity->memberId != memberId || identity->sourceRosterId != sourceRosterId
            || m_pendingMapMove || events.pendingMapMove || events.activeHookContext
            || events.pendingMovie || events.pendingCampaignRestartMap || events.pendingReturnToMainMenu)
        {
            error = "The Inn release selection or current scene changed";
            return false;
        }
        const MercenaryReleaseProfile profile = *data().mercenaryReleaseProfile();
        Party candidateParty = party;
        if (!candidateParty.releaseAdventurersInnMember(memberId, sourceRosterId, profile, error)) return false;
        const uint32_t npcId = profile.npcRosterOffset + sourceRosterId;
        EventRuntimeState candidateEvents = events;
        auto candidateOutdoor = m_currentOutdoorWorldState;
        auto candidateOutdoorCache = m_outdoorWorldStates;
        auto candidateIndoor = m_currentIndoorSceneState;
        auto candidateIndoorCache = m_indoorSceneStates;
        auto candidateMaze = m_breachMazeRunData;
        clearReleasedRecruit(candidateEvents, npcId, sourceRosterId);
        const auto project = [&](auto &snapshot)
        {
            if (snapshot.eventRuntimeState)
                clearReleasedRecruit(*snapshot.eventRuntimeState, npcId, sourceRosterId);
        };
        if (candidateOutdoor) project(*candidateOutdoor);
        if (candidateIndoor) project(*candidateIndoor);
        for (auto &[key, snapshot] : candidateOutdoorCache) project(snapshot);
        for (auto &[key, snapshot] : candidateIndoorCache) project(snapshot);
        if (candidateMaze)
            for (auto &[level, floor] : candidateMaze->floors) project(floor.runtime.scene);
        std::optional<Party> candidatePartyCache = candidateParty;

        // No callbacks run between preparation and these swaps. Recheck the exact UI identity,
        // then publish only the prepared Party and targeted NPC projections without allocations.
        if (activeWorldRuntime() != pWorld || pWorld->party() != &party
            || pWorld->eventRuntimeState() != &events
            || m_gameplayScreenRuntime.selectedAdventurersInnReleaseIdentity() != identity)
        {
            error = "Permanent release became stale before publication";
            return false;
        }
        static_assert(std::is_nothrow_swappable_v<Party>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_partyState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_currentOutdoorWorldState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_outdoorWorldStates)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_currentIndoorSceneState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_indoorSceneStates)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_breachMazeRunData)>);
        using std::swap;
        swap(party, candidateParty);
        if (!m_partyState || &*m_partyState != &party) swap(m_partyState, candidatePartyCache);
        swap(m_currentOutdoorWorldState, candidateOutdoor);
        swap(m_outdoorWorldStates, candidateOutdoorCache);
        swap(m_currentIndoorSceneState, candidateIndoor);
        swap(m_indoorSceneStates, candidateIndoorCache);
        swap(m_breachMazeRunData, candidateMaze);
        events.generatedMercenaryRecruitsByNpcId.swap(candidateEvents.generatedMercenaryRecruitsByNpcId);
        events.npcHouseOverrides.swap(candidateEvents.npcHouseOverrides);
        events.unavailableNpcIds.swap(candidateEvents.unavailableNpcIds);
        events.npcNameOverrides.swap(candidateEvents.npcNameOverrides);
        events.npcPictureOverrides.swap(candidateEvents.npcPictureOverrides);
        events.npcProfessionOverrides.swap(candidateEvents.npcProfessionOverrides);
        events.dialogueState.currentOffer.swap(candidateEvents.dialogueState.currentOffer);
        events.dialogueState.ownedMercenaryOffer.swap(candidateEvents.dialogueState.ownedMercenaryOffer);
        events.dialogueState.pendingOwnedMercenaryJoin.swap(candidateEvents.dialogueState.pendingOwnedMercenaryJoin);
        events.pendingDialogueContext.swap(candidateEvents.pendingDialogueContext);
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}
}
