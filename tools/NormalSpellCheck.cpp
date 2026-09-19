#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/CorpseLootRuntime.h"
#include "game/gameplay/GameplayItemService.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
}

struct NormalSpellTestAccess
{
    static void step(GameApplication &application, const OutdoorMovementInput &movement = {})
    {
        OutdoorSceneRuntime *pScene = dynamic_cast<OutdoorSceneRuntime *>(application.m_pMapSceneRuntime.get());
        require(pScene != nullptr, "No normal outdoor scene");
        pScene->advanceFrame(movement, 0.05f);
        OutdoorWorldRuntime &world = *application.m_pOutdoorWorldRuntime;
        world.updateActorAi(0.05f);
        GameplayScreenRuntime &runtime = application.m_gameSession.gameplayScreenRuntime();
        GameplayCombatController::PendingCombatEventContext combat{*runtime.party(), &world, &runtime};
        application.m_gameSession.gameplayCombatController().handleAndClearPendingCombatEvents(combat);
    }

    static void waitReady(GameApplication &application)
    {
        const Character &member = application.m_pOutdoorPartyRuntime->party().members().front();
        for (unsigned frame = 0; frame < 200 && member.recoverySecondsRemaining > 0.0f; ++frame)
        {
            step(application);
        }
        require(GameMechanics::canTakeGameplayAction(member), "Caster never recovered normally");
    }

    static void learnAndCast(GameApplication &application, const std::filesystem::path &output, bool fight)
    {
        GameplayScreenRuntime &runtime = application.m_gameSession.gameplayScreenRuntime();
        Party &party = *runtime.party();
        const Character &member = party.members().front();
        GameplaySpellService &spells = runtime.spellService();
        const SpellTable &table = application.m_gameDataLoader.getSpellTable();
        const SpellEntry *pTorch = table.findById(1);
        const SpellEntry *pBolt = table.findById(2);
        require(pTorch && pBolt && !pTorch->name.empty() && !pBolt->name.empty(),
            "Imported spell names are missing");
        require(member.knowsSpell(1) && !member.knowsSpell(2)
            && (member.knownSpellIds == std::unordered_set<uint32_t>{1}
                || member.knownSpellIds == std::unordered_set<uint32_t>{1, 12, 23}),
            "Input must be a normal fresh Fire Magic Sorcerer");
        const int startMana = member.spellPoints;
        // A headless application has no scene view adapter. Use the shared spell resolver directly;
        // the formal Metal capture separately exercises the view-backed cast entry point.
        const auto castTorch = [&]()
        {
            PartySpellCastRequest request;
            request.spellId = 1;
            return spells.resolveSpellRequest(runtime, request, pTorch->name).disposition
                == GameplaySpellService::SpellRequestDisposition::CastSucceeded;
        };
        require(!spells.tryCastSpellFromMember(runtime, 0, 2, pBolt->name)
            && member.spellPoints == startMana && member.recoverySecondsRemaining == 0.0f,
            "An unlearned spell started or spent resources");

        const auto book = std::find_if(member.inventory.begin(), member.inventory.end(),
            [](const InventoryItem &item) { return item.objectDescriptionId == 401; });
        require(book != member.inventory.end(), "No original starter Fire Bolt book");
        const size_t inventoryBefore = member.inventory.size();
        InventoryItem held;
        require(party.takeItemFromMemberInventoryCell(0, book->gridX, book->gridY, held),
            "Cannot pick up starter book");
        GameplayHeldItemController::setHeldInventoryItem(runtime.heldInventoryItem(), held);
        require(runtime.itemService().tryUseHeldItemOnPartyMember(runtime, 0, false)
            && !runtime.heldInventoryItem().active && member.knowsSpell(2)
            && member.inventory.size() + 1 == inventoryBefore && party.inventoryItemCount(401) == 0,
            "Shared inventory action did not learn Fire Bolt and consume exactly its book");

        require(castTorch()
            && party.hasPartyBuff(PartyBuffId::TorchLight) && member.spellPoints == startMana - 1
            && member.recoverySecondsRemaining > 0.0f, "Learned Torch Light did not apply its buff, cost and recovery");
        require(!castTorch()
            && member.spellPoints == startMana - 1, "Repeated casting bypassed recovery");
        waitReady(application);
        require(application.quickSaveToPath(output / "learned.oysav", "法术学习", {}), "Cannot save learned spells");
        std::cout << "Learning and buff passed: torch=" << pTorch->name << " bolt=" << pBolt->name
            << " known=1,2 book_consumed=401 mana=" << startMana << "->" << member.spellPoints << '\n';

        if (fight)
        {
            castAtHostile(application, *pBolt);
            require(application.quickSaveToPath(output / "spell-impact.oysav", "法术命中", {}),
                "Cannot save actual spell damage");
            return;
        }
        // Spend the remaining real mana with learned casts, allowing each recovery to finish.
        unsigned casts = 0;
        waitReady(application);
        while (member.spellPoints > 0 && casts < 100)
        {
            const int before = member.spellPoints;
            require(castTorch()
                && member.spellPoints == before - 1, "Normal repeated Torch Light cost changed");
            ++casts;
            waitReady(application);
        }
        require(member.spellPoints == 0 && !castTorch()
            && member.spellPoints == 0, "Empty-mana casting succeeded or changed the mana balance");
        require(application.quickSaveToPath(output / "spell-exhausted.oysav", "法力耗尽", {}),
            "Cannot save exhausted caster");
        std::cout << "Resource limits passed: further_casts=" << casts << " mana=0, unknown/recovery/empty rejected\n";
    }

