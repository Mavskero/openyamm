#include "game/outdoor/OutdoorWorldRuntime.h"

#include "game/StringUtils.h"
#include "game/events/GameTime.h"
#include "game/gameplay/OwnedMercenaryRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/outdoor/OutdoorGameView.h"
#include "game/tables/CharacterDollTable.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <type_traits>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
bool isBreachMap(const std::string &fileName)
{
    const std::string lower = toLowerCopy(fileName);
    return lower == "breach.odm" || lower == BreachMercenaryMap;
}

bool validSourceHash(const std::string &hash)
{
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c)
        { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

std::optional<uint32_t> sourceAlias(const Character &character)
{
    if (character.sourceMercenaryRetention.sourceRosterId)
    {
        return character.sourceMercenaryRetention.sourceRosterId;
    }
    return character.rosterId != 0 ? std::optional<uint32_t>(character.rosterId) : std::nullopt;
}

bool removedActorIsInert(const OutdoorWorldRuntime::MapActorState &actor)
{
    return !actor.hostileToParty && !actor.hasDetectedParty && actor.isInvisible && !actor.canReceiveDamage
        && actor.controlMode == OutdoorWorldRuntime::ActorControlMode::None && actor.controlRemainingSeconds == 0.0f
        && actor.aiState == OutdoorWorldRuntime::ActorAiState::Standing
        && actor.animation == OutdoorWorldRuntime::ActorAnimation::Standing
        && actor.moveDirectionX == 0.0f && actor.moveDirectionY == 0.0f
        && actor.velocityX == 0.0f && actor.velocityY == 0.0f && actor.velocityZ == 0.0f
        && actor.movementState.verticalVelocity == 0.0f && actor.actionSeconds == 0.0f
        && !actor.attackImpactTriggered;
}

void discardActorReferences(OutdoorWorldRuntime::Snapshot &snapshot, std::optional<uint32_t> actorId)
{
    const auto removedSource = [actorId](const auto &projectile)
    {
        return projectile.sourceKind == OutdoorWorldRuntime::ProjectileState::SourceKind::Actor
            && (!actorId || projectile.sourceId == *actorId);
    };
    std::erase_if(snapshot.projectiles, removedSource);
    std::erase_if(snapshot.fireSpikeTraps, removedSource);
    std::erase_if(snapshot.bloodSplats, [actorId](const auto &splat)
        { return !actorId || splat.sourceActorId == *actorId; });
}

void clearPopulationEventReferences(EventRuntimeState &state, const std::string &mapFileName)
{
    state.actorSetMasks.clear();
    state.actorClearMasks.clear();
    state.actorGroupSetMasks.clear();
    state.actorGroupClearMasks.clear();
    state.actorHostilityRequests.clear();
    state.actorGroupHostilityRequests.clear();
    state.actorIdGroupOverrides.clear();
    state.actorGroupOverrides.clear();
    state.actorGroupAllyOverrides.clear();
    state.actorItemOverrides.clear();
    state.actorExtraItemOverrides.clear();
    const std::string prefix = toLowerCopy(mapFileName) + "#";
    std::erase_if(state.generatedNpcIdsByActorKey, [&prefix](const auto &entry)
        { return toLowerCopy(entry.first).starts_with(prefix); });
}

void publishPopulationEventReferences(EventRuntimeState &live, EventRuntimeState &candidate) noexcept
{
    live.actorSetMasks.swap(candidate.actorSetMasks);
    live.actorClearMasks.swap(candidate.actorClearMasks);
    live.actorGroupSetMasks.swap(candidate.actorGroupSetMasks);
    live.actorGroupClearMasks.swap(candidate.actorGroupClearMasks);
    live.actorHostilityRequests.swap(candidate.actorHostilityRequests);
    live.actorGroupHostilityRequests.swap(candidate.actorGroupHostilityRequests);
    live.actorIdGroupOverrides.swap(candidate.actorIdGroupOverrides);
    live.actorGroupOverrides.swap(candidate.actorGroupOverrides);
    live.actorGroupAllyOverrides.swap(candidate.actorGroupAllyOverrides);
    live.actorItemOverrides.swap(candidate.actorItemOverrides);
    live.actorExtraItemOverrides.swap(candidate.actorExtraItemOverrides);
    live.generatedNpcIdsByActorKey.swap(candidate.generatedNpcIdsByActorKey);
}
}

