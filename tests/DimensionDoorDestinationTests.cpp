#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "game/app/GameSession.h"
#include "game/app/GameSettings.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayPartyOverlayInputController.h"
#include "game/party/SpellIds.h"
#include "game/ui/GameplayOverlayAdapters.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
constexpr uint32_t TownPortalSpellId = spellIdValue(SpellId::TownPortal);
constexpr const char *MetVerdantVar = "MMerge.CrossContinents.MetVerdant";
constexpr int ScreenWidth = 1210;
constexpr int ScreenHeight = 834;

struct DestinationExpectation
{
    std::string id;
    uint32_t mapId = 0;
    std::string icon;
};

struct DayExpectation
{
    uint32_t day = 0;
    std::array<DestinationExpectation, 3> destinations;
};

std::filesystem::path originalDailyDestinationFixturePath()
{
#if defined(OPENYAMM_SOURCE_DIR)
    return std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "tests/fixtures/dimension_door_original_cycle.tsv";
#else
    throw std::runtime_error("OPENYAMM_SOURCE_DIR is required for the independent destination fixture");
#endif
}

std::vector<DayExpectation> loadOriginalDailyDestinations()
{
    std::ifstream input(originalDailyDestinationFixturePath());
    if (!input)
    {
        throw std::runtime_error("Cannot open the independent Dimension Door cycle fixture");
    }

    std::string header;
    std::getline(input, header);
    if (header != "day\tjadame_map\tjadame_icon\tantagarich_map\tantagarich_icon\tenroth_map\tenroth_icon")
    {
        throw std::runtime_error("Unexpected Dimension Door cycle fixture header");
    }

    std::vector<DayExpectation> result;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
        {
            continue;
        }

        DayExpectation expected;
        expected.destinations[0].id = "TPGlobal:0";
        expected.destinations[1].id = "TPGlobal:2";
        expected.destinations[2].id = "TPGlobal:4";
        std::istringstream row(line);
        if (!(row >> expected.day
                >> expected.destinations[0].mapId >> expected.destinations[0].icon
                >> expected.destinations[1].mapId >> expected.destinations[1].icon
                >> expected.destinations[2].mapId >> expected.destinations[2].icon))
        {
            throw std::runtime_error("Invalid Dimension Door cycle fixture row: " + line);
        }
        std::string trailing;
        if (row >> trailing)
        {
            throw std::runtime_error("Extra Dimension Door cycle fixture field: " + line);
        }
        if (expected.day != result.size())
        {
            throw std::runtime_error("Dimension Door cycle fixture days must be consecutive from zero");
        }
        result.push_back(std::move(expected));
    }

    if (result.size() != 240)
    {
        throw std::runtime_error("Dimension Door cycle fixture must contain exactly 240 days");
    }
    return result;
}

std::filesystem::path requiredRuntimeRoot()
{
    const char *value = std::getenv("OPENYAMM_CC_RUNTIME_ROOT");
    REQUIRE_MESSAGE(value != nullptr, "Missing OPENYAMM_CC_RUNTIME_ROOT");
    REQUIRE_MESSAGE(*value != '\0', "Empty OPENYAMM_CC_RUNTIME_ROOT");
    return value;
}

Party makeTownPortalParty(const GameDataLoader &loader, int spellPoints)
{
    Character member;
    member.name = "Dimension Door destination boundary member";
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
    // The ordinary dedicated-spot path is valid at Master while no enemies are nearby.
    // Keeping the boundary caster below Grandmaster catches an accidental GM override.
    member.skills["WaterMagic"] = {"WaterMagic", 10, SkillMastery::Master};
    REQUIRE(member.learnSpell(TownPortalSpellId));

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
    Character *caster = party.member(0);
    REQUIRE(caster != nullptr);
    REQUIRE_GE(caster->maxSpellPoints, 20);
    caster->spellPoints = spellPoints;
    caster->recoverySecondsRemaining = 0.0f;
    return party;
}

