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

// Audited outd3 event 42 entrance and occupied/empty room loops; global topic 1343 owns the scroll transaction.
std::optional<ArchibaldProfile> loadArchibaldProfile(const std::filesystem::path &source);
}
