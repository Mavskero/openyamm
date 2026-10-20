#include "game/content/BreachMazeLifecycle.h"
#include "game/events/GameTime.h"

#include <bit>
#include <cmath>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::invalid_argument(std::string("Breach maze lifecycle: ") + pMessage);
    }
}

void requireFinite(std::initializer_list<double> values)
{
    for (double value : values)
    {
        require(std::isfinite(value), "non-finite runtime value");
    }
}

uint64_t increment(uint64_t value, const char *pMessage)
{
    require(value != std::numeric_limits<uint64_t>::max(), pMessage);
    return value + 1;
}

uint64_t fingerprint(const BreachMazeTemplate &mazeTemplate)
{
    // Canonical FNV-1a identity, not a cryptographic authenticity check. Each integer is encoded
    // in eight little-endian bytes, including IEEE-754 float bits; padding/host endian never enter it.
    uint64_t hash = UINT64_C(14695981039346656037);
    const auto append = [&hash](uint64_t value)
    {
        for (unsigned byte = 0; byte < 8; ++byte)
        {
            hash ^= (value >> (byte * 8)) & 0xff;
            hash *= UINT64_C(1099511628211);
        }
    };
    append(mazeTemplate.version);
    append(mazeTemplate.vertexCount);
    append(mazeTemplate.decorationCount);
    append(mazeTemplate.lightCount);
    append(mazeTemplate.faces.size());
    for (const BreachMazeTemplateFace &face : mazeTemplate.faces)
    {
        append(face.faceIndex);
        append(face.cellId);
        append(uint8_t(face.role));
        append(face.wallSide ? uint8_t(*face.wallSide) + 1 : 0);
        for (int16_t bound : face.sourceBounds)
        {
            append(uint16_t(bound));
        }
        for (float normal : face.sourceNormal)
        {
            append(std::bit_cast<uint32_t>(normal));
        }
    }
    for (const BreachMazeTemplateCell &cell : mazeTemplate.cells)
    {
        append(cell.cellId);
        append(cell.x);
        append(cell.y);
        append(cell.faceIndices.size());
        for (uint32_t face : cell.faceIndices)
        {
            append(face);
        }
        for (uint32_t face : cell.wallFaceIndices)
        {
            append(face);
        }
        append(cell.centralFloorFaceIndex);
    }
    return hash;
}

void validateMovement(const IndoorMoveState &movement, size_t faceCount)
{
    requireFinite({movement.x, movement.y, movement.footZ, movement.eyeHeight, movement.verticalVelocity,
        movement.fallStartZ, movement.fallDistance});
    require(movement.eyeHeight > 0.0f, "invalid party eye height");
    require(movement.supportFaceIndex == size_t(-1) || movement.supportFaceIndex < faceCount,
        "movement support face is outside the template");
}

BreachMazeArrival savedArrival(const BreachMazeFloorRuntime &runtime)
{
    const IndoorMoveState &movement = runtime.scene.partyRuntime.movementState;
    return {BreachMazeArrivalKind::SavedPose, movement.x, movement.y, movement.footZ,
        runtime.yawRadians, runtime.pitchRadians};
}
}

BreachMazePlanKind BreachMazePreparedTransition::kind() const
{
    return m_kind;
}

const BreachMazeFloorDescriptor *BreachMazePreparedTransition::target() const
{
    if (m_newFloor)
    {
        return &*m_newFloor;
    }
    const BreachMazeFloorState *pFloor = retainedFloor();
    return pFloor != nullptr ? &pFloor->descriptor : nullptr;
}

const BreachMazeFloorState *BreachMazePreparedTransition::retainedFloor() const
{
    if (m_kind != BreachMazePlanKind::RetainedFloor || !m_candidate)
    {
        return nullptr;
    }
    return &m_candidate->floors.at(m_candidate->currentLevel);
}

const BreachMazeArrival &BreachMazePreparedTransition::arrival() const
{
    return m_arrival;
}

const std::optional<BreachMazeDeparture> &BreachMazePreparedTransition::departure() const
{
    return m_departure;
}

const BreachMazeQuestEffects &BreachMazePreparedTransition::questEffects() const
{
    return m_effects;
}

