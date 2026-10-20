#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OpenYAMM::Game
{
struct Character;
class CharacterDollTable;
class ClassMultiplierTable;

inline constexpr uint32_t BreachAlvarCompanionContentVersion = 2;
inline constexpr std::string_view BreachAlvarCompanionContentPath = "breach_alvar_companions.txt";

// NPCMercenaries, MenuChooseCharacter, RacialSkills, DataTablesSupport, ConstAndBits, ExtraPotions,
// Character selection, Class Skills, Class Starting Skills, Class Starting Stats, Class HP SP, Race Skills.
inline constexpr std::array<std::string_view, 12> BreachAlvarGenerationSourceHashes{{
    "95a53fa79282b211907ff33ca5509ac892283d111fd6a5a4f171acc75cb7384b",
    "46ce1fd90dd0519e1939362ba242e78b874bd84f712035d5fc605374a438b336",
    "c369f814799c47a732d698ee1363c3ceff7921d9de3aebcc6fa839d6dd145d61",
    "18ed199172ddd89ca0b78dae505bf7077d2229f158225ca463c317173ea277a5",
    "466b2f8b3ccceb3a2ab2d95795493dfabc523ddf3c3cb57d4c6dc33609f12893",
    "c655cf64c1349d6af5430f98e43aabd168b555e5a9c4b4047b4281e9cc06621f",
    "3f3021a8c16f68c6940daa6d4cf4db313ecb85faaa1f70714213b232dcf7ebdc",
    "d7eeb62980c132b160c027247dee30c86ffc8a7d2a968b9e6b76fd58dab57765",
    "0ceb1104f701fd904dc194a91c0513d1f296f644de428cbd46b7d2f2e2148725",
    "e42706859ca4386ca1e3330929a6ef272bd1d02d82b59888003512fde942eb87",
    "af23362349f7a02f971de624ddc17d30f635c900229d1241c8571603c271b190",
    "99580627fa5afed87b6706338a4623f2272260ad8c8666c81afd454bedae0996",
}};

struct BreachAlvarRaceAppearanceRule
{
    uint32_t raceId = 0;
    uint32_t monsterId = 0;
    bool operator==(const BreachAlvarRaceAppearanceRule &) const = default;
};

struct BreachAlvarClassAppearanceRule
{
    uint32_t classKind = 0;
    uint32_t otherSexMonsterId = 0;
    uint32_t sexOneMonsterId = 0;
    std::optional<uint32_t> overrideClassId;
    uint32_t overrideMonsterId = 0;
    bool operator==(const BreachAlvarClassAppearanceRule &) const = default;
};

// Read-only source table facts, not a second Character/roster store.
struct BreachAlvarPortraitFact
{
    uint32_t characterDataId = 0; // Original Face + 1, including the source's unused slots.
    int32_t raceId = -1;
    uint32_t defaultSex = 0;
    uint32_t npcPictureId = 0;
    bool operator==(const BreachAlvarPortraitFact &) const = default;
};

struct BreachAlvarClassFact
{
    uint32_t classId = 0;
    uint32_t classKind = 0;
    bool operator==(const BreachAlvarClassFact &) const = default;
};

struct BreachAlvarHostilityRule
{
    uint32_t targetMonsterKind = 0;
    uint32_t hostility = 0;
    bool operator==(const BreachAlvarHostilityRule &) const = default;
};

struct BreachAlvarFriendActorRules
{
    uint32_t npcId = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    uint32_t group = 0;
    uint32_t ally = 0;
    uint32_t fullHealthMultiplier = 0;
    bool initiallyHostile = false;
    uint32_t restoreIntervalGameMinutes = 0;
    uint32_t restoreHealth = 0;
    bool restoreClearsHostile = false;
    bool restoreSkipsRemoved = false;
    bool restoreDeadToStand = false;
    // Directed, map-local source HostileTxt[monsterKind][targetKind] entries.
    std::vector<BreachAlvarHostilityRule> hostility;
    bool operator==(const BreachAlvarFriendActorRules &) const = default;
};

struct BreachAlvarCompanionProfile
{
    uint32_t version = 1; // Programmatic legacy profiles retain the existing-five-member contract.
    std::string canonicalMapKey;
    // BrAlvar.lua, Quest_CrossContinents.lua, Character portraits.txt, Class Extra.txt.
    std::array<std::string, 4> sourceSha256;
    uint32_t requiredPartySize = 0; // Final capacity, including prepared reserves.
    uint32_t minimumActiveCount = 5;
    uint32_t maximumActiveCount = 5;
    std::vector<std::string> generationSourceSha256; // Empty in v1; exact ordered evidence in v2.
    std::array<uint32_t, 4> npcIds{};
    std::array<uint32_t, 4> houseIds{};
    uint32_t initialTopicId = 0;
    uint32_t hintTopicId = 0;
    uint32_t hintExperience = 0;
    bool hintsFollowEncounterOrder = false;
    uint32_t defaultMonsterId = 0;
    std::vector<BreachAlvarRaceAppearanceRule> raceRules;
    std::vector<BreachAlvarClassAppearanceRule> classRules;
    std::vector<BreachAlvarPortraitFact> portraits;
    std::vector<BreachAlvarClassFact> classes;
    BreachAlvarFriendActorRules friendActor;

    bool operator==(const BreachAlvarCompanionProfile &) const = default;
};

// Strict, bounded content codec. This profile does not authorize scene admission or a transaction.
bool validBreachAlvarCompanionProfile(const BreachAlvarCompanionProfile &profile, std::string &error);
std::string serializeBreachAlvarCompanionProfile(const BreachAlvarCompanionProfile &profile);
std::optional<BreachAlvarCompanionProfile> loadBreachAlvarCompanionProfileText(
    const std::string &text, std::string &error);

struct BreachAlvarCompanionAppearance
{
    uint32_t originalFace = 0;
    uint32_t npcPictureId = 0;
    uint32_t monsterId = 0;
    uint32_t monsterKind = 0; // The source uses floor(monsterId / 3) + 1, without subtracting one.
    bool operator==(const BreachAlvarCompanionAppearance &) const = default;
};

std::optional<BreachAlvarCompanionAppearance> resolveBreachAlvarCompanionAppearance(
    const BreachAlvarCompanionProfile &profile, const Character &character,
    const CharacterDollTable &dolls, const ClassMultiplierTable &classes, std::string &error);
}
