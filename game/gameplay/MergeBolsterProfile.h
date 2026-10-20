#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct MergedBolsterFormulaEntry;

inline constexpr uint32_t MergeBolsterProfileVersion = 1;
inline constexpr uint32_t MergeBolsterRandomVersion = 1;

enum class MergeBolsterErrorCode
{
    UnsupportedProfile,
    InvalidInput,
    UnsupportedRange,
    MissingSummonReference,
    EmptySpellCandidates,
};

class MergeBolsterError : public std::runtime_error
{
public:
    MergeBolsterError(MergeBolsterErrorCode code, const std::string &message);
    MergeBolsterErrorCode code() const;

private:
    MergeBolsterErrorCode m_code;
};

enum class MergeBolsterMapKind : uint8_t
{
    NoBolster = 0, OriginalStats = 1, LowerToEqual = 2, AllToEqual = 3,
};
enum class MergeBolsterStyle : uint8_t
{
    Strength = 0, Endurance = 1, Speed = 2, Magic = 3, Wimpy = 4,
};
enum class MergeBolsterCreed : uint8_t
{
    Neutral = 0, Light = 1, Dark = 2, Peasant = 3,
};
enum class MergeBolsterMagic : uint8_t
{
    Any = 0, Fire = 1, Air = 2, Water = 3, Earth = 4, Spirit = 5,
    Mind = 6, Body = 7, Light = 8, Dark = 9, Self = 10, Elemental = 11,
};

struct MergeBolsterAttack
{
    uint8_t type = 0;
    uint8_t diceCount = 0;
    uint8_t diceSides = 0;
    uint8_t damageAdd = 0;
    uint8_t missile = 0;
    bool operator==(const MergeBolsterAttack &) const = default;
};

struct MergeBolsterSpell
{
    uint8_t id = 0;
    uint8_t chance = 0;
    // RemoveSkillValueLimits.lua: rank in low eight bits; mastery bits 0x400/0x800/0x1000.
    uint16_t packedSkill = 0;
    uint8_t rank() const;
    uint8_t mastery() const;
    bool operator==(const MergeBolsterSpell &) const = default;
};

struct MergeBolsterSpecial
{
    uint8_t kind = 0;
    uint8_t a = 0;
    uint8_t b = 0;
    uint8_t c = 0;
    uint16_t d = 0;
    bool operator==(const MergeBolsterSpecial &) const = default;
};

struct MergeBolsterRewards
{
    int32_t experience = 0;
    uint8_t goldDiceCount = 0;
    uint8_t goldDiceSides = 0;
    uint8_t itemChance = 0;
    uint8_t itemLevel = 0;
    uint8_t itemType = 0;
    bool operator==(const MergeBolsterRewards &) const = default;
};

// Full source CommonMonsterProps combat payload, in source numeric IDs, not native enum ordinals.
// Names/art/position/identity/lifecycle are caller-owned and are never replaced by this module.
struct MergeBolsterCombatStats
{
    uint16_t id = 0;
    uint8_t level = 0;
    int32_t fullHitPoints = 1;
    int32_t armorClass = 0;
    int32_t moveSpeed = 0;
    int32_t attackRecovery = 0;
    uint8_t fly = 0;
    uint8_t moveType = 0;
    uint8_t aiType = 0;
    uint8_t hostileType = 0;
    uint8_t bonus = 0;
    uint8_t bonusMultiplier = 0;
    uint8_t preferredTargetCount = 0;
    uint32_t preferredTargets = 0;
    uint16_t bloodSplat = 0;
    MergeBolsterAttack attack1;
    MergeBolsterAttack attack2;
    uint8_t attack2Chance = 0;
    std::array<MergeBolsterSpell, 2> spells;
    // Source order: Fire, Air, Water, Earth, Mind, Spirit, Body, Light, Dark, Physical.
    std::array<uint16_t, 10> resistances = {};
    MergeBolsterSpecial special;
    MergeBolsterRewards rewards;
    bool operator==(const MergeBolsterCombatStats &) const = default;
};

struct MergeBolsterMonsterSettings
{
    MergeBolsterStyle style = MergeBolsterStyle::Strength;
    MergeBolsterCreed creed = MergeBolsterCreed::Neutral;
    MergeBolsterMagic magic = MergeBolsterMagic::Any;
    bool ranged = false;
    bool spells = false;
    bool hpBySize = false;
    bool replicate = false;
    bool summons = false;
    uint16_t summonId = 0;
    int32_t levelShift = 0;
    uint32_t maximumHpBoostPercent = 300; // Loaded source setting; NOT used by this formula profile.
    bool operator==(const MergeBolsterMonsterSettings &) const = default;
};

struct MergeBolsterSummonReference
{
    uint16_t id = 0; // Must equal settings.summonId, including zero.
    uint8_t fly = 0;
    bool operator==(const MergeBolsterSummonReference &) const = default;
};