bool BreachMazePreparedTransition::requiresSourceEntryReset() const
{
    return m_kind == BreachMazePlanKind::NewFloor || m_kind == BreachMazePlanKind::RetainedFloor;
}

BreachMazeLifecycle::BreachMazeLifecycle(BreachMazeTemplate mazeTemplate)
    : m_mazeTemplate(std::move(mazeTemplate))
{
    // Reuse the topology contract's structural validation. The importer/145 own original BLV identity.
    generateBreachMazeLayout(m_mazeTemplate, 1, 0);
    m_templateFingerprint = fingerprint(m_mazeTemplate);
    m_playableTemplateFingerprint = breachMazePopulationTemplateFingerprint(m_mazeTemplate);
}

uint64_t BreachMazeLifecycle::templateFingerprintForVersion(uint32_t version) const
{
    // Playable v3 retains v2's population codec template identity; interaction persistence changes no layout.
    // Foundation v1 retains its original independent identity and is never serialized as a playable save.
    return version == BreachMazePlayableStateVersion ? m_playableTemplateFingerprint : m_templateFingerprint;
}

void BreachMazeLifecycle::validateRuntime(const BreachMazeFloorRuntime &runtime, uint32_t version) const
{
    const IndoorSceneRuntime::Snapshot &scene = runtime.scene;
    require(scene.mapDeltaData.has_value(), "floor runtime lacks its authoritative delta");
    const MapDeltaData &delta = *scene.mapDeltaData;
    require(delta.faceAttributes.size() == m_mazeTemplate.faces.size(), "delta face count mismatch");
    require(delta.decorationFlags.size() == m_mazeTemplate.decorationCount, "delta decoration count mismatch");
    require(delta.locationInfo.totalFacesCount == m_mazeTemplate.faces.size(), "delta face metadata mismatch");
    require(delta.locationInfo.decorationCount == m_mazeTemplate.decorationCount,
        "delta decoration metadata mismatch");
    require(version == BreachMazePlayableStateVersion ? runtime.actorMaximumHitPoints.empty()
        : runtime.actorMaximumHitPoints.size() == delta.actors.size(), "invalid versioned actor HP authority");
    require(version == BreachMazePlayableStateVersion ? runtime.interaction.has_value() : !runtime.interaction,
        "invalid versioned maze interaction state");
    require(runtime.actorRage.size() == delta.actors.size(), "missing actor rage state");
    require(scene.worldRuntime.mapActorAiStates.size() == delta.actors.size(), "actor AI count mismatch");
    // Native corpse caches grow lazily through the last searched actor; absent trailing entries
    // have never materialized loot. Preserve that prefix rather than requiring or filling it.
    require(scene.worldRuntime.mapActorCorpseViews.size() <= delta.actors.size(), "actor corpse count mismatch");
    for (size_t index = 0; index < scene.worldRuntime.mapActorCorpseViews.size(); ++index)
    {
        const std::optional<GameplayCorpseViewState> &corpse = scene.worldRuntime.mapActorCorpseViews[index];
        require(!corpse || corpse->sourceIndex == index, "actor corpse source index mismatch");
    }
    require(scene.worldRuntime.mapActorSpellEffectStates.empty()
        || scene.worldRuntime.mapActorSpellEffectStates.size() == delta.actors.size(), "actor spell count mismatch");
    for (size_t index = 0; index < delta.actors.size(); ++index)
    {
        if (version == BreachMazeStateVersion)
        {
            require(runtime.actorMaximumHitPoints[index] > 0
                && runtime.actorMaximumHitPoints[index] <= uint32_t(std::numeric_limits<int32_t>::max()),
                "actor HP ceiling is outside the foundation protocol");
            require(delta.actors[index].hp <= 0
                || uint32_t(delta.actors[index].hp) <= runtime.actorMaximumHitPoints[index],
                "actor HP exceeds its foundation ceiling");
        }
        else
        {
            const auto &combat = scene.worldRuntime.mapActorAiStates[index].combatState;
            std::string error;
            require(combat.resolved() != nullptr && validateGameplayActorCombatState(combat, delta.actors[index].hp, error),
                "playable actor lacks valid resolved combat");
        }
        require(runtime.actorRage[index] <= 1, "invalid actor rage state");
        const IndoorWorldRuntime::MapActorAiState &actor = scene.worldRuntime.mapActorAiStates[index];
        requireFinite({actor.preciseX, actor.preciseY, actor.preciseZ, actor.homePreciseX, actor.homePreciseY,
            actor.homePreciseZ, actor.velocityX, actor.velocityY, actor.velocityZ, actor.yawRadians,
            actor.animationTimeTicks, actor.recoverySeconds, actor.attackCooldownSeconds});
        require(actor.supportFaceIndex == size_t(-1) || actor.supportFaceIndex < m_mazeTemplate.faces.size(),
            "actor support face is outside the template");
    }
    validateMovement(scene.partyRuntime.movementState, m_mazeTemplate.faces.size());
    if (scene.lastProcessedPartyMoveStateForFaceTriggers)
    {
        validateMovement(*scene.lastProcessedPartyMoveStateForFaceTriggers, m_mazeTemplate.faces.size());
    }
    require(!scene.lastPartyFloorFaceForPressurePlateTriggers
        || *scene.lastPartyFloorFaceForPressurePlateTriggers < m_mazeTemplate.faces.size(),
        "pressure plate face is outside the template");
    requireFinite({runtime.yawRadians, runtime.pitchRadians, scene.mechanismAccumulatorMilliseconds,
        scene.partyRuntime.movementAccumulatorSeconds,
        scene.worldRuntime.gameMinutes, scene.worldRuntime.actorUpdateAccumulatorSeconds});
    require(scene.partyRuntime.movementAccumulatorSeconds >= 0.0f
        && scene.partyRuntime.movementAccumulatorSeconds <= 1.0f / 128.0f + 0.000001f,
        "party movement accumulator is outside a native substep");
    require(validPartyJumpState(scene.partyRuntime.jumpState), "invalid party jump state");
    require(validGameMinutes(scene.worldRuntime.gameMinutes), "world clock is outside the native protocol");
    for (const IndoorSceneRuntime::TimerState &timer : scene.timers)
    {
        requireFinite({timer.nextAlarmGameMinutes, timer.eligibleGameMinutes, timer.migratedRemainingGameMinutes,
            timer.definition.intervalGameMinutes, timer.definition.initialDelayGameMinutes});
    }
    for (const IndoorWorldRuntime::BloodSplatState &blood : scene.worldRuntime.bloodSplats)
    {
        requireFinite({blood.x, blood.y, blood.z, blood.radius});
        require(blood.radius >= 0.0f, "negative blood radius");
        for (const IndoorWorldRuntime::BloodSplatState::Vertex &vertex : blood.vertices)
        {
            requireFinite({vertex.x, vertex.y, vertex.z, vertex.u, vertex.v});
        }
    }
}

