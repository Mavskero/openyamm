#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace OpenYAMM::Game
{
enum class SourceMercenaryRetentionKind : uint8_t
{
    Unknown,
    Retained,
    Released,
};

enum class SourceMercenaryProvenance : uint8_t
{
    None,
    OrdinaryDismissal,
    BrAlvarActiveDismissal,
    LegacyBrAlvarActiveDismissal,
    AuthenticatedSource,
};

// Character owns this fact. A known source alias alone proves neither a dismissal nor retention.
// Unknown/None may carry an explicitly authenticated alias, including a fresh source roster zero.
// An absent alias must never be filled by guessing an old zero-roster character's original slot.
struct SourceMercenaryRetention
{
    SourceMercenaryRetentionKind kind = SourceMercenaryRetentionKind::Unknown;
    SourceMercenaryProvenance provenance = SourceMercenaryProvenance::None;
    std::optional<uint32_t> sourceRosterId;

    bool operator==(const SourceMercenaryRetention &) const = default;
};

enum class OwnedMercenaryCustody : uint8_t
{
    Active,
    AdventurersInn,
    Detached,
};

// Positional active/Inn indices are deliberately excluded: each operation resolves memberId anew.
// ownerKey/slotId are populated only for Detached custody.
struct OwnedMercenaryOwner
{
    OwnedMercenaryCustody kind = OwnedMercenaryCustody::Active;
    std::string ownerKey;
    uint32_t slotId = 0;

    bool operator==(const OwnedMercenaryOwner &) const = default;
};

struct OwnedMercenaryIdentity
{
    uint64_t memberId = 0;
    uint32_t sourceRosterId = 0;
    OwnedMercenaryOwner owner;

    bool operator==(const OwnedMercenaryIdentity &) const = default;
};

// Weak presentation identity only. The containing world projection owns its map/profile identity;
// sceneGeneration and sourceActorIndex belong to transient queries/requests, never this saved value.
// A stale, active or released member keeps this binding so dialogue cannot fall through to a template hire.
struct OwnedMercenaryActorBinding
{
    uint64_t memberId = 0;
    uint32_t sourceRosterId = 0;
    uint32_t npcId = 0;
    uint32_t actorId = 0; // Zero is a valid stable actor ID.
    uint32_t sourceSpotIndex = 0;

    bool operator==(const OwnedMercenaryActorBinding &) const = default;
};
}
