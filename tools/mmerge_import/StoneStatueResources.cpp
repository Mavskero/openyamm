#include "StoneStatueResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/out07.lua";
constexpr std::string_view ExpectedSha256 =
    "3eb167089254b740051b4242d031065e7c68dc3fd882eb9dd83d9d33677549c4";

constexpr std::string_view Overlay = R"OY_STATUE(
-- The source click checks the current character's learned spell; it is not a spell cast.
for eventId = 132, 136 do
    local statueId = eventId - 112
    local mainStatue = eventId == 132
    ReplaceMapEvent(eventId, statueHint, function()
        local player = math.max(evt.GetCurrentPlayerIndex(), 0)
        local knowsSpell = PlayerKnowsSpell(player, 40)
        evt.ForPlayer(Players.All)
        if not knowsSpell and not HasItem(339) then
            return
        end
        if not knowsSpell then
            RemoveItem(339)
        end
        evt.SetSprite(statueId, 0, "0")
        if mainStatue then
            SetQBit(QBit(40))
            SetQBit(QBit(430))
        end
        evt.SpeakNPC(mainStatue and 42 or 46)
    end, statueHint)
end
)OY_STATUE";

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

std::optional<StoneStatueProfile> loadStoneStatueProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Stone statue source script escapes the game source root");
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid stone statue source script size");
    }
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read stone statue source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/out07.lua fingerprint; "
            "review this Merge version before enabling stone statue rescue");
    }
    return StoneStatueProfile{"out07", sourceSha256};
}

std::string makeStoneStatueOverlay(std::string_view statueHint)
{
    if (statueHint.empty())
    {
        throw std::runtime_error("Missing stone statue source hint");
    }
    return "local statueHint = " + luaStringLiteral(statueHint) + "\n" + std::string(Overlay);
}
}
