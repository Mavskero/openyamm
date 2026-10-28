#include "SkeletonTransformerResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumScriptBytes = 256 * 1024;
constexpr std::string_view RelativePath = "Scripts/Maps/d19.lua";
constexpr std::string_view ExpectedSha256 =
    "d6cb8ff49cbdf5af931eb693ca163d4d94341179d1cb8669ff0a8a9bc78a4034";

constexpr std::string_view Overlay = R"OY_SKELETON(
-- OpenYAMM translation of the audited MMerge Necromancers' Guild entrance and transformer gates only.
ReplaceMapEvent(15, entranceHint, function()
    if IsQBitSet(QBit(20)) or IsQBitSet(QBit(19)) or HasPlayer(34) then
        evt.SetDoorState(5, DoorAction.Open)
    elseif IsInvisible() then
        evt.FaceAnimation(18)
    else
        evt.SetNPCGreeting(45, 107)
        evt.SpeakNPC(45)
    end
end, entranceHint)

ReplaceMapEvent(131, nil, function()
    -- Source Map.Vars[19] uses MM8's 0x7D variable base; the current exporter uses 0x7B,
    -- so the existing d19 puzzle writes MapVar(21). Preserve that layout without moving saved bytes.
    -- Cmp is >= for map variables: the two bounds below preserve the source's exact == 15 condition.
    if IsQBitSet(QBit(27)) or not IsQBitSet(QBit(26))
        or not IsAtLeast(MapVar(21), 15) or IsAtLeast(MapVar(21), 16) then
        return
    end
    SetQBit(QBit(27))
    evt.ShowMovie("skeltrans", true)
    evt.SetFacetBit(30, FacetBits.Untouchable, true)
    evt.SetFacetBit(30, FacetBits.Invisible, true)
end)
)OY_SKELETON";

std::string luaStringLiteral(std::string_view text)
{
    std::string literal = "\"";
    for (unsigned char character : text)
    {
        if (character == '\\' || character == '"')
        {
            literal += '\\';
            literal += character;
        }
        else if (character < 32 || character == 127)
        {
            literal += '\\';
            literal += char('0' + character / 100);
            literal += char('0' + (character / 10) % 10);
            literal += char('0' + character % 10);
        }
        else
        {
            literal += character;
        }
    }
    literal += '"';
    return literal;
}
}

std::optional<SkeletonTransformerProfile> loadSkeletonTransformerProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path candidate = root / RelativePath;
    if (!std::filesystem::exists(candidate))
    {
        return std::nullopt;
    }

    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Skeleton transformer source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid skeleton transformer source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read skeleton transformer source script");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != ExpectedSha256)
    {
        throw std::runtime_error("Unsupported Scripts/Maps/d19.lua fingerprint; "
            "review this Merge version before enabling the skeleton transformer overlay");
    }

    return SkeletonTransformerProfile{"d19", sourceSha256};
}

std::string makeSkeletonTransformerOverlay(std::string_view entranceHint)
{
    if (entranceHint.empty())
    {
        throw std::runtime_error("Missing skeleton transformer entrance source text");
    }
    return "local entranceHint = " + luaStringLiteral(entranceHint) + "\n" + std::string(Overlay);
}
}
