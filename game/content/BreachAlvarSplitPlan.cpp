#include "game/content/BreachAlvarSplitPlan.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
uint64_t mixRandomBits(uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

uint64_t generationSeedForNpc(uint64_t seed, uint32_t npcId)
{
    constexpr uint64_t GenerationDomain = 0x4252414c56415231ULL; // "BRALVAR1"
    return mixRandomBits(seed ^ GenerationDomain ^ (uint64_t(npcId) * 0x9e3779b97f4a7c15ULL));
}

class SplitRandom
{
public:
    explicit SplitRandom(uint64_t seed) : m_state(seed)
    {
    }

    uint32_t bounded(uint32_t bound)
    {
        const uint64_t threshold = (uint64_t{0} - bound) % bound;
        uint64_t value;
        do
        {
            m_state += 0x9e3779b97f4a7c15ULL;
            ++m_drawCount;
            value = mixRandomBits(m_state);
        } while (value < threshold);
        return uint32_t(value % bound);
    }

    uint64_t state() const
    {
        return m_state;
    }

    uint64_t drawCount() const
    {
        return m_drawCount;
    }

private:
    uint64_t m_state;
    uint64_t m_drawCount = 0;
};

bool validOwnerKey(const std::string &key)
{
    return !key.empty() && key.size() <= 128 && key.find('\0') == std::string::npos;
}

struct OwnedMember
{
    const BreachAlvarMemberView *pMember = nullptr;
    BreachAlvarTransferSource source;
};
}

std::optional<BreachAlvarSplitPlan> planBreachAlvarSplit(
    const BreachAlvarSplitInput &input, uint64_t seed, std::string &error)
{
    error.clear();
    const auto fail = [&error](const char *pMessage) -> std::optional<BreachAlvarSplitPlan>
    {
        error = std::string("Breach Alvar split: ") + pMessage;
        return std::nullopt;
    };
    if (input.planVersion != BreachAlvarSplitPlanVersion || input.splitAlreadyCommitted
        || input.activeMembers.empty() || input.activeMembers.size() > 5 || input.nextMemberId == 0)
    {
        return fail("invalid version, existing split receipt, party size or next member identity");
    }

    std::unordered_map<uint64_t, OwnedMember> owned;
    std::unordered_map<uint32_t, uint64_t> reservedRosterOwners;
    const auto addOwned = [&](const BreachAlvarMemberView &member, BreachAlvarTransferSource source)
    {
        if (member.memberId == 0 || member.memberId >= input.nextMemberId || member.level == 0
            || owned.contains(member.memberId))
        {
            return false;
        }
        if (member.rosterId >= 38 && member.rosterId <= 41
            && !reservedRosterOwners.emplace(member.rosterId, member.memberId).second)
        {
            return false;
        }
        source.memberId = member.memberId;
        source.rosterId = member.rosterId;
        owned.emplace(member.memberId, OwnedMember{&member, std::move(source)});
        return true;
    };
    for (size_t index = 0; index < input.activeMembers.size(); ++index)
    {
        BreachAlvarTransferSource source;
        source.kind = BreachAlvarTransferKind::ActiveMember;
        source.activeMemberIndex = index;
        if (!addOwned(input.activeMembers[index], std::move(source)))
        {
            return fail("invalid or duplicate active identity or reserved roster owner");
        }
    }
    for (size_t index = 0; index < input.innMembers.size(); ++index)
    {
        BreachAlvarTransferSource source;
        source.kind = BreachAlvarTransferKind::InnMember;
        source.innMemberIndex = index;
        if (!addOwned(input.innMembers[index], std::move(source)))
        {
            return fail("invalid or duplicate inn identity or reserved roster owner");
        }
    }
    std::unordered_map<std::string, std::unordered_set<uint32_t>> detachedSlots;
    for (const BreachAlvarDetachedView &member : input.detachedMembers)
    {
        if (!validOwnerKey(member.ownerKey) || member.ownerKey == BreachAlvarSplitOwner || member.slotId == 0
            || !detachedSlots[member.ownerKey].insert(member.slotId).second)
        {
            return fail("invalid, duplicate or already split detached owner slot");
        }
        BreachAlvarTransferSource source;
        source.kind = BreachAlvarTransferKind::DetachedMember;
        source.detachmentOwnerKey = member.ownerKey;
        source.detachmentSlotId = member.slotId;
        if (!addOwned(member.member, std::move(source)))
        {
            return fail("invalid or duplicate detached identity or reserved roster owner");
        }
    }

    BreachAlvarSplitPlan result;
    result.seed = seed;
    result.leaderMemberId = input.activeMembers.front().memberId;
    result.setMissingFriendsQuestBit = input.activeMembers.size() > 1;
    SplitRandom random(seed);
    std::vector<size_t> remaining;
    for (size_t index = 1; index < input.activeMembers.size(); ++index)
    {
        remaining.push_back(index);
    }
    if (!remaining.empty())
    {
        const size_t selected = random.bounded(uint32_t(remaining.size()));
        result.companions[0].source = owned.at(input.activeMembers[remaining[selected]].memberId).source;
        remaining.erase(remaining.begin() + selected);
    }

    for (size_t slot = 0; slot < result.companions.size(); ++slot)
    {
        BreachAlvarCompanionPlan &companion = result.companions[slot];
        companion.npcId = uint32_t(772 + slot);
        if (slot == 0 && input.activeMembers.size() > 1)
        {
            continue;
        }
        if (slot > 0 && slot <= remaining.size())
        {
            companion.source = owned.at(input.activeMembers[remaining[slot - 1]].memberId).source;
            continue;
        }

        const uint32_t rosterId = uint32_t(38 + slot);
        const BreachAlvarReserveEvidence &reserve = input.reserves[slot];
        uint32_t experience = 0;
        uint32_t level = 0;
        if (reserve.kind == BreachAlvarReserveKind::UninstantiatedTemplate)
        {
            if (reserve.memberId != 0 || reserve.templateLevel == 0 || reservedRosterOwners.contains(rosterId))
            {
                return fail("template reserve conflicts with an existing owner or lacks valid template evidence");
            }
            companion.source.rosterId = rosterId;
            experience = reserve.templateExperience;
            level = reserve.templateLevel;
        }
        else if (reserve.kind == BreachAlvarReserveKind::OwnedMember)
        {
            const auto found = owned.find(reserve.memberId);
            if (found == owned.end() || found->second.pMember->rosterId != rosterId
                || found->second.source.kind == BreachAlvarTransferKind::ActiveMember)
            {
                return fail("reserved character is missing, has a different roster or is already active");
            }
            companion.source = found->second.source;
            experience = found->second.pMember->experience;
            level = found->second.pMember->level;
        }
        else
        {
            return fail("required reserve roster evidence is missing or unsupported");
        }
        if (experience == 100000 && level == 10)
        {
            const int64_t generatedLevel = std::max<int64_t>(1,
                int64_t(input.activeMembers.front().level) + int64_t(random.bounded(14)) - 5);
            if (generatedLevel > std::numeric_limits<uint32_t>::max())
            {
                return fail("generated level exceeds the character level range");
            }
            // Domain separation does not consume the selection/level stream or generate any character data.
            const uint64_t generationSeed = generationSeedForNpc(seed, companion.npcId);
            companion.generation = BreachAlvarGenerationRequest{rosterId, uint32_t(generatedLevel), generationSeed};
        }
    }
    result.finalRandomState = random.state();
    result.randomDrawCount = random.drawCount();
    return result;
}

