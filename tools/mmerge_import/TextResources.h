#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
std::string decodeLegacyText(const std::vector<uint8_t> &bytes, bool chinese, size_t &restoredGlyphs);
std::string serializeTextRows(const std::vector<std::vector<std::string>> &rows);
}
