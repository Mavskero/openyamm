#include "ControlCubeRefillResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/mdt15.lua";
constexpr std::string_view ExpectedSha256 =
    "4a6b0298e0d8c754897464042fd0b14809b42e1f02fd630c7a38d261dd80f739";
constexpr uint16_t RefillHookEventId = 65473;
constexpr uint32_t RefillIntervalDays = 672;

std::string makeOverlay()
{
    return "\n-- Source AfterLoadMap / Map.Refilled; the map registry owns the 672-day refill interval.\n"
        "RegisterMapRefillHook(" + std::to_string(RefillHookEventId) + R"OY_CUBE_REFILL(, nil, function()
    local function resetItems(actorId)
        evt.SetMonsterItem(actorId, 0, false)
        evt.SetMonsterItem(actorId, 1477, true)
        evt.SetMonsterItem(actorId, 866, true)
    end
    local count = evt.GetMapActorCount()
    if count >= 1 then
        resetItems(0)
    end
    if count >= 2 then
        resetItems(1)
    end
end)
)OY_CUBE_REFILL";
}
}

std::optional<ControlCubeRefillProfile> loadControlCubeRefillProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Control cube refill source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid control cube refill source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read control cube refill source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/mdt15.lua fingerprint; "
            "review this Merge version before enabling the control cube refill overlay");
    }

    return ControlCubeRefillProfile{"mdt15", sourceSha256, RefillHookEventId, RefillIntervalDays, makeOverlay()};
}
}
