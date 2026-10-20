#pragma once

#include "game/content/BreachMazeFloorDescriptor.h"
#include "game/content/BreachMazePopulation.h"
#include "game/gameplay/BreachMazeInteractionRuntime.h"
#include "game/scene/IndoorSceneRuntime.h"

#include <limits>
#include <map>
#include <memory>

namespace OpenYAMM::Game
{
inline constexpr uint32_t BreachMazeStateVersion = 1; // Frozen foundation fixture protocol, never a playable save.
inline constexpr uint32_t BreachMazeLegacyPlayableStateVersion = 2; // Save94, before live interactions.
inline constexpr uint32_t BreachMazePlayableStateVersion = 3;
struct BreachMazeFloorRuntime
{
    // The sole authority for mutable delta, AI, movement, blood, loot, mechanisms and timers.
    // EventRuntime's global view is not authority for Party/session globals. On a real floor change
    // the controller rebinds current session globals AND the global game clock, rather than rolling
    // time back to this floor's old worldRuntime.gameMinutes. There is no Party in this record.
    IndoorSceneRuntime::Snapshot scene;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;

    // Version 1 foundation fixture only. Playable versions require this empty: combatState owns all caps.
    std::vector<uint32_t> actorMaximumHitPoints;
    std::vector<uint8_t> actorRage;
    // Required in playable v3. Only explicit Save94 migration may initialize a missing legacy record.
    std::optional<BreachMazeInteractionState> interaction;
};

struct BreachMazeFloorState
{
    BreachMazeFloorDescriptor descriptor;
    BreachMazeFloorRuntime runtime;
    // Required in playable versions; forbidden in version 1 foundation fixtures. Never rerolled on restore.
    std::optional<BreachMazePopulationPlan> population;
};

enum class BreachMazeDirection : uint8_t
{
    Fresh,
    Up,
    Down,
    Exited,
    Escaped, // Playable v3 portal departure; retains the actual current floor and grants no stair-exit effects.
};

struct BreachMazeRunData
{
    uint32_t version = BreachMazeStateVersion;
    uint32_t seedVersion = BreachMazeSeedVersion;
    uint64_t templateFingerprint = 0;
    uint64_t revision = 0;
    uint64_t rootSeed = 0;
    uint64_t epoch = 0;
    uint64_t nextGenerationSerial = 1;
    uint32_t currentLevel = 0;
    bool inside = false;
    BreachMazeDirection direction = BreachMazeDirection::Fresh;
    std::map<uint32_t, BreachMazeFloorState> floors;
};

// Data-only save boundary for current playable v3. Authenticated template/catalog checks still belong to
// BreachMazeLifecycle::restore and the geometry restore builder before scene publication.
bool validateBreachMazePlayableData(const BreachMazeRunData &data, std::string &error);
// Save94's v2 never executed live interactions. Initialize only those missing records from each
// frozen population once; scene, actors, quest state, clocks, seeds and lifecycle counters stay intact.
// Failure leaves data untouched. Save95/v3 must never use this to repair a missing/corrupt record.
bool migrateBreachMazeV2Interactions(BreachMazeRunData &data, std::string &error);

class BreachMazeLifecycle;

// Immutable externally. Copies share a const record; successful commits replace the whole record.
// Serialization may copy data(), but must return through BreachMazeLifecycle::restore for validation.
class BreachMazeState
{
public:
    BreachMazeState(); // Explicitly retains the version 1 foundation fixture default.
    static BreachMazeState playable();
    const BreachMazeRunData &data() const;
    const BreachMazeFloorState *activeFloor() const;

private:
    friend class BreachMazeLifecycle;
    explicit BreachMazeState(std::shared_ptr<const BreachMazeRunData> data);
    std::shared_ptr<const BreachMazeRunData> m_data;
};
}
