#include "CrossContinentsIntroSource.h"
#include "EventSupportSource.h"
#include "doctest/doctest.h"

#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/ChestRuntime.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"
#include "tests/RegressionGameData.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>

using namespace OpenYAMM::Game;

namespace
{
constexpr const char *Generated = "MMerge.CrossContinents.ScrollGenerated";
constexpr const char *Gotten = "MMerge.CrossContinents.ScrollGotten";
constexpr const char *MainQuest = "MMerge.CrossContinents.GotMainQuest";

const ItemTable &items()
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());
    return OpenYAMM::Tests::regressionGameData().itemTable;
}

std::optional<ScriptedEventProgram> program(ScriptedEventScope scope, const std::string &source)
{
    std::string error;
    std::optional<ScriptedEventProgram> value = ScriptedEventProgram::loadFromLuaText(
        std::string(OpenYAMM::Import::EventSupportSource) + "\n" + source,
        "@invitation-production-chain", scope, error);
    REQUIRE_MESSAGE(value.has_value(), error);
    return value;
}

MapDeltaChest chestWithRecords(std::initializer_list<uint32_t> ids)
{
    MapDeltaChest chest;
    chest.rawItems.resize(ids.size() * 36);
    chest.inventoryMatrix.resize(81);
    size_t index = 0;
    for (uint32_t id : ids)
    {
        std::memcpy(chest.rawItems.data() + index * 36, &id, sizeof(id));
        if (index < 81 && id != 0)
        {
            chest.inventoryMatrix[index] = int16_t(index + 1);
        }
        ++index;
    }
    return chest;
}

uint32_t recordId(const MapDeltaChest &chest, size_t index)
{
    uint32_t id = 0;
    std::memcpy(&id, chest.rawItems.data() + index * 36, sizeof(id));
    return id;
}

struct ScopedSessionMechanicsBinding
{
    ~ScopedSessionMechanicsBinding()
    {
        GameMechanics::bindClassMultiplierTable(nullptr);
    }
};

class InvitationWorld : public OpenYAMM::Tests::PartySpellTestWorldRuntime
{
public:
    Party party;
    InventoryItem acquired;
    EventRuntimeState state;
    EventRuntime events;
    std::optional<ScriptedEventProgram> global = program(ScriptedEventScope::Global,
        std::string(OpenYAMM::Import::CrossContinentsIntroSource));
    std::optional<ScriptedEventProgram> local = program(ScriptedEventScope::Map,
        "RegisterEvent(100, nil, function() evt.OpenChest(0) end)\n"
        "RegisterEvent(101, nil, function() evt.OpenChest(1) end)");
    std::vector<MapDeltaChest> chests = {chestWithRecords({1, 0, 1}), chestWithRecords({0, 0, 0})};
    std::vector<std::optional<GameplayChestViewState>> views;
    std::optional<GameplayChestViewState> active;
    std::function<void()> afterNaturalActivation;

    InvitationWorld(uint32_t experience = 50001, uint32_t continent = 1)
    {
        PartySeed seed = OpenYAMM::Tests::createSpellRegressionPartySeed();
        seed.members[0].experience = experience;
        seed.members[1].experience = 90000;
        party.setItemTable(&items());
        party.setClassMultiplierTable(&OpenYAMM::Tests::regressionGameData().classMultiplierTable);
        party.seed(seed);
        REQUIRE(party.setActiveMemberIndex(1));
        state.activeHistoryContinentId = continent;
        bindParty(&party);
        bindEventRuntimeState(&state);
        bindGlobalEventProgram(&global);
    }

    bool executeEventHooks(EventRuntimeHookKind kind) override
    {
        if (kind == EventRuntimeHookKind::DimensionDoorActivated && afterNaturalActivation)
        {
            afterNaturalActivation();
        }
        return events.executeHooks(local, global, kind, state, &party);
    }

