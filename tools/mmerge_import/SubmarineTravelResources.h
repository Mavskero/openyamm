#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct SubmarineTravelProfile
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

// Translates only the authenticated d06/d34 event 451 replacements.
// Both absent leaves the profiles empty; a partial or changed source pair is rejected.
std::vector<SubmarineTravelProfile> loadSubmarineTravelProfiles(const std::filesystem::path &source);
}
