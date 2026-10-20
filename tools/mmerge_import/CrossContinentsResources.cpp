#include "CrossContinentsResources.h"
#include "CrossContinentsIntroSource.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace OpenYAMM::Import
{
namespace
{
std::filesystem::path checkedSourcePath(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
    {
        throw std::runtime_error("Cross-continent input escapes game source root");
    }
    return path;
}

std::vector<std::string> sourceRow(const std::filesystem::path &root, bool chinese,
    const std::string &member, uint32_t id, size_t columns)
{
    const std::filesystem::path archive = checkedSourcePath(root,
        root / "Data" / (chinese ? "10 LocZHCN.EnglishT.lod" : "mmmerge.T.lod"));
    size_t restoredGlyphs = 0;
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(
        decodeLegacyText(readLodText(archive, member), chinese, restoredGlyphs));
    if (!table)
    {
        throw std::runtime_error("Invalid cross-continent source table: " + member);
    }
    const std::string key = std::to_string(id);
    std::optional<std::vector<std::string>> result;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (!row.empty() && row[0] == key)
        {
            if (result || row.size() != columns || row[1].empty())
            {
                throw std::runtime_error("Invalid cross-continent source row: " + member + ":" + key);
            }
            result = row;
        }
    }
    if (!result)
    {
        throw std::runtime_error("Missing cross-continent source row: " + member + ":" + key);
    }
    return *result;
}

std::string luaText(const std::string &text)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char byte : text)
    {
        if (byte == '"' || byte == '\\')
        {
            out << '\\' << char(byte);
        }
        else if (byte < 32 || byte == 127)
        {
            out << '\\' << std::setw(3) << std::setfill('0') << uint32_t(byte);
        }
        else
        {
            out << char(byte);
        }
    }
    out << '"';
    return out.str();
}
}

std::optional<CrossContinentsIntroProfile> loadCrossContinentsIntroProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path candidate = root / "Scripts/Global/Quest_CrossContinents.lua";
    if (!std::filesystem::exists(candidate))
    {
        return std::nullopt;
    }
    const std::filesystem::path path = checkedSourcePath(root, candidate);
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > 256 * 1024)
    {
        throw std::runtime_error("Invalid cross-continent script size");
    }
    std::string script(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read cross-continent script");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    if (digest.str() != "0790af9b1b387b7db901f0b568593df725516e823a86fa0edb2d1fde459bce49")
    {
        throw std::runtime_error("Unsupported Quest_CrossContinents.lua fingerprint; "
            "review this Merge version before enabling the quest overlay");
    }
    const std::vector<std::string> map = sourceRow(root, false, "mapstats.txt", 204, 34);
    const std::vector<std::string> message = sourceRow(root, false, "npctopic.txt", 186, 7);
    if (map[2] != "Breach.odm")
    {
        throw std::runtime_error("Unexpected cross-continent Breach map identity");
    }
    CrossContinentsBreachTravelTopic topic;
    topic.topic = map[1];
    topic.baseTopic = map[1];
    topic.message = message[1];
    if (std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod"))
    {
        const std::vector<std::string> translatedMap = sourceRow(root, true, "mapstats.txt", 204, 34);
        if (translatedMap[2] != map[2])
        {
            throw std::runtime_error("Cross-continent Breach map translation changes its identity");
        }
        topic.topic = translatedMap[1];
        topic.message = sourceRow(root, true, "npctopic.txt", 186, 7)[1];
    }
    std::ostringstream lua;
    lua << "-- Source Quest_CrossContinents.lua447-463: MapStats204.Name / NPCTopic186.\n"
        << "local breachTravel = {topicId = " << topic.id << ", mapName = " << luaText(map[2])
        << ", message = " << luaText(topic.message) << "}\n" << CrossContinentsIntroSource;
    return CrossContinentsIntroProfile{digest.str(), lua.str(), std::move(topic)};
}
}
