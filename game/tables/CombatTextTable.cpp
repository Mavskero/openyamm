#include "game/tables/CombatTextTable.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace OpenYAMM::Game
{
bool CombatTextTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    m_patterns = {};
    constexpr std::array<std::string_view, size_t(Message::Count)> keys{
        "combat.melee_hit", "combat.projectile_hit", "combat.kill", "combat.stun", "combat.paralyze"};
    if (rows.size() != keys.size()) return false;
    decltype(m_patterns) patterns;
    for (const auto &row : rows)
    {
        if (row.size() != 2 || row[1].empty() || row[1].size() > 4096) return false;
        const auto found = std::find(keys.begin(), keys.end(), row[0]);
        if (found == keys.end()) return false;
        const size_t index = size_t(found - keys.begin());
        if (!patterns[index].empty()) return false;
        const Message message = Message(index);
        const std::array<Argument, 3> arguments = message == Message::Kill
            ? std::array{Argument::Attacker, Argument::Damage, Argument::Target}
            : std::array{Argument::Attacker, Argument::Target, Argument::Damage};
        const size_t count = message == Message::Stun || message == Message::Paralyze ? 2 : 3;
        const std::string &text = row[1];
        size_t argumentIndex = 0;
        std::string literal;
        for (size_t position = 0; position < text.size();)
        {
            const unsigned char character = text[position];
            if (character < 32 || character == 127) return false;
            if (character != '%')
            {
                literal += text[position++];
                continue;
            }
            if (text.compare(position, 2, "%%") == 0)
            {
                literal += '%';
                position += 2;
                continue;
            }
            if (argumentIndex >= count) return false;
            const Argument argument = arguments[argumentIndex++];
            const std::string_view specifier = argument == Argument::Damage ? "%lu" : "%s";
            if (text.compare(position, specifier.size(), specifier) != 0) return false;
            if (!literal.empty())
            {
                patterns[index].push_back({Argument::Literal, std::move(literal)});
                literal.clear();
            }
            patterns[index].push_back({argument, {}});
            position += specifier.size();
        }
        if (argumentIndex != count) return false;
        if (!literal.empty()) patterns[index].push_back({Argument::Literal, std::move(literal)});
    }
    m_patterns = std::move(patterns);
    return true;
}

std::string CombatTextTable::format(Message message, const std::string &attacker,
    const std::string &target, int damage) const
{
    const size_t index = size_t(message);
    if (index >= m_patterns.size()) return {};
    std::string result;
    for (const Part &part : m_patterns[index])
    {
        switch (part.argument)
        {
            case Argument::Literal: result += part.literal; break;
            case Argument::Attacker: result += attacker; break;
            case Argument::Target: result += target; break;
            case Argument::Damage: result += std::to_string(std::max(0, damage)); break;
        }
    }
    return result;
}
}
