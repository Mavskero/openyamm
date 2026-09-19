#include "engine/AssetFileSystem.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayDialogController.h"
#include "game/gameplay/CorpseLootRuntime.h"
#include "game/scene/IndoorSceneRuntime.h"

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

bool close(float a, float b)
{
    return std::abs(a - b) < 0.001f;
}

IndoorSceneRuntime &indoor(IMapSceneRuntime *pScene)
{
    require(pScene && pScene->kind() == SceneKind::Indoor, "Expected actual indoor scene runtime");
    return *static_cast<IndoorSceneRuntime *>(pScene);
}
}

struct IndoorSessionTestAccess
{
    static void activate(GameApplication &application, uint16_t eventId)
    {
        IMapSceneRuntime &scene = *application.m_pMapSceneRuntime;
        EventRuntimeState &state = *scene.eventRuntimeState();
        const GameDataLoader &data = application.m_gameDataLoader;
        EventRuntime runtime(&data.getHouseTable(), &data.getNpcDialogTable());
        require(runtime.executeEventById(scene.localEventProgram(), scene.globalEventProgram(), eventId,
            state, &scene.party(), scene.sceneEventContext()), "Map interaction event failed");
        if (state.pendingDialogueContext && state.pendingDialogueContext->transitionMapMove)
        {
            GameplayUiController ui;
            EventDialogContent &dialog = ui.eventDialog().content;
            size_t &selection = ui.eventDialog().selectionIndex;
            GameplayDialogController::Context context{ui, state, dialog, selection};
            context.pParty = &scene.party();
            context.pWorldRuntime = application.m_gameSession.activeWorldRuntime();
            context.pHouseTable = &data.getHouseTable();
            context.pTransitionTable = &data.getTransitionTable();
            context.pCurrentMap = &data.getSelectedMap()->map;
            context.pMapEntries = &data.getMapStats().getEntries();
            GameplayDialogController controller;
            require(controller.presentPendingEventDialog(context, state.messages.size(), true, false).dialogOpened,
                "Cannot construct the actual transition confirmation");
            const auto confirm = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                [](const EventDialogAction &action)
                {
                    return action.kind == EventDialogActionKind::MapTransitionConfirm;
                });
            require(confirm != dialog.actions.end(), "Transition has no confirm action");
            selection = size_t(confirm - dialog.actions.begin());
            require(controller.executeActiveDialogAction(context).shouldCloseActiveDialog && state.pendingMapMove,
                "Transition confirmation did not queue the map move");
            state.pendingDialogueContext.reset();
        }
        require(application.processPendingMapMove(false), "Actual application map transition failed");
        application.m_gameSession.gameplayScreenRuntime().bindSettings(&application.m_settings);
    }

    static void checkRestored(GameApplication &application, const GameSaveData &expected)
    {
        IndoorSceneRuntime &scene = indoor(application.m_pMapSceneRuntime.get());
        const IndoorMoveState &position = scene.partyRuntime().movementState();
        const IndoorMoveState &saved = expected.indoorScene.partyRuntime.movementState;
        require(scene.currentMapFileName() == expected.mapFileName && close(position.x, saved.x)
            && close(position.y, saved.y) && close(position.footZ, saved.footZ)
            && position.sectorId == saved.sectorId, "Indoor reload changed the saved map, position or sector");
        require(close(application.m_indoorRenderer.cameraYawRadians(), expected.indoorCameraYawRadians)
            && close(application.m_indoorRenderer.cameraPitchRadians(), expected.indoorCameraPitchRadians),
            "Indoor reload lost saved camera orientation");
        const std::optional<MapDeltaData> &current = scene.mapDeltaData();
        require(current && expected.indoorScene.mapDeltaData
            && current->actors.size() == expected.indoorScene.mapDeltaData->actors.size(),
            "Indoor reload changed the materialized monster count");
        for (size_t index = 0; index < current->actors.size(); ++index)
        {
            const MapDeltaActor &a = current->actors[index];
            const MapDeltaActor &b = expected.indoorScene.mapDeltaData->actors[index];
            require(a.monsterInfoId == b.monsterInfoId && a.hp == b.hp
                && a.x == b.x && a.y == b.y && a.z == b.z && a.attributes == b.attributes
                && a.group == b.group, "Indoor reload changed an actor identity/HP/position/flags/group");
        }
        require(scene.party().gold() == expected.party.gold && scene.party().food() == expected.party.food
            && scene.party().members().size() == expected.party.members.size(),
            "Indoor reload changed party health or supplies");
        for (size_t index = 0; index < expected.party.members.size(); ++index)
        {
            const Character &member = scene.party().members()[index];
            const Character &savedMember = expected.party.members[index];
            require(member.name == savedMember.name && member.className == savedMember.className
                && member.health == savedMember.health && member.spellPoints == savedMember.spellPoints
                && member.conditions == savedMember.conditions && member.experience == savedMember.experience,
                "Indoor reload changed a party member's identity, resources or progression");
        }
        const auto restoredWorld = scene.worldRuntime().snapshot();
        size_t consumedCorpses = 0;
        const auto &savedCorpses = expected.indoorScene.worldRuntime.mapActorCorpseViews;
        for (size_t index = 0; index < savedCorpses.size(); ++index)
        {
            if (!isConsumedCorpseView(savedCorpses[index])) continue;
            require(index < restoredWorld.mapActorCorpseViews.size()
                && isConsumedCorpseView(restoredWorld.mapActorCorpseViews[index])
                && !scene.worldRuntime().openMapActorCorpseView(index),
                "Indoor reload regenerated a consumed corpse");
            ++consumedCorpses;
        }
        std::cout << "Indoor reload passed: " << expected.mapFileName << ", actors=" << current->actors.size()
            << ", members=" << expected.party.members.size() << ", consumed_corpses=" << consumedCorpses
            << ", position=" << position.x << ',' << position.y << ',' << position.footZ
            << ", camera=" << application.m_indoorRenderer.cameraYawRadians() << ','
            << application.m_indoorRenderer.cameraPitchRadians() << '\n';
    }

    static void run(const std::filesystem::path &root, const std::filesystem::path &input,
        const std::filesystem::path &output, const std::string &mapFile)
    {
        require(!std::filesystem::exists(output), "Indoor check requires a new isolated output directory");
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        require(setenv("OPENYAMM_DISABLE_LOADING_OVERLAY", "1", 1) == 0, "Cannot disable headless loading graphics");
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        std::string error;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot write isolated settings");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.activeWorldId = "mm6";
        config.assetRoot = root.string();
        Engine::AssetFileSystem files;
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount runtime");
        GameApplication application(config);
        require(application.loadGameData(files) && application.quickLoadFromPath(input, false),
            "Cannot load input save through GameApplication");
        application.m_gameSession.gameplayScreenRuntime().bindSettings(&application.m_settings);
        const std::string town = mapFile == "7d06.blv" ? "7out01.odm" : "oute3.odm";
        const bool reloading = application.m_pMapSceneRuntime->kind() == SceneKind::Indoor;
        if (reloading)
        {
            const std::optional<GameSaveData> expected = loadGameDataFromPath(input, error);
            require(expected.has_value(), "Cannot read expected indoor save");
            checkRestored(application, *expected);
        }
        else
        {
            require(application.m_gameSession.currentMapFileName() == town,
                "Input must start in the route's outdoor scene");
            const uint16_t entrance = (mapFile == "6d01.blv" || mapFile == "7d06.blv") ? 101
                : mapFile == "6d02.blv" ? 102 : 103;
            if (mapFile == "6d01.blv")
            {
                // Explicit prerequisite fixture, not a claim of earning this quest item during a playthrough.
                require(application.m_pMapSceneRuntime->party().tryGrantItem(2109, 1), "Cannot prepare entrance key");
            }
            activate(application, entrance);
            IndoorSceneRuntime &scene = indoor(application.m_pMapSceneRuntime.get());
            require(scene.currentMapFileName() == mapFile && scene.worldRuntime().mapActorCount() > 0,
                "Transition did not initialize the requested dungeon and its monster population");
            const IndoorMoveState start = scene.partyRuntime().movementState();
            std::cout << "Entered " << mapFile << ": position=" << start.x << ',' << start.y << ',' << start.footZ
                << ", sector=" << start.sectorId << ", actors=" << scene.worldRuntime().mapActorCount() << '\n';
            require(start.sectorId >= 0 && start.supportFaceIndex != size_t(-1), "Dungeon entry has no valid floor");
            const float yaw = application.m_indoorRenderer.cameraYawRadians();
            for (unsigned frame = 0; frame < 12; ++frame)
            {
                scene.partyRuntime().setActorColliders(scene.worldRuntime().actorMovementCollidersForPartyMovement());
                scene.partyRuntime().setDecorationColliders(scene.worldRuntime().decorationMovementColliders());
                scene.partyRuntime().setSpriteObjectColliders(scene.worldRuntime().spriteObjectMovementColliders());
                scene.partyRuntime().update(std::cos(yaw) * 200.0f, std::sin(yaw) * 200.0f,
                    false, false, 0.05f);
                scene.advanceSimulation(50.0f);
                scene.worldRuntime().updateActorAi(0.05f);
            }
            const IndoorMoveState moved = scene.partyRuntime().movementState();
            const float distance = std::hypot(moved.x - start.x, moved.y - start.y);
            std::cout << "Moved: distance=" << distance << ", position=" << moved.x << ',' << moved.y << ','
                << moved.footZ << ", sector=" << moved.sectorId << '\n';
            require(distance > 1.0f && moved.sectorId >= 0 && std::isfinite(moved.footZ),
                "Normal indoor movement did not retain a valid position");
            application.m_indoorRenderer.setCameraAngles(yaw + 0.37f, -0.23f);
        }
        const std::filesystem::path savedPath = output / "indoor.oysav";
        require(application.quickSaveToPath(savedPath, "地牢视角验证", {}), "Cannot save actual indoor runtime");
        const std::optional<GameSaveData> saved = loadGameDataFromPath(savedPath, error);
        require(saved && saved->currentSceneKind == SceneKind::Indoor, "Indoor save could not be decoded");
        require(close(saved->indoorCameraYawRadians, application.m_indoorRenderer.cameraYawRadians())
            && close(saved->indoorCameraPitchRadians, application.m_indoorRenderer.cameraPitchRadians()),
            "Serialized indoor camera differs from the current view");
        require(application.quickLoadFromPath(savedPath, false), "Cannot reload actual indoor runtime");
        checkRestored(application, *saved);
        const uint16_t exit = saved->mapFileName == "7d06.blv" ? 100 : saved->mapFileName == "6d01.blv" ? 51
            : saved->mapFileName == "6d02.blv" ? 50 : 90;
        activate(application, exit);
        require(application.m_gameSession.currentMapFileName() == town
            && application.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
            "Dungeon exit failed to restore the actual town runtime");
        require(application.quickSaveToPath(output / "returned.oysav", "地牢返回城镇", {}),
            "Cannot save after the indoor-to-outdoor transition");
        std::cout << "Actual indoor save/reload and return-to-town passed; no graphics or accessory input tested.\n";
        application.shutdownApplication();
    }
};
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM::Game;
    try
    {
        require(argc == 5, "Usage: indoor_session_check RUNTIME INPUT_SAVE NEW_OUTPUT MAP.blv");
        const std::string map = argv[4];
        require(map == "6d01.blv" || map == "6d02.blv" || map == "6d18.blv" || map == "7d06.blv",
            "Unsupported test route");
        require(SDL_Init(SDL_INIT_EVENTS), "Cannot initialize SDL events");
        std::cout << std::unitbuf;
        IndoorSessionTestAccess::run(std::filesystem::absolute(argv[1]), std::filesystem::absolute(argv[2]),
            std::filesystem::absolute(argv[3]), map);
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
