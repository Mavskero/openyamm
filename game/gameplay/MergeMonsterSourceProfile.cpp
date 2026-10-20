#include "game/gameplay/MergeMonsterSourceProfile.h"

#include "game/tables/MergedBaseTables.h"
#include "game/tables/MonsterProjectileTable.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/SpellTable.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
using Row = std::vector<std::string>;
constexpr uint16_t SourceCount = 652;
constexpr uint64_t MechanicalFingerprint = 0x0612c98e795b9de0ULL;
constexpr uint64_t DescriptorFingerprint = 0x3042097306818832ULL;

// This fixed profile is documented in 148/merge-monster-effective-parser-001. The compact Ice
// correction is explicit content policy, not a claim to emulate the original unsafe token cursor.
[[noreturn]] void fail(MergeMonsterSourceErrorCode code, uint16_t id, int column,
    std::string_view token, const char *pReason)
{
    throw MergeMonsterSourceError({code, id, column, std::string(token), pReason});
}

std::string lower(std::string value)
{
    for (char &letter : value)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return value;
}

uint32_t integer(std::string_view value, uint32_t maximum, uint16_t id, int column)
{
    uint32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size() || result > maximum)
    {
        fail(MergeMonsterSourceErrorCode::UnsupportedRange, id, column, value, "invalid source integer");
    }
    return result;
}

uint8_t byte(std::string_view value, uint16_t id, int column)
{
    return uint8_t(integer(value, 255, id, column));
}

uint8_t tokenValue(std::string_view token, std::initializer_list<std::pair<std::string_view, uint8_t>> values,
    uint16_t id, int column)
{
    const std::string normalized = lower(std::string(token));
    for (const auto &[name, value] : values)
    {
        if (normalized == name)
        {
            return value;
        }
    }
    fail(MergeMonsterSourceErrorCode::UnsupportedToken, id, column, token, "unsupported source token");
}

class Fingerprint
{
public:
    void number(uint64_t value)
    {
        for (size_t shift = 0; shift < 64; shift += 8)
        {
            m_value = (m_value ^ uint8_t(value >> shift)) * 1099511628211ULL;
        }
    }
    void string(std::string_view value)
    {
        number(value.size());
        for (unsigned char letter : value)
        {
            m_value = (m_value ^ letter) * 1099511628211ULL;
        }
    }
    uint64_t value() const { return m_value; }

private:
    uint64_t m_value = 14695981039346656037ULL;
};

Row descriptorRow(uint16_t id, const MonsterEntry &entry)
{
    Row result = {std::to_string(id), entry.internalName, std::to_string(entry.height),
        std::to_string(entry.radius), std::to_string(entry.movementSpeed), std::to_string(entry.toHitRadius),
        std::to_string(entry.tintColor)};
    for (uint16_t sound : entry.soundSampleIds)
    {
        result.push_back(std::to_string(sound));
    }
    result.insert(result.end(), entry.spriteNames.begin(), entry.spriteNames.end());
    return result;
}

std::vector<std::string> split(std::string_view value, char delimiter)
{
    std::vector<std::string> result;
    size_t start = 0;
    while (true)
    {
        const size_t end = value.find(delimiter, start);
        result.emplace_back(value.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos)
        {
            return result;
        }
        start = end + 1;
    }
}

MergeBolsterAttack dice(std::string_view token, uint16_t id, int column)
{
    const std::string value = lower(std::string(token));
    const size_t separator = value.find('d');
    const size_t addition = value.find('+');
    MergeBolsterAttack result;
    if (separator == std::string::npos)
    {
        result.diceCount = byte(value, id, column);
        result.diceSides = 1; // Bare N is Nd1, including zero.
        return result;
    }
    result.diceCount = byte(std::string_view(value).substr(0, separator), id, column);
    result.diceSides = byte(std::string_view(value).substr(separator + 1,
        addition == std::string::npos ? addition : addition - separator - 1), id, column);
    if (addition != std::string::npos)
    {
        result.damageAdd = byte(std::string_view(value).substr(addition + 1), id, column);
    }
    return result;
}

