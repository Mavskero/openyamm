#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/screens/CutsceneVideoScreen.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

SDL_Window *captureWindow()
{
    int count = 0;
    SDL_Window **pWindows = SDL_GetWindows(&count);
    SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
    SDL_free(pWindows);
    require(pWindow != nullptr, "Expected one actual capture window");
    return pWindow;
}

void queueClick(float x, float y, int width, int height, bool worldGesture)
{
    SDL_Window *pWindow = captureWindow();
    int pointWidth = 0, pointHeight = 0;
    require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot query capture window size");
    const auto edge = [&](uint8_t button, bool down)
    {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = SDL_GetWindowID(pWindow);
        event.button.button = button;
        event.button.down = down;
        event.button.clicks = 1;
        event.button.x = x * float(pointWidth) / float(width);
        event.button.y = y * float(pointHeight) / float(height);
        require(SDL_PushEvent(&event), "Cannot queue ordinary SDL mouse edge");
    };
    if (worldGesture) edge(SDL_BUTTON_RIGHT, true);
    edge(SDL_BUTTON_LEFT, true);
    edge(SDL_BUTTON_LEFT, false);
    if (worldGesture) edge(SDL_BUTTON_RIGHT, false);
}

void queueEscape()
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(captureWindow());
    event.key.key = SDLK_ESCAPE;
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    event.key.down = true;
    require(SDL_PushEvent(&event), "Cannot queue movie Escape down");
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    require(SDL_PushEvent(&event), "Cannot queue movie Escape up");
}

GameplayScreenRuntime::ResolvedHudLayoutElement layout(
    GameplayScreenRuntime &screen, const std::string &id, int width, int height)
{
    const auto *pLayout = screen.findHudLayoutElement(id);
    require(pLayout != nullptr, "Missing actual dialogue layout: " + id);
    const auto resolved = screen.resolveHudLayoutElement(id, width, height, pLayout->width, pLayout->height);
    require(resolved.has_value(), "Cannot resolve actual dialogue layout: " + id);
    return *resolved;
}

size_t actionIndex(GameplayScreenRuntime &screen, EventDialogActionKind kind, uint32_t id)
{
    const auto &dialog = screen.activeEventDialog();
    require(dialog.isActive, "Expected an active original dialogue");
    const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(), [&](const auto &action)
    {
        return action.kind == kind && action.id == id;
    });
    require(found != dialog.actions.end() && found->enabled && !found->textOnly && !found->label.empty(),
        "Required visible dialogue action is absent: " + std::to_string(id));
    return size_t(found - dialog.actions.begin());
}