std::vector<uint8_t> OutdoorWorldRuntime::breachMercenaryWorldComparison() const
{
    Snapshot complete = snapshot();
    // Include live timers and serializable event requests instead of the stripped save-snapshot versions.
    // The two unsaved hostility maps are compared separately; other transient requests stay Session-owned.
    complete.timers = m_timers;
    complete.eventRuntimeState = m_eventRuntimeState;
    return encodeOutdoorWorldSnapshotForComparison(complete);
}

bool OutdoorWorldRuntime::validateBreachMercenaryProjection(
    const Snapshot &snapshot, const Party &party, const BreachMercenaryProfile *pProfile, std::string &error)
{
    error.clear();
    if (!snapshot.breachMercenaries)
    {
        return true; // Legacy Save100 has no population receipt. Cold load must not rebuild it.
    }
    const auto fail = [&error](const char *message)
    {
        error = std::string("Breach mercenary projection: ") + message;
        return false;
    };
    const BreachMercenaryProjectionState &state = *snapshot.breachMercenaries;
    if (state.version != 1 || state.profileVersion != BreachMercenaryContentVersion
        || state.canonicalMapKey != BreachMercenaryMap || state.bindings.size() > 11
        || !std::all_of(state.sourceSha256.begin(), state.sourceSha256.end(), validSourceHash)
        || !validGameMinutes(snapshot.gameMinutes))
    {
        return fail("invalid version, map, source identity, population size or clock");
    }
    if (pProfile != nullptr)
    {
        if (!validBreachMercenaryProfile(*pProfile, error))
        {
            return false;
        }
        if (state.profileVersion != pProfile->version || state.canonicalMapKey != pProfile->canonicalMapKey
            || state.sourceSha256 != pProfile->sourceSha256)
        {
            return fail("saved population differs from authenticated source profile");
        }
    }
    const Party::Snapshot partyState = party.snapshot();
    if (!Party::validMemberOwnership(partyState, error))
    {
        return false;
    }
    std::vector<const Character *> owned;
    for (const Character &member : party.members()) owned.push_back(&member);
    for (const AdventurersInnMember &member : party.adventurersInnMembers()) owned.push_back(&member.character);
    for (const Party::DetachedPartyMember &member : party.detachedMembers()) owned.push_back(&member.character);
    std::unordered_set<uint32_t> actorIds;
    for (const MapActorState &actor : snapshot.mapActors)
    {
        if (!actorIds.insert(actor.actorId).second || actor.actorId >= snapshot.nextActorId
            || (actor.isSourceRemoved() && !removedActorIsInert(actor)))
        {
            return fail("duplicate actor identity, invalid next identity or inconsistent removed actor");
        }
    }
    std::unordered_set<uint64_t> memberIds;
    std::unordered_set<uint32_t> rosterIds, npcIds, boundActorIds, spotIndices;
    for (const OwnedMercenaryActorBinding &binding : state.bindings)
    {
        if (binding.memberId == 0 || binding.npcId == 0
            || binding.npcId > uint32_t(std::numeric_limits<int16_t>::max())
            || binding.sourceSpotIndex == 0 || binding.sourceSpotIndex > 11
            || !memberIds.insert(binding.memberId).second || !rosterIds.insert(binding.sourceRosterId).second
            || !npcIds.insert(binding.npcId).second || !boundActorIds.insert(binding.actorId).second
            || !spotIndices.insert(binding.sourceSpotIndex).second)
        {
            return fail("invalid or duplicate weak binding");
        }
        const auto member = std::find_if(owned.begin(), owned.end(), [&binding](const Character *pMember)
            { return pMember->memberId == binding.memberId; });
        if (member == owned.end())
        {
            // Permanent Inn release removes the Character, but its old actor remains a weak historical binding.
            // Validate its v1 source alias even before a cold load has the content profile; never rebind by roster.
            if (binding.memberId >= partyState.nextMemberId || binding.sourceRosterId > 49
                || binding.npcId != binding.sourceRosterId + 261
                || !partyState.reusableMercenaryRosterIds.contains(binding.sourceRosterId))
            {
                return fail("missing bound Character has no valid released source slot");
            }
        }
        else if (sourceAlias(**member) != binding.sourceRosterId
            || std::count_if(owned.begin(), owned.end(), [&binding](const Character *pMember)
                { return sourceAlias(*pMember) == binding.sourceRosterId; }) != 1)
        {
            return fail("binding has no unique owned Character and source roster alias");
        }
        const auto actor = std::find_if(snapshot.mapActors.begin(), snapshot.mapActors.end(), [&binding](const auto &a)
            { return a.actorId == binding.actorId; });
        if (actor == snapshot.mapActors.end() || actor->npcId != int16_t(binding.npcId)
            || actor->monsterId <= 0 || !actor->spawnedAtRuntime || actor->fromSpawnPoint
            || actor->breachAlvarCompanion || actor->breachAlvarCompanionRemoved
            || actor->breachAlvarNextMaintenanceGameMinute != 0.0 || actor->combatState.maximumHitPoints() <= 0
            || actor->currentHp < 0 || actor->currentHp > actor->combatState.maximumHitPoints()
            || !actor->levelOverride || *actor->levelOverride == 0
            || *actor->levelOverride > uint32_t(std::numeric_limits<int>::max())
            || std::count_if(snapshot.mapActors.begin(), snapshot.mapActors.end(), [&binding](const auto &a)
                { return a.npcId == int16_t(binding.npcId); }) != 1)
        {
            return fail("missing actor or incompatible NPC, health, level or lifecycle");
        }
        if (pProfile != nullptr
            && (breachMercenaryNpcId(*pProfile, binding.sourceRosterId) != binding.npcId
                || (uint32_t(actor->monsterId) != pProfile->fallbackMonsterId
                    && std::find(pProfile->monsterIdByFace.begin(), pProfile->monsterIdByFace.end(),
                        uint32_t(actor->monsterId)) == pProfile->monsterIdByFace.end())
                || actor->group != 0 || actor->ally != pProfile->actor.ally
                || actor->wanderRadius != float(pProfile->actor.guardRadius)
                || actor->proceduralDeathLoot || actor->specialItemId != pProfile->actor.item))
        {
            return fail("saved actor differs from source rules");
        }
        // Ownership, health, appearance and level may change after this projection was made. In particular,
        // an Inn return may make the Character active while its old actor remains. Never rederive on cold load.
    }
    return true;
}

