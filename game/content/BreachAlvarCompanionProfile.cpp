#include "BreachAlvarCompanionProfile.h"

#include "game/content/BreachAlvarCompanionState.h"
#include "game/party/Party.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/ClassMultiplierTable.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
bool validMonster(uint32_t id)
{
    return id > 0 && id <= uint32_t(std::numeric_limits<int16_t>::max());
}

bool reject(std::string &error, const char *pMessage)
{
    error = pMessage;
    return false;
}

class ProfileReader
{
public:
    explicit ProfileReader(const std::string &text) : m_stream(text)
    {
    }

    void tag(const char *pExpected)
    {
        if (token() != pExpected)
        {
            throw std::runtime_error("Unexpected Breach Alvar companion profile field");
        }
    }

    std::string token()
    {
        std::string result;
        if (!(m_stream >> result))
        {
            throw std::runtime_error("Truncated Breach Alvar companion profile");
        }
        return result;
    }

    template<typename T> T number()
    {
        const std::string text = token();
        T result = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
        {
            throw std::runtime_error("Invalid Breach Alvar companion profile number");
        }
        return result;
    }

    bool flag()
    {
        const uint32_t value = number<uint32_t>();
        if (value > 1)
        {
            throw std::runtime_error("Invalid Breach Alvar companion profile flag");
        }
        return value != 0;
    }

    uint32_t count(uint32_t maximum)
    {
        const uint32_t value = number<uint32_t>();
        if (value > maximum)
        {
            throw std::runtime_error("Oversized Breach Alvar companion profile collection");
        }
        return value;
    }

    void finish()
    {
        std::string extra;
        if (m_stream >> extra)
        {
            throw std::runtime_error("Trailing Breach Alvar companion profile fields");
        }
    }

private:
    std::istringstream m_stream;
};
}