void BreachMazeLifecycle::validate(const BreachMazeRunData &data) const
{
    require((data.version == BreachMazeStateVersion || data.version == BreachMazePlayableStateVersion)
        && data.seedVersion == BreachMazeSeedVersion, "unsupported state or seed version");
    if (data.version == BreachMazePlayableStateVersion)
    {
        std::string error;
        if (!validateBreachMazePlayableData(data, error)) throw std::invalid_argument(error);
    }
    if (data.epoch == 0)
    {
        require(data.templateFingerprint == 0 && data.revision == 0 && data.rootSeed == 0
            && data.nextGenerationSerial == 1 && data.currentLevel == 0 && !data.inside
            && data.direction == BreachMazeDirection::Fresh && data.floors.empty(), "noncanonical empty state");
        return;
    }
    require(data.templateFingerprint == templateFingerprintForVersion(data.version), "template identity mismatch");
    require(data.revision > 0 && data.nextGenerationSerial > 1, "invalid initialized state counters");
    require(data.currentLevel > 0 && data.currentLevel <= BreachMazeMaximumPlannedLevel, "invalid current level");
    require(data.floors.size() > 0 && data.floors.size() <= 3, "invalid retained floor count");
    require(data.floors.contains(data.currentLevel), "current floor is missing");
    if (data.inside)
    {
        require(data.direction == BreachMazeDirection::Fresh || data.direction == BreachMazeDirection::Up
            || data.direction == BreachMazeDirection::Down, "invalid active direction");
        require(data.direction != BreachMazeDirection::Fresh || data.currentLevel == 1 || data.currentLevel == 3,
            "fresh entry level is neither 1 nor 3");
    }
    else
    {
        require((data.direction == BreachMazeDirection::Exited && data.currentLevel == 1)
            || (data.version == BreachMazePlayableStateVersion && data.direction == BreachMazeDirection::Escaped),
            "invalid exited state");
    }
    std::set<uint64_t> serials;
    uint32_t previousLevel = 0;
    for (const auto &[level, floor] : data.floors)
    {
        const BreachMazeFloorIdentity &identity = floor.descriptor.identity;
        require(level > 0 && level <= BreachMazeMaximumPlannedLevel && identity.level == level,
            "floor key/identity mismatch");
        require(identity.epoch == data.epoch && identity.generationSerial > 0
            && identity.generationSerial < data.nextGenerationSerial, "floor generation identity mismatch");
        require(serials.insert(identity.generationSerial).second, "duplicate floor generation serial");
        require(previousLevel == 0 || level == previousLevel + 1, "retained floors are not contiguous");
        previousLevel = level;
        const BreachMazeFloorSeeds expectedSeeds = deriveBreachMazeFloorSeeds(data.rootSeed, identity);
        require(floor.descriptor.seeds == expectedSeeds, "floor seed provenance mismatch");
        require(floor.descriptor.layout == generateBreachMazeLayout(m_mazeTemplate, level, expectedSeeds.layout),
            "saved layout differs from its versioned generation");
        require(data.version == BreachMazePlayableStateVersion || !floor.population,
            "foundation protocol cannot carry a playable population");
        validateRuntime(floor.runtime, data.version);
    }
}

