#include "doctest/doctest.h"
#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"
#include "tools/LegacyLuaExport.h"
#include "tools/mmerge_import/LodResources.h"
#include "tools/mmerge_import/Mm8EndgameResources.h"
#include "tools/mmerge_import/QuestItemCapacityResources.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
std::filesystem::path sourceRoot()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

const std::vector<uint8_t> &originalBytes()
{
    static const auto bytes = readLodText(sourceRoot() / "Data/mmmerge.T.lod", "global.evt");
    return bytes;
}

const OpenYAMM::Tests::RegressionGameData &data()
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(), OpenYAMM::Tests::regressionGameDataFailure());
    return OpenYAMM::Tests::regressionGameData();
}

std::optional<ScriptedEventProgram> program(const std::string &source)
{
    std::string error;
    const auto result = ScriptedEventProgram::loadFromLuaText(std::string(EventSupportSource) + "\n" + source,
        "@quest-capacity151", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(result.has_value(), error);
    return result;
}

const std::string &exportedSource()
{
    static const std::string source = []()
    {
        EvtProgram events;
        REQUIRE(events.loadFromBytes(originalBytes()));
        const auto profile = loadMm8EndgameProfile(sourceRoot());
        REQUIRE(profile);
        LegacyLuaExportLookups lookups;
        lookups.inputStringAnswerBindings = profile->inputStringAnswerBindings;
        for (uint32_t id = 1; id <= 3000; ++id)
        {
            const auto text = data().npcDialogTable.getText(id);
            if (text) lookups.npcTexts.emplace(id, *text);
        }
        return generateLegacyEventLuaChunk(events, {}, lookups, LegacyLuaExportScope::Global, LegacyEventVersion::Mm8);
    }();
    return source;
}

const std::optional<ScriptedEventProgram> &guardedProgram()
{
    // An explicit baseline run executes the real original export without the new guard, to preserve
    // the partial-reward failure. No production behavior depends on this test-only environment.
    static const auto result = program(exportedSource()
        + (std::getenv("OPENYAMM_QUEST_CAPACITY_BASELINE") != nullptr ? ""
            : "\n" + buildQuestItemCapacityOverlay(originalBytes(), true)));
    return result;
}

std::filesystem::path temporarySave()
{
    const char *pDirectory = std::getenv("TMPDIR");
    REQUIRE(pDirectory != nullptr);
    static uint64_t index = 0;
    static const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::path(pDirectory)
        / ("quest-capacity151-" + std::to_string(nonce) + "-" + std::to_string(index++) + ".oysav");
}

InventoryItem item(uint32_t id)
{
    InventoryItem result;
    result.objectDescriptionId = id;
    return result;
}

struct Fixture
{
    Party party;
    EventRuntimeState state;
    EventRuntime events{&data().houseTable, &data().npcDialogTable};
    std::optional<ScriptedEventProgram> global = guardedProgram();

    Fixture()
    {
        party.setItemTable(&data().itemTable);
        party.seed(OpenYAMM::Tests::createSpellRegressionPartySeed());
        state.mapFileName = "out02.odm";
        state.activeHistoryContinentId = 1;
        state.normalGameplayScreen = true;
        // Existing held ownership is not available capacity and must survive rejected grants.
        party.setHeldItemForQueries(item(2125));
    }

    void fill(size_t count)
    {
        REQUIRE(party.pendingItemDeliveries().empty());
        for (size_t index = 0; index < count; ++index)
        {
            REQUIRE(party.grantOwnedQuestItem(item(543), -1));
        }
    }

    bool run(uint16_t event, std::optional<uint8_t> continuation = std::nullopt)
    {
        return events.executeNpcTopicEventById(std::nullopt, global, event, state, &party, nullptr, continuation);
    }

    void succeed(uint16_t event, std::optional<uint8_t> continuation = std::nullopt)
    {
        const bool ok = run(event, continuation);
        CHECK_MESSAGE(ok, events.lastScriptError().value_or("event not executed"));
    }

    void set(uint32_t bit)
    {
        EventRuntime::setQuestBitValue(state, party, bit, true);
    }

    GameSaveData save() const
    {
        GameSaveData result;
        result.mapFileName = state.mapFileName;
        result.currentSceneKind = SceneKind::Outdoor;
        result.party = party.snapshot();
        result.outdoorWorld.eventRuntimeState = state;
        result.namedGlobalVars = state.namedGlobalVars;
        result.heldInventoryItemActive = true;
        result.heldInventoryItem = item(2125);
        return result;
    }

    std::vector<char> durableBytes(bool withoutCapacityMessage = false) const
    {
        const auto path = temporarySave();
        std::string error;
        GameSaveData snapshot = save();
        if (withoutCapacityMessage)
        {
            // Rejection must append exactly this UI message. Keep every other serialized field,
            // including prior messages, pending actions, quest progress and reward ownership.
            auto &messages = snapshot.outdoorWorld.eventRuntimeState->messages;
            REQUIRE_FALSE(messages.empty());
            REQUIRE_EQ(messages.back(), "请先整理物品，再来领取奖励。");
            messages.pop_back();
        }
        REQUIRE_MESSAGE(saveGameDataToPath(path, snapshot, error), error);
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }

    void cold()
    {
        const auto path = temporarySave();
        std::string error;
        REQUIRE_MESSAGE(saveGameDataToPath(path, save(), error), error);
        const auto loaded = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(loaded, error);
        party.restoreSnapshot(loaded->party);
        party.setHeldItemForQueries(loaded->heldInventoryItem);
        REQUIRE(loaded->outdoorWorld.eventRuntimeState);
        state = *loaded->outdoorWorld.eventRuntimeState;
        state.mapFileName = loaded->mapFileName;
        state.normalGameplayScreen = true;
        // A new EventRuntime/Lua session restores state without rerunning a reward.
        events = EventRuntime(&data().houseTable, &data().npcDialogTable);
    }
};
}

