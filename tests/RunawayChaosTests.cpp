#include "RunawayChaosSource.h"
#include "tests/MergeCrossContinentsHarness.h"
#include "tools/mmerge_import/RunawayChaosResources.h"
#include "game/app/GameApplication.h"
#include "game/maps/SaveGame.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace OpenYAMM::Game
{
struct GameApplicationTestAccess
{
    static void submitInput(IGameplayWorldRuntime &world, const std::string &answer, bool accepted = true)
    {
        // Exercise the production answer matcher and NPC continuation dispatcher without a renderer.
        GameApplication application(Engine::ApplicationConfig::createDefault());
        application.m_gameDataRepository.bind(application.m_gameDataLoader);
        application.m_gameSession.bindDataRepository(&application.m_gameDataRepository);
        application.m_gameSession.bindActiveWorldRuntime(&world);
        application.m_pendingInputText = answer;
        application.finishPendingInputPrompt(accepted);
        application.m_gameSession.bindActiveWorldRuntime(nullptr);
    }
};
}

using namespace OpenYAMM::Game;
using OpenYAMM::Tests::MergeQuestFixture;

namespace
{
struct ChaosFixture : MergeQuestFixture
{
    NpcDialogTable texts = OpenYAMM::Tests::regressionGameData().npcDialogTable;
    uint32_t continuationCalls = 0;

    explicit ChaosFixture(bool syntheticAsciiAnswers = false)
    {
        if (syntheticAsciiAnswers)
        {
            std::vector<std::vector<std::string>> rows;
            for (uint32_t id = 1; id <= 3000; ++id)
            {
                const std::optional<std::string> value = texts.getText(id);
                if (value)
                {
                    const bool answer = id >= 2219 && id <= 2239 && id % 2 == 1;
                    rows.push_back({std::to_string(id), answer ? "MiXeD42_" + std::to_string(id) : *value});
                }
            }
            REQUIRE(texts.loadTextsFromRows(rows));
        }
        events = EventRuntime(&OpenYAMM::Tests::regressionGameData().houseTable, &texts);
        std::string error;
        const std::string source = std::string(OpenYAMM::Import::EventSupportSource) + "\n"
            + "RegisterGlobalEvent(1790, nil, function() AddValue(Gold, 17) end)\n"
            + std::string(OpenYAMM::Import::CrossContinentsIntroSource) + "\n"
            + std::string(OpenYAMM::Import::RunawayChaosSource);
        global = ScriptedEventProgram::loadFromLuaText(source, "@runaway-chaos-controlled-142",
            ScriptedEventScope::Global, error);
        REQUIRE_MESSAGE(global.has_value(), error);
        // This is explicitly an accessible NPC fixture; unsupported Merge maps remain unmounted.
        setMapName("out01.odm");
    }

    bool executeNpcTopicEvent(uint16_t eventId, size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        previousMessageCount = state.messages.size();
        continuationCalls += continueStep.has_value() ? 1u : 0u;
        const bool executed = events.executeNpcTopicEventById(
            std::nullopt, global, eventId, state, &party, this, continueStep);
        REQUIRE_MESSAGE(executed, events.lastScriptError().value_or("NPC continuation failed"));
        apply();
        return executed;
    }

    uint32_t npcSlot(uint32_t npc, uint32_t index) const
    {
        return state.npcTopicOverrides.at(npc).at(index);
    }

    void hints(uint32_t count)
    {
        for (uint32_t index = 0; index < count; ++index)
        {
            enter(772 + index);
            topic(1793);
        }
    }

    int startRiddle(uint32_t house = 0)
    {
        enter(1092, house);
        REQUIRE(visible(1790));
        topic(1790);
        REQUIRE(state.pendingInputPrompt.has_value());
        REQUIRE_EQ(state.pendingInputPrompt->eventId, 1790);
        REQUIRE_FALSE(state.pendingInputPrompt->trimWhitespace);
        const int id = flag("PendingRiddle");
        REQUIRE(id >= 1);
        REQUIRE(id <= 11);
        CHECK_EQ(state.pendingInputPrompt->answers.at(0), texts.getText(2217 + id * 2).value());
        return id;
    }

    void answer(const std::string &value, bool accepted = true)
    {
        REQUIRE(state.pendingInputPrompt.has_value());
        GameApplicationTestAccess::submitInput(*this, value, accepted);
        CHECK_FALSE(state.pendingInputPrompt.has_value());
    }

    int solve()
    {
        const int id = startRiddle();
        answer(texts.getText(2217 + id * 2).value());
        CHECK_EQ(flag("RiddleSolved." + std::to_string(id)), 1);
        return id;
    }