    bool executeNpcTopicEvent(uint16_t eventId, size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        previousMessageCount = state.messages.size();
        const bool result = events.executeNpcTopicEventById(local, global, eventId, state, &party,
            nullptr, continueStep);
        party.applyEventRuntimeState(state, false);
        return result;
    }

    void open(uint16_t eventId = 100)
    {
        REQUIRE(events.executeEventById(local, global, eventId, state, &party));
        applyPendingChestItemInsertions(state, chests, views, active, &items());
        const std::vector<EventRuntimeState::OpenedChestRequest> opened = consumeOpenedChestRequests(state);
        REQUIRE_EQ(opened.size(), 1);
        const uint32_t id = opened.front().chestId;
        if (views.size() <= id)
        {
            views.resize(id + 1);
        }
        if (!views[id])
        {
            views[id] = buildMaterializedChestView(id, chests[id], 0, 1, 42, nullptr, &items(), &party);
        }
        active = views[id];
    }

    void take(uint32_t itemId)
    {
        REQUIRE(active.has_value());
        const auto found = std::find_if(active->items.begin(), active->items.end(),
            [itemId](const GameplayChestItemState &item) { return item.itemId == itemId; });
        REQUIRE(found != active->items.end());
        GameplayChestItemState taken;
        REQUIRE(takeChestItem(*active, size_t(found - active->items.begin()), taken));
        views[active->chestId] = active;
        acquired = taken.item;
        party.setHeldItemForQueries(taken.item);
        notifyItemAcquired(taken.itemId);
    }
};

std::filesystem::path outputPath(const char *name)
{
    const char *root = std::getenv("OPENYAMM_INVITATION_TEST_OUTPUT");
    REQUIRE(root != nullptr);
    std::filesystem::create_directories(root);
    return std::filesystem::path(root) / name;
}
}

TEST_CASE("Invitation threshold is strictly Party0 experience for each original continent")
{
    for (uint32_t continent : {1u, 2u, 3u, 4u})
    {
        for (uint32_t experience : {49999u, 50000u, 50001u})
        {
            CAPTURE(continent);
            CAPTURE(experience);
            InvitationWorld world(experience, continent);
            world.open();
            const bool generated = experience > 50000;
            const uint32_t expected = continent <= 3 ? 769 + continent : 770;
            CHECK_EQ(world.state.namedGlobalVars[Generated], generated ? 1 : 0);
            CHECK_EQ(world.state.namedGlobalVars[Gotten], 0);
            CHECK_EQ(recordId(world.chests[0], 0), 1);
            CHECK_EQ(recordId(world.chests[0], 1), generated ? expected : 0);
            CHECK_EQ(recordId(world.chests[0], 2), 1);
            CHECK_EQ(world.state.namedGlobalVars["MMerge.LostItems." + std::to_string(expected)],
                generated ? int(continent) : 0);
            CHECK(world.state.pendingChestItemInsertions.empty());
            CHECK(world.state.chestItemRequests.empty());
            if (generated)
            {
                world.take(expected);
                CHECK_EQ(world.state.namedGlobalVars[Gotten], 1);
                world.open();
                CHECK(std::none_of(world.active->items.begin(), world.active->items.end(),
                    [expected](const GameplayChestItemState &item) { return item.itemId == expected; }));
                world.open(101);
                CHECK_EQ(recordId(world.chests[1], 0), 0);
            }
        }
    }
    InvitationWorld firstMemberOnly;
    firstMemberOnly.party.member(1)->experience = 0;
    firstMemberOnly.open();
    CHECK_EQ(firstMemberOnly.state.namedGlobalVars[Generated], 1);
}