bool validBreachAlvarCompanionProfile(const BreachAlvarCompanionProfile &profile, std::string &error)
{
    error.clear();
    if ((profile.version != 1 && profile.version != BreachAlvarCompanionContentVersion)
        || profile.canonicalMapKey != BreachAlvarCompanionMap
        || profile.requiredPartySize != 5 || profile.npcIds != std::array<uint32_t, 4>{772, 773, 774, 775}
        || profile.houseIds != std::array<uint32_t, 4>{0, 705, 706, 707}
        || profile.initialTopicId != 1789 || profile.hintTopicId != 1793
        || profile.hintExperience != 0 || !profile.hintsFollowEncounterOrder)
    {
        return reject(error, "Unsupported Breach Alvar companion content contract");
    }
    if (profile.version == 1)
    {
        if (profile.minimumActiveCount != 5 || profile.maximumActiveCount != 5
            || !profile.generationSourceSha256.empty())
        {
            return reject(error, "Legacy Breach Alvar content cannot authorize reserve generation");
        }
    }
    else if (profile.minimumActiveCount != 1 || profile.maximumActiveCount != 5
        || profile.generationSourceSha256.size() != BreachAlvarGenerationSourceHashes.size()
        || !std::equal(profile.generationSourceSha256.begin(), profile.generationSourceSha256.end(),
            BreachAlvarGenerationSourceHashes.begin()))
    {
        return reject(error, "Missing or unsupported Breach Alvar reserve generation evidence");
    }
    for (const std::string &hash : profile.sourceSha256)
    {
        if (hash.size() != 64 || !std::all_of(hash.begin(), hash.end(), [](char c)
            {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            }))
        {
            return reject(error, "Missing or invalid Breach Alvar source fingerprint");
        }
    }
    if (!validMonster(profile.defaultMonsterId) || profile.raceRules.size() != 5 || profile.classRules.size() != 10
        || profile.portraits.size() != 77 || profile.classes.size() != 51)
    {
        return reject(error, "Incomplete Breach Alvar appearance content");
    }
    std::set<uint32_t> raceIds;
    for (const BreachAlvarRaceAppearanceRule &rule : profile.raceRules)
    {
        if (rule.raceId > 255 || !validMonster(rule.monsterId) || !raceIds.insert(rule.raceId).second)
        {
            return reject(error, "Invalid or duplicate Breach Alvar race appearance rule");
        }
    }
    std::set<uint32_t> classIds;
    for (const BreachAlvarClassFact &entry : profile.classes)
    {
        if (entry.classId > 255 || entry.classKind == 0 || entry.classKind > 255
            || !classIds.insert(entry.classId).second)
        {
            return reject(error, "Invalid or duplicate Breach Alvar class fact");
        }
    }
    std::set<uint32_t> classKinds;
    for (const BreachAlvarClassAppearanceRule &rule : profile.classRules)
    {
        if (rule.classKind == 0 || rule.classKind > 255 || !classKinds.insert(rule.classKind).second
            || !validMonster(rule.otherSexMonsterId) || !validMonster(rule.sexOneMonsterId)
            || (rule.overrideClassId ? !validMonster(rule.overrideMonsterId) : rule.overrideMonsterId != 0))
        {
            return reject(error, "Invalid or duplicate Breach Alvar class appearance rule");
        }
        const bool knownKind = std::any_of(profile.classes.begin(), profile.classes.end(), [&](const auto &entry)
        {
            return entry.classKind == rule.classKind;
        });
        const bool knownOverride = !rule.overrideClassId
            || std::any_of(profile.classes.begin(), profile.classes.end(), [&](const auto &entry)
            {
                return entry.classId == *rule.overrideClassId && entry.classKind == rule.classKind;
            });
        if (!knownKind || !knownOverride)
        {
            return reject(error, "Breach Alvar appearance refers to an unknown class");
        }
    }
    for (size_t index = 0; index < profile.portraits.size(); ++index)
    {
        const BreachAlvarPortraitFact &entry = profile.portraits[index];
        if (entry.characterDataId != index + 1 || entry.raceId < -1 || entry.raceId > 255
            || entry.defaultSex > 1 || entry.npcPictureId > 65535)
        {
            return reject(error, "Invalid or out-of-order Breach Alvar portrait fact");
        }
    }
    const BreachAlvarFriendActorRules &actor = profile.friendActor;
    if (actor.npcId != profile.npcIds[0] || actor.group > 255 || actor.ally > 32767
        || actor.fullHealthMultiplier == 0 || actor.fullHealthMultiplier > 100 || actor.initiallyHostile
        || actor.restoreIntervalGameMinutes == 0 || actor.restoreIntervalGameMinutes > 1440
        || actor.restoreHealth == 0 || actor.restoreHealth > 32767
        || !actor.restoreClearsHostile || !actor.restoreSkipsRemoved || !actor.restoreDeadToStand
        || actor.hostility.size() != 4)
    {
        return reject(error, "Invalid Breach Alvar friend actor rules");
    }
    std::set<uint32_t> targetKinds;
    for (const BreachAlvarHostilityRule &rule : actor.hostility)
    {
        if (rule.targetMonsterKind > 255 || rule.hostility > 4 || !targetKinds.insert(rule.targetMonsterKind).second)
        {
            return reject(error, "Invalid or duplicate Breach Alvar hostility rule");
        }
    }
    return true;
}

