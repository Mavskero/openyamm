#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct Mm7ShoalsMapOverlay
{
    std::string mapStem;
    std::string sourceSha256;
    std::string luaSource;
};

struct Mm7ShoalsProfile
{
    std::vector<Mm7ShoalsMapOverlay> maps;
};

// The three source scripts form one route. An absent source family is optional;
// a partial, changed, or escaping family is rejected rather than partly enabled.
std::optional<Mm7ShoalsProfile> loadMm7ShoalsProfile(const std::filesystem::path &source);
}