TEST_CASE("quest capacity overlay authenticates the actual source and preserves metadata")
{
    auto altered = originalBytes();
    altered.back() ^= 1;
    CHECK_THROWS_AS(buildQuestItemCapacityOverlay(altered, true), std::runtime_error);
    CHECK_THROWS_AS(buildQuestItemCapacityOverlay({}, false), std::runtime_error);
    CHECK(buildQuestItemCapacityOverlay(originalBytes(), true).find("请先整理物品") != std::string::npos);
    CHECK(buildQuestItemCapacityOverlay(originalBytes(), false).find("Please make room") != std::string::npos);
    const auto original = program(exportedSource());
    const auto guarded = guardedProgram();
    CHECK(guarded->eventIds() == original->eventIds());
    CHECK(guarded->canShowTopicEventIds() == original->canShowTopicEventIds());
    CHECK(guarded->onLoadEventIds() == original->onLoadEventIds());
    CHECK(guarded->onLeaveEventIds() == original->onLeaveEventIds());
    Fixture f;
    for (uint16_t id : {29, 164, 165, 166, 175, 188})
    {
        CHECK(guarded->getHint(id) == original->getHint(id));
        CHECK(guarded->summarizeEvent(id) == original->summarizeEvent(id));
        CHECK(f.events.canShowTopic(guarded, id, f.state, &f.party)
            == f.events.canShowTopic(original, id, f.state, &f.party));
    }
    f.set(108);
    f.set(118);
    CHECK_FALSE(f.events.canShowTopic(guarded, 175, f.state, &f.party));
    CHECK_FALSE(f.events.canShowTopic(guarded, 188, f.state, &f.party));
}

