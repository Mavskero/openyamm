#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertFrameResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
