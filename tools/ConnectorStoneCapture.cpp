#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayItemService.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ConnectorStoneRuntime.h"
#include "game/maps/SaveGame.h"
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace OpenYAMM::Game
{
namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
InventoryItem controlledItem(uint32_t id)
{
    InventoryItem item; item.objectDescriptionId = id; item.quantity = 2;
    item.identified = false; item.broken = true; item.stolen = true;
    item.standardEnchantId = 3; item.standardEnchantPower = 31; item.specialEnchantId = 5;
    item.artifactId = 7; item.rarity = ItemRarity::Special; item.currentCharges = 2; item.maxCharges = 9;
    item.temporaryBonusRemainingSeconds = 321;
    return item;
}
auto payload(const InventoryItem &i)
{
    return std::tuple{i.objectDescriptionId,i.quantity,i.width,i.height,i.gridX,i.gridY,
        i.identified,i.broken,i.stolen,i.standardEnchantId,i.standardEnchantPower,i.specialEnchantId,
        i.artifactId,i.rarity,i.currentCharges,i.maxCharges,i.temporaryBonusRemainingSeconds};
}
void checkDeliverySave(const GameSaveData &save)
{
    require(save.heldInventoryItemActive && save.heldInventoryItem.objectDescriptionId == 624,
        "New624 held owner missing");
    require(save.party.pendingItemDeliveries.empty(), "Delivered item still pending");
    size_t oldOwners = 0;
    for (const auto &worldItem : save.outdoorWorld.worldItems)
    {
        if (!worldItem.isExpired && worldItem.item.objectDescriptionId == 2125)
        {
            ++oldOwners;
            require(worldItem.spawnedByPlayer, "Old item is not actual player world drop");
            require(payload(worldItem.item) == payload(controlledItem(2125)), "World drop lost complete old payload");
        }
        require(worldItem.isExpired || worldItem.item.objectDescriptionId != 624, "New624 duplicated in world");
    }
    for (const auto &member : save.party.members)
        for (const auto &item : member.inventory)
            require(item.objectDescriptionId != 2125 && item.objectDescriptionId != 624, "Held/drop duplicated in inventory");
    require(oldOwners == 1, "Old item must have exactly one live world owner");
}
}
struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path seed, std::filesystem::path output,
        const std::string &mode)
    {
        const bool cold = mode.starts_with("reload-");
        const bool stone = mode == "stone" || mode == "reload-stone";
        require(stone || mode == "delivery" || mode == "reload-delivery", "Unknown bounded mode");
        root = std::filesystem::canonical(root); seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Capture output must be new");
        std::filesystem::create_directories(output); output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault(); settings.startInMainMenu = true;
        settings.preseedParty = false; settings.immortal = false; settings.unlimitedMana = false;
        settings.debugConsole = false; settings.assetRoot = root.string(); settings.windowMode = WindowMode::Windowed;
        std::string error; require(saveGameSettings(output / "settings.ini",settings,error),"Settings failed");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Connector139"; config.assetRoot = root.string(); config.activeWorldId = "mm6";
        config.startupMapFileOverride = "outc2.odm"; config.windowWidth = 1210; config.windowHeight = 834;
        config.verticalSync = true; SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
        GameApplication game(config); uint64_t start = 0; unsigned frames = 0, captures = 0;
        std::string pending; bool complete = false;
        std::ofstream(output / "fixture.json") << "{\"controlled\":true,\"mode\":\"" << mode
            << "\",\"seed\":\"" << seed.string() << "\",\"cold_changes\":\"none\","
            "\"new_stone_changes\":\"Main/Met/GotStone/StartedAt3/upgrade/hidden0, held624 and low HP/SP; character inventory open\","
            "\"new_delivery_changes\":\"same quest flags, full active inventories, complete controlled held2125 plus persistent pending624\","
            "\"natural_campaign\":false,\"renderer\":\"Metal\",\"audio\":\"dummy\"}\n";
        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]() {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal,"Metal required");
                require(game.initializeRenderer(),"Renderer failed"); require(game.loadSessionFromPath(seed),"Seed load failed");
                auto *scene = game.m_pMapSceneRuntime.get(); require(scene != nullptr,"No real scene");
                auto &party = scene->party(); auto *state = scene->eventRuntimeState(); require(state != nullptr,"No scene state");
                auto &screen = game.m_gameSession.gameplayScreenRuntime(); screen.closeActiveEventDialog();
                if (!cold)
                {
                    for (const auto &[name,value] : std::vector<std::pair<std::string,int32_t>>{
                        {"GotMainQuest",1},{"MetVerdant",1},{"GotConnectorStone",1},{"StartedAt",3},
                        {"ImporvedConnector",1},{"HiddenCaster49DivineInterventionCasts",0}})
                    { state->namedGlobalVars[std::string(MergeQuestPrefix)+name] = value;
                      game.m_gameSession.setNamedGlobalVar(std::string(MergeQuestPrefix)+name,value); }
                    if (stone)
                    {
                        InventoryItem item; item.objectDescriptionId = 624;
                        GameplayHeldItemController::setHeldInventoryItem(screen.heldInventoryItem(),item);
                        party.setHeldItemForQueries(item); screen.characterScreen().open = true;
                        party.member(0)->health = 1; party.member(0)->spellPoints = 0;
                    }
                    else
                    {
                        for (size_t member = 0; member < party.memberCount(); ++member)
                        {
                            party.member(member)->inventory.clear(); InventoryItem fill; fill.objectDescriptionId = 1;
                            for (size_t n = 0; n < 500; ++n) if (!party.member(member)->addInventoryItem(fill)) break;
                            require(!party.member(member)->addInventoryItem(fill),"Fullpack fixture failed");
                        }
                        const auto old = controlledItem(2125);
                        GameplayHeldItemController::setHeldInventoryItem(screen.heldInventoryItem(),old); party.setHeldItemForQueries(old);
                        InventoryItem newItem; newItem.objectDescriptionId = 624;
                        require(party.grantOwnedQuestItem(newItem,0),"Pending owner failed");
                    }
                }
                require(game.quickSaveToPath(output / "fixture.oysav","139受控或冷加载入口"),"Fixture save failed");
                if (cold)
                {
                    const auto data = loadGameDataFromPath(output / "fixture.oysav",error); require(data.has_value(),"Cold fixture parse failed");
                    if (!stone) checkDeliverySave(*data);
                    else require(data->heldInventoryItem.objectDescriptionId == 625
                        && data->namedGlobalVars.at(HiddenCaster49CounterName) == 1,"Cold stone state changed");
                }
                start = SDL_GetTicks(); return true;
            },
            [&](const SDL_Event &event) {
                if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP && event.type != SDL_EVENT_MOUSE_MOTION
                    && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                    game.handleSdlEvent(event);
            },
            [&](int width,int height,float wheel,float delta) {
                require(SDL_GetTicks()-start < 30000,"Finite Mac guard exceeded"); ++frames;
                auto &screen = game.m_gameSession.gameplayScreenRuntime();
                if (stone && !cold && frames == 45)
                {
                    require(game.m_gameSession.gameplayItemService().tryUseHeldItemOnPartyMember(screen,0,true),"Real stone use rejected");
                    require(screen.heldInventoryItem().item.objectDescriptionId == 625,"Stone not discharged");
                    require(!screen.characterScreen().open,"Inventory not closed");
                    const auto &party = game.m_pMapSceneRuntime->party();
                    require(party.member(0)->health == party.member(0)->maxHealth && party.member(0)->spellPoints == party.member(0)->maxSpellPoints,"DI effect failed");
                }
                if (stone && !cold && frames == 90)
                    require(screen.activeEventDialog().isActive && screen.activeEventDialog().sourceId == 803,"Real NPC803 presentation missing");
                if (stone && !cold && frames == 110) screen.closeActiveEventDialog();
                game.renderFrame(width,height,wheel,delta);
                if (frames == 30 || (stone && !cold && frames == 90))
                {
                    require(pending.empty(),"Pending capture overlap");
                    pending = stone && !cold ? (frames == 30 ? "inventory-stone" : "stone-npc803")
                        : stone ? "cold-stone" : cold ? "cold-owned-drop" : "owned-drop";
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE,pending.c_str());
                }
                if (!pending.empty()) if (auto capture = Engine::BgfxContext::consumeScreenshot(pending))
                {
                    auto *surface = SDL_CreateSurfaceFrom(int(capture->width),int(capture->height),SDL_PIXELFORMAT_BGRA32,
                        capture->bgraPixels.data(),int(capture->width*4)); require(surface != nullptr,"Capture surface failed");
                    const bool saved = SDL_SaveBMP(surface,(output/(pending+".bmp")).string().c_str()); SDL_DestroySurface(surface);
                    require(saved,"Capture failed"); pending.clear(); ++captures;
                }
                if (frames >= (stone && !cold ? 130u : 70u) && captures == (stone && !cold ? 2u : 1u) && !complete)
                {
                    require(game.quickSaveToPath(output/"final.oysav","139结果"),"Final save failed");
                    auto final = loadGameDataFromPath(output/"final.oysav",error); require(final.has_value(),"Final parse failed");
                    if (!stone) checkDeliverySave(*final);
                    require(game.quickLoadFromPath(output/"final.oysav",true),"Warm reload failed");
                    require(game.quickSaveToPath(output/"warm-restored.oysav","139暖恢复"),"Warm save failed");
                    auto warm = loadGameDataFromPath(output/"warm-restored.oysav",error); require(warm.has_value(),"Warm parse failed");
                    if (!stone) checkDeliverySave(*warm);
                    complete = true; SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit);
                }
            },[&]() { game.shutdownApplication(); });
        require(app.run() == 0 && complete,"Capture incomplete");
        std::ofstream(output/"result.json") << "{\"status\":\"passed\",\"mode\":\"" << mode
            << "\",\"frames\":" << frames << ",\"captures\":" << captures << ",\"warm_reload\":true}\n";
        return 0;
    }
    static constexpr const char *MergeQuestPrefix = "MMerge.CrossContinents.";
};
}
int main(int argc,char **argv)
{
    try { if (argc != 5) throw std::runtime_error("connector_stone_capture RUNTIME READ_ONLY_SEED NEW_OUTPUT stone|delivery|reload-stone|reload-delivery");
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1],argv[2],argv[3],argv[4]); }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
