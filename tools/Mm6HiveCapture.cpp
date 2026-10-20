#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayActorAiTypes.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/ui/screens/CutsceneVideoScreen.h"
#include "game/maps/SaveGame.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
void require(bool value, const std::string &message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

int32_t flag(const EventRuntimeState &state, const std::string &name)
{
    const auto found = state.namedMapVars.find(name);
    return found == state.namedMapVars.end() ? 0 : found->second;
}

void checkSuccess(const GameSaveData &save)
{
    require(save.mapFileName == "oute3.odm", "Successful ending did not arrive in source oute3");
    require(save.party.questBits.contains(784), "MM6 completion bit absent");
    require(save.party.questBits.contains(1226), "Queen quest bit absent");
    require(!save.party.questBits.contains(1222), "Destroy-the-Hive quest was not cleared");
    require(!save.party.questBits.contains(1261), "Obsolete EVT ending ran beside the Merge ending");
    require(save.namedGlobalVars.at("MMerge.CrossContinents.ContinentFinished.3") == 1,
        "Source continent completion bridge absent");
    Party party;
    party.restoreSnapshot(save.party);
    require(!party.hasItemAnywhere(2164), "Ritual scroll was not consumed");
    for (size_t index = 0; index < party.memberCount(); ++index)
    {
        require(party.hasAward(index, 78), "A party member did not receive award78");
    }
    const auto hive = save.indoorSceneStates.find("hive.blv");
    require(hive != save.indoorSceneStates.end() && hive->second.eventRuntimeState.has_value(),
        "Hive durable map state missing from outside save");
    const EventRuntimeState &state = *hive->second.eventRuntimeState;
    require(flag(state, "ReactorKilled") == 1 && flag(state, "QueenKilled") == 1,
        "Ordered Hive kill history lost");
    require(flag(state, "OpenYAMM.SuppressDeathMovie") == 0, "Death movie remains suppressed after queen");
}
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path seed,
        std::filesystem::path output, const std::string &mode)
    {
        const bool reloadSuccess = mode == "reload-success";
        const bool reloadReactor = mode == "reload-reactor-defeat";
        const bool cold = reloadSuccess || reloadReactor;
        const bool success = mode == "success";
        const bool missingScroll = mode == "missing-scroll";
        const bool earlyLeave = mode == "leave-early";
        const bool defeat = mode == "defeat" || reloadReactor;
        require(success || missingScroll || earlyLeave || defeat || reloadSuccess, "Unknown finite Hive mode");
        root = std::filesystem::canonical(root);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Hive output already exists");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);

        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.skipEventCutscenes = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save Hive capture settings");

        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM MM6 Hive controlled integration";
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = "hive.blv";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");

        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"natural_campaign\":false,\"renderer\":\"Metal\",\"audio\":\"dummy\","
            << "\"mode\":" << std::quoted(mode) << ",\"seed\":" << std::quoted(seed.string()) << ','
            << "\"changes\":\"Fresh modes: load normal seed then controlled Hive map jump; "
            << "controlled reactor-room position (4416,22100,-2256), heading90; explicit Q1222; "
            << "reactor and queen HP1 for bounded real production damage/kill callbacks; scroll2164 to last member "
            << "except missing-scroll. Gameplay simulation delta is frozen during captures. Force actor-vector "
            << "capacity to its current size before kills to exercise summons reallocation. Defeat mode explicitly "
            << "sets all members HP0/unconscious. Cold modes preserve supplied save state. After the source-facing "
            << "reactor capture, temporarily turn the camera 180 degrees for a room view, then restore it.\"}\n";

        GameApplication game(config);
        uint64_t started = 0;
        unsigned frames = 0;
        unsigned movieStartFrame = 0;
        unsigned captures = 0;
        bool complete = false;
        bool movieStarted = false;
        bool movieClosed = false;
        bool reallocObserved = false;
        float reactorCaptureYaw = 0.0f;
        size_t originalActorCount = 0;
        size_t afterReactorActorCount = 0;
        size_t reactorIndex = std::numeric_limits<size_t>::max();
        size_t queenIndex = std::numeric_limits<size_t>::max();
        std::string pendingCapture;
        std::ofstream milestones(output / "milestones.jsonl");
        std::ofstream killStates(output / "actor-kills.jsonl");

        const auto scene = [&]() -> IndoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime != nullptr && game.m_pMapSceneRuntime->kind() == SceneKind::Indoor,
                "Expected current Hive indoor scene");
            require(game.m_gameSession.currentMapFileName() == "hive.blv", "Expected Hive current map");
            return *static_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(output / (name + ".oysav"), "142受控Hive " + name),
                "Cannot save Hive milestone " + name);
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Hive screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto startMovie = [&](const std::string &expected)
        {
            EventRuntimeState *pState = game.m_pMapSceneRuntime->eventRuntimeState();
            require(pState != nullptr && pState->pendingMovie.has_value(), "Ending movie not queued");
            require(pState->pendingMovie->movieName == expected, "Wrong Hive movie queued");
            require(pState->pendingReturnToMainMenu == (expected == "mm6end2"), "Wrong ending continuation");
            require(!game.m_pendingPartyDefeatRespawnMapFileName.has_value(), "Ordinary defeat movie owns ending");
            require(game.processPendingEventMovie(), "Ending movie was not accepted by application");
            require(dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr,
                "Ending did not open a real video screen");
            movieStartFrame = frames;
            movieStarted = true;
            milestones << "{\"stage\":\"movie-started\",\"movie\":" << std::quoted(expected)
                << ",\"frame\":" << frames << "}\n";
        };
        const auto kill = [&](size_t index, uint32_t expectedId)
        {
            IndoorWorldRuntime &world = scene().worldRuntime();
            const auto record = [&](const char *phase)
            {
                const MapDeltaData *pDelta = world.mapDeltaData();
                require(pDelta != nullptr && index < pDelta->actors.size(), "Boss actor storage disappeared");
                const MapDeltaActor &savedActor = pDelta->actors[index];
                const IndoorWorldRuntime::MapActorAiState *pAi = world.mapActorAiState(index);
                GameplayRuntimeActorState current;
                require(world.actorRuntimeState(index, current), "Cannot read boss state around real damage");
                killStates << "{\"phase\":" << std::quoted(phase) << ",\"frame\":" << frames
                    << ",\"index\":" << index << ",\"monster_id\":" << current.monsterId
                    << ",\"hp\":" << savedActor.hp << ",\"reported_dead\":"
                    << (current.isDead ? "true" : "false") << ",\"ai_motion\":"
                    << (pAi != nullptr ? int(pAi->motionState) : -1) << ",\"ai_animation\":"
                    << (pAi != nullptr ? int(pAi->animationState) : -1) << ",\"dying\":"
                    << (pAi != nullptr && pAi->motionState == ActorAiMotionState::Dying ? "true" : "false")
                    << ",\"dead\":"
                    << (pAi != nullptr && pAi->motionState == ActorAiMotionState::Dead ? "true" : "false")
                    << ",\"action_seconds\":" << (pAi != nullptr ? pAi->actionSeconds : -1.0f)
                    << ",\"actors\":" << pDelta->actors.size()
                    << ",\"reactor_killed\":" << flag(*scene().eventRuntimeState(), "ReactorKilled")
                    << ",\"queen_killed\":" << flag(*scene().eventRuntimeState(), "QueenKilled") << "}\n";
                killStates.flush();
            };
            record("before-damage");
            GameplayRuntimeActorState actor;
            require(world.actorRuntimeState(index, actor) && actor.monsterId == expectedId && !actor.isDead,
                "Real source boss identity/death state changed");
            require(world.mapDeltaData()->actors[index].hp == 1,
                "Declared HP1 boss fixture changed before production damage; inspect actor-kills.jsonl");
            const GameplayWorldPoint origin = {world.partyX(), world.partyY(), world.partyFootZ()};
            const bool applied = world.applyPartyAttackMeleeDamage(index, 1000, CombatDamageType::Physical, origin);
            record("after-damage");
            require(applied, "Controlled real physical damage failed; inspect actor-kills.jsonl");
            const IndoorWorldRuntime::MapActorAiState *pAfterAi = world.mapActorAiState(index);
            // The production kill hook runs when Dying starts. isDead only marks the
            // completed corpse animation, which cannot advance while fixture time is frozen.
            require(world.actorRuntimeState(index, actor) && actor.monsterId == expectedId
                && world.mapDeltaData()->actors[index].hp == 0 && pAfterAi != nullptr
                && (pAfterAi->motionState == ActorAiMotionState::Dying
                    || pAfterAi->motionState == ActorAiMotionState::Dead),
                "Production damage did not begin source boss death; inspect actor-kills.jsonl");
        };

        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Hive capture requires Metal");
                require(game.initializeRenderer(), "Hive renderer initialization failed");
                require(game.loadSessionFromPath(seed), "Hive input save load failed");
                game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                if (!cold)
                {
                    const MapStatsEntry *pMap = game.m_gameDataLoader.getMapStats().findByFileName("hive.blv");
                    require(pMap != nullptr, "Hive map registry absent");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared Hive map jump failed");
                    EventRuntimeState::PendingMapMove fixturePosition;
                    fixturePosition.x = 4416;
                    fixturePosition.y = 22100;
                    fixturePosition.z = -2256;
                    fixturePosition.directionDegrees = 90;
                    fixturePosition.lookAngleDegrees = 0;
                    scene().eventRuntimeState()->pendingMapMove = fixturePosition;
                    require(game.processPendingMapMove(), "Declared reactor-room positioning failed");
                }
                if (reloadSuccess)
                {
                    const std::optional<GameSaveData> loaded = loadGameDataFromPath(seed, error);
                    require(loaded.has_value(), "Cannot inspect cold success input");
                    checkSuccess(*loaded);
                }
                else
                {
                    IndoorWorldRuntime &world = scene().worldRuntime();
                    MapDeltaData *pDelta = world.mapDeltaData();
                    require(pDelta != nullptr, "Hive DLV absent in actual runtime");
                    originalActorCount = pDelta->actors.size();
                    std::ofstream identities(output / "actors.jsonl");
                    for (size_t index = 0; index < world.mapActorCount(); ++index)
                    {
                        GameplayRuntimeActorState actor;
                        require(world.actorRuntimeState(index, actor), "Cannot inspect Hive runtime actor");
                        identities << "{\"index\":" << index << ",\"id\":" << actor.monsterId
                            << ",\"dead\":" << (actor.isDead ? "true" : "false")
                            << ",\"x\":" << actor.preciseX << ",\"y\":" << actor.preciseY
                            << ",\"z\":" << actor.preciseZ << "}\n";
                        if (actor.monsterId == 647) reactorIndex = index;
                        if (actor.monsterId == 646) queenIndex = index;
                    }
                    require(reactorIndex != std::numeric_limits<size_t>::max(), "Source reactor647 missing at load");
                    require(queenIndex != std::numeric_limits<size_t>::max(), "Source queen646 missing at load");
                    EventRuntimeState &state = *scene().eventRuntimeState();
                    Party &party = scene().party();
                    if (!cold)
                    {
                        require(!party.hasQuestBit(784) && !party.hasQuestBit(1226)
                            && flag(state, "ReactorKilled") == 0 && flag(state, "QueenKilled") == 0,
                            "Fresh capture requires unfinished unvisited Hive state");
                        party.setQuestBit(1222, true);
                        require(!party.hasItemAnywhere(2164), "Fresh input already contains scroll2164");
                        if (!missingScroll)
                        {
                            require(party.grantItemToMember(party.memberCount() - 1, 2164),
                                "Cannot grant declared last-member scroll fixture");
                        }
                        pDelta->actors[reactorIndex].hp = 1;
                        pDelta->actors[queenIndex].hp = 1;
                        std::vector<MapDeltaActor>(pDelta->actors).swap(pDelta->actors);
                        require(pDelta->actors.capacity() == pDelta->actors.size(),
                            "Cannot force bounded actor reallocation regression fixture");
                    }
                    else
                    {
                        require(flag(state, "ReactorKilled") == 1 && flag(state, "QueenKilled") == 0
                            && flag(state, "OpenYAMM.SuppressDeathMovie") == 1,
                            "Cold reactor state or death-video suppression lost");
                        require(!party.hasQuestBit(784) && !party.hasQuestBit(1226)
                            && party.hasItemAnywhere(2164), "Cold reactor party prerequisites changed");
                        afterReactorActorCount = originalActorCount;
                    }
                }
                save("fixture");
                started = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                    && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                {
                    game.handleSdlEvent(event);
                }
            },
            [&](int width, int height, float wheel, float delta)
            {
                require(SDL_GetTicks() - started < 60000, "Finite Hive capture exceeded 60 seconds");
                ++frames;
                if (frames == 35 && !cold)
                {
                    IndoorSceneRuntime *pBeforeScene = &scene();
                    const float beforeZ = scene().worldRuntime().partyFootZ();
                    const MapDeltaActor *pBeforeActors = scene().worldRuntime().mapDeltaData()->actors.data();
                    kill(reactorIndex, 647);
                    EventRuntimeState &state = *scene().eventRuntimeState();
                    if (missingScroll)
                    {
                        require(flag(state, "ReactorKilled") == 0 && !scene().party().hasQuestBit(784),
                            "Missing-scroll failure incorrectly progressed");
                        require(scene().worldRuntime().mapActorCount() == originalActorCount,
                            "Missing-scroll failure spawned reinforcements");
                        save("before-failure-movie");
                        startMovie("mm6end2");
                    }
                    else
                    {
                        afterReactorActorCount = scene().worldRuntime().mapActorCount();
                        require(afterReactorActorCount == originalActorCount + 16,
                            "Real reactor kill did not summon exactly sixteen actors");
                        reallocObserved = pBeforeActors != scene().worldRuntime().mapDeltaData()->actors.data();
                        require(reallocObserved, "Actor reallocation regression path was not exercised");
                        require(flag(state, "ReactorKilled") == 1 && flag(state, "QueenKilled") == 0
                            && flag(state, "OpenYAMM.SuppressDeathMovie") == 1,
                            "Reactor state did not commit through production kill hook");
                        require(state.pendingMapMove.has_value() && !state.pendingMapMove->mapName,
                            "Reactor reaction did not request same-map teleport");
                        const EventRuntimeState::PendingMapMove move = *state.pendingMapMove;
                        require(move.z == int32_t(std::lround(beforeZ)) && move.directionDegrees == 90,
                            "Reactor same-map teleport lost original Z or source direction512");
                        require(game.processPendingMapMove(), "Actual same-map teleport failed");
                        require(&scene() == pBeforeScene && scene().worldRuntime().mapActorCount() == afterReactorActorCount,
                            "Same-map teleport reloaded the scene or replayed summons");
                        const IndoorMoveState &landed = scene().partyRuntime().movementState();
                        milestones << "{\"stage\":\"reactor-landing\",\"requested_x\":" << move.x
                            << ",\"requested_y\":" << move.y << ",\"requested_z\":" << move.z
                            << ",\"x\":" << landed.x << ",\"y\":" << landed.y << ",\"z\":" << landed.footZ
                            << ",\"grounded\":" << (landed.grounded ? "true" : "false")
                            << ",\"sector\":" << landed.sectorId << ",\"support_face\":" << landed.supportFaceIndex
                            << "}\n";
                        milestones.flush();
                        save("reactor-teleport-diagnostic");
                        // The source keeps its requested Z; normal indoor teleport initialization
                        // then snaps a valid destination to its supported floor.
                        require(std::abs(scene().worldRuntime().partyX() - move.x) < 1.0f
                            && std::abs(scene().worldRuntime().partyY() - move.y) < 1.0f
                            && landed.grounded && landed.sectorId >= 0
                            && std::abs(landed.footZ - move.z) <= 256.0f,
                            "Reactor teleport did not land on a nearby supported floor");
                        save("reactor");
                        milestones << "{\"stage\":\"real-reactor-kill\",\"actors_before\":" << originalActorCount
                            << ",\"actors_after\":" << afterReactorActorCount << ",\"reallocated\":true}\n";
                    }
                }
                if (frames == 70 && !missingScroll && !reloadSuccess)
                {
                    if (success)
                    {
                        kill(queenIndex, 646);
                        require(flag(*scene().eventRuntimeState(), "QueenKilled") == 1
                            && scene().party().hasQuestBit(1226) && !scene().party().hasQuestBit(784),
                            "Queen kill must unlock exit without completing before leave");
                        save("queen");
                        size_t previousMessages = 0;
                        require(scene().worldRuntime().executeMapEvent(60, previousMessages), "Real Hive exit event failed");
                        require(scene().eventRuntimeState()->pendingMapMove.has_value(), "Hive exit did not queue move");
                        require(game.processPendingMapMove(), "Actual Hive successful map leave failed");
                        save("success-before-movie");
                        const std::optional<GameSaveData> saved = loadGameDataFromPath(output / "success-before-movie.oysav", error);
                        require(saved.has_value(), "Cannot inspect actual successful leave save");
                        checkSuccess(*saved);
                        startMovie("mm6end1");
                    }
                    else if (earlyLeave)
                    {
                        const MapStatsEntry *pMap = game.m_gameDataLoader.getMapStats().findByFileName("outa1.odm");
                        require(pMap != nullptr, "Early-leave destination absent");
                        game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                        require(game.processPendingDebugMapJump(), "Controlled actual premature leave failed");
                        require(!game.m_pMapSceneRuntime->party().hasQuestBit(784), "Premature leave granted victory");
                        save("before-failure-movie");
                        startMovie("mm6end2");
                    }
                    else if (defeat)
                    {
                        require(flag(*scene().eventRuntimeState(), "OpenYAMM.SuppressDeathMovie") == 1,
                            "Expected suppression before declared defeat fixture");
                        Party &party = scene().party();
                        for (size_t index = 0; index < party.memberCount(); ++index)
                        {
                            party.member(index)->health = 0;
                            party.member(index)->conditions.set(size_t(CharacterCondition::Unconscious));
                        }
                        save("declared-defeat");
                        require(game.processPendingPartyDefeat(), "Declared defeated party did not run production defeat");
                        require(game.m_screenManager.activeScreen() == nullptr,
                            "Suppressed defeat opened ordinary death presentation");
                        require(!game.m_pMapSceneRuntime->party().hasQuestBit(784), "Defeat granted victory");
                        startMovie("mm6end2");
                    }
                }

                if (frames == 58 && !missingScroll && !reloadSuccess)
                {
                    reactorCaptureYaw = game.m_indoorRenderer.cameraYawRadians();
                    game.m_indoorRenderer.setCameraAngles(
                        reactorCaptureYaw + 3.14159265358979323846f,
                        game.m_indoorRenderer.cameraPitchRadians());
                }
                if (frames == 67 && !missingScroll && !reloadSuccess)
                {
                    game.m_indoorRenderer.setCameraAngles(
                        reactorCaptureYaw, game.m_indoorRenderer.cameraPitchRadians());
                }

                // Rendering is actual Metal. Freeze world time for bounded visual/ending integration;
                // once the video is active its decoder receives actual elapsed frame time.
                game.renderFrame(width, height, wheel, movieStarted && !movieClosed ? delta : 0.0f);
                if (frames == 20) capture(cold ? "cold-entry" : "hive-entry");
                if (frames == 55 && !missingScroll && !reloadSuccess) capture("reactor-position");
                if (frames == 63 && !missingScroll && !reloadSuccess) capture("reactor-room-look");
                if (movieStarted && frames == movieStartFrame + 35)
                {
                    require(dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr,
                        "Ending video failed to remain open for actual rendered evidence");
                    capture(success ? "mm6-good-ending" : "mm6-bad-ending");
                }
                if (movieStarted && frames == movieStartFrame + 65)
                {
                    CutsceneVideoScreen *pMovie = dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen());
                    require(pMovie != nullptr, "Ending video disappeared before controlled skip");
                    SDL_Event escape = {};
                    escape.type = SDL_EVENT_KEY_DOWN;
                    escape.key.key = SDLK_ESCAPE;
                    pMovie->handleSdlEvent(escape);
                    game.handleCompletedEventMovieScreen();
                    movieClosed = true;
                    if (success)
                    {
                        require(game.m_screenManager.activeScreen() == nullptr,
                            "Good ending should resume gameplay without certificate or main menu");
                        save("final");
                        const std::optional<GameSaveData> saved = loadGameDataFromPath(output / "final.oysav", error);
                        require(saved.has_value(), "Cannot inspect final successful save");
                        checkSuccess(*saved);
                    }
                    else
                    {
                        require(game.m_screenManager.activeScreen() != nullptr
                            && game.m_screenManager.activeScreen()->mode() == AppMode::MainMenu,
                            "Bad ending did not return to the main menu");
                    }
                }
                if (movieClosed && frames == movieStartFrame + 75)
                {
                    capture(success ? "continued-oute3" : "failure-main-menu");
                }
                if (!pendingCapture.empty())
                {
                    if (std::optional<Engine::BgfxContext::ScreenshotCapture> screenshot =
                        Engine::BgfxContext::consumeScreenshot(pendingCapture))
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(screenshot->width), int(screenshot->height),
                            SDL_PIXELFORMAT_BGRA32, screenshot->bgraPixels.data(), int(screenshot->width * 4));
                        require(pSurface != nullptr, "Cannot create Hive screenshot surface");
                        const bool written = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(written, "Cannot save Hive screenshot");
                        pendingCapture.clear();
                        ++captures;
                    }
                }

                const bool done = reloadSuccess ? frames >= 50 : movieClosed && frames >= movieStartFrame + 90;
                if (done && pendingCapture.empty() && !complete)
                {
                    if (reloadSuccess)
                    {
                        save("final");
                        const std::optional<GameSaveData> loaded = loadGameDataFromPath(output / "final.oysav", error);
                        require(loaded.has_value(), "Cold successful save failed");
                        checkSuccess(*loaded);
                    }
                    require(captures == (reloadSuccess ? 1u : missingScroll ? 3u : 5u),
                        "Hive capture set incomplete");
                    complete = true;
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]() { game.shutdownApplication(); });

        require(app.run() == 0 && complete, "Actual Hive integration did not finish");
        std::ofstream(output / "result.json")
            << "{\"status\":\"passed\",\"controlled\":true,\"natural_campaign\":false,\"mode\":"
            << std::quoted(mode) << ",\"frames\":" << frames << ",\"captures\":" << captures
            << ",\"actor_reallocation_observed\":" << (reallocObserved ? "true" : "false")
            << ",\"actors_initial\":" << originalActorCount << ",\"actors_after_reactor\":"
            << afterReactorActorCount << ",\"movie_started\":" << (movieStarted ? "true" : "false")
            << ",\"movie_skipped_after_render\":" << (movieClosed ? "true" : "false")
            << ",\"independent_cold_process\":" << (cold ? "true" : "false") << "}\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 5)
        {
            throw std::runtime_error("mm6_hive_capture RUNTIME READONLY_SEED NEW_OUTPUT "
                "success|missing-scroll|leave-early|defeat|reload-success|reload-reactor-defeat");
        }
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
