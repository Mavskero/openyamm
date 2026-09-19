#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertSpeechResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination);
}
