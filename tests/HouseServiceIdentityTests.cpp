#include "doctest/doctest.h"
#include "game/tables/HouseTable.h"

using OpenYAMM::Game::HouseEntry;

TEST_CASE("adventurers inn role is independent of translated display name and house id")
{
    HouseEntry inn;
    inn.id = 1607;
    inn.type = "The Adventurer's Inn";
    inn.name = "冒险者的旅店";
    CHECK(inn.isAdventurersInn());
    inn.name = "Auberge des aventuriers";
    inn.id = 29001;
    CHECK(inn.isAdventurersInn());
    inn.name.clear();
    CHECK(inn.isAdventurersInn());
}

TEST_CASE("historical English house representation retains its adventurers inn role")
{
    HouseEntry inn;
    inn.id = 756;
    inn.type = "House";
    inn.name = "The Adventurer's Inn";
    CHECK(inn.isAdventurersInn());
}

TEST_CASE("ordinary named establishments do not acquire the adventurers inn service")
{
    HouseEntry house;
    house.name = "The Adventurer's Inn";
    for (const char *pType : {"Tavern", "Weapon Shop", "Elemental Guild", ""})
    {
        house.type = pType;
        CHECK_FALSE(house.isAdventurersInn());
    }
    house.type = "House";
    house.name = "普通民居";
    CHECK_FALSE(house.isAdventurersInn());
}
