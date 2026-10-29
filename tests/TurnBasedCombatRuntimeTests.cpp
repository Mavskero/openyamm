#include "doctest/doctest.h"

#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayActorAiSystem.h"
#include "game/gameplay/TurnBasedCombatRuntime.h"
#include "game/party/Party.h"
#include "game/maps/SaveGame.h"

#include <fstream>
#include <limits>
#include "tests/PartySpellTestHarness.h"

namespace OpenYAMM::Tests
{
namespace
{
class MutableTurnWorld : public PartySpellTestWorldRuntime
{
public:
    std::vector<std::optional<Game::GameplayRuntimeActorState>> actors;

    size_t mapActorCount() const override
    {
        return actors.size();
    }

    bool actorRuntimeState(size_t index, Game::GameplayRuntimeActorState &state) const override
    {
        if (index >= actors.size() || !actors[index])
        {
            return false;
        }
        state = *actors[index];
        return true;
    }
};

Game::GameplayRuntimeActorState nearbyHostile()
{
    Game::GameplayRuntimeActorState actor;
    actor.hostileToParty = true;
    actor.preciseX = 128.0f;
    actor.radius = 32;
    return actor;
}

bool hasQueuedActor(const Game::TurnBasedCombatRuntime &runtime, size_t id)
{
    return std::any_of(runtime.queue().begin(), runtime.queue().end(), [id](const auto &entry)
    {
        return entry.kind == Game::TurnBasedCombatRuntime::QueueEntry::Kind::Actor && entry.id == id;
    });
}

OpenYAMM::Game::Party makeTurnBasedParty(size_t memberCount = 2)
{
    OpenYAMM::Game::PartySeed seed = {};

    for (size_t memberIndex = 0; memberIndex < memberCount; ++memberIndex)
    {
        OpenYAMM::Game::Character member = {};
        member.name = "Member " + std::to_string(memberIndex + 1);
        member.health = 100;
        member.maxHealth = 100;
        seed.members.push_back(member);
    }

    OpenYAMM::Game::Party party = {};
    party.seed(seed);
    return party;
}
} // namespace

TEST_CASE("turn based queue includes all nearby hostile actor indices")
{
    Game::Party party = makeTurnBasedParty(1);
    MutableTurnWorld world;
    world.bindParty(&party);
    world.actors.assign(12, nearbyHostile());
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, &world));
    for (size_t index = 0; index < world.actors.size(); ++index)
    {
        CAPTURE(index);
        CHECK(hasQueuedActor(runtime, index));
    }
}

TEST_CASE("turn based queue removes defeated unavailable friendly and distant actors")
{
    Game::Party party = makeTurnBasedParty(1);
    MutableTurnWorld world;
    world.bindParty(&party);
    world.actors.push_back(nearbyHostile());
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, &world));
    runtime.update(&party, &world, 0.6f);
    REQUIRE(hasQueuedActor(runtime, 0));
    SUBCASE("defeated")
    {
        world.actors[0]->isDead = true;
    }
    SUBCASE("unavailable")
    {
        world.actors[0].reset();
    }
    SUBCASE("friendly")
    {
        world.actors[0]->hostileToParty = false;
    }
    SUBCASE("distant")
    {
        world.actors[0]->preciseX = 6000.0f;
    }
    runtime.update(&party, &world, 0.016f);
    CHECK_FALSE(hasQueuedActor(runtime, 0));
    CHECK(runtime.canBeginPlayerAction(party));
}

TEST_CASE("turn based queue discovers an approaching hostile at the next attack phase without duplicates")
{
    Game::Party party = makeTurnBasedParty(1);
    MutableTurnWorld world;
    world.bindParty(&party);
    world.actors.push_back(nearbyHostile());
    world.actors[0]->preciseX = 6000.0f;
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, &world));
    REQUIRE_FALSE(hasQueuedActor(runtime, 0));
    world.actors[0]->preciseX = 256.0f;
    runtime.update(&party, &world, 0.6f);
    CHECK(hasQueuedActor(runtime, 0));
    REQUIRE(runtime.applyPlayerAction(party, 0, 5.0f));
    REQUIRE(runtime.stage() == Game::TurnBasedCombatStage::Movement);
    REQUIRE(runtime.finishMovementPhase());
    runtime.update(&party, &world, 0.016f);
    runtime.update(&party, &world, 0.6f);
    CHECK(hasQueuedActor(runtime, 0));
    CHECK_EQ(std::count_if(runtime.queue().begin(), runtime.queue().end(), [](const auto &entry)
    {
        return entry.kind == Game::TurnBasedCombatRuntime::QueueEntry::Kind::Actor && entry.id == 0;
    }), 1);
}

