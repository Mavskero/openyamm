#pragma once

#include "game/gameplay/MergeBolsterProfile.h"

#include <string_view>

namespace OpenYAMM::Game
{
class MonsterTable;
class MonsterProjectileTable;
class SpellTable;
class MergedBolsterMonsterTable;

inline constexpr uint32_t MergeMonsterSourceProfileVersion = 1;
inline constexpr uint32_t MergeMonsterSourceContentFixVersion = 1;
inline constexpr std::string_view MergeMonsterSourceMemberSha256 =
    "e37be5cb43d52295a377f780a0e160bd4f21fe91bfd6ab98b58ee6e286833085";

enum class MergeMonsterSourceErrorCode
{
    UnsupportedProfile, InvalidRow, UnsupportedToken, UnsupportedRange, MissingResource,
    MissingTemplate, IncompleteFamily, InvalidPreparedFamily,
};

struct MergeMonsterSourceIssue
{
    MergeMonsterSourceErrorCode code = MergeMonsterSourceErrorCode::InvalidRow;
    uint16_t templateId = 0;
    int column = -1;
    std::string token;
    std::string reason;
};

class MergeMonsterSourceError : public std::runtime_error
{
public:
    explicit MergeMonsterSourceError(MergeMonsterSourceIssue issue);
    const MergeMonsterSourceIssue &issue() const;

private:
    MergeMonsterSourceIssue m_issue;
};

struct MergeMonsterSourceIdentity
{
    uint32_t parserVersion = MergeMonsterSourceProfileVersion;
    uint32_t contentFixVersion = MergeMonsterSourceContentFixVersion;
    uint64_t mechanicalFingerprint = 0;
    uint64_t descriptorFingerprint = 0;
    std::string sourceMemberSha256;
    uint16_t templateId = 0;
    uint16_t descriptorId = 0;
    std::string pictureName;
    bool operator==(const MergeMonsterSourceIdentity &) const = default;
};

struct MergeMonsterSourceEntry
{
    MergeMonsterSourceIdentity identity;
    uint16_t modelHeight = 0;
    uint8_t fly = 0;
    bool compactIceSpellCorrected = false;
    // Unsupported source rows remain identifiable. They cannot silently supply zeroed combat values.
    std::optional<MergeBolsterCombatStats> combat;
    std::optional<MergeMonsterSourceIssue> issue;
};

struct MergeMonsterFamilyContext
{
    MergeBolsterMapRules map;
    int32_t effectivePartyLevel = 0;
    double bolsterAmountPercent = 100;
    MergeBolsterMultipliers multipliers;
    bool enabled = true;
    bool templatesLocalized = true;
    uint64_t familySeed = 0;
    bool operator==(const MergeMonsterFamilyContext &) const = default;
};

struct MergeMonsterStandardSummon
{
    MergeMonsterSourceIdentity identity;
    MergeBolsterActorInput actor;
    bool hostile = false; // Separate from CommonMonsterProps.HostileType=4.
};

// Fixed source preparation input only. No live stats, RNG, Party, World, or save mutation is owned here.
class MergeMonsterSourceProfile
{
public:
    static MergeMonsterSourceProfile fromRows(const std::vector<std::vector<std::string>> &baseRows,
        const MonsterTable &descriptors, const MonsterProjectileTable &projectiles, const SpellTable &spells);
    const std::vector<MergeMonsterSourceEntry> &entries() const;
    const MergeMonsterSourceEntry *findById(uint16_t id) const;
    const MergeBolsterCombatStats &requireById(uint16_t id) const;
    MergeBolsterSummonReference summonReference(uint16_t id) const;
    MergeBolsterFamilyInput familyInput(uint16_t id, const MergedBolsterMonsterTable &settings,
        const MergeMonsterFamilyContext &context) const;
    // First family instance: omit preparedFamily, capture before, then prepareFamily and prepareActor.
    // Later instances: supply the same catalog's immutable B2 plan as the current template authority.
    MergeMonsterStandardSummon standardSummonBefore(uint16_t id, uint64_t actorSeed,
        const MergeBolsterFamilyPlan *pPreparedFamily = nullptr, bool restoreTemplateTreasure = false) const;
    void validateResolvedResources(const MergeBolsterCombatStats &combat,
        const MonsterProjectileTable &projectiles, const SpellTable &spells) const;

private:
    MergeMonsterSourceProfile() = default;
    std::vector<MergeMonsterSourceEntry> m_entries;
};
}
