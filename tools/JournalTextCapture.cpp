#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/JournalQuestRuntime.h"
#include "game/gameplay/StoryTextFormatter.h"
#include "game/maps/SaveGame.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/tables/JournalHistoryTable.h"

#include <CommonCrypto/CommonDigest.h>
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
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr size_t MaximumNextClicks = 64;
constexpr uint64_t MaximumMilliseconds = 180000;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string jsonText(std::string_view text)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char value : text)
    {
        if (value == '"' || value == '\\')
        {
            out << '\\' << char(value);
        }
        else if (value < 0x20)
        {
            out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
        }
        else
        {
            out << char(value);
        }
    }
    out << '"';
    return out.str();
}

std::string sha256(const std::vector<uint8_t> &bytes)
{
    require(bytes.size() <= std::numeric_limits<CC_LONG>::max(), "Digest input exceeds bounded file size");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), digest.data());
    std::ostringstream out;
    for (unsigned char byte : digest)
    {
        out << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    }
    return out.str();
}

std::string fileSha256(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot read file for digest: " + path.string());
    return sha256(std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()));
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream file(path);
    file << text;
    require(bool(file), "Cannot write evidence: " + path.string());
}

void writeCapture(const Engine::BgfxContext::ScreenshotCapture &capture, const std::filesystem::path &path)
{
    SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture.width), int(capture.height),
        SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(capture.bgraPixels.data()), int(capture.width * 4));
    require(pSurface != nullptr, "Capture surface creation failed");
    const bool written = SDL_SaveBMP(pSurface, path.string().c_str());
    SDL_DestroySurface(pSurface);
    require(written, "Cannot write capture: " + path.string());
}

struct SourceTexts
{
    std::string json;
    size_t historyCount = 0;
    size_t questCount = 0;
    std::array<size_t, 6> notesByCategory{};
};

SourceTexts sourceTexts(GameplayScreenRuntime &screen, const Party &party)
{
    const IGameplayWorldRuntime *pWorld = screen.worldRuntime();
    const EventRuntimeState *pState = pWorld != nullptr ? pWorld->eventRuntimeState() : nullptr;
    require(pState != nullptr && screen.journalHistoryTable() != nullptr, "Journal source state missing");
    SourceTexts result;
    std::ostringstream out;
    out << "{\"scope\":\"unlocked source text, not renderer page contents\",\"histories\":[";
    const auto &times = historyEventTimesForActiveContinent(*pState);
    for (const JournalHistoryEntry &entry :
        screen.journalHistoryTable()->entriesForContinent(screen.activeHistoryContinentId()))
    {
        const auto found = times.find(entry.id);
        if (found == times.end())
        {
            continue;
        }
        if (result.historyCount++ != 0)
        {
            out << ',';
        }
        out << "{\"id\":" << entry.id << ",\"unlocked_game_minutes\":" << found->second
            << ",\"title\":" << jsonText(entry.pageTitle)
            << ",\"formatted_text\":" << jsonText(StoryTextFormatter::format(entry.text, party, found->second)) << '}';
    }
    out << "],\"quests\":[";
    if (screen.journalQuestTable() != nullptr)
    {
        for (const std::string &text : buildCurrentQuestTexts(*screen.journalQuestTable(), party))
        {
            if (result.questCount++ != 0)
            {
                out << ',';
            }
            out << jsonText(text);
        }
    }
    out << "],\"notes\":[";
    bool firstNote = true;
    if (screen.journalAutonoteTable() != nullptr)
    {
        for (const JournalAutonoteEntry &entry : screen.journalAutonoteTable()->entries())
        {
            const auto found = pState->variables.find(entry.noteBit);
            if (found == pState->variables.end() || found->second == 0)
            {
                continue;
            }
            if (!firstNote)
            {
                out << ',';
            }
            firstNote = false;
            ++result.notesByCategory.at(size_t(entry.category));
            out << "{\"note_bit\":" << entry.noteBit << ",\"category\":" << unsigned(entry.category)
                << ",\"text\":" << jsonText(entry.text) << '}';
        }
    }
    out << "]}\n";
    result.json = out.str();
    return result;
}