class DestinationFixture final : public IGameplayOverlaySceneAdapter
{
public:
    explicit DestinationFixture(float gameMinutes = 0.0f, int spellPoints = 20)
        : m_party(loadPartyAfterData(spellPoints))
    {
        m_settings.immortal = false;
        m_settings.unlimitedMana = false;
        m_session.setCurrentMapFileName("out01.odm");
        m_session.gameplayScreenRuntime().bindSettings(&m_settings);
        m_session.gameplayScreenRuntime().bindSceneAdapter(this);
        m_eventState.mapFileName = "out01.odm";
        m_eventState.namedGlobalVars[MetVerdantVar] = 1;
        m_world.bindParty(&m_party);
        m_world.bindEventRuntimeState(&m_eventState);
        m_world.bindMonsterTable(&m_loader.getMonsterTable());
        m_world.bindMergedBolsterMonsterTable(&m_loader.getMergedBolsterMonsterTable());
        m_world.setMapName("out01.odm");
        m_world.advanceGameMinutes(gameMinutes);
        m_session.bindActiveWorldRuntime(&m_world);
    }

    GameplayScreenRuntime &runtime()
    {
        return m_session.gameplayScreenRuntime();
    }

    GameplayUiRuntime &uiRuntime()
    {
        return m_session.gameplayUiRuntime();
    }

    GameSession &session()
    {
        return m_session;
    }

    const GameDataLoader &loader() const
    {
        return m_loader;
    }

    Party &party()
    {
        return m_party;
    }

    EventRuntimeState &eventState()
    {
        return m_eventState;
    }

    OpenYAMM::Tests::PartySpellTestWorldRuntime &world()
    {
        return m_world;
    }

    void prepareHeldDimensionDoorScroll()
    {
        Character *pMember = m_party.member(0);
        REQUIRE(pMember != nullptr);
        pMember->health = pMember->maxHealth;
        pMember->spellPoints = 0;
        pMember->recoverySecondsRemaining = 0.0f;
        pMember->forgetSpell(TownPortalSpellId);
        REQUIRE_EQ(m_party.inventoryItemCount(190), 0);

        GameplayUiController::HeldInventoryItemState &heldItem = runtime().heldInventoryItem();
        heldItem = {};
        heldItem.active = true;
        heldItem.item.objectDescriptionId = 190;
        heldItem.item.quantity = 1;
        m_party.setHeldItemForQueries(heldItem.item);
    }

    bool useHeldItemOnMember(size_t memberIndex = 0)
    {
        return m_session.gameplayItemService().tryUseHeldItemOnPartyMember(runtime(), memberIndex, false);
    }

    GameplaySpellService::SpellRequestResolution beginTownPortal(PartySpellCastRequest request = {})
    {
        request.casterMemberIndex = 0;
        request.spellId = TownPortalSpellId;
        return m_session.gameplaySpellService().resolveSpellRequest(runtime(), request, "Town Portal");
    }

    float gameplayCameraYawRadians() const override
    {
        return 0.0f;
    }

    void executeActiveDialogAction() override
    {
    }

    bool tryCastSpellRequest(
        const PartySpellCastRequest &request,
        const std::string &spellName) override
    {
        GameplaySpellService &spellService = m_session.gameplaySpellService();
        GameplayScreenRuntime &screenRuntime = runtime();
        const GameplaySpellService::SpellRequestResolution resolution =
            spellService.resolveSpellRequest(screenRuntime, request, spellName);

        if (resolution.disposition == GameplaySpellService::SpellRequestDisposition::CastSucceeded)
        {
            spellService.clearPendingTargetSelection(screenRuntime, "Cast " + spellName);
            return true;
        }

        if (resolution.disposition == GameplaySpellService::SpellRequestDisposition::NeedsTargetSelection)
        {
            spellService.armPendingTargetSelection(
                screenRuntime,
                request,
                resolution.castResult.targetKind,
                spellName);
            return true;
        }

        return resolution.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi;
    }

    bool trySaveToSelectedGameSlot() override
    {
        return false;
    }

    void clickLayout(const std::string &layoutId)
    {
        GameplayScreenRuntime &screen = runtime();
        const GameplayScreenRuntime::HudLayoutElement *layout = screen.findHudLayoutElement(layoutId);
        REQUIRE(layout != nullptr);
        REQUIRE(layout->visible);
        const auto resolved = screen.resolveHudLayoutElement(
            layoutId, ScreenWidth, ScreenHeight, layout->width, layout->height);
        REQUIRE(resolved.has_value());
        clickAt(resolved->x + resolved->width * 0.5f, resolved->y + resolved->height * 0.5f);
    }

