#include "doctest/doctest.h"

#include "engine/AssetFileSystem.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/BreachAlvarCompanionTransaction.h"
#include "game/maps/MapIdentity.h"
#include "game/tables/RosterTable.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>

using namespace OpenYAMM::Game;

namespace
{
using Request = EventRuntimeState::PendingBreachAlvarCompanionRequest;
using Code = BreachAlvarSessionResult::Code;

struct Content
{
    OpenYAMM::Engine::AssetFileSystem files;
    GameDataLoader loader;
    GameDataRepository repository;
    MapAssetInfo map;

    Content()
    {
        const char *pRoot = std::getenv("OPENYAMM_REGRESSION_RUNTIME_ROOT");
        REQUIRE(pRoot != nullptr);
        REQUIRE(files.initialize(pRoot, pRoot, OpenYAMM::Engine::AssetScaleTier::X1, "mmmerge"));
        const char *pInspection = std::getenv("OPENYAMM_BREACH_ALVAR_INSPECTION_ROOT");
        REQUIRE(pInspection != nullptr);
        REQUIRE(files.mountDevelopmentRoot(pInspection));
        loader.setActiveWorldId("mmmerge");
        REQUIRE(loader.loadCommonForGameplay(files));
        REQUIRE(loader.getBreachAlvarCompanionProfile());
        repository.bind(loader);
        const auto *pEntry = repository.mapStats().findByFileName("bralvar.odm");
        REQUIRE(pEntry != nullptr);
        REQUIRE(pEntry->worldId == "mmmerge");
        // Explicit resource inspection path. Never promote the scene to normal admission for this check.
        const auto loaded = MapAssetLoader().load(files, *pEntry, repository.monsterTable(),
            repository.objectTable(), MapLoadPurpose::HeadlessGameplay,
            {.allowSceneYml = false, .allowLegacyCompanion = false});
        REQUIRE(loaded);
        map = *loaded;
        REQUIRE(map.outdoorMapData);
        if (!map.eventRuntimeState)
        {
            map.eventRuntimeState.emplace();
        }
        map.eventRuntimeState->mapFileName = "bralvar.odm";
    }
};

Content &content()
{
    static Content shared;
    return shared;
}

InventoryItem heldItem()
{
    InventoryItem item;
    item.objectDescriptionId = 606;
    item.quantity = 1;
    item.width = item.height = 1;
    item.currentCharges = 2;
    item.maxCharges = 11;
    item.stolen = true;
    item.temporaryBonusRemainingSeconds = 19.5f;
    return item;
}

Party fiveMembers()
{
    PartySeed seed;
    seed.members.resize(5);
    for (size_t index = 0; index < seed.members.size(); ++index)
    {
        auto &member = seed.members[index];
        member.name = "Session friend " + std::to_string(index);
        member.characterDataId = uint32_t(index + 1);
        member.className = "Knight";
        member.health = member.maxHealth = int(100 + index);
    }
    Party party;
    party.seed(seed);
    auto snapshot = party.snapshot();
    for (size_t index = 0; index < snapshot.members.size(); ++index)
    {
        snapshot.members[index].healthRegenAccumulator = 0.25f;
        snapshot.members[index].spellRegenAccumulator = 0.75f;
        InventoryItem item = heldItem();
        item.objectDescriptionId = uint32_t(100 + index);
        snapshot.members[index].inventory.push_back(item);
        snapshot.pendingItemDeliveries.push_back({item, int32_t(index), false});
    }
    party.restoreSnapshot(snapshot);
    for (size_t target = 0; target < 5; ++target)
    {
        party.applyCharacterBuff(target, CharacterBuffId::Regeneration, 3600, 0, 71, 10,
            SkillMastery::Grandmaster, uint32_t((target + 1) % 5));
    }
    // Each buff application rebuilds all members' derived bonuses and resets their accumulators.
    // Seed elapsed progress only after the fixture has finished applying every buff.
    for (size_t target = 0; target < 5; ++target)
    {
        party.member(target)->healthRegenAccumulator = 0.25f;
        party.member(target)->spellRegenAccumulator = 0.75f;
    }
    party.setHeldItemForQueries(heldItem());
    return party;
}

// Explicit restored-world/context fixture: it never claims a successful GPU-backed initial spawn.
// Return commands below still execute the actual Session/Party/World removal and cache publication.
struct Fixture
{
    Content &resources = content();
    std::unique_ptr<OutdoorPartyRuntime> movement;
    OutdoorWorldRuntime world;
    GameSession session;
    std::string error;