BreachMazeState BreachMazeLifecycle::restore(BreachMazeRunData data) const
{
    validate(data);
    return BreachMazeState(std::make_shared<const BreachMazeRunData>(std::move(data)));
}

const BreachMazeFloorState &BreachMazeLifecycle::coldRestore(const BreachMazeState &state) const
{
    require(state.data().templateFingerprint == templateFingerprintForVersion(state.data().version), "cold restore template mismatch");
    const BreachMazeFloorState *pFloor = state.activeFloor();
    require(pFloor != nullptr, "cold restore requires an active floor");
    // No new layout, stream advance, source entry reset, position edit or quest effect.
    return *pFloor;
}

void BreachMazeLifecycle::selectTarget(BreachMazePreparedTransition &prepared, bool useExit) const
{
    BreachMazeRunData &candidate = *prepared.m_candidate;
    const auto found = candidate.floors.find(candidate.currentLevel);
    if (found != candidate.floors.end())
    {
        prepared.m_kind = BreachMazePlanKind::RetainedFloor;
        // TriggerReload's MoveToMap sets Direction/LookAngle to zero; LoadMazeData restores XYZ only.
        // ColdRestore bypasses this real travel path and preserves both saved angles.
        found->second.runtime.yawRadians = 0.0f;
        found->second.runtime.pitchRadians = 0.0f;
        prepared.m_arrival = savedArrival(found->second.runtime);
        return;
    }
    BreachMazeFloorDescriptor descriptor;
    descriptor.identity = {candidate.epoch, candidate.currentLevel, candidate.nextGenerationSerial};
    candidate.nextGenerationSerial = increment(candidate.nextGenerationSerial, "generation serial exhausted");
    descriptor.seeds = deriveBreachMazeFloorSeeds(candidate.rootSeed, descriptor.identity);
    descriptor.layout = generateBreachMazeLayout(m_mazeTemplate, candidate.currentLevel, descriptor.seeds.layout);
    const BreachMazeEndpoint &endpoint = useExit
        ? descriptor.layout.endpoints.exit : descriptor.layout.endpoints.entrance;
    const BreachMazeTemplateCell &cell = m_mazeTemplate.cells.at(endpoint.templateCellId - 1);
    prepared.m_kind = BreachMazePlanKind::NewFloor;
    prepared.m_arrival = {useExit ? BreachMazeArrivalKind::Exit : BreachMazeArrivalKind::Entrance,
        float((cell.x - 1) * BreachMazeCellWidth), float(-(cell.y - 1) * BreachMazeCellWidth), -200.0f, 0.0f, 0.0f};
    prepared.m_newFloor = std::move(descriptor);
}

