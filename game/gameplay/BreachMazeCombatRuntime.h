#pragma once

#include "game/content/BreachMazeLayout.h"

#include <span>

namespace OpenYAMM::Game
{
struct BreachMazeCombatActorFacts
{
    size_t actorIndex = 0;
    int hp = 0;
    uint32_t ally = 0;
    double x = 0.0;
    double y = 0.0;
};

struct BreachMazeCombatHostilityDecision
{
    size_t actorIndex = 0;
    uint8_t hostilityType = 3;
    bool setHostile = false;
    bool setShowAsHostile = false;
    bool restorePosition = false; // Source recovery sets XY to (300, 300), preserving Z.
};

struct BreachMazeCombatProjection
{
    int torchPower = 0;
    std::vector<BreachMazeCombatHostilityDecision> actors;
};

// Active-map state. Only actorRage is captured by the existing floor save; the frozen layout
// and timer are rebound on entry. Ordinary maps do not install this runtime.
class BreachMazeCombatRuntime
{
public:
    BreachMazeCombatRuntime(const BreachMazeLayout &layout,
        std::span<const uint8_t> actorRage, double installGameMinutes);

    void ensureActorCount(size_t actorCount);
    void notePartyDamage(size_t actorIndex);
    bool allowsPathfinding(size_t actorIndex) const;
    std::span<const uint8_t> actorRage() const;
    bool updateDue(double gameMinutes) const;
    BreachMazeCombatProjection projectHostility(double gameMinutes,
        double partyX, double partyY, int torchPower, std::span<const BreachMazeCombatActorFacts> actors);

private:
    const BreachMazeCell *cellAt(double x, double y) const;

    BreachMazeLayout m_layout;
    std::vector<uint8_t> m_actorRage;
    double m_nextUpdateGameMinutes = 0.0;
};
}
