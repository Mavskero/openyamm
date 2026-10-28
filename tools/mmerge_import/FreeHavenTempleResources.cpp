#include "FreeHavenTempleResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/outc2.lua";
constexpr std::string_view ExpectedSha256 =
    "8551ac3062309de282c22470aabd4e27ea151d761a7985fda65b2cb119007b7f";

constexpr std::string_view Overlay = R"OY_FREE_HAVEN(
-- Translate the source follower transaction; hiring, fees and custody remain native operations.
ReplaceMapEvent(19, templeHint, function()
    if IsQBitSet(QBit(1131)) then
        evt.ForPlayer(Players.All)
        if HasItem(2054) then
            RemoveItem(2054)
            ClearQBit(QBit(1212))
            SetQBit(QBit(1132))
            evt.SimpleMessage(chaliceText)
        else
            evt.EnterHouse(326)
        end
    elseif IsQBitSet(QBit(1130)) then
        if IsQBitSet(QBit(1129)) then
            evt.EnterHouse(1442)
        else
            evt.EnterHouse(326)
        end
    elseif HasFollowerProfession(63) and HasFollowerProfession(64) then
        evt.SimpleMessage(repairedText)
        SetQBit(QBit(1130))
        RemoveFollowerProfession(63)
        RemoveFollowerProfession(64)
    else
        evt.EnterHouse(1442)
    end
end, templeHint)

RegisterEvent(140, innHint, function()
    evt.EnterHouse(1607)
end, innHint)
)OY_FREE_HAVEN";

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

std::optional<FreeHavenTempleProfile> loadFreeHavenTempleProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Free Haven temple source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Free Haven temple source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Free Haven temple source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/outc2.lua fingerprint; "
            "review this Merge version before enabling the Free Haven temple and inn overlay");
    }

    return FreeHavenTempleProfile{"outc2", sourceSha256};
}

std::string makeFreeHavenTempleOverlay(std::string_view templeHint, std::string_view repairedText,
    std::string_view chaliceText, std::string_view innHint)
{
    if (templeHint.empty() || repairedText.empty() || chaliceText.empty() || innHint.empty())
    {
        throw std::runtime_error("Missing Free Haven temple or inn source text");
    }
    return "local templeHint = " + luaStringLiteral(templeHint)
        + "\nlocal repairedText = " + luaStringLiteral(repairedText)
        + "\nlocal chaliceText = " + luaStringLiteral(chaliceText)
        + "\nlocal innHint = " + luaStringLiteral(innHint) + "\n" + std::string(Overlay);
}
}