    Fixture()
    {
        const auto &map = resources.map;
        movement = std::make_unique<OutdoorPartyRuntime>(OutdoorMovementDriver(*map.outdoorMapData,
            map.outdoorLandMask, map.outdoorDecorationCollisionSet, map.outdoorActorCollisionSet,
            map.outdoorSpriteObjectCollisionSet), resources.loader.getItemTable());
        movement->initialize(-20633, -14746, 4864, false);
        movement->setParty(fiveMembers());
        world.initialize(map.map, resources.loader.getMonsterTable(), resources.loader.getMonsterProjectileTable(),
            resources.loader.getObjectTable(), resources.loader.getSpellTable(), resources.loader.getItemTable(),
            &movement->party(), movement.get(), resources.loader.getStandardItemEnchantTable(),
            resources.loader.getSpecialItemEnchantTable(), &resources.loader.getChestTable(), map.outdoorMapData,
            std::nullopt, map.outdoorWeatherProfile, map.eventRuntimeState);
        world.setBolsterMonstersEnabled(false);
        session.bindDataRepository(&resources.repository);
        session.setCurrentSceneKind(SceneKind::Outdoor);
        session.setCurrentMapFileName("bralvar.odm");
        auto &held = session.gameplayUiController().heldInventoryItem();
        held.active = true;
        held.item = heldItem();
        held.grabCellOffsetX = 1;
        held.grabOffsetX = 3.25f;
        held.grabOffsetY = 9.75f;
        session.bindActiveWorldRuntime(&world);
        capture();
        REQUIRE_MESSAGE(session.bindBreachAlvarCompanionRuntime(error), error);
    }

    ~Fixture()
    {
        session.bindActiveWorldRuntime(nullptr);
        session.bindDataRepository(nullptr);
    }

    Party &party() { return movement->party(); }
    EventRuntimeState &events() { return *world.eventRuntimeState(); }

    void capture()
    {
        session.captureOutdoorRuntimeState("bralvar.odm", party(), movement->snapshot(), world.snapshot(), 0, 0);
    }

    Request splitRequest()
    {
        Request request;
        request.kind = Request::Kind::Split;
        request.sourceEventId = 65469;
        request.sourceMapName = "bralvar.odm";
        events().pendingBreachAlvarCompanionRequest = request;
        return request;
    }

    void installRestoredSplit()
    {
        const auto prepared = prepareBreachAlvarFiveMemberSplit(party(), std::nullopt,
            resources.repository.characterDollTable(), {}, 149);
        REQUIRE_MESSAGE(prepared.mutation, prepared.error);
        const auto &mutation = *prepared.mutation;
        const auto *pFriend = mutation.party().detachedMember(std::string(BreachAlvarSplitOwner), 772);
        REQUIRE(pFriend != nullptr);
        const auto &profile = *resources.repository.breachAlvarCompanionProfile();
        const auto appearance = resolveBreachAlvarCompanionAppearance(profile, pFriend->character,
            resources.repository.characterDollTable(), resources.repository.classMultiplierTable(), error);
        REQUIRE_MESSAGE(appearance, error);
        GameSaveData save = *session.buildSaveData();
        save.party = mutation.party().snapshot();
        save.namedGlobalVars["MMerge.CrossContinents.BreachSplit"] = 1;
        save.outdoorWorld.mapActors.clear();
        save.outdoorWorld.nextActorId = 1;
        OutdoorWorldRuntime::MapActorState actor;
        actor.actorId = 0;
        actor.monsterId = int16_t(appearance->monsterId);
        actor.npcId = 772;
        actor.spawnedAtRuntime = true;
        actor.x = float(profile.friendActor.x);
        actor.y = float(profile.friendActor.y);
        actor.z = float(profile.friendActor.z);
        actor.group = profile.friendActor.group;
        actor.ally = profile.friendActor.ally;
        actor.currentHp = actor.maxHp = 500;
        actor.breachAlvarCompanion = true;
        actor.breachAlvarNextMaintenanceGameMinute = save.savedGameMinutes + 1;
        save.outdoorWorld.mapActors.push_back(actor);
        save.breachAlvarCompanions = BreachAlvarCompanionState{mutation.receipt(),
            {std::string(BreachAlvarCompanionMap), 0, appearance->monsterId}};
        auto &state = *save.outdoorWorld.eventRuntimeState;
        state.namedGlobalVars = save.namedGlobalVars;
        for (const auto &npc : mutation.npcs())
        {
            state.npcHouseOverrides[npc.npcId] = npc.houseId;
            state.npcGreetingOverrides[npc.npcId] = npc.greetingId;
            for (size_t index = 0; index < npc.topicIds.size(); ++index)
            {
                state.npcTopicOverrides[npc.npcId][uint32_t(index)] = npc.topicIds[index];
            }
            state.npcNameOverrides[npc.npcId] = *npc.name;
            state.npcPictureOverrides[npc.npcId] = *npc.pictureId;
        }
        synchronizeAliases(save);
        REQUIRE_MESSAGE(validateSavedBreachAlvarCompanions(save, error, &profile.friendActor), error);
        restore(save);
    }

