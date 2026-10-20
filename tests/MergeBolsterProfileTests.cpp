#include "doctest/doctest.h"
#include "game/gameplay/MergeBolsterProfile.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

using namespace OpenYAMM::Game;

namespace
{
std::vector<MergedBolsterFormulaEntry> sourceFormulas()
{
    const std::filesystem::path path = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001/Data/Tables/Bolster - formulas.txt";
    std::ifstream stream(path);
    if (!stream)
    {
        throw std::runtime_error("original Merge formula table missing");
    }
    std::string line;
    std::getline(stream, line);
    std::vector<MergedBolsterFormulaEntry> result;
    while (std::getline(stream, line))
    {
        std::istringstream fields(line);
        MergedBolsterFormulaEntry entry;
        std::getline(fields, entry.target, '\t');
        std::getline(fields, entry.stat, '\t');
        std::getline(fields, entry.formula, '\t');
        if (entry.target.empty())
        {
            continue;
        }
        if (entry.target == "76")
        {
            entry.monsterKindId = 76;
        }
        result.push_back(entry);
    }
    return result;
}

const MergeBolsterProfile &profile()
{
    static const MergeBolsterProfile value = MergeBolsterProfile::fromFormulas(sourceFormulas());
    return value;
}

MergeBolsterFamilyInput familyInput(uint16_t familyId = 1)
{
    MergeBolsterFamilyInput input;
    input.map = {1, MergeBolsterMapKind::LowerToEqual, true, true, 0};
    input.effectivePartyLevel = 30;
    input.familySeed = 42;
    for (size_t index = 0; index < input.members.size(); ++index)
    {
        MergeBolsterFamilyMember &member = input.members[index];
        member.modelHeight = 160;
        MergeBolsterCombatStats &stats = member.original;
        stats.id = uint16_t(familyId * 3 - 2 + index);
        stats.level = 10;
        stats.fullHitPoints = 100;
        stats.armorClass = 20;
        stats.moveSpeed = 100;
        stats.attackRecovery = 90;
        stats.aiType = 2;
        stats.hostileType = 3;
        stats.bonus = 7;
        stats.bonusMultiplier = 2;
        stats.preferredTargetCount = 1;
        stats.preferredTargets = 0x42;
        stats.bloodSplat = 123;
        stats.attack1 = {3, 2, 4, 2, 0}; // Maximum10, deliberately distinct type.
        stats.attack2 = {6, 1, 3, 0, 5};
        stats.resistances = {1, 2, 3, 4, 5, 6, 7, 8, 9, 200};
        stats.special = {1, 2, 3, 4, 5};
        stats.rewards = {50, 2, 8, 30, 2, 5};
    }
    return input;
}

template<class Function>
void expectError(Function action, MergeBolsterErrorCode code)
{
    try
    {
        action();
        FAIL_CHECK("expected explicit profile/input/range failure");
    }
    catch (const MergeBolsterError &error)
    {
        CHECK(error.code() == code);
    }
}

// Independent scalar oracle for the published integer protocol. It knows nothing about formulas.
struct RollOracle
{
    uint64_t state;
    uint64_t count = 0;
    uint64_t next()
    {
        state += UINT64_C(0x9e3779b97f4a7c15);
        ++count;
        uint64_t bits = state;
        bits ^= bits >> 30;
        bits *= UINT64_C(0xbf58476d1ce4e5b9);
        bits ^= bits >> 27;
        bits *= UINT64_C(0x94d049bb133111eb);
        return bits ^ (bits >> 31);
    }
    uint32_t uniform(uint32_t low, uint32_t high)
    {
        const uint64_t width = uint64_t(high) - low + 1;
        const uint64_t threshold = (uint64_t(0) - width) % width;
        uint64_t word;
        do
        {
            word = next();
        } while (word < threshold);
        return low + uint32_t(word % width);
    }
};

void checkTrace(std::span<const MergeBolsterDraw> trace, uint64_t seed, MergeBolsterRandomDomain domain)
{
    RollOracle oracle{seed ^ (domain == MergeBolsterRandomDomain::Family
        ? UINT64_C(0x4d4246414d494c59) : UINT64_C(0x4d424143544f5221))};
    for (const MergeBolsterDraw &draw : trace)
    {
        CHECK(draw.domain == domain);
        REQUIRE(draw.minimum <= draw.maximum);
        CHECK(draw.value == oracle.uniform(draw.minimum, draw.maximum));
        CHECK(draw.rawDrawsAfter == oracle.count);
    }
}
}

