#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
// The guard is valid only for the audited, synchronous reward branches in this original Global.evt.
// Unknown source bytes are rejected; the original event handlers and topic predicates stay authoritative.
std::string buildQuestItemCapacityOverlay(const std::vector<uint8_t> &globalEventBytes, bool chinese);
}