bool OutdoorWorldRuntime::bindBreachMercenaryProjection(
    const BreachMercenaryProfile &profile, const Party &party, std::string &error)
{
    error.clear();
    if (!isInitialized() || !isBreachMap(m_map.fileName) || m_pParty != &party)
    {
        error = "Breach mercenary binding requires its actual world and Party";
        return false;
    }
    if (!validBreachMercenaryProfile(profile, error)
        || !validateBreachMercenaryProjection(snapshot(), party, &profile, error))
    {
        return false;
    }
    std::optional<BreachMercenaryProfile> candidate(profile);
    m_breachMercenaryProfile.swap(candidate);
    ++m_breachAlvarSceneGeneration;
    return true;
}

std::optional<GameplayOwnedMercenaryActorTarget> OutdoorWorldRuntime::ownedMercenaryActorBinding(
    uint32_t npcId, uint32_t sourceActorIndex) const
{
    if (!m_breachMercenaries || sourceActorIndex >= m_mapActors.size())
    {
        return std::nullopt;
    }
    const MapActorState &actor = m_mapActors[sourceActorIndex];
    const auto found = std::find_if(m_breachMercenaries->bindings.begin(), m_breachMercenaries->bindings.end(),
        [&](const auto &binding) { return binding.actorId == actor.actorId && binding.npcId == npcId; });
    if (found == m_breachMercenaries->bindings.end() || actor.npcId <= 0 || uint32_t(actor.npcId) != npcId)
    {
        return std::nullopt;
    }
    return GameplayOwnedMercenaryActorTarget{*found, m_breachMercenaries->canonicalMapKey,
        m_breachMercenaries->profileVersion, m_breachMercenaries->sourceSha256,
        m_breachAlvarSceneGeneration, sourceActorIndex,
        !actor.isSourceRemoved() && !actor.isInvisible && !actor.isDead && actor.currentHp > 0
            && actor.aiState != ActorAiState::Dying && actor.aiState != ActorAiState::Dead && !actor.hostileToParty};
}