TEST_CASE("Merge bolster authenticates the unique finite original formula profile")
{
    std::vector<MergedBolsterFormulaEntry> entries = sourceFormulas();
    REQUIRE(entries.size() == 11);
    CHECK_NOTHROW(MergeBolsterProfile::fromFormulas(entries));
    std::reverse(entries.begin(), entries.end());
    entries[0].notes = "Notes do not execute";
    entries[0].formula = " \n\t" + entries[0].formula + " \r";
    CHECK_NOTHROW(MergeBolsterProfile::fromFormulas(entries));

    SUBCASE("missing or additional formulas")
    {
        entries.push_back(entries[0]);
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
        entries.pop_back();
        entries.pop_back();
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
    }
    SUBCASE("duplicate replaces another required row")
    {
        entries[1] = entries[0];
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
    }
    SUBCASE("unrecognized executable expression")
    {
        entries[0].formula += "+ 0";
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
    }
    SUBCASE("metadata cannot disguise target family")
    {
        entries[0].monsterKindId = 75;
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
    }
    SUBCASE("whitespace cannot split an identifier or numeric token")
    {
        entries = sourceFormulas();
        entries[0].formula.insert(1, " ");
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
        entries = sourceFormulas();
        const size_t decimal = entries[3].formula.find("0.33");
        REQUIRE(decimal != std::string::npos);
        entries[3].formula.insert(decimal + 1, " ");
        expectError([&] { MergeBolsterProfile::fromFormulas(entries); }, MergeBolsterErrorCode::UnsupportedProfile);
    }
}

TEST_CASE("Merge bolster default and family76 HP use source family level size and multipliers")
{
    MergeBolsterFamilyInput input = familyInput();
    input.map.levelShift = 99; // Already included in effectivePartyLevel; must not be added again.
    input.members[1].settings.hpBySize = true;
    input.members[1].modelHeight = 320;
    input.members[1].settings.style = MergeBolsterStyle::Speed;
    input.members[2].settings.style = MergeBolsterStyle::Endurance;
    const MergeBolsterFamilyPlan originalPlan = profile().prepareFamily(input);
    CHECK(originalPlan.profileVersion() == 1);
    CHECK(originalPlan.randomVersion() == 1);
    CHECK(originalPlan.familyLevel() == 10);
    CHECK(originalPlan.bolsterStep() == 3);
    CHECK(originalPlan.templates()[0].fullHitPoints == 400); // 100 + 100/10*20 + 100.
    CHECK(originalPlan.templates()[1].fullHitPoints == 500); // Final size term is200.
    CHECK(originalPlan.templates()[2].fullHitPoints == 400);
    CHECK(originalPlan.templates()[0].moveSpeed == 175);
    CHECK(originalPlan.templates()[1].moveSpeed == 300);
    CHECK(originalPlan.templates()[2].moveSpeed == 100);
    CHECK(originalPlan.templates()[0].armorClass == 20);
    CHECK(originalPlan.templates()[1].armorClass == 40);
    CHECK(originalPlan.templates()[2].armorClass == 60);
    for (size_t index = 0; index < 3; ++index)
    {
        const MergeBolsterCombatStats &out = originalPlan.templates()[index];
        const MergeBolsterCombatStats &raw = input.members[index].original;
        CHECK(out.rewards == raw.rewards);
        CHECK(out.level == raw.level);
        CHECK(out.resistances == raw.resistances);
        CHECK(out.attackRecovery == raw.attackRecovery);
        CHECK(out.special == raw.special);
        CHECK(out.bonus == raw.bonus);
        CHECK(out.bonusMultiplier == raw.bonusMultiplier);
        CHECK(out.preferredTargets == raw.preferredTargets);
        CHECK(out.bloodSplat == raw.bloodSplat);
    }
    input.multipliers.hp = 0.5;
    input.multipliers.ac = 0.5;
    input.multipliers.moveSpeed = 0.5;
    const MergeBolsterFamilyPlan half = profile().prepareFamily(input);
    CHECK(half.templates()[0].fullHitPoints == 250);
    CHECK(half.templates()[1].fullHitPoints == 300);
    CHECK(half.templates()[1].armorClass == 30);
    CHECK(half.templates()[2].armorClass == 40);
    CHECK(half.templates()[0].moveSpeed == 137); // 137.5 truncates, not rounds.
    input = familyInput(76);
    input.members[1].settings.hpBySize = true;
    input.members[1].modelHeight = 320;
    const MergeBolsterFamilyPlan special = profile().prepareFamily(input);
    CHECK(special.familyId() == 76);
    CHECK(special.templates()[0].fullHitPoints == 300);
    CHECK(special.templates()[1].fullHitPoints == 500);
    input.members[0].original.fullHitPoints = 15000;
    input.members[0].settings.maximumHpBoostPercent = 1; // Source loads this but never uses it here.
    const MergeBolsterFamilyPlan capped = profile().prepareFamily(input);
    CHECK(capped.templates()[0].fullHitPoints == 30000);
    CHECK(input.members[0].original.fullHitPoints == 15000);
}

