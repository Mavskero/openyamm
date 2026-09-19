#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertClassResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
