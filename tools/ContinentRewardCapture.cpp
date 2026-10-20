#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/events/NamedGameTime.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayScreenRuntime.h"
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
void require(bool v,const char *message) { if(!v)throw std::runtime_error(message); }
constexpr const char *Prefix="MMerge.CrossContinents.";
InventoryItem oldItem()
{
    InventoryItem item; item.objectDescriptionId=2125;item.quantity=2;item.identified=false;item.broken=true;
    item.stolen=true;item.standardEnchantId=3;item.standardEnchantPower=31;item.specialEnchantId=5;
    item.artifactId=7;item.rarity=ItemRarity::Special;item.currentCharges=2;item.maxCharges=9;
    item.temporaryBonusRemainingSeconds=321;return item;
}
auto payload(const InventoryItem &i)
{ return std::tuple{i.objectDescriptionId,i.quantity,i.width,i.height,i.gridX,i.gridY,i.identified,i.broken,i.stolen,
    i.standardEnchantId,i.standardEnchantPower,i.specialEnchantId,i.artifactId,i.rarity,i.currentCharges,i.maxCharges,i.temporaryBonusRemainingSeconds}; }
void checkRing(const GameSaveData &save)
{
    require(save.party.pendingItemDeliveries.empty(),"Ring pending after delivery");
    require(save.heldInventoryItemActive && save.heldInventoryItem.objectDescriptionId==543
        && save.heldInventoryItem.identified,"Identified mouse543 missing");
    size_t old=0;
    for(const auto &entry:save.outdoorWorld.worldItems)
    {
        if(!entry.isExpired && entry.item.objectDescriptionId==2125)
        {++old;require(entry.spawnedByPlayer && payload(entry.item)==payload(oldItem()),"Old held fullpayload drop changed");}
        require(entry.isExpired || entry.item.objectDescriptionId!=543,"Ring duplicated in world");
    }
    for(const auto &member:save.party.members)for(const auto &entry:member.inventory)
        require(entry.objectDescriptionId!=543 && entry.objectDescriptionId!=2125,"Held/drop duplicated in inventory");
    require(old==1,"Old held must have exactly one live world owner");
    require(save.namedGlobalVars.at(std::string(Prefix)+"GotReward.3")==1,"Reward3 missing");
}
}
struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root,std::filesystem::path seed,std::filesystem::path output,std::string mode)
    {
        const bool cold=mode.starts_with("reload-"); const bool ring=mode=="ring"||mode=="reload-ring";
        require(ring||mode=="guide"||mode=="reload-guide","Unknown finite mode");
        root=std::filesystem::canonical(root);seed=std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output),"Output exists");std::filesystem::create_directories(output);
        output=std::filesystem::canonical(output);std::filesystem::current_path(output);
        GameSettings settings=GameSettings::createDefault();settings.startInMainMenu=true;settings.preseedParty=false;
        settings.immortal=false;settings.unlimitedMana=false;settings.assetRoot=root.string();settings.windowMode=WindowMode::Windowed;
        std::string error;require(saveGameSettings(output/"settings.ini",settings,error),"Settings failed");
        Engine::ApplicationConfig config=Engine::ApplicationConfig::createDefault();config.appName="OpenYAMM Rewards140";
        config.assetRoot=root.string();config.activeWorldId="mm6";config.startupMapFileOverride="outc2.odm";
        config.windowWidth=1210;config.windowHeight=834;config.verticalSync=true;SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
        GameApplication game(config);uint64_t start=0;unsigned frames=0,captures=0;std::string pending;bool complete=false;
        std::ofstream(output/"fixture.json")<<"{\"controlled\":true,\"mode\":\""<<mode<<"\",\"seed\":\""<<seed.string()
            <<"\",\"changes\":\"new ring:Q784 fullpack+held2125; guide:Q228783784+GotReward12, actual third Enter; explicit later FQCatchTime=current for bounded final timer; cold:none\",\"natural_campaign\":false,\"renderer\":\"Metal\",\"audio\":\"dummy\"}\n";
        auto talk=[&](){EventRuntimeState::PendingDialogueContext context;context.kind=DialogueContextKind::NpcTalk;
            context.sourceId=803;context.npcEnterPending=true;game.m_pMapSceneRuntime->eventRuntimeState()->pendingDialogueContext=context;};
        auto click=[&](uint32_t id){auto &screen=game.m_gameSession.gameplayScreenRuntime();
            const auto &actions=screen.activeEventDialog().actions;for(size_t i=0;i<actions.size();++i)
            if(actions[i].kind==EventDialogActionKind::NpcTopic && actions[i].id==id)
            {screen.eventDialogSelectionIndex()=i;screen.executeActiveDialogAction();return;}
            throw std::runtime_error("Missing actual topic "+std::to_string(id));};
        Engine::EngineApplication app(config,[&](Engine::AssetFileSystem &files){return game.loadGameData(files);},[&](){
            require(bgfx::getRendererType()==bgfx::RendererType::Metal,"Metal required");require(game.initializeRenderer(),"Renderer failed");
            require(game.loadSessionFromPath(seed),"Seed load failed");auto &screen=game.m_gameSession.gameplayScreenRuntime();
            screen.closeActiveEventDialog();
            if(!cold)
            {
                auto *scene=game.m_pMapSceneRuntime.get();auto *state=scene->eventRuntimeState();auto &party=scene->party();
                for(const char *name:{"GotReward.1","GotReward.2","GotReward.3","AllStoriesFinished","ShowInterlude","GotFinalQuest","GotInstructions","QuestFinished","HideVerdant","ImporvedConnector"})
                {const int32_t value=!ring && (std::string(name)=="GotReward.1"||std::string(name)=="GotReward.2");
                 state->namedGlobalVars[std::string(Prefix)+name]=value;game.m_gameSession.setNamedGlobalVar(std::string(Prefix)+name,value);}
                party.setQuestBit(228,!ring);party.setQuestBit(783,!ring);party.setQuestBit(784,true);party.setQuestBit(1713,false);
                if(ring)
                {
                    for(size_t m=0;m<party.memberCount();++m)
                    {party.member(m)->inventory.clear();InventoryItem fill;fill.objectDescriptionId=1;
                     for(size_t n=0;n<500;++n)if(!party.member(m)->addInventoryItem(fill))break;
                     require(!party.member(m)->addInventoryItem(fill),"Fullpack failed");}
                    GameplayHeldItemController::setHeldInventoryItem(screen.heldInventoryItem(),oldItem());party.setHeldItemForQueries(oldItem());
                }
                require(game.quickSaveToPath(output/"fixture.oysav","140声明奖励前置"),"Fixture save failed");
                const auto *map=game.m_gameDataLoader.getMapStats().findByFileName("outc2.odm");require(map!=nullptr,"Map absent");
                game.m_pendingDebugMapJump=GameApplication::PendingDebugMapJump{.mapId=map->id};
                require(game.processPendingDebugMapJump(),"Controlled actual commit failed");
                if(ring)
                {
                    // No re-creation of the old owner after commits: cover both view teardown/bootstraps.
                    for(const char *target : {"6d01.blv","outc2.odm"})
                    {
                        require(screen.heldInventoryItem().active && payload(screen.heldInventoryItem().item)==payload(oldItem()),"Held owner lost on first commit");
                        const auto *next=game.m_gameDataLoader.getMapStats().findByFileName(target);require(next!=nullptr,"Lifecycle map absent");
                        game.m_pendingDebugMapJump=GameApplication::PendingDebugMapJump{.mapId=next->id};
                        require(game.processPendingDebugMapJump(),"Bounded indoor/outdoor commit failed");
                        require(screen.heldInventoryItem().active && payload(screen.heldInventoryItem().item)==payload(oldItem()),"Held fullpayload lost across view types");
                        require(game.m_pMapSceneRuntime->party().heldItemIdForQueries()==2125,"New Party cursor query lost");
                    }
                }
                talk();
            }
            else
            {
                require(game.quickSaveToPath(output/"fixture.oysav","140独立冷入口"),"Cold fixture save failed");
                const auto save=loadGameDataFromPath(output/"fixture.oysav",error);require(save.has_value(),"Cold parse failed");
                if(ring)checkRing(*save);
                else require(save->party.questBits.contains(1713) && save->namedGlobalVars.at(std::string(Prefix)+"GotInstructions")==1,"Cold guide flags lost");
                require(game.m_pMapSceneRuntime->eventRuntimeState()->npcPrimaryGreetingTextOverrides.empty(),"Cold primary append retained");
                talk();
            }
            start=SDL_GetTicks();return true;
        },[&](const SDL_Event &event){if(event.type!=SDL_EVENT_KEY_DOWN && event.type!=SDL_EVENT_KEY_UP && event.type!=SDL_EVENT_MOUSE_MOTION
            && event.type!=SDL_EVENT_MOUSE_BUTTON_DOWN && event.type!=SDL_EVENT_MOUSE_BUTTON_UP && event.type!=SDL_EVENT_MOUSE_WHEEL)game.handleSdlEvent(event);},
        [&](int width,int height,float wheel,float delta){
            require(SDL_GetTicks()-start<40000,"Finite Mac guard exceeded");++frames;
            auto &screen=game.m_gameSession.gameplayScreenRuntime();
            if(!ring && !cold && frames==50)
            {
                require(game.quickSaveToPath(output/"third.oysav","140第三奖励"),"Third save failed");
                auto *state=game.m_pMapSceneRuntime->eventRuntimeState();
                require(state->namedGlobalVars.at(std::string(Prefix)+"AllStoriesFinished")==1,"Third reward not committed");
                require(!state->namedGlobalVars.contains(std::string(Prefix)+"FQCatchTime.TicksLo"),"Week initialized inline with reward");
                require(state->npcPrimaryGreetingTextOverrides.contains(803),"Primary upgrade absent");
                screen.closeActiveEventDialog();
                require(writeNamedGameTime(state->namedGlobalVars,std::string(Prefix)+"FQCatchTime",
                    int64_t(game.m_gameSession.gameMinutes()*256),error),"Controlled FQ deadline failed");
                for(const char *suffix:{"FQCatchTime.TicksHi","FQCatchTime.TicksLo"})
                    game.m_gameSession.setNamedGlobalVar(std::string(Prefix)+suffix,state->namedGlobalVars.at(std::string(Prefix)+suffix));
                require(game.quickSaveToPath(output/"final-arm.oysav","140声明一周期限fixture"),"Final fixture save failed");
                require(game.quickLoadFromPath(output/"final-arm.oysav",true),"Actual new afterload failed");
                require(game.m_pMapSceneRuntime->eventRuntimeState()->npcPrimaryGreetingTextOverrides.empty(),"Warm append persisted");
            }
            if(!ring && !cold && frames==75)game.m_pMapSceneRuntime->advanceGameMinutes(10.0f);
            if(!ring && !cold && frames==130)click(1785);
            if(!ring && !cold && frames==185)click(1786);
            if(ring && frames==55)screen.closeActiveEventDialog();
            if(!ring && frames==(cold?60u:235u))screen.closeActiveEventDialog();
            game.renderFrame(width,height,wheel,delta);
            std::string shot;
            if(frames==30)shot=cold?(ring?"cold-ring":"cold-guide"):(ring?"ring-reward":"third-upgrade");
            if(!ring && !cold && frames==110)shot="final-greeting";
            if(!ring && !cold && frames==160)shot="first-instructions";
            if(!ring && !cold && frames==215)shot="repeat-instructions";
            if(!shot.empty()){require(pending.empty(),"Capture overlap");pending=shot;bgfx::requestScreenShot(BGFX_INVALID_HANDLE,pending.c_str());}
            if(!pending.empty())if(auto capture=Engine::BgfxContext::consumeScreenshot(pending))
            {auto *surface=SDL_CreateSurfaceFrom(int(capture->width),int(capture->height),SDL_PIXELFORMAT_BGRA32,capture->bgraPixels.data(),int(capture->width*4));
             require(surface!=nullptr,"Capture surface failed");const bool saved=SDL_SaveBMP(surface,(output/(pending+".bmp")).string().c_str());SDL_DestroySurface(surface);
             require(saved,"BMP save failed");pending.clear();++captures;}
            if(frames>=(cold?85u:ring?80u:255u) && captures==(cold||ring?1u:4u) && !complete)
            {
                require(game.quickSaveToPath(output/"final.oysav","140结果"),"Final save failed");
                auto save=loadGameDataFromPath(output/"final.oysav",error);require(save.has_value(),"Final parse failed");
                if(ring)checkRing(*save);
                else require(save->party.questBits.contains(1713) && save->namedGlobalVars.at(std::string(Prefix)+"GotInstructions")==1
                    && save->namedGlobalVars.at(std::string(Prefix)+"GotFinalQuest")==1 && save->namedGlobalVars.at(std::string(Prefix)+"QuestFinished")==0,"Final flags wrong");
                require(game.quickLoadFromPath(output/"final.oysav",true),"Warm load failed");
                require(game.quickSaveToPath(output/"warm-restored.oysav","140暖恢复"),"Warm save failed");
                save=loadGameDataFromPath(output/"warm-restored.oysav",error);require(save.has_value(),"Warm parse failed");if(ring)checkRing(*save);
                complete=true;SDL_Event quit{};quit.type=SDL_EVENT_QUIT;SDL_PushEvent(&quit);
            }
        },[&](){game.shutdownApplication();});
        require(app.run()==0 && complete,"Actual incomplete");
        std::ofstream(output/"result.json")<<"{\"status\":\"passed\",\"mode\":\""<<mode<<"\",\"frames\":"<<frames<<",\"captures\":"<<captures<<",\"warm_reload\":true}\n";
        return 0;
    }
};
}
int main(int argc,char **argv)
{
    try{if(argc!=5)throw std::runtime_error("continent_reward_capture RUNTIME READONLY_SEED NEW_OUTPUT ring|guide|reload-ring|reload-guide");
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1],argv[2],argv[3],argv[4]);}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
