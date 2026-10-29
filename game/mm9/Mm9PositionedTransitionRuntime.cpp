#include "game/mm9/Mm9PositionedTransitionRuntime.h"

#include <algorithm>
#include <cmath>

namespace OpenYAMM::Game
{
namespace
{
bool pointInside(const Mm9MapTransition &transition, float x, float y, float z)
{
    return std::abs(x - static_cast<float>(transition.centerX)) <= static_cast<float>(transition.halfExtentX)
        && std::abs(y - static_cast<float>(transition.centerY)) <= static_cast<float>(transition.halfExtentY)
        && std::abs(z - static_cast<float>(transition.centerZ)) <= static_cast<float>(transition.halfExtentZ);
}

EventRuntimeState::PendingMapMove buildPendingMove(const Mm9MapTransition &transition)
{
    EventRuntimeState::PendingMapMove move = {};
    move.x = transition.arrivalX;
    move.y = transition.arrivalY;
    move.z = transition.arrivalZ;
    move.mapName = transition.destinationMapFileName;
    move.directionDegrees = static_cast<int32_t>(std::lround(transition.facingDegrees)) % 360;
    move.useMapStartPosition = false;
    move.useFullscreenLoading = !transition.loadScreen.empty();
    move.travelPlan.mode = EventRuntimeState::TravelPlanMode::TravelDays;
    move.travelPlan.travelDays = std::max(0, transition.travelDays);
    move.travelPlan.foodRequired = std::max(0, transition.travelDays);
    move.traceSourceKind = "mm9_exit_trigger";
    move.traceSourceId = transition.sourceObjectIndex;
    move.traceDestinationName = transition.destinationMapFileName;
    return move;
}

EventRuntimeState::MapTransitionTrace buildTrace(
    const Mm9MapTransition &transition,
    const EventRuntimeState::PendingMapMove &move)
{
    EventRuntimeState::MapTransitionTrace trace = {};
    trace.sourceKind = "mm9_exit_trigger";
    trace.sourceId = transition.sourceObjectIndex;
    trace.confirmationRequired = transition.askPlayer;
    trace.destinationMap = transition.destinationMapFileName;
    trace.destinationName = transition.destinationMapFileName;
    trace.travelDays = static_cast<uint32_t>(std::max(0, transition.travelDays));
    trace.x = move.x;
    trace.y = move.y;
    trace.z = move.z;
    trace.directionDegrees = move.directionDegrees;
    return trace;
}
}

void Mm9PositionedTransitionRuntime::configure(const std::vector<const Mm9MapTransition *> &transitions)
{
    clear();
    m_gates.reserve(transitions.size());
    for (const Mm9MapTransition *pTransition : transitions)
    {
        if (pTransition != nullptr)
        {
            m_gates.push_back({.pTransition = pTransition});
        }
    }
}

void Mm9PositionedTransitionRuntime::clear()
{
    m_gates.clear();
    m_initialized = false;
}

void Mm9PositionedTransitionRuntime::resetOverlapState()
{
    for (Gate &gate : m_gates)
    {
        gate.inside = false;
    }
    m_initialized = false;
}

bool Mm9PositionedTransitionRuntime::empty() const
{
    return m_gates.empty();
}

std::optional<Mm9PositionedTransitionActivation> Mm9PositionedTransitionRuntime::update(
    float partyX,
    float partyY,
    float partyZ,
    EventRuntimeState &eventRuntimeState)
{
    const bool canActivate = m_initialized
        && !eventRuntimeState.pendingDialogueContext.has_value()
        && !eventRuntimeState.pendingMapMove.has_value();
    const Mm9MapTransition *pActivatedTransition = nullptr;
    for (Gate &gate : m_gates)
    {
        const bool inside = pointInside(*gate.pTransition, partyX, partyY, partyZ);
        if (canActivate && !gate.inside && inside && pActivatedTransition == nullptr)
        {
            pActivatedTransition = gate.pTransition;
        }
        gate.inside = inside;
    }
    m_initialized = true;
    if (pActivatedTransition == nullptr)
    {
        return std::nullopt;
    }

    Mm9PositionedTransitionActivation activation = {};
    activation.pTransition = pActivatedTransition;
    activation.move = buildPendingMove(*pActivatedTransition);
    eventRuntimeState.lastMapTransitionRequested = buildTrace(*pActivatedTransition, activation.move);
    if (pActivatedTransition->askPlayer)
    {
        EventRuntimeState::PendingDialogueContext context = {};
        context.kind = DialogueContextKind::MapTransition;
        context.sourceId = pActivatedTransition->sourceObjectIndex;
        context.transitionMapMove = activation.move;
        eventRuntimeState.pendingDialogueContext = std::move(context);
    }
    else
    {
        eventRuntimeState.pendingMapMove = activation.move;
    }
    return activation;
}
}
