#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
// Imports the global registry/SFT and the decoration presentation used by oute3.odm.
void convertDecorationResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
