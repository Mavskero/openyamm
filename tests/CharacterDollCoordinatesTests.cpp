#include "game/tables/CharacterDollTable.h"

#include <doctest/doctest.h>

namespace
{
using Rows = std::vector<std::vector<std::string>>;

Rows originalPortraits()
{
    Rows rows(2, std::vector<std::string>(27, "0"));
    rows[0][1] = "Paperdoll type";
    rows[0][8] = "Helm X";
    rows[0][9] = "Helm Y";
    rows[1][6] = "30";
    rows[1][7] = "33";
    rows[1][8] = "-2";
    rows[1][9] = "1";
    return rows;
}

Rows originalDollTypes()
{
    Rows rows(2, std::vector<std::string>(28, "0"));
    rows[0][14] = "LHo X";
    rows[0][16] = "LHc X";
    // Original MM8 male layout: screen coordinates relative to the paperdoll origin.
    const int coordinates[] = {29, 110, 27, 117, 27, 118, 132, 150, 138, 150,
        138, 150, 0, 0, 3, 10, 112, 100, 12, 44};
    for (size_t index = 0; index < std::size(coordinates); ++index)
    {
        rows[1][index + 8] = std::to_string(coordinates[index]);
    }
    return rows;
}
}

TEST_CASE("original Merge doll coordinates place body and hands below the screen origin")
{
    OpenYAMM::Game::CharacterDollTable table;
    REQUIRE(table.loadCharacterRows(originalPortraits()));
    REQUIRE(table.loadDollTypeRows(originalDollTypes()));
    const auto *pCharacter = table.getCharacter(1);
    const auto *pDoll = table.getDollType(0);
    REQUIRE(pCharacter != nullptr);
    REQUIRE(pDoll != nullptr);
    CHECK(pCharacter->bodyOffsetX == 30);
    CHECK(pCharacter->bodyOffsetY == 33);
    CHECK(pCharacter->helmOffsetX == -2);
    CHECK(pCharacter->helmOffsetY == 1);
    CHECK(pDoll->rightHandOpenY == 110);
    CHECK(pDoll->rightHandClosedY == 117);
    CHECK(pDoll->rightHandFingersY == 118);
    CHECK(pDoll->leftHandOpenX == 132);
    CHECK(pDoll->leftHandClosedX == 138);
    CHECK(pDoll->leftHandOpenY == 150);
    CHECK(pDoll->leftHandClosedY == 150);
    CHECK(pDoll->leftHandFingersY == 150);
    CHECK(pDoll->mainHandWeaponAnchorX() == 30);
    CHECK(pDoll->mainHandWeaponAnchorY() == 127);
    CHECK(pDoll->offHandWeaponAnchorX() == 132);
    CHECK(pDoll->offHandWeaponAnchorY() == 150);
    CHECK(pDoll->bowOffsetY == 100);
    CHECK(pDoll->shieldY == 44);
}

TEST_CASE("original Merge negative body and weapon offsets retain their signed direction")
{
    Rows portraits = originalPortraits();
    portraits[1][7] = "-8";
    Rows dolls = originalDollTypes();
    dolls[1][21] = "-40";
    dolls[1][23] = "-10";
    OpenYAMM::Game::CharacterDollTable table;
    REQUIRE(table.loadCharacterRows(portraits));
    REQUIRE(table.loadDollTypeRows(dolls));
    CHECK(table.getCharacter(1)->bodyOffsetY == -8);
    CHECK(table.getDollType(0)->offHandWeaponAnchorY() == 110);
    CHECK(table.getDollType(0)->mainHandWeaponAnchorY() == 107);
}

TEST_CASE("legacy converted doll tables keep upward Y and closed-before-open hand columns")
{
    Rows portraits(2, std::vector<std::string>(24, "0"));
    portraits[1][0] = "7";
    portraits[1][7] = "-33";
    Rows dolls = originalDollTypes();
    dolls[0][14] = "LHc X";
    dolls[0][16] = "LHo X";
    dolls[1][11] = "-117";
    dolls[1][15] = "-150";
    dolls[1][17] = "-160";
    dolls[1][23] = "-10";
    OpenYAMM::Game::CharacterDollTable table;
    REQUIRE(table.loadCharacterRows(portraits));
    REQUIRE(table.loadDollTypeRows(dolls));
    CHECK(table.getCharacter(7)->bodyOffsetY == 33);
    CHECK(table.getCharacter(8) == nullptr);
    CHECK(table.getDollType(0)->leftHandClosedX == 132);
    CHECK(table.getDollType(0)->leftHandOpenX == 138);
    CHECK(table.getDollType(0)->leftHandClosedY == 150);
    CHECK(table.getDollType(0)->leftHandOpenY == 160);
    CHECK(table.getDollType(0)->mainHandWeaponAnchorY() == 127);
}