TEST_CASE("turn based runtime enters attack after OE wait and exposes first character action")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty();
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    CHECK(runtime.active());
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Wait);
    CHECK(runtime.shouldUpdateActorAi(&party));
    CHECK_FALSE(runtime.canToggleOff(party));

    runtime.update(&party, nullptr, 0.6f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    CHECK(runtime.canBeginPlayerAction(party));
    CHECK_FALSE(runtime.shouldUpdateActorAi(&party));
    CHECK(runtime.canToggleOff(party));

    size_t memberIndex = 99;
    REQUIRE(runtime.frontQueueCharacter(memberIndex));
    CHECK_EQ(memberIndex, 0);
    CHECK_EQ(party.activeMemberIndex(), 0);
}

TEST_CASE("turn based runtime stores player action recovery outside realtime recovery")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.canBeginPlayerAction(party));

    runtime.storeMemberTurnRecovery(0, 1.0f);
    CHECK(runtime.applyPlayerAction(party, 0, 5.0f));

    const OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    CHECK_EQ(pMember->recoverySecondsRemaining, doctest::Approx(0.0f));
}

TEST_CASE("turn based runtime keeps recovering characters queued until their turn initiative is ready")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    pMember->recoverySecondsRemaining = 1.0f;

    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    CHECK(runtime.canBeginPlayerAction(party));
    CHECK_EQ(party.activeMemberIndex(), 0);
    CHECK_EQ(pMember->recoverySecondsRemaining, doctest::Approx(0.0f));
}

TEST_CASE("turn based runtime does not enter movement just because actors are ready after wait")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    pMember->recoverySecondsRemaining = 1.0f;

    OpenYAMM::Tests::PartySpellTestWorldRuntime world = {};
    world.bindParty(&party);
    world.setPartyPosition(0.0f, 0.0f, 0.0f);

    OpenYAMM::Game::GameplayRuntimeActorState actor = {};
    actor.monsterId = 1;
    actor.hostileToParty = true;
    actor.preciseX = 128.0f;
    actor.preciseY = 0.0f;
    actor.preciseZ = 0.0f;
    actor.radius = 32;
    world.addActor(actor);

    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, &world));
    runtime.update(&party, &world, 0.6f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    CHECK(runtime.canBeginPlayerAction(party));
    CHECK_EQ(runtime.movementActionPoints(), 0);
}

TEST_CASE("turn based runtime keeps actor AI live while resolving already-started monster attacks")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Tests::PartySpellTestWorldRuntime world = {};
    world.bindParty(&party);
    world.setTurnBasedActorActionInProgress(true);

    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, &world));
    runtime.update(&party, &world, 0.6f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Wait);
    CHECK(runtime.shouldUpdateActorAi(&party));
    CHECK(runtime.actorAiUpdateMode() == Game::ActorAiUpdateMode::FinishCurrentActions);
    Game::TurnBasedCombatRuntime restored;
    restored.restoreSnapshot(runtime.snapshot());
    CHECK(restored.actorAiUpdateMode() == Game::ActorAiUpdateMode::FinishCurrentActions);
    restored.end(&party);
    CHECK(restored.actorAiUpdateMode() == Game::ActorAiUpdateMode::Full);
    CHECK_EQ(world.stopTurnBasedActorMovementCount(), 0);

    world.setTurnBasedActorActionInProgress(false);
    runtime.update(&party, &world, 0.016f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    CHECK(runtime.canBeginPlayerAction(party));
    CHECK_EQ(world.stopTurnBasedActorMovementCount(), 1);
    CHECK(runtime.actorAiUpdateMode() == Game::ActorAiUpdateMode::Full);
}

TEST_CASE("turn based runtime pending actions block player actions until resolved")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.canBeginPlayerAction(party));

    runtime.registerPendingAction();
    CHECK(runtime.hasPendingActions());
    CHECK_FALSE(runtime.beginPlayerActionOrFinishMovement(party));
    CHECK_FALSE(runtime.applyPlayerAction(party, 0, 1.0f));

    runtime.resolvePendingAction();
    CHECK_FALSE(runtime.hasPendingActions());
    CHECK(runtime.beginPlayerActionOrFinishMovement(party));
}

