#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "engine/AssetScaleTier.h"
#include "engine/TextTable.h"
#include "game/events/GameTime.h"
#include "game/gameplay/GameplayBolsterRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/maps/MapAssetLoader.h"
#include "game/outdoor/OutdoorWorldRuntime.h"
#include "game/tables/ObjectTable.h"
#include "tests/RegressionGameData.h"

#include <cstdlib>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace OpenYAMM::Game;

namespace OpenYAMM::Game
{
struct BreachAlvarWorldActorTestAccess
{
    static auto prepare(OutdoorWorldRuntime &world, uint32_t monsterId,
        const BreachAlvarFriendActorRules &rules, const Party &spawnBolsterContext,
        const SpriteFrameTable &frames, std::string &error)
    {
        // This seam bypasses only real-view GPU preparation. It exercises the production candidate builder;
        // it does not certify pixels, Metal upload or the authored BrAlvar geometry.
        return world.prepareBreachAlvarFriendSpawnCandidate(monsterId, rules, spawnBolsterContext, frames, error);
    }

    static void bindBolster(OutdoorWorldRuntime &world, const MergedBolsterMapTable &maps,
        const MergedBolsterMonsterTable &monsters)
    {
        world.m_map.id = 65;
        world.m_pMergedBolsterMapTable = &maps;
        world.m_pMergedBolsterMonsterTable = &monsters;
        world.setBolsterMonstersEnabled(true);
    }

    static bool publish(OutdoorWorldRuntime &world,
        OutdoorWorldRuntime::PreparedBreachAlvarFriendActorMutation &&prepared)
    {
        return world.publishPreparedBreachAlvarFriendActor(std::move(prepared));
    }

    static OutdoorWorldRuntime::MapActorState &actor(OutdoorWorldRuntime &world, size_t index = 0)
    {
        return world.m_mapActors.at(index);
    }

    static void addSolidWallAt(OutdoorWorldRuntime &world, const BreachAlvarFriendActorRules &rules)
    {
        OutdoorFaceGeometryData wall;
        wall.hasPlane = true;
        wall.normal = {1.0f, 0.0f, 0.0f};
        wall.minX = wall.maxX = float(rules.x);
        wall.minY = float(rules.y - 512);
        wall.maxY = float(rules.y + 512);
        wall.minZ = float(rules.z - 512);
        wall.maxZ = float(rules.z + 1024);
        wall.vertices = {{wall.minX, wall.minY, wall.minZ}, {wall.minX, wall.maxY, wall.minZ},
            {wall.minX, wall.maxY, wall.maxZ}, {wall.minX, wall.minY, wall.maxZ}};
        world.m_outdoorFaces.push_back(std::move(wall));
    }
};
}

namespace
{
using World = OutdoorWorldRuntime;
static_assert(noexcept(std::declval<World &>().syncSpellMovementStatesFromPartyBuffs()));
using Access = BreachAlvarWorldActorTestAccess;

BreachAlvarFriendActorRules sourceRules()
{
    BreachAlvarFriendActorRules rules;
    rules.npcId = 772;
    rules.x = -20633;
    rules.y = -14746;
    rules.z = 4864;
    rules.group = 50;
    rules.ally = 9999;
    rules.fullHealthMultiplier = 5;
    rules.restoreIntervalGameMinutes = 1;
    rules.restoreHealth = 15;
    rules.restoreClearsHostile = true;
    rules.restoreSkipsRemoved = true;
    rules.restoreDeadToStand = true;
    rules.hostility = {{0, 0}, {25, 4}, {150, 4}, {217, 4}};
    return rules;
}

std::vector<std::vector<std::string>> importedRows(
    const OpenYAMM::Engine::AssetFileSystem &assets, const std::string &name)
{
    const auto text = assets.readTextFile("engine/data_tables/" + name);
    REQUIRE_MESSAGE(text.has_value(), name);
    const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(*text);
    REQUIRE_MESSAGE(table.has_value(), name);
    std::vector<std::vector<std::string>> rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    return rows;
}

struct Fixture
{
    const OpenYAMM::Tests::RegressionGameData &data;
    OpenYAMM::Engine::AssetFileSystem assets;
    SpriteFrameTable frames;
    MonsterTable monsters;
    std::optional<OutdoorMapData> geometry = OutdoorMapData{};
    MapStatsEntry map;
    MonsterProjectileTable projectiles;
    ObjectTable objects;
    Party party;
    BreachAlvarFriendActorRules rules = sourceRules();
    World world;
    std::string error;
    static constexpr uint32_t MonsterId = 417;

