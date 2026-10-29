#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplaySpellService.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/party/SpellIds.h"
#include "game/tables/ItemTable.h"
#include "game/ui/DialogueTopicLayout.h"
#include "game/ui/screens/LoadGameScreen.h"
#include "game/ui/screens/WinGameScreen.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
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
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string jsonText(std::string_view value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char character : value)
    {
        if (character == '"' || character == '\\')
        {
            out << '\\' << char(character);
        }
        else if (character < 0x20)
        {
            out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(character);
        }
        else
        {
            out << char(character);
        }
    }
    out << '"';
    return out.str();
}

std::vector<char> fileBytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot read preserved capture input: " + path.string());
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

void queueEscape()
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(captureWindow());
    event.key.key = SDLK_ESCAPE;
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    event.key.down = true;
    require(SDL_PushEvent(&event), "Cannot queue actual SDL Escape down");
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    require(SDL_PushEvent(&event), "Cannot queue actual SDL Escape up in the same event batch");
}

void queueClick(const GameplayResolvedHudLayoutElement &rect, int width, int height)
{
    SDL_Window *pWindow = captureWindow();
    int pointWidth = 0;
    int pointHeight = 0;
    require(width > 0 && height > 0 && SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight),
        "Cannot resolve actual pointer coordinates");
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(pWindow);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.clicks = 1;
    event.button.x = (rect.x + rect.width * 0.5f) * float(pointWidth) / float(width);
    event.button.y = (rect.y + rect.height * 0.5f) * float(pointHeight) / float(height);
    require(SDL_PushEvent(&event), "Cannot queue actual SDL left button down");
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    require(SDL_PushEvent(&event), "Cannot queue actual SDL left button up");
}

struct SessionBaseline
{
    SceneKind kind = SceneKind::Outdoor;
    std::string map;
    std::optional<std::filesystem::path> savePath;
    Party::Snapshot party;
    std::vector<uint8_t> partyBytes;
    GameplayUiController::HeldInventoryItemState held;
    double gameMinutes = 0.0;
    float x = 0.0f;
    float y = 0.0f;
    float footZ = 0.0f;
};
} // namespace