TEST_CASE("turn based runtime spends OE movement actions only while movement is held")
{
    constexpr float MovementStepSeconds = 26.0f / 128.0f;

    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.canBeginPlayerAction(party));
    CHECK(runtime.applyPlayerAction(party, 0, 5.0f));
    runtime.update(&party, nullptr, 0.016f);
    REQUIRE(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);
    CHECK_FALSE(runtime.shouldUpdateActorAi(&party));
    CHECK_EQ(runtime.movementActionPoints(), 130);

    OpenYAMM::Game::GameplayInputFrame input = {};
    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;

    CHECK(runtime.noteMovementInput(input));
    CHECK_EQ(runtime.movementActionPoints(), 104);
    CHECK_EQ(runtime.movementDeltaSecondsForFrame(0.016f), doctest::Approx(MovementStepSeconds));
    CHECK(input.turnBasedMovementStep);

    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;
    CHECK(runtime.noteMovementInput(input));
    CHECK_EQ(runtime.movementActionPoints(), 78);
    CHECK_EQ(runtime.movementDeltaSecondsForFrame(0.016f), doctest::Approx(MovementStepSeconds));

    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = false;
    input.keyboardHeld[SDL_SCANCODE_W] = false;
    CHECK_FALSE(runtime.noteMovementInput(input));
    CHECK_EQ(runtime.movementActionPoints(), 78);
    runtime.update(&party, nullptr, 0.016f);
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);

    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;
    CHECK(runtime.noteMovementInput(input));
    CHECK_EQ(runtime.movementActionPoints(), 52);
    CHECK_EQ(runtime.movementDeltaSecondsForFrame(0.016f), doctest::Approx(MovementStepSeconds));

    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;
    CHECK(runtime.noteMovementInput(input));
    CHECK_EQ(runtime.movementActionPoints(), 26);
    CHECK_EQ(runtime.movementDeltaSecondsForFrame(0.016f), doctest::Approx(MovementStepSeconds));

    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;
    CHECK(runtime.noteMovementInput(input));
    CHECK_EQ(runtime.movementActionPoints(), 0);
    CHECK_EQ(runtime.movementDeltaSecondsForFrame(0.016f), doctest::Approx(MovementStepSeconds));

    runtime.update(&party, nullptr, 0.016f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Wait);
    CHECK_EQ(runtime.movementActionPoints(), 0);
}

