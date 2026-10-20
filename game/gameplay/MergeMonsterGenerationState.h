#pragma once

#include "game/gameplay/GameplayActorCombatState.h"
#include "game/gameplay/MergeMonsterSourceProfile.h"

namespace OpenYAMM::Game
{
inline constexpr uint32_t MergeMonsterGenerationStateVersion = 1;
inline constexpr uint32_t MergeMonsterGenerationSeedVersion = 1;
inline constexpr std::string_view MergeMonsterGenerationProfileSha256 =
    "f0be4d23cb7c8f504e57fa83a1ae1dc03230a8a72b6227bb9aa435d23a8db539";

// Serializable B2 plan. Restore reconstructs a private B2 plan from this saved input and checks
// its result against these saved templates; existing actors are never recalculated.
struct MergeMonsterPreparedFamily
{
    uint16_t familyId = 0;
    MergeBolsterFamilyInput input;
    std::array<MergeBolsterCombatStats, 3> templates;
    int32_t familyLevel = 0;
    std::optional<uint8_t> bolsterStep;
    uint64_t rawDrawCount = 0;
    bool operator==(const MergeMonsterPreparedFamily &) const = default;
};

struct MergeMonsterGenerationState
{
    uint32_t version = MergeMonsterGenerationStateVersion;
    uint32_t sourceProfileVersion = MergeMonsterSourceProfileVersion;
    uint32_t sourceContentFixVersion = MergeMonsterSourceContentFixVersion;
    uint32_t bolsterProfileVersion = MergeBolsterProfileVersion;
    uint32_t bolsterRandomVersion = MergeBolsterRandomVersion;
    uint32_t seedVersion = MergeMonsterGenerationSeedVersion;
    std::string contentPackageId;
    uint32_t contentPackageSchema = 0;
    std::string catalogSha256;
    std::string profileSha256;
    uint64_t mechanicalFingerprint = 0;
    uint64_t descriptorFingerprint = 0;
    // Fixed generation context, not the current party. Its familySeed is always zero; each
    // saved family's input carries its own derived seed.
    MergeMonsterFamilyContext context;
    uint64_t populationSeed = 0;
    uint64_t generationEpoch = 0;
    uint32_t generationLevel = 0;
    uint64_t generationSerial = 0;
    uint64_t nextActorOrdinal = 0;
    std::vector<MergeMonsterPreparedFamily> preparedFamilies; // Strictly ascending familyId.
    bool operator==(const MergeMonsterGenerationState &) const = default;
};

// Runtime references only; none are serialized or retained by a generation state.
struct MergeMonsterGenerationServices
{
    const MergeMonsterSourceProfile &source;
    const MergeBolsterProfile &bolster;
    const MergedBolsterMonsterTable &settings;
    const MonsterProjectileTable &projectiles;
    const SpellTable &spells;
};

struct MergeMonsterGenerationOrigin
{
    std::string contentPackageId;
    uint32_t contentPackageSchema = 0;
    MergeMonsterFamilyContext context;
    uint64_t populationSeed = 0;
    uint64_t generationEpoch = 0;
    uint32_t generationLevel = 0;
    uint64_t generationSerial = 0;
};

enum class MergeMonsterGenerationMode : uint8_t
{
    LuaSummon, LuaSummonWithTreasure, NativeSpecial,
};

MergeMonsterGenerationState makeMergeMonsterGenerationState(const MergeMonsterGenerationServices &services,
    const MergeMonsterGenerationOrigin &origin);
uint64_t mergeMonsterGenerationFamilySeed(const MergeMonsterGenerationState &state, uint16_t familyId);
uint64_t mergeMonsterGenerationActorSeed(const MergeMonsterGenerationState &state, uint64_t ordinal);

// Save codec validation is data-only. Loaded validation additionally authenticates all saved family
// inputs/resources against this source profile and exactly reproduces each saved immutable plan.
bool validateMergeMonsterGenerationState(const MergeMonsterGenerationState &state, std::string &error);
bool validateMergeMonsterGenerationResources(const MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, std::string &error);

// All preparation commits atomically and advances nextActorOrdinal exactly once on success.
// Lua order is StdSummonBefore -> first prepareFamily -> prepareActor. No maze HP/gold overrides
// are inherited. NativeSpecial copies a prepared template and clears the native three reward fields.
GameplayActorCombatState prepareMergeMonsterGenerationActor(MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, uint16_t monsterId,
    MergeMonsterGenerationMode mode = MergeMonsterGenerationMode::LuaSummon);
GameplayActorCombatState prepareMergeMonsterSummon(MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, const GameplayMergeResolvedActorCombat &parent,
    uint16_t monsterId, MergeMonsterGenerationMode mode);
}