    Fixture() : data(OpenYAMM::Tests::regressionGameData())
    {
        REQUIRE_MESSAGE(OpenYAMM::Tests::regressionGameDataLoaded(),
            OpenYAMM::Tests::regressionGameDataFailure());
        const char *pRuntime = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
        const std::filesystem::path source = pRuntime != nullptr && *pRuntime != '\0'
            ? std::filesystem::path(pRuntime) : std::filesystem::path(OPENYAMM_SOURCE_DIR);
        const std::filesystem::path root = pRuntime != nullptr && *pRuntime != '\0' ? source : source / "assets_dev";
        REQUIRE(assets.initialize(source, root, OpenYAMM::Engine::AssetScaleTier::X1));
        monsters = data.monsterTable; // RegressionGameData supplies relations, not definitions/stats.
        const auto monsterRows = importedRows(assets, "monster_data.txt");
        REQUIRE(monsters.loadEntriesFromRows(importedRows(assets, "monster_descriptors.txt")));
        REQUIRE(monsters.loadDisplayNamesFromRows(monsterRows));
        REQUIRE(monsters.loadStatsFromRows(monsterRows));
        // Match the production loader: ensure supplements an existing common SFT; it does not load one.
        const auto commonFrames = assets.readTextFile("Data/rendering/sprite_frame_data_common.yml");
        REQUIRE(commonFrames.has_value());
        REQUIRE_MESSAGE(frames.loadFromYaml(*commonFrames, error), error);
        REQUIRE(ensureMonsterSpriteFramesLoaded(assets, monsters, int16_t(MonsterId), frames));
        for (const std::string &name : monsters.findById(int16_t(MonsterId))->spriteNames)
        {
            if (!name.empty())
            {
                REQUIRE_MESSAGE(frames.findFrameIndexBySpriteName(name).has_value(), name);
            }
        }
        REQUIRE(monsters.findStatsById(int16_t(MonsterId)) != nullptr);
        REQUIRE(monsters.findById(int16_t(MonsterId)) != nullptr);
        geometry->heightMap.assign(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 151);
        geometry->attributeMap.assign(geometry->heightMap.size(), 0);
        map.fileName = "bralvar.odm";
        map.name = "Breach Alvar transaction flat geometry fixture";
        PartySeed seed;
        seed.members.resize(1);
        seed.members[0].characterDataId = 1;
        seed.members[0].health = seed.members[0].maxHealth = 100;
        party.seed(seed);
        initialize(world);
    }

    void initialize(World &target)
    {
        std::optional<EventRuntimeState> events = EventRuntimeState{};
        events->mapFileName = map.fileName;
        target.initialize(map, monsters, projectiles, objects, data.spellTable, data.itemTable,
            &party, nullptr, data.standardItemEnchantTable, data.specialItemEnchantTable,
            nullptr, geometry, std::nullopt, std::nullopt, events);
        target.setBolsterMonstersEnabled(false);
    }

    auto prepare()
    {
        return Access::prepare(world, MonsterId, rules, party, frames, error);
    }

    void populateAuthoredPlacement()
    {
        // Controlled actor fixture with real shared definitions/frames, not a claim about live BrAlvar population.
        auto prepared = prepare();
        REQUIRE_MESSAGE(prepared, error);
        auto snapshot = world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, error);
        REQUIRE(snapshot);
        REQUIRE(snapshot->mapActors.size() == 1);
        World::MapActorState original = snapshot->mapActors[0];
        original.breachAlvarCompanion = false;
        original.breachAlvarNextMaintenanceGameMinute = 0.0;
        original.npcId = 0;
        original.spawnedAtRuntime = false;
        original.group = 7;
        original.ally = 12;
        original.currentHp = 31;
        original.hostileToParty = true;
        original.controlMode = World::ActorControlMode::Berserk;
        original.controlRemainingSeconds = 9.0f;
        original.specialItemId = 220;
        original.recoverySeconds = 2.25f;
        original.movementState.fallDistance = 1.5f;
        snapshot->mapActors.clear();
        const std::array<uint32_t, 3> identities = {7, 11, 42};
        for (size_t index = 0; index < identities.size(); ++index)
        {
            auto actor = original;
            actor.actorId = identities[index];
            actor.displayName = "Existing actor " + std::to_string(identities[index]);
            // Exact overlap, partial overlap, then a separate existing actor.
            const int offsetX = index == 1 ? int(actor.radius) / 2 : index == 2 ? 1024 : 0;
            actor.x += offsetX;
            actor.preciseX += float(offsetX);
            actor.homeX += offsetX;
            actor.homePreciseX += float(offsetX);
            actor.movementState.x += float(offsetX);
            snapshot->mapActors.push_back(std::move(actor));
        }
        snapshot->nextActorId = 43;
        world.restoreSnapshot(*snapshot);
    }