    static void castAtHostile(GameApplication &application, const SpellEntry &bolt)
    {
        GameplayScreenRuntime &runtime = application.m_gameSession.gameplayScreenRuntime();
        Party &party = *runtime.party();
        OutdoorWorldRuntime &world = *application.m_pOutdoorWorldRuntime;
        const OutdoorMoveState &position = application.m_pOutdoorPartyRuntime->movementState();
        std::vector<std::pair<float, size_t>> nearest;
        for (size_t index = 0; index < world.mapActorCount(); ++index)
        {
            const OutdoorWorldRuntime::MapActorState *pActor = world.mapActorState(index);
            if (pActor && pActor->hostileToParty && !pActor->isDead)
            {
                nearest.emplace_back(std::hypot(pActor->x - position.x, pActor->y - position.y), index);
            }
        }
        std::sort(nearest.begin(), nearest.end());
        require(!nearest.empty(), "No original hostile actor");
        const size_t target = nearest.front().second;
        bool approached = false;
        for (unsigned frame = 0; frame < 3000; ++frame)
        {
            const OutdoorWorldRuntime::MapActorState &actor = *world.mapActorState(target);
            const float distance = std::hypot(actor.preciseX - position.x, actor.preciseY - position.y);
            OutdoorMovementInput movement;
            movement.yawRadians = std::atan2(actor.preciseY - position.y, actor.preciseX - position.x);
            application.m_outdoorGameView.setCameraAngles(movement.yawRadians, 0.0f);
            if (distance < 600.0f)
            {
                approached = true;
                break;
            }
            movement.forward = true;
            step(application, movement);
            require(GameMechanics::canSelectInGameplay(party.members().front()), "Caster fell while approaching");
        }
        require(approached, "Normal movement could not reach casting range");
        waitReady(application);
        GameplaySpellService &spells = runtime.spellService();
        const int beforeHp = world.mapActorState(target)->currentHp;
        const int beforeMana = party.members().front().spellPoints;
        PartySpellCastRequest initial;
        initial.spellId = 2;
        const auto selection = spells.resolveSpellRequest(runtime, initial, bolt.name);
        require(selection.disposition == GameplaySpellService::SpellRequestDisposition::NeedsTargetSelection,
            "Learned Fire Bolt did not request a target");
        spells.armPendingTargetSelection(runtime, initial, selection.castResult.targetKind, bolt.name);
        require(application.m_gameSession.gameplayScreenState().pendingSpellTarget().active
            && party.members().front().spellPoints == beforeMana, "Target selection spent mana prematurely");
        PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
        request.targetActorIndex = target;
        require(spells.validatePendingTargetSelectionRequest(runtime, request), "Valid original target was rejected");
        const auto cast = spells.resolvePendingTargetSelectionCast(runtime, request);
        require(cast.castResult.succeeded() && cast.castResult.manaCost == 2
            && party.members().front().spellPoints == beforeMana - 2
            && world.projectileCount() > 0, "Fire Bolt did not create a projectile with normal mana cost");
        spells.clearPendingTargetSelection(runtime);
        for (unsigned frame = 0; frame < 200 && world.mapActorState(target)->currentHp == beforeHp; ++frame)
        {
            step(application);
        }
        const OutdoorWorldRuntime::MapActorState &actor = *world.mapActorState(target);
        require(actor.currentHp < beforeHp, "Actual Fire Bolt projectile did not damage the original target");
        std::cout << "Fire Bolt impact passed: actor=" << actor.actorId << " name=" << actor.displayName
            << " hp=" << beforeHp << "->" << actor.currentHp << " mana=" << beforeMana << "->"
            << party.members().front().spellPoints << '\n';
    }

