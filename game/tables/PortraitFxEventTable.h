#pragma once

#include "game/tables/PortraitEnums.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
enum class PortraitFxEventKind : uint8_t
{
    None = 0,
    Disease,
    AutoNote,
    AwardGain,
    QuestComplete,
    StatIncrease,
    StatDecrease,
    MonsterSpecial,
};

enum class PortraitFxAction : uint8_t
{
    Default,
    Set,
    Add,
    Subtract,
};

struct PortraitFxEventEntry
{
    PortraitFxEventKind kind = PortraitFxEventKind::None;
    std::string name;
    std::string animationName;
    std::optional<FaceAnimationId> faceAnimationId;
    std::string setAnimationName;
    std::string addAnimationName;
    std::string subtractAnimationName;

    const std::string &animationForAction(PortraitFxAction action) const;
};

class PortraitFxEventTable
{
public:
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const PortraitFxEventEntry *findByKind(PortraitFxEventKind kind) const;

private:
    std::vector<PortraitFxEventEntry> m_entries;
    std::unordered_map<PortraitFxEventKind, size_t> m_entryIndexByKind;
};
}
