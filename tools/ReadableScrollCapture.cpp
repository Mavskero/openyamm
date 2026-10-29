#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/ui/DialogueTopicLayout.h"
#include "game/ui/ReadableScrollLayout.h"
#include "game/ui/screens/LoadGameScreen.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string jsonText(std::string_view value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char character : value)
    {
        if (character == '"' || character == '\\') out << '\\' << char(character);
        else if (character < 0x20)
            out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(character);
        else out << char(character);
    }
    out << '"';
    return out.str();
}

std::vector<char> fileBytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot read preserved input: " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

SDL_Window *captureWindow()
{
    int count = 0;
    SDL_Window **pWindows = SDL_GetWindows(&count);
    SDL_Window *pWindow = pWindows != nullptr && count == 1 ? pWindows[0] : nullptr;
    SDL_free(pWindows);
    require(pWindow != nullptr, "Expected one actual capture window");
    return pWindow;
}

void queueKeyPress(SDL_Keycode key, SDL_Scancode scan)
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(captureWindow());
    event.key.key = key;
    event.key.scancode = scan;
    event.key.down = true;
    require(SDL_PushEvent(&event), "Cannot queue SDL key down");
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    require(SDL_PushEvent(&event), "Cannot queue SDL key up in the same event batch");
}

void queueMouse(float x, float y, int width, int height, uint8_t button, bool down)
{
    SDL_Window *pWindow = captureWindow();
    int pointWidth = 0, pointHeight = 0;
    require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot query actual window size");
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(pWindow);
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = 1;
    event.button.x = x * float(pointWidth) / float(width);
    event.button.y = y * float(pointHeight) / float(height);
    require(SDL_PushEvent(&event), "Cannot queue ordinary SDL pointer edge");
}

void queueClick(const GameplayResolvedHudLayoutElement &rect, int width, int height)
{
    const float x = rect.x + rect.width * 0.5f;
    const float y = rect.y + rect.height * 0.5f;
    queueMouse(x, y, width, height, SDL_BUTTON_LEFT, true);
    queueMouse(x, y, width, height, SDL_BUTTON_LEFT, false);
}

void checkCompletedInspectTouch(const GameSettings &settings)
{
    GameInputSystem input;
    const auto touch = [&](Uint32 type, SDL_FingerID id, float x, float y)
    {
        SDL_Event event{};
        event.type = type;
        event.tfinger.fingerID = id;
        event.tfinger.x = x;
        event.tfinger.y = y;
        input.handleSdlEvent(event);
    };
    const auto update = [&]()
    {
        input.updateFromEngineInput(640, 480, 0.0f, settings, false, false, false, false, true);
    };
    touch(SDL_EVENT_FINGER_DOWN, 1, 60.0f / 640.0f, 330.0f / 480.0f);
    update();
    touch(SDL_EVENT_FINGER_DOWN, 2, 200.0f / 640.0f, 200.0f / 480.0f);
    touch(SDL_EVENT_FINGER_UP, 2, 200.0f / 640.0f, 200.0f / 480.0f);
    update();
    require(input.frame().rightMouseButton.held && !input.frame().leftMouseButton.held
        && input.frame().pointerFromTouch && std::abs(input.frame().pointerX - 200.0f) < 0.1f
        && std::abs(input.frame().pointerY - 200.0f) < 0.1f,
        "Completed inspection tap did not retain its target without a left click");
    touch(SDL_EVENT_FINGER_UP, 1, 60.0f / 640.0f, 330.0f / 480.0f);
    update();
    update();
    require(!input.frame().leftMouseButton.held && !input.frame().leftMouseButton.pressed
        && !input.frame().rightMouseButton.held,
        "Released inspect gesture leaked into a queued ordinary tap");
}

bool insideViewport(const GameplayResolvedHudLayoutElement &rect, int width, int height)
{
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width)
        && std::isfinite(rect.height) && rect.width > 0.0f && rect.height > 0.0f
        && rect.x >= -0.5f && rect.y >= -0.5f
        && rect.x + rect.width <= float(width) + 0.5f
        && rect.y + rect.height <= float(height) + 0.5f;
}
}

