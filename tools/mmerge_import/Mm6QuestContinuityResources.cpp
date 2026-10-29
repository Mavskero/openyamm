#include "Mm6QuestContinuityResources.h"

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

constexpr std::array<SourceFingerprint, 4> Sources = {{
    {"Scripts/Maps/sewer.lua", "3d53056e843fcbad3b7269c6cf6618d35d73deba4e370153784f70569045bb0b"},
    {"Scripts/Global/StdQuestsFollowers.lua",
        "36c5e4a5830f975febe007042a8b52600bd97f0a53bc726d4f0193d94fa616a8"},
    {"Scripts/General/NPCFollowers.lua",
        "d1798480623d05d954b6b82de04761b8e54d2dfbce237d2a150549368810c107"},
    {"Scripts/Maps/6d07.lua", "1ab7ce1bdcee95a61b88a2e9fbd3229e3d5c41c6be368939b6b052a83f28dd03"},
}};

constexpr std::string_view SewerOverlay = R"OY_MM6_SEWER(
-- sewer.lua 5-9 runs after the original event sets the prisoner quest bit and opens NPC802.
-- NPCFollowers.lua 97-117 stores an idempotent follower identity, not a playable Character or a hire fee.
AppendMapEvent(8, function(continueStep)
    if continueStep ~= nil then return end
    if IsQBitSet(QBit(1701)) then
        AddFollowerNpc(802)
    end
end)
)OY_MM6_SEWER";

constexpr std::string_view GlobalOverlay = R"OY_MM6_FOLLOW(
-- StdQuestsFollowers.lua 128-136 appends cleanup after the original global reward/topic transactions.
AppendGlobalEvent(1346, function(continueStep)
    if continueStep ~= nil then return end
    if not IsQBitSet(QBit(1701)) then
        RemoveFollowerNpc(802)
    end
end)

AppendGlobalEvent(1347, function(continueStep)
    if continueStep ~= nil then return end
    RemoveFollowerNpc(802)
end)
)OY_MM6_FOLLOW";

constexpr std::string_view KeyChestOverlay = R"OY_MM6_KEY(
-- 6d07.lua 4-14 selects the original key chest using current inventory, not the historical quest bit.
-- Source MapVar4 uses the existing exported MapVar6 selector; retain that persisted counter.
RegisterEvent(16, nil, function()
    evt.ForPlayer(Players.All)
    if HasItem(2107) and not IsAtLeast(MapVar(6), 1) then
        evt.OpenChest(6)
        return
    end

    SetValue(MapVar(6), 1)
    SetQBit(QBit(1035))
    SetQBit(QBit(1223))
    evt.OpenChest(1)
end)
)OY_MM6_KEY";

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
        throw std::runtime_error("MM6 quest continuity source escapes the game source root: "
            + std::string(source.relativePath));
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumSourceBytes)
    {
        throw std::runtime_error("Invalid MM6 quest continuity source size: " + std::string(source.relativePath));
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM6 quest continuity source: " + std::string(source.relativePath));
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
        throw std::runtime_error("Unsupported MM6 quest continuity source fingerprint: "
            + std::string(source.relativePath) + "; review this Merge version before enabling the projection");
    }
    return sourceSha256;
}
}

std::optional<Mm6QuestContinuityProfile> loadMm6QuestContinuityProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("MM6 quest continuity source root is not a directory");
    }

    std::array<std::optional<std::string>, Sources.size()> hashes;
    bool complete = true;
    for (size_t index = 0; index < Sources.size(); ++index)
    {
        hashes[index] = authenticateSource(root, Sources[index]);
        complete = hashes[index].has_value() && complete;
    }
    if (!complete)
    {
        return std::nullopt;
    }

    return Mm6QuestContinuityProfile{*hashes[0], *hashes[1], *hashes[2], *hashes[3],
        std::string(SewerOverlay), std::string(GlobalOverlay), std::string(KeyChestOverlay)};
}
}
