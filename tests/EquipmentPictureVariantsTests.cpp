#include "game/items/EquipmentPictureVariants.h"

#include <doctest/doctest.h>

using namespace OpenYAMM::Game;

TEST_CASE("Merge armor has separate open-arm and weapon-holding pictures for every body")
{
    CHECK(equipmentPictureName("item084", "Armor", 0, false, true) == "item084v1");
    CHECK(equipmentPictureName("item084", "armor", 0, false, false) == "item084v1a");
    CHECK(equipmentPictureName("6plate5", "Armor", 4, false, true) == "6plate5v5");
    CHECK(equipmentPictureName("6plate5", "Armor", 4, false, false) == "6plate5v5a");
    CHECK(equipmentPictureName("custom", "Armor", 5, false, true) == "customv6");
}

TEST_CASE("Merge cloaks keep body-specific back and collar images for minotaurs and trolls")
{
    CHECK(equipmentPictureName("item124", "Cloak", 2, false, true) == "item124v3a");
    CHECK(equipmentPictureName("item124", "cloak", 3, false, false) == "item124v4a");
    CHECK(equipmentPictureName("item124", "Cloak", 2, false, true,
        EquipmentPicturePart::Collar) == "item124v3b");
    CHECK(equipmentPictureName("item124", "Cloak", 2, false, true,
        EquipmentPicturePart::CollarFront).empty());
    CHECK(equipmentPictureName("item126", "Cloak", 3, true, true,
        EquipmentPicturePart::CollarFront) == "item126v4c");
}

TEST_CASE("original complex flags control female hats and minotaur or troll belts")
{
    CHECK(equipmentPictureName("Hat", "Helm", 1, false, false) == "hatv1");
    CHECK(equipmentPictureName("Helm", "Helm", 1, true, false) == "helmv2");
    CHECK(equipmentPictureName("Belt", "Belt", 2, false, false) == "beltv1");
    CHECK(equipmentPictureName("Belt", "Belt", 3, false, false) == "beltv1");
    CHECK(equipmentPictureName("Belt", "Belt", 3, true, false) == "beltv4");
    CHECK(equipmentPictureName("Boots", "Boots", 4, false, false) == "bootsv5");
    CHECK(equipmentPictureName("item099", "Shield", 0, false, false) == "item099");
    CHECK(equipmentPictureName("item084", "Armor", 0, false, false,
        EquipmentPicturePart::Collar).empty());
}