    static void run(const std::filesystem::path &root, const std::filesystem::path &input,
        const std::filesystem::path &output, const std::string &mode)
    {
        require(!std::filesystem::exists(output), "Spell check requires a fresh output directory");
        std::string error;
        const std::optional<GameSaveData> expected = loadGameDataFromPath(input, error);
        require(expected.has_value(), "Cannot inspect input: " + error);
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        require(setenv("OPENYAMM_DISABLE_LOADING_OVERLAY", "1", 1) == 0, "Cannot disable headless graphics");
        Engine::AssetFileSystem files;
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount runtime");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot write normal settings");
        GameApplication application(config);
        require(application.loadGameData(files) && application.quickLoadFromPath(input, false),
            "Normal application could not load the created Sorcerer");
        GameplayScreenRuntime &runtime = application.m_gameSession.gameplayScreenRuntime();
        runtime.bindSettings(&application.m_settings);
        require(runtime.party() && application.m_pOutdoorWorldRuntime, "Missing outdoor gameplay context");
        Party &party = *runtime.party();
        require(party.members().size() == 1 && party.members().front().className == "Sorcerer"
            && party.members().front().level == 1, "Expected a single normal level-one Sorcerer");
        if (mode == "reload")
        {
            const Character &member = party.members().front();
            const Character &saved = expected->party.members.front();
            require(member.knownSpellIds == saved.knownSpellIds && member.knowsSpell(1) && member.knowsSpell(2)
                && member.spellPoints == saved.spellPoints && member.health == saved.health
                && member.experience == saved.experience && party.gold() == expected->party.gold
                && party.food() == expected->party.food
                && member.quickSpellName == saved.quickSpellName && member.attackSpellName == saved.attackSpellName
                && member.inventory.size() == saved.inventory.size() && party.inventoryItemCount(401) == 0
                && party.hasPartyBuff(PartyBuffId::TorchLight),
                "Reload lost learned spells, consumed book or resources");
            const auto restored = application.m_pOutdoorWorldRuntime->snapshot();
            require(restored.mapActors.size() == expected->outdoorWorld.mapActors.size(), "Reload changed actors");
            size_t consumedCorpses = 0;
            for (size_t index = 0; index < restored.mapActors.size(); ++index)
            {
                const auto &before = expected->outdoorWorld.mapActors[index];
                const auto &after = restored.mapActors[index];
                require(after.currentHp == before.currentHp && after.actorId == before.actorId
                    && after.monsterId == before.monsterId && after.isDead == before.isDead
                    && after.isInvisible == before.isInvisible, "Reload lost saved actor damage or death state");
                if (index < expected->outdoorWorld.mapActorCorpseViews.size()
                    && isConsumedCorpseView(expected->outdoorWorld.mapActorCorpseViews[index]))
                {
                    require(index < restored.mapActorCorpseViews.size()
                        && isConsumedCorpseView(restored.mapActorCorpseViews[index])
                        && !application.m_pOutdoorWorldRuntime->openMapActorCorpseView(index),
                        "A consumed corpse became lootable again after reload");
                    ++consumedCorpses;
                }
            }
            require(application.quickSaveToPath(output / "spell-reloaded.oysav", expected->saveName, {}),
                "Cannot save reloaded spell state");
            std::cout << "Spell reload passed: map=" << expected->mapFileName << " known=1,2 mana="
                << member.spellPoints << " actors=" << restored.mapActors.size()
                << " quick=" << member.quickSpellName << " attack=" << member.attackSpellName
                << " health=" << member.health << " experience=" << member.experience
                << " gold=" << party.gold() << " consumed_corpses=" << consumedCorpses << '\n';
        }
        else
        {
            learnAndCast(application, output, mode == "fight");
        }
        application.shutdownApplication();
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        OpenYAMM::Game::require(argc == 5, "Usage: openyamm_spell_check ROOT SAVE NEW_OUTPUT learn|fight|reload");
        const std::string mode = argv[4];
        OpenYAMM::Game::require(mode == "learn" || mode == "fight" || mode == "reload", "Unknown check mode");
        OpenYAMM::Game::require(SDL_Init(SDL_INIT_EVENTS), "Cannot initialize SDL events");
        std::cout << std::unitbuf;
        OpenYAMM::Game::NormalSpellTestAccess::run(std::filesystem::absolute(argv[1]),
            std::filesystem::absolute(argv[2]), std::filesystem::absolute(argv[3]), mode);
        SDL_Quit();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
