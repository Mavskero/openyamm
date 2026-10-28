#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachMercenaryContentVersion = 1;
inline constexpr std::string_view BreachMercenaryContentPath = "breach_mercenaries.txt";
inline constexpr std::string_view BreachMercenaryMap = "mmmerge:breach.odm";

// Breach.lua, NPCMercenaries.lua, MenuChooseCharacter.lua, in this order.
inline constexpr std::array<std::string_view, 3> BreachMercenarySourceHashes{{
    "e3ef00b2de86c3f795fe18739adc978b41d8c7bd3c7aac08bafd90f4116a9eac",
    "95a53fa79282b211907ff33ca5509ac892283d111fd6a5a4f171acc75cb7384b",
    "46ce1fd90dd0519e1939362ba242e78b874bd84f712035d5fc605374a438b336",
}};

struct BreachMercenarySpot
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    bool operator==(const BreachMercenarySpot &) const = default;
};

struct BreachMercenaryActorRules
{
    bool initiallyHostile = false;
    uint32_t ally = 0;
    uint32_t guardRadius = 0;
    uint32_t gold = 0;
    uint32_t item = 0;
    bool operator==(const BreachMercenaryActorRules &) const = default;
};

// Authenticated source facts only. Character ownership, Hired provenance and NPC bindings live elsewhere.
struct BreachMercenaryProfile
{
    uint32_t version = BreachMercenaryContentVersion;
    std::string canonicalMapKey;
    std::array<std::string, 3> sourceSha256;
    uint32_t minimumSourceRosterId = 0;
    uint32_t maximumSourceRosterId = 0;
    uint32_t npcRosterOffset = 0;
    uint32_t partyCapacity = 0;
    std::array<BreachMercenarySpot, 11> spots{};
    std::array<uint32_t, 65> monsterIdByFace{}; // Zero represents Lua false at original Face 28, 29 and 30.
    uint32_t fallbackMonsterId = 0;
    BreachMercenaryActorRules actor;
    uint32_t joinTopicId = 0;
    uint32_t yesGlobalTextId = 0;
    uint32_t noGlobalTextId = 0;
    uint32_t partyFullNpcTextId = 0;
    std::array<uint32_t, 5> freshRosterIds{}; // NewGameMap prepares slot zero as source roster zero.
    bool chineseText = false;
    std::string globalTextSha256; // SHA256 of the selected LOD's uncompressed global.txt, before decoding.
    std::string yesText;
    std::string noText;
    bool operator==(const BreachMercenaryProfile &) const = default;
};

bool validBreachMercenaryProfile(const BreachMercenaryProfile &profile, std::string &error);
std::string serializeBreachMercenaryProfile(const BreachMercenaryProfile &profile);
std::optional<BreachMercenaryProfile> loadBreachMercenaryProfileText(const std::string &text, std::string &error);

// Invalid profiles and out-of-range source rosters produce no alias. This does not establish Character ownership.
std::optional<uint32_t> breachMercenaryNpcId(const BreachMercenaryProfile &profile, uint32_t sourceRosterId);

// Original Face is CharacterDollEntry::id - 1. Lua false and Face beyond 64 use fallbackMonsterId.
// The caller must separately resolve a valid CharacterDollEntry and its positive npcPictureId.
std::optional<uint32_t> breachMercenaryMonsterIdForFace(
    const BreachMercenaryProfile &profile, uint32_t originalFace);
}
