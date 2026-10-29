#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/screens/CutsceneVideoScreen.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

void queueWorldClick(float x, float y, int width, int height)
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
        require(SDL_PushEvent(&event), "Cannot queue ordinary SDL Council door edge");
    };
    edge(SDL_BUTTON_RIGHT, true);
    edge(SDL_BUTTON_LEFT, true);
    edge(SDL_BUTTON_LEFT, false);
    edge(SDL_BUTTON_RIGHT, false);
}

void queueEscape()
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(captureWindow());
    event.key.key = SDLK_ESCAPE;
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    event.key.down = true;
    require(SDL_PushEvent(&event), "Cannot queue citytrtr Escape down");
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    require(SDL_PushEvent(&event), "Cannot queue citytrtr Escape up");
}

using TopicOverrides = std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>>;

std::array<uint32_t, 6> resolvedTopics(const NpcEntry &npc, const TopicOverrides &overrides)
{
    std::array<uint32_t, 6> result{};
    require(npc.topicIds.size() <= result.size(), "NPC789 has an unsupported topic slot count");
    std::copy(npc.topicIds.begin(), npc.topicIds.end(), result.begin());
    const auto found = overrides.find(npc.id);
    if (found != overrides.end())
    {
        for (const auto &[slot, id] : found->second)
        {
            require(slot < result.size(), "NPC789 has an out-of-range topic override");
            result[slot] = id;
        }
    }
    return result;
}
}