std::string serializeBreachAlvarCompanionProfile(const BreachAlvarCompanionProfile &profile)
{
    std::string error;
    if (!validBreachAlvarCompanionProfile(profile, error))
    {
        throw std::invalid_argument(error);
    }
    std::ostringstream out;
    out << "breach_alvar_companions " << profile.version << "\nmap " << profile.canonicalMapKey << '\n';
    for (const std::string &hash : profile.sourceSha256)
    {
        out << "source " << hash << '\n';
    }
    out << "party " << profile.requiredPartySize << '\n';
    if (profile.version >= 2)
    {
        out << "active " << profile.minimumActiveCount << ' ' << profile.maximumActiveCount << '\n';
        out << "generation_sources " << profile.generationSourceSha256.size() << '\n';
        for (const std::string &hash : profile.generationSourceSha256)
        {
            out << "generation_source " << hash << '\n';
        }
    }
    for (size_t index = 0; index < profile.npcIds.size(); ++index)
    {
        out << "npc " << profile.npcIds[index] << ' ' << profile.houseIds[index] << '\n';
    }
    out << "topics " << profile.initialTopicId << ' ' << profile.hintTopicId << ' ' << profile.hintExperience
        << ' ' << profile.hintsFollowEncounterOrder << "\ndefault_monster " << profile.defaultMonsterId;
    out << "\nraces " << profile.raceRules.size() << '\n';
    for (const auto &rule : profile.raceRules)
    {
        out << rule.raceId << ' ' << rule.monsterId << '\n';
    }
    out << "kinds " << profile.classRules.size() << '\n';
    for (const auto &rule : profile.classRules)
    {
        out << rule.classKind << ' ' << rule.otherSexMonsterId << ' ' << rule.sexOneMonsterId << ' '
            << bool(rule.overrideClassId) << ' ' << rule.overrideClassId.value_or(0) << ' '
            << rule.overrideMonsterId << '\n';
    }
    out << "portraits " << profile.portraits.size() << '\n';
    for (const auto &entry : profile.portraits)
    {
        out << entry.characterDataId << ' ' << entry.raceId << ' ' << entry.defaultSex << ' '
            << entry.npcPictureId << '\n';
    }
    out << "classes " << profile.classes.size() << '\n';
    for (const auto &entry : profile.classes)
    {
        out << entry.classId << ' ' << entry.classKind << '\n';
    }
    const BreachAlvarFriendActorRules &actor = profile.friendActor;
    out << "actor " << actor.npcId << ' ' << actor.x << ' ' << actor.y << ' ' << actor.z << ' '
        << actor.group << ' ' << actor.ally << ' ' << actor.fullHealthMultiplier << ' ' << actor.initiallyHostile;
    out << "\nrestore " << actor.restoreIntervalGameMinutes << ' ' << actor.restoreHealth << ' '
        << actor.restoreClearsHostile << ' ' << actor.restoreSkipsRemoved << ' ' << actor.restoreDeadToStand;
    out << "\nhostility " << actor.hostility.size() << '\n';
    for (const auto &rule : actor.hostility)
    {
        out << rule.targetMonsterKind << ' ' << rule.hostility << '\n';
    }
    out << "end\n";
    return out.str();
}

std::optional<BreachAlvarCompanionProfile> loadBreachAlvarCompanionProfileText(
    const std::string &text, std::string &error)
{
    error.clear();
    if (text.empty() || text.size() > 128 * 1024)
    {
        reject(error, "Invalid Breach Alvar companion profile size");
        return std::nullopt;
    }
    try
    {
        ProfileReader input(text);
        BreachAlvarCompanionProfile profile;
        input.tag("breach_alvar_companions");
        profile.version = input.number<uint32_t>();
        input.tag("map");
        profile.canonicalMapKey = input.token();
        for (std::string &hash : profile.sourceSha256)
        {
            input.tag("source");
            hash = input.token();
        }
        input.tag("party");
        profile.requiredPartySize = input.number<uint32_t>();
        if (profile.version >= 2)
        {
            input.tag("active");
            profile.minimumActiveCount = input.number<uint32_t>();
            profile.maximumActiveCount = input.number<uint32_t>();
            input.tag("generation_sources");
            profile.generationSourceSha256.resize(input.count(uint32_t(BreachAlvarGenerationSourceHashes.size())));
            for (std::string &hash : profile.generationSourceSha256)
            {
                input.tag("generation_source");
                hash = input.token();
            }
        }
        for (size_t index = 0; index < profile.npcIds.size(); ++index)
        {
            input.tag("npc");
            profile.npcIds[index] = input.number<uint32_t>();
            profile.houseIds[index] = input.number<uint32_t>();
        }
        input.tag("topics");
        profile.initialTopicId = input.number<uint32_t>();
        profile.hintTopicId = input.number<uint32_t>();
        profile.hintExperience = input.number<uint32_t>();
        profile.hintsFollowEncounterOrder = input.flag();
        input.tag("default_monster");
        profile.defaultMonsterId = input.number<uint32_t>();
        input.tag("races");
        profile.raceRules.resize(input.count(256));
        for (auto &rule : profile.raceRules)
        {
            rule.raceId = input.number<uint32_t>();
            rule.monsterId = input.number<uint32_t>();
        }
        input.tag("kinds");
        profile.classRules.resize(input.count(256));
        for (auto &rule : profile.classRules)
        {
            rule.classKind = input.number<uint32_t>();
            rule.otherSexMonsterId = input.number<uint32_t>();
            rule.sexOneMonsterId = input.number<uint32_t>();
            const bool hasOverride = input.flag();
            const uint32_t overrideId = input.number<uint32_t>();
            rule.overrideMonsterId = input.number<uint32_t>();
            if (hasOverride)
            {
                rule.overrideClassId = overrideId;
            }
            else if (overrideId != 0)
            {
                throw std::runtime_error("Unexpected Breach Alvar class override ID");
            }
        }
        input.tag("portraits");
        profile.portraits.resize(input.count(256));
        for (auto &entry : profile.portraits)
        {
            entry.characterDataId = input.number<uint32_t>();
            entry.raceId = input.number<int32_t>();
            entry.defaultSex = input.number<uint32_t>();
            entry.npcPictureId = input.number<uint32_t>();
        }
        input.tag("classes");
        profile.classes.resize(input.count(256));
        for (auto &entry : profile.classes)
        {
            entry.classId = input.number<uint32_t>();
            entry.classKind = input.number<uint32_t>();
        }
        input.tag("actor");
        BreachAlvarFriendActorRules &actor = profile.friendActor;
        actor.npcId = input.number<uint32_t>();
        actor.x = input.number<int32_t>();
        actor.y = input.number<int32_t>();
        actor.z = input.number<int32_t>();
        actor.group = input.number<uint32_t>();
        actor.ally = input.number<uint32_t>();
        actor.fullHealthMultiplier = input.number<uint32_t>();
        actor.initiallyHostile = input.flag();
        input.tag("restore");
        actor.restoreIntervalGameMinutes = input.number<uint32_t>();
        actor.restoreHealth = input.number<uint32_t>();
        actor.restoreClearsHostile = input.flag();
        actor.restoreSkipsRemoved = input.flag();
        actor.restoreDeadToStand = input.flag();
        input.tag("hostility");
        actor.hostility.resize(input.count(256));
        for (auto &rule : actor.hostility)
        {
            rule.targetMonsterKind = input.number<uint32_t>();
            rule.hostility = input.number<uint32_t>();
        }
        input.tag("end");
        input.finish();
        if (!validBreachAlvarCompanionProfile(profile, error))
        {
            return std::nullopt;
        }
        return profile;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return std::nullopt;
    }
}