    GameSaveData save() const
    {
        GameSaveData value;
        value.saveName = "142 Runaway Chaos controlled dialogue";
        value.mapFileName = "out01.odm";
        value.party = party.snapshot();
        value.namedGlobalVars = state.namedGlobalVars;
        value.savedGameMinutes = minutes;
        value.hasOutdoorRuntimeState = true;
        value.outdoorWorld.eventRuntimeState = state;
        return value;
    }

    void restore(const GameSaveData &value)
    {
        REQUIRE(value.outdoorWorld.eventRuntimeState.has_value());
        state = *value.outdoorWorld.eventRuntimeState;
        state.namedGlobalVars = value.namedGlobalVars;
        party.restoreSnapshot(value.party);
        minutes = value.savedGameMinutes;
        after();
    }
};

GameSaveData roundTrip(const GameSaveData &save, const std::string &name)
{
    const char *pTemporaryRoot = std::getenv("TMPDIR");
    REQUIRE(pTemporaryRoot != nullptr);
    const std::filesystem::path path = std::filesystem::path(pTemporaryRoot) / ("chaos142-" + name + ".oysav");
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const std::optional<GameSaveData> restored = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(restored.has_value(), error);
    return *restored;
}
}

TEST_CASE("Runaway Chaos source profile requires every original NPC topic riddle and localized hint")
{
    REQUIRE(OpenYAMM::Tests::regressionGameDataLoaded());
    const NpcDialogTable &table = OpenYAMM::Tests::regressionGameData().npcDialogTable;
    std::unordered_map<uint32_t, std::string> names;
    std::unordered_map<uint32_t, std::string> topics;
    std::unordered_map<uint32_t, std::string> texts;
    for (uint32_t id : {772u, 773u, 774u, 775u, 1092u})
    {
        REQUIRE(table.getNpc(id) != nullptr);
        names[id] = table.getNpc(id)->name;
    }
    for (uint32_t id : {1789u, 1790u, 1792u, 1793u, 1794u})
    {
        REQUIRE(table.getTopicById(id).has_value());
        topics[id] = table.getTopicById(id)->topic;
    }
    for (uint32_t id = 2218; id <= 2713; ++id)
    {
        const std::optional<std::string> value = table.getText(id);
        if (value) texts[id] = *value;
    }
    const std::filesystem::path source = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
    const std::optional<OpenYAMM::Import::RunawayChaosProfile> profile =
        OpenYAMM::Import::loadRunawayChaosProfile(source, names, topics, texts);
    REQUIRE(profile.has_value());
    CHECK_EQ(profile->sourceSha256, "0790af9b1b387b7db901f0b568593df725516e823a86fa0edb2d1fde459bce49");
    CHECK_EQ(profile->luaSource, std::string(OpenYAMM::Import::RunawayChaosSource));
    for (uint32_t id : {2218u, 2239u, 2241u, 2254u, 2265u, 2709u, 2713u})
    {
        const std::string value = texts.at(id);
        texts.erase(id);
        CHECK_THROWS(OpenYAMM::Import::loadRunawayChaosProfile(source, names, topics, texts));
        texts[id] = value;
    }
    names.erase(1092);
    CHECK_THROWS(OpenYAMM::Import::loadRunawayChaosProfile(source, names, topics, texts));
}

TEST_CASE("Runaway Chaos friend hints follow all 24 encounter orders exactly once without campaign shortcuts")
{
    std::array<uint32_t, 4> friends = {772, 773, 774, 775};
    do
    {
        ChaosFixture fixture;
        fixture.after();
        const uint64_t experience = fixture.party.member(0)->experience;
        int count = 0;
        for (uint32_t npc : friends)
        {
            fixture.enter(npc);
            CHECK_EQ(fixture.npcSlot(npc, 0), 1793);
            fixture.topic(1793);
            ++count;
            CHECK_EQ(fixture.flag("GotFQHints"), count);
            CHECK_EQ(fixture.flag("HintByNPC." + std::to_string(npc)), count);
            CHECK_EQ(fixture.flag("GotFQHint" + std::to_string(count)), 1);
            CHECK_EQ(fixture.npcSlot(npc, 0), 1789);
            REQUIRE_EQ(fixture.state.messages.size(), 1);
            CHECK(fixture.state.messages.back().find(fixture.texts.getText(2250 + count).value()) != std::string::npos);
            if (npc == 772 || npc == 774)
            {
                CHECK(fixture.state.messages.back().find("Ariel") != std::string::npos);
                CHECK(fixture.state.messages.back().find("%s") == std::string::npos);
            }
            fixture.topic(1793);
            CHECK_EQ(fixture.flag("GotFQHints"), count);
        }
        CHECK_EQ(fixture.party.member(0)->experience, experience);
        CHECK_FALSE(fixture.party.hasQuestBit(1713));
        CHECK_FALSE(fixture.party.hasQuestBit(1714));
        CHECK_FALSE(fixture.party.hasQuestBit(1715));
        CHECK_EQ(fixture.flag("QuestFinished"), 0);
        CHECK_FALSE(fixture.state.pendingMapMove.has_value());
    } while (std::next_permutation(friends.begin(), friends.end()));
}

