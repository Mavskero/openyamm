#include "BreachMercenaryResources.h"

#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
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

std::filesystem::path checkedPath(const std::filesystem::path &root, std::string_view name)
{
    const std::filesystem::path candidate = root / name;
    require(std::filesystem::exists(candidate), "Incomplete Breach mercenary source: " + std::string(name));
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path), "Breach mercenary source escapes source root");
    return path;
}

std::string sha256(const void *pBytes, size_t size)
{
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid Breach mercenary fingerprint payload size");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(pBytes, CC_LONG(size), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return digest.str();
}

void verifyScript(const std::filesystem::path &root, size_t index)
{
    const std::filesystem::path path = checkedPath(root, BreachMercenarySourcePaths[index]);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 256 * 1024, "Invalid Breach mercenary script size");
    std::string text(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(text.data(), std::streamsize(size))), "Cannot read Breach mercenary script");
    require(sha256(text.data(), text.size()) == Game::BreachMercenarySourceHashes[index],
        "Unsupported Breach mercenary source fingerprint: " + std::string(BreachMercenarySourcePaths[index]));
}

uint32_t textId(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    require(!text.empty(), "Missing Breach mercenary global text ID");
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(),
        "Invalid Breach mercenary global text ID");
    return value;
}

void loadJoinLabels(const std::filesystem::path &root, Game::BreachMercenaryProfile &profile)
{
    const std::string_view archive = profile.chineseText
        ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod";
    const std::vector<uint8_t> bytes = readLodText(checkedPath(root, archive), "global.txt");
    profile.globalTextSha256 = sha256(bytes.data(), bytes.size());
    size_t restoredGlyphs = 0;
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(
        decodeLegacyText(bytes, profile.chineseText, restoredGlyphs));
    require(table && table->getRowCount() > 2
        && table->getRow(0) == std::vector<std::string>{"Global Text", ""},
        "Unsupported Breach mercenary global text header");
    std::set<uint32_t> ids;
    for (size_t index = 1; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (row.empty() || row == std::vector<std::string>{""})
        {
            continue;
        }
        require(row.size() >= 2, "Incomplete Breach mercenary global text row");
        const uint32_t id = textId(row[0]);
        require(ids.insert(id).second, "Duplicate Breach mercenary global text ID");
        if (id == profile.yesGlobalTextId)
        {
            profile.yesText = row[1];
        }
        else if (id == profile.noGlobalTextId)
        {
            profile.noText = row[1];
        }
    }
    require(ids.contains(profile.yesGlobalTextId) && ids.contains(profile.noGlobalTextId),
        "Missing Breach mercenary Yes/No source text");
}
}

std::optional<BreachMercenaryResources> loadBreachMercenaryResources(
    const std::filesystem::path &source, bool chineseText)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "Breach mercenary source root is not a directory");
    const bool anyScript = std::any_of(BreachMercenarySourcePaths.begin(), BreachMercenarySourcePaths.end(),
        [&](std::string_view name)
        {
            return std::filesystem::exists(root / name) || std::filesystem::is_symlink(root / name);
        });
    if (!anyScript)
    {
        return std::nullopt;
    }
    BreachMercenaryResources resources;
    Game::BreachMercenaryProfile &profile = resources.profile;
    for (size_t index = 0; index < BreachMercenarySourcePaths.size(); ++index)
    {
        verifyScript(root, index);
        profile.sourceSha256[index] = Game::BreachMercenarySourceHashes[index];
    }
    profile.version = Game::BreachMercenaryContentVersion;
    profile.canonicalMapKey = Game::BreachMercenaryMap;
    profile.minimumSourceRosterId = 0;
    profile.maximumSourceRosterId = 49;
    profile.npcRosterOffset = 261;
    profile.partyCapacity = 5;
    // Breach.lua 365–377: preserve the source's one-based slot order.
    profile.spots = {{
        {-1406, 292, 3127}, {408, 1311, 3127}, {453, -1478, 3265}, {-125, -792, 3168},
        {-2307, -1447, 921}, {-466, -555, 887}, {1254, -314, 896}, {-1142, -1030, 2046},
        {-2, 1119, 2508}, {1345, 766, 2054}, {1202, -1065, 2048},
    }};
    // Breach.lua 379–445: Face is zero-based; Lua false remains zero until fallback resolution.
    profile.monsterIdByFace = {{
        194, 255, 44, 357, 368, 353, 372, 377, 55, 307, 293, 418, 416,
        307, 54, 307, 27, 518, 20, 24, 301, 301, 58, 59, 223, 225, 290, 290,
        0, 0, 0, 517, 343, 249, 637, 405, 419, 459, 335, 459, 336, 459, 337,
        459, 384, 459, 459, 459, 459, 261, 420, 405, 419, 261, 519, 271,
        436, 271, 437, 427, 427, 231, 231, 320, 319,
    }};
    profile.fallbackMonsterId = 459;
    profile.actor = {false, 9999, 5000, 0, 0};
    // NPCMercenaries.lua 43–76 and 103–149: the owned-member Join branch has no recruitment biography.
    profile.joinTopicId = 100;
    profile.yesGlobalTextId = 704;
    profile.noGlobalTextId = 705;
    profile.partyFullNpcTextId = 1681;
    // MenuChooseCharacter.lua 459–481: PrepareChar(0, 0) precedes RosterIdSpots entries two through five.
    profile.freshRosterIds = {0, 38, 39, 40, 41};
    profile.chineseText = chineseText;
    loadJoinLabels(root, profile);
    std::string error;
    if (!Game::validBreachMercenaryProfile(profile, error))
    {
        throw std::runtime_error("Invalid Breach mercenary profile: " + error);
    }
    return resources;
}
}
