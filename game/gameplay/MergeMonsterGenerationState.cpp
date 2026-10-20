#include "game/gameplay/MergeMonsterGenerationState.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint64_t ActorSeedTag = 0x4143544f52530001ULL;
constexpr uint16_t MaximumFamilyId = 217;

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::invalid_argument(std::string("Merge monster generation: ") + pMessage);
    }
}

uint64_t mix(uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

void validateContext(const MergeMonsterFamilyContext &context)
{
    require(context.familySeed == 0 && context.map.mapId != 0
        && uint8_t(context.map.kind) <= uint8_t(MergeBolsterMapKind::AllToEqual)
        && context.effectivePartyLevel >= -65535 && context.effectivePartyLevel <= 65535
        && context.map.levelShift >= -65535 && context.map.levelShift <= 65535
        && std::isfinite(context.bolsterAmountPercent)
        && context.bolsterAmountPercent >= 0 && context.bolsterAmountPercent <= 200,
        "invalid frozen family context");
    for (double value : {context.multipliers.hp, context.multipliers.ac, context.multipliers.maxDamage,
             context.multipliers.moveSpeed, context.multipliers.playerAc})
    {
        require(std::isfinite(value) && value >= 0, "invalid frozen multiplier");
    }
}

void validateCombat(const MergeBolsterCombatStats &combat)
{
    require(combat.id > 0 && combat.id <= MaximumFamilyId * 3 && combat.fullHitPoints > 0
        && combat.armorClass >= 0 && combat.moveSpeed >= 0 && combat.attackRecovery >= 0
        && combat.rewards.experience >= 0 && combat.fly <= 1 && combat.moveType <= 5 && combat.aiType <= 3,
        "invalid saved family combat values");
    for (const MergeBolsterSpell &spell : combat.spells)
    {
        const uint16_t masteryBits = spell.packedSkill & 0xff00;
        require(masteryBits == 0 || masteryBits == 0x400 || masteryBits == 0x800 || masteryBits == 0x1000,
            "invalid saved family spell skill");
    }
}

MergeMonsterFamilyContext familyContext(const MergeMonsterGenerationState &state, uint16_t familyId)
{
    MergeMonsterFamilyContext result = state.context;
    result.familySeed = mergeMonsterGenerationFamilySeed(state, familyId);
    return result;
}

bool matchesContext(const MergeBolsterFamilyInput &input, const MergeMonsterFamilyContext &context)
{
    return input.map == context.map && input.effectivePartyLevel == context.effectivePartyLevel
        && input.bolsterAmountPercent == context.bolsterAmountPercent && input.multipliers == context.multipliers
        && input.enabled == context.enabled && input.templatesLocalized == context.templatesLocalized
        && input.familySeed == context.familySeed;
}

MergeMonsterPreparedFamily snapshot(const MergeBolsterFamilyPlan &plan)
{
    MergeMonsterPreparedFamily result;
    result.familyId = plan.familyId();
    result.input = plan.input();
    result.templates = plan.templates();
    result.familyLevel = plan.familyLevel();
    result.bolsterStep = plan.bolsterStep();
    result.rawDrawCount = plan.draws().empty() ? 0 : plan.draws().back().rawDrawsAfter;
    return result;
}

void authenticateSource(const MergeMonsterGenerationState &state, const MergeMonsterGenerationServices &services)
{
    const MergeMonsterSourceEntry *pEntry = services.source.findById(1);
    require(pEntry != nullptr && pEntry->identity.parserVersion == state.sourceProfileVersion
        && pEntry->identity.contentFixVersion == state.sourceContentFixVersion
        && pEntry->identity.sourceMemberSha256 == state.catalogSha256
        && pEntry->identity.mechanicalFingerprint == state.mechanicalFingerprint
        && pEntry->identity.descriptorFingerprint == state.descriptorFingerprint,
        "loaded source profile differs from saved generation");
}

MergeBolsterFamilyPlan restoreFamily(const MergeMonsterPreparedFamily &saved,
    const MergeMonsterGenerationState &state, const MergeMonsterGenerationServices &services)
{
    const MergeBolsterFamilyInput loaded = services.source.familyInput(saved.input.members[0].original.id,
        services.settings, familyContext(state, saved.familyId));
    require(loaded == saved.input, "loaded family differs from its frozen source input");
    MergeBolsterFamilyPlan result = services.bolster.prepareFamily(saved.input);
    require(snapshot(result) == saved, "saved family templates or RNG cursor differ from the fixed profile");
    return result;
}

MergeBolsterFamilyPlan prepareFamily(MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, uint16_t monsterId)
{
    const uint16_t familyId = uint16_t((monsterId + 2) / 3);
    require(familyId > 0 && familyId <= MaximumFamilyId, "incomplete or invalid requested family");
    const auto position = std::lower_bound(state.preparedFamilies.begin(), state.preparedFamilies.end(), familyId,
        [](const MergeMonsterPreparedFamily &family, uint16_t id) { return family.familyId < id; });
    if (position != state.preparedFamilies.end() && position->familyId == familyId)
    {
        return restoreFamily(*position, state, services);
    }
    const MergeBolsterFamilyInput input = services.source.familyInput(monsterId, services.settings,
        familyContext(state, familyId));
    MergeBolsterFamilyPlan result = services.bolster.prepareFamily(input);
    state.preparedFamilies.insert(position, snapshot(result));
    return result;
}

std::optional<MergeBolsterFamilyPlan> readyFamily(const MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, uint16_t monsterId)
{
    const uint16_t familyId = uint16_t((monsterId + 2) / 3);
    const auto found = std::lower_bound(state.preparedFamilies.begin(), state.preparedFamilies.end(), familyId,
        [](const MergeMonsterPreparedFamily &family, uint16_t id) { return family.familyId < id; });
    if (found == state.preparedFamilies.end() || found->familyId != familyId)
    {
        return std::nullopt;
    }
    return restoreFamily(*found, state, services);
}

GameplayActorCombatState prepareActor(MergeMonsterGenerationState &candidate,
    const MergeMonsterGenerationServices &services, uint16_t monsterId, MergeMonsterGenerationMode mode)
{
    require(candidate.nextActorOrdinal < std::numeric_limits<uint64_t>::max(), "actor ordinal exhausted");
    require(mode == MergeMonsterGenerationMode::LuaSummon
        || mode == MergeMonsterGenerationMode::LuaSummonWithTreasure
        || mode == MergeMonsterGenerationMode::NativeSpecial, "unknown preparation mode");
    const uint64_t actorSeed = mergeMonsterGenerationActorSeed(candidate, candidate.nextActorOrdinal);
    std::optional<MergeBolsterFamilyPlan> family;
    MergeBolsterCombatStats combat;
    if (mode == MergeMonsterGenerationMode::NativeSpecial)
    {
        family = prepareFamily(candidate, services, monsterId);
        combat = family->templates()[(monsterId - 1) % 3];
        // Native summon copies CommonMonsterProps after its PrepareTxtMon hook. It never calls
        // Lua PrepareMapMon and clears precisely these three reward fields (not all treasure).
        combat.rewards.experience = 0;
        combat.rewards.itemChance = 0;
        combat.rewards.goldDiceSides = 0;
        require(combat.fullHitPoints <= std::numeric_limits<int16_t>::max()
            && combat.moveSpeed <= std::numeric_limits<int16_t>::max(), "native summon HP or velocity overflow");
    }
    else
    {
        family = readyFamily(candidate, services, monsterId);
        const MergeMonsterStandardSummon before = services.source.standardSummonBefore(monsterId, actorSeed,
            family ? &*family : nullptr, mode == MergeMonsterGenerationMode::LuaSummonWithTreasure);
        if (!family)
        {
            family = prepareFamily(candidate, services, monsterId);
        }
        combat = services.bolster.prepareActor(*family, before.actor).combat();
    }
    services.source.validateResolvedResources(combat, services.projectiles, services.spells);
    GameplayMergeResolvedActorCombat resolved;
    resolved.contentPackageId = candidate.contentPackageId;
    resolved.contentPackageSchema = candidate.contentPackageSchema;
    resolved.catalogSha256 = candidate.catalogSha256;
    resolved.profileSha256 = candidate.profileSha256;
    resolved.familySeed = family->input().familySeed;
    resolved.actorSeed = actorSeed;
    resolved.generationEpoch = candidate.generationEpoch;
    resolved.generationLevel = candidate.generationLevel;
    resolved.generationSerial = candidate.generationSerial;
    resolved.playerArmorClassPrepared = family->bolsterStep().has_value();
    resolved.bolsterAmountPercent = candidate.context.bolsterAmountPercent;
    resolved.playerArmorClassMultiplier = candidate.context.multipliers.playerAc;
    resolved.combat = combat;
    GameplayActorCombatState result(std::move(resolved));
    std::string error;
    if (!validateGameplayActorCombatState(result, combat.fullHitPoints, error))
    {
        throw std::invalid_argument(error);
    }
    ++candidate.nextActorOrdinal;
    return result;
}

void requireValidState(const MergeMonsterGenerationState &state, const MergeMonsterGenerationServices &services)
{
    std::string error;
    if (!validateMergeMonsterGenerationState(state, error))
    {
        throw std::invalid_argument(error);
    }
    authenticateSource(state, services);
}
}

