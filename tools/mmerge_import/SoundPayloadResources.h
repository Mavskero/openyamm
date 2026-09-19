#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>

namespace OpenYAMM::Import
{
struct SoundPayloadImport
{
    std::set<std::string> names;
    uint64_t bytes = 0;
};

// Caller owns and rolls back the fresh sound-stage output on any failure.
SoundPayloadImport importSoundPayloadResources(const std::filesystem::path &source,
    const std::filesystem::path &output);
}
