#include "TempleBottleResources.h"

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
constexpr std::string_view MapSourcePath = "Scripts/Maps/7nwc.lua";
constexpr std::string_view ItemSourcePath = "Scripts/General/UsableItems.lua";
constexpr std::string_view MapSourceSha256 =
    "ae80365f5502ac3f5a8707cf49b5012d255d322fd89842d1b9edb13f7d898edc";
constexpr std::string_view ItemSourceSha256 =
    "4d3409d6464ab8e5dead5effa67e83200b7bfd3e2088cb7534b17ba0c686649f";

constexpr std::string_view Overlay = R"OY_TEMPLE_BOTTLE(
-- Replace only the exit handler, retaining the original localized title and hint.
-- The item keeps its origin in session state; a successful return consumes that location.
RegisterEvent(501, nil, function()
    if not evt.MoveToSessionLocation("TempleInABottleReturn", true, 0, 0) then
        evt.MoveToMap(-177331, 12547, 465, 0, 0, 0, 0, 8, "out02.odm")
    end
end)
)OY_TEMPLE_BOTTLE";

std::string authenticatedSourceHash(const std::filesystem::path &root, std::string_view relativePath,
    std::string_view expectedSha256)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relativePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Temple bottle source script is not a regular file within the game source root: "
            + std::string(relativePath));
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid temple bottle source script size: " + std::string(relativePath));
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read temple bottle source script: " + std::string(relativePath));
    }

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != expectedSha256)
    {
        throw std::runtime_error("Unsupported " + std::string(relativePath) + " fingerprint; "
            "review this Merge version before enabling temple bottle return");
    }
    return sourceSha256;
}
}

std::optional<TempleBottleProfile> loadTempleBottleProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("Temple bottle source root is not a directory");
    }
    if (!std::filesystem::exists(root / MapSourcePath))
    {
        return std::nullopt;
    }
    if (!std::filesystem::exists(root / ItemSourcePath))
    {
        throw std::runtime_error("Temple bottle return requires Scripts/General/UsableItems.lua");
    }

    const std::string sourceSha256 = authenticatedSourceHash(root, MapSourcePath, MapSourceSha256);
    const std::string itemSourceSha256 = authenticatedSourceHash(root, ItemSourcePath, ItemSourceSha256);
    return TempleBottleProfile{"7nwc", sourceSha256, itemSourceSha256};
}

std::string makeTempleBottleOverlay()
{
    return std::string(Overlay);
}
}
