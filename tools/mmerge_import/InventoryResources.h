#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertInventoryResources(const std::filesystem::path &source, const std::filesystem::path &gameAssets,
    const std::filesystem::path &destination);
}