TEST_CASE("Runaway Chaos synthetic companions use source alternate greets and NPC scope preserves other handlers")
{
    ChaosFixture fixture;
    for (uint32_t friendId = 772; friendId <= 775; ++friendId)
    {
        fixture.flag("FQMercRnd." + std::to_string(friendId), 1);
        fixture.enter(friendId);
        fixture.topic(1793);
        const uint32_t greetId = friendId % 2 == 0 ? 2245 : 2246;
        CHECK(fixture.state.messages.back().starts_with(fixture.texts.getText(greetId).value()));
    }
    fixture.enter(67);
    const uint32_t gold = fixture.party.gold();
    fixture.topic(1790);
    CHECK_EQ(fixture.party.gold(), gold + 17);
    CHECK_EQ(fixture.flag("RiddlesAnswered"), 0);
    CHECK_FALSE(fixture.state.pendingInputPrompt.has_value());
    fixture.flag("QuestFinished", 1);
    fixture.enter(1092);
    CHECK_FALSE(fixture.visible(1790));
    CHECK_FALSE(fixture.visible(1794));
    fixture.enter(772);
    CHECK_FALSE(fixture.visible(1793));
}

TEST_CASE("Runaway Chaos exact input goes through application matcher and real Lua continuation")
{
    ChaosFixture fixture(true); // Declared synthetic ASCII answers isolate original lower-only matching.
    fixture.after();
    fixture.enter(1092);
    CHECK_FALSE(fixture.visible(1794));
    CHECK_FALSE(fixture.visible(1790));
    fixture.hints(2);
    fixture.enter(1092);
    CHECK(fixture.visible(1794));
    CHECK_FALSE(fixture.visible(1790));
    fixture.topic(1794);
    CHECK_EQ(fixture.flag("CoughtChaos"), 0);
    fixture.enter(774);
    fixture.topic(1793);

    fixture.startRiddle();
    fixture.answer("wrong");
    CHECK_EQ(fixture.flag("RiddlesAnswered"), 0);
    CHECK_EQ(fixture.flag("PendingRiddle"), 0);
    fixture.startRiddle();
    const std::string spacedAnswer = " " + fixture.state.pendingInputPrompt->answers.front() + " ";
    fixture.answer(spacedAnswer);
    CHECK_EQ(fixture.flag("RiddlesAnswered"), 0);
    fixture.startRiddle();
    const uint32_t calls = fixture.continuationCalls;
    fixture.answer("ignored", false);
    CHECK_EQ(fixture.continuationCalls, calls);
    CHECK_EQ(fixture.flag("RiddlesAnswered"), 0);
    const int id = fixture.startRiddle();
    std::string answer = fixture.state.pendingInputPrompt->answers.front();
    std::transform(answer.begin(), answer.end(), answer.begin(), [](unsigned char value)
    {
        return char(std::toupper(value));
    });
    fixture.answer(answer);
    CHECK_EQ(fixture.flag("RiddlesAnswered"), 1);
    CHECK_EQ(fixture.flag("RiddleSolved." + std::to_string(id)), 1);
    CHECK_EQ(fixture.state.npcPictureOverrides.at(1092), 358);
    CHECK_EQ(fixture.state.npcProfessionOverrides.at(1092), 33);
    CHECK_EQ(fixture.state.npcNameOverrides.at(1092), fixture.texts.getText(2710).value());
}

