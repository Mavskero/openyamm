#include "game/gameplay/MergeBolsterProfile.h"

#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

constexpr uint64_t FamilyDomain = UINT64_C(0x4d4246414d494c59);
constexpr uint64_t ActorDomain = UINT64_C(0x4d424143544f5221);

struct FormulaDefinition
{
    std::string_view target;
    std::string_view stat;
    std::string_view formula;
};

// Content signatures, not an expression interpreter. Unknown formulas cannot select this profile.
constexpr std::array<FormulaDefinition, 11> Formulas = {{
    {"def", "HP", "HP + HP/MonsterLevel * (PartyLevel-MonsterLevel) + "
        "(MonSettings.HPBySize and MonsterHeight/160 or 1) * HP"},
    {"def", "AC", "AC * MonsterPower"},
    {"def", "MoveSpeed", "(MonSettings.Style == 2 or MonSettings.Style == 0) and "
        "(min(MoveSpeed*PartyLevel/MonsterLevel, MonSettings.Style == 2 and MoveSpeed*3 or MoveSpeed*1.75)) "
        "or MoveSpeed"},
    {"def", "MaxDamage", "MaxDamage + max(MaxDamage * (1 + MonsterPower / 3 - 0.33), 0)"},
    {"def", "SpellSkill", "MonSettings.Style == 3 and min(SpellSkill*PartyLevel/MonsterLevel, 10*BolsterMul) "
        "or min(SpellSkill*MonsterPower, 7)"},
    {"def", "SpellMastery", "MonSettings.Style == 3 and min(floor(SpellMastery + PartyLevel/MonsterLevel), 4) "
        "or min(SpellMastery + MonsterPower, 3)"},
    {"def", "AllowNewSpell", "MapSettings.Spells and MonSettings.Spells and PartyLevel/MonsterLevel > 2"},
    {"def", "AllowSummons", "MapSettings.Summons and MonSettings.Summons and PartyLevel/MonsterLevel > 2"},
    {"def", "AllowReplicate", "MapSettings.Summons and MonSettings.Replicate and PartyLevel/MonsterLevel > 2"},
    {"def", "PlayerAC", "(PlayerAC * min((Monster.Level / Player.LevelBase),1)) "
        "^ clamp(2 - BolsterMul^2, 0.1, 1)"},
    {"76", "HP", "HP+(MonSettings.HPBySize and MonsterHeight/160 or 1)*"
        "(HP/MonsterLevel*(PartyLevel-MonsterLevel))"},
}};

[[noreturn]] void fail(MergeBolsterErrorCode code, const std::string &message)
{
    throw MergeBolsterError(code, message);
}

std::string formulaTokens(std::string_view text)
{
    std::string result;
    const auto wordCharacter = [](char value)
    {
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')
            || (value >= '0' && value <= '9') || value == '_';
    };
    for (size_t index = 0; index < text.size();)
    {
        const char value = text[index];
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' || value == '\v')
        {
            ++index;
            continue;
        }
        const size_t start = index++;
        if (value >= '0' && value <= '9')
        {
            while (index < text.size() && text[index] >= '0' && text[index] <= '9')
            {
                ++index;
            }
            if (index + 1 < text.size() && text[index] == '.' && text[index + 1] >= '0' && text[index + 1] <= '9')
            {
                ++index;
                while (index < text.size() && text[index] >= '0' && text[index] <= '9')
                {
                    ++index;
                }
            }
        }
        else if (wordCharacter(value))
        {
            while (index < text.size() && wordCharacter(text[index]))
            {
                ++index;
            }
        }
        else if (value == '=' && index < text.size() && text[index] == '=')
        {
            ++index;
        }
        result.append(text.substr(start, index - start));
        result.push_back('\0'); // Preserve token boundaries: "H P" must not authenticate as "HP".
    }
    return result;
}

double finite(double value, const char *field)
{
    if (!std::isfinite(value))
    {
        fail(MergeBolsterErrorCode::UnsupportedRange, std::string(field) + ": non-finite result");
    }
    return value;
}

