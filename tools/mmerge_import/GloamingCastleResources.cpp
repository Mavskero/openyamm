#include "GloamingCastleResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/d03.lua";
constexpr std::string_view ExpectedSha256 =
    "ca33bc4f3250e0ff1e85efa3ca8bbb4f362acd54d220b21ed235f023ca49f214";

constexpr std::string_view Overlay = R"OY_GLOAMING(
-- OpenYAMM translation of the audited MMerge Castle Gloaming throne-room entrance only.
ReplaceMapEvent(5, entranceHint, function()
    local yellow, red = evt.GetEnemyDetectorState()
    if IsQBitSet(QBit(611)) or not IsQBitSet(QBit(612)) or yellow or red then
        evt.StatusText(lockedText)
        return
    end
    if IsQBitSet(QBit(710)) then
        evt.EnterHouse(221)
    else
        evt.EnterHouse(219)
    end
end, entranceHint)
)OY_GLOAMING";

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

std::optional<GloamingCastleProfile> loadGloamingCastleProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Castle Gloaming source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Castle Gloaming source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Castle Gloaming source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/d03.lua fingerprint; "
            "review this Merge version before enabling the Castle Gloaming entrance overlay");
    }

    return GloamingCastleProfile{"d03", sourceSha256};
}

std::string makeGloamingCastleOverlay(std::string_view entranceHint, std::string_view lockedText)
{
    if (entranceHint.empty() || lockedText.empty())
    {
        throw std::runtime_error("Missing Castle Gloaming entrance or locked-door source text");
    }
    return "local entranceHint = " + luaStringLiteral(entranceHint)
        + "\nlocal lockedText = " + luaStringLiteral(lockedText) + "\n" + std::string(Overlay);
}
}
