#include "BreachMercenaryProfile.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace OpenYAMM::Game
{
namespace
{
bool reject(std::string &error, const char *pMessage)
{
    error = pMessage;
    return false;
}

bool validHash(const std::string &hash)
{
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char value)
    {
        return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
    });
}

bool validLabel(const std::string &text)
{
    if (text.empty() || text.size() > 256)
    {
        return false;
    }
    bool hasVisibleCharacter = false;
    for (size_t index = 0; index < text.size();)
    {
        const uint8_t lead = uint8_t(text[index++]);
        uint32_t codePoint = lead;
        uint32_t minimum = 0;
        size_t continuation = 0;
        if (lead >= 0xc2 && lead <= 0xdf)
        {
            codePoint = lead & 0x1f;
            minimum = 0x80;
            continuation = 1;
        }
        else if (lead >= 0xe0 && lead <= 0xef)
        {
            codePoint = lead & 0x0f;
            minimum = 0x800;
            continuation = 2;
        }
        else if (lead >= 0xf0 && lead <= 0xf4)
        {
            codePoint = lead & 0x07;
            minimum = 0x10000;
            continuation = 3;
        }
        else if (lead >= 0x80)
        {
            return false;
        }
        if (continuation > text.size() - index)
        {
            return false;
        }
        for (size_t count = 0; count < continuation; ++count)
        {
            const uint8_t value = uint8_t(text[index++]);
            if ((value & 0xc0) != 0x80)
            {
                return false;
            }
            codePoint = (codePoint << 6) | (value & 0x3f);
        }
        if (codePoint < minimum || codePoint > 0x10ffff || (codePoint >= 0xd800 && codePoint <= 0xdfff)
            || codePoint < 0x20 || (codePoint >= 0x7f && codePoint <= 0x9f))
        {
            return false;
        }
        hasVisibleCharacter = hasVisibleCharacter || codePoint != 0x20;
    }
    return hasVisibleCharacter;
}

class ProfileReader
{
public:
    explicit ProfileReader(const std::string &text) : m_stream(text)
    {
    }

    std::string token()
    {
        std::string result;
        if (!(m_stream >> result))
        {
            throw std::runtime_error("Truncated Breach mercenary profile");
        }
        return result;
    }

    void tag(const char *pExpected)
    {
        if (token() != pExpected)
        {
            throw std::runtime_error("Unexpected Breach mercenary profile field");
        }
    }

    template<typename T> T number()
    {
        const std::string text = token();
        T value = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
        {
            throw std::runtime_error("Invalid Breach mercenary profile number");
        }
        return value;
    }

    void expectedNumber(uint32_t expected)
    {
        if (number<uint32_t>() != expected)
        {
            throw std::runtime_error("Invalid Breach mercenary profile collection size or order");
        }
    }

    bool flag()
    {
        const uint32_t value = number<uint32_t>();
        if (value > 1)
        {
            throw std::runtime_error("Invalid Breach mercenary profile flag");
        }
        return value != 0;
    }

    std::string quotedText()
    {
        m_stream >> std::ws;
        std::string result;
        if (m_stream.peek() != '"' || !(m_stream >> std::quoted(result)))
        {
            throw std::runtime_error("Invalid quoted Breach mercenary label");
        }
        return result;
    }

    void finish()
    {
        m_stream >> std::ws;
        if (m_stream.peek() != std::char_traits<char>::eof())
        {
            throw std::runtime_error("Trailing Breach mercenary profile fields");
        }
    }

private:
    std::istringstream m_stream;
};
}

