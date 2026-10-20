#include "doctest/doctest.h"
#include "game/content/BreachAlvarSplitPlan.h"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <string>
#include <utility>

using namespace OpenYAMM::Game;

namespace
{
BreachAlvarSplitInput inputFor(size_t count)
{
    BreachAlvarSplitInput input;
    input.nextMemberId = 1000;
    for (size_t index = 0; index < count; ++index)
    {
        // Multiple roster zero characters have distinct Party identities.
        input.activeMembers.push_back({10 + index, 0, 123456, index == 0 ? 20u : 99u});
    }
    for (BreachAlvarReserveEvidence &reserve : input.reserves)
    {
        reserve.kind = BreachAlvarReserveKind::UninstantiatedTemplate;
        reserve.templateExperience = 100000;
        reserve.templateLevel = 10;
    }
    return input;
}

BreachAlvarSplitPlan prepare(const BreachAlvarSplitInput &input, uint64_t seed)
{
    std::string error = "previous error";
    const auto plan = planBreachAlvarSplit(input, seed, error);
    REQUIRE_MESSAGE(plan.has_value(), error);
    CHECK(error.empty());
    return *plan;
}

void rejectWithoutMutation(const BreachAlvarSplitInput &input)
{
    const auto before = input;
    std::string error;
    CHECK_FALSE(planBreachAlvarSplit(input, 0, error).has_value());
    CHECK_FALSE(error.empty());
    CHECK(input == before);
}
}

TEST_CASE("BrAlvar plan covers every original party size and every legal NPC772 choice")
{
    for (size_t count = 1; count <= 5; ++count)
    {
        CAPTURE(count);
        const auto input = inputFor(count);
        const auto before = input;
        std::set<size_t> chosen;
        for (uint64_t seed = 0; seed < 128; ++seed)
        {
            const auto plan = prepare(input, seed);
            CHECK(plan == prepare(input, seed));
            CHECK(input == before);
            CHECK(plan.leaderMemberId == input.activeMembers.front().memberId);
            CHECK(plan.setMissingFriendsQuestBit == (count > 1));
            CHECK(plan.seed == seed);
            std::set<uint64_t> movedIds;
            std::vector<size_t> remainingSlots;
            if (count > 1)
            {
                const auto &first = plan.companions[0];
                REQUIRE(first.source.activeMemberIndex.has_value());
                const size_t selected = *first.source.activeMemberIndex;
                REQUIRE(selected > 0);
                REQUIRE(selected < count);
                chosen.insert(selected);
                for (size_t slot = 1; slot < count; ++slot)
                {
                    if (slot != selected)
                    {
                        remainingSlots.push_back(slot);
                    }
                }
            }
            for (size_t slot = 0; slot < 4; ++slot)
            {
                const auto &companion = plan.companions[slot];
                CHECK(companion.npcId == 772 + slot);
                if (companion.source.kind == BreachAlvarTransferKind::ActiveMember)
                {
                    REQUIRE(companion.source.activeMemberIndex.has_value());
                    const size_t originalSlot = *companion.source.activeMemberIndex;
                    REQUIRE(originalSlot < input.activeMembers.size());
                    CHECK(companion.source.memberId == input.activeMembers[originalSlot].memberId);
                    CHECK(companion.source.memberId != plan.leaderMemberId);
                    CHECK(movedIds.insert(companion.source.memberId).second);
                    CHECK_FALSE(companion.generation.has_value());
                    if (slot > 0)
                    {
                        REQUIRE(slot - 1 < remainingSlots.size());
                        CHECK(originalSlot == remainingSlots[slot - 1]);
                    }
                }
                else
                {
                    CHECK(companion.source.kind == BreachAlvarTransferKind::UninstantiatedTemplate);
                    CHECK(companion.source.memberId == 0);
                    CHECK(companion.source.rosterId == 38 + slot);
                    CHECK_FALSE(companion.source.activeMemberIndex.has_value());
                    REQUIRE(companion.generation.has_value());
                    CHECK(companion.generation->rosterId == 38 + slot);
                    CHECK(companion.generation->level >= 15);
                    CHECK(companion.generation->level <= 28);
                }
            }
            CHECK(movedIds.size() == count - 1);
        }
        CHECK(chosen.size() == count - 1);
    }
}