uint8_t damage(std::string_view token, uint16_t id, int column)
{
    // Pois and the inactive attack token 0 take the original Physical default path.
    return tokenValue(token, {{"fire", 0}, {"air", 1}, {"water", 2}, {"earth", 3}, {"phys", 4},
        {"0", 4}, {"pois", 4}, {"magic", 5}, {"spirit", 6}, {"mind", 7}, {"body", 8},
        {"light", 9}, {"dark", 10}, {"ener", 12}}, id, column);
}

uint8_t missile(std::string_view token, const MonsterProjectileTable &projectiles, uint16_t id, int column)
{
    if (token == "0" || lower(std::string(token)) == "dagger")
    {
        return 0;
    }
    const MonsterProjectileEntry *pEntry = projectiles.findByToken(std::string(token));
    if (!pEntry || !pEntry->sourceMissileId || *pEntry->sourceMissileId == 0)
    {
        fail(MergeMonsterSourceErrorCode::MissingResource, id, column, token,
            "source missile ID missing from shared projectile table; refresh the combat package");
    }
    return *pEntry->sourceMissileId;
}

MergeBolsterRewards rewards(const Row &row, uint16_t id)
{
    MergeBolsterRewards result;
    result.experience = int32_t(integer(row[6], std::numeric_limits<int32_t>::max(), id, 6));
    const std::string value = lower(row[7]);
    if (value == "0" || value == "1")
    {
        return result; // Original parser only reacts to %, D, and L markers.
    }
    const size_t percent = value.find('%');
    const size_t level = value.find('l');
    const size_t goldStart = percent == std::string::npos ? 0 : percent + 1;
    result.itemChance = percent == std::string::npos ? 100 : byte(value.substr(0, percent), id, 7);
    if (goldStart < value.size() && value[goldStart] >= '0' && value[goldStart] <= '9')
    {
        size_t goldEnd = level == std::string::npos ? value.size() : level;
        if (goldEnd > goldStart && value[goldEnd - 1] == '+')
        {
            --goldEnd;
        }
        const MergeBolsterAttack gold = dice(std::string_view(value).substr(goldStart, goldEnd - goldStart), id, 7);
        result.goldDiceCount = gold.diceCount;
        result.goldDiceSides = gold.diceSides;
    }
    if (level != std::string::npos)
    {
        result.itemLevel = byte(std::string_view(value).substr(level + 1, 1), id, 7);
        // Source TreasureItemType numbers only; item filtering/drawing remains owned by shared item gameplay.
        result.itemType = tokenValue(std::string_view(value).substr(level + 2), {
            {"", 0}, {"weapon", 20}, {"armor", 21}, {"misc", 22}, {"sword", 23}, {"dagger", 24},
            {"axe", 25}, {"spear", 26}, {"bow", 27}, {"mace", 28}, {"club", 29}, {"staff", 30},
            {"leather", 31}, {"chain", 32}, {"plate", 33}, {"shield", 34}, {"helm", 35}, {"belt", 36},
            {"cape", 37}, {"gauntlets", 38}, {"boots", 39}, {"ring", 40}, {"amulet", 41}, {"wand", 42},
            {"scroll", 43}, {"gem", 46}, {"ore", 47},
            {"cloak", 0}, {"gloves", 0}}, id, 7); // These two exact original spellings are not source keywords.
    }
    return result;
}

void preferences(MergeBolsterCombatStats &stats, std::string_view token)
{
    // MM8 parser letters, not the MM6/MM7 class alphabet found in some imported rows.
    for (char letter : lower(std::string(token)))
    {
        switch (letter)
        {
        case '2': case '3': case '4': stats.preferredTargetCount = uint8_t(letter - '0'); break;
        case 'n': stats.preferredTargets |= 0x001; break;
        case 'c': stats.preferredTargets |= 0x002; break;
        case 'k': stats.preferredTargets |= 0x004; break;
        case 't': stats.preferredTargets |= 0x008; break;
        case 'm': stats.preferredTargets |= 0x010; break;
        case 'e': stats.preferredTargets |= 0x020; break;
        case 'v': stats.preferredTargets |= 0x040; break;
        case 'd': stats.preferredTargets |= 0x080; break;
        case 'x': stats.preferredTargets |= 0x100; break;
        case 'o': stats.preferredTargets |= 0x200; break;
        // Unrecognized original letters are ignored by the source parser, under the fixed table fingerprint only.
        case '0': case 'a': case 'r': case 'p': case 'g': case 'h': case 's': case 'w': break;
        default: fail(MergeMonsterSourceErrorCode::UnsupportedToken, stats.id, 15, token,
            "unsupported preference letter");
        }
    }
}

