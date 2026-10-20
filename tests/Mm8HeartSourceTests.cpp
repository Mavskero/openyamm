#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "game/ui/GameplayOverlayTypes.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/ChestRuntime.h"
#include "game/indoor/IndoorWorldRuntime.h"
#include "game/outdoor/OutdoorWorldRuntime.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <array>
#include <cstdlib>

using namespace OpenYAMM::Game;

TEST_CASE("MM8 original four heart sources materialize and feed the original conflux handover")
{
    // Source locations were independently decoded from mm8.games.lod in 146.
    // This is a bounded production-resource/pickup-interface test, not a route
    // through combat, a pointer-picking test, or a natural campaign claim.
    const char *pRoot = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
    REQUIRE(pRoot != nullptr);
    OpenYAMM::Engine::AssetFileSystem files;
    REQUIRE(files.initialize(pRoot, pRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mm8"));
    GameDataLoader loader;
    loader.setActiveWorldId("mm8");
    REQUIRE(loader.loadCommonForGameplay(files));
    Party party;
    party.seed(OpenYAMM::Tests::createSpellRegressionPartySeed());
    party.setItemTable(&loader.getItemTable());
    REQUIRE_EQ(party.memberCount(), 4u);
    const std::array<const char *, 4> sourceMaps = {"d29.blv", "elemw.odm", "elema.odm", "eleme.blv"};
    const std::array<const char *, 4> exitMaps = {"elemf.odm", "elemw.odm", "elema.odm", "eleme.blv"};
    const std::array<uint32_t, 4> acquiredBits = {242, 241, 243, 244};

    for (size_t heartIndex = 0; heartIndex < sourceMaps.size(); ++heartIndex)
    {
        const uint32_t heartId = uint32_t(606 + heartIndex);
        CAPTURE(sourceMaps[heartIndex]);
        CAPTURE(heartId);
        REQUIRE_FALSE(party.hasItemAnywhere(heartId));
        REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(files, sourceMaps[heartIndex]));
        REQUIRE(loader.getSelectedMap());
        MapAssetInfo assets = *loader.getSelectedMap();
        InventoryItem picked;
        if (heartId == 608)
        {
            REQUIRE(assets.outdoorMapDeltaData);
            REQUIRE_FALSE(assets.outdoorMapDeltaData->chests.empty());
            GameplayChestViewState view = buildMaterializedChestView(0,
                assets.outdoorMapDeltaData->chests[0], assets.map.treasureLevel, assets.map.id, 146,
                &loader.getChestTable(), &loader.getItemTable(), &party);
            const auto found = std::find_if(view.items.begin(), view.items.end(), [&](const auto &item)
                { return item.itemId == heartId; });
            REQUIRE(found != view.items.end());
            GameplayChestItemState removed;
            REQUIRE(takeChestItemAt(view, found->gridX, found->gridY, removed));
            picked = removed.item;
            CHECK(std::none_of(view.items.begin(), view.items.end(), [&](const auto &item)
                { return item.itemId == heartId; }));
        }
        else if (assets.indoorMapData)
        {
            REQUIRE(assets.indoorMapDeltaData);
            REQUIRE_EQ(assets.indoorMapDeltaData->spriteObjects.size(), 1u);
            IndoorWorldRuntime world;
            world.initialize(assets.map, loader.getMonsterTable(), loader.getObjectTable(), loader.getItemTable(),
                loader.getChestTable(), &party, nullptr, &assets.indoorMapDeltaData,
                &assets.eventRuntimeState, nullptr, nullptr, &*assets.indoorMapData);
            GameplayWorldItemInspectState source;
            REQUIRE(world.worldItemInspectState(0, source));
            REQUIRE_EQ(source.item.objectDescriptionId, heartId);
            GameplayWorldItemInspectState removed;
            REQUIRE(world.takeWorldItemInspectState(0, removed));
            picked = removed.item;
            CHECK_FALSE(world.takeWorldItemInspectState(0, removed));
            CHECK(assets.indoorMapDeltaData->spriteObjects.empty());
        }
        else
        {
            REQUIRE(assets.outdoorMapData);
            REQUIRE(assets.outdoorMapDeltaData);
            OutdoorWorldRuntime world;
            world.initialize(assets.map, loader.getMonsterTable(), loader.getMonsterProjectileTable(),
                loader.getObjectTable(), loader.getSpellTable(), loader.getItemTable(), &party, nullptr,
                loader.getStandardItemEnchantTable(), loader.getSpecialItemEnchantTable(), &loader.getChestTable(),
                assets.outdoorMapData, assets.outdoorMapDeltaData, assets.outdoorWeatherProfile,
                assets.eventRuntimeState, std::nullopt, assets.outdoorLandMask);
            size_t heartWorldIndex = world.worldItemCount();
            for (size_t index = 0; index < world.worldItemCount(); ++index)
            {
                GameplayWorldItemInspectState candidate;
                if (world.worldItemInspectState(index, candidate) && candidate.item.objectDescriptionId == heartId)
                {
                    REQUIRE_EQ(heartWorldIndex, world.worldItemCount());
                    heartWorldIndex = index;
                }
            }
            REQUIRE_LT(heartWorldIndex, world.worldItemCount());
            GameplayWorldItemInspectState removed;
            REQUIRE(world.takeWorldItemInspectState(heartWorldIndex, removed));
            picked = removed.item;
            for (size_t index = 0; index < world.worldItemCount(); ++index)
            {
                GameplayWorldItemInspectState remaining;
                REQUIRE(world.worldItemInspectState(index, remaining));
                CHECK_NE(remaining.item.objectDescriptionId, heartId);
            }
        }
        REQUIRE_EQ(picked.objectDescriptionId, heartId);
        REQUIRE(party.member(heartIndex)->addInventoryItem(picked));
        CHECK_EQ(party.inventoryItemCount(heartId), 1);

        REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(files, exitMaps[heartIndex]));
        const MapAssetInfo &exit = *loader.getSelectedMap();
        REQUIRE(exit.localEventProgram);
        EventRuntimeState state = exit.eventRuntimeState.value_or(EventRuntimeState{});
        EventRuntime events;
        const uint32_t experience = party.member(0)->experience;
        REQUIRE(events.executeOnLeaveEvents(exit.localEventProgram, std::nullopt, state, &party));
        party.applyEventRuntimeState(state, false);
        CHECK(party.hasQuestBit(acquiredBits[heartIndex]));
        CHECK_EQ(party.member(0)->experience, experience + 100000u);
        REQUIRE(events.executeOnLeaveEvents(exit.localEventProgram, std::nullopt, state, &party));
        party.applyEventRuntimeState(state, false);
        CHECK_EQ(party.member(0)->experience, experience + 100000u);
    }

    REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(files, "out02.odm"));
    const MapAssetInfo &handover = *loader.getSelectedMap();
    REQUIRE(handover.localEventProgram);
    EventRuntimeState state = handover.eventRuntimeState.value_or(EventRuntimeState{});
    EventRuntime events(&loader.getHouseTable(), &loader.getNpcDialogTable());
    const uint32_t experience = party.member(0)->experience;
    REQUIRE(events.executeEventById(handover.localEventProgram, std::nullopt, 75, state, &party));
    party.applyEventRuntimeState(state, false);
    REQUIRE(state.pendingMovie);
    CHECK_EQ(state.pendingMovie->movieName, "\"confluxkey\"");
    CHECK(party.hasQuestBit(286));
    CHECK(party.hasQuestBit(45));
    CHECK(party.hasQuestBit(46));
    CHECK(party.hasQuestBit(209));
    CHECK_EQ(party.member(0)->experience, experience + 250000u);
    for (uint32_t heart = 606; heart <= 609; ++heart)
    {
        CHECK_FALSE(party.hasItemAnywhere(heart));
    }
    CHECK_EQ(party.pendingItemCount(610) + party.inventoryItemCount(610), 1);
    for (size_t index = 0; index < party.memberCount(); ++index)
    {
        CHECK(party.hasAward(index, 13));
    }
    state.pendingMovie.reset();
    state.pendingDialogueContext.reset();
    REQUIRE(events.executeEventById(handover.localEventProgram, std::nullopt, 75, state, &party));
    party.applyEventRuntimeState(state, false);
    CHECK_FALSE(state.pendingMovie);
    CHECK_EQ(party.member(0)->experience, experience + 250000u);
    CHECK_EQ(party.pendingItemCount(610) + party.inventoryItemCount(610), 1);
}