TEST_CASE("BrAlvar version one has fixed selection level and independent generation stream goldens")
{
    struct Golden
    {
        size_t count;
        uint64_t seed;
        std::array<int, 4> activeSlots; // -1 denotes source reserve roster, not an assigned member ID.
        std::array<uint32_t, 4> generatedLevels;
        uint64_t state;
        uint64_t draws;
    };
    const std::array<Golden, 7> goldens = {{
        {1, 0, {-1, -1, -1, -1}, {24, 23, 24, 19}, 8709371129873690708ULL, 4},
        {2, 0, {1, -1, -1, -1}, {0, 23, 24, 19}, 8709371129873690708ULL, 4},
        {3, 0, {2, 1, -1, -1}, {0, 0, 23, 24}, 15755400384260043839ULL, 3},
        {4, 0, {2, 1, 3, -1}, {0, 0, 0, 23}, 4354685564936845354ULL, 2},
        {5, 0, {4, 1, 2, 3}, {}, 11400714819323198485ULL, 1},
        {5, 1, {2, 1, 3, 4}, {}, 11400714819323198486ULL, 1},
        {5, UINT64_MAX, {1, 2, 3, 4}, {}, 11400714819323198484ULL, 1},
    }};
    const std::array<uint64_t, 4> generationSeeds = {
        7866118998006862880ULL, 5520317576090220576ULL, 11988668474769635317ULL, 5119434521468628895ULL};
    for (const Golden &golden : goldens)
    {
        CAPTURE(golden.count);
        CAPTURE(golden.seed);
        const auto plan = prepare(inputFor(golden.count), golden.seed);
        CHECK(plan.planVersion == 1);
        CHECK(plan.finalRandomState == golden.state);
        CHECK(plan.randomDrawCount == golden.draws);
        for (size_t slot = 0; slot < 4; ++slot)
        {
            const auto &companion = plan.companions[slot];
            if (golden.activeSlots[slot] >= 0)
            {
                CHECK(companion.source.activeMemberIndex == size_t(golden.activeSlots[slot]));
            }
            else
            {
                REQUIRE(companion.generation.has_value());
                CHECK(companion.generation->level == golden.generatedLevels[slot]);
                CHECK(companion.generation->seed == generationSeeds[slot]);
            }
        }
    }
}

TEST_CASE("BrAlvar generation requires both source defaults and uses the leader level with checked bounds")
{
    const std::array<std::pair<uint32_t, uint32_t>, 5> states = {{{100000, 10}, {99999, 10},
        {100001, 10}, {100000, 9}, {100000, 11}}};
    for (const auto &[experience, level] : states)
    {
        auto input = inputFor(1);
        for (auto &reserve : input.reserves)
        {
            reserve.templateExperience = experience;
            reserve.templateLevel = level;
        }
        const auto plan = prepare(input, 0);
        const bool needsGeneration = experience == 100000 && level == 10;
        for (const auto &companion : plan.companions)
        {
            CHECK(companion.generation.has_value() == needsGeneration);
        }
        CHECK(plan.randomDrawCount == (needsGeneration ? 4 : 0));
    }
    std::set<uint32_t> levels;
    for (uint64_t seed = 0; seed < 128; ++seed)
    {
        const auto high = prepare(inputFor(2), seed); // Other active member is level99; leader is level20.
        REQUIRE(high.companions[1].generation.has_value());
        levels.insert(high.companions[1].generation->level);
        auto low = inputFor(1);
        low.activeMembers[0].level = 1;
        const auto lowPlan = prepare(low, seed);
        for (const auto &companion : lowPlan.companions)
        {
            REQUIRE(companion.generation.has_value());
            CHECK(companion.generation->level >= 1);
            CHECK(companion.generation->level <= 9);
        }
    }
    CHECK(levels.size() == 14);
    CHECK(*levels.begin() == 15);
    CHECK(*levels.rbegin() == 28);
    auto overflow = inputFor(1);
    overflow.activeMembers[0].level = std::numeric_limits<uint32_t>::max();
    rejectWithoutMutation(overflow); // Seed0's first offset is +4; it must not wrap to a low level.
}

