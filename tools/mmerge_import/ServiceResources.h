#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertServiceResources(const std::filesystem::path &source, const std::filesystem::path &destination);
void convertHousePresentationResources(const std::filesystem::path &source,
    const std::filesystem::path &convertedBase, const std::filesystem::path &destination);
}
