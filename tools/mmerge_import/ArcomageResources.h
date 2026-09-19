#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertArcomageResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination);
}