TEST_CASE("BrAlvar rejection draw and nonzero character generation seeds retain the version one contract")
{
    // Independent uint64 Python oracle, saved in 149/main-test-supplement-002/oracle.json.
    const auto input = inputFor(1);
    const auto zero = prepare(input, 0);
    const auto one = prepare(input, 1);
    const std::array<uint64_t, 4> oneSeeds = {6576037540791022519ULL, 9744246470470141576ULL,
        8778304705672814999ULL, 15876705547954027371ULL};
    const std::array<uint32_t, 4> oneLevels = {24, 22, 23, 22};
    CHECK(one.randomDrawCount == 4);
    CHECK(one.finalRandomState == 8709371129873690709ULL);
    std::set<uint64_t> uniqueSeeds;
    for (size_t index = 0; index < 4; ++index)
    {
        REQUIRE(one.companions[index].generation.has_value());
        REQUIRE(zero.companions[index].generation.has_value());
        CHECK(one.companions[index].generation->seed == oneSeeds[index]);
        CHECK(one.companions[index].generation->seed != zero.companions[index].generation->seed);
        CHECK(one.companions[index].generation->level == oneLevels[index]);
        CHECK(uniqueSeeds.insert(one.companions[index].generation->seed).second);
    }

    // The first state becomes zero, so its mixed value must be rejected for bound14 (threshold2).
    const auto rejectedFirstDraw = prepare(input, 0x61c8864680b583ebULL);
    CHECK(rejectedFirstDraw.randomDrawCount == 5);
    CHECK(rejectedFirstDraw.finalRandomState == 8709371129873690708ULL);
    const std::array<uint64_t, 4> rejectionSeeds = {13054453670304507230ULL, 16902136954921450947ULL,
        5021422409850203760ULL, 11697815066720866194ULL};
    for (size_t index = 0; index < 4; ++index)
    {
        REQUIRE(rejectedFirstDraw.companions[index].generation.has_value());
        CHECK(rejectedFirstDraw.companions[index].generation->level == zero.companions[index].generation->level);
        CHECK(rejectedFirstDraw.companions[index].generation->seed == rejectionSeeds[index]);
    }
}

TEST_CASE("BrAlvar owned reserve descriptions move the original inn or detached identity")
{
    auto input = inputFor(2);
    input.innMembers = {{101, 39, 54321, 7}, {102, 41, 100000, 10}};
    input.detachedMembers = {{"other.custody", 55, {103, 40, 67890, 17}}};
    input.reserves[1] = {BreachAlvarReserveKind::OwnedMember, 101};
    input.reserves[2] = {BreachAlvarReserveKind::OwnedMember, 103};
    input.reserves[3] = {BreachAlvarReserveKind::OwnedMember, 102};
    const auto before = input;
    const auto plan = prepare(input, 0);
    CHECK(input == before);
    CHECK(plan.companions[1].source.kind == BreachAlvarTransferKind::InnMember);
    CHECK(plan.companions[1].source.innMemberIndex == 0);
    CHECK(plan.companions[1].source.memberId == 101);
    CHECK_FALSE(plan.companions[1].generation.has_value());
    CHECK(plan.companions[2].source.kind == BreachAlvarTransferKind::DetachedMember);
    CHECK(plan.companions[2].source.memberId == 103);
    CHECK(plan.companions[2].source.detachmentOwnerKey == "other.custody");
    CHECK(plan.companions[2].source.detachmentSlotId == 55);
    CHECK_FALSE(plan.companions[2].generation.has_value());
    CHECK(plan.companions[3].source.kind == BreachAlvarTransferKind::InnMember);
    CHECK(plan.companions[3].source.innMemberIndex == 1);
    CHECK(plan.companions[3].source.memberId == 102);
    REQUIRE(plan.companions[3].generation.has_value());
    CHECK(plan.companions[3].generation->level == 23);
    CHECK(plan.randomDrawCount == 2); // One selection, one actual default reserve; no draws for preserved characters.
    CHECK(input.nextMemberId == before.nextMemberId);
}

