#include "game/gameplay/MonsterSpellSupport.h"

#include "game/StringUtils.h"

#include <unordered_set>

namespace OpenYAMM::Game
{
bool isMonsterProjectileSpellName(const std::string &spellName)
{
    static const std::unordered_set<std::string> ProjectileSpellNames = {
        "fire bolt", "fireball", "incinerate", "lightning bolt", "implosion", "meteor shower",
        "ice bolt", "icebolt", "ice blast", "iceblast", "acid burst", "deadly swarm", "blades",
        "rock blast", "mass distortion", "sparks", "mind blast", "psychic shock", "psychic shockt",
        "harm", "flying fist", "light bolt", "spirit lash", "toxic cloud", "shrapmetal", "dark grasp",
        "dragon breath", "poison spray", "sunray"};

    const std::string lowered = toLowerCopy(spellName);
    return ProjectileSpellNames.find(lowered) != ProjectileSpellNames.end();
}

bool isMonsterSelfBuffSpellName(const std::string &spellName)
{
    static const std::unordered_set<std::string> SelfBuffSpellNames = {
        "bless", "day of protection", "fate", "hammerhands", "haste", "heroism", "hour of power",
        "pain reflection", "shield", "stone skin", "stoneskin"};

    const std::string lowered = toLowerCopy(spellName);
    return SelfBuffSpellNames.find(lowered) != SelfBuffSpellNames.end();
}

bool isMonsterSelfActionSpellName(const std::string &spellName)
{
    static const std::unordered_set<std::string> SelfActionSpellNames = {
        "heal", "power cure", "dispel magic", "bless", "day of protection", "fate", "hammerhands",
        "haste", "heroism", "hour of power", "pain reflection", "shield", "stone skin", "stoneskin"};

    const std::string lowered = toLowerCopy(spellName);
    return SelfActionSpellNames.find(lowered) != SelfActionSpellNames.end();
}

bool isKnownUnsupportedMonsterSpellName(const std::string &spellName)
{
    static const std::unordered_set<std::string> UnsupportedMonsterSpellNames = {
        "day-o-gods",
        "day of the gods",
        "finger of death",
        "mass curse",
        "paralyze",
    };

    const std::string lowered = toLowerCopy(spellName);
    return UnsupportedMonsterSpellNames.find(lowered) != UnsupportedMonsterSpellNames.end();
}
}