std::array<float, 2> dialogueActionPoint(
    GameplayScreenRuntime &screen, size_t index, int width, int height)
{
    const auto &dialog = screen.activeEventDialog();
    const auto panel = layout(screen, "DialogueEventDialog", width, height);
    const auto portrait = layout(screen, "DialogueNpcPortrait", width, height);
    const auto name = layout(screen, "DialogueNpcName", width, height);
    const auto title = layout(screen, "DialogueHouseTitle", width, height);
    const float scale = panel.scale;
    const float padding = 10.0f * scale;
    const float gap = 8.0f * scale;
    const auto titleHeight = [&](const std::string &id, const std::string &label, float baseHeight)
    {
        const auto *pLayout = screen.findHudLayoutElement(id);
        const auto font = screen.findHudFont(pLayout->fontName);
        if (!font || label.empty()) return baseHeight;
        const float fontScale = GameplayHudCommon::snappedHudFontScale(scale * std::max(0.1f, pLayout->textScale));
        const float textWidth = std::max(0.0f,
            panel.width - 8.0f * scale - std::abs(pLayout->textPadX * scale) * 2.0f) / std::max(1.0f, fontScale);
        const size_t lines = std::max<size_t>(1, screen.wrapHudTextToWidth(*font, label, textWidth).size());
        return std::max(baseHeight,
            (float(font->fontHeight) + float(lines - 1) * float(std::max(1, font->fontHeight - 3))) * fontScale);
    };
    const auto *pHouse = screen.houseTable()->get(1244);
    const std::string houseTitle = !dialog.houseTitle.empty() ? dialog.houseTitle : (pHouse ? pHouse->name : "");
    const float portraitTop = std::round(std::max(panel.y + padding
        + titleHeight("DialogueHouseTitle", houseTitle, title.height) + 2.0f * scale, portrait.y));
    const bool residents = std::all_of(dialog.actions.begin(), dialog.actions.end(), [](const auto &action)
    {
        return action.kind == EventDialogActionKind::HouseResident
            || action.kind == EventDialogActionKind::HouseProprietor;
    });
    if (residents)
    {
        const bool columns = dialog.actions.size() > 3;
        const size_t column = columns ? index % 2 : 0;
        const size_t row = columns ? index / 2 : index;
        const float columnWidth = (panel.width - padding * 2.0f) / (columns ? 2.0f : 1.0f);
        return {panel.x + padding + (float(column) + 0.5f) * columnWidth,
            portraitTop + float(row) * (portrait.height + (columns ? 0.0f : name.height) + gap)
                + portrait.height * 0.5f};
    }
    const auto topicRow = layout(screen, "DialogueTopicRow_1", width, height);
    const auto *pTopicLayout = screen.findHudLayoutElement("DialogueTopicRow_1");
    const auto font = screen.findHudFont(pTopicLayout->fontName);
    require(font.has_value(), "Missing actual topic font");
    const float fontScale = GameplayHudCommon::snappedHudFontScale(topicRow.scale);
    const float textWidth = std::max(0.0f,
        (std::min(topicRow.width, 140.0f * topicRow.scale)
            - std::abs(pTopicLayout->textPadX * fontScale) * 2.0f - 4.0f * fontScale)
            / std::max(1.0f, fontScale));
    const size_t visible = std::min<size_t>(dialog.actions.size(), 5);
    require(index < visible, "Required topic is outside the visible rows");
    float total = 0, offset = 0, targetHeight = 0;
    for (size_t row = 0; row < visible; ++row)
    {
        std::string label = dialog.actions[row].label;
        if (!dialog.actions[row].enabled && !dialog.actions[row].disabledReason.empty()) label += " [disabled]";
        const size_t lines = std::max<size_t>(1, screen.wrapHudTextToWidth(*font, label, textWidth).size());
        const float rowHeight = std::max(topicRow.height, float(lines * font->fontHeight) * fontScale);
        if (row < index) offset += rowHeight + 4.0f * scale;
        if (row == index) targetHeight = rowHeight;
        total += rowHeight + (row + 1 < visible ? 4.0f * scale : 0.0f);
    }
    const float contentY = portraitTop + name.y - portrait.y
        + titleHeight("DialogueNpcName", dialog.title, name.height) + gap;
    const float available = panel.y + panel.height - padding - contentY;
    return {topicRow.x + topicRow.width * 0.5f,
        contentY + std::max(0.0f, (available - total) * 0.5f) - gap + offset + targetHeight * 0.5f};
}
}

