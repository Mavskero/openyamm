#include "game/gameplay/MergeMonsterRuntimeView.h"

#include "game/tables/MonsterProjectileTable.h"
#include "game/items/ItemGenerationRandom.h"
#include "game/gameplay/GameMechanics.h"
#include "game/tables/SpellTable.h"

#include <limits>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
std::string damageToken(uint8_t type)
{
    switch (type)
    {
        case 0: return "fire";
        case 1: return "air";
        case 2: return "water";
        case 3: return "earth";
        case 4: return "phys";
        case 5: return "magic";
        case 6: return "spirit";
        case 7: return "mind";
        case 8: return "body";
        case 9: return "light";
        case 10: return "dark";
        case 12: return "energy";
        default: throw std::invalid_argument("Resolved monster: unsupported source damage type");
    }
}

MonsterSpecialAttackKind specialAttackKind(uint8_t bonus)
{
    switch (bonus)
    {
        case 0: return MonsterSpecialAttackKind::None;
        case 1: return MonsterSpecialAttackKind::Curse;
        case 2: return MonsterSpecialAttackKind::Weak;
        case 3: return MonsterSpecialAttackKind::Sleep;
        case 4: return MonsterSpecialAttackKind::Drunk;
        case 5: return MonsterSpecialAttackKind::Insane;
        case 6: return MonsterSpecialAttackKind::PoisonWeak;
        case 7: return MonsterSpecialAttackKind::PoisonMedium;
        case 8: return MonsterSpecialAttackKind::PoisonSevere;
        case 9: return MonsterSpecialAttackKind::DiseaseWeak;
        case 10: return MonsterSpecialAttackKind::DiseaseMedium;
        case 11: return MonsterSpecialAttackKind::DiseaseSevere;
        case 12: return MonsterSpecialAttackKind::Paralyze;
        case 13: return MonsterSpecialAttackKind::Unconscious;
        case 14: return MonsterSpecialAttackKind::Dead;
        case 15: return MonsterSpecialAttackKind::Petrify;
        case 16: return MonsterSpecialAttackKind::Eradicate;
        case 17: return MonsterSpecialAttackKind::BreakAny;
        case 18: return MonsterSpecialAttackKind::BreakArmor;
        case 19: return MonsterSpecialAttackKind::BreakWeapon;
        case 20: return MonsterSpecialAttackKind::Steal;
        case 21: return MonsterSpecialAttackKind::Aging;
        case 22: return MonsterSpecialAttackKind::ManaDrain;
        case 23: return MonsterSpecialAttackKind::Fear;
        default: throw std::invalid_argument("Resolved monster: unsupported source hit bonus");
    }
}

MonsterTable::LootItemKind lootKind(uint8_t type)
{
    using Kind = MonsterTable::LootItemKind;
    switch (type)
    {
        case 0: return Kind::Any;
        case 20: return Kind::Weapon;
        case 21: return Kind::Armor;
        case 22: return Kind::Misc;
        case 23: return Kind::Sword;
        case 24: return Kind::Dagger;
        case 25: return Kind::Axe;
        case 26: return Kind::Spear;
        case 27: return Kind::Bow;
        case 28: return Kind::Mace;
        case 29: return Kind::Club;
        case 30: return Kind::Staff;
        case 31: return Kind::Leather;
        case 32: return Kind::Chain;
        case 33: return Kind::Plate;
        case 34: return Kind::Shield;
        case 35: return Kind::Helm;
        case 36: return Kind::Belt;
        case 37: return Kind::Cloak;
        case 38: return Kind::Gauntlets;
        case 39: return Kind::Boots;
        case 40: return Kind::Ring;
        case 41: return Kind::Amulet;
        case 42: return Kind::Wand;
        case 43: return Kind::Scroll;
        case 46: return Kind::Gem;
        case 47: return Kind::Ore;
        default: throw std::invalid_argument("Resolved monster: unsupported source treasure type");
    }
}

