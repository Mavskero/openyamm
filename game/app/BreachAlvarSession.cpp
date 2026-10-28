#include "game/app/GameSession.h"
#include "game/app/BreachAlvarSessionSupport.h"

#include "game/StringUtils.h"
#include "game/content/BreachAlvarCompanionProfile.h"
#include "game/gameplay/BreachAlvarCompanionTransaction.h"
#include "game/gameplay/SourceMercenaryGenerationRuntime.h"
#include "game/maps/MapIdentity.h"

#include <algorithm>
#include <array>
#include <exception>
#include <type_traits>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *QuestPrefix = "MMerge.CrossContinents.";
constexpr const char *SplitFlag = "MMerge.CrossContinents.BreachSplit";
constexpr const char *PortraitHistoryPrefix = "MMerge.Mercenary.LastPortraits.";
using Request = EventRuntimeState::PendingBreachAlvarCompanionRequest;
using Result = BreachAlvarSessionResult;
using ActorToken = OutdoorWorldRuntime::PreparedBreachAlvarFriendActorMutation;

bool isBreachMapName(const std::string &name)
{
    const std::string lower = toLowerCopy(name);
    return lower == "bralvar.odm" || lower == BreachAlvarCompanionMap
        || lower == buildCanonicalMapId("mmmerge", "bralvar.odm");
}

struct RuntimeContext
{
    OutdoorWorldRuntime *pWorld = nullptr;
    Party *pParty = nullptr;
    EventRuntimeState *pEvents = nullptr;
    const BreachAlvarCompanionProfile *pProfile = nullptr;
};

std::optional<RuntimeContext> runtimeContext(const GameSession &session, std::string &error)
{
    error.clear();
    auto *pWorld = dynamic_cast<OutdoorWorldRuntime *>(session.activeWorldRuntime());
    if (!session.hasDataRepository() || pWorld == nullptr || !pWorld->isInitialized()
        || pWorld->boundGameSession() != &session || session.currentSceneKind() != SceneKind::Outdoor
        || !isBreachMapName(session.currentMapFileName())
        || pWorld->party() == nullptr || pWorld->eventRuntimeState() == nullptr)
    {
        error = "Breach Alvar request has no current initialized outdoor Session context";
        return std::nullopt;
    }
    const auto &profile = session.data().breachAlvarCompanionProfile();
    if (!profile || !validBreachAlvarCompanionProfile(*profile, error))
    {
        if (error.empty())
        {
            error = "Breach Alvar companion content is not loaded";
        }
        return std::nullopt;
    }
    const auto &maps = session.data().mapEntries();
    const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapStatsEntry &entry)
    {
        return entry.id == pWorld->mapId() && entry.name == pWorld->mapName()
            && toLowerCopy(entry.worldId) == "mmmerge"
            && toLowerCopy(entry.canonicalId) == buildCanonicalMapId("mmmerge", "bralvar.odm")
            && isBreachMapName(entry.fileName);
    });
    if (map == maps.end() || !isBreachMapName(pWorld->eventRuntimeState()->mapFileName))
    {
        error = "Breach Alvar world is not the verified namespaced content map";
        return std::nullopt;
    }
    return RuntimeContext{pWorld, pWorld->party(), pWorld->eventRuntimeState(), &*profile};
}

int32_t globalValue(const GameSession &session, const EventRuntimeState &state, const std::string &name)
{
    const auto found = state.namedGlobalVars.find(name);
    return found == state.namedGlobalVars.end() ? session.namedGlobalVar(name) : found->second;
}

}

