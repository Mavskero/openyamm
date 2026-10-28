#include "SubmarineTravelResources.h"

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

constexpr std::string_view EntranceOverlay = R"OY_SUBMARINE_IN(
-- RegisterEvent replaces only the handler, preserving the original localized title and hint.
-- Source d06.lua:4-13 retains the key and remembers the first permitted submarine use.
RegisterEvent(451, nil, function()
    evt.ForPlayer(Players.All)
    if HasItem(619) or IsQBitSet(QBit(214)) then
        SetQBit(QBit(214))
        evt.ShowMovie("\"Subcut\" ", true)
        evt.MoveToMap(-2416, 1850, -687, 1536, 0, 0, 0, 0, "d34.blv")
    else
        evt.ForPlayer(Players.Current)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end)
)OY_SUBMARINE_IN";

constexpr std::string_view ReturnOverlay = R"OY_SUBMARINE_OUT(
-- Source d34.lua:4-6 makes the return unconditional and retains its original Icon 1 confirmation.
RegisterEvent(451, nil, function()
    evt.MoveToMap(7097, -1117, -639, 1536, 0, 0, 0, 1, "d06.blv")
end)
)OY_SUBMARINE_OUT";

struct SourceDefinition
{
    std::string_view mapStem;
    std::string_view relativePath;
    std::string_view expectedSha256;
    std::string_view overlay;
};

constexpr std::array<SourceDefinition, 2> Sources = {{
    {"d06", "Scripts/Maps/d06.lua",
        "2dda697d6b39645d49ad231174ab0d5f673dd1074e62905ccebe301540d8a13c", EntranceOverlay},
    {"d34", "Scripts/Maps/d34.lua",
        "0657938b8b2e024e1da036c7affd75825906ffe694b8dc196a87b4036ae8844e", ReturnOverlay}
}};

std::string authenticatedSourceHash(const std::filesystem::path &root, const SourceDefinition &definition)
{
    const std::filesystem::path path = std::filesystem::canonical(root / definition.relativePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Submarine travel source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid submarine travel source script size: "
            + std::string(definition.relativePath));
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read submarine travel source script: " + std::string(definition.relativePath));
    }

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != definition.expectedSha256)
    {
        throw std::runtime_error("Unsupported " + std::string(definition.relativePath) + " fingerprint; "
            "review this Merge version before enabling submarine travel");
    }
    return sourceSha256;
}
}

std::vector<SubmarineTravelProfile> loadSubmarineTravelProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("Submarine travel source root is not a directory");
    }

    size_t presentSources = 0;
    for (const SourceDefinition &definition : Sources)
    {
        if (std::filesystem::exists(root / definition.relativePath))
        {
            ++presentSources;
        }
    }
    if (presentSources == 0)
    {
        return {};
    }
    if (presentSources != Sources.size())
    {
        throw std::runtime_error("Submarine travel requires both Scripts/Maps/d06.lua and Scripts/Maps/d34.lua");
    }

    std::vector<SubmarineTravelProfile> profiles;
    profiles.reserve(Sources.size());
    for (const SourceDefinition &definition : Sources)
    {
        const std::string sourceSha256 = authenticatedSourceHash(root, definition);
        profiles.push_back({std::string(definition.mapStem), sourceSha256, std::string(definition.overlay)});
    }
    return profiles;
}
}