void bonus(MergeBolsterCombatStats &stats, std::string token)
{
    token = lower(std::move(token));
    token.erase(std::remove(token.begin(), token.end(), ' '), token.end());
    const size_t multiplier = token.find('x');
    stats.bonusMultiplier = multiplier == std::string::npos ? 1
        : byte(std::string_view(token).substr(multiplier + 1), stats.id, 16);
    stats.bonus = tokenValue(std::string_view(token).substr(0, multiplier), {
        {"0", 0}, {"curse", 1}, {"weak", 2}, {"asleep", 3}, {"drunk", 4}, {"insane", 5},
        {"poison1", 6}, {"poison2", 7}, {"poison3", 8}, {"disease1", 9}, {"disease2", 10},
        {"disease3", 11}, {"paralyze", 12}, {"uncon", 13}, {"dead", 14}, {"stone", 15}, {"errad", 16},
        {"brkitem", 17}, {"brkarmor", 18}, {"brkweapon", 19}, {"steal", 20}, {"age", 21},
        {"drainsp", 22}, {"afraid", 23}}, stats.id, 16);
}

MergeBolsterSpell spell(const Row &row, uint16_t id, int column, const SpellTable &spells, bool &corrected)
{
    MergeBolsterSpell result;
    result.chance = byte(row[column - 1], id, column - 1);
    if (row[column] == "0")
    {
        return result;
    }
    const std::vector<std::string> parts = split(row[column], ',');
    if (parts.size() != 3)
    {
        fail(MergeMonsterSourceErrorCode::InvalidRow, id, column, row[column], "expected spell,mastery,rank");
    }
    const std::string name = lower(parts[0]);
    if (name == "icebolt" || name == "iceblast")
    {
        // All ten occurrences in the authenticated source table; approved compact-Ice-spell-v1 policy.
        result.id = name == "icebolt" ? 26 : 32;
        corrected = true;
    }
    else
    {
        // Numeric outputs of the audited original parser plus MiscTweaks hooks. This is not a second
        // spell resource registry: every result must still resolve in the shared SpellTable below.
        result.id = tokenValue(name, {
            {"dispel magic", 80}, {"day of protection", 85}, {"hour of power", 86}, {"shield", 17},
            {"spirit lash", 52}, {"power cure", 77}, {"meteor shower", 9}, {"lightning bolt", 18},
            {"implosion", 20}, {"stone skin", 38}, {"haste", 5}, {"heroism", 51}, {"pain reflection", 95},
            {"sparks", 15}, {"light bolt", 78}, {"toxic cloud", 90}, {"shrapmetal", 93}, {"paralyze", 81},
            {"fireball", 6}, {"incinerate", 11}, {"fire bolt", 2}, {"poison spray", 24}, {"rock blast", 41},
            {"mass distortion", 44}, {"mass curse", 44}, {"acid burst", 29}, {"bless", 46},
            {"dragon breath", 97}, {"fate", 47}, {"harm", 70}, {"mind blast", 59}, {"blades", 39},
            {"psychic shock", 65}, {"psychic shockt", 65}, {"hammerhands", 73}, {"heal", 68},
            {"day-o-gods", 83}, {"deadly swarm", 37}, {"flying fist", 76}}, id, column);
    }
    const uint8_t mastery = tokenValue(parts[1], {{"n", 1}, {"e", 2}, {"m", 3}, {"g", 4}, {"gm", 4}},
        id, column);
    constexpr std::array<uint16_t, 5> MasteryBits = {0, 0, 0x400, 0x800, 0x1000};
    result.packedSkill = MasteryBits[mastery] | byte(parts[2], id, column);
    if (!spells.findById(result.id))
    {
        fail(MergeMonsterSourceErrorCode::MissingResource, id, column, row[column], "missing source spell resource");
    }
    return result;
}

