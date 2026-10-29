#include "PostQuestVisitResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumScriptBytes = 256 * 1024;
constexpr std::string_view TritonSourcePath = "Scripts/Maps/d24.lua";
constexpr std::string_view TritonSourceSha256 =
    "cb195e12763f41e197ef423064add77c52f7672b14c41bf4420c97fb69cc4a75";

constexpr std::string_view ClankerOverlay = R"OY_CLANKER_VISIT(
-- The source completed-quest visit uses a house with an ordinary extra exit into the laboratory.
RegisterEvent(503, nil, function()
    if IsQBitSet(QBit(710)) then
        evt.MoveNPC(427, 395)
        evt.EnterHouse(395)
    else
        evt.MoveToMap(0, -709, 1, 512, 0, 0, 395, 9, "7d12.blv")
    end
end)
)OY_CLANKER_VISIT";

std::string tritonOverlay()
{
    return "\n-- Apply the source LoadMap supplement after a saved map snapshot has been restored.\n"
        "RegisterMapAfterLoadEvent(" + std::to_string(PostQuestTritonHideHookEventId)
        + R"OY_TRITON_VISIT(, nil, function()
    if IsQBitSet(QBit(23)) then
        evt.SetMonGroupBit(0, MonsterBits.Invisible, 1)
    end
end)
)OY_TRITON_VISIT";
}

std::optional<std::string> authenticatedSourceHash(const std::filesystem::path &root,
    std::string_view relativePath, std::string_view expectedSha256)
{
    if (!std::filesystem::exists(root / relativePath))
    {
        return std::nullopt;
    }
    const std::filesystem::path path = std::filesystem::canonical(root / relativePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Post-quest visit source script escapes the game source root");
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid post-quest visit source script size");
    }
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read post-quest visit source script");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    if (digest.str() != expectedSha256)
    {
        throw std::runtime_error("Unsupported " + std::string(relativePath)
            + " fingerprint; review this Merge version before enabling post-quest visits");
    }
    return digest.str();
}
}

std::vector<PostQuestVisitProfile> loadPostQuestVisitProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    std::vector<PostQuestVisitProfile> profiles;
    if (const std::optional<std::string> hash = authenticatedSourceHash(root, TritonSourcePath, TritonSourceSha256))
    {
        profiles.push_back({"mm8", "d24", *hash, tritonOverlay(), PostQuestTritonHideHookEventId});
    }
    if (const std::optional<std::string> hash =
        authenticatedSourceHash(root, ClankerPostQuestSourcePath, ClankerPostQuestSourceSha256))
    {
        profiles.push_back({"mm7", "7out04", *hash, std::string(ClankerOverlay), 0});
    }
    return profiles;
}
}
