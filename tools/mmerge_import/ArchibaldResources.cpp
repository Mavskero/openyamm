#include "ArchibaldResources.h"

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
constexpr std::string_view SourcePath = "Scripts/Maps/outd3.lua";
constexpr std::string_view SourceSha256 =
    "c2191191f244a0cead4a1337032cbcb0d3f9ee1c216820e4c8fad6559745fb28";

constexpr std::string_view Overlay = R"OY_ARCHIBALD(
-- Audited outd3.lua 19-43: restore access to Archibald without granting his quest reward here.
-- Source pictures 486/487 select archloop/noarchie; recompute the room loop on each entrance.
-- Register the replacement handler while retaining the exported localized door hint.
RegisterEvent(42, nil, function()
    if IsQBitSet(QBit(1201)) then
        local roomVideo = evt.GetNPCHouse(797) == 1244 and "archloop" or "noarchie"
        evt.SetHouseVideo(1244, roomVideo)
        evt.EnterHouse(1244)
        return
    end
    evt.ForPlayer(Players.All)
    if HasItem(2081) then
        evt.ShowMovie("archie", false)
        evt.SetHouseVideo(1244, "archloop")
        evt.EnterHouse(1244)
    else
        evt.EnterHouse(1215)
    end
end)
)OY_ARCHIBALD";

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string authenticatedSourceHash(const std::filesystem::path &root)
{
    const std::filesystem::path path = std::filesystem::canonical(root / SourcePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path), "Archibald entrance source script escapes the game source root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= MaximumScriptBytes, "Invalid Archibald entrance source script size");
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(script.data(), static_cast<std::streamsize>(size))),
        "Cannot read Archibald entrance source script");

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    require(sourceSha256 == SourceSha256,
        "Unsupported Scripts/Maps/outd3.lua fingerprint; "
        "review this Merge version before enabling the Archibald entrance");
    return sourceSha256;
}
}

std::optional<ArchibaldProfile> loadArchibaldProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "Archibald entrance source root is not a directory");
    if (!std::filesystem::exists(root / SourcePath))
    {
        return std::nullopt;
    }

    return ArchibaldProfile{authenticatedSourceHash(root), std::string(Overlay)};
}
}