template<class T>
T checkedInteger(double value, double minimum, double maximum, const char *field)
{
    const double integral = std::trunc(finite(value, field));
    if (integral < minimum || integral > maximum)
    {
        fail(MergeBolsterErrorCode::UnsupportedRange, std::string(field) + ": unrepresentable integer");
    }
    return static_cast<T>(integral);
}

uint8_t byte(double value, const char *field)
{
    return checkedInteger<uint8_t>(value, 0, 255, field);
}

int32_t nonnegativeInteger(double value, const char *field)
{
    return checkedInteger<int32_t>(value, 0, std::numeric_limits<int32_t>::max(), field);
}

double multiplyDifference(double oldValue, double newValue, double multiplier, const char *field)
{
    finite(newValue, field);
    return finite(oldValue + (newValue - oldValue) * multiplier, field);
}

void validateCombat(const MergeBolsterCombatStats &stats)
{
    if (stats.id == 0 || stats.fullHitPoints <= 0 || stats.armorClass < 0 || stats.moveSpeed < 0
        || stats.attackRecovery < 0 || stats.rewards.experience < 0 || stats.fly > 1 || stats.moveType > 5
        || stats.aiType > 3)
    {
        fail(MergeBolsterErrorCode::InvalidInput, "invalid source combat descriptor");
    }
    for (const MergeBolsterSpell &spell : stats.spells)
    {
        const uint16_t masteryBits = spell.packedSkill & 0xff00;
        if (masteryBits != 0 && masteryBits != 0x400 && masteryBits != 0x800 && masteryBits != 0x1000)
        {
            fail(MergeBolsterErrorCode::InvalidInput, "skill is not in the supported Merge eight-bit encoding");
        }
    }
}

void validateInput(const MergeBolsterFamilyInput &input)
{
    if (input.map.mapId == 0 || uint8_t(input.map.kind) > uint8_t(MergeBolsterMapKind::AllToEqual)
        || input.effectivePartyLevel < -65535 || input.effectivePartyLevel > 65535
        || input.map.levelShift < -65535 || input.map.levelShift > 65535
        || !std::isfinite(input.bolsterAmountPercent)
        || input.bolsterAmountPercent < 0 || input.bolsterAmountPercent > 200)
    {
        fail(MergeBolsterErrorCode::InvalidInput,
            "map, effective party level or bolster amount outside finite profile");
    }
    for (double value : {input.multipliers.hp, input.multipliers.ac, input.multipliers.maxDamage,
             input.multipliers.moveSpeed, input.multipliers.playerAc})
    {
        if (!std::isfinite(value) || value < 0)
        {
            fail(MergeBolsterErrorCode::InvalidInput, "multipliers must be finite and nonnegative");
        }
    }
    const uint32_t firstId = input.members[0].original.id;
    if (firstId == 0 || (firstId - 1) % 3 != 0 || firstId > 65533)
    {
        fail(MergeBolsterErrorCode::InvalidInput, "family must start at monster 3N-2");
    }
    for (size_t index = 0; index < input.members.size(); ++index)
    {
        const MergeBolsterFamilyMember &member = input.members[index];
        validateCombat(member.original);
        const MergeBolsterMonsterSettings &settings = member.settings;
        if (member.original.id != firstId + index || uint8_t(settings.style) > 4 || uint8_t(settings.creed) > 3
            || uint8_t(settings.magic) > 11 || settings.levelShift < -65535 || settings.levelShift > 65535)
        {
            fail(MergeBolsterErrorCode::InvalidInput, "incomplete, misordered family or invalid monster settings");
        }
        if (member.summonReference
            && (member.summonReference->id != settings.summonId || member.summonReference->fly > 1))
        {
            fail(MergeBolsterErrorCode::InvalidInput, "summon reference does not match source SummonId");
        }
    }
}

class RandomStream
{
public:
    RandomStream(uint64_t seed, MergeBolsterRandomDomain domain, std::vector<MergeBolsterDraw> &draws)
        : m_state(seed ^ (domain == MergeBolsterRandomDomain::Family ? FamilyDomain : ActorDomain)),
          m_domain(domain), m_draws(draws)
    {
    }

