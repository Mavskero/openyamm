#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayItemService.h"
#include "game/gameplay/GameplaySaveLoadUiSupport.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/maps/SaveGame.h"
#include "game/ui/DialogueTopicLayout.h"
#include "game/ui/SaveSlotText.h"
#include "game/ui/Utf8Text.h"
#include "game/ui/screens/LoadGameScreen.h"
#include "game/ui/screens/NewGameScreen.h"
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <chrono>
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
        if (argc != 4 && argc != 5) throw std::runtime_error("ui_playability_capture RUNTIME INPUT NEW_OUTPUT [--remaining-only|--travel-rest]");
        if (argc == 5 && std::string(argv[4]) == "--travel-rest")
            return OpenYAMM::Game::ApplicationCaptureTestAccess::runTravel(argv[1], argv[2], argv[3]);
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(argv[1], argv[2], argv[3], argc == 5 && std::string(argv[4]) == "--remaining-only");
    }
    catch (const std::exception &exception) { std::cerr << exception.what() << '\n'; return 1; }
}