BreachMazePreparedTransition BreachMazeLifecycle::prepareFreshEntry(const BreachMazeState &state,
    BreachMazeEntry entry, BreachMazeQuestView quest, uint64_t rootSeed) const
{
    validate(state.data());
    require(!state.data().inside, "fresh entry cannot replace an active floor");
    require(entry == BreachMazeEntry::BrAlvar53 || entry == BreachMazeEntry::BrAlvar54
        || entry == BreachMazeEntry::Breach54, "unknown entry");
    BreachMazePreparedTransition prepared;
    prepared.m_expected = state.m_data;
    prepared.m_quest = quest;
    prepared.m_candidate.emplace();
    BreachMazeRunData &candidate = *prepared.m_candidate;
    candidate.version = state.data().version;
    candidate.templateFingerprint = templateFingerprintForVersion(candidate.version);
    candidate.revision = increment(state.data().revision, "revision exhausted");
    candidate.epoch = increment(state.data().epoch, "epoch exhausted");
    candidate.rootSeed = rootSeed;
    candidate.inside = true;
    // Source 53 resets EnteredBasement before OnLoad; 54 deliberately leaves it alone.
    const bool firstBasement = quest.present && !quest.questFinished
        && (entry == BreachMazeEntry::BrAlvar53 || !quest.enteredBasement);
    candidate.currentLevel = firstBasement ? 3 : 1;
    prepared.m_effects.setEnteredBasement = quest.present;
    selectTarget(prepared, firstBasement);
    return prepared;
}

BreachMazePreparedTransition BreachMazeLifecycle::prepareStep(const BreachMazeState &state,
    BreachMazeStep step, BreachMazeQuestView quest, const BreachMazeFloorRuntime &leavingRuntime) const
{
    validate(state.data());
    require(state.data().inside, "step requires an active floor");
    require(step == BreachMazeStep::Up || step == BreachMazeStep::Down, "unknown step");
    BreachMazePreparedTransition prepared;
    prepared.m_expected = state.m_data;
    prepared.m_quest = quest;
    if (step == BreachMazeStep::Down && quest.present && !quest.questFinished)
    {
        return prepared; // Source lock precedes capture, random draws and any travel dialog.
    }
    validateRuntime(leavingRuntime, state.data().version);
    prepared.m_candidate = state.data();
    BreachMazeRunData &candidate = *prepared.m_candidate;
    candidate.revision = increment(candidate.revision, "revision exhausted");
    const uint32_t oldLevel = candidate.currentLevel;
    candidate.floors.at(oldLevel).runtime = leavingRuntime;
    if (step == BreachMazeStep::Up && oldLevel == 1)
    {
        candidate.inside = false;
        candidate.direction = BreachMazeDirection::Exited;
        prepared.m_kind = BreachMazePlanKind::LeaveMaze;
        prepared.m_effects.setEnteredBreach = quest.present;
        if (quest.present)
        {
            prepared.m_departure = BreachMazeDeparture{
                quest.questFinished ? "Breach.odm" : "BrAlvar.odm", 1578, 899, 847, 1160};
        }
        else
        {
            prepared.m_departure = BreachMazeDeparture{"out02.odm", 20487, -4443, 2, 1167};
        }
        return prepared;
    }
    if (step == BreachMazeStep::Down)
    {
        require(oldLevel < BreachMazeMaximumPlannedLevel, "next level exceeds the planning protocol");
    }
    // Exact source order/key semantics: prune old Cur+2 and old Cur-2, not new current +/-1.
    // All of this remains private until the target is ready and commit succeeds.
    candidate.floors.erase(oldLevel + 2);
    if (oldLevel > 2)
    {
        candidate.floors.erase(oldLevel - 2);
    }
    candidate.currentLevel = step == BreachMazeStep::Up ? oldLevel - 1 : oldLevel + 1;
    candidate.direction = step == BreachMazeStep::Up ? BreachMazeDirection::Up : BreachMazeDirection::Down;
    prepared.m_effects.setEnteredBasement = quest.present;
    selectTarget(prepared, step == BreachMazeStep::Up);
    return prepared;
}

