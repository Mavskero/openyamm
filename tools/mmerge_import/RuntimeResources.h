#pragma once

#include "ImportCancellation.h"

#include <filesystem>
#include <functional>
#include <string>

namespace OpenYAMM::Import
{
using RuntimeImportProgress = std::function<void(const std::string &stage, size_t current, size_t total)>;

// Assembles all currently implemented converters and validates the resulting packages.
// Available source maps and supported event commands determine coverage; conversion.json records the map list.
// Destination must not exist and must be outside both inputs. Failure removes only that new destination.
void convertRuntimeResources(const std::filesystem::path &source, const std::filesystem::path &layoutDirectory,
    const std::filesystem::path &destination, const RuntimeImportProgress &progress = {},
    const ImportCancelCheck &cancelled = {});
}