    BreachAlvarFriendActorBinding spawn()
    {
        auto prepared = prepare();
        REQUIRE_MESSAGE(prepared.has_value(), error);
        const BreachAlvarFriendActorBinding binding = prepared->binding();
        REQUIRE(Access::publish(world, std::move(*prepared)));
        return binding;
    }
};

void checkExistingActorsPreserved(const World::Snapshot &before, const World::Snapshot &after)
{
    REQUIRE(after.mapActors.size() >= before.mapActors.size());
    for (size_t index = 0; index < before.mapActors.size(); ++index)
    {
        CAPTURE(index);
        const auto &a = after.mapActors[index];
        const auto &b = before.mapActors[index];
        CHECK(a.actorId == b.actorId);
        CHECK(a.monsterId == b.monsterId);
        CHECK(a.npcId == b.npcId);
        CHECK(a.displayName == b.displayName);
        CHECK(a.spawnedAtRuntime == b.spawnedAtRuntime);
        CHECK(a.group == b.group);
        CHECK(a.ally == b.ally);
        CHECK(a.x == b.x);
        CHECK(a.y == b.y);
        CHECK(a.z == b.z);
        CHECK(a.preciseX == b.preciseX);
        CHECK(a.preciseY == b.preciseY);
        CHECK(a.preciseZ == b.preciseZ);
        CHECK(a.homePreciseX == b.homePreciseX);
        CHECK(a.homePreciseY == b.homePreciseY);
        CHECK(a.homePreciseZ == b.homePreciseZ);
        CHECK(a.currentHp == b.currentHp);
        CHECK(a.combatState.maximumHitPoints() == b.combatState.maximumHitPoints());
        CHECK(a.radius == b.radius);
        CHECK(a.height == b.height);
        CHECK(a.aiState == b.aiState);
        CHECK(a.isDead == b.isDead);
        CHECK(a.isInvisible == b.isInvisible);
        CHECK(a.canReceiveDamage == b.canReceiveDamage);
        CHECK(a.hostileToParty == b.hostileToParty);
        CHECK(a.controlMode == b.controlMode);
        CHECK(a.controlRemainingSeconds == b.controlRemainingSeconds);
        CHECK(a.recoverySeconds == b.recoverySeconds);
        CHECK(a.proceduralDeathLoot == b.proceduralDeathLoot);
        CHECK(a.specialItemId == b.specialItemId);
        CHECK(a.spriteFrameIndex == b.spriteFrameIndex);
        CHECK(a.actionSpriteFrameIndices == b.actionSpriteFrameIndices);
        CHECK(a.movementState.x == b.movementState.x);
        CHECK(a.movementState.y == b.movementState.y);
        CHECK(a.movementState.footZ == b.movementState.footZ);
        CHECK(a.movementState.fallDistance == b.movementState.fallDistance);
        CHECK(a.breachAlvarCompanion == b.breachAlvarCompanion);
        CHECK(a.breachAlvarCompanionRemoved == b.breachAlvarCompanionRemoved);
        CHECK(a.breachAlvarNextMaintenanceGameMinute == b.breachAlvarNextMaintenanceGameMinute);
    }
}
}

