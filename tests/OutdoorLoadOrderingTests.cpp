#include "EventSupportSource.h"
#include "doctest/doctest.h"

#include "game/app/GameApplication.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
struct CameraObservation
{
    float yaw = 0.0f;
    float pitch = 0.0f;
};

class ObservedOutdoorWorld : public OutdoorWorldRuntime
{
public:
    explicit ObservedOutdoorWorld(const OutdoorGameView &view) : m_view(view)
    {
    }

    float partyX() const override
    {
        // Lua's real GetPartyPosition calls this getter. Observe the camera at that exact point;
        // the position still comes from the production world, with no mocked restore or synchronization.
        camerasOnPositionRead.push_back({m_view.cameraYawRadians(), m_view.cameraPitchRadians()});
        return OutdoorWorldRuntime::partyX();
    }

    mutable std::vector<CameraObservation> camerasOnPositionRead;

private:
    const OutdoorGameView &m_view;
};
}

struct GameApplicationTestAccess
{
    static void checkOutdoorAfterLoadOrdering(bool loadingSavedGame)
    {
        // These referenced map objects outlive the application and its world/party runtimes.
        std::optional<OutdoorMapData> geometry = OutdoorMapData{};
        geometry->heightMap.assign(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 0);
        geometry->attributeMap.assign(geometry->heightMap.size(), 0);
        MapStatsEntry map;
        map.fileName = "load-order-fixture.odm";
        map.name = "Load order fixture";

        std::string error;
        const std::optional<ScriptedEventProgram> global = ScriptedEventProgram::loadFromLuaText(
            std::string(OpenYAMM::Import::EventSupportSource) + R"lua(
RegisterGlobalAfterLoadEvent(65461, nil, function()
    local x, y, z = evt.GetPartyPosition(true)
    evt.SetGlobalVar("Fixture.SeenX", x)
    evt.SetGlobalVar("Fixture.SeenY", y)
    evt.SetGlobalVar("Fixture.SeenZ", z)
    evt.SetGlobalVar("Fixture.AfterLoadCount", evt.GetGlobalVar("Fixture.AfterLoadCount", 0) + 1)
    AddValue(Gold, 17)
end)
)lua", "@outdoor-load-ordering-fixture", ScriptedEventScope::Global, error);
        REQUIRE_MESSAGE(global.has_value(), error);

        // Do not call run(), initializeRenderer(), or mount an AssetFileSystem. Empty table objects
        // supply the real dependencies; the flat map has no actors, models, treasures or resources.
        GameApplication application(Engine::ApplicationConfig::createDefault());
        application.m_gameDataRepository.bind(application.m_gameDataLoader);
        application.m_gameSession.bindDataRepository(&application.m_gameDataRepository);
        application.m_gameplayController.bindSession(application.m_gameSession);
        application.m_settings.gameplayTrace = false;
        application.m_settings.combatTrace = false;
        application.m_settings.collisionTrace = false;
        application.m_settings.startFlying = false;
        application.m_settings.immortal = false;
        application.m_settings.unlimitedMana = false;
        const GameDataRepository &data = application.m_gameDataRepository;
        application.m_pOutdoorPartyRuntime = std::make_unique<OutdoorPartyRuntime>(
            OutdoorMovementDriver(*geometry, std::nullopt, std::nullopt, std::nullopt, std::nullopt),
            data.itemTable());
        OutdoorPartyRuntime &partyRuntime = *application.m_pOutdoorPartyRuntime;
        partyRuntime.initialize(-96.0f, 128.0f, 1.0f, false);
        PartySeed seed;
        seed.gold = 37;
        seed.members.resize(1);
        seed.members[0].characterDataId = 1;
        seed.members[0].name = "Load order observer";
        seed.members[0].health = seed.members[0].maxHealth = 100;
        partyRuntime.party().seed(seed);

        std::optional<EventRuntimeState> initialState = EventRuntimeState{};
        initialState->mapFileName = map.fileName;
        initialState->normalGameplayScreen = true;
        auto pWorld = std::make_unique<ObservedOutdoorWorld>(application.m_outdoorGameView);
        ObservedOutdoorWorld *pObservedWorld = pWorld.get();
        pWorld->initialize(map, data.monsterTable(), data.monsterProjectileTable(), data.objectTable(),
            data.spellTable(), data.itemTable(), &partyRuntime.party(), &partyRuntime,
            data.standardItemEnchantTable(), data.specialItemEnchantTable(), nullptr,
            geometry, std::nullopt, std::nullopt, initialState);
        application.m_pOutdoorWorldRuntime = std::move(pWorld);
        application.m_pMapSceneRuntime = std::make_unique<OutdoorSceneRuntime>(map.fileName, map,
            partyRuntime, *application.m_pOutdoorWorldRuntime, std::nullopt, global);
        application.m_gameSession.setCurrentSceneKind(SceneKind::Outdoor);
        application.m_gameSession.setCurrentMapFileName(map.fileName);
        application.m_gameSession.setPartyState(partyRuntime.party());
        application.m_gameplayController.bindRuntime(application.m_pMapSceneRuntime.get());
        REQUIRE(application.m_pAssetFileSystem == nullptr);

        const OutdoorPartyRuntime::Snapshot initialPose = partyRuntime.snapshot();
        OutdoorPartyRuntime::Snapshot savedPose = initialPose;
        savedPose.movementState.x = 20096.0f;
        savedPose.movementState.y = -16448.0f;
        savedPose.movementState.footZ = 2477.0f;
        savedPose.movementState.verticalVelocity = -12.5f;
        savedPose.movementState.airborne = true;
        application.m_gameSession.setOutdoorPartyState(savedPose);
        application.m_gameSession.setOutdoorCameraAngles(1.25f, -0.4f);
        application.m_outdoorGameView.setCameraAngles(-2.0f, 0.2f);
        application.m_loadingSavedGameRuntime = loadingSavedGame;
        pObservedWorld->camerasOnPositionRead.clear();

        // The regression enters the same production method used after renderer and headless map setup.
        // There is intentionally no test-side application of savedPose before this call.
        REQUIRE(application.executeCurrentMapAfterLoadEvents());
        const OutdoorPartyRuntime::Snapshot &expectedPose = loadingSavedGame ? savedPose : initialPose;
        const float expectedYaw = loadingSavedGame ? 1.25f : -2.0f;
        const float expectedPitch = loadingSavedGame ? -0.4f : 0.2f;
        const EventRuntimeState &state = *application.m_pMapSceneRuntime->eventRuntimeState();
        CHECK(state.namedGlobalVars.at("Fixture.SeenX") == int32_t(expectedPose.movementState.x));
        CHECK(state.namedGlobalVars.at("Fixture.SeenY") == int32_t(expectedPose.movementState.y));
        CHECK(state.namedGlobalVars.at("Fixture.SeenZ") == int32_t(expectedPose.movementState.footZ));
        CHECK(state.namedGlobalVars.at("Fixture.AfterLoadCount") == 1);
        CHECK_FALSE(state.normalGameplayScreen);
        REQUIRE_FALSE(pObservedWorld->camerasOnPositionRead.empty());
        for (const CameraObservation &camera : pObservedWorld->camerasOnPositionRead)
        {
            CHECK(camera.yaw == doctest::Approx(expectedYaw));
            CHECK(camera.pitch == doctest::Approx(expectedPitch));
        }
        CHECK(partyRuntime.party().gold() == 54);
        REQUIRE(application.m_gameSession.partyState().has_value());
        CHECK(application.m_gameSession.partyState()->gold() == 54);
        CHECK(application.m_gameSession.namedGlobalVars().at("Fixture.AfterLoadCount") == 1);
        CHECK(application.m_gameSession.namedGlobalVars().at("Fixture.SeenZ")
            == int32_t(expectedPose.movementState.footZ));

        // quickLoad's existing final phase consumes the post-hook Session, never the old saved data.
        application.m_loadingSavedGameRuntime = false;
        REQUIRE(application.applyCurrentSessionToRuntime());
        CHECK(partyRuntime.movementState().x == expectedPose.movementState.x);
        CHECK(partyRuntime.movementState().y == expectedPose.movementState.y);
        CHECK(partyRuntime.movementState().footZ == expectedPose.movementState.footZ);
        CHECK(partyRuntime.movementState().verticalVelocity == expectedPose.movementState.verticalVelocity);
        CHECK(partyRuntime.movementState().airborne == expectedPose.movementState.airborne);
        CHECK(application.m_outdoorGameView.cameraYawRadians() == doctest::Approx(expectedYaw));
        CHECK(application.m_outdoorGameView.cameraPitchRadians() == doctest::Approx(expectedPitch));
        CHECK(partyRuntime.party().gold() == 54);
        CHECK(application.m_gameSession.partyState()->gold() == 54);
        CHECK(application.m_gameSession.namedGlobalVars().at("Fixture.AfterLoadCount") == 1);
        REQUIRE(application.m_gameSession.outdoorPartyState().has_value());
        CHECK(application.m_gameSession.outdoorPartyState()->movementState.footZ
            == expectedPose.movementState.footZ);
        CHECK(application.m_gameSession.outdoorCameraYawRadians() == doctest::Approx(expectedYaw));
        CHECK(application.m_gameSession.outdoorCameraPitchRadians() == doctest::Approx(expectedPitch));

        REQUIRE(application.executeCurrentMapAfterLoadEvents());
        CHECK(state.namedGlobalVars.at("Fixture.AfterLoadCount") == 1);
        CHECK(partyRuntime.party().gold() == 54); // Scene-owned AfterLoad receipt still prevents a second reward.
        application.m_gameplayController.clearRuntime();
        application.m_gameSession.bindDataRepository(nullptr);
    }
};
}

TEST_CASE("saved outdoor pose and camera reach real AfterLoad before synchronization and rewards survive final apply")
{
    OpenYAMM::Game::GameApplicationTestAccess::checkOutdoorAfterLoadOrdering(true);
}

TEST_CASE("ordinary outdoor AfterLoad preserves the new runtime pose instead of applying stale saved coordinates")
{
    OpenYAMM::Game::GameApplicationTestAccess::checkOutdoorAfterLoadOrdering(false);
}