uint64_t mergeMonsterGenerationFamilySeed(const MergeMonsterGenerationState &state, uint16_t familyId)
{
    return mix(mix(state.populationSeed ^ ActorSeedTag) ^ familyId);
}

uint64_t mergeMonsterGenerationActorSeed(const MergeMonsterGenerationState &state, uint64_t ordinal)
{
    return mix(mix(state.populationSeed ^ ActorSeedTag) ^ ordinal);
}

MergeMonsterGenerationState makeMergeMonsterGenerationState(const MergeMonsterGenerationServices &services,
    const MergeMonsterGenerationOrigin &origin)
{
    const MergeMonsterSourceEntry *pEntry = services.source.findById(1);
    require(pEntry != nullptr, "loaded source catalog has no identity");
    MergeMonsterGenerationState result;
    result.contentPackageId = origin.contentPackageId;
    result.contentPackageSchema = origin.contentPackageSchema;
    result.catalogSha256 = pEntry->identity.sourceMemberSha256;
    result.profileSha256 = MergeMonsterGenerationProfileSha256;
    result.mechanicalFingerprint = pEntry->identity.mechanicalFingerprint;
    result.descriptorFingerprint = pEntry->identity.descriptorFingerprint;
    result.context = origin.context;
    result.context.familySeed = 0;
    result.populationSeed = origin.populationSeed;
    result.generationEpoch = origin.generationEpoch;
    result.generationLevel = origin.generationLevel;
    result.generationSerial = origin.generationSerial;
    requireValidState(result, services);
    return result;
}