TEST_CASE("capacity query is read only bounded and permits no-party tooling")
{
    Fixture f;
    f.fill(127);
    const auto before = f.durableBytes();
    auto query = program("RegisterCanShowTopic(61000,function() return evt.CanQueueItemDeliveries(1) end)"
        " RegisterCanShowTopic(61001,function() return evt.CanQueueItemDeliveries(2) end)");
    CHECK(f.events.canShowTopic(query, 61000, f.state, &f.party));
    CHECK_FALSE(f.events.canShowTopic(query, 61001, f.state, &f.party));
    CHECK(f.events.canShowTopic(query, 61001, f.state, nullptr));
    CHECK(f.durableBytes() == before);
    for (const char *count : {"-1", "129", "1.5"})
    {
        const auto invalid = program(std::string("RegisterGlobalEvent(61002,nil,function() ")
            + "evt.CanQueueItemDeliveries(" + count + ") SetQBit(QBit(9)) end)");
        CHECK_FALSE(f.events.executeNpcTopicEventById(std::nullopt, invalid, 61002, f.state, &f.party));
        CHECK_FALSE(f.party.hasQuestBit(9));
        CHECK(f.events.lastScriptError());
    }

    // Move one already queued delivery to a fixed recipient, then use the real detachment path.
    // The active queue alone has two spare slots, while total owned pending deliveries have one.
    auto snapshot = f.party.snapshot();
    REQUIRE(snapshot.members.size() > 1);
    snapshot.pendingItemDeliveries.back().recipientMemberIndex = 1;
    f.party.restoreSnapshot(snapshot);
    REQUIRE(f.party.detachMembers("quest-capacity-151", {{1, 1}}));
    REQUIRE_EQ(f.party.pendingItemDeliveries().size(), 126);
    REQUIRE_EQ(f.party.detachedMembers().front().pendingItemDeliveries.size(), 1);
    const auto detachedBefore = f.durableBytes();
    CHECK(f.events.canShowTopic(query, 61000, f.state, &f.party));
    CHECK_FALSE(f.events.canShowTopic(query, 61001, f.state, &f.party));
    CHECK(f.durableBytes() == detachedBefore);
    REQUIRE(f.party.grantOwnedQuestItem(item(543), -1));
    CHECK_FALSE(f.events.canShowTopic(query, 61000, f.state, &f.party));
}

TEST_CASE("full quest deliveries keep original letter and report branches retryable after cold load")
{
    for (const auto [event, reward, bit] : {std::array<uint16_t, 3>{29, 742, 9}, {188, 602, 117}})
    {
        CAPTURE(event);
        Fixture f;
        f.fill(128);
        const auto before = f.durableBytes();
        f.succeed(event);
        CHECK(f.durableBytes(true) == before);
        CHECK_FALSE(f.party.hasQuestBit(bit));
        CHECK_EQ(f.party.pendingItemCount(reward), 0);
        REQUIRE_FALSE(f.state.messages.empty());
        CHECK(f.state.messages.back() == "请先整理物品，再来领取奖励。");
        f.cold();
        f.party.completeFirstPendingItemDelivery();
        f.succeed(event);
        CHECK(f.party.hasQuestBit(bit));
        CHECK_EQ(f.party.pendingItemCount(reward), 1);
        CHECK_EQ(f.party.pendingItemDeliveries().size(), 128);
        CHECK_EQ(f.party.heldItemIdForQueries(), 2125);
        f.cold();
        CHECK_EQ(f.party.pendingItemCount(reward), 1);
        if (event == 29)
        {
            f.succeed(29); // Original Q9 reminder branch, even while full.
        }
        else
        {
            CHECK_EQ(f.party.snapshot().npcTopicOverrides.at(4).at(1), 189);
            f.succeed(189); // Normal post-grant topic; do not force hidden/replaced188 again.
        }
        CHECK_EQ(f.party.pendingItemCount(reward), 1);
    }
}