// One consolidated UI flow. Its item/dialog/storage fixtures are recorded below.
struct ApplicationCaptureTestAccess
{
    // A separate four-image preview for the newly localized labels; no reader/save replay.
    static int previewUiText(std::filesystem::path runtime, std::filesystem::path input,
        std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(!std::filesystem::exists(output), "UI text preview output must be new");
        const std::vector<char> originalBytes = fileBytes(input);
        std::string error;
        const std::optional<GameSaveData> source = loadGameDataFromPath(input, error);
        require(source && source->party.members.size() == 4, "Expected unchanged normal132 input");
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"scope\":\"Four UI-only Metal previews on original normal132 input: HUD menu, empty save "
               "slots, empty Lloyd set page, recalled beacon duration. UI overlays opened directly; one "
               "temporary172800-second beacon inserted only for recall presentation. No cast, travel, "
               "save write or campaign progression; delta0 throughout; source file unchanged.\","
               "\"capture_limit\":4,\"wall_limit_seconds\":45,\"device\":false,\"new_unit_cases\":0}\n";
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot write isolated UI settings");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM UI Text Preview";
        config.assetRoot = runtime.string();
        config.activeWorldId = source->mapFileName.starts_with("7") ? "mm7" : "mm6";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        const std::array<const char *, 4> names{"localized-hud", "localized-empty-slots",
            "localized-beacon-set", "localized-beacon-recall"};
        unsigned frames = 0, readyFrame = 8, captured = 0;
        std::string pending;
        bool complete = false;
        const auto started = std::chrono::steady_clock::now();
        const auto elapsedMs = [&]()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
        };
        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal renderer required");
                return game.initializeRenderer() && game.loadSessionFromPath(input);
            },
            [&](const SDL_Event &event) { game.handleSdlEvent(event); },
            [&](int width, int height, float wheel, float)
            {
                require(++frames <= 240 && elapsedMs() < 45000, "UI text preview exceeded bound");
                game.renderFrame(width, height, wheel, 0.0f);
                if (complete) return;
                GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                if (!pending.empty())
                {
                    if (const auto image = Engine::BgfxContext::consumeScreenshot(pending))
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                            SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()),
                            int(image->width * 4));
                        require(pSurface != nullptr, "Cannot wrap UI text screenshot");
                        const bool saved = SDL_SaveBMP(pSurface, (output / (pending + ".bmp")).c_str());
                        SDL_DestroySurface(pSurface);
                        require(saved, "Cannot save UI text screenshot");
                        pending.clear();
                        ++captured;
                        if (captured == 1)
                        {
                            screen.openSaveGameOverlay();
                        }
                        else if (captured == 2)
                        {
                            screen.saveGameScreenState() = {};
                            screen.openUtilitySpellOverlay(GameplayUiController::UtilitySpellOverlayMode::LloydsBeacon,
                                33, 0, false);
                        }
                        else if (captured == 3)
                        {
                            Character *pCaster = game.m_pMapSceneRuntime->party().member(0);
                            require(pCaster != nullptr, "No caster for UI-only beacon fixture");
                            LloydBeacon beacon;
                            beacon.mapName = source->mapFileName;
                            beacon.locationName = "UI preview";
                            beacon.remainingSeconds = 172800.0f;
                            pCaster->lloydsBeacons[0] = std::move(beacon);
                            screen.utilitySpellOverlay().lloydRecallMode = true;
                        }
                        else
                        {
                            require(fileBytes(input) == originalBytes, "Read-only source save changed");
                            complete = true;
                            SDL_Event quit = {};
                            quit.type = SDL_EVENT_QUIT;
                            require(SDL_PushEvent(&quit), "Cannot queue preview exit");
                        }
                        readyFrame = frames + 4;
                    }
                }
                else if (frames >= readyFrame)
                {
                    require(captured < names.size(), "Too many UI screenshots");
                    pending = names[captured];
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pending.c_str());
                }
            }, [&]() { game.shutdownApplication(); });
        require(app.run() == 0 && complete && captured == names.size(), "UI text preview incomplete");
        std::ofstream(output / "result.json")
            << "{\"passed\":true,\"captures\":" << captured << ",\"frames\":" << frames
            << ",\"elapsed_ms\":" << elapsedMs()
            << ",\"visual_review\":\"required\",\"device\":false,\"campaign_replay\":false,"
               "\"save_write\":false,\"normal_frames\":0,\"new_unit_cases\":0}\n";
        return 0;
    }

    static int run(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(runtime / "engine.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm6.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm7.zip"), "Complete current runtime required");
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(runtime).generic_string().starts_with("../"),
            "Use a new NVMe output directory outside the read-only runtime");
        std::string error;
        const std::optional<GameSaveData> source = loadGameDataFromPath(input, error);
        require(source && source->party.members.size() == 4 && !source->heldInventoryItemActive
            && source->party.pendingItemDeliveries.empty(), "Expected an empty-handed normal132 seed: " + error);
        Party inputParty;
        inputParty.restoreSnapshot(source->party);
        require(!inputParty.hasItemAnywhere(2125), "The normal seed must not already own item2125");
        const std::vector<char> originalBytes = fileBytes(input);
        std::filesystem::create_directories(output / "saves");
        require(std::filesystem::copy_file(input, output / "source-input.oysav")
            && std::filesystem::copy_file(input, output / "saves/capture-input.oysav"),
            "Cannot preserve original input and the single menu slot");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":191,\"controlled\":true,\"natural_campaign\":false,\"device_acceptance\":false,"
               "\"runtime\":" << jsonText(runtime.string()) << ",\"input\":" << jsonText(input.string())
            << ",\"scope\":\"Load an unchanged normal132 MM6/MM7 save. The declared gameplay fixtures are one "
               "real-table long message scroll2125 held by the cursor plus NPC100/house617 dialogue context. "
               "No map, pose, AI, HP or game-time edits. An isolated GameInputSystem receives one completed "
               "inspection touch; this is SDL input evidence, not a physical-device check. "
               "Use the real LoadGameScreen twice with counting callbacks to observe same-batch SDL Return "
               "and Escape down/up dispatch; these callbacks do not reload a game. Open the scroll through "
               "a real SDL right hold to enter cursor mode, move the held gesture onto a portrait, "
               "release and retain the reader, click Next and Previous, "
               "then use SDL PageDown to the last page. Click Close and check the complete gesture did not "
               "reach the lower UI or move/consume the held scroll. Freeze simulation only during the UI "
               "observation, then run30 ordinary frames and explicitly save/decode. Geometry observations "
               "supplement actual Metal screenshots and do not replace visual review. An isolated directory "
               "contains a regular file named saves to observe directory and asynchronous manual-save failure "
               "feedback without changing any user data.\","
               "\"wall_limit_seconds\":60,\"frame_limit\":900,\"capture_limit\":9,\"audio\":\"dummy\","
               "\"new_unit_tests\":0,\"old_route_replays\":0,\"cold_replays\":0}\n";

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.debugConsole = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save settings: " + error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Readable Scroll191";
        config.assetRoot = runtime.string();
        config.activeWorldId = source->mapFileName.starts_with("7") ? "mm7" : "mm6";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        enum class Phase
        {
            Warmup, ReturnWarmup, ReturnCheck, EscapeWarmup, EscapeCheck, GameplayWarmup,
            CursorWarmup, Opening, Released, FirstCapture, Next, Previous, PageDown, LastCapture,
            Closing, ClosedCapture, DialogueWarmup, DialogueCapture, DialogueClosed,
            StorageWarmup, StorageCapture, StorageClosed, SaveWarmup, SaveEdit, SaveFailed,
            SaveFailureCapture, SaveFailureClosed, Continuing, MainMenuWarmup, CreditsWarmup,
            CreditsFirstCapture, CreditsNextWarmup, CreditsNextCapture, CreditsClosed, CreditsClosedCapture, Complete
        };
        Phase phase = Phase::Warmup;
        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        const auto elapsedMilliseconds = [&]()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
        };
        unsigned frames = 0, readyFrame = 0, normalFrames = 0, captures = 0;
        unsigned menuReturnCalls = 0, menuEscapeCalls = 0, pageDownPresses = 0, dialogueEscapes = 0;
        size_t expectedPage = 0, pageCount = 0;
        int drawableWidth = 0, drawableHeight = 0;
        double normalSeconds = 0.0, initialMinutes = 0.0;
        bool persistentObserved = false, nextObserved = false, previousObserved = false;
        bool closeConsumed = false, complete = false, savedVerified = false;
        bool inspectTouchChecked = false, saveErrorChecked = false, dialogueChecked = false;
        bool failedSaveVisible = false;
        std::string pendingCapture;
        std::array<float, 2> portrait{};
        Party::Snapshot baseline;
        std::vector<uint8_t> heldBytes;
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(12);
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto party = [&]() -> Party &
        {
            require(game.m_pMapSceneRuntime != nullptr
                && game.m_gameSession.currentMapFileName() == source->mapFileName,
                "The original map changed during reader observation");
            return game.m_pMapSceneRuntime->party();
        };
        const auto noLowerUi = [&]()
        {
            require(game.m_screenManager.activeScreen() == nullptr && !screen().characterScreenReadOnly().open
                && !screen().journalScreenState().active && !screen().activeEventDialog().isActive
                && !screen().itemInspectOverlayReadOnly().active && !screen().menuScreenState().active,
                "The reader gesture reached another screen or lower overlay");
        };
        const auto unchanged = [&]()
        {
            const Party::Snapshot current = party().snapshot();
            require(current.members.size() == baseline.members.size()
                && current.gold == baseline.gold && current.bankGold == baseline.bankGold
                && current.food == baseline.food && current.fineGold == baseline.fineGold
                && current.questBits == baseline.questBits && current.eventVariables == baseline.eventVariables,
                "Reader UI changed the roster, economy or quest progression");
            // Reading can change presentation state (for example portrait reactions).
            // Check the affected gameplay data without requiring all transient Party bytes to freeze.
            Party::Snapshot inventoryComparison = baseline;
            for (size_t index = 0; index < current.members.size(); ++index)
            {
                require(current.members[index].memberId == baseline.members[index].memberId
                    && current.members[index].experience == baseline.members[index].experience,
                    "Reader UI changed a stable member or experience");
                inventoryComparison.members[index].inventory = current.members[index].inventory;
                inventoryComparison.members[index].equipment = current.members[index].equipment;
                inventoryComparison.members[index].equipmentRuntime = current.members[index].equipmentRuntime;
            }
            require(encodePartySnapshotForComparison(inventoryComparison)
                == encodePartySnapshotForComparison(baseline), "Reader UI changed inventory or equipped items");
            require(screen().heldInventoryItem().active
                && encodeInventoryItemForComparison(screen().heldInventoryItem().item) == heldBytes
                && party().inventoryItemCount(2125) == 0,
                "Reader UI consumed, stored or replaced held item2125");
            require(game.m_gameSession.activeWorldRuntime()->gameMinutes() == initialMinutes,
                "Reader-only frames advanced the original game clock");
        };
        const auto layout = [&](int width, int height)
        {
            const std::optional<ReadableScrollLayout> value = resolveReadableScrollLayout(screen(), width, height);
            require(value && value->pageCount > 1 && value->pageCount <= 32 && value->linesPerPage > 0
                && !value->bodyLines.empty(), "Original item2125 does not resolve to bounded multiple pages");
            for (const GameplayResolvedHudLayoutElement *pRect : {&value->rootRect, &value->titleRect,
                    &value->bodyRect, &value->previousButton, &value->nextButton, &value->closeButton})
                require(insideViewport(*pRect, width, height), "A readable-scroll region exceeds the actual viewport");
            const size_t page = screen().readableScrollOverlayReadOnly().pageIndex;
            require(page < value->pageCount && std::isfinite(value->lineHeight) && value->lineHeight > 0.0f,
                "Reader page or font metrics are invalid");
            const size_t first = page * value->linesPerPage;
            require(first < value->bodyLines.size(), "Reader page starts beyond the original body text");
            const size_t lines = std::min(value->linesPerPage, value->bodyLines.size() - first);
            require(lines > 0
                && float(lines) * value->lineHeight <= value->bodyRect.height + 0.5f,
                "Visible body lines exceed their measured text region");
            require(float(value->titleLines.size()) * value->titleLineHeight <= value->titleRect.height + 0.5f,
                "Visible title lines exceed their measured text region");
            return *value;
        };
        const auto recordPage = [&](const char *pStage, int width, int height)
        {
            const ReadableScrollLayout value = layout(width, height);
            const size_t page = screen().readableScrollOverlayReadOnly().pageIndex;
            const size_t first = page * value.linesPerPage;
            const size_t end = std::min(first + value.linesPerPage, value.bodyLines.size());
            trace << "{\"stage\":" << jsonText(pStage) << ",\"frame\":" << frames
                << ",\"page\":" << page << ",\"page_count\":" << value.pageCount
                << ",\"viewport\":[" << width << ',' << height << "],\"body_rect\":[" << value.bodyRect.x
                << ',' << value.bodyRect.y << ',' << value.bodyRect.width << ',' << value.bodyRect.height
                << "],\"line_height\":" << value.lineHeight << ",\"first_line\":"
                << jsonText(value.bodyLines[first]) << ",\"last_line\":" << jsonText(value.bodyLines[end - 1])
                << ",\"lines\":" << end - first << ",\"geometry_inside_viewport\":true,"
                   "\"visual_review_required\":true}\n";
            trace.flush();
        };
        const auto capture = [&](const char *pName)
        {
            require(pendingCapture.empty() && captures < 9, "Overlapping or excessive screenshots");
            pendingCapture = pName;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pName);
        };
        const auto openMenu = [&]()
        {
            game.m_screenManager.setActiveScreen(std::make_unique<LoadGameScreen>(
                *game.m_pAssetFileSystem, game.m_gameSession.data(),
                [&](const std::filesystem::path &path)
                {
                    require(std::filesystem::canonical(path) == output / "saves/capture-input.oysav",
                        "The real load screen selected an unexpected save");
                    ++menuReturnCalls;
                    return true;
                }, [&]() { ++menuEscapeCalls; }));
        };
        const auto report = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(12)
                << "{\"passed\":" << passed << ",\"frames\":" << frames << ",\"phase\":" << int(phase)
                << ",\"elapsed_ms\":" << elapsedMilliseconds()
                << ",\"drawable\":[" << drawableWidth << ',' << drawableHeight << ']'
                << ",\"menu_same_batch_return_callbacks\":" << menuReturnCalls
                << ",\"menu_same_batch_escape_callbacks\":" << menuEscapeCalls
                << ",\"menu_callbacks_reloaded_game\":false,\"reader_persists_after_release\":" << persistentObserved
                << ",\"pointer_next\":" << nextObserved << ",\"pointer_previous\":" << previousObserved
                << ",\"page_down_presses\":" << pageDownPresses << ",\"page_count\":" << pageCount
                << ",\"close_gesture_consumed\":" << closeConsumed << ",\"captures\":" << captures
                << ",\"normal_frames\":" << normalFrames << ",\"normal_seconds\":" << normalSeconds
                << ",\"final_save_verified\":" << savedVerified
                << ",\"completed_inspect_touch_checked\":" << inspectTouchChecked
                << ",\"save_directory_error_checked\":" << saveErrorChecked
                << ",\"failed_save_visible\":" << failedSaveVisible
                << ",\"dialogue_topic_geometry_checked\":" << dialogueChecked
                << ",\"visual_review\":\"required\",\"natural_campaign\":false,\"device_acceptance\":false,"
                   "\"new_unit_tests\":0,\"old_route_replays\":0,\"cold_replays\":0,\"failure\":"
                << jsonText(failure) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "source-input.oysav"),
                        "Cannot load the unchanged normal input copy");
                    checkCompletedInspectTouch(settings);
                    inspectTouchChecked = true;
                    std::filesystem::create_directories(output / "bad-storage");
                    std::ofstream(output / "bad-storage/saves") << "preserved-nondirectory";
                    std::string saveError;
                    require(!saveGameDataToPath(output / "bad-storage/saves/rejected.oysav", *source, saveError)
                        && !saveError.empty(), "Bad save directory did not return a normal failure");
                    LoadGameScreen::invalidateCachedSaveSlots();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 900 && elapsedMilliseconds() < 60000,
                        "Readable-scroll capture exceeded its finite bound");
                    drawableWidth = width;
                    drawableHeight = height;
                    const bool normal = phase == Phase::Continuing;
                    game.renderFrame(width, height, wheel, normal ? delta : 0.0f);
                    if (normal) { ++normalFrames; normalSeconds += delta; }
                    if (!pendingCapture.empty())
                    {
                        if (std::optional<Engine::BgfxContext::ScreenshotCapture> image =
                                Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot wrap actual Metal pixels");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save actual Metal screenshot");
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        openMenu();
                        phase = Phase::ReturnWarmup;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::ReturnWarmup && frames >= readyFrame)
                    {
                        queueKeyPress(SDLK_RETURN, SDL_SCANCODE_RETURN);
                        phase = Phase::ReturnCheck;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::ReturnCheck && frames >= readyFrame)
                    {
                        require(menuReturnCalls == 1 && menuEscapeCalls == 0, "Same-batch Return was lost/repeated");
                        openMenu();
                        phase = Phase::EscapeWarmup;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::EscapeWarmup && frames >= readyFrame)
                    {
                        queueKeyPress(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
                        phase = Phase::EscapeCheck;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::EscapeCheck && frames >= readyFrame)
                    {
                        require(menuEscapeCalls == 1 && menuReturnCalls == 1, "Same-batch Escape was lost/repeated");
                        trace << "{\"stage\":\"actual-load-screen-short-keys\",\"return_callbacks\":1,"
                            "\"escape_callbacks\":1,\"game_reload\":false}\n";
                        game.m_screenManager.setActiveScreen(nullptr);
                        game.m_screenManager.setCurrentMode(source->currentSceneKind == SceneKind::Indoor
                            ? AppMode::GameplayIndoor : AppMode::GameplayOutdoor);
                        phase = Phase::GameplayWarmup;
                        readyFrame = frames + 4;
                    }
                    else if (phase == Phase::GameplayWarmup && frames >= readyFrame)
                    {
                        noLowerUi();
                        require(!screen().heldInventoryItem().active && screen().itemTable()->get(2125) != nullptr
                            && screen().readableScrollTable()->get(2125) != nullptr,
                            "Original scroll2125 or empty cursor is absent");
                        screen().heldInventoryItem() = {};
                        screen().heldInventoryItem().active = true;
                        screen().heldInventoryItem().item = ItemGenerator::makeInventoryItem(
                            2125, *screen().itemTable(), ItemGenerationMode::Generic);
                        // Normal item acquisition also publishes cursor ownership before a frame.
                        party().setHeldItemForQueries(screen().heldInventoryItem().item);
                        baseline = party().snapshot();
                        heldBytes = encodeInventoryItemForComparison(screen().heldInventoryItem().item);
                        initialMinutes = game.m_gameSession.activeWorldRuntime()->gameMinutes();
                        const std::optional<GameplayScreenRuntime::ResolvedHudLayoutElement> rect =
                            screen().resolvePartyPortraitRect(width, height, 0);
                        require(rect.has_value(), "The original first-member portrait is absent");
                        portrait = {rect->x + rect->width * 0.5f, rect->y + rect->height * 0.5f};
                        // Modern controls first release relative mouse capture. SDL_PushEvent
                        // does not update the host button state, so continue that hold next frame.
                        queueMouse(5.0f, 5.0f, width, height, SDL_BUTTON_RIGHT, true);
                        phase = Phase::CursorWarmup;
                        readyFrame = frames + 1;
                    }
                    else if (phase == Phase::CursorWarmup && frames >= readyFrame)
                    {
                        queueMouse(portrait[0], portrait[1], width, height, SDL_BUTTON_RIGHT, true);
                        phase = Phase::Opening;
                        readyFrame = frames + 1;
                    }
                    else if (phase == Phase::Opening && frames >= readyFrame)
                    {
                        const GameplayInputFrame &inputFrame = game.m_gameInputSystem.frame();
                        trace << "{\"stage\":\"reader-open-input\",\"right_held\":"
                            << inputFrame.rightMouseButton.held << ",\"pointer\":["
                            << inputFrame.pointerX << ',' << inputFrame.pointerY
                            << "],\"portrait\":[" << portrait[0] << ',' << portrait[1]
                            << "],\"reader_active\":" << screen().readableScrollOverlayReadOnly().active << "}\n";
                        trace.flush();
                        require(screen().readableScrollOverlayReadOnly().active,
                            "Real right portrait input did not open the scroll");
                        queueMouse(portrait[0], portrait[1], width, height, SDL_BUTTON_RIGHT, false);
                        phase = Phase::Released;
                        readyFrame = frames + 4;
                    }
                    else if (phase == Phase::Released && frames >= readyFrame)
                    {
                        const auto &overlay = screen().readableScrollOverlayReadOnly();
                        require(overlay.active && overlay.persistent && overlay.pageIndex == 0
                            && !game.m_gameInputSystem.frame().rightMouseButton.held
                            && overlay.body == screen().readableScrollTable()->get(2125)->text,
                            "Long reader did not persist on page0 after real pointer release");
                        unchanged();
                        noLowerUi();
                        pageCount = layout(width, height).pageCount;
                        persistentObserved = true;
                        recordPage("first-page-after-release", width, height);
                        capture("scroll-first-page");
                        phase = Phase::FirstCapture;
                    }
                    else if (phase == Phase::FirstCapture && pendingCapture.empty())
                    {
                        queueClick(layout(width, height).nextButton, width, height);
                        phase = Phase::Next;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::Next && frames >= readyFrame)
                    {
                        require(screen().readableScrollOverlayReadOnly().pageIndex == 1,
                            "Real Next button did not advance exactly one page");
                        nextObserved = true;
                        queueClick(layout(width, height).previousButton, width, height);
                        phase = Phase::Previous;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::Previous && frames >= readyFrame)
                    {
                        require(screen().readableScrollOverlayReadOnly().pageIndex == 0,
                            "Real Previous button did not return exactly to page0");
                        previousObserved = true;
                        expectedPage = 1;
                        queueKeyPress(SDLK_PAGEDOWN, SDL_SCANCODE_PAGEDOWN);
                        ++pageDownPresses;
                        phase = Phase::PageDown;
                        readyFrame = frames + 2;
                    }
                    else if (phase == Phase::PageDown && frames >= readyFrame)
                    {
                        require(screen().readableScrollOverlayReadOnly().pageIndex == expectedPage,
                            "SDL PageDown did not advance exactly one page");
                        if (expectedPage + 1 < pageCount)
                        {
                            ++expectedPage;
                            queueKeyPress(SDLK_PAGEDOWN, SDL_SCANCODE_PAGEDOWN);
                            ++pageDownPresses;
                            readyFrame = frames + 2;
                        }
                        else
                        {
                            unchanged();
                            noLowerUi();
                            recordPage("last-page", width, height);
                            capture("scroll-last-page");
                            phase = Phase::LastCapture;
                        }
                    }
                    else if (phase == Phase::LastCapture && pendingCapture.empty())
                    {
                        queueClick(layout(width, height).closeButton, width, height);
                        phase = Phase::Closing;
                        readyFrame = frames + 4;
                    }
                    else if (phase == Phase::Closing && frames >= readyFrame)
                    {
                        require(!screen().readableScrollOverlayReadOnly().active
                            && !game.m_gameInputSystem.frame().leftMouseButton.held,
                            "Complete real Close gesture did not dismiss the reader");
                        unchanged();
                        noLowerUi();
                        closeConsumed = true;
                        capture("scroll-closed-gameplay");
                        phase = Phase::ClosedCapture;
                    }
                    else if (phase == Phase::ClosedCapture && pendingCapture.empty())
                    {
                        EventRuntimeState::PendingDialogueContext dialogue;
                        dialogue.kind = DialogueContextKind::NpcTalk;
                        dialogue.sourceId = 100;
                        dialogue.hostHouseId = 617;
                        dialogue.npcEnterPending = true;
                        game.m_pMapSceneRuntime->eventRuntimeState()->pendingDialogueContext = dialogue;
                        phase = Phase::DialogueWarmup;
                        readyFrame = frames + 4;
                    }
                    else if (phase == Phase::DialogueWarmup && frames >= readyFrame)
                    {
                        require(screen().activeEventDialog().isActive, "NPC100 dialogue fixture did not open");
                        const std::optional<DialogueTopicLayout> topics =
                            resolveDialogueTopicLayout(screen(), width, height);
                        require(topics && !topics->pages.empty() && topics->pageIndex < topics->pages.size(),
                            "Original dialogue did not resolve to visible topic rows");
                        bool foundLongTopic = false;
                        for (const DialogueTopicRow &row : topics->pages[topics->pageIndex].rows)
                        {
                            require(insideViewport(row.rect, width, height), "A topic row exceeds the actual viewport");
                            require(row.actionIndex < screen().activeEventDialog().actions.size(),
                                "A visible topic row lost its original action");
                            const EventDialogAction &action = screen().activeEventDialog().actions[row.actionIndex];
                            if (action.kind == EventDialogActionKind::NpcTopic && action.id == 256)
                            {
                                require(row.text.lines.size() > 1 && row.text.lineHeight > 0.0f
                                    && row.text.height <= row.textRect.height + 0.5f,
                                    "Original long Chinese topic has invalid text bounds");
                                foundLongTopic = true;
                                trace << "{\"stage\":\"original-npc100-topic256\",\"page\":" << topics->pageIndex
                                    << ",\"page_count\":" << topics->pages.size() << ",\"lines\":"
                                    << row.text.lines.size() << ",\"line_height\":" << row.text.lineHeight
                                    << ",\"row_height\":" << row.rect.height
                                    << ",\"visual_review_required\":true}\n";
                                trace.flush();
                            }
                        }
                        require(foundLongTopic, "Original NPC100 topic256 is not on the observed page");
                        dialogueChecked = true;
                        capture("dialogue-long-chinese-topic");
                        phase = Phase::DialogueCapture;
                    }
                    else if (phase == Phase::DialogueCapture && pendingCapture.empty())
                    {
                        queueKeyPress(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
                        ++dialogueEscapes;
                        phase = Phase::DialogueClosed;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::DialogueClosed && frames >= readyFrame)
                    {
                        trace << "{\"stage\":\"dialogue-escape\",\"presses\":" << dialogueEscapes
                            << ",\"active\":" << screen().activeEventDialog().isActive
                            << ",\"title\":" << jsonText(screen().activeEventDialog().title)
                            << ",\"video\":" << jsonText(screen().activeEventDialog().videoName) << "}\n";
                        trace.flush();
                        if (screen().activeEventDialog().isActive)
                        {
                            // Escape first stops a house movie and can then return to resident selection.
                            require(dialogueEscapes < 3, "Three short Escapes did not leave the house dialogue");
                            queueKeyPress(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
                            ++dialogueEscapes;
                            readyFrame = frames + 3;
                            return;
                        }
                        std::filesystem::current_path(output / "bad-storage");
                        LoadGameScreen::invalidateCachedSaveSlots();
                        openMenu();
                        phase = Phase::StorageWarmup;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::StorageWarmup && frames >= readyFrame)
                    {
                        capture("load-directory-error");
                        phase = Phase::StorageCapture;
                    }
                    else if (phase == Phase::StorageCapture && pendingCapture.empty())
                    {
                        queueKeyPress(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
                        phase = Phase::StorageClosed;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::StorageClosed && frames >= readyFrame)
                    {
                        require(menuEscapeCalls == 2 && menuReturnCalls == 1,
                            "Directory error screen did not retain its short Escape cancellation");
                        const std::vector<char> marker = fileBytes(output / "bad-storage/saves");
                        require(std::string(marker.begin(), marker.end()) == "preserved-nondirectory",
                            "Storage error handling modified the original blocking file");
                        saveErrorChecked = true;
                        std::filesystem::current_path(output);
                        LoadGameScreen::invalidateCachedSaveSlots();
                        game.m_screenManager.setActiveScreen(nullptr);
                        game.m_screenManager.setCurrentMode(source->currentSceneKind == SceneKind::Indoor
                            ? AppMode::GameplayIndoor : AppMode::GameplayOutdoor);
                        screen().openSaveGameOverlay();
                        phase = Phase::SaveWarmup;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::SaveWarmup && frames >= readyFrame)
                    {
                        const auto *pRow = screen().findHudLayoutElement("SaveGameSlotRow0");
                        require(pRow != nullptr, "Manual save slot is absent");
                        const auto row = screen().resolveHudLayoutElement(
                            pRow->id, width, height, pRow->width, pRow->height);
                        require(row.has_value(), "Manual save slot has no bounds");
                        queueClick(*row, width, height);
                        phase = Phase::SaveEdit;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::SaveEdit && frames >= readyFrame)
                    {
                        require(screen().saveGameScreenState().active,
                            "Save screen closed during slot selection");
                        std::filesystem::current_path(output / "bad-storage");
                        const auto *pConfirm = screen().findHudLayoutElement("SaveGameConfirmButton");
                        require(pConfirm != nullptr, "Manual Save button is absent");
                        const auto confirm = screen().resolveHudLayoutElement(
                            pConfirm->id, width, height, pConfirm->width, pConfirm->height);
                        require(confirm.has_value(), "Manual Save button has no bounds");
                        queueClick(*confirm, width, height);
                        phase = Phase::SaveFailed;
                        readyFrame = frames + 8;
                    }
                    else if (phase == Phase::SaveFailed && frames >= readyFrame)
                    {
                        const auto &saveScreen = screen().saveGameScreenState();
                        require(saveScreen.active && saveScreen.errorText.starts_with("保存失败"),
                            "Asynchronous write failure was not visible in the save screen");
                        trace << "{\"stage\":\"actual-save-failure\",\"error\":"
                            << jsonText(saveScreen.errorText) << "}\n";
                        trace.flush();
                        failedSaveVisible = true;
                        capture("save-write-error");
                        phase = Phase::SaveFailureCapture;
                    }
                    else if (phase == Phase::SaveFailureCapture && pendingCapture.empty())
                    {
                        queueKeyPress(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
                        phase = Phase::SaveFailureClosed;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::SaveFailureClosed && frames >= readyFrame)
                    {
                        require(!screen().saveGameScreenState().active,
                            "Failed manual save did not retain cancellation");
                        const std::vector<char> marker = fileBytes(output / "bad-storage/saves");
                        require(std::string(marker.begin(), marker.end()) == "preserved-nondirectory",
                            "Failed manual save replaced the blocking file");
                        std::filesystem::current_path(output);
                        // Cancelling Save returns to the pause menu. Close that normal parent
                        // screen before counting gameplay frames or expecting its clock to advance.
                        if (screen().menuScreenState().active)
                        {
                            queueKeyPress(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
                            readyFrame = frames + 3;
                        }
                        else
                        {
                            noLowerUi();
                            phase = Phase::Continuing;
                        }
                    }
                    else if (phase == Phase::Continuing && normalFrames >= 30)
                    {
                        require(party().hasActableMember(), "Ordinary continuation ended in party defeat");
                        const std::filesystem::path savedPath = output / "readable-scroll-complete.oysav";
                        require(std::filesystem::copy_file(input, savedPath),
                            "Cannot create an owned copy for the atomic replacement save");
                        require(game.quickSaveToPath(savedPath, "191 long scroll navigation and continuation"),
                            "Cannot explicitly save ordinary continuation");
                        const std::optional<GameSaveData> saved = loadGameDataFromPath(savedPath, error);
                        require(saved && saved->mapFileName == source->mapFileName
                            && saved->heldInventoryItemActive
                            && encodeInventoryItemForComparison(saved->heldInventoryItem) == heldBytes
                            && saved->party.members.size() == baseline.members.size()
                            && saved->party.gold == baseline.gold && saved->party.food == baseline.food
                            && saved->savedGameMinutes > initialMinutes,
                            "Final save lost the held scroll, original roster/Gold/food or normal time: " + error);
                        for (size_t index = 0; index < baseline.members.size(); ++index)
                            require(saved->party.members[index].memberId == baseline.members[index].memberId
                                && saved->party.members[index].experience == baseline.members[index].experience,
                                "Reading changed a stable member or experience");
                        require(fileBytes(input) == originalBytes
                            && fileBytes(output / "source-input.oysav") == originalBytes,
                            "The read-only source save or its original copy changed");
                        require(captures == 6 && persistentObserved && nextObserved
                            && previousObserved && closeConsumed && inspectTouchChecked
                            && dialogueChecked && saveErrorChecked && failedSaveVisible,
                            "Bounded evidence chain is incomplete");
                        savedVerified = true;
                        const auto credits = game.m_pAssetFileSystem->readTextFile("Data/data_tables/credits.txt");
                        require(credits && credits->size() > 1000, "Original imported credits are absent");
                        game.openMainMenuScreen();
                        phase = Phase::MainMenuWarmup;
                        readyFrame = frames + 4;
                    }
                    else if (phase == Phase::MainMenuWarmup && frames >= readyFrame)
                    {
                        const float scale = std::clamp(std::min(width / 640.0f, height / 480.0f), 1.0f, 8.0f);
                        const float rootX = (width - 854.0f * scale) * 0.5f;
                        const float rootY = (height - 480.0f * scale) * 0.5f;
                        const float right = std::min(rootX + 854.0f * scale, float(width));
                        queueClick({std::round(right - 126.0f * scale), std::round(rootY + 199.0f * scale),
                            106.0f * scale, 32.0f * scale, scale}, width, height);
                        phase = Phase::CreditsWarmup;
                        readyFrame = frames + 4;
                    }
                    else if (phase == Phase::CreditsWarmup && frames >= readyFrame)
                    {
                        capture("credits-first-page");
                        phase = Phase::CreditsFirstCapture;
                    }
                    else if (phase == Phase::CreditsFirstCapture && pendingCapture.empty())
                    {
                        const float scale = std::min({width / 640.0f, height / 480.0f, 8.0f});
                        const float x = (width - 640.0f * scale) * 0.5f;
                        const float y = (height - 480.0f * scale) * 0.5f;
                        queueClick({x + 148.0f * scale, y + 414.0f * scale,
                            80.0f * scale, 26.0f * scale, scale}, width, height);
                        phase = Phase::CreditsNextWarmup;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::CreditsNextWarmup && frames >= readyFrame)
                    {
                        capture("credits-next-page");
                        phase = Phase::CreditsNextCapture;
                    }
                    else if (phase == Phase::CreditsNextCapture && pendingCapture.empty())
                    {
                        const float scale = std::min({width / 640.0f, height / 480.0f, 8.0f});
                        const float x = (width - 640.0f * scale) * 0.5f;
                        const float y = (height - 480.0f * scale) * 0.5f;
                        queueClick({x + 500.0f * scale, y + 414.0f * scale,
                            80.0f * scale, 26.0f * scale, scale}, width, height);
                        phase = Phase::CreditsClosed;
                        readyFrame = frames + 3;
                    }
                    else if (phase == Phase::CreditsClosed && frames >= readyFrame)
                    {
                        capture("credits-returned-menu");
                        phase = Phase::CreditsClosedCapture;
                    }
                    else if (phase == Phase::CreditsClosedCapture && pendingCapture.empty())
                    {
                        require(captures == 9, "Credits pixel chain is incomplete");
                        complete = true;
                        phase = Phase::Complete;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete && savedVerified, "Actual App stopped before completing the UI chain");
            report(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            report(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc == 5 && std::string_view(argv[4]) == "--ui-text-only")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::previewUiText(argv[1], argv[2], argv[3]);
        if (argc != 4)
            throw std::runtime_error("readable_scroll_capture COMPLETE_RUNTIME READONLY_NORMAL132_SEED NEW_OUTPUT "
                "[--ui-text-only]");
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
