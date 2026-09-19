#include "doctest/doctest.h"

#include "game/events/EventRuntime.h"
#include "game/events/EvtEnums.h"
#include "game/gameplay/ChestRuntime.h"

TEST_CASE("Opening a chest supersedes a discharged trap's persistent close override")
{
    using namespace OpenYAMM::Game;
    constexpr uint32_t opened = static_cast<uint32_t>(EvtChestFlag::Opened);
    constexpr uint32_t trapped = static_cast<uint32_t>(EvtChestFlag::Trapped);
    EventRuntimeState events;
    MapDeltaChest chest;
    chest.flags = 0x20;
    events.chestSetMasks[2] = 0x20;
    events.chestClearMasks[2] = trapped | opened;
    events.chestClearMasks[3] = opened;

    markChestOpened(2, chest, &events);
    CHECK_EQ(chest.flags, 0x20 | opened);
    CHECK_EQ(events.chestSetMasks.at(2), 0x20 | opened);
    CHECK_EQ(events.chestClearMasks.at(2), trapped);
    CHECK_EQ(events.chestClearMasks.at(3), opened);

    // Replay the persisted overrides as world restoration does. The successful open must survive.
    uint16_t restoredFlags = chest.flags;
    restoredFlags |= static_cast<uint16_t>(events.chestSetMasks.at(2));
    restoredFlags &= ~static_cast<uint16_t>(events.chestClearMasks.at(2));
    CHECK_EQ(restoredFlags, chest.flags);
    markChestOpened(2, chest, &events);
    CHECK_EQ(chest.flags, restoredFlags);
    CHECK_EQ(events.chestClearMasks.at(2), trapped);
}

TEST_CASE("Opening an untrapped chest works without an event runtime")
{
    using namespace OpenYAMM::Game;
    MapDeltaChest chest;
    chest.flags = 0x20;
    markChestOpened(0, chest, nullptr);
    CHECK_EQ(chest.flags, 0x20 | static_cast<uint16_t>(EvtChestFlag::Opened));
}