TEST_CASE("three original disease scrolls preflight as one batch with no partial quest mutation")
{
    for (size_t queued : {125u, 126u, 127u, 128u})
    {
        CAPTURE(queued);
        Fixture f;
        f.fill(queued);
        const auto before = f.durableBytes();
        f.succeed(175);
        if (queued > 125)
        {
            CHECK(f.durableBytes(true) == before);
            CHECK_FALSE(f.party.hasQuestBit(101));
            CHECK_EQ(f.party.pendingItemCount(373), 0);
            f.cold();
            for (size_t count = queued; count > 125; --count) f.party.completeFirstPendingItemDelivery();
            f.succeed(175);
        }
        CHECK(f.party.hasQuestBit(101));
        CHECK_EQ(f.party.pendingItemCount(373), 3);
        CHECK_EQ(f.party.pendingItemDeliveries().size(), 128);
        CHECK_EQ(f.party.snapshot().npcTopicOverrides.at(65).at(0), 176);
        f.cold();
        CHECK_EQ(f.party.pendingItemCount(373), 3);
    }
}

TEST_CASE("all six Escaton answer orders defer only the final key reward when full")
{
    std::array<uint16_t, 3> order = {164, 165, 166};
    do
    {
        Fixture f;
        f.fill(128);
        f.succeed(163);
        for (size_t index = 0; index < order.size(); ++index)
        {
            const uint16_t event = order[index];
            CAPTURE(event);
            f.succeed(event);
            REQUIRE(f.state.pendingInputPrompt);
            REQUIRE_EQ(f.state.pendingInputPrompt->eventId, event);
            REQUIRE_EQ(f.state.pendingInputPrompt->correctStep, 4);
            REQUIRE_FALSE(f.state.pendingInputPrompt->answers.empty());
            // App input matching is covered by146. Here continue the real exported correct branch.
            f.state.pendingInputPrompt.reset();
            const auto before = f.durableBytes();
            f.succeed(event, 4);
            if (index < 2)
            {
                CHECK(f.party.hasQuestBit(94 + event - 164));
                CHECK_FALSE(f.party.hasQuestBit(97));
                CHECK_EQ(f.party.pendingItemCount(629), 0);
            }
            else
            {
                CHECK(f.durableBytes(true) == before);
                CHECK_FALSE(f.party.hasQuestBit(97));
                CHECK_EQ(f.party.pendingItemCount(629), 0);
                f.cold();
                f.party.completeFirstPendingItemDelivery();
                f.succeed(event);
                REQUIRE(f.state.pendingInputPrompt);
                f.state.pendingInputPrompt.reset();
                f.succeed(event, 4);
                for (uint32_t bit : {97u, 47u, 48u, 50u, 52u, 54u, 220u}) CHECK(f.party.hasQuestBit(bit));
                CHECK_EQ(f.party.pendingItemCount(629), 1);
                f.cold();
                f.succeed(171); // Original completed-riddle route does not grant a second key.
                CHECK_EQ(f.party.pendingItemCount(629), 1);
            }
        }
    } while (std::next_permutation(order.begin(), order.end()));
}

TEST_CASE("full delivery capacity leaves nonreward source branches and wrong answers available")
{
    for (uint32_t bit : {9u, 10u, 12u, 24u})
    {
        Fixture f;
        f.fill(128);
        f.set(bit);
        f.succeed(29);
        CHECK_EQ(f.party.pendingItemCount(742), 0);
        REQUIRE_FALSE(f.state.messages.empty());
        CHECK(f.state.messages.back() != "请先整理物品，再来领取奖励。");
        if (bit == 10) CHECK(f.party.hasQuestBit(24));
        if (bit == 24) CHECK(f.party.hasQuestBit(11));
    }
    Fixture f;
    f.fill(128);
    f.set(95);
    f.set(96);
    f.succeed(164);
    REQUIRE(f.state.pendingInputPrompt);
    f.state.pendingInputPrompt.reset();
    f.succeed(164, 2);
    CHECK_FALSE(f.party.hasQuestBit(97));
    CHECK_EQ(f.party.pendingItemCount(629), 0);
    CHECK_EQ(f.party.snapshot().npcTopicOverrides.at(22).at(3), 171);
}