TEST_CASE("Full invitation chest still records generation and never retries another chest")
{
    InvitationWorld world;
    world.chests[0] = chestWithRecords({1, 1, 1});
    const std::vector<uint8_t> original = world.chests[0].rawItems;
    world.open();
    CHECK_EQ(world.chests[0].rawItems, original);
    CHECK_EQ(world.state.namedGlobalVars[Generated], 1);
    CHECK_EQ(world.state.namedGlobalVars["MMerge.LostItems.770"], 1);
    CHECK_EQ(world.state.namedGlobalVars[Gotten], 0);
    world.take(1);
    CHECK_EQ(world.state.namedGlobalVars[Gotten], 0);
    world.open(101);
    CHECK_EQ(recordId(world.chests[1], 0), 0);
}

TEST_CASE("First freed materialized record is used independently of stale raw records and grid capacity")
{
    InvitationWorld world(50000);
    world.chests[0] = chestWithRecords({1, 1, 1});
    world.open();
    REQUIRE_EQ(world.active->items.size() + world.active->hiddenItems.size(), 3);
    GameplayChestItemState taken;
    REQUIRE(takeChestItem(*world.active, 0, taken));
    const int32_t freed = taken.itemRecordIndex;
    REQUIRE(freed >= 0);
    world.views[0] = world.active;
    world.party.member(0)->experience = 50001;
    world.open();
    CHECK_EQ(recordId(world.chests[0], size_t(freed)), 1); // Deliberately stale original raw contents.
    const auto invitation = std::find_if(world.active->items.begin(), world.active->items.end(),
        [](const GameplayChestItemState &item) { return item.itemId == 770; });
    REQUIRE(invitation != world.active->items.end());
    CHECK_EQ(invitation->itemRecordIndex, freed);

    MapDeltaChest raw = chestWithRecords({1, 0});
    GameplayChestViewState fullGrid;
    fullGrid.itemRecordCapacity = 2;
    fullGrid.gridWidth = 1;
    fullGrid.gridHeight = 1;
    GameplayChestItemState blocker;
    blocker.itemId = 1;
    blocker.itemRecordIndex = 0;
    fullGrid.items.push_back(blocker);
    std::optional<GameplayChestViewState> cached = fullGrid;
    REQUIRE(insertChestItemOnce(raw, cached, 772, &items()));
    REQUIRE_EQ(cached->hiddenItems.size(), 1);
    CHECK_EQ(cached->hiddenItems[0].itemId, 772);
    CHECK_EQ(cached->hiddenItems[0].itemRecordIndex, 1);
    CHECK_FALSE(insertChestItemOnce(raw, cached, 771, &items()));
}

TEST_CASE("Unindexed legacy chest migration preserves contents and assigns stable native records")
{
    MapDeltaChest raw = chestWithRecords({1, 1, 1, 1});
    GameplayChestViewState legacy;
    legacy.gridWidth = 9;
    legacy.gridHeight = 9;
    GameplayChestItemState item;
    item.itemId = 1;
    item.quantity = 7;
    item.gridX = 3;
    item.gridY = 4;
    legacy.items.push_back(item);
    item.quantity = 2;
    legacy.hiddenItems.push_back(item);
    std::optional<GameplayChestViewState> cached = legacy;
    REQUIRE(insertChestItemOnce(raw, cached, 770, &items()));
    CHECK_EQ(cached->itemRecordCapacity, 4);
    CHECK_EQ(cached->items[0].quantity, 7);
    CHECK_EQ(cached->items[0].gridX, 3);
    CHECK_EQ(cached->items[0].gridY, 4);
    CHECK_EQ(cached->items[0].itemRecordIndex, 0);
    CHECK_EQ(cached->items.size(), 3);
    CHECK_EQ(cached->items[1].quantity, 2);
    CHECK_EQ(cached->items[1].itemRecordIndex, 1);
    CHECK_EQ(cached->items[2].itemRecordIndex, 2);
}