MergeBolsterSpecial special(std::string_view token, uint16_t id,
    const std::map<std::string, uint16_t> &descriptorIds)
{
    if (token == "0")
    {
        return {};
    }
    const std::vector<std::string> parts = split(lower(std::string(token)), ',');
    MergeBolsterSpecial result;
    if (parts.size() == 2 && parts[0] == "shot" && parts[1].starts_with('x'))
    {
        result.kind = 1;
        result.c = byte(std::string_view(parts[1]).substr(1), id, 38);
    }
    else if (parts.size() == 3 && parts[0] == "summon")
    {
        result.kind = 2;
        result.b = tokenValue(parts[1], {{"air", 0}, {"ground", 1}}, id, 38);
        const auto target = descriptorIds.find(parts[2]);
        if (target == descriptorIds.end() || parts[2].empty())
        {
            fail(MergeMonsterSourceErrorCode::MissingResource, id, 38, token, "unknown summon internal model name");
        }
        result.a = tokenValue(std::string_view(parts[2]).substr(parts[2].size() - 1),
            {{"a", 1}, {"b", 2}, {"c", 3}}, id, 38);
        result.d = target->second; // Descriptors are already one-based.
    }
    else if (parts.size() == 3 && parts[0] == "explode")
    {
        const MergeBolsterAttack explosion = dice(parts[1], id, 38);
        result = {3, explosion.diceCount, explosion.diceSides, explosion.damageAdd, 4};
        // Original quoted raw field, rather than the third CSV token, was passed to damage parsing.
    }
    else
    {
        fail(MergeMonsterSourceErrorCode::UnsupportedToken, id, 38, token, "unsupported source special");
    }
    return result;
}

MergeBolsterCombatStats decode(const Row &row, uint16_t id, const MonsterProjectileTable &projectiles,
    const SpellTable &spells, const std::map<std::string, uint16_t> &descriptorIds, bool &corrected)
{
    MergeBolsterCombatStats result;
    result.id = id;
    result.level = byte(row[3], id, 3);
    result.fullHitPoints = int32_t(integer(row[4], std::numeric_limits<int32_t>::max(), id, 4));
    result.armorClass = int32_t(integer(row[5], std::numeric_limits<int32_t>::max(), id, 5));
    result.rewards = rewards(row, id);
    result.bloodSplat = uint16_t(integer(row[8], 65535, id, 8));
    result.fly = tokenValue(row[9], {{"n", 0}, {"y", 1}}, id, 9);
    result.moveType = tokenValue(row[10], {{"short", 0}, {"med", 1}, {"long", 2}, {"global", 3},
        {"free", 4}, {"stand", 5}}, id, 10);
    result.aiType = tokenValue(row[11], {{"suicidal", 0}, {"wimp", 1}, {"normal", 2}, {"aggress", 3}}, id, 11);
    result.hostileType = byte(row[12], id, 12);
    result.moveSpeed = int32_t(integer(row[13], std::numeric_limits<int32_t>::max(), id, 13));
    result.attackRecovery = int32_t(integer(row[14], std::numeric_limits<int32_t>::max(), id, 14));
    preferences(result, row[15]);
    bonus(result, row[16]);
    result.attack1 = dice(row[18], id, 18);
    result.attack1.type = damage(row[17], id, 17);
    result.attack1.missile = missile(row[19], projectiles, id, 19);
    result.attack2Chance = byte(row[20], id, 20);
    result.attack2 = dice(row[22], id, 22);
    result.attack2.type = damage(row[21], id, 21);
    result.attack2.missile = missile(row[23], projectiles, id, 23);
    result.spells[0] = spell(row, id, 25, spells, corrected);
    result.spells[1] = spell(row, id, 27, spells, corrected);
    for (size_t index = 0; index < result.resistances.size(); ++index)
    {
        const size_t column = 28 + index;
        result.resistances[index] = lower(row[column]) == "imm" ? 65000
            : uint16_t(integer(row[column], 65535, id, int(column)));
    }
    result.special = special(row[38], id, descriptorIds);
    return result;
}