    void clickDestination(const std::string &destinationId)
    {
        GameplayScreenRuntime &screen = runtime();
        const auto destination = std::find_if(
            screen.townPortalDestinations().begin(), screen.townPortalDestinations().end(),
            [&](const GameplayTownPortalDestination &candidate) { return candidate.id == destinationId; });
        REQUIRE(destination != screen.townPortalDestinations().end());

        const GameplayScreenRuntime::HudLayoutElement *backgroundLayout =
            screen.findHudLayoutElement("TownPortalBackground");
        REQUIRE(backgroundLayout != nullptr);
        REQUIRE(backgroundLayout->visible);
        const auto background = screen.resolveHudLayoutElement(
            "TownPortalBackground",
            ScreenWidth,
            ScreenHeight,
            backgroundLayout->width,
            backgroundLayout->height);
        REQUIRE(background.has_value());

        // Source artwork has three exclusive stars; rectangle centers overlap other destinations.
        const float logicalX = destinationId == "TPGlobal:0" ? 37.0f
            : destinationId == "TPGlobal:2" ? 592.0f : 310.0f;
        const float logicalY = destinationId == "TPGlobal:4" ? 24.0f : 100.0f;
        const float scale = background->width / 640.0f;
        clickAt(background->x + logicalX * scale, background->y + logicalY * scale);
    }

    std::optional<size_t> destinationAtLogicalPoint(float x, float y)
    {
        const auto background = runtime().resolveHudLayoutElement(
            "TownPortalBackground", ScreenWidth, ScreenHeight, 640.0f, 480.0f);
        REQUIRE(background.has_value());
        const float scale = background->width / 640.0f;
        return runtime().resolveTownPortalDestinationAtPoint(
            ScreenWidth, ScreenHeight, background->x + x * scale, background->y + y * scale);
    }

    void dragLogicalPoint(float fromX, float fromY, float toX, float toY)
    {
        const auto background = runtime().resolveHudLayoutElement(
            "TownPortalBackground", ScreenWidth, ScreenHeight, 640.0f, 480.0f);
        REQUIRE(background.has_value());
        const float scale = background->width / 640.0f;
        GameplayInputFrame input;
        input.screenWidth = ScreenWidth;
        input.screenHeight = ScreenHeight;
        input.pointerX = background->x + fromX * scale;
        input.pointerY = background->y + fromY * scale;
        input.leftMouseButton = {true, true, false};
        m_session.updateGameplay(input, 0.0f);
        input.pointerX = background->x + toX * scale;
        input.pointerY = background->y + toY * scale;
        input.leftMouseButton = {false, false, true};
        m_session.updateGameplay(input, 0.0f);
    }

private:
    void clickAt(float x, float y)
    {
        GameplayInputFrame input;
        input.screenWidth = ScreenWidth;
        input.screenHeight = ScreenHeight;
        input.pointerX = x;
        input.pointerY = y;
        input.leftMouseButton = {true, true, false};
        m_session.updateGameplay(input, 0.0f);
        input.leftMouseButton = {false, false, true};
        m_session.updateGameplay(input, 0.0f);
    }

    Party loadPartyAfterData(int spellPoints)
    {
        REQUIRE(m_files.initialize(
            requiredRuntimeRoot(),
            requiredRuntimeRoot(),
            OpenYAMM::Engine::AssetScaleTier::X1,
            "mm6"));
        m_loader.setActiveWorldId("mm6");
        REQUIRE(m_loader.loadCommonForGameplay(m_files));
        m_repository.bind(m_loader);
        // Match the application's shared UI bootstrap: the repository supplies
        // destination data, while the mounted package supplies normal layout files.
        m_session.gameplayScreenRuntime().bindAssetFileSystem(&m_files);
        m_session.bindDataRepository(&m_repository);
        REQUIRE(m_session.gameplayScreenRuntime().ensureGameplayLayoutsLoaded());
        return makeTownPortalParty(m_loader, spellPoints);
    }

    OpenYAMM::Engine::AssetFileSystem m_files;
    GameDataLoader m_loader;
    GameDataRepository m_repository;
    GameSession m_session;
    GameSettings m_settings = GameSettings::createDefault();
    Party m_party;
    EventRuntimeState m_eventState;
    OpenYAMM::Tests::PartySpellTestWorldRuntime m_world;
};