// One controlled source hand-in. All event and topic dispatch comes from ordinary SDL input.
struct GameApplicationTestAccess
{
    static int run(std::filesystem::path runtime, std::filesystem::path input, std::filesystem::path output)
    {
        runtime = std::filesystem::canonical(runtime);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(runtime / "engine.zip")
            && std::filesystem::is_regular_file(runtime / "worlds/mm6.zip"), "Complete MM6 runtime required");
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(runtime).generic_string().starts_with("../"),
            "Use a new NVMe output directory outside the read-only runtime");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && !source->party.members.empty() && !source->heldInventoryItemActive
            && source->party.pendingItemDeliveries.empty(), "Expected the normal132 empty-handed MM6 save: " + error);
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot preserve the input save");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":164,\"controlled\":true,\"natural_campaign\":false,\"input\":"
            << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(runtime.string())
            << ",\"changes\":\"Load an unchanged copy of normal132 MM6; debug-map jump to outd3.odm. "
               "Require no item2081 or2164, then place exactly one real-table bell2081 in a member inventory. "
               "Declare upstream Q1201=false,Q1222=false,Q1221=true,Q1121=true,Q1259=true. "
               "Keep actual NPC797 house1244/topic1343 tables and runtime overrides unchanged. "
               "Locate a clickable original event42 face and position the party256 units along its normal, "
               "foot at face bottom+1, aiming at its center. One ordinary SDL world gesture opens the door. "
               "The source event queues archie and house1244; ordinary Escape closes the movie. "
               "Click NPC797 if the actual house first shows its resident selector, then click visible1343 once. "
               "Use the existing held-item placement API to put the earned2164 into party inventory. "
               "Freeze simulation before and during dialogue; movie and30 post-dialogue frames use normal delta. "
               "No actor changes, direct event execution, synthetic NPC requests, combat route or cold replay.\","
               "\"screenshots\":[\"archie-movie\",\"before-topic1343\",\"continued-game\"],"
               "\"audio\":\"dummy\",\"movie_visual_review_required\":true,\"new_unit_tests\":0}\n";

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = runtime.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.debugConsole = false;
        settings.skipEventCutscenes = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot save capture settings: " + error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Archibald164";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(9);
        enum class Phase { Warmup, DoorClicked, Movie, MovieClosing, House, ResidentClicked,
            TopicReady, TopicClicked, Continuing, Finished };
        Phase phase = Phase::Warmup;
        uint64_t started = 0, movieStartedAt = 0, bellMemberId = 0;
        unsigned frames = 0, doorFrame = 0, movieFrame = 0, nextMovieCapture = 0, escapeFrame = 0;
        unsigned residentFrame = 0, topicFrame = 0, normalFrames = 0, captures = 0, blankCaptures = 0;
        double normalSeconds = 0;
        bool movieStarted = false, movieCaptured = false, movieClosed = false, topicVisible = false;
        bool complete = false, savedVerified = false;
        size_t doorModel = 0, doorFace = 0, movieNonblackPixels = 0;
        bx::Vec3 doorCenter = {0, 0, 0};
        std::vector<std::pair<uint64_t, uint32_t>> experienceBefore;
        int goldBefore = 0;
        std::string pendingCapture;
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && game.m_gameSession.currentMapFileName() == "outd3.odm", "Expected the actual outd3 outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto verifyReward = [&](const Party::Snapshot &party)
        {
            require(party.questBits.contains(1201) && party.questBits.contains(1222)
                && !party.questBits.contains(1221) && !party.questBits.contains(1121)
                && !party.questBits.contains(1259), "Original Archibald quest transaction is incomplete");
            require(party.gold == goldBefore && party.members.size() == experienceBefore.size(),
                "The hand-in changed Gold or party size");
            for (size_t index = 0; index < party.members.size(); ++index)
            {
                require(party.members[index].memberId == experienceBefore[index].first
                    && uint64_t(party.members[index].experience) == uint64_t(experienceBefore[index].second) + 50000,
                    "The original50000 XP reward or member identity is wrong");
            }
            require(party.npcHouseOverrides.contains(797) && party.npcHouseOverrides.at(797) == 0,
                "Archibald was not removed from his house");
            Party restored;
            restored.restoreSnapshot(party);
            require(restored.inventoryItemCount(2164) == 1 && restored.inventoryItemCount(2081) == 1,
                "Earned scroll2164 or retained bell2081 has the wrong inventory quantity");
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto logDialog = [&](const std::string &stage)
        {
            const auto &dialog = screen().activeEventDialog();
            trace << "{\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"source_id\":" << dialog.sourceId << ",\"is_house\":" << dialog.isHouseDialog
                << ",\"host_house\":" << scene().eventRuntimeState()->dialogueState.hostHouseId
                << ",\"title\":" << std::quoted(dialog.title) << ",\"actions\":[";
            for (size_t index = 0; index < dialog.actions.size(); ++index)
            {
                if (index) trace << ',';
                const auto &action = dialog.actions[index];
                trace << "{\"kind\":" << int(action.kind) << ",\"id\":" << action.id
                    << ",\"label\":" << std::quoted(action.label) << ",\"enabled\":" << action.enabled << '}';
            }
            trace << "]}\n";
        };
        const auto clickDialog = [&](EventDialogActionKind kind, uint32_t id, int width, int height)
        {
            const size_t index = actionIndex(screen(), kind, id);
            const auto point = dialogueActionPoint(screen(), index, width, height);
            logDialog("before-ordinary-dialogue-click");
            queueClick(point[0], point[1], width, height, false);
            trace << "{\"stage\":\"queued-dialogue-click\",\"id\":" << id << ",\"index\":" << index
                << ",\"pixel\":[" << point[0] << ',' << point[1] << "]}\n";
        };
        const auto verifyTopic = [&]()
        {
            const auto &dialog = screen().activeEventDialog();
            require(dialog.isActive && !dialog.isHouseDialog && dialog.sourceId == 797
                && scene().eventRuntimeState()->dialogueState.hostHouseId == 1244,
                "Original house1244 did not present NPC797");
            const size_t index = actionIndex(screen(), EventDialogActionKind::NpcTopic, 1343);
            const auto topic = screen().npcDialogTable()->getTopicById(1343);
            require(topic && dialog.actions[index].label == topic->topic,
                "The visible1343 label is not from the actual NPC table");
            require(!scene().party().hasQuestBit(1201) && !scene().party().hasItemAnywhere(2164),
                "The entrance granted the topic reward before its real click");
            topicVisible = true;
        };
        const auto result = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"passed\":" << passed << ",\"controlled\":true,\"natural_campaign\":false"
                << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"event42_ui_clicks\":" << (doorFrame ? 1 : 0)
                << ",\"resident797_ui_clicks\":" << (residentFrame ? 1 : 0)
                << ",\"topic1343_ui_clicks\":" << (topicFrame ? 1 : 0)
                << ",\"door_model\":" << doorModel << ",\"door_face\":" << doorFace
                << ",\"expected_source_movie\":\"archie\",\"cutscene_started\":" << movieStarted
                << ",\"cutscene_start_frame\":" << movieFrame
                << ",\"movie_capture_saved\":" << movieCaptured << ",\"movie_closed_by_escape\":" << movieClosed
                << ",\"movie_nonblack_pixels\":" << movieNonblackPixels
                << ",\"blank_movie_captures_discarded\":" << blankCaptures
                << ",\"movie_visual_review_required\":true,\"source_topic_visible\":" << topicVisible
                << ",\"bell_fixture_member_id\":" << bellMemberId << ",\"normal_frames\":" << normalFrames
                << ",\"normal_seconds\":" << normalSeconds << ",\"final_save_verified\":" << savedVerified
                << ",\"failure\":" << std::quoted(failure) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "source-input.oysav"),
                        "Cannot load the unchanged input copy");
                    const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName("outd3.odm");
                    require(pMap != nullptr, "Castle Ironfist is absent from MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared outd3 map fixture failed");
                    Party &party = scene().party();
                    require(!party.hasItemAnywhere(2081) && !party.hasItemAnywhere(2164),
                        "Normal seed already owns a bell or ritual scroll");
                    const NpcEntry *pNpc = game.m_gameDataLoader.getNpcDialogTable().getNpc(797);
                    require(pNpc && pNpc->houseId == 1244
                        && std::find(pNpc->topicIds.begin(), pNpc->topicIds.end(), 1343) != pNpc->topicIds.end(),
                        "Original NPC797 house1244/topic1343 is absent; no NPC fixture is permitted");
                    const auto before = party.snapshot();
                    require(!before.npcHouseOverrides.contains(797) || before.npcHouseOverrides.at(797) == 1244,
                        "Input already moved Archibald away from the original house");
                    const ItemTable &items = game.m_gameDataLoader.getItemTable();
                    require(items.get(2081) != nullptr, "Original bell2081 is absent from the loaded item table");
                    const InventoryItem bell = ItemGenerator::makeInventoryItem(
                        2081, items, ItemGenerationMode::Generic);
                    for (size_t index = 0; index < party.memberCount(); ++index)
                    {
                        if (party.tryAutoPlaceItemInMemberInventory(index, bell))
                        {
                            bellMemberId = party.member(index)->memberId;
                            break;
                        }
                    }
                    require(bellMemberId != 0 && party.inventoryItemCount(2081) == 1,
                        "Cannot place exactly one bell2081 in an existing member inventory");
                    party.setQuestBit(1201, false);
                    party.setQuestBit(1222, false);
                    for (uint32_t bit : {1221u, 1121u, 1259u}) party.setQuestBit(bit, true);
                    goldBefore = party.gold();
                    for (const Character &member : party.members())
                        experienceBefore.emplace_back(member.memberId, member.experience);
                    const auto &selected = game.m_gameDataLoader.getSelectedMap();
                    require(selected && selected->outdoorMapData, "Missing original outd3 model geometry");
                    const auto &models = selected->outdoorMapData->bmodels;
                    bool found = false;
                    for (size_t modelIndex = 0; modelIndex < models.size() && !found; ++modelIndex)
                    {
                        const auto &model = models[modelIndex];
                        for (size_t faceIndex = 0; faceIndex < model.faces.size(); ++faceIndex)
                        {
                            const auto &face = model.faces[faceIndex];
                            if (face.cogTriggeredNumber != 42 || face.vertexIndices.empty()
                                || !hasFaceAttribute(face.attributes, FaceAttribute::Clickable)
                                || hasFaceAttribute(face.attributes, FaceAttribute::Invisible)
                                || hasFaceAttribute(face.attributes, FaceAttribute::HasHint)) continue;
                            const float nx = float(face.planeNormalX), ny = float(face.planeNormalY);
                            const float length = std::hypot(nx, ny);
                            if (length == 0) continue;
                            float bottom = std::numeric_limits<float>::max();
                            for (uint16_t index : face.vertexIndices)
                            {
                                const auto &vertex = model.vertices.at(index);
                                doorCenter = bx::add(doorCenter, {float(vertex.x), float(vertex.y), float(vertex.z)});
                                bottom = std::min(bottom, float(vertex.z));
                            }
                            doorCenter = bx::mul(doorCenter, 1.0f / float(face.vertexIndices.size()));
                            scene().partyRuntime().teleportTo(
                                doorCenter.x + nx / length * 256, doorCenter.y + ny / length * 256, bottom + 1);
                            const auto &pose = scene().partyRuntime().movementState();
                            game.m_outdoorGameView.setCameraAngles(
                                std::atan2(doorCenter.y - pose.y, doorCenter.x - pose.x),
                                std::atan2(doorCenter.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                                    std::hypot(doorCenter.x - pose.x, doorCenter.y - pose.y)));
                            doorModel = modelIndex;
                            doorFace = faceIndex;
                            found = true;
                            trace << "{\"stage\":\"declared-door-pose\",\"event\":42,\"model\":" << doorModel
                                << ",\"face\":" << doorFace << ",\"pose\":[" << pose.x << ',' << pose.y << ','
                                << pose.footZ << "],\"bell_member_id\":" << bellMemberId << ",\"members\":[";
                            for (size_t i = 0; i < experienceBefore.size(); ++i)
                            {
                                if (i) trace << ',';
                                trace << "{\"member_id\":" << experienceBefore[i].first
                                    << ",\"xp\":" << experienceBefore[i].second << '}';
                            }
                            trace << "]}\n";
                            break;
                        }
                    }
                    require(found, "No original clickable event42 door face was found");
                    require(!screen().activeEventDialog().isActive && !scene().eventRuntimeState()->pendingMovie,
                        "Fixture unexpectedly opened a dialog or movie");
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 1800 && SDL_GetTicks() - started < 45000,
                        "Bounded Archibald capture timed out");
                    const bool normalGameplay = phase == Phase::Continuing;
                    const bool normalDelta = normalGameplay || phase == Phase::Finished
                        || dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr;
                    game.renderFrame(width, height, wheel, normalDelta ? delta : 0.0f);
                    auto *pMovie = dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen());
                    require(game.m_screenManager.activeScreen() == nullptr
                        || (pMovie && (phase == Phase::DoorClicked || phase == Phase::Movie
                            || phase == Phase::MovieClosing)), "Unexpected full-screen interruption");
                    if (phase == Phase::Movie)
                        require(pMovie && SDL_GetTicks() - movieStartedAt < 10000,
                            "Archie closed or exceeded the ten-second screenshot deadline");
                    if (normalGameplay)
                    {
                        require(!screen().activeEventDialog().isActive && !pMovie && scene().party().hasActableMember(),
                            "Post-hand-in normal gameplay was interrupted");
                        if (delta > 0) { ++normalFrames; normalSeconds += delta; }
                    }
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            const bool movieImage = pendingCapture == "archie-movie";
                            if (movieImage)
                            {
                                require(pMovie != nullptr, "Movie ended before its screenshot was collected");
                                movieNonblackPixels = 0;
                                for (size_t p = 0; p + 2 < image->bgraPixels.size(); p += 4)
                                    if (image->bgraPixels[p] > 16 || image->bgraPixels[p + 1] > 16
                                        || image->bgraPixels[p + 2] > 16) ++movieNonblackPixels;
                                // Reject blank images only; a human reviews the actual movie content.
                                if (movieNonblackPixels < size_t(image->width) * image->height / 100)
                                {
                                    ++blankCaptures;
                                    pendingCapture.clear();
                                    nextMovieCapture = frames + 15;
                                    trace << "{\"stage\":\"blank-movie-capture-discarded\",\"frame\":"
                                        << frames << "}\n";
                                    return;
                                }
                            }
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create screenshot surface");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save capture image");
                            trace << "{\"capture\":" << std::quoted(pendingCapture) << "}\n";
                            if (movieImage) movieCaptured = true;
                            pendingCapture.clear();
                            ++captures;
                        }
                        return;
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        auto &world = scene().worldRuntime();
                        const auto camera = world.buildWorldPickRequest({0, 0, width, height, false});
                        float viewProjection[16];
                        bx::mtxMul(viewProjection, camera.viewMatrix.data(), camera.projectionMatrix.data());
                        const auto &model = world.mapData()->bmodels.at(doorModel);
                        std::vector<bx::Vec3> points{doorCenter};
                        for (uint16_t index : model.faces.at(doorFace).vertexIndices)
                        {
                            if (points.size() == 9) break;
                            const auto &v = model.vertices.at(index);
                            points.push_back(bx::add(doorCenter,
                                bx::mul(bx::sub({float(v.x), float(v.y), float(v.z)}, doorCenter), 0.65f)));
                        }
                        std::optional<std::array<float, 2>> pixel;
                        for (const auto &point : points)
                        {
                            const auto projected = bx::mulH(point, viewProjection);
                            const float x = (projected.x + 1) * float(width) * 0.5f;
                            const float y = (1 - projected.y) * float(height) * 0.5f;
                            if (x < 0 || y < 0 || x >= width || y >= height) continue;
                            const auto hit = world.pickMouseInteractionTarget(
                                world.buildWorldPickRequest({x, y, width, height, true}));
                            if (hit.eventTarget && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                                && hit.eventTarget->targetIndex == doorModel
                                && hit.eventTarget->secondaryIndex == doorFace
                                && hit.eventTarget->triggeredEventId == 42
                                && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                            {
                                pixel = std::array<float, 2>{x, y};
                                break;
                            }
                        }
                        require(pixel.has_value(), "Original event42 door is not reachable by the real mouse picker");
                        queueClick((*pixel)[0], (*pixel)[1], width, height, true);
                        doorFrame = frames;
                        trace << "{\"stage\":\"queued-real-event42-click\",\"model\":" << doorModel
                            << ",\"face\":" << doorFace << ",\"pixel\":["
                            << (*pixel)[0] << ',' << (*pixel)[1] << "]}\n";
                        phase = Phase::DoorClicked;
                    }
                    else if (phase == Phase::DoorClicked)
                    {
                        if (pMovie)
                        {
                            require(!scene().party().hasQuestBit(1201) && !scene().party().hasItemAnywhere(2164),
                                "The entrance bypassed the original NPC hand-in");
                            movieStarted = true;
                            movieFrame = frames;
                            movieStartedAt = SDL_GetTicks();
                            nextMovieCapture = frames + 65;
                            trace << "{\"stage\":\"actual-cutscene-started\",\"source_event\":42,"
                                "\"expected_source_movie\":\"archie\",\"frame\":" << frames << "}\n";
                            phase = Phase::Movie;
                        }
                        else require(frames < doorFrame + 20, "Real event42 click did not open archie");
                    }
                    else if (phase == Phase::Movie)
                    {
                        if (movieCaptured)
                        {
                            queueEscape();
                            escapeFrame = frames;
                            phase = Phase::MovieClosing;
                        }
                        else if (frames >= nextMovieCapture) capture("archie-movie");
                    }
                    else if (phase == Phase::MovieClosing)
                    {
                        if (!pMovie)
                        {
                            movieClosed = true;
                            logDialog("movie-closed-existing-house-dialog");
                            phase = Phase::House;
                        }
                        else require(frames < escapeFrame + 20, "Ordinary Escape did not close archie");
                    }
                    else if (phase == Phase::House)
                    {
                        const auto &dialog = screen().activeEventDialog();
                        require(dialog.isActive, "Movie return lost the original house dialogue");
                        if (dialog.isHouseDialog)
                        {
                            require(dialog.sourceId == 1244, "The bell did not enter house1244");
                            clickDialog(EventDialogActionKind::HouseResident, 797, width, height);
                            residentFrame = frames;
                            phase = Phase::ResidentClicked;
                        }
                        else
                        {
                            verifyTopic();
                            logDialog("source1343-visible");
                            capture("before-topic1343");
                            phase = Phase::TopicReady;
                        }
                    }
                    else if (phase == Phase::ResidentClicked && frames >= residentFrame + 4)
                    {
                        verifyTopic();
                        logDialog("source1343-visible");
                        capture("before-topic1343");
                        phase = Phase::TopicReady;
                    }
                    else if (phase == Phase::TopicReady)
                    {
                        verifyTopic();
                        require(topicFrame == 0, "Only one source1343 hand-in is allowed");
                        clickDialog(EventDialogActionKind::NpcTopic, 1343, width, height);
                        topicFrame = frames;
                        phase = Phase::TopicClicked;
                    }
                    else if (phase == Phase::TopicClicked && frames >= topicFrame + 6)
                    {
                        require(scene().party().hasItemAnywhere(2164),
                            "Real1343 click did not grant ritual scroll2164");
                        require(GameplayHeldItemController::tryDisplaceHeldInventoryItem(screen()),
                            "The earned ritual scroll cannot be placed in ordinary inventory");
                        verifyReward(scene().party().snapshot());
                        require(scene().eventRuntimeState()->npcHouseOverrides.contains(797)
                            && scene().eventRuntimeState()->npcHouseOverrides.at(797) == 0,
                            "Live Archibald house removal is missing");
                        logDialog("source1343-reward-verified");
                        screen().closeActiveEventDialog();
                        phase = Phase::Continuing;
                    }
                    else if (phase == Phase::Continuing && normalFrames >= 30)
                    {
                        verifyReward(scene().party().snapshot());
                        const auto &events = *scene().eventRuntimeState();
                        require(!events.pendingMovie && !events.pendingWinGame && !events.pendingReturnToMainMenu,
                            "Post-hand-in gameplay retained an unexpected transition");
                        const auto finalPath = output / "archibald-scroll-complete.oysav";
                        require(game.quickSaveToPath(finalPath, "164阿基巴德卷轴后继续游戏"), "Cannot save final gameplay");
                        const auto saved = loadGameDataFromPath(finalPath, error);
                        require(saved && saved->mapFileName == "outd3.odm" && !saved->heldInventoryItemActive
                            && saved->party.pendingItemDeliveries.empty() && saved->outdoorWorld.eventRuntimeState,
                            "Final save is missing its ordinary outdoor state: " + error);
                        verifyReward(saved->party);
                        const auto &houses = saved->outdoorWorld.eventRuntimeState->npcHouseOverrides;
                        require(houses.contains(797) && houses.at(797) == 0, "Final save lost Archibald removal");
                        savedVerified = true;
                        trace << "{\"stage\":\"continued-game-saved\",\"normal_frames\":" << normalFrames
                            << ",\"normal_seconds\":" << normalSeconds
                            << ",\"Q1201\":true,\"Q1222\":true,\"Q1221\":false,\"Q1121\":false,\"Q1259\":false"
                               ",\"scroll2164\":1,\"npc797_house\":0,\"xp_each\":50000}\n";
                        capture("continued-game");
                        phase = Phase::Finished;
                    }
                    else if (phase == Phase::Finished)
                    {
                        require(captures == 3 && doorFrame && topicFrame && movieStarted && movieCaptured
                            && movieClosed && topicVisible && normalFrames >= 30 && savedVerified,
                            "The single Archibald integration evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the bounded Archibald hand-in completed");
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            result(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "archibald_capture RUNTIME READONLY_MM6_SAVE NEW_OUTPUT\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Archibald capture: " << failure.what() << '\n';
        return 1;
    }
}
