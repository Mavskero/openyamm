#pragma once

#include "game/gameplay/GameplayActorCombatState.h"
#include "game/gameplay/GameplayBolsterRuntime.h"

namespace OpenYAMM::Game
{
enum class CombatDamageType;
class MonsterProjectileTable;
class SpellTable;

// Immutable translation for existing combat consumers. Names, sound and kind tags come from
// presentation; every CommonMonsterProps combat value comes from the resolved actor payload.
// This is a disposable cache, never another saved or writable monster-stat authority.
class MergeMonsterRuntimeView
{
public:
    static std::optional<MergeMonsterRuntimeView> fromCombat(
        const GameplayMergeResolvedActorCombat &resolved,
        const MonsterTable::MonsterStatsEntry &presentation,
        const MonsterProjectileTable *pProjectiles,
        const SpellTable *pSpells,
        std::string &error);
    static CombatDamageType sourceDamageType(uint8_t type);
    static uint32_t specialRandom(uint64_t actorSeed, uint64_t &cursor, uint32_t bound);
    const MonsterTable::MonsterStatsEntry &stats() const;
    const GameplayMonsterBolsterResult &bolster() const;

private:
    MonsterTable::MonsterStatsEntry m_stats;
    GameplayMonsterBolsterResult m_bolster;
};
}
