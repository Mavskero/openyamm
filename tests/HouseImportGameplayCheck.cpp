#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/GameplayDialogController.h"
#include "game/maps/SaveGame.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace OpenYAMM;
using namespace OpenYAMM::Game;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void check(const std::filesystem::path &assets, const std::filesystem::path &output)
{
    require(!std::filesystem::exists(output), "Check output must be new");
    Engine::AssetFileSystem files;
    require(files.initialize(assets, assets, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount imported game");
    GameDataLoader data;
    data.setInitialMapFileName("oute3.odm");
    require(data.loadForHeadlessGameplay(files), "Cannot load imported tables and New Sorpigal");
    const MapAssetInfo &map = *data.getSelectedMap();
    require(map.localEventProgram && map.globalEventProgram, "Missing imported event programs");

    Party party;
    party.setItemTable(&data.getItemTable());
    Character member;
    member.name = "Quest check";
    member.health = member.maxHealth = 100;
    PartySeed seed;
    seed.members.push_back(member);
    party.seed(seed);
    EventRuntimeState state;
    EventRuntime events;
    Tests::PartySpellTestWorldRuntime world;
    world.bindParty(&party);
    world.bindEventRuntimeState(&state);
    world.bindGlobalEventProgram(&map.globalEventProgram);
    world.advanceGameMinutes(720);
    GameplayUiController ui;
    GameplayDialogController controller;
    GameplayDialogController::Context context{ui, state, ui.eventDialog().content, ui.eventDialog().selectionIndex};
    context.pParty = &party;
    context.pWorldRuntime = &world;
    context.pGlobalEventProgram = &map.globalEventProgram;
    context.pHouseTable = &data.getHouseTable();
    context.pNpcDialogTable = &data.getNpcDialogTable();
    context.pClassSkillTable = &data.getClassSkillTable();
    context.pCurrentMap = &map.map;
    context.dialogueHudActive = true;
    const auto present = [&](const GameplayDialogController::Result &result)
    {
        require(result.shouldOpenPendingEventDialog, "Dialog action did not request presentation");
        require(controller.presentPendingEventDialog(context, result.previousMessageCount,
            result.allowNpcFallbackContent, false).dialogOpened, "Dialog action failed to open");
    };
    const auto choose = [&](EventDialogActionKind kind, uint32_t id)
    {
        const std::vector<EventDialogAction> &actions = ui.eventDialog().content.actions;
        const auto found = std::find_if(actions.begin(), actions.end(), [&](const EventDialogAction &action)
        {
            return action.kind == kind && action.id == id;
        });
        require(found != actions.end() && found->enabled, "Expected enabled dialog action: " + std::to_string(id));
        context.selectionIndex = static_cast<size_t>(found - actions.begin());
        present(controller.executeActiveDialogAction(context));
    };
    const auto openAndover = [&]()
    {
        ui.clearEventDialog();
        require(events.executeEventById(map.localEventProgram, map.globalEventProgram, 11, state, &party),
            "Tavern entrance event failed");
        require(controller.presentPendingEventDialog(context, 0, true, false).dialogOpened,
            "Tavern entrance dialog failed");
        require(ui.eventDialog().content.sourceId == 260, "Entrance selected a different house");
        choose(EventDialogActionKind::HouseResident, 786);
        require(ui.eventDialog().content.title == data.getNpcDialogTable().getNpc(786)->name,
            "NPC name was not preserved");
    };
    openAndover();
    choose(EventDialogActionKind::NpcTopic, 1314);
    require(party.gold() == 0 && !party.hasQuestBit(1106), "Missing letter incorrectly earned a reward");

    require(events.executeEventById(map.localEventProgram, map.globalEventProgram, 100, state, &party),
        "Starting quest event failed");
    party.applyEventRuntimeState(state);
    require(party.hasQuestBit(1105) && party.inventoryItemCount(2125) == 1, "Starting letter not granted");
    openAndover();
    choose(EventDialogActionKind::NpcTopic, 1314);
    require(party.gold() == 1000 && !party.hasQuestBit(1105) && party.hasQuestBit(1106)
        && party.inventoryItemCount(2125) == 1, "Letter delivery did not follow the original event");
    require(state.npcTopicOverrides.at(786).at(0) == 1315, "Reward did not replace the NPC's letter topic");
    choose(EventDialogActionKind::NpcTopic, 1315);
    require(party.gold() == 1000, "Repeat letter conversation granted more gold");

    std::filesystem::create_directory(output);
    GameSaveData save;
    save.mapFileName = "oute3.odm";
    save.saveName = "交信验证";
    save.party = party.snapshot();
    std::string error;
    require(saveGameDataToPath(output / "letter.oysav", save, error), "Cannot save quest state: " + error);
    const std::optional<GameSaveData> restored = loadGameDataFromPath(output / "letter.oysav", error);
    require(restored.has_value(), "Cannot reload quest state: " + error);
    party.restoreSnapshot(restored->party);
    state = {};
    party.applyGlobalNpcStateTo(state);
    openAndover();
    choose(EventDialogActionKind::NpcTopic, 1315);
    require(party.gold() == 1000 && !party.hasQuestBit(1105) && party.hasQuestBit(1106)
        && party.inventoryItemCount(2125) == 1, "Reload changed the completed delivery state");
    std::cout << "{\"passed\":true,\"house\":260,\"npc\":786,\"reward_gold\":1000,"
        "\"missing_letter_denied\":true,\"quest_cleared\":1105,\"quest_added\":1106,"
        "\"letter_retained\":true,\"repeat_reward_denied\":true,\"save_reload_passed\":true}\n";
}
}

int main(int argc, char **argv)
{
    try
    {
        require(argc == 3, "Usage: house_import_gameplay_check ASSETS NEW_CHECK_DIRECTORY");
        check(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