    uint32_t uniform(uint32_t low, uint32_t high, uint16_t monsterId, uint8_t slot, MergeBolsterRoll purpose)
    {
        if (low > high)
        {
            fail(MergeBolsterErrorCode::UnsupportedRange, "empty source random interval");
        }
        const uint64_t width = uint64_t(high) - low + 1;
        const uint64_t rejected = (uint64_t(0) - width) % width;
        uint64_t sample = next();
        while (sample < rejected)
        {
            sample = next();
        }
        const uint32_t value = low + uint32_t(sample % width);
        m_draws.push_back({m_domain, purpose, monsterId, slot, low, high, value, m_count});
        return value;
    }

private:
    uint64_t next()
    {
        m_state += UINT64_C(0x9e3779b97f4a7c15);
        ++m_count;
        uint64_t value = m_state;
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    uint64_t m_state;
    uint64_t m_count = 0;
    MergeBolsterRandomDomain m_domain;
    std::vector<MergeBolsterDraw> &m_draws;
};

double maximumDamage(const MergeBolsterAttack &attack)
{
    return double(attack.diceCount) * attack.diceSides + attack.damageAdd;
}

void assignDamage(MergeBolsterAttack &attack, double maximum, uint16_t monsterId, uint8_t slot,
    RandomStream &random)
{
    const double rounded = std::ceil(finite(maximum, "maximum damage"));
    if (rounded <= 0)
    {
        fail(MergeBolsterErrorCode::UnsupportedRange, "zero or negative damage produces an empty source dice interval");
    }
    const double fixed = rounded * random.uniform(1, 3, monsterId, slot, MergeBolsterRoll::DamagePortion) / 10;
    const double dice = std::sqrt(finite(rounded - fixed, "damage remainder"));
    const uint32_t extraBound = checkedInteger<uint32_t>(std::ceil(std::sqrt(dice)), 1,
        std::numeric_limits<uint32_t>::max(), "extra sides bound");
    const double sides = dice + random.uniform(1, extraBound, monsterId, slot, MergeBolsterRoll::ExtraSides);
    attack.damageAdd = byte(fixed, "damage add");
    attack.diceSides = byte(sides, "dice sides");
    attack.diceCount = byte(dice, "dice count");
}

uint16_t resolveSkill(uint8_t oldRank, uint8_t oldMastery, MergeBolsterStyle style,
    double partyLevel, double familyLevel, uint8_t power, double bolsterMultiplier)
{
    const bool magic = style == MergeBolsterStyle::Magic;
    const double rank = magic ? std::min(oldRank * partyLevel / familyLevel, 10 * bolsterMultiplier)
                             : std::min(double(oldRank) * power, 7.0);
    const double mastery = magic ? std::min(std::floor(oldMastery + partyLevel / familyLevel), 4.0)
                                : std::min(double(oldMastery) + power, 3.0);
    // JoinSkill clamps rank at 255 before adding mastery bits. The field assignment truncates last.
    const uint8_t masteryValue = byte(mastery, "spell mastery");
    constexpr std::array<uint16_t, 5> bits = {0, 0, 0x400, 0x800, 0x1000};
    if (masteryValue > 4 || mastery != masteryValue || rank < 0)
    {
        fail(MergeBolsterErrorCode::UnsupportedRange, "unrepresentable generated skill");
    }
    return checkedInteger<uint16_t>(std::min(finite(rank, "spell rank"), 255.0) + bits[masteryValue],
        0, 65535, "packed spell skill");
}

std::span<const uint8_t> spellsForSchool(uint32_t school, bool offensive)
{
    static constexpr std::array<std::array<uint8_t, 4>, 9> attack = {{
        {2, 6, 11}, {15, 18}, {24, 26, 29, 32}, {39, 41}, {46, 47, 51, 59},
        {59, 65}, {68, 70, 76}, {78, 87}, {90, 93, 95},
    }};
    static constexpr std::array<uint8_t, 9> attackCounts = {3, 2, 4, 2, 4, 2, 3, 2, 3};
    static constexpr std::array<std::array<uint8_t, 4>, 9> defense = {{
        {5}, {17}, {26}, {38}, {46, 47, 51, 52}, {59, 65}, {68, 71, 77}, {86}, {95},
    }};
    static constexpr std::array<uint8_t, 9> defenseCounts = {1, 1, 1, 1, 4, 2, 3, 1, 1};
    const size_t index = school - 1;
    return offensive ? std::span<const uint8_t>(attack[index].data(), attackCounts[index])
                     : std::span<const uint8_t>(defense[index].data(), defenseCounts[index]);
}

uint8_t generateSpell(const MergeBolsterMonsterSettings &settings, uint8_t step, uint8_t slot,
    uint8_t otherSpell, uint16_t monsterId, RandomStream &random)
{
    // Source passes 0 and 1, so its SpellNum==2 Strength/Speed branch is never taken here.
    if (settings.style == MergeBolsterStyle::Wimpy)
    {
        return 0;
    }
    uint32_t school = uint8_t(settings.magic);
    if (settings.magic == MergeBolsterMagic::Any)
    {
        school = random.uniform(1, 9, monsterId, slot, MergeBolsterRoll::SpellSchool);
    }
    else if (settings.magic == MergeBolsterMagic::Self)
    {
        school = random.uniform(5, 7, monsterId, slot, MergeBolsterRoll::SpellSchool);
    }
    else if (settings.magic == MergeBolsterMagic::Elemental)
    {
        school = random.uniform(1, 4, monsterId, slot, MergeBolsterRoll::SpellSchool);
    }
    const bool magic = settings.style == MergeBolsterStyle::Magic;
    const bool offensive = (settings.creed == MergeBolsterCreed::Dark && magic)
        || (settings.creed != MergeBolsterCreed::Dark && magic && slot == 0)
        || (settings.style == MergeBolsterStyle::Strength && slot == 0);
    const std::span<const uint8_t> candidates = spellsForSchool(school, offensive);
    const uint32_t prefix = std::min(uint32_t(candidates.size()), uint32_t(step) + slot);
    if (prefix == 0)
    {
        fail(MergeBolsterErrorCode::EmptySpellCandidates, "source generated spell has an empty candidate prefix");
    }
    uint8_t result = candidates[random.uniform(1, prefix, monsterId, slot, MergeBolsterRoll::SpellCandidate) - 1];
    if (slot == 1 && result == otherSpell)
    {
        result = spellsForSchool(school, false)[0];
        if (result == otherSpell)
        {
            result = 0;
        }
    }
    return result;
}

size_t familyIndex(const MergeBolsterFamilyPlan &family, uint16_t monsterId)
{
    const uint32_t first = family.input().members[0].original.id;
    if (monsterId < first || uint32_t(monsterId) >= first + 3)
    {
        fail(MergeBolsterErrorCode::InvalidInput, "actor does not belong to prepared family");
    }
    return monsterId - first;
}
}

