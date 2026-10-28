#include "MercenaryReleaseResources.h"

#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::filesystem::path checkedSourcePath(const std::filesystem::path &root, std::string_view name)
{
    const std::filesystem::path candidate = root / name;
    require(std::filesystem::exists(candidate), "Missing mercenary release source: " + std::string(name));
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path), "Mercenary release source escapes source root");
    return path;
}

std::string sha256(const void *pBytes, size_t size)
{
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid mercenary release fingerprint payload size");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(pBytes, CC_LONG(size), hash.data());
    std::ostringstream output;
    for (unsigned char value : hash)
    {
        output << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return output.str();
}

void authenticateScript(const std::filesystem::path &root)
{
    const std::filesystem::path path = checkedSourcePath(root, MercenaryReleaseSourcePath);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 256 * 1024, "Invalid mercenary release script size");
    std::string text(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(text.data(), std::streamsize(size))), "Cannot read mercenary release script");
    require(sha256(text.data(), text.size()) == Game::MercenaryReleaseSourceSha256,
        "Unsupported mercenary release source fingerprint");
}

uint32_t globalTextId(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    require(!text.empty(), "Missing mercenary release global text ID");
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(),
        "Invalid mercenary release global text ID");
    return value;
}

void loadConfirmationText(const std::filesystem::path &root, Game::MercenaryReleaseProfile &profile)
{
    const std::string_view archive = profile.chineseText
        ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod";
    const std::vector<uint8_t> bytes = readLodText(checkedSourcePath(root, archive), "global.txt");
    profile.globalTextSha256 = sha256(bytes.data(), bytes.size());
    const std::string_view expectedHash = profile.chineseText
        ? Game::MercenaryReleaseChineseGlobalTextSha256 : Game::MercenaryReleaseEnglishGlobalTextSha256;
    require(profile.globalTextSha256 == expectedHash, "Unsupported mercenary release global text fingerprint");
    size_t restoredGlyphs = 0;
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(
        decodeLegacyText(bytes, profile.chineseText, restoredGlyphs));
    require(table && table->getRowCount() > 2
        && table->getRow(0) == std::vector<std::string>{"Global Text", ""},
        "Unsupported mercenary release global text header");
    std::set<uint32_t> ids;
    for (size_t index = 1; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (row.empty() || row == std::vector<std::string>{""})
        {
            continue;
        }
        require(row.size() >= 2, "Incomplete mercenary release global text row");
        const uint32_t id = globalTextId(row[0]);
        require(ids.insert(id).second, "Duplicate mercenary release global text ID");
        if (id == profile.confirmationGlobalTextId)
        {
            profile.confirmationText = row[1];
        }
    }
    require(ids.contains(profile.confirmationGlobalTextId), "Missing mercenary release confirmation source text");
}
}

std::optional<MercenaryReleaseResources> loadMercenaryReleaseResources(
    const std::filesystem::path &source, bool chineseText)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "Mercenary release source root is not a directory");
    const std::filesystem::path script = root / MercenaryReleaseSourcePath;
    if (!std::filesystem::exists(script) && !std::filesystem::is_symlink(script))
    {
        return std::nullopt;
    }
    authenticateScript(root);
    MercenaryReleaseResources resources;
    Game::MercenaryReleaseProfile &profile = resources.profile;
    profile.sourceSha256 = Game::MercenaryReleaseSourceSha256;
    profile.minimumSourceRosterId = 0;
    profile.maximumSourceRosterId = 49;
    profile.npcRosterOffset = 261;
    profile.rosterQuestBitOffset = 400;
    // NPCMercenaries.lua 764–806: any unlock bit removes its roster's conditional protection.
    profile.protections = {{{0, {}}, {34, {19, 20}}, {4, {59}}}};
    profile.confirmationWindowMilliseconds = 2000;
    profile.confirmationGlobalTextId = 738;
    profile.confirmationSoundId = 205;
    profile.rejectionSoundId = 27;
    profile.chineseText = chineseText;
    loadConfirmationText(root, profile);
    std::string error;
    if (!Game::validMercenaryReleaseProfile(profile, error))
    {
        throw std::runtime_error("Invalid mercenary release profile: " + error);
    }
    return resources;
}
}
