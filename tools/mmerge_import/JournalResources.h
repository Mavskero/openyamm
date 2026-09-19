#pragma once

#include <filesystem>

namespace OpenYAMM::Import
{
void convertJournalResources(const std::filesystem::path &source, const std::filesystem::path &destination);
}
