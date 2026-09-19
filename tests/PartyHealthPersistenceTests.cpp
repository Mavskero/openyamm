#include "doctest/doctest.h"
#include "game/party/Party.h"
#include "game/gameplay/GameMechanics.h"

using namespace OpenYAMM::Game;

namespace
{
Party injuredParty(int negativeHealth)
{
    PartySeed seed;
    Character member;
    member.name = "Original injured knight";
    member.className = "Knight";
    member.role = "Knight";
    member.endurance = 14;
    member.health = 30;
    member.maxHealth = 30;
    seed.members.push_back(member);
    Party party;
    party.seed(seed);
    REQUIRE(party.applyDamageToMember(0, party.members()[0].health - negativeHealth, ""));
    REQUIRE(party.members()[0].health == negativeHealth);
    return party;
}
}

TEST_CASE("negative health and incapacitation survive party restore and derived refresh")
{
    for (const int health : {-1, -6, -20})
    {
        INFO("saved health=" << health);
        Party original = injuredParty(health);
        const Party::Snapshot snapshot = original.snapshot();
        Party restored;
        restored.restoreSnapshot(snapshot);
        CHECK(restored.members()[0].health == health);
        CHECK(restored.members()[0].conditions == snapshot.members[0].conditions);
        CHECK(restored.members()[0].conditionStartGameMinutes == snapshot.members[0].conditionStartGameMinutes);
        GameMechanics::refreshCharacterBaseResources(*restored.member(0), false);
        CHECK(restored.members()[0].health == health);
        restored.refreshDerivedState();
        restored.refreshDerivedState();
        CHECK(restored.members()[0].health == health);
        CHECK(restored.members()[0].conditions == snapshot.members[0].conditions);
    }
}

TEST_CASE("insufficient healing reduces negative health by exactly its amount")
{
    Party party = injuredParty(-6);
    REQUIRE(party.healMember(0, 2));
    CHECK(party.members()[0].health == -4);
    CHECK(party.members()[0].conditions.test(size_t(CharacterCondition::Unconscious)));
    REQUIRE(party.healMember(0, 4));
    CHECK(party.members()[0].health == 0);
    CHECK(party.members()[0].conditions.test(size_t(CharacterCondition::Unconscious)));
    REQUIRE(party.healMember(0, 1));
    CHECK(party.members()[0].health == 1);
    CHECK_FALSE(party.members()[0].conditions.test(size_t(CharacterCondition::Unconscious)));
}

TEST_CASE("healing still caps health and ignores nonpositive amounts")
{
    Party party = injuredParty(-1);
    CHECK_FALSE(party.healMember(0, 0));
    CHECK_FALSE(party.healMember(0, -5));
    CHECK(party.members()[0].health == -1);
    REQUIRE(party.healMember(0, 1000));
    CHECK(party.members()[0].health == Party::effectiveMaximumHealth(party.members()[0]));
    CHECK_FALSE(party.healMember(0, 1000));
    party.refreshDerivedState();
    CHECK(party.members()[0].health == Party::effectiveMaximumHealth(party.members()[0]));
}
