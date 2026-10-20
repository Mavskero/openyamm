#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachAlvarSplitPlanVersion = 1;
inline constexpr std::string_view BreachAlvarSplitPrng = "splitmix64-rejection-v1";
inline constexpr std::string_view BreachAlvarSplitOwner = "MMerge.BrAlvar.Split";

// A read-only projection of every character owned by the current Party, including inn and detached custody.
struct BreachAlvarMemberView
{
    uint64_t memberId = 0;
    uint32_t rosterId = 0;
    uint32_t experience = 0;
    uint32_t level = 1;

    bool operator==(const BreachAlvarMemberView &) const = default;
};

struct BreachAlvarDetachedView
{
    std::string ownerKey;
    uint32_t slotId = 0;
    BreachAlvarMemberView member;

    bool operator==(const BreachAlvarDetachedView &) const = default;
};

enum class BreachAlvarReserveKind : uint8_t
{
    Unknown,
    UninstantiatedTemplate,
    OwnedMember,
};

struct BreachAlvarReserveEvidence
{
    BreachAlvarReserveKind kind = BreachAlvarReserveKind::Unknown;
    uint64_t memberId = 0; // OwnedMember only; resolved against the complete ownership view.
    uint32_t templateExperience = 0; // UninstantiatedTemplate only; caller must have verified the source roster.
    uint32_t templateLevel = 1;

    bool operator==(const BreachAlvarReserveEvidence &) const = default;
};

struct BreachAlvarSplitInput
{
    uint32_t planVersion = BreachAlvarSplitPlanVersion;
    uint64_t nextMemberId = 1;
    bool splitAlreadyCommitted = false;
    std::vector<BreachAlvarMemberView> activeMembers; // In current slot order; slot 0 remains the leader.
    std::vector<BreachAlvarMemberView> innMembers;
    std::vector<BreachAlvarDetachedView> detachedMembers;
    std::array<BreachAlvarReserveEvidence, 4> reserves; // Fixed roster IDs 38, 39, 40, 41.

    bool operator==(const BreachAlvarSplitInput &) const = default;
};

enum class BreachAlvarTransferKind : uint8_t
{
    ActiveMember,
    InnMember,
    DetachedMember,
    UninstantiatedTemplate,
};

struct BreachAlvarTransferSource
{
    BreachAlvarTransferKind kind = BreachAlvarTransferKind::UninstantiatedTemplate;
    uint64_t memberId = 0; // Zero means no existing character: the later transaction must allocate its identity.
    uint32_t rosterId = 0;
    std::optional<size_t> activeMemberIndex;
    std::optional<size_t> innMemberIndex;
    std::string detachmentOwnerKey;
    uint32_t detachmentSlotId = 0;

    bool operator==(const BreachAlvarTransferSource &) const = default;
};

struct BreachAlvarGenerationRequest
{
    uint32_t rosterId = 0;
    uint32_t level = 1;
    uint64_t seed = 0; // Independent generation stream; no character or equipment has been generated yet.

    bool operator==(const BreachAlvarGenerationRequest &) const = default;
};

struct BreachAlvarCompanionPlan
{
    uint32_t npcId = 0;
    BreachAlvarTransferSource source;
    std::optional<BreachAlvarGenerationRequest> generation;

    bool operator==(const BreachAlvarCompanionPlan &) const = default;
};

struct BreachAlvarSplitPlan
{
    uint32_t planVersion = BreachAlvarSplitPlanVersion;
    uint64_t seed = 0;
    uint64_t finalRandomState = 0;
    uint64_t randomDrawCount = 0;
    uint64_t leaderMemberId = 0;
    bool setMissingFriendsQuestBit = false; // Source Q1714/status message only when initial active count exceeds one.
    std::array<BreachAlvarCompanionPlan, 4> companions; // NPC772, 773, 774, 775 in source order.

    bool operator==(const BreachAlvarSplitPlan &) const = default;
};

// Pure preparation: no Party mutation, ID allocation, NPC publication, external random draws or save codec.
// The caller supplies a complete ownership view and must revalidate it before committing the returned transfers.
// This version preserves source conditions and order, not Lua math.random's exact stream or mercenary generation.
std::optional<BreachAlvarSplitPlan> planBreachAlvarSplit(
    const BreachAlvarSplitInput &input, uint64_t seed, std::string &error);

// Replays only the accepted v1 selection/level stream from immutable historical facts.
// A missing generation request consumes no level draw, regardless of current character progression.
bool validateBreachAlvarSplitRandomReceipt(
    uint64_t seed, uint32_t initialActiveCount, uint32_t leaderLevelAtSplit,
    const std::array<std::optional<BreachAlvarGenerationRequest>, 4> &generations,
    uint64_t finalRandomState, uint64_t randomDrawCount, std::string &error);
}
