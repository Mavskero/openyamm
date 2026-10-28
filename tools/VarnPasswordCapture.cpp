#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/scene/IndoorSceneRuntime.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void queueKey(SDL_Keycode key, SDL_Scancode scan, bool down)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.key = key;
    event.key.scancode = scan;
    event.key.down = down;
    require(SDL_PushEvent(&event), "Cannot queue ordinary SDL key");
}

void queueMouse(float x, float y, int width, int height, uint8_t button, bool down)
{
    int count = 0;
    SDL_Window **pWindows = SDL_GetWindows(&count);
    SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
    SDL_free(pWindows);
    require(pWindow != nullptr, "Expected one actual capture window");
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
}

// One controlled original-chest -> readable scroll -> original password input chain.
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
            "Use a new NVMe output outside the read-only runtime");
        std::string error;
        const auto seed = loadGameDataFromPath(input, error);
        require(seed && seed->mapFileName == "oute3.odm" && seed->party.members.size() == 4
            && !seed->heldInventoryItemActive && seed->party.pendingItemDeliveries.empty()
            && !seed->party.questBits.contains(1253), "Expected the normal132 MM6 seed: " + error);
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot preserve source input");
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":184,\"controlled\":true,\"natural_campaign\":false,"
               "\"input\":" << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(runtime.string())
            << ",\"scope\":\"Load unchanged normal132 MM6 seed; debug-map jump to original Pyramid. "
               "Declare two position/camera fixtures: original trapped chest2 and the outside rim of password pool21. "
               "Declare one member's DisarmTraps/Normal at exactly twice the original map disarmDifficulty; "
               "this is a late-quest capability precondition, not naturally earned training. Record before/after. "
               "Original box42 opens chest2 and sets Q1253. Pick actual2158 with SDL chest pointer, "
               "read it with SDL right portrait, store with SDL left portrait, then click original face47. "
               "Submit kcops through SDL text/Return. No granted item, direct event dispatch, "
               "quest/completion writes or actor/AI/buff/HP/immortality edits. "
               "Simulation delta is zero during framing/UI/captures; only the final30 frames run normal AI.\","
               "\"wall_limit_seconds\":90,\"frame_limit\":900,\"capture_limit\":3,"
               "\"new_unit_tests\":0,\"cold_replays\":0,\"audio\":\"dummy\"}\n";

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
        config.appName = "OpenYAMM VARN184";
        config.assetRoot = runtime.string();
        config.activeWorldId = "mm6";
        config.startupMapFileOverride = "pyramid.blv";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        const uint64_t started = SDL_GetTicks();
        unsigned frames = 0, phaseFrame = 0, normalFrames = 0, captures = 0, worldClicks = 0;
        double normalSeconds = 0, initialMinutes = 0;
        int phase = 0;
        bool complete = false, savedVerified = false, scrollRead = false, answerObserved = false;
        std::array<float, 2> pointer{};
        std::string pendingCapture;
        Party::Snapshot baseline;
        size_t chestItemCount = 0;
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(12);
        const auto scene = [&]() -> IndoorSceneRuntime &
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene && pScene->currentMapFileName() == "pyramid.blv", "Actual Pyramid scene is absent");
            return *pScene;
        };
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto events = [&]() -> EventRuntimeState &
        {
            require(scene().eventRuntimeState() != nullptr, "Missing Pyramid event state");
            return *scene().eventRuntimeState();
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty() && captures < 3, "Overlapping or excessive capture");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto record = [&](const std::string &stage)
        {
            const auto &pose = scene().partyRuntime().movementState();
            trace << "{\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"phase\":" << phase << ",\"pose\":[" << pose.x << ',' << pose.y << ',' << pose.footZ
                << "],\"sector\":" << pose.sectorId << ",\"game_minutes\":" << scene().worldRuntime().gameMinutes()
                << ",\"Q1253\":" << scene().party().hasQuestBit(1253)
                << ",\"inventory2158\":" << scene().party().inventoryItemCount(2158)
                << ",\"held2158\":" << (screen().heldInventoryItem().active
                    && screen().heldInventoryItem().item.objectDescriptionId == 2158)
                << ",\"MapVar12\":" << int(events().mapVars[12])
                << ",\"MapVar29\":" << int(events().mapVars[29])
                << ",\"pending_prompt\":" << events().pendingInputPrompt.has_value() << ",\"health\":[";
            for (size_t index = 0; index < scene().party().members().size(); ++index)
            {
                if (index) trace << ',';
                trace << scene().party().members()[index].health;
            }
            trace << "]}\n";
            trace.flush();
        };
        const auto aim = [&](float x, float y, float z)
        {
            const auto &pose = scene().partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(y - pose.y, x - pose.x),
                std::atan2(z - pose.eyeZ(), std::hypot(x - pose.x, y - pose.y)));
        };
        const auto clickFace = [&](size_t faceIndex, uint16_t eventId, int width, int height)
        {
            const auto &selected = game.m_gameDataLoader.getSelectedMap();
            require(selected && selected->indoorMapData, "Original Pyramid geometry is absent");
            const auto &map = *selected->indoorMapData;
            const auto &face = map.faces.at(faceIndex);
            require(face.cogTriggered == eventId && hasFaceAttribute(face.attributes, FaceAttribute::Clickable),
                "Original clickable face changed");
            bx::Vec3 center{0, 0, 0};
            for (uint16_t id : face.vertexIndices)
            {
                const auto &vertex = map.vertices.at(id);
                center = bx::add(center, {float(vertex.x), float(vertex.y), float(vertex.z)});
            }
            center = bx::mul(center, 1.0f / float(face.vertexIndices.size()));
            std::vector<bx::Vec3> points{center};
            for (uint16_t id : face.vertexIndices)
            {
                const auto &vertex = map.vertices.at(id);
                for (float fraction : {0.35f, 0.65f, 0.85f, 0.94f})
                    points.push_back(bx::add(center, bx::mul(
                        bx::sub({float(vertex.x), float(vertex.y), float(vertex.z)}, center), fraction)));
            }
            for (const auto &point : points)
            {
                float x = 0, y = 0;
                if (!game.m_indoorRenderer.projectGameplayWorldPointToScreen(point, width, height, x, y)) continue;
                const auto hit = scene().worldRuntime().pickMouseInteractionTarget(
                    scene().worldRuntime().buildWorldPickRequest({x, y, width, height, true}));
                trace << "{\"stage\":\"picker-observation\",\"expected_face\":" << faceIndex
                    << ",\"hit\":" << hit.hasHit
                    << ",\"event\":" << (hit.eventTarget ? hit.eventTarget->triggeredEventId : 0)
                    << ",\"face\":" << (hit.eventTarget ? int64_t(hit.eventTarget->secondaryIndex) : -1)
                    << ",\"distance\":" << (hit.eventTarget ? hit.eventTarget->distance : -1) << "}\n";
                trace.flush();
                if (!hit.eventTarget || hit.eventTarget->targetKind != GameplayWorldEventTargetKind::Surface
                    || hit.eventTarget->secondaryIndex != faceIndex || hit.eventTarget->triggeredEventId != eventId
                    || !scene().worldRuntime().canActivateWorldHit(hit, GameplayInteractionMethod::Mouse)) continue;
                pointer = {x, y};
                trace << "{\"stage\":\"source-face-sdl-down\",\"event\":" << eventId << ",\"face\":" << faceIndex
                    << ",\"pixel\":[" << x << ',' << y << "],\"distance\":" << hit.eventTarget->distance << "}\n";
                queueMouse(x, y, width, height, SDL_BUTTON_RIGHT, true);
                queueMouse(x, y, width, height, SDL_BUTTON_LEFT, true);
                ++worldClicks;
                return;
            }
            throw std::runtime_error("Real picker cannot reach original face " + std::to_string(faceIndex));
        };
        const auto verifyAnswer = [&]()
        {
            require(events().mapVars[12] == 1 && events().mapVars[29] == 0
                && !scene().party().hasQuestBit(1253) && scene().party().inventoryItemCount(2158) == 0
                && !screen().heldInventoryItem().active && !events().pendingInputPrompt,
                "Original correct-answer transaction is incomplete");
            for (size_t index = 13; index <= 17; ++index)
                require(events().mapVars[index] == 0, "The single password changed another password flag");
        };
        const auto report = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"passed\":" << passed << ",\"frames\":" << frames << ",\"phase\":" << phase
                << ",\"normal_frames\":" << normalFrames << ",\"normal_seconds\":" << normalSeconds
                << ",\"captures\":" << captures << ",\"source_world_clicks\":" << worldClicks
                << ",\"scroll_read\":" << scrollRead << ",\"answer_observed\":" << answerObserved
                << ",\"final_save_verified\":" << savedVerified << ",\"natural_campaign\":false,"
                   "\"device_acceptance\":false,\"new_unit_tests\":0,\"cold_replays\":0,\"failure\":"
                << std::quoted(failure) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(output / "source-input.oysav"),
                        "Cannot load original normal seed");
                    const MapStatsEntry *pMap = game.m_gameDataLoader.getMapStats().findByFileName("pyramid.blv");
                    require(pMap != nullptr, "Pyramid is absent from production MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared Pyramid map fixture failed");
                    require(scene().party().inventoryItemCount(2158) == 0 && events().mapVars[12] == 0
                        && events().mapVars[29] == 0, "Input already owns or solved the first-mate code");
                    const auto *pDelta = scene().worldRuntime().mapDeltaData();
                    require(pDelta && pDelta->chests.size() > 2 && (pDelta->chests[2].flags & 1u) != 0,
                        "Original chest2 must retain its unmodified Trapped flag before the real click");
                    Character &member = *scene().party().member(0);
                    const CharacterSkill *pBeforeSkill = member.findSkillByCanonicalName("DisarmTraps");
                    const uint32_t beforeLevel = pBeforeSkill ? pBeforeSkill->level : 0;
                    const SkillMastery beforeMastery = pBeforeSkill ? pBeforeSkill->mastery : SkillMastery::None;
                    const int beforeValue = scene().party().bestPartyWideUtilitySkillValue("DisarmTraps");
                    const int requiredValue = pMap->disarmDifficulty * 2;
                    int itemBonus = 0;
                    for (const char *pName : {"Disarm", "DisarmTrap", "DisarmTraps"})
                    {
                        const auto found = member.itemSkillBonuses.find(pName);
                        if (found != member.itemSkillBonuses.end()) itemBonus += found->second;
                    }
                    require(requiredValue > itemBonus && beforeValue < requiredValue,
                        "Expected normal132's insufficient trap skill and a positive exact Normal-level fixture");
                    CharacterSkill *pSkill = member.findSkillByCanonicalName("DisarmTraps");
                    if (pSkill == nullptr)
                    {
                        member.skills["DisarmTraps"] =
                            {"DisarmTraps", uint32_t(requiredValue - itemBonus), SkillMastery::Normal};
                    }
                    else
                    {
                        pSkill->level = uint32_t(requiredValue - itemBonus);
                        pSkill->mastery = SkillMastery::Normal;
                    }
                    require(scene().party().bestPartyWideUtilitySkillValue("DisarmTraps") == requiredValue,
                        "Declared DisarmTraps ability must exactly meet the original trap threshold");
                    std::ofstream(output / "ability-fixture.json")
                        << "{\"member_index\":0,\"member_id\":" << member.memberId
                        << ",\"skill\":\"DisarmTraps\",\"before_level\":" << beforeLevel
                        << ",\"before_mastery\":" << int(beforeMastery) << ",\"before_party_value\":" << beforeValue
                        << ",\"after_level\":" << requiredValue - itemBonus << ",\"after_mastery\":1"
                        << ",\"after_party_value\":" << requiredValue << ",\"map_disarm_difficulty\":"
                        << pMap->disarmDifficulty << ",\"map_trap_d20_dice\":" << pMap->trapDamageD20DiceCount
                        << ",\"original_chest_flags\":" << pDelta->chests[2].flags
                        << ",\"naturally_earned_skill\":false,\"HP_resistance_AI_edits\":false}\n";
                    baseline = scene().party().snapshot();
                    initialMinutes = scene().worldRuntime().gameMinutes();
                    scene().partyRuntime().teleportPartyPosition(-10240, -2116, -923);
                    aim(-10463, -2116, -892);
                    record("declared-chest-pose");
                    require(game.quickSaveToPath(output / "declared-fixture.oysav", "184维兰原箱前受控输入"),
                        "Cannot save declared fixture");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= 900 && SDL_GetTicks() - started < 90000, "VARN capture exceeded finite bound");
                    const bool normal = phase == 19;
                    game.renderFrame(width, height, wheel, normal ? delta : 0.0f);
                    if (normal) { ++normalFrames; normalSeconds += delta; }
                    require(game.m_screenManager.activeScreen() == nullptr, "Unexpected full-screen interruption");
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot wrap actual Metal pixels");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save actual capture");
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    const auto portrait = [&]()
                    {
                        const auto rect = screen().resolvePartyPortraitRect(width, height, 0);
                        require(rect.has_value(), "Original recipient portrait is absent");
                        return std::array<float, 2>{rect->x + rect->width * 0.5f, rect->y + rect->height * 0.5f};
                    };
                    if (phase == 0 && frames >= 8)
                    {
                        clickFace(3061, 42, width, height);
                        phaseFrame = frames;
                        phase = 1;
                    }
                    else if (phase == 1)
                    {
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_LEFT, false);
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_RIGHT, false);
                        phase = 2;
                    }
                    // GameInputSystem deliberately consumes one queued mouse edge per frame.
                    else if (phase == 2 && frames >= phaseFrame + 4)
                    {
                        const auto *pChest = scene().worldRuntime().activeChestView();
                        require(pChest && pChest->chestId == 2 && scene().party().hasQuestBit(1253),
                            "Original event42 did not open chest2/set Q1253");
                        chestItemCount = pChest->items.size();
                        record("original-chest-open");
                        capture("original-chest2");
                        phase = 3;
                    }
                    else if (phase == 3 && pendingCapture.empty())
                    {
                        const auto *pChest = scene().worldRuntime().activeChestView();
                        require(pChest != nullptr, "Original chest closed before pickup");
                        const auto item = std::find_if(pChest->items.begin(), pChest->items.end(), [](const auto &value)
                            { return value.item.objectDescriptionId == 2158; });
                        const auto grid = screen().resolveChestGridArea(width, height);
                        require(item != pChest->items.end() && grid.has_value(), "Actual2158 has no chest cell");
                        pointer = {grid->x + (item->gridX + 0.5f) * grid->width / pChest->gridWidth,
                            grid->y + (item->gridY + 0.5f) * grid->height / pChest->gridHeight};
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_LEFT, true);
                        phase = 4;
                    }
                    else if (phase == 4)
                    {
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_LEFT, false);
                        phase = 5;
                    }
                    else if (phase == 5)
                    {
                        const auto *pChest = scene().worldRuntime().activeChestView();
                        require(pChest && pChest->items.size() + 1 == chestItemCount
                            && screen().heldInventoryItem().active
                            && screen().heldInventoryItem().item.objectDescriptionId == 2158,
                            "SDL chest pointer did not transfer exactly the original2158");
                        queueMouse(5, 5, width, height, SDL_BUTTON_RIGHT, true);
                        phase = 50;
                    }
                    else if (phase == 50)
                    {
                        require(!screen().interactionState().partyPortraitRightClickItemUseLatch,
                            "A right-button gesture without a portrait target was consumed prematurely");
                        pointer = portrait();
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_RIGHT, true);
                        phase = 6;
                    }
                    else if (phase == 6)
                    {
                        const auto &overlay = screen().readableScrollOverlayReadOnly();
                        const auto &inputFrame = game.m_gameInputSystem.frame();
                        trace << "{\"stage\":\"read-scroll-input\",\"active\":" << overlay.active
                            << ",\"title\":" << std::quoted(overlay.title)
                            << ",\"body\":" << std::quoted(overlay.body)
                            << ",\"right_held\":" << inputFrame.rightMouseButton.held
                            << ",\"pointer\":[" << inputFrame.pointerX << ',' << inputFrame.pointerY
                            << "],\"portrait_latch\":"
                            << screen().interactionState().partyPortraitRightClickItemUseLatch << "}\n";
                        trace.flush();
                        require(overlay.active && overlay.title == "大副的密码"
                            && overlay.body.find("kcops") != std::string::npos,
                            "Actual scroll did not show source password");
                        trace << "{\"stage\":\"read-original-scroll\",\"title\":" << std::quoted(overlay.title)
                            << ",\"body\":" << std::quoted(overlay.body) << "}\n";
                        scrollRead = true;
                        // Keep a real held gesture through the asynchronous screenshot frames.
                        // SDL_PushEvent does not alter the host mouse's physical button state.
                        for (unsigned heldFrame = 0; heldFrame < 4; ++heldFrame)
                            queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_RIGHT, true);
                        capture("first-mate-scroll");
                        phase = 7;
                        phaseFrame = frames;
                    }
                    else if (phase == 7 && pendingCapture.empty() && frames >= phaseFrame + 4)
                    {
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_RIGHT, false);
                        phase = 8;
                    }
                    else if (phase == 8)
                    {
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_LEFT, true);
                        phase = 9;
                    }
                    else if (phase == 9)
                    {
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_LEFT, false);
                        phase = 10;
                    }
                    else if (phase == 10)
                    {
                        require(!screen().heldInventoryItem().active && scene().party().inventoryItemCount(2158) == 1,
                            "SDL recipient portrait did not store the actual scroll");
                        queueKey(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, true);
                        phase = 11;
                    }
                    else if (phase == 11)
                    {
                        queueKey(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, false);
                        phase = 12;
                    }
                    else if (phase == 12)
                    {
                        require(!scene().worldRuntime().activeChestView() && !screen().characterScreenReadOnly().open
                            && !screen().readableScrollOverlayReadOnly().active,
                            "Ordinary Escape did not close chest UI");
                        scene().partyRuntime().teleportPartyPosition(-12250, -1850, 772);
                        // Frame the reachable inner edge rather than the distant pool center,
                        // so the source interaction is visibly inside the world view above the HUD.
                        aim(-12307.2f, -1524.13f, 585);
                        phaseFrame = frames;
                        phase = 13;
                        record("declared-pool-rim-pose");
                    }
                    else if (phase == 13 && frames >= phaseFrame + 4)
                    {
                        require(!events().pendingInputPrompt && events().mapVars[12] == 0,
                            "Pose fixture triggered the password before the physical click");
                        clickFace(47, 21, width, height);
                        phaseFrame = frames;
                        phase = 14;
                    }
                    else if (phase == 14)
                    {
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_LEFT, false);
                        queueMouse(pointer[0], pointer[1], width, height, SDL_BUTTON_RIGHT, false);
                        phase = 15;
                    }
                    else if (phase == 15 && frames >= phaseFrame + 4)
                    {
                        require(events().pendingInputPrompt && events().pendingInputPrompt->eventId == 21
                            && events().pendingInputPrompt->correctStep == 9 && game.pendingInputPromptActive(),
                            "Actual original face21 did not open its normal text prompt");
                        SDL_Event text{};
                        text.type = SDL_EVENT_TEXT_INPUT;
                        text.text.text = "kcops";
                        game.handleSdlEvent(text);
                        phase = 16;
                    }
                    else if (phase == 16)
                    {
                        require(game.m_pendingInputText == "kcops", "SDL text did not enter the visible normal prompt");
                        record("source21-kcops-entered");
                        capture("password-input");
                        phase = 17;
                    }
                    else if (phase == 17 && pendingCapture.empty())
                    {
                        queueKey(SDLK_RETURN, SDL_SCANCODE_RETURN, true);
                        phase = 18;
                    }
                    else if (phase == 18)
                    {
                        queueKey(SDLK_RETURN, SDL_SCANCODE_RETURN, false);
                        verifyAnswer();
                        require(!screen().activeEventDialog().isActive,
                            "Answered map prompt did not return to gameplay");
                        answerObserved = true;
                        record("source21-answer-applied");
                        phase = 19;
                    }
                    else if (phase == 19 && normalFrames >= 30)
                    {
                        verifyAnswer();
                        require(scene().party().hasActableMember(), "Normal continuation ended in party defeat");
                        const auto path = output / "varn-first-mate-complete.oysav";
                        require(game.quickSaveToPath(path, "184维兰大副密码后续玩"), "Cannot save actual continuation");
                        const auto saved = loadGameDataFromPath(path, error);
                        require(saved && saved->mapFileName == "pyramid.blv" && saved->hasIndoorSceneState
                            && saved->indoorScene.eventRuntimeState && !saved->heldInventoryItemActive
                            && saved->party.pendingItemDeliveries.empty(),
                            "Cannot decode complete final save: " + error);
                        const auto &state = *saved->indoorScene.eventRuntimeState;
                        Party restored;
                        restored.restoreSnapshot(saved->party);
                        require(state.mapVars[12] == 1 && state.mapVars[29] == 0
                            && !restored.hasQuestBit(1253) && restored.inventoryItemCount(2158) == 0
                            && saved->party.gold == baseline.gold
                            && saved->party.members.size() == baseline.members.size(),
                            "Final save lost source answer, consumption, Gold or roster");
                        for (size_t index = 0; index < baseline.members.size(); ++index)
                            require(saved->party.members[index].memberId == baseline.members[index].memberId
                                && saved->party.members[index].experience == baseline.members[index].experience,
                                "Password changed a stable member or experience");
                        const auto &views = saved->indoorScene.worldRuntime.materializedChestViews;
                        require(views.size() > 2 && views[2].has_value(), "Save lost the actual original chest view");
                        for (const auto &item : views[2]->items)
                            require(item.item.objectDescriptionId != 2158,
                                "Save regenerated the taken original scroll");
                        savedVerified = true;
                        record("continued-and-save-decoded");
                        trace << "{\"stage\":\"normal-time\",\"before\":" << initialMinutes
                            << ",\"after\":" << scene().worldRuntime().gameMinutes() << "}\n";
                        require(captures == 3 && worldClicks == 2 && scrollRead && answerObserved,
                            "Evidence chain incomplete");
                        complete = true;
                        phase = 20;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete && savedVerified, "Actual App stopped before the chain completed");
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
        if (argc != 4)
            throw std::runtime_error("Usage: varn_password_capture RUNTIME NORMAL132_MM6_SAVE NEW_NVME_OUTPUT");
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "VARN password capture: " << failure.what() << '\n';
        return 1;
    }
}
