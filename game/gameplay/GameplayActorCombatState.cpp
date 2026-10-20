#include "game/gameplay/GameplayActorCombatState.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace OpenYAMM::Game
{
GameplayActorCombatState::GameplayActorCombatState(GameplayActorLegacySnapshot legacy) : m_value(legacy)
{
}

GameplayActorCombatState::GameplayActorCombatState(GameplayMergeResolvedActorCombat resolved)
    : m_value(std::move(resolved))
{
}

GameplayActorCombatMode GameplayActorCombatState::mode() const
{
    return static_cast<GameplayActorCombatMode>(m_value.index());
}

int32_t GameplayActorCombatState::maximumHitPoints(int32_t legacyDynamicDefault) const
{
    if (const GameplayMergeResolvedActorCombat *pResolved = resolved())
    {
        return pResolved->combat.fullHitPoints;
    }
    if (const GameplayActorLegacySnapshot *pLegacy = legacySnapshot())
    {
        return pLegacy->maximumHitPoints;
    }
    return legacyDynamicDefault;
}

bool GameplayActorCombatState::setLegacyMaximumHitPoints(int32_t value)
{
    GameplayActorLegacySnapshot *pLegacy = std::get_if<GameplayActorLegacySnapshot>(&m_value);
    if (pLegacy == nullptr)
    {
        return false;
    }
    pLegacy->maximumHitPoints = value;
    return true;
}

const GameplayMergeResolvedActorCombat *GameplayActorCombatState::resolved() const
{
    return std::get_if<GameplayMergeResolvedActorCombat>(&m_value);
}

const GameplayActorLegacySnapshot *GameplayActorCombatState::legacySnapshot() const
{
    return std::get_if<GameplayActorLegacySnapshot>(&m_value);
}

bool validateGameplayActorCombatState(const GameplayActorCombatState &state, int32_t currentHitPoints,
    std::string &error)
{
    const GameplayMergeResolvedActorCombat *pResolved = state.resolved();
    if (pResolved == nullptr)
    {
        return true;
    }
    const GameplayMergeResolvedActorCombat &resolved = *pResolved;
    const auto validSha256 = [](const std::string &value)
    {
        return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char digit)
        {
            return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
        });
    };
    if (resolved.schemaVersion != GameplayActorCombatSchemaVersion
        || resolved.profileVersion != MergeBolsterProfileVersion
        || resolved.randomVersion != MergeBolsterRandomVersion
        || resolved.contentPackageId.empty() || resolved.contentPackageSchema == 0
        || !validSha256(resolved.catalogSha256) || !validSha256(resolved.profileSha256))
    {
        error = "Invalid resolved actor combat version or source identity";
        return false;
    }
    const bool hasGeneration = resolved.generationEpoch != 0 || resolved.generationLevel != 0
        || resolved.generationSerial != 0;
    if ((hasGeneration && (resolved.generationEpoch == 0 || resolved.generationLevel == 0
            || resolved.generationSerial == 0))
        || (resolved.appliedModifiers & ~GameplayActorMazeDeadEndDoubleHp) != 0
        || !std::isfinite(resolved.bolsterAmountPercent) || resolved.bolsterAmountPercent < 0
        || resolved.bolsterAmountPercent > 200 || !std::isfinite(resolved.playerArmorClassMultiplier)
        || resolved.playerArmorClassMultiplier < 0)
    {
        error = "Invalid resolved actor generation, modifier or player AC state";
        return false;
    }
    const MergeBolsterCombatStats &combat = resolved.combat;
    if (combat.id == 0 || combat.fullHitPoints <= 0 || currentHitPoints > combat.fullHitPoints
        || combat.armorClass < 0 || combat.moveSpeed < 0 || combat.attackRecovery < 0
        || combat.rewards.experience < 0 || combat.fly > 1 || combat.moveType > 5 || combat.aiType > 3)
    {
        error = "Invalid resolved actor combat values or current HP above its persisted cap";
        return false;
    }
    for (const MergeBolsterSpell &spell : combat.spells)
    {
        const uint16_t masteryBits = spell.packedSkill & 0xff00;
        if (masteryBits != 0 && masteryBits != 0x400 && masteryBits != 0x800 && masteryBits != 0x1000)
        {
            error = "Invalid resolved actor Merge spell skill encoding";
            return false;
        }
    }
    return true;
}

bool validateGameplayActorSpecialRuntimeState(const GameplayActorSpecialRuntimeState &state,
    const GameplayActorCombatState &combat, std::string &error)
{
    if (state.summonedCount > 255 || state.replicatedCount > 255
        || !std::isfinite(state.replicationDueGameMinutes) || state.replicationDueGameMinutes < 0
        || !std::isfinite(state.materializeRemainingSeconds) || state.materializeRemainingSeconds < 0)
    {
        error = "Invalid actor Special count or lifecycle time";
        return false;
    }
    const GameplayMergeResolvedActorCombat *pResolved = combat.resolved();
    if (pResolved == nullptr)
    {
        if (state != GameplayActorSpecialRuntimeState{})
        {
            error = "Legacy actor cannot own source-resolved Special runtime state";
            return false;
        }
        return true;
    }
    if ((state.summonedCount != 0 && pResolved->combat.special.kind != 2)
        || ((state.replicatedCount != 0 || state.replicationPending) && pResolved->combat.special.kind != 4))
    {
        error = "Actor Special runtime state does not match its immutable source kind";
        return false;
    }
    return true;
}

int32_t gameplayResolvedPlayerArmorClass(const GameplayMergeResolvedActorCombat &resolved,
    int32_t playerLevelBase, int32_t originalArmorClass)
{
    if (playerLevelBase <= 0 || originalArmorClass < 0)
    {
        throw std::invalid_argument("Resolved actor PlayerAC requires positive LevelBase and nonnegative AC");
    }
    if (!resolved.playerArmorClassPrepared)
    {
        return originalArmorClass;
    }
    const double amount = resolved.bolsterAmountPercent / 100;
    const double exponent = std::clamp(2 - amount * amount, 0.1, 1.0);
    const double base = originalArmorClass * std::min(double(resolved.combat.level) / playerLevelBase, 1.0);
    const double formula = std::pow(base, exponent);
    const double value = originalArmorClass + (formula - originalArmorClass) * resolved.playerArmorClassMultiplier;
    const double integral = std::trunc(std::max(value, 0.0));
    if (!std::isfinite(value) || integral > std::numeric_limits<int32_t>::max())
    {
        throw std::range_error("Resolved actor PlayerAC is not representable");
    }
    return static_cast<int32_t>(integral);
}
}
