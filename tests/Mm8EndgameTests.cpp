#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "CrossContinentsIntroSource.h"
#include "game/app/GameApplication.h"
#include "game/events/EventDialogContent.h"
#include "game/events/ISceneEventContext.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"
#include "tools/LegacyLuaExport.h"
#include "tools/mmerge_import/LodResources.h"
#include "tools/mmerge_import/Mm8EndgameResources.h"
#include "tools/mmerge_import/TextResources.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

namespace OpenYAMM::Game
{
struct GameApplicationTestAccess
{
    static void submitInput(IGameplayWorldRuntime &world, const std::string &answer, const EventDialogContent &dialog,
        bool accepted = true)
    {
        GameApplication application(Engine::ApplicationConfig::createDefault());
        application.m_gameDataRepository.bind(application.m_gameDataLoader);
        application.m_gameSession.bindDataRepository(&application.m_gameDataRepository);
        application.m_gameSession.bindActiveWorldRuntime(&world);
        application.m_gameSession.gameplayScreenRuntime().activeEventDialog() = dialog;
        application.m_pendingInputText = answer;
        application.finishPendingInputPrompt(accepted);
        application.m_gameSession.bindActiveWorldRuntime(nullptr);
    }
};
}

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
using Bytes = std::vector<uint8_t>;

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

std::filesystem::path outputPath(const std::string &name)
{
    const char *pRoot = std::getenv("TMPDIR");
    REQUIRE_MESSAGE(pRoot != nullptr, "MM8 endgame tests require external-storage TMPDIR");
    return std::filesystem::path(pRoot) / name;
}

std::filesystem::path uniqueFixture(const std::string &name)
{
    const std::string nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path path = outputPath(name + "-" + nonce);
    std::filesystem::create_directories(path / "Data");
    return path;
}

Bytes originalGlobal()
{
    return readLodText(sourceRoot() / "Data/mmmerge.T.lod", "global.evt");
}

Bytes originalAnswers(bool chinese)
{
    return readLodText(sourceRoot() / "Data" / (chinese ? "10 LocZHCN.EnglishT.lod" : "mmmerge.T.lod"), "d35.str");
}

void put32(Bytes &bytes, size_t offset, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
    {
        bytes.at(offset + index) = uint8_t(value >> (index * 8));
    }
}

// Small container fixture: every accepted payload remains the exact original resource.
// Avoid copying complete multi-megabyte archives for language/negative profile checks.
void writeArchive(const std::filesystem::path &path, const std::map<std::string, Bytes> &resources)
{
    Bytes archive(288 + resources.size() * 32, 0);
    const std::string signature("LOD\0", 4);
    std::copy(signature.begin(), signature.end(), archive.begin());
    const std::string version = "MMVI";
    std::copy(version.begin(), version.end(), archive.begin() + 4);
    put32(archive, 272, 288);
    put32(archive, 284, uint32_t(resources.size()));
    size_t index = 0;
    for (const auto &[name, payload] : resources)
    {
        REQUIRE(name.size() < 16);
        const size_t entry = 288 + index++ * 32;
        std::copy(name.begin(), name.end(), archive.begin() + entry);
        put32(archive, entry + 16, uint32_t(archive.size() - 288));
        put32(archive, entry + 20, uint32_t(48 + payload.size()));
        Bytes wrapped(48, 0);
        std::copy(name.begin(), name.end(), wrapped.begin());
        put32(wrapped, 20, uint32_t(payload.size()));
        archive.insert(archive.end(), wrapped.begin(), wrapped.end());
        archive.insert(archive.end(), payload.begin(), payload.end());
    }
    put32(archive, 276, uint32_t(archive.size() - 288));
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(archive.data()), std::streamsize(archive.size()));
    REQUIRE(file.good());
}

Mm8EndgameProfile profileForLanguage(bool chinese)
{
    const std::filesystem::path fixture = uniqueFixture(chinese ? "escaton-chinese" : "escaton-english");
    writeArchive(fixture / "Data/mmmerge.T.lod", {{"global.evt", originalGlobal()}, {"d35.str", originalAnswers(false)}});
    if (chinese)
    {
        writeArchive(fixture / "Data/10 LocZHCN.EnglishT.lod", {{"d35.str", originalAnswers(true)}});
    }
    const auto profile = loadMm8EndgameProfile(fixture);
    REQUIRE(profile);
    return *profile;
}

EvtProgram originalEvents(uint16_t first, uint16_t last)
{
    const Bytes original = originalGlobal();
    Bytes selected;
    for (size_t offset = 0; offset < original.size();)
    {
        const size_t size = size_t(original.at(offset)) + 1;
        REQUIRE(size >= 5);
        REQUIRE(offset + size <= original.size());
        const uint16_t event = uint16_t(original[offset + 1]) | uint16_t(original[offset + 2]) << 8;
        if (event >= first && event <= last)
        {
            selected.insert(selected.end(), original.begin() + offset, original.begin() + offset + size);
        }
        offset += size;
    }
    EvtProgram result;
    REQUIRE(result.loadFromBytes(selected));
    return result;
}