TEST_CASE("Breach actor preparation uses shared real definitions and stable actor zero without publication")
{
    Fixture f;
    const World::Snapshot before = f.world.snapshot();
    CHECK(before.mapActors.empty());
    CHECK(before.nextActorId == 0);
    CHECK_FALSE(f.world.prepareBreachAlvarFriendSpawn(Fixture::MonsterId, f.rules, f.party, f.error));
    CHECK_FALSE(f.error.empty()); // Headless must not pretend to have preflighted a renderer.
    auto prepared = f.prepare();
    REQUIRE_MESSAGE(prepared, f.error);
    CHECK(prepared->binding().actorId == 0);
    CHECK(prepared->binding().spawnMonsterId == Fixture::MonsterId);
    CHECK(f.world.snapshot().mapActors.empty());
    CHECK(f.world.snapshot().nextActorId == 0);
    auto preview = f.world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, f.error);
    REQUIRE(preview);
    REQUIRE(preview->mapActors.size() == 1);
    const auto &actor = preview->mapActors[0];
    CHECK(actor.monsterId == int16_t(Fixture::MonsterId)); // No EVT +1 conversion.
    CHECK(actor.combatState.maximumHitPoints() == f.monsters.findStatsById(int16_t(Fixture::MonsterId))->hitPoints * 5);
    CHECK(actor.currentHp == actor.combatState.maximumHitPoints());
    CHECK(actor.preciseX == -20633.0f);
    CHECK(actor.preciseY == -14746.0f);
    CHECK(actor.preciseZ == 4864.0f);
    CHECK(actor.group == 50);
    CHECK(actor.ally == 9999);
    CHECK(actor.npcId == 772);
    CHECK_FALSE(actor.proceduralDeathLoot);
    CHECK(actor.specialItemId == 0);
    CHECK_FALSE(actor.hostileToParty);
    CHECK(actor.controlMode == World::ActorControlMode::None);
    CHECK(actor.controlRemainingSeconds == 0.0f);
    CHECK(actor.breachAlvarNextMaintenanceGameMinute == before.gameMinutes + 1.0);
    CHECK(World::validateBreachAlvarFriendActorBinding(*preview, prepared->binding(), false, f.error, &f.rules));
    auto moved = std::move(*prepared);
    CHECK_FALSE(f.world.isPreparedBreachAlvarFriendActorCurrent(*prepared));
    CHECK_FALSE(Access::publish(f.world, std::move(*prepared)));
    REQUIRE(Access::publish(f.world, std::move(moved)));
    CHECK_FALSE(Access::publish(f.world, std::move(moved)));
    CHECK(f.world.snapshot().nextActorId == 1);
    CHECK_FALSE(f.prepare());
}

TEST_CASE("Breach actor preparation failures leave live actors identities and clock untouched")
{
    Fixture f;
    CHECK_FALSE(Access::prepare(f.world, 0, f.rules, f.party, f.frames, f.error));
    CHECK_FALSE(Access::prepare(f.world, 32767, f.rules, f.party, f.frames, f.error));
    SpriteFrameTable absentFrames;
    CHECK_FALSE(Access::prepare(f.world, Fixture::MonsterId, f.rules, f.party, absentFrames, f.error));
    auto invalid = f.rules;
    invalid.fullHealthMultiplier = std::numeric_limits<uint32_t>::max();
    CHECK_FALSE(Access::prepare(f.world, Fixture::MonsterId, invalid, f.party, f.frames, f.error));
    invalid = f.rules;
    invalid.z = 4000; // Below this fixture's actual terrain.
    CHECK_FALSE(Access::prepare(f.world, Fixture::MonsterId, invalid, f.party, f.frames, f.error));
    CHECK(f.world.snapshot().mapActors.empty());
    CHECK(f.world.snapshot().nextActorId == 0);
    auto prepared = f.prepare();
    REQUIRE_MESSAGE(prepared, f.error);
    f.world.advanceGameMinutes(0.25);
    CHECK_FALSE(f.world.isPreparedBreachAlvarFriendActorCurrent(*prepared));
    CHECK_FALSE(f.world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, f.error));
    CHECK_FALSE(Access::publish(f.world, std::move(*prepared)));
    CHECK(f.world.snapshot().mapActors.empty());
    auto snapshot = f.world.snapshot();
    snapshot.nextActorId = std::numeric_limits<uint32_t>::max();
    f.world.restoreSnapshot(snapshot);
    CHECK_FALSE(f.prepare());
    CHECK(f.world.snapshot().nextActorId == std::numeric_limits<uint32_t>::max());
}

TEST_CASE("Breach preservation detects actor changes and does not restart maintenance or allocate a new identity")
{
    Fixture f;
    const auto binding = f.spawn();
    auto prepared = f.world.prepareBreachAlvarFriendPreservation(binding, f.rules, false, f.error);
    REQUIRE(prepared);
    Access::actor(f.world).displayName = "changed after prepare";
    CHECK_FALSE(f.world.isPreparedBreachAlvarFriendActorCurrent(*prepared));
    CHECK_FALSE(Access::publish(f.world, std::move(*prepared)));
    prepared = f.world.prepareBreachAlvarFriendPreservation(binding, f.rules, false, f.error);
    REQUIRE(prepared);
    Access::actor(f.world).movementState.fallDistance = 1.25f;
    CHECK_FALSE(Access::publish(f.world, std::move(*prepared)));
    prepared = f.world.prepareBreachAlvarFriendPreservation(binding, f.rules, false, f.error);
    REQUIRE(prepared);
    const auto before = f.world.snapshot();
    const auto preview = f.world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, f.error);
    REQUIRE(preview);
    CHECK_FALSE(prepared->removesActor());
    CHECK(preview->nextActorId == before.nextActorId);
    CHECK(preview->mapActors[0].displayName == before.mapActors[0].displayName);
    CHECK(preview->mapActors[0].breachAlvarNextMaintenanceGameMinute
        == before.mapActors[0].breachAlvarNextMaintenanceGameMinute);
    REQUIRE(Access::publish(f.world, std::move(*prepared)));
    CHECK(f.world.snapshot().mapActors.size() == 1);
    CHECK(f.world.snapshot().nextActorId == 1);
}

