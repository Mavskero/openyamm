#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "engine/TextTable.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/events/ISceneEventContext.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/TownPortalContactState.h"
#include "game/party/SpellIds.h"
#include "game/tables/NpcDialogTable.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
constexpr uint32_t VerdantNpcId = 803;
constexpr uint16_t VerdantReadyTopicId = 1778;
constexpr uint32_t VerdantFirstMessageId = 2149;
constexpr const char *MetVerdantVar = "MMerge.CrossContinents.MetVerdant";
constexpr const char *GotMainQuestVar = "MMerge.CrossContinents.GotMainQuest";
constexpr float BoundaryStartGameMinutes = 12.0f * 60.0f;

std::filesystem::path requiredEnvironmentPath(const char *pName)
{
    const char *pValue = std::getenv(pName);
    REQUIRE_MESSAGE(pValue != nullptr, (std::string("Missing required environment variable ") + pName).c_str());
    REQUIRE_MESSAGE(*pValue != '\0', (std::string("Empty required environment variable ") + pName).c_str());
    return pValue;
}

std::string readTextFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE_MESSAGE(input.is_open(), ("Could not open " + path.string()).c_str());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::vector<std::string>> parseTextTableRows(const std::filesystem::path &path)
{
    const std::optional<OpenYAMM::Engine::TextTable> table =
        OpenYAMM::Engine::TextTable::parseTabSeparated(readTextFile(path));
    REQUIRE(table.has_value());

    std::vector<std::vector<std::string>> rows;
    rows.reserve(table->getRowCount());
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    return rows;
}

int32_t namedGlobal(const EventRuntimeState &state, const char *pName)
{
    const auto iterator = state.namedGlobalVars.find(pName);
    return iterator != state.namedGlobalVars.end() ? iterator->second : 0;
}

const EventDialogAction *findDialogAction(
    const EventDialogContent &dialog,
    EventDialogActionKind kind,
    uint32_t id)
{
    const auto iterator = std::find_if(
        dialog.actions.begin(), dialog.actions.end(), [kind, id](const EventDialogAction &action)
        {
            return action.kind == kind && action.id == id;
        });
    return iterator != dialog.actions.end() ? &*iterator : nullptr;
}

struct PartyObservation
{
    int gold = 0;
    int food = 0;
    uint32_t experience = 0;
    int spellPoints = 0;
    float recoverySeconds = 0.0f;
    size_t inventoryItems = 0;
    bool quest1104 = false;
    bool quest1105 = false;
};

PartyObservation observeParty(const Party &party)
{
    const Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    return {
        party.gold(),
        party.food(),
        pMember->experience,
        pMember->spellPoints,
        pMember->recoverySecondsRemaining,
        party.inventoryItemCount(),
        party.hasQuestBit(1104),
        party.hasQuestBit(1105),
    };
}

void checkPartyObservation(const Party &party, const PartyObservation &expected)
{
    const PartyObservation actual = observeParty(party);
    CHECK_EQ(actual.gold, expected.gold);
    CHECK_EQ(actual.food, expected.food);
    CHECK_EQ(actual.experience, expected.experience);
    CHECK_EQ(actual.spellPoints, expected.spellPoints);
    CHECK_EQ(actual.recoverySeconds, expected.recoverySeconds);
    CHECK_EQ(actual.inventoryItems, expected.inventoryItems);
    CHECK_EQ(actual.quest1104, expected.quest1104);
    CHECK_EQ(actual.quest1105, expected.quest1105);
}

