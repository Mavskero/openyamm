#include "Mm7EndingResources.h"

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
constexpr std::string_view SourcePath = "Scripts/Global/StdQuestsFunctions.lua";
constexpr std::string_view SourceSha256 =
    "201eed1ebb5b6ed02878eaf92268a4190b7de90eecedbffa7cda63b7b2754623";

constexpr std::string_view Overlay = R"OY_MM7_ENDING(
-- Audited StdQuestsFunctions.lua 795-797: run after the original topic 920 transaction.
AppendGlobalEvent(920, function(continueStep)
    if continueStep ~= nil then return end
    if IsQBitSet(QBit(783)) then
        ClearQBit(QBit(642))
    else
        SetQBit(QBit(642))
    end
end)
)OY_MM7_ENDING";

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
        && std::filesystem::is_regular_file(path), "MM7 ending source script escapes the game source root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= MaximumScriptBytes, "Invalid MM7 ending source script size");
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(script.data(), static_cast<std::streamsize>(size))),
        "Cannot read MM7 ending source script");

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    require(sourceSha256 == SourceSha256,
        "Unsupported StdQuestsFunctions.lua fingerprint; "
        "review this Merge version before enabling the MM7 ending tail");
    return sourceSha256;
}
}

std::optional<Mm7EndingProfile> loadMm7EndingProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "MM7 ending source root is not a directory");
    if (!std::filesystem::exists(root / SourcePath))
    {
        return std::nullopt;
    }

    return Mm7EndingProfile{authenticatedSourceHash(root), std::string(Overlay)};
}
}