TEST_CASE("Breach active companion keeps ordinary hostility and control until the source minute tick")
{
    Fixture f;
    const auto binding = f.spawn();
    const int initialHp = Access::actor(f.world).currentHp;
    REQUIRE(f.world.applyPartyAttackToMapActor(0, 1, 0, 0, 4832));
    auto &actor = Access::actor(f.world);
    CHECK(actor.currentHp == initialHp - 1);
    REQUIRE(actor.hostileToParty);
    actor.currentHp = actor.combatState.maximumHitPoints() - 40;
    actor.controlMode = World::ActorControlMode::Enslaved;
    actor.controlRemainingSeconds = 90.0f;
    REQUIRE(f.world.bindBreachAlvarFriendActorRules(f.rules, f.error));
    CHECK(actor.hostileToParty); // Binding a source rule is not a combat reset.
    CHECK(actor.controlMode == World::ActorControlMode::Enslaved);
    CHECK(World::validateBreachAlvarFriendActorBinding(f.world.snapshot(), binding, false, f.error, &f.rules));
    f.world.advanceGameMinutes(0.5);
    CHECK(actor.hostileToParty);
    CHECK(actor.currentHp == actor.combatState.maximumHitPoints() - 40);
    f.world.advanceGameMinutes(0.5);
    CHECK_FALSE(actor.hostileToParty);
    CHECK(actor.currentHp == actor.combatState.maximumHitPoints() - 25);
    CHECK(actor.controlMode == World::ActorControlMode::Enslaved);
    CHECK(actor.controlRemainingSeconds == 90.0f);
    f.world.advanceGameMinutes(3.0);
    CHECK(actor.currentHp == actor.combatState.maximumHitPoints());
}

TEST_CASE("Breach maintenance revives Dead only and preserves its phase across a cold world restore")
{
    Fixture f;
    const auto binding = f.spawn();
    auto &actor = Access::actor(f.world);
    actor.currentHp = 0;
    actor.isDead = true;
    actor.aiState = World::ActorAiState::Dying;
    f.world.advanceGameMinutes(1.0);
    CHECK(actor.currentHp == 15);
    CHECK(actor.aiState == World::ActorAiState::Dying);
    CHECK(actor.isDead);
    actor.aiState = World::ActorAiState::Dead;
    actor.currentHp = 0;
    actor.isInvisible = true; // The source timer does not clear independent visibility masks.
    f.world.advanceGameMinutes(0.5);
    const auto saved = f.world.snapshot();
    World cold;
    f.initialize(cold);
    cold.restoreSnapshot(saved);
    CHECK_THROWS_AS(cold.advanceGameMinutes(0.25), std::logic_error);
    CHECK(cold.snapshot().gameMinutes == saved.gameMinutes);
    REQUIRE(cold.bindBreachAlvarFriendActorRules(f.rules, f.error));
    cold.advanceGameMinutes(0.25);
    CHECK(Access::actor(cold).currentHp == 0);
    cold.advanceGameMinutes(0.25);
    CHECK(Access::actor(cold).currentHp == 15);
    CHECK_FALSE(Access::actor(cold).isDead);
    CHECK(Access::actor(cold).isInvisible);
    CHECK(Access::actor(cold).aiState == World::ActorAiState::Standing);
    CHECK(World::validateBreachAlvarFriendActorBinding(cold.snapshot(), binding, false, f.error, &f.rules));
}

