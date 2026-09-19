#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
// Convert shared NPC and journal tables, preserving base mechanics and IDs.
// A table subset is not a complete playable asset package.
void convertStoryResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