TEST_CASE("turn based runtime cycles party actions before OE movement phase")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(2);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);

    size_t memberIndex = 99;
    REQUIRE(runtime.frontQueueCharacter(memberIndex));
    CHECK_EQ(memberIndex, 0);
    CHECK(runtime.applyPlayerAction(party, 0, 5.0f));
    runtime.update(&party, nullptr, 0.016f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    REQUIRE(runtime.frontQueueCharacter(memberIndex));
    CHECK_EQ(memberIndex, 1);
    CHECK_EQ(party.activeMemberIndex(), 1);

    CHECK(runtime.applyPlayerAction(party, 1, 5.0f));
    runtime.update(&party, nullptr, 0.016f);

    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);
    CHECK_EQ(runtime.movementActionPoints(), 130);

    {
        Game::Party recoveringParty = makeTurnBasedParty(2);
        Game::TurnBasedCombatRuntime recoveringRuntime;
        REQUIRE(recoveringRuntime.begin(recoveringParty, nullptr));
        recoveringRuntime.update(&recoveringParty, nullptr, 0.6f);
        Game::Character *pRecoveringMember = recoveringParty.member(0);
        REQUIRE(pRecoveringMember != nullptr);
        pRecoveringMember->health = -1;
        pRecoveringMember->conditions.set(static_cast<size_t>(Game::CharacterCondition::Unconscious));
        recoveringRuntime.update(&recoveringParty, nullptr, 0.0f);
        REQUIRE_EQ(recoveringParty.activeMemberIndex(), 1);
        const Game::TurnBasedCombatRuntime::Snapshot beforeHealing = recoveringRuntime.snapshot();

        REQUIRE(recoveringParty.healMember(0, 2));
        CHECK_FALSE(recoveringRuntime.applyPlayerAction(recoveringParty, 0, 1.0f));
        recoveringRuntime.storeMemberTurnRecovery(1, 1.0f);
        REQUIRE(recoveringRuntime.applyPlayerAction(recoveringParty, 1, 5.0f));
        CHECK(recoveringRuntime.stage() == Game::TurnBasedCombatStage::Attack);
        CHECK_EQ(recoveringRuntime.snapshot().turnCount, beforeHealing.turnCount);
        CHECK_EQ(recoveringParty.activeMemberIndex(), 0);
        CHECK_EQ(recoveringRuntime.queue().size(), 2);
        CHECK_EQ(recoveringRuntime.queue()[0].id, 0);
        CHECK_EQ(recoveringRuntime.queue()[0].initiative, beforeHealing.queue[0].initiative);
        CHECK_EQ(recoveringRuntime.queue()[1].id, 1);
        CHECK_EQ(recoveringRuntime.queue()[1].initiative, 60);
        CHECK(recoveringRuntime.applyPlayerAction(recoveringParty, 0, 5.0f));
    }

    {
        Game::Party sleepingParty = makeTurnBasedParty(2);
        Game::Character *pSleepingMember = sleepingParty.member(1);
        REQUIRE(pSleepingMember != nullptr);
        pSleepingMember->conditions.set(static_cast<size_t>(Game::CharacterCondition::Asleep));
        pSleepingMember->recoverySecondsRemaining = 1.0f;
        MutableTurnWorld world;
        world.bindParty(&sleepingParty);
        world.actors.push_back(nearbyHostile());
        Game::TurnBasedCombatRuntime wakingRuntime;
        REQUIRE(wakingRuntime.begin(sleepingParty, &world));
        wakingRuntime.update(&sleepingParty, &world, 0.6f);
        const Game::TurnBasedCombatRuntime::Snapshot beforeAwakening = wakingRuntime.snapshot();

        pSleepingMember->conditions.reset(static_cast<size_t>(Game::CharacterCondition::Asleep));
        wakingRuntime.update(&sleepingParty, &world, 0.0f);
        CHECK(wakingRuntime.stage() == beforeAwakening.stage);
        CHECK_EQ(wakingRuntime.snapshot().turnCount, beforeAwakening.turnCount);
        CHECK_EQ(wakingRuntime.snapshot().turnInitiative, beforeAwakening.turnInitiative);
        CHECK_EQ(wakingRuntime.queue().size(), beforeAwakening.queue.size() + 1);
        for (const Game::TurnBasedCombatRuntime::QueueEntry &entry : beforeAwakening.queue)
        {
            CHECK(std::find(wakingRuntime.queue().begin(), wakingRuntime.queue().end(), entry)
                != wakingRuntime.queue().end());
        }
        CHECK_EQ(sleepingParty.members()[1].recoverySecondsRemaining, doctest::Approx(1.0f));
        wakingRuntime.update(&sleepingParty, &world, 0.0f);
        CHECK_EQ(wakingRuntime.queue().size(), beforeAwakening.queue.size() + 1);

        REQUIRE(wakingRuntime.applyPlayerAction(sleepingParty, 0, 5.0f));
        REQUIRE(wakingRuntime.stage() == Game::TurnBasedCombatStage::Movement);
        CHECK_EQ(wakingRuntime.movementActionPoints(), 130);
        REQUIRE(wakingRuntime.finishMovementPhase());
        wakingRuntime.update(&sleepingParty, &world, 0.016f);
        wakingRuntime.update(&sleepingParty, &world, 0.6f);
        CHECK(wakingRuntime.stage() == Game::TurnBasedCombatStage::Attack);
        CHECK_EQ(sleepingParty.activeMemberIndex(), 1);
        CHECK_EQ(sleepingParty.members()[1].recoverySecondsRemaining, doctest::Approx(0.0f));
    }

    {
        Game::Party disabledParty = makeTurnBasedParty(2);
        Game::TurnBasedCombatRuntime disabledRuntime;
        REQUIRE(disabledRuntime.begin(disabledParty, nullptr));
        disabledRuntime.update(&disabledParty, nullptr, 0.6f);
        for (size_t index = 0; index < disabledParty.members().size(); ++index)
        {
            disabledParty.member(index)->conditions.set(static_cast<size_t>(Game::CharacterCondition::Paralyzed));
        }
        CHECK_FALSE(disabledParty.hasActableMember());
        disabledRuntime.update(&disabledParty, nullptr, 0.0f);
        CHECK(disabledRuntime.stage() == Game::TurnBasedCombatStage::Movement);
        CHECK_EQ(disabledRuntime.movementActionPoints(), 130);
        CHECK_FALSE(disabledRuntime.canBeginPlayerAction(disabledParty));
        REQUIRE(disabledRuntime.finishMovementPhase());
        disabledRuntime.update(&disabledParty, nullptr, 0.0f);
        CHECK(disabledRuntime.stage() == Game::TurnBasedCombatStage::Wait);
        disabledRuntime.update(&disabledParty, nullptr, 0.6f);
        CHECK(disabledRuntime.stage() == Game::TurnBasedCombatStage::Movement);
        CHECK_EQ(disabledRuntime.queue().size(), 2);
        CHECK(Game::TurnBasedCombatRuntime::isValidSnapshot(disabledRuntime.snapshot()));

        disabledParty.member(0)->conditions.reset(static_cast<size_t>(Game::CharacterCondition::Paralyzed));
        REQUIRE(disabledRuntime.finishMovementPhase());
        disabledRuntime.update(&disabledParty, nullptr, 0.0f);
        disabledRuntime.update(&disabledParty, nullptr, 0.6f);
        CHECK(disabledRuntime.stage() == Game::TurnBasedCombatStage::Attack);
        CHECK(disabledRuntime.canBeginPlayerAction(disabledParty));
        CHECK_EQ(disabledParty.activeMemberIndex(), 0);
    }
}