bool validateMergeMonsterGenerationState(const MergeMonsterGenerationState &state, std::string &error)
{
    try
    {
        require(state.version == MergeMonsterGenerationStateVersion
            && state.sourceProfileVersion == MergeMonsterSourceProfileVersion
            && state.sourceContentFixVersion == MergeMonsterSourceContentFixVersion
            && state.bolsterProfileVersion == MergeBolsterProfileVersion
            && state.bolsterRandomVersion == MergeBolsterRandomVersion
            && state.seedVersion == MergeMonsterGenerationSeedVersion, "unsupported saved state version");
        require(!state.contentPackageId.empty() && state.contentPackageSchema != 0
            && state.catalogSha256 == MergeMonsterSourceMemberSha256
            && state.profileSha256 == MergeMonsterGenerationProfileSha256
            && state.mechanicalFingerprint != 0 && state.descriptorFingerprint != 0,
            "invalid source identity");
        require(state.generationEpoch != 0 && state.generationLevel != 0 && state.generationSerial != 0,
            "incomplete generation identity");
        validateContext(state.context);
        require(state.preparedFamilies.size() <= MaximumFamilyId, "too many prepared families");
        uint16_t previousFamily = 0;
        for (const MergeMonsterPreparedFamily &family : state.preparedFamilies)
        {
            require(family.familyId > previousFamily && family.familyId <= MaximumFamilyId
                && family.familyLevel >= 0 && (!family.bolsterStep || *family.bolsterStep <= 4),
                "invalid prepared family identity or order");
            previousFamily = family.familyId;
            require(matchesContext(family.input, familyContext(state, family.familyId)),
                "prepared family context or seed differs from generation");
            for (size_t index = 0; index < 3; ++index)
            {
                const MergeBolsterFamilyMember &member = family.input.members[index];
                const uint16_t expectedId = uint16_t(family.familyId * 3 - 2 + index);
                require(member.original.id == expectedId && family.templates[index].id == expectedId,
                    "incomplete prepared family");
                validateCombat(member.original);
                validateCombat(family.templates[index]);
                const MergeBolsterMonsterSettings &settings = member.settings;
                require(uint8_t(settings.style) <= 4 && uint8_t(settings.creed) <= 3 && uint8_t(settings.magic) <= 11
                    && settings.levelShift >= -65535 && settings.levelShift <= 65535,
                    "invalid saved monster settings");
                require(member.summonReference && member.summonReference->id == settings.summonId
                    && member.summonReference->fly <= 1, "missing or invalid saved summon reference");
            }
        }
        error.clear();
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

bool validateMergeMonsterGenerationResources(const MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, std::string &error)
{
    try
    {
        requireValidState(state, services);
        for (const MergeMonsterPreparedFamily &family : state.preparedFamilies)
        {
            const MergeBolsterFamilyPlan plan = restoreFamily(family, state, services);
            for (const MergeBolsterCombatStats &combat : plan.templates())
            {
                services.source.validateResolvedResources(combat, services.projectiles, services.spells);
            }
        }
        error.clear();
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

GameplayActorCombatState prepareMergeMonsterGenerationActor(MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, uint16_t monsterId, MergeMonsterGenerationMode mode)
{
    requireValidState(state, services);
    MergeMonsterGenerationState candidate = state;
    GameplayActorCombatState result = prepareActor(candidate, services, monsterId, mode);
    state = std::move(candidate);
    return result;
}

GameplayActorCombatState prepareMergeMonsterSummon(MergeMonsterGenerationState &state,
    const MergeMonsterGenerationServices &services, const GameplayMergeResolvedActorCombat &parent,
    uint16_t monsterId, MergeMonsterGenerationMode mode)
{
    requireValidState(state, services);
    require(parent.schemaVersion == GameplayActorCombatSchemaVersion
        && parent.profileVersion == state.bolsterProfileVersion && parent.randomVersion == state.bolsterRandomVersion
        && parent.contentPackageId == state.contentPackageId
        && parent.contentPackageSchema == state.contentPackageSchema
        && parent.catalogSha256 == state.catalogSha256 && parent.profileSha256 == state.profileSha256
        && parent.generationEpoch == state.generationEpoch && parent.generationLevel == state.generationLevel
        && parent.generationSerial == state.generationSerial
        && parent.familySeed == mergeMonsterGenerationFamilySeed(state, uint16_t((parent.combat.id + 2) / 3))
        && parent.bolsterAmountPercent == state.context.bolsterAmountPercent
        && parent.playerArmorClassMultiplier == state.context.multipliers.playerAc,
        "summoning actor belongs to another generation");
    require(readyFamily(state, services, parent.combat.id).has_value(), "summoning actor family was not prepared");
    MergeMonsterGenerationState candidate = state;
    if (mode == MergeMonsterGenerationMode::NativeSpecial)
    {
        require(parent.combat.special.kind == 2 && parent.combat.special.d > 0, "actor has no native summon");
        // The original hook visits D..D+2, not a normalized family start. D can be a middle/strong
        // tier; preserving that range also prepares the adjacent family when the hook crosses it.
        const std::optional<MergeBolsterFamilyPlan> startFamily = readyFamily(candidate, services,
            parent.combat.special.d);
        require(monsterId == parent.combat.special.d || (parent.combat.special.a == 0
            && monsterId > parent.combat.special.d && uint32_t(monsterId) <= uint32_t(parent.combat.special.d) + 2),
            "native summon tier differs from its source Special");
        if (candidate.context.enabled && (!startFamily || !startFamily->bolsterStep()))
        {
            for (uint32_t id = parent.combat.special.d; id <= uint32_t(parent.combat.special.d) + 2; ++id)
            {
                require(id <= MaximumFamilyId * 3, "native summon hook exceeds complete source families");
                (void)prepareFamily(candidate, services, static_cast<uint16_t>(id));
            }
        }
    }
    GameplayActorCombatState result = prepareActor(candidate, services, monsterId, mode);
    state = std::move(candidate);
    return result;
}
}