bool validBreachMercenaryProfile(const BreachMercenaryProfile &profile, std::string &error)
{
    error.clear();
    if (profile.version != BreachMercenaryContentVersion || profile.canonicalMapKey != BreachMercenaryMap
        || profile.minimumSourceRosterId != 0 || profile.maximumSourceRosterId != 49
        || profile.npcRosterOffset != 261 || profile.partyCapacity != 5
        || profile.fallbackMonsterId != 459
        || profile.freshRosterIds != std::array<uint32_t, 5>{0, 38, 39, 40, 41})
    {
        return reject(error, "Unsupported Breach mercenary content contract");
    }
    for (size_t index = 0; index < profile.sourceSha256.size(); ++index)
    {
        if (profile.sourceSha256[index] != BreachMercenarySourceHashes[index])
        {
            return reject(error, "Missing or unsupported Breach mercenary source fingerprint");
        }
    }
    if (profile.actor.initiallyHostile || profile.actor.ally != 9999 || profile.actor.guardRadius != 5000
        || profile.actor.gold != 0 || profile.actor.item != 0)
    {
        return reject(error, "Invalid Breach mercenary actor rules");
    }
    if (profile.joinTopicId != 100 || profile.yesGlobalTextId != 704 || profile.noGlobalTextId != 705
        || profile.partyFullNpcTextId != 1681 || !validHash(profile.globalTextSha256)
        || !validLabel(profile.yesText) || !validLabel(profile.noText))
    {
        return reject(error, "Invalid Breach mercenary dialogue content");
    }
    std::set<std::tuple<int32_t, int32_t, int32_t>> positions;
    for (const BreachMercenarySpot &spot : profile.spots)
    {
        for (int32_t value : {spot.x, spot.y, spot.z})
        {
            if (value < std::numeric_limits<int16_t>::min() || value > std::numeric_limits<int16_t>::max())
            {
                return reject(error, "Breach mercenary spot is outside the source coordinate range");
            }
        }
        if (!positions.emplace(spot.x, spot.y, spot.z).second)
        {
            return reject(error, "Duplicate Breach mercenary spot");
        }
    }
    for (size_t face = 0; face < profile.monsterIdByFace.size(); ++face)
    {
        const uint32_t monsterId = profile.monsterIdByFace[face];
        const bool sourceFalse = face >= 28 && face <= 30;
        if ((sourceFalse && monsterId != 0)
            || (!sourceFalse && (monsterId == 0 || monsterId > uint32_t(std::numeric_limits<int16_t>::max()))))
        {
            return reject(error, "Invalid Breach mercenary face mapping");
        }
    }
    return true;
}

std::string serializeBreachMercenaryProfile(const BreachMercenaryProfile &profile)
{
    std::string error;
    if (!validBreachMercenaryProfile(profile, error))
    {
        throw std::invalid_argument(error);
    }
    std::ostringstream out;
    out << "breach_mercenaries " << profile.version << "\nmap " << profile.canonicalMapKey
        << "\nsources " << profile.sourceSha256.size() << '\n';
    for (const std::string &hash : profile.sourceSha256)
    {
        out << "source " << hash << '\n';
    }
    out << "roster " << profile.minimumSourceRosterId << ' ' << profile.maximumSourceRosterId << ' '
        << profile.npcRosterOffset << "\nparty " << profile.partyCapacity
        << "\nspots " << profile.spots.size() << '\n';
    for (size_t index = 0; index < profile.spots.size(); ++index)
    {
        const BreachMercenarySpot &spot = profile.spots[index];
        out << "spot " << index + 1 << ' ' << spot.x << ' ' << spot.y << ' ' << spot.z << '\n';
    }
    out << "faces " << profile.monsterIdByFace.size() << '\n';
    for (size_t face = 0; face < profile.monsterIdByFace.size(); ++face)
    {
        out << "face " << face << ' ' << profile.monsterIdByFace[face] << '\n';
    }
    out << "fallback " << profile.fallbackMonsterId << "\nactor " << profile.actor.initiallyHostile << ' '
        << profile.actor.ally << ' ' << profile.actor.guardRadius << ' ' << profile.actor.gold << ' '
        << profile.actor.item << "\ntopics " << profile.joinTopicId << ' ' << profile.yesGlobalTextId << ' '
        << profile.noGlobalTextId << ' ' << profile.partyFullNpcTextId
        << "\nfresh_rosters " << profile.freshRosterIds.size();
    for (uint32_t rosterId : profile.freshRosterIds)
    {
        out << ' ' << rosterId;
    }
    out << "\nglobal_text " << profile.chineseText << ' ' << profile.globalTextSha256
        << "\nyes " << std::quoted(profile.yesText) << "\nno " << std::quoted(profile.noText) << "\nend\n";
    return out.str();
}