    void synchronizeAliases(GameSaveData &save)
    {
        for (const std::string &key : std::array<std::string, 3>{"bralvar.odm",
            std::string(BreachAlvarCompanionMap), buildCanonicalMapId("mmmerge", "bralvar.odm")})
        {
            save.outdoorWorldStates[key] = save.outdoorWorld;
        }
    }

    void restore(const GameSaveData &save)
    {
        session.restoreFromSaveData(save);
        movement->party().restoreSnapshot(save.party);
        movement->restoreSnapshot(save.outdoorParty);
        world.restoreSnapshot(save.outdoorWorld);
        session.restoreHeldInventoryItemFromSaveData(save);
        session.bindActiveWorldRuntime(&world);
        REQUIRE_MESSAGE(session.bindBreachAlvarCompanionRuntime(error), error);
    }

    Request returnRequest(uint32_t npc)
    {
        // Captured facts are explicit test inputs, not a simulated pointer click or executed Lua topic.
        Request request;
        request.kind = Request::Kind::Return;
        request.sourceEventId = 1789;
        request.executingGlobalHandler = true;
        request.sourceMapName = "bralvar.odm";
        request.npcId = request.sourceNpcId = npc;
        request.dialogueKind = DialogueContextKind::NpcTalk;
        if (npc == 772)
        {
            request.sourceActorIndex = 0;
            request.stableActorId = 0;
        }
        else
        {
            request.hostHouseId = 705 + npc - 773;
        }
        events().dialogueState.hostHouseId = request.hostHouseId;
        EventDialogContent dialog;
        dialog.isActive = true;
        dialog.sourceId = npc;
        dialog.sourceActorIndex = request.sourceActorIndex;
        EventDialogAction action;
        action.kind = EventDialogActionKind::NpcTopic;
        action.id = 1789;
        action.label = "Declared fixture Join";
        dialog.actions.push_back(action);
        session.gameplayUiController().setEventDialogContent(dialog);
        events().pendingBreachAlvarCompanionRequest = request;
        return request;
    }

    void assignHints(const std::array<uint32_t, 4> &order)
    {
        for (size_t index = 0; index < order.size(); ++index)
        {
            events().namedGlobalVars["MMerge.CrossContinents.HintByNPC." + std::to_string(order[index])] =
                int32_t(index + 1);
            events().namedGlobalVars["MMerge.CrossContinents.GotFQHint" + std::to_string(index + 1)] = 1;
        }
        events().namedGlobalVars["MMerge.CrossContinents.GotFQHints"] = 4;
    }