const GameplayTownPortalDestination &destinationById(
    const std::vector<GameplayTownPortalDestination> &destinations,
    const std::string &id)
{
    const auto iterator = std::find_if(
        destinations.begin(), destinations.end(),
        [&](const GameplayTownPortalDestination &candidate) { return candidate.id == id; });
    REQUIRE_MESSAGE(iterator != destinations.end(), ("Missing destination " + id).c_str());
    return *iterator;
}

void checkDailyDestinations(
    const DestinationFixture &fixture,
    const std::vector<GameplayTownPortalDestination> &destinations,
    const DayExpectation &expected)
{
    REQUIRE_EQ(destinations.size(), expected.destinations.size());
    for (const DestinationExpectation &entry : expected.destinations)
    {
        const GameplayTownPortalDestination &actual = destinationById(destinations, entry.id);
        const MapStatsEntry *map = fixture.loader().getMapStats().findById(entry.mapId);
        REQUIRE(map != nullptr);
        CHECK_EQ(actual.mapName, map->fileName);
        CHECK_EQ(actual.iconTextureName, entry.icon);
        CHECK_EQ(actual.x, 0);
        CHECK_EQ(actual.y, 0);
        CHECK_EQ(actual.z, 0);
        REQUIRE(actual.directionDegrees.has_value());
        CHECK_EQ(*actual.directionDegrees, 0);
    }
}
}

TEST_CASE("actual TPGlobal data matches the independent 240-day fixture for two complete cycles")
{
    DestinationFixture fixture;
    const std::vector<DayExpectation> expectedCycle = loadOriginalDailyDestinations();
    REQUIRE_EQ(expectedCycle.size(), 240);
    for (uint32_t day = 0; day < 480; ++day)
    {
        const DayExpectation &expected = expectedCycle[day % expectedCycle.size()];
        CAPTURE(day);
        REQUIRE(fixture.uiRuntime().ensureDimensionDoorDestinationsLoaded(day));
        checkDailyDestinations(fixture, fixture.uiRuntime().townPortalDestinations(), expected);
    }
}

TEST_CASE("activation freezes destination day across midnight until explicit reactivation")
{
    const std::vector<DayExpectation> expectedCycle = loadOriginalDailyDestinations();
    DestinationFixture fixture(1439.75f);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.runtime().ensureDimensionDoorDestinationsLoaded());
    checkDailyDestinations(fixture, fixture.runtime().townPortalDestinations(), expectedCycle[0]);
    REQUIRE(fixture.beginTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    fixture.clickLayout("TownPortalCloseButton");
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);

    fixture.world().advanceGameMinutes(1.0f);
    REQUIRE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    REQUIRE(fixture.runtime().ensureDimensionDoorDestinationsLoaded());
    checkDailyDestinations(fixture, fixture.runtime().townPortalDestinations(), expectedCycle[0]);

    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.runtime().ensureDimensionDoorDestinationsLoaded());
    checkDailyDestinations(fixture, fixture.runtime().townPortalDestinations(), expectedCycle[1]);
    REQUIRE(fixture.beginTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    fixture.clickLayout("TownPortalCloseButton");

    fixture.world().advanceGameMinutes(1.0f);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.runtime().ensureDimensionDoorDestinationsLoaded());
    checkDailyDestinations(fixture, fixture.runtime().townPortalDestinations(), expectedCycle[1]);
}

TEST_CASE("original Antagarich ceil boundaries retain explicit Lua-double samples")
{
    struct BoundaryExpectation
    {
        uint32_t day = 0;
        uint32_t mapId = 0;
        const char *icon = nullptr;
    };
    constexpr std::array<BoundaryExpectation, 12> cases = {{
        {0, 64, "tpantag1"},
        {1, 67, "tpantag2"},
        {5, 65, "tpantag6"},
        {6, 65, "tpantag6"},
        {7, 66, "tpantag7"},
        {11, 67, "tpantag3"},
        {12, 67, "tpantag3"},
        {47, 64, "tpantag1"},
        {48, 64, "tpantag1"},
        {239, 64, "tpantag1"},
        {240, 64, "tpantag1"},
        {479, 64, "tpantag1"},
    }};

    DestinationFixture fixture;
    for (const BoundaryExpectation &expected : cases)
    {
        CAPTURE(expected.day);
        REQUIRE(fixture.uiRuntime().ensureDimensionDoorDestinationsLoaded(expected.day));
        const GameplayTownPortalDestination &actual =
            destinationById(fixture.uiRuntime().townPortalDestinations(), "TPGlobal:2");
        const MapStatsEntry *map = fixture.loader().getMapStats().findById(expected.mapId);
        REQUIRE(map != nullptr);
        CHECK_EQ(actual.mapName, map->fileName);
        CHECK_EQ(actual.iconTextureName, expected.icon);
    }
}