TEST_CASE("Runaway Chaos five distinct answers require two capture topics and exactly one real follower")
{
    ChaosFixture fixture;
    fixture.after();
    fixture.hints(3);
    std::unordered_set<int> ids;
    const uint64_t experience = fixture.party.member(0)->experience;
    for (int count = 1; count <= 5; ++count)
    {
        CHECK(ids.insert(fixture.solve()).second);
        CHECK_EQ(fixture.flag("RiddlesAnswered"), count);
        fixture.enter(1092);
        if (count < 5)
        {
            fixture.topic(1794);
            CHECK_EQ(fixture.flag("CoughtChaos"), 0);
            CHECK(fixture.state.hiredNpcFollowers.empty());
        }
    }
    fixture.enter(1092);
    CHECK_FALSE(fixture.visible(1790));
    CHECK(fixture.visible(1794));
    fixture.topic(1792); // Cannot skip the offer by invoking its second event id directly.
    CHECK_EQ(fixture.flag("CoughtChaos"), 0);
    fixture.topic(1794);
    CHECK_EQ(fixture.flag("CoughtChaos"), 0);
    CHECK_EQ(fixture.npcSlot(1092, 0), 1792);
    fixture.topic(1792);
    CHECK_EQ(fixture.flag("CoughtChaos"), 1);
    REQUIRE_EQ(fixture.state.hiredNpcFollowers.size(), 1);
    CHECK_EQ(fixture.state.hiredNpcFollowers.front().npcId, 1092);
    CHECK_EQ(fixture.state.npcHouseOverrides.at(1092), 0);
    fixture.topic(1792);
    REQUIRE_EQ(fixture.party.snapshot().hiredNpcFollowers.size(), 1);
    CHECK_EQ(fixture.party.member(0)->experience, experience);
    CHECK_EQ(fixture.flag("QuestFinished"), 0);
    CHECK_FALSE(fixture.state.pendingMapMove.has_value());

    for (int count = 6; count <= 11; ++count)
    {
        CHECK(ids.insert(fixture.solve()).second);
        CHECK_EQ(fixture.flag("RiddlesAnswered"), count);
    }
    CHECK_EQ(ids.size(), 11);
    fixture.enter(1092);
    CHECK(fixture.visible(1790)); // Original captured Chaos continues riddles, even when all are exhausted.
    fixture.topic(1790);
    CHECK_FALSE(fixture.state.pendingInputPrompt.has_value());
    CHECK_EQ(fixture.state.messages.back(), fixture.texts.getText(2263).value());
}

TEST_CASE("Runaway Chaos cold load discards UI input while solved riddles and follower survive")
{
    ChaosFixture initial;
    initial.after();
    initial.hints(3);
    initial.solve();
    initial.solve();
    const int abandoned = initial.startRiddle(712);
    initial.after(); // Re-running load hooks must not invalidate a live UI prompt.
    REQUIRE(initial.state.pendingInputPrompt.has_value());
    CHECK_EQ(initial.flag("PendingRiddle"), abandoned);
    const GameSaveData first = roundTrip(initial.save(), "pending");
    ChaosFixture resumed;
    resumed.restore(first);
    CHECK_FALSE(resumed.state.pendingInputPrompt.has_value());
    CHECK_EQ(resumed.flag("PendingRiddle"), 0);
    CHECK_EQ(resumed.flag("RiddlesAnswered"), 2);
    GameApplicationTestAccess::submitInput(resumed, resumed.texts.getText(2217 + abandoned * 2).value());
    CHECK_EQ(resumed.continuationCalls, 0); // Discarded UI input cannot answer a saved riddle.
    CHECK_EQ(resumed.flag("RiddlesAnswered"), 2);
    const int pending = resumed.startRiddle(712);
    resumed.answer(resumed.texts.getText(2217 + pending * 2).value());
    CHECK_EQ(resumed.flag("RiddlesAnswered"), 3);
    CHECK_EQ(resumed.state.npcHouseOverrides.at(1092), 0);
    resumed.solve();
    resumed.solve();
    resumed.enter(1092);
    resumed.topic(1794);
    CHECK_EQ(resumed.npcSlot(1092, 0), 1792);
    const GameSaveData offered = roundTrip(resumed.save(), "offered");
    ChaosFixture coldOffer;
    coldOffer.restore(offered);
    coldOffer.enter(1092);
    CHECK_EQ(coldOffer.npcSlot(1092, 0), 1794); // Source unsaved topic text does not bypass confirmation.
    coldOffer.topic(1794);
    coldOffer.topic(1792);
    const GameSaveData caught = roundTrip(coldOffer.save(), "caught");
    ChaosFixture coldCaught;
    coldCaught.restore(caught);
    CHECK_EQ(coldCaught.flag("CoughtChaos"), 1);
    CHECK_EQ(coldCaught.flag("RiddlesAnswered"), 5);
    REQUIRE_EQ(coldCaught.party.snapshot().hiredNpcFollowers.size(), 1);
    CHECK_EQ(coldCaught.party.snapshot().hiredNpcFollowers.front().npcId, 1092);
    CHECK_EQ(coldCaught.flag("QuestFinished"), 0);
    coldCaught.enter(1092);
    CHECK(coldCaught.visible(1790));
    CHECK_FALSE(coldCaught.visible(1794));
    coldCaught.solve();
    CHECK_EQ(coldCaught.flag("RiddlesAnswered"), 6);
}