bool validateBreachAlvarSplitRandomReceipt(
    uint64_t seed, uint32_t initialActiveCount, uint32_t leaderLevelAtSplit,
    const std::array<std::optional<BreachAlvarGenerationRequest>, 4> &generations,
    uint64_t finalRandomState, uint64_t randomDrawCount, std::string &error)
{
    error.clear();
    const auto fail = [&error](const char *pMessage)
    {
        error = std::string("Breach Alvar split random receipt: ") + pMessage;
        return false;
    };
    const bool hasGeneration = std::any_of(generations.begin(), generations.end(),
        [](const auto &request) { return request.has_value(); });
    if (initialActiveCount == 0 || initialActiveCount > 5
        || (leaderLevelAtSplit == 0 && (initialActiveCount != 5 || hasGeneration)))
    {
        return fail("invalid initial party size or historical leader level");
    }

    SplitRandom random(seed);
    if (initialActiveCount > 1)
    {
        random.bounded(initialActiveCount - 1);
    }
    for (size_t slot = 0; slot < generations.size(); ++slot)
    {
        if (!generations[slot]) continue;
        const BreachAlvarGenerationRequest &request = *generations[slot];
        if ((initialActiveCount > 1 && slot < initialActiveCount - 1) || request.rosterId != 38 + slot)
        {
            return fail("generation does not belong to a required source reserve");
        }
        const int64_t level = std::max<int64_t>(1,
            int64_t(leaderLevelAtSplit) + int64_t(random.bounded(14)) - 5);
        if (level > std::numeric_limits<uint32_t>::max() || request.level != uint32_t(level)
            || request.seed != generationSeedForNpc(seed, uint32_t(772 + slot)))
        {
            return fail("generated reserve level or seed differs from the v1 plan");
        }
    }
    if (random.state() != finalRandomState || random.drawCount() != randomDrawCount)
    {
        return fail("selection or level draw count and final state do not replay");
    }
    return true;
}
}