TEST_CASE("Merge bolster amount family completeness and preparation gates follow original map policy")
{
    MergeBolsterFamilyInput input = familyInput();
    input.members[0].original.level = 4;
    input.members[1].original.level = 10;
    input.members[2].original.level = 16;
    input.members[0].settings.levelShift = -1;
    input.members[2].settings.levelShift = 1;
    for (const auto [amount, expected] : std::array<std::pair<int, int>, 5>{{{0, 20}, {50, 15}, {100, 10},
             {150, 5}, {200, 3}}})
    {
        input.bolsterAmountPercent = amount;
        CHECK(profile().prepareFamily(input).familyLevel() == expected);
    }
    input = familyInput();
    for (MergeBolsterFamilyMember &member : input.members)
    {
        member.original.level = 100;
    }
    input.bolsterAmountPercent = 0;
    CHECK(profile().prepareFamily(input).familyLevel() == 125); // Upper clamp is base+25.
    input = familyInput();
    input.effectivePartyLevel = 10;
    MergeBolsterFamilyPlan skipped = profile().prepareFamily(input);
    CHECK_FALSE(skipped.bolsterStep().has_value());
    CHECK(skipped.draws().empty());
    CHECK(skipped.templates()[0] == input.members[0].original);
    input.map.kind = MergeBolsterMapKind::AllToEqual;
    const MergeBolsterFamilyPlan equal = profile().prepareFamily(input);
    CHECK(equal.bolsterStep() == 1);
    CHECK(equal.templates()[0].fullHitPoints == 200);
    input.effectivePartyLevel = 0;
    CHECK(profile().prepareFamily(input).bolsterStep() == 0); // Lua zero is truthy / prepared.
    input = familyInput();
    for (int gate = 0; gate < 4; ++gate)
    {
        MergeBolsterFamilyInput disabled = input;
        if (gate == 0)
        {
            disabled.enabled = false;
        }
        if (gate == 1)
        {
            disabled.templatesLocalized = false;
        }
        if (gate == 2)
        {
            disabled.effectivePartyLevel = -1;
        }
        if (gate == 3)
        {
            disabled.map.kind = MergeBolsterMapKind::NoBolster;
        }
        const MergeBolsterFamilyPlan out = profile().prepareFamily(disabled);
        CHECK_FALSE(out.bolsterStep().has_value());
        CHECK(out.draws().empty());
        CHECK(out.templates()[0] == input.members[0].original);
    }
}