std::optional<OutdoorWorldRuntime::PreparedBreachMercenaryActorMutation>
OutdoorWorldRuntime::prepareBreachMercenaryProjection(
    const Party &party, const BreachMercenaryProfile &profile, const CharacterDollTable &dolls, std::string &error)
{
    error.clear();
    const auto fail = [&error](const char *message) -> std::optional<PreparedBreachMercenaryActorMutation>
    {
        error = std::string("Breach mercenary preparation: ") + message;
        return std::nullopt;
    };
    if (!isInitialized() || !isBreachMap(m_map.fileName) || m_pParty != &party || !validGameMinutes(m_gameMinutes))
    {
        return fail("requires the actual Breach world and Party");
    }
    if (!validBreachMercenaryProfile(profile, error) || !Party::validMemberOwnership(party.snapshot(), error))
    {
        return std::nullopt;
    }
    PreparedBreachMercenaryActorMutation prepared;
    prepared.m_pWorld = this;
    prepared.m_pParty = &party;
    prepared.m_sceneGeneration = m_breachAlvarSceneGeneration;
    prepared.m_beforeWorld = breachMercenaryWorldComparison();
    prepared.m_beforeParty = encodePartySnapshotForComparison(party.snapshot());
    if (m_eventRuntimeState)
    {
        prepared.m_beforeActorHostilityRequests = m_eventRuntimeState->actorHostilityRequests;
        prepared.m_beforeActorGroupHostilityRequests = m_eventRuntimeState->actorGroupHostilityRequests;
    }
    prepared.m_beforePartyStatus = party.lastStatus();
    prepared.m_beforeHeldItemQuery = party.heldItemIdForQueries();
    prepared.m_beforePartyAudio = party.pendingAudioRequests();
    prepared.m_afterWorld = snapshot();
    prepared.m_profile = profile;
    prepared.m_replacesPopulation = true;

    struct Candidate
    {
        Character character; // Disposable preparation value, never another persistent Character owner.
        uint32_t sourceRosterId = 0;
        uint32_t npcId = 0;
        uint32_t monsterId = 0;
    };
    std::vector<Candidate> candidates;
    const auto consider = [&](const Character &character)
    {
        const OwnedMercenaryResolution resolution = resolveOwnedMercenary(party, character.memberId, profile);
        if (resolution.code == OwnedMercenaryResolutionCode::AmbiguousRoster
            || resolution.code == OwnedMercenaryResolutionCode::InvalidState)
        {
            error = resolution.error;
            return false;
        }
        if (resolution.code == OwnedMercenaryResolutionCode::Eligible)
        {
            if (!resolution.pCharacter || !resolution.sourceRosterId || !resolution.npcId)
            {
                error = "Eligible owned mercenary has incomplete identity";
                return false;
            }
            candidates.push_back({*resolution.pCharacter, *resolution.sourceRosterId, *resolution.npcId, 0});
        }
        return true;
    };
    for (const AdventurersInnMember &member : party.adventurersInnMembers())
    {
        if (!consider(member.character)) return std::nullopt;
    }
    for (const Party::DetachedPartyMember &member : party.detachedMembers())
    {
        if (!consider(member.character)) return std::nullopt;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b)
    {
        return std::tie(a.sourceRosterId, a.character.memberId) < std::tie(b.sourceRosterId, b.character.memberId);
    });
    if (candidates.size() > profile.spots.size()) candidates.resize(profile.spots.size());
    if (candidates.size() > std::numeric_limits<uint32_t>::max() - m_nextActorId)
    {
        return fail("actor identity capacity exhausted");
    }
    std::unordered_set<uint32_t> appearances;
    for (Candidate &candidate : candidates)
    {
        const CharacterDollEntry *pDoll = dolls.getCharacter(candidate.character.characterDataId);
        if (!pDoll || pDoll->id == 0 || pDoll->npcPictureId == 0)
        {
            return fail("owned Character has no valid source face/NPC portrait");
        }
        const auto monster = breachMercenaryMonsterIdForFace(profile, pDoll->id - 1);
        if (!monster || *monster == 0 || *monster > uint32_t(std::numeric_limits<int16_t>::max()))
        {
            return fail("source face has no supported monster appearance");
        }
        candidate.monsterId = *monster;
        appearances.insert(*monster);
    }
    if (!appearances.empty() && m_pInteractionView == nullptr)
    {
        return fail("visual preparation requires the actual interaction view");
    }
    for (uint32_t monsterId : appearances)
    {
        if (!m_pInteractionView->prepareMonsterVisualResourcesForTransaction(int16_t(monsterId), error))
        {
            return std::nullopt;
        }
    }
    prepared.m_pFrames = m_pInteractionView != nullptr
        ? m_pInteractionView->actorSpriteFrameTable() : m_pActorSpriteFrameTable;
    if ((!candidates.empty() && prepared.m_pFrames == nullptr) || !isPreparedBreachMercenaryActorCurrent(prepared))
    {
        return fail("world/Party changed during visual preparation or frames disappeared");
    }

    Snapshot &after = prepared.m_afterWorld;
    after.mapActors.clear();
    after.mapActorCorpseViews.clear();
    after.activeCorpseView.reset();
    after.actorUpdateAccumulatorSeconds = 0.0f;
    discardActorReferences(after, std::nullopt);
    if (after.eventRuntimeState) clearPopulationEventReferences(*after.eventRuntimeState, m_map.fileName);
    after.breachMercenaries = BreachMercenaryProjectionState{
        1, profile.version, profile.canonicalMapKey, profile.sourceSha256, {}};
    for (size_t index = 0; index < candidates.size(); ++index)
    {
        const Candidate &candidate = candidates[index];
        std::optional<MapActorState> actor = prepareBreachMercenaryActor(candidate.character, candidate.monsterId,
            candidate.npcId, after.nextActorId, profile.spots[index], profile.actor, *prepared.m_pFrames, error);
        if (!actor) return std::nullopt;
        after.breachMercenaries->bindings.push_back({candidate.character.memberId, candidate.sourceRosterId,
            candidate.npcId, actor->actorId, uint32_t(index + 1)});
        after.mapActors.push_back(std::move(*actor));
        ++after.nextActorId;
    }
    after.mapActorCorpseViews.resize(after.mapActors.size());
    if (!validateBreachMercenaryProjection(after, party, &profile, error)
        || !isPreparedBreachMercenaryActorCurrent(prepared))
    {
        if (error.empty()) error = "Breach mercenary baseline changed before projection was prepared";
        return std::nullopt;
    }
    return prepared;
}