BreachMazePreparedTransition BreachMazeLifecycle::preparePortalDeparture(const BreachMazeState &state,
    BreachMazeQuestView quest, const BreachMazeFloorRuntime &leavingRuntime) const
{
    validate(state.data());
    require(state.data().version == BreachMazePlayableStateVersion,
        "portal departure requires the current playable protocol");
    require(state.data().inside, "portal departure requires an active floor");
    validateRuntime(leavingRuntime, state.data().version);
    BreachMazePreparedTransition prepared;
    prepared.m_expected = state.m_data;
    prepared.m_quest = quest;
    prepared.m_kind = BreachMazePlanKind::PortalDeparture;
    prepared.m_candidate = state.data();
    BreachMazeRunData &candidate = *prepared.m_candidate;
    candidate.revision = increment(candidate.revision, "revision exhausted");
    candidate.floors.at(candidate.currentLevel).runtime = leavingRuntime;
    candidate.inside = false;
    candidate.direction = BreachMazeDirection::Escaped;
    return prepared;
}

BreachMazeCommitResult BreachMazeLifecycle::commit(BreachMazeState &state, BreachMazePreparedTransition &prepared,
    bool confirmed, BreachMazeQuestView currentQuest, const BreachMazeReadyFloor *pReadyFloor) const
{
    if (prepared.m_consumed || !prepared.m_expected)
    {
        return BreachMazeCommitResult::Consumed;
    }
    if (state.m_data != prepared.m_expected)
    {
        return BreachMazeCommitResult::StaleState;
    }
    if (currentQuest != prepared.m_quest)
    {
        return BreachMazeCommitResult::StaleQuest;
    }
    if (prepared.m_kind == BreachMazePlanKind::DownLocked)
    {
        prepared.m_consumed = true;
        return BreachMazeCommitResult::DownLocked;
    }
    if (!confirmed)
    {
        prepared.m_consumed = true;
        return BreachMazeCommitResult::Cancelled;
    }
    require(prepared.m_candidate.has_value(), "transition has no candidate");
    // Copy before validation/insertion: allocation, invalid payload or resource failure never
    // alters the live record or prevents a caller retry with the same prepared generation.
    BreachMazeRunData candidate = *prepared.m_candidate;
    if (prepared.m_kind == BreachMazePlanKind::NewFloor)
    {
        require(pReadyFloor != nullptr && prepared.m_newFloor.has_value(), "new floor is not ready");
        require(pReadyFloor->identity == prepared.m_newFloor->identity, "ready floor identity mismatch");
        validateRuntime(pReadyFloor->runtime, candidate.version);
        const BreachMazeArrival pose = savedArrival(pReadyFloor->runtime);
        require(pose.x == prepared.m_arrival.x && pose.y == prepared.m_arrival.y
            && pose.footZ == prepared.m_arrival.footZ && pose.yawRadians == prepared.m_arrival.yawRadians
            && pose.pitchRadians == prepared.m_arrival.pitchRadians, "new floor was not staged at its source endpoint");
        candidate.floors.emplace(candidate.currentLevel,
            BreachMazeFloorState{*prepared.m_newFloor, pReadyFloor->runtime, pReadyFloor->population});
    }
    else
    {
        require(pReadyFloor == nullptr, "retained/departure transition cannot replace its saved runtime");
    }
    validate(candidate);
    std::shared_ptr<const BreachMazeRunData> committed =
        std::make_shared<const BreachMazeRunData>(std::move(candidate));
    state.m_data = std::move(committed); // The sole live-state write; everything throwable is finished.
    prepared.m_consumed = true;
    return BreachMazeCommitResult::Committed;
}

void BreachMazeLifecycle::captureCurrentFloor(BreachMazeState &state, const BreachMazeFloorRuntime &runtime) const
{
    require(state.data().templateFingerprint == templateFingerprintForVersion(state.data().version) && state.data().inside,
        "capture requires an active floor from this template");
    validateRuntime(runtime, state.data().version);
    BreachMazeRunData candidate = state.data();
    candidate.revision = increment(candidate.revision, "revision exhausted");
    candidate.floors.at(candidate.currentLevel).runtime = runtime;
    validate(candidate);
    state.m_data = std::make_shared<const BreachMazeRunData>(std::move(candidate));
}
}
