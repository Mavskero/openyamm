#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
// assetRoot is the output resources root containing engine/. Original source remains read-only.
// Returns false, without writing, when the source has no BrBase profile; invalid present sources throw.
bool writeBreachMazeRuntimeResources(const std::filesystem::path &source, const std::filesystem::path &assetRoot);
}