TEST_CASE("Merge bolster family damage has fixed goldens and explicit draw order")
{
    MergeBolsterFamilyInput input = familyInput();
    const MergeBolsterFamilyPlan result = profile().prepareFamily(input);
    REQUIRE(result.draws().size() == 6);
    const std::array<uint32_t, 6> values = {1, 2, 1, 1, 2, 1};
    const std::array<MergeBolsterAttack, 3> expected = {{{3, 4, 6, 2, 0}, {3, 4, 5, 2, 0}, {3, 4, 5, 5, 0}}};
    // Ceil maxima21/24/27, fixed portions2.1/2.4/5.4. See independent-golden.json, not resolver output.
    for (size_t index = 0; index < 6; ++index)
    {
        CHECK(result.draws()[index].value == values[index]);
        CHECK(result.draws()[index].minimum == 1);
        CHECK(result.draws()[index].maximum == 3);
        CHECK(result.draws()[index].monsterId == index / 2 + 1);
        CHECK(result.draws()[index].slot == 0);
        CHECK(result.draws()[index].purpose == (index % 2 == 0
            ? MergeBolsterRoll::DamagePortion : MergeBolsterRoll::ExtraSides));
        CHECK(result.draws()[index].rawDrawsAfter == index + 1);
    }
    for (size_t index = 0; index < 3; ++index)
    {
        CHECK(result.templates()[index].attack1 == expected[index]);
    }
    checkTrace(result.draws(), input.familySeed, MergeBolsterRandomDomain::Family);

    input.multipliers.maxDamage = 0;
    const MergeBolsterFamilyPlan zeroMultiplier = profile().prepareFamily(input);
    CHECK(zeroMultiplier.templates()[0].attack1 == MergeBolsterAttack{3, 3, 5, 1, 0});
    CHECK(zeroMultiplier.draws().size() == 6); // The multiplier preserves maximum, not the old dice split.
    input.multipliers.maxDamage = 1;

    input.members[0].original.attack2Chance = 25;
    input.members[1].settings.ranged = true;
    input.members[1].settings.magic = MergeBolsterMagic::Fire;
    input.members[2].settings.ranged = true;
    const MergeBolsterFamilyPlan additional = profile().prepareFamily(input);
    REQUIRE(additional.draws().size() == 12);
    for (size_t index = 0; index < 12; ++index)
    {
        CHECK(additional.draws()[index].monsterId == index / 4 + 1);
        CHECK(additional.draws()[index].slot == (index % 4) / 2);
    }
    CHECK(additional.templates()[0].attack2Chance == 25);
    CHECK(additional.templates()[0].attack2.type == 6);
    CHECK(additional.templates()[0].attack2.missile == 5);
    CHECK(additional.templates()[1].attack2Chance == 30);
    CHECK(additional.templates()[1].attack2.type == 3);
    CHECK(additional.templates()[1].attack2.missile == 6);
    CHECK(additional.templates()[2].attack2.missile == 1);
    checkTrace(additional.draws(), input.familySeed, MergeBolsterRandomDomain::Family);
    for (int partyLevel : {11, 20, 30, 40})
    {
        input.effectivePartyLevel = partyLevel;
        const MergeBolsterFamilyPlan plan = profile().prepareFamily(input);
        CHECK(plan.templates()[1].attack2Chance == std::min((partyLevel / 10) * 10, 35));
        CHECK(plan.templates()[1].attack2.missile == (partyLevel >= 30 ? 6 : 0));
    }
}

TEST_CASE("Merge OriginalStats keeps base combat but still resolves movement attacks spells and special")
{
    MergeBolsterFamilyInput input = familyInput();
    input.map.kind = MergeBolsterMapKind::OriginalStats;
    input.members[0].settings.ranged = true;
    input.members[0].settings.spells = true;
    input.members[0].settings.replicate = true;
    input.members[0].settings.summonId = 17;
    input.members[1].original.spells[0] = {2, 30, 0x100c};
    const MergeBolsterFamilyPlan out = profile().prepareFamily(input);
    REQUIRE(out.draws().size() == 2); // Only new Attack2 rolls; no Attack1 or original Attack2 rolls.
    CHECK(out.draws()[0].slot == 1);
    CHECK(out.templates()[0].fullHitPoints == 100);
    CHECK(out.templates()[0].armorClass == 20);
    CHECK(out.templates()[0].attack1 == input.members[0].original.attack1);
    CHECK(out.templates()[0].moveSpeed == 175);
    CHECK(out.templates()[0].attack2Chance == 30);
    CHECK(out.templates()[0].attack2 == MergeBolsterAttack{3, 3, 5, 1, 1});
    // New Attack2 starts from old maximum10 in OriginalStats: fixed1, sqrt9=3, extra2, no formula applied.
    CHECK(out.templates()[0].spells[0].id == 0); // IDs belong to the actor phase.
    CHECK(out.templates()[0].spells[0].chance == 35);
    CHECK(out.templates()[0].spells[0].rank() == 1);
    CHECK(out.templates()[0].spells[0].mastery() == 2);
    CHECK(out.templates()[0].special == MergeBolsterSpecial{4, 0, 0, 2, 17});
    CHECK(out.templates()[1].spells[0] == input.members[1].original.spells[0]);
    CHECK(out.templates()[1].rewards == input.members[1].original.rewards);
}

