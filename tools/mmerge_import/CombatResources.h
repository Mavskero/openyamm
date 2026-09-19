#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertCombatResources(const std::filesystem::path &source, const std::filesystem::path &coreAssets,
    const std::filesystem::path &destination);
}
