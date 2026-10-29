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
    {"ui.arcomage_resign", "Resign"},
    {"ui.arcomage_touch_hint", "Tap to play; swipe up to discard."},
    {"ui.arcomage_discard_hint", "Choose a card to discard."},
    {"ui.item_type", "Type: %s"},
    {"ui.item_stolen", "Stolen"},
    {"ui.sp_cost", "SP Cost"},
    {"ui.item_power", "Power"},
    {"ui.item_attack", "Attack"},
    {"ui.item_shoot", "Shoot"},
    {"ui.item_damage", "Damage"},
    {"ui.item_charges", "Charges"},
    {"ui.item_armor", "Armor"},
    {"ui.item_value", "Value"},
}};

size_t itemTypeParameterOffset(const std::string &format)
{
    const size_t offset = format.find('%');
    return offset != std::string::npos
            && format.compare(offset, 2, "%s") == 0
            && format.find('%', offset + 2) == std::string::npos
        ? offset : std::string::npos;
}
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
    // Migrate only complete older prefixes: the original 13 captions, the 16 with
    // Arcomage and the 19 with initial inspect labels. Other missing-key shapes remain errors.
    constexpr size_t OriginalMessageCount = size_t(Message::ArcomageResign);
    constexpr size_t ArcomageMessageCount = size_t(Message::ItemTypeFormat);
    constexpr size_t InspectMessageCount = size_t(Message::ItemPower);
    const size_t schemaMessageCount = rows.size();
    if (schemaMessageCount != Definitions.size()
        && schemaMessageCount != OriginalMessageCount
        && schemaMessageCount != ArcomageMessageCount
        && schemaMessageCount != InspectMessageCount)
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
        if (index >= schemaMessageCount || !texts[index].empty()
            || (index == size_t(Message::ItemTypeFormat)
                && itemTypeParameterOffset(row[1]) == std::string::npos))
        {
            return false;
        }
        texts[index] = row[1];
    }
    for (size_t index = schemaMessageCount; index < Definitions.size(); ++index)
    {
        texts[index] = Definitions[index].pEnglish;
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

std::string UiTextTable::formatItemType(const std::string &type) const
{
    std::string result = text(Message::ItemTypeFormat);
    const size_t offset = itemTypeParameterOffset(result);
    if (offset == std::string::npos)
    {
        return {};
    }
    result.replace(offset, 2, type);
    return result;
}

const char *UiTextTable::key(Message message)
{
    const size_t index = size_t(message);
    return index < Definitions.size() ? Definitions[index].key.data() : "";
}
}