TEST_CASE("Breach return is durable removal without death rewards and cannot be undone by event visibility")
{
    Fixture f;
    const auto binding = f.spawn();
    SUBCASE("return while alive")
    {
        CHECK_FALSE(Access::actor(f.world).isDead);
    }
    SUBCASE("return from an already dead companion")
    {
        Access::actor(f.world).isDead = true;
        Access::actor(f.world).aiState = World::ActorAiState::Dead;
        Access::actor(f.world).currentHp = 0;
    }
    const auto before = f.world.snapshot();
    const auto experience = f.party.members()[0].experience;
    auto removal = f.world.prepareBreachAlvarFriendRemoval(binding, f.rules, f.error);
    REQUIRE(removal);
    CHECK(removal->removesActor());
    CHECK_FALSE(f.world.snapshot().mapActors[0].breachAlvarCompanionRemoved);
    REQUIRE(Access::publish(f.world, std::move(*removal)));
    CHECK(f.world.mapActorState(0) == nullptr);
    CHECK_FALSE(f.world.applyPartyAttackMeleeDamage(0, 1, {0, 0, 4832}));
    CHECK_FALSE(f.world.setMapActorDead(0, false, false));
    CHECK_FALSE(f.world.setMapActorPosition(0, 0, 0, 0));
    REQUIRE(f.world.eventRuntimeState() != nullptr);
    f.world.eventRuntimeState()->actorClearMasks[0] = std::numeric_limits<uint32_t>::max();
    f.world.eventRuntimeState()->actorSetMasks[0] = std::numeric_limits<uint32_t>::max();
    f.world.applyEventRuntimeState(true);
    f.world.advanceGameMinutes(3.0);
    auto after = f.world.snapshot();
    CHECK(after.nextActorId == before.nextActorId);
    CHECK(after.mapActors[0].currentHp == before.mapActors[0].currentHp);
    CHECK(after.mapActors[0].isDead == before.mapActors[0].isDead);
    CHECK(after.mapActors[0].breachAlvarNextMaintenanceGameMinute
        == before.mapActors[0].breachAlvarNextMaintenanceGameMinute);
    CHECK(f.party.members()[0].experience == experience);
    CHECK(after.worldItems.size() == before.worldItems.size());
    CHECK(f.world.drainCombatFeedbackEvents().empty());
    CHECK(World::validateBreachAlvarFriendActorBinding(after, binding, true, f.error, &f.rules));
    CHECK_FALSE(World::validateBreachAlvarFriendActorBinding(after, binding, false, f.error, &f.rules));
    CHECK_FALSE(f.world.prepareBreachAlvarFriendRemoval(binding, f.rules, f.error));
    auto preserved = f.world.prepareBreachAlvarFriendPreservation(binding, f.rules, true, f.error);
    REQUIRE(preserved);
    REQUIRE(Access::publish(f.world, std::move(*preserved)));
    World cold;
    f.initialize(cold);
    cold.restoreSnapshot(after);
    REQUIRE(cold.bindBreachAlvarFriendActorRules(f.rules, f.error));
    cold.applyMapReentryReset();
    CHECK(cold.mapActorState(0) == nullptr);
    CHECK(World::validateBreachAlvarFriendActorBinding(cold.snapshot(), binding, true, f.error, &f.rules));
}

TEST_CASE("Breach snapshot binding rejects ambiguous identities and inconsistent persistent lifecycle")
{
    Fixture f;
    const auto binding = f.spawn();
    const auto original = f.world.snapshot();
    auto valid = [&](const World::Snapshot &state)
    {
        return World::validateBreachAlvarFriendActorBinding(state, binding, false, f.error, &f.rules);
    };
    REQUIRE(valid(original));
    auto bad = original;
    bad.mapActors.push_back(bad.mapActors[0]);
    CHECK_FALSE(valid(bad));
    bad.mapActors[1].actorId = 1;
    bad.nextActorId = 2;
    bad.mapActors[1].breachAlvarCompanion = false;
    bad.mapActors[1].breachAlvarNextMaintenanceGameMinute = 0;
    CHECK_FALSE(valid(bad)); // Distinct stable actors still may not both claim NPC 772.
    bad = original;
    bad.mapActors[0].monsterId += 1;
    CHECK_FALSE(valid(bad));
    bad = original;
    bad.nextActorId = 0;
    CHECK_FALSE(valid(bad));
    bad = original;
    bad.mapActors[0].breachAlvarNextMaintenanceGameMinute = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(valid(bad));
    bad = original;
    bad.gameMinutes = MaximumGameMinutes + 1;
    CHECK_FALSE(valid(bad));
    bad = original;
    bad.gameMinutes = MaximumGameMinutes;
    bad.mapActors[0].breachAlvarNextMaintenanceGameMinute = MaximumGameMinutes + 1;
    CHECK(valid(bad));
}

