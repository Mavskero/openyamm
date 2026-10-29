#include "ArtifactMessengerResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cstddef>
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
constexpr std::string_view RelativePath = "Scripts/Maps/7out04.lua";
constexpr std::string_view ExpectedSha256 =
    "a3c2c6008c3cd49ee99548b1adae5af8f792e83d6cfb77adc63815a1e9311cb3";

constexpr std::string_view Overlay = R"OY_MESSENGER(
-- Source LoadMap stops the old war quest once Judge Grey has died.
ReplaceMapEvent(401, nil, function()
    if IsQBitSet(QBit(646)) then
        return
    end

    if not IsQBitSet(QBit(649)) then
        if not IsQBitSet(QBit(600)) or IsQBitSet(QBit(589)) or IsQBitSet(QBit(590)) then
            return
        end
        evt.SpeakNPC(412)
        AddValue(InventoryItem(1502), 1502)
        SetQBit(QBit(649))
        SetQBit(QBit(591))
        SetValue(MapVar(13), 0)
    end

    -- Source MMExtension MapVar11 is the existing exported MapVar13 selector.
    if not IsQBitSet(QBit(591)) or IsAtLeast(MapVar(13), 1) then
        return
    end
    SetValue(MapVar(13), 1)
    evt.SetFacetBit(1, FacetBits.Untouchable, 0)
    evt.SetFacetBit(1, FacetBits.Invisible, 0)
    evt.SummonMonsters(2, 2, 3, -15752, 21272, 3273, 51, 0)
    evt.SummonMonsters(2, 2, 5, -14000, 18576, 4250, 51, 0)
    evt.SummonMonsters(2, 2, 10, -16016, 19280, 3284, 51, 0)
    evt.SummonMonsters(3, 2, 3, -15752, 21272, 3273, 50, 0)
    evt.SummonMonsters(3, 2, 9, -14000, 18576, 4250, 50, 0)
    evt.SummonMonsters(3, 2, 10, -16016, 19280, 3284, 50, 0)
end)
-- ReplaceMapEvent removes the legacy load registration; the source handler runs once per load.
RegisterMapOnLoadEvent(401, nil, nil)
)OY_MESSENGER";
}

std::optional<ArtifactMessengerProfile> loadArtifactMessengerProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Artifact messenger source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid artifact messenger source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read artifact messenger source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/7out04.lua fingerprint; "
            "review this Merge version before enabling the artifact messenger overlay");
    }

    return ArtifactMessengerProfile{"7out04", sourceSha256, std::string(Overlay)};
}
}
