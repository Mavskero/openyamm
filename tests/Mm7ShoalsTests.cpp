#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "game/events/EventRuntime.h"
#include "game/events/ISceneEventContext.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"
#include "tools/LegacyLuaExport.h"
#include "tools/mmerge_import/LodResources.h"
#include "tools/mmerge_import/Mm7ShoalsResources.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
constexpr uint16_t SurfaceTimer = 65472;
constexpr std::array<const char *, 3> MapStems{"out14", "7out15", "7d23"};

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

std::filesystem::path outputPath(const std::string &name)
{
    const char *pRoot = std::getenv("TMPDIR");
    REQUIRE_MESSAGE(pRoot != nullptr, "Shoals tests require the external-storage TMPDIR");
    return std::filesystem::path(pRoot) / name;
}

std::string originalScript(const std::string &stem)
{
    std::ifstream input(sourceRoot() / "Scripts/Maps" / (stem + ".lua"), std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

StrTable originalStrings(const std::string &stem)
{
    StrTable strings;
    REQUIRE(strings.loadFromNullSeparatedBytes(readLodText(sourceRoot() / "Data/mmmerge.T.lod", stem + ".str")));
    return strings;
}

std::optional<ScriptedEventProgram> mapProgram(const std::string &stem)
{
    const std::optional<Mm7ShoalsProfile> profile = loadMm7ShoalsProfile(sourceRoot());
    REQUIRE(profile.has_value());
    const auto found = std::find_if(profile->maps.begin(), profile->maps.end(), [&](const Mm7ShoalsMapOverlay &map)
    {
        return map.mapStem == stem;
    });
    REQUIRE(found != profile->maps.end());
    EvtProgram evt;
    REQUIRE(evt.loadFromBytes(readLodText(sourceRoot() / "Data/mm7.T.lod", stem + ".evt")));
    const std::string base = generateLegacyEventLuaChunk(
        evt, originalStrings(stem), {}, LegacyLuaExportScope::Map, LegacyEventVersion::Mm8);
    std::string error;
    const std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(EventSupportSource) + "\n" + base + "\n" + found->luaSource,
        "@original-" + stem + "+source-verified-shoals", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    return program;
}

InventoryItem item(uint32_t id)
{
    InventoryItem result;
    result.objectDescriptionId = id;
    return result;
}

class ShoalsFixture : public OpenYAMM::Tests::PartySpellTestWorldRuntime, public ISceneEventContext
{
public:
    Party party;
    EventRuntimeState state;
    EventRuntime events;
    std::string mapStem;
    std::optional<ScriptedEventProgram> local;
    std::vector<ScriptedEventTimerState> timers;
    double minutes = 600;

    explicit ShoalsFixture(const std::string &stem, size_t memberCount = 5) : mapStem(stem), local(mapProgram(stem))
    {
        REQUIRE(memberCount >= 1);
        REQUIRE(memberCount <= 5);
        PartySeed seed = OpenYAMM::Tests::createSpellRegressionPartySeed();
        seed.members.push_back(OpenYAMM::Tests::makeSpellRegressionPartyMember("Fifth", "Knight", "PC01-01", 8));
        seed.members.resize(memberCount);
        party.seed(seed);
        state.mapFileName = stem + (stem == "7d23" ? ".blv" : ".odm");
        state.activeHistoryContinentId = 2;
        state.normalGameplayScreen = true;
        bindParty(&party);
        bindEventRuntimeState(&state);
        setMapName(state.mapFileName);
        setIndoorMap(stem == "7d23");
        setPartyPosition(100, 200, 300);
        timers = reconcileScriptedEventTimers({}, local->timerTriggers(), minutes, 0, false);
    }

    float currentGameMinutes() const override { return float(minutes); }
    const MapDeltaData *mapDeltaData() const override { return nullptr; }
    MapDeltaData *mapDeltaData() override { return nullptr; }
    bool setFacetBit(uint32_t, uint32_t, bool) override { return false; }
    bool castEventSpell(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        int32_t, int32_t, int32_t) override { return false; }
    bool summonMonsters(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t,
        uint32_t, uint32_t) override { return false; }
    bool summonEventItem(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool summonEventObject(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override { return false; }
    bool checkMonstersKilled(uint32_t, uint32_t, uint32_t, bool) const override { return false; }

    void suit(size_t member, bool equipped = false)
    {
        REQUIRE(party.member(member) != nullptr);
        if (equipped)
        {
            party.member(member)->equipment.armor = 1406;
        }
        else
        {
            REQUIRE(party.member(member)->addInventoryItem(item(1406)));
        }
    }

    void suitAll()
    {
        for (size_t member = 0; member < party.memberCount(); ++member)
        {
            suit(member, member % 2 == 0);
        }
    }

    bool boundary(uint32_t edge, const std::string &destination = {})
    {
        // This deliberately requires the real MapBoundary API. No test shim can
        // prove that production emits it before an absent table destination.
        EventRuntimeState::ActiveHookContext hook;
        hook.kind = EventRuntimeHookKind::MapBoundary;
        hook.boundaryEdge = edge;
        hook.destinationMapName = destination;
        state.activeHookContext = hook;
        REQUIRE_MESSAGE(events.executeHooks(local, std::nullopt, hook.kind, state, &party, this),
            events.lastScriptError().value_or("Avlee boundary hook missing"));
        const bool blocked = state.activeHookContext->blocked;
        state.activeHookContext.reset();
        party.applyEventRuntimeState(state, false);
        return blocked;
    }

    void run(uint16_t eventId, bool timer = false)
    {
        REQUIRE_MESSAGE(events.executeEventById(local, std::nullopt, eventId, state, &party, this,
            std::nullopt, true, ScriptedEventScope::Map, timer),
            events.lastScriptError().value_or("Shoals event missing"));
        party.applyEventRuntimeState(state, false);
    }

    void pollAt(double gameMinutes)
    {
        minutes = gameMinutes;
        updateScriptedEventTimers(timers, minutes, [&](const ScriptedEventTimerDefinition &definition)
        {
            run(definition.eventId, true);
            return true;
        });
    }

    void clearTransition()
    {
        state.pendingMapMove.reset();
        state.pendingDialogueContext.reset();
    }

    void checkNoTransition() const
    {
        CHECK_FALSE(state.pendingMapMove);
        CHECK_FALSE(state.pendingDialogueContext);
    }

    void checkTransition(const std::string &target, int x, int y, int z, int degrees,
        uint32_t image = 8, uint32_t text = 0) const
    {
        CHECK_FALSE(state.pendingMapMove);
        REQUIRE(state.pendingDialogueContext);
        CHECK_EQ(state.pendingDialogueContext->kind, DialogueContextKind::MapTransition);
        CHECK_EQ(state.pendingDialogueContext->transitionImageId, image);
        CHECK_EQ(state.pendingDialogueContext->transitionTextId, text);
        REQUIRE(state.pendingDialogueContext->transitionMapMove);
        const EventRuntimeState::PendingMapMove &move = *state.pendingDialogueContext->transitionMapMove;
        CHECK_EQ(move.mapName, std::optional<std::string>(target));
        CHECK_EQ(move.x, x);
        CHECK_EQ(move.y, y);
        CHECK_EQ(move.z, z);
        CHECK_EQ(move.directionDegrees, std::optional<int32_t>(degrees));
        CHECK_FALSE(move.useMapStartPosition);
    }

    int32_t mapVariable(uint32_t index) const
    {
        return index < state.mapVars.size() ? state.mapVars[index] : 0;
    }

    void coldReload(const std::string &name)
    {
        GameSaveData save;
        save.currentSceneKind = mapStem == "7d23" ? SceneKind::Indoor : SceneKind::Outdoor;
        save.mapFileName = state.mapFileName;
        save.party = party.snapshot();
        save.savedGameMinutes = minutes;
        if (mapStem == "7d23")
        {
            save.hasIndoorSceneState = true;
            save.indoorScene.eventRuntimeState = state;
            save.indoorScene.worldRuntime.gameMinutes = float(minutes);
            save.indoorSceneStates[state.mapFileName] = save.indoorScene;
        }
        else
        {
            save.hasOutdoorRuntimeState = true;
            save.outdoorWorld.eventRuntimeState = state;
            save.outdoorWorld.gameMinutes = float(minutes);
            save.outdoorWorldStates[state.mapFileName] = save.outdoorWorld;
        }
        std::string error;
        const std::filesystem::path path = outputPath(name);
        REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
        const std::optional<GameSaveData> loaded = loadGameDataFromPath(path, error);
        REQUIRE_MESSAGE(loaded.has_value(), error);
        party.restoreSnapshot(loaded->party);
        const std::optional<EventRuntimeState> &loadedState = mapStem == "7d23"
            ? loaded->indoorScene.eventRuntimeState : loaded->outdoorWorld.eventRuntimeState;
        REQUIRE(loadedState);
        state = *loadedState;
        state.normalGameplayScreen = true;
        local = mapProgram(mapStem);
        // Source LastAt is a fresh local after a cold map load. Timer scheduling
        // is reconciled separately; this helper does not simulate swim physics.
    }
};
}

TEST_CASE("MM7 Shoals profile pins all three source scripts and rejects partial changed or escaping families")
{
    const std::optional<Mm7ShoalsProfile> profile = loadMm7ShoalsProfile(sourceRoot());
    REQUIRE(profile);
    REQUIRE_EQ(profile->maps.size(), 3);
    const std::array<const char *, 3> hashes{
        "51391e04e9e5402ccac889cbd9b043298c42c72a700b1144c516c5ee07226f73",
        "3bdb718ed844ce2f5bdc59682962e1dc5673f7e7b61f1d68fc0f680b698ba483",
        "6f22a6f6d8bbec764c8f1ef334a3bd841a4901324e971fd523fe7d9b5ff2f6cd"};
    const std::string nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path fixture = outputPath("shoals-profile-" + nonce);
    std::filesystem::create_directories(fixture / "Scripts/Maps");
    CHECK_FALSE(loadMm7ShoalsProfile(fixture));
    for (size_t index = 0; index < MapStems.size(); ++index)
    {
        CHECK_EQ(profile->maps[index].mapStem, MapStems[index]);
        CHECK_EQ(profile->maps[index].sourceSha256, hashes[index]);
        CHECK_FALSE(profile->maps[index].luaSource.empty());
        const std::filesystem::path candidate = fixture / "Scripts/Maps" / (std::string(MapStems[index]) + ".lua");
        std::ofstream(candidate, std::ios::binary) << originalScript(MapStems[index]);
        if (index + 1 < MapStems.size())
        {
            CHECK_THROWS_AS(loadMm7ShoalsProfile(fixture), std::runtime_error);
        }
    }
    REQUIRE(loadMm7ShoalsProfile(fixture));
    for (const char *pStem : MapStems)
    {
        const std::filesystem::path candidate = fixture / "Scripts/Maps" / (std::string(pStem) + ".lua");
        std::ofstream(candidate, std::ios::binary) << originalScript(pStem) << "\n-- unreviewed";
        CHECK_THROWS_AS(loadMm7ShoalsProfile(fixture), std::runtime_error);
        std::filesystem::remove(candidate);
        std::filesystem::create_symlink(sourceRoot() / "Scripts/Maps" / (std::string(pStem) + ".lua"), candidate);
        CHECK_THROWS_AS(loadMm7ShoalsProfile(fixture), std::runtime_error);
        std::filesystem::remove(candidate);
        std::ofstream(candidate, std::ios::binary) << originalScript(pStem);
    }
}

TEST_CASE("Avlee west boundary supplies an absent destination for one through five suited party members")
{
    for (size_t members = 1; members <= 5; ++members)
    {
        ShoalsFixture f("out14", members);
        f.suitAll();
        CHECK_FALSE(f.boundary(3));
        f.checkTransition("7out15.odm", 20096, -16448, 2404, 177);
        CHECK_FALSE(f.party.hasQuestBit(642));
        CHECK_FALSE(f.party.hasQuestBit(643));
        CHECK_FALSE(f.party.hasQuestBit(783));
        CHECK(f.state.statusMessages.empty());
    }
}

TEST_CASE("Avlee requires each actual member even when another member carries surplus wetsuits")
{
    for (size_t missing = 0; missing < 5; ++missing)
    {
        ShoalsFixture f("out14");
        for (size_t member = 0; member < 5; ++member)
        {
            if (member != missing)
            {
                f.suit(member, member % 2 == 0);
            }
        }
        f.suit((missing + 1) % 5);
        CHECK(f.boundary(3));
        f.checkNoTransition();
        CHECK(f.state.statusMessages.empty());
    }
}

TEST_CASE("Avlee quest bits only enable the source missing-wetsuit status message")
{
    for (uint32_t qbit : {642, 643, 783})
    {
        ShoalsFixture f("out14");
        f.party.setQuestBit(qbit, true);
        CHECK(f.boundary(3));
        f.checkNoTransition();
        REQUIRE_EQ(f.state.statusMessages.size(), 1);
        CHECK_EQ(f.state.statusMessages.front(), "You must all be wearing your wetsuits!");
    }
}

TEST_CASE("Avlee north south and east leave existing travel selection untouched")
{
    for (uint32_t edge : {0, 1, 2})
    {
        ShoalsFixture f("out14");
        f.party.setQuestBit(642, true);
        CHECK_FALSE(f.boundary(edge, "table-destination.odm"));
        f.checkNoTransition();
        CHECK(f.state.statusMessages.empty());
    }
}

TEST_CASE("Lincoln exit handles real party size and preserves imported transition fields")
{
    for (size_t members = 1; members <= 5; ++members)
    {
        ShoalsFixture f("7d23", members);
        f.suitAll();
        f.run(501);
        f.checkTransition("7out15.odm", -7005, 7856, 225, 22);
        CHECK_FALSE(f.party.hasQuestBit(783));
        CHECK_EQ(f.mapVariable(4), 0);
    }
}

TEST_CASE("Lincoln rejects each actual missing wetsuit with its original localized STR20")
{
    const StrTable strings = originalStrings("7d23");
    REQUIRE(strings.get(20));
    for (size_t members = 1; members <= 5; ++members)
    {
        for (size_t missing = 0; missing < members; ++missing)
        {
            ShoalsFixture f("7d23", members);
            for (size_t member = 0; member < members; ++member)
            {
                if (member != missing)
                {
                    f.suit(member, member % 2 == 0);
                }
            }
            f.run(501);
            f.checkNoTransition();
            REQUIRE_EQ(f.state.statusMessages.size(), 1);
            CHECK_EQ(f.state.statusMessages.front(), *strings.get(20));
        }
    }
}

TEST_CASE("Lincoln source load hides and pacifies group56 without replacing power or oscillator events")
{
    ShoalsFixture f("7d23");
    REQUIRE(f.events.executeOnLoadEvents(f.local, std::nullopt, f.state, &f.party, &f));
    CHECK((f.state.actorGroupSetMasks.at(56) & uint32_t(EvtActorAttribute::Invisible)) != 0);
    CHECK((f.state.actorGroupClearMasks.at(56) & uint32_t(EvtActorAttribute::Hostile)) != 0);
    CHECK_FALSE(f.state.actorGroupHostilityRequests.at(56));
    CHECK(f.local->hasEvent(376));
    CHECK(f.local->hasEvent(475));
    CHECK(f.local->hasEvent(476));
    CHECK(f.local->hasEvent(477));
}

TEST_CASE("Shoals original Lincoln entrance survives the surface overlay")
{
    ShoalsFixture f("7out15");
    f.run(501);
    f.checkTransition("7d23.blv", 524, 1463, 225, 270, 9, 416);
}

TEST_CASE("Shoals surface timer preserves strict altitude screen and source LastAt rearm semantics")
{
    ShoalsFixture f("7out15");
    f.setPartyPosition(1, 2, 3900);
    f.run(SurfaceTimer, true);
    f.checkNoTransition();
    f.setPartyPosition(1, 2, 3900.25f);
    f.state.normalGameplayScreen = false;
    f.run(SurfaceTimer, true);
    f.checkNoTransition();
    f.state.normalGameplayScreen = true;
    f.run(SurfaceTimer, true);
    f.checkTransition("out14.odm", -18584, -16562, 1, 50);
    f.clearTransition(); // User cancelled the source exit prompt while still above the threshold.
    f.run(SurfaceTimer, true);
    f.checkNoTransition();
    f.setPartyPosition(1, 2, 3900);
    f.run(SurfaceTimer, true);
    f.setPartyPosition(1, 2, 3900.25f);
    f.run(SurfaceTimer, true);
    f.checkTransition("out14.odm", -18584, -16562, 1, 50);
}

TEST_CASE("Shoals surface waits while a movie input map move or dialogue already owns the screen")
{
    for (int pending = 0; pending < 4; ++pending)
    {
        ShoalsFixture f("7out15");
        f.setPartyPosition(1, 2, 4000);
        if (pending == 0) f.state.pendingMovie = EventRuntimeState::PendingMovie{"test", false};
        if (pending == 1) f.state.pendingInputPrompt = EventRuntimeState::PendingInputPrompt{};
        if (pending == 2) f.state.pendingMapMove = EventRuntimeState::PendingMapMove{};
        if (pending == 3)
        {
            f.state.pendingDialogueContext = EventRuntimeState::PendingDialogueContext{};
            f.state.pendingDialogueContext->kind = DialogueContextKind::NpcTalk;
        }
        f.run(SurfaceTimer, true);
        if (pending != 3) CHECK_FALSE(f.state.pendingDialogueContext);
        else CHECK_EQ(f.state.pendingDialogueContext->kind, DialogueContextKind::NpcTalk);
        f.state.pendingMovie.reset();
        f.state.pendingInputPrompt.reset();
        f.clearTransition();
        f.run(SurfaceTimer, true);
        f.checkTransition("out14.odm", -18584, -16562, 1, 50);
    }
}

TEST_CASE("Shoals surface uses production timer scheduling at half a game minute")
{
    ShoalsFixture f("7out15");
    REQUIRE_EQ(f.timers.size(), 1);
    const ScriptedEventTimerDefinition &definition = f.timers.front().definition;
    CHECK_EQ(definition.eventId, SurfaceTimer);
    CHECK_EQ(definition.origin, ScriptedEventTimerOrigin::Native);
    CHECK_EQ(definition.intervalGameMinutes, 0.5);
    CHECK_EQ(definition.initialDelayGameMinutes, 0.5);
    CHECK(definition.repeating);
    f.setPartyPosition(1, 2, 4000);
    f.pollAt(600.49);
    f.checkNoTransition();
    f.pollAt(600.5);
    f.checkTransition("out14.odm", -18584, -16562, 1, 50);
    f.clearTransition();
    f.pollAt(601);
    f.checkNoTransition();
}

TEST_CASE("Shoals cold map reload resets the source local surface latch without granting completion")
{
    ShoalsFixture f("7out15");
    f.setPartyPosition(1, 2, 4000);
    f.run(SurfaceTimer, true);
    f.clearTransition();
    f.run(SurfaceTimer, true);
    f.checkNoTransition();
    f.coldReload("shoals-surface-cold.oysav");
    f.run(SurfaceTimer, true);
    f.checkTransition("out14.odm", -18584, -16562, 1, 50);
    CHECK_FALSE(f.party.hasQuestBit(783));
}

TEST_CASE("Lincoln original power and oscillator chain awards one item and persists its source quest flags")
{
    ShoalsFixture f("7d23");
    f.run(376);
    CHECK_FALSE(f.party.hasItemAnywhere(1407));
    CHECK_FALSE(f.party.hasQuestBit(633));
    CHECK_FALSE(f.party.hasQuestBit(748));
    f.run(475);
    CHECK_EQ(f.mapVariable(4), 1);
    f.run(376);
    CHECK(f.party.hasItemAnywhere(1407));
    CHECK_EQ(f.party.inventoryItemCount(1407) + f.party.pendingItemCount(1407), 1);
    CHECK(f.party.hasQuestBit(633));
    CHECK(f.party.hasQuestBit(748));
    CHECK_FALSE(f.party.hasQuestBit(783));
    CHECK_EQ(f.mapVariable(5), 1);
    f.run(376);
    CHECK_EQ(f.party.inventoryItemCount(1407) + f.party.pendingItemCount(1407), 1);
    f.coldReload("lincoln-oscillator-cold.oysav");
    CHECK_EQ(f.mapVariable(4), 1);
    CHECK_EQ(f.mapVariable(5), 1);
    CHECK(f.party.hasQuestBit(633));
    CHECK(f.party.hasQuestBit(748));
    f.run(376);
    CHECK_EQ(f.party.inventoryItemCount(1407) + f.party.pendingItemCount(1407), 1);
}

TEST_CASE("Lincoln oscillator source guard does not duplicate a preexisting item or completed quest")
{
    for (bool completed : {false, true})
    {
        ShoalsFixture f("7d23");
        f.run(475);
        if (completed) f.party.setQuestBit(633, true);
        else REQUIRE(f.party.member(0)->addInventoryItem(item(1407)));
        f.run(376);
        CHECK_EQ(f.party.inventoryItemCount(1407) + f.party.pendingItemCount(1407), completed ? 0 : 1);
        CHECK_EQ(f.mapVariable(5), 0);
        CHECK_FALSE(f.party.hasQuestBit(748));
        CHECK_FALSE(f.party.hasQuestBit(783));
    }
}
