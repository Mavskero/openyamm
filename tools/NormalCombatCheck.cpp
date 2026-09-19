#include "engine/AssetFileSystem.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/CorpseLootRuntime.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

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

void equip(Party &party, const GameDataLoader &data, uint32_t itemId, EquipmentSlot slot)
{
    const Character &member = party.members().front();
    const auto found = std::find_if(member.inventory.begin(), member.inventory.end(),
        [itemId](const InventoryItem &item) { return item.objectDescriptionId == itemId; });
    require(found != member.inventory.end(), "Missing starting item " + std::to_string(itemId));
    const ItemDefinition *pDefinition = data.getItemTable().get(itemId);
    const CharacterDollEntry *pDoll = data.getCharacterDollTable().getCharacter(member.characterDataId);
    require(pDefinition != nullptr && pDoll != nullptr, "Missing imported equipment or doll definition");
    const CharacterDollTypeEntry *pDollType = data.getCharacterDollTable().getDollType(pDoll->dollTypeId);
    require(pDollType != nullptr, "Missing imported doll type");
    const std::optional<CharacterEquipPlan> plan = GameMechanics::resolveCharacterEquipPlan(
        member, *pDefinition, &data.getItemTable(), pDollType, std::nullopt, false);
    require(plan.has_value(), "Normal character cannot equip starting item " + std::to_string(itemId));
    require(plan->targetSlot == slot, "Doll placement chose an unexpected equipment slot");
    InventoryItem item;
    require(party.takeItemFromMemberInventoryCell(0, found->gridX, found->gridY, item),
        "Cannot pick up starting item");
    std::optional<InventoryItem> replacement;
    require(party.tryEquipItemOnMember(0, plan->targetSlot, item, plan->displacedSlot,
        plan->autoStoreDisplacedItem, replacement), "Cannot equip starting item: " + party.lastStatus());
    require(!replacement.has_value(), "Empty equipment slot unexpectedly displaced an item");
    require(party.equippedItemId(0, slot) == itemId && party.inventoryItemCount(itemId) == 1,
        "Equipping lost or duplicated the starting item");
}
}