MergeBolsterError::MergeBolsterError(MergeBolsterErrorCode code, const std::string &message)
    : std::runtime_error(message), m_code(code)
{
}

MergeBolsterErrorCode MergeBolsterError::code() const
{
    return m_code;
}

uint8_t MergeBolsterSpell::rank() const
{
    return packedSkill % 256;
}

uint8_t MergeBolsterSpell::mastery() const
{
    return packedSkill >= 0x1000 ? 4 : packedSkill >= 0x800 ? 3 : packedSkill >= 0x400 ? 2 : packedSkill > 0 ? 1 : 0;
}

uint32_t MergeBolsterFamilyPlan::profileVersion() const
{
    return MergeBolsterProfileVersion;
}

uint32_t MergeBolsterFamilyPlan::randomVersion() const
{
    return MergeBolsterRandomVersion;
}

const MergeBolsterFamilyInput &MergeBolsterFamilyPlan::input() const
{
    return m_input;
}

const std::array<MergeBolsterCombatStats, 3> &MergeBolsterFamilyPlan::templates() const
{
    return m_templates;
}

uint16_t MergeBolsterFamilyPlan::familyId() const
{
    return m_familyId;
}

int32_t MergeBolsterFamilyPlan::familyLevel() const
{
    return m_familyLevel;
}
std::optional<uint8_t> MergeBolsterFamilyPlan::bolsterStep() const
{
    return m_bolsterStep;
}