void requireProgressUnchanged(const Party::Snapshot &before, const Party::Snapshot &after)
{
    require(before.questBits == after.questBits && before.eventVariables == after.eventVariables
        && before.gold == after.gold && before.bankGold == after.bankGold && before.food == after.food
        && before.everOwnedItemIds == after.everOwnedItemIds && before.npcTopicOverrides == after.npcTopicOverrides
        && before.npcHouseOverrides == after.npcHouseOverrides && before.nextMemberId == after.nextMemberId
        && before.monsterTargetSelectionCounter == after.monsterTargetSelectionCounter,
        "Journal viewing changed checked party progression or combat state");
    require(before.members.size() == after.members.size(), "Journal viewing changed party size");
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        const Character &a = before.members[index];
        const Character &b = after.members[index];
        require(a.memberId == b.memberId && a.experience == b.experience && a.level == b.level
            && a.health == b.health && a.spellPoints == b.spellPoints && a.conditions == b.conditions
            && a.awards == b.awards && a.eventVariables == b.eventVariables,
            "Journal viewing changed checked character progression or resources");
    }
}

enum class Phase
{
    OpenStory,
    StoryFirst,
    StorySecond,
    StoryBack,
    StorySweep,
    QuestFirst,
    QuestSweep,
    Notes,
    Closed,
    Complete,
};
constexpr std::array<const char *, 10> PhaseNames{
    "open-story", "story-first", "story-second", "story-back", "story-sweep",
    "quest-first", "quest-sweep", "notes", "closed", "complete"};
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path runtime, std::filesystem::path seed,
        std::filesystem::path output, const std::string &windowSize)
    {
        const auto started = std::chrono::steady_clock::now();
        const auto elapsedMilliseconds = [&]()
        {
            return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
        };
        runtime = std::filesystem::canonical(runtime);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Output directory must be new");
        int requestedWidth = 1210;
        int requestedHeight = 834;
        if (windowSize == "960x720")
        {
            requestedWidth = 960;
            requestedHeight = 720;
        }
        else if (windowSize == "1280x960")
        {
            requestedWidth = 1280;
            requestedHeight = 960;
        }
        else
        {
            require(windowSize == "1210x834", "Supported sizes: 960x720, 1210x834, 1280x960");
        }
        const std::string seedHash = fileSha256(seed);
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.skipEventCutscenes = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM journal150 controlled UI observation";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = requestedWidth;
        config.windowHeight = requestedHeight;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        writeText(output / "fixture.json", "{\"controlled_ui_driver\":true,\"natural_campaign\":false,"
            "\"renderer_required\":\"Metal\",\"audio\":\"dummy\",\"simulation_delta\":0,\"seed\":"
            + jsonText(seed.string()) + ",\"seed_sha256_before\":" + jsonText(seedHash)
            + ",\"requested_window\":" + jsonText(windowSize)
            + ",\"max_next_clicks_per_view\":64,\"timeout_ms\":180000,"
              "\"layout_page_count_available\":false,\"pixel_endpoint_is_observation_only\":true,"
              "\"text_conservation\":\"requires separate pure layout tests and visual review; not proved by pixels\","
              "\"progress_injection\":false,\"source_seed_opened_read_only\":true}\n");
        std::ofstream proof(output / "observations.jsonl");
        require(bool(proof), "Cannot open observation log");
        Phase phase = Phase::OpenStory;
        unsigned frames = 0;
        unsigned readyFrame = 12;
        unsigned readbacks = 0;
        unsigned savedCaptures = 0;
        unsigned stableComparisons = 0;
        size_t lastDistinctRequested = 0;
        size_t storyLastDistinct = 0;
        size_t questLastDistinct = 0;
        size_t nextClicksInView = 0;
        size_t storyNextClicks = 0;
        size_t questNextClicks = 0;
        size_t noteCategory = 0;
        int drawableWidth = 0;
        int drawableHeight = 0;
        double initialMinutes = 0.0;
        std::string initialMap;
        std::string pendingCapture;
        SourceTexts sources;
        Party::Snapshot initialParty;
        std::vector<uint8_t> firstStoryPixels;
        std::vector<uint8_t> previousPixels;
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto world = [&]() -> IGameplayWorldRuntime &
        {
            IGameplayWorldRuntime *pWorld = game.m_gameSession.activeWorldRuntime();
            require(pWorld != nullptr, "Loaded world is missing");
            return *pWorld;
        };
        const auto party = [&]() -> Party &
        {
            require(game.m_pMapSceneRuntime != nullptr, "Loaded scene is missing");
            return game.m_pMapSceneRuntime->party();
        };
        const auto requestedPage = [&]()
        {
            const auto &journal = screen().journalScreenState();
            if (journal.view == GameplayUiController::JournalView::Story)
            {
                return journal.storyPage;
            }
            return journal.view == GameplayUiController::JournalView::Notes ? journal.notesPage : journal.questPage;
        };
        const auto resolve = [&](const char *pId)
        {
            const auto *pLayout = screen().findHudLayoutElement(pId);
            require(pLayout != nullptr, std::string("Missing real journal layout: ") + pId);
            const auto result = screen().resolveHudLayoutElement(pId, drawableWidth, drawableHeight,
                pLayout->width, pLayout->height);
            require(result.has_value(), std::string("Cannot resolve real journal layout: ") + pId);
            return *result;
        };
        const auto sendInput = [&](GameplayInputFrame frame)
        {
            frame.screenWidth = drawableWidth;
            frame.screenHeight = drawableHeight;
            game.m_gameSession.updateGameplay(frame, 0.0f);
        };
        const auto click = [&](const char *pId)
        {
            const auto rect = resolve(pId);
            GameplayInputFrame press;
            press.pointerX = rect.x + rect.width * 0.5f;
            press.pointerY = rect.y + rect.height * 0.5f;
            press.leftMouseButton = {true, true, false};
            sendInput(press);
            press.leftMouseButton = {false, false, true};
            sendInput(press);
            sendInput({});
            proof << "{\"click\":" << jsonText(pId) << ",\"x\":" << press.pointerX
                << ",\"y\":" << press.pointerY << ",\"frame\":" << frames << "}\n";
            proof.flush();
        };
        const auto next = [&]()
        {
            const size_t before = requestedPage();
            require(nextClicksInView < MaximumNextClicks, "Visual endpoint not found within 64 Next clicks");
            click("JournalNextPageButton");
            ++nextClicksInView;
            require(requestedPage() == before + 1, "Real Next input did not advance the requested page");
            readyFrame = frames + 6;
        };
        const auto cropPixels = [&](const Engine::BgfxContext::ScreenshotCapture &capture)
        {
            require(capture.width == uint32_t(drawableWidth) && capture.height == uint32_t(drawableHeight),
                "Capture dimensions differ from resolved journal coordinates");
            require(capture.bgraPixels.size() == size_t(capture.width) * capture.height * 4,
                "Capture pixel buffer is incomplete");
            std::vector<uint8_t> pixels;
            for (const char *pId : {"JournalTitleText", "JournalTextViewport"})
            {
                const auto rect = resolve(pId);
                const int left = std::clamp(int(std::floor(rect.x)), 0, drawableWidth);
                const int right = std::clamp(int(std::ceil(rect.x + rect.width)), 0, drawableWidth);
                const int top = std::clamp(int(std::floor(rect.y)), 0, drawableHeight);
                const int bottom = std::clamp(int(std::ceil(rect.y + rect.height)), 0, drawableHeight);
                require(left < right && top < bottom, "Journal observation region is empty");
                if (readbacks == 1)
                {
                    proof << "{\"region\":" << jsonText(pId) << ",\"left\":" << left
                        << ",\"top\":" << top << ",\"right\":" << right << ",\"bottom\":" << bottom << "}\n";
                }
                for (int y = top; y < bottom; ++y)
                {
                    const size_t start = (size_t(y) * capture.width + size_t(left)) * 4;
                    const size_t end = (size_t(y) * capture.width + size_t(right)) * 4;
                    pixels.insert(pixels.end(), capture.bgraPixels.begin() + start, capture.bgraPixels.begin() + end);
                }
            }
            return pixels;
        };
        const auto saveImage = [&](const Engine::BgfxContext::ScreenshotCapture &capture, const char *pName)
        {
            const std::filesystem::path path = output / (std::string(pName) + ".bmp");
            writeCapture(capture, path);
            ++savedCaptures;
            proof << "{\"saved_capture\":" << jsonText(path.filename().string())
                << ",\"sha256\":" << jsonText(fileSha256(path)) << "}\n";
            proof.flush();
        };

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Journal capture requires Metal");
                require(game.initializeRenderer(), "Renderer initialization failed");
                require(game.loadSessionFromPath(seed), "Read-only journal seed could not load");
                require(game.m_screenManager.activeScreen() == nullptr, "Seed did not resume ordinary gameplay");
                initialMinutes = world().gameMinutes();
                initialMap = game.m_gameSession.currentMapFileName();
                require(game.quickSaveToPath(output / "loaded-before.oysav", "150 journal loaded baseline"),
                    "Cannot save fresh observation baseline");
                initialParty = party().snapshot();
                sources = sourceTexts(screen(), party());
                require(sources.historyCount != 0, "Seed has no unlocked history to inspect");
                writeText(output / "source-texts-before.json", sources.json);
                proof << std::setprecision(17) << "{\"loaded_map\":" << jsonText(initialMap)
                    << ",\"game_minutes\":" << initialMinutes << ",\"members\":" << initialParty.members.size()
                    << ",\"quest_bits\":" << initialParty.questBits.size() << "}\n";
                proof.flush();
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
                (void)wheel;
                (void)delta;
                require(elapsedMilliseconds() < MaximumMilliseconds, "Bounded journal capture timeout");
                if (drawableWidth == 0)
                {
                    drawableWidth = width;
                    drawableHeight = height;
                }
                require(width == drawableWidth && height == drawableHeight, "Window resized during fixed-size capture");
                require(world().gameMinutes() == initialMinutes, "Frozen capture advanced authoritative game time");
                require(game.m_gameSession.currentMapFileName() == initialMap, "Journal capture changed map");
                ++frames;
                if (phase == Phase::OpenStory && frames >= readyFrame)
                {
                    GameplayInputFrame input;
                    input.actions[keyboardActionIndex(KeyboardAction::History)] = {true, true, false};
                    sendInput(input);
                    sendInput({});
                    require(screen().journalScreenState().active
                        && screen().journalScreenState().view == GameplayUiController::JournalView::Story
                        && requestedPage() == 0, "History input did not open the first story page");
                    phase = Phase::StoryFirst;
                    readyFrame = frames + 6;
                }
                game.renderFrame(width, height, 0.0f, 0.0f);
                if (pendingCapture.empty() && phase != Phase::OpenStory && phase != Phase::Complete
                    && frames >= readyFrame)
                {
                    pendingCapture = "journal-readback-" + std::to_string(readbacks + 1);
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pendingCapture.c_str());
                }
                if (pendingCapture.empty())
                {
                    return;
                }
                const auto capture = Engine::BgfxContext::consumeScreenshot(pendingCapture);
                if (!capture)
                {
                    return;
                }
                pendingCapture.clear();
                ++readbacks;
                if (phase == Phase::Closed)
                {
                    require(!screen().journalScreenState().active && game.m_screenManager.activeScreen() == nullptr,
                        "Journal close did not restore ordinary gameplay");
                    saveImage(*capture, "closed-gameplay");
                    requireProgressUnchanged(initialParty, party().snapshot());
                    const SourceTexts after = sourceTexts(screen(), party());
                    writeText(output / "source-texts-after.json", after.json);
                    require(after.json == sources.json, "Unlocked journal source text changed during viewing");
                    require(world().gameMinutes() == initialMinutes, "Game clock changed after closing journal");
                    require(game.quickSaveToPath(output / "closed-after.oysav", "150 journal closed observation"),
                        "Cannot save closed observation");
                    require(fileSha256(seed) == seedHash, "Read-only seed bytes changed");
                    phase = Phase::Complete;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                    return;
                }
                std::vector<uint8_t> pixels = cropPixels(*capture);
                const bool same = !previousPixels.empty() && pixels == previousPixels;
                proof << "{\"phase\":" << jsonText(PhaseNames.at(size_t(phase)))
                    << ",\"requested_page\":" << requestedPage()
                    << ",\"journal_view\":" << unsigned(screen().journalScreenState().view)
                    << ",\"notes_category\":" << unsigned(screen().journalScreenState().notesCategory)
                    << ",\"region_sha256\":" << jsonText(sha256(pixels))
                    << ",\"same_as_previous\":" << (same ? "true" : "false") << ",\"frame\":" << frames << "}\n";
                proof.flush();
                if (phase == Phase::StoryFirst)
                {
                    firstStoryPixels = pixels;
                    saveImage(*capture, "story-first");
                    phase = Phase::StorySecond;
                    next();
                }
                else if (phase == Phase::StorySecond)
                {
                    require(pixels != firstStoryPixels, "Seed does not demonstrate two distinct rendered story pages");
                    saveImage(*capture, "story-second");
                    previousPixels = std::move(pixels);
                    lastDistinctRequested = requestedPage();
                    click("JournalPrevPageButton");
                    require(requestedPage() == 0, "Real Previous input did not return to story page zero");
                    phase = Phase::StoryBack;
                    readyFrame = frames + 6;
                }
                else if (phase == Phase::StoryBack)
                {
                    require(pixels == firstStoryPixels, "Previous did not restore the original title and body pixels");
                    proof << "{\"story_back_pixels_match_first\":true}\n";
                    phase = Phase::StorySweep;
                    next();
                    next(); // Both transitions use the same real press/release handler, with no state assignment.
                }
                else if (phase == Phase::QuestFirst)
                {
                    saveImage(*capture, "quests-first");
                    previousPixels = std::move(pixels);
                    lastDistinctRequested = requestedPage();
                    stableComparisons = 0;
                    phase = Phase::QuestSweep;
                    next();
                }
                else if (phase == Phase::StorySweep || phase == Phase::QuestSweep)
                {
                    stableComparisons = same ? stableComparisons + 1 : 0;
                    if (!same)
                    {
                        lastDistinctRequested = requestedPage();
                    }
                    previousPixels = std::move(pixels);
                    if (stableComparisons < 2)
                    {
                        next();
                        return;
                    }
                    proof << "{\"observed_visual_endpoint\":true,\"authoritative_page_count\":null,"
                        "\"last_distinct_requested_page\":" << lastDistinctRequested
                        << ",\"endpoint_requested_page\":" << requestedPage()
                        << ",\"next_clicks\":" << nextClicksInView
                        << ",\"stable_comparisons\":2,\"journal_view\":"
                        << unsigned(screen().journalScreenState().view) << "}\n";
                    proof.flush();
                    if (phase == Phase::StorySweep)
                    {
                        storyLastDistinct = lastDistinctRequested;
                        storyNextClicks = nextClicksInView;
                        saveImage(*capture, "story-observed-last");
                        click("JournalMainViewQuests");
                        require(screen().journalScreenState().view == GameplayUiController::JournalView::Quests
                            && requestedPage() == 0, "Quest tab input did not open its first page");
                        phase = Phase::QuestFirst;
                        nextClicksInView = 0;
                    }
                    else
                    {
                        questLastDistinct = lastDistinctRequested;
                        questNextClicks = nextClicksInView;
                        saveImage(*capture, "quests-observed-last");
                        click("JournalMainViewNotes");
                        const std::array<const char *, 6> buttons{
                            "JournalNotesPotionButton", "JournalNotesFountainButton", "JournalNotesObeliskButton",
                            "JournalNotesSeerButton", "JournalNotesMiscButton", "JournalNotesTrainerButton"};
                        for (size_t index = 0; index < sources.notesByCategory.size(); ++index)
                        {
                            if (sources.notesByCategory[index] != 0)
                            {
                                noteCategory = index;
                                break;
                            }
                        }
                        click(buttons[noteCategory]);
                        require(screen().journalScreenState().view == GameplayUiController::JournalView::Notes
                            && size_t(screen().journalScreenState().notesCategory) == noteCategory,
                            "Notes tab/category input failed");
                        phase = Phase::Notes;
                    }
                    previousPixels.clear();
                    stableComparisons = 0;
                    readyFrame = frames + 6;
                }
                else if (phase == Phase::Notes)
                {
                    saveImage(*capture, sources.notesByCategory[noteCategory] == 0 ? "notes-empty" : "notes-existing");
                    click("JournalCloseButton");
                    require(!screen().journalScreenState().active, "Actual journal close button did not close");
                    phase = Phase::Closed;
                    readyFrame = frames + 12;
                }
            },
            [&]() { game.shutdownApplication(); });
        require(application.run() == 0 && phase == Phase::Complete, "Journal capture did not complete");
        require(savedCaptures == 7, "Required bounded screenshot set incomplete");
        std::ostringstream result;
        result << std::setprecision(17)
            << "{\"status\":\"passed\",\"scope\":\"controlled UI navigation and observed pixel endpoints\","
            "\"visual_acceptance\":\"pending manual review\",\"authoritative_page_count\":null,"
            "\"text_conservation\":\"requires separate pure layout tests and visual review\","
            "\"seed_sha256_before\":" << jsonText(seedHash) << ",\"seed_sha256_after\":" << jsonText(fileSha256(seed))
            << ",\"drawable_width\":" << drawableWidth << ",\"drawable_height\":" << drawableHeight
            << ",\"frames\":" << frames << ",\"readbacks\":" << readbacks << ",\"saved_captures\":" << savedCaptures
            << ",\"story_last_distinct_requested_page\":" << storyLastDistinct
            << ",\"quest_last_distinct_requested_page\":" << questLastDistinct
            << ",\"story_next_clicks\":" << storyNextClicks << ",\"quest_next_clicks\":" << questNextClicks
            << ",\"unlocked_history_count\":" << sources.historyCount
            << ",\"current_quest_count\":" << sources.questCount
            << ",\"observed_notes_category\":" << noteCategory
            << ",\"observed_notes_count\":" << sources.notesByCategory[noteCategory]
            << ",\"game_minutes_before_and_after\":" << initialMinutes
            << ",\"game_minutes_unchanged\":true,\"checked_party_progress_unchanged\":true,"
            "\"unlocked_source_text_unchanged\":true,\"journal_closed\":true,\"progress_injection\":false,"
            "\"natural_campaign\":false,\"elapsed_ms\":" << elapsedMilliseconds() << "}\n";
        writeText(output / "result.json", result.str());
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
            throw std::runtime_error("journal_text_capture RUNTIME READONLY_SEED NEW_OUTPUT "
                "960x720|1210x834|1280x960");
        }
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