TEST_CASE("BrAlvar rejects missing reserve evidence and conflicting ownership without partial plans")
{
    auto input = inputFor(4);
    input.reserves[3] = {};
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.reserves = {}; // A full original party needs no reserve evidence.
    CHECK(prepare(input, 0).companions[3].source.memberId != 0);

    input = inputFor(1);
    input.innMembers.push_back({100, 38, 50000, 9});
    rejectWithoutMutation(input); // Template claim cannot clone a real inn owner.
    input.reserves[0] = {BreachAlvarReserveKind::OwnedMember, 999};
    rejectWithoutMutation(input); // Claimed member is absent.
    input.reserves[0].memberId = 100;
    input.innMembers[0].rosterId = 39;
    rejectWithoutMutation(input); // Claimed member belongs to a different fixed source roster.
    input = inputFor(2);
    input.activeMembers[1].rosterId = 39;
    input.reserves[1] = {BreachAlvarReserveKind::OwnedMember, input.activeMembers[1].memberId};
    rejectWithoutMutation(input); // NPC772 and NPC773 cannot both own the same original person.
    input = inputFor(1);
    input.innMembers = {{100, 38, 1, 1}};
    input.detachedMembers = {{"other", 1, {101, 38, 2, 2}}};
    input.reserves[0] = {BreachAlvarReserveKind::OwnedMember, 100};
    rejectWithoutMutation(input); // Two identities claiming one reserved roster are ambiguous.
    input = inputFor(1);
    input.reserves[0].memberId = 99;
    rejectWithoutMutation(input); // Templates cannot carry an existing identity.
    input = inputFor(1);
    input.reserves[0].kind = static_cast<BreachAlvarReserveKind>(99);
    rejectWithoutMutation(input);
    input = inputFor(1);
    input.reserves[0].templateLevel = 0;
    rejectWithoutMutation(input);
}

TEST_CASE("BrAlvar validates all active inn and detached identities before planning a split")
{
    auto input = inputFor(0);
    rejectWithoutMutation(input);
    input = inputFor(6);
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.planVersion = 2;
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.splitAlreadyCommitted = true;
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.activeMembers[2].memberId = 0;
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.nextMemberId = input.activeMembers.back().memberId;
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.nextMemberId = 0;
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.innMembers.push_back(input.activeMembers[1]);
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.detachedMembers.push_back({"other", 1, input.activeMembers[2]});
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.detachedMembers = {{"other", 1, {100, 0, 1, 1}}, {"other", 1, {101, 0, 1, 1}}};
    rejectWithoutMutation(input);
    input.detachedMembers.resize(1);
    input.detachedMembers[0].ownerKey = std::string(BreachAlvarSplitOwner);
    rejectWithoutMutation(input);
    input.detachedMembers[0].ownerKey = std::string(129, 'x');
    rejectWithoutMutation(input);
    input.detachedMembers[0].ownerKey = std::string("bad\0owner", 9);
    rejectWithoutMutation(input);
    input.detachedMembers[0].ownerKey.clear();
    rejectWithoutMutation(input);
    input.detachedMembers[0].ownerKey = "other";
    input.detachedMembers[0].slotId = 0;
    rejectWithoutMutation(input);
    input = inputFor(5);
    input.innMembers = {{100, 0, 1, 0}};
    rejectWithoutMutation(input);
}