const std::vector<MergeBolsterDraw> &MergeBolsterFamilyPlan::draws() const
{
    return m_draws;
}

uint32_t MergeBolsterActorPlan::profileVersion() const
{
    return MergeBolsterProfileVersion;
}

uint32_t MergeBolsterActorPlan::randomVersion() const
{
    return MergeBolsterRandomVersion;
}

const MergeBolsterCombatStats &MergeBolsterActorPlan::combat() const
{
    return m_combat;
}

int32_t MergeBolsterActorPlan::currentHitPoints() const
{
    return m_currentHitPoints;
}

int32_t MergeBolsterActorPlan::velocity() const
{
    return m_velocity;
}

uint64_t MergeBolsterActorPlan::actorSeed() const
{
    return m_actorSeed;
}

const std::vector<MergeBolsterDraw> &MergeBolsterActorPlan::draws() const
{
    return m_draws;
}

MergeBolsterProfile MergeBolsterProfile::fromFormulas(std::span<const MergedBolsterFormulaEntry> formulas)
{
    if (formulas.size() != Formulas.size())
    {
        fail(MergeBolsterErrorCode::UnsupportedProfile, "expected exactly ten defaults and family76 HP override");
    }
    std::array<bool, Formulas.size()> found = {};
    for (const MergedBolsterFormulaEntry &entry : formulas)
    {
        bool matched = false;
        for (size_t index = 0; index < Formulas.size(); ++index)
        {
            const FormulaDefinition &expected = Formulas[index];
            if (entry.target != expected.target || entry.stat != expected.stat)
            {
                continue;
            }
            const std::optional<uint32_t> kind = expected.target == "76" ? std::optional<uint32_t>(76) : std::nullopt;
            if (found[index] || entry.monsterKindId != kind
                || formulaTokens(entry.formula) != formulaTokens(expected.formula))
            {
                fail(MergeBolsterErrorCode::UnsupportedProfile, "duplicate or changed formula: " + entry.stat);
            }
            found[index] = true;
            matched = true;
            break;
        }
        if (!matched)
        {
            fail(MergeBolsterErrorCode::UnsupportedProfile, "unknown formula target/stat");
        }
    }
    return MergeBolsterProfile();
}

