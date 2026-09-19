#include "game/tables/HouseServiceTextTable.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
using Message = HouseServiceTextTable::Message;
using Arguments = HouseServiceTextTable::Arguments;

struct MessageDefinition
{
    std::string_view key;
    uint16_t requiredPlaceholders;
};

constexpr uint16_t Member = 1u << 0;
constexpr uint16_t Skill = 1u << 1;
constexpr uint16_t Price = 1u << 2;
constexpr uint16_t Item = 1u << 3;
constexpr uint16_t Destination = 1u << 4;
constexpr uint16_t Open = 1u << 5;
constexpr uint16_t Close = 1u << 6;
constexpr uint16_t Days = 1u << 7;
constexpr uint16_t Level = 1u << 8;
constexpr uint16_t Experience = 1u << 9;
constexpr uint16_t Points = 1u << 10;
constexpr uint16_t Food = 1u << 11;

constexpr std::array<MessageDefinition, size_t(Message::Count)> Definitions{{
    {"temple.heal_action", Price},
    {"temple.donate_action", Price},
    {"temple.learn_skills_action", 0},
    {"temple.learn_skill_action", Skill | Price},
    {"temple.no_skills", 0},
    {"temple.heal_unavailable", 0},
    {"temple.no_character", 0},
    {"temple.cannot_treat", Member},
    {"temple.already_well", Member},
    {"temple.heal_insufficient_gold", Price},
    {"temple.heal_success", Member | Price},
    {"temple.donate_unavailable", 0},
    {"temple.donate_insufficient_gold", Price},
    {"temple.donate_success", 0},
    {"temple.lesson_unavailable", 0},
    {"temple.cannot_learn", Member | Skill},
    {"temple.learn_insufficient_gold", 0},
    {"temple.learn_success", Member | Skill | Price},
    {"common.closed_hours", Open | Close},
    {"common.not_enough_gold", 0},
    {"tavern.rent_action", Price},
    {"tavern.food_action", Food | Price},
    {"tavern.packs_full", 0},
    {"tavern.rent_insufficient_gold", Price},
    {"tavern.food_insufficient_gold", Price},
    {"tavern.food_success", Food},
    {"training.action", 0},
    {"training.action_with_level_price", Level | Price},
    {"training.max_level", 0},
    {"training.experience_shortfall", Experience | Level},
    {"training.no_character", 0},
    {"training.insufficient_gold", Price},
    {"training.unavailable", 0},
    {"training.success", Member | Level | Points},
    {"shop.buy_standard_action", 0},
    {"shop.buy_special_action", 0},
    {"shop.display_equipment_action", 0},
    {"shop.sell_action", 0},
    {"shop.identify_action", 0},
    {"shop.repair_action", 0},
    {"shop.select_buy_standard", 0},
    {"shop.select_buy_special", 0},
    {"shop.select_sell", 0},
    {"shop.select_identify", 0},
    {"shop.select_repair", 0},
    {"shop.nothing_for_sale", 0},
    {"shop.unavailable", 0},
    {"shop.inventory_full", 0},
    {"shop.bought", Item | Price},
    {"shop.no_item", 0},
    {"shop.cannot_sell", 0},
    {"shop.sold", Item | Price},
    {"shop.already_identified", 0},
    {"shop.wrong_identify_shop", 0},
    {"shop.identify_failed", 0},
    {"shop.identify_success", 0},
    {"shop.nothing_to_repair", 0},
    {"shop.wrong_repair_shop", 0},
    {"shop.repair_failed", 0},
    {"shop.repair_success", 0},
    {"shop.buy_hover", Item | Price},
    {"shop.sell_hover", Item | Price},
    {"shop.identify_hover", Item | Price},
    {"shop.repair_hover", Item | Price},
    {"transport.route_action_single", Days | Destination | Price},
    {"transport.route_action_plural", Days | Destination | Price},
    {"transport.no_route_today", 0},
    {"transport.route_unavailable", 0},
    {"transport.travel_unavailable", 0},
    {"transport.insufficient_gold", Price},
    {"transport.success_single", Days | Destination},
    {"transport.success_plural", Days | Destination},
}};

constexpr std::array<std::string_view, 12> PlaceholderNames{
    "member", "skill", "price", "item", "destination", "open", "close",
    "days", "level", "experience", "points", "food",
};