struct MergeBolsterFamilyMember
{
    MergeBolsterCombatStats original;
    MergeBolsterMonsterSettings settings;
    uint16_t modelHeight = 0;
    std::optional<MergeBolsterSummonReference> summonReference;
    bool operator==(const MergeBolsterFamilyMember &) const = default;
};

struct MergeBolsterMapRules
{
    uint32_t mapId = 0;
    MergeBolsterMapKind kind = MergeBolsterMapKind::NoBolster;
    bool spells = false;
    bool summons = false;
    int32_t levelShift = 0; // Provenance only: already included in effectivePartyLevel.
    bool operator==(const MergeBolsterMapRules &) const = default;
};

struct MergeBolsterMultipliers
{
    double hp = 1;
    double ac = 1;
    double maxDamage = 1;
    double moveSpeed = 1;
    double playerAc = 1;
    bool operator==(const MergeBolsterMultipliers &) const = default;
};

struct MergeBolsterFamilyInput
{
    std::array<MergeBolsterFamilyMember, 3> members; // Complete original family, ascending IDs.
    MergeBolsterMapRules map;
    int32_t effectivePartyLevel = 0; // Includes supported CalcBolsterLevel hook AND map LevelShift.
    double bolsterAmountPercent = 100; // Finite original menu range 0..200.
    MergeBolsterMultipliers multipliers;
    bool enabled = true;
    bool templatesLocalized = true;
    uint64_t familySeed = 0;
    bool operator==(const MergeBolsterFamilyInput &) const = default;
};

enum class MergeBolsterRandomDomain : uint8_t
{
    Family, Actor,
};
enum class MergeBolsterRoll : uint8_t
{
    DamagePortion, ExtraSides, SpellSchool, SpellCandidate,
};
struct MergeBolsterDraw
{
    MergeBolsterRandomDomain domain = MergeBolsterRandomDomain::Family;
    MergeBolsterRoll purpose = MergeBolsterRoll::DamagePortion;
    uint16_t monsterId = 0;
    uint8_t slot = 0; // Attack/spell slot 0 or 1.
    uint32_t minimum = 0;
    uint32_t maximum = 0;
    uint32_t value = 0;
    uint64_t rawDrawsAfter = 0;
    bool operator==(const MergeBolsterDraw &) const = default;
};

class MergeBolsterProfile;

// Immutable prepared family. This is a pure value, not a second live actor HP authority.
class MergeBolsterFamilyPlan
{
public:
    uint32_t profileVersion() const;
    uint32_t randomVersion() const;
    const MergeBolsterFamilyInput &input() const;
    const std::array<MergeBolsterCombatStats, 3> &templates() const;
    uint16_t familyId() const;
    int32_t familyLevel() const;
    std::optional<uint8_t> bolsterStep() const; // No value means PrepareTxtMon skipped all three rows.
    const std::vector<MergeBolsterDraw> &draws() const;

private:
    friend class MergeBolsterProfile;
    MergeBolsterFamilyPlan() = default;
    MergeBolsterFamilyInput m_input;
    std::array<MergeBolsterCombatStats, 3> m_templates;
    uint16_t m_familyId = 0;
    int32_t m_familyLevel = 0;
    std::optional<uint8_t> m_bolsterStep;
    std::vector<MergeBolsterDraw> m_draws;
};

struct MergeBolsterActorInput
{
    MergeBolsterCombatStats before;
    int32_t currentHitPoints = 1; // Checked against current native/source int16 HP persistence range.
    int32_t velocity = 0;
    uint64_t actorSeed = 0;
};
class MergeBolsterActorPlan
{
public:
    uint32_t profileVersion() const;
    uint32_t randomVersion() const;
    const MergeBolsterCombatStats &combat() const;
    int32_t currentHitPoints() const;
    int32_t velocity() const;
    uint64_t actorSeed() const;
    const std::vector<MergeBolsterDraw> &draws() const;

private:
    friend class MergeBolsterProfile;
    MergeBolsterActorPlan() = default;
    MergeBolsterCombatStats m_combat;
    int32_t m_currentHitPoints = 1;
    int32_t m_velocity = 0;
    uint64_t m_actorSeed = 0;
    std::vector<MergeBolsterDraw> m_draws;
};

// No public constructor: validation of the exact finite source formula set is mandatory.
class MergeBolsterProfile
{
public:
    static MergeBolsterProfile fromFormulas(std::span<const MergedBolsterFormulaEntry> formulas);
    MergeBolsterFamilyPlan prepareFamily(const MergeBolsterFamilyInput &input) const;
    MergeBolsterActorPlan prepareActor(const MergeBolsterFamilyPlan &family,
        const MergeBolsterActorInput &actor) const;
    int32_t playerArmorClass(const MergeBolsterFamilyPlan &family, uint16_t monsterId,
        int32_t playerLevelBase, int32_t originalArmorClass) const;

private:
    MergeBolsterProfile() = default;
};
}