TEST_CASE("Actual acquired invitation and materialized records survive save and independent session restore")
{
    InvitationWorld world(50001, 3);
    world.open();
    world.take(772);
    GameSaveData saved;
    saved.mapFileName = "oute3.odm";
    saved.party = world.party.snapshot();
    saved.namedGlobalVars = world.state.namedGlobalVars;
    saved.heldInventoryItemActive = true;
    saved.heldInventoryItem = world.acquired;
    saved.hasOutdoorRuntimeState = true;
    saved.outdoorWorld.eventRuntimeState = world.state;
    saved.outdoorWorld.materializedChestViews = world.views;
    std::string error;
    const std::filesystem::path path = outputPath("invitation-held-records.oysav");
    REQUIRE_MESSAGE(saveGameDataToPath(path, saved, error), error.c_str());
    const std::optional<GameSaveData> loaded = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(loaded.has_value(), error.c_str());
    CHECK_EQ(loaded->heldInventoryItem.objectDescriptionId, 772);
    CHECK_EQ(loaded->namedGlobalVars, saved.namedGlobalVars);
    REQUIRE(loaded->outdoorWorld.materializedChestViews[0].has_value());
    const GameplayChestViewState &restored = *loaded->outdoorWorld.materializedChestViews[0];
    CHECK_EQ(restored.itemRecordCapacity, 3);
    CHECK_EQ(restored.items.size(), world.views[0]->items.size());
    for (size_t index = 0; index < restored.items.size(); ++index)
    {
        CHECK_EQ(restored.items[index].itemRecordIndex, world.views[0]->items[index].itemRecordIndex);
        CHECK_EQ(restored.items[index].itemId, world.views[0]->items[index].itemId);
    }
    GameDataLoader loader;
    GameDataRepository repository;
    repository.bind(loader);
    GameSession session;
    session.bindDataRepository(&repository);
    const ScopedSessionMechanicsBinding mechanicsBinding;
    session.restoreFromSaveData(*loaded);
    InvitationWorld second(50001, 1);
    session.applyNamedGlobalVarsToRuntime(second.state);
    second.open();
    CHECK_EQ(recordId(second.chests[0], 1), 0);
    CHECK_EQ(second.state.namedGlobalVars[Gotten], 1);
    CHECK_EQ(second.state.namedGlobalVars["MMerge.LostItems.772"], 3);
}