Party makeTownPortalParty(const GameDataLoader &loader)
{
    Character member;
    member.name = "Town Portal contact boundary member";
    member.className = "Sorcerer";
    member.role = "Sorcerer";
    member.maxHealth = 40;
    member.health = 40;
    member.maxSpellPoints = 100;
    member.spellPoints = 100;
    member.experience = 456;
    member.level = 10;
    member.intellect = 30;
    member.personality = 30;
    member.skills["WaterMagic"] = {"WaterMagic", 10, SkillMastery::Grandmaster};
    REQUIRE(member.learnSpell(spellIdValue(SpellId::TownPortal)));

    PartySeed seed;
    seed.gold = 789;
    seed.food = 6;
    seed.members.push_back(member);

    Party party;
    party.setItemTable(&loader.getItemTable());
    party.setCharacterDollTable(&loader.getCharacterDollTable());
    party.setItemEnchantTables(
        &loader.getStandardItemEnchantTable(),
        &loader.getSpecialItemEnchantTable());
    party.setClassMultiplierTable(&loader.getClassMultiplierTable());
    party.setClassSkillTable(&loader.getClassSkillTable());
    party.seed(seed);
    const Character *pCaster = party.member(0);
    const SpellEntry *pTownPortal = loader.getSpellTable().findById(
        static_cast<int>(spellIdValue(SpellId::TownPortal)));
    REQUIRE(pCaster != nullptr);
    REQUIRE(pTownPortal != nullptr);
    REQUIRE_GE(pCaster->spellPoints, pTownPortal->grandmasterManaCost);
    return party;
}

PartySpellCastRequest makeTownPortalRequest()
{
    PartySpellCastRequest request;
    request.casterMemberIndex = 0;
    request.spellId = spellIdValue(SpellId::TownPortal);
    return request;
}