TEST_CASE("ordinary Town Portal validates the twenty-point cost before opening Dimension Door")
{
    SUBCASE("nineteen spell points fail before selection")
    {
        DestinationFixture fixture(23.0f * 1440.0f, 19);
        fixture.runtime().activateDimensionDoor();
        const GameplaySpellService::SpellRequestResolution result = fixture.beginTownPortal();
        CHECK(result.disposition == GameplaySpellService::SpellRequestDisposition::Failed);
        CHECK(result.castResult.status == PartySpellCastStatus::NotEnoughSpellPoints);
        CHECK_EQ(fixture.party().member(0)->spellPoints, 19);
        CHECK_EQ(fixture.party().member(0)->recoverySecondsRemaining, doctest::Approx(0.0f));
        CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK_FALSE(fixture.eventState().pendingMapMove.has_value());
    }

    SUBCASE("twenty spell points open selection without an early charge")
    {
        DestinationFixture fixture(23.0f * 1440.0f, 20);
        fixture.runtime().activateDimensionDoor();
        const GameplaySpellService::SpellRequestResolution result = fixture.beginTownPortal();
        REQUIRE(result.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        REQUIRE(result.castResult.status == PartySpellCastStatus::NeedUtilityUi);
        CHECK_EQ(fixture.party().member(0)->spellPoints, 20);
        CHECK_EQ(fixture.party().member(0)->recoverySecondsRemaining, doctest::Approx(0.0f));
        REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().mode
            == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
        CHECK_EQ(fixture.runtime().utilitySpellOverlayReadOnly().skillLevelOverride, 0u);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().skillMasteryOverride == SkillMastery::None);
        REQUIRE(fixture.party().member(0)->findSkillByCanonicalName("WaterMagic") != nullptr);
        CHECK(fixture.party().member(0)->findSkillByCanonicalName("WaterMagic")->mastery == SkillMastery::Master);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().spendMana);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().applyRecovery);
        CHECK_FALSE(fixture.eventState().pendingMapMove.has_value());
    }
}

TEST_CASE("cancelling ordinary Dimension Door selection preserves mana recovery and map state")
{
    DestinationFixture fixture(23.0f * 1440.0f, 20);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.beginTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);

    fixture.clickLayout("TownPortalCloseButton");

    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_EQ(fixture.party().member(0)->spellPoints, 20);
    CHECK_EQ(fixture.party().member(0)->recoverySecondsRemaining, doctest::Approx(0.0f));
    CHECK_FALSE(fixture.eventState().pendingMapMove.has_value());
}

TEST_CASE("direct Town Portal request context remains free through Dimension Door selection")
{
    DestinationFixture fixture(23.0f * 1440.0f, 20);
    fixture.runtime().activateDimensionDoor();
    PartySpellCastRequest directRequest;
    directRequest.skillLevelOverride = 10;
    directRequest.skillMasteryOverride = SkillMastery::Grandmaster;
    directRequest.spendMana = false;
    directRequest.applyRecovery = false;

    const GameplaySpellService::SpellRequestResolution opened = fixture.beginTownPortal(directRequest);
    REQUIRE(opened.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_EQ(fixture.runtime().utilitySpellOverlayReadOnly().skillLevelOverride, 10u);
    CHECK(fixture.runtime().utilitySpellOverlayReadOnly().skillMasteryOverride == SkillMastery::Grandmaster);
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().spendMana);
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().applyRecovery);

    fixture.clickDestination("TPGlobal:4");

    CHECK_EQ(fixture.party().member(0)->spellPoints, 20);
    CHECK_EQ(fixture.party().member(0)->recoverySecondsRemaining, doctest::Approx(0.0f));
    REQUIRE(fixture.eventState().pendingMapMove.has_value());
    const MapStatsEntry *expectedMap = fixture.loader().getMapStats().findById(143);
    REQUIRE(expectedMap != nullptr);
    REQUIRE(fixture.eventState().pendingMapMove->mapName.has_value());
    CHECK_EQ(*fixture.eventState().pendingMapMove->mapName, expectedMap->fileName);
    CHECK(fixture.eventState().pendingMapMove->useMapStartPosition);
    CHECK_FALSE(fixture.eventState().pendingMapMove->directionDegrees.has_value());
    CHECK_FALSE(fixture.eventState().pendingMapMove->lookAngleDegrees.has_value());
}