TEST_CASE("Invitation natural activation follows timer and item190 does not run the encounter hook")
{
    InvitationWorld world;
    world.open();
    world.take(770);
    const char *root = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
    REQUIRE(root != nullptr);
    OpenYAMM::Engine::AssetFileSystem files;
    REQUIRE(files.initialize(root, root, OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
    GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadCommonForGameplay(files));
    GameDataRepository repository;
    repository.bind(loader);
    GameSession session;
    session.bindDataRepository(&repository);
    const ScopedSessionMechanicsBinding mechanicsBinding;
    session.bindActiveWorldRuntime(&world);
    GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
    int naturalHooks = 0;
    world.afterNaturalActivation = [&]()
    {
        ++naturalHooks;
        CHECK(runtime.shouldTownPortalCastOpenDimensionDoor());
    };
    runtime.activateDimensionDoor(DimensionDoorActivationSource::ItemScroll);
    CHECK_EQ(naturalHooks, 0);
    CHECK_FALSE(world.state.pendingDialogueContext.has_value());
    world.advanceGameMinutes(1.0f);
    CHECK_FALSE(runtime.shouldTownPortalCastOpenDimensionDoor());
    runtime.activateDimensionDoor();
    CHECK_EQ(naturalHooks, 1);
    REQUIRE(runtime.activeEventDialog().isActive);
    CHECK_EQ(runtime.activeEventDialog().sourceId, 803);
    CHECK_EQ(world.state.npcGreetingOverrides.at(803), 329);
    // Completion is earned through the existing four real intro handlers, not preset.
    for (uint16_t topic : {1778, 1779, 1779, 1780})
    {
        const auto &actions = runtime.activeEventDialog().actions;
        const auto action = std::find_if(actions.begin(), actions.end(), [topic](const EventDialogAction &entry)
        {
            return entry.kind == EventDialogActionKind::NpcTopic && entry.id == topic;
        });
        REQUIRE_MESSAGE(action != actions.end(), "Original intro topic must be visible after natural NPC entry");
        runtime.eventDialogSelectionIndex() = size_t(action - actions.begin());
        runtime.executeActiveDialogAction([&](EventRuntimeState &state)
        {
            GameplayDialogController::Context context = {
                session.gameplayUiController(), state, runtime.activeEventDialog(),
                runtime.eventDialogSelectionIndex()
            };
            context.pScreenRuntime = &runtime;
            context.pParty = &world.party;
            context.pWorldRuntime = &world;
            context.pGlobalEventProgram = &world.global;
            context.pNpcDialogTable = &loader.getNpcDialogTable();
            return context;
        });
    }
    CHECK_EQ(world.state.namedGlobalVars[MainQuest], 1);
    runtime.closeActiveEventDialog();
    world.state.pendingDialogueContext.reset();
    runtime.activateDimensionDoor();
    CHECK_EQ(naturalHooks, 2);
    CHECK_FALSE(world.state.pendingDialogueContext.has_value());
    world.advanceGameMinutes(7.99f);
    CHECK(runtime.shouldTownPortalCastOpenDimensionDoor());
    world.advanceGameMinutes(0.02f);
    CHECK_FALSE(runtime.shouldTownPortalCastOpenDimensionDoor());
}

TEST_CASE("An invitation acquired before the threshold prevents later chest generation")
{
    InvitationWorld world(49999, 2);
    world.chests[0] = chestWithRecords({771, 0, 1});
    world.open();
    CHECK_EQ(world.state.namedGlobalVars[Generated], 0);
    world.take(771);
    CHECK_EQ(world.state.namedGlobalVars[Gotten], 1);
    world.party.member(0)->experience = 50001;
    world.open(101);
    CHECK_EQ(recordId(world.chests[1], 0), 0);
    CHECK_EQ(world.state.namedGlobalVars[Generated], 0);
}

TEST_CASE("Accepted prior-version normal saves remain readable and round-trip with no quest migration")
{
    const std::filesystem::path root = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path();
    const std::array<const char *, 3> paths = {
        "local-data/validation/mm6-goblinwatch-127/actual-007/mm6-goblinwatch-complete.oysav",
        "local-data/validation/mm7-castle-clear-128/astra-actual-008/mm7-castle-clear-complete.oysav",
        "local-data/validation/mm8-blackmail-129/astra-reload-001/mm8-blackmail-complete.oysav",
    };
    for (size_t index = 0; index < paths.size(); ++index)
    {
        std::string error;
        const std::optional<GameSaveData> prior = loadGameDataFromPath(root / paths[index], error);
        REQUIRE_MESSAGE(prior.has_value(), error);
        const std::filesystem::path path = outputPath(("prior-normal-" + std::to_string(index) + ".oysav").c_str());
        REQUIRE_MESSAGE(saveGameDataToPath(path, *prior, error), error);
        const std::optional<GameSaveData> current = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(current.has_value(), error);
        CHECK_EQ(current->mapFileName, prior->mapFileName);
        CHECK_EQ(current->savedGameMinutes, prior->savedGameMinutes);
        CHECK_EQ(current->namedGlobalVars, prior->namedGlobalVars);
        CHECK_EQ(current->party.members.size(), prior->party.members.size());
        for (size_t member = 0; member < prior->party.members.size(); ++member)
        {
            CHECK_EQ(current->party.members[member].experience, prior->party.members[member].experience);
            CHECK_EQ(current->party.members[member].health, prior->party.members[member].health);
            CHECK_EQ(current->party.members[member].inventory.size(), prior->party.members[member].inventory.size());
        }
    }
}