// This executable owns fixtures and observations only; it calls the production application lifecycle.
struct ApplicationCaptureTestAccess
{
    static int previewVisuals(std::filesystem::path runtime, std::filesystem::path input,
        std::filesystem::path output, bool certificateOnly = false)
    {
        const int64_t wallLimitMilliseconds = certificateOnly ? 30000 : 45000;
        const unsigned frameLimit = certificateOnly ? 300 : 600;
        const unsigned captureLimit = certificateOnly ? 1 : 3;
        const std::string mode = certificateOnly ? "certificate-only" : "visual-only";
        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        const auto elapsedMilliseconds = [&]()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
        };
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        const std::filesystem::path relativeOutput = output.lexically_relative(runtime);
        require(!std::filesystem::exists(output) && !relativeOutput.empty()
            && *relativeOutput.begin() == "..", "Visual preview requires new output outside the complete runtime");
        const std::vector<char> originalBytes = fileBytes(input);
        std::string error;
        const std::optional<GameSaveData> source = loadGameDataFromPath(input, error);
        require(source && source->party.members.size() == 4 && source->mapFileName.starts_with("7")
            && !source->heldInventoryItemActive, "Expected the unchanged empty-handed normal132 MM7 seed: " + error);
        std::filesystem::create_directories(certificateOnly ? output : output / "saves");
        const std::filesystem::path copiedInput = output / "source-input.oysav";
        const std::filesystem::path longSlotPath = output / "saves/long-label.oysav";
        require(std::filesystem::copy_file(input, copiedInput), "Cannot preserve visual preview input copy");
        GameSaveData longSlot = *source;
        longSlot.saveName = "跨越三个世界的冒险者这是用于检验加载列表裁剪的超长中文存档名称"
            "跨越三个世界的冒险者这是用于检验加载列表裁剪的超长中文存档名称"
            "跨越三个世界的冒险者这是用于检验加载列表裁剪的超长中文存档名称";
        std::vector<char> longSlotBytes;
        if (!certificateOnly)
        {
            require(saveGameDataToPath(longSlotPath, longSlot, error),
                "Cannot write isolated long-label fixture: " + error);
            const std::optional<GameSaveData> decodedSlot = loadGameDataFromPath(longSlotPath, error);
            require(decodedSlot && decodedSlot->saveName == longSlot.saveName,
                "Independent long-label fixture failed normal save decoding: " + error);
            longSlotBytes = fileBytes(longSlotPath);
        }
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save isolated visual settings");
        const char *pScope = certificateOnly
            ? "One certificate visual follow-up on the complete normal runtime and unchanged normal132 MM7 input. "
              "Directly display the actual certificate builder with the same long display-only Chinese name "
              "and original winBG art, capture once and quit normally. No LoadGame or Bank views, encoded "
              "save fixture, transaction, rollback, new-session request, quick-key/service assertion, scroll, "
              "normal-time frame or old campaign/cold/device replay. Delta0 throughout; immortal and "
              "unlimitedMana explicitly disabled. This is a visual fixture with no campaign-ending claim."
            : "Three affected visual previews on the complete normal runtime. Directly open the actual "
              "LoadGame screen with one independently encoded valid long-name save slot. Directly display "
              "the actual certificate builder with a long display-only Chinese character line and original "
              "winBG art. Then declare one original open Bank HouseService entry, click its actual Deposit "
              "topic and send SDL amount1 to preview the visible amount/buttons. Click Cancel after the "
              "image without submitting. Screen transitions are direct visual fixtures. No rollback, "
              "new-session request, quick-key/service assertion, transaction, scroll, normal-time frame, "
              "old campaign route or cold/device replay. Delta0 throughout; immortal and unlimitedMana "
              "explicitly disabled, no live Party/map/time edits and source input remains unchanged.";
        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"checkpoint\":192,\"mode\":" << jsonText(mode) << ",\"runtime\":" << jsonText(runtime.string())
            << ",\"input\":" << jsonText(input.string())
            << ",\"long_save_name\":" << jsonText(certificateOnly ? std::string() : longSlot.saveName)
            << ",\"scope\":" << jsonText(pScope) << ",\"window_points\":[1210,834]"
            << ",\"wall_limit_seconds\":" << wallLimitMilliseconds / 1000
            << ",\"frame_limit\":" << frameLimit << ",\"capture_limit\":" << captureLimit
            << ",\"fixture_save_write\":" << !certificateOnly
            << ",\"immortal\":false,\"unlimited_mana\":false,\"source_save_write\":false,"
               "\"natural_house_entry\":false,"
               "\"new_unit_cases\":0,\"natural_campaign\":false,\"device\":false}\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM UI Visual Followup192";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned frames = 0;
        unsigned readyFrame = 6;
        unsigned captures = 0;
        int drawableWidth = 0;
        int drawableHeight = 0;
        uint32_t bankHouseId = 0;
        bool complete = false;
        bool bankAmountCanceled = false;
        std::string phase = certificateOnly ? "certificate-open" : "load-menu";
        std::string pendingCapture;
        Party::Snapshot originalParty;
        double originalMinutes = 0.0;
        const AppMode gameplayMode = source->currentSceneKind == SceneKind::Indoor
            ? AppMode::GameplayIndoor : AppMode::GameplayOutdoor;
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha;
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto button = [&](const char *pId, int width, int height)
        {
            const GameplayScreenRuntime::HudLayoutElement *pLayout = screen().findHudLayoutElement(pId);
            require(pLayout != nullptr && pLayout->visible && pLayout->interactive,
                "Visible visual-preview control is absent: " + std::string(pId));
            const std::optional<GameplayResolvedHudLayoutElement> rect = screen().resolveHudLayoutElement(
                pId, width, height, pLayout->width, pLayout->height);
            require(rect && rect->width > 0.0f && rect->height > 0.0f,
                "Visual-preview control has no bounds: " + std::string(pId));
            return *rect;
        };
        const auto capture = [&](const char *pName)
        {
            require(pendingCapture.empty() && captures < captureLimit, "Visual preview exceeded its image bound");
            pendingCapture = pName;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pendingCapture.c_str());
        };
        const auto report = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"passed\":" << passed << ",\"failure\":" << jsonText(failure)
                << ",\"mode\":" << jsonText(mode) << ",\"phase\":" << jsonText(phase)
                << ",\"frames\":" << frames << ",\"elapsed_ms\":" << elapsedMilliseconds()
                << ",\"captures\":" << captures << ",\"drawable\":[" << drawableWidth << ',' << drawableHeight
                << "],\"bank_house_id\":" << bankHouseId << ",\"bank_amount_canceled\":" << bankAmountCanceled
                << ",\"bank_transactions\":0,\"normal_frames\":0,\"lifecycle_replay\":false"
                << ",\"fixture_save_write\":" << !certificateOnly
                << ",\"immortal\":false,\"unlimited_mana\":false,\"source_save_write\":false,\"new_unit_cases\":0,"
                   "\"natural_campaign\":false,\"device\":false,\"visual_review\":\"required\"}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Mac Metal preview required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(copiedInput),
                        "Cannot load unchanged visual-preview input");
                    require(game.m_pAssetFileSystem->exists("worlds/mm7/maps/7d24.blv")
                        || game.m_pAssetFileSystem->exists("Data/games/7d24.blv"),
                        "Visual-only preview requires the complete runtime, without the resource-failure fixture");
                    require(screen().ensureGameplayLayoutsLoaded(), "Cannot load current visual-preview layouts");
                    require(!(SDL_GetWindowFlags(captureWindow()) & SDL_WINDOW_FULLSCREEN),
                        "Visual-only preview must remain windowed");
                    originalParty = game.m_pMapSceneRuntime->party().snapshot();
                    originalMinutes = game.m_gameSession.activeWorldRuntime()->gameMinutes();
                    if (!certificateOnly)
                    {
                        LoadGameScreen::invalidateCachedSaveSlots();
                        game.openLoadGameScreen(true, "gameplay_menu");
                    }
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= frameLimit && elapsedMilliseconds() < wallLimitMilliseconds,
                        "Visual-only preview exceeded its finite bound");
                    drawableWidth = width;
                    drawableHeight = height;
                    game.renderFrame(width, height, wheel, 0.0f);
                    trace << "{\"frame\":" << frames << ",\"phase\":" << jsonText(phase)
                        << ",\"captures\":" << captures << ",\"bank_input\":"
                        << screen().houseBankState().inputActive() << "}\n";
                    trace.flush();
                    if (!pendingCapture.empty())
                    {
                        if (const std::optional<Engine::BgfxContext::ScreenshotCapture> image =
                                Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()),
                                int(image->width * 4));
                            require(pSurface != nullptr, "Cannot wrap actual visual-only screenshot");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot preserve actual visual-only screenshot");
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (phase == "load-menu" && frames >= readyFrame)
                    {
                        require(game.m_screenManager.currentMode() == AppMode::LoadMenu,
                            "Direct actual LoadGame visual fixture did not open");
                        capture("visual-load-long-label");
                        phase = "load-image";
                    }
                    else if ((phase == "load-image" || phase == "certificate-open") && pendingCapture.empty())
                    {
                        WinGameCertificate certificate = game.buildWinGameCertificate();
                        certificate.characterLine = screen().gameplayUiRuntime().uiTextTable().formatWinCharacter(
                            "跨越三个世界的冒险者百分号%s", 120, "光明骑士");
                        game.m_screenManager.setActiveScreen(std::make_unique<WinGameScreen>(
                            *game.m_pAssetFileSystem, certificate, gameplayMode));
                        phase = "certificate";
                        readyFrame = frames + 4;
                    }
                    else if (phase == "certificate" && frames >= readyFrame)
                    {
                        capture("visual-certificate-winbg");
                        phase = "certificate-image";
                    }
                    else if (phase == "certificate-image" && pendingCapture.empty())
                    {
                        if (certificateOnly)
                        {
                            const Party::Snapshot observed = game.m_pMapSceneRuntime->party().snapshot();
                            require(captures == 1 && observed.gold == originalParty.gold
                                && observed.bankGold == originalParty.bankGold
                                && observed.questBits == originalParty.questBits
                                && game.m_gameSession.currentMapFileName() == source->mapFileName
                                && game.m_gameSession.currentSavePath() == copiedInput
                                && game.m_gameSession.activeWorldRuntime()->gameMinutes() == originalMinutes,
                                "Certificate-only visual fixture changed preserved gameplay state");
                            require(fileBytes(input) == originalBytes && fileBytes(copiedInput) == originalBytes,
                                "Certificate-only preview changed the preserved source input");
                            complete = true;
                            phase = "complete";
                            SDL_Event quit{};
                            quit.type = SDL_EVENT_QUIT;
                            require(SDL_PushEvent(&quit), "Cannot queue normal certificate-only exit");
                            return;
                        }
                        game.m_screenManager.setActiveScreen(nullptr);
                        game.m_screenManager.setCurrentMode(gameplayMode);
                        require(screen().houseTable() != nullptr, "Original Bank table is absent");
                        for (const auto &[id, house] : screen().houseTable()->entries())
                        {
                            if (resolveHouseServiceType(house) == HouseServiceType::Bank
                                && house.residentNpcIds.empty()
                                && isHouseOpenAtGameMinute(house, float(originalMinutes))
                                && (bankHouseId == 0 || id < bankHouseId))
                            {
                                bankHouseId = id;
                            }
                        }
                        require(bankHouseId != 0, "No original open Bank is available for the direct visual fixture");
                        EventRuntimeState *pEvents = game.m_pMapSceneRuntime->eventRuntimeState();
                        require(pEvents != nullptr, "Visual Bank fixture has no event runtime");
                        EventRuntimeState::PendingDialogueContext bankEntry;
                        bankEntry.kind = DialogueContextKind::HouseService;
                        bankEntry.sourceId = bankHouseId;
                        bankEntry.hostHouseId = bankHouseId;
                        pEvents->pendingDialogueContext = bankEntry;
                        phase = "bank-root";
                        readyFrame = frames + 4;
                    }
                    else if (phase == "bank-root" && frames >= readyFrame)
                    {
                        const std::optional<DialogueTopicLayout> topics =
                            resolveDialogueTopicLayout(screen(), width, height);
                        require(screen().activeEventDialog().isActive && topics
                            && topics->pageIndex < topics->pages.size(), "Original Bank Deposit rows are absent");
                        bool clicked = false;
                        for (const DialogueTopicRow &row : topics->pages[topics->pageIndex].rows)
                        {
                            require(row.actionIndex < screen().activeEventDialog().actions.size(),
                                "Visual Bank row lost its actual action");
                            const EventDialogAction &action = screen().activeEventDialog().actions[row.actionIndex];
                            if (action.kind == EventDialogActionKind::HouseService
                                && action.id == uint32_t(HouseActionId::BankDepositAll) && action.enabled)
                            {
                                queueClick(row.rect, width, height);
                                clicked = true;
                                break;
                            }
                        }
                        require(clicked, "Actual enabled Deposit topic was not visible");
                        phase = "bank-amount";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "bank-amount" && frames >= readyFrame)
                    {
                        require(screen().houseBankState().inputActive(), "Deposit did not open the amount preview");
                        SDL_Event text{};
                        text.type = SDL_EVENT_TEXT_INPUT;
                        text.text.windowID = SDL_GetWindowID(captureWindow());
                        text.text.text = "1";
                        require(SDL_PushEvent(&text), "Cannot queue visual Bank amount text");
                        phase = "bank-typed";
                        readyFrame = frames + 2;
                    }
                    else if (phase == "bank-typed" && frames >= readyFrame)
                    {
                        require(screen().houseBankState().inputText == "1", "Visual Bank amount is not visible1");
                        (void)button("DialogueOkButton", width, height);
                        (void)button("DialogueGoodbyeButton", width, height);
                        capture("visual-bank-amount-labels");
                        phase = "bank-image";
                    }
                    else if (phase == "bank-image" && pendingCapture.empty())
                    {
                        queueClick(button("DialogueGoodbyeButton", width, height), width, height);
                        phase = "bank-cancel";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "bank-cancel" && frames >= readyFrame)
                    {
                        const Party::Snapshot observed = game.m_pMapSceneRuntime->party().snapshot();
                        require(!screen().houseBankState().inputActive() && observed.gold == originalParty.gold
                            && observed.bankGold == originalParty.bankGold
                            && observed.questBits == originalParty.questBits
                            && observed.members.size() == originalParty.members.size(),
                            "Visual-only Bank cancel changed original accounting, quests or party size");
                        for (size_t index = 0; index < originalParty.members.size(); ++index)
                        {
                            require(observed.members[index].memberId == originalParty.members[index].memberId
                                && observed.members[index].name == originalParty.members[index].name,
                                "Visual preview replaced an original party member");
                        }
                        require(game.m_gameSession.currentMapFileName() == source->mapFileName
                            && game.m_gameSession.currentSavePath() == copiedInput
                            && game.m_gameSession.activeWorldRuntime()->gameMinutes() == originalMinutes
                            && !screen().heldInventoryItem().active && captures == 3,
                            "Visual-only preview changed map, provenance, game clock, cursor or capture count");
                        require(fileBytes(input) == originalBytes && fileBytes(copiedInput) == originalBytes
                            && fileBytes(longSlotPath) == longSlotBytes,
                            "Visual preview changed a preserved input file");
                        bankAmountCanceled = true;
                        complete = true;
                        phase = "complete";
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal visual-preview exit");
                    }
                }, [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete && elapsedMilliseconds() < wallLimitMilliseconds,
                "Visual-only preview stopped before bounded completion");
            require(fileBytes(input) == originalBytes && fileBytes(copiedInput) == originalBytes,
                "Visual preview shutdown changed the preserved input");
            report(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            report(false, failure.what());
            throw;
        }
    }

    static int run(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        const auto elapsedMilliseconds = [&]()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
        };
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        const std::filesystem::path relativeOutput = output.lexically_relative(runtime);
        require(!std::filesystem::exists(output) && !relativeOutput.empty()
            && *relativeOutput.begin() == "..", "Use a new output directory outside the preserved runtime");
        require(std::filesystem::is_regular_file(runtime / "engine.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm6.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm7.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm8.zip"),
            "Isolated inspection runtime retaining the engine and three world packages required");
        std::string error;
        const std::optional<GameSaveData> source = loadGameDataFromPath(input, error);
        require(source && source->party.members.size() == 4 && source->mapFileName.starts_with("7")
            && source->mapFileName != "7d24.blv"
            && !source->heldInventoryItemActive && source->party.pendingItemDeliveries.empty(),
            "Expected the unchanged empty-handed normal132 MM7 seed: " + error);
        const std::vector<char> originalBytes = fileBytes(input);
        std::filesystem::create_directories(output / "saves");
        const std::filesystem::path copiedInput = output / "source-input.oysav";
        const std::filesystem::path incomingPath = output / "saves/incoming-resource-failure.oysav";
        require(std::filesystem::copy_file(input, copiedInput), "Cannot preserve the unchanged input copy");
        std::filesystem::current_path(output);
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.debugConsole = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save isolated capture settings");
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":192,\"runtime\":" << jsonText(runtime.string())
            << ",\"input\":" << jsonText(input.string()) << ",\"input_copy\":" << jsonText(copiedInput.string())
            << ",\"runtime_inspection_only\":true,\"fault_map\":\"7d24.blv\","
               "\"omitted_mm7_archive_members\":[\"maps/7d24.blv\",\"maps/7d24.dlv\"],"
               "\"original_complete_runtime_modified\":false"
            << ",\"scope\":\"Load an unchanged normal132 MM7 save into the actual GameApplication. "
               "Open its gameplay menu and NewGame screen directly with source gameplay_menu, cancel through "
               "same-batch SDL Escape down/up, and verify return to the live gameplay menu. Then directly "
               "accept a new-session request with only the startup override set to missing-resource 7d24, "
               "and verify its failure also reconstructs the original gameplay menu. Then place one "
               "real-table message scroll2125 on the cursor solely as a positive rollback fixture. Build an "
               "independent incoming save in this new output directory for known map 7d24.blv, changed first "
               "member name, changed gold, toggled QBit10001 and an empty cursor. Call actual production "
               "quickLoadFromPath while the actual gameplay-origin LoadGame screen is active. Before this "
               "lifecycle flow, directly arm/rebuild/clear one Master5 Incinerate request and assert its "
               "bypass/no-mana/no-recovery context; that assertion is not an actual target cast. The decoded "
               "incoming file is valid and original MapStats still contains 7d24.blv. The caller supplies an "
               "inspection-only runtime copy whose mm7.zip omits maps/7d24.blv and maps/7d24.dlv; all other "
               "members remain unchanged. Resource loading fails after session replacement and renderer "
               "shutdown. Require the production runtime teardown generation to increase and the original "
               "scene to be reconstructed. The original complete runtime is untouched. "
               "Check original session state immediately, cancel the LoadGame screen through SDL Escape, "
               "close the returned gameplay menu through SDL Escape, and allow 30 normal frames of 1/60 second. "
               "Then directly preview the actual certificate builder with a long display-only Chinese name, "
               "dismiss it through SDL Escape and return to gameplay; this is not a campaign ending. "
               "Finally declare a direct HouseService entry into an original currently-open Bank with no "
               "resident lobby. Click the actual Deposit topic, send SDL text input 1, and click the visible "
               "DialogueOkButton through SDL down/up. Verify cash decreases by1 and bank balance increases "
               "by1, then leave through visible Goodbye. The bank entry is a UI fixture, not a natural "
               "house visit; no game time, gold or bank balance is directly edited for this transaction. "
               "UI observation uses delta 0. Across rendered frames the Party comparison excludes only "
               "portraitState, portraitElapsedTicks, portraitDurationTicks and portraitSequenceCounter, "
               "which advance from the real animation clock; all other canonical Party bytes must match. "
               "Immediate synchronous rollback checks compare every Party byte, including these fields. "
               "No live map/pose/AI/HP/time edits, input-source save writes, successful new-game start, "
               "natural campaign, device or cold-restart claim.\",\"window_points\":[1210,834],"
               "\"wall_limit_seconds\":90,\"frame_limit\":900,\"capture_limit\":5,"
               "\"fixture_save_write\":true,\"source_save_write\":false,\"audio\":\"dummy\","
               "\"new_unit_cases\":0,\"old_route_replays\":0,\"cold_replays\":0,\"device\":false}\n";
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM UI Continuation192";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        unsigned frames = 0;
        unsigned readyFrame = 8;
        unsigned normalFrames = 0;
        unsigned captures = 0;
        unsigned escapePairs = 0;
        unsigned portraitOnlyComparisons = 0;
        unsigned bankDepositClicks = 0;
        unsigned bankSubmitClicks = 0;
        unsigned bankCloseClicks = 0;
        uint32_t bankHouseId = 0;
        int bankCashBefore = 0;
        int bankBalanceBefore = 0;
        int drawableWidth = 0;
        int drawableHeight = 0;
        bool newGameReturned = false;
        bool failedLoadObserved = false;
        bool failedLoadRestored = false;
        bool resourceFailureReconstructed = false;
        bool loadMenuReturned = false;
        bool serviceContextChecked = false;
        bool newGameFailureRestored = false;
        bool quickKeyEdgesChecked = false;
        bool bankInputAreaChecked = false;
        bool bankSubmitted = false;
        bool smallRootLayoutsChecked = false;
        bool complete = false;
        std::string phase = "warmup";
        std::string pendingCapture;
        SessionBaseline baseline;
        IMapSceneRuntime *pOriginalScene = nullptr;
        std::vector<char> incomingBytes;
        uint64_t failedLoadGenerationBefore = 0;
        uint64_t failedLoadGenerationAfter = 0;
        double normalStartMinutes = 0.0;
        double normalEndMinutes = 0.0;
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(12);
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto party = [&]() -> Party &
        {
            require(game.m_pMapSceneRuntime != nullptr, "The live map scene disappeared");
            return game.m_pMapSceneRuntime->party();
        };
        const auto captureBaseline = [&]()
        {
            const IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
            require(pWorld != nullptr, "The active world runtime disappeared before the fixture");
            baseline.kind = game.m_gameSession.currentSceneKind();
            baseline.map = game.m_gameSession.currentMapFileName();
            baseline.savePath = game.m_gameSession.currentSavePath();
            baseline.party = party().snapshot();
            baseline.partyBytes = encodePartySnapshotForComparison(baseline.party);
            baseline.held = screen().heldInventoryItem();
            baseline.gameMinutes = pWorld->gameMinutes();
            baseline.x = pWorld->partyX();
            baseline.y = pWorld->partyY();
            baseline.footZ = pWorld->partyFootZ();
        };
        const auto checkRetainedState = [&](bool compareCompleteParty, bool compareClockAndPose,
                                           bool allowPortraitAnimation)
        {
            const IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
            require(pWorld != nullptr && game.m_pMapSceneRuntime != nullptr,
                "Failed lifecycle operation detached the live runtime");
            require(game.m_gameSession.currentSceneKind() == baseline.kind
                && game.m_pMapSceneRuntime->kind() == baseline.kind
                && game.m_gameSession.currentMapFileName() == baseline.map
                && game.m_gameSession.currentSavePath() == baseline.savePath,
                "Lifecycle operation changed the original map, scene kind or current save path");
            const Party::Snapshot observed = party().snapshot();
            require(observed.members.size() == baseline.party.members.size()
                && observed.gold == baseline.party.gold && observed.questBits == baseline.party.questBits,
                "Lifecycle operation changed the original party size, gold or quest bits");
            for (size_t index = 0; index < baseline.party.members.size(); ++index)
            {
                const Character &expected = baseline.party.members[index];
                const Character &actual = observed.members[index];
                require(actual.memberId == expected.memberId && actual.rosterId == expected.rosterId
                    && actual.name == expected.name, "Lifecycle operation replaced an original party member");
            }
            if (compareCompleteParty)
            {
                const std::vector<uint8_t> observedBytes = encodePartySnapshotForComparison(observed);
                const bool rawEqual = observedBytes == baseline.partyBytes;
                Party::Snapshot comparison = observed;
                trace << "{\"stage\":\"party-snapshot-comparison\",\"phase\":" << jsonText(phase)
                    << ",\"allow_wall_clock_portrait_animation\":" << allowPortraitAnimation
                    << ",\"raw_canonical_bytes_equal\":" << rawEqual << ",\"portrait_deltas\":[";
                bool firstDelta = true;
                for (size_t index = 0; index < baseline.party.members.size(); ++index)
                {
                    const Character &expected = baseline.party.members[index];
                    const Character &actual = observed.members[index];
                    const auto reportDelta = [&](const char *pField, uint64_t before, uint64_t after)
                    {
                        if (before != after)
                        {
                            if (!firstDelta)
                            {
                                trace << ',';
                            }
                            firstDelta = false;
                            trace << "{\"member_index\":" << index << ",\"member_id\":" << actual.memberId
                                << ",\"field\":" << jsonText(pField) << ",\"before\":" << before
                                << ",\"after\":" << after << '}';
                        }
                    };
                    reportDelta("portraitState", uint64_t(expected.portraitState), uint64_t(actual.portraitState));
                    reportDelta("portraitElapsedTicks", expected.portraitElapsedTicks, actual.portraitElapsedTicks);
                    reportDelta("portraitDurationTicks", expected.portraitDurationTicks, actual.portraitDurationTicks);
                    reportDelta("portraitSequenceCounter", expected.portraitSequenceCounter,
                        actual.portraitSequenceCounter);
                    if (allowPortraitAnimation)
                    {
                        comparison.members[index].portraitState = expected.portraitState;
                        comparison.members[index].portraitElapsedTicks = expected.portraitElapsedTicks;
                        comparison.members[index].portraitDurationTicks = expected.portraitDurationTicks;
                        comparison.members[index].portraitSequenceCounter = expected.portraitSequenceCounter;
                    }
                }
                const bool comparisonEqual = encodePartySnapshotForComparison(comparison) == baseline.partyBytes;
                trace << "],\"canonical_comparison_equal\":" << comparisonEqual << "}\n";
                trace.flush();
                if (allowPortraitAnimation && !rawEqual && comparisonEqual)
                {
                    ++portraitOnlyComparisons;
                }
                if (!comparisonEqual)
                {
                    std::ofstream(output / ("party-baseline-" + phase + ".bin"), std::ios::binary)
                        .write(reinterpret_cast<const char *>(baseline.partyBytes.data()),
                            std::streamsize(baseline.partyBytes.size()));
                    std::ofstream(output / ("party-observed-" + phase + ".bin"), std::ios::binary)
                        .write(reinterpret_cast<const char *>(observedBytes.data()),
                            std::streamsize(observedBytes.size()));
                }
                require(comparisonEqual, allowPortraitAnimation
                    ? "Lifecycle operation changed Party fields beyond wall-clock portrait presentation"
                    : "Synchronous rollback changed the complete Party snapshot");
            }
            const GameplayUiController::HeldInventoryItemState &held = screen().heldInventoryItem();
            require(held.active == baseline.held.active && held.grabCellOffsetX == baseline.held.grabCellOffsetX
                && held.grabCellOffsetY == baseline.held.grabCellOffsetY
                && held.grabOffsetX == baseline.held.grabOffsetX && held.grabOffsetY == baseline.held.grabOffsetY
                && encodeInventoryItemForComparison(held.item) == encodeInventoryItemForComparison(baseline.held.item),
                "Lifecycle operation lost or replaced the cursor item or its grab offsets");
            if (compareClockAndPose)
            {
                require(std::abs(pWorld->gameMinutes() - baseline.gameMinutes) < 0.00001
                    && std::abs(pWorld->partyX() - baseline.x) < 0.001f
                    && std::abs(pWorld->partyY() - baseline.y) < 0.001f
                    && std::abs(pWorld->partyFootZ() - baseline.footZ) < 0.001f,
                    "Paused lifecycle operation changed the original clock or world pose");
            }
        };
        const auto requireGameplayMenu = [&]()
        {
            require(game.m_screenManager.activeScreen() == nullptr && screen().menuScreenState().active
                && game.m_screenManager.currentMode() == (baseline.kind == SceneKind::Indoor
                    ? AppMode::GameplayIndoor : AppMode::GameplayOutdoor),
                "Cancel did not return to the actual live gameplay menu");
        };
        const auto capture = [&](const char *pName)
        {
            require(pendingCapture.empty() && captures < 5, "Overlapping or excessive lifecycle screenshots");
            pendingCapture = pName;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pendingCapture.c_str());
        };
        const auto sendEscape = [&]()
        {
            queueEscape();
            ++escapePairs;
        };
        const auto resolveButton = [&](const char *pId, int width, int height)
        {
            const GameplayScreenRuntime::HudLayoutElement *pLayout = screen().findHudLayoutElement(pId);
            require(pLayout != nullptr && pLayout->visible && pLayout->interactive,
                "Required visible bank control is absent: " + std::string(pId));
            const std::optional<GameplayResolvedHudLayoutElement> rect = screen().resolveHudLayoutElement(
                pId, width, height, pLayout->width, pLayout->height);
            require(rect && rect->width > 0.0f && rect->height > 0.0f && rect->x >= -0.5f && rect->y >= -0.5f
                && rect->x + rect->width <= float(width) + 0.5f
                && rect->y + rect->height <= float(height) + 0.5f,
                "Bank control does not fit the actual drawable viewport: " + std::string(pId));
            return *rect;
        };
        const auto report = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(12)
                << "{\"passed\":" << passed << ",\"failure\":" << jsonText(failure)
                << ",\"phase\":" << jsonText(phase) << ",\"frames\":" << frames
                << ",\"elapsed_ms\":" << elapsedMilliseconds() << ",\"captures\":" << captures
                << ",\"drawable\":[" << drawableWidth << ',' << drawableHeight << ']'
                << ",\"new_game_cancel_restored_gameplay_menu\":" << newGameReturned
                << ",\"actual_quick_load_failed\":" << failedLoadObserved
                << ",\"failed_load_restored_state\":" << failedLoadRestored
                << ",\"resource_failure_reconstructed\":" << resourceFailureReconstructed
                << ",\"teardown_generation_before\":" << failedLoadGenerationBefore
                << ",\"teardown_generation_after\":" << failedLoadGenerationAfter
                << ",\"runtime_inspection_only\":true,\"fault_map\":\"7d24.blv\""
                << ",\"load_cancel_restored_gameplay_menu\":" << loadMenuReturned
                << ",\"direct_service_context_round_trip\":" << serviceContextChecked
                << ",\"new_game_failure_restored\":" << newGameFailureRestored
                << ",\"direct_quick_key_edges_checked\":" << quickKeyEdgesChecked
                << ",\"actual_target_cast\":false"
                << ",\"sdl_escape_pairs\":" << escapePairs << ",\"normal_frames\":" << normalFrames
                << ",\"portrait_only_comparisons\":" << portraitOnlyComparisons
                << ",\"bank_direct_entry_fixture\":true,\"bank_house_id\":" << bankHouseId
                << ",\"bank_deposit_topic_clicks\":" << bankDepositClicks
                << ",\"bank_submit_clicks\":" << bankSubmitClicks
                << ",\"bank_goodbye_clicks\":" << bankCloseClicks
                << ",\"bank_input_area_checked\":" << bankInputAreaChecked
                << ",\"bank_one_gold_submitted\":" << bankSubmitted
                << ",\"bank_cash_before\":" << bankCashBefore
                << ",\"bank_balance_before\":" << bankBalanceBefore
                << ",\"direct_small_root_layout_check\":" << smallRootLayoutsChecked
                << ",\"actual_small_viewport\":false"
                << ",\"normal_seconds\":" << double(normalFrames) / 60.0
                << ",\"normal_start_minutes\":" << normalStartMinutes
                << ",\"normal_end_minutes\":" << normalEndMinutes
                << ",\"fixture_save_write\":true,\"source_save_write\":false,\"new_unit_cases\":0,"
                   "\"natural_campaign\":false,\"device\":false,\"visual_review\":\"required\"}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Mac Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(copiedInput),
                        "Cannot load the preserved normal132 MM7 input copy");
                    require(screen().ensureGameplayLayoutsLoaded(), "Cannot load the current gameplay layouts");
                    for (const char *pRootId : {"SaveGameRoot", "ControlsRoot", "KeyboardRoot"})
                    {
                        const GameplayScreenRuntime::HudLayoutElement *pRoot = screen().findHudLayoutElement(pRootId);
                        require(pRoot != nullptr, "Small-root layout is absent: " + std::string(pRootId));
                        const std::optional<GameplayResolvedHudLayoutElement> rect = screen().resolveHudLayoutElement(
                            pRootId, 320, 240, pRoot->width, pRoot->height);
                        require(rect && rect->x >= -0.5f && rect->y >= -0.5f && rect->width > 0.0f
                            && rect->height > 0.0f && rect->x + rect->width <= 320.5f
                            && rect->y + rect->height <= 240.5f,
                            "Direct 320x240 modal root resolution does not fit: " + std::string(pRootId));
                        trace << "{\"stage\":\"direct-small-root-layout\",\"root\":" << jsonText(pRootId)
                            << ",\"rect\":[" << rect->x << ',' << rect->y << ',' << rect->width << ',' << rect->height
                            << "],\"actual_small_viewport\":false}\n";
                    }
                    smallRootLayoutsChecked = true;
                    const MapStatsEntry *pFaultMap = game.m_gameDataLoader.getMapStats().findByFileName("7d24.blv");
                    require(pFaultMap != nullptr && pFaultMap->worldId == "mm7",
                        "Inspection runtime must retain the original known-world 7d24 MapStats record");
                    for (const char *pPath : {"worlds/mm7/maps/7d24.blv", "worlds/mm7/maps/7d24.dlv",
                             "Data/games/7d24.blv", "Data/games/7d24.dlv"})
                    {
                        require(!game.m_pAssetFileSystem->exists(pPath),
                            "Inspection-only runtime still exposes an intentionally omitted map resource: "
                                + std::string(pPath));
                    }
                    require(!(SDL_GetWindowFlags(captureWindow()) & SDL_WINDOW_FULLSCREEN),
                        "Lifecycle capture must remain windowed");
                    PartySpellCastRequest scrollRequest = {};
                    scrollRequest.casterMemberIndex = 0;
                    scrollRequest.spellId = spellIdValue(SpellId::Incinerate);
                    scrollRequest.skillLevelOverride = 5;
                    scrollRequest.skillMasteryOverride = SkillMastery::Master;
                    scrollRequest.spendMana = false;
                    scrollRequest.applyRecovery = false;
                    scrollRequest.bypassRequiredMastery = true;
                    screen().spellService().armPendingTargetSelection(
                        screen(), scrollRequest, PartySpellCastTargetKind::Actor, "Incinerate");
                    const PartySpellCastRequest rebuilt = screen().spellService().makePendingTargetSelectionRequest();
                    require(rebuilt.casterMemberIndex == scrollRequest.casterMemberIndex
                        && rebuilt.spellId == scrollRequest.spellId && rebuilt.skillLevelOverride == 5
                        && rebuilt.skillMasteryOverride == SkillMastery::Master && !rebuilt.spendMana
                        && !rebuilt.applyRecovery && rebuilt.bypassRequiredMastery,
                        "Direct pending-target service round-trip lost the scroll cast context");
                    screen().spellService().clearPendingTargetSelection(screen());
                    for (SDL_Scancode scan : {SDL_SCANCODE_F9, SDL_SCANCODE_F10})
                    {
                        SDL_Event event{};
                        event.type = SDL_EVENT_KEY_DOWN;
                        event.key.scancode = scan;
                        event.key.key = scan == SDL_SCANCODE_F9 ? SDLK_F9 : SDLK_F10;
                        event.key.down = true;
                        game.m_gameInputSystem.handleSdlEvent(event);
                        event.type = SDL_EVENT_KEY_UP;
                        event.key.down = false;
                        game.m_gameInputSystem.handleSdlEvent(event);
                    }
                    game.m_gameInputSystem.updateFromEngineInput(640, 480, 0.0f, game.m_settings,
                        false, false, false, false, true);
                    require(!game.m_gameInputSystem.frame().isScancodeHeld(SDL_SCANCODE_F9)
                        && !game.m_gameInputSystem.frame().isScancodeHeld(SDL_SCANCODE_F10),
                        "Quick-key fixture must contain completed taps, not held keys");
                    game.updateQuickSaveInput();
                    require(game.m_pendingQuickSave && game.m_pendingQuickLoad,
                        "Completed F9/F10 taps did not queue their production actions");
                    game.m_pendingQuickSave = false;
                    game.m_pendingQuickLoad = false;
                    game.m_gameInputSystem.updateFromEngineInput(640, 480, 0.0f, game.m_settings,
                        false, false, false, false, true);
                    quickKeyEdgesChecked = true;
                    require(!game.m_gameSession.gameplayScreenState().pendingSpellTarget().active,
                        "Direct service context assertion left a pending target");
                    serviceContextChecked = true;
                    trace << "{\"stage\":\"direct-service-context-round-trip\",\"mastery\":\"Master\","
                        "\"skill_level\":5,\"bypass_required_mastery\":true,\"spend_mana\":false,"
                        "\"apply_recovery\":false,\"actual_target_cast\":false}\n";
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 900 && elapsedMilliseconds() < 90000,
                        "UI continuation capture exceeded its finite bound");
                    drawableWidth = width;
                    drawableHeight = height;
                    const bool normal = phase == "normal-continuation";
                    if (normal)
                    {
                        require(game.m_screenManager.activeScreen() == nullptr
                            && !screen().menuScreenState().active, "Normal continuation was intercepted by a screen");
                    }
                    game.renderFrame(width, height, wheel, normal ? 1.0f / 60.0f : 0.0f);
                    if (normal)
                    {
                        require(!game.m_gameSession.sharedInputFrameResult().worldInputBlocked,
                            "Normal continuation remained blocked by UI input");
                        ++normalFrames;
                    }
                    trace << "{\"frame\":" << frames << ",\"phase\":" << jsonText(phase)
                        << ",\"mode\":" << int(game.m_screenManager.currentMode())
                        << ",\"active_screen\":" << (game.m_screenManager.activeScreen() != nullptr)
                        << ",\"menu\":" << screen().menuScreenState().active
                        << ",\"map\":" << jsonText(game.m_gameSession.currentMapFileName())
                        << ",\"held_item\":" << screen().heldInventoryItem().active
                        << ",\"normal_frames\":" << normalFrames << "}\n";
                    trace.flush();
                    if (!pendingCapture.empty())
                    {
                        if (const std::optional<Engine::BgfxContext::ScreenshotCapture> image =
                                Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(image->bgraPixels.data()),
                                int(image->width * 4));
                            require(pSurface != nullptr, "Cannot wrap actual lifecycle screenshot");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot preserve actual lifecycle screenshot");
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (phase == "warmup" && frames >= readyFrame)
                    {
                        captureBaseline();
                        pOriginalScene = game.m_pMapSceneRuntime.get();
                        require(baseline.map == source->mapFileName && baseline.savePath == copiedInput,
                            "Initial session did not retain the unchanged input copy's provenance");
                        screen().openMenuOverlay();
                        game.openNewGameScreen("gameplay_menu");
                        require(game.m_screenManager.activeScreen() != nullptr
                            && game.m_screenManager.currentMode() == AppMode::NewGame,
                            "Gameplay-origin NewGame screen did not open");
                        phase = "new-game-open";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "new-game-open" && frames >= readyFrame)
                    {
                        checkRetainedState(true, true, true);
                        sendEscape();
                        phase = "new-game-cancel";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "new-game-cancel" && frames >= readyFrame)
                    {
                        requireGameplayMenu();
                        require(game.m_pMapSceneRuntime.get() == pOriginalScene,
                            "Canceling NewGame replaced the original live scene");
                        checkRetainedState(true, true, true);
                        newGameReturned = true;
                        capture("new-game-cancel-gameplay-menu");
                        phase = "new-game-return-image";
                    }
                    else if (phase == "new-game-return-image" && pendingCapture.empty())
                    {
                        requireGameplayMenu();
                        checkRetainedState(true, true, true);
                        captureBaseline();
                        const std::string previousStartupOverride = game.m_config.startupMapFileOverride;
                        const uint64_t previousGeneration = game.m_runtimeTeardownGeneration;
                        game.m_config.startupMapFileOverride = "7d24.blv";
                        const bool created = game.startNewSessionFromCharacterCreation(
                            baseline.party.members, 0, false, true);
                        game.m_config.startupMapFileOverride = previousStartupOverride;
                        require(!created && game.m_runtimeTeardownGeneration > previousGeneration,
                            "Accepted new-session resource fault did not reach teardown and fail");
                        requireGameplayMenu();
                        checkRetainedState(true, true, false);
                        newGameFailureRestored = true;
                        require(screen().itemTable() != nullptr && screen().itemTable()->get(2125) != nullptr,
                            "Original message scroll2125 is absent from the complete runtime");
                        GameplayUiController::HeldInventoryItemState &held = screen().heldInventoryItem();
                        held = {};
                        held.active = true;
                        held.item = ItemGenerator::makeInventoryItem(2125, *screen().itemTable(),
                            ItemGenerationMode::Generic);
                        held.grabOffsetX = 3.0f;
                        held.grabOffsetY = 5.0f;
                        party().setHeldItemForQueries(held.item);
                        game.synchronizeSessionFromRuntime();
                        captureBaseline();
                        const std::optional<GameSaveData> current = game.m_gameSession.buildSaveData();
                        require(current && current->heldInventoryItemActive,
                            "Cannot capture the positive cursor-item rollback fixture");
                        GameSaveData incoming = *current;
                        incoming.mapFileName = "7d24.blv";
                        incoming.currentSceneKind = SceneKind::Indoor;
                        incoming.hasIndoorSceneState = false;
                        incoming.hasOutdoorRuntimeState = false;
                        incoming.party.members.front().name = "UI192 incoming replacement";
                        incoming.party.gold = baseline.party.gold == 731 ? 732 : 731;
                        if (incoming.party.questBits.contains(10001))
                        {
                            incoming.party.questBits.erase(10001);
                        }
                        else
                        {
                            incoming.party.questBits.insert(10001);
                        }
                        incoming.heldInventoryItemActive = false;
                        incoming.heldInventoryItem = {};
                        incoming.heldInventoryItemGrabCellOffsetX = 0;
                        incoming.heldInventoryItemGrabCellOffsetY = 0;
                        incoming.heldInventoryItemGrabOffsetX = 0.0f;
                        incoming.heldInventoryItemGrabOffsetY = 0.0f;
                        incoming.saveName = "UI192 deliberate known-map resource-failure fixture";
                        require(saveGameDataToPath(incomingPath, incoming, error),
                            "Cannot write independent known-map resource-failure fixture: " + error);
                        const std::optional<GameSaveData> decoded = loadGameDataFromPath(incomingPath, error);
                        require(decoded && decoded->mapFileName == incoming.mapFileName
                            && decoded->party.gold == incoming.party.gold
                            && decoded->party.members.front().name == incoming.party.members.front().name
                            && decoded->party.questBits == incoming.party.questBits
                            && !decoded->heldInventoryItemActive,
                            "Incoming fixture failed to decode before the actual production load: " + error);
                        incomingBytes = fileBytes(incomingPath);
                        LoadGameScreen::invalidateCachedSaveSlots();
                        game.openLoadGameScreen(true, "gameplay_menu");
                        require(game.m_screenManager.activeScreen() != nullptr
                            && game.m_screenManager.currentMode() == AppMode::LoadMenu,
                            "Actual gameplay-origin LoadGame screen did not open");
                        failedLoadGenerationBefore = game.m_runtimeTeardownGeneration;
                        failedLoadObserved = !game.quickLoadFromPath(incomingPath, true);
                        failedLoadGenerationAfter = game.m_runtimeTeardownGeneration;
                        require(failedLoadObserved, "The incoming map with omitted resources unexpectedly loaded");
                        require(failedLoadGenerationAfter > failedLoadGenerationBefore,
                            "Known-map resource failure did not reach production renderer teardown");
                        require(!game.m_loadingOverlayActive && game.m_screenManager.activeScreen() != nullptr
                            && game.m_screenManager.currentMode() == AppMode::LoadMenu,
                            "Failed production load did not retain the active LoadGame screen");
                        checkRetainedState(true, true, false);
                        failedLoadRestored = true;
                        require(!game.m_loadingSavedGameRuntime && !game.m_restoringSessionAfterReplacementFailure,
                            "Failed resource load leaked a temporary load or rollback mode");
                        resourceFailureReconstructed = true;
                        trace << "{\"stage\":\"production-resource-load-returned-false\",\"incoming_map\":"
                            << jsonText(incoming.mapFileName) << ",\"incoming_gold\":" << incoming.party.gold
                            << ",\"teardown_generation_before\":" << failedLoadGenerationBefore
                            << ",\"teardown_generation_after\":" << failedLoadGenerationAfter
                            << ",\"retained_gold\":" << party().gold()
                            << ",\"incoming_qbit10001\":" << incoming.party.questBits.contains(10001)
                            << ",\"retained_qbit10001\":" << party().hasQuestBit(10001)
                            << ",\"retained_held_item\":2125}\n";
                        phase = "failed-resource-load-menu";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "failed-resource-load-menu" && frames >= readyFrame)
                    {
                        checkRetainedState(true, true, true);
                        capture("resource-load-failure-retained-load-menu");
                        phase = "failed-resource-load-image";
                    }
                    else if (phase == "failed-resource-load-image" && pendingCapture.empty())
                    {
                        sendEscape();
                        phase = "load-menu-cancel";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "load-menu-cancel" && frames >= readyFrame)
                    {
                        requireGameplayMenu();
                        checkRetainedState(true, true, true);
                        loadMenuReturned = true;
                        sendEscape();
                        phase = "close-gameplay-menu";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "close-gameplay-menu" && frames >= readyFrame)
                    {
                        require(game.m_screenManager.activeScreen() == nullptr && !screen().menuScreenState().active,
                            "Actual SDL Escape did not close the returned gameplay menu");
                        checkRetainedState(true, true, true);
                        normalStartMinutes = game.m_gameSession.activeWorldRuntime()->gameMinutes();
                        phase = "normal-continuation";
                    }
                    else if (phase == "normal-continuation" && normalFrames >= 30)
                    {
                        checkRetainedState(false, false, false);
                        normalEndMinutes = game.m_gameSession.activeWorldRuntime()->gameMinutes();
                        require(normalEndMinutes > normalStartMinutes,
                            "Thirty ordinary gameplay frames did not advance the restored world clock");
                        capture("rollback-normal-gameplay");
                        phase = "normal-continuation-image";
                    }
                    else if (phase == "normal-continuation-image" && pendingCapture.empty())
                    {
                        require(normalFrames == 30 && captures == 3 && escapePairs == 3,
                            "Lifecycle flow did not remain within its declared action and capture counts");
                        WinGameCertificate certificate = game.buildWinGameCertificate();
                        const UiTextTable &text = screen().gameplayUiRuntime().uiTextTable();
                        require(certificate.title == text.text(UiTextTable::Message::Congratulations)
                            && certificate.scoreLine.starts_with("你的得分")
                            && certificate.totalTimeLine.starts_with("时间总计"),
                            "Certificate builder did not use the imported Chinese captions");
                        certificate.characterLine = text.formatWinCharacter(
                            "跨越三个世界的冒险者百分号%s", 120, "光明骑士");
                        game.m_screenManager.setActiveScreen(std::make_unique<WinGameScreen>(
                            *game.m_pAssetFileSystem, certificate, game.m_screenManager.currentMode()));
                        phase = "certificate-preview";
                        readyFrame = frames + 4;
                    }
                    else if (phase == "certificate-preview" && frames >= readyFrame)
                    {
                        capture("localized-certificate-long-name");
                        phase = "certificate-image";
                    }
                    else if (phase == "certificate-image" && pendingCapture.empty())
                    {
                        sendEscape();
                        phase = "certificate-dismissed";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "certificate-dismissed" && frames >= readyFrame)
                    {
                        require(game.m_screenManager.activeScreen() == nullptr && captures == 4 && escapePairs == 4,
                            "Certificate did not dismiss to gameplay or capture count changed");
                        require(!screen().activeEventDialog().isActive && screen().houseTable() != nullptr,
                            "Bank fixture requires ordinary gameplay and the original House table");
                        captureBaseline();
                        bankCashBefore = party().gold();
                        bankBalanceBefore = party().bankGold();
                        require(bankCashBefore > 0, "Original party has no gold for the one-gold deposit");
                        for (const auto &[id, house] : screen().houseTable()->entries())
                        {
                            if (resolveHouseServiceType(house) == HouseServiceType::Bank
                                && house.residentNpcIds.empty()
                                && isHouseOpenAtGameMinute(house, float(baseline.gameMinutes))
                                && (bankHouseId == 0 || id < bankHouseId))
                            {
                                bankHouseId = id;
                            }
                        }
                        require(bankHouseId != 0, "No original Bank is currently open for the direct UI fixture");
                        EventRuntimeState *pEvents = game.m_pMapSceneRuntime->eventRuntimeState();
                        require(pEvents != nullptr, "Bank fixture has no current event runtime");
                        EventRuntimeState::PendingDialogueContext bankEntry;
                        bankEntry.kind = DialogueContextKind::HouseService;
                        bankEntry.sourceId = bankHouseId;
                        bankEntry.hostHouseId = bankHouseId;
                        pEvents->pendingDialogueContext = bankEntry;
                        trace << "{\"stage\":\"bank-direct-entry-fixture\",\"house_id\":" << bankHouseId
                            << ",\"gold\":" << bankCashBefore << ",\"bank_balance\":" << bankBalanceBefore
                            << ",\"game_minutes\":" << baseline.gameMinutes
                            << ",\"natural_house_entry\":false}\n";
                        phase = "bank-root";
                        readyFrame = frames + 4;
                    }
                    else if (phase == "bank-root" && frames >= readyFrame)
                    {
                        require(screen().activeEventDialog().isActive,
                            "The declared Bank HouseService fixture did not open");
                        const std::optional<DialogueTopicLayout> topics =
                            resolveDialogueTopicLayout(screen(), width, height);
                        require(topics && topics->pageIndex < topics->pages.size(),
                            "Actual Bank topics did not resolve to visible rows");
                        bool foundDeposit = false;
                        for (const DialogueTopicRow &row : topics->pages[topics->pageIndex].rows)
                        {
                            require(row.actionIndex < screen().activeEventDialog().actions.size(),
                                "Bank topic row lost its production action");
                            const EventDialogAction &action = screen().activeEventDialog().actions[row.actionIndex];
                            if (action.kind == EventDialogActionKind::HouseService
                                && action.id == uint32_t(HouseActionId::BankDepositAll) && action.enabled)
                            {
                                queueClick(row.rect, width, height);
                                ++bankDepositClicks;
                                foundDeposit = true;
                                break;
                            }
                        }
                        require(foundDeposit, "Actual visible Bank Deposit topic is absent or disabled");
                        phase = "bank-amount";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "bank-amount" && frames >= readyFrame)
                    {
                        require(screen().houseBankState().inputMode == GameplayUiController::HouseBankInputMode::Deposit
                            && screen().houseBankState().inputText.empty(),
                            "Actual Bank Deposit topic did not open an empty amount prompt");
                        SDL_Event text{};
                        text.type = SDL_EVENT_TEXT_INPUT;
                        text.text.windowID = SDL_GetWindowID(captureWindow());
                        text.text.text = "1";
                        require(SDL_PushEvent(&text), "Cannot queue actual bank amount text input");
                        phase = "bank-typed";
                        readyFrame = frames + 2;
                    }
                    else if (phase == "bank-typed" && frames >= readyFrame)
                    {
                        require(screen().houseBankState().inputActive() && screen().houseBankState().inputText == "1"
                            && game.applicationTextInputActive(), "Actual Bank amount text was not retained");
                        const GameplayResolvedHudLayoutElement ok = resolveButton("DialogueOkButton", width, height);
                        const GameplayResolvedHudLayoutElement cancel =
                            resolveButton("DialogueGoodbyeButton", width, height);
                        int pointWidth = 0;
                        int pointHeight = 0;
                        require(SDL_GetWindowSize(captureWindow(), &pointWidth, &pointHeight),
                            "Cannot query bank keyboard-area window points");
                        const SDL_Rect area = game.applicationTextInputArea(pointWidth, pointHeight);
                        require(area.w > 0 && area.h > 0, "Bank amount input has no positive keyboard-area union");
                        for (const GameplayResolvedHudLayoutElement &rect : {ok, cancel})
                        {
                            const float x = (rect.x + rect.width * 0.5f) * float(pointWidth) / float(width);
                            const float y = (rect.y + rect.height * 0.5f) * float(pointHeight) / float(height);
                            require(x >= area.x && x <= area.x + area.w && y >= area.y && y <= area.y + area.h,
                                "Bank keyboard-area union excludes a visible amount control");
                        }
                        bankInputAreaChecked = true;
                        trace << "{\"stage\":\"bank-amount-pointer-controls\",\"amount\":\"1\",\"input_area\":["
                            << area.x << ',' << area.y << ',' << area.w << ',' << area.h
                            << "],\"ok_button\":[" << ok.x << ',' << ok.y << ',' << ok.width << ',' << ok.height
                            << "],\"goodbye_button\":[" << cancel.x << ',' << cancel.y << ',' << cancel.width
                            << ',' << cancel.height << "]}\n";
                        capture("bank-amount-pointer-controls");
                        phase = "bank-amount-image";
                    }
                    else if (phase == "bank-amount-image" && pendingCapture.empty())
                    {
                        queueClick(resolveButton("DialogueOkButton", width, height), width, height);
                        ++bankSubmitClicks;
                        phase = "bank-submit";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "bank-submit" && frames >= readyFrame)
                    {
                        require(!screen().houseBankState().inputActive()
                            && screen().houseBankState().transactionPerformed
                            && party().gold() == bankCashBefore - 1 && party().bankGold() == bankBalanceBefore + 1,
                            "Visible Bank submit did not transfer exactly one gold through production accounting");
                        bankSubmitted = true;
                        baseline.party.gold = bankCashBefore - 1;
                        baseline.party.bankGold = bankBalanceBefore + 1;
                        baseline.partyBytes = encodePartySnapshotForComparison(baseline.party);
                        checkRetainedState(true, true, true);
                        trace << "{\"stage\":\"bank-pointer-submit-applied\",\"cash_before\":" << bankCashBefore
                            << ",\"cash_after\":" << party().gold() << ",\"bank_before\":" << bankBalanceBefore
                            << ",\"bank_after\":" << party().bankGold() << ",\"pointer_submit\":true}\n";
                        queueClick(resolveButton("DialogueGoodbyeButton", width, height), width, height);
                        ++bankCloseClicks;
                        phase = "bank-closed";
                        readyFrame = frames + 3;
                    }
                    else if (phase == "bank-closed" && frames >= readyFrame)
                    {
                        if (screen().activeEventDialog().isActive)
                        {
                            require(bankCloseClicks < 3, "Three visible Goodbye clicks did not leave the Bank");
                            queueClick(resolveButton("DialogueGoodbyeButton", width, height), width, height);
                            ++bankCloseClicks;
                            readyFrame = frames + 3;
                            return;
                        }
                        checkRetainedState(true, true, true);
                        require(captures == 5 && escapePairs == 4 && bankDepositClicks == 1 && bankSubmitClicks == 1
                            && bankSubmitted && bankInputAreaChecked && !screen().houseBankState().inputActive(),
                            "Bank pointer flow or combined capture counts are incomplete");
                        require(fileBytes(input) == originalBytes && fileBytes(copiedInput) == originalBytes
                            && fileBytes(incomingPath) == incomingBytes,
                            "The original input, unchanged input copy or incoming fixture changed during observation");
                        complete = true;
                        phase = "complete";
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal capture exit");
                    }
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete && elapsedMilliseconds() < 90000,
                "Actual UI continuation capture stopped before bounded completion");
            require(fileBytes(input) == originalBytes && fileBytes(copiedInput) == originalBytes,
                "Preserved input changed during application shutdown");
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
} // namespace OpenYAMM::Game

int main(int argc, char **argv)
{
    try
    {
        if (argc == 5 && std::string_view(argv[4]) == "--visual-only")
        {
            return OpenYAMM::Game::ApplicationCaptureTestAccess::previewVisuals(argv[1], argv[2], argv[3]);
        }
        if (argc == 5 && std::string_view(argv[4]) == "--certificate-only")
        {
            return OpenYAMM::Game::ApplicationCaptureTestAccess::previewVisuals(argv[1], argv[2], argv[3], true);
        }
        if (argc != 4)
        {
            throw std::runtime_error(
                "ui_continuation_capture RUNTIME READONLY_NORMAL132_MM7_SEED NEW_OUTPUT "
                "[--visual-only|--certificate-only]; "
                "the full lifecycle mode requires INSPECTION_RUNTIME_MISSING_7D24_ASSETS");
        }
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