std::optional<OutdoorWorldRuntime::PreparedBreachMercenaryActorMutation>
OutdoorWorldRuntime::prepareBreachMercenaryRemoval(const GameplayOwnedMercenaryActorTarget &target, std::string &error)
{
    error.clear();
    const auto current = ownedMercenaryActorBinding(target.binding.npcId, target.sourceActorIndex);
    if (!current || *current != target || !target.actorInteractable || !m_pParty || !m_breachMercenaryProfile)
    {
        error = "Breach mercenary removal requires the current actual actor binding";
        return std::nullopt;
    }
    Snapshot after = snapshot();
    if (!validateBreachMercenaryProjection(after, *m_pParty, &*m_breachMercenaryProfile, error))
    {
        return std::nullopt;
    }
    PreparedBreachMercenaryActorMutation prepared;
    prepared.m_pWorld = this;
    prepared.m_pParty = m_pParty;
    prepared.m_sceneGeneration = m_breachAlvarSceneGeneration;
    prepared.m_beforeWorld = breachMercenaryWorldComparison();
    prepared.m_beforeParty = encodePartySnapshotForComparison(m_pParty->snapshot());
    if (m_eventRuntimeState)
    {
        prepared.m_beforeActorHostilityRequests = m_eventRuntimeState->actorHostilityRequests;
        prepared.m_beforeActorGroupHostilityRequests = m_eventRuntimeState->actorGroupHostilityRequests;
    }
    prepared.m_beforePartyStatus = m_pParty->lastStatus();
    prepared.m_beforeHeldItemQuery = m_pParty->heldItemIdForQueries();
    prepared.m_beforePartyAudio = m_pParty->pendingAudioRequests();
    prepared.m_profile = m_breachMercenaryProfile;
    prepared.m_pFrames = m_pActorSpriteFrameTable;
    MapActorState &actor = after.mapActors[target.sourceActorIndex];
    actor.sourceRemoved = true;
    enforceSourceRemovedActorLifecycle(actor);
    if (target.sourceActorIndex < after.mapActorCorpseViews.size())
    {
        after.mapActorCorpseViews[target.sourceActorIndex].reset();
    }
    if (after.activeCorpseView && after.activeCorpseView->sourceIndex == target.sourceActorIndex)
    {
        after.activeCorpseView.reset();
    }
    discardActorReferences(after, actor.actorId);
    if (!validateBreachMercenaryProjection(after, *m_pParty, &*m_breachMercenaryProfile, error))
    {
        return std::nullopt;
    }
    prepared.m_afterWorld = std::move(after);
    return prepared;
}

