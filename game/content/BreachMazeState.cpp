#include "game/content/BreachMazeState.h"
#include "game/events/GameTime.h"

#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
uint64_t mix(uint64_t value)
{
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}
}

BreachMazeFloorSeeds deriveBreachMazeFloorSeeds(uint64_t rootSeed, const BreachMazeFloorIdentity &identity)
{
    if (identity.epoch == 0 || identity.generationSerial == 0 || identity.level == 0
        || identity.level > BreachMazeMaximumPlannedLevel)
    {
        throw std::invalid_argument("Breach maze seed identity is outside the planning protocol");
    }
    // Version 1: SplitMix64 finalizer with four ordered identity folds and four fixed domain tags.
    // Unsigned multiplication/addition wrap modulo 2^64. No std::hash, entropy or Lua PRNG emulation.
    uint64_t folded = mix(rootSeed ^ UINT64_C(0x42524d415a450001));
    folded = mix(folded ^ identity.epoch);
    folded = mix(folded ^ identity.generationSerial);
    folded = mix(folded ^ uint64_t(identity.level));
    return {
        mix(folded ^ UINT64_C(0x4c41594f55540001)),
        mix(folded ^ UINT64_C(0x4d41544552490001)),
        mix(folded ^ UINT64_C(0x504f50554c410001)),
        mix(folded ^ UINT64_C(0x494e544552410001)),
    };
}

namespace
{
bool validatePlayableDataForVersion(const BreachMazeRunData &data, uint32_t expectedVersion, std::string &error)
{
    error.clear();
    const auto require = [](bool condition, const char *pMessage)
    {
        if (!condition) throw std::invalid_argument(pMessage);
    };
    try
    {
        require(data.version == expectedVersion && data.seedVersion == BreachMazeSeedVersion,
            "unsupported playable maze state or seed version");
        if (data.epoch == 0)
        {
            require(data.templateFingerprint == 0 && data.revision == 0 && data.rootSeed == 0
                && data.nextGenerationSerial == 1 && data.currentLevel == 0 && !data.inside
                && data.direction == BreachMazeDirection::Fresh && data.floors.empty(), "noncanonical empty maze");
            return true;
        }
        require(data.templateFingerprint != 0 && data.revision > 0 && data.nextGenerationSerial > 1,
            "invalid maze identity or counters");
        require(!data.floors.empty() && data.floors.size() <= 3 && data.floors.contains(data.currentLevel),
            "invalid maze floor count or missing current floor");
        require(data.inside
            ? (data.direction == BreachMazeDirection::Up || data.direction == BreachMazeDirection::Down
                || (data.direction == BreachMazeDirection::Fresh && (data.currentLevel == 1 || data.currentLevel == 3)))
            : ((data.direction == BreachMazeDirection::Exited && data.currentLevel == 1)
                || (expectedVersion == BreachMazePlayableStateVersion
                    && data.direction == BreachMazeDirection::Escaped)), "invalid maze direction");
        std::set<uint64_t> serials;
        uint32_t previousLevel = 0;
        for (const auto &[level, floor] : data.floors)
        {
            const BreachMazeFloorDescriptor &descriptor = floor.descriptor;
            require(level >= 1 && level <= BreachMazeMaximumPlannedLevel && descriptor.identity.level == level
                && descriptor.identity.epoch == data.epoch && descriptor.identity.generationSerial > 0
                && descriptor.identity.generationSerial < data.nextGenerationSerial
                && serials.insert(descriptor.identity.generationSerial).second, "invalid maze floor identity");
            require(previousLevel == 0 || level == previousLevel + 1, "maze retained floors are not contiguous");
            previousLevel = level;
            require(descriptor.seeds == deriveBreachMazeFloorSeeds(data.rootSeed, descriptor.identity),
                "maze seed provenance mismatch");
            const BreachMazeLayout &layout = descriptor.layout;
            require(layout.algorithmVersion == BreachMazeAlgorithmVersion && layout.seed == descriptor.seeds.layout
                && layout.level == level && layout.side >= 4 && layout.side <= BreachMazePopulationMaximumSide
                && layout.cells.size() == size_t(layout.side) * layout.side, "invalid saved maze layout");
            for (size_t cellIndex = 0; cellIndex < layout.cells.size(); ++cellIndex)
            {
                const BreachMazeCell &cell = layout.cells[cellIndex];
                require(cell.templateCellId == (cellIndex / layout.side) * BreachMazeTemplateSide
                    + cellIndex % layout.side + 1 && cell.room > 0 && cell.room <= BreachMazeTemplateSide,
                    "invalid saved maze cell identity or material room");
            }
            require(floor.population.has_value(), "playable maze floor lacks its frozen population plan");
            const BreachMazePopulationPlan &plan = *floor.population;
            require(plan.version == BreachMazePopulationVersion
                && plan.contentFixVersion == BreachMazePopulationContentFixVersion
                && plan.identity == descriptor.identity && plan.seeds == descriptor.seeds
                && plan.templateFingerprint == data.templateFingerprint && plan.catalogFingerprint != 0
                && plan.layoutFinalRandomState == layout.finalRandomState
                && plan.layoutRandomDrawCount == layout.randomDrawCount, "maze population provenance mismatch");
            const BreachMazeFloorRuntime &runtime = floor.runtime;
            if (expectedVersion == BreachMazePlayableStateVersion)
            {
                require(runtime.interaction.has_value(), "playable maze floor lacks its interaction state");
                std::string interactionError;
                if (!validateBreachMazeInteractionState(*runtime.interaction, plan, interactionError))
                    throw std::invalid_argument(interactionError);
            }
            else
            {
                require(!runtime.interaction, "legacy maze unexpectedly contains interaction state");
            }
            require(runtime.actorMaximumHitPoints.empty(), "playable maze cannot have duplicate legacy HP ceilings");
            require(std::isfinite(runtime.yawRadians) && std::isfinite(runtime.pitchRadians), "invalid maze camera");
            require(validGameMinutes(runtime.scene.worldRuntime.gameMinutes), "invalid maze floor clock");
            require(runtime.scene.mapDeltaData.has_value(), "maze floor lacks its saved delta");
            const MapDeltaData &delta = *runtime.scene.mapDeltaData;
            const auto &actors = runtime.scene.worldRuntime.mapActorAiStates;
            require(delta.actors.size() == actors.size() && actors.size() == runtime.actorRage.size()
                && actors.size() >= plan.actors.size(), "maze actor delta/AI/rage count mismatch");
            require(delta.faceAttributes.size() == delta.locationInfo.totalFacesCount
                && delta.decorationFlags.size() == BreachMazeTemplateCellCount
                && delta.locationInfo.decorationCount == BreachMazeTemplateCellCount, "maze delta metadata mismatch");
            for (const BreachMazeEndpoint *pEndpoint : {&layout.endpoints.entrance, &layout.endpoints.exit})
            {
                require(pEndpoint->templateCellId > 0 && pEndpoint->templateCellId <= BreachMazeTemplateCellCount
                    && pEndpoint->faceIndex < delta.faceAttributes.size()
                    && uint8_t(pEndpoint->wallSide) <= uint8_t(BreachMazeSide::South), "invalid maze endpoint");
            }
            require(uint8_t(layout.endpoints.entranceSearchSide) <= uint8_t(BreachMazeSide::South)
                && uint8_t(layout.endpoints.exitSearchSide) <= uint8_t(BreachMazeSide::South),
                "invalid maze endpoint search side");
            const auto &generation = runtime.scene.worldRuntime.mergeMonsterGeneration;
            require(generation.has_value(), "playable maze lacks its monster generation context");
            require(generation->generationEpoch == descriptor.identity.epoch
                && generation->generationLevel == level
                && generation->generationSerial == descriptor.identity.generationSerial
                && generation->populationSeed == descriptor.seeds.population, "maze generation identity mismatch");
            for (size_t index = 0; index < actors.size(); ++index)
            {
                const auto &actor = actors[index];
                const auto *pResolved = actor.combatState.resolved();
                require(actor.actorId == index && pResolved != nullptr && runtime.actorRage[index] <= 1,
                    "playable maze actor is not resolved or has invalid identity/rage");
                require(pResolved->combat.id == delta.actors[index].monsterInfoId
                    && pResolved->generationEpoch == descriptor.identity.epoch
                    && pResolved->generationLevel == level
                    && pResolved->generationSerial == descriptor.identity.generationSerial,
                    "maze actor source identity mismatch");
                if (index < plan.actors.size())
                {
                    require(plan.actors[index].ordinal == index
                        && plan.actors[index].monsterId == pResolved->combat.id,
                        "maze initial actor differs from its frozen population");
                }
                std::string actorError;
                require(validateGameplayActorCombatState(actor.combatState, delta.actors[index].hp, actorError),
                    "invalid maze actor combat state or current HP");
            }
        }
        return true;
    }
    catch (const std::exception &exception)
    {
        error = std::string("Breach maze data: ") + exception.what();
        return false;
    }
}
}

