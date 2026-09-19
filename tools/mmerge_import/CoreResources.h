#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertCoreResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
