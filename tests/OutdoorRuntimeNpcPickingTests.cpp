#include "doctest/doctest.h"

#include "game/app/GameApplication.h"
#include "game/outdoor/OutdoorInteractionController.h"

#include <memory>
#include <optional>

namespace OpenYAMM::Game
{
struct GameApplicationTestAccess
{
    struct Fixture
    {
        // Geometry outlives the real world and view. This fixture supplies no renderer or fake GPU handles.
        std::optional<OutdoorMapData> geometry = OutdoorMapData{};
        GameApplication application{Engine::ApplicationConfig::createDefault()};

        Fixture(int16_t runtimeNpcId, std::optional<int16_t> previewNpcId, bool hidden, bool removed)
        {
            geometry->heightMap.assign(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 0);
            geometry->attributeMap.assign(geometry->heightMap.size(), 0);
            application.m_gameDataRepository.bind(application.m_gameDataLoader);
            application.m_gameSession.bindDataRepository(&application.m_gameDataRepository);
            const GameDataRepository &data = application.m_gameDataRepository;
            MapStatsEntry map;
            map.fileName = "runtime-npc-picking-fixture.odm";
            map.name = "Runtime NPC picking fixture";
            Party party;
            PartySeed seed;
            seed.members.resize(1);
            seed.members[0].name = "Picker";
            seed.members[0].health = seed.members[0].maxHealth = 100;
            party.seed(seed);
            application.m_gameSession.setPartyState(party);
            std::optional<EventRuntimeState> events = EventRuntimeState{};
            events->mapFileName = map.fileName;
            application.m_pOutdoorWorldRuntime = std::make_unique<OutdoorWorldRuntime>();
            auto &world = *application.m_pOutdoorWorldRuntime;
            world.initialize(map, data.monsterTable(), data.monsterProjectileTable(), data.objectTable(),
                data.spellTable(), data.itemTable(), &*application.m_gameSession.partyState(), nullptr,
                data.standardItemEnchantTable(), data.specialItemEnchantTable(), nullptr,
                geometry, std::nullopt, std::nullopt, events);
            auto snapshot = world.snapshot();
            OutdoorWorldRuntime::MapActorState actor;
            actor.actorId = 7;
            actor.monsterId = 1;
            actor.npcId = runtimeNpcId;
            actor.spawnedAtRuntime = true;
            actor.displayName = "Declared runtime actor";
            actor.currentHp = actor.maxHp = 100;
            actor.x = actor.homeX = 0;
            actor.y = actor.homeY = 512;
            actor.z = actor.homeZ = 0;
            actor.preciseX = actor.homePreciseX = 0.0f;
            actor.preciseY = actor.homePreciseY = 512.0f;
            actor.preciseZ = actor.homePreciseZ = 0.0f;
            actor.radius = 32;
            actor.height = 128;
            actor.isInvisible = hidden;
            actor.breachAlvarCompanion = removed;
            actor.breachAlvarCompanionRemoved = removed;
            snapshot.mapActors.push_back(actor);
            snapshot.nextActorId = 8;
            world.restoreSnapshot(snapshot);
            application.m_gameSession.bindActiveWorldRuntime(&world);
            auto &view = application.m_outdoorGameView;
            view.m_map = map;
            view.m_pOutdoorMapData = &*geometry;
            view.m_pOutdoorWorldRuntime = &world;
            view.m_outdoorActorPreviewBillboardSet = ActorPreviewBillboardSet{};
            if (previewNpcId)
            {
                // A declared imported identity may differ from the runtime record. Preserve the
                // existing preview mapping contract rather than applying a new live-first policy.
                ActorPreviewBillboard preview;
                preview.source = ActorPreviewSource::Companion;
                preview.runtimeActorIndex = 0;
                preview.npcId = *previewNpcId;
                preview.actorName = "Imported NPC identity";
                preview.x = actor.x;
                preview.y = actor.y;
                preview.z = actor.z;
                preview.radius = actor.radius;
                preview.height = actor.height;
                view.m_outdoorActorPreviewBillboardSet->billboards.push_back(preview);
            }
        }

