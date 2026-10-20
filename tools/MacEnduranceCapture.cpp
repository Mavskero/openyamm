#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/app/GameSettings.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <mach/mach.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unistd.h>

namespace OpenYAMM::Game
{
namespace
{
void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

struct State
{
    unsigned phase = 0;
    unsigned phaseFrames = 0;
    unsigned roundtrips = 0;
    unsigned uiCycles = 0;
    unsigned uiStep = 0;
    unsigned saves = 0;
    uint64_t phaseTicks = 0;
    uint64_t nextUiTicks = 2000;
    uint64_t nextTripTicks = 5000;
    uint64_t uiTicks = 0;
    uint64_t movementTicks = 0;
    float distance = 0.0f;
    unsigned stalledFrames = 0;
    bool nearEnd = false;
    bool houseChecked = false;
    bool complete = false;
    bool patrolReverse = false;
    size_t waypoint = 0;
    std::vector<std::array<float, 2>> route;
};

bool isPhysicalInput(const SDL_Event &event)
{
    return (event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_KEYMAP_CHANGED)
        || (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
        || (event.type >= SDL_EVENT_FINGER_DOWN && event.type <= SDL_EVENT_FINGER_CANCELED);
}

uint64_t residentBytes()
{
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    require(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count)
        == KERN_SUCCESS, "Cannot measure process RSS");
    return info.resident_size;
}
}

struct ApplicationCaptureTestAccess
{
    static void action(GameSession &session, int width, int height, KeyboardAction actionId, float delta = 0.0f)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.actions[keyboardActionIndex(actionId)] = {true, true, false};
        session.updateGameplay(input, delta);
        input.actions[keyboardActionIndex(actionId)] = {false, false, true};
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
    }

