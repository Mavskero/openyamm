#pragma once

#include "game/gameplay/MergeBolsterProfile.h"

#include <string>
#include <variant>

namespace OpenYAMM::Game
{
inline constexpr uint32_t GameplayActorCombatSchemaVersion = 1;
inline constexpr uint32_t GameplayActorMazeDeadEndDoubleHp = 1;

enum class GameplayActorCombatMode : uint8_t
{
    LegacyDynamic = 0,
    LegacySnapshot = 1,
    MergeResolvedV1 = 2,
};

struct GameplayActorLegacyDynamic
{
    bool operator==(const GameplayActorLegacyDynamic &) const = default;
};

struct GameplayActorLegacySnapshot
{
    int32_t maximumHitPoints = 0;
    bool operator==(const GameplayActorLegacySnapshot &) const = default;
};

struct GameplayMergeResolvedActorCombat
{
    uint32_t schemaVersion = GameplayActorCombatSchemaVersion;
    uint32_t profileVersion = MergeBolsterProfileVersion;
    uint32_t randomVersion = MergeBolsterRandomVersion;
    std::string contentPackageId;
    uint32_t contentPackageSchema = 0;
    // SHA256 of the original mechanical catalog and formula profile inputs.
    std::string catalogSha256;
    std::string profileSha256;
    uint64_t familySeed = 0;
    uint64_t actorSeed = 0;
    uint64_t generationEpoch = 0;
    uint32_t generationLevel = 0;
    uint64_t generationSerial = 0;
    uint32_t appliedModifiers = 0;
    // These are the complete inputs used by the profile's playerArmorClass formula.
    // The resolved monster level comes exclusively from combat.level.
    bool playerArmorClassPrepared = false;
    double bolsterAmountPercent = 100;
    double playerArmorClassMultiplier = 1;
    MergeBolsterCombatStats combat;
    bool operator==(const GameplayMergeResolvedActorCombat &) const = default;
};

// Mutable Special lifecycle. Source Special values remain exclusively in immutable combat.
struct GameplayActorSpecialRuntimeState
{
    uint32_t summonedCount = 0; // Successful summons in addition to source combat.special.c.
    uint32_t replicatedCount = 0; // Successful replications in addition to source combat.special.a.
    uint64_t randomCursor = 0; // V1 Special stream position, keyed by resolved.actorSeed.
    bool replicationPending = false;
    double replicationDueGameMinutes = 0.0;
    float materializeRemainingSeconds = 0.0f;
    bool operator==(const GameplayActorSpecialRuntimeState &) const = default;
};

// Current HP belongs to the world actor/delta. This variant exclusively owns its resolved cap.
class GameplayActorCombatState
{
public:
    GameplayActorCombatState() = default;
    explicit GameplayActorCombatState(GameplayActorLegacySnapshot legacy);
    explicit GameplayActorCombatState(GameplayMergeResolvedActorCombat resolved);
    GameplayActorCombatMode mode() const;
    int32_t maximumHitPoints(int32_t legacyDynamicDefault = 0) const;
    // Returns false without mutation for LegacyDynamic or MergeResolvedV1.
    bool setLegacyMaximumHitPoints(int32_t value);
    const GameplayMergeResolvedActorCombat *resolved() const;
    const GameplayActorLegacySnapshot *legacySnapshot() const;
    bool operator==(const GameplayActorCombatState &) const = default;

private:
    std::variant<GameplayActorLegacyDynamic, GameplayActorLegacySnapshot, GameplayMergeResolvedActorCombat> m_value;
};

// Data-only validation: actual catalog/resource existence is checked by the loaded-content restore candidate.
bool validateGameplayActorCombatState(const GameplayActorCombatState &state, int32_t currentHitPoints,
    std::string &error);
bool validateGameplayActorSpecialRuntimeState(const GameplayActorSpecialRuntimeState &state,
    const GameplayActorCombatState &combat, std::string &error);
int32_t gameplayResolvedPlayerArmorClass(const GameplayMergeResolvedActorCombat &resolved,
    int32_t playerLevelBase, int32_t originalArmorClass);
}