TEST_CASE("Breach bolster uses only the NPC772 removal context while the live party remains intact")
{
    Fixture f;
    PartySeed seed;
    const std::array<uint32_t, 5> levels = {1, 125, 65, 65, 65};
    for (uint32_t level : levels)
    {
        Character member;
        member.characterDataId = 1;
        member.level = level;
        member.health = member.maxHealth = 100;
        seed.members.push_back(member);
    }
    f.party.seed(seed);
    const auto beforeLive = encodePartySnapshotForComparison(f.party.snapshot());
    Party sourceSpawn = f.party;
    REQUIRE(sourceSpawn.detachMembers("fixture:source-spawn", {{1, 772}}));
    REQUIRE(sourceSpawn.members().size() == 4);
    Party finalParty = f.party;
    REQUIRE(finalParty.detachMembers("fixture:final-split", {{1, 772}, {2, 773}, {3, 774}, {4, 775}}));
    REQUIRE(finalParty.members().size() == 1);
    CHECK(gameplayBolsterAveragePartyLevel(&f.party) == 65);
    CHECK(gameplayBolsterAveragePartyLevel(&sourceSpawn) == 49);
    CHECK(gameplayBolsterAveragePartyLevel(&finalParty) == 1);

    MergedBolsterMapTable maps;
    REQUIRE(maps.loadFromRows({
        {"#", "Note", "Continent", "Bolster kind", "Spells", "Summons", "Weather", "Extra", "Rarity"},
        {"65", "Controlled bolster context", "2", "LowerToEqual", "-", "-", "-", "0", "30"},
    }));
    MergedBolsterMonsterTable monsters; // No per-monster HP cap masks the context difference.
    Access::bindBolster(f.world, maps, monsters);
    MapStatsEntry map;
    map.id = 65;
    const auto *pStats = f.monsters.findStatsById(int16_t(Fixture::MonsterId));
    const auto *pMonster = f.monsters.findById(int16_t(Fixture::MonsterId));
    REQUIRE(pStats != nullptr);
    REQUIRE(pMonster != nullptr);
    const auto resolve = [&](const Party &party)
    {
        return resolveGameplayMonsterBolster({
            .pMap = &map,
            .pMonsterTable = &f.monsters,
            .pBolsterMapTable = &maps,
            .pBolsterMonsterTable = &monsters,
            .pParty = &party,
            .bolsterMonstersEnabled = true,
        }, *pStats, pMonster);
    };
    const auto expected = resolve(sourceSpawn);
    REQUIRE(expected.statsEnabled);
    REQUIRE(expected.maxHp != resolve(f.party).maxHp);
    REQUIRE(expected.maxHp != resolve(finalParty).maxHp);
    const auto sourceBefore = encodePartySnapshotForComparison(sourceSpawn.snapshot());
    auto prepared = Access::prepare(f.world, Fixture::MonsterId, f.rules, sourceSpawn, f.frames, f.error);
    REQUIRE_MESSAGE(prepared.has_value(), f.error);
    const auto preview = f.world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, f.error);
    REQUIRE(preview.has_value());
    REQUIRE(preview->mapActors.size() == 1);
    CHECK(preview->mapActors[0].combatState.maximumHitPoints() == expected.maxHp * 5);
    CHECK(preview->mapActors[0].combatState.maximumHitPoints() != resolve(f.party).maxHp * 5);
    CHECK(preview->mapActors[0].combatState.maximumHitPoints() != resolve(finalParty).maxHp * 5);
    CHECK(encodePartySnapshotForComparison(f.party.snapshot()) == beforeLive);
    CHECK(encodePartySnapshotForComparison(sourceSpawn.snapshot()) == sourceBefore);
    CHECK(f.world.snapshot().mapActors.empty());
    REQUIRE(Access::publish(f.world, std::move(*prepared)));
    CHECK(Access::actor(f.world).combatState.maximumHitPoints() == expected.maxHp * 5);
    CHECK(encodePartySnapshotForComparison(f.party.snapshot()) == beforeLive);
    CHECK(encodePartySnapshotForComparison(sourceSpawn.snapshot()) == sourceBefore);
}

