#include "game/tables/PortraitFxEventTable.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace OpenYAMM::Game
{
namespace
{
std::string toLowerCopy(const std::string &value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) -> char
    {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

PortraitFxEventKind parsePortraitFxEventKind(const std::string &value)
{
    const std::string normalized = toLowerCopy(value);

    if (normalized == "disease")
    {
        return PortraitFxEventKind::Disease;
    }

    if (normalized == "autonote")
    {
        return PortraitFxEventKind::AutoNote;
    }

    if (normalized == "awardgain")
    {
        return PortraitFxEventKind::AwardGain;
    }

    if (normalized == "questcomplete")
    {
        return PortraitFxEventKind::QuestComplete;
    }

    if (normalized == "statincrease")
    {
        return PortraitFxEventKind::StatIncrease;
    }

    if (normalized == "statdecrease")
    {
        return PortraitFxEventKind::StatDecrease;
    }

    if (normalized == "monsterspecial")
    {
        return PortraitFxEventKind::MonsterSpecial;
    }

    return PortraitFxEventKind::None;
}
}

const std::string &PortraitFxEventEntry::animationForAction(PortraitFxAction action) const
{
    switch (action)
    {
        case PortraitFxAction::Set:
            return setAnimationName.empty() ? animationName : setAnimationName;
        case PortraitFxAction::Add:
            return addAnimationName.empty() ? animationName : addAnimationName;
        case PortraitFxAction::Subtract:
            return subtractAnimationName.empty() ? animationName : subtractAnimationName;
        case PortraitFxAction::Default:
            return animationName;
    }
    return animationName;
}

bool PortraitFxEventTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    m_entries.clear();
    m_entryIndexByKind.clear();

    for (const std::vector<std::string> &row : rows)
    {
        if (row.size() < 3)
        {
            continue;
        }

        const PortraitFxEventKind kind = parsePortraitFxEventKind(row[0]);

        if (kind == PortraitFxEventKind::None || row[2].empty())
        {
            continue;
        }

        PortraitFxEventEntry entry = {};
        entry.kind = kind;
        entry.name = row[1];
        entry.animationName = row[2];

        if (row.size() >= 4 && !row[3].empty())
        {
            entry.faceAnimationId = faceAnimationIdFromName(row[3]);
        }
        if (row.size() >= 7)
        {
            entry.setAnimationName = row[4];
            entry.addAnimationName = row[5];
            entry.subtractAnimationName = row[6];
        }

        m_entryIndexByKind[entry.kind] = m_entries.size();
        m_entries.push_back(std::move(entry));
    }

    return !m_entries.empty();
}

const PortraitFxEventEntry *PortraitFxEventTable::findByKind(PortraitFxEventKind kind) const
{
    const std::unordered_map<PortraitFxEventKind, size_t>::const_iterator entryIt = m_entryIndexByKind.find(kind);

    if (entryIt == m_entryIndexByKind.end())
    {
        return nullptr;
    }

    return &m_entries[entryIt->second];
}
}