TEST_CASE("turn based runtime advances OE game time once when a combat turn reaches movement")
{
    constexpr float ExpectedTurnGameMinutes = 213.0f / 256.0f;

    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    CHECK_EQ(runtime.consumePendingGameTimeAdvanceMinutes(), doctest::Approx(0.0f));

    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.canBeginPlayerAction(party));
    CHECK_EQ(runtime.consumePendingGameTimeAdvanceMinutes(), doctest::Approx(0.0f));

    REQUIRE(runtime.applyPlayerAction(party, 0, 5.0f));
    REQUIRE(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);
    CHECK_EQ(runtime.consumePendingGameTimeAdvanceMinutes(), doctest::Approx(ExpectedTurnGameMinutes));
    CHECK_EQ(runtime.consumePendingGameTimeAdvanceMinutes(), doctest::Approx(0.0f));

    runtime.update(&party, nullptr, 0.5f);
    CHECK_EQ(runtime.consumePendingGameTimeAdvanceMinutes(), doctest::Approx(0.0f));
}

TEST_CASE("turn based runtime yields to movement when actors become ready before character recovery")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Tests::PartySpellTestWorldRuntime world = {};
    world.bindParty(&party);
    world.setPartyPosition(0.0f, 0.0f, 0.0f);

    OpenYAMM::Game::GameplayRuntimeActorState actor = {};
    actor.monsterId = 1;
    actor.hostileToParty = true;
    actor.preciseX = 128.0f;
    actor.preciseY = 0.0f;
    actor.preciseZ = 0.0f;
    actor.radius = 32;
    world.addActor(actor);

    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};
    REQUIRE(runtime.begin(party, &world));
    runtime.update(&party, &world, 0.6f);
    REQUIRE(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    REQUIRE(runtime.canBeginPlayerAction(party));

    CHECK(runtime.applyPlayerAction(party, 0, 5.0f));
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);
    CHECK_EQ(runtime.movementActionPoints(), 130);
}

TEST_CASE("turn based runtime blocks movement input until OE movement phase")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));

    OpenYAMM::Game::GameplayInputFrame input = {};
    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;

    CHECK_FALSE(runtime.noteMovementInput(input));
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Wait);

    input.actions[OpenYAMM::Game::keyboardActionIndex(OpenYAMM::Game::KeyboardAction::Forward)].held = true;
    input.keyboardHeld[SDL_SCANCODE_W] = true;
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);

    CHECK_FALSE(runtime.noteMovementInput(input));
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Attack);
    CHECK_EQ(runtime.movementActionPoints(), 0);
    CHECK_FALSE(input.turnBasedMovementStep);
}

TEST_CASE("turn based runtime keeps movement phase open until explicit movement finish")
{
    OpenYAMM::Game::Party party = makeTurnBasedParty(1);
    OpenYAMM::Game::TurnBasedCombatRuntime runtime = {};

    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.canBeginPlayerAction(party));
    CHECK(runtime.applyPlayerAction(party, 0, 5.0f));
    runtime.update(&party, nullptr, 0.016f);
    REQUIRE(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);

    CHECK_FALSE(runtime.canBeginPlayerAction(party));
    runtime.update(&party, nullptr, 0.5f);
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Movement);
    CHECK_EQ(runtime.movementActionPoints(), 130);

    CHECK(runtime.finishMovementPhase());
    runtime.update(&party, nullptr, 0.016f);
    CHECK(runtime.stage() == OpenYAMM::Game::TurnBasedCombatStage::Wait);
}
} // namespace OpenYAMM::Tests

namespace OpenYAMM::Tests
{
TEST_CASE("turn save resumes remaining wait and recovery without restarting the round")
{
    Game::Party party = makeTurnBasedParty(2);
    Game::TurnBasedCombatRuntime uninterrupted;
    REQUIRE(uninterrupted.begin(party, nullptr));
    uninterrupted.update(&party, nullptr, 0.2f);
    Game::TurnBasedCombatRuntime restored;
    restored.restoreSnapshot(uninterrupted.snapshot());
    CHECK(restored.snapshot() == uninterrupted.snapshot());
    Game::Party restoredParty = party;
    for (int frame = 0; frame < 40; ++frame)
    {
        uninterrupted.update(&party, nullptr, 0.025f);
        restored.update(&restoredParty, nullptr, 0.025f);
        REQUIRE(restored.snapshot() == uninterrupted.snapshot());
        if (uninterrupted.canBeginPlayerAction(party))
        {
            size_t member = 0;
            REQUIRE(uninterrupted.frontQueueCharacter(member));
            uninterrupted.storeMemberTurnRecovery(member, 3.25f);
            restored.storeMemberTurnRecovery(member, 3.25f);
            CHECK(uninterrupted.applyPlayerAction(party, member, 1.0f)
                == restored.applyPlayerAction(restoredParty, member, 1.0f));
        }
    }
    CHECK(restored.snapshot() == uninterrupted.snapshot());
    CHECK(restored.consumePendingGameTimeAdvanceMinutes() == uninterrupted.consumePendingGameTimeAdvanceMinutes());
    restored.end(&restoredParty);
    uninterrupted.end(&party);
    CHECK(restoredParty.members()[0].recoverySecondsRemaining == party.members()[0].recoverySecondsRemaining);
    CHECK(restoredParty.members()[1].recoverySecondsRemaining == party.members()[1].recoverySecondsRemaining);
}

TEST_CASE("turn save preserves spent movement and resets only previous input latches")
{
    Game::Party party = makeTurnBasedParty(1);
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.applyPlayerAction(party, 0, 5.0f));
    REQUIRE(runtime.stage() == Game::TurnBasedCombatStage::Movement);
    Game::GameplayInputFrame input;
    input.actions[Game::keyboardActionIndex(Game::KeyboardAction::Forward)].held = true;
    input.actions[Game::keyboardActionIndex(Game::KeyboardAction::Forward)].pressed = true;
    REQUIRE(runtime.noteMovementInput(input));
    const int spent = runtime.movementActionPoints();
    REQUIRE(spent == 104);
    Game::TurnBasedCombatRuntime restored;
    restored.restoreSnapshot(runtime.snapshot());
    CHECK(restored.movementActionPoints() == spent);
    CHECK(restored.movementDeltaSecondsForFrame(0.016f) == 0.0f);
    CHECK(restored.consumePendingGameTimeAdvanceMinutes() == runtime.consumePendingGameTimeAdvanceMinutes());
    CHECK(restored.consumePendingGameTimeAdvanceMinutes() == 0.0f);
    REQUIRE(restored.noteMovementInput(input));
    CHECK(restored.movementActionPoints() == 78);
}

