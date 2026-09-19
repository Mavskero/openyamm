#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class CombatTextTable
{
public:
    enum class Message { MeleeHit, ProjectileHit, Kill, Stun, Paralyze, Count };

    // The content file contains all five semantic keys and original legacy templates.
    // Only the declared %s/%lu argument sequence and escaped %% are accepted.
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    std::string format(Message message, const std::string &attacker, const std::string &target, int damage) const;

private:
    enum class Argument { Literal, Attacker, Target, Damage };
    struct Part
    {
        Argument argument = Argument::Literal;
        std::string literal;
    };
    std::array<std::vector<Part>, size_t(Message::Count)> m_patterns;
};
}