std::optional<BreachMercenaryProfile> loadBreachMercenaryProfileText(const std::string &text, std::string &error)
{
    error.clear();
    if (text.empty() || text.size() > 64 * 1024)
    {
        reject(error, "Invalid Breach mercenary profile size");
        return std::nullopt;
    }
    try
    {
        ProfileReader reader(text);
        BreachMercenaryProfile profile;
        reader.tag("breach_mercenaries");
        profile.version = reader.number<uint32_t>();
        reader.tag("map");
        profile.canonicalMapKey = reader.token();
        reader.tag("sources");
        reader.expectedNumber(uint32_t(profile.sourceSha256.size()));
        for (std::string &hash : profile.sourceSha256)
        {
            reader.tag("source");
            hash = reader.token();
        }
        reader.tag("roster");
        profile.minimumSourceRosterId = reader.number<uint32_t>();
        profile.maximumSourceRosterId = reader.number<uint32_t>();
        profile.npcRosterOffset = reader.number<uint32_t>();
        reader.tag("party");
        profile.partyCapacity = reader.number<uint32_t>();
        reader.tag("spots");
        reader.expectedNumber(uint32_t(profile.spots.size()));
        for (size_t index = 0; index < profile.spots.size(); ++index)
        {
            reader.tag("spot");
            reader.expectedNumber(uint32_t(index + 1));
            BreachMercenarySpot &spot = profile.spots[index];
            spot.x = reader.number<int32_t>();
            spot.y = reader.number<int32_t>();
            spot.z = reader.number<int32_t>();
        }
        reader.tag("faces");
        reader.expectedNumber(uint32_t(profile.monsterIdByFace.size()));
        for (size_t face = 0; face < profile.monsterIdByFace.size(); ++face)
        {
            reader.tag("face");
            reader.expectedNumber(uint32_t(face));
            profile.monsterIdByFace[face] = reader.number<uint32_t>();
        }
        reader.tag("fallback");
        profile.fallbackMonsterId = reader.number<uint32_t>();
        reader.tag("actor");
        profile.actor.initiallyHostile = reader.flag();
        profile.actor.ally = reader.number<uint32_t>();
        profile.actor.guardRadius = reader.number<uint32_t>();
        profile.actor.gold = reader.number<uint32_t>();
        profile.actor.item = reader.number<uint32_t>();
        reader.tag("topics");
        profile.joinTopicId = reader.number<uint32_t>();
        profile.yesGlobalTextId = reader.number<uint32_t>();
        profile.noGlobalTextId = reader.number<uint32_t>();
        profile.partyFullNpcTextId = reader.number<uint32_t>();
        reader.tag("fresh_rosters");
        reader.expectedNumber(uint32_t(profile.freshRosterIds.size()));
        for (uint32_t &rosterId : profile.freshRosterIds)
        {
            rosterId = reader.number<uint32_t>();
        }
        reader.tag("global_text");
        profile.chineseText = reader.flag();
        profile.globalTextSha256 = reader.token();
        reader.tag("yes");
        profile.yesText = reader.quotedText();
        reader.tag("no");
        profile.noText = reader.quotedText();
        reader.tag("end");
        reader.finish();
        if (!validBreachMercenaryProfile(profile, error))
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

std::optional<uint32_t> breachMercenaryNpcId(const BreachMercenaryProfile &profile, uint32_t sourceRosterId)
{
    std::string error;
    if (!validBreachMercenaryProfile(profile, error)
        || sourceRosterId < profile.minimumSourceRosterId || sourceRosterId > profile.maximumSourceRosterId)
    {
        return std::nullopt;
    }
    return sourceRosterId + profile.npcRosterOffset;
}

std::optional<uint32_t> breachMercenaryMonsterIdForFace(
    const BreachMercenaryProfile &profile, uint32_t originalFace)
{
    std::string error;
    if (!validBreachMercenaryProfile(profile, error))
    {
        return std::nullopt;
    }
    if (originalFace < profile.monsterIdByFace.size() && profile.monsterIdByFace[originalFace] != 0)
    {
        return profile.monsterIdByFace[originalFace];
    }
    return profile.fallbackMonsterId;
}
}
