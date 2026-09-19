#pragma once

#include "ImportCancellation.h"

#include <filesystem>

namespace OpenYAMM::Import
{
// Streams a stored ZIP archive with UTF-8 names and CRCs; never loads an entire resource into memory.
// Rejects ZIP32 limits instead of producing a truncated package.
void writeRuntimeZip(const std::filesystem::path &root, const std::filesystem::path &destination,
    const ImportCancelCheck &cancelled = {});
}