class ContactTestWorld :
    public OpenYAMM::Tests::PartySpellTestWorldRuntime,
    public ISceneEventContext
{
public:
    void bindContactRuntime(
        Party *pParty,
        EventRuntimeState *pEventState,
        const std::optional<ScriptedEventProgram> *pLocalProgram,
        const std::optional<ScriptedEventProgram> *pGlobalProgram,
        const NpcDialogTable *pNpcDialogs)
    {
        m_pContactParty = pParty;
        m_pContactEventState = pEventState;
        m_pContactLocalProgram = pLocalProgram;
        m_pContactGlobalProgram = pGlobalProgram;
        m_pContactNpcDialogs = pNpcDialogs;
        bindParty(pParty);
        bindEventRuntimeState(pEventState);
        bindGlobalEventProgram(pGlobalProgram);
    }

    bool executeEventHooks(EventRuntimeHookKind kind) override
    {
        if (m_pContactEventState == nullptr || m_pContactGlobalProgram == nullptr)
        {
            return false;
        }

        EventRuntime runtime(nullptr, m_pContactNpcDialogs);
        const bool executed = runtime.executeHooks(
            m_pContactLocalProgram != nullptr ? *m_pContactLocalProgram : std::nullopt,
            *m_pContactGlobalProgram,
            kind,
            *m_pContactEventState,
            m_pContactParty,
            this);
        if (executed && m_pContactParty != nullptr)
        {
            m_pContactParty->applyEventRuntimeState(*m_pContactEventState);
        }
        return executed;
    }

    bool executeNpcTopicEvent(
        uint16_t eventId,
        size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        if (m_pContactEventState == nullptr || m_pContactGlobalProgram == nullptr)
        {
            return false;
        }

        previousMessageCount = m_pContactEventState->messages.size();
        EventRuntime runtime(nullptr, m_pContactNpcDialogs);
        const bool executed = runtime.executeNpcTopicEventById(
            m_pContactLocalProgram != nullptr ? *m_pContactLocalProgram : std::nullopt,
            *m_pContactGlobalProgram,
            eventId,
            *m_pContactEventState,
            m_pContactParty,
            this,
            continueStep);
        if (executed && m_pContactParty != nullptr)
        {
            m_pContactParty->applyEventRuntimeState(*m_pContactEventState);
        }
        return executed;
    }

    bool executeMapEvent(
        uint16_t eventId,
        size_t &previousMessageCount,
        std::optional<uint8_t> continueStep = std::nullopt) override
    {
        if (m_pContactEventState == nullptr || m_pContactGlobalProgram == nullptr)
        {
            return false;
        }

        previousMessageCount = m_pContactEventState->messages.size();
        EventRuntime runtime(nullptr, m_pContactNpcDialogs);
        const bool executed = runtime.executeEventById(
            m_pContactLocalProgram != nullptr ? *m_pContactLocalProgram : std::nullopt,
            *m_pContactGlobalProgram,
            eventId,
            *m_pContactEventState,
            m_pContactParty,
            this,
            continueStep);
        if (executed && m_pContactParty != nullptr)
        {
            m_pContactParty->applyEventRuntimeState(*m_pContactEventState);
        }
        return executed;
    }

    float currentGameMinutes() const override
    {
        return gameMinutes();
    }

    const MapDeltaData *mapDeltaData() const override
    {
        return nullptr;
    }

    MapDeltaData *mapDeltaData() override
    {
        return nullptr;
    }

    bool setFacetBit(uint32_t, uint32_t, bool) override
    {
        return false;
    }

    bool castEventSpell(
        uint32_t spellId,
        uint32_t skillLevel,
        uint32_t skillMastery,
        int32_t fromX,
        int32_t fromY,
        int32_t fromZ,
        int32_t toX,
        int32_t toY,
        int32_t toZ) override
    {
        return OpenYAMM::Tests::PartySpellTestWorldRuntime::castEventSpell(
            spellId, skillLevel, skillMastery, fromX, fromY, fromZ, toX, toY, toZ);
    }

    bool summonMonsters(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t) override
    {
        return false;
    }

    bool summonEventItem(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override
    {
        return false;
    }

    bool summonEventObject(uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, bool) override
    {
        return false;
    }

    bool checkMonstersKilled(uint32_t, uint32_t, uint32_t, bool) const override
    {
        return false;
    }

private:
    Party *m_pContactParty = nullptr;
    EventRuntimeState *m_pContactEventState = nullptr;
    const std::optional<ScriptedEventProgram> *m_pContactLocalProgram = nullptr;
    const std::optional<ScriptedEventProgram> *m_pContactGlobalProgram = nullptr;
    const NpcDialogTable *m_pContactNpcDialogs = nullptr;
};

struct ActualTownPortalFixture
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository repository;
    GameSession session;
    Party party;
    EventRuntimeState eventState;
    ContactTestWorld world;
    std::string readyTopicText;
    std::string firstIntroMessage;

    ActualTownPortalFixture()
    {
        const std::filesystem::path runtimeRoot = requiredEnvironmentPath("OPENYAMM_CC_RUNTIME_ROOT");
        const std::filesystem::path eventRoot = requiredEnvironmentPath("OPENYAMM_CC_EVENT_ROOT");
        const std::filesystem::path baseRoot = requiredEnvironmentPath("OPENYAMM_CC_BASE_ROOT");

        REQUIRE(files.initialize(runtimeRoot, runtimeRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
        REQUIRE(files.mountDevelopmentRoot(eventRoot));
        loader.setActiveWorldId("mm6");
        REQUIRE(loader.loadCommonForGameplay(files));
        loader.setActiveWorldId("mm6");
        REQUIRE(loader.loadMapByFileNameForHeadlessGameplay(files, "out01.odm"));
        REQUIRE(loader.getSelectedMap().has_value());
        REQUIRE(loader.getSelectedMap()->globalEventProgram.has_value());
        REQUIRE(loader.getSelectedMap()->globalEventProgram->hasEvent(65500));
        REQUIRE(loader.getSelectedMap()->globalEventProgram->hasEvent(VerdantReadyTopicId));
        REQUIRE(loader.getNpcDialogTable().getNpc(VerdantNpcId) != nullptr);

        NpcDialogTable convertedDialogs;
        REQUIRE(convertedDialogs.loadTextsFromRows(parseTextTableRows(
            baseRoot / "engine/data_tables/npc_topic_text.txt")));
        REQUIRE(convertedDialogs.loadTopicsFromRows(parseTextTableRows(
            baseRoot / "engine/data_tables/npc_topic.txt")));
        const std::optional<NpcDialogTable::ResolvedTopic> convertedReadyTopic =
            convertedDialogs.getTopicById(VerdantReadyTopicId);
        const std::optional<std::string> convertedFirstMessage = convertedDialogs.getText(VerdantFirstMessageId);
        REQUIRE(convertedReadyTopic.has_value());
        REQUIRE_FALSE(convertedReadyTopic->topic.empty());
        REQUIRE(convertedFirstMessage.has_value());
        REQUIRE_FALSE(convertedFirstMessage->empty());
        readyTopicText = convertedReadyTopic->topic;
        firstIntroMessage = *convertedFirstMessage;
        const std::optional<NpcDialogTable::ResolvedTopic> loadedReadyTopic =
            loader.getNpcDialogTable().getTopicById(VerdantReadyTopicId);
        const std::optional<std::string> loadedFirstMessage =
            loader.getNpcDialogTable().getText(VerdantFirstMessageId);
        REQUIRE(loadedReadyTopic.has_value());
        REQUIRE(loadedFirstMessage.has_value());
        CHECK_EQ(loadedReadyTopic->topic, convertedReadyTopic->topic);
        CHECK_EQ(*loadedFirstMessage, *convertedFirstMessage);

        repository.bind(loader);
        session.bindDataRepository(&repository);
        session.setCurrentMapFileName("out01.odm");
        session.setGameMinutes(BoundaryStartGameMinutes);
        party = makeTownPortalParty(loader);
        world.bindContactRuntime(
            &party,
            &eventState,
            &loader.getSelectedMap()->localEventProgram,
            &loader.getSelectedMap()->globalEventProgram,
            &loader.getNpcDialogTable());
        world.bindMonsterTable(&loader.getMonsterTable());
        world.bindMergedBolsterMonsterTable(&loader.getMergedBolsterMonsterTable());
        world.setMapName("out01.odm");
        world.setPartyPosition(-512.0f, 2560.0f, 0.0f);
        world.advanceGameMinutes(BoundaryStartGameMinutes);
        session.bindActiveWorldRuntime(&world);
    }

    GameplayScreenRuntime &runtime()
    {
        return session.gameplayScreenRuntime();
    }

    GameplaySpellService::SpellRequestResolution castTownPortal(
        const PartySpellCastRequest &request = makeTownPortalRequest())
    {
        return session.gameplaySpellService().resolveSpellRequest(runtime(), request, "Town Portal");
    }

    void dismissDialog()
    {
        runtime().closeActiveEventDialog();
        REQUIRE_FALSE(runtime().activeEventDialog().isActive);
    }

    void closeTownPortalOverlay()
    {
        runtime().closeUtilitySpellOverlayAfterSpellResolution(spellIdValue(SpellId::TownPortal));
        REQUIRE_FALSE(runtime().utilitySpellOverlayReadOnly().active);
    }

    void activateOut01DimensionDoorBoundary()
    {
        runtime().dispatchOutdoorStep(63, 59);
        REQUIRE(runtime().shouldTownPortalCastOpenDimensionDoor());
    }

    void rebindSessionAfterClear()
    {
        session.bindDataRepository(&repository);
        session.setCurrentMapFileName("out01.odm");
        session.bindActiveWorldRuntime(&world);
    }
};
}

TEST_CASE("Town Portal contact policy uses a strict four-minute moving deadline")
{
    TownPortalContactState state;

    CHECK_FALSE(state.onTownPortalAttempt(0.0, true, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(4.0));
    CHECK_FALSE(state.onTownPortalAttempt(4.0, true, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(8.0));
    CHECK(state.onTownPortalAttempt(8.001, true, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(12.001));

    CHECK_FALSE(state.onTownPortalAttempt(13.0, false, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(17.0));
    CHECK_FALSE(state.onTownPortalAttempt(16.999, true, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(20.999));
    CHECK(state.onTownPortalAttempt(21.0, true, false));
}

TEST_CASE("Town Portal contact policy leaves the deadline untouched after meeting Verdant and resets transiently")
{
    TownPortalContactState state;
    CHECK(state.onTownPortalAttempt(100.0, true, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(104.0));

    CHECK_FALSE(state.onTownPortalAttempt(1000.0, true, true));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(104.0));

    state = {};
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(0.0));
    CHECK(state.onTownPortalAttempt(1.0, true, false));
    CHECK_EQ(state.interruptAfterGameMinutes, doctest::Approx(5.0));
}

TEST_CASE("actual Town Portal first contact opens Verdant and a quiet-window retry opens three destinations")
{
    ActualTownPortalFixture fixture;
    fixture.activateOut01DimensionDoorBoundary();
    const PartyObservation initialParty = observeParty(fixture.party);

    const GameplaySpellService::SpellRequestResolution first = fixture.castTownPortal();
    REQUIRE(first.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(first.castResult.status == PartySpellCastStatus::NeedUtilityUi);
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    const EventDialogContent &contact = fixture.runtime().activeEventDialog();
    REQUIRE(contact.isActive);
    CHECK_EQ(contact.sourceId, VerdantNpcId);
    REQUIRE_EQ(contact.title, std::string("维尔丹"));
    const EventDialogAction *pReady =
        findDialogAction(contact, EventDialogActionKind::NpcTopic, VerdantReadyTopicId);
    REQUIRE(pReady != nullptr);
    CHECK(pReady->enabled);
    CHECK_EQ(pReady->label, fixture.readyTopicText);
    CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);
    CHECK_EQ(namedGlobal(fixture.eventState, GotMainQuestVar), 0);
    CHECK(fixture.eventState.messages.empty());
    checkPartyObservation(fixture.party, initialParty);

    fixture.dismissDialog();
    fixture.world.advanceGameMinutes(3.0f);
    const GameplaySpellService::SpellRequestResolution second = fixture.castTownPortal();
    REQUIRE(second.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(second.castResult.status == PartySpellCastStatus::NeedUtilityUi);
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_EQ(
        fixture.runtime().utilitySpellOverlayReadOnly().mode,
        GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
    CHECK_EQ(fixture.runtime().townPortalDestinations().size(), 3u);
    CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);
    CHECK_EQ(namedGlobal(fixture.eventState, GotMainQuestVar), 0);
    CHECK(fixture.eventState.messages.empty());
    checkPartyObservation(fixture.party, initialParty);
}

TEST_CASE("the first imported intro click disables later Town Portal contact without completing the quest")
{
    ActualTownPortalFixture fixture;
    fixture.activateOut01DimensionDoorBoundary();
    const PartyObservation initialParty = observeParty(fixture.party);
    REQUIRE(
        fixture.castTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    const EventDialogAction *pReady = findDialogAction(
        fixture.runtime().activeEventDialog(), EventDialogActionKind::NpcTopic, VerdantReadyTopicId);
    REQUIRE(pReady != nullptr);

    const size_t previousMessageCount = fixture.eventState.messages.size();
    size_t eventPreviousMessageCount = previousMessageCount;
    REQUIRE(fixture.world.executeNpcTopicEvent(VerdantReadyTopicId, eventPreviousMessageCount));
    REQUIRE_EQ(fixture.eventState.messages.size(), previousMessageCount + 1);
    CHECK_EQ(fixture.eventState.messages.back(), fixture.firstIntroMessage);
    CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 1);
    CHECK_EQ(namedGlobal(fixture.eventState, GotMainQuestVar), 0);
    checkPartyObservation(fixture.party, initialParty);

    fixture.dismissDialog();
    fixture.world.advanceGameMinutes(5.0f);
    const GameplaySpellService::SpellRequestResolution afterMeeting = fixture.castTownPortal();
    REQUIRE(afterMeeting.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_EQ(
        fixture.runtime().utilitySpellOverlayReadOnly().mode,
        GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
    CHECK_EQ(fixture.runtime().townPortalDestinations().size(), 3u);
    CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
    CHECK_EQ(fixture.eventState.messages.size(), previousMessageCount + 1);
    CHECK_EQ(fixture.eventState.messages.back(), fixture.firstIntroMessage);
    checkPartyObservation(fixture.party, initialParty);
}

TEST_CASE("ineligible and invalid Town Portal casts preserve contact eligibility boundaries")
{
    SUBCASE("an ordinary ineligible Town Portal attempt delays eligible contact")
    {
        ActualTownPortalFixture fixture;
        fixture.world.setPartyPosition(0.0f, 0.0f, 0.0f);
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_EQ(
            fixture.runtime().utilitySpellOverlayReadOnly().mode,
            GameplayUiController::UtilitySpellOverlayMode::TownPortal);
        fixture.closeTownPortalOverlay();

        fixture.world.setPartyPosition(-512.0f, 2560.0f, 0.0f);
        fixture.world.advanceGameMinutes(3.0f);
        fixture.activateOut01DimensionDoorBoundary();
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
        REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_EQ(
            fixture.runtime().utilitySpellOverlayReadOnly().mode,
            GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
        CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);
    }

    SUBCASE("insufficient mastery is rejected before contact policy")
    {
        ActualTownPortalFixture fixture;
        fixture.activateOut01DimensionDoorBoundary();
        Character *pCaster = fixture.party.member(0);
        REQUIRE(pCaster != nullptr);
        pCaster->skills["WaterMagic"] = {"WaterMagic", 0, SkillMastery::None};
        const GameplaySpellService::SpellRequestResolution rejected = fixture.castTownPortal();
        CHECK(rejected.disposition == GameplaySpellService::SpellRequestDisposition::Failed);
        CHECK(rejected.castResult.status == PartySpellCastStatus::NotSkilledEnough);
        CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
        CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);
    }

    SUBCASE("insufficient mana is rejected before contact policy")
    {
        ActualTownPortalFixture fixture;
        fixture.activateOut01DimensionDoorBoundary();
        Character *pCaster = fixture.party.member(0);
        REQUIRE(pCaster != nullptr);
        pCaster->spellPoints = 0;
        const GameplaySpellService::SpellRequestResolution rejected = fixture.castTownPortal();
        CHECK(rejected.disposition == GameplaySpellService::SpellRequestDisposition::Failed);
        CHECK(rejected.castResult.status == PartySpellCastStatus::NotEnoughSpellPoints);
        CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
        CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);

        pCaster->spellPoints = 100;
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        REQUIRE(fixture.runtime().activeEventDialog().isActive);
        CHECK_EQ(fixture.runtime().activeEventDialog().sourceId, VerdantNpcId);
    }

    SUBCASE("nearby hostility rejects a non-grandmaster cast before contact policy")
    {
        ActualTownPortalFixture fixture;
        fixture.activateOut01DimensionDoorBoundary();
        Character *pCaster = fixture.party.member(0);
        REQUIRE(pCaster != nullptr);
        pCaster->skills["WaterMagic"] = {"WaterMagic", 10, SkillMastery::Master};
        GameplayRuntimeActorState hostile;
        hostile.preciseX = -400.0f;
        hostile.preciseY = 2560.0f;
        hostile.preciseZ = 0.0f;
        hostile.radius = 48;
        hostile.hostileToParty = true;
        hostile.hasDetectedParty = true;
        fixture.world.addActor(hostile);

        const GameplaySpellService::SpellRequestResolution rejected = fixture.castTownPortal();
        CHECK(rejected.disposition == GameplaySpellService::SpellRequestDisposition::Failed);
        CHECK(rejected.castResult.status == PartySpellCastStatus::Failed);
        CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
        CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);
    }
}

TEST_CASE("a missing intro profile fails closed without delaying later contact")
{
    ActualTownPortalFixture fixture;
    fixture.activateOut01DimensionDoorBoundary();
    fixture.world.bindGlobalEventProgram(nullptr);

    REQUIRE(
        fixture.castTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_EQ(
        fixture.runtime().utilitySpellOverlayReadOnly().mode,
        GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
    CHECK_EQ(fixture.runtime().townPortalDestinations().size(), 3u);
    fixture.closeTownPortalOverlay();

    fixture.world.bindGlobalEventProgram(&fixture.loader.getSelectedMap()->globalEventProgram);
    REQUIRE(
        fixture.castTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(fixture.runtime().activeEventDialog().isActive);
    CHECK_EQ(fixture.runtime().activeEventDialog().sourceId, VerdantNpcId);
    CHECK_EQ(namedGlobal(fixture.eventState, MetVerdantVar), 0);
}

TEST_CASE("session clear and restore reset Town Portal contact while map changes do not")
{
    SUBCASE("map changes preserve the quiet window")
    {
        ActualTownPortalFixture fixture;
        fixture.world.setPartyPosition(0.0f, 0.0f, 0.0f);
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        fixture.closeTownPortalOverlay();

        ContactTestWorld otherWorld;
        otherWorld.bindContactRuntime(
            &fixture.party,
            &fixture.eventState,
            &fixture.loader.getSelectedMap()->localEventProgram,
            &fixture.loader.getSelectedMap()->globalEventProgram,
            &fixture.loader.getNpcDialogTable());
        otherWorld.bindMonsterTable(&fixture.loader.getMonsterTable());
        otherWorld.bindMergedBolsterMonsterTable(&fixture.loader.getMergedBolsterMonsterTable());
        otherWorld.setMapName("oute3.odm");
        otherWorld.advanceGameMinutes(fixture.world.gameMinutes());
        fixture.session.setCurrentMapFileName("oute3.odm");
        fixture.session.bindActiveWorldRuntime(&otherWorld);

        fixture.session.setCurrentMapFileName("out01.odm");
        fixture.session.bindActiveWorldRuntime(&fixture.world);
        fixture.world.setPartyPosition(-512.0f, 2560.0f, 0.0f);
        fixture.world.advanceGameMinutes(3.0f);
        fixture.activateOut01DimensionDoorBoundary();
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        CHECK_FALSE(fixture.runtime().activeEventDialog().isActive);
        REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_EQ(
            fixture.runtime().utilitySpellOverlayReadOnly().mode,
            GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
    }

    SUBCASE("session clear resets the quiet window")
    {
        ActualTownPortalFixture fixture;
        fixture.world.setPartyPosition(0.0f, 0.0f, 0.0f);
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        fixture.closeTownPortalOverlay();
        fixture.world.setPartyPosition(-512.0f, 2560.0f, 0.0f);

        fixture.session.clear();
        fixture.rebindSessionAfterClear();
        fixture.activateOut01DimensionDoorBoundary();
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        REQUIRE(fixture.runtime().activeEventDialog().isActive);
        CHECK_EQ(fixture.runtime().activeEventDialog().sourceId, VerdantNpcId);
    }

    SUBCASE("save restoration resets the quiet window")
    {
        ActualTownPortalFixture fixture;
        fixture.world.setPartyPosition(0.0f, 0.0f, 0.0f);
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        fixture.closeTownPortalOverlay();
        fixture.world.setPartyPosition(-512.0f, 2560.0f, 0.0f);

        GameSaveData save;
        save.currentSceneKind = SceneKind::Outdoor;
        save.mapFileName = "out01.odm";
        save.savedGameMinutes = fixture.world.gameMinutes();
        save.party = fixture.party.snapshot();
        fixture.session.restoreFromSaveData(save);
        fixture.activateOut01DimensionDoorBoundary();
        REQUIRE(
            fixture.castTownPortal().disposition
            == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        REQUIRE(fixture.runtime().activeEventDialog().isActive);
        CHECK_EQ(fixture.runtime().activeEventDialog().sourceId, VerdantNpcId);
    }
}