TEST_CASE("held Dimension Door scroll opens and cancels through the item service with a one-minute timer")
{
    DestinationFixture fixture(100.0f, 0);
    fixture.prepareHeldDimensionDoorScroll();
    const Character *pMember = fixture.party().member(0);
    REQUIRE(pMember != nullptr);
    CHECK_EQ(pMember->health, pMember->maxHealth);
    CHECK_EQ(pMember->spellPoints, 0);
    CHECK_EQ(pMember->recoverySecondsRemaining, doctest::Approx(0.0f));
    CHECK_FALSE(pMember->knowsSpell(TownPortalSpellId));
    CHECK_EQ(fixture.party().heldItemIdForQueries(), 190u);

    REQUIRE(fixture.useHeldItemOnMember());
    CHECK_FALSE(fixture.runtime().heldInventoryItem().active);
    CHECK_EQ(fixture.party().heldItemIdForQueries(), 0u);
    CHECK_EQ(fixture.party().inventoryItemCount(190), 0);
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK(fixture.runtime().utilitySpellOverlayReadOnly().mode
        == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
    CHECK_EQ(fixture.runtime().utilitySpellOverlayReadOnly().skillLevelOverride, 10u);
    CHECK(fixture.runtime().utilitySpellOverlayReadOnly().skillMasteryOverride == SkillMastery::Grandmaster);
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().spendMana);
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().applyRecovery);
    CHECK_FALSE(fixture.useHeldItemOnMember());

    fixture.clickLayout("TownPortalCloseButton");
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_FALSE(fixture.eventState().pendingMapMove.has_value());
    CHECK_EQ(pMember->spellPoints, 0);
    CHECK_EQ(pMember->recoverySecondsRemaining, doctest::Approx(0.0f));
    CHECK_EQ(fixture.party().inventoryItemCount(190), 0);

    fixture.world().advanceGameMinutes(0.999f);
    CHECK(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
    fixture.world().advanceGameMinutes(0.001f);
    CHECK_FALSE(fixture.runtime().shouldTownPortalCastOpenDimensionDoor());
}

TEST_CASE("held Dimension Door scroll selects one daily target without mana recovery or duplicate requests")
{
    DestinationFixture fixture(23.0f * 1440.0f, 0);
    fixture.prepareHeldDimensionDoorScroll();
    REQUIRE(fixture.useHeldItemOnMember());
    REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);

    fixture.clickDestination("TPGlobal:2");

    const Character *pMember = fixture.party().member(0);
    REQUIRE(pMember != nullptr);
    CHECK_EQ(pMember->spellPoints, 0);
    CHECK_EQ(pMember->recoverySecondsRemaining, doctest::Approx(0.0f));
    CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
    CHECK_FALSE(fixture.runtime().heldInventoryItem().active);
    CHECK_EQ(fixture.party().heldItemIdForQueries(), 0u);
    CHECK_EQ(fixture.party().inventoryItemCount(190), 0);
    REQUIRE(fixture.eventState().pendingMapMove.has_value());
    const MapStatsEntry *pExpectedMap = fixture.loader().getMapStats().findById(70);
    REQUIRE(pExpectedMap != nullptr);
    REQUIRE(fixture.eventState().pendingMapMove->mapName.has_value());
    CHECK_EQ(*fixture.eventState().pendingMapMove->mapName, pExpectedMap->fileName);
    CHECK(fixture.eventState().pendingMapMove->useMapStartPosition);

    fixture.eventState().pendingMapMove.reset();
    fixture.clickDestination("TPGlobal:2");
    CHECK_FALSE(fixture.eventState().pendingMapMove.has_value());
    CHECK_FALSE(fixture.useHeldItemOnMember());
}