        ~Fixture()
        {
            application.m_gameSession.bindActiveWorldRuntime(nullptr);
            application.m_outdoorGameView.m_pOutdoorWorldRuntime = nullptr;
            application.m_outdoorGameView.m_pOutdoorMapData = nullptr;
        }

        OutdoorGameView::InspectHit pick()
        {
            // Exercise production ray/AABB picking. Without matrices the real picker deliberately
            // uses its geometric fallback; opacity/Metal billboard candidates remain an actual-Mac check.
            return OutdoorInteractionController::inspectBModelFace(application.m_outdoorGameView, *geometry,
                {0.0f, 0.0f, 64.0f}, {0.0f, 1.0f, 0.0f}, 0.0f, 0.0f, 0, 0, nullptr, nullptr,
                OutdoorGameView::DecorationPickMode::Interaction);
        }

        void unlinkPreview(bool keepWorld)
        {
            auto &view = application.m_outdoorGameView;
            view.m_outdoorActorPreviewBillboardSet->billboards[0].runtimeActorIndex = size_t(-1);
            if (!keepWorld)
            {
                view.m_pOutdoorWorldRuntime = nullptr;
            }
        }
    };
};
}

using PickingFixture = OpenYAMM::Game::GameApplicationTestAccess::Fixture;

TEST_CASE("runtime-only outdoor NPC identity survives the production ray pick")
{
    for (int16_t npcId : {int16_t(772), int16_t(0)})
    {
        CAPTURE(npcId);
        PickingFixture fixture(npcId, std::nullopt, false, false);
        const auto hit = fixture.pick();
        REQUIRE(hit.hasHit);
        REQUIRE(hit.kind == "actor");
        CHECK(hit.runtimeActorIndex == 0);
        CHECK(hit.bModelIndex == 0);
        CHECK(hit.npcId == npcId);
        CHECK(hit.name == "Declared runtime actor");
        CHECK(hit.isFriendly);
        CHECK(hit.distance > 0.0f);
    }
}

TEST_CASE("outdoor NPC picking preserves imported preview identity including explicit zero")
{
    for (int16_t previewNpcId : {int16_t(772), int16_t(941), int16_t(0)})
    {
        CAPTURE(previewNpcId);
        PickingFixture fixture(772, previewNpcId, false, false);
        const auto hit = fixture.pick();
        REQUIRE(hit.hasHit);
        REQUIRE(hit.kind == "actor");
        CHECK(hit.runtimeActorIndex == 0);
        CHECK(hit.npcId == previewNpcId);
        CHECK(hit.name == "Imported NPC identity");
    }
}

TEST_CASE("hidden or removed runtime-only NPCs stay absent from outdoor ray picking")
{
    SUBCASE("invisible")
    {
        PickingFixture fixture(772, std::nullopt, true, false);
        CHECK_FALSE(fixture.pick().hasHit);
    }
    SUBCASE("removed")
    {
        PickingFixture fixture(772, std::nullopt, false, true);
        CHECK_FALSE(fixture.pick().hasHit);
    }
}

TEST_CASE("a linked imported NPC preview cannot resurrect its removed runtime actor")
{
    PickingFixture fixture(772, int16_t(772), false, true);
    CHECK_FALSE(fixture.pick().hasHit);
}

TEST_CASE("unbound imported NPC previews retain picking with or without a runtime world")
{
    for (bool keepWorld : {false, true})
    {
        CAPTURE(keepWorld);
        PickingFixture fixture(772, int16_t(941), false, true);
        fixture.unlinkPreview(keepWorld);
        const auto hit = fixture.pick();
        REQUIRE(hit.hasHit);
        CHECK(hit.kind == "actor");
        CHECK(hit.npcId == 941);
    }
}
