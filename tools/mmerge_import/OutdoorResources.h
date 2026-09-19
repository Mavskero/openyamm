#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
// Native scene import for MM6 oute3 in the supplied MMMerge layout. Converts
// terrain/building surfaces, optionally actor presentation; output must be new.
void convertOutdoorResources(const std::filesystem::path &source, const std::filesystem::path &destination,
    bool includeActors = false);
}