struct NormalCombatTestAccess
{
    static void fight(GameApplication &application, const std::filesystem::path &output, size_t targetIndex)
    {
        Party &party = application.m_pOutdoorPartyRuntime->party();
        OutdoorWorldRuntime &world = *application.m_pOutdoorWorldRuntime;
        OutdoorSceneRuntime *pScene = dynamic_cast<OutdoorSceneRuntime *>(application.m_pMapSceneRuntime.get());
        require(pScene != nullptr, "No outdoor scene to advance normal movement");
        GameplayScreenRuntime &runtime = application.m_gameSession.gameplayScreenRuntime();
        // The graphical view normally binds settings; this check intentionally does not initialize that view.
        runtime.bindSettings(&application.m_settings);
        const GameDataLoader &data = application.m_gameDataLoader;
        const int startingHealth = party.members().front().health;
        const uint32_t startingExperience = party.members().front().experience;
        int lastHealth = startingHealth;
        bool damaged = false;
        bool killed = false;
        unsigned attacks = 0;
        constexpr float stepSeconds = 0.05f;
        for (unsigned frame = 0; frame < 4000; ++frame)
        {
            const OutdoorWorldRuntime::MapActorState *pActor = world.mapActorState(targetIndex);
            require(pActor != nullptr, "Target disappeared from actual map");
            const OutdoorMoveState &position = application.m_pOutdoorPartyRuntime->movementState();
            const float distance = std::hypot(pActor->preciseX - position.x, pActor->preciseY - position.y);
            OutdoorMovementInput movement;
            movement.yawRadians = std::atan2(pActor->preciseY - position.y, pActor->preciseX - position.x);
            movement.forward = distance > 200.0f;
            application.m_outdoorGameView.setCameraAngles(movement.yawRadians, 0.0f);
            pScene->advanceFrame(movement, stepSeconds);
            world.updateActorAi(stepSeconds);
            GameplayCombatController::PendingCombatEventContext combat{party, &world, &runtime};
            application.m_gameSession.gameplayCombatController().handleAndClearPendingCombatEvents(combat);
            const Character &member = party.members().front();
            if (member.health != lastHealth)
            {
                std::cout << "Health: frame=" << frame << " before=" << lastHealth << " after=" << member.health
                    << " distance=" << distance << '\n';
                damaged = damaged || member.health < lastHealth;
                lastHealth = member.health;
            }
            if (frame % 100 == 0)
            {
                std::cout << "Move: frame=" << frame << " position=" << position.x << ',' << position.y
                    << ',' << position.footZ << " distance=" << distance << " actor_hp=" << pActor->currentHp
                    << " health=" << member.health << '\n';
            }
            if (!GameMechanics::canSelectInGameplay(member))
            {
                break;
            }
            if (distance <= 250.0f && GameMechanics::canTakeGameplayAction(member))
            {
                GameplayActionController::PartyAttackConfig config;
                config.pRuntime = &runtime;
                config.pWorldRuntime = &world;
                config.pParty = &party;
                config.pItemTable = &data.getItemTable();
                config.pSpellTable = &data.getSpellTable();
                config.pMonsterTable = &data.getMonsterTable();
                config.pSpecialItemEnchantTable = &data.getSpecialItemEnchantTable();
                config.directTargetActorIndex = targetIndex;
                config.partyPosition = {position.x, position.y, position.footZ};
                config.randomSeed = frame + 43;
                config.pressedThisFrame = true;
                const auto result = GameplayActionController::executePartyAttack(config);
                attacks += result.actionPerformed;
                std::cout << "Attack: frame=" << frame << " performed=" << result.actionPerformed
                    << " hit=" << result.attack.hit << " damage=" << result.attack.damage
                    << " killed=" << result.killed << " recovery=" << member.recoverySecondsRemaining << '\n';
                killed = result.killed;
                if (result.actionPerformed)
                {
                    require(member.recoverySecondsRemaining > 0.0f
                        && !GameplayActionController::executePartyAttack(config).actionPerformed,
                        "Repeated attack bypassed the normal recovery period");
                }
                if (killed)
                {
                    break;
                }
            }
        }
        require(application.quickSaveToPath(output / "combat-observed.oysav", "战斗观察", {}),
            "Cannot save observed combat state");
        require(killed, "Normal route did not defeat the target; inspect combat-observed.oysav and movement log");
        require(attacks > 0 && party.members().front().experience > startingExperience,
            "Actual attack did not grant experience for the kill");
        require(damaged, "This encounter did not exercise incoming damage");
        // A lethal impact begins the death animation. Normal looting becomes available after it completes.
        for (unsigned frame = 0; frame < 200 && !world.mapActorState(targetIndex)->isDead; ++frame)
        {
            pScene->advanceFrame({}, stepSeconds);
            world.updateActorAi(stepSeconds);
            GameplayCombatController::PendingCombatEventContext combat{party, &world, &runtime};
            application.m_gameSession.gameplayCombatController().handleAndClearPendingCombatEvents(combat);
        }
        require(world.mapActorState(targetIndex)->isDead, "Death animation never reached corpse state");
        require(GameMechanics::canSelectInGameplay(party.members().front()), "Party could not survive to loot");
        require(world.openMapActorCorpseView(targetIndex), "Killed actor did not yield a searchable corpse");
        const GameplayCorpseAutoLootResult loot = autoLootActiveCorpseView(world, party, &data.getItemTable());
        require(!loot.blockedByInventory, "Starting inventory could not hold the corpse loot");
        const int afterLootGold = party.gold();
        world.closeActiveCorpseView();
        if (world.openMapActorCorpseView(targetIndex))
        {
            const auto repeated = autoLootActiveCorpseView(world, party, &data.getItemTable());
            require(!repeated.lootedAny && party.gold() == afterLootGold, "Corpse granted loot twice");
            world.closeActiveCorpseView();
        }
        require(application.quickSaveToPath(output / "combat-looted.oysav", "初次战斗", {}),
            "Cannot save actual combat and loot");
        std::cout << "Combat passed: target=" << targetIndex << " attacks=" << attacks
            << " damaged=" << damaged << " health=" << party.members().front().health
            << " experience=" << party.members().front().experience << " gold=" << party.gold()
            << " loot_gold=" << loot.goldAmount << " loot_item=" << loot.firstItemName << '\n';
    }

    static void checkReload(GameApplication &application, const std::filesystem::path &input,
        const std::filesystem::path &output)
    {
        std::string error;
        const std::optional<GameSaveData> expected = loadGameDataFromPath(input, error);
        require(expected.has_value(), "Cannot inspect source combat save: " + error);
        Party &party = application.m_pOutdoorPartyRuntime->party();
        OutdoorWorldRuntime &world = *application.m_pOutdoorWorldRuntime;
        const Character &member = party.members().front();
        const Character &savedMember = expected->party.members.front();
        require(party.gold() == expected->party.gold && party.food() == expected->party.food
            && member.health == savedMember.health && member.experience == savedMember.experience
            && member.name == savedMember.name && member.level == savedMember.level,
            "Combat reload changed party health, progression or resources");
        require(party.equippedItemId(0, EquipmentSlot::MainHand) == 1
            && party.equippedItemId(0, EquipmentSlot::Armor) == 84, "Combat reload lost starting equipment");
        require(world.mapActorCount() == expected->outdoorWorld.mapActors.size(), "Combat reload changed actor count");
        size_t consumed = 0;
        const OutdoorWorldRuntime::Snapshot restored = world.snapshot();
        for (size_t index = 0; index < world.mapActorCount(); ++index)
        {
            const OutdoorWorldRuntime::MapActorState &before = expected->outdoorWorld.mapActors[index];
            const OutdoorWorldRuntime::MapActorState &after = restored.mapActors[index];
            require(before.actorId == after.actorId && before.monsterId == after.monsterId
                && before.currentHp == after.currentHp && before.isDead == after.isDead
                && before.isInvisible == after.isInvisible,
                "Combat reload changed actor identity, health or death state: " + std::to_string(index));
            if (index < expected->outdoorWorld.mapActorCorpseViews.size()
                && isConsumedCorpseView(expected->outdoorWorld.mapActorCorpseViews[index]))
            {
                require(index < restored.mapActorCorpseViews.size()
                    && isConsumedCorpseView(restored.mapActorCorpseViews[index])
                    && !world.openMapActorCorpseView(index), "Consumed corpse became lootable after reload");
                ++consumed;
            }
        }
        require(consumed > 0, "Reload input did not contain a consumed corpse");
        require(application.quickSaveToPath(output / "combat-reloaded.oysav", expected->saveName, {}),
            "Cannot save the restored combat state");
        std::cout << "Combat reload passed: actors=" << world.mapActorCount() << " consumed_corpses=" << consumed
            << " health=" << member.health << " experience=" << member.experience << " gold=" << party.gold() << '\n';
    }