std::string missileToken(uint8_t id, const MonsterProjectileTable *pProjectiles)
{
    if (id == 0)
    {
        return {};
    }
    const MonsterProjectileEntry *pEntry = pProjectiles != nullptr
        ? pProjectiles->findBySourceMissileId(id) : nullptr;
    if (pEntry == nullptr)
    {
        throw std::invalid_argument("Resolved monster: missing source missile ID " + std::to_string(id));
    }
    return pEntry->token;
}

std::string spellName(uint8_t id, const SpellTable *pSpells)
{
    if (id == 0)
    {
        return {};
    }
    const SpellEntry *pEntry = pSpells != nullptr ? pSpells->findById(id) : nullptr;
    if (pEntry == nullptr)
    {
        throw std::invalid_argument("Resolved monster: missing spell ID " + std::to_string(id));
    }
    return pEntry->name;
}
}

std::optional<MergeMonsterRuntimeView> MergeMonsterRuntimeView::fromCombat(
    const GameplayMergeResolvedActorCombat &resolved,
    const MonsterTable::MonsterStatsEntry &presentation,
    const MonsterProjectileTable *pProjectiles,
    const SpellTable *pSpells,
    std::string &error)
{
    error.clear();
    const MergeBolsterCombatStats &combat = resolved.combat;
    if (combat.moveSpeed < 0 || combat.moveSpeed > std::numeric_limits<uint16_t>::max())
    {
        error = "Resolved monster: movement speed exceeds the existing AI interface";
        return std::nullopt;
    }
    if (combat.special.kind > 4)
    {
        error = "Resolved monster: source Special kind " + std::to_string(combat.special.kind)
            + " has no installed generation runtime";
        return std::nullopt;
    }
    try
    {
        MergeMonsterRuntimeView result;
        MonsterTable::MonsterStatsEntry &stats = result.m_stats;
        stats = presentation;
        stats.id = combat.id;
        stats.level = combat.level;
        stats.hitPoints = combat.fullHitPoints;
        stats.armorClass = combat.armorClass;
        stats.experience = combat.rewards.experience;
        stats.bloodSplatOnDeath = combat.bloodSplat != 0;
        stats.hostility = combat.hostileType;
        stats.speed = combat.moveSpeed;
        stats.recovery = combat.attackRecovery;
        // Only exact legacy mask correspondences. MM8 class flags must never be reinterpreted
        // as the unrelated MM7 masks; the remaining source preferences need a shared Party extension.
        stats.attackPreferences = ((combat.preferredTargets & 0x002) ? 0x0010u : 0)
            | ((combat.preferredTargets & 0x004) ? 0x0001u : 0)
            | ((combat.preferredTargets & 0x100) ? 0x0200u : 0)
            | ((combat.preferredTargets & 0x200) ? 0x0400u : 0);
        stats.specialAttackDescriptor.clear();
        stats.specialAttackKind = specialAttackKind(combat.bonus);
        stats.specialAttackLevel = combat.bonusMultiplier;
        stats.canFly = combat.fly != 0;
        stats.movementType = static_cast<MonsterTable::MonsterMovementType>(combat.moveType);
        stats.aiType = static_cast<MonsterTable::MonsterAiType>(combat.aiType);
        stats.attack1Type = damageToken(combat.attack1.type);
        stats.attack2Type = damageToken(combat.attack2.type);
        stats.attack1MissileType = missileToken(combat.attack1.missile, pProjectiles);
        stats.attack2MissileType = missileToken(combat.attack2.missile, pProjectiles);
        stats.attack1HasMissile = combat.attack1.missile != 0;
        stats.attack2HasMissile = combat.attack2.missile != 0;
        stats.attack1Damage = {combat.attack1.diceCount, combat.attack1.diceSides, combat.attack1.damageAdd};
        stats.attack2Damage = {combat.attack2.diceCount, combat.attack2.diceSides, combat.attack2.damageAdd};
        stats.attack2Chance = combat.attack2Chance;
        stats.spell1Descriptor.clear();
        stats.spell2Descriptor.clear();
        stats.spell1Name = spellName(combat.spells[0].id, pSpells);
        stats.spell2Name = spellName(combat.spells[1].id, pSpells);
        stats.hasSpell1 = combat.spells[0].id != 0;
        stats.hasSpell2 = combat.spells[1].id != 0;
        stats.spell1UseChance = combat.spells[0].chance;
        stats.spell2UseChance = combat.spells[1].chance;
        stats.spell1SkillLevel = combat.spells[0].rank();
        stats.spell2SkillLevel = combat.spells[1].rank();
        stats.spell1SkillMastery = static_cast<SkillMastery>(combat.spells[0].mastery());
        stats.spell2SkillMastery = static_cast<SkillMastery>(combat.spells[1].mastery());
        stats.attackStyle = stats.attack1HasMissile ? MonsterTable::MonsterAttackStyle::Ranged
            : stats.attack2HasMissile ? MonsterTable::MonsterAttackStyle::MixedMeleeRanged
            : MonsterTable::MonsterAttackStyle::MeleeOnly;
        stats.fireResistance = combat.resistances[0];
        stats.airResistance = combat.resistances[1];
        stats.waterResistance = combat.resistances[2];
        stats.earthResistance = combat.resistances[3];
        stats.mindResistance = combat.resistances[4];
        stats.spiritResistance = combat.resistances[5];
        stats.bodyResistance = combat.resistances[6];
        stats.lightResistance = combat.resistances[7];
        stats.darkResistance = combat.resistances[8];
        stats.physicalResistance = combat.resistances[9];
        stats.treasureDefinition.clear();
        stats.loot = {combat.rewards.goldDiceCount, combat.rewards.goldDiceSides, combat.rewards.itemChance,
            combat.rewards.itemLevel, lootKind(combat.rewards.itemType)};

        GameplayMonsterBolsterResult &bolster = result.m_bolster;
        bolster.maxHp = combat.fullHitPoints;
        bolster.armorClass = combat.armorClass;
        bolster.moveSpeed = uint16_t(combat.moveSpeed);
        bolster.immobile = combat.moveType == 5;
        bolster.attack1DamageDiceRolls = combat.attack1.diceCount;
        bolster.attack1DamageDiceSides = combat.attack1.diceSides;
        bolster.attack1DamageBonus = combat.attack1.damageAdd;
        bolster.attack2DamageDiceRolls = combat.attack2.diceCount;
        bolster.attack2DamageDiceSides = combat.attack2.diceSides;
        bolster.attack2DamageBonus = combat.attack2.damageAdd;
        bolster.spell1SkillLevel = stats.spell1SkillLevel;
        bolster.spell1SkillMastery = stats.spell1SkillMastery;
        bolster.spell2SkillLevel = stats.spell2SkillLevel;
        bolster.spell2SkillMastery = stats.spell2SkillMastery;
        // All fields are already resolved; generated bonuses and reward multipliers stay disabled.
        return result;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return std::nullopt;
    }
}

CombatDamageType MergeMonsterRuntimeView::sourceDamageType(uint8_t type)
{
    return GameMechanics::parseCombatDamageType(damageToken(type));
}

uint32_t MergeMonsterRuntimeView::specialRandom(uint64_t actorSeed, uint64_t &cursor, uint32_t bound)
{
    // FixedV1's counter is sufficient to recover its SplitMix64 stream. A distinct domain
    // keeps combat choices separate from generation; rejected operations retain their cursor.
    FixedItemGenerationRandom random({FixedItemGenerationProtocol::V1,
        (actorSeed ^ 0x5350454349414c31ULL) + cursor * 0x9e3779b97f4a7c15ULL, cursor});
    const uint32_t value = uint32_t(random.bounded(bound));
    cursor = random.cursor().rawDrawCount;
    return value;
}

const MonsterTable::MonsterStatsEntry &MergeMonsterRuntimeView::stats() const
{
    return m_stats;
}

const GameplayMonsterBolsterResult &MergeMonsterRuntimeView::bolster() const
{
    return m_bolster;
}
}
