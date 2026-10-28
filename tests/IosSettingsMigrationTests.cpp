#include "doctest/doctest.h"

#include "game/app/GameSettings.h"

TEST_CASE("iOS upgrades legacy desktop controls without changing game progress or diagnostics")
{
    using namespace OpenYAMM::Game;
    GameSettings settings = GameSettings::createDefault();
    settings.gameplayUiLayout = GameplayUiLayout::Standard;
    settings.contextActionPopup = false;
    settings.soundVolume = 3;
    settings.musicVolume = 4;
    settings.startWorldId = "mm6";
    settings.startMapFile = "oute3.odm";
    settings.gameplayTrace = true;
    settings.gameplayTraceFile = "logs/keep.log";
    settings.fpsTrace = true;
    settings.preseedParty = false;
    settings.overrideStartPosition = false;
    settings.immortal = false;

    REQUIRE(migrateLegacyIosSettings(settings));
    CHECK_EQ(settings.settingsProfileName, "ios");
    CHECK_EQ(settings.settingsProfileVersion, 1);
    CHECK(settings.contextActionPopup);
    CHECK(settings.gameplayUiLayout == GameplayUiLayout::Standard);
    CHECK_EQ(settings.soundVolume, 3);
    CHECK_EQ(settings.musicVolume, 4);
    CHECK_EQ(settings.startWorldId, "mm6");
    CHECK_EQ(settings.startMapFile, "oute3.odm");
    CHECK(settings.gameplayTrace);
    CHECK_EQ(settings.gameplayTraceFile, "logs/keep.log");
    CHECK(settings.fpsTrace);
    CHECK_FALSE(settings.preseedParty);
    CHECK_FALSE(settings.overrideStartPosition);
    CHECK_FALSE(settings.immortal);

    settings.contextActionPopup = false;
    settings.soundVolume = 5;
    CHECK_FALSE(migrateLegacyIosSettings(settings));
    CHECK_FALSE(settings.contextActionPopup);
    CHECK_EQ(settings.soundVolume, 5);
}

TEST_CASE("iOS migration leaves a newer profile untouched")
{
    using namespace OpenYAMM::Game;
    GameSettings settings = GameSettings::createDefault();
    settings.settingsProfileName = "ios";
    settings.settingsProfileVersion = 2;
    settings.contextActionPopup = false;
    CHECK_FALSE(migrateLegacyIosSettings(settings));
    CHECK_EQ(settings.settingsProfileVersion, 2);
    CHECK_FALSE(settings.contextActionPopup);
}
