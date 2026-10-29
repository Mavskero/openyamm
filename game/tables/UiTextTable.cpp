#include "game/tables/UiTextTable.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
struct TextDefinition
{
    std::string_view key;
    const char *pEnglish;
};

constexpr std::array<TextDefinition, size_t(UiTextTable::Message::Count)> Definitions{{
    {"ui.empty_slot", "Empty"},
    {"ui.menu", "Menu"},
    {"ui.submit", "Submit"},
    {"ui.cancel", "Cancel"},
    {"ui.beacon_set", "Set Beacon"},
    {"ui.beacon_recall", "Recall Beacon"},
    {"ui.beacon_available", "Available"},
    {"ui.day", "Day"},
    {"ui.days", "Days"},
    {"ui.hour", "Hour"},
    {"ui.hours", "Hours"},
    {"ui.minute", "Minute"},
    {"ui.minutes", "Minutes"},
}};
}

UiTextTable::UiTextTable()
{
    for (size_t index = 0; index < Definitions.size(); ++index)
    {
        m_texts[index] = Definitions[index].pEnglish;
    }
}

bool UiTextTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    if (rows.size() != Definitions.size())
    {
        return false;
    }
    decltype(m_texts) texts;
    for (const std::vector<std::string> &row : rows)
    {
        if (row.size() != 2 || row[1].empty() || row[1].size() > 4096
            || row[1].find_first_not_of(' ') == std::string::npos
            || std::any_of(row[1].begin(), row[1].end(), [](unsigned char character)
                { return character < 32 || character == 127; }))
        {
            return false;
        }
        const auto found = std::find_if(Definitions.begin(), Definitions.end(),
            [&row](const TextDefinition &definition) { return definition.key == row[0]; });
        if (found == Definitions.end())
        {
            return false;
        }
        const size_t index = size_t(found - Definitions.begin());
        if (!texts[index].empty())
        {
            return false;
        }
        texts[index] = row[1];
    }
    m_texts = std::move(texts);
    return true;
}

const std::string &UiTextTable::text(Message message) const
{
    static const std::string Empty;
    const size_t index = size_t(message);
    return index < m_texts.size() ? m_texts[index] : Empty;
}

const char *UiTextTable::key(Message message)
{
    const size_t index = size_t(message);
    return index < Definitions.size() ? Definitions[index].key.data() : "";
}
}