MergeBolsterMonsterSettings settingsFor(const MergedBolsterMonsterEntry &source, uint16_t id)
{
    MergeBolsterMonsterSettings result;
    result.style = MergeBolsterStyle(tokenValue(source.style, {{"0", 0}, {"strength", 0}, {"endurance", 1},
        {"speed", 2}, {"magic", 3}, {"wimpy", 4}}, id, -1));
    result.creed = MergeBolsterCreed(tokenValue(source.creed, {{"0", 0}, {"neutral", 0}, {"light", 1},
        {"dark", 2}, {"peasant", 3}}, id, -1));
    result.magic = MergeBolsterMagic(tokenValue(source.preferredMagic, {{"0", 0}, {"any", 0}, {"fire", 1},
        {"air", 2}, {"water", 3}, {"earth", 4}, {"spirit", 5}, {"mind", 6}, {"body", 7}, {"light", 8},
        {"dark", 9}, {"self", 10}, {"elemental", 11}}, id, -1));
    result.ranged = source.newRangedAttacks;
    result.spells = source.newSpells;
    result.hpBySize = source.sizeAffectsHp;
    result.replicate = source.replicate;
    result.summons = source.newSummons;
    if (source.summonId.value_or(0) > SourceCount || source.extraPoints.value_or(0) != 0)
    {
        fail(MergeMonsterSourceErrorCode::UnsupportedRange, id, -1, "bolster settings",
            "unsupported summon ID or extra-points policy");
    }
    result.summonId = uint16_t(source.summonId.value_or(0));
    result.maximumHpBoostPercent = source.maxHpBoostPercent.value_or(300);
    return result;
}
}

MergeMonsterSourceError::MergeMonsterSourceError(MergeMonsterSourceIssue issue)
    : std::runtime_error("Merge monster source " + std::to_string(issue.templateId) + " column "
        + std::to_string(issue.column) + ": " + issue.reason + " [" + issue.token + "]"), m_issue(std::move(issue))
{
}

const MergeMonsterSourceIssue &MergeMonsterSourceError::issue() const
{
    return m_issue;
}

MergeMonsterSourceProfile MergeMonsterSourceProfile::fromRows(const std::vector<Row> &baseRows,
    const MonsterTable &descriptors, const MonsterProjectileTable &projectiles, const SpellTable &spells)
{
    if (baseRows.size() != SourceCount)
    {
        fail(MergeMonsterSourceErrorCode::UnsupportedProfile, 0, -1, "monster_data",
            "fixed source profile requires exactly 652 original base rows");
    }
    std::array<const Row *, SourceCount + 1> rows = {};
    for (const Row &row : baseRows)
    {
        if (row.size() != 39)
        {
            fail(MergeMonsterSourceErrorCode::InvalidRow, 0, -1, "monster_data", "expected 39 source columns");
        }
        const uint16_t id = uint16_t(integer(row[0], SourceCount, 0, 0));
        if (id == 0 || rows[id])
        {
            fail(MergeMonsterSourceErrorCode::InvalidRow, id, 0, row[0], "zero or duplicate source ID");
        }
        rows[id] = &row;
    }
    Fingerprint mechanics;
    mechanics.string("MergeMonsterSourceProfile/mechanics/v1");
    mechanics.number(SourceCount);
    Fingerprint models;
    models.string("MergeMonsterSourceProfile/descriptors/v1");
    models.number(SourceCount);
    std::map<std::string, uint16_t> descriptorIds;
    for (uint16_t id = 1; id <= SourceCount; ++id)
    {
        const Row &row = *rows[id]; // Exactly 652 unique nonzero IDs were checked above.
        const MonsterEntry *pDescriptor = descriptors.findById(int16_t(id));
        if (!pDescriptor || pDescriptor->internalName.empty() || lower(pDescriptor->internalName) != lower(row[2]))
        {
            fail(MergeMonsterSourceErrorCode::MissingResource, id, 2, row[2], "descriptor does not match source ID");
        }
        // Original zReactor rows share a name. Match MonsterTable's first-ID name lookup;
        // every descriptor remains authenticated by its own ID and the fixed fingerprint below.
        const auto firstDescriptor = descriptorIds.emplace(lower(pDescriptor->internalName), id).first;
        if (descriptors.findByInternalName(pDescriptor->internalName)
            != descriptors.findById(int16_t(firstDescriptor->second)))
        {
            fail(MergeMonsterSourceErrorCode::MissingResource, id, 2, row[2],
                "descriptor name index does not match first source ID");
        }
        for (size_t column = 0; column < row.size(); ++column)
        {
            if (column != 1)
            {
                mechanics.string(row[column]);
            }
        }
        for (const std::string &cell : descriptorRow(id, *pDescriptor))
        {
            models.string(cell);
        }
    }
    if (mechanics.value() != MechanicalFingerprint || models.value() != DescriptorFingerprint)
    {
        fail(MergeMonsterSourceErrorCode::UnsupportedProfile, 0, -1, "source fingerprints",
            "mechanical rows or descriptors differ from the authenticated original profile");
    }
    MergeMonsterSourceProfile result;
    result.m_entries.reserve(SourceCount);
    for (uint16_t id = 1; id <= SourceCount; ++id)
    {
        const Row &row = *rows[id];
        MergeMonsterSourceEntry entry;
        entry.identity = {MergeMonsterSourceProfileVersion, MergeMonsterSourceContentFixVersion,
            MechanicalFingerprint, DescriptorFingerprint, std::string(MergeMonsterSourceMemberSha256), id, id, row[2]};
        entry.modelHeight = descriptors.findById(int16_t(id))->height;
        entry.fly = tokenValue(row[9], {{"n", 0}, {"y", 1}}, id, 9);
        try
        {
            entry.combat = decode(row, id, projectiles, spells, descriptorIds, entry.compactIceSpellCorrected);
        }
        catch (const MergeMonsterSourceError &error)
        {
            // A bounded unresolved source token is recorded, never converted to a usable zero-valued template.
            // Broken shared resources/schema fail catalog construction rather than disguising a stale package.
            if (error.issue().code != MergeMonsterSourceErrorCode::UnsupportedToken)
            {
                throw;
            }
            entry.issue = error.issue();
        }
        result.m_entries.push_back(std::move(entry));
    }
    return result;
}

