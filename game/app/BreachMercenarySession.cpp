#include "game/app/GameSession.h"

#include "game/StringUtils.h"
#include "game/app/BreachAlvarSessionSupport.h"
#include "game/gameplay/OwnedMercenaryRuntime.h"
#include "game/maps/MapIdentity.h"

#include <algorithm>
#include <exception>
#include <type_traits>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
using Offer = EventRuntimeState::OwnedMercenaryJoinOffer;
using Result = OwnedMercenarySessionResult;

bool isMap(const std::string &key, const char *file)
{
    const std::string lower = toLowerCopy(key);
    return lower == file || lower == std::string("mmmerge:") + file
        || lower == buildCanonicalMapId("mmmerge", file);
}

bool matches(const GameplayOwnedMercenaryActorTarget &target, const Offer &offer)
{
    return target.actorInteractable && target.binding == offer.binding
        && target.canonicalMapKey == offer.canonicalMapKey && target.profileVersion == offer.profileVersion
        && target.sourceSha256 == offer.sourceSha256 && target.sceneGeneration == offer.sceneGeneration
        && target.sourceActorIndex == offer.sourceActorIndex;
}

// Publish only the NPC fields changed by the prepared Party operation. Other map-local outputs retain ownership.
void projectJoin(EventRuntimeState &events, const PreparedOwnedMercenaryJoin &mutation)
{
    const uint32_t npcId = mutation.npcId();
    events.npcHouseOverrides[npcId] = 0;
    std::erase_if(events.hiredNpcFollowers, [npcId](const HiredNpcFollower &follower)
        { return follower.npcId == npcId; });
    if (mutation.party().snapshot().unavailableNpcIds.contains(npcId)) events.unavailableNpcIds.insert(npcId);
    else events.unavailableNpcIds.erase(npcId);
    if (mutation.companionReturn()) applyBreachAlvarNpcProjection(events, mutation.companionReturn()->npcs, false);
}
}