    void consumeOutputs()
    {
        events().pendingBreachAlvarCompanionRequest.reset();
        events().pendingCloseNpcDialogue = false;
        events().pendingDialogueContext.reset();
        session.gameplayUiController().clearEventDialog();
    }
};

void checkHeld(const GameSession &session)
{
    const auto &held = session.gameplayUiController().heldInventoryItem();
    CHECK(held.active);
    CHECK(encodeInventoryItemForComparison(held.item) == encodeInventoryItemForComparison(heldItem()));
    CHECK(held.grabCellOffsetX == 1);
    CHECK(held.grabOffsetX == 3.25f);
    CHECK(held.grabOffsetY == 9.75f);
    CHECK(session.activeWorldRuntime()->party()->heldItemIdForQueries() == 606);
}

void checkRejectedWithoutMutation(Fixture &fixture, const Request &request, Code expected = Code::Rejected)
{
    const auto before = encodePartySnapshotForComparison(fixture.party().snapshot());
    const auto world = fixture.world.snapshot();
    const auto globals = fixture.session.namedGlobalVars();
    const auto receipt = fixture.session.breachAlvarCompanions();
    const auto events = fixture.events();
    const auto result = fixture.session.processBreachAlvarCompanionRequest(request);
    CHECK_MESSAGE(result.code == expected, result.error);
    CHECK(encodePartySnapshotForComparison(fixture.party().snapshot()) == before);
    const auto afterWorld = fixture.world.snapshot();
    CHECK(afterWorld.nextActorId == world.nextActorId);
    REQUIRE(afterWorld.mapActors.size() == world.mapActors.size());
    for (size_t index = 0; index < world.mapActors.size(); ++index)
    {
        const auto &actor = afterWorld.mapActors[index];
        CHECK(actor.actorId == world.mapActors[index].actorId);
        CHECK(actor.breachAlvarCompanionRemoved == world.mapActors[index].breachAlvarCompanionRemoved);
        CHECK(actor.breachAlvarNextMaintenanceGameMinute
            == world.mapActors[index].breachAlvarNextMaintenanceGameMinute);
        // Public actor lookup intentionally hides a returned companion. Its saved tombstone
        // remains authoritative for rejection checks and must not be dereferenced as a live actor.
        CHECK((fixture.world.mapActorState(index) == nullptr) == actor.breachAlvarCompanionRemoved);
    }
    CHECK(fixture.session.namedGlobalVars() == globals);
    CHECK(fixture.session.breachAlvarCompanions() == receipt);
    CHECK(fixture.events().eventRandomState == events.eventRandomState);
    CHECK(fixture.events().npcHouseOverrides == events.npcHouseOverrides);
    CHECK(fixture.events().npcTopicOverrides == events.npcTopicOverrides);
    CHECK(fixture.events().npcNameOverrides == events.npcNameOverrides);
    CHECK(fixture.events().npcPictureOverrides == events.npcPictureOverrides);
    checkHeld(fixture.session);
}

void requireFixtureRegenerationProgress(const Party::Snapshot &snapshot)
{
    const auto check = [](const Character &member)
    {
        CAPTURE(member.memberId);
        REQUIRE(member.healthRegenAccumulator == 0.25f);
        REQUIRE(member.spellRegenAccumulator == 0.75f);
    };
    for (const Character &member : snapshot.members)
    {
        check(member);
    }
    for (const Party::DetachedPartyMember &member : snapshot.detachedMembers)
    {
        check(member.character);
    }
}
}

TEST_CASE("Breach Session requires actual bound world and preserves ordinary maps")
{
    GameSession session;
    Request request;
    request.sourceMapName = "bralvar.odm";
    request.sourceEventId = 65469;
    CHECK(session.processBreachAlvarCompanionRequest(request).code == Code::Rejected);
    CHECK_FALSE(session.isBreachAlvarCompanionAvailable(772));
    session.setCurrentMapFileName("out02.odm");
    std::string error;
    CHECK(session.bindBreachAlvarCompanionRuntime(error));
    CHECK_FALSE(session.breachAlvarCompanions());
}

TEST_CASE("Breach Session split without real GPU view cannot partially detach or spend RNG")
{
    Fixture fixture;
    const Request request = fixture.splitRequest();
    checkRejectedWithoutMutation(fixture, request);
    const auto result = fixture.session.processBreachAlvarCompanionRequest(request);
    REQUIRE_MESSAGE(result.code == Code::Rejected, result.error);
    CHECK(result.error.find("real view resource preparation") != std::string::npos);
    CHECK(fixture.party().memberCount() == 5);
    CHECK(fixture.party().detachedMembers().empty());
    CHECK_FALSE(fixture.party().hasQuestBit(1714));
}

TEST_CASE("Breach Session queue provenance and source scope cannot be bypassed")
{
    Fixture fixture;
    Request request = fixture.splitRequest();
    fixture.events().pendingBreachAlvarCompanionRequest.reset();
    checkRejectedWithoutMutation(fixture, request);
    fixture.events().pendingBreachAlvarCompanionRequest = request;
    request.sourceEventId = 1789;
    checkRejectedWithoutMutation(fixture, request);
    fixture.events().pendingBreachAlvarCompanionRequest = request;
    checkRejectedWithoutMutation(fixture, request);
    request = fixture.splitRequest();
    request.executingGlobalHandler = true;
    fixture.events().pendingBreachAlvarCompanionRequest = request;
    checkRejectedWithoutMutation(fixture, request);
}