const OpenYAMM::Tests::RegressionGameData &data()
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(), OpenYAMM::Tests::regressionGameDataFailure());
    return OpenYAMM::Tests::regressionGameData();
}

LegacyLuaExportLookups questionLookups()
{
    LegacyLuaExportLookups result;
    for (uint32_t id = 1; id <= 3000; ++id)
    {
        const auto text = data().npcDialogTable.getText(id);
        if (text) result.npcTexts.emplace(id, *text);
    }
    return result;
}

std::optional<ScriptedEventProgram> exported(const EvtProgram &evt, const StrTable &str,
    const LegacyLuaExportLookups &lookups, LegacyLuaExportScope scope, bool includeContinentBridge = false)
{
    const std::string source = generateLegacyEventLuaChunk(evt, str, lookups, scope, LegacyEventVersion::Mm8);
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(std::string(EventSupportSource) + "\n" + source
            + (includeContinentBridge ? "\n" + std::string(CrossContinentsIntroSource) : ""),
        "@mm8-endgame-original-export", scope == LegacyLuaExportScope::Global
            ? ScriptedEventScope::Global : ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    return program;
}

std::optional<ScriptedEventProgram> globalProgram(const Mm8EndgameProfile &profile)
{
    LegacyLuaExportLookups lookups = questionLookups();
    lookups.inputStringAnswerBindings = profile.inputStringAnswerBindings;
    return exported(originalEvents(159, 171), {}, lookups, LegacyLuaExportScope::Global, true);
}

std::optional<ScriptedEventProgram> mapProgram(const std::string &stem)
{
    EvtProgram evt;
    StrTable str;
    REQUIRE(evt.loadFromBytes(readLodText(sourceRoot() / "Data/mmmerge.T.lod", stem + ".evt")));
    REQUIRE(str.loadFromNullSeparatedBytes(readLodText(sourceRoot() / "Data/mmmerge.T.lod", stem + ".str")));
    return exported(evt, str, questionLookups(), LegacyLuaExportScope::Map);
}

class EndgameFixture : public OpenYAMM::Tests::PartySpellTestWorldRuntime, public ISceneEventContext
{
public:
    Party party;
    EventRuntimeState state;
    EventRuntime events{&data().houseTable, &data().npcDialogTable};
    std::optional<ScriptedEventProgram> global;
    std::optional<ScriptedEventProgram> local;
    uint32_t continuations = 0;
    uint8_t lastContinuation = 0;
    float minutes = 600;
    std::map<std::string, EventRuntimeState> mapStates;
    std::vector<std::array<uint32_t, 3>> facetChanges;

    explicit EndgameFixture(const Mm8EndgameProfile &profile) : global(globalProgram(profile))
    {
        PartySeed seed = OpenYAMM::Tests::createSpellRegressionPartySeed();
        seed.members.push_back(OpenYAMM::Tests::makeSpellRegressionPartyMember("Fifth", "Knight", "PC01-01", 8));
        party.setItemTable(&data().itemTable);
        party.seed(seed);
        state.activeHistoryContinentId = 1;
        state.normalGameplayScreen = true;
        bindParty(&party);
        bindEventRuntimeState(&state);
        bindGlobalEventProgram(&global);
        selectMap("d35");
    }

    float currentGameMinutes() const override { return minutes; }
    const MapDeltaData *mapDeltaData() const override { return nullptr; }
    MapDeltaData *mapDeltaData() override { return nullptr; }
    bool setFacetBit(uint32_t facet, uint32_t bit, bool enabled) override
    {
        facetChanges.push_back({facet, bit, uint32_t(enabled)});
        return true;
    }
    bool castEventSpell(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        int32_t, int32_t, int32_t) override { return false; }
    bool summonMonsters(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        uint32_t, uint32_t) override { return false; }
    bool summonEventItem(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool summonEventObject(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool checkMonstersKilled(uint32_t, uint32_t, uint32_t, bool) const override { return false; }

    void apply() { party.applyEventRuntimeState(state, false); }

    void selectMap(const std::string &stem)
    {
        const std::string name = stem + (stem == "pbp" || stem == "out02" ? ".odm" : ".blv");
        if (!state.mapFileName.empty()) mapStates[state.mapFileName] = state;
        const auto globals = state.namedGlobalVars;
        const auto history = state.historyEventTimesByContinent;
        const auto legacyHistory = state.historyEventTimes;
        state = mapStates.contains(name) ? mapStates.at(name) : EventRuntimeState{};
        state.namedGlobalVars = globals;
        state.historyEventTimesByContinent = history;
        state.historyEventTimes = legacyHistory;
        state.activeHistoryContinentId = 1;
        state.normalGameplayScreen = true;
        state.pendingDialogueContext.reset();
        state.pendingMapMove.reset();
        state.pendingMovie.reset();
        state.pendingInputPrompt.reset();
        state.mapFileName = name;
        party.applyGlobalNpcStateTo(state);
        local = mapProgram(stem);
        setIndoorMap(name.ends_with(".blv"));
        setMapName(name);
    }

    void loadEvents()
    {
        REQUIRE_MESSAGE(events.executeOnLoadEvents(local, global, state, &party, this),
            events.lastScriptError().value_or("Map load failed"));
        apply();
        REQUIRE_MESSAGE(events.executeGlobalAfterLoadEvents(local, global, state, &party, this),
            events.lastScriptError().value_or("Continent bridge failed"));
        apply();
    }

    void transition(uint16_t eventId, const std::string &target)
    {
        mapEvent(eventId);
        REQUIRE(state.pendingDialogueContext);
        REQUIRE(state.pendingDialogueContext->transitionMapMove);
        const auto move = *state.pendingDialogueContext->transitionMapMove;
        CHECK_EQ(move.mapName, std::optional<std::string>(target));
        REQUIRE(events.executeOnLeaveEvents(local, global, state, &party, this));
        apply();
        selectMap(std::filesystem::path(target).stem().string());
        loadEvents();
    }

    bool resident(uint32_t npc, uint32_t house)
    {
        const HouseEntry *pHouse = data().houseTable.get(house);
        REQUIRE(pHouse != nullptr);
        const auto residents = collectSelectableResidentNpcIds(*pHouse, data().npcDialogTable, state);
        return std::find(residents.begin(), residents.end(), npc) != residents.end();
    }

    void coldReload(const Mm8EndgameProfile &profile, const std::string &name)
    {
        GameSaveData save;
        const bool indoor = state.mapFileName.ends_with(".blv");
        save.currentSceneKind = indoor ? SceneKind::Indoor : SceneKind::Outdoor;
        save.mapFileName = state.mapFileName;
        save.party = party.snapshot();
        save.namedGlobalVars = state.namedGlobalVars;
        save.savedGameMinutes = minutes;
        save.hasIndoorSceneState = indoor;
        save.hasOutdoorRuntimeState = !indoor;
        mapStates[state.mapFileName] = state;
        for (const auto &[map, mapState] : mapStates)
        {
            if (map.ends_with(".blv"))
            {
                auto &scene = save.indoorSceneStates[map];
                scene.eventRuntimeState = mapState;
                scene.worldRuntime.gameMinutes = minutes;
                if (map == state.mapFileName) save.indoorScene = scene;
            }
            else
            {
                auto &world = save.outdoorWorldStates[map];
                world.eventRuntimeState = mapState;
                world.gameMinutes = minutes;
                if (map == state.mapFileName) save.outdoorWorld = world;
            }
        }
        const auto path = uniqueFixture(name) / "cold.oysav";
        std::string error;
        REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
        const auto loaded = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(loaded, error);
        CHECK_EQ(loaded->party.questBits, save.party.questBits);
        CHECK_EQ(loaded->namedGlobalVars, save.namedGlobalVars);
        party.restoreSnapshot(loaded->party);
        mapStates.clear();
        for (const auto &[map, scene] : loaded->indoorSceneStates)
        {
            REQUIRE(scene.eventRuntimeState);
            mapStates[map] = *scene.eventRuntimeState;
        }
        for (const auto &[map, world] : loaded->outdoorWorldStates)
        {
            REQUIRE(world.eventRuntimeState);
            mapStates[map] = *world.eventRuntimeState;
        }
        state = mapStates.at(loaded->mapFileName);
        // Scene identity is reconstructed from loaded map metadata by the real scene runtimes.
        // EventRuntimeState does not serialize mapFileName; restoring it is not rerunning OnLoad.
        state.mapFileName = loaded->mapFileName;
        state.normalGameplayScreen = true;
        setIndoorMap(loaded->currentSceneKind == SceneKind::Indoor);
        setMapName(loaded->mapFileName);
        REQUIRE(state.mapFileName == save.mapFileName);
        state.namedGlobalVars = loaded->namedGlobalVars;
        party.applyGlobalNpcStateTo(state);
        minutes = float(loaded->savedGameMinutes);
        events = EventRuntime(&data().houseTable, &data().npcDialogTable);
        global = globalProgram(profile);
        local = mapProgram(std::filesystem::path(loaded->mapFileName).stem().string());
        // A cold save load restores map state; it must not rerun quest events to recover it.
    }

    void mapEvent(uint16_t eventId, bool timer = false)
    {
        REQUIRE_MESSAGE(events.executeEventById(local, global, eventId, state, &party, this,
            std::nullopt, false, ScriptedEventScope::Map, timer), events.lastScriptError().value_or("Map event failed"));
        apply();
    }

    bool executeNpcTopicEvent(uint16_t eventId, size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        previousMessageCount = state.messages.size();
        continuations += continueStep.has_value();
        if (continueStep) lastContinuation = *continueStep;
        const bool ok = events.executeNpcTopicEventById(local, global, eventId, state, &party, this, continueStep);
        REQUIRE_MESSAGE(ok, events.lastScriptError().value_or("NPC continuation failed"));
        apply();
        return ok;
    }

    EventDialogContent dialogue()
    {
        return buildEventDialogContent(state, state.messages.size(), true, &global, &data().houseTable,
            &data().classSkillTable, &data().npcDialogTable, &data().transitionTable, nullptr, nullptr,
            &party, this, minutes);
    }

    bool hasTopic(uint16_t id)
    {
        const EventDialogContent dialog = dialogue();
        return std::any_of(dialog.actions.begin(), dialog.actions.end(), [&](const EventDialogAction &action)
        {
            return action.kind == EventDialogActionKind::NpcTopic && action.id == id && action.enabled;
        });
    }

    void enterNpc(uint32_t npc, uint32_t house)
    {
        mapEvent(201);
        REQUIRE(state.pendingDialogueContext);
        CHECK_EQ(state.pendingDialogueContext->sourceId, house);
        REQUIRE(resident(npc, house));
        state.pendingDialogueContext->kind = DialogueContextKind::NpcTalk;
        state.pendingDialogueContext->sourceId = npc;
        state.pendingDialogueContext->hostHouseId = house;
    }

    void topic(uint16_t id)
    {
        REQUIRE_MESSAGE(hasTopic(id), "Topic must be naturally selectable before execution");
        const auto context = state.pendingDialogueContext;
        size_t ignored = 0;
        REQUIRE(executeNpcTopicEvent(id, ignored));
        state.pendingDialogueContext = context;
    }

    void answer(const std::string &text, bool accepted = true)
    {
        REQUIRE(state.pendingInputPrompt);
        GameApplicationTestAccess::submitInput(*this, text, dialogue(), accepted);
        CHECK_FALSE(state.pendingInputPrompt);
    }

    void startQuestions()
    {
        enterNpc(22, 775);
        topic(159);
        topic(163);
    }

    void solve(const Mm8EndgameProfile &profile, const std::array<uint16_t, 3> &order, size_t answerIndex)
    {
        startQuestions();
        for (size_t index = 0; index < order.size(); ++index)
        {
            if (index != 0) topic(171);
            const uint16_t id = order[index];
            topic(id);
            REQUIRE(state.pendingInputPrompt);
            CHECK_EQ(state.pendingInputPrompt->answers, profile.inputStringAnswerBindings.at(id - 164).answers);
            answer(profile.inputStringAnswerBindings.at(id - 164).answers.at(answerIndex));
            CHECK_EQ(lastContinuation, 4);
            CHECK_EQ(party.hasQuestBit(97), index == 2);
            CHECK_EQ(party.inventoryItemCount(629) + party.pendingItemCount(629), index == 2 ? 1 : 0);
        }
    }
};

std::vector<std::string> promptAnswers(const LegacyLuaExportLookups &lookups, LegacyLuaExportScope scope,
    const StrTable &strings = {})
{
    const auto program = exported(originalEvents(164, 164), strings, lookups, scope);
    Party party;
    party.seed(OpenYAMM::Tests::createSpellRegressionPartySeed());
    EventRuntimeState state;
    EventRuntime runtime;
    const std::optional<ScriptedEventProgram> none;
    REQUIRE(runtime.executeEventById(scope == LegacyLuaExportScope::Map ? program : none,
        scope == LegacyLuaExportScope::Global ? program : none, 164, state, &party));
    REQUIRE(state.pendingInputPrompt);
    REQUIRE(state.pendingInputPrompt->text.has_value());
    REQUIRE_FALSE(state.pendingInputPrompt->text->empty());
    return state.pendingInputPrompt->answers;
}
}

TEST_CASE("Escaton profile selects audited current-language map answers and records the winning source")
{
    for (bool chinese : {false, true})
    {
        const Mm8EndgameProfile profile = profileForLanguage(chinese);
        CHECK_EQ(profile.chineseAnswers, chinese);
        CHECK_EQ(profile.globalEventArchive, "Data/mmmerge.T.lod");
        CHECK_EQ(profile.answerArchive, chinese ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod");
        REQUIRE_EQ(profile.inputStringAnswerBindings.size(), 3);
        StrTable str;
        REQUIRE(str.loadFromNullSeparatedBytes(originalAnswers(chinese)));
        for (size_t index = 0; index < 3; ++index)
        {
            const auto &binding = profile.inputStringAnswerBindings[index];
            CHECK_EQ(binding.scope, LegacyLuaExportScope::Global);
            CHECK_EQ(binding.eventId, 164 + index);
            CHECK_EQ(binding.step, 1);
            std::vector<std::string> expected;
            for (size_t id : {100 + index * 2, 101 + index * 2})
            {
                const std::string raw = *str.get(id);
                size_t glyphs = 0;
                expected.push_back(decodeLegacyText({raw.begin(), raw.end()}, chinese, glyphs));
            }
            CHECK_EQ(binding.answers, expected);
        }
    }
    const auto production = loadMm8EndgameProfile(sourceRoot());
    REQUIRE(production);
    CHECK(production->chineseAnswers);
}

TEST_CASE("Escaton profile rejects changed missing mislabeled and escaping answer sources")
{
    const auto path = uniqueFixture("escaton-invalid");
    CHECK_FALSE(loadMm8EndgameProfile(path));
    writeArchive(path / "Data/mmmerge.T.lod", {{"global.evt", originalGlobal()}});
    CHECK_THROWS_AS(loadMm8EndgameProfile(path), std::runtime_error);
    Bytes changedGlobal = originalGlobal();
    changedGlobal.at(0) ^= 1;
    writeArchive(path / "Data/mmmerge.T.lod", {{"global.evt", changedGlobal}, {"d35.str", originalAnswers(false)}});
    CHECK_THROWS_AS(loadMm8EndgameProfile(path), std::runtime_error);
    Bytes changed = originalAnswers(false);
    changed.at(0) ^= 1;
    writeArchive(path / "Data/mmmerge.T.lod", {{"global.evt", originalGlobal()}, {"d35.str", changed}});
    CHECK_THROWS_AS(loadMm8EndgameProfile(path), std::runtime_error);
    writeArchive(path / "Data/mmmerge.T.lod", {{"global.evt", originalGlobal()}, {"d35.str", originalAnswers(false)}});
    writeArchive(path / "Data/10 LocZHCN.EnglishT.lod", {{"d35.str", originalAnswers(false)}});
    CHECK_THROWS_AS(loadMm8EndgameProfile(path), std::runtime_error);
    writeArchive(path / "Data/10 LocZHCN.EnglishT.lod", {{"global.evt", originalGlobal()}});
    CHECK_THROWS_AS(loadMm8EndgameProfile(path), std::runtime_error);
    std::filesystem::remove(path / "Data/10 LocZHCN.EnglishT.lod");
    std::filesystem::create_symlink(sourceRoot() / "Data/10 LocZHCN.EnglishT.lod", path / "Data/10 LocZHCN.EnglishT.lod");
    CHECK_THROWS_AS(loadMm8EndgameProfile(path), std::runtime_error);
}

TEST_CASE("Explicit input bindings reject duplicate instruction keys and empty answer definitions")
{
    LegacyLuaExportLookups lookups;
    lookups.inputStringAnswerBindings = profileForLanguage(true).inputStringAnswerBindings;
    lookups.inputStringAnswerBindings.push_back(lookups.inputStringAnswerBindings.front());
    CHECK_THROWS_AS(generateLegacyEventLuaChunk(originalEvents(164, 164), {}, lookups,
        LegacyLuaExportScope::Global), std::runtime_error);
    lookups.inputStringAnswerBindings.pop_back();
    lookups.inputStringAnswerBindings.front().answers.clear();
    CHECK_THROWS_AS(generateLegacyEventLuaChunk(originalEvents(164, 164), {}, lookups,
        LegacyLuaExportScope::Global), std::runtime_error);
    lookups.inputStringAnswerBindings.front().answers = {"answer", ""};
    CHECK_THROWS_AS(generateLegacyEventLuaChunk(originalEvents(164, 164), {}, lookups,
        LegacyLuaExportScope::Global), std::runtime_error);
}

TEST_CASE("Global input binding does not capture map164 another event or a different instruction step")
{
    // Keep the original question text present, as the production global importer does.
    LegacyLuaExportLookups lookups = questionLookups();
    lookups.inputStringAnswerTexts = {{100, "legacy one"}, {101, "legacy two"}};
    const std::vector<std::string> legacy{"legacy one", "legacy two"};
    CHECK_EQ(promptAnswers(lookups, LegacyLuaExportScope::Global), legacy);
    lookups.inputStringAnswerBindings = profileForLanguage(true).inputStringAnswerBindings;
    CHECK_EQ(promptAnswers(lookups, LegacyLuaExportScope::Global),
        lookups.inputStringAnswerBindings.front().answers);
    CHECK_EQ(promptAnswers(lookups, LegacyLuaExportScope::Map), legacy);
    lookups.inputStringAnswerBindings.front().step = 2;
    CHECK_EQ(promptAnswers(lookups, LegacyLuaExportScope::Global), legacy);
    lookups.inputStringAnswerBindings.front().step = 1;
    lookups.inputStringAnswerBindings.front().eventId = 163;
    CHECK_EQ(promptAnswers(lookups, LegacyLuaExportScope::Global), legacy);
    Bytes text;
    for (size_t index = 0; index <= 101; ++index)
    {
        const std::string value = index >= 100 ? "map source " + std::to_string(index) : "";
        text.insert(text.end(), value.begin(), value.end());
        text.push_back(0);
    }
    StrTable str;
    REQUIRE(str.loadFromNullSeparatedBytes(text));
    const std::vector<std::string> mapAnswers{"map source 100", "map source 101"};
    CHECK_EQ(promptAnswers(lookups, LegacyLuaExportScope::Map, str), mapAnswers);
}

TEST_CASE("Escaton missing export answers reproduce real input failure and exact binding repairs the same path")
{
    const Mm8EndgameProfile profile = profileForLanguage(true);
    Mm8EndgameProfile missing = profile;
    missing.inputStringAnswerBindings.clear();
    EndgameFixture f(missing);
    f.startQuestions();
    f.topic(164);
    REQUIRE(f.state.pendingInputPrompt);
    CHECK(f.state.pendingInputPrompt->answers.empty());
    f.answer(profile.inputStringAnswerBindings.front().answers.back());
    CHECK_EQ(f.lastContinuation, 2);
    CHECK_FALSE(f.party.hasQuestBit(94));
    CHECK_FALSE(f.party.hasQuestBit(97));
    CHECK_FALSE(f.party.hasItemAnywhere(629));
    f.global = globalProgram(profile);
    f.topic(171);
    f.topic(164);
    f.answer(profile.inputStringAnswerBindings.front().answers.back());
    CHECK_EQ(f.lastContinuation, 4);
    CHECK(f.party.hasQuestBit(94));
}

TEST_CASE("Escaton wrong and cancelled inputs never enter a success continuation or award the key")
{
    const Mm8EndgameProfile profile = profileForLanguage(true);
    for (uint16_t id : {164, 165, 166})
    {
        EndgameFixture f(profile);
        f.startQuestions();
        f.topic(id);
        f.answer("definitely-wrong");
        CHECK_EQ(f.lastContinuation, 2);
        CHECK_FALSE(f.party.hasQuestBit(id - 70));
        CHECK_FALSE(f.party.hasQuestBit(97));
        CHECK_FALSE(f.party.hasItemAnywhere(629));
        f.topic(171);
        f.topic(id);
        const uint32_t before = f.continuations;
        f.answer(profile.inputStringAnswerBindings.at(id - 164).answers.front(), false);
        CHECK_EQ(f.continuations, before);
        CHECK_FALSE(f.party.hasQuestBit(97));
        CHECK_FALSE(f.party.hasItemAnywhere(629));
    }
}

TEST_CASE("Six Escaton question orders and both source aliases produce exactly one key through production input")
{
    for (bool chinese : {false, true})
    {
        const Mm8EndgameProfile profile = profileForLanguage(chinese);
        for (size_t alias = 0; alias < 2; ++alias)
        {
            std::array<uint16_t, 3> order{164, 165, 166};
            size_t permutations = 0;
            do
            {
                EndgameFixture f(profile);
                f.solve(profile, order, alias);
                CHECK_EQ(f.continuations, 3);
                for (uint32_t bit : {47, 48, 50, 52, 54, 97, 220}) CHECK(f.party.hasQuestBit(bit));
                CHECK_FALSE(f.party.hasQuestBit(46));
                CHECK_FALSE(f.party.hasQuestBit(56));
                CHECK_FALSE(f.party.hasQuestBit(228));
                f.topic(171);
                for (uint16_t id : {164, 165, 166}) CHECK_FALSE(f.hasTopic(id));
                f.enterNpc(22, 775);
                f.topic(171);
                for (uint16_t id : {164, 165, 166}) CHECK_FALSE(f.hasTopic(id));
                CHECK_EQ(f.party.inventoryItemCount(629) + f.party.pendingItemCount(629), 1);
                ++permutations;
            } while (std::next_permutation(order.begin(), order.end()));
            CHECK_EQ(permutations, 6);
        }
    }
}

TEST_CASE("Crystal original timer and nine input events unlock pbp without setting MapVar12 directly")
{
    const Mm8EndgameProfile profile = profileForLanguage(true);
    EndgameFixture f(profile);
    f.selectMap("d10");
    f.mapEvent(502);
    CHECK_FALSE(f.state.pendingDialogueContext);
    f.mapEvent(152);
    CHECK_EQ(f.state.mapVars[11], 1);
    CHECK_EQ(f.state.mapVars[12], 1);
    auto timers = reconcileScriptedEventTimers({}, f.local->timerTriggers(), f.minutes, 0, false);
    for (size_t tick = 0; tick < 10; ++tick)
    {
        f.minutes += 0.5f;
        REQUIRE(updateScriptedEventTimers(timers, f.minutes, [&](const ScriptedEventTimerDefinition &timer)
        {
            CHECK_EQ(timer.eventId, 151);
            f.mapEvent(timer.eventId, true);
            return true;
        }));
    }
    CHECK_EQ(f.state.mapVars[11], 0);
    f.mapEvent(155); // Incorrect first colour resets to the source initial step.
    CHECK_EQ(f.state.mapVars[12], 1);
    uint8_t expected = 1;
    for (uint16_t id : {153, 154, 155, 153, 155, 154, 153, 154, 153})
    {
        f.mapEvent(id);
        CHECK_EQ(f.state.mapVars[12], ++expected);
        if (expected == 5)
        {
            f.coldReload(profile, "mm8-crystal-mid-sequence");
            CHECK_EQ(f.state.mapVars[12], 5);
        }
    }
    f.coldReload(profile, "mm8-crystal-unlocked");
    f.loadEvents();
    CHECK_EQ(f.state.mapVars[12], 10);
    f.mapEvent(502);
    REQUIRE(f.state.pendingDialogueContext);
    REQUIRE(f.state.pendingDialogueContext->transitionMapMove);
    const auto &move = *f.state.pendingDialogueContext->transitionMapMove;
    CHECK_EQ(move.mapName, std::optional<std::string>("pbp.odm"));
    CHECK_EQ(move.x, 1395);
    CHECK_EQ(move.y, 20751);
    CHECK_EQ(move.z, 1152);
    CHECK_EQ(move.directionDegrees, std::optional<int32_t>(270));
}

namespace
{
struct Lord
{
    uint16_t gate;
    const char *pMap;
    uint32_t npc;
    uint32_t house;
    uint16_t topic;
    uint32_t quest;
    uint32_t completed;
    uint32_t award;
};

constexpr std::array<Lord, 4> Lords{{
    {502, "d36.blv", 26, 661, 167, 54, 55, 14},
    {504, "d38.blv", 24, 662, 168, 52, 53, 15},
    {505, "d39.blv", 25, 663, 169, 50, 51, 16},
    {503, "d37.blv", 23, 664, 170, 48, 49, 17},
}};

void checkReleased(EndgameFixture &f, const Lord &lord)
{
    CHECK_FALSE(f.resident(lord.npc, lord.house));
    REQUIRE(f.state.npcTopicOverrides.contains(lord.npc));
    CHECK_EQ(f.state.npcTopicOverrides.at(lord.npc).at(0), 0);
    REQUIRE(f.state.npcHouseOverrides.contains(lord.npc));
    CHECK_EQ(f.state.npcHouseOverrides.at(lord.npc), 0);
    CHECK_FALSE(f.party.hasQuestBit(lord.quest));
    CHECK(f.party.hasQuestBit(lord.completed));
}

void release(EndgameFixture &f, const Lord &lord)
{
    REQUIRE(f.state.mapFileName == "pbp.odm");
    REQUIRE(f.party.hasItemAnywhere(629));
    f.transition(lord.gate, lord.pMap);
    f.enterNpc(lord.npc, lord.house);
    f.topic(lord.topic);
    checkReleased(f, lord);
    for (size_t member = 0; member < f.party.memberCount(); ++member)
    {
        CHECK(f.party.hasAward(member, lord.award));
    }
    const int gold = f.party.gold();
    const int64_t experience = f.party.member(0)->experience;
    f.mapEvent(201); // Close and revisit the real room, without invoking an inaccessible old topic.
    CHECK_FALSE(f.resident(lord.npc, lord.house));
    const auto dialog = f.dialogue();
    CHECK_FALSE(std::any_of(dialog.actions.begin(), dialog.actions.end(), [&](const EventDialogAction &action)
    {
        return (action.kind == EventDialogActionKind::HouseResident && action.id == lord.npc)
            || (action.kind == EventDialogActionKind::NpcTopic && action.id == lord.topic);
    }));
    CHECK_EQ(f.party.gold(), gold);
    CHECK_EQ(f.party.member(0)->experience, experience);
    f.transition(501, "pbp.odm");
}

void checkFinished(EndgameFixture &f)
{
    CHECK(f.party.hasQuestBit(56));
    CHECK(f.party.hasQuestBit(228));
    CHECK_FALSE(f.party.hasQuestBit(220));
    REQUIRE(f.state.namedGlobalVars.contains("MMerge.CrossContinents.ContinentFinished.1"));
    CHECK_EQ(f.state.namedGlobalVars.at("MMerge.CrossContinents.ContinentFinished.1"), 1);
    CHECK(historyEventTimesForActiveContinent(f.state).contains(17));
    CHECK(historyEventTimesForActiveContinent(f.state).contains(18));
    CHECK_FALSE(f.state.pendingReturnToMainMenu);
}
}

TEST_CASE("Four prison gates reject the absent key")
{
    EndgameFixture f(profileForLanguage(true));
    f.selectMap("pbp");
    for (const Lord &lord : Lords)
    {
        f.mapEvent(lord.gate);
        CHECK_FALSE(f.state.pendingMapMove);
        CHECK_FALSE(f.state.pendingDialogueContext);
        CHECK_FALSE(f.party.hasQuestBit(lord.completed));
    }
}

TEST_CASE("All24 lord orders earn Q56 only from the final reachable NPC and survive cold saves to Q228")
{
    const Mm8EndgameProfile profile = profileForLanguage(true);
    std::array<size_t, 4> order{0, 1, 2, 3};
    size_t permutations = 0;
    do
    {
        CAPTURE(permutations);
        EndgameFixture f(profile);
        f.solve(profile, {164, 165, 166}, 1);
        const int initialGold = f.party.gold();
        const int64_t initialExperience = f.party.member(0)->experience;
        f.selectMap("out02");
        f.loadEvents();
        CHECK_FALSE(f.party.hasQuestBit(228));
        CHECK_FALSE(f.state.pendingMovie);
        CHECK_FALSE(historyEventTimesForActiveContinent(f.state).contains(18));
        f.selectMap("pbp");
        for (size_t stage = 0; stage < order.size(); ++stage)
        {
            f.minutes += 1;
            release(f, Lords[order[stage]]);
            CHECK_EQ(f.party.hasQuestBit(56), stage == 3);
            CHECK_EQ(historyEventTimesForActiveContinent(f.state).contains(17), stage == 3);
            CHECK_FALSE(f.party.hasQuestBit(228));
            CHECK_EQ(f.party.gold(), initialGold + int(stage + 1) * 10000);
            for (size_t member = 0; member < f.party.memberCount(); ++member)
            {
                CHECK_EQ(f.party.member(member)->experience, initialExperience + int64_t(stage + 1) * 100000);
            }
            CHECK_EQ(f.party.inventoryItemCount(629) + f.party.pendingItemCount(629), 1);
            if (stage >= 2)
            {
                f.coldReload(profile, "mm8-lords-" + std::to_string(permutations) + "-" + std::to_string(stage));
                for (size_t visited = 0; visited <= stage; ++visited) checkReleased(f, Lords[order[visited]]);
                CHECK_EQ(f.party.hasQuestBit(56), stage == 3);
                CHECK_FALSE(f.party.hasQuestBit(228));
            }
        }
        f.minutes += 1;
        f.transition(506, "d10.blv");
        f.transition(501, "out02.odm");
        checkFinished(f);
        REQUIRE(f.state.pendingMovie);
        CHECK_EQ(f.state.pendingMovie->movieName, "wingame");
        CHECK_EQ(f.party.gold(), initialGold + 40000);
        for (size_t member = 0; member < f.party.memberCount(); ++member)
        {
            CHECK_EQ(f.party.member(member)->experience, initialExperience + 500000);
        }
        const auto history = historyEventTimesForActiveContinent(f.state);
        f.coldReload(profile, "mm8-finished-" + std::to_string(permutations));
        checkFinished(f);
        f.selectMap("pbp");
        f.selectMap("out02");
        f.minutes += 1;
        f.loadEvents();
        checkFinished(f);
        CHECK_FALSE(f.state.pendingMovie);
        CHECK_EQ(historyEventTimesForActiveContinent(f.state), history);
        CHECK_EQ(f.party.member(0)->experience, initialExperience + 500000);
        for (const Lord &lord : Lords) checkReleased(f, lord);
        ++permutations;
    } while (std::next_permutation(order.begin(), order.end()));
    CHECK_EQ(permutations, 24);
}

TEST_CASE("MM8 ending source NPC aftermath covers all four Q19 Q22 branches through OnLoad")
{
    const Mm8EndgameProfile profile = profileForLanguage(true);
    for (bool dark : {false, true})
    {
        for (bool dragons : {false, true})
        {
            EndgameFixture f(profile);
            f.solve(profile, {166, 164, 165}, 1);
            f.selectMap("pbp");
            for (const Lord &lord : Lords) release(f, lord);
            // Declared alliance fixture; completion Q56 and the key are always earned above.
            f.party.setQuestBit(19, dark);
            f.party.setQuestBit(22, dragons);
            f.transition(506, "d10.blv");
            f.transition(501, "out02.odm");
            checkFinished(f);
            CHECK_EQ(f.state.npcHouseOverrides.at(55), 755);
            CHECK_EQ(f.state.npcHouseOverrides.at(dragons ? 53 : 52), dragons ? 774 : 753);
            CHECK_EQ(f.state.npcHouseOverrides.at(dark ? 56 : 54), dark ? 213 : 214);
            if (!dragons) CHECK_EQ(f.state.npcHouseOverrides.at(51), 0);
            if (dark) CHECK_EQ(f.state.npcHouseOverrides.at(63), 0);
            CHECK_EQ(f.state.npcHouseOverrides.at(20), 0);
            CHECK_EQ(f.state.npcHouseOverrides.at(21), 0);
            CHECK_EQ(f.state.npcTopicOverrides.at(19).at(0), 0);
            CHECK_EQ(f.state.npcTopicOverrides.at(19).at(1), 157);
            CHECK(std::any_of(f.facetChanges.begin(), f.facetChanges.end(), [](const auto &change)
            {
                return change[0] == 30 && change[2] == 1;
            }));
        }
    }
}