std::optional<BreachAlvarCompanionTopicFacts> resolveBreachAlvarCompanionTopicFacts(
    const GameSession &session, const EventRuntimeState &state, std::string &error)
{
    BreachAlvarCompanionTopicFacts facts;
    facts.questFinished = globalValue(session, state, std::string(QuestPrefix) + "QuestFinished") != 0;
    std::array<bool, 5> hints{};
    uint32_t count = 0;
    for (size_t index = 0; index < facts.hintByNpc.size(); ++index)
    {
        const std::string npc = std::to_string(772 + index);
        const int32_t hint = globalValue(session, state, std::string(QuestPrefix) + "HintByNPC." + npc);
        const int32_t generated = session.breachAlvarCompanions()
            && session.breachAlvarCompanions()->receipt.companions[index].generation ? 1 : 0;
        if (hint < 0 || hint > 4 || (hint != 0 && hints[size_t(hint)])
            || globalValue(session, state, std::string(QuestPrefix) + "FQMercRnd." + npc) != generated)
        {
            error = "Invalid Breach Alvar hint identity or generated-mercenary receipt state";
            return std::nullopt;
        }
        facts.hintByNpc[index] = uint32_t(hint);
        if (hint != 0)
        {
            hints[size_t(hint)] = true;
            ++count;
        }
    }
    if (globalValue(session, state, std::string(QuestPrefix) + "GotFQHints") != int32_t(count))
    {
        error = "Breach Alvar hint count does not match its NPC assignments";
        return std::nullopt;
    }
    for (size_t index = 1; index <= 4; ++index)
    {
        const bool stored = globalValue(session, state,
            std::string(QuestPrefix) + "GotFQHint" + std::to_string(index)) != 0;
        if (hints[index] != (index <= count) || stored != hints[index])
        {
            error = "Breach Alvar hint sequence is not the original contiguous encounter order";
            return std::nullopt;
        }
    }
    return facts;
}

namespace
{
bool validLiveReceipt(const GameSession &session, const RuntimeContext &context, std::string &error)
{
    if (!validBreachAlvarCompanionState(session.breachAlvarCompanions(), context.pParty->snapshot(), error))
    {
        return false;
    }
    const int32_t expected = session.breachAlvarCompanions() ? 1 : 0;
    if (globalValue(session, *context.pEvents, SplitFlag) != expected
        || (session.breachAlvarCompanions() && session.breachAlvarCompanions()->receipt.initialActiveCount > 1
            && !context.pParty->hasQuestBit(1714)
            && globalValue(session, *context.pEvents, std::string(QuestPrefix) + "QuestFinished") != 1))
    {
        error = "Breach Alvar compatibility flag or quest bit disagrees with its typed receipt";
        return false;
    }
    return true;
}

bool validReturnDialogue(const GameSession &session, const RuntimeContext &context,
    const Request &request, std::string &error)
{
    const EventDialogContent &dialog = session.gameplayUiController().eventDialog().content;
    if (request.sourceEventId != 1789 || !request.executingGlobalHandler
        || request.dialogueKind != DialogueContextKind::NpcTalk || request.sourceNpcId != request.npcId
        || request.npcId < 772 || request.npcId > 775 || !dialog.isActive || dialog.isHouseDialog
        || dialog.sourceId != request.npcId || dialog.sourceActorIndex != request.sourceActorIndex
        || context.pEvents->dialogueState.hostHouseId != request.hostHouseId)
    {
        error = "Breach Alvar return did not originate from the current companion dialogue";
        return false;
    }
    const bool hasJoinAction = std::any_of(dialog.actions.begin(), dialog.actions.end(), [](const auto &action)
    {
        return action.enabled && !action.textOnly
            && action.kind == EventDialogActionKind::NpcTopic && action.id == 1789;
    });
    if (!hasJoinAction)
    {
        error = "Current Breach Alvar dialogue has no available Join action";
        return false;
    }
    if (request.npcId == 772)
    {
        if (request.hostHouseId != 0 || !request.sourceActorIndex || !request.stableActorId
            || !session.breachAlvarCompanions())
        {
            error = "Breach Alvar street return has no captured actor identity";
            return false;
        }
        const auto *pActor = context.pWorld->mapActorState(*request.sourceActorIndex);
        const auto &binding = session.breachAlvarCompanions()->friendActor;
        if (pActor == nullptr || pActor->actorId != *request.stableActorId || pActor->actorId != binding.actorId
            || pActor->monsterId != int16_t(binding.spawnMonsterId) || pActor->npcId != 772
            || !pActor->breachAlvarCompanion || pActor->breachAlvarCompanionRemoved
            || pActor->isInvisible || pActor->currentHp <= 0 || pActor->isDead
            || pActor->aiState == OutdoorWorldRuntime::ActorAiState::Dying
            || pActor->aiState == OutdoorWorldRuntime::ActorAiState::Dead)
        {
            error = "Breach Alvar street actor has changed or is no longer an interactable companion";
            return false;
        }
    }
    else
    {
        const uint32_t expectedHouse = context.pProfile->houseIds[request.npcId - 772];
        const Party::Snapshot party = context.pParty->snapshot();
        const auto house = party.npcHouseOverrides.find(request.npcId);
        if (request.sourceActorIndex || request.stableActorId || request.hostHouseId != expectedHouse
            || session.data().houseTable().get(expectedHouse) == nullptr
            || house == party.npcHouseOverrides.end() || house->second != expectedHouse)
        {
            error = "Breach Alvar house return does not match the companion's real residence";
            return false;
        }
    }
    return true;
}

}

