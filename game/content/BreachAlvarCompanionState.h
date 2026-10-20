#pragma once

#include "game/content/BreachAlvarSplitPlan.h"
#include "game/party/Party.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachAlvarCompanionProfileVersion = 2;
inline constexpr std::string_view BreachAlvarCompanionMap = "mmmerge:bralvar.odm";

struct BreachAlvarCompanionGenerationReceipt
{
    BreachAlvarGenerationRequest request;
    uint64_t finalRandomState = 0;
    uint64_t randomDrawCount = 0;

    bool operator==(const BreachAlvarCompanionGenerationReceipt &) const = default;
};

struct BreachAlvarCompanionReceiptEntry
{
    uint64_t memberId = 0;
    bool returned = false;
    std::optional<BreachAlvarCompanionGenerationReceipt> generation;

    bool operator==(const BreachAlvarCompanionReceiptEntry &) const = default;
};

// Identity preparation only. This type is deliberately not the persistable Session state.
// Characters, their items and buffs belong exclusively to Party custody.
struct BreachAlvarCompanionReceipt
{
    uint32_t profileVersion = BreachAlvarCompanionProfileVersion;
    uint32_t planVersion = BreachAlvarSplitPlanVersion;
    uint64_t seed = 0;
    uint64_t finalRandomState = 0;
    uint64_t randomDrawCount = 0;
    uint64_t leaderMemberId = 0;
    uint32_t initialActiveCount = 5;
    uint32_t leaderLevelAtSplit = 0; // Zero is permitted only for migrated five-member, non-generated receipts.
    std::array<BreachAlvarCompanionReceiptEntry, 4> companions; // NPC772..775, in this fixed order.

    bool operator==(const BreachAlvarCompanionReceipt &) const = default;
};

struct BreachAlvarFriendActorBinding
{
    std::string canonicalMapKey;
    uint32_t actorId = 0; // Zero is a valid stable actor ID; the binding itself is mandatory.
    uint32_t spawnMonsterId = 0;

    bool operator==(const BreachAlvarFriendActorBinding &) const = default;
};

// Session may construct this only after its real world preparation supplies the actor binding.
// Structural validation below does not prove that this actor exists in the loaded/cached world.
struct BreachAlvarCompanionState
{
    BreachAlvarCompanionReceipt receipt;
    BreachAlvarFriendActorBinding friendActor;

    bool operator==(const BreachAlvarCompanionState &) const = default;
};

bool validBreachAlvarCompanionReceipt(
    const BreachAlvarCompanionReceipt &receipt, const Party::Snapshot &party, std::string &error);
bool validBreachAlvarCompanionState(
    const std::optional<BreachAlvarCompanionState> &state, const Party::Snapshot &party, std::string &error);
}