TEST_CASE("turn save pending projectile blocks actions until its one resolution")
{
    Game::Party party = makeTurnBasedParty(1);
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.6f);
    REQUIRE(runtime.playerActionReady(party));
    runtime.registerPendingAction();
    Game::TurnBasedCombatRuntime restored;
    restored.restoreSnapshot(runtime.snapshot());
    CHECK_FALSE(restored.playerActionReady(party));
    CHECK_FALSE(restored.applyPlayerAction(party, 0, 1.0f));
    restored.update(&party, nullptr, 1.0f);
    CHECK(restored.pendingActions() == 1);
    restored.resolvePendingAction();
    CHECK(restored.playerActionReady(party));
    restored.resolvePendingAction();
    CHECK(restored.pendingActions() == 0);
}

TEST_CASE("turn save binary retains indoor and outdoor projectile pending marker")
{
    Game::Party party = makeTurnBasedParty(1);
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, nullptr));
    runtime.update(&party, nullptr, 0.2f);
    runtime.registerPendingAction();
    Game::GameSaveData data;
    data.party = party.snapshot();
    data.mapFileName = "turn_save_fixture";
    data.turnBasedCombat = runtime.snapshot();
    Game::GameplayProjectileService::ProjectileState projectile;
    projectile.projectileId = 42;
    projectile.turnBasedPendingAction = true;
    projectile.spellId = 2;
    projectile.x = 12.75f;
    projectile.velocityX = 512.0f;
    SUBCASE("outdoor")
    {
        data.hasOutdoorRuntimeState = true;
        data.outdoorWorld.projectiles.push_back(projectile);
    }
    SUBCASE("indoor")
    {
        data.currentSceneKind = Game::SceneKind::Indoor;
        data.hasIndoorSceneState = true;
        data.indoorScene.worldRuntime.projectileState.projectiles.push_back(projectile);
    }
    const auto path = std::filesystem::temp_directory_path() / "openyamm_turn_snapshot_roundtrip.oysav";
    std::string error;
    REQUIRE_MESSAGE(Game::saveGameDataToPath(path, data, error), error);
    const auto loaded = Game::loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK(loaded->turnBasedCombat == data.turnBasedCombat);
    const auto &actual = data.currentSceneKind == Game::SceneKind::Outdoor ? loaded->outdoorWorld.projectiles
        : loaded->indoorScene.worldRuntime.projectileState.projectiles;
    REQUIRE(actual.size() == 1);
    CHECK(actual[0].turnBasedPendingAction);
    CHECK(actual[0].projectileId == 42);
    CHECK(actual[0].x == 12.75f);
    CHECK(actual[0].velocityX == 512.0f);
    std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);
    CHECK_FALSE(Game::loadGameDataFromPath(path, error));
    std::filesystem::remove(path);
}

