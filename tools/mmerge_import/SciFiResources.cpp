#include "SciFiResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/sci-fi.lua";
constexpr std::string_view ExpectedSha256 =
    "ad17446014d220995db9fba7a61e70926f2ef7e3076680af5ef844fcfa7d348f";

constexpr std::string_view Overlay = R"OY_SCI_FI(
-- Append only the audited MMerge terminal's basic Blaster learning; preserve every existing nonzero skill.
AppendMapEvent(61, function(continueStep)
    if continueStep ~= nil then return end
    for _, player in ipairs(PartyMembers()) do
        evt.ForPlayer(player)
        if not IsAtLeast(BlasterSkill, 1) then
            SetValue(BlasterSkill, 1)
        end
    end
end)
)OY_SCI_FI";
}

std::optional<SciFiProfile> loadSciFiProfile(const std::filesystem::path &source)
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
        throw std::runtime_error("Sci-fi source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid sci-fi source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read sci-fi source script");
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
        throw std::runtime_error("Unsupported Scripts/Maps/sci-fi.lua fingerprint; "
            "review this Merge version before enabling the terminal Blaster learning overlay");
    }

    return SciFiProfile{"sci-fi", sourceSha256, std::string(Overlay)};
}
}
