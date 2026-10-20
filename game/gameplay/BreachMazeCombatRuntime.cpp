#include "game/gameplay/BreachMazeCombatRuntime.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
std::array<double, 2> mazeCellPosition(double x, double y)
{
    return {std::floor((x + 300.525) / BreachMazeCellWidth) + 1.0,
        std::floor((-y + 300.525) / BreachMazeCellWidth) + 1.0};
}
}

BreachMazeCombatRuntime::BreachMazeCombatRuntime(const BreachMazeLayout &layout,
    std::span<const uint8_t> actorRage, double installGameMinutes)
    : m_layout(layout), m_actorRage(actorRage.begin(), actorRage.end()),
      m_nextUpdateGameMinutes(installGameMinutes)
{
    if (layout.algorithmVersion != BreachMazeAlgorithmVersion
        || layout.side < 4 || layout.side > BreachMazeTemplateSide
        || layout.cells.size() != size_t(layout.side) * layout.side
        || !std::isfinite(installGameMinutes) || installGameMinutes < 0.0)
    {
        throw std::invalid_argument("Invalid Breach maze combat profile");
    }
    for (size_t index = 0; index < layout.cells.size(); ++index)
    {
        const size_t x = index % layout.side + 1;
        const size_t y = index / layout.side;
        if (layout.cells[index].templateCellId != y * BreachMazeTemplateSide + x)
        {
            throw std::invalid_argument("Breach maze combat cell identity mismatch");
        }
    }
    if (std::any_of(m_actorRage.begin(), m_actorRage.end(), [](uint8_t value) { return value > 1; }))
    {
        throw std::invalid_argument("Invalid Breach maze actor Rage");
    }
}

void BreachMazeCombatRuntime::ensureActorCount(size_t actorCount)
{
    if (m_actorRage.size() < actorCount)
    {
        m_actorRage.resize(actorCount, 0);
    }
}

void BreachMazeCombatRuntime::notePartyDamage(size_t actorIndex)
{
    if (actorIndex < m_actorRage.size())
    {
        m_actorRage[actorIndex] = 1;
    }
}

bool BreachMazeCombatRuntime::allowsPathfinding(size_t actorIndex) const
{
    return actorIndex < m_actorRage.size() && m_actorRage[actorIndex] != 0;
}

std::span<const uint8_t> BreachMazeCombatRuntime::actorRage() const
{
    return m_actorRage;
}

bool BreachMazeCombatRuntime::updateDue(double gameMinutes) const
{
    return std::isfinite(gameMinutes) && gameMinutes >= m_nextUpdateGameMinutes;
}

const BreachMazeCell *BreachMazeCombatRuntime::cellAt(double x, double y) const
{
    if (!std::isfinite(x) || !std::isfinite(y) || x < 1 || y < 1 || x > m_layout.side || y > m_layout.side)
    {
        return nullptr;
    }
    return &m_layout.cells[(size_t(y) - 1) * m_layout.side + size_t(x) - 1];
}

BreachMazeCombatProjection BreachMazeCombatRuntime::projectHostility(double gameMinutes,
    double partyX, double partyY, int torchPower, std::span<const BreachMazeCombatActorFacts> actors)
{
    if (!updateDue(gameMinutes))
    {
        throw std::invalid_argument("Breach maze hostility update is not due");
    }
    // Source Timer(..., Minute, true): first eligible tick, then one minute after each invocation.
    // A large clock advance invokes it once rather than replaying missed minutes.
    m_nextUpdateGameMinutes = gameMinutes + 1.0;
    BreachMazeCombatProjection projection;
    projection.torchPower = std::min(torchPower, 3);
    const std::array<double, 2> partyCell = mazeCellPosition(partyX, partyY);
    const BreachMazeCell *pPartyCell = cellAt(partyCell[0], partyCell[1]);
    if (pPartyCell == nullptr)
    {
        return projection;
    }
    projection.actors.reserve(actors.size());
    for (const BreachMazeCombatActorFacts &actor : actors)
    {
        if (actor.hp <= 0 || actor.ally == 9999)
        {
            continue;
        }
        BreachMazeCombatHostilityDecision decision;
        decision.actorIndex = actor.actorIndex;
        if (allowsPathfinding(actor.actorIndex))
        {
            decision.hostilityType = 4;
            decision.setHostile = true;
        }
        else
        {
            const std::array<double, 2> actorCell = mazeCellPosition(actor.x, actor.y);
            const BreachMazeCell *pActorCell = cellAt(actorCell[0], actorCell[1]);
            const uint16_t room = pActorCell != nullptr ? pActorCell->room : m_layout.cells.front().room;
            decision.restorePosition = pActorCell == nullptr;
            // Source recovery preserves the old cell coordinates for this invocation's range check.
            const double dx = partyCell[0] - actorCell[0];
            const double dy = partyCell[1] - actorCell[1];
            const bool inLine = std::abs(dx) <= 1 && std::abs(dy) <= 1
                && (dx == 0 || !pPartyCell->walls[size_t(dx < 0 ? BreachMazeSide::East : BreachMazeSide::West)])
                && (dy == 0 || !pPartyCell->walls[size_t(dy < 0 ? BreachMazeSide::South : BreachMazeSide::North)]);
            if (inLine || (room == pPartyCell->room
                && std::abs(dx) <= 1 + projection.torchPower && std::abs(dy) <= 1 + projection.torchPower))
            {
                decision.hostilityType = 4;
                decision.setHostile = true;
            }
            else
            {
                decision.setShowAsHostile = true;
            }
        }
        projection.actors.push_back(decision);
    }
    return projection;
}
}