TEST_CASE("same-map Dimension Door keeps the authored literal zero transform")
{
    DestinationFixture fixture(0.0f, 20);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.beginTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);

    fixture.clickDestination("TPGlobal:0");

    const Character *caster = fixture.party().member(0);
    REQUIRE(caster != nullptr);
    CHECK_EQ(caster->spellPoints, 0);
    CHECK_GT(caster->recoverySecondsRemaining, 0.0f);
    REQUIRE(fixture.eventState().pendingMapMove.has_value());
    const EventRuntimeState::PendingMapMove &move = *fixture.eventState().pendingMapMove;
    const MapStatsEntry *expectedMap = fixture.loader().getMapStats().findById(1);
    REQUIRE(expectedMap != nullptr);
    REQUIRE(move.mapName.has_value());
    CHECK_EQ(*move.mapName, expectedMap->fileName);
    CHECK_EQ(move.x, 0);
    CHECK_EQ(move.y, 0);
    CHECK_EQ(move.z, 0);
    REQUIRE(move.directionDegrees.has_value());
    CHECK_EQ(*move.directionDegrees, 0);
    REQUIRE(move.lookAngleDegrees.has_value());
    CHECK_EQ(*move.lookAngleDegrees, 0);
    CHECK_FALSE(move.useMapStartPosition);
}

TEST_CASE("ordinary pointer selection covers every original map and icon combination")
{
    struct SelectionCase
    {
        uint32_t day = 0;
        DestinationExpectation destination;
    };

    const std::vector<DayExpectation> expectedCycle = loadOriginalDailyDestinations();
    std::set<std::string> uniqueKeys;
    std::vector<SelectionCase> cases;
    for (const DayExpectation &day : expectedCycle)
    {
        for (const DestinationExpectation &destination : day.destinations)
        {
            const std::string key =
                destination.id + ":" + std::to_string(destination.mapId) + ":" + destination.icon;
            if (uniqueKeys.insert(key).second)
            {
                cases.push_back({day.day, destination});
            }
        }
    }
    REQUIRE_EQ(cases.size(), 21);

    for (const SelectionCase &testCase : cases)
    {
        CAPTURE(testCase.day);
        CAPTURE(testCase.destination.id);
        CAPTURE(testCase.destination.mapId);
        CAPTURE(testCase.destination.icon);
        DestinationFixture fixture(static_cast<float>(testCase.day) * 1440.0f, 20);
        fixture.party().setQuestBit(4, true);
        fixture.runtime().activateDimensionDoor();
        const GameplaySpellService::SpellRequestResolution opened = fixture.beginTownPortal();
        REQUIRE(opened.disposition == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
        REQUIRE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().mode
            == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().spendMana);
        CHECK(fixture.runtime().utilitySpellOverlayReadOnly().applyRecovery);
        const GameplayTownPortalDestination &resolved =
            destinationById(fixture.runtime().townPortalDestinations(), testCase.destination.id);
        CHECK_EQ(resolved.iconTextureName, testCase.destination.icon);

        fixture.clickDestination(testCase.destination.id);

        const Character *caster = fixture.party().member(0);
        REQUIRE(caster != nullptr);
        CHECK_EQ(caster->spellPoints, 0);
        CHECK_GT(caster->recoverySecondsRemaining, 0.0f);
        REQUIRE(fixture.eventState().pendingMapMove.has_value());
        const EventRuntimeState::PendingMapMove &move = *fixture.eventState().pendingMapMove;
        const MapStatsEntry *expectedMap =
            fixture.loader().getMapStats().findById(testCase.destination.mapId);
        REQUIRE(expectedMap != nullptr);
        REQUIRE(move.mapName.has_value());
        CHECK_EQ(*move.mapName, expectedMap->fileName);
        CHECK_EQ(move.x, 0);
        CHECK_EQ(move.y, 0);
        CHECK_EQ(move.z, 0);
        const bool sameMap = expectedMap->fileName == "out01.odm";
        CHECK_EQ(move.useMapStartPosition, !sameMap);
        if (sameMap)
        {
            REQUIRE(move.directionDegrees.has_value());
            CHECK_EQ(*move.directionDegrees, 0);
            REQUIRE(move.lookAngleDegrees.has_value());
            CHECK_EQ(*move.lookAngleDegrees, 0);
        }
        else
        {
            CHECK_FALSE(move.directionDegrees.has_value());
            CHECK_FALSE(move.lookAngleDegrees.has_value());
        }
        CHECK_FALSE(fixture.runtime().utilitySpellOverlayReadOnly().active);
        CHECK(fixture.party().hasQuestBit(4));
    }
}

