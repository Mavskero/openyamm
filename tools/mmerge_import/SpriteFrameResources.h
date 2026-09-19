#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct ConvertedSpriteFrames
{
    std::string yaml;
    std::map<std::string, uint16_t> groupIds;
    std::set<int16_t> paletteIds;
    size_t frameCount = 0;
    size_t groupCount = 0;
};

// Rows exclude the header. Original global SFT indices are retained even for a subset.
ConvertedSpriteFrames convertSpriteFrameRows(const std::vector<std::vector<std::string>> &rows,
    const std::set<std::string> *pSelectedGroups = nullptr);
}
