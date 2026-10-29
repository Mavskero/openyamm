#pragma once

#include "game/events/EventRuntime.h"
#include "game/gameplay/PartyMovementStatus.h"
#include "game/outdoor/OutdoorMovementDriver.h"
#include "game/party/Party.h"

#include <optional>
#include <string>

namespace OpenYAMM::Game
{
class ItemTable;
struct GameplayWorldMovementFrameDiagnostics;

class OutdoorPartyRuntime
{
public:
    struct Snapshot
    {
        OutdoorMoveState movementState = {};
        OutdoorPartyMovementState partyMovementState = {};
        float movementAccumulatorSeconds = 0.0f;
        PartyJumpState jumpState;
    };

    OutdoorPartyRuntime(OutdoorMovementDriver movementDriver, const ItemTable &itemTable);

    void initialize(float x, float y, float footZHint, bool resetParty = true);
    // Explicit position: preserve altitude above support and resolve nearby platforms within the normal step height.
    void teleportTo(float x, float y, float footZHint);
    void setPositionForEvents(float x, float y, float footZ);
    void setDecorationFlags(const std::vector<uint16_t> &decorationFlags);
    void update(
        const OutdoorMovementInput &input,
        float deltaSeconds,
        GameplayWorldMovementFrameDiagnostics *pPerformanceDiagnostics = nullptr);
    void setActorColliders(const std::vector<OutdoorActorCollision> &actorColliders);
    void setFaceAttributes(size_t bModelIndex, size_t faceIndex, uint32_t attributes);
    void updateFaceGeometries(const std::vector<OutdoorFaceGeometryData> &geometries);
    bool translateWithSupportedBModel(size_t bModelIndex, float deltaX, float deltaY, float deltaZ);
    void applyEventRuntimeState(const EventRuntimeState &runtimeState, bool grantItemsToInventory = true);

    const OutdoorMoveState &movementState() const;
    const OutdoorMovementEvents &movementEvents() const;
    const OutdoorMovementConsequences &movementConsequences() const;
    const OutdoorPartyMovementState &partyMovementState() const;
    const Party &party() const;
    Party &party();
    PartyMovementStatus movementStatus() const;
    const std::string &movementStatusText() const;
    float partyX() const;
    float partyY() const;
    float partyFootZ() const;
    void setParty(const Party &party);
    Snapshot snapshot() const noexcept;
    void restoreSnapshot(const Snapshot &snapshot);

    void toggleRunning();
    void toggleFlying();
    void toggleWaterWalk();
    void toggleFeatherFall();
    void setRunning(bool active);
    void setDebugFlyingOverride(bool active);
    void setUnderwater(bool active);
    void setMovementSpeedMultiplier(float multiplier);
    void setBodyDimensions(float radius, float height, float maxStepHeight = 128.0f);
    void setCollisionTraceEnabled(bool enabled, const std::string &mapName);
    void syncSpellMovementStatesFromPartyBuffs() noexcept;
    void requestJump(std::optional<float> verticalVelocity = std::nullopt, float lift = 1.0f);
    void requestSpecialJump(float velocityX, float velocityY, float velocityZ);

private:
    OutdoorMovementDriver m_movementDriver;
    Party m_party;
    std::string m_movementStatusText;
    PartyMovementStatus m_movementStatus = PartyMovementStatus::None;
    bool m_debugFlyingOverride = false;
};
}
