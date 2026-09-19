#pragma once

#include "ImportCancellation.h"

#include <filesystem>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
// Read merged EVT/STR archives and the already converted shared table base.
// An optional map stem limits a diagnostic import; shared Global is always included.
void convertEventResources(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::string &mapStem = {});
void convertEventResourcesForMaps(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::vector<std::string> &mapStems,
    const ImportCancelCheck &cancelled = {});
}
