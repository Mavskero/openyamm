#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct CrossContinentsIntroProfile
{
    std::string sourceSha256;
    std::string luaSource;
};

// Only the audited introduction is translated. The original MMExtension script is never executed.
// Missing scripts leave the profile absent; changed scripts require an explicit compatibility review.
std::optional<CrossContinentsIntroProfile> loadCrossContinentsIntroProfile(const std::filesystem::path &source);
}