void applyBreachAlvarNpcProjection(EventRuntimeState &events, const BreachAlvarCompanionNpcs &npcs, bool split)
{
    for (const auto &npc : npcs)
    {
        events.npcHouseOverrides[npc.npcId] = npc.houseId;
        if (split)
        {
            events.npcGreetingOverrides[npc.npcId] = npc.greetingId;
        }
        for (size_t slot = 0; slot < npc.topicIds.size(); ++slot)
        {
            events.npcTopicOverrides[npc.npcId][uint32_t(slot)] = npc.topicIds[slot];
        }
        if (npc.name)
        {
            events.npcNameOverrides[npc.npcId] = *npc.name;
        }
        else
        {
            events.npcNameOverrides.erase(npc.npcId);
        }
        if (npc.pictureId)
        {
            events.npcPictureOverrides[npc.npcId] = *npc.pictureId;
        }
        else
        {
            events.npcPictureOverrides.erase(npc.npcId);
        }
    }
}

namespace
{
// Compare exactly the event containers that will be published. Other event outputs are never
// replaced wholesale by an older copy; they remain owned by the normal event/UI handoff.
bool sameEventWriteSet(const EventRuntimeState &before, const EventRuntimeState &live)
{
    return before.mapFileName == live.mapFileName && before.eventRandomState == live.eventRandomState
        && before.namedGlobalVars == live.namedGlobalVars && before.npcHouseOverrides == live.npcHouseOverrides
        && before.npcGreetingOverrides == live.npcGreetingOverrides
        && before.npcTopicOverrides == live.npcTopicOverrides
        && before.npcNameOverrides == live.npcNameOverrides && before.npcPictureOverrides == live.npcPictureOverrides;
}

bool sameRequest(const Request &a, const Request &b)
{
    return a.kind == b.kind && a.sourceEventId == b.sourceEventId
        && a.executingGlobalHandler == b.executingGlobalHandler && a.sourceMapName == b.sourceMapName
        && a.npcId == b.npcId && a.dialogueKind == b.dialogueKind && a.sourceNpcId == b.sourceNpcId
        && a.hostHouseId == b.hostHouseId && a.sourceActorIndex == b.sourceActorIndex
        && a.stableActorId == b.stableActorId;
}

bool sameHeldItem(const GameplayUiController::HeldInventoryItemState &before,
    const GameplayUiController::HeldInventoryItemState &live, const std::vector<uint8_t> &itemBytes)
{
    return before.active == live.active && before.grabCellOffsetX == live.grabCellOffsetX
        && before.grabCellOffsetY == live.grabCellOffsetY && before.grabOffsetX == live.grabOffsetX
        && before.grabOffsetY == live.grabOffsetY && encodeInventoryItemForComparison(live.item) == itemBytes;
}
}

std::optional<bool> GameSession::breachAlvarPrinceOutsideHouse705() const
{
    std::string error;
    const auto context = runtimeContext(*this, error);
    if (!context || !validLiveReceipt(*this, *context, error)) return std::nullopt;
    // Original BrAlvar.lua 201–228 records the first house705 companion as Prince.
    // The supported split receipt preserves that slot even after the Character returns.
    if (!m_breachAlvarCompanions) return true;
    const auto house = std::find(context->pProfile->houseIds.begin(), context->pProfile->houseIds.end(), 705u);
    if (house == context->pProfile->houseIds.end()) return std::nullopt;
    const size_t index = size_t(house - context->pProfile->houseIds.begin());
    const uint32_t npcId = context->pProfile->npcIds[index];
    const Party::Snapshot party = context->pParty->snapshot();
    const auto currentHouse = party.npcHouseOverrides.find(npcId);
    if (currentHouse != party.npcHouseOverrides.end()) return currentHouse->second != 705;
    const auto *pNpc = data().npcDialogTable().getNpc(npcId);
    return pNpc == nullptr || pNpc->houseId != 705;
}