const std::vector<MergeMonsterSourceEntry> &MergeMonsterSourceProfile::entries() const
{
    return m_entries;
}

const MergeMonsterSourceEntry *MergeMonsterSourceProfile::findById(uint16_t id) const
{
    return id > 0 && id <= m_entries.size() ? &m_entries[id - 1] : nullptr;
}

const MergeBolsterCombatStats &MergeMonsterSourceProfile::requireById(uint16_t id) const
{
    const MergeMonsterSourceEntry *pEntry = findById(id);
    if (!pEntry)
    {
        fail(MergeMonsterSourceErrorCode::MissingTemplate, id, 0, std::to_string(id), "template is not spawnable");
    }
    if (pEntry->issue)
    {
        throw MergeMonsterSourceError(*pEntry->issue);
    }
    return *pEntry->combat;
}

MergeBolsterSummonReference MergeMonsterSourceProfile::summonReference(uint16_t id) const
{
    if (id == 0)
    {
        return {0, 0}; // Explicit original zero-initialized Fly slot, not a spawnable monster.
    }
    const MergeMonsterSourceEntry *pEntry = findById(id);
    if (!pEntry)
    {
        fail(MergeMonsterSourceErrorCode::MissingTemplate, id, 0, std::to_string(id), "missing summon reference");
    }
    return {id, pEntry->fly};
}

