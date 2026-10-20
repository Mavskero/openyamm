#include "Mm7ShoalsResources.h"
#include "Mm7ShoalsSource.h"

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
struct SourceDefinition
{
    std::string_view stem;
    std::string_view hash;
    std::string_view overlay;
};

std::string verifiedSourceHash(const std::filesystem::path &root, const SourceDefinition &definition)
{
    const std::filesystem::path candidate = root / "Scripts/Maps" / (std::string(definition.stem) + ".lua");
    if (!std::filesystem::exists(candidate))
    {
        throw std::runtime_error("Partial MM7 Shoals source family: " + candidate.filename().string());
    }
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("MM7 Shoals script escapes the game source root");
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > 256 * 1024)
    {
        throw std::runtime_error("Invalid MM7 Shoals source script size");
    }
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM7 Shoals source script");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    if (digest.str() != definition.hash)
    {
        throw std::runtime_error("Unsupported Scripts/Maps/" + std::string(definition.stem)
            + ".lua fingerprint; review this Merge version before enabling the MM7 Shoals route");
    }
    return digest.str();
}
}

std::optional<Mm7ShoalsProfile> loadMm7ShoalsProfile(const std::filesystem::path &source)
{
    constexpr std::array<SourceDefinition, 3> Definitions{{
        {"out14", "51391e04e9e5402ccac889cbd9b043298c42c72a700b1144c516c5ee07226f73", Mm7AvleeSource},
        {"7out15", "3bdb718ed844ce2f5bdc59682962e1dc5673f7e7b61f1d68fc0f680b698ba483", Mm7ShoalsSource},
        {"7d23", "6f22a6f6d8bbec764c8f1ef334a3bd841a4901324e971fd523fe7d9b5ff2f6cd", Mm7LincolnSource},
    }};
    const std::filesystem::path root = std::filesystem::canonical(source);
    bool anySource = false;
    for (const SourceDefinition &definition : Definitions)
    {
        anySource = anySource || std::filesystem::exists(
            root / "Scripts/Maps" / (std::string(definition.stem) + ".lua"));
    }
    if (!anySource)
    {
        return std::nullopt;
    }
    Mm7ShoalsProfile profile;
    for (const SourceDefinition &definition : Definitions)
    {
        profile.maps.push_back({std::string(definition.stem), verifiedSourceHash(root, definition),
            std::string(definition.overlay)});
    }
    return profile;
}
}
