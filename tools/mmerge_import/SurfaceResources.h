#pragma once

#include <string>
#include <vector>

namespace OpenYAMM::Import
{
// Read the source's declarative hardware-water bindings and patch-option overrides; never execute Lua.
std::string convertHardwareWaterMaterials(const std::vector<std::vector<std::string>> &source,
    const std::string &miscTweaks);

// Tile/Tile2/Tile3 share one schema; result includes the portable terrain header.
std::vector<std::vector<std::string>> convertTerrainRows(
    const std::vector<std::vector<std::string>> &source);

// Source rows include the MMMerge TFT header. Preserve frame indices and 1/32-second source times.
std::vector<std::vector<std::string>> convertTextureFrameRows(
    const std::vector<std::vector<std::string>> &source);
}
