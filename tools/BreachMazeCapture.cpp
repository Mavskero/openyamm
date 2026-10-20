#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/content/BreachMazeInstanceBuilder.h"
#include "game/content/BreachMazeLifecycle.h"
#include "game/content/BreachMazeRuntimeProfile.h"
#include "game/gameplay/GameplayBolsterRuntime.h"
#include "game/maps/MapIdentity.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/tables/SpellTable.h"
#include "tools/mmerge_import/BreachMazePopulationResources.h"
#include "tools/mmerge_import/BreachMazeRuntimeResources.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}
}

// Bounded actual-application driver. Source preparation and arrival are declared fixtures;
// rendering, AI, combat, movement integration and save writing use the normal runtime.
struct GameApplicationTestAccess
{
    static int runPortal(std::filesystem::path runtime, std::filesystem::path seed, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Use a fresh output directory");
        require(std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Output must remain on NVMe");
        std::ifstream header(seed, std::ios::binary);
        std::array<unsigned char, 12> bytes = {};
        header.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
        require(header && bytes[8] == 95 && bytes[9] == 0 && bytes[10] == 0 && bytes[11] == 0,
            "Portal input must be an existing Save95 file");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        settings.gameplayTrace = true;
        settings.gameplayTraceAppend = false;
        settings.gameplayTraceFile = (output / "gameplay.trace").string();
        std::string error;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Breach maze portal";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        enum class Phase { Cold, OverlayRender, OverlayCapture, Arrival, ArrivalRender, ArrivalCapture, Complete };
        Phase phase = Phase::Cold;
        unsigned frames = 0;
        unsigned phaseStarted = 0;
        unsigned captures = 0;
        uint64_t started = 0;
        bool complete = false;
        bool earlyExit = false;
        bool restrictionsVerified = false;
        bool firstContactDismissed = false;
        bool pendingVerified = false;
        bool savedVerified = false;
        std::string pendingCapture;
        std::string targetMap;
        int spellPointsBefore = 0;
        int spellPointsAfter = 0;
        float recoveryAtRequest = 0.0f;
        BreachMazeRunData beforeRun;
        Party::Snapshot beforeParty;
        int32_t enteredBreachBefore = 0;
        const auto writeResult = [&](const std::string &status, const std::string &message)
        {
            std::ofstream result(output / "result.json");
            result << std::boolalpha << std::setprecision(17) << "{\"status\":" << std::quoted(status)
                << ",\"frames\":" << frames << ",\"captures\":" << captures << ",\"complete\":" << complete
                << ",\"early_exit\":" << earlyExit << ",\"controlled\":true,\"natural_campaign\":false"
                   ",\"normal_merge_admission\":false,\"cold_restore\":true,\"caster_member_index\":2"
                << ",\"destination_id\":\"TPGlobal:0\",\"destination_map\":" << std::quoted(targetMap)
                << ",\"spell_points_before\":" << spellPointsBefore
                << ",\"spell_points_after\":" << spellPointsAfter
                << ",\"recovery_at_request\":" << recoveryAtRequest
                << ",\"cold_maze_restrictions_verified\":" << restrictionsVerified
                << ",\"first_contact_dismissed\":" << firstContactDismissed
                << ",\"pending_trace_verified\":" << pendingVerified
                << ",\"saved_escape_and_retained_floors_verified\":" << savedVerified;
            if (!message.empty()) result << ",\"error\":" << std::quoted(message);
            result << "}\n";
        };
        const auto requestCapture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Portal screenshots overlapped");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto verifyArrival = [&](const GameSaveData &saved)
        {
            require(saved.breachMaze.has_value(), "Portal arrival lost its maze save record");
            const BreachMazeRunData &after = *saved.breachMaze;
            require(!after.inside && after.currentLevel == 2 && after.direction == BreachMazeDirection::Escaped
                && after.version == beforeRun.version && after.seedVersion == beforeRun.seedVersion
                && after.templateFingerprint == beforeRun.templateFingerprint
                && after.rootSeed == beforeRun.rootSeed && after.epoch == beforeRun.epoch
                && after.nextGenerationSerial == beforeRun.nextGenerationSerial
                && after.floors.size() == beforeRun.floors.size(), "Portal did not preserve the L2 escaped run");
            for (const auto &[level, previous] : beforeRun.floors)
            {
                const auto &floor = after.floors.at(level);
                require(floor.descriptor.identity == previous.descriptor.identity
                    && floor.descriptor.seeds == previous.descriptor.seeds
                    && floor.descriptor.layout == previous.descriptor.layout && floor.population == previous.population
                    && floor.runtime.interaction == previous.runtime.interaction
                    && floor.runtime.actorRage == previous.runtime.actorRage,
                    "Portal changed a retained floor identity, population, interaction or actor Rage");
            }
            const auto entered = saved.namedGlobalVars.find("MMerge.CrossContinents.EnteredBreach");
            require((entered == saved.namedGlobalVars.end() ? 0 : entered->second) == enteredBreachBefore,
                "Portal departure assigned EnteredBreach");
            require(saved.party.questBits == beforeParty.questBits
                && saved.party.members.size() == beforeParty.members.size(), "Portal changed party quest state");
            for (size_t index = 0; index < beforeParty.members.size(); ++index)
            {
                const Character &left = beforeParty.members[index];
                const Character &right = saved.party.members[index];
                require(left.memberId == right.memberId && left.experience == right.experience
                    && left.health == right.health && left.conditions == right.conditions,
                    "Portal changed party identity, XP, HP or conditions");
                require(right.spellPoints == (index == 2 ? spellPointsAfter : left.spellPoints),
                    "Portal saved an unexpected mana cost");
            }
            require(saved.party.members[2].recoverySecondsRemaining == recoveryAtRequest,
                "Portal did not retain normal caster recovery");
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer(), "Portal renderer initialization failed");
                    require(game.loadSessionFromPath(seed), "Portal L2 input could not cold-load");
                    GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                    const auto &run = game.m_gameSession.breachMazeRunData();
                    require(run && run->version == BreachMazePlayableStateVersion && run->inside
                        && run->currentLevel == 2
                        && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase",
                        "Portal input must be a playable saved L2 inside BrBase");
                    beforeRun = *run;
                    require(screen.worldRuntime() && !screen.worldRuntime()->allowsLloydsBeacon()
                        && screen.worldRuntime()->townPortalUsesDimensionDoor()
                        && screen.worldRuntime()->eventRuntimeState()
                        && !screen.worldRuntime()->eventRuntimeState()->dimensionDoorActive
                        && screen.worldRuntime()->eventRuntimeState()->pendingDimensionDoorActivations == 0
                        && screen.shouldTownPortalCastOpenDimensionDoor()
                        && !screen.activeEventDialog().isActive && !screen.utilitySpellOverlayReadOnly().active,
                        "Cold maze restrictions or inactive natural Dimension Door state are incorrect");
                    restrictionsVerified = true;
                    Party *pParty = screen.party();
                    Character *pCaster = pParty ? pParty->member(2) : nullptr;
                    require(pCaster && pCaster->name == "阿肯那斯" && pCaster->spellPoints == 55
                        && pCaster->health > 0 && pCaster->recoverySecondsRemaining == 0.0f,
                        "Portal fixture requires the ready original member2 阿肯那斯 with 55 SP");
                    pCaster->skills["WaterMagic"] = {"WaterMagic", 10, SkillMastery::Grandmaster};
                    pCaster->learnSpell(31);
                    beforeParty = pParty->snapshot();
                    spellPointsBefore = pCaster->spellPoints;
                    enteredBreachBefore = game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBreach");
                    std::ofstream(output / "fixture.json")
                        << "{\"controlled\":true,\"input_save\":" << std::quoted(seed.string())
                        << ",\"caster_member_index\":2,\"caster_member_id\":" << pCaster->memberId
                        << ",\"caster_name\":" << std::quoted(pCaster->name)
                        << ",\"changes\":\"Only member2 WaterMagic=10 Grandmaster and learned Spell31. "
                           "No XP, quest, seed, HP or SP injection. Dismiss any real Verdant first contact "
                           "with Escape, then use the normal cast and Dimension Door target UI; "
                           "zero simulation delta keeps this bounded capture from advancing combat.\","
                           "\"natural_campaign\":false,\"normal_merge_admission\":false}\n";
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event)
                {
                    if (!complete && (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED))
                        earlyExit = true;
                    game.handleSdlEvent(event);
                },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 60 && SDL_GetTicks() - started < 60000, "Portal capture exceeded its guard");
                    game.renderFrame(width, height, wheel, 0.0f);
                    GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                    if (!pendingCapture.empty())
                    {
                        if (auto capture = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                                SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                            require(pSurface != nullptr, "Portal screenshot surface failed");
                            const std::string path = (output / (pendingCapture + ".bmp")).string();
                            const bool saved = SDL_SaveBMP(pSurface, path.c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Portal screenshot write failed");
                            ++captures;
                            pendingCapture.clear();
                        }
                    }
                    if (phase == Phase::Cold && frames >= 3)
                    {
                        const SpellEntry *pSpell = screen.spellTable() ? screen.spellTable()->findById(31) : nullptr;
                        require(pSpell && screen.tryCastSpellFromMember(2, 31, pSpell->name),
                            "Normal member2 Town Portal cast did not start");
                        if (screen.activeEventDialog().isActive)
                        {
                            require(screen.activeEventDialog().sourceId == 803,
                                "Town Portal opened an unexpected dialog");
                            GameplayInputFrame escape;
                            escape.screenWidth = width;
                            escape.screenHeight = height;
                            escape.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                            escape.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                            game.m_gameSession.updateGameplay(escape, 0.0f);
                            game.m_gameSession.updateGameplay({}, 0.0f);
                            require(!screen.activeEventDialog().isActive
                                && screen.party()->member(2)->spellPoints == spellPointsBefore
                                && screen.tryCastSpellFromMember(2, 31, pSpell->name),
                                "Normal Escape/retry did not leave the real Verdant first contact");
                            firstContactDismissed = true;
                        }
                        const auto &overlay = screen.utilitySpellOverlayReadOnly();
                        require(overlay.active
                            && overlay.mode == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor
                            && overlay.casterMemberIndex == 2 && overlay.spellId == 31
                            && overlay.spendMana && overlay.applyRecovery && !overlay.bypassRequiredMastery
                            && !overlay.bypassGameplayCasterValidation && !overlay.bypassTownPortalFailureChecks
                            && screen.party()->member(2)->spellPoints == spellPointsBefore
                            && !screen.worldRuntime()->eventRuntimeState()->dimensionDoorActive
                            && screen.worldRuntime()->eventRuntimeState()->pendingDimensionDoorActivations == 0,
                            "Town Portal did not open the normal paid Dimension Door overlay without a natural timer");
                        phase = Phase::OverlayRender;
                        phaseStarted = frames;
                    }
                    else if (phase == Phase::OverlayRender && frames - phaseStarted >= 3)
                    {
                        requestCapture("portal-overlay");
                        phase = Phase::OverlayCapture;
                    }
                    else if (phase == Phase::OverlayCapture && pendingCapture.empty())
                    {
                        const auto &destinations = screen.townPortalDestinations();
                        const auto target = std::find_if(destinations.begin(), destinations.end(),
                            [](const auto &destination) { return destination.id == "TPGlobal:0"; });
                        require(target != destinations.end(), "Current real TPGlobal:0 destination is absent");
                        targetMap = target->mapName;
                        const auto *pLayout = screen.findHudLayoutElement("TownPortalBackground");
                        require(pLayout && pLayout->visible, "Dimension Door background is absent");
                        const auto background = screen.resolveHudLayoutElement(
                            "TownPortalBackground", width, height, pLayout->width, pLayout->height);
                        require(background.has_value(), "Dimension Door background cannot be resolved");
                        GameplayInputFrame input;
                        input.screenWidth = width;
                        input.screenHeight = height;
                        // Existing CrossWorldTravelCapture's audited point in the original left continent region.
                        input.pointerX = background->x + 37.0f * background->width / 640.0f;
                        input.pointerY = background->y + 100.0f * background->width / 640.0f;
                        const auto hit = screen.resolveTownPortalDestinationAtPoint(
                            width, height, input.pointerX, input.pointerY);
                        require(hit && destinations[*hit].id == target->id,
                            "Original target UI point missed TPGlobal:0");
                        input.leftMouseButton = {true, true, false};
                        game.m_gameSession.updateGameplay(input, 0.0f);
                        input.leftMouseButton = {false, false, true};
                        game.m_gameSession.updateGameplay(input, 0.0f);
                        const EventRuntimeState *pEvents = screen.worldRuntime()->eventRuntimeState();
                        require(pEvents && pEvents->pendingMapMove
                            && pEvents->pendingMapMove->traceSourceKind == "town_portal_dimension_door"
                            && pEvents->pendingMapMove->mapName == targetMap
                            && pEvents->pendingMapMove->traceDestinationName == targetMap
                            && game.m_gameSession.breachMazeRunData()->inside
                            && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase",
                            "Normal target UI did not queue the typed source move for the App safe point");
                        spellPointsAfter = screen.party()->member(2)->spellPoints;
                        recoveryAtRequest = screen.party()->member(2)->recoverySecondsRemaining;
                        require(spellPointsAfter == spellPointsBefore - 20 && recoveryAtRequest > 0.0f,
                            "Target confirmation did not spend normal Town Portal mana and recovery");
                        pendingVerified = true;
                        phase = Phase::Arrival;
                    }
                    else if (phase == Phase::Arrival && game.m_gameSession.currentMapFileName() == targetMap)
                    {
                        const auto &map = game.m_gameDataLoader.getSelectedMap();
                        require(map && (map->map.worldId == "mm6" || map->map.worldId == "mm7"
                            || map->map.worldId == "mm8") && screen.worldRuntime()
                            && !screen.worldRuntime()->townPortalUsesDimensionDoor()
                            && !screen.shouldTownPortalCastOpenDimensionDoor(),
                            "App safe point did not arrive in a normal world without a natural Dimension Door timer");
                        const std::filesystem::path arrival = output / "portal-arrived.oysav";
                        require(game.quickSaveToPath(arrival, "148 L2 受控普通城镇传送到达"),
                            "Portal arrival save failed");
                        const auto saved = loadGameDataFromPath(arrival, error);
                        require(saved.has_value(), "Cannot read actual portal arrival save: " + error);
                        verifyArrival(*saved);
                        savedVerified = true;
                        phase = Phase::ArrivalRender;
                        phaseStarted = frames;
                    }
                    else if (phase == Phase::ArrivalRender && frames - phaseStarted >= 3)
                    {
                        requestCapture("portal-arrived");
                        phase = Phase::ArrivalCapture;
                    }
                    else if (phase == Phase::ArrivalCapture && pendingCapture.empty())
                    {
                        complete = true;
                        phase = Phase::Complete;
                        SDL_Event quit = {};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Portal driver could not request completion quit");
                    }
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete && !earlyExit && captures == 2,
                "Portal application ended before both actual captures and the saved arrival");
            std::ifstream trace(output / "gameplay.trace");
            bool arrivalTrace = false;
            for (std::string line; std::getline(trace, line);)
            {
                arrivalTrace = arrivalTrace || (line.find("map_arrived ") != std::string::npos
                    && line.find("source_kind=\"town_portal_dimension_door\"") != std::string::npos
                    && line.find(" map=\"" + targetMap + "\"") != std::string::npos);
            }
            require(arrivalTrace, "Normal App map_arrived trace did not retain the typed portal source");
            writeResult("passed", "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            writeResult("failed", failure.what());
            throw;
        }
    }

    static int runRoute(std::filesystem::path runtime, std::filesystem::path seed,
        std::filesystem::path output, bool interactRoute = false, bool entryRoute = false)
    {
        runtime = std::filesystem::canonical(runtime);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Use a fresh output directory");
        require(std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Output must remain on NVMe");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        std::string error;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Breach maze148 actual route";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        enum class Phase
        {
            ColdRender,
            ColdCapture,
            EntryOffer,
            EntryCapture,
            EntryArrival,
            Interactions,
            InteractionRender,
            InteractionCapture,
            CancelOffer,
            CancelCapture,
            CancelVerify,
            StepOffer,
            StepArrival,
            LandingRender,
            LandingCapture,
            Complete,
        };
        Phase phase = Phase::ColdRender;
        std::string stage = entryRoute ? "loading-entry-input" : "loading-l3-input";
        unsigned frames = 0;
        unsigned phaseStarted = 0;
        unsigned captures = 0;
        unsigned interactionPasses = 0;
        unsigned confirmations = 0;
        unsigned quitEvents = 0;
        unsigned windowCloseEvents = 0;
        bool cancelVerified = false;
        bool interactionsDone = false;
        bool trapVerified = false;
        bool bookcaseVerified = false;
        bool hiddenDecorationVerified = false;
        bool entryPointerVerified = false;
        bool freshIdentityVerified = false;
        uint64_t freshGenerationSerial = 0;
        size_t entryModel = 0;
        size_t entryFace = 0;
        bx::Vec3 entryCenter = {0.0f, 0.0f, 0.0f};
        std::vector<BreachMazeFloorIdentity> previousFloorIdentities;
        bool exitPlatformVerified = false;
        int exitSupportKind = -1;
        bool complete = false;
        bool earlyExitEventObserved = false;
        bool completionQuitRequested = false;
        std::optional<int> appExitCode;
        std::string pendingCapture;
        unsigned captureRequestedFrame = 0;
        std::string landingCapture;
        uint32_t leavingLevel = 3;
        uint32_t targetLevel = 2;
        uint64_t initialRootSeed = 0;
        uint64_t initialEpoch = 0;
        std::optional<BreachMazeRunData> cancelMaze;
        std::map<std::string, int32_t> cancelQuestGlobals;
        std::unordered_set<uint32_t> cancelQuestBits;
        uint64_t started = SDL_GetTicks();
        std::string lastMap;
        uint32_t lastLevel = 0;
        uint64_t lastRevision = 0;
        uint64_t lastRootSeed = 0;
        uint64_t lastEpoch = 0;
        bool lastInside = false;
        float lastX = 0;
        float lastY = 0;
        float lastFootZ = 0;
        int32_t lastEnteredBasement = 0;
        int32_t lastEnteredBreach = 0;
        int32_t lastQuestFinished = 0;
        std::ofstream trace(output / "trace.jsonl");
        const auto observe = [&]()
        {
            lastMap = game.m_gameSession.currentMapFileName();
            const auto &run = game.m_gameSession.breachMazeRunData();
            lastLevel = run ? run->currentLevel : 0;
            lastRevision = run ? run->revision : 0;
            lastRootSeed = run ? run->rootSeed : 0;
            lastEpoch = run ? run->epoch : 0;
            lastInside = run && run->inside;
            const auto &screen = game.m_gameSession.gameplayScreenRuntime();
            lastX = screen.partyX();
            lastY = screen.partyY();
            lastFootZ = screen.partyFootZ();
            lastEnteredBasement = game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBasement");
            lastEnteredBreach = game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBreach");
            lastQuestFinished = game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.QuestFinished");
        };
        const auto record = [&](const std::string &action)
        {
            trace << std::boolalpha << std::setprecision(17) << "{\"stage\":" << std::quoted(stage)
                << ",\"action\":" << std::quoted(action) << ",\"frame\":" << frames
                << ",\"captures\":" << captures << ",\"map\":" << std::quoted(lastMap)
                << ",\"level\":" << lastLevel << ",\"inside\":" << lastInside
                << ",\"root_seed\":" << lastRootSeed << ",\"epoch\":" << lastEpoch
                << ",\"revision\":" << lastRevision << ",\"pose\":[" << lastX << ',' << lastY << ',' << lastFootZ
                << "],\"entered_basement\":" << lastEnteredBasement << ",\"entered_breach\":" << lastEnteredBreach
                << ",\"quest_finished\":" << lastQuestFinished << ",\"complete\":" << complete << "}\n";
            trace.flush();
        };
        const auto advance = [&](Phase next, const std::string &name)
        {
            phase = next;
            phaseStarted = frames;
            stage = name;
            record("stage-started");
        };
        const auto finish = [&]()
        {
            complete = true;
            advance(Phase::Complete, entryRoute ? "entry-complete" : "route-complete");
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            completionQuitRequested = true;
            SDL_PushEvent(&quit);
        };
        const auto writeResult = [&](const std::string &status, const std::string &message)
        {
            std::ofstream result(output / "result.json");
            result << std::boolalpha << "{\"status\":" << std::quoted(status) << ",\"stage\":" << std::quoted(stage)
                << ",\"frames\":" << frames << ",\"captures\":" << captures << ",\"complete\":" << complete;
            if (entryRoute) result << ",\"mode\":\"entry\",\"entry_confirmations\":" << confirmations;
            else result << ",\"cancel_verified\":" << cancelVerified << ",\"confirmed_transitions\":" << confirmations;
            result << ",\"app_exit_code\":";
            if (appExitCode) result << *appExitCode;
            else result << "null";
            result << ",\"quit_events\":" << quitEvents << ",\"window_close_events\":" << windowCloseEvents
                << ",\"early_exit_event_observed\":" << earlyExitEventObserved
                << ",\"completion_quit_requested\":" << completionQuitRequested
                << ",\"pending_capture\":" << std::quoted(pendingCapture) << ",\"map\":" << std::quoted(lastMap)
                << ",\"level\":" << lastLevel << ",\"inside\":" << lastInside
                << ",\"root_seed\":" << lastRootSeed << ",\"epoch\":" << lastEpoch
                << ",\"controlled\":true,\"natural_campaign\":false,\"normal_merge_admission\":false"
                   ",\"cold_restore\":true";
            if (entryRoute)
            {
                result << ",\"entry_route\":true,\"entry_event\":53,\"entry_model\":" << entryModel
                    << ",\"entry_face\":" << entryFace << ",\"entry_pointer_verified\":" << entryPointerVerified
                    << ",\"fresh_identity_verified\":" << freshIdentityVerified
                    << ",\"fresh_generation_serial\":" << freshGenerationSerial
                    << ",\"input_root_seed\":" << initialRootSeed << ",\"input_epoch\":" << initialEpoch;
            }
            if (interactRoute)
            {
                result << ",\"interact_route\":true,\"interactions_done\":" << interactionsDone
                    << ",\"interaction_passes\":" << interactionPasses
                    << ",\"trap_verified\":" << trapVerified << ",\"bookcase_verified\":" << bookcaseVerified
                    << ",\"hidden_decoration_verified\":" << hiddenDecorationVerified
                    << ",\"interaction_coverage_complete\":"
                    << (trapVerified && bookcaseVerified && hiddenDecorationVerified)
                    << ",\"exit_foot_z\":" << lastFootZ << ",\"exit_support_kind\":" << exitSupportKind
                    << ",\"exit_platform_verified\":" << exitPlatformVerified;
            }
            if (!message.empty()) result << ",\"error\":" << std::quoted(message);
            result << "}\n";
        };
        const auto scene = [&]() -> IndoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Indoor,
                "Route expected an actual indoor scene");
            return *static_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto maze = [&]() -> const BreachMazeRunData &
        {
            const auto &run = game.m_gameSession.breachMazeRunData();
            require(run.has_value(), "Route lost its saved maze state");
            return *run;
        };
        const auto questGlobals = [&]()
        {
            std::map<std::string, int32_t> result;
            const auto append = [&](const auto &values, const std::string &scope)
            {
                for (const auto &[name, value] : values)
                {
                    if (name.starts_with("MMerge.CrossContinents.")) result.emplace(scope + name, value);
                }
            };
            append(game.m_gameSession.namedGlobalVars(), "session:");
            require(scene().eventRuntimeState() != nullptr, "Route scene has no event state");
            append(scene().eventRuntimeState()->namedGlobalVars, "scene:");
            return result;
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(output / (name + ".oysav"), "148 受控迷宫路线 " + name),
                "Cannot save route stage " + name);
            observe();
            record("save-written:" + name);
        };
        const auto requestCapture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Route screenshot request overlapped " + pendingCapture);
            pendingCapture = name;
            captureRequestedFrame = frames;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
            record("screenshot-requested:" + name);
        };
        const auto stageEntryDoor = [&]()
        {
            const auto &selected = game.m_gameDataLoader.getSelectedMap();
            require(selected && selected->outdoorMapData && game.m_pOutdoorPartyRuntime,
                "Entry input lacks actual BrAlvar outdoor geometry");
            const auto &models = selected->outdoorMapData->bmodels;
            const OutdoorBModelFace *pDoor = nullptr;
            for (size_t model = 0; model < models.size(); ++model)
                for (size_t face = 0; face < models[model].faces.size(); ++face)
                    if (models[model].faces[face].cogTriggeredNumber == 53)
                    {
                        require(pDoor == nullptr, "Entry event53 has ambiguous source faces");
                        pDoor = &models[model].faces[face];
                        entryModel = model;
                        entryFace = face;
                    }
            require(pDoor && !pDoor->vertexIndices.empty()
                && hasFaceAttribute(pDoor->attributes, FaceAttribute::Clickable)
                && !hasFaceAttribute(pDoor->attributes, FaceAttribute::Invisible)
                && !hasFaceAttribute(pDoor->attributes, FaceAttribute::HasHint),
                "Actual event53 face is missing or not clickable");
            float bottom = std::numeric_limits<float>::max();
            for (uint16_t index : pDoor->vertexIndices)
            {
                const auto &vertex = models[entryModel].vertices.at(index);
                entryCenter = bx::add(entryCenter, {float(vertex.x), float(vertex.y), float(vertex.z)});
                bottom = std::min(bottom, float(vertex.z));
            }
            entryCenter = bx::mul(entryCenter, 1.0f / float(pDoor->vertexIndices.size()));
            const float nx = float(pDoor->planeNormalX);
            const float ny = float(pDoor->planeNormalY);
            const float length = std::hypot(nx, ny);
            require(length > 0, "Entry event53 face has no horizontal source normal");
            game.m_pOutdoorPartyRuntime->teleportTo(
                entryCenter.x + nx / length * 256, entryCenter.y + ny / length * 256, bottom + 1);
            const OutdoorMoveState &pose = game.m_pOutdoorPartyRuntime->movementState();
            game.m_outdoorGameView.setCameraAngles(std::atan2(entryCenter.y - pose.y, entryCenter.x - pose.x),
                std::atan2(entryCenter.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                    std::hypot(entryCenter.x - pose.x, entryCenter.y - pose.y)));
            observe();
            record("controlled-event53-door-pose");
        };
        const auto clickEntryDoor = [&](int width, int height)
        {
            IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
            require(pWorld && !pWorld->isIndoorMap(), "Entry pointer lost its actual outdoor world");
            const auto camera = pWorld->buildWorldPickRequest({0, 0, width, height, false});
            float viewProjection[16] = {};
            bx::mtxMul(viewProjection, camera.viewMatrix.data(), camera.projectionMatrix.data());
            // Read-only samples inside the actual door polygon respect foreground decorations and opaque picking.
            const auto *pMap = game.m_pOutdoorWorldRuntime->mapData();
            const auto &model = pMap->bmodels.at(entryModel);
            const auto &face = model.faces.at(entryFace);
            std::vector<bx::Vec3> points{entryCenter};
            for (uint16_t index : face.vertexIndices)
            {
                if (points.size() == 9) break;
                const auto &vertex = model.vertices.at(index);
                points.push_back(bx::add(entryCenter,
                    bx::mul(bx::sub({float(vertex.x), float(vertex.y), float(vertex.z)}, entryCenter), 0.65f)));
            }
            std::optional<std::array<float, 2>> pixel;
            for (const bx::Vec3 &point : points)
            {
                const bx::Vec3 projected = bx::mulH(point, viewProjection);
                const float x = (projected.x + 1) * float(width) * 0.5f;
                const float y = (1 - projected.y) * float(height) * 0.5f;
                const auto hit = pWorld->pickMouseInteractionTarget(
                    pWorld->buildWorldPickRequest({x, y, width, height, true}));
                trace << "{\"stage\":\"entry-pointer-probe\",\"frame\":" << frames
                    << ",\"pixel\":[" << x << ',' << y << "],\"hit_kind\":" << int(hit.kind)
                    << ",\"target_kind\":" << (hit.eventTarget ? int(hit.eventTarget->targetKind) : 0)
                    << ",\"model\":" << (hit.eventTarget ? hit.eventTarget->targetIndex : GameplayInvalidWorldIndex)
                    << ",\"face\":" << (hit.eventTarget ? hit.eventTarget->secondaryIndex : GameplayInvalidWorldIndex)
                    << ",\"event\":" << (hit.eventTarget ? hit.eventTarget->triggeredEventId : 0) << "}\n";
                if (hit.eventTarget && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                    && hit.eventTarget->targetIndex == entryModel && hit.eventTarget->secondaryIndex == entryFace
                    && hit.eventTarget->triggeredEventId == 53
                    && pWorld->canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                {
                    pixel = std::array<float, 2>{x, y};
                    break;
                }
            }
            trace.flush();
            require(pixel.has_value(), "Actual event53 door has no visible activatable sample");
            const float x = (*pixel)[0];
            const float y = (*pixel)[1];
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.rightMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(input, 0);
            input.rightMouseButton.pressed = false;
            input.leftMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(input, 0);
            input.leftMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(input, 0);
            input.leftMouseButton = {};
            input.rightMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(input, 0);
            const EventRuntimeState *pEvents = game.m_gameplayController.eventRuntimeState();
            require(pEvents && pEvents->pendingBreachMazeTransitionRequest
                && pEvents->pendingBreachMazeTransitionRequest->kind
                    == EventRuntimeState::PendingBreachMazeTransitionRequest::Kind::BrAlvar53,
                "Normal event53 pointer input did not queue its source entry request");
            entryPointerVerified = true;
            record("actual-event53-pointer-queued");
        };
        const auto interact = [&]()
        {
            const uint32_t interactionLevel = maze().currentLevel;
            const BreachMazeFloorState initial = maze().floors.at(interactionLevel);
            require(initial.population && initial.runtime.interaction, "Migrated live floor lacks interactions");
            require(scene().worldRuntime().hasNativeEventHandler(), "Live floor lacks its native interaction handler");
            const auto &profile = game.m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*game.m_pAssetFileSystem);
            const auto &selected = game.m_gameDataLoader.getSelectedMap();
            require(selected && selected->indoorMapData && selected->localEventProgram,
                "Interaction route lacks its actual map and local text program");
            const auto &map = *selected->indoorMapData;
            const auto interaction = [&]() -> const BreachMazeInteractionState &
            {
                const auto &floor = maze().floors.at(maze().currentLevel);
                require(floor.runtime.interaction.has_value(), "Live native interaction record disappeared");
                return *floor.runtime.interaction;
            };
            const auto placeAtCell = [&](size_t cell)
            {
                require(cell < profile.mazeTemplate.cells.size(), "Interaction fixture cell is outside the template");
                const auto &source = profile.mazeTemplate.cells[cell];
                scene().partyRuntime().teleportPartyPosition(
                    601.5f * float(source.x - 1), -601.5f * float(source.y - 1), -200);
                observe();
                record("controlled-interaction-cell:" + std::to_string(cell + 1));
            };
            const auto attributes = [&](size_t face)
            {
                const MapDeltaData *pDelta = scene().worldRuntime().mapDeltaData();
                return pDelta && face < pDelta->faceAttributes.size()
                    ? pDelta->faceAttributes[face] : map.faces.at(face).attributes;
            };
            const auto ownedItems = [&]()
            {
                std::map<uint32_t, uint64_t> items;
                for (const Character &member : scene().party().members())
                    for (const InventoryItem &item : member.inventory) items[item.objectDescriptionId] += item.quantity;
                for (const auto &delivery : scene().party().pendingItemDeliveries())
                    items[delivery.item.objectDescriptionId] += delivery.item.quantity;
                const auto &held = game.m_gameSession.gameplayScreenRuntime().heldInventoryItem();
                if (held.active) items[held.item.objectDescriptionId] += held.item.quantity;
                return items;
            };
            const auto itemCount = [&](const auto &items)
            {
                uint64_t count = 0;
                for (const auto &[id, quantity] : items) count += quantity;
                return count;
            };
            std::ofstream evidence(output / "interactions.jsonl", std::ios::app);
            evidence << std::boolalpha;
            const auto finishRecord = [&]()
            {
                evidence << "}\n";
                evidence.flush();
                observe();
            };
            std::optional<size_t> trapCell;
            for (size_t cell = 0; !trapVerified && cell < initial.population->traps.size(); ++cell)
            {
                const auto &trap = interaction().traps[cell];
                const uint32_t face = profile.mazeTemplate.cells[cell].centralFloorFaceIndex;
                if (initial.population->traps[cell].placed && (!trap.spellId || *trap.spellId != 0)
                    && trap.nextShotGameMinute <= scene().worldRuntime().currentGameMinutesPrecise()
                    && face < map.faces.size() && map.faces[face].cogTriggered == 3
                    && hasFaceAttribute(attributes(face), FaceAttribute::PressurePlate))
                {
                    trapCell = cell;
                    break;
                }
            }
            evidence << "{\"kind\":\"trap\",\"level\":" << maze().currentLevel
                << ",\"skipped_verified\":" << trapVerified << ",\"available\":" << trapCell.has_value();
            if (trapCell)
            {
                const size_t cell = *trapCell;
                const uint32_t face = profile.mazeTemplate.cells[cell].centralFloorFaceIndex;
                const BreachMazeTrapState before = interaction().traps[cell];
                const FixedItemGenerationCursor randomBefore = interaction().random;
                placeAtCell(cell);
                require(scene().worldRuntime().executeFaceTriggeredEvent(face, FaceAttribute::PressurePlate, false),
                    "Actual trap pressure-plate event did not execute");
                const BreachMazeTrapState after = interaction().traps[cell];
                require(after != before && interaction().random.rawDrawCount >= randomBefore.rawDrawCount,
                    "Actual trap did not change its live state or rolled its cursor back");
                if (!before.spellId)
                    require(interaction().random.rawDrawCount > randomBefore.rawDrawCount,
                        "Fresh trap spell selection did not consume the source cursor");
                if (after.spellId == 0)
                {
                    const uint32_t disabled = faceAttributeBit(FaceAttribute::IsSecret)
                        | faceAttributeBit(FaceAttribute::Clickable) | faceAttributeBit(FaceAttribute::PressurePlate);
                    require((attributes(face) & disabled) == 0, "Disarmed/exhausted trap projection stayed active");
                }
                trapVerified = true;
                evidence << ",\"cell\":" << cell + 1 << ",\"face\":" << face
                    << ",\"draws_before\":" << randomBefore.rawDrawCount
                    << ",\"draws_after\":" << interaction().random.rawDrawCount
                    << ",\"next_shot_before\":" << before.nextShotGameMinute
                    << ",\"next_shot_after\":" << after.nextShotGameMinute << ",\"verified\":true";
                record("actual-trap-verified");
            }
            finishRecord();

            std::optional<size_t> bookcaseFace;
            size_t bookcaseCell = 0;
            for (size_t face = 0; interactionLevel == 3 && !bookcaseVerified
                && face < map.faces.size() && face < profile.mazeTemplate.faces.size(); ++face)
            {
                const uint16_t cellId = profile.mazeTemplate.faces[face].cellId;
                if (map.faces[face].cogTriggered == 4 && hasFaceAttribute(attributes(face), FaceAttribute::Clickable)
                    && cellId > 0 && cellId <= BreachMazeTemplateCellCount
                    && initial.population->bookcaseCells[cellId - 1] && !interaction().bookcaseUsed[cellId - 1])
                {
                    bookcaseFace = face;
                    bookcaseCell = cellId - 1;
                    break;
                }
            }
            evidence << "{\"kind\":\"bookcase\",\"level\":" << maze().currentLevel
                << ",\"skipped_verified\":" << bookcaseVerified << ",\"eligible\":" << (interactionLevel == 3)
                << ",\"available\":" << bookcaseFace.has_value();
            if (bookcaseFace)
            {
                placeAtCell(bookcaseCell);
                const auto before = ownedItems();
                const FixedItemGenerationCursor randomBefore = interaction().random;
                require(scene().activateEvent(4, "face", *bookcaseFace), "Actual bookcase event did not execute");
                const auto after = ownedItems();
                require(interaction().bookcaseUsed[bookcaseCell]
                    && interaction().random.rawDrawCount > randomBefore.rawDrawCount
                    && itemCount(after) == itemCount(before) + 1,
                    "Bookcase did not commit one owned scroll and used state");
                for (const auto &[id, quantity] : before)
                    require(after.contains(id) && after.at(id) >= quantity, "Bookcase lost an existing owned item");
                const BreachMazeInteractionState once = interaction();
                EventRuntimeState *pEvents = scene().eventRuntimeState();
                const size_t messagesBefore = pEvents->statusMessages.size();
                require(scene().activateEvent(4, "face", *bookcaseFace), "Repeated bookcase event did not execute");
                const auto emptyText = selected->localEventProgram->getImportedGlobalText(521);
                require(interaction() == once && ownedItems() == after,
                    "Repeated bookcase changed its cursor/state or granted a second item");
                require(emptyText && pEvents->statusMessages.size() > messagesBefore
                    && pEvents->statusMessages.back() == *emptyText, "Empty bookcase did not display source Global521");
                bookcaseVerified = true;
                evidence << ",\"cell\":" << bookcaseCell + 1 << ",\"face\":" << *bookcaseFace
                    << ",\"draws_before\":" << randomBefore.rawDrawCount
                    << ",\"draws_after\":" << once.random.rawDrawCount
                    << ",\"owned_items_before\":" << itemCount(before) << ",\"owned_items_after\":" << itemCount(after)
                    << ",\"repeat_reward\":false,\"empty_message\":" << std::quoted(*emptyText) << ",\"verified\":true";
                record("actual-bookcase-and-empty-repeat-verified");
            }
            finishRecord();

            std::optional<size_t> hiddenSlot;
            const auto collidersBefore = scene().worldRuntime().decorationMovementColliders();
            for (size_t slot = 0; !hiddenDecorationVerified && slot < initial.population->decorations.size(); ++slot)
            {
                const auto &decoration = initial.population->decorations[slot];
                const auto handler = decoration.interaction.handler;
                if (decoration.invisible || decoration.cellId == 0 || interaction().decorations[slot].used
                    || (handler != BreachMazeDecorationHandler::MiscBag
                        && handler != BreachMazeDecorationHandler::FoodSack))
                    continue;
                if (!hiddenSlot) hiddenSlot = slot;
                if (std::any_of(collidersBefore.begin(), collidersBefore.end(),
                    [&](const auto &collider) { return collider.sourceIndex == slot; }))
                {
                    hiddenSlot = slot;
                    break;
                }
            }
            evidence << "{\"kind\":\"hidden-container\",\"level\":" << maze().currentLevel
                << ",\"skipped_verified\":" << hiddenDecorationVerified << ",\"available\":" << hiddenSlot.has_value();
            if (hiddenSlot)
            {
                const size_t slot = *hiddenSlot;
                const auto &decoration = initial.population->decorations[slot];
                require(slot < map.entities.size() && map.entities[slot].scriptEventId() == decoration.eventId,
                    "Container slot is not bound to its actual source decoration event");
                placeAtCell(decoration.cellId - 1);
                const auto beforeItems = ownedItems();
                const int foodBefore = scene().party().food();
                const auto randomBefore = interaction().decorations[slot].random;
                require(scene().activateEvent(uint16_t(decoration.eventId), "decoration", slot),
                    "Actual container decoration event did not execute");
                const auto &after = interaction().decorations[slot];
                require(after.used && after.random.rawDrawCount > randomBefore.rawDrawCount,
                    "Actual container did not commit its used state and random cursor");
                const MapDeltaData *pDelta = scene().worldRuntime().mapDeltaData();
                const EventRuntimeState *pEvents = scene().eventRuntimeState();
                const uint32_t key = map.entities[slot].spriteOverrideKey(slot);
                require(pDelta && slot < pDelta->decorationFlags.size() && (pDelta->decorationFlags[slot] & 0x20) != 0
                    && pEvents->spriteOverrides.contains(key) && pEvents->spriteOverrides.at(key).hidden,
                    "Used container is not hidden in the live delta and sprite override");
                const auto collidersAfter = scene().worldRuntime().decorationMovementColliders();
                const size_t removed = size_t(std::count_if(collidersBefore.begin(), collidersBefore.end(),
                    [&](const auto &collider) { return collider.sourceIndex == slot; }));
                require(collidersAfter.size() + removed == collidersBefore.size()
                    && std::none_of(collidersAfter.begin(), collidersAfter.end(),
                        [&](const auto &collider) { return collider.sourceIndex == slot; }),
                    "Used container kept its movement collider or removed unrelated colliders");
                for (const auto &collider : collidersBefore)
                    if (collider.sourceIndex != slot)
                        require(std::any_of(collidersAfter.begin(), collidersAfter.end(),
                            [&](const auto &other) { return other.sourceIndex == collider.sourceIndex; }),
                            "Container hiding removed another decoration collider");
                const auto afterItems = ownedItems();
                const int foodAfter = scene().party().food();
                const uint64_t itemsBefore = itemCount(beforeItems);
                const uint64_t itemsAfter = itemCount(afterItems);
                const bool foodSack = decoration.interaction.handler == BreachMazeDecorationHandler::FoodSack;
                if (foodSack)
                    require(itemsAfter == itemsBefore && foodAfter >= foodBefore && foodAfter - foodBefore <= 2,
                        "Food sack produced an invalid food/item change");
                else
                    require(foodAfter == foodBefore && itemsAfter >= itemsBefore && itemsAfter - itemsBefore <= 1,
                        "Misc bag produced an invalid food/item change");
                if (itemsAfter == itemsBefore && foodAfter == foodBefore)
                {
                    const auto emptyText = game.m_gameSession.data().npcDialogTable().getText(foodSack ? 2121 : 2123);
                    require(emptyText && !pEvents->statusMessages.empty()
                        && pEvents->statusMessages.back() == *emptyText,
                        "Empty container branch did not emit its original source message");
                }
                hiddenDecorationVerified = true;
                evidence << ",\"slot\":" << slot << ",\"cell\":" << decoration.cellId
                    << ",\"event\":" << decoration.eventId << ",\"handler\":" << int(decoration.interaction.handler)
                    << ",\"draws_before\":" << randomBefore.rawDrawCount
                    << ",\"draws_after\":" << after.random.rawDrawCount
                    << ",\"owned_items_before\":" << itemsBefore << ",\"owned_items_after\":" << itemsAfter
                    << ",\"food_before\":" << foodBefore << ",\"food_after\":" << foodAfter
                    << ",\"colliders_before\":" << collidersBefore.size()
                    << ",\"colliders_after\":" << collidersAfter.size()
                    << ",\"slot_colliders_removed\":" << removed << ",\"verified\":true";
                record("actual-container-reward-hide-and-colliders-verified");
            }
            finishRecord();
            interactionsDone = true;
            ++interactionPasses;
            save("l" + std::to_string(interactionLevel) + "-interaction");
            save("interaction-save");
        };
        const auto activateUp = [&]()
        {
            const auto &run = maze();
            require(run.inside && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase",
                "Up event is not on a saved live maze floor");
            const auto &floor = run.floors.at(run.currentLevel);
            const auto &profile = game.m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*game.m_pAssetFileSystem);
            const BreachMazeEndpoint &endpoint = floor.descriptor.layout.endpoints.entrance;
            require(endpoint.templateCellId > 0 && endpoint.templateCellId <= profile.mazeTemplate.cells.size(),
                "Route entrance has an invalid template cell");
            const auto &cell = profile.mazeTemplate.cells[endpoint.templateCellId - 1];
            // Declared navigation fixture only; the Scene still resolves the real local event and live bindings.
            scene().partyRuntime().teleportPartyPosition(601.5f * float(cell.x - 1), -601.5f * float(cell.y - 1), -200);
            EventRuntimeState *pState = scene().eventRuntimeState();
            require(pState && pState->normalGameplayScreen && !pState->activeHookContext
                && !pState->activeDecorationContext && !pState->pendingDialogueContext && !pState->pendingMapMove,
                "Route entrance is not in the normal local-map gameplay context");
            require(game.m_gameSession.activeWorldRuntime() == &scene().worldRuntime()
                && scene().worldRuntime().boundGameSession() == &game.m_gameSession
                && scene().worldRuntime().eventRuntimeState() == pState
                && scene().worldRuntime().party() == &scene().party(),
                "Route live scene/world/session bindings differ");
            observe();
            record("controlled-entrance-pose");
            require(scene().activateEvent(1, "face", endpoint.faceIndex), "Actual Scene maze event 1 did not execute");
            require(pState->pendingBreachMazeTransitionRequest.has_value(),
                "Actual Scene event 1 did not queue a maze transition request");
            record("scene-event-1-queued");
        };
        const auto selectTransition = [&](EventDialogActionKind kind)
        {
            auto &screen = game.m_gameSession.gameplayScreenRuntime();
            const auto &dialog = screen.activeEventDialog();
            require(dialog.isActive && dialog.presentation == EventDialogPresentation::Transition,
                "App did not present the normal map transition confirmation");
            const EventRuntimeState *pState = game.m_gameplayController.eventRuntimeState();
            require(pState && pState->pendingDialogueContext && pState->pendingDialogueContext->transitionMapMove
                && pState->pendingDialogueContext->transitionMapMove->traceSourceKind == "breach_maze",
                "Transition dialogue does not belong to the actual maze request");
            const auto action = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                [&](const EventDialogAction &candidate) { return candidate.kind == kind && candidate.enabled; });
            require(action != dialog.actions.end(), "Normal transition action is absent or disabled");
            screen.eventDialogSelectionIndex() = size_t(action - dialog.actions.begin());
            screen.executeActiveDialogAction();
            if (kind == EventDialogActionKind::MapTransitionConfirm)
            {
                const auto &queued = game.m_gameSession.pendingMapMove();
                require(queued && queued->traceSourceKind == "breach_maze",
                    "Normal confirmation did not publish the maze map move");
            }
            record(kind == EventDialogActionKind::MapTransitionConfirm ? "dialog-confirmed" : "dialog-cancelled");
        };
        const auto beginCancel = [&]()
        {
            cancelMaze = maze();
            cancelQuestGlobals = questGlobals();
            cancelQuestBits = scene().party().snapshot().questBits;
            activateUp();
            advance(Phase::CancelOffer, "l3-cancel-offer");
        };
        const auto verifyExitPlatform = [&]()
        {
            if (interactRoute && targetLevel == 0)
            {
                require(game.m_pOutdoorPartyRuntime != nullptr, "BrAlvar exit has no live outdoor party runtime");
                const OutdoorMoveState &movement = game.m_pOutdoorPartyRuntime->movementState();
                exitSupportKind = int(movement.supportKind);
                exitPlatformVerified = movement.supportKind == OutdoorSupportKind::BModelFace
                    && std::abs(movement.footZ - 857.0f) <= 0.25f;
                require(exitPlatformVerified,
                    "BrAlvar exit requires BModelFace support and footZ 857 +/- 0.25; support="
                        + std::to_string(exitSupportKind) + ", footZ=" + std::to_string(movement.footZ));
            }
        };
        std::ofstream(output / "fixture.json") << "{\"controlled\":true,\"input_save\":" << std::quoted(seed.string())
            << ",\"changes\":" << std::quoted(entryRoute
                ? "Cold-load the saved BrAlvar exit. Place only party pose 256 units in front of the existing "
                  "event53 face and aim at its source vertex center. Verify its actual world-pointer pick, then "
                  "use normal right-held cursor and left-click input. Confirm the normal transition dialog. "
                  "App owns fresh seed, epoch, L3 generation and entry effects. No Party/quest/seed injection."
                : "Cold-load the saved L3. At each Up door, teleport only the live party pose to its source entrance "
                  "cell center, then activate Scene event 1 with no decoration/dialogue context. Cancel the first "
                  "normal confirmation once; confirm the next three through existing dialog actions. App frame "
                  "safe points own all L3-to-L2-to-L1-to-BrAlvar transitions. No quest/HP/level/item injection.")
            << ",\"natural_campaign\":false,\"normal_merge_admission\":false,\"cold_restore\":true"
            << ",\"entry_route\":" << (entryRoute ? "true" : "false")
            << ",\"interact_route\":" << (interactRoute ? "true" : "false")
            << ",\"interaction_fixture\":" << std::quoted(interactRoute
                ? "Before door cancellation, teleport only to existing L3 source cell centers. Activate one live trap "
                  "through its pressure-plate face, one existing bookcase twice, and one existing misc bag/food sack. "
                  "On L2/L1 arrival, try only still-unverified traps/containers using that floor's actual slots. "
                  "Report absent types. Preserve per-floor interaction saves and update interaction-save with the "
                  "latest native rewards, used state, cursors and hidden/collider projections."
                : "none") << "}\n";
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer(), "Route renderer initialization failed");
                    require(game.loadSessionFromPath(seed), "Actual route input could not cold-load");
                    require(maze().version == BreachMazePlayableStateVersion, "Route input is not playable maze state");
                    initialRootSeed = maze().rootSeed;
                    initialEpoch = maze().epoch;
                    if (entryRoute)
                    {
                        require(!maze().inside && game.m_pMapSceneRuntime
                            && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                            && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "bralvar",
                            "Entry input must be outside in BrAlvar");
                        for (const auto &[level, floor] : maze().floors)
                            previousFloorIdentities.push_back(floor.descriptor.identity);
                        stageEntryDoor();
                    }
                    else
                    {
                        require(maze().inside && maze().currentLevel == 3
                            && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase",
                            "Route input must be a playable saved L3 inside BrBase");
                    }
                    require(!game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive,
                        "Route input has an unfinished dialogue");
                    require(game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.QuestFinished") == 0,
                        "Route requires the unfinished source main quest");
                    save(entryRoute ? "entry-before" : "l3-cold");
                    started = SDL_GetTicks();
                    advance(Phase::ColdRender, entryRoute ? "entry-door-render" : "l3-cold-render");
                    return true;
                },
                [&](const SDL_Event &event)
                {
                    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    {
                        if (!complete) earlyExitEventObserved = true;
                        if (event.type == SDL_EVENT_QUIT) ++quitEvents;
                        else ++windowCloseEvents;
                        record(event.type == SDL_EVENT_QUIT ? "quit-event-observed" : "window-close-event-observed");
                    }
                    game.handleSdlEvent(event);
                },
                [&](int width, int height, float wheel, float delta)
                {
                    ++frames;
                    require(SDL_GetTicks() - started < 120000, "Actual route exceeded its wall-clock guard");
                    require(frames - phaseStarted < 600, "Actual route stage did not progress");
                    game.renderFrame(width, height, wheel, delta);
                    observe();
                    if (!pendingCapture.empty())
                    {
                        require(frames - captureRequestedFrame < 180,
                            "Screenshot callback missing for " + pendingCapture);
                        if (auto capture = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                                SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                            require(pSurface != nullptr, "Route screenshot surface failed");
                            const std::filesystem::path path = output / (pendingCapture + ".bmp");
                            const bool saved = SDL_SaveBMP(pSurface, path.string().c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Route screenshot write failed for " + pendingCapture);
                            ++captures;
                            record("screenshot-saved:" + pendingCapture);
                            pendingCapture.clear();
                        }
                    }
                    switch (phase)
                    {
                    case Phase::ColdRender:
                        if (frames - phaseStarted >= 3)
                        {
                            if (entryRoute)
                            {
                                clickEntryDoor(width, height);
                                advance(Phase::EntryOffer, "entry-confirmation-offer");
                            }
                            else
                            {
                                requestCapture("l3-cold");
                                advance(Phase::ColdCapture, "l3-cold-capture");
                            }
                        }
                        break;
                    case Phase::EntryOffer:
                        if (game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive)
                        {
                            const EventRuntimeState *pEvents = game.m_gameplayController.eventRuntimeState();
                            require(pEvents && pEvents->pendingDialogueContext
                                && pEvents->pendingDialogueContext->sourceId == 53
                                && pEvents->pendingDialogueContext->transitionTextId == 710
                                && pEvents->pendingDialogueContext->transitionImageId == 3,
                                "Pointer entry did not present source event53 confirmation");
                            requestCapture("entry-confirmation");
                            advance(Phase::EntryCapture, "entry-confirmation-capture");
                        }
                        break;
                    case Phase::EntryCapture:
                        if (pendingCapture.empty())
                        {
                            selectTransition(EventDialogActionKind::MapTransitionConfirm);
                            ++confirmations;
                            advance(Phase::EntryArrival, "entry-fresh-arrival");
                        }
                        break;
                    case Phase::EntryArrival:
                    {
                        const auto &run = maze();
                        if (!run.inside) break;
                        require(run.currentLevel == 3 && run.direction == BreachMazeDirection::Fresh
                            && run.epoch == initialEpoch + 1 && run.rootSeed != initialRootSeed
                            && run.floors.size() == 1
                            && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase",
                            "Source entry did not create a new L3 run epoch and seed");
                        const auto &floor = run.floors.at(3);
                        const auto &identity = floor.descriptor.identity;
                        require(identity.epoch == run.epoch && identity.level == 3
                            && std::none_of(previousFloorIdentities.begin(), previousFloorIdentities.end(),
                                [&](const auto &previous) { return previous == identity; })
                            && floor.population && floor.runtime.interaction
                            && scene().worldRuntime().hasNativeEventHandler(),
                            "Fresh entry reused an old floor identity or lacks native population/interactions");
                        const auto live = scene().worldRuntime().snapshot();
                        require(live.mergeMonsterGeneration
                            && live.mergeMonsterGeneration->generationEpoch == identity.epoch
                            && live.mergeMonsterGeneration->generationLevel == identity.level
                            && live.mergeMonsterGeneration->generationSerial == identity.generationSerial,
                            "Fresh floor live generation does not match its committed identity");
                        require(lastEnteredBasement != 0 && lastQuestFinished == 0,
                            "Fresh L3 entry did not preserve source quest effects");
                        freshGenerationSerial = identity.generationSerial;
                        freshIdentityVerified = true;
                        landingCapture = "entry-fresh";
                        save(landingCapture);
                        advance(Phase::LandingRender, landingCapture + "-render");
                        break;
                    }
                    case Phase::ColdCapture:
                        if (pendingCapture.empty())
                        {
                            if (interactRoute) advance(Phase::Interactions, "l3-actual-interactions");
                            else beginCancel();
                        }
                        break;
                    case Phase::Interactions:
                        interact();
                        advance(Phase::InteractionRender, "l" + std::to_string(maze().currentLevel)
                            + "-interactions-render");
                        break;
                    case Phase::InteractionRender:
                        if (frames - phaseStarted >= 3)
                        {
                            const std::string name = "l" + std::to_string(maze().currentLevel) + "-interactions";
                            requestCapture(name);
                            advance(Phase::InteractionCapture, name + "-capture");
                        }
                        break;
                    case Phase::InteractionCapture:
                        if (pendingCapture.empty())
                        {
                            if (!cancelVerified) beginCancel();
                            else
                            {
                                activateUp();
                                advance(Phase::StepOffer, "l" + std::to_string(maze().currentLevel) + "-up-offer");
                            }
                        }
                        break;
                    case Phase::CancelOffer:
                        if (game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive)
                        {
                            require(game.m_gameSession.gameplayScreenRuntime().activeEventDialog().presentation
                                == EventDialogPresentation::Transition, "Cancel route offered a different dialogue");
                            requestCapture("l3-cancel-confirmation");
                            advance(Phase::CancelCapture, "l3-cancel-capture");
                        }
                        break;
                    case Phase::CancelCapture:
                        if (pendingCapture.empty())
                        {
                            selectTransition(EventDialogActionKind::MapTransitionCancel);
                            advance(Phase::CancelVerify, "l3-cancel-verify");
                        }
                        break;
                    case Phase::CancelVerify:
                    {
                        const auto &run = maze();
                        require(cancelMaze && run.inside && run.currentLevel == 3
                            && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase"
                            && run.rootSeed == cancelMaze->rootSeed && run.epoch == cancelMaze->epoch
                            && run.nextGenerationSerial == cancelMaze->nextGenerationSerial
                            && run.floors.size() == cancelMaze->floors.size(), "Cancellation changed the maze run");
                        for (const auto &[level, previous] : cancelMaze->floors)
                        {
                            const auto &current = run.floors.at(level);
                            require(current.descriptor.identity == previous.descriptor.identity
                                && current.descriptor.seeds == previous.descriptor.seeds
                                && current.descriptor.layout == previous.descriptor.layout,
                                "Cancellation changed floor identity, random seeds or layout");
                        }
                        require(questGlobals() == cancelQuestGlobals
                            && scene().party().snapshot().questBits == cancelQuestBits,
                            "Cancellation changed quest state");
                        require(!game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive
                            && !scene().eventRuntimeState()->pendingDialogueContext
                            && !scene().eventRuntimeState()->pendingMapMove
                            && !scene().eventRuntimeState()->pendingBreachMazeTransitionRequest
                            && !game.m_gameSession.pendingMapMove(), "Cancellation left pending dialogue or travel");
                        cancelVerified = true;
                        save("l3-cancelled");
                        activateUp();
                        advance(Phase::StepOffer, "l3-up-offer");
                        break;
                    }
                    case Phase::StepOffer:
                        if (game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive)
                        {
                            leavingLevel = maze().currentLevel;
                            targetLevel = leavingLevel - 1;
                            selectTransition(EventDialogActionKind::MapTransitionConfirm);
                            ++confirmations;
                            advance(Phase::StepArrival, "l" + std::to_string(leavingLevel) + "-up-arrival");
                        }
                        break;
                    case Phase::StepArrival:
                    {
                        const auto &run = maze();
                        require(run.rootSeed == initialRootSeed && run.epoch == initialEpoch,
                            "App transition changed the saved run seed or epoch");
                        if (targetLevel > 0)
                        {
                            require(run.inside && (run.currentLevel == leavingLevel || run.currentLevel == targetLevel),
                                "App arrived on an unexpected maze floor");
                            if (run.currentLevel != targetLevel) break;
                            require(normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "brbase",
                                "App floor transition left the actual maze map");
                            require(game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBasement") != 0,
                                "Committed floor transition did not apply source EnteredBasement");
                            landingCapture = "l" + std::to_string(targetLevel) + "-arrival";
                        }
                        else
                        {
                            if (run.inside) break;
                            require(run.currentLevel == 1 && run.direction == BreachMazeDirection::Exited
                                && normalizeMapFileStem(game.m_gameSession.currentMapFileName()) == "bralvar"
                                && game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                                "App maze exit did not arrive in actual BrAlvar outdoors");
                            require(game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBreach") != 0,
                                "Committed maze exit did not apply source EnteredBreach");
                            verifyExitPlatform();
                            landingCapture = "bralvar-exit";
                        }
                        require(game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.QuestFinished") == 0,
                            "Actual route unexpectedly completed the main quest");
                        save(landingCapture);
                        advance(Phase::LandingRender, landingCapture + "-render");
                        break;
                    }
                    case Phase::LandingRender:
                        if (frames - phaseStarted >= 3)
                        {
                            verifyExitPlatform();
                            requestCapture(landingCapture);
                            advance(Phase::LandingCapture, landingCapture + "-capture");
                        }
                        break;
                    case Phase::LandingCapture:
                        if (pendingCapture.empty())
                        {
                            if (entryRoute)
                            {
                                require(entryPointerVerified && freshIdentityVerified && confirmations == 1
                                    && captures == 2, "Actual fresh entry evidence is incomplete");
                                finish();
                            }
                            else if (targetLevel == 0)
                            {
                                verifyExitPlatform();
                                require(cancelVerified && confirmations == 3 && captures == 5u + interactionPasses
                                    && (!interactRoute || interactionsDone),
                                    "Actual route evidence is incomplete");
                                finish();
                            }
                            else if (interactRoute && (!trapVerified || !hiddenDecorationVerified))
                            {
                                advance(Phase::Interactions,
                                    "l" + std::to_string(targetLevel) + "-actual-interactions");
                            }
                            else
                            {
                                activateUp();
                                advance(Phase::StepOffer, "l" + std::to_string(targetLevel) + "-up-offer");
                            }
                        }
                        break;
                    case Phase::Complete:
                        break;
                    }
                },
                [&]() { game.shutdownApplication(); });
            appExitCode = app.run();
            record("application-returned");
            if (*appExitCode != 0) throw std::runtime_error("Actual route application returned a nonzero exit code");
            if (earlyExitEventObserved)
            {
                writeResult("interrupted", "Quit or window-close event received before route completion");
                return 1;
            }
            require(complete, "Application returned before the maze route completed");
            writeResult("passed", "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            const std::string message = "Route stage " + stage + " at frame " + std::to_string(frames)
                + ": " + failure.what();
            record("failed:" + std::string(failure.what()));
            writeResult("failed", message);
            std::cerr << message << '\n';
            return 1;
        }
    }

    static int run(std::filesystem::path runtime, std::filesystem::path source,
        std::filesystem::path seed, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        source = std::filesystem::canonical(source);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Use a fresh output directory");
        require(std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Output must remain on NVMe");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        const auto profile = Import::loadBreachMazePopulationProfile(source);
        require(profile.has_value(), "Original maze source profile is absent");
        BreachMazeLifecycle lifecycle(profile->mazeTemplate);
        BreachMazeState initialState = BreachMazeState::playable();
        constexpr uint64_t RootSeed = 148;
        BreachMazePreparedTransition entry = lifecycle.prepareFreshEntry(initialState,
            BreachMazeEntry::BrAlvar53, {true, false, false}, RootSeed);
        require(entry.target() != nullptr, "Fresh entry did not produce a floor");
        const BreachMazeFloorDescriptor floor = *entry.target();
        const BreachMazeArrival arrival = entry.arrival();
        const BreachMazePopulationPlan population = planBreachMazePopulation(
            profile->catalog, profile->mazeTemplate, floor);

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        std::string error;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Breach maze148 actual runtime";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned frames = 0;
        bool captured = false;
        bool complete = false;
        std::optional<int> appExitCode;
        unsigned quitEvents = 0;
        unsigned windowCloseEvents = 0;
        bool earlyExitEventObserved = false;
        bool completionQuitRequested = false;
        std::ofstream trace(output / "trace.jsonl");
        const auto record = [&](const std::string &stage)
        {
            trace << std::boolalpha << "{\"stage\":" << std::quoted(stage) << ",\"frames\":" << frames
                << ",\"captured\":" << captured << ",\"complete\":" << complete << "}\n";
            trace.flush();
        };
        const auto writeResult = [&](const std::string &status, const std::string &message)
        {
            std::ofstream result(output / "result.json");
            result << std::boolalpha << "{\"status\":" << std::quoted(status) << ",\"frames\":" << frames
                << ",\"captured\":" << captured << ",\"complete\":" << complete << ",\"app_exit_code\":";
            if (appExitCode) result << *appExitCode;
            else result << "null";
            result << ",\"quit_events\":" << quitEvents << ",\"window_close_events\":" << windowCloseEvents
                << ",\"early_exit_event_observed\":" << earlyExitEventObserved
                << ",\"completion_quit_requested\":" << completionQuitRequested
                << ",\"controlled\":true,\"natural_campaign\":false,\"cold_restore\":false";
            if (!message.empty()) result << ",\"error\":" << std::quoted(message);
            result << "}\n";
        };
        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"root_seed\":" << RootSeed
            << ",\"level\":" << floor.identity.level << ",\"actors\":" << population.actors.size()
            << ",\"items\":" << population.items.size()
            << ",\"input_save\":" << std::quoted(seed.string())
            << ",\"changes\":\"Direct prepared-map entry and source entrance pose. "
               "First 36 frames request 100 units/s toward an open neighbor via the actual movement controller. "
               "No level/HP/item/quest changes to Party. Simulation runs for 300 frames.\","
               "\"natural_campaign\":false,\"normal_merge_admission\":false,\"cold_restore\":false}\n";
        float initialX = 0;
        float initialY = 0;
        float velocityX = 0;
        float velocityY = 0;
        const uint64_t started = SDL_GetTicks();
        const auto scene = [&]() -> IndoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Indoor,
                "Prepared scene is no longer indoors");
            return *static_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer(), "Renderer initialization failed");
                    require(game.loadSessionFromPath(seed), "Read-only seed could not load");
                    game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                    game.captureCurrentSceneState();
                    game.shutdownRenderer();
                    require(game.activateWorldForMapFileName("brbase.blv"), "Maze world activation failed");
                    GameDataLoader &loader = game.m_gameDataLoader;
                    require(loader.loadMapByFileNameForHeadlessGameplay(*game.m_pAssetFileSystem, "brbase.blv"),
                        "Original base map could not load");
                    const auto &base = loader.getSelectedMap();
                    require(base && base->map.worldId == "mmmerge" && base->indoorMapData
                        && base->indoorMapDeltaData, "Complete original maze base is required");
                    MergeMonsterFamilyContext context;
                    const MergedBolsterMapEntry *pMap = loader.getMergedBolsterMapTable().findById(206);
                    require(pMap != nullptr, "Source map bolster settings missing");
                    context.map = {206, MergeBolsterMapKind::LowerToEqual, pMap->spells, pMap->summons,
                        int32_t(pMap->bolsterExtra)};
                    context.effectivePartyLevel = gameplayBolsterAveragePartyLevel(&game.ensureSessionPartyState())
                        + context.map.levelShift;
                    context.enabled = game.m_settings.bolsterMonsters;
                    const BreachMazeInstanceBuildInput input{
                        *base->indoorMapData, *base->indoorMapDeltaData, profile->mazeTemplate, floor, population,
                        profile->catalog, loader.getMonsterTable(), loader.getMonsterProjectileTable(),
                        loader.getSpellTable(), loader.getObjectTable(), loader.getItemTable(),
                        loader.getStandardItemEnchantTable(), loader.getSpecialItemEnchantTable(),
                        loader.getMergedBolsterMonsterTable(), loader.getMergedBolsterFormulaTable(), context};
                    record("building-prepared-instance");
                    BreachMazePreparedInstance prepared = buildBreachMazeInstance(input);
                    require(loader.loadPreparedIndoorMapForGameplay(*game.m_pAssetFileSystem, prepared.instance),
                        "Prepared map asset load failed");
                    game.m_gameSession.setCurrentMapFileName("brbase.blv");
                    require(game.initializeSelectedMapRuntime(true),
                        "Prepared application scene initialization failed");
                    scene().partyRuntime().teleportPartyPosition(arrival.x, arrival.y, arrival.footZ);
                    IndoorPartyRuntime::Snapshot placement = scene().partyRuntime().snapshot();
                    std::ofstream(output / "arrival-grounding.json") << "{\"source_z\":" << arrival.footZ
                        << ",\"teleport_grounded_z\":" << placement.movementState.footZ << "}\n";
                    // The source assigns Party.Z before gravity runs. The generic teleport helper
                    // grounds immediately; stage the declared source pose before committing the floor.
                    placement.movementState.x = arrival.x;
                    placement.movementState.y = arrival.y;
                    placement.movementState.footZ = arrival.footZ;
                    placement.movementState.grounded = false;
                    placement.movementState.supportFaceIndex = size_t(-1);
                    placement.movementState.fallStartZ = arrival.footZ;
                    placement.movementState.fallDistance = 0;
                    scene().partyRuntime().restoreSnapshot(placement);
                    game.m_indoorRenderer.setCameraAngles(arrival.yawRadians, arrival.pitchRadians);
                    initialX = scene().partyRuntime().partyX();
                    initialY = scene().partyRuntime().partyY();
                    const auto &endpoint = arrival.kind == BreachMazeArrivalKind::Exit
                        ? floor.layout.endpoints.exit : floor.layout.endpoints.entrance;
                    const auto &cell = profile->mazeTemplate.cells[endpoint.templateCellId - 1];
                    const auto &layoutCell = floor.layout.cellAt(cell.x, cell.y);
                    if (!layoutCell.walls[0]) velocityX = -100;
                    else if (!layoutCell.walls[1]) velocityX = 100;
                    else if (!layoutCell.walls[2]) velocityY = 100;
                    else if (!layoutCell.walls[3]) velocityY = -100;
                    BreachMazeReadyFloor ready;
                    ready.identity = floor.identity;
                    ready.runtime.scene = scene().snapshot();
                    ready.runtime.yawRadians = arrival.yawRadians;
                    ready.runtime.pitchRadians = arrival.pitchRadians;
                    ready.runtime.actorRage.resize(ready.runtime.scene.worldRuntime.mapActorAiStates.size(), 0);
                    ready.runtime.interaction = makeBreachMazeInteractionState(population);
                    ready.population = population;
                    require(lifecycle.commit(initialState, entry, true, {true, false, false}, &ready)
                        == BreachMazeCommitResult::Committed, "Prepared maze state commit failed");
                    game.m_gameSession.setBreachMazeRunData(initialState.data());
                    require(game.bindBreachMazeInteractions(), "Prepared maze native profile binding failed");
                    require(game.quickSaveToPath(output / "initialized.oysav", "148 受控迷宫初始化"),
                        "Initial generated save failed");
                    record("prepared-application-running");
                    return true;
                },
                [&](const SDL_Event &event)
                {
                    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    {
                        if (!complete) earlyExitEventObserved = true;
                        if (event.type == SDL_EVENT_QUIT)
                        {
                            ++quitEvents;
                            record("quit-event-observed");
                        }
                        else
                        {
                            ++windowCloseEvents;
                            record("window-close-event-observed");
                        }
                    }
                    game.handleSdlEvent(event);
                },
                [&](int width, int height, float wheel, float delta)
                {
                    require(SDL_GetTicks() - started < 120000, "Bounded maze run timed out");
                    ++frames;
                    if (frames <= 36)
                        scene().partyRuntime().update(velocityX, velocityY, false, false, 1.0f / 60.0f);
                    game.renderFrame(width, height, wheel, delta);
                    if (frames == 90)
                    {
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "maze");
                        record("screenshot-requested");
                    }
                    if (!captured)
                    {
                        if (auto screenshot = Engine::BgfxContext::consumeScreenshot("maze"))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(screenshot->width),
                                int(screenshot->height), SDL_PIXELFORMAT_BGRA32, screenshot->bgraPixels.data(),
                                int(screenshot->width * 4));
                            require(pSurface != nullptr, "Screenshot surface failed");
                            const bool saved = SDL_SaveBMP(pSurface, (output / "maze.bmp").string().c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Screenshot write failed");
                            captured = true;
                            record("screenshot-saved");
                        }
                    }
                    if (frames >= 300 && captured && !complete)
                    {
                        require(game.quickSaveToPath(output / "after-running.oysav", "148 受控迷宫运行"),
                            "Generated runtime save failed");
                        std::ofstream(output / "movement.json") << "{\"from\":[" << initialX << ',' << initialY
                            << "],\"to\":[" << scene().partyRuntime().partyX() << ','
                            << scene().partyRuntime().partyY() << "]}\n";
                        complete = true;
                        record("runtime-save-complete");
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        completionQuitRequested = true;
                        SDL_PushEvent(&quit);
                    }
                },
                [&]() { game.shutdownApplication(); });
            appExitCode = app.run();
            record("application-returned");
            if (*appExitCode != 0)
            {
                const std::string message = "EngineApplication returned a nonzero exit code";
                writeResult("failed", message);
                std::cerr << message << ": " << *appExitCode << '\n';
                return 1;
            }
            if (earlyExitEventObserved)
            {
                const std::string message = "Quit or window-close event received before driver completion";
                writeResult("interrupted", message);
                std::cerr << message << " (frames=" << frames << ")\n";
                return 1;
            }
            if (!complete)
            {
                const std::string message = "Application returned before completing the 300-frame run and screenshot";
                writeResult("failed", message);
                std::cerr << message << " (frames=" << frames << ")\n";
                return 1;
            }
            writeResult("passed", "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            writeResult("failed", failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc == 4 && std::string_view(argv[1]) == "--export-profile")
    {
        try
        {
            return OpenYAMM::Import::writeBreachMazeRuntimeResources(argv[2], argv[3]) ? 0 : 1;
        }
        catch (const std::exception &failure)
        {
            std::cerr << failure.what() << '\n';
            return 1;
        }
    }
    if (argc != 5)
    {
        std::cerr << "breach_maze_capture COMPLETE_RUNTIME ORIGINAL_SOURCE READONLY_SEED NEW_OUTPUT\n"
                     "breach_maze_capture --portal COMPLETE_RUNTIME INPUT_SAVE NEW_OUTPUT\n"
                     "breach_maze_capture --entry COMPLETE_RUNTIME INPUT_SAVE NEW_OUTPUT\n"
                     "breach_maze_capture --route COMPLETE_RUNTIME INPUT_SAVE NEW_OUTPUT\n"
                     "breach_maze_capture --interact-route COMPLETE_RUNTIME INPUT_SAVE NEW_OUTPUT\n";
        return 1;
    }
    try
    {
        if (std::string_view(argv[1]) == "--portal")
            return OpenYAMM::Game::GameApplicationTestAccess::runPortal(argv[2], argv[3], argv[4]);
        if (std::string_view(argv[1]) == "--entry")
            return OpenYAMM::Game::GameApplicationTestAccess::runRoute(argv[2], argv[3], argv[4], false, true);
        if (std::string_view(argv[1]) == "--interact-route")
            return OpenYAMM::Game::GameApplicationTestAccess::runRoute(argv[2], argv[3], argv[4], true);
        if (std::string_view(argv[1]) == "--route")
            return OpenYAMM::Game::GameApplicationTestAccess::runRoute(argv[2], argv[3], argv[4]);
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