    static void run(const std::filesystem::path &root, const std::filesystem::path &input,
        const std::filesystem::path &output, bool reload)
    {
        require(!std::filesystem::exists(output), "Combat check output must be a new directory");
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        require(setenv("OPENYAMM_DISABLE_LOADING_OVERLAY", "1", 1) == 0, "Cannot disable headless graphics");
        Engine::AssetFileSystem files;
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount imported runtime");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.activeWorldId = "mm6";
        config.assetRoot = root.string();
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        std::string error;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot write test settings: " + error);
        GameApplication application(config);
        require(application.loadGameData(files) && application.quickLoadFromPath(input, false),
            "Cannot reload the normal party through GameApplication");
        require(application.m_pOutdoorPartyRuntime && application.m_pOutdoorWorldRuntime,
            "Missing outdoor runtime");
        Party &party = application.m_pOutdoorPartyRuntime->party();
        OutdoorWorldRuntime &world = *application.m_pOutdoorWorldRuntime;
        require(party.members().size() == 1 && party.members().front().level == 1,
            "Expected the normal level-one character");
        if (reload)
        {
            checkReload(application, input, output);
            application.shutdownApplication();
            return;
        }
        const GameDataLoader &data = application.m_gameDataLoader;
        equip(party, data, 1, EquipmentSlot::MainHand);
        equip(party, data, 84, EquipmentSlot::Armor);
        const CharacterAttackProfile attack = GameMechanics::buildCharacterAttackProfile(
            party.members().front(), &data.getItemTable(), &data.getSpellTable());
        const CharacterSheetSummary sheet = GameMechanics::buildCharacterSheetSummary(party.members().front(),
            &data.getItemTable(), &data.getStandardItemEnchantTable(), &data.getSpecialItemEnchantTable());
        require(attack.canMelee && attack.meleeMinDamage == 7 && attack.meleeMaxDamage == 11
            && sheet.armorClass.actual == 5, "Imported starter weapon or armor did not affect combat statistics");
        std::cout << "Equipment: sword=1 armor=84, damage=" << attack.meleeMinDamage << '-'
            << attack.meleeMaxDamage << ", AC=" << sheet.armorClass.actual << '\n';
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
        for (size_t index = 0; index < std::min<size_t>(20, nearest.size()); ++index)
        {
            const OutdoorWorldRuntime::MapActorState &actor = *world.mapActorState(nearest[index].second);
            std::cout << "Hostile index=" << nearest[index].second << " id=" << actor.actorId
                << " monster=" << actor.monsterId << " name=" << actor.displayName << " hp=" << actor.currentHp
                << " position=" << actor.x << ',' << actor.y << ',' << actor.z
                << " distance=" << nearest[index].first << '\n';
        }
        require(application.quickSaveToPath(output / "equipped.oysav", "初始装备", {}), "Cannot save equipment");
        require(!nearest.empty(), "Imported map has no living hostiles");
        fight(application, output, nearest.front().second);
        application.shutdownApplication();
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        OpenYAMM::Game::require(argc == 4 || (argc == 5 && std::string(argv[4]) == "--reload"),
            "Usage: openyamm_combat_check RUNTIME_ROOT NORMAL_SAVE NEW_OUTPUT_DIR [--reload]");
        OpenYAMM::Game::require(SDL_Init(SDL_INIT_EVENTS), "Cannot initialize SDL events");
        std::cout << std::unitbuf;
        OpenYAMM::Game::NormalCombatTestAccess::run(std::filesystem::absolute(argv[1]),
            std::filesystem::absolute(argv[2]), std::filesystem::absolute(argv[3]), argc == 5);
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
