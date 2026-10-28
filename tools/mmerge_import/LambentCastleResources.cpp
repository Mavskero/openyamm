#include "LambentCastleResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/7d30.lua";
constexpr std::string_view ExpectedSha256 =
    "c24c8145a68ac45534eac037fafb3d80eb53e9cb48cd5441cdaaca4174e58275";

constexpr std::string_view Overlay = R"OY_LAMBENT(
-- OpenYAMM translation of the audited MMerge Castle Lambent throne-room entrance only.
ReplaceMapEvent(416, entranceHint, function()
    local yellow, red = evt.GetEnemyDetectorState()
    if IsQBitSet(QBit(612)) or not IsQBitSet(QBit(611)) or yellow or red then
        evt.StatusText(lockedText)
        return
    end
    evt.EnterHouse(220)
end, entranceHint)
)OY_LAMBENT";

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

std::optional<LambentCastleProfile> loadLambentCastleProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Castle Lambent source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Castle Lambent source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Castle Lambent source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/7d30.lua fingerprint; "
            "review this Merge version before enabling the Castle Lambent entrance overlay");
    }

    return LambentCastleProfile{"7d30", sourceSha256};
}

std::string makeLambentCastleOverlay(std::string_view entranceHint, std::string_view lockedText)
{
    if (entranceHint.empty() || lockedText.empty())
    {
        throw std::runtime_error("Missing Castle Lambent entrance or locked-door source text");
    }
    return "local entranceHint = " + luaStringLiteral(entranceHint)
        + "\nlocal lockedText = " + luaStringLiteral(lockedText) + "\n" + std::string(Overlay);
}
}
