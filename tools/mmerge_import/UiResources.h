#pragma once

#include <cstdint>
#include <filesystem>

namespace OpenYAMM::Import
{
void convertUiResources(const std::filesystem::path &source, const std::filesystem::path &layoutDirectory,
    uint32_t uiSetId, const std::filesystem::path &destination);
}