MergeBolsterFamilyPlan MergeBolsterProfile::prepareFamily(const MergeBolsterFamilyInput &input) const
{
    validateInput(input);
    MergeBolsterFamilyPlan result;
    result.m_input = input;
    result.m_familyId = uint16_t((uint32_t(input.members[0].original.id) + 2) / 3);
    double sum = 0;
    for (size_t index = 0; index < input.members.size(); ++index)
    {
        result.m_templates[index] = input.members[index].original;
        sum += double(input.members[index].original.level) + input.members[index].settings.levelShift;
    }
    const double baseLevel = std::max(std::ceil(sum / 3), 3.0);
    const double amount = input.bolsterAmountPercent / 100;
    const double familyLevel = std::clamp(baseLevel + std::ceil(baseLevel * (1 - amount)), 3.0, baseLevel + 25);
    result.m_familyLevel = nonnegativeInteger(familyLevel, "family level");
    const double partyLevel = input.effectivePartyLevel;
    if (!input.enabled || !input.templatesLocalized || partyLevel < 0
        || input.map.kind == MergeBolsterMapKind::NoBolster
        || (familyLevel >= partyLevel && input.map.kind != MergeBolsterMapKind::AllToEqual))
    {
        return result;
    }
    const uint8_t step = byte(std::min(std::floor(partyLevel / familyLevel), 4.0), "bolster step");
    result.m_bolsterStep = step;
    RandomStream random(input.familySeed, MergeBolsterRandomDomain::Family, result.m_draws);
    for (size_t index = 0; index < input.members.size(); ++index)
    {
        const MergeBolsterFamilyMember &member = input.members[index];
        const MergeBolsterMonsterSettings &settings = member.settings;
        const MergeBolsterCombatStats &original = member.original;
        MergeBolsterCombatStats &stats = result.m_templates[index];
        const uint8_t power = uint8_t(index + 1);
        if (input.map.kind != MergeBolsterMapKind::OriginalStats)
        {
            const double hp = original.fullHitPoints;
            const double size = settings.hpBySize ? member.modelHeight / 160.0 : 1;
            const double growth = hp / familyLevel * (partyLevel - familyLevel);
            const double formula = result.m_familyId == 76 ? hp + size * growth : hp + growth + size * hp;
            const double boosted = multiplyDifference(hp, formula, input.multipliers.hp, "HP");
            stats.fullHitPoints = checkedInteger<int32_t>(std::min(boosted, 30000.0), 1, 30000, "HP");
            stats.armorClass = nonnegativeInteger(multiplyDifference(original.armorClass,
                double(original.armorClass) * power, input.multipliers.ac, "AC"), "AC");
            const auto boostAttack = [&](MergeBolsterAttack &attack, uint8_t slot)
            {
                const double oldMaximum = maximumDamage(attack);
                const double newMaximum = oldMaximum + std::max(oldMaximum * (1 + power / 3.0 - 0.33), 0.0);
                assignDamage(attack, multiplyDifference(oldMaximum, newMaximum, input.multipliers.maxDamage,
                    "max damage"), stats.id, slot, random);
            };
            boostAttack(stats.attack1, 0);
            if (stats.attack2Chance > 0)
            {
                boostAttack(stats.attack2, 1);
            }
            for (MergeBolsterSpell &spell : stats.spells)
            {
                if (spell.id > 0)
                {
                    spell.packedSkill = resolveSkill(spell.rank(), spell.mastery(), settings.style,
                        partyLevel, familyLevel, power, amount);
                }
            }
        }
        double speed = original.moveSpeed;
        if (settings.style == MergeBolsterStyle::Strength || settings.style == MergeBolsterStyle::Speed)
        {
            speed = std::min(speed * partyLevel / familyLevel,
                speed * (settings.style == MergeBolsterStyle::Speed ? 3.0 : 1.75));
        }
        stats.moveSpeed = nonnegativeInteger(multiplyDifference(original.moveSpeed, speed,
            input.multipliers.moveSpeed, "move speed"), "move speed");
        if (stats.attack2Chance == 0 && settings.ranged && step > 0)
        {
            stats.attack2Chance = std::min(step * 10, 35);
            stats.attack2.type = stats.attack1.type;
            stats.attack2.missile = step > 2 ? (settings.magic == MergeBolsterMagic::Any ? 1 : 6) : 0;
            assignDamage(stats.attack2, maximumDamage(stats.attack1), stats.id, 1, random);
        }
        const bool overDouble = partyLevel / familyLevel > 2;
        if (input.map.spells && settings.spells && overDouble)
        {
            const bool magic = settings.style == MergeBolsterStyle::Magic;
            const uint16_t skill = resolveSkill(magic ? 4 : 1, magic ? 2 : 1, settings.style,
                partyLevel, familyLevel, power, amount);
            if (stats.spells[0].id == 0 && (step >= 1 || magic))
            {
                stats.spells[0].packedSkill = skill;
                stats.spells[0].chance = magic ? 60 : 35;
            }
            if (stats.spells[1].id == 0 && (step >= 2 || (magic && step >= 1)))
            {
                stats.spells[1].packedSkill = skill;
                stats.spells[1].chance = magic ? 35 : 20;
            }
        }
        if (input.map.summons && settings.replicate && overDouble)
        {
            stats.special = {4, 0, 0, 2, settings.summonId};
        }
        if (input.map.summons && settings.summons && overDouble)
        {
            if (!member.summonReference)
            {
                fail(MergeBolsterErrorCode::MissingSummonReference, "missing source SummonId/Fly row, including ID0");
            }
            stats.special = {2, uint8_t(stats.moveType == 5 ? 0 : std::max(1 + step, 3)),
                uint8_t(member.summonReference->fly == 1 ? 0 : 1), 0,
                settings.summonId == 0 ? stats.id : settings.summonId};
        }
    }
    return result;
}

