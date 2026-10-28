#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace OpenYAMM::Import
{
struct ArchibaldProfile
{
    std::string sourceSha256;
    std::string luaSource;
};

// Audited outd3 event 42 entrance only; the original global topic owns the ritual scroll transaction.
std::optional<ArchibaldProfile> loadArchibaldProfile(const std::filesystem::path &source);
}
