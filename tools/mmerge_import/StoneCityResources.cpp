#include "StoneCityResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/7d24.lua";
constexpr std::string_view ExpectedSha256 =
    "d2e0d1d8f694b0bc04bf37efc8e948d785918bb4605e9e3f18da3c6d92892fda";

constexpr std::string_view Overlay = R"OY_STONE_CITY(
-- The source party quest survives replacement of the characters who received award 3.
ReplaceMapEvent(416, entranceHint, function()
    if IsQBitSet(QBit(647)) then
        evt.EnterHouse(216)
    else
        evt.StatusText(lockedText)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end, entranceHint)
)OY_STONE_CITY";

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

std::optional<StoneCityProfile> loadStoneCityProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Stone City source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Stone City source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Stone City source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/7d24.lua fingerprint; "
            "review this Merge version before enabling the Stone City entrance overlay");
    }

    return StoneCityProfile{"7d24", sourceSha256};
}

std::string makeStoneCityOverlay(std::string_view entranceHint, std::string_view lockedText)
{
    if (entranceHint.empty() || lockedText.empty())
    {
        throw std::runtime_error("Missing Stone City entrance or locked-door source text");
    }
    return "local entranceHint = " + luaStringLiteral(entranceHint)
        + "\nlocal lockedText = " + luaStringLiteral(lockedText) + "\n" + std::string(Overlay);
}
}
