#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct Mm7EndingProfile
{
    std::string sourceSha256;
    std::string luaSource;
};

// Appends the source topic 920/922 quest-bit tails, preserving the exported ending transactions.
// Missing StdQuestsFunctions disables the projection; an available script must match the audited source.
std::optional<Mm7EndingProfile> loadMm7EndingProfile(const std::filesystem::path &source);
}
