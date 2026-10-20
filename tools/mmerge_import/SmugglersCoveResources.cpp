#include "SmugglersCoveResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumScriptBytes = 256 * 1024;
constexpr std::string_view RelativePath = "Scripts/Maps/d07.lua";
constexpr std::string_view ExpectedSha256 =
    "a2a6d4166098658ebe87d1923739f5b2f2d96a4fc932c7737aec06e080334a26";

constexpr std::string_view Overlay = R"OY_SMUGGLERS(
-- Audited MMerge d07.lua: entry hostility/visibility and persistent kill anger only.
RemoveMapEvent(1)
RegisterMapOnLoadEvent(900, nil, function()
    local elapsed = evt.GameMinutesSinceLastVisit()
    if elapsed ~= nil and elapsed > 1440 then
        evt.SetMapVar("WereratsMad", 0)
    end

    local peaceful = IsQBitSet(QBit(10)) and evt.GetMapVar("WereratsMad") == 0
    for _, group in ipairs({8, 10, 11}) do
        evt.SetMonGroupBit(group, MonsterBits.Hostile, peaceful and 0 or 1)
    end
    evt.SetMonGroupBit(8, MonsterBits.Invisible, peaceful and 1 or 0)
    evt.SetMonGroupBit(11, MonsterBits.Invisible, peaceful and 0 or 1)
    if not peaceful then
        -- MM8 MapVar9 has raw EVT selector 0x7d + 9. Share its existing exported EVT slot.
        SetValue(0x0086, 2)
    end
end)

RegisterMonsterKilledHook(901, nil, function()
    evt.SetMapVar("WereratsMad", 1)
end)
)OY_SMUGGLERS";

bool pathEscapesRoot(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const std::filesystem::path relative = path.lexically_relative(root);
    return relative.empty() || relative.is_absolute() || *relative.begin() == "..";
}
}

std::optional<SmugglersCoveProfile> loadSmugglersCoveProfile(
    const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path candidate = root / RelativePath;
    if (!std::filesystem::exists(candidate))
    {
        return std::nullopt;
    }

    const std::filesystem::path path = std::filesystem::canonical(candidate);
    if (pathEscapesRoot(path, root) || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Smugglers Cove source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Smugglers Cove source script size");
    }

    std::string script(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Smugglers Cove source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/d07.lua fingerprint; "
            "review this Merge version before enabling the Smugglers Cove monster-state overlay");
    }

    return SmugglersCoveProfile{"d07", sourceSha256, std::string(Overlay)};
}
}
