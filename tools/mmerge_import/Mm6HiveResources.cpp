#include "Mm6HiveResources.h"
#include "Mm6HiveSource.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
std::optional<Mm6HiveProfile> loadMm6HiveProfile(const std::filesystem::path &source)
{
    constexpr std::string_view RelativePath = "Scripts/Maps/hive.lua";
    constexpr std::string_view ExpectedSha256 =
        "17cba593aec07d22b9611acfa68374427d6bfacde3aa36efe82b7cb09e825b03";
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
        throw std::runtime_error("MM6 Hive source script escapes the game source root");
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > 256 * 1024)
    {
        throw std::runtime_error("Invalid MM6 Hive source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM6 Hive source script");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    if (digest.str() != ExpectedSha256)
    {
        throw std::runtime_error("Unsupported Scripts/Maps/hive.lua fingerprint; "
            "review this Merge version before enabling the MM6 ending overlay");
    }
    return Mm6HiveProfile{"hive", digest.str(), std::string(Mm6HiveSource)};
}
}
