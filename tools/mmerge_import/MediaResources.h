#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertMediaResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
