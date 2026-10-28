#include "BlasterLearningResources.h"

#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
constexpr std::string_view SourcePath = "Scripts/Global/PromotionTopics.lua";
constexpr std::string_view SourceSha256 =
    "6266dd3fc45736d60901f0e3c4f98e7b4dcb0c2f4bfca6dd816dabc371d94014";
constexpr std::string_view BaseArchive = "Data/mmmerge.T.lod";
constexpr std::string_view ChineseArchive = "Data/10 LocZHCN.EnglishT.lod";

constexpr std::string_view Overlay = R"OY_BLASTER(
-- Audited PromotionTopics.lua 4-14 only: retain learned skills and check the source class cap, without race gates.
ReplaceGlobalEvent(950, nil, function()
    for _, player in ipairs(PartyMembers()) do
        evt.ForPlayer(player)
        if not IsAtLeast(BlasterSkill, 1)
            and evt.CanClassLearnSkill(evt.GetPlayerClass(player), "Blaster") then
            SetValue(BlasterSkill, 1)
        end
    end
end)
)OY_BLASTER";

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::filesystem::path checkedPath(const std::filesystem::path &root, std::string_view relativePath)
{
    const std::filesystem::path candidate = root / relativePath;
    require(std::filesystem::exists(candidate), "Missing Blaster learning source: " + std::string(relativePath));
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path), "Blaster learning source escapes the game source root");
    return path;
}

std::string sha256(const void *pBytes, size_t size)
{
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid Blaster learning fingerprint payload size");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(pBytes, static_cast<CC_LONG>(size), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return digest.str();
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
    uint32_t value = 0;
    require(!text.empty(), "Missing Blaster learning global text ID");
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(),
        "Invalid Blaster learning global text ID");
    return value;
}

struct SourceLabel
{
    std::string text;
    std::string hash;
};

SourceLabel loadLabel(const std::filesystem::path &root, bool chineseText)
{
    const std::filesystem::path archive = checkedPath(root, chineseText ? ChineseArchive : BaseArchive);
    const std::vector<uint8_t> bytes = readLodText(archive, "global.txt");
    SourceLabel label;
    label.hash = sha256(bytes.data(), bytes.size());
    size_t restoredGlyphs = 0;
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(
        decodeLegacyText(bytes, chineseText, restoredGlyphs));
    require(table && table->getRowCount() > 2
        && table->getRow(0) == std::vector<std::string>{"Global Text", ""},
        "Unsupported Blaster learning global text header");
    std::set<uint32_t> ids;
    for (size_t index = 1; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (row.empty() || row == std::vector<std::string>{""})
        {
            continue;
        }
        require(row.size() >= 2, "Incomplete Blaster learning global text row");
        const uint32_t id = textId(row[0]);
        require(ids.insert(id).second, "Duplicate Blaster learning global text ID");
        if (id == 278)
        {
            require(row.size() == 2 && row[1].find_first_not_of(' ') != std::string::npos
                && std::none_of(row[1].begin(), row[1].end(), [](unsigned char character)
                    { return character < 32 || character == 127; }),
                "Invalid Blaster learning topic label in global.txt:278");
            label.text = row[1];
        }
    }
    require(ids.contains(278), "Missing Blaster learning topic label in global.txt:278");
    return label;
}
}

std::optional<BlasterLearningProfile> loadBlasterLearningProfile(
    const std::filesystem::path &source, bool chineseText)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "Blaster learning source root is not a directory");
    if (!std::filesystem::exists(root / SourcePath))
    {
        return std::nullopt;
    }
    const std::filesystem::path path = checkedPath(root, SourcePath);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 256 * 1024, "Invalid Blaster learning source script size");
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(script.data(), static_cast<std::streamsize>(size))),
        "Cannot read Blaster learning source script");
    const std::string sourceSha256 = sha256(script.data(), script.size());
    require(sourceSha256 == SourceSha256,
        "Unsupported PromotionTopics.lua fingerprint; review this Merge version before enabling topic 950");

    const SourceLabel baseLabel = loadLabel(root, false);
    const SourceLabel label = chineseText ? loadLabel(root, true) : baseLabel;
    return BlasterLearningProfile{sourceSha256, std::string(Overlay), label.text, baseLabel.text,
        label.hash, baseLabel.hash};
}
}
