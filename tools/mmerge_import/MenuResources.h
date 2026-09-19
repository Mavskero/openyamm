#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
// Converts only the original MM8/merged title and buttons, with the supplied
// Chinese D archive taking precedence. This is not a complete gameplay package.
// The destination must be new. Throws on invalid resources; never changes source.
void convertMenuResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