bool validateTemplate(std::string_view text, uint16_t requiredPlaceholders)
{
    if (text.empty() || text.size() > 4096)
    {
        return false;
    }

    uint16_t foundPlaceholders = 0;

    for (size_t position = 0; position < text.size();)
    {
        const unsigned char character = text[position];

        if (character < 32 || character == 127)
        {
            return false;
        }

        if (character == '}')
        {
            return false;
        }

        if (character != '{')
        {
            ++position;
            continue;
        }

        const size_t end = text.find('}', position + 1);

        if (end == std::string_view::npos)
        {
            return false;
        }

        const std::string_view name = text.substr(position + 1, end - position - 1);
        const auto found = std::find(PlaceholderNames.begin(), PlaceholderNames.end(), name);

        if (found == PlaceholderNames.end())
        {
            return false;
        }

        const uint16_t flag = uint16_t(1u << size_t(found - PlaceholderNames.begin()));

        if ((requiredPlaceholders & flag) == 0 || (foundPlaceholders & flag) != 0)
        {
            return false;
        }

        foundPlaceholders |= flag;
        position = end + 1;
    }

    return foundPlaceholders == requiredPlaceholders;
}

void replacePlaceholder(std::string &text, std::string_view name, const std::string &value)
{
    const std::string token = "{" + std::string(name) + "}";
    const size_t position = text.find(token);

    if (position != std::string::npos)
    {
        text.replace(position, token.size(), value);
    }
}

std::array<std::string, PlaceholderNames.size()> argumentValues(const Arguments &arguments)
{
    return {
        arguments.member,
        arguments.skill,
        arguments.price,
        arguments.item,
        arguments.destination,
        arguments.open,
        arguments.close,
        arguments.days,
        arguments.level,
        arguments.experience,
        arguments.points,
        arguments.food,
    };
}
}

bool HouseServiceTextTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    m_templates = {};

    if (rows.size() != Definitions.size())
    {
        return false;
    }

    decltype(m_templates) templates;

    for (const std::vector<std::string> &row : rows)
    {
        if (row.size() != 2)
        {
            return false;
        }

        const auto found = std::find_if(
            Definitions.begin(),
            Definitions.end(),
            [&row](const MessageDefinition &definition)
            {
                return definition.key == row[0];
            });

        if (found == Definitions.end())
        {
            return false;
        }

        const size_t index = size_t(found - Definitions.begin());

        if (!templates[index].empty()
            || !validateTemplate(row[1], found->requiredPlaceholders))
        {
            return false;
        }

        templates[index] = row[1];
    }

    m_templates = std::move(templates);
    return true;
}

std::string HouseServiceTextTable::format(Message message, const Arguments &arguments) const
{
    const size_t index = size_t(message);

    if (index >= m_templates.size() || m_templates[index].empty())
    {
        return {};
    }

    const uint16_t requiredPlaceholders = Definitions[index].requiredPlaceholders;
    const std::array<std::string, PlaceholderNames.size()> values = argumentValues(arguments);

    for (size_t placeholderIndex = 0; placeholderIndex < values.size(); ++placeholderIndex)
    {
        const uint16_t flag = uint16_t(1u << placeholderIndex);

        if ((requiredPlaceholders & flag) != 0 && values[placeholderIndex].empty())
        {
            return {};
        }
    }

    std::string result = m_templates[index];

    for (size_t placeholderIndex = 0; placeholderIndex < values.size(); ++placeholderIndex)
    {
        if ((requiredPlaceholders & uint16_t(1u << placeholderIndex)) != 0)
        {
            replacePlaceholder(result, PlaceholderNames[placeholderIndex], values[placeholderIndex]);
        }
    }

    return result;
}

std::string HouseServiceTextTable::format(
    Message message,
    const std::string &member,
    const std::string &skill,
    int price) const
{
    Arguments arguments;
    arguments.member = member;
    arguments.skill = skill;
    arguments.price = price >= 0 ? std::to_string(price) : std::string {};
    return format(message, arguments);
}

const char *HouseServiceTextTable::key(Message message)
{
    const size_t index = size_t(message);
    return index < Definitions.size() ? Definitions[index].key.data() : "";
}
}