    static void escape(GameSession &session, int width, int height)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
        input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
    }

    static void click(GameSession &session, int width, int height, float x, float y)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = x;
        input.pointerY = y;
        input.leftMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.leftMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
    }

    static void clickLayout(GameSession &session, int width, int height, const char *name)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto *pLayout = runtime.findHudLayoutElement(name);
        require(pLayout != nullptr, std::string("Missing UI control: ") + name);
        const auto rect = runtime.resolveHudLayoutElement(name, width, height, pLayout->width, pLayout->height);
        require(rect.has_value(), std::string("Cannot resolve UI control: ") + name);
        click(session, width, height, rect->x + rect->width * 0.5f, rect->y + rect->height * 0.5f);
    }

    static void portrait(GameSession &session, int width, int height)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto rect = runtime.resolvePartyPortraitRect(width, height, 0);
        require(rect.has_value(), "Missing actual portrait");
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = rect->x + rect->width * 0.5f;
        input.pointerY = rect->y + rect->height * 0.5f;
        input.rightMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.rightMouseButton.pressed = false;
        for (unsigned i = 0; i < 2; ++i)
        {
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        }
        input.leftMouseButton = {};
        input.rightMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
        require(runtime.characterScreen().open, "Ordinary portrait input did not open character");
    }

    static void confirm(GameplayScreenRuntime &runtime)
    {
        const auto &actions = runtime.activeEventDialog().actions;
        const auto found = std::find_if(actions.begin(), actions.end(), [](const EventDialogAction &value)
        {
            return value.kind == EventDialogActionKind::MapTransitionConfirm;
        });
        require(found != actions.end(), "Original map transition has no confirm action");
        runtime.eventDialogSelectionIndex() = size_t(found - actions.begin());
        runtime.executeActiveDialogAction();
    }

    static void aim(GameApplication &game, float x, float y, float z)
    {
        IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
        const float dx = x - pWorld->partyX(), dy = y - pWorld->partyY();
        const float yaw = std::atan2(dy, dx);
        const float pitch = std::atan2(z - pWorld->partyFootZ() - 176.0f, std::hypot(dx, dy));
        if (pWorld->isIndoorMap()) game.m_indoorRenderer.setCameraAngles(yaw, pitch);
        else game.m_outdoorGameView.setCameraAngles(yaw, pitch);
    }

    static bool move(GameApplication &game, State &state, int width, int height,
        float x, float y, uint64_t elapsedTicks)
    {
        IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
        const float oldX = pWorld->partyX(), oldY = pWorld->partyY();
        const float distance = std::hypot(x - oldX, y - oldY);
        if (distance < 18.0f) return true;
        aim(game, x, y, pWorld->partyFootZ() + 176.0f);
        action(game.m_gameSession, width, height, KeyboardAction::Forward,
            std::min(distance / 1500.0f, std::clamp(float(elapsedTicks) / 1000.0f, 0.001f, 0.025f)));
        const float moved = std::hypot(pWorld->partyX() - oldX, pWorld->partyY() - oldY);
        state.distance += moved;
        if (moved > 0.2f)
        {
            // Count actual wall time only on frames that produced physical movement; no time fixture.
            state.movementTicks += std::min<uint64_t>(elapsedTicks, 100);
            state.stalledFrames = 0;
        }
        else ++state.stalledFrames;
        require(state.stalledFrames < 180, "Endurance physical route is obstructed");
        return false;
    }

    static void loadRoute(State &state, const char *name, bool reverse = false)
    {
        std::ifstream input(std::filesystem::current_path().parent_path() / "routes" / name);
        require(input.good(), "Missing audited physical route");
        state.route.clear();
        state.waypoint = 0;
        state.stalledFrames = 0;
        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream row(line);
            float x, y, z;
            require(bool(row >> x >> y >> z), "Invalid route row");
            state.route.push_back({x, y});
        }
        require(!state.route.empty(), "Empty route");
        if (reverse) std::reverse(state.route.begin(), state.route.end());
    }

    static bool walkRoute(GameApplication &game, State &state, int width, int height, uint64_t delta)
    {
        if (state.waypoint >= state.route.size()) return true;
        if (move(game, state, width, height, state.route[state.waypoint][0],
            state.route[state.waypoint][1], delta)) ++state.waypoint;
        return state.waypoint >= state.route.size();
    }

    static void enterCastle(GameApplication &game, int width, int height)
    {
        IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
        const float yaw = std::atan2(12544.0f - pWorld->partyY(), -18368.0f - pWorld->partyX());
        for (float pitch : {0.5f, 0.25f, 0.0f, -0.35f, -0.6f})
        {
            for (float offset : {0.0f, -0.25f, 0.25f, -0.5f, 0.5f})
            {
                game.m_outdoorGameView.setCameraAngles(yaw + offset, pitch);
                const auto request = pWorld->buildWorldPickRequest({.screenX = width * 0.5f,
                    .screenY = height * 0.5f, .screenWidth = width, .screenHeight = height, .includeRay = true});
                const auto hit = pWorld->pickMouseInteractionTarget(request);
                if (!hit.eventTarget || hit.eventTarget->triggeredEventId != 301
                    || !pWorld->canActivateWorldHit(hit, GameplayInteractionMethod::Keyboard)) continue;
                action(game.m_gameSession, width, height, KeyboardAction::Use);
                return;
            }
        }
        throw std::runtime_error("Original castle entrance is not visible/reachable");
    }

    static void save(GameApplication &game, State &state, const std::string &name)
    {
        require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
            "Mac持续运行验证", {}), "Normal save failed: " + name);
        ++state.saves;
    }

    static void update(GameApplication &game, State &state, int width, int height, uint64_t ticks,
        uint64_t delta, bool pilot, std::vector<std::string> &captures)
    {
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pWorld && pParty && pParty->memberCount() == 4, "Endurance lost original party/world");
        require(std::all_of(pParty->members().begin(), pParty->members().end(),
            [](const Character &member) { return member.health > 0; }), "Normal party member died during endurance");
        const unsigned targetTrips = pilot ? 1 : 10;
        const unsigned targetUi = pilot ? 2 : 20;
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseTicks = ticks;
            state.phaseFrames = 0;
            state.stalledFrames = 0;
        };
        if (state.uiStep != 0)
        {
            if (ticks < state.uiTicks + 400) return;
            switch (state.uiStep)
            {
            case 1:
                portrait(session, width, height);
                break;
            case 2:
                clickLayout(session, width, height, "CharacterInventoryButton");
                require(runtime.characterScreen().page == GameplayUiController::CharacterPage::Inventory,
                    "Inventory tab did not open");
                break;
            case 3:
                clickLayout(session, width, height, "CharacterSkillsButton");
                require(runtime.characterScreen().page == GameplayUiController::CharacterPage::Skills,
                    "Skills tab did not open");
                if (state.uiCycles == 0) captures.push_back("skills-first-cycle");
                break;
            case 4:
                clickLayout(session, width, height, "CharacterStatsButton");
                require(runtime.characterScreen().page == GameplayUiController::CharacterPage::Stats,
                    "Stats tab did not open");
                break;
            case 5:
                clickLayout(session, width, height, "CharacterExitButton");
                require(!runtime.characterScreen().open, "Character close failed");
                action(session, width, height, KeyboardAction::History);
                require(runtime.journalScreenState().active, "Quest journal did not open");
                break;
            case 6:
                clickLayout(session, width, height, "JournalMainViewQuests");
                require(runtime.journalScreenState().view == GameplayUiController::JournalView::Quests,
                    "Quest journal tab did not open");
                if (state.uiCycles == 0) captures.push_back("quest-first-cycle");
                break;
            case 7:
                clickLayout(session, width, height, "JournalMainViewStory");
                require(runtime.journalScreenState().view == GameplayUiController::JournalView::Story,
                    "History journal tab did not open");
                if (state.uiCycles == 0) captures.push_back("history-first-cycle");
                break;
            case 8:
                clickLayout(session, width, height, "JournalMainViewNotes");
                require(runtime.journalScreenState().view == GameplayUiController::JournalView::Notes,
                    "Notes journal tab did not open");
                if (state.uiCycles == 0) captures.push_back("notes-first-cycle");
                break;
            case 9:
                escape(session, width, height);
                require(!runtime.journalScreenState().active, "Journal close failed");
                break;
            case 10:
                escape(session, width, height);
                require(runtime.menuScreenState().active, "Ordinary menu did not open");
                if (state.uiCycles == 0) captures.push_back("menu-first-cycle");
                break;
            case 11:
                escape(session, width, height);
                require(!runtime.menuScreenState().active, "Menu close failed");
                ++state.uiCycles;
                save(game, state, "ui-after-" + std::to_string(state.uiCycles));
                std::cout << "UI cycle passed: " << state.uiCycles << " wall_ms=" << ticks << std::endl;
                state.uiStep = 0;
                state.nextUiTicks = ticks + (pilot ? 15000 : 60000);
                return;
            }
            ++state.uiStep;
            state.uiTicks = ticks;
            return;
        }
        if ((state.phase == 0 || state.phase == 30) && ticks >= state.nextUiTicks
            && (state.uiCycles < targetUi || state.houseChecked))
        {
            if (runtime.activeEventDialog().isActive) escape(session, width, height);
            save(game, state, "ui-before-" + std::to_string(state.uiCycles + 1));
            state.uiStep = 1;
            state.uiTicks = ticks;
            return;
        }
        switch (state.phase)
        {
        case 0:
            require(session.currentMapFileName() == "7d29.blv", "Indoor patrol lost castle");
            if (state.roundtrips < targetTrips && ticks >= state.nextTripTicks) next(1);
            else if (state.roundtrips >= targetTrips && state.uiCycles >= targetUi) next(1);
            else if (move(game, state, width, height, -5120.0f, state.nearEnd ? -2700.0f : -2400.0f, delta))
                state.nearEnd = !state.nearEnd;
            break;
        case 1:
            if (move(game, state, width, height, -5120.0f, -2784.0f, delta)) next(2);
            break;
        case 2:
            aim(game, -5120.0f, -2880.0f, 128.0f);
            action(session, width, height, KeyboardAction::Trigger);
            next(3);
            break;
        case 3:
            if (ticks < state.phaseTicks + 500) break;
            confirm(runtime);
            next(4);
            break;
        case 4:
            if (++state.phaseFrames < 30) break;
            if (ticks < state.phaseTicks + 1000) break;
            require(session.currentMapFileName() == "7out02.odm", "Normal castle exit failed");
            if (state.roundtrips == 0) captures.push_back("first-outdoor-arrival");
            if (state.roundtrips >= targetTrips)
            {
                loadRoute(state, "02-castle-to-inn.tsv");
                next(20);
            }
            else next(5);
            break;
        case 5:
            enterCastle(game, width, height);
            next(6);
            break;
        case 6:
            if (ticks < state.phaseTicks + 500) break;
            confirm(runtime);
            next(7);
            break;
        case 7:
            if (++state.phaseFrames < 30) break;
            if (ticks < state.phaseTicks + 1000) break;
            require(session.currentMapFileName() == "7d29.blv", "Normal castle re-entry failed");
            ++state.roundtrips;
            save(game, state, "castle-roundtrip-" + std::to_string(state.roundtrips));
            if (state.roundtrips == 1 || state.roundtrips == targetTrips)
                captures.push_back("castle-roundtrip-" + std::to_string(state.roundtrips));
            std::cout << "Castle roundtrip passed: " << state.roundtrips << " wall_ms=" << ticks << std::endl;
            state.nextTripTicks = ticks + (pilot ? 10000 : 100000);
            next(0);
            break;
        case 20:
            if (runtime.activeEventDialog().isActive) escape(session, width, height);
            else if (walkRoute(game, state, width, height, delta))
            {
                loadRoute(state, "05-hospitality-to-castle.tsv", true);
                next(21);
            }
            break;
        case 21:
            if (runtime.activeEventDialog().isActive) escape(session, width, height);
            else if (walkRoute(game, state, width, height, delta)) next(22);
            break;
        case 22:
            aim(game, -5184.0f, 13120.0f, 128.0f);
            action(session, width, height, KeyboardAction::Trigger);
            next(23);
            break;
        case 23:
            if (ticks < state.phaseTicks + 3000) break;
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 240,
                "Physical inn route did not open original house240");
            require(runtime.gameplayUiRuntime().m_houseVideoPlayer.hasActiveFrame(),
                "Ordinary inn dialogue has no decoded video frame");
            captures.push_back("house-video-first");
            next(24);
            break;
        case 24:
            if (ticks < state.phaseTicks + 5000) break;
            require(runtime.gameplayUiRuntime().m_houseVideoPlayer.hasActiveFrame(), "Inn video stopped unexpectedly");
            captures.push_back("house-video-later");
            std::cout << "House video decoded: width="
                << runtime.gameplayUiRuntime().m_houseVideoPlayer.videoTextureWidth() << " height="
                << runtime.gameplayUiRuntime().m_houseVideoPlayer.videoTextureHeight() << std::endl;
            next(25);
            break;
        case 25:
            escape(session, width, height);
            if (!runtime.activeEventDialog().isActive)
            {
                next(26);
            }
            break;
        case 26:
            if (ticks < state.phaseTicks + 500) break;
            require(!runtime.gameplayUiRuntime().m_houseVideoPlayer.hasActiveFrame(),
                "Closed house retained an active video frame after normal world updates");
            require(!game.m_gameAudioSystem.isBackgroundMusicPaused(),
                "House exit did not resume ordinary background music");
            state.houseChecked = true;
            loadRoute(state, "05-hospitality-to-castle.tsv");
            next(30);
            break;
        case 30:
            if (runtime.activeEventDialog().isActive) escape(session, width, height);
            else if (walkRoute(game, state, width, height, delta))
            {
                state.patrolReverse = !state.patrolReverse;
                loadRoute(state, "05-hospitality-to-castle.tsv", state.patrolReverse);
            }
            break;
        default: throw std::runtime_error("Invalid endurance phase");
        }
        if (ticks >= (pilot ? 120000u : 1800000u) && state.movementTicks >= (pilot ? 15000u : 600000u)
            && state.roundtrips >= targetTrips && state.uiCycles >= targetUi && state.saves >= 5
            && state.houseChecked && state.phase == 30)
        {
            save(game, state, "endurance-complete");
            captures.push_back("endurance-complete");
            state.complete = true;
        }
    }

    static int run(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, bool pilot, bool reload)
    {
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        require(!std::filesystem::exists(output), "Endurance output must be a new directory");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        std::string error;
        const auto original = loadGameDataFromPath(input, error);
        require(original.has_value(), error);
        require(original->party.members.size() == 4 && (reload || original->mapFileName == "7d29.blv"),
            "Endurance requires the accepted normal cleared castle save");
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.gameplayTrace = false;
        settings.combatTrace = false;
        settings.collisionTrace = false;
        settings.assetRoot = root.string();
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Mac endurance validation";
        config.assetRoot = root.string();
        config.activeWorldId = "mm7";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;
        GameApplication game(config);
        State state;
        uint64_t start = 0, previous = 0, nextSample = 0, pendingSince = 0;
        uint64_t frames = 0;
        std::vector<float> frameTimes;
        std::set<std::string> pending;
        unsigned captured = 0;
        std::ofstream samples(output / "samples.tsv");
        samples << "wall_ms\tframes\tmap\tphase\tx\ty\tmovement_ms\tdistance\ttrips\tui_cycles\trss_bytes"
            "\taudio_clips\tpcm_bytes\ttransient_pcm_bytes\tevictions\tmusic_track\tmusic_paused"
            "\thouse_video\tframe_p50_ms\tframe_p95_ms\tframe_max_ms\n";
        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Endurance requires actual Metal");
                require(game.initializeRenderer() && game.loadSessionFromPath(input), "Actual application load failed");
                save(game, state, reload ? "cold-restored" : "endurance-input");
                if (reload)
                {
                    require(game.quickLoadFromPath(input, true), "Warm independent reload failed");
                    save(game, state, "warm-restored");
                }
                start = previous = SDL_GetTicks();
                std::cout << "Endurance start: unix=" << std::time(nullptr) << " pilot=" << pilot
                    << " reload=" << reload << " PID=" << getpid() << std::endl;
                return true;
            },
            [&](const SDL_Event &event) { if (!isPhysicalInput(event)) game.handleSdlEvent(event); },
            [&](int width, int height, float wheel, float delta)
            {
                const uint64_t now = SDL_GetTicks(), elapsed = now - start, step = now - previous;
                previous = now;
                ++frames;
                frameTimes.push_back(float(step));
                std::vector<std::string> captures;
                try
                {
                    if (!state.complete && !reload && pending.empty())
                        update(game, state, width, height, elapsed, step, pilot, captures);
                    if (reload && frames == 180)
                    {
                        save(game, state, "endurance-reload-complete");
                        captures.push_back("endurance-reload-complete");
                        state.complete = true;
                    }
                    game.renderFrame(width, height, wheel, delta);
                    if (frames == 30) captures.push_back("loaded-scene");
                    for (const std::string &name : captures)
                    {
                        require(pending.empty(), "Screenshot must be consumed before the next request");
                        pendingSince = now;
                        require(pending.insert(name).second, "Duplicate pending screenshot");
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
                    }
                    for (auto it = pending.begin(); it != pending.end();)
                    {
                        auto image = Engine::BgfxContext::consumeScreenshot(*it);
                        if (!image) { ++it; continue; }
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                            SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                        require(pSurface != nullptr, "Cannot create screenshot surface");
                        const bool written = SDL_SaveBMP(pSurface, (output / (*it + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(written, "Cannot save actual Metal screenshot");
                        ++captured;
                        it = pending.erase(it);
                    }
                    require(pending.empty() || now - pendingSince < 5000, "Screenshot readback timed out");
                    if (elapsed >= nextSample || (state.complete && pending.empty()))
                    {
                        std::sort(frameTimes.begin(), frameTimes.end());
                        const auto audio = game.m_gameAudioSystem.cacheStats();
                        auto *pWorld = game.m_gameSession.activeWorldRuntime();
                        const bool video = game.m_gameSession.gameplayScreenRuntime().gameplayUiRuntime()
                            .m_houseVideoPlayer.hasActiveFrame();
                        samples << elapsed << '\t' << frames << '\t' << game.m_gameSession.currentMapFileName()
                            << '\t' << state.phase << '\t' << pWorld->partyX() << '\t' << pWorld->partyY()
                            << '\t' << state.movementTicks << '\t' << state.distance << '\t' << state.roundtrips
                            << '\t' << state.uiCycles << '\t' << residentBytes() << '\t' << audio.clipCount
                            << '\t' << audio.sampleBytes << '\t' << audio.transientSampleBytes
                            << '\t' << audio.evictedClipCount
                            << '\t' << game.m_gameAudioSystem.currentBackgroundMusicTrack()
                            << '\t' << game.m_gameAudioSystem.isBackgroundMusicPaused() << '\t' << video
                            << '\t' << frameTimes[frameTimes.size() / 2]
                            << '\t' << frameTimes[(frameTimes.size() - 1) * 95 / 100]
                            << '\t' << frameTimes.back() << std::endl;
                        frameTimes.clear();
                        nextSample = elapsed + 30000;
                    }
                    require(elapsed < (pilot ? 300000u : reload ? 60000u : 2400000u),
                        "Endurance finite guard exceeded");
                    if (state.complete && pending.empty())
                    {
                        std::ofstream result(output / "run.json");
                        result << "{\"status\":\"passed\",\"pilot\":" << (pilot ? "true" : "false")
                            << ",\"reload\":" << (reload ? "true" : "false") << ",\"wall_ms\":" << elapsed
                            << ",\"movement_ms\":" << state.movementTicks << ",\"frames\":" << frames
                            << ",\"roundtrips\":" << state.roundtrips << ",\"ui_cycles\":" << state.uiCycles
                            << ",\"saves\":" << state.saves << ",\"captures\":" << captured << "}\n";
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        SDL_PushEvent(&quit);
                    }
                }
                catch (...)
                {
                    game.quickSaveToPath(output / "endurance-failed.oysav", "持续运行中止", {});
                    throw;
                }
            },
            [&]() { game.shutdownApplication(); },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });
        const int status = application.run();
        require(status == 0 && state.complete && pending.empty(), "Actual endurance did not complete normally");
        std::cout << "Endurance completed: unix=" << std::time(nullptr) << " frames=" << frames
            << " movement_ms=" << state.movementTicks << " trips=" << state.roundtrips
            << "; Mac Metal, dummy audio; no physical device or subjective listening acceptance." << std::endl;
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 5) throw std::runtime_error(
            "Usage: mac_endurance_capture RUNTIME INPUT_SAVE NEW_OUTPUT --pilot|--full|--reload");
        const std::string mode = argv[4];
        if (mode != "--pilot" && mode != "--full" && mode != "--reload")
            throw std::runtime_error("Invalid endurance mode");
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(argv[1], argv[2], argv[3],
            mode == "--pilot", mode == "--reload");
    }
    catch (const std::exception &error)
    {
        std::cerr << "Mac endurance failed: " << error.what() << std::endl;
        return 1;
    }
}