MergeBolsterActorPlan MergeBolsterProfile::prepareActor(const MergeBolsterFamilyPlan &family,
    const MergeBolsterActorInput &actor) const
{
    validateCombat(actor.before);
    const size_t index = familyIndex(family, actor.before.id);
    checkedInteger<int32_t>(actor.currentHitPoints, -32768, 32767, "old actor HP");
    checkedInteger<int32_t>(actor.velocity, -32768, 32767, "old actor velocity");
    const MergeBolsterFamilyInput &input = family.input();
    const MergeBolsterMonsterSettings &settings = input.members[index].settings;
    const MergeBolsterCombatStats &prepared = family.templates()[index];
    MergeBolsterActorPlan result;
    result.m_combat = actor.before;
    result.m_currentHitPoints = actor.currentHitPoints;
    result.m_velocity = actor.velocity;
    result.m_actorSeed = actor.actorSeed;
    if (actor.currentHitPoints > 0)
    {
        const double scaled = std::max(std::floor(double(prepared.fullHitPoints) * actor.currentHitPoints
            / actor.before.fullHitPoints), 1.0);
        result.m_currentHitPoints = checkedInteger<int32_t>(scaled, 1, 32767, "new actor HP");
    }
    MergeBolsterCombatStats &combat = result.m_combat;
    combat.fullHitPoints = prepared.fullHitPoints;
    combat.armorClass = prepared.armorClass;
    combat.moveSpeed = prepared.moveSpeed;
    result.m_velocity = checkedInteger<int32_t>(prepared.moveSpeed, -32768, 32767, "new actor velocity");
    combat.attack1.diceCount = prepared.attack1.diceCount;
    combat.attack1.diceSides = prepared.attack1.diceSides;
    combat.attack1.damageAdd = prepared.attack1.damageAdd;
    combat.attack2 = prepared.attack2;
    combat.attack2Chance = prepared.attack2Chance;
    RandomStream random(actor.actorSeed, MergeBolsterRandomDomain::Actor, result.m_draws);
    const bool needSpells = family.bolsterStep().has_value() && input.map.spells && settings.spells;
    for (uint8_t slot = 0; slot < 2; ++slot)
    {
        MergeBolsterSpell &spell = combat.spells[slot];
        if (spell.id == 81)
        {
            spell.id = 87;
        }
        // Deliberately inspect the old instance chance before overwriting it from the template.
        if (spell.chance > 0 && spell.id == 0 && needSpells)
        {
            spell.id = generateSpell(settings, *family.bolsterStep(), slot, combat.spells[0].id, combat.id, random);
        }
        spell.chance = prepared.spells[slot].chance;
        spell.packedSkill = prepared.spells[slot].packedSkill;
    }
    const uint8_t oldSpecialC = combat.special.c;
    combat.special = prepared.special;
    combat.special.c = oldSpecialC;
    combat.rewards.experience = prepared.rewards.experience;
    combat.rewards.itemChance = prepared.rewards.itemChance;
    combat.rewards.itemLevel = prepared.rewards.itemLevel;
    combat.level = prepared.level;
    return result;
}

int32_t MergeBolsterProfile::playerArmorClass(const MergeBolsterFamilyPlan &family, uint16_t monsterId,
    int32_t playerLevelBase, int32_t originalArmorClass) const
{
    const size_t index = familyIndex(family, monsterId);
    if (originalArmorClass < 0 || playerLevelBase <= 0)
    {
        fail(MergeBolsterErrorCode::InvalidInput, "player AC must be nonnegative and LevelBase positive");
    }
    if (!family.bolsterStep())
    {
        return originalArmorClass;
    }
    const double amount = family.input().bolsterAmountPercent / 100;
    const double exponent = std::clamp(2 - amount * amount, 0.1, 1.0);
    const double base = originalArmorClass * std::min(double(family.templates()[index].level) / playerLevelBase, 1.0);
    const double formula = std::pow(base, exponent);
    const double value = multiplyDifference(originalArmorClass, formula,
        family.input().multipliers.playerAc, "PlayerAC");
    return nonnegativeInteger(std::max(value, 0.0), "PlayerAC");
}
}