TEST_CASE("turn save rejects inconsistent state before replacing a valid save")
{
    Game::Party party = makeTurnBasedParty(1);
    Game::TurnBasedCombatRuntime runtime;
    REQUIRE(runtime.begin(party, nullptr));
    Game::GameSaveData data;
    data.party = party.snapshot();
    data.hasOutdoorRuntimeState = true;
    data.turnBasedCombat = runtime.snapshot();
    const auto path = std::filesystem::temp_directory_path() / "openyamm_turn_invalid_state.oysav";
    std::string error;
    REQUIRE(Game::saveGameDataToPath(path, data, error));
    const auto original = data.turnBasedCombat;
    SUBCASE("invalid stage") { data.turnBasedCombat.stage = static_cast<Game::TurnBasedCombatStage>(255); }
    SUBCASE("nonfinite timer") { data.turnBasedCombat.aiTurnTimerTicks = std::numeric_limits<float>::infinity(); }
    SUBCASE("negative pending") { data.turnBasedCombat.pendingActions = -1; }
    SUBCASE("missing pending projectile") { data.turnBasedCombat.pendingActions = 1; }
    SUBCASE("duplicate combatant") { data.turnBasedCombat.queue.push_back(data.turnBasedCombat.queue.front()); }
    SUBCASE("missing combatant") { data.turnBasedCombat.queue.front().id = 1; }
    SUBCASE("invalid movement points") { data.turnBasedCombat.movementActionPoints = 131; }
    SUBCASE("nonfinite member recovery")
    {
        data.turnBasedCombat.memberTurnRecoverySeconds[0] = std::numeric_limits<float>::quiet_NaN();
    }
    CHECK_FALSE(Game::saveGameDataToPath(path, data, error));
    const auto preserved = Game::loadGameDataFromPath(path, error);
    REQUIRE(preserved.has_value());
    CHECK(preserved->turnBasedCombat == original);
    std::filesystem::remove(path);
}
}

