#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertRaceResources(const std::filesystem::path &source, const std::filesystem::path &classAssets,
    const std::filesystem::path &destination);
}