TEST_CASE("Breach Session restored companions return through all 24 orders without identity or held loss")
{
    Fixture fixture;
    requireFixtureRegenerationProgress(fixture.party().snapshot());
    fixture.installRestoredSplit();
    const auto initial = *fixture.session.buildSaveData();
    requireFixtureRegenerationProgress(initial.party);
    std::array<uint32_t, 4> order{772, 773, 774, 775};
    do
    {
        CAPTURE(order);
        fixture.restore(initial);
        requireFixtureRegenerationProgress(fixture.party().snapshot());
        fixture.assignHints(order);
        std::vector<uint64_t> joined{initial.breachAlvarCompanions->receipt.leaderMemberId};
        const Party *pLive = &fixture.party();
        for (uint32_t npc : order)
        {
            REQUIRE(fixture.session.isBreachAlvarCompanionAvailable(npc));
            const auto request = fixture.returnRequest(npc);
            const auto result = fixture.session.processBreachAlvarCompanionRequest(request);
            REQUIRE_MESSAGE(result.code == Code::Committed, result.error);
            CHECK(&fixture.party() == pLive);
            joined.push_back(initial.breachAlvarCompanions->receipt.companions[npc - 772].memberId);
            const auto snapshot = fixture.party().snapshot();
            REQUIRE(snapshot.members.size() == joined.size());
            for (size_t index = 0; index < joined.size(); ++index)
            {
                CHECK(snapshot.members[index].memberId == joined[index]);
                CHECK(snapshot.members[index].healthRegenAccumulator == 0.25f);
                CHECK(snapshot.members[index].spellRegenAccumulator == 0.75f);
                const auto &buff = snapshot.characterBuffs[index][size_t(CharacterBuffId::Regeneration)];
                const uint64_t casterId = joined[index] == 5 ? 1 : joined[index] + 1;
                CHECK(buff.casterMemberId == casterId);
                const auto caster = std::find(joined.begin(), joined.end(), casterId);
                CHECK(buff.casterMemberIndex == (caster == joined.end() ? Party::NoActiveMember
                    : uint32_t(caster - joined.begin())));
            }
            size_t totalPending = snapshot.pendingItemDeliveries.size();
            for (const auto &detached : snapshot.detachedMembers)
            {
                totalPending += detached.pendingItemDeliveries.size();
            }
            CHECK(totalPending == 5);
            for (const auto &delivery : snapshot.pendingItemDeliveries)
            {
                REQUIRE(delivery.recipientMemberIndex >= 0);
                REQUIRE(size_t(delivery.recipientMemberIndex) < joined.size());
                CHECK(delivery.item.objectDescriptionId
                    == 99 + joined[size_t(delivery.recipientMemberIndex)]);
            }
            CHECK(snapshot.nextMemberId == initial.party.nextMemberId);
            CHECK(snapshot.gold == initial.party.gold);
            CHECK(snapshot.questBits == initial.party.questBits);
            CHECK_FALSE(fixture.session.isBreachAlvarCompanionAvailable(npc));
            CHECK(fixture.events().npcHouseOverrides.at(npc) == 0);
            CHECK(fixture.events().npcTopicOverrides.at(npc).at(0) == 0);
            CHECK_FALSE(fixture.events().npcNameOverrides.contains(npc));
            CHECK_FALSE(fixture.events().npcPictureOverrides.contains(npc));
            checkHeld(fixture.session);
            fixture.consumeOutputs();
            checkRejectedWithoutMutation(fixture, fixture.returnRequest(npc));
            fixture.consumeOutputs();
            const auto saved = fixture.session.buildSaveData();
            REQUIRE(saved);
            REQUIRE_MESSAGE(validateSavedBreachAlvarCompanions(*saved, fixture.error,
                &fixture.resources.repository.breachAlvarCompanionProfile()->friendActor), fixture.error);
            CHECK(saved->outdoorWorldStates.size() >= 3);
            for (const auto &[key, world] : saved->outdoorWorldStates)
            {
                REQUIRE(world.eventRuntimeState);
                CHECK_FALSE(world.eventRuntimeState->pendingBreachAlvarCompanionRequest);
                CHECK(world.mapActors.at(0).breachAlvarCompanionRemoved
                    == saved->breachAlvarCompanions->receipt.companions[0].returned);
            }
            fixture.restore(*saved); // Same production restore/bind contract, no fresh OnLoad or respawn.
        }
        CHECK(fixture.party().detachedMembers().empty());
    } while (std::next_permutation(order.begin(), order.end()));
}

