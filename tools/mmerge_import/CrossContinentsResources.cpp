#include "CrossContinentsResources.h"
#include "CrossContinentsIntroSource.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
std::optional<CrossContinentsIntroProfile> loadCrossContinentsIntroProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path candidate = root / "Scripts/Global/Quest_CrossContinents.lua";
    if (!std::filesystem::exists(candidate))
    {
        return std::nullopt;
    }
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
    {
        throw std::runtime_error("Cross-continent script escapes game source root");
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > 256 * 1024)
    {
        throw std::runtime_error("Invalid cross-continent script size");
    }
    std::string script(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read cross-continent script");
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    if (digest.str() != "0790af9b1b387b7db901f0b568593df725516e823a86fa0edb2d1fde459bce49")
    {
        throw std::runtime_error("Unsupported Quest_CrossContinents.lua fingerprint; "
            "review this Merge version before enabling the introduction overlay");
    }
    return CrossContinentsIntroProfile{digest.str(), std::string(CrossContinentsIntroSource)};
}
}