TEST_CASE("Merge skills use eight-bit rank original caps and strict greater-than-two allowance")
{
    MergeBolsterFamilyInput input = familyInput();
    input.members[0].original.spells[0] = {2, 40, 0x080c};
    input.members[1].original.spells[0] = {81, 60, 0x040c};
    input.members[1].settings.style = MergeBolsterStyle::Magic;
    input.members[1].settings.spells = true;
    input.members[2].settings.spells = true;
    const MergeBolsterFamilyPlan out = profile().prepareFamily(input);
    CHECK(out.templates()[0].spells[0].packedSkill == 0x0807); // Existing rank12 is lowered to7.
    CHECK(out.templates()[1].spells[0].packedSkill == 0x100a); // min(12*3,10)=10, min(2+3,4)=4.
    CHECK(out.templates()[1].spells[0].id == 81); // Replacement happens in actor phase.
    CHECK(out.templates()[1].spells[1].packedSkill == 0x100a);
    CHECK(out.templates()[1].spells[1].chance == 35);
    CHECK(out.templates()[2].spells[0].packedSkill == 0x0803);
    CHECK(out.templates()[2].spells[1].chance == 20);
    input.bolsterAmountPercent = 50; // Family15; magic cap5.
    const MergeBolsterFamilyPlan halfAmount = profile().prepareFamily(input);
    CHECK(halfAmount.templates()[1].spells[0].packedSkill == 0x1005);
    CHECK(halfAmount.templates()[1].spells[1].chance == 0); // Ratio exactly2 does not allow new spells.
    input.bolsterAmountPercent = 0; // Family20, magic rank cap0; Mastery is still separately encoded.
    CHECK(profile().prepareFamily(input).templates()[1].spells[0].packedSkill == 0x0800);
    input.bolsterAmountPercent = 100;
    input.effectivePartyLevel = 25; // Generated magic4*2.5=10; existing12*2.5 caps10.
    CHECK(profile().prepareFamily(input).templates()[1].spells[0].packedSkill == 0x100a);
    input.members[1].original.spells[0].packedSkill = 0x0401;
    CHECK(profile().prepareFamily(input).templates()[1].spells[0].packedSkill == 0x1002); // 2.5 ->2.
}

TEST_CASE("Merge actor materialization retains old chance SpecialC type and noncopied loot fields")
{
    MergeBolsterFamilyInput input = familyInput();
    input.members[0].settings.style = MergeBolsterStyle::Magic;
    input.members[0].settings.magic = MergeBolsterMagic::Fire;
    input.members[0].settings.spells = true;
    input.members[0].settings.replicate = true;
    const MergeBolsterFamilyPlan family = profile().prepareFamily(input);
    MergeBolsterActorInput actor;
    actor.before = input.members[0].original;
    actor.before.attack1.type = 9;
    actor.before.attack1.missile = 12;
    actor.before.special.c = 9;
    actor.before.rewards = {12345, 8, 9, 99, 6, 7};
    actor.currentHitPoints = 25;
    actor.actorSeed = 42;
    const MergeBolsterActorPlan first = profile().prepareActor(family, actor);
    CHECK(first.profileVersion() == 1);
    CHECK(first.randomVersion() == 1);
    CHECK(first.currentHitPoints() == 100);
    CHECK(first.combat().fullHitPoints == 400);
    CHECK(first.combat().spells[0].id == 0); // StdSummon precedes first PrepareTxt: old chance is0.
    CHECK(first.combat().spells[1].id == 0);
    CHECK(first.combat().spells[0].chance == 60);
    CHECK(first.combat().spells[1].chance == 35);
    CHECK(first.draws().empty());
    CHECK(first.combat().attack1.type == 9);
    CHECK(first.combat().attack1.missile == 12);
    CHECK(first.combat().attack1.diceCount == family.templates()[0].attack1.diceCount);
    CHECK(first.combat().special.kind == 4);
    CHECK(first.combat().special.c == 9);
    CHECK(first.combat().rewards.experience == 50);
    CHECK(first.combat().rewards.itemChance == 30);
    CHECK(first.combat().rewards.itemLevel == 2);
    CHECK(first.combat().rewards.goldDiceCount == 8);
    CHECK(first.combat().rewards.goldDiceSides == 9);
    CHECK(first.combat().rewards.itemType == 7);
    CHECK(actor.currentHitPoints == 25);
    CHECK(actor.before.fullHitPoints == 100);

    actor.before = family.templates()[0]; // Later spawn sees already-prepared template chances.
    actor.currentHitPoints = 400;
    const MergeBolsterActorPlan later = profile().prepareActor(family, actor);
    CHECK(later.currentHitPoints() == 400);
    CHECK(later.combat().spells[0].id == 6); // Independent actor-domain seed42 golden: candidate2 then1.
    CHECK(later.combat().spells[1].id == 5);
    REQUIRE(later.draws().size() == 2); // Fixed Fire school consumes only two candidate rolls.
    CHECK(later.draws()[0].maximum == 3);
    CHECK(later.draws()[1].maximum == 1);
    CHECK(later.draws()[0].slot == 0);
    CHECK(later.draws()[1].slot == 1);
    checkTrace(later.draws(), actor.actorSeed, MergeBolsterRandomDomain::Actor);
    CHECK(profile().prepareActor(family, actor).draws() == later.draws());
    for (int hp : {0, -1, -32768})
    {
        actor.currentHitPoints = hp;
        CHECK(profile().prepareActor(family, actor).currentHitPoints() == hp);
    }
    actor.before.spells[0].id = 81;
    actor.before.spells[1].id = 81;
    const MergeBolsterActorPlan replaced = profile().prepareActor(family, actor);
    CHECK(replaced.combat().spells[0].id == 87);
    CHECK(replaced.combat().spells[1].id == 87);
    CHECK(replaced.draws().empty());
}

