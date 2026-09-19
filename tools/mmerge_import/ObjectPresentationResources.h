#pragma once

#include "ImportCancellation.h"

#include <filesystem>

namespace OpenYAMM::Import
{
// Global object descriptors also create sprites during play, outside authored map object lists.
// Export their original animation frames/palettes into a new, shared engine staging directory.
void convertObjectPresentationResources(const std::filesystem::path &source,
    const std::filesystem::path &convertedBase, const std::filesystem::path &destination,
    const ImportCancelCheck &cancelled = {});
}