bool GameSession::bindBreachMercenaryRuntime(bool coldLoad, std::string &error)
{
    error.clear();
    if (!isMap(m_currentMapFileName, "breach.odm")) return true;
    try
    {
        auto *pWorld = dynamic_cast<OutdoorWorldRuntime *>(activeWorldRuntime());
        if (!hasDataRepository() || pWorld == nullptr || !pWorld->isInitialized()
            || pWorld->boundGameSession() != this || pWorld->party() == nullptr)
        {
            error = "Breach mercenaries require the current initialized outdoor world";
            return false;
        }
        const auto &profile = data().breachMercenaryProfile();
        // Older complete content packages precede this source feature. A saved projection still requires its profile.
        if (!profile)
        {
            if (!pWorld->snapshot().breachMercenaries) return true;
            error = "Saved Breach mercenaries require their source content profile";
            return false;
        }
        if (coldLoad) return pWorld->bindBreachMercenaryProjection(*profile, *pWorld->party(), error);
        auto prepared = pWorld->prepareBreachMercenaryProjection(
            *pWorld->party(), *profile, data().characterDollTable(), error);
        if (!prepared) return false;
        if (activeWorldRuntime() != pWorld || pWorld->boundGameSession() != this
            || !isMap(m_currentMapFileName, "breach.odm")
            || !pWorld->publishPreparedBreachMercenaryActors(std::move(*prepared)))
        {
            error = "Breach population preparation became stale before publication";
            return false;
        }
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

OwnedMercenarySessionResult GameSession::processOwnedMercenaryJoinRequest(const Offer &queuedOffer)
{
    try
    {
        const Offer offer = queuedOffer;
        auto *pWorld = dynamic_cast<OutdoorWorldRuntime *>(activeWorldRuntime());
        if (!hasDataRepository() || pWorld == nullptr || !pWorld->isInitialized()
            || pWorld->boundGameSession() != this || currentSceneKind() != SceneKind::Outdoor
            || !isMap(m_currentMapFileName, "breach.odm") || pWorld->party() == nullptr
            || pWorld->eventRuntimeState() == nullptr || !data().breachMercenaryProfile())
            return {Result::Code::Rejected, "Owned mercenary request has no current Breach context"};
        OutdoorWorldRuntime &world = *pWorld;
        Party &party = *world.party();
        EventRuntimeState &events = *world.eventRuntimeState();
        const BreachMercenaryProfile profile = *data().breachMercenaryProfile();
        const auto &maps = data().mapEntries();
        if (std::none_of(maps.begin(), maps.end(), [&](const MapStatsEntry &entry)
            {
                return entry.id == world.mapId() && entry.name == world.mapName()
                    && toLowerCopy(entry.worldId) == "mmmerge"
                    && toLowerCopy(entry.canonicalId) == buildCanonicalMapId("mmmerge", "breach.odm")
                    && isMap(entry.fileName, "breach.odm");
            }) || !isMap(events.mapFileName, "breach.odm"))
            return {Result::Code::Rejected, "Owned mercenary world is not the namespaced source map"};
        const auto validOffer = [&]()
        {
            const EventDialogContent &dialog = m_gameplayUiController.eventDialog().content;
            const auto target = world.ownedMercenaryActorBinding(offer.binding.npcId, offer.sourceActorIndex);
            return events.dialogueState.pendingOwnedMercenaryJoin == offer
                && events.dialogueState.ownedMercenaryOffer == offer && target && matches(*target, offer)
                && offer.profileVersion == profile.version && offer.sourceSha256 == profile.sourceSha256
                && offer.canonicalMapKey == profile.canonicalMapKey
                && dialog.isActive && !dialog.isHouseDialog && dialog.sourceId == offer.binding.npcId
                && dialog.sourceActorIndex == offer.sourceActorIndex && events.dialogueState.hostHouseId == 0
                && std::any_of(dialog.actions.begin(), dialog.actions.end(), [](const EventDialogAction &action)
                    { return action.enabled && action.kind == EventDialogActionKind::OwnedMercenaryJoinAccept; });
        };
        if (!validOffer()) return {Result::Code::Rejected, "Owned mercenary confirmation became stale"};
        const auto safe = [&]()
        {
            const auto &ui = m_gameplayUiController.state();
            return world.m_pPartyRuntime != nullptr && &world.m_pPartyRuntime->party() == &party
                && world.gameMinutes() == m_gameMinutes
                && !m_turnBasedCombatRuntime.active() && !m_turnBasedCombatRuntime.hasPendingActions()
                && m_turnBasedPendingWorldActions == 0 && world.turnBasedPendingWorldActionCount() == 0
                && !world.turnBasedActorActionInProgress() && !world.m_actorAiUpdateQueued
                && m_gameplayCombatController.pendingCombatEvents().empty() && world.pendingCombatEvents().empty()
                && m_gameplayProjectileService.projectileCount() == 0 && world.projectileCount() == 0
                && world.fireSpikeTrapCount() == 0 && !world.m_armageddonState.active()
                && !m_gameplayScreenState.pendingSpellTarget().active && !ui.utilitySpellOverlay.active
                && !ui.characterScreen.open && !ui.spellbook.active && !ui.itemInspectOverlay.active
                && !ui.characterInspectOverlay.active && !ui.characterDetailOverlay.active
                && !ui.buffInspectOverlay.active && !ui.spellInspectOverlay.active
                && !ui.restScreen.active && !ui.menuScreen.active && !ui.saveGameScreen.active
                && !ui.loadGameScreen.active && !ui.inventoryNestedOverlay.active && !ui.houseShopOverlay.active
                && !ui.houseBankState.inputActive() && !ui.readableScrollOverlay.active
                && !m_pendingMapMove && !events.pendingMapMove && !events.pendingMovie && !events.pendingWinGame
                && !events.pendingReturnToMainMenu && !events.pendingCampaignRestartMap
                && !events.pendingDelayedMapMove && !events.pendingInputPrompt && !events.pendingArcomageGame
                && !events.activeHookContext && !events.clearHeldItemRequest && !events.pendingCloseNpcDialogue
                && !events.pendingDimensionDoorOverlay && events.pendingDimensionDoorActivations == 0
                && !events.pendingBreachAlvarCompanionRequest && !events.pendingBreachAlvarChaosRequest
                && events.grantedItems.empty() && events.grantedItemIds.empty() && events.removedItemIds.empty()
                && events.portraitFxRequests.empty() && events.spellFxRequests.empty()
                && party.pendingAudioRequests().empty();
        };
        if (!safe()) return {Result::Code::Unsafe, "Owned mercenary return requires an idle gameplay point"};

        std::string error;
        const auto facts = resolveBreachAlvarCompanionTopicFacts(*this, events, error);
        if (!facts) return {Result::Code::Rejected, std::move(error)};
        const auto beforeReceipt = m_breachAlvarCompanions;
        const auto beforeGlobals = m_namedGlobalVars;
        const auto beforeHeld = m_gameplayUiController.heldInventoryItem();
        const auto beforeHeldBytes = encodeInventoryItemForComparison(beforeHeld.item);
        if (party.heldItemIdForQueries() != (beforeHeld.active ? beforeHeld.item.objectDescriptionId : 0))
            return {Result::Code::Rejected, "Held item query differs from its UI owner"};
        const double beforeTime = m_gameMinutes;
        const std::string beforeMap = m_currentMapFileName;
        auto preparation = prepareOwnedMercenaryJoin(party,
            {offer.binding.memberId, offer.binding.sourceRosterId, offer.owner}, profile,
            m_breachAlvarCompanions, data().characterDollTable(), *facts);
        if (preparation.code == OwnedMercenaryJoinPreparationCode::Full) return {Result::Code::Full, {}};
        if (!preparation.mutation) return {Result::Code::Rejected, std::move(preparation.error)};
        PreparedOwnedMercenaryJoin &mutation = *preparation.mutation;
        const auto target = world.ownedMercenaryActorBinding(offer.binding.npcId, offer.sourceActorIndex);
        if (!target || !matches(*target, offer))
            return {Result::Code::Rejected, "Owned mercenary actor changed before preparation"};
        auto actor = world.prepareBreachMercenaryRemoval(*target, error);
        if (!actor) return {Result::Code::Rejected, std::move(error)};
        auto candidateWorld = world.snapshotWithPreparedBreachMercenaryActors(*actor, error);
        if (!candidateWorld) return {Result::Code::Rejected, std::move(error)};
        auto candidateReceipt = beforeReceipt;
        const auto &returned = mutation.companionReturn();
        if (returned) candidateReceipt = BreachAlvarCompanionState{returned->receipt, returned->existingFriendActor};
        EventRuntimeState candidateEvents = events;
        projectJoin(candidateEvents, mutation);
        candidateWorld->eventRuntimeState = candidateEvents;
        clearTransientEventRuntimeState(*candidateWorld->eventRuntimeState);
        auto candidateOutdoorCache = m_outdoorWorldStates;
        auto candidateIndoorCache = m_indoorSceneStates;
        auto candidateIndoor = m_currentIndoorSceneState;
        const auto &companionProfile = data().breachAlvarCompanionProfile();
        if (returned && !companionProfile)
            return {Result::Code::Rejected, "Cross-map companion return requires its original content profile"};
        for (auto &[key, snapshot] : candidateOutdoorCache)
        {
            if (isMap(key, "breach.odm")) snapshot = *candidateWorld;
            if (returned && returned->npcId == 772 && isMap(key, "bralvar.odm"))
            {
                auto removed = OutdoorWorldRuntime::snapshotWithRemovedBreachAlvarFriend(
                    snapshot, returned->existingFriendActor, companionProfile->friendActor, error);
                if (!removed) return {Result::Code::Rejected, std::move(error)};
                snapshot = std::move(*removed);
            }
            if (snapshot.eventRuntimeState) projectJoin(*snapshot.eventRuntimeState, mutation);
        }
        candidateOutdoorCache[beforeMap] = *candidateWorld;
        candidateOutdoorCache[buildCanonicalMapId("mmmerge", "breach.odm")] = *candidateWorld;
        for (auto &[key, snapshot] : candidateIndoorCache)
            if (snapshot.eventRuntimeState) projectJoin(*snapshot.eventRuntimeState, mutation);
        if (candidateIndoor && candidateIndoor->eventRuntimeState)
            projectJoin(*candidateIndoor->eventRuntimeState, mutation);
        std::optional<Party> candidatePartyCache = mutation.party();
        std::optional<OutdoorPartyRuntime::Snapshot> candidateMovement = world.m_pPartyRuntime->snapshot();
        GameSaveData prospective;
        prospective.currentSceneKind = SceneKind::Outdoor;
        prospective.mapFileName = beforeMap;
        prospective.party = mutation.party().snapshot();
        prospective.breachAlvarCompanions = candidateReceipt;
        prospective.namedGlobalVars = beforeGlobals;
        prospective.savedGameMinutes = beforeTime;
        prospective.hasOutdoorRuntimeState = true;
        prospective.outdoorParty = *candidateMovement;
        prospective.outdoorWorld = *candidateWorld;
        prospective.outdoorWorldStates = candidateOutdoorCache;
        prospective.indoorSceneStates = candidateIndoorCache;
        if (candidateIndoor)
        {
            prospective.hasIndoorSceneState = true;
            prospective.indoorScene = *candidateIndoor;
        }
        if (!validateSavedBreachAlvarCompanions(prospective, error,
                companionProfile ? &companionProfile->friendActor : nullptr)
            || !validateSavedBreachMercenaries(prospective, error, &profile, true))
            return {Result::Code::Rejected, std::move(error)};

        const auto &held = m_gameplayUiController.heldInventoryItem();
        if (activeWorldRuntime() != &world || world.boundGameSession() != this || world.party() != &party
            || world.eventRuntimeState() != &events || m_currentMapFileName != beforeMap || m_gameMinutes != beforeTime
            || m_breachAlvarCompanions != beforeReceipt || m_namedGlobalVars != beforeGlobals
            || !data().breachMercenaryProfile() || *data().breachMercenaryProfile() != profile
            || held.active != beforeHeld.active || held.grabCellOffsetX != beforeHeld.grabCellOffsetX
            || held.grabCellOffsetY != beforeHeld.grabCellOffsetY || held.grabOffsetX != beforeHeld.grabOffsetX
            || held.grabOffsetY != beforeHeld.grabOffsetY
            || encodeInventoryItemForComparison(held.item) != beforeHeldBytes
            || !validOffer() || !safe() || !world.isPreparedBreachMercenaryActorCurrent(*actor))
            return {Result::Code::Rejected, "Owned mercenary preparation became stale before publication"};

        static_assert(std::is_nothrow_swappable_v<Party>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_partyState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_currentOutdoorWorldState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_outdoorWorldStates)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_indoorSceneStates)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_currentIndoorSceneState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_breachAlvarCompanions)>);
        static_assert(noexcept(std::declval<OutdoorPartyRuntime &>().syncSpellMovementStatesFromPartyBuffs()));
        GameplayOverlayInteractionState clearedInteraction;
        const auto publish = [&]() noexcept
        {
            // The actor token still sees the original Party. No fallible operation follows its publication.
            if (!world.publishPreparedBreachMercenaryActors(std::move(*actor))) return false;
            using std::swap;
            swap(party, mutation.m_party);
            if (!m_partyState || &*m_partyState != &party) swap(m_partyState, candidatePartyCache);
            swap(m_breachAlvarCompanions, candidateReceipt);
            swap(m_currentOutdoorWorldState, candidateWorld);
            swap(m_outdoorWorldStates, candidateOutdoorCache);
            swap(m_currentIndoorSceneState, candidateIndoor);
            swap(m_indoorSceneStates, candidateIndoorCache);
            swap(m_outdoorPartyState, candidateMovement);
            events.npcHouseOverrides.swap(candidateEvents.npcHouseOverrides);
            events.npcGreetingOverrides.swap(candidateEvents.npcGreetingOverrides);
            events.npcTopicOverrides.swap(candidateEvents.npcTopicOverrides);
            events.npcNameOverrides.swap(candidateEvents.npcNameOverrides);
            events.npcPictureOverrides.swap(candidateEvents.npcPictureOverrides);
            events.hiredNpcFollowers.swap(candidateEvents.hiredNpcFollowers);
            events.unavailableNpcIds.swap(candidateEvents.unavailableNpcIds);
            events.dialogueState.ownedMercenaryOffer.reset();
            events.dialogueState.pendingOwnedMercenaryJoin.reset();
            events.pendingDialogueContext.reset();
            events.pendingCloseNpcDialogue = true;
            swap(m_overlayInteractionState, clearedInteraction);
            world.m_pPartyRuntime->syncSpellMovementStatesFromPartyBuffs();
            *m_outdoorPartyState = world.m_pPartyRuntime->snapshot();
            return true;
        };
        if (!publish()) return {Result::Code::Rejected, "Owned mercenary world refused stale publication"};
        return {Result::Code::Committed, {}};
    }
    catch (const std::exception &exception)
    {
        return {Result::Code::Rejected, exception.what()};
    }
}
}