TEST_CASE("Merge summon overwrites replicate but reads exact SummonId zero Fly row")
{
    MergeBolsterFamilyInput input = familyInput();
    MergeBolsterFamilyMember &member = input.members[0];
    member.settings.replicate = true;
    member.settings.summons = true;
    member.settings.summonId = 0;
    member.original.fly = 0;
    member.summonReference = MergeBolsterSummonReference{0, 1};
    const MergeBolsterFamilyPlan flying = profile().prepareFamily(input);
    CHECK(flying.templates()[0].special == MergeBolsterSpecial{2, 4, 0, 0, 1});
    member.summonReference->fly = 0;
    member.original.moveType = 5;
    CHECK(profile().prepareFamily(input).templates()[0].special == MergeBolsterSpecial{2, 0, 1, 0, 1});
    member.settings.summonId = 9;
    member.summonReference = MergeBolsterSummonReference{9, 0};
    CHECK(profile().prepareFamily(input).templates()[0].special.d == 9);
    member.summonReference.reset();
    expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::MissingSummonReference);
    input.effectivePartyLevel = 20;
    CHECK_NOTHROW(profile().prepareFamily(input)); // Ratio2 does not dereference the unavailable row.
    input.effectivePartyLevel = 30;
    member.summonReference = MergeBolsterSummonReference{0, 0};
    expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
}

TEST_CASE("Merge actor spell schools duplicate fallback and source slot numbers are explicit")
{
    for (MergeBolsterMagic magic : {MergeBolsterMagic::Any, MergeBolsterMagic::Self, MergeBolsterMagic::Elemental})
    {
        MergeBolsterFamilyInput input = familyInput();
        input.members[0].settings.spells = true;
        input.members[0].settings.style = MergeBolsterStyle::Magic;
        input.members[0].settings.magic = magic;
        const MergeBolsterFamilyPlan family = profile().prepareFamily(input);
        for (uint64_t seed = 0; seed < 16; ++seed)
        {
            MergeBolsterActorInput actor{family.templates()[0], 400, 0, seed};
            const MergeBolsterActorPlan result = profile().prepareActor(family, actor);
            REQUIRE(result.draws().size() == 4);
            for (size_t slot = 0; slot < 2; ++slot)
            {
                const MergeBolsterDraw &school = result.draws()[slot * 2];
                CHECK(school.purpose == MergeBolsterRoll::SpellSchool);
                CHECK(school.minimum == (magic == MergeBolsterMagic::Self ? 5 : 1));
                CHECK(school.maximum == (magic == MergeBolsterMagic::Any ? 9
                    : magic == MergeBolsterMagic::Self ? 7 : 4));
                CHECK(result.draws()[slot * 2 + 1].purpose == MergeBolsterRoll::SpellCandidate);
            }
            checkTrace(result.draws(), seed, MergeBolsterRandomDomain::Actor);
        }
    }
    MergeBolsterFamilyInput input = familyInput();
    input.members[0].settings.spells = true;
    input.members[0].settings.magic = MergeBolsterMagic::Fire;
    input.members[0].settings.style = MergeBolsterStyle::Speed;
    const MergeBolsterFamilyPlan speed = profile().prepareFamily(input);
    MergeBolsterActorInput actor{speed.templates()[0], 400, 0, 3};
    const MergeBolsterActorPlan duplicate = profile().prepareActor(speed, actor);
    CHECK(duplicate.combat().spells[0].id == 5);
    CHECK(duplicate.combat().spells[1].id == 0); // Both selectdefensive5, fallback5 duplicates ->0.
    REQUIRE(duplicate.draws().size() == 2); // Source slot1 is not its unused SpellNum==2 early exit.
    input.members[0].settings.style = MergeBolsterStyle::Strength;
    const MergeBolsterFamilyPlan strength = profile().prepareFamily(input);
    actor.before = strength.templates()[0];
    CHECK(profile().prepareActor(strength, actor).combat().spells[1].id == 5);
    input.members[0].settings.style = MergeBolsterStyle::Wimpy;
    const MergeBolsterFamilyPlan wimpy = profile().prepareFamily(input);
    actor.before = wimpy.templates()[0];
    const MergeBolsterActorPlan empty = profile().prepareActor(wimpy, actor);
    CHECK(empty.combat().spells[0].id == 0);
    CHECK(empty.combat().spells[1].id == 0);
    CHECK(empty.draws().empty());

    input.members[0].settings.style = MergeBolsterStyle::Magic;
    input.members[0].settings.magic = MergeBolsterMagic::Water;
    input.members[0].settings.creed = MergeBolsterCreed::Dark;
    const MergeBolsterFamilyPlan dark = profile().prepareFamily(input);
    actor = {dark.templates()[0], 400, 0, 42};
    const MergeBolsterActorPlan darkSpells = profile().prepareActor(dark, actor);
    CHECK(darkSpells.combat().spells[0].id == 26);
    CHECK(darkSpells.combat().spells[1].id == 24); // Dark mage's second slot is offensive too.
    input.members[0].settings.creed = MergeBolsterCreed::Light;
    const MergeBolsterFamilyPlan light = profile().prepareFamily(input);
    actor.before = light.templates()[0];
    const MergeBolsterActorPlan lightSpells = profile().prepareActor(light, actor);
    CHECK(lightSpells.combat().spells[0].id == 26);
    CHECK(lightSpells.combat().spells[1].id == 0); // Defensive26 and fallback26 both duplicate first slot.

    input = familyInput();
    input.map.kind = MergeBolsterMapKind::AllToEqual;
    input.effectivePartyLevel = 0;
    input.members[0].settings.spells = true;
    input.members[0].original.spells[0].chance = 1;
    const MergeBolsterFamilyPlan zeroStep = profile().prepareFamily(input);
    actor.before = zeroStep.templates()[0];
    actor.currentHitPoints = 100;
    expectError([&] { profile().prepareActor(zeroStep, actor); }, MergeBolsterErrorCode::EmptySpellCandidates);
}