TEST_CASE("Breach Session return needs hint and exact street or house context")
{
    Fixture fixture;
    fixture.installRestoredSplit();
    auto request = fixture.returnRequest(772);
    checkRejectedWithoutMutation(fixture, request); // Join cannot precede the source hint.
    fixture.assignHints({772, 773, 774, 775});
    request.stableActorId = 1;
    fixture.events().pendingBreachAlvarCompanionRequest = request;
    checkRejectedWithoutMutation(fixture, request);
    request = fixture.returnRequest(773);
    request.hostHouseId = 706;
    fixture.events().pendingBreachAlvarCompanionRequest = request;
    checkRejectedWithoutMutation(fixture, request);
    request = fixture.returnRequest(773);
    fixture.session.gameplayUiController().state().eventDialog.content.actions.clear();
    checkRejectedWithoutMutation(fixture, request);
}

TEST_CASE("Breach Session full active party and in-flight spell reject before removing companion")
{
    Fixture fixture;
    fixture.installRestoredSplit();
    fixture.assignHints({772, 773, 774, 775});
    auto request = fixture.returnRequest(772);
    fixture.session.gameplayScreenState().pendingSpellTarget().active = true;
    checkRejectedWithoutMutation(fixture, request, Code::Unsafe);
    fixture.session.gameplayScreenState().pendingSpellTarget().active = false;
    auto full = fixture.party().snapshot();
    const auto original = fiveMembers().snapshot();
    for (size_t index = 1; index < 5; ++index)
    {
        Character extra = original.members[index];
        extra.memberId = full.nextMemberId++;
        extra.name = "Unrelated current recruit " + std::to_string(index);
        full.members.push_back(std::move(extra));
    }
    fixture.party().restoreSnapshot(full); // Declared occupancy fixture; detached identities stay authoritative.
    fixture.party().setHeldItemForQueries(heldItem());
    checkRejectedWithoutMutation(fixture, request);
    REQUIRE(fixture.party().dismissMemberToAdventurersInn(4));
    const auto retry = fixture.session.processBreachAlvarCompanionRequest(request);
    CHECK_MESSAGE(retry.code == Code::Committed, retry.error);
    CHECK(fixture.party().memberCount() == 5);
    CHECK(fixture.party().snapshot().adventurersInnMembers.size() == 1);
    checkHeld(fixture.session);
}

TEST_CASE("Breach Session historical receipt survives QuestFinished without reopening companions")
{
    Fixture fixture;
    fixture.installRestoredSplit();
    auto finished = *fixture.session.buildSaveData();
    finished.party.questBits.erase(1714);
    finished.namedGlobalVars["MMerge.CrossContinents.QuestFinished"] = 1;
    finished.outdoorWorld.eventRuntimeState->namedGlobalVars = finished.namedGlobalVars;
    fixture.synchronizeAliases(finished);
    REQUIRE_MESSAGE(validateSavedBreachAlvarCompanions(finished, fixture.error,
        &fixture.resources.repository.breachAlvarCompanionProfile()->friendActor), fixture.error);
    fixture.restore(finished);
    for (uint32_t npc = 772; npc <= 775; ++npc)
    {
        CHECK_FALSE(fixture.session.isBreachAlvarCompanionAvailable(npc));
    }
    const auto request = fixture.splitRequest();
    checkRejectedWithoutMutation(fixture, request);
    CHECK(fixture.party().memberCount() == 1);
    CHECK(fixture.party().detachedMembers().size() == 4);
}

TEST_CASE("Breach Session rejects one through four members and repeats a real receipt without new RNG")
{
    Fixture fixture;
    const Party original = fixture.party();
    for (size_t count = 1; count <= 4; ++count)
    {
        auto reduced = original.snapshot();
        reduced.members.resize(count);
        reduced.pendingItemDeliveries.clear();
        reduced.characterBuffs = {};
        fixture.party().restoreSnapshot(reduced);
        fixture.party().setHeldItemForQueries(heldItem());
        const auto request = fixture.splitRequest();
        checkRejectedWithoutMutation(fixture, request);
        const auto result = fixture.session.processBreachAlvarCompanionRequest(request);
        CHECK(result.error.find("requires five existing active members") != std::string::npos);
    }
    fixture.party() = original;
    fixture.capture();
    fixture.installRestoredSplit();
    const auto request = fixture.splitRequest();
    checkRejectedWithoutMutation(fixture, request, Code::AlreadyApplied);
    CHECK(fixture.party().memberCount() == 1);
    CHECK(fixture.party().detachedMembers().size() == 4);
}
