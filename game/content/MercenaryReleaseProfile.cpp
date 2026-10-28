#include "MercenaryReleaseProfile.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
bool reject(std::string &error, const char *pMessage)
{
    error = pMessage;
    return false;
}

class ReleaseProfileReader
{
public:
    explicit ReleaseProfileReader(const std::string &text) : m_stream(text)
    {
    }

    std::string token()
    {
        std::string value;
        if (!(m_stream >> value))
        {
            throw std::runtime_error("Truncated mercenary release profile");
        }
        return value;
    }

    void tag(std::string_view expected)
    {
        if (token() != expected)
        {
            throw std::runtime_error("Unexpected mercenary release profile field");
        }
    }

    uint32_t number()
    {
        const std::string text = token();
        uint32_t value = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
        {
            throw std::runtime_error("Invalid mercenary release profile number");
        }
        return value;
    }

    std::string quotedText()
    {
        m_stream >> std::ws;
        std::string value;
        if (m_stream.peek() != '"' || !(m_stream >> std::quoted(value)))
        {
            throw std::runtime_error("Invalid mercenary release confirmation text");
        }
        return value;
    }

    void finish()
    {
        m_stream >> std::ws;
        if (m_stream.peek() != std::char_traits<char>::eof())
        {
            throw std::runtime_error("Trailing mercenary release profile fields");
        }
    }

private:
    std::istringstream m_stream;
};
}

bool validMercenaryReleaseProfile(const MercenaryReleaseProfile &profile, std::string &error)
{
    error.clear();
    if (profile.version != MercenaryReleaseContentVersion || profile.sourceSha256 != MercenaryReleaseSourceSha256
        || profile.minimumSourceRosterId != 0 || profile.maximumSourceRosterId != 49
        || profile.npcRosterOffset != 261 || profile.rosterQuestBitOffset != 400
        || profile.confirmationWindowMilliseconds != 2000 || profile.confirmationGlobalTextId != 738
        || profile.confirmationSoundId != 205 || profile.rejectionSoundId != 27)
    {
        return reject(error, "Unsupported mercenary release source contract");
    }
    const std::array<MercenaryReleaseProtection, 3> expectedProtections{{
        {0, {}}, {34, {19, 20}}, {4, {59}},
    }};
    if (profile.protections != expectedProtections)
    {
        return reject(error, "Unsupported mercenary release quest protection");
    }
    const std::string_view expectedHash = profile.chineseText
        ? MercenaryReleaseChineseGlobalTextSha256 : MercenaryReleaseEnglishGlobalTextSha256;
    const std::string_view expectedText = profile.chineseText
        ? "再点一次“遣散”按钮即可遣散该角色。"
        : "Click the Dismiss button again to Dismiss this character...";
    if (profile.globalTextSha256 != expectedHash || profile.confirmationText != expectedText)
    {
        return reject(error, "Unsupported mercenary release source confirmation text");
    }
    return true;
}

std::string serializeMercenaryReleaseProfile(const MercenaryReleaseProfile &profile)
{
    std::string error;
    if (!validMercenaryReleaseProfile(profile, error))
    {
        throw std::invalid_argument(error);
    }
    std::ostringstream output;
    output << "mercenary_release " << profile.version << "\nsource " << profile.sourceSha256
        << "\nroster " << profile.minimumSourceRosterId << ' ' << profile.maximumSourceRosterId
        << "\naliases " << profile.npcRosterOffset << ' ' << profile.rosterQuestBitOffset
        << "\nconfirmation " << profile.confirmationWindowMilliseconds << ' ' << profile.confirmationGlobalTextId
        << ' ' << profile.confirmationSoundId << ' ' << profile.rejectionSoundId
        << "\nprotections " << profile.protections.size() << '\n';
    for (const MercenaryReleaseProtection &protection : profile.protections)
    {
        output << "protection " << protection.sourceRosterId << ' ' << protection.unlockQuestBits.size();
        for (uint32_t questBit : protection.unlockQuestBits)
        {
            output << ' ' << questBit;
        }
        output << '\n';
    }
    output << "global_text " << profile.chineseText << ' ' << profile.globalTextSha256
        << "\ntext " << std::quoted(profile.confirmationText) << "\nend\n";
    return output.str();
}

std::optional<MercenaryReleaseProfile> loadMercenaryReleaseProfileText(
    const std::string &text, std::string &error)
{
    error.clear();
    if (text.empty() || text.size() > 16 * 1024)
    {
        reject(error, "Invalid mercenary release profile size");
        return std::nullopt;
    }
    try
    {
        ReleaseProfileReader reader(text);
        MercenaryReleaseProfile profile;
        reader.tag("mercenary_release");
        profile.version = reader.number();
        reader.tag("source");
        profile.sourceSha256 = reader.token();
        reader.tag("roster");
        profile.minimumSourceRosterId = reader.number();
        profile.maximumSourceRosterId = reader.number();
        reader.tag("aliases");
        profile.npcRosterOffset = reader.number();
        profile.rosterQuestBitOffset = reader.number();
        reader.tag("confirmation");
        profile.confirmationWindowMilliseconds = reader.number();
        profile.confirmationGlobalTextId = reader.number();
        profile.confirmationSoundId = reader.number();
        profile.rejectionSoundId = reader.number();
        reader.tag("protections");
        if (reader.number() != profile.protections.size())
        {
            throw std::runtime_error("Invalid mercenary release protection count");
        }
        for (MercenaryReleaseProtection &protection : profile.protections)
        {
            reader.tag("protection");
            protection.sourceRosterId = reader.number();
            const uint32_t count = reader.number();
            if (count > 2)
            {
                throw std::runtime_error("Invalid mercenary release unlock bit count");
            }
            for (uint32_t index = 0; index < count; ++index)
            {
                protection.unlockQuestBits.push_back(reader.number());
            }
        }
        reader.tag("global_text");
        const uint32_t chineseText = reader.number();
        if (chineseText > 1)
        {
            throw std::runtime_error("Invalid mercenary release language flag");
        }
        profile.chineseText = chineseText != 0;
        profile.globalTextSha256 = reader.token();
        reader.tag("text");
        profile.confirmationText = reader.quotedText();
        reader.tag("end");
        reader.finish();
        if (!validMercenaryReleaseProfile(profile, error))
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

bool sourceMercenaryReleaseProtected(const MercenaryReleaseProfile &profile, uint32_t sourceRosterId,
    const std::unordered_set<uint32_t> &questBits)
{
    std::string error;
    if (!validMercenaryReleaseProfile(profile, error)
        || sourceRosterId < profile.minimumSourceRosterId || sourceRosterId > profile.maximumSourceRosterId)
    {
        return true;
    }
    for (const MercenaryReleaseProtection &protection : profile.protections)
    {
        if (protection.sourceRosterId == sourceRosterId)
        {
            return std::none_of(protection.unlockQuestBits.begin(), protection.unlockQuestBits.end(),
                [&](uint32_t questBit) { return questBits.contains(questBit); });
        }
    }
    return false;
}
}
