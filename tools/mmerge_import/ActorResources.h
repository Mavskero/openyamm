#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
// Called within the outdoor import transaction; adds presentation, not actor AI.
void convertActorResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