TEST_CASE("Merge player AC uses original exponent fifth multiplier and prepared gate")
{
    MergeBolsterFamilyInput input = familyInput();
    const MergeBolsterFamilyPlan family = profile().prepareFamily(input);
    CHECK(profile().playerArmorClass(family, 1, 20, 100) == 50);
    input.multipliers.playerAc = 0.5;
    CHECK(profile().playerArmorClass(profile().prepareFamily(input), 1, 20, 100) == 75);
    input.multipliers.playerAc = 1;
    input.bolsterAmountPercent = 200; // Exponent lower clamp0.1.
    // 2^10=1024 <10000 <59049=3^10, so truncation is2, away from a libm integer boundary.
    CHECK(profile().playerArmorClass(profile().prepareFamily(input), 1, 10, 10000) == 2);
    CHECK(profile().playerArmorClass(profile().prepareFamily(input), 1, 10, 0) == 0);
    input.bolsterAmountPercent = 100;
    input.multipliers.playerAc = 3;
    CHECK(profile().playerArmorClass(profile().prepareFamily(input), 1, 20, 100) == 0); // 100+(50-100)*3.
    input.enabled = false;
    CHECK(profile().playerArmorClass(profile().prepareFamily(input), 1, 20, 100) == 100);
    expectError([&] { profile().playerArmorClass(family, 1, 0, 100); }, MergeBolsterErrorCode::InvalidInput);
    expectError([&] { profile().playerArmorClass(family, 4, 20, 100); }, MergeBolsterErrorCode::InvalidInput);
}