TEST_CASE("ordinary Town Portal destinations remain separate after Dimension Door cycle loads")
{
    DestinationFixture fixture(239.0f * 1440.0f);
    REQUIRE(fixture.uiRuntime().ensureDimensionDoorDestinationsLoaded(239));
    REQUIRE_EQ(fixture.uiRuntime().townPortalDestinations().size(), 3);
    for (const GameplayTownPortalDestination &destination : fixture.uiRuntime().townPortalDestinations())
    {
        CHECK(destination.id.starts_with("TPGlobal:"));
    }

    REQUIRE(fixture.uiRuntime().ensureTownPortalDestinationsLoaded("out01.odm"));
    REQUIRE_FALSE(fixture.uiRuntime().townPortalDestinations().empty());
    for (const GameplayTownPortalDestination &destination : fixture.uiRuntime().townPortalDestinations())
    {
        CHECK_FALSE(destination.id.starts_with("TPGlobal:"));
    }

    REQUIRE(fixture.uiRuntime().ensureDimensionDoorDestinationsLoaded(240));
    REQUIRE_EQ(fixture.uiRuntime().townPortalDestinations().size(), 3);
    for (const GameplayTownPortalDestination &destination : fixture.uiRuntime().townPortalDestinations())
    {
        CHECK(destination.id.starts_with("TPGlobal:"));
    }
}

TEST_CASE("original portal image bounds select exclusive stars and topmost overlapping slots")
{
    DestinationFixture fixture(23.0f * 1440.0f);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.runtime().ensureDimensionDoorDestinationsLoaded());
    struct PointCase
    {
        float x;
        float y;
        std::optional<size_t> index;
    };
    // Original slot order is 4 > 2 > 0; native visible indices are 2 > 1 > 0.
    const std::array<PointCase, 11> cases = {{
        {37, 100, 0}, {592, 100, 1}, {310, 24, 2},
        {300, 200, 2}, {500, 200, 1}, {100, 200, 0},
        {310, 400, 2}, {310, 403, 1},
        {20, 430, std::nullopt}, {50, 20, std::nullopt}, {500, 20, std::nullopt},
    }};
    for (const PointCase &point : cases)
    {
        CAPTURE(point.x);
        CAPTURE(point.y);
        CHECK(fixture.destinationAtLogicalPoint(point.x, point.y) == point.index);
    }
}

TEST_CASE("portal release keeps the pressed destination inside its own overlapping image bounds")
{
    DestinationFixture fixture(23.0f * 1440.0f);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.beginTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(fixture.destinationAtLogicalPoint(200, 100) == std::optional<size_t>(2));
    fixture.dragLogicalPoint(37, 100, 200, 100);
    REQUIRE(fixture.eventState().pendingMapMove.has_value());
    CHECK(fixture.eventState().pendingMapMove->mapName == std::optional<std::string>("out08.odm"));
    CHECK_EQ(fixture.party().member(0)->spellPoints, 0);
    CHECK_GT(fixture.party().member(0)->recoverySecondsRemaining, 0.0f);
}

TEST_CASE("portal release outside the pressed image cancels even over another destination")
{
    DestinationFixture fixture(23.0f * 1440.0f);
    fixture.runtime().activateDimensionDoor();
    REQUIRE(fixture.beginTownPortal().disposition
        == GameplaySpellService::SpellRequestDisposition::OpenedSelectionUi);
    REQUIRE(fixture.destinationAtLogicalPoint(500, 100) == std::optional<size_t>(1));
    fixture.dragLogicalPoint(37, 100, 500, 100);
    CHECK_FALSE(fixture.eventState().pendingMapMove.has_value());
    CHECK_EQ(fixture.party().member(0)->spellPoints, 20);
    CHECK_EQ(fixture.party().member(0)->recoverySecondsRemaining, 0.0f);
    CHECK(fixture.runtime().utilitySpellOverlayReadOnly().active);
}
