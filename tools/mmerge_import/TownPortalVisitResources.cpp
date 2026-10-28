#include "TownPortalVisitResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <optional>
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

struct MapVisitSource
{
    SourceFingerprint fingerprint;
    std::string_view mapStem;
    uint16_t eventId;
    uint32_t legacyQBit;
    uint32_t unlockQBit;
    bool afterLoad;
};

constexpr std::array<SourceFingerprint, 3> MappingSources = {{
    {"Scripts/General/1_TownPortalSwitch.lua",
        "ebbda89651ab71b7311dd94c18802d94e6c56b12b6776c02f73bb9d077f2d72f"},
    {"Scripts/Global/TownPortalSwitches.lua",
        "ea7114b1515c8815940c83e48f4aee7609769993e99491a534457a8814dc7fda"},
    {"Data/Tables/TownPortalSwitch.txt",
        "aefeac53ef099d9549c8834b49ed913611a53ca17d3b6359aa5bd9208d38fbca"},
}};

// Source Game.Version 8 reuses six display slots; SwitchTo persists them to the Enroth table's QBits.
constexpr std::array<MapVisitSource, 6> MapSources = {{
    {{"Scripts/Maps/outb2.lua", "f7bea5436ce2366d07b201a831b0d0121ca7e09ceca8783465755b2e8f4dafa8"},
        "outb2", 65024, 181, 310, true},
    {{"Scripts/Maps/outc1.lua", "b9964160284ae83fe3555cc9658cb8e5539b3e3d829a90e79d89cf8d1c9285fc"},
        "outc1", 65025, 185, 315, false},
    {{"Scripts/Maps/outc2.lua", "8551ac3062309de282c22470aabd4e27ea151d761a7985fda65b2cb119007b7f"},
        "outc2", 65026, 180, 311, true},
    {{"Scripts/Maps/outd1.lua", "1ee4775921cdc9d0b0cc94d899e199542b5cbc33fe1765ec2fc67edc338caecc"},
        "outd1", 65027, 182, 314, true},
    {{"Scripts/Maps/oute2.lua", "3fa6cd65264130bdd354d357b9fcdd469c5a4b464b999a92f610bc06504b5a76"},
        "oute2", 65028, 184, 312, true},
    {{"Scripts/Maps/oute3.lua", "731955ac2b119b5ed6f32d7a01ff08ea82fac4cb7b60596b9fca80250f846e08"},
        "oute3", 65029, 183, 313, true},
}};

std::optional<std::string> authenticateSource(
    const std::filesystem::path &root, const SourceFingerprint &fingerprint)
{
    const std::filesystem::path candidate = root / fingerprint.relativePath;
    if (std::filesystem::symlink_status(candidate).type() == std::filesystem::file_type::not_found)
    {
        return std::nullopt;
    }

    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Invalid Town Portal visit source path: " + std::string(fingerprint.relativePath));
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumSourceBytes)
    {
        throw std::runtime_error("Invalid Town Portal visit source size: " + std::string(fingerprint.relativePath));
    }

    std::string bytes(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(bytes.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Town Portal visit source: " + std::string(fingerprint.relativePath));
    }

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != fingerprint.sha256)
    {
        throw std::runtime_error("Unsupported Town Portal visit source fingerprint: "
            + std::string(fingerprint.relativePath) + "; review this Merge version before enabling visits");
    }
    return sourceSha256;
}

std::string makeOverlay(const MapVisitSource &map)
{
    const std::string hook = map.afterLoad ? "RegisterMapAfterLoadEvent" : "RegisterMapOnLoadEvent";
    return "-- Persist the source visit to its continent destination, not the engine's shared display slot.\n"
        + hook + "(" + std::to_string(map.eventId) + ", nil, function()\n"
        + "    SetQBit(QBit(" + std::to_string(map.unlockQBit) + "))\nend)\n";
}
}

std::vector<TownPortalVisitProfile> loadTownPortalVisitProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    bool hasMappingSources = true;
    for (const SourceFingerprint &fingerprint : MappingSources)
    {
        if (!authenticateSource(root, fingerprint))
        {
            hasMappingSources = false;
        }
    }

    std::vector<TownPortalVisitProfile> profiles;
    for (const MapVisitSource &map : MapSources)
    {
        const std::optional<std::string> sourceSha256 = authenticateSource(root, map.fingerprint);
        if (hasMappingSources && sourceSha256)
        {
            profiles.push_back({std::string(map.mapStem), *sourceSha256, map.eventId, map.legacyQBit,
                map.unlockQBit, map.afterLoad, makeOverlay(map)});
        }
    }
    return profiles;
}
}