bool validateBreachMazePlayableData(const BreachMazeRunData &data, std::string &error)
{
    return validatePlayableDataForVersion(data, BreachMazePlayableStateVersion, error);
}

bool migrateBreachMazeV2Interactions(BreachMazeRunData &data, std::string &error)
{
    if (!validatePlayableDataForVersion(data, BreachMazeLegacyPlayableStateVersion, error)) return false;
    try
    {
        BreachMazeRunData candidate = data;
        for (auto &[level, floor] : candidate.floors)
        {
            floor.runtime.interaction = makeBreachMazeInteractionState(*floor.population);
        }
        candidate.version = BreachMazePlayableStateVersion;
        if (!validateBreachMazePlayableData(candidate, error)) return false;
        data = std::move(candidate);
        return true;
    }
    catch (const std::exception &exception)
    {
        error = std::string("Breach maze v2 interaction migration: ") + exception.what();
        return false;
    }
}

BreachMazeState::BreachMazeState() : m_data(std::make_shared<const BreachMazeRunData>())
{
}

BreachMazeState::BreachMazeState(std::shared_ptr<const BreachMazeRunData> data) : m_data(std::move(data))
{
}

BreachMazeState BreachMazeState::playable()
{
    BreachMazeRunData data;
    data.version = BreachMazePlayableStateVersion;
    return BreachMazeState(std::make_shared<const BreachMazeRunData>(std::move(data)));
}

const BreachMazeRunData &BreachMazeState::data() const
{
    return *m_data;
}

const BreachMazeFloorState *BreachMazeState::activeFloor() const
{
    if (!m_data->inside)
    {
        return nullptr;
    }
    const auto found = m_data->floors.find(m_data->currentLevel);
    return found != m_data->floors.end() ? &found->second : nullptr;
}
}