bool OutdoorWorldRuntime::isPreparedBreachMercenaryActorCurrent(
    const PreparedBreachMercenaryActorMutation &prepared) const noexcept
{
    if (prepared.m_pWorld != this || prepared.m_pParty == nullptr || m_pParty != prepared.m_pParty
        || prepared.m_sceneGeneration != m_breachAlvarSceneGeneration || !prepared.m_profile)
    {
        return false;
    }
    try
    {
        const auto sameAudio = [](const Party::PendingAudioRequest &a, const Party::PendingAudioRequest &b)
        {
            return a.kind == b.kind && a.soundId == b.soundId && a.speechId == b.speechId
                && a.memberIndex == b.memberIndex;
        };
        return prepared.m_beforeWorld == breachMercenaryWorldComparison()
            && prepared.m_beforeParty == encodePartySnapshotForComparison(m_pParty->snapshot())
            && (!m_eventRuntimeState
                || (prepared.m_beforeActorHostilityRequests == m_eventRuntimeState->actorHostilityRequests
                    && prepared.m_beforeActorGroupHostilityRequests
                        == m_eventRuntimeState->actorGroupHostilityRequests))
            && prepared.m_beforePartyStatus == m_pParty->lastStatus()
            && prepared.m_beforeHeldItemQuery == m_pParty->heldItemIdForQueries()
            && prepared.m_beforePartyAudio.size() == m_pParty->pendingAudioRequests().size()
            && std::equal(prepared.m_beforePartyAudio.begin(), prepared.m_beforePartyAudio.end(),
                m_pParty->pendingAudioRequests().begin(), sameAudio);
    }
    catch (...)
    {
        return false;
    }
}

std::optional<OutdoorWorldRuntime::Snapshot> OutdoorWorldRuntime::snapshotWithPreparedBreachMercenaryActors(
    const PreparedBreachMercenaryActorMutation &prepared, std::string &error) const
{
    error.clear();
    if (!isPreparedBreachMercenaryActorCurrent(prepared))
    {
        error = "Breach mercenary prepared world/Party baseline is stale";
        return std::nullopt;
    }
    return prepared.m_afterWorld;
}