bool GameSession::isBreachAlvarCompanionAvailable(uint32_t npcId) const
{
    if (npcId < 772 || npcId > 775 || !m_breachAlvarCompanions
        || m_breachAlvarCompanions->receipt.companions[npcId - 772].returned)
    {
        return false;
    }
    try
    {
        std::string error;
        const auto context = runtimeContext(*this, error);
        if (!context || !validLiveReceipt(*this, *context, error))
        {
            return false;
        }
        const auto facts = resolveBreachAlvarCompanionTopicFacts(*this, *context->pEvents, error);
        if (!facts || facts->questFinished)
        {
            return false;
        }
        return OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(context->pWorld->snapshot(),
            m_breachAlvarCompanions->friendActor, m_breachAlvarCompanions->receipt.companions[0].returned,
            error, &context->pProfile->friendActor);
    }
    catch (const std::exception &)
    {
        return false;
    }
}

bool GameSession::bindBreachAlvarCompanionRuntime(std::string &error)
{
    error.clear();
    // Other initialized maps have no companion timer. Their normal initialization clears world rules.
    if (!isBreachMapName(m_currentMapFileName))
    {
        return true;
    }
    try
    {
        const auto context = runtimeContext(*this, error);
        if (!context || !validLiveReceipt(*this, *context, error))
        {
            return false;
        }
        const auto snapshot = context->pWorld->snapshot();
        if (m_breachAlvarCompanions)
        {
            if (!OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(snapshot,
                m_breachAlvarCompanions->friendActor, m_breachAlvarCompanions->receipt.companions[0].returned,
                error, &context->pProfile->friendActor)) return false;
        }
        else if (std::any_of(snapshot.mapActors.begin(), snapshot.mapActors.end(), [](const auto &actor)
            { return actor.breachAlvarCompanion || actor.breachAlvarCompanionRemoved; }))
        {
            error = "Breach Alvar world contains a companion without its Session receipt";
            return false;
        }
        return context->pWorld->bindBreachAlvarFriendActorRules(context->pProfile->friendActor, error);
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

BreachAlvarSessionResult GameSession::processBreachAlvarCompanionRequest(const Request &queuedRequest)
{
    try
    {
        const Request request = queuedRequest;
        std::string error;
        const auto context = runtimeContext(*this, error);
        if (!context)
        {
            return {Result::Code::Rejected, std::move(error)};
        }
        OutdoorWorldRuntime &world = *context->pWorld;
        Party &party = *context->pParty;
        EventRuntimeState &events = *context->pEvents;
        const auto &profile = *context->pProfile;
        const bool split = request.kind == Request::Kind::Split;
        if (!events.pendingBreachAlvarCompanionRequest
            || !sameRequest(request, *events.pendingBreachAlvarCompanionRequest)
            || !isBreachMapName(request.sourceMapName)
            || (request.kind != Request::Kind::Split && request.kind != Request::Kind::Return)
            || (split && (request.sourceEventId != 65469 || request.executingGlobalHandler
                || request.npcId != 0 || request.dialogueKind != DialogueContextKind::None
                || request.sourceNpcId != 0 || request.hostHouseId != 0
                || request.sourceActorIndex || request.stableActorId)))
        {
            return {Result::Code::Rejected, "Request is not the currently queued Breach Alvar source command"};
        }
        if (!validLiveReceipt(*this, *context, error))
        {
            return {Result::Code::Rejected, std::move(error)};
        }
        for (size_t index = 0; index < profile.npcIds.size(); ++index)
        {
            if (data().npcDialogTable().getNpc(profile.npcIds[index]) == nullptr
                || (profile.houseIds[index] != 0 && data().houseTable().get(profile.houseIds[index]) == nullptr))
            {
                return {Result::Code::Rejected, "Breach Alvar source NPC or house definition is absent"};
            }
        }
        const auto facts = resolveBreachAlvarCompanionTopicFacts(*this, events, error);
        if (!facts)
        {
            return {Result::Code::Rejected, std::move(error)};
        }
        if (facts->questFinished)
        {
            return {Result::Code::Rejected, "Breach Alvar companion operations ended with QuestFinished"};
        }
        if (split && m_breachAlvarCompanions)
        {
            if (!OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(world.snapshot(),
                    m_breachAlvarCompanions->friendActor,
                    m_breachAlvarCompanions->receipt.companions[0].returned, error, &profile.friendActor))
            {
                return {Result::Code::Rejected, std::move(error)};
            }
            return {Result::Code::AlreadyApplied, {}};
        }
        if (split && (party.memberCount() < profile.minimumActiveCount
            || party.memberCount() > profile.maximumActiveCount))
        {
            return {Result::Code::Rejected, "Breach Alvar content does not admit this initial Party size"};
        }
        if (!split)
        {
            if (request.npcId < 772 || request.npcId > 775 || facts->hintByNpc[request.npcId - 772] == 0)
            {
                return {Result::Code::Rejected, "Companion return requires its original encountered hint"};
            }
            if (!validReturnDialogue(*this, *context, request, error))
            {
                return {Result::Code::Rejected, std::move(error)};
            }
        }

        // No partial remapping of active actions: this bounded coordinator only publishes between them.
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
                && events.grantedItems.empty() && events.grantedItemIds.empty() && events.removedItemIds.empty()
                && events.portraitFxRequests.empty() && events.spellFxRequests.empty()
                && party.pendingAudioRequests().empty();
        };
        if (!safe())
        {
            return {Result::Code::Unsafe, "Breach Alvar requires an idle gameplay publication point"};
        }

        // Capture before GPU preparation, which may invoke resource-loader callbacks.
        const Request beforeRequest = request;
        const auto beforeParty = encodePartySnapshotForComparison(party.snapshot());
        const auto beforeHeld = m_gameplayUiController.heldInventoryItem();
        const auto beforeHeldBytes = encodeInventoryItemForComparison(beforeHeld.item);
        const uint32_t beforeHeldQuery = party.heldItemIdForQueries();
        if (beforeHeldQuery != (beforeHeld.active ? beforeHeld.item.objectDescriptionId : 0))
        {
            return {Result::Code::Rejected, "Held item query cache disagrees with the actual UI item"};
        }
        const std::string beforeStatus = party.lastStatus();
        const EventRuntimeState beforeEvents = events;
        const auto beforeReceipt = m_breachAlvarCompanions;
        const auto beforeGlobals = m_namedGlobalVars;
        const double beforeTime = m_gameMinutes;
        const std::string beforeMap = m_currentMapFileName;
        const auto *pBeforeRepository = &data();
        const BreachAlvarCompanionProfile beforeProfile = profile;
        EventRuntimeState candidateEvents = beforeEvents;
        uint64_t seed = 0;
        if (split)
        {
            seed = uint64_t(EventRuntime::randomValue(candidateEvents, 65469, 0)) << 32;
            seed |= EventRuntime::randomValue(candidateEvents, 65469, 1);
        }
        SourceMercenaryGenerationTables generationTables{
            &data().classMultiplierTable(), &data().classSkillTable(), &data().characterDollTable(),
            &data().mergedCharacterSelectionTable(), &data().mergedNpcNameTable(),
            &data().mergedClassStartingStatsSourceTable(), &data().itemTable(),
            &data().standardItemEnchantTable(), &data().specialItemEnchantTable(), &data().spellTable()};
        BreachAlvarSplitGenerationContext generationContext;
        if (split && party.memberCount() < 5)
        {
            const auto *pMapRules = data().mergedBolsterMapTable().findById(uint32_t(world.mapId()));
            if (profile.version < 2 || profile.generationSourceSha256.empty() || !pMapRules
                || pMapRules->continent == 0)
                return {Result::Code::Rejected, "Breach Alvar reserve generation lacks verified source context"};
            generationContext.pRosterTable = &data().rosterTable();
            generationContext.pTables = &generationTables;
            generationContext.currentSwitch = pMapRules->continent;
            const int32_t historyCount = globalValue(*this, events, std::string(PortraitHistoryPrefix) + "Count");
            if (historyCount < 0 || historyCount > 4096)
                return {Result::Code::Rejected, "Invalid source mercenary portrait history size"};
            for (int32_t index = 0; index < historyCount; ++index)
            {
                const std::string key = std::string(PortraitHistoryPrefix) + std::to_string(index);
                const auto value = events.namedGlobalVars.find(key);
                if (value == events.namedGlobalVars.end() || value->second < 0
                    || data().characterDollTable().getCharacter(uint32_t(value->second) + 1) == nullptr)
                    return {Result::Code::Rejected, "Incomplete source mercenary portrait history"};
                generationContext.portraitHistory.push_back(uint32_t(value->second));
            }
        }
        auto preparation = split
            ? prepareBreachAlvarSplit(party, m_breachAlvarCompanions,
                data().characterDollTable(), *facts, seed, generationContext)
            : prepareBreachAlvarCompanionReturn(party, m_breachAlvarCompanions,
                data().characterDollTable(), *facts, request.npcId);
        if (!preparation.mutation)
        {
            return {Result::Code::Rejected, std::move(preparation.error)};
        }
        auto &mutation = *preparation.mutation;
        std::optional<ActorToken> actor;
        if (split)
        {
            const auto *pFriend = mutation.m_party.detachedMember(std::string(BreachAlvarSplitOwner), 772);
            if (pFriend == nullptr)
            {
                return {Result::Code::Rejected, "Prepared split has no NPC772-owned character"};
            }
            const auto appearance = resolveBreachAlvarCompanionAppearance(profile, pFriend->character,
                data().characterDollTable(), data().classMultiplierTable(), error);
            if (!appearance)
            {
                return {Result::Code::Rejected, std::move(error)};
            }
            // Source summons after removing the existing NPC772, before removing the other active members.
            // A leader-only Party instead uses reserve38 and still has its leader during the summon.
            Party spawnBolsterContext = party;
            const auto sourceParty = party.snapshot();
            if (sourceParty.members.size() > 1)
            {
                const auto chosen = std::find_if(sourceParty.members.begin(), sourceParty.members.end(),
                    [&](const Character &member) { return member.memberId == pFriend->character.memberId; });
                if (chosen == sourceParty.members.end()
                    || !spawnBolsterContext.detachMembers(std::string(BreachAlvarSplitOwner),
                        {{size_t(chosen - sourceParty.members.begin()), 772}}))
                    return {Result::Code::Rejected, "Cannot prepare the original pre-summon Party context"};
            }
            actor = world.prepareBreachAlvarFriendSpawn(
                appearance->monsterId, profile.friendActor, spawnBolsterContext, error);
        }
        else if (request.npcId == 772)
        {
            actor = world.prepareBreachAlvarFriendRemoval(
                *mutation.m_existingFriendActor, profile.friendActor, error);
        }
        else
        {
            actor = world.prepareBreachAlvarFriendPreservation(*mutation.m_existingFriendActor,
                profile.friendActor, m_breachAlvarCompanions->receipt.companions[0].returned, error);
        }
        if (!actor)
        {
            return {Result::Code::Rejected, std::move(error)};
        }
        auto candidateWorld = world.snapshotWithPreparedBreachAlvarFriendActor(*actor, error);
        if (!candidateWorld)
        {
            return {Result::Code::Rejected, std::move(error)};
        }
        std::optional<BreachAlvarCompanionState> candidateReceipt =
            BreachAlvarCompanionState{mutation.m_receipt, actor->binding()};
        applyBreachAlvarNpcProjection(candidateEvents, mutation.m_npcs, split);
        auto candidateGlobals = m_namedGlobalVars;
        for (const auto &[key, value] : candidateEvents.namedGlobalVars)
        {
            candidateGlobals[key] = value;
        }
        candidateGlobals[SplitFlag] = 1;
        for (size_t index = 0; index < 4; ++index)
        {
            candidateGlobals[std::string(QuestPrefix) + "FQMercRnd." + std::to_string(772 + index)] =
                mutation.m_receipt.companions[index].generation ? 1 : 0;
        }
        if (mutation.portraitHistoryChanged())
        {
            std::erase_if(candidateGlobals, [](const auto &entry)
                { return entry.first.starts_with(PortraitHistoryPrefix); });
            const auto &history = mutation.portraitHistoryAfter();
            candidateGlobals[std::string(PortraitHistoryPrefix) + "Count"] = int32_t(history.size());
            for (size_t index = 0; index < history.size(); ++index)
                candidateGlobals[std::string(PortraitHistoryPrefix) + std::to_string(index)] = int32_t(history[index]);
        }
        candidateEvents.namedGlobalVars = candidateGlobals;
        candidateWorld->eventRuntimeState = candidateEvents;
        clearTransientEventRuntimeState(*candidateWorld->eventRuntimeState);
        candidateWorld->eventRuntimeState->pendingBreachAlvarCompanionRequest.reset();
        candidateWorld->gameMinutes = beforeTime;
        std::optional<Party> candidatePartyCache = mutation.m_party;
        std::optional<OutdoorPartyRuntime::Snapshot> candidateMovement = world.m_pPartyRuntime->snapshot();
        auto candidateOutdoorCache = m_outdoorWorldStates;
        auto candidateIndoorCache = m_indoorSceneStates;
        auto candidateIndoor = m_currentIndoorSceneState;
        // Existing aliases and the current key all refer to one prospective world generation.
        for (auto &[key, value] : candidateOutdoorCache)
        {
            if (isBreachMapName(key))
            {
                value = *candidateWorld;
            }
            if (value.eventRuntimeState)
            {
                value.eventRuntimeState->namedGlobalVars = candidateGlobals;
            }
        }
        candidateOutdoorCache[beforeMap] = *candidateWorld;
        candidateOutdoorCache[buildCanonicalMapId("mmmerge", "bralvar.odm")] = *candidateWorld;
        for (auto &[key, value] : candidateIndoorCache)
        {
            if (value.eventRuntimeState)
            {
                value.eventRuntimeState->namedGlobalVars = candidateGlobals;
            }
        }
        if (candidateIndoor && candidateIndoor->eventRuntimeState)
        {
            candidateIndoor->eventRuntimeState->namedGlobalVars = candidateGlobals;
        }
        GameSaveData prospective;
        prospective.currentSceneKind = SceneKind::Outdoor;
        prospective.mapFileName = beforeMap;
        prospective.party = mutation.m_party.snapshot();
        prospective.breachAlvarCompanions = candidateReceipt;
        prospective.namedGlobalVars = candidateGlobals;
        prospective.savedGameMinutes = beforeTime;
        prospective.heldInventoryItemActive = beforeHeld.active;
        prospective.heldInventoryItem = beforeHeld.item;
        prospective.heldInventoryItemGrabCellOffsetX = beforeHeld.grabCellOffsetX;
        prospective.heldInventoryItemGrabCellOffsetY = beforeHeld.grabCellOffsetY;
        prospective.heldInventoryItemGrabOffsetX = beforeHeld.grabOffsetX;
        prospective.heldInventoryItemGrabOffsetY = beforeHeld.grabOffsetY;
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
        if (!validateSavedBreachAlvarCompanions(prospective, error, &profile.friendActor))
        {
            return {Result::Code::Rejected, std::move(error)};
        }
        std::optional<EventRuntimeState::PendingDialogueContext> afterDialogue = events.pendingDialogueContext;
        if (!split && request.npcId != 772)
        {
            afterDialogue.emplace();
            afterDialogue->kind = DialogueContextKind::HouseService;
            afterDialogue->sourceId = request.hostHouseId;
            afterDialogue->hostHouseId = request.hostHouseId;
        }
        GameplayOverlayInteractionState clearedInteraction;

        const auto current = runtimeContext(*this, error);
        if (!current || current->pWorld != &world || current->pParty != &party || current->pEvents != &events
            || current->pProfile != context->pProfile || &data() != pBeforeRepository
            || profile != beforeProfile || m_currentMapFileName != beforeMap || m_gameMinutes != beforeTime
            || m_breachAlvarCompanions != beforeReceipt || m_namedGlobalVars != beforeGlobals
            || encodePartySnapshotForComparison(party.snapshot()) != beforeParty || party.lastStatus() != beforeStatus
            || party.heldItemIdForQueries() != beforeHeldQuery
            || !sameHeldItem(beforeHeld, m_gameplayUiController.heldInventoryItem(), beforeHeldBytes)
            || !sameEventWriteSet(beforeEvents, events) || !events.pendingBreachAlvarCompanionRequest
            || !sameRequest(beforeRequest, *events.pendingBreachAlvarCompanionRequest) || !safe()
            || (!split && !validReturnDialogue(*this, *current, beforeRequest, error))
            || !world.isPreparedBreachAlvarFriendActorCurrent(*actor))
        {
            return {Result::Code::Rejected, "Breach Alvar preparation became stale before publication"};
        }

        static_assert(noexcept(std::declval<OutdoorPartyRuntime &>().syncSpellMovementStatesFromPartyBuffs()));
        static_assert(noexcept(std::declval<const OutdoorPartyRuntime &>().snapshot()));
        static_assert(std::is_nothrow_swappable_v<Party>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_partyState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_outdoorPartyState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_breachAlvarCompanions)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_currentOutdoorWorldState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_currentIndoorSceneState)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_outdoorWorldStates)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_indoorSceneStates)>);
        static_assert(std::is_nothrow_swappable_v<decltype(m_namedGlobalVars)>);
        static_assert(std::is_nothrow_swappable_v<decltype(events.npcTopicOverrides)>);
        static_assert(std::is_nothrow_swappable_v<decltype(events.npcNameOverrides)>);
        static_assert(std::is_nothrow_swappable_v<decltype(afterDialogue)>);
        static_assert(std::is_nothrow_swappable_v<GameplayOverlayInteractionState>);
        const auto publish = [&]() noexcept
        {
            if (!world.publishPreparedBreachAlvarFriendActor(std::move(*actor)))
            {
                return false;
            }
            using std::swap;
            swap(party, mutation.m_party); // Preserve the live Party address held by scene/world systems.
            if (!m_partyState || &*m_partyState != &party)
            {
                swap(m_partyState, candidatePartyCache);
            }
            swap(m_breachAlvarCompanions, candidateReceipt);
            swap(m_namedGlobalVars, candidateGlobals);
            swap(m_currentOutdoorWorldState, candidateWorld);
            swap(m_outdoorPartyState, candidateMovement);
            swap(m_outdoorWorldStates, candidateOutdoorCache);
            swap(m_currentIndoorSceneState, candidateIndoor);
            swap(m_indoorSceneStates, candidateIndoorCache);
            events.namedGlobalVars.swap(candidateEvents.namedGlobalVars);
            events.npcHouseOverrides.swap(candidateEvents.npcHouseOverrides);
            events.npcGreetingOverrides.swap(candidateEvents.npcGreetingOverrides);
            events.npcTopicOverrides.swap(candidateEvents.npcTopicOverrides);
            events.npcNameOverrides.swap(candidateEvents.npcNameOverrides);
            events.npcPictureOverrides.swap(candidateEvents.npcPictureOverrides);
            events.eventRandomState = candidateEvents.eventRandomState;
            swap(events.pendingDialogueContext, afterDialogue);
            if (!split && beforeRequest.npcId == 772)
            {
                events.pendingCloseNpcDialogue = true;
            }
            swap(m_overlayInteractionState, clearedInteraction);
            auto &ui = m_gameplayUiController.state();
            ui.characterScreen = {};
            ui.utilitySpellOverlay.casterMemberIndex = 0;
            ui.itemInspectOverlay.sourceMemberIndex = 0;
            m_gameplayScreenState.pendingSpellTarget().casterMemberIndex = 0;
            world.m_pPartyRuntime->syncSpellMovementStatesFromPartyBuffs();
            *m_outdoorPartyState = world.m_pPartyRuntime->snapshot();
            return true;
        };
        if (!publish())
        {
            return {Result::Code::Rejected, "Breach Alvar world refused stale publication"};
        }
        return {Result::Code::Committed, {}};
    }
    catch (const std::exception &exception)
    {
        return {Result::Code::Rejected, exception.what()};
    }
}
}
