#include "Mm6LostShieldResources.h"

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
constexpr size_t MaximumSourceBytes = 256 * 1024;

struct SourceFingerprint
{
    std::string_view relativePath;
    std::string_view sha256;
};

constexpr std::array<SourceFingerprint, 2> Sources = {{
    {"Scripts/Maps/outb2.lua", "f7bea5436ce2366d07b201a831b0d0121ca7e09ceca8783465755b2e8f4dafa8"},
    {"Scripts/Global/StdQuestsFunctions.lua",
        "201eed1ebb5b6ed02878eaf92268a4190b7de90eecedbffa7cda63b7b2754623"},
}};

constexpr std::string_view Overlay = R"OY_MM6_SHIELD(
-- outb2.lua 38-53; StdQuestsFunctions.lua 746-772 records this zero-value quest item after GotItem.
-- Existing possession history is permanent. A shield already in the chest leaves every record untouched.
RegisterChestOpenHook(65476, nil, function(context)
    if context.chestId == 1 and not HasEverOwnedItem(2119) then
        -- The shared operation scans all current chest records, then changes only source Items[1].
        -- It resets Number/Bonus/Bonus2 while retaining the record's other state and cached identity.
        evt.EnsureChestItemRecord(1, 2119, 1)
    end
end)
)OY_MM6_SHIELD";

std::optional<std::string> authenticateSource(
    const std::filesystem::path &root, const SourceFingerprint &source)
{
    const std::filesystem::path candidate = root / source.relativePath;
    if (std::filesystem::symlink_status(candidate).type() == std::filesystem::file_type::not_found)
    {
        return std::nullopt;
    }
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("MM6 lost shield source escapes the game source root: "
            + std::string(source.relativePath));
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumSourceBytes)
    {
        throw std::runtime_error("Invalid MM6 lost shield source size: " + std::string(source.relativePath));
    }
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM6 lost shield source: " + std::string(source.relativePath));
    }

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != source.sha256)
    {
        throw std::runtime_error("Unsupported MM6 lost shield source fingerprint: "
            + std::string(source.relativePath) + "; review this Merge version before enabling the projection");
    }
    return sourceSha256;
}
}

std::optional<Mm6LostShieldProfile> loadMm6LostShieldProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::optional<std::string> mapHash = authenticateSource(root, Sources[0]);
    const std::optional<std::string> trackerHash = authenticateSource(root, Sources[1]);
    if (!mapHash || !trackerHash)
    {
        return std::nullopt;
    }
    return Mm6LostShieldProfile{"outb2", *mapHash, *trackerHash, std::string(Overlay)};
}
}
