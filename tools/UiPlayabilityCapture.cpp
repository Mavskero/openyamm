#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayItemService.h"
#include "game/gameplay/GameplaySaveLoadUiSupport.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/maps/SaveGame.h"
#include "game/tables/AwardTable.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/ui/CharacterAwardsLayout.h"
#include "game/ui/CharacterSkillRows.h"
#include "game/ui/CharacterSkillsLayout.h"
#include "game/ui/DialogueTopicLayout.h"
#include "game/ui/KeyboardScreenLayout.h"
#include "game/ui/SaveSlotText.h"
#include "game/ui/Utf8Text.h"
#include "game/ui/screens/LoadGameScreen.h"
#include "game/ui/screens/NewGameScreen.h"
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
SDL_Window *window()
{
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);
    SDL_Window *result = count == 1 ? windows[0] : nullptr;
    SDL_free(windows);
    require(result != nullptr, "Expected one Mac window");
    return result;
}
void key(SDL_Scancode code)
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(window());
    event.key.scancode = code;
    event.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
    event.key.down = true;
    require(SDL_PushEvent(&event), "Key down queue failed");
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    require(SDL_PushEvent(&event), "Key up queue failed");
}
void pointer(float x, float y, int width, int height, uint8_t button, bool down)
{
    int pointWidth = 0, pointHeight = 0;
    require(SDL_GetWindowSize(window(), &pointWidth, &pointHeight), "Window points unavailable");
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window());
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = 1;
    event.button.x = x * float(pointWidth) / width;
    event.button.y = y * float(pointHeight) / height;
    require(SDL_PushEvent(&event), "Pointer queue failed");
}
void click(const GameplayResolvedHudLayoutElement &rect, int width, int height)
{
    pointer(rect.x + rect.width / 2, rect.y + rect.height / 2, width, height, SDL_BUTTON_LEFT, true);
    pointer(rect.x + rect.width / 2, rect.y + rect.height / 2, width, height, SDL_BUTTON_LEFT, false);
}
}
struct NewGameScreenTestAccess
{
    static void choose(NewGameScreen &screen) { screen.selectContinent("jadame"); }
    static MenuScreenBase::Rect skillRect(NewGameScreen &screen)
    {
        return screen.resolveLayoutRect("CharacterCreationSelectedSkill01").value();
    }
    static void checkWrapping(NewGameScreen &screen)
    {
        const std::string text = "这是创建角色界面的中文说明用于检查完整字符换行与顺序";
        std::string combined;
        for (const std::string &line : screen.wrapTextToWidth("SMALLNUM", text, 80.0f, 1.0f))
        {
            require(utf8CharacterCount(line).has_value(), "Creation wrap split a UTF8 character");
            combined += line;
        }
        require(combined == text, "Creation wrap changed text order");
    }
};
struct ApplicationCaptureTestAccess
{
    static int runKeyboard(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output,
        bool finishOnly = false)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Keyboard output must be new");
        std::filesystem::create_directories(output);
        std::filesystem::copy_file(input, output / "input.oysav");
        std::filesystem::current_path(output);
        std::string error;
        auto settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string(); settings.startInMainMenu = true; settings.preseedParty = false;
        settings.controlScheme = ControlScheme::Modern;
        settings.keyboard.restoreDefaults(ControlScheme::Modern);
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings("settings.ini", settings, error), "Keyboard settings write failed");
        std::ofstream("fixture.txt") << "203/204 affected Keyboard only. Original normal132 save copied; "
            "Modern defaults and direct overlay opening, UI delta0, SDL row clicks/key events. "
            "One declared duplicate Backward=D fixture exercises ambiguous old configuration. "
            "3screenshots; no movement/save/campaign/device acceptance. Settings persistence read from INI.\n";
        if (finishOnly)
            std::ofstream("fixture.txt", std::ios::app) << "Footer/height repair continuation only: directly seed "
                "Backward=D and Jump waiting/conflict UI to reproduce the already-observed presentation; "
                "2screenshots, no repeated swap/M-binding flow.\n";
        auto config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Keyboard203-204"; config.assetRoot = runtime.string(); config.activeWorldId = "mm7";
        config.windowWidth = 1210; config.windowHeight = 834; config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned phase = 0, frames = 0, captures = 0, ready = 6;
        bool complete = false;
        std::string pending;
        const auto started = std::chrono::steady_clock::now();
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < 3, "Keyboard capture budget exceeded");
            pending = name; bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "input.oysav"),
                        "Keyboard initial load failed");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                    require(screen().ensureGameplayLayoutsLoaded(), "Keyboard layouts unavailable");
                    require(screen().gameplayUiRuntime().uiTextTable().text(UiTextTable::Message::KeyboardForward) == "向前",
                        "Keyboard localized labels not loaded");
                    screen().openKeyboardOverlay();
                    if (finishOnly)
                    {
                        screen().mutableSettings().keyboard.setBinding(KeyboardAction::Backward, SDL_SCANCODE_D);
                        screen().keyboardScreenState().pendingAction = KeyboardAction::Jump;
                        screen().keyboardScreenState().waitingForBinding = true;
                        screen().keyboardScreenState().bindingConflict = true;
                        phase = 12;
                    }
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 600 && std::chrono::steady_clock::now() - started < std::chrono::seconds(60),
                        "Keyboard capture exceeded bound");
                    game.renderFrame(width, height, wheel, 0.0f);
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Keyboard screenshot failed");
                            SDL_DestroySurface(surface); pending.clear(); ++captures;
                        }
                    }
                    if (frames < ready || !pending.empty()) return;
                    const auto rowClick = [&](KeyboardAction action)
                    {
                        const auto layout = resolveKeyboardScreenLayout(screen(), width, height);
                        require(layout.has_value(), "Keyboard row layout unavailable");
                        const auto &definition = keyboardBindingDefinition(action);
                        require(screen().keyboardScreenState().page == definition.page, "Keyboard row on wrong page");
                        const float x = layout->valueColumnX(definition.column) + 4 * layout->rootScale;
                        const float y = layout->rowY(definition.row) + layout->rowHeight * 0.5f;
                        pointer(x, y, width, height, SDL_BUTTON_LEFT, true);
                        pointer(x, y, width, height, SDL_BUTTON_LEFT, false);
                    };
                    const auto buttonClick = [&](const char *name)
                    {
                        const auto *layout = screen().findHudLayoutElement(name);
                        require(layout != nullptr, "Keyboard button layout missing");
                        const auto rect = screen().resolveHudLayoutElement(name, width, height, layout->width, layout->height);
                        require(rect.has_value(), "Keyboard button unavailable");
                        click(*rect, width, height);
                    };
                    switch (phase)
                    {
                    case 0: rowClick(KeyboardAction::Left); phase = 1; break;
                    case 1:
                        if (!screen().keyboardScreenState().waitingForBinding) break;
                        key(SDL_SCANCODE_D); phase = 2; break;
                    case 2:
                        require(screen().settingsSnapshot().keyboard.keyboardBinding(KeyboardAction::Left) == SDL_SCANCODE_D
                            && screen().settingsSnapshot().keyboard.keyboardBinding(KeyboardAction::Right) == SDL_SCANCODE_A,
                            "Single key conflict did not swap");
                        capture("keyboard-page1-swap"); phase = 3; break;
                    case 3: rowClick(KeyboardAction::Forward); phase = 4; break;
                    case 4:
                        if (!screen().keyboardScreenState().waitingForBinding) break;
                        key(SDL_SCANCODE_M); phase = 5; break;
                    case 5:
                    {
                        const auto persisted = loadGameSettings("settings.ini", error);
                        require(persisted && persisted->keyboard.keyboardBinding(KeyboardAction::Forward) == SDL_SCANCODE_M
                            && persisted->keyboard.keyboardBinding(KeyboardAction::Left) == SDL_SCANCODE_D
                            && persisted->keyboard.keyboardBinding(KeyboardAction::Right) == SDL_SCANCODE_A,
                            "Keyboard changes did not persist");
                        key(SDL_SCANCODE_ESCAPE); phase = 6; break;
                    }
                    case 6: key(SDL_SCANCODE_ESCAPE); phase = 7; break;
                    case 7: key(SDL_SCANCODE_ESCAPE); phase = 8; break;
                    case 8:
                        require(screen().currentHudScreenState() == GameplayHudScreenState::Gameplay, "Keyboard return failed");
                        key(SDL_SCANCODE_M); phase = 9; break;
                    case 9:
                        require(game.m_gameInputSystem.frame().action(KeyboardAction::Forward).pressed
                            && !screen().journalScreenState().active, "Rebound M was consumed by map alias");
                        screen().openKeyboardOverlay(); phase = 10; break;
                    case 10:
                        screen().mutableSettings().keyboard.setBinding(KeyboardAction::Backward, SDL_SCANCODE_D);
                        rowClick(KeyboardAction::Jump); phase = 11; break;
                    case 11:
                        if (!screen().keyboardScreenState().waitingForBinding) break;
                        key(SDL_SCANCODE_D); phase = 12; break;
                    case 12:
                        require(screen().keyboardScreenState().waitingForBinding
                            && screen().keyboardScreenState().bindingConflict
                            && screen().settingsSnapshot().keyboard.keyboardBinding(KeyboardAction::Jump) == SDL_SCANCODE_X,
                            "Ambiguous duplicate configuration was overwritten");
                        capture("keyboard-duplicate-feedback"); phase = 13; break;
                    case 13: buttonClick("KeyboardDefaultButton"); phase = 14; break;
                    case 14:
                        if (game.m_gameInputSystem.frame().leftMouseButton.held) break;
                        require(!screen().keyboardScreenState().waitingForBinding
                            && screen().settingsSnapshot().keyboard.keyboardBinding(KeyboardAction::Backward) == SDL_SCANCODE_S,
                            "Keyboard defaults did not restore");
                        buttonClick("KeyboardPage2Button"); phase = 15; break;
                    case 15:
                        if (game.m_gameInputSystem.frame().leftMouseButton.held) break;
                        require(screen().keyboardScreenState().page == KeyboardBindingPage::Page2, "Keyboard page2 failed");
                        capture("keyboard-page2"); phase = 16; break;
                    case 16:
                        complete = true; phase = 17;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Keyboard capture ended early");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoints\":[203,204],\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"new_unit_cases\":0,\"device\":false,\"natural_campaign\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
    static int runMonsterTint(
        std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Tint output must be new");
        std::filesystem::create_directories(output);
        std::filesystem::copy_file(input, output / "input.oysav");
        std::filesystem::current_path(output);
        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.windowMode = WindowMode::Windowed;
        settings.tinting = false;
        require(saveGameSettings("settings.ini", settings, error), "Cannot write tint settings");
        std::ofstream("fixture.txt") << "200 actor rendering only. Original normal132 save copied; "
            "direct original 7out01 map jump and party/camera pose near one existing living colored actor. "
            "No actor/color/palette/quest edits. UI delta0 throughout; SDL Tinting checkbox; "
            "three screenshots, no save/campaign/device/new unit cases.\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Monster Tint200";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned phase = 0, frames = 0, captures = 0;
        bool complete = false;
        std::string pending;
        const auto started = std::chrono::steady_clock::now();
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < 3, "Tint capture budget exceeded");
            pending = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "input.oysav"),
                        "Tint initial load failed");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                    const MapStatsEntry *map = game.m_gameDataLoader.getMapStats().findByFileName("7out01.odm");
                    require(map != nullptr, "Original Emerald Island missing");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = map->id};
                    require(game.processPendingDebugMapJump(), "Tint map fixture failed");
                    require(game.m_pOutdoorWorldRuntime && game.m_pOutdoorPartyRuntime, "Outdoor runtime missing");
                    const auto &monsters = game.m_gameDataRepository.monsterTable();
                    bool found = false;
                    for (size_t i = 0; i < game.m_pOutdoorWorldRuntime->mapActorCount(); ++i)
                    {
                        const auto *actor = game.m_pOutdoorWorldRuntime->mapActorState(i);
                        if (!actor || actor->isDead || actor->isInvisible) continue;
                        const auto *name = monsters.findDisplayEntryById(actor->monsterId);
                        const auto *entry = name ? monsters.findByInternalName(name->pictureName) : nullptr;
                        if (!entry) entry = monsters.findById(actor->monsterId);
                        if (!entry || (entry->tintColor & 0xffffffu) == 0) continue;
                        game.m_pOutdoorPartyRuntime->setPositionForEvents(
                            actor->preciseX, actor->preciseY - 320.0f, actor->preciseZ);
                        game.m_outdoorGameView.setCameraAngles(1.57079632679f, 0.0f);
                        std::ofstream("actor-fixture.json") << "{\"actor_index\":" << i
                            << ",\"monster_id\":" << actor->monsterId << ",\"tint_argb\":" << entry->tintColor
                            << ",\"actor_pose\":[" << actor->preciseX << ',' << actor->preciseY << ',' << actor->preciseZ
                            << "],\"source_name\":" << std::quoted(entry->internalName) << "}\n";
                        found = true;
                        break;
                    }
                    require(found, "No existing visible colored actor found");
                    require(screen().ensureGameplayLayoutsLoaded(), "Tint layouts unavailable");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 900 && std::chrono::steady_clock::now() - started < std::chrono::seconds(75),
                        "Tint capture exceeded bound");
                    game.renderFrame(width, height, wheel, 0.0f);
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Tint screenshot failed");
                            SDL_DestroySurface(surface);
                            pending.clear();
                            ++captures;
                        }
                    }
                    if (frames < 6 || !pending.empty()) return;
                    switch (phase)
                    {
                    case 0:
                        require(!screen().settingsSnapshot().tinting, "Tint must start disabled");
                        capture("actor-tint-off"); phase = 1; break;
                    case 1:
                        screen().openVideoOptionsOverlay(); phase = 2; break;
                    case 2:
                    {
                        const auto *layout = screen().findHudLayoutElement("VideoOptionsTintingButton");
                        require(layout != nullptr, "Tint checkbox layout missing");
                        const auto resolved = screen().resolveHudLayoutElement(
                            "VideoOptionsTintingButton", width, height, layout->width, layout->height);
                        require(resolved.has_value(), "Tint checkbox did not resolve");
                        click(*resolved, width, height); phase = 3; break;
                    }
                    case 3:
                        if (game.m_gameInputSystem.frame().leftMouseButton.held) break;
                        require(game.m_gameInputSystem.frame().leftMouseButton.released
                            && screen().settingsSnapshot().tinting, "Tint checkbox did not enable");
                        {
                            const auto savedSettings = loadGameSettings("settings.ini", error);
                            require(savedSettings && savedSettings->tinting, "Tint did not persist");
                        }
                        phase = 4; break;
                    case 4:
                        require(game.m_framePresentation.worldPreviewTexture().has_value(), "Tint preview absent");
                        capture("actor-tint-preview"); phase = 5; break;
                    case 5: key(SDL_SCANCODE_ESCAPE); phase = 6; break;
                    case 6: key(SDL_SCANCODE_ESCAPE); phase = 7; break;
                    case 7: key(SDL_SCANCODE_ESCAPE); phase = 8; break;
                    case 8:
                        require(screen().currentHudScreenState() == GameplayHudScreenState::Gameplay,
                            "Tint return did not restore gameplay");
                        capture("actor-tint-on"); phase = 9; break;
                    case 9:
                        complete = true;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        phase = 10; break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Tint capture ended early");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoint\":200,\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"new_unit_cases\":0,\"device\":false,\"natural_campaign\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
    static int runVideoBrightness(
        std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Video output must be new");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->currentSceneKind == SceneKind::Indoor, "Expected normal132 indoor seed");
        std::filesystem::create_directories(output / "saves");
        std::filesystem::copy_file(input, output / "input.oysav");
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings("settings.ini", settings, error), "Cannot write isolated video settings");
        std::ofstream("fixture.txt") << "198 affected video flow: original normal132 MM7 indoor input copied. "
            "Direct VideoOptions opening is a UI fixture; actual SDL slider down/held/up, arrow and Escape. "
            "Repeated down maintains synthetic held state; not physical mouse/touch acceptance. UI delta0, "
            "then30normalframes and save/decode.3screenshots/<90seconds; no campaign/device/newunitcases.\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Video Brightness198";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned phase = 0, frames = 0, captures = 0, normalFrames = 0;
        bool complete = false;
        std::string pending;
        std::ofstream trace("observations.jsonl");
        const auto started = std::chrono::steady_clock::now();
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto rect = [&](const char *id, int width, int height)
        {
            const auto *layout = screen().findHudLayoutElement(id);
            require(layout != nullptr, "Missing video layout");
            const auto resolved = screen().resolveHudLayoutElement(id, width, height, layout->width, layout->height);
            require(resolved.has_value(), "Video layout did not resolve");
            return *resolved;
        };
        const auto diskGamma = [&]()
        {
            const auto saved = loadGameSettings("settings.ini", error);
            require(saved.has_value(), "Cannot read committed settings");
            return saved->displayGamma;
        };
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < 3, "Video capture budget exceeded");
            pending = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        const auto at = [](float actual, float expected) { return std::abs(actual - expected) < 0.001f; };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "input.oysav"),
                        "Video initial load failed");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                    require(screen().ensureGameplayLayoutsLoaded(), "Video layouts unavailable");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 1200 && std::chrono::steady_clock::now() - started < std::chrono::seconds(90),
                        "Video capture exceeded bound");
                    game.renderFrame(width, height, wheel, phase == 12 ? 1.0f / 60.0f : 0.0f);
                    trace << "{\"frame\":" << frames << ",\"phase\":" << phase
                        << ",\"gamma\":" << screen().settingsSnapshot().displayGamma
                        << ",\"drag\":" << screen().videoOptionsScreenState().brightnessDragActive
                        << ",\"dirty\":" << screen().videoOptionsScreenState().brightnessDirty << "}\n";
                    trace.flush();
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Video screenshot failed");
                            SDL_DestroySurface(surface);
                            pending.clear();
                            ++captures;
                        }
                    }
                    if (frames < 6 || !pending.empty()) return;
                    const auto track = rect("VideoOptionsBrightnessTrack", width, height);
                    const float y = track.y + track.height * 0.5f;
                    switch (phase)
                    {
                    case 0:
                        screen().openVideoOptionsOverlay();
                        phase = 1;
                        break;
                    case 1:
                        require(game.m_framePresentation.worldPreviewTexture().has_value(), "World preview unavailable");
                        require(at(screen().settingsSnapshot().displayGamma, 1), "Initial gamma is not neutral");
                        capture("video-neutral-preview");
                        phase = 2;
                        break;
                    case 2:
                        pointer(track.x + track.width * 0.5f, y, width, height, SDL_BUTTON_LEFT, true);
                        phase = 3;
                        break;
                    case 3:
                        require(screen().videoOptionsScreenState().brightnessDragActive, "Slider did not begin drag");
                        require(at(screen().settingsSnapshot().displayGamma, 1), "Thumb grab changed neutral gamma");
                        pointer(track.x + track.width, y, width, height, SDL_BUTTON_LEFT, true);
                        phase = 4;
                        break;
                    case 4:
                        require(game.m_gameInputSystem.frame().leftMouseButton.held
                            && !game.m_gameInputSystem.frame().leftMouseButton.pressed, "Held drag edge changed");
                        require(at(screen().settingsSnapshot().displayGamma, 2)
                            && screen().videoOptionsScreenState().brightnessDirty && at(diskGamma(), 1),
                            "Held brightness was not immediate or wrote settings prematurely");
                        pointer(track.x + track.width, y, width, height, SDL_BUTTON_LEFT, false);
                        phase = 5;
                        break;
                    case 5:
                        require(!screen().videoOptionsScreenState().brightnessDragActive
                            && !screen().videoOptionsScreenState().brightnessDirty && at(diskGamma(), 2),
                            "Release did not commit brightness");
                        capture("video-bright-preview");
                        phase = 6;
                        break;
                    case 6:
                        click(rect("VideoOptionsBrightnessLeftButton", width, height), width, height);
                        phase = 7;
                        break;
                    case 7:
                        // The production input queue delivers each mouse edge on its own frame.
                        if (game.m_gameInputSystem.frame().leftMouseButton.held) break;
                        require(game.m_gameInputSystem.frame().leftMouseButton.released, "Arrow release not delivered");
                        require(at(screen().settingsSnapshot().displayGamma, std::exp2(0.95f))
                            && at(diskGamma(), std::exp2(0.95f)), "Brightness arrow did not commit one step");
                        pointer(track.x + track.width * 0.5f, y, width, height, SDL_BUTTON_LEFT, true);
                        phase = 8;
                        break;
                    case 8:
                        require(at(screen().settingsSnapshot().displayGamma, 1)
                            && screen().videoOptionsScreenState().brightnessDirty, "Neutral track selection failed");
                        key(SDL_SCANCODE_ESCAPE);
                        pointer(track.x + track.width * 0.5f, y, width, height, SDL_BUTTON_LEFT, false);
                        phase = 9;
                        break;
                    case 9:
                        require(!screen().videoOptionsScreenState().active && at(diskGamma(), 1),
                            "Closing video options lost uncommitted brightness");
                        key(SDL_SCANCODE_ESCAPE);
                        phase = 10;
                        break;
                    case 10:
                        key(SDL_SCANCODE_ESCAPE);
                        phase = 11;
                        break;
                    case 11:
                        require(screen().currentHudScreenState() == GameplayHudScreenState::Gameplay,
                            "Video return did not restore gameplay");
                        phase = 12;
                        break;
                    case 12:
                        if (++normalFrames < 30) break;
                        capture("continued-neutral-world");
                        phase = 13;
                        break;
                    case 13:
                        require(game.quickSaveToPath(output / "continued.oysav"), "Video continued save failed");
                        require(loadGameDataFromPath(output / "continued.oysav", error).has_value(), "Save decode failed");
                        complete = true;
                        phase = 14;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Video capture ended early");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoint\":198,\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"normal_frames\":" << normalFrames
                << ",\"new_unit_cases\":0,\"device\":false,\"natural_campaign\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
    static int runJournalPartySave(
        std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Journal/party output must be new");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->currentSceneKind == SceneKind::Indoor, "Expected original normal132 indoor seed");
        std::filesystem::create_directories(output / "saves");
        std::filesystem::copy_file(input, output / "input.oysav");
        std::filesystem::create_directory(output / "blocked-save.oysav");
        std::ofstream(output / "blocked-save.oysav" / "preserved.txt") << "197 directory must survive\n";
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings("settings.ini", settings, error), "Cannot write isolated settings");
        std::ofstream fixture("fixture.txt");
        fixture << "197 affected journal/party/save flow; original normal132 MM7 indoor save copied to input.oysav. "
            "Runtime=" << runtime.string() << ". One previously locked potion note is placed only in another "
            "known indoor map's cached EventRuntimeState, not the current map. No actual travel claimed. "
            "Party0/1 learned Axe level1/Normal and20points are fixtures; original other skills retained. "
            "Member1 receives original CharacterAward IDs from the loaded award table solely to create overflow. "
            "Actual SDL down/Tab/held/up and dynamic-thumb middle/bottom drags; repeated synthetic down events "
            "maintain held input because SDL_PushEvent does not change native SDL_GetMouseState. "
            "Held continuation frames must report held=true/pressed=false. Adjacent M and keypad zoom pairs "
            "have no idle update between repetitions. UI delta0; final30normalframes and actual save/decode. "
            "Empty-slot double-click, Unicode draft, real asynchronous preview/write failure at a nonempty "
            "directory, then successful retry to the original empty slot. At most3screenshots/<90seconds. "
            "No195/196 flow replay, natural acquisition/campaign/cold/device/new unit cases.\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Journal Party Save197";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned phase = 0, frames = 0, ready = 6, captures = 0, normalFrames = 0, saveWaitFrames = 0;
        bool complete = false;
        int initialZoom = 0;
        int zoomDirection = -1;
        SDL_Scancode zoomCode = SDL_SCANCODE_KP_MINUS;
        uint32_t noteBit = 0;
        size_t awardMiddleOffset = 0, awardMaximumOffset = 0;
        float dragX = 0, dragGrab = 0;
        std::string pending, emptyLabel;
        const std::string draft = "重试草稿一九七";
        std::filesystem::path retryPath;
        GameplayResolvedHudLayoutElement skillRect{};
        std::ofstream trace("observations.jsonl");
        const auto started = std::chrono::steady_clock::now();
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto rect = [&](const char *id, int width, int height)
        {
            const auto *layout = screen().findHudLayoutElement(id);
            require(layout != nullptr, "Missing affected layout");
            const auto result = screen().resolveHudLayoutElement(id, width, height, layout->width, layout->height);
            require(result.has_value(), "Affected layout did not resolve");
            return *result;
        };
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < 3, "Journal/party capture budget exceeded");
            pending = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        const auto skillUnchanged = [&]()
        {
            for (size_t index = 0; index < 2; ++index)
            {
                const Character *member = screen().party()->member(index);
                const CharacterSkill *skill = member->findSkillByCanonicalName("Axe");
                require(skill && skill->level == 1 && member->skillPoints == 20,
                    "Cross-member held skill press spent points or upgraded a member");
            }
        };
        const auto heldPointer = [&](float x, float y, int width, int height)
        {
            pointer(x, y, width, height, SDL_BUTTON_LEFT, true);
        };
        const auto requireHeldContinuation = [&]()
        {
            const auto &button = game.m_gameInputSystem.frame().leftMouseButton;
            require(button.held && !button.pressed, "Synthetic held continuation gained or lost a button edge");
        };
        const auto resolveAxeRow = [&](int width, int height)
        {
            const auto data = buildCharacterSkillUiData(
                screen().party()->member(screen().characterScreen().sourceIndex), screen().characterInspectTable());
            require(!data.weaponRows.empty() && data.weaponRows.front().canonicalName == "Axe",
                "Fixture Axe is not the first weapon row");
            const auto font = screen().findHudFont("Lucida");
            const float natural = font ? float(std::max(1, font->fontHeight - 3)) : 11.0f;
            const auto layout = layoutCharacterSkillRows(screen(), width, height, natural,
                {data.weaponRows.size(), data.magicRows.size(), data.armorRows.size(), data.miscRows.size()});
            const auto region = rect("CharacterSkillsWeaponsListRegion", width, height);
            const auto level = rect("CharacterSkillsWeaponsLevelHeader", width, height);
            GameplayResolvedHudLayoutElement row{region.x, region.y, level.x + level.width - region.x,
                layout.rowHeight * region.scale, region.scale};
            require(layout.placeVisible(row), "Fixture Axe row is not visible");
            return row;
        };
        const auto awards = [&](int width, int height)
        {
            const auto layout = layoutCharacterAwards(screen(), screen().party()->member(1), screen().party(),
                width, height);
            require(layout && layout->thumbRect && layout->maximumScrollOffset > 1 && layout->thumbTravel > 0,
                "Original award fixture did not create a draggable overflow");
            return *layout;
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "input.oysav"),
                        "Journal/party initial load failed");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                    require(screen().ensureGameplayLayoutsLoaded(), "Affected layouts unavailable");
                    require(screen().party() && screen().party()->memberCount() >= 2, "Seed needs two party members");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 1200 && std::chrono::steady_clock::now() - started < std::chrono::seconds(90),
                        "Journal/party capture exceeded bound");
                    game.renderFrame(width, height, wheel, phase == 32 ? 1.0f / 60.0f : 0.0f);
                    trace << "{\"frame\":" << frames << ",\"phase\":" << phase
                        << ",\"journal\":" << screen().journalScreenState().active
                        << ",\"save_edit\":" << screen().saveGameScreenState().editActive
                        << ",\"award_offset\":" << screen().characterScreen().awardScrollOffset << "}\n";
                    trace.flush();
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Affected screenshot failed");
                            SDL_DestroySurface(surface);
                            pending.clear();
                            ++captures;
                        }
                    }
                    if (frames < ready || !pending.empty()) return;
                    switch (phase)
                    {
                    case 0:
                    {
                        const auto *table = screen().journalAutonoteTable();
                        require(table != nullptr, "Original note table unavailable");
                        const auto note = std::find_if(table->entries().begin(), table->entries().end(), [&](const auto &entry)
                        {
                            return entry.category == JournalAutonoteCategory::Potion
                                && !screen().isAutonoteUnlocked(entry.noteBit >> 16);
                        });
                        require(note != table->entries().end(), "Seed needs a locked original potion note");
                        noteBit = note->noteBit;
                        screen().worldRuntime()->eventRuntimeState()->variables.erase(noteBit);
                        const auto &maps = game.m_gameSession.data().mapEntries();
                        const auto map = std::find_if(maps.begin(), maps.end(), [&](const auto &entry)
                        {
                            return entry.worldId == "mm7" && entry.fileName.ends_with(".blv")
                                && entry.fileName != std::filesystem::path(source->mapFileName).filename().string();
                        });
                        require(map != maps.end(), "Another known MM7 indoor map unavailable");
                        const std::string cacheKey = map->canonicalId.empty() ? map->fileName : map->canonicalId;
                        IndoorSceneRuntime::Snapshot cached{};
                        cached.eventRuntimeState.emplace();
                        cached.eventRuntimeState->mapFileName = map->fileName;
                        cached.eventRuntimeState->variables[noteBit] = 1;
                        game.m_gameSession.storeIndoorSceneState(cacheKey, cached);
                        require(!screen().worldRuntime()->eventRuntimeState()->variables.contains(noteBit)
                            && screen().isAutonoteUnlocked(noteBit >> 16), "Cached-only note fixture not isolated");
                        fixture << "note_id=" << (noteBit >> 16) << " cached_map=" << cacheKey << " text="
                            << std::quoted(note->text) << '\n';
                        key(SDL_SCANCODE_M); phase = 1; break;
                    }
                    case 1:
                        require(screen().journalScreenState().active, "First complete M press did not open journal");
                        key(SDL_SCANCODE_M); phase = 2; break;
                    case 2:
                        require(!screen().journalScreenState().active, "Adjacent complete M press did not close journal");
                        key(SDL_SCANCODE_M); phase = 3; break;
                    case 3:
                        require(screen().journalScreenState().active, "Journal did not reopen");
                        initialZoom = screen().journalScreenState().mapZoomStep;
                        require(initialZoom == 0 || initialZoom == 2, "Indoor fixture lacks two zoom steps");
                        zoomDirection = initialZoom == 0 ? 1 : -1;
                        zoomCode = initialZoom == 0 ? SDL_SCANCODE_KP_PLUS : SDL_SCANCODE_KP_MINUS;
                        fixture << "initial_zoom=" << initialZoom << " zoom_direction=" << zoomDirection << '\n';
                        key(zoomCode); phase = 4; break;
                    case 4:
                        require(screen().journalScreenState().mapZoomStep == initialZoom + zoomDirection, "First zoom press lost");
                        key(zoomCode); phase = 5; break;
                    case 5:
                        require(screen().journalScreenState().mapZoomStep == initialZoom + zoomDirection * 2, "Adjacent zoom press lost");
                        click(rect("JournalMainViewNotes", width, height), width, height);
                        phase = 6; ready = frames + 2; break;
                    case 6:
                        require(screen().journalScreenState().view == GameplayUiController::JournalView::Notes
                            && screen().journalScreenState().notesCategory == GameplayUiController::JournalNotesCategory::Potion,
                            "Actual Notes click did not select potion notes");
                        require(!screen().worldRuntime()->eventRuntimeState()->variables.contains(noteBit)
                            && screen().isAutonoteUnlocked(noteBit >> 16), "Notes opening changed the cached-only bit");
                        capture("cached-map-potion-note");
                        key(SDL_SCANCODE_ESCAPE); phase = 7; ready = frames + 1; break;
                    case 7:
                    {
                        require(!screen().journalScreenState().active, "Journal Escape did not close");
                        for (size_t index = 0; index < 2; ++index)
                        {
                            Character &member = *screen().party()->member(index);
                            member.skills["Axe"] = {"Axe", 1, SkillMastery::Normal};
                            member.skillPoints = 20;
                        }
                        screen().party()->setActiveMemberIndex(0);
                        auto &character = screen().characterScreen();
                        character = {};
                        character.open = true;
                        character.page = GameplayUiController::CharacterPage::Skills;
                        character.source = GameplayUiController::CharacterScreenSource::Party;
                        character.sourceIndex = 0;
                        phase = 8; ready = frames + 2; break;
                    }
                    case 8:
                        skillRect = resolveAxeRow(width, height);
                        heldPointer(skillRect.x + skillRect.width / 2, skillRect.y + skillRect.height / 2, width, height);
                        phase = 9; break;
                    case 9:
                        require(game.m_gameInputSystem.frame().leftMouseButton.held, "Initial skill down was not held");
                        skillUnchanged();
                        key(SDL_SCANCODE_TAB);
                        heldPointer(skillRect.x + skillRect.width / 2, skillRect.y + skillRect.height / 2, width, height);
                        phase = 10; break;
                    case 10:
                        requireHeldContinuation();
                        require(screen().party()->activeMemberIndex() == 1
                            && screen().characterScreen().sourceIndex == 1, "Tab did not select member1");
                        skillUnchanged();
                        heldPointer(skillRect.x + skillRect.width / 2, skillRect.y + skillRect.height / 2, width, height);
                        phase = 11; break;
                    case 11:
                        requireHeldContinuation();
                        skillUnchanged();
                        pointer(skillRect.x + skillRect.width / 2, skillRect.y + skillRect.height / 2,
                            width, height, SDL_BUTTON_LEFT, false);
                        phase = 12; break;
                    case 12:
                        skillUnchanged();
                        require(!game.m_gameInputSystem.frame().leftMouseButton.held, "Skill release remained held");
                        skillRect = resolveAxeRow(width, height);
                        click(skillRect, width, height);
                        phase = 13; ready = frames + 2; break;
                    case 13:
                    {
                        const Character &first = *screen().party()->member(0);
                        const Character &second = *screen().party()->member(1);
                        require(first.skillPoints == 20 && first.findSkillByCanonicalName("Axe")->level == 1
                            && second.skillPoints == 18 && second.findSkillByCanonicalName("Axe")->level == 2,
                            "Fresh skill click did not upgrade only member1 once");
                        auto &character = screen().characterScreen();
                        character.page = GameplayUiController::CharacterPage::Awards;
                        const auto *table = screen().awardTable();
                        require(table != nullptr, "Original award table unavailable");
                        fixture << "member1_original_award_ids=";
                        size_t added = 0;
                        for (const auto &entry : table->entries())
                        {
                            if (entry.stateKind == AwardStateKind::CharacterAward
                                && screen().party()->member(1)->awards.insert(entry.stateId).second)
                            {
                                fixture << entry.stateId << ',';
                                if (++added == 40) break;
                            }
                        }
                        fixture << '\n';
                        character.awardScrollOffset = 0;
                        phase = 14; ready = frames + 2; break;
                    }
                    case 14:
                    {
                        const auto layout = awards(width, height);
                        awardMaximumOffset = layout.maximumScrollOffset;
                        dragX = layout.thumbRect->x + std::min(1.0f, layout.thumbRect->width / 2);
                        dragGrab = layout.thumbRect->height / 2;
                        heldPointer(dragX, layout.thumbRect->y + dragGrab, width, height);
                        phase = 15; break;
                    }
                    case 15:
                    {
                        require(screen().characterScreen().awardDragActive, "Actual thumb down did not start drag");
                        const auto layout = awards(width, height);
                        heldPointer(dragX, layout.thumbTrackTop + layout.thumbTravel * 0.5f + dragGrab, width, height);
                        phase = 16; break;
                    }
                    case 16:
                        requireHeldContinuation();
                        awardMiddleOffset = screen().characterScreen().awardScrollOffset;
                        require(awardMiddleOffset > 0 && awardMiddleOffset < awardMaximumOffset,
                            "Held thumb drag did not reach a middle offset");
                        pointer(dragX, awards(width, height).thumbRect->y + dragGrab,
                            width, height, SDL_BUTTON_LEFT, false);
                        phase = 17; break;
                    case 17:
                    {
                        require(!screen().characterScreen().awardDragActive, "Award release left a drag active");
                        const auto layout = awards(width, height);
                        heldPointer(dragX, layout.thumbRect->y + dragGrab, width, height);
                        phase = 18; break;
                    }
                    case 18:
                    {
                        require(screen().characterScreen().awardDragActive, "Moved thumb did not accept a new drag");
                        const auto layout = awards(width, height);
                        heldPointer(dragX, layout.thumbTrackTop + layout.thumbTravel + dragGrab, width, height);
                        phase = 19; break;
                    }
                    case 19:
                        requireHeldContinuation();
                        require(screen().characterScreen().awardScrollOffset == awardMaximumOffset,
                            "Held thumb drag did not reach final awards");
                        pointer(dragX, awards(width, height).thumbRect->y + dragGrab,
                            width, height, SDL_BUTTON_LEFT, false);
                        phase = 20; break;
                    case 20:
                        require(!screen().characterScreen().awardDragActive, "Final award release remained active");
                        fixture << "award_middle_offset=" << awardMiddleOffset
                            << " award_bottom_offset=" << screen().characterScreen().awardScrollOffset
                            << " award_maximum_offset=" << awardMaximumOffset << '\n';
                        capture("awards-dynamic-thumb-bottom");
                        screen().characterScreen() = {};
                        screen().openSaveGameOverlay();
                        phase = 21; ready = frames + 2; break;
                    case 21:
                    {
                        auto &save = screen().saveGameScreenState();
                        require(save.active && !save.slots.empty() && !save.slots[0].populated,
                            "Isolated slot0 is not empty");
                        save.selectedIndex = 0;
                        emptyLabel = screen().gameplayUiRuntime().uiTextTable().text(UiTextTable::Message::EmptySlot);
                        require(save.slots[0].fileLabel == emptyLabel, "Empty slot did not use loaded Chinese caption");
                        retryPath = save.slots[0].path;
                        save.slots[0].populated = true;
                        save.slots[0].fileLabel = "Empty";
                        const auto populated = screen().prepareSelectedSaveGameRequest();
                        require(populated && populated->saveName == "Empty", "Populated custom Empty name was discarded");
                        save.slots[0].populated = false;
                        save.slots[0].fileLabel = emptyLabel;
                        const auto unpopulated = screen().prepareSelectedSaveGameRequest();
                        require(unpopulated && !unpopulated->saveName.empty() && unpopulated->saveName != emptyLabel,
                            "Empty slot used its display caption as the save name");
                        click(rect("SaveGameSlotRow0", width, height), width, height);
                        phase = 22; ready = frames + 2; break;
                    }
                    case 22:
                        click(rect("SaveGameSlotRow0", width, height), width, height);
                        phase = 23; ready = frames + 2; break;
                    case 23:
                    {
                        auto &save = screen().saveGameScreenState();
                        require(save.editActive && save.editSlotIndex == 0 && save.editBuffer.empty(),
                            "Actual empty-slot double-click prefills the Chinese caption");
                        save.slots[0].path = output / "blocked-save.oysav";
                        SDL_Event text{};
                        text.type = SDL_EVENT_TEXT_INPUT;
                        text.text.windowID = SDL_GetWindowID(window());
                        text.text.text = draft.c_str();
                        require(SDL_PushEvent(&text), "Unicode draft queue failed");
                        key(SDL_SCANCODE_RETURN);
                        saveWaitFrames = 0;
                        phase = 24; break;
                    }
                    case 24:
                    {
                        auto &save = screen().saveGameScreenState();
                        require(++saveWaitFrames < 180, "Asynchronous save failure did not return");
                        require(save.active && save.editActive && save.editBuffer == draft
                            && save.slots[0].fileLabel == emptyLabel && !save.slots[0].populated,
                            "Failed save lost the draft or replaced original slot metadata");
                        if (save.errorText.empty()) break;
                        require(save.errorText == "保存失败：无法写入，原档未改动",
                            "Failure was not the actual asynchronous write rejection");
                        require(std::filesystem::is_directory(output / "blocked-save.oysav")
                            && std::filesystem::is_regular_file(output / "blocked-save.oysav" / "preserved.txt"),
                            "Failed save replaced the protected directory fixture");
                        fixture << "save_failure=" << std::quoted(save.errorText) << '\n';
                        capture("save-failure-retains-unicode-draft");
                        phase = 25; break;
                    }
                    case 25:
                        screen().saveGameScreenState().slots[0].path = retryPath;
                        key(SDL_SCANCODE_RETURN);
                        saveWaitFrames = 0;
                        phase = 26; break;
                    case 26:
                        require(++saveWaitFrames < 180, "Asynchronous save retry did not finish");
                        if (screen().saveGameScreenState().active) break;
                        {
                            const auto saved = loadGameDataFromPath(retryPath, error);
                            require(saved && saved->saveName == draft && !saved->previewBmp.empty(),
                                "Actual save retry did not persist Unicode name and preview");
                        }
                        phase = 31; break;
                    case 31: phase = 32; break;
                    case 32:
                        if (++normalFrames < 30) break;
                        require(game.quickSaveToPath(output / "continued.oysav"), "Continued actual save failed");
                        require(loadGameDataFromPath(output / "continued.oysav", error).has_value(),
                            "Continued save decode failed");
                        complete = true;
                        phase = 33;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Journal/party capture ended early");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoint\":197,\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"normal_frames\":" << normalFrames
                << ",\"cached_note_id\":" << (noteBit >> 16)
                << ",\"award_middle_offset\":" << awardMiddleOffset << ",\"award_bottom_offset\":" << awardMaximumOffset
                << ",\"new_unit_cases\":0,\"device\":false,\"natural_campaign\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
    static int runLabels(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output,
        bool creationOnly = false)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Label output must be new");
        std::string error;
        auto source = loadGameDataFromPath(input, error);
        require(source.has_value(), "Preview seed unavailable");
        std::filesystem::create_directories(output / "saves");
        source->saveName.clear(); source->savedGameMinutes = 336 * 1440 + 540;
        for (const char *name : {"autosave.oysav", "quicksave.oysav", "save99.oysav"})
            require(saveGameDataToPath(output / "saves" / name, *source, error), "Preview slot write failed");
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string(); settings.startInMainMenu = true; settings.preseedParty = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings("settings.ini", settings, error), "Settings write failed");
        std::ofstream("fixture.txt") << (creationOnly
            ? "196 creation-only punctuation preview; no Load screen or old label checks replayed. "
            : "195 cold-menu label preview. ") << "Original seed decoded solely for3independent "
            "preview files with empty names/year1169; no game session or new party loaded/committed. "
            "Production mainmenu Load/NewGame calls; creation Jadame selected directly, actual SDLright skill press. "
            << (creationOnly ? "1screenshot" : "2screenshots")
            << "/delta0/no gameplay/oldroute/device/newcase. Original runtime/save preserved.\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = creationOnly ? "OpenYAMM Creation Wrap196" : "OpenYAMM UI Labels195";
        config.assetRoot = runtime.string(); config.activeWorldId = "mm7";
        config.windowWidth = 1210; config.windowHeight = 834; config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned frames = 0, phase = 0, ready = 6, captures = 0;
        bool complete = false;
        std::string pending;
        const auto started = std::chrono::steady_clock::now();
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < 2, "Label capture budget exceeded");
            pending = name; bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer(), "Menu initialization failed");
                    require(game.m_pMapSceneRuntime == nullptr, "Cold-menu preview unexpectedly loaded gameplay");
                    if (creationOnly)
                    {
                        phase = 1;
                        return true;
                    }
                    game.openLoadGameScreen(false, "main_menu");
                    const UiTextTable &text = game.m_gameSession.gameplayUiRuntime().uiTextTable();
                    require(text.text(UiTextTable::Message::SaveAutosave) == "自动存档", "Cold-menu text not loaded");
                    require(formatSaveSlotTime(source->savedGameMinutes, &text).date.find("一月") != std::string::npos,
                        "Localized month missing");
                    require(friendlySaveFileLabel("save99.oysav", &text) == "存档 99", "Localized slot missing");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 300 && std::chrono::steady_clock::now() - started < std::chrono::seconds(45),
                        "Cold-menu label preview exceeded bound");
                    game.renderFrame(width, height, wheel, 0);
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Label screenshot failed");
                            SDL_DestroySurface(surface); pending.clear(); ++captures;
                        }
                    }
                    if (frames < ready || !pending.empty()) return;
                    switch (phase)
                    {
                    case 0: capture("cold-load-localized-calendar"); phase = 1; break;
                    case 1:
                        game.openNewGameScreen("main_menu");
                        NewGameScreenTestAccess::choose(*dynamic_cast<NewGameScreen *>(game.m_screenManager.activeScreen()));
                        phase = 2; ready = frames + 3; break;
                    case 2:
                    {
                        auto &screen = *dynamic_cast<NewGameScreen *>(game.m_screenManager.activeScreen());
                        const auto rect = NewGameScreenTestAccess::skillRect(screen);
                        pointer(rect.x, rect.y + 3, width, height, SDL_BUTTON_RIGHT, true);
                        phase = 3; ready = frames + 1; break;
                    }
                    case 3:
                        capture(creationOnly ? "creation-punctuation-wrap" : "creation-localized-mastery");
                        pointer(0, 0, width, height, SDL_BUTTON_RIGHT, false); phase = 4; break;
                    case 4:
                        require(game.m_pMapSceneRuntime == nullptr, "Label preview created a campaign");
                        complete = true; phase = 5;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Label preview ended early");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoint\":" << (creationOnly ? 196 : 195)
                << ",\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"gameplay_session_loaded\":false,\"new_unit_cases\":0,\"device\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
    static int runTravel(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Travel capture output must be new");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->currentSceneKind == SceneKind::Indoor, "Expected original indoor seed");
        std::filesystem::create_directories(output);
        std::filesystem::copy_file(input, output / "input.oysav");
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings("settings.ini", settings, error), "Cannot save settings");
        std::ofstream("fixture.txt") << "194 affected travel/rest flow. Original normal132 indoor save copied. "
            "Inspection-only runtime has current194 engine and preserved192 missing7d24 map fixture. "
            "Direct queued TravelDays plan (fare1/day1/food1), injured activeHP1/SP0; missing destination resource "
            "forces actual loader failure. Immediate full Party comparison, original clock/map/path and no retry. "
            "Then same-map plan commits costs once and pending-root Save104 codec is observed. "
            "Rest overlay opened with world restriction bypass for visual inspection; real 5-minute click/Escape. "
            "UI delta0, final30normalframes. No actual coach/walking map edge/MM9 callback/campaign/device/cold.\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Travel and Rest194";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned phase = 0, frames = 0, ready = 6, captures = 0, normalFrames = 0;
        bool complete = false;
        double beforeMinutes = 0, restMinutes = 0;
        int beforeGold = 0, beforeFood = 0;
        std::string pending;
        const auto started = std::chrono::steady_clock::now();
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < 4, "Travel capture budget exceeded");
            pending = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "input.oysav"),
                        "Travel initial load failed");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                    require(screen().ensureGameplayLayoutsLoaded(), "Rest layout unavailable");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 900 && std::chrono::steady_clock::now() - started < std::chrono::seconds(90),
                        "Travel capture exceeded bound");
                    game.renderFrame(width, height, wheel, phase == 7 ? 1.0f / 60.0f : 0.0f);
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Travel screenshot failed");
                            SDL_DestroySurface(surface);
                            pending.clear();
                            ++captures;
                        }
                    }
                    if (frames < ready || !pending.empty()) return;
                    switch (phase)
                    {
                    case 0:
                    {
                        Party &party = *screen().party();
                        require(party.gold() > 1 && party.food() > 1, "Seed needs original fare and provisions");
                        party.member(0)->health = 1;
                        party.member(0)->spellPoints = 0;
                        const auto beforeParty = encodePartySnapshotForComparison(party.snapshot());
                        beforeGold = party.gold(); beforeFood = party.food();
                        beforeMinutes = game.m_gameSession.activeWorldRuntime()->gameMinutes();
                        EventRuntimeState::PendingMapMove move;
                        move.mapName = "7d24.blv";
                        move.useMapStartPosition = true;
                        move.travelPlan.mode = EventRuntimeState::TravelPlanMode::TravelDays;
                        move.travelPlan.goldFare = 1;
                        move.travelPlan.travelDays = 1;
                        move.travelPlan.foodRequired = 1;
                        move.traceSourceKind = "controlled-travel-194";
                        game.m_gameSession.setPendingMapMove(move);
                        require(game.quickSaveToPath(output / "queued.oysav"), "Queued travel save failed");
                        const auto queued = loadGameDataFromPath(output / "queued.oysav", error);
                        require(queued && queued->pendingMapMove && queued->pendingMapMove->travelPlan == move.travelPlan,
                            "Queued root travel plan was not saved");
                        const auto generation = game.m_runtimeTeardownGeneration;
                        require(!game.processPendingMapMove(), "Missing map unexpectedly loaded");
                        require(game.m_runtimeTeardownGeneration > generation, "Failure did not exercise runtime reconstruction");
                        require(encodePartySnapshotForComparison(screen().party()->snapshot()) == beforeParty,
                            "Failed travel changed Party fields");
                        require(game.m_gameSession.activeWorldRuntime()->gameMinutes() == beforeMinutes
                            && game.m_gameSession.currentMapFileName() == source->mapFileName
                            && game.m_gameSession.currentSavePath() == output / "queued.oysav",
                            "Failed travel changed clock, map or save path");
                        require(!game.m_gameplayController.consumePendingMapMove(), "Failed travel remained queued");
                        require(screen().statusBarEventText() == "无法到达目的地，已恢复出发前进度。",
                            "Travel failure did not publish visible feedback");
                        phase = 1; ready = frames + 2; break;
                    }
                    case 1:
                        capture("failed-travel-restored"); phase = 2; break;
                    case 2:
                    {
                        auto *world = game.m_gameSession.activeWorldRuntime();
                        EventRuntimeState::PendingMapMove move;
                        move.x = int32_t(world->partyX()); move.y = int32_t(world->partyY());
                        move.z = int32_t(world->partyFootZ());
                        move.travelPlan.mode = EventRuntimeState::TravelPlanMode::TravelDays;
                        move.travelPlan.goldFare = 1; move.travelPlan.travelDays = 1; move.travelPlan.foodRequired = 1;
                        game.m_gameSession.setPendingMapMove(move);
                        require(game.processPendingMapMove(), "Same-map travel plan did not commit");
                        require(screen().party()->gold() == beforeGold - 1 && screen().party()->food() == beforeFood - 1
                            && world->gameMinutes() == beforeMinutes + 1440.0,
                            "Successful travel costs differ from quoted plan");
                        require(screen().party()->member(0)->health == screen().party()->member(0)->maxHealth,
                            "Successful travel did not retain rest effect");
                        require(!game.processPendingMapMove(), "Travel plan committed twice");
                        screen().openRestOverlay(false);
                        restMinutes = world->gameMinutes();
                        phase = 3; ready = frames + 3; break;
                    }
                    case 3:
                    {
                        capture("rest-complete-assets-labels");
                        const auto *element = screen().findHudLayoutElement("RestButton5Minutes");
                        require(element != nullptr, "Rest five-minute button missing");
                        click(screen().resolveHudLayoutElement(element->id, width, height,
                            element->width, element->height).value(), width, height);
                        phase = 4; ready = frames + 3; break;
                    }
                    case 4:
                        require(screen().restScreenState().mode != GameplayUiController::RestMode::None,
                            "Actual rest button did not start waiting");
                        capture("rest-hourglass-start"); key(SDL_SCANCODE_ESCAPE);
                        phase = 5; ready = frames + 3; break;
                    case 5:
                        require(!screen().restScreenState().active
                            && game.m_gameSession.activeWorldRuntime()->gameMinutes() == restMinutes + 5.0,
                            "Rest Escape did not finish selected wait");
                        capture("continued-after-travel-rest"); phase = 6; break;
                    case 6: phase = 7; break;
                    case 7:
                        if (++normalFrames < 30) break;
                        require(game.quickSaveToPath(output / "continued.oysav"), "Continued save failed");
                        require(loadGameDataFromPath(output / "continued.oysav", error).has_value(), "Continued decode failed");
                        complete = true; phase = 8;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Travel capture ended before completion");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoint\":194,\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"normal_frames\":" << normalFrames
                << ",\"new_unit_cases\":0,\"device\":false,\"natural_campaign\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
    static int run(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output,
        bool remainingOnly = false)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "Capture output must be new");
        std::string error;
        const std::optional<GameSaveData> source = loadGameDataFromPath(input, error);
        require(source && source->currentSceneKind == SceneKind::Indoor, "Expected normal132 indoor seed");
        std::filesystem::create_directories(output / "saves");
        std::filesystem::copy_file(input, output / "input.oysav");
        std::vector<std::filesystem::path> paths;
        for (int i = 1; i <= 12; ++i) paths.push_back(std::filesystem::path("saves") / ("save" + std::to_string(i) + ".oysav"));
        paths.push_back("saves/save999999999999999999999999999999999999999.oysav");
        GameSaveData slot = *source;
        slot.saveName.clear();
        slot.savedGameMinutes = 336 * 1440 + 540;
        for (const std::filesystem::path &path : paths)
            require(saveGameDataToPath(output / path, slot, error), "Cannot write isolated preview slot");
        std::sort(paths.begin(), paths.end(), compareSavePathsForDisplay);
        require(formatSaveSlotTime(slot.savedGameMinutes).date == "1 January 1169", "Save year rollover incorrect");
        require(formatSaveSlotTime(28 * 1440).date == "1 February 1168", "Save month rollover incorrect");
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings("settings.ini", settings, error), "Cannot write isolated settings");
        std::ofstream("fixture.txt") << "193 affected UI flow; remainingOnly=" << remainingOnly << "; complete192/runtime003; original normal132 indoor seed copied. "
            "13 independently encoded preview slots have empty names and year1169; never loaded as sessions. "
            "Creation screen/continent selected directly; no new party committed. Caster0 condition/recovery cleared, "
            "HP1/SP0/WaterMagic absent; added wand152(5/20) and held scrolls327,314. Real inventory target click. "
            "Sparks pending via normal held-item service; actual HUD press then release. Direct indoor HouseService252 "
            "entry (current map7d29 retained), actual RentRoom click and Escape completion. Delta0 except final30frames. "
            "Inspectedmember1 asleep potion227 paperdoll fixture; same-map transition prompt fixture. "
            "Remaining-only skips accepted load selection, creation labels and Recharge click; synchronizes relative cursor "
            "before MenuDown. No natural acquisition/map door/campaign/device/cold/replayed192 checks. No new unit cases.\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM UI Playability193";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned frames = 0, phase = remainingOnly ? 22 : 0, ready = 6, captures = 0, normalFrames = 0;
        bool complete = false;
        std::string pending;
        std::filesystem::path selected;
        uint8_t wandX = 0, wandY = 0;
        int goldBefore = 0, foodBefore = 0;
        double innStart = 0, innTarget = 0;
        GameplayResolvedHudLayoutElement menuRect{};
        const auto started = std::chrono::steady_clock::now();
        std::ofstream trace("trace.jsonl");
        const auto &screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto member = [&]() -> Character & { return *screen().party()->member(0); };
        const auto rect = [&](const char *id, int width, int height)
        {
            const auto *layout = screen().findHudLayoutElement(id);
            require(layout != nullptr, "Missing affected layout");
            return screen().resolveHudLayoutElement(id, width, height, layout->width, layout->height).value();
        };
        const auto capture = [&](const char *name)
        {
            require(pending.empty() && captures < (remainingOnly ? 5u : 8u), "Capture budget exceeded");
            pending = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name);
        };
        const auto useScroll = [&](uint32_t item)
        {
            member().recoverySecondsRemaining = 0;
            auto &held = screen().heldInventoryItem();
            held = {};
            held.active = true;
            held.item.objectDescriptionId = item;
            held.item.quantity = 1;
            screen().party()->setHeldItemForQueries(held.item);
            require(screen().itemService().tryUseHeldItemOnPartyMember(screen(), 0, false), "Scroll use rejected");
            require(!screen().heldInventoryItem().active, "Scroll not consumed");
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "input.oysav"), "Initial load failed");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal required");
                    require(screen().ensureGameplayLayoutsLoaded(), "UI layouts failed");
                    LoadGameScreen::invalidateCachedSaveSlots();
                    if (!remainingOnly) game.m_screenManager.setActiveScreen(std::make_unique<LoadGameScreen>(*game.m_pAssetFileSystem,
                        game.m_gameSession.data(), [&](const std::filesystem::path &path) { selected = path; return false; },
                        []() {}, &screen().gameplayUiRuntime().uiTextTable()));
                    if (remainingOnly)
                    {
                        game.openNewGameScreen("gameplay_menu");
                        NewGameScreenTestAccess::choose(*dynamic_cast<NewGameScreen *>(game.m_screenManager.activeScreen()));
                    }
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames < 900 && std::chrono::steady_clock::now() - started < std::chrono::seconds(90),
                        "Affected UI capture exceeded bound");
                    game.renderFrame(width, height, wheel, phase == 18 ? 1.0f / 60.0f : 0.0f);
                    trace << "{\"frame\":" << frames << ",\"phase\":" << phase << ",\"pending_spell\":"
                        << screen().pendingSpellTargetActive() << ",\"rest\":" << screen().restScreenState().active << "}\n";
                    trace.flush();
                    if (!pending.empty())
                    {
                        if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()), int(image->width * 4));
                            require(surface && SDL_SaveBMP(surface, (pending + ".bmp").c_str()), "Screenshot save failed");
                            SDL_DestroySurface(surface);
                            pending.clear();
                            ++captures;
                        }
                    }
                    if (frames < ready || !pending.empty()) return;
                    switch (phase)
                    {
                    case 0: key(SDL_SCANCODE_END); phase = 1; ready = frames + 2; break;
                    case 1: capture("load-calendar-long-number"); key(SDL_SCANCODE_RETURN); phase = 2; ready = frames + 2; break;
                    case 2:
                        require(selected == paths.back(), "End/Enter did not choose last slot");
                        key(SDL_SCANCODE_HOME); key(SDL_SCANCODE_RETURN); phase = 3; ready = frames + 2; break;
                    case 3:
                        require(selected == paths.front(), "Home/Enter did not choose first slot");
                        game.openNewGameScreen("gameplay_menu");
                        NewGameScreenTestAccess::choose(*dynamic_cast<NewGameScreen *>(game.m_screenManager.activeScreen()));
                        phase = 4; ready = frames + 3; break;
                    case 4:
                        capture("creation-localized-labels"); phase = 22; break;
                    case 22:
                    {
                        auto &creation = *dynamic_cast<NewGameScreen *>(game.m_screenManager.activeScreen());
                        NewGameScreenTestAccess::checkWrapping(creation);
                        const auto skill = NewGameScreenTestAccess::skillRect(creation);
                        pointer(skill.x, skill.y + 3, width, height, SDL_BUTTON_RIGHT, true);
                        phase = 20; ready = frames + 1; break;
                    }
                    case 20:
                        capture("creation-skill-description");
                        pointer(0, 0, width, height, SDL_BUTTON_RIGHT, false);
                        phase = 6; ready = frames + 2; break;
                    case 6:
                    {
                        game.m_screenManager.setActiveScreen(nullptr);
                        game.m_screenManager.setCurrentMode(AppMode::GameplayIndoor);
                        screen().menuScreenState().active = false;
                        member().conditions.reset(); member().health = 1; member().spellPoints = 0;
                        member().skills.erase("WaterMagic");
                        if (remainingOnly)
                        {
                            useScroll(314);
                            require(screen().pendingSpellTargetActive(), "Sparks did not arm pending target");
                            phase = 21; ready = frames + 2; break;
                        }
                        InventoryItem wand{};
                        wand.objectDescriptionId = 152; wand.quantity = 1; wand.currentCharges = 5; wand.maxCharges = 20;
                        bool placed = false;
                        for (int y = 0; y < Character::InventoryHeight && !placed; ++y)
                            for (int x = 0; x < Character::InventoryWidth && !placed; ++x)
                                if (member().addInventoryItemAt(wand, uint8_t(x), uint8_t(y)))
                                { wandX = uint8_t(x); wandY = uint8_t(y); placed = true; }
                        require(placed, "No wand fixture space");
                        useScroll(327);
                        require(screen().characterScreen().open && screen().utilitySpellOverlay().active,
                            "Recharge scroll did not retain visible target inventory");
                        phase = 7; ready = frames + 2; break;
                    }
                    case 7:
                    {
                        capture("scroll-target-visible");
                        auto target = rect("CharacterInventoryGrid", width, height);
                        target.width /= Character::InventoryWidth; target.height /= Character::InventoryHeight;
                        target.x += target.width * wandX; target.y += target.height * wandY;
                        click(target, width, height); phase = 8; ready = frames + 3; break;
                    }
                    case 8:
                    {
                        const InventoryItem *wand = member().inventoryItemAt(wandX, wandY);
                        require(wand && wand->currentCharges == 15 && wand->maxCharges == 15
                            && member().spellPoints == 0 && !member().hasSkill("WaterMagic"), "Scroll target lost cast context");
                        screen().characterScreen().open = false;
                        useScroll(314);
                        require(screen().pendingSpellTargetActive(), "Sparks did not arm pending target");
                        phase = 21; ready = frames + 2; break;
                    }
                    case 21:
                        require(!SDL_GetWindowRelativeMouseMode(window()), "Target selection cursor mode not synchronized");
                        menuRect = rect(screen().settingsSnapshot().gameplayUiLayout == GameplayUiLayout::Standard
                            ? "OutdoorStandardButtonOptions" : "OutdoorButtonOptions", width, height);
                        pointer(menuRect.x + menuRect.width / 2, menuRect.y + menuRect.height / 2,
                            width, height, SDL_BUTTON_LEFT, true);
                        phase = 9; ready = frames + 1; break;
                    case 9:
                        trace << "{\"stage\":\"hud-press\",\"pointer\":[" << game.m_gameInputSystem.frame().pointerX
                            << ',' << game.m_gameInputSystem.frame().pointerY << "],\"relative\":"
                            << SDL_GetWindowRelativeMouseMode(window()) << "}\n";
                        require(screen().pendingSpellTargetActive(), "HUD press cast the pending spell");
                        pointer(menuRect.x + menuRect.width / 2, menuRect.y + menuRect.height / 2,
                            width, height, SDL_BUTTON_LEFT, false);
                        phase = 10; ready = frames + 2; break;
                    case 10:
                        require(!screen().pendingSpellTargetActive(), "Menu release did not cancel targeting");
                        if (screen().menuScreenState().active) key(SDL_SCANCODE_ESCAPE);
                        phase = 11; ready = frames + 2; break;
                    case 11:
                        key(SDL_SCANCODE_M); phase = 12; ready = frames + 2; break;
                    case 12:
                    {
                        require(screen().journalScreenState().active, "Same-batch Map shortcut was lost");
                        key(SDL_SCANCODE_ESCAPE);
                        EventRuntimeState *events = screen().worldRuntime()->eventRuntimeState();
                        EventRuntimeState::PendingDialogueContext entry;
                        entry.kind = DialogueContextKind::HouseService; entry.sourceId = 252; entry.hostHouseId = 252;
                        events->pendingDialogueContext = entry;
                        phase = 13; ready = frames + 3; break;
                    }
                    case 13:
                    {
                        const auto topics = resolveDialogueTopicLayout(screen(), width, height);
                        require(topics && screen().activeEventDialog().isActive, "Indoor inn fixture did not open");
                        bool clicked = false;
                        goldBefore = screen().party()->gold(); foodBefore = screen().party()->food();
                        innStart = screen().worldRuntime()->gameMinutes();
                        for (const auto &row : topics->pages[topics->pageIndex].rows)
                        {
                            const auto &action = screen().activeEventDialog().actions[row.actionIndex];
                            if (action.kind == EventDialogActionKind::HouseService
                                && action.id == uint32_t(HouseActionId::TavernRentRoom) && action.enabled)
                            { click(row.rect, width, height); clicked = true; break; }
                        }
                        require(clicked, "Actual indoor RentRoom topic absent");
                        phase = 14; ready = frames + 3; break;
                    }
                    case 14:
                        require(screen().restScreenState().active && screen().restScreenState().innRest
                            && screen().party()->gold() < goldBefore && !screen().activeEventDialog().isActive,
                            "Indoor room charged without rest");
                        innTarget = screen().restScreenState().targetGameMinutes;
                        require(innTarget > innStart && screen().party()->food() == foodBefore, "Inn target or food wrong");
                        capture("indoor-inn-rest"); key(SDL_SCANCODE_ESCAPE); phase = 15; ready = frames + 2; break;
                    case 15:
                        require(!screen().restScreenState().active && screen().worldRuntime()->gameMinutes() == innTarget
                            && member().health > 1 && screen().party()->food() == foodBefore, "Inn did not heal and advance time");
                        key(SDL_SCANCODE_2); phase = 16; ready = frames + 2; break;
                    case 16:
                        require(screen().party()->activeMemberIndex() == 1, "Same-batch member shortcut was lost");
                        screen().party()->setActiveMemberIndex(0);
                        screen().party()->member(1)->conditions.set(size_t(CharacterCondition::Asleep));
                        screen().characterScreen().open = true;
                        screen().characterScreen().source = GameplayUiController::CharacterScreenSource::Party;
                        screen().characterScreen().sourceIndex = 1;
                        screen().characterScreen().page = GameplayUiController::CharacterPage::Inventory;
                        screen().heldInventoryItem() = {};
                        screen().heldInventoryItem().active = true;
                        screen().heldInventoryItem().item.objectDescriptionId = 227;
                        screen().heldInventoryItem().item.standardEnchantPower = 1;
                        screen().party()->setHeldItemForQueries(screen().heldInventoryItem().item);
                        phase = 23; ready = frames + 2; break;
                    case 23:
                        capture("sleeping-companion-doll");
                        click(rect("CharacterDollPanel", width, height), width, height);
                        phase = 24; ready = frames + 3; break;
                    case 24:
                    {
                        require(!screen().party()->member(1)->conditions.test(size_t(CharacterCondition::Asleep))
                            && !screen().heldInventoryItem().active, "Paperdoll Awaken potion rejected sleeping recipient");
                        screen().characterScreen().open = false;
                        EventRuntimeState::PendingDialogueContext entry;
                        entry.kind = DialogueContextKind::MapTransition;
                        EventRuntimeState::PendingMapMove move;
                        move.mapName = game.m_gameSession.currentMapFileName();
                        move.x = int(screen().partyX()); move.y = int(screen().partyY()); move.z = int(screen().partyFootZ());
                        move.useMapStartPosition = false;
                        entry.transitionMapMove = move;
                        screen().worldRuntime()->eventRuntimeState()->pendingDialogueContext = entry;
                        phase = 25; ready = frames + 3; break;
                    }
                    case 25:
                        require(screen().activeEventDialog().isActive
                            && screen().activeEventDialog().presentation == EventDialogPresentation::Transition,
                            "Same-map transition prompt did not open");
                        capture("transition-visible-confirm");
                        click(rect("DialogueOkButton", width, height), width, height);
                        phase = 26; ready = frames + 3; break;
                    case 26:
                        require(!screen().activeEventDialog().isActive
                            && game.m_gameSession.currentMapFileName() == source->mapFileName
                            && screen().worldRuntime()->eventRuntimeState()->lastMapTransitionConfirmed.has_value()
                            && screen().worldRuntime()->gameMinutes() == innTarget,
                            "Visible transition OK failed to confirm without extra travel time");
                        capture("continued-indoor-play"); phase = 17; break;
                    case 17: phase = 18; break;
                    case 18:
                        if (++normalFrames < 30) break;
                        require(game.quickSaveToPath(output / "continued.oysav"), "Final save failed");
                        require(loadGameDataFromPath(output / "continued.oysav", error).has_value(), "Final save decode failed");
                        complete = true;
                        { SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); }
                        phase = 19; break;
                    default: break;
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "Affected capture ended early");
            std::ofstream("result.json") << "{\"passed\":true,\"checkpoint\":193,\"frames\":" << frames
                << ",\"captures\":" << captures << ",\"normal_frames\":" << normalFrames
                << ",\"inn_start\":" << std::setprecision(17) << innStart << ",\"inn_target\":" << innTarget
                << ",\"remaining_only\":" << (remainingOnly ? "true" : "false")
                << ",\"new_unit_cases\":0,\"device\":false,\"natural_campaign\":false}\n";
            return 0;
        }
        catch (const std::exception &exception)
        {
            std::ofstream("failure.txt") << "phase=" << phase << " frame=" << frames << " " << exception.what() << '\n';
            throw;
        }
    }
};
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 4 && argc != 5) throw std::runtime_error("ui_playability_capture RUNTIME INPUT NEW_OUTPUT [--remaining-only|--travel-rest|--labels-only|--creation-only|--journal-party-save|--video-brightness|--monster-tint|--keyboard]");
        if (argc == 5 && std::string(argv[4]) == "--keyboard")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runKeyboard(argv[1], argv[2], argv[3]);
        if (argc == 5 && std::string(argv[4]) == "--keyboard-finish")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runKeyboard(argv[1], argv[2], argv[3], true);
        if (argc == 5 && std::string(argv[4]) == "--monster-tint")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runMonsterTint(argv[1], argv[2], argv[3]);
        if (argc == 5 && std::string(argv[4]) == "--video-brightness")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runVideoBrightness(argv[1], argv[2], argv[3]);
        if (argc == 5 && std::string(argv[4]) == "--journal-party-save")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runJournalPartySave(argv[1], argv[2], argv[3]);
        if (argc == 5 && std::string(argv[4]) == "--creation-only")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runLabels(argv[1], argv[2], argv[3], true);
        if (argc == 5 && std::string(argv[4]) == "--labels-only")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runLabels(argv[1], argv[2], argv[3]);
        if (argc == 5 && std::string(argv[4]) == "--travel-rest")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runTravel(argv[1], argv[2], argv[3]);
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(argv[1], argv[2], argv[3], argc == 5 && std::string(argv[4]) == "--remaining-only");
    }
    catch (const std::exception &exception) { std::cerr << exception.what() << '\n'; return 1; }
}
