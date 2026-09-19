#pragma once

#include "game/tables/PortraitEnums.h"
#include "game/party/SpeechIds.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
struct SpeechReactionEntry
{
    SpeechId speechId = SpeechId::None;
    std::string name;
    std::vector<std::string> soundTypes;
    // Original one-based voice groups, each containing exactly two variant slots.
    std::vector<uint32_t> voiceGroups;
    std::optional<FaceAnimationId> faceAnimationId;
};

class SpeechReactionTable
{
public:
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const SpeechReactionEntry *find(SpeechId speechId) const;

private:
    std::vector<SpeechReactionEntry> m_entries;
    std::unordered_map<SpeechId, size_t> m_entryIndexBySpeechId;
};
}