TEST_CASE("Merge literal spell palettes cover all nine source schools with independent actor rolls")
{
    // These are source content fixtures, separate from the resolver's palette storage.
    const std::array<std::vector<uint8_t>, 9> offense = {{
        {2, 6, 11}, {15, 18}, {24, 26, 29, 32}, {39, 41}, {46, 47, 51, 59},
        {59, 65}, {68, 70, 76}, {78, 87}, {90, 93, 95},
    }};
    const std::array<std::vector<uint8_t>, 9> defense = {{
        {5}, {17}, {26}, {38}, {46, 47, 51, 52}, {59, 65}, {68, 71, 77}, {86}, {95},
    }};
    for (size_t school = 0; school < 9; ++school)
    {
        for (bool dark : {false, true})
        {
            MergeBolsterFamilyInput input = familyInput();
            input.members[0].settings.style = MergeBolsterStyle::Magic;
            input.members[0].settings.magic = static_cast<MergeBolsterMagic>(school + 1);
            input.members[0].settings.creed = dark ? MergeBolsterCreed::Dark : MergeBolsterCreed::Light;
            input.members[0].settings.spells = true;
            const MergeBolsterFamilyPlan family = profile().prepareFamily(input);
            for (uint64_t seed = 0; seed < 8; ++seed)
            {
                RollOracle oracle{seed ^ UINT64_C(0x4d424143544f5221)};
                const uint32_t firstBound = std::min(uint32_t(offense[school].size()), 3u);
                const uint8_t first = offense[school][oracle.uniform(1, firstBound) - 1];
                const std::vector<uint8_t> &secondPalette = dark ? offense[school] : defense[school];
                const uint32_t secondBound = std::min(uint32_t(secondPalette.size()), 4u);
                uint8_t second = secondPalette[oracle.uniform(1, secondBound) - 1];
                if (second == first)
                {
                    second = defense[school][0] == first ? 0 : defense[school][0];
                }
                const MergeBolsterActorPlan result = profile().prepareActor(family,
                    {family.templates()[0], 400, 0, seed});
                CHECK(result.combat().spells[0].id == first);
                CHECK(result.combat().spells[1].id == second);
                REQUIRE(result.draws().size() == 2);
                CHECK(result.draws()[0].maximum == firstBound);
                CHECK(result.draws()[1].maximum == secondBound);
            }
        }
    }
}

TEST_CASE("Merge finite profile rejects malformed input and overflow without modifying input")
{
    MergeBolsterFamilyInput input = familyInput();
    SUBCASE("family gap")
    {
        input.members[1].original.id = 4;
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
    }
    SUBCASE("nonfinite multiplier")
    {
        input.multipliers.hp = std::numeric_limits<double>::infinity();
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
    }
    SUBCASE("amount outside original menu")
    {
        input.bolsterAmountPercent = 201;
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
    }
    SUBCASE("unknown map and monster rules")
    {
        input.map.kind = static_cast<MergeBolsterMapKind>(4);
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
        input = familyInput();
        input.members[1].settings.magic = static_cast<MergeBolsterMagic>(12);
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
    }
    SUBCASE("invalid legacy skill bits")
    {
        input.members[0].original.spells[0].packedSkill = 0x0140;
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::InvalidInput);
    }
    SUBCASE("overflow cannot be concealed by HP cap")
    {
        input.multipliers.hp = std::numeric_limits<double>::max();
        const MergeBolsterFamilyInput before = input;
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::UnsupportedRange);
        CHECK(input == before);
    }
    SUBCASE("negative result")
    {
        input.map.kind = MergeBolsterMapKind::AllToEqual;
        input.effectivePartyLevel = 0;
        input.multipliers.moveSpeed = 2;
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::UnsupportedRange);
    }
    SUBCASE("zero source damage random interval")
    {
        input.members[0].original.attack1 = {};
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::UnsupportedRange);
    }
    SUBCASE("byte damage overflow")
    {
        input.members[0].original.attack1 = {0, 255, 255, 255, 0};
        expectError([&] { profile().prepareFamily(input); }, MergeBolsterErrorCode::UnsupportedRange);
    }
    SUBCASE("actor HP keeps explicit native int16 boundary")
    {
        const MergeBolsterFamilyPlan family = profile().prepareFamily(input);
        MergeBolsterActorInput actor{input.members[0].original, 32767, 0, 0};
        expectError([&] { profile().prepareActor(family, actor); }, MergeBolsterErrorCode::UnsupportedRange);
        actor.currentHitPoints = 32768;
        expectError([&] { profile().prepareActor(family, actor); }, MergeBolsterErrorCode::UnsupportedRange);
        actor.currentHitPoints = 100;
        actor.before.id = 4;
        expectError([&] { profile().prepareActor(family, actor); }, MergeBolsterErrorCode::InvalidInput);
    }
    SUBCASE("source velocity has its own int16 boundary")
    {
        input.members[0].original.moveSpeed = 32767;
        const MergeBolsterFamilyPlan family = profile().prepareFamily(input); // Template MoveSpeed is int32.
        MergeBolsterActorInput actor{input.members[0].original, 100, 0, 0};
        expectError([&] { profile().prepareActor(family, actor); }, MergeBolsterErrorCode::UnsupportedRange);
    }
}
