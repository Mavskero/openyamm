#pragma once

#include "ImportCancellation.h"

#include <filesystem>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct SceneMapSelection
{
    std::vector<std::string> available;
    std::vector<std::string> missing;
};

enum class SceneImportMode
{
    Gameplay,
    ResourceInspection,
    // Only the three authenticated Breach source projections; never general Merge Map Lua admission.
    NativeMergeGameplay,
};

// Registered filenames with both geometry and companion present, including patch overrides.
SceneMapSelection discoverSceneMapFiles(const std::filesystem::path &source,
    const std::filesystem::path &convertedBase, const std::string &worldId);

// Base must include shared tables and converted map events, including their texture/sprite metadata.
// NativeMergeGameplay writes normal world resources, retaining ResourceInspection's exact source-profile checks.
// Convert registered BLV/DLV and ODM/DDM scenes and referenced presentation resources into a new staging directory.
void convertSceneResources(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::vector<std::string> &mapFiles,
    const ImportCancelCheck &cancelled = {}, SceneImportMode mode = SceneImportMode::Gameplay,
    bool chinese = true);
}
