#include "HarmondaleCastleResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/7out02.lua";
constexpr std::string_view ExpectedSha256 =
    "95ddcdcfbfa41519d072652e02e6c94bd5365bdbe5041eadc00052ec6d718957";

constexpr std::string_view Overlay = R"OY_HARMONDALE(
-- SHA-gated translation of the source Castle Harmondale entrance and Judge Grey succession trigger.
ReplaceMapEvent(301, "进入哈蒙代尔城堡", function()
    if not IsQBitSet(QBit(519)) then
        evt.FaceAnimation(FaceAnimation.DoorLocked)
        return
    end

    if IsQBitSet(QBit(610)) then
        evt.MoveToMap(-5073, -2842, 1, 512, 0, 0, 382, 9, "7d29.blv")
        return
    end

    if IsQBitSet(QBit(644)) then
        evt.MoveToMap(-5073, -2842, 1, 512, 0, 0, 390, 9, "7d29.blv")
        return
    end

    SetQBit(QBit(644))
    SetQBit(QBit(587))
    AddValue(History(3), 0)
    evt.MoveNPC(397, 240)
    evt.SpeakNPC(397)
end, "进入哈蒙代尔城堡")

local rebuiltDateKey = "MMerge.HarmondaleRebuiltDate"
local sixMonthsTicks = 6 * 28 * 24 * 60 * 256

-- Source LoadMap records the first post-repair visit, including when an older save first enters here.
ReplaceMapEvent(211, nil, function()
    if not IsQBitSet(QBit(610)) then
        return
    end

    local now = evt.GameTimeTicks()
    -- A missing date uses now; a saved zero remains a valid date, not an uninitialized sentinel.
    local rebuiltDate = evt.GetGlobalTimeTicks(rebuiltDateKey, now)
    evt.SetGlobalTimeTicks(rebuiltDateKey, rebuiltDate)

    if IsQBitSet(QBit(646)) then
        return
    end

    if IsQBitSet(QBit(659)) or IsQBitSet(QBit(596)) or IsQBitSet(QBit(597))
        or now > rebuiltDate + sixMonthsTicks then
        evt.SpeakNPC(430)
        SetQBit(QBit(665))
        AddValue(History(6), 0)
        evt.MoveNPC(406, 0)
        evt.MoveNPC(407, 0)
        evt.MoveNPC(414, 1169)
        evt.MoveNPC(415, 1169)
        evt.MoveNPC(416, 244)
        evt.MoveNPC(417, 243)
        SetQBit(QBit(646))
    end
end)
-- ReplaceMapEvent removed the legacy OnLoad entry; retain the new handler and register it once.
RegisterMapOnLoadEvent(211, nil, nil)
)OY_HARMONDALE";

bool pathEscapesRoot(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const std::filesystem::path relative = path.lexically_relative(root);
    return relative.empty() || relative.is_absolute() || *relative.begin() == "..";
}
}

std::optional<HarmondaleCastleProfile> loadHarmondaleCastleProfile(
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
        throw std::runtime_error("Castle Harmondale source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Castle Harmondale source script size");
    }

    std::string script(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Castle Harmondale source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/7out02.lua fingerprint; "
            "review this Merge version before enabling the Castle Harmondale source overlay");
    }

    return HarmondaleCastleProfile{"7out02", sourceSha256, std::string(Overlay)};
}
}