bool OutdoorWorldRuntime::publishPreparedBreachMercenaryActors(
    PreparedBreachMercenaryActorMutation &&prepared) noexcept
{
    static_assert(std::is_nothrow_swappable_v<std::optional<BreachMercenaryProjectionState>>);
    static_assert(std::is_nothrow_swappable_v<std::optional<BreachMercenaryProfile>>);
    static_assert(std::is_nothrow_swappable_v<std::optional<CorpseViewState>>);
    if (!isPreparedBreachMercenaryActorCurrent(prepared)) return false;
    // Invalidate asynchronous actor-index paths before the first authoritative write. Its only fallible
    // operation is acquiring the job mutex; a failure leaves the prepared population unpublished.
    try
    {
        m_actorPathRuntime.clear();
    }
    catch (...)
    {
        return false;
    }
    Snapshot &after = prepared.m_afterWorld;
    m_mapActors.swap(after.mapActors);
    m_mapActorCorpseViews.swap(after.mapActorCorpseViews);
    m_activeCorpseView.swap(after.activeCorpseView);
    m_breachMercenaries.swap(after.breachMercenaries);
    m_breachMercenaryProfile.swap(prepared.m_profile);
    projectileService().projectiles().swap(after.projectiles);
    m_fireSpikeTraps.swap(after.fireSpikeTraps);
    m_bloodSplats.swap(after.bloodSplats);
    ++m_bloodSplatRevision;
    m_nextActorId = after.nextActorId;
    m_pActorSpriteFrameTable = prepared.m_pFrames;
    if (prepared.m_replacesPopulation)
    {
        if (m_eventRuntimeState && after.eventRuntimeState)
        {
            publishPopulationEventReferences(*m_eventRuntimeState, *after.eventRuntimeState);
        }
        m_actorUpdateAccumulatorSeconds = 0.0f;
        m_mm9FoundPlayerActorIndices.clear();
        m_mm9FoundPlayerEventAttempted.clear();
        m_mm9CivilianActorIndices.clear();
        m_mm9GuardActorIndices.clear();
        m_breachAlvarFriendActorRules.reset();
        m_actorCorpsePhysicsActorIndices.clear();
        m_actorAiUpdateQueued = false;
        m_queuedActorAiDeltaSeconds = 0.0f;
        m_queuedActorAiPartyX = m_queuedActorAiPartyY = m_queuedActorAiPartyZ = 0.0f;
    }
    else
    {
        std::erase_if(m_actorCorpsePhysicsActorIndices, [this](size_t index)
            { return index >= m_mapActors.size() || m_mapActors[index].isSourceRemoved(); });
    }
    m_outdoorPathMapValid = m_outdoorLandPathMapValid = false;
    m_outdoorPathMapSnapshot.reset();
    m_outdoorLandPathMapSnapshot.reset();
    // Empty assignment only destroys old trivial colliders; it cannot allocate. Normal movement rebuilds
    // colliders from the published actors before stepping, including the common removed/invisible gate.
    if (m_outdoorMovementController) m_outdoorMovementController->setActorColliders({});
    ++m_breachAlvarSceneGeneration;
    prepared.m_pWorld = nullptr;
    return true;
}

void OutdoorWorldRuntime::enforceSourceRemovedActorLifecycle(MapActorState &actor) noexcept
{
    if (!actor.isSourceRemoved()) return;
    actor.hostileToParty = false;
    actor.hasDetectedParty = false;
    actor.controlMode = ActorControlMode::None;
    actor.controlRemainingSeconds = 0.0f;
    actor.isInvisible = true;
    actor.canReceiveDamage = false;
    actor.aiState = ActorAiState::Standing;
    actor.animation = ActorAnimation::Standing;
    actor.moveDirectionX = actor.moveDirectionY = 0.0f;
    actor.velocityX = actor.velocityY = actor.velocityZ = 0.0f;
    actor.movementState.verticalVelocity = 0.0f;
    actor.actionSeconds = 0.0f;
    actor.attackImpactTriggered = false;
}

std::optional<OutdoorWorldRuntime::Snapshot> OutdoorWorldRuntime::snapshotWithRemovedBreachAlvarFriend(
    const Snapshot &snapshot, const BreachAlvarFriendActorBinding &binding,
    const BreachAlvarFriendActorRules &rules, std::string &error)
{
    if (!validateBreachAlvarFriendActorBinding(snapshot, binding, false, error, &rules)) return std::nullopt;
    Snapshot result = snapshot;
    const auto found = std::find_if(result.mapActors.begin(), result.mapActors.end(), [&binding](const auto &actor)
        { return actor.actorId == binding.actorId; });
    if (found == result.mapActors.end() || found->isSourceRemoved())
    {
        error = "Breach Alvar cached friend is already removed or missing";
        return std::nullopt;
    }
    const size_t actorIndex = size_t(found - result.mapActors.begin());
    found->breachAlvarCompanionRemoved = true;
    enforceSourceRemovedActorLifecycle(*found);
    if (actorIndex < result.mapActorCorpseViews.size()) result.mapActorCorpseViews[actorIndex].reset();
    if (result.activeCorpseView && result.activeCorpseView->sourceIndex == actorIndex) result.activeCorpseView.reset();
    discardActorReferences(result, binding.actorId);
    if (!validateBreachAlvarFriendActorBinding(result, binding, true, error, &rules)) return std::nullopt;
    return result;
}
}