TEST_CASE("Breach authored companion spawn preserves overlapping actors and appends exactly one fixed placement")
{
    Fixture f;
    f.populateAuthoredPlacement();
    const auto before = f.world.snapshot();
    REQUIRE(before.mapActors.size() == 3);
    REQUIRE(before.mapActors[0].preciseX == float(f.rules.x));
    REQUIRE(before.mapActors[0].preciseY == float(f.rules.y));
    REQUIRE(before.mapActors[0].preciseZ == float(f.rules.z));
    REQUIRE_FALSE(before.mapActors[0].isDead);
    REQUIRE(before.mapActors[0].currentHp > 0);
    const auto partyBefore = encodePartySnapshotForComparison(f.party.snapshot());

    auto prepared = f.prepare();
    REQUIRE_MESSAGE(prepared.has_value(), f.error); // The old occupancy gate deterministically rejects this.
    CHECK(prepared->binding().actorId == 43);
    const auto liveAfterPrepare = f.world.snapshot();
    CHECK(liveAfterPrepare.mapActors.size() == before.mapActors.size());
    CHECK(liveAfterPrepare.nextActorId == before.nextActorId);
    CHECK(liveAfterPrepare.gameMinutes == before.gameMinutes);
    checkExistingActorsPreserved(before, liveAfterPrepare);

    const auto preview = f.world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, f.error);
    REQUIRE_MESSAGE(preview, f.error);
    REQUIRE(preview->mapActors.size() == before.mapActors.size() + 1);
    CHECK(preview->nextActorId == before.nextActorId + 1);
    checkExistingActorsPreserved(before, *preview);
    const auto &companion = preview->mapActors.back();
    CHECK(companion.actorId == 43);
    CHECK(companion.preciseX == float(f.rules.x));
    CHECK(companion.preciseY == float(f.rules.y));
    CHECK(companion.preciseZ == float(f.rules.z));
    CHECK(companion.npcId == int16_t(f.rules.npcId));
    CHECK(World::validateBreachAlvarFriendActorBinding(*preview, prepared->binding(), false, f.error, &f.rules));

    REQUIRE(Access::publish(f.world, std::move(*prepared)));
    const auto published = f.world.snapshot();
    CHECK(published.mapActors.size() == before.mapActors.size() + 1);
    CHECK(published.nextActorId == before.nextActorId + 1);
    checkExistingActorsPreserved(before, published);
    CHECK(encodePartySnapshotForComparison(f.party.snapshot()) == partyBefore);
    CHECK(f.world.drainCombatFeedbackEvents().empty());
    CHECK_FALSE(f.prepare()); // A second NPC772 is still forbidden.
    CHECK_FALSE(Access::publish(f.world, std::move(*prepared)));
    CHECK(f.world.snapshot().mapActors.size() == published.mapActors.size());
}

TEST_CASE("Breach overlapping placement keeps geometry identity resource and stale-publication guards")
{
    Fixture f;
    f.populateAuthoredPlacement();
    SUBCASE("solid geometry still rejects")
    {
        Access::addSolidWallAt(f.world, f.rules);
        CHECK_FALSE(f.prepare());
        CHECK(f.error == "Breach Alvar spawn preparation: authored placement intersects solid map geometry");
    }
    SUBCASE("occupied NPC identity still rejects")
    {
        Access::actor(f.world).npcId = int16_t(f.rules.npcId);
        CHECK_FALSE(f.prepare());
        CHECK(f.error == "Breach Alvar spawn preparation: existing companion/NPC or conflicting actor identity");
    }
    SUBCASE("duplicate stable actor identity still rejects")
    {
        Access::actor(f.world, 1).actorId = Access::actor(f.world).actorId;
        CHECK_FALSE(f.prepare());
        CHECK(f.error == "Breach Alvar spawn preparation: existing companion/NPC or conflicting actor identity");
    }
    SUBCASE("real view is still required by the public entry")
    {
        CHECK_FALSE(f.world.prepareBreachAlvarFriendSpawn(Fixture::MonsterId, f.rules, f.party, f.error));
        CHECK(f.error == "Breach Alvar spawn requires a valid monster, Party and real view resource preparation");
    }
    SUBCASE("actor changes after preparation prevent all publication")
    {
        auto prepared = f.prepare();
        REQUIRE_MESSAGE(prepared, f.error);
        Access::actor(f.world).currentHp -= 1;
        const auto changed = f.world.snapshot();
        CHECK_FALSE(f.world.isPreparedBreachAlvarFriendActorCurrent(*prepared));
        CHECK_FALSE(f.world.snapshotWithPreparedBreachAlvarFriendActor(*prepared, f.error));
        CHECK_FALSE(Access::publish(f.world, std::move(*prepared)));
        checkExistingActorsPreserved(changed, f.world.snapshot());
    }
    CHECK(f.world.snapshot().mapActors.size() == 3);
    CHECK(f.world.snapshot().nextActorId == 43);
}