// One original event49 mouse interaction. Upstream evidence/endorsements and the door pose are fixtures.
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
            && source->party.pendingItemDeliveries.empty(), "Expected an empty-handed normal132 MM6 save: " + error);
        require(!source->party.questBits.contains(1191) && !source->party.questBits.contains(1192),
            "The input must precede the Council evidence reward");
        for (const Character &member : source->party.members)
            require(!member.awards.contains(63), "The input already has Council award63");
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot preserve the input save");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":190,\"controlled\":true,\"natural_campaign\":false,\"device_acceptance\":false,"
               "\"input\":" << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(runtime.string())
            << ",\"changes\":\"Load an unchanged copy of normal132 MM6; require Q1191/Q1192 and award63 absent. "
               "Debug-map jump to outc2.odm. Require initial stored local reputation0 and no item2122. "
               "Place exactly one real-table item2122 in an existing active member inventory; add awards57..62 "
               "to every active member; set the upstream Q1225=true to observe its source cleanup. "
               "Keep original NPC1089 house209 and all NPC789 topics and overrides unchanged. "
               "Place the party256 units along a real clickable event49 face normal, foot at its bottom+1, "
               "and aim at its center. One ordinary SDL world gesture opens the door. "
               "Observe the source door's cutscene, save one nonblack movie frame, close with SDL Escape, "
               "and save one nonblack actual house209 frame. Close the dialogue with its existing UI API, "
               "run30 normal frames, explicitly save and independently decode. "
               "Freeze simulation only during framing/dialogue capture. No actor/AI/buff fixtures, "
               "direct event dispatch, second entrance, old-route replay, cold replay or new unit test.\","
               "\"wall_limit_seconds\":45,\"frame_limit\":1800,\"screenshots\":[\"citytrtr-movie\",\"council-house\"],"
               "\"audio\":\"dummy\",\"visual_review_required\":true,\"new_unit_tests\":0,\"cold_replays\":0}\n";

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
        config.appName = "OpenYAMM Council190";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = source->mapFileName;
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(12);
        enum class Phase { Warmup, DoorClicked, Movie, MovieClosing, House, Continuing, Finished };
        Phase phase = Phase::Warmup;
        const auto started = std::chrono::steady_clock::now();
        const auto elapsedSeconds = [&]()
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        };
        uint64_t movieStartedAt = 0, itemMemberId = 0;
        unsigned frames = 0, doorFrame = 0, movieFrame = 0, escapeFrame = 0, houseFrame = 0;
        unsigned nextMovieCapture = 0, captures = 0, blankMovieCaptures = 0, normalFrames = 0;
        double normalSeconds = 0;
        size_t doorModel = 0, doorFace = 0, movieNonblackPixels = 0, houseNonblackPixels = 0;
        bx::Vec3 doorCenter{0, 0, 0};
        bool movieStarted = false, movieCaptured = false, movieClosed = false;
        bool houseCaptured = false, complete = false, savedVerified = false;
        int reputationBefore = 0;
        Party::Snapshot beforeReward;
        NpcEntry originalNpc789;
        std::array<uint32_t, 6> topicsBefore{}, topicsAfter{};
        std::string pendingCapture;
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && game.m_gameSession.currentMapFileName() == "outc2.odm", "Expected the actual outc2 outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto verifyReward = [&](const Party::Snapshot &party, const EventRuntimeState &events, int reputation)
        {
            require(party.questBits.contains(1192) && party.questBits.contains(1191)
                && !party.questBits.contains(1225), "Original Council quest transaction is incomplete");
            require(reputationBefore == 0 && reputation == -20 && events.currentLocationReputation == -20,
                "Stored local reputation must improve exactly once from0 to-20");
            require(party.gold == beforeReward.gold && party.members.size() == beforeReward.members.size(),
                "The Council entrance changed Gold or party size");
            for (size_t index = 0; index < party.members.size(); ++index)
            {
                const Character &member = party.members[index];
                const Character &before = beforeReward.members[index];
                auto expectedAwards = before.awards;
                expectedAwards.insert(63);
                require(member.memberId == before.memberId && member.experience == before.experience
                    && member.awards == expectedAwards, "Council award63, member identity or unchanged XP is wrong");
            }
            Party restored;
            restored.restoreSnapshot(party);
            require(restored.inventoryItemCount(2122) == 0 && !restored.hasItemAnywhere(2122),
                "The original evidence item2122 was not consumed");
            require(party.npcHouseOverrides.contains(1089) && party.npcHouseOverrides.at(1089) == 0
                && events.npcHouseOverrides.contains(1089) && events.npcHouseOverrides.at(1089) == 0,
                "Silvertongue was not removed from the original Council house");
            require(resolvedTopics(originalNpc789, party.npcTopicOverrides) == topicsAfter
                && resolvedTopics(originalNpc789, events.npcTopicOverrides) == topicsAfter,
                "Council cleanup lost NPC789 topics or retained topic1415");
        };
        const auto verifyLiveReward = [&]()
        {
            require(scene().eventRuntimeState() != nullptr, "Missing live Council event state");
            verifyReward(scene().party().snapshot(), *scene().eventRuntimeState(),
                scene().worldRuntime().currentLocationReputation());
        };
        const auto verifyHouse = [&]()
        {
            const auto &dialog = screen().activeEventDialog();
            require(dialog.isActive && dialog.isHouseDialog && dialog.sourceId == 209
                && scene().eventRuntimeState()->dialogueState.hostHouseId == 209,
                "The original event49 did not present actual house209");
            require(std::none_of(dialog.actions.begin(), dialog.actions.end(), [](const auto &action)
                { return action.kind == EventDialogActionKind::HouseResident && action.id == 1089; }),
                "The actual Council lobby still presents removed NPC1089");
            verifyLiveReward();
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto result = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(12)
                << "{\"passed\":" << passed << ",\"controlled\":true,\"natural_campaign\":false"
                << ",\"device_acceptance\":false,\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"event49_sdl_clicks\":" << (doorFrame ? 1 : 0)
                << ",\"door_model\":" << doorModel << ",\"door_face\":" << doorFace
                << ",\"expected_source_movie\":\"citytrtr\",\"movie_identity_requires_visual_review\":true"
                << ",\"cutscene_started\":" << movieStarted << ",\"cutscene_start_frame\":" << movieFrame
                << ",\"movie_capture_saved\":" << movieCaptured << ",\"movie_closed_by_escape\":" << movieClosed
                << ",\"house209_capture_saved\":" << houseCaptured
                << ",\"movie_nonblack_pixels\":" << movieNonblackPixels
                << ",\"house_nonblack_pixels\":" << houseNonblackPixels
                << ",\"blank_movie_captures_discarded\":" << blankMovieCaptures
                << ",\"item2122_fixture_member_id\":" << itemMemberId
                << ",\"stored_reputation_before\":" << reputationBefore
                << ",\"expected_stored_reputation_after\":-20,\"normal_frames\":" << normalFrames
                << ",\"npc789_initial_topic1415_slots\":" << std::count(topicsBefore.begin(), topicsBefore.end(), 1415u)
                << ",\"normal_seconds\":" << normalSeconds << ",\"final_save_verified\":" << savedVerified
                << ",\"elapsed_wall_seconds\":" << elapsedSeconds()
                << ",\"visual_review_required\":true,\"new_unit_tests\":0,\"cold_replays\":0"
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
                    const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName("outc2.odm");
                    require(pMap != nullptr, "Free Haven is absent from MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared outc2 map fixture failed");
                    Party &party = scene().party();
                    EventRuntimeState &events = *scene().eventRuntimeState();
                    require(!party.hasItemAnywhere(2122) && !party.hasQuestBit(1191) && !party.hasQuestBit(1192),
                        "Normal seed already owns Council evidence or completion");
                    const NpcDialogTable &npcs = game.m_gameDataLoader.getNpcDialogTable();
                    const NpcEntry *pTraitor = npcs.getNpc(1089);
                    const NpcEntry *pTopicNpc = npcs.getNpc(789);
                    require(pTraitor && pTraitor->houseId == 209 && pTopicNpc,
                        "Original NPC1089 house209 or NPC789 is absent; no NPC fixture is permitted");
                    const auto house = events.npcHouseOverrides.find(1089);
                    require(house == events.npcHouseOverrides.end() || house->second == 209,
                        "Input already moved Silvertongue away from the original Council");
                    originalNpc789 = *pTopicNpc;
                    topicsBefore = resolvedTopics(originalNpc789, events.npcTopicOverrides);
                    topicsAfter = topicsBefore;
                    std::replace(topicsAfter.begin(), topicsAfter.end(), 1415u, 0u);
                    reputationBefore = scene().worldRuntime().currentLocationReputation();
                    require(reputationBefore == 0, "Normal outc2 seed must have stored local reputation0");
                    const ItemTable &items = game.m_gameDataLoader.getItemTable();
                    require(items.get(2122) != nullptr, "Original evidence item2122 is absent from the item table");
                    const InventoryItem evidence = ItemGenerator::makeInventoryItem(
                        2122, items, ItemGenerationMode::Generic);
                    for (size_t index = 0; index < party.memberCount(); ++index)
                    {
                        if (party.tryAutoPlaceItemInMemberInventory(index, evidence))
                        {
                            itemMemberId = party.member(index)->memberId;
                            break;
                        }
                    }
                    require(itemMemberId != 0 && party.inventoryItemCount(2122) == 1,
                        "Cannot place exactly one item2122 in an existing member inventory");
                    for (size_t member = 0; member < party.memberCount(); ++member)
                        for (uint32_t award = 57; award <= 62; ++award) party.addAward(member, award);
                    party.setQuestBit(1225, true);
                    beforeReward = party.snapshot();
                    const OutdoorMapData *pData = scene().worldRuntime().mapData();
                    require(pData != nullptr, "Missing original outc2 model geometry");
                    bool found = false;
                    for (size_t modelIndex = 0; modelIndex < pData->bmodels.size() && !found; ++modelIndex)
                    {
                        const auto &model = pData->bmodels[modelIndex];
                        for (size_t faceIndex = 0; faceIndex < model.faces.size(); ++faceIndex)
                        {
                            const auto &face = model.faces[faceIndex];
                            if (face.cogTriggeredNumber != 49 || face.vertexIndices.empty()
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
                            scene().partyRuntime().teleportTo(doorCenter.x + nx / length * 256,
                                doorCenter.y + ny / length * 256, bottom + 1);
                            const auto &pose = scene().partyRuntime().movementState();
                            game.m_outdoorGameView.setCameraAngles(
                                std::atan2(doorCenter.y - pose.y, doorCenter.x - pose.x),
                                std::atan2(doorCenter.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                                    std::hypot(doorCenter.x - pose.x, doorCenter.y - pose.y)));
                            doorModel = modelIndex;
                            doorFace = faceIndex;
                            found = true;
                            trace << "{\"stage\":\"declared-door-pose\",\"event\":49,\"model\":" << doorModel
                                << ",\"face\":" << doorFace << ",\"pose\":[" << pose.x << ',' << pose.y << ','
                                << pose.footZ << "],\"item2122_member_id\":" << itemMemberId
                                << ",\"stored_reputation\":" << reputationBefore << ",\"Q1225\":true,\"members\":[";
                            for (size_t index = 0; index < beforeReward.members.size(); ++index)
                            {
                                if (index) trace << ',';
                                trace << "{\"member_id\":" << beforeReward.members[index].memberId
                                    << ",\"xp\":" << beforeReward.members[index].experience
                                    << ",\"fixture_awards\":[57,58,59,60,61,62]}";
                            }
                            trace << "],\"npc789_topics_before\":[";
                            for (size_t slot = 0; slot < topicsBefore.size(); ++slot)
                            {
                                if (slot) trace << ',';
                                trace << topicsBefore[slot];
                            }
                            trace << "]}\n";
                            break;
                        }
                    }
                    require(found, "No original clickable event49 Council door face was found");
                    require(!screen().activeEventDialog().isActive && !events.pendingMovie,
                        "Fixture unexpectedly opened a dialogue or movie");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 1800 && elapsedSeconds() < 45.0,
                        "Bounded Council capture timed out");
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
                            "citytrtr closed or exceeded the ten-second screenshot deadline");
                    if (normalGameplay)
                    {
                        require(!screen().activeEventDialog().isActive && !pMovie && scene().party().hasActableMember(),
                            "Post-Council normal gameplay was interrupted");
                        verifyLiveReward();
                        if (delta > 0) { ++normalFrames; normalSeconds += delta; }
                    }
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            const bool movieImage = pendingCapture == "citytrtr-movie";
                            require(movieImage ? pMovie != nullptr : !pMovie, "Capture lost its intended presentation");
                            size_t nonblack = 0;
                            for (size_t pixel = 0; pixel + 2 < image->bgraPixels.size(); pixel += 4)
                                if (image->bgraPixels[pixel] > 16 || image->bgraPixels[pixel + 1] > 16
                                    || image->bgraPixels[pixel + 2] > 16) ++nonblack;
                            if (nonblack < size_t(image->width) * image->height / 100)
                            {
                                require(movieImage, "Actual Council house screenshot is black");
                                ++blankMovieCaptures;
                                pendingCapture.clear();
                                nextMovieCapture = frames + 15;
                                trace << "{\"stage\":\"blank-movie-capture-discarded\",\"frame\":" << frames << "}\n";
                                return;
                            }
                            if (!movieImage) verifyHouse();
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create screenshot surface");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save capture image");
                            trace << "{\"capture\":" << std::quoted(pendingCapture)
                                << ",\"nonblack_pixels\":" << nonblack << "}\n";
                            if (movieImage) { movieCaptured = true; movieNonblackPixels = nonblack; }
                            else { houseCaptured = true; houseNonblackPixels = nonblack; }
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
                            const auto &vertex = model.vertices.at(index);
                            points.push_back(bx::add(doorCenter, bx::mul(bx::sub(
                                {float(vertex.x), float(vertex.y), float(vertex.z)}, doorCenter), 0.65f)));
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
                                && hit.eventTarget->triggeredEventId == 49
                                && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                            {
                                pixel = std::array<float, 2>{x, y};
                                break;
                            }
                        }
                        require(pixel.has_value(), "Original event49 door is not reachable by the real mouse picker");
                        queueWorldClick((*pixel)[0], (*pixel)[1], width, height);
                        doorFrame = frames;
                        trace << "{\"stage\":\"queued-real-event49-click\",\"frame\":" << frames
                            << ",\"pixel\":[" << (*pixel)[0] << ',' << (*pixel)[1] << "]}\n";
                        phase = Phase::DoorClicked;
                    }
                    else if (phase == Phase::DoorClicked)
                    {
                        if (pMovie)
                        {
                            verifyLiveReward();
                            movieStarted = true;
                            movieFrame = frames;
                            movieStartedAt = SDL_GetTicks();
                            nextMovieCapture = frames + 30;
                            trace << "{\"stage\":\"original-door-cutscene-started\","
                                     "\"expected_source_movie\":\"citytrtr\","
                                     "\"identity_requires_visual_review\":true,\"frame\":" << frames << "}\n";
                            phase = Phase::Movie;
                        }
                        else require(frames < doorFrame + 20, "Real event49 click did not open citytrtr");
                    }
                    else if (phase == Phase::Movie)
                    {
                        if (movieCaptured)
                        {
                            queueEscape();
                            escapeFrame = frames;
                            phase = Phase::MovieClosing;
                        }
                        else if (frames >= nextMovieCapture) capture("citytrtr-movie");
                    }
                    else if (phase == Phase::MovieClosing)
                    {
                        if (!pMovie)
                        {
                            movieClosed = true;
                            houseFrame = frames;
                            phase = Phase::House;
                        }
                        else require(frames < escapeFrame + 20, "Ordinary Escape did not close citytrtr");
                    }
                    else if (phase == Phase::House && frames >= houseFrame + 4)
                    {
                        verifyHouse();
                        if (!houseCaptured)
                        {
                            const auto &dialog = screen().activeEventDialog();
                            trace << "{\"stage\":\"actual-council-house\",\"source_id\":" << dialog.sourceId
                                << ",\"host_house\":209,\"title\":" << std::quoted(dialog.houseTitle)
                                << ",\"stored_reputation\":" << scene().worldRuntime().currentLocationReputation()
                                << ",\"npc1089_house\":0,\"Q1192\":true,\"Q1191\":true,\"Q1225\":false}\n";
                            capture("council-house");
                        }
                        else
                        {
                            screen().handleDialogueCloseRequest();
                            require(!screen().activeEventDialog().isActive, "Council lobby did not close normally");
                            phase = Phase::Continuing;
                        }
                    }
                    else if (phase == Phase::Continuing && normalFrames >= 30)
                    {
                        verifyLiveReward();
                        const auto &events = *scene().eventRuntimeState();
                        require(!events.pendingMovie && !events.pendingWinGame && !events.pendingReturnToMainMenu,
                            "Post-Council gameplay retained an unexpected transition");
                        const auto finalPath = output / "council-complete.oysav";
                        require(game.quickSaveToPath(finalPath, "190议会揭露后继续游戏"), "Cannot save final Council gameplay");
                        const auto saved = loadGameDataFromPath(finalPath, error);
                        require(saved && saved->mapFileName == "outc2.odm" && !saved->heldInventoryItemActive
                            && saved->party.pendingItemDeliveries.empty() && saved->outdoorWorld.eventRuntimeState,
                            "Final save is missing its ordinary outdoor state: " + error);
                        verifyReward(saved->party, *saved->outdoorWorld.eventRuntimeState,
                            saved->outdoorWorld.locationInfo.reputation);
                        savedVerified = true;
                        trace << "{\"stage\":\"continued-game-saved\",\"normal_frames\":" << normalFrames
                            << ",\"normal_seconds\":" << normalSeconds
                            << ",\"item2122\":0,\"Q1192\":true,\"Q1191\":true,\"Q1225\":false,"
                               "\"npc1089_house\":0,\"award63_all_active\":true,\"stored_reputation\":-20,"
                               "\"duplicate_reward_observed\":false}\n";
                        phase = Phase::Finished;
                    }
                    else if (phase == Phase::Finished)
                    {
                        require(captures == 2 && doorFrame && movieStarted && movieCaptured
                            && movieClosed && houseCaptured && normalFrames >= 30 && savedVerified,
                            "The single Council integration evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the bounded Council entrance completed");
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
        std::cerr << "council_capture RUNTIME READONLY_MM6_SAVE NEW_OUTPUT\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Council capture: " << failure.what() << '\n';
        return 1;
    }
}
