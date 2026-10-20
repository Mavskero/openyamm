#include "doctest/doctest.h"

#include "game/events/EventRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/party/Party.h"

#include <cstdlib>
#include <filesystem>

using namespace OpenYAMM::Game;

TEST_CASE("dismissed earlier followers become available in cached maps while later followers remain hidden")
{
    for (uint32_t dismissed : {1001u, 1002u})
    {
        CAPTURE(dismissed);
        Party party;
        party.reset();
        party.addHiredNpcFollower({1001, 10, 300});
        party.addHiredNpcFollower({1002, 11, 400});
        EventRuntimeState cached;
        cached.unavailableNpcIds.insert(1999); // Unrelated map-local exclusion remains intact.
        party.applyGlobalNpcStateTo(cached);
        REQUIRE_EQ(cached.hiredNpcFollowers.size(), 2u);
        party.removeHiredNpcFollower(dismissed);
        party.setNpcUnavailable(dismissed, false);
        const uint32_t retained = dismissed == 1001 ? 1002 : 1001;

        EventRuntimeState warm = cached;
        party.applyGlobalNpcStateTo(warm);
        REQUIRE_EQ(warm.hiredNpcFollowers.size(), 1u);
        CHECK_EQ(warm.hiredNpcFollowers[0].npcId, retained);
        CHECK_FALSE(warm.unavailableNpcIds.contains(dismissed));
        CHECK(warm.unavailableNpcIds.contains(retained));
        CHECK(warm.unavailableNpcIds.contains(1999));

        GameSaveData save;
        save.mapFileName = "out01.odm";
        save.party = party.snapshot();
        save.hasOutdoorRuntimeState = true;
        save.outdoorWorld.eventRuntimeState = cached;
        const char *pTemporaryDirectory = std::getenv("TMPDIR");
        REQUIRE(pTemporaryDirectory != nullptr);
        const std::filesystem::path path = std::filesystem::path(pTemporaryDirectory)
            / ("npc-cache142-" + std::to_string(dismissed) + ".oysav");
        std::string error;
        REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
        const std::optional<GameSaveData> restored = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(restored.has_value(), error);
        REQUIRE(restored->outdoorWorld.eventRuntimeState.has_value());
        Party cold;
        cold.restoreSnapshot(restored->party);
        EventRuntimeState coldCache = *restored->outdoorWorld.eventRuntimeState;
        cold.applyGlobalNpcStateTo(coldCache);
        REQUIRE_EQ(coldCache.hiredNpcFollowers.size(), 1u);
        CHECK_EQ(coldCache.hiredNpcFollowers[0].npcId, retained);
        CHECK_FALSE(coldCache.unavailableNpcIds.contains(dismissed));
        CHECK(coldCache.unavailableNpcIds.contains(retained));
        CHECK(coldCache.unavailableNpcIds.contains(1999));
        cold.applyEventRuntimeState(coldCache, false);
        CHECK_EQ(cold.snapshot().hiredNpcFollowers.size(), 1u);
    }
}