MergeBolsterFamilyInput MergeMonsterSourceProfile::familyInput(uint16_t id,
    const MergedBolsterMonsterTable &settings, const MergeMonsterFamilyContext &context) const
{
    requireById(id);
    const uint16_t first = uint16_t(((id - 1) / 3) * 3 + 1);
    if (first + 2 > SourceCount)
    {
        fail(MergeMonsterSourceErrorCode::IncompleteFamily, id, 0, std::to_string(id),
            "source does not contain the complete three-member family");
    }
    MergeBolsterFamilyInput result;
    result.map = context.map;
    result.effectivePartyLevel = context.effectivePartyLevel;
    result.bolsterAmountPercent = context.bolsterAmountPercent;
    result.multipliers = context.multipliers;
    result.enabled = context.enabled;
    result.templatesLocalized = context.templatesLocalized;
    result.familySeed = context.familySeed;
    for (size_t index = 0; index < 3; ++index)
    {
        const uint16_t memberId = uint16_t(first + index);
        const MergedBolsterMonsterEntry *pSettings = settings.findById(memberId);
        if (!pSettings || std::count_if(settings.entries().begin(), settings.entries().end(),
            [memberId](const MergedBolsterMonsterEntry &entry) { return entry.id == memberId; }) != 1)
        {
            fail(MergeMonsterSourceErrorCode::MissingResource, memberId, -1, "bolster settings",
                "missing or duplicate original monster settings");
        }
        MergeBolsterFamilyMember &member = result.members[index];
        member.original = requireById(memberId);
        member.settings = settingsFor(*pSettings, memberId);
        member.modelHeight = findById(memberId)->modelHeight;
        member.summonReference = summonReference(member.settings.summonId);
    }
    return result;
}

MergeMonsterStandardSummon MergeMonsterSourceProfile::standardSummonBefore(uint16_t id, uint64_t actorSeed,
    const MergeBolsterFamilyPlan *pPreparedFamily, bool restoreTemplateTreasure) const
{
    MergeMonsterStandardSummon result;
    result.actor.before = requireById(id);
    result.identity = findById(id)->identity;
    if (pPreparedFamily)
    {
        bool found = false;
        for (size_t index = 0; index < pPreparedFamily->input().members.size(); ++index)
        {
            const MergeBolsterFamilyMember &member = pPreparedFamily->input().members[index];
            if (member.original != requireById(member.original.id)
                || member.modelHeight != findById(member.original.id)->modelHeight)
            {
                fail(MergeMonsterSourceErrorCode::InvalidPreparedFamily, id, -1, "prepared family",
                    "prepared family belongs to another source catalog");
            }
            if (member.original.id == id)
            {
                result.actor.before = pPreparedFamily->templates()[index];
                found = true;
            }
        }
        if (!found || pPreparedFamily->profileVersion() != MergeBolsterProfileVersion)
        {
            fail(MergeMonsterSourceErrorCode::InvalidPreparedFamily, id, -1, "prepared family",
                "prepared family does not contain this source template");
        }
    }
    const MergeBolsterCombatStats &current = result.actor.before;
    if (current.fullHitPoints <= 0 || current.fullHitPoints > std::numeric_limits<int16_t>::max()
        || current.moveSpeed < 0 || current.moveSpeed > std::numeric_limits<int16_t>::max())
    {
        fail(MergeMonsterSourceErrorCode::UnsupportedRange, id, -1, "StdSummon",
            "current HP or velocity is not representable in source actor fields");
    }
    result.actor.currentHitPoints = current.fullHitPoints;
    result.actor.velocity = current.moveSpeed;
    result.actor.actorSeed = actorSeed;
    result.actor.before.hostileType = 4;
    if (!restoreTemplateTreasure)
    {
        const int32_t experience = current.rewards.experience;
        result.actor.before.rewards = {};
        result.actor.before.rewards.experience = experience;
    }
    // SpecialC is deliberately retained from the current template.
    return result;
}

void MergeMonsterSourceProfile::validateResolvedResources(const MergeBolsterCombatStats &combat,
    const MonsterProjectileTable &projectiles, const SpellTable &spells) const
{
    requireById(combat.id);
    for (uint8_t id : {combat.attack1.missile, combat.attack2.missile})
    {
        if (id != 0 && !projectiles.findBySourceMissileId(id))
        {
            fail(MergeMonsterSourceErrorCode::MissingResource, combat.id, -1, std::to_string(id),
                "resolved missile lacks a shared projectile resource");
        }
    }
    for (const MergeBolsterSpell &spell : combat.spells)
    {
        if (spell.id != 0 && !spells.findById(spell.id))
        {
            fail(MergeMonsterSourceErrorCode::MissingResource, combat.id, -1, std::to_string(spell.id),
                "resolved spell lacks a shared spell resource");
        }
    }
    if (combat.special.kind == 2)
    {
        requireById(combat.special.d);
    }
}
}