std::optional<BreachAlvarCompanionAppearance> resolveBreachAlvarCompanionAppearance(
    const BreachAlvarCompanionProfile &profile, const Character &character,
    const CharacterDollTable &dolls, const ClassMultiplierTable &classes, std::string &error)
{
    if (!validBreachAlvarCompanionProfile(profile, error))
    {
        return std::nullopt;
    }
    const CharacterDollEntry *pPortrait = dolls.getCharacter(character.characterDataId);
    const ClassMultiplierEntry *pClass = classes.get(character.className);
    const auto portrait = std::find_if(profile.portraits.begin(), profile.portraits.end(), [&](const auto &entry)
    {
        return entry.characterDataId == character.characterDataId;
    });
    const auto classFact = std::find_if(profile.classes.begin(), profile.classes.end(), [&](const auto &entry)
    {
        return pClass && entry.classId == pClass->classId && entry.classKind == pClass->classKind;
    });
    if (!pPortrait || portrait == profile.portraits.end() || pPortrait->id != portrait->characterDataId
        || pPortrait->raceId != portrait->raceId || pPortrait->defaultSex != portrait->defaultSex
        || pPortrait->npcPictureId != portrait->npcPictureId || pPortrait->npcPictureId == 0
        || classFact == profile.classes.end())
    {
        reject(error, "Missing, unused, or mismatched Breach Alvar character presentation metadata");
        return std::nullopt;
    }
    uint32_t monsterId = profile.defaultMonsterId;
    const auto raceRule = std::find_if(profile.raceRules.begin(), profile.raceRules.end(), [&](const auto &rule)
    {
        return portrait->raceId >= 0 && rule.raceId == uint32_t(portrait->raceId);
    });
    if (raceRule != profile.raceRules.end())
    {
        monsterId = raceRule->monsterId;
    }
    else
    {
        const auto rule = std::find_if(profile.classRules.begin(), profile.classRules.end(), [&](const auto &entry)
        {
            return entry.classKind == pClass->classKind;
        });
        if (rule != profile.classRules.end())
        {
            monsterId = rule->overrideClassId == pClass->classId ? rule->overrideMonsterId
                : portrait->defaultSex == 1 ? rule->sexOneMonsterId : rule->otherSexMonsterId;
        }
    }
    return BreachAlvarCompanionAppearance{character.characterDataId - 1, pPortrait->npcPictureId,
        monsterId, monsterId / 3 + 1};
}
}