namespace OpenYAMM::Tests
{
namespace
{
Game::ActorAiFacts resolutionActor(size_t index)
{
    Game::ActorAiFacts actor;
    actor.actorIndex = index;
    actor.actorId = uint32_t(index + 1);
    actor.identity.actorId = actor.actorId;
    actor.identity.hostilityType = 4;
    actor.world.active = true;
    actor.status.hostileToParty = true;
    actor.status.hasDetectedParty = true;
    actor.stats.currentHp = 20;
    actor.stats.maxHp = 20;
    actor.stats.radius = 32;
    actor.stats.height = 128;
    actor.stats.moveSpeed = 200;
    actor.stats.attack1Damage.bonus = 3;
    actor.movement.position = {100.0f, 0.0f, 0.0f};
    actor.movement.movementAllowed = true;
    actor.movement.effectiveMoveSpeed = 200.0f;
    actor.target.currentKind = Game::ActorAiTargetKind::Party;
    actor.target.currentPosition = {0.0f, 0.0f, 64.0f};
    actor.target.currentDistance = 100.0f;
    actor.target.currentEdgeDistance = 36.0f;
    actor.target.currentCanSense = true;
    actor.target.partyCanSenseActor = true;
    return actor;
}

Game::ActorAiFrameFacts resolutionFrame()
{
    Game::ActorAiFrameFacts frame;
    frame.updateMode = Game::ActorAiUpdateMode::FinishCurrentActions;
    frame.deltaSeconds = 0.125f;
    frame.fixedStepSeconds = 0.125f;
    frame.party.collisionRadius = 32.0f;
    frame.party.collisionHeight = 128.0f;
    return frame;
}
}

TEST_CASE("turn based actor resolution prevents new attacks and pursuit")
{
    Game::ActorAiFrameFacts frame = resolutionFrame();
    Game::ActorAiFacts actor = resolutionActor(0);
    SUBCASE("ready melee attacker") {}
    SUBCASE("ready ranged attacker")
    {
        actor.stats.attackConstraints.attack1IsRanged = true;
    }
    SUBCASE("pursuing distant enemy")
    {
        actor.runtime.motionState = Game::ActorAiMotionState::Pursuing;
        actor.runtime.animationState = Game::ActorAiAnimationState::Walking;
        actor.target.currentDistance = 1500.0f;
        actor.target.currentEdgeDistance = 1400.0f;
    }
    SUBCASE("previous impact already delivered")
    {
        actor.runtime.motionState = Game::ActorAiMotionState::Attacking;
        actor.runtime.attackImpactTriggered = true;
    }
    frame.activeActors.push_back(actor);
    const Game::ActorAiFrameResult result = Game::GameplayActorAiSystem().updateActors(frame);
    REQUIRE_EQ(result.actorUpdates.size(), 1);
    const Game::ActorAiUpdate &update = result.actorUpdates.front();
    CHECK(update.state.motionState.value_or(actor.runtime.motionState) == Game::ActorAiMotionState::Standing);
    CHECK_FALSE(update.attackRequest.has_value());
    CHECK_FALSE(update.movementIntent.applyMovement);
    CHECK_EQ(update.state.attackDecisionCount.value_or(actor.runtime.attackDecisionCount), 0);
    CHECK(result.projectileRequests.empty());
    CHECK(result.audioRequests.empty());
}

TEST_CASE("turn based actor resolution completes existing melee and ranged impacts once")
{
    Game::ActorAiFrameFacts frame = resolutionFrame();
    Game::ActorAiFacts actor = resolutionActor(0);
    actor.runtime.motionState = Game::ActorAiMotionState::Attacking;
    actor.runtime.animationState = Game::ActorAiAnimationState::AttackMelee;
    actor.runtime.actionSeconds = 0.1f;
    SUBCASE("melee") {}
    SUBCASE("ranged")
    {
        actor.stats.attackConstraints.attack1IsRanged = true;
        actor.runtime.animationState = Game::ActorAiAnimationState::AttackRanged;
    }
    frame.activeActors = {actor, resolutionActor(1)};
    const Game::GameplayActorAiSystem system;
    const Game::ActorAiFrameResult result = system.updateActors(frame);
    REQUIRE_EQ(result.actorUpdates.size(), 2);
    const Game::ActorAiUpdate &update = result.actorUpdates[0];
    REQUIRE(update.attackRequest.has_value());
    CHECK_EQ(update.attackRequest->damage, 3);
    CHECK(update.state.attackImpactTriggered.value_or(false));
    CHECK_EQ(result.projectileRequests.size(), actor.stats.attackConstraints.attack1IsRanged ? 1 : 0);
    CHECK(result.actorUpdates[1].state.motionState.value_or(Game::ActorAiMotionState::Standing)
        == Game::ActorAiMotionState::Standing);
    CHECK_FALSE(result.actorUpdates[1].attackRequest.has_value());
    actor.runtime.motionState = update.state.motionState.value_or(actor.runtime.motionState);
    actor.runtime.actionSeconds = update.state.actionSeconds.value_or(actor.runtime.actionSeconds);
    actor.runtime.attackImpactTriggered = update.state.attackImpactTriggered.value_or(false);
    frame.activeActors[0] = actor;
    const Game::ActorAiFrameResult repeated = system.updateActors(frame);
    for (const Game::ActorAiUpdate &next : repeated.actorUpdates)
    {
        CHECK_FALSE(next.attackRequest.has_value());
        CHECK(next.state.motionState.value_or(Game::ActorAiMotionState::Standing)
            == Game::ActorAiMotionState::Standing);
    }
    CHECK(repeated.projectileRequests.empty());
}

TEST_CASE("turn based actor resolution advances unfinished attacks without early damage")
{
    Game::ActorAiFrameFacts frame = resolutionFrame();
    Game::ActorAiFacts actor = resolutionActor(0);
    actor.runtime.motionState = Game::ActorAiMotionState::Attacking;
    actor.runtime.actionSeconds = 0.5f;
    frame.activeActors.push_back(actor);
    const Game::ActorAiFrameResult result = Game::GameplayActorAiSystem().updateActors(frame);
    const Game::ActorAiUpdate &update = result.actorUpdates.front();
    CHECK(update.state.motionState.value_or(actor.runtime.motionState) == Game::ActorAiMotionState::Attacking);
    CHECK_EQ(update.state.actionSeconds.value_or(-1.0f), doctest::Approx(0.375f));
    CHECK_FALSE(update.attackRequest.has_value());
}

TEST_CASE("turn based actor resolution preserves death and paralysis cancellation")
{
    Game::ActorAiFrameFacts frame = resolutionFrame();
    Game::ActorAiFacts actor = resolutionActor(0);
    actor.runtime.motionState = Game::ActorAiMotionState::Attacking;
    actor.runtime.actionSeconds = 0.1f;
    SUBCASE("killed")
    {
        actor.stats.currentHp = 0;
    }
    SUBCASE("paralyzed")
    {
        actor.status.spellEffects.paralyzeRemainingSeconds = 2.0f;
    }
    frame.activeActors.push_back(actor);
    const Game::ActorAiFrameResult result = Game::GameplayActorAiSystem().updateActors(frame);
    const Game::ActorAiUpdate &update = result.actorUpdates.front();
    CHECK(update.state.motionState.value_or(actor.runtime.motionState) != Game::ActorAiMotionState::Attacking);
    CHECK_FALSE(update.attackRequest.has_value());
}
}

namespace OpenYAMM::Tests
{
TEST_CASE("ordinary actor updates still start ready attacks after resolution ends")
{
    Game::ActorAiFrameFacts frame = resolutionFrame();
    frame.updateMode = Game::ActorAiUpdateMode::Full;
    Game::ActorAiFacts actor = resolutionActor(0);
    SUBCASE("melee") {}
    SUBCASE("ranged")
    {
        actor.stats.attackConstraints.attack1IsRanged = true;
    }
    frame.activeActors.push_back(actor);
    const Game::ActorAiFrameResult result = Game::GameplayActorAiSystem().updateActors(frame);
    REQUIRE_EQ(result.actorUpdates.size(), 1);
    CHECK(result.actorUpdates.front().state.motionState.value_or(actor.runtime.motionState)
        == Game::ActorAiMotionState::Attacking);
    CHECK_FALSE(result.actorUpdates.front().attackRequest.has_value());
    CHECK_FALSE(result.audioRequests.empty());
}
}
