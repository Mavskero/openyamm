#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertTransitionResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
