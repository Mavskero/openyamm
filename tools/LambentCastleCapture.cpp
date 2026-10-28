#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayDialogContextBuilder.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/maps/SaveGame.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/screens/CutsceneVideoScreen.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
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
}

struct ApplicationCaptureTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path input, std::filesystem::path output,
        bool blasterLearning = false, bool lightEnding = false, bool darkEnding = false)
    {
        const bool ending = lightEnding || darkEnding;
        const uint32_t endingNpc = darkEnding ? 423 : 419;
        const uint32_t endingTopic = darkEnding ? 922 : 920;
        const uint32_t endingQuestBit = darkEnding ? 643 : 642;
        const std::string endingName = darkEnding ? "dark" : "light";
        const std::string endingCapturePrefix = "source-" + endingName + "-ending";
        const std::string expectedMovie = darkEnding ? "Endgame 2 Evil" : "Endgame 1 Good";
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(root / "engine.zip")
            && std::filesystem::is_regular_file(root / "worlds/mm7.zip"), "Complete MM7 runtime required");
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(root).generic_string().starts_with("../"),
            "Use a new NVMe output directory outside the read-only runtime");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && !source->party.members.empty() && !source->heldInventoryItemActive
            && source->party.pendingItemDeliveries.empty(), "Expected a normal empty-handed MM7 input: " + error);
        std::filesystem::create_directories(output);
        require(std::filesystem::copy_file(input, output / "source-input.oysav"), "Cannot copy read-only input");
        std::filesystem::current_path(output);
        if (darkEnding)
        {
            std::ofstream(output / "fixture.json")
                << "{\"checkpoint\":175,\"controlled\":true,\"natural_campaign\":false,\"input\":"
                << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(root.string())
                << ",\"changes\":\"Load an unchanged copy of accepted157 actual-001/entered-house220.oysav. "
                   "Inherit normal132 characters and controlled 7d30 map, Q611=true/Q612=false/Q642=true, "
                   "NPC419 house220, MapVar6=2, invisible actors with unchanged HP/AI/loot, and door-facing pose. "
                   "This is a direct NPC423 dialogue fixture, not a house221 entrance or natural faction journey. "
                   "Project only source921 delivery preconditions: Q643=true and NPC423 slot1=922 in Party "
                   "and live events; upstream history and journey are not recreated. "
                   "Declare Q748=true, Q783=false and exactly one real-table item1407 in a member inventory, "
                   "requiring no preexisting item1407. Preserve unrelated Q642/Q747 and other NPC topic slots. "
                   "Open NPC423 through the native NPC request and actual tables, then select visible topic922 "
                   "with one ordinary SDL mouse click. Freeze simulation before the movie; use normal frame "
                   "delta for the movie and at least 30 gameplay frames after ordinary Escape dismisses it. "
                   "Close the remaining NPC dialogue through the existing UI method before continued gameplay. "
                   "No direct event execution, door or Light replay, extra reward fixture, cold run, "
                   "natural acquisition or full-Character comparison.\","
                   "\"artifacts\":\"Before-topic, in-movie and continued-game screenshots plus final save. "
                   "Nonblack pixels only reject blank captures; movie contents require human visual review.\","
                   "\"audio\":\"dummy\",\"visual_review_required\":true,\"new_unit_tests\":0}\n";
        }
        else if (lightEnding)
        {
            std::ofstream(output / "fixture.json")
                << "{\"checkpoint\":163,\"controlled\":true,\"natural_campaign\":false,\"input\":"
                << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(root.string())
                << ",\"changes\":\"Load an unchanged copy of accepted157 actual-001/entered-house220.oysav. "
                   "Inherit its normal132 characters and controlled 7d30 map, Q611=true/Q612=false/Q642=true, "
                   "NPC419 house220, MapVar6=2, invisible actors with unchanged HP/AI/loot, and door-facing pose. "
                   "No door or combat replay. Declare Q642=true, Q783=false, Q747=true, Q748=true and "
                   "NPC419 slot1=920 in Party and live events, preserving slot0. Require no existing item1407; "
                   "place exactly one real-table item1407 in a member inventory. Open NPC419 using the native "
                   "NPC request and actual tables. Select visible topic920 with one ordinary SDL mouse click. "
                   "Freeze simulation before the movie; use normal frame delta for the movie and at least "
                   "30 gameplay frames after an ordinary Escape dismisses it. Close the remaining NPC dialog "
                   "through the existing UI method before those gameplay frames. No direct event execution, "
                   "other reward fixture, cold replay or full-Character comparison.\","
                   "\"artifacts\":\"Before-topic, in-movie and continued-game screenshots plus final save. "
                   "Nonblack pixels only reject blank captures; movie contents require human visual review.\","
                   "\"audio\":\"dummy\",\"visual_review_required\":true,\"new_unit_tests\":0}\n";
        }
        else if (blasterLearning)
        {
            std::ofstream(output / "fixture.json")
                << "{\"checkpoint\":160,\"controlled\":true,\"natural_campaign\":false,\"input\":"
                << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(root.string())
                << ",\"changes\":\"Load an unchanged copy of the normal132 MM7 save in its original map. "
                   "Declare upstream Q631=true and NPC422 slot0=950 in Party and live event state. "
                   "If all members have zero Blaster, set one member other than the retained eligible learner "
                   "to Blaster5 Expert; record its memberId in trace/result. Open NPC422 through the existing "
                   "native NPC request, using actual NPC/topic tables and the global program. "
                   "One ordinary SDL mouse click must select the visible source topic950; no direct event execution. "
                   "All operation/render frames have zero simulation delta. No 416 door, cold load, repeat "
                   "lesson or whole-Character hash.\",\"artifacts\":\"Two screenshots and learned-blaster.oysav; "
                   "the final save preserves gameplay state, not an open dialogue.\","
                   "\"audio\":\"dummy\",\"visual_review_required\":true,\"new_unit_tests\":0}\n";
        }
        else
        {
            std::ofstream(output / "fixture.json")
            << "{\"checkpoint\":157,\"controlled\":true,\"natural_campaign\":false,\"input\":"
            << std::quoted(input.string()) << ",\"runtime\":" << std::quoted(root.string())
            << ",\"changes\":\"Load an unchanged copy of the normal132 MM7 save. Debug-map jump to 7d30.blv; "
               "Q611=true, Q612=false, Q642=true, MapVar6=2 (the ordinary event176 chest aftermath). "
               "Declare NPC419 already moved to house220 in both Party and live event state. "
               "Set Invisible on every loaded 7d30 actor, preserving HP/AI/loot; no kills or rewards. "
               "Place the party 256 units south of original clickable face1962, at its bottom+1; aim at its center. "
               "All operation/render frames have zero simulation delta. One queued ordinary SDL mouse gesture "
               "must hit the real event416 door and show house220 with NPC419. No direct event execution, "
               "NPC topic selection, quest hand-in, ending, cold run or Character-byte comparison.\","
               "\"artifacts\":\"Two screenshots, declared-fixture.oysav, entered-house220.oysav; "
               "the explicit final save preserves gameplay state, not an open dialogue.\","
               "\"audio\":\"dummy\",\"visual_review_required\":true,\"new_unit_tests\":0}\n";
        }

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.debugConsole = false;
        settings.windowMode = WindowMode::Windowed;
        if (ending) settings.skipEventCutscenes = false;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = darkEnding ? "OpenYAMM Dark Ending175" : (lightEnding ? "OpenYAMM Light Ending163"
            : (blasterLearning ? "OpenYAMM Blaster Learning160" : "OpenYAMM Lambent Castle157"));
        config.assetRoot = root.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = blasterLearning || ending ? source->mapFileName : "7d30.blv";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(9);
        enum class Phase
        {
            Warmup, BeforeDoor, Clicked, Entered,
            LearningOpen, LearningBefore, LearningReady, LearningClicked, LearningFinished,
            EndingOpen, EndingBefore, EndingReady, EndingClicked, EndingMovie,
            EndingClosing, EndingContinuing, EndingFinished
        };
        Phase phase = ending ? Phase::EndingOpen : (blasterLearning ? Phase::LearningOpen : Phase::Warmup);
        uint64_t started = 0;
        unsigned frames = 0, clickedFrame = 0, captures = 0;
        size_t hiddenActors = 0;
        bool complete = false, npcVisible = false, sourceLabelVisible = false;
        struct BlasterObservation
        {
            uint64_t memberId = 0;
            std::string className;
            uint32_t level = 0;
            SkillMastery mastery = SkillMastery::None;
            bool eligible = false;
        };
        std::vector<BlasterObservation> blasterBefore;
        std::unordered_set<uint32_t> learningQuestBits;
        int learningGold = 0;
        uint64_t skillFixtureMemberId = 0;
        size_t learnedMembers = 0, preservedMembers = 0;
        std::vector<std::pair<uint64_t, uint32_t>> endingExperience;
        bool inheritedQ642 = false, inheritedQ747 = false;
        int endingGold = 0;
        uint64_t itemFixtureMemberId = 0, movieStartedAt = 0;
        unsigned movieFrame = 0, nextMovieCaptureFrame = 0, escapeFrame = 0;
        unsigned normalFrames = 0, blankMovieCaptures = 0;
        double normalSeconds = 0.0;
        size_t movieNonblackPixels = 0;
        bool movieStarted = false, movieCaptured = false, movieClosed = false, endingSaved = false;
        bool movieSourceVerified = false;
        std::string pendingCapture;
        constexpr size_t DoorFace = 1962;
        const auto scene = [&]() -> IndoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Indoor
                && game.m_gameSession.currentMapFileName() == "7d30.blv", "Expected the actual 7d30 indoor scene");
            return *static_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto screen = [&]() -> GameplayScreenRuntime &
        {
            return game.m_gameSession.gameplayScreenRuntime();
        };
        const auto checkGate = [&]()
        {
            const auto [yellow, red] = EventRuntime::enemyDetectorState(scene().worldRuntime());
            require(scene().party().hasQuestBit(611) && !scene().party().hasQuestBit(612)
                && scene().party().hasQuestBit(642) && scene().eventRuntimeState()->mapVars[6] == 2
                && !yellow && !red, "Declared source gate changed before/after the real click");
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto queueClick = [&](int width, int height)
        {
            const auto hit = scene().worldRuntime().pickMouseInteractionTarget(
                scene().worldRuntime().buildWorldPickRequest(
                    {float(width) * 0.5f, float(height) * 0.5f, width, height, true}));
            require(hit.eventTarget && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                && hit.eventTarget->targetIndex == DoorFace && hit.eventTarget->triggeredEventId == 416
                && scene().worldRuntime().canActivateWorldHit(hit, GameplayInteractionMethod::Mouse),
                "Camera center does not hit the original activatable event416 face");
            trace << "{\"stage\":\"real-door-pick\",\"face\":" << hit.eventTarget->targetIndex
                << ",\"event\":" << hit.eventTarget->triggeredEventId
                << ",\"distance\":" << hit.eventTarget->distance << "}\n";
            SDL_Window *pWindow = SDL_GetKeyboardFocus();
            require(pWindow != nullptr, "Capture window has no keyboard focus");
            int pointWidth = 0, pointHeight = 0;
            require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot query SDL window points");
            const auto button = [&](uint8_t id, bool down)
            {
                SDL_Event event{};
                event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID = SDL_GetWindowID(pWindow);
                event.button.button = id;
                event.button.down = down;
                event.button.clicks = 1;
                event.button.x = float(pointWidth) * 0.5f;
                event.button.y = float(pointHeight) * 0.5f;
                require(SDL_PushEvent(&event), "Cannot queue ordinary SDL mouse edge");
            };
            // GameInputSystem delivers one queued edge per frame, preserving the right-held cursor gesture.
            button(SDL_BUTTON_RIGHT, true);
            button(SDL_BUTTON_LEFT, true);
            button(SDL_BUTTON_LEFT, false);
            button(SDL_BUTTON_RIGHT, false);
            clickedFrame = frames;
        };
        const auto observeBlaster = [&](const Character &member)
        {
            const CharacterSkill *pSkill = member.findSkill("Blaster");
            require(screen().classSkillTable() != nullptr, "Missing class skill table");
            return BlasterObservation{member.memberId, member.className,
                pSkill != nullptr ? pSkill->level : 0,
                pSkill != nullptr ? pSkill->mastery : SkillMastery::None,
                screen().classSkillTable()->getClassCap(member.className, "Blaster") >= SkillMastery::Normal};
        };
        const auto isUnlearned = [](const BlasterObservation &skill)
        {
            return skill.level == 0 && skill.mastery <= SkillMastery::Normal;
        };
        const auto recordBlaster = [&](const std::string &stage, const std::vector<Character> &members)
        {
            trace << "{\"stage\":" << std::quoted(stage) << ",\"members\":[";
            for (size_t index = 0; index < members.size(); ++index)
            {
                const BlasterObservation skill = observeBlaster(members[index]);
                if (index != 0) trace << ',';
                trace << "{\"member_id\":" << skill.memberId << ",\"class\":" << std::quoted(skill.className)
                    << ",\"level\":" << skill.level << ",\"mastery\":" << int(skill.mastery)
                    << ",\"class_eligible\":" << skill.eligible << '}';
            }
            trace << "]}\n";
        };
        const auto verifyLearning = [&](const Party::Snapshot &party)
        {
            require(party.gold == learningGold && party.questBits == learningQuestBits,
                "The lesson changed Gold or quest bits");
            require(party.members.size() == blasterBefore.size(), "The lesson changed the member count");
            for (size_t index = 0; index < blasterBefore.size(); ++index)
            {
                const BlasterObservation &before = blasterBefore[index];
                const BlasterObservation after = observeBlaster(party.members[index]);
                const bool learned = before.eligible && isUnlearned(before);
                require(after.memberId == before.memberId && after.className == before.className
                    && after.level == (learned ? 1u : before.level)
                    && after.mastery == (learned ? SkillMastery::Normal : before.mastery),
                    "Blaster learning or preservation failed for member " + std::to_string(before.memberId));
            }
        };
        const auto learningAction = [&]() -> const EventDialogAction &
        {
            const EventDialogContent &dialog = screen().activeEventDialog();
            require(dialog.isActive && !dialog.isHouseDialog && dialog.sourceId == 422,
                "Expected the real NPC422 dialogue");
            require(screen().npcDialogTable() != nullptr, "Missing actual NPC table");
            const auto topic = screen().npcDialogTable()->getTopicById(950);
            require(topic && topic->topic == "榴弹枪", "Imported topic950 is not the source Blaster label");
            const size_t selected = screen().eventDialogSelectionIndex();
            require(selected == 0 && selected < dialog.actions.size(), "NPC topic950 is not the first selected action");
            const EventDialogAction &action = dialog.actions[selected];
            require(action.kind == EventDialogActionKind::NpcTopic && action.id == 950
                && action.enabled && !action.textOnly && action.label == topic->topic,
                "The normal NPC menu did not select the real source topic950");
            return action;
        };
        const auto endingActionIndex = [&]() -> size_t
        {
            const EventDialogContent &dialog = screen().activeEventDialog();
            require(dialog.isActive && !dialog.isHouseDialog && dialog.sourceId == endingNpc,
                "Expected the actual ending NPC dialogue");
            require(screen().npcDialogTable() != nullptr, "Missing actual NPC table");
            const auto topic = screen().npcDialogTable()->getTopicById(endingTopic);
            require(topic && !topic->topic.empty(), "Imported ending topic has no source label");
            for (size_t index = 0; index < dialog.actions.size(); ++index)
            {
                const EventDialogAction &action = dialog.actions[index];
                if (action.kind == EventDialogActionKind::NpcTopic && action.id == endingTopic)
                {
                    require(index < 5 && action.enabled && !action.textOnly && action.label == topic->topic,
                        "The source ending topic is not a visible enabled NPC action");
                    return index;
                }
            }
            throw std::runtime_error("The ending NPC does not expose source topic " + std::to_string(endingTopic));
        };
        const auto verifyEnding = [&](const Party::Snapshot &party)
        {
            require(party.questBits.contains(783) && !party.questBits.contains(endingQuestBit)
                && !party.questBits.contains(748),
                "Source MM7 completion or cleared journal bits are incorrect");
            if (darkEnding)
            {
                require(party.questBits.contains(642) == inheritedQ642
                    && party.questBits.contains(747) == inheritedQ747,
                    "Source dark ending changed unrelated Q642 or Q747");
            }
            else
            {
                require(!party.questBits.contains(747), "Source light ending did not clear Q747");
            }
            require(party.gold == endingGold && party.members.size() == endingExperience.size(),
                "The ending changed Gold or the party roster");
            for (size_t index = 0; index < endingExperience.size(); ++index)
            {
                require(party.members[index].memberId == endingExperience[index].first
                    && party.members[index].experience == endingExperience[index].second,
                    "The ending changed member identity or awarded extra XP");
            }
            const auto npc = party.npcTopicOverrides.find(endingNpc);
            require(npc != party.npcTopicOverrides.end() && npc->second.contains(1)
                && npc->second.at(1) == 0, "Source ending NPC slot1 was not cleared");
            for (uint32_t group = 61; group <= 83; ++group)
            {
                const uint32_t expectedNews = darkEnding
                    ? ((group == 67 || group == 68 || (group >= 74 && group <= 76)) ? 87u : 85u)
                    : (group >= 69 && group <= 73 ? 86u : 84u);
                const auto news = party.npcGroupNews.find(group);
                require(news != party.npcGroupNews.end() && news->second == expectedNews,
                    "Source ending NPC group news is incorrect");
            }
            Party restored;
            restored.restoreSnapshot(party);
            require(!restored.hasItemAnywhere(1407), "The single source item1407 was not consumed");
        };
        const auto openLearningNpc = [&]()
        {
            auto &world = *screen().worldRuntime();
            auto &events = *game.m_pMapSceneRuntime->eventRuntimeState();
            auto &data = game.m_gameSession.data();
            auto context = buildGameplayDialogContext(game.m_gameSession.gameplayUiController(), events,
                screen().activeEventDialog(), screen().eventDialogSelectionIndex(), screen().party(), &world,
                world.globalEventProgram(), screen().houseTable(), screen().classSkillTable(),
                screen().npcDialogTable(),
                &data.mm9RudeDialogueTable(), &data.mm9MapTransitionTable(), &data.mm9SkillTrainerTable(),
                &data.mm9TransportRouteTable(), &data.transitionTable(),
                data.mapStats().findByFileName(game.m_gameSession.currentMapFileName()), &data.mapEntries(),
                screen().rosterTable(), &data.arcomageLibrary(), false, &screen(), &data.mergedNpcProfessionTable(),
                &data.mergedNewsProfessionTopicTable(), &data.mergedNpcBtbTable(), &data.mergedBolsterMapTable(),
                &data.mergedContinentSettingTable(), &data.mergedTeacherTopicTable(),
                &data.mergedTeacherAutonoteTable(),
                &data.spellTable(), &data.characterInspectTable(), &data.houseServiceTextTable());
            screen().presentPendingDialogActionResult(
                game.m_gameSession.gameplayDialogController().openNpcDialogue(context, ending ? endingNpc : 422));
            const EventDialogAction &action = ending
                ? screen().activeEventDialog().actions[endingActionIndex()] : learningAction();
            sourceLabelVisible = true;
            trace << "{\"stage\":\"native-npc-request\",\"npc\":" << (ending ? endingNpc : 422)
                << ",\"topic\":" << action.id
                << ",\"label\":" << std::quoted(action.label) << "}\n";
        };
        const auto result = [&](bool passed, const std::string &failure)
        {
            if (ending)
            {
                std::ofstream(output / "result.json") << std::boolalpha
                    << "{\"passed\":" << passed << ",\"controlled\":true,\"natural_campaign\":false"
                    << ",\"mode\":" << std::quoted(endingName + "-ending")
                    << ",\"checkpoint\":" << (darkEnding ? 175 : 163)
                    << ",\"frames\":" << frames << ",\"captures\":" << captures
                    << ",\"topic" << endingTopic << "_ui_clicks\":" << (clickedFrame != 0 ? 1 : 0)
                    << ",\"source_label_visible\":" << sourceLabelVisible
                    << ",\"source_movie_name\":" << std::quoted(expectedMovie)
                    << ",\"source_movie_call_verified\":" << movieSourceVerified
                    << ",\"cutscene_started\":" << movieStarted << ",\"movie_capture_saved\":" << movieCaptured
                    << ",\"movie_nonblack_pixels\":" << movieNonblackPixels
                    << ",\"blank_movie_captures_discarded\":" << blankMovieCaptures
                    << ",\"movie_visual_review_required\":true,\"movie_closed_by_escape\":" << movieClosed
                    << ",\"normal_gameplay_frames\":" << normalFrames
                    << ",\"normal_gameplay_seconds\":" << normalSeconds
                    << ",\"inherited_invisible_actor_count\":" << hiddenActors
                    << ",\"item1407_fixture_member_id\":" << itemFixtureMemberId
                    << ",\"gold_before\":" << endingGold << ",\"final_save_verified\":" << endingSaved
                    << ",\"failure\":" << std::quoted(failure) << "}\n";
                return;
            }
            if (blasterLearning)
            {
                std::ofstream(output / "result.json") << std::boolalpha
                    << "{\"passed\":" << passed << ",\"controlled\":true,\"natural_campaign\":false"
                    << ",\"mode\":\"blaster-learning\",\"frames\":" << frames << ",\"captures\":" << captures
                    << ",\"topic950_ui_clicks\":" << (clickedFrame != 0 ? 1 : 0)
                    << ",\"source_label_visible\":" << sourceLabelVisible
                    << ",\"skill_fixture_member_id\":" << skillFixtureMemberId
                    << ",\"eligible_zero_members\":" << learnedMembers
                    << ",\"preserved_nonzero_members\":" << preservedMembers
                    << ",\"gold_before\":" << learningGold << ",\"quest_bit_count_before\":" << learningQuestBits.size()
                    << ",\"gold_and_quest_bits_unchanged\":" << (passed && complete)
                    << ",\"failure\":" << std::quoted(failure) << "}\n";
                return;
            }
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"passed\":" << passed << ",\"controlled\":true,\"natural_campaign\":false"
                << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"door_clicks\":" << (clickedFrame != 0 ? 1 : 0)
                << ",\"actor_invisible_fixture_count\":" << hiddenActors
                << ",\"house220_with_npc419\":" << npcVisible
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
                    require(!screen().activeEventDialog().isActive, "Unexpected loaded dialogue");
                    if (ending)
                    {
                        require(scene().eventRuntimeState() != nullptr, "Missing inherited indoor event state");
                        Party &party = scene().party();
                        EventRuntimeState &events = *scene().eventRuntimeState();
                        const Party::Snapshot inherited = party.snapshot();
                        inheritedQ642 = inherited.questBits.contains(642);
                        inheritedQ747 = inherited.questBits.contains(747);
                        require(party.hasQuestBit(611) && !party.hasQuestBit(612) && party.hasQuestBit(642)
                            && events.mapVars[6] == 2 && inherited.npcHouseOverrides.contains(419)
                            && inherited.npcHouseOverrides.at(419) == 220,
                            "Expected the accepted157 inherited entrance fixtures");
                        const MapDeltaData *pDelta = scene().worldRuntime().mapDeltaData();
                        require(pDelta != nullptr, "Missing inherited indoor actors");
                        for (const MapDeltaActor &actor : pDelta->actors)
                        {
                            require((actor.attributes & static_cast<uint32_t>(EvtActorAttribute::Invisible)) != 0,
                                "Input does not retain the declared157 invisible-actor fixture");
                            ++hiddenActors;
                        }
                        require(!party.hasItemAnywhere(1407), "Input already owns the final quest item1407");
                        const ItemTable &items = game.m_gameDataLoader.getItemTable();
                        require(items.get(1407) != nullptr, "Original item1407 is absent from the loaded item table");
                        const InventoryItem item = ItemGenerator::makeInventoryItem(
                            1407, items, ItemGenerationMode::Generic);
                        require(item.objectDescriptionId == 1407 && item.quantity == 1,
                            "Real-table item1407 fixture has the wrong identity or quantity");
                        for (size_t index = 0; index < party.memberCount(); ++index)
                        {
                            if (party.tryAutoPlaceItemInMemberInventory(index, item))
                            {
                                itemFixtureMemberId = party.member(index)->memberId;
                                break;
                            }
                        }
                        require(itemFixtureMemberId != 0 && party.inventoryItemCount(1407) == 1,
                            "Cannot place exactly one source quest item in an existing member inventory");
                        party.setQuestBit(endingQuestBit, true);
                        party.setQuestBit(783, false);
                        if (lightEnding) party.setQuestBit(747, true);
                        party.setQuestBit(748, true);
                        party.setNpcTopicOverride(endingNpc, 1, endingTopic);
                        events.npcTopicOverrides[endingNpc][1] = endingTopic;
                        endingGold = party.gold();
                        for (const Character &member : party.members())
                        {
                            endingExperience.emplace_back(member.memberId, member.experience);
                        }
                        const auto &program = scene().globalEventProgram();
                        require(program && program->luaSourceText(), "Missing the actual global Lua program");
                        const std::string &lua = *program->luaSourceText();
                        const size_t eventStart = lua.find("RegisterGlobalEvent(" + std::to_string(endingTopic) + ",");
                        const size_t eventEnd = lua.find(
                            "RegisterGlobalEvent(" + std::to_string(endingTopic + 1) + ",", eventStart);
                        require(eventStart != std::string::npos && eventEnd != std::string::npos,
                            "Cannot locate the original ending transaction");
                        const std::string event = lua.substr(eventStart, eventEnd - eventStart);
                        const size_t movieCall = event.find("evt.ShowMovie(");
                        const std::string expectedMovieCall = darkEnding
                            ? R"(evt.ShowMovie("\"Endgame 2 Evil\"", true))"
                            : R"(evt.ShowMovie("\"Endgame 1 Good\"", true))";
                        require(movieCall != std::string::npos
                            && event.find("evt.ShowMovie(", movieCall + 1) == std::string::npos
                            && event.find(expectedMovieCall) != std::string::npos,
                            "The source transaction does not contain the single expected ending movie call");
                        movieSourceVerified = true;
                        require(!events.pendingMovie && !events.pendingWinGame && !events.pendingReturnToMainMenu,
                            "Input already has a pending movie, certificate or menu transition");
                        trace << "{\"stage\":\"ending-fixture-ready\",\"npc\":" << endingNpc
                            << ",\"slot\":1,\"topic\":" << endingTopic
                            << ",\"quest_bit\":" << endingQuestBit
                            << ",\"inherited_Q642\":" << inheritedQ642 << ",\"inherited_Q747\":" << inheritedQ747
                            << ",\"item1407_before\":0,\"item1407_after_fixture\":1,\"item_member_id\":"
                            << itemFixtureMemberId << ",\"inherited_hidden_actors\":" << hiddenActors
                            << ",\"gold\":" << endingGold << ",\"source_movie\":"
                            << std::quoted(expectedMovie) << ",\"members\":[";
                        for (size_t index = 0; index < endingExperience.size(); ++index)
                        {
                            if (index != 0) trace << ',';
                            trace << "{\"member_id\":" << endingExperience[index].first
                                << ",\"xp\":" << endingExperience[index].second << '}';
                        }
                        trace << "]}\n";
                        started = SDL_GetTicks();
                        return true;
                    }
                    if (blasterLearning)
                    {
                        require(game.m_pMapSceneRuntime && screen().party()
                            && game.m_pMapSceneRuntime->eventRuntimeState() && screen().worldRuntime(),
                            "Missing loaded MM7 scene state");
                        Party &party = *screen().party();
                        party.setQuestBit(631, true);
                        party.setNpcTopicOverride(422, 0, 950);
                        game.m_pMapSceneRuntime->eventRuntimeState()->npcTopicOverrides[422][0] = 950;
                        recordBlaster("input-blaster", party.members());
                        size_t learner = party.memberCount();
                        bool hasNonzero = false;
                        for (size_t index = 0; index < party.memberCount(); ++index)
                        {
                            const BlasterObservation skill = observeBlaster(*party.member(index));
                            hasNonzero = hasNonzero || !isUnlearned(skill);
                            if (learner == party.memberCount() && skill.eligible && isUnlearned(skill)) learner = index;
                        }
                        require(learner < party.memberCount(), "Input has no eligible unlearned Blaster member");
                        if (!hasNonzero)
                        {
                            require(party.memberCount() >= 2, "Preservation fixture needs another member");
                            Character &member = *party.member(learner == 0 ? 1 : 0);
                            CharacterSkill *pSkill = member.findSkill("Blaster");
                            if (pSkill == nullptr) pSkill = &member.skills["Blaster"];
                            *pSkill = CharacterSkill{"Blaster", 5, SkillMastery::Expert};
                            skillFixtureMemberId = member.memberId;
                        }
                        for (const Character &member : party.members())
                        {
                            const BlasterObservation skill = observeBlaster(member);
                            blasterBefore.push_back(skill);
                            if (skill.eligible && isUnlearned(skill)) ++learnedMembers;
                            if (!isUnlearned(skill)) ++preservedMembers;
                        }
                        require(learnedMembers > 0 && preservedMembers > 0, "Incomplete bounded Blaster fixture");
                        learningGold = party.gold();
                        learningQuestBits = party.snapshot().questBits;
                        recordBlaster("fixture-blaster", party.members());
                        trace << "{\"stage\":\"learning-fixture-ready\",\"q631\":true,\"npc\":422,\"slot\":0,"
                            "\"topic\":950,\"skill_fixture_member_id\":" << skillFixtureMemberId
                            << ",\"gold\":" << learningGold << ",\"quest_bits\":" << learningQuestBits.size() << "}\n";
                        started = SDL_GetTicks();
                        return true;
                    }
                    const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName("7d30.blv");
                    require(pMap != nullptr, "Lambent Castle is absent from MapStats");
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared 7d30 map fixture failed");
                    scene().party().setQuestBit(611, true);
                    scene().party().setQuestBit(612, false);
                    scene().party().setQuestBit(642, true);
                    scene().party().setNpcHouseOverride(419, 220);
                    scene().eventRuntimeState()->npcHouseOverrides[419] = 220;
                    scene().eventRuntimeState()->mapVars[6] = 2;
                    MapDeltaData *pDelta = scene().worldRuntime().mapDeltaData();
                    require(pDelta != nullptr, "Missing source indoor actors");
                    hiddenActors = pDelta->actors.size();
                    for (MapDeltaActor &actor : pDelta->actors)
                    {
                        actor.attributes |= static_cast<uint32_t>(EvtActorAttribute::Invisible);
                    }
                    const auto &selected = game.m_gameDataLoader.getSelectedMap();
                    require(selected && selected->indoorMapData, "Missing original indoor geometry");
                    const auto &map = *selected->indoorMapData;
                    const auto &face = map.faces.at(DoorFace);
                    require(face.cogTriggered == 416 && face.vertexIndices.size() == 4
                        && hasFaceAttribute(face.attributes, FaceAttribute::Clickable)
                        && !hasFaceAttribute(face.attributes, FaceAttribute::Invisible),
                        "Original lower-left throne-room door face changed");
                    float x = 0, y = 0, z = 0, bottom = 32767;
                    for (uint16_t index : face.vertexIndices)
                    {
                        const auto &vertex = map.vertices.at(index);
                        x += float(vertex.x); y += float(vertex.y); z += float(vertex.z);
                        bottom = std::min(bottom, float(vertex.z));
                    }
                    x *= 0.25f; y *= 0.25f; z *= 0.25f;
                    require(x == -3584 && y == 12864 && bottom == 1120, "Original door coordinates changed");
                    scene().partyRuntime().teleportPartyPosition(x, y - 256, bottom + 1);
                    const auto &pose = scene().partyRuntime().movementState();
                    require(pose.sectorId >= 0, "Declared door position has no indoor sector");
                    game.m_indoorRenderer.setCameraAngles(std::atan2(y - pose.y, x - pose.x),
                        std::atan2(z - pose.eyeZ(), std::hypot(x - pose.x, y - pose.y)));
                    checkGate();
                    require(game.quickSaveToPath(output / "declared-fixture.oysav", "157受控辉煌城堡门前"),
                        "Cannot save the declared fixture");
                    trace << "{\"stage\":\"fixture-ready\",\"face\":" << DoorFace
                        << ",\"pose\":[" << pose.x << ',' << pose.y << ',' << pose.footZ
                        << "],\"sector\":" << pose.sectorId << ",\"hidden_actors\":" << hiddenActors << "}\n";
                    started = SDL_GetTicks();
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float delta)
                {
                    require(++frames <= (ending ? 1200u : 300u)
                        && SDL_GetTicks() - started < (ending ? 30000u : 90000u), "Bounded capture timed out");
                    const bool normalGameplay = ending && phase == Phase::EndingContinuing;
                    const bool normalDelta = ending && (normalGameplay || phase == Phase::EndingFinished
                        || dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr);
                    game.renderFrame(width, height, wheel, normalDelta ? delta : 0.0f);
                    auto *pMovie = dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen());
                    require(game.m_screenManager.activeScreen() == nullptr
                        || (ending && pMovie != nullptr && (phase == Phase::EndingClicked
                            || phase == Phase::EndingMovie || phase == Phase::EndingClosing)),
                        "Unexpected full-screen interruption");
                    if (ending && phase == Phase::EndingMovie)
                    {
                        require(pMovie != nullptr && SDL_GetTicks() - movieStartedAt < 10000,
                            "Ending movie closed or exceeded the ten-second capture deadline");
                    }
                    if (normalGameplay)
                    {
                        require(game.m_screenManager.activeScreen() == nullptr
                            && !screen().activeEventDialog().isActive && scene().party().hasActableMember(),
                            "Normal post-ending gameplay was interrupted");
                        if (delta > 0.0f)
                        {
                            ++normalFrames;
                            normalSeconds += delta;
                        }
                    }
                    if (!pendingCapture.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            const bool movieImage = ending && pendingCapture == endingCapturePrefix + "-movie";
                            if (movieImage)
                            {
                                require(pMovie != nullptr, "Movie ended before its screenshot was collected");
                                movieNonblackPixels = 0;
                                for (size_t pixel = 0; pixel + 2 < image->bgraPixels.size(); pixel += 4)
                                {
                                    if (image->bgraPixels[pixel] > 16 || image->bgraPixels[pixel + 1] > 16
                                        || image->bgraPixels[pixel + 2] > 16) ++movieNonblackPixels;
                                }
                                // Only reject blank frames; the actual movie image still needs human review.
                                if (movieNonblackPixels < size_t(image->width) * image->height / 100)
                                {
                                    ++blankMovieCaptures;
                                    trace << "{\"stage\":\"blank-movie-capture-discarded\",\"frame\":" << frames
                                        << ",\"nonblack_pixels\":" << movieNonblackPixels << "}\n";
                                    pendingCapture.clear();
                                    nextMovieCaptureFrame = frames + 15;
                                    return;
                                }
                            }
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create screenshot surface");
                            const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(saved, "Cannot save screenshot");
                            trace << "{\"capture\":" << std::quoted(pendingCapture) << "}\n";
                            if (movieImage) movieCaptured = true;
                            pendingCapture.clear();
                            ++captures;
                        }
                        return;
                    }
                    if (phase == Phase::EndingOpen && frames >= 8)
                    {
                        openLearningNpc();
                        phase = Phase::EndingBefore;
                    }
                    else if (phase == Phase::EndingBefore)
                    {
                        endingActionIndex();
                        capture("before-" + endingCapturePrefix + "-topic" + std::to_string(endingTopic));
                        phase = Phase::EndingReady;
                    }
                    else if (phase == Phase::EndingClicked)
                    {
                        if (pMovie != nullptr)
                        {
                            verifyEnding(scene().party().snapshot());
                            require(!scene().eventRuntimeState()->pendingWinGame
                                && !scene().eventRuntimeState()->pendingReturnToMainMenu,
                                "The ending incorrectly requested a certificate or main menu");
                            movieStarted = true;
                            movieFrame = frames;
                            movieStartedAt = SDL_GetTicks();
                            nextMovieCaptureFrame = movieFrame + 65;
                            trace << "{\"stage\":\"actual-cutscene-started\",\"frame\":" << movieFrame
                                << ",\"source" << endingTopic << "_movie_name\":" << std::quoted(expectedMovie)
                                << ",\"source_transaction_verified\":true}\n";
                            phase = Phase::EndingMovie;
                        }
                        else require(frames < clickedFrame + 20, "The real ending topic click did not start its movie");
                    }
                    else if (phase == Phase::EndingMovie)
                    {
                        if (!movieCaptured && frames >= nextMovieCaptureFrame)
                        {
                            capture(endingCapturePrefix + "-movie");
                        }
                        else if (movieCaptured)
                        {
                            SDL_Event escape{};
                            escape.type = SDL_EVENT_KEY_DOWN;
                            escape.key.key = SDLK_ESCAPE;
                            escape.key.scancode = SDL_SCANCODE_ESCAPE;
                            escape.key.down = true;
                            require(SDL_PushEvent(&escape), "Cannot queue ordinary movie Escape down");
                            escape.type = SDL_EVENT_KEY_UP;
                            escape.key.down = false;
                            require(SDL_PushEvent(&escape), "Cannot queue ordinary movie Escape up");
                            escapeFrame = frames;
                            trace << "{\"stage\":\"queued-movie-escape\",\"frame\":" << frames << "}\n";
                            phase = Phase::EndingClosing;
                        }
                    }
                    else if (phase == Phase::EndingClosing)
                    {
                        if (pMovie == nullptr)
                        {
                            movieClosed = true;
                            screen().closeActiveEventDialog();
                            phase = Phase::EndingContinuing;
                            trace << "{\"stage\":\"movie-closed-resume-gameplay\",\"frame\":" << frames << "}\n";
                        }
                        else require(frames < escapeFrame + 20, "Ordinary Escape did not close the movie");
                    }
                    else if (phase == Phase::EndingContinuing && normalFrames >= 30)
                    {
                        verifyEnding(scene().party().snapshot());
                        const EventRuntimeState &events = *scene().eventRuntimeState();
                        require(!events.pendingMovie && !events.pendingWinGame && !events.pendingReturnToMainMenu,
                            "Continued gameplay retained an ending transition");
                        require(events.npcTopicOverrides.contains(endingNpc)
                            && events.npcTopicOverrides.at(endingNpc).contains(1)
                            && events.npcTopicOverrides.at(endingNpc).at(1) == 0,
                            "Live ending NPC topic was not cleared");
                        const auto finalPath = output / ("mm7-" + endingName + "-ending-complete.oysav");
                        require(game.quickSaveToPath(finalPath,
                            darkEnding ? "175黑暗结局后继续游戏" : "163光明结局后继续游戏"),
                            "Cannot save completed ending");
                        const auto saved = loadGameDataFromPath(finalPath, error);
                        require(saved && saved->mapFileName == source->mapFileName && saved->hasIndoorSceneState
                            && saved->indoorScene.eventRuntimeState && !saved->heldInventoryItemActive
                            && saved->party.pendingItemDeliveries.empty(), "Final ending save is invalid: " + error);
                        verifyEnding(saved->party);
                        const auto &savedTopics = saved->indoorScene.eventRuntimeState->npcTopicOverrides;
                        require(savedTopics.contains(endingNpc) && savedTopics.at(endingNpc).contains(1)
                            && savedTopics.at(endingNpc).at(1) == 0, "Saved ending NPC topic was not cleared");
                        endingSaved = true;
                        trace << "{\"stage\":\"continued-game-saved\",\"normal_frames\":" << normalFrames
                            << ",\"normal_seconds\":" << normalSeconds
                            << ",\"Q783\":true,\"Q" << endingQuestBit << "\":false"
                            << ",\"Q747\":" << saved->party.questBits.contains(747)
                            << ",\"Q748\":false,\"item1407\":0,\"npc" << endingNpc << "_slot1\":0"
                            << ",\"unrelated_Q642_retained\":" << (!darkEnding
                                || saved->party.questBits.contains(642) == inheritedQ642)
                            << ",\"gold_xp_unchanged\":true}\n";
                        capture("after-" + endingCapturePrefix + "-continued-game");
                        phase = Phase::EndingFinished;
                    }
                    else if (phase == Phase::EndingFinished)
                    {
                        require(captures == 3 && clickedFrame != 0 && sourceLabelVisible && movieSourceVerified
                            && movieStarted && movieCaptured && movieClosed && normalFrames >= 30 && endingSaved,
                            "The bounded ending evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    else if (phase == Phase::LearningOpen && frames >= 8)
                    {
                        openLearningNpc();
                        phase = Phase::LearningBefore;
                    }
                    else if (phase == Phase::LearningBefore)
                    {
                        learningAction();
                        capture("before-source-blaster-topic950");
                        phase = Phase::LearningReady;
                    }
                    else if (phase == Phase::LearningReady || phase == Phase::EndingReady)
                    {
                        const size_t targetIndex = ending ? endingActionIndex() : 0;
                        if (!ending) learningAction();
                        require(clickedFrame == 0, "Only one source NPC topic click is allowed");
                        // Match the live standard NPC panel's row geometry; do not synthesize an action.
                        const auto resolve = [&](const std::string &id)
                        {
                            const auto *pLayout = screen().findHudLayoutElement(id);
                            require(pLayout != nullptr, "Missing actual dialogue layout: " + id);
                            const auto rect = screen().resolveHudLayoutElement(
                                id, width, height, pLayout->width, pLayout->height);
                            require(rect.has_value(), "Cannot resolve actual dialogue layout: " + id);
                            return *rect;
                        };
                        const auto panel = resolve("DialogueEventDialog");
                        const auto row = resolve("DialogueTopicRow_1");
                        const auto portrait = resolve("DialogueNpcPortrait");
                        const auto name = resolve("DialogueNpcName");
                        const auto *pRow = screen().findHudLayoutElement("DialogueTopicRow_1");
                        const auto font = screen().findHudFont(pRow->fontName);
                        require(font.has_value(), "Missing actual dialogue topic font");
                        const float fontScale = GameplayHudCommon::snappedHudFontScale(row.scale);
                        const float lineHeight = float(font->fontHeight) * fontScale;
                        const float textWidth = std::max(0.0f,
                            (std::min(row.width, 140.0f * row.scale)
                                - std::abs(pRow->textPadX * fontScale) * 2.0f - 4.0f * fontScale)
                                / std::max(1.0f, fontScale));
                        const auto &dialog = screen().activeEventDialog();
                        require(dialog.presentation == EventDialogPresentation::Standard && !dialog.title.empty()
                            && game.m_pMapSceneRuntime->eventRuntimeState()->dialogueState.hostHouseId == 0,
                            "Expected the declared direct standard NPC panel");
                        const float portraitY = std::round(std::max(panel.y + 10.0f * panel.scale, portrait.y));
                        const float contentY = portraitY + name.y - portrait.y + name.height + 8.0f * panel.scale;
                        const float availableHeight = panel.y + panel.height - 10.0f * panel.scale - contentY;
                        const size_t visible = std::min<size_t>(dialog.actions.size(), 5);
                        float totalHeight = 0.0f, targetHeight = 0.0f, targetOffset = 0.0f;
                        for (size_t index = 0; index < visible; ++index)
                        {
                            std::string label = dialog.actions[index].label;
                            if (!dialog.actions[index].enabled && !dialog.actions[index].disabledReason.empty())
                            {
                                label += " [disabled]";
                            }
                            const size_t lines = std::max<size_t>(1,
                                screen().wrapHudTextToWidth(*font, label, textWidth).size());
                            const float rowHeight = std::max(row.height, float(lines) * lineHeight);
                            if (index == targetIndex) targetHeight = rowHeight;
                            if (index < targetIndex) targetOffset += rowHeight + 4.0f * panel.scale;
                            totalHeight += rowHeight + (index + 1 < visible ? 4.0f * panel.scale : 0.0f);
                        }
                        const float pixelX = row.x + row.width * 0.5f;
                        const float pixelY = contentY + std::max(0.0f, (availableHeight - totalHeight) * 0.5f)
                            - 8.0f * panel.scale + targetOffset + targetHeight * 0.5f;
                        int windowCount = 0;
                        SDL_Window **pWindows = SDL_GetWindows(&windowCount);
                        SDL_Window *pWindow = pWindows && windowCount == 1 ? pWindows[0] : nullptr;
                        SDL_free(pWindows);
                        require(pWindow != nullptr, "Topic click needs the single capture window");
                        int pointWidth = 0, pointHeight = 0;
                        require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight),
                            "Cannot query SDL window points");
                        SDL_Event click{};
                        click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                        click.button.windowID = SDL_GetWindowID(pWindow);
                        click.button.button = SDL_BUTTON_LEFT;
                        click.button.down = true;
                        click.button.clicks = 1;
                        click.button.x = pixelX * float(pointWidth) / float(width);
                        click.button.y = pixelY * float(pointHeight) / float(height);
                        require(SDL_PushEvent(&click), "Cannot queue ordinary topic click down");
                        click.type = SDL_EVENT_MOUSE_BUTTON_UP;
                        click.button.down = false;
                        require(SDL_PushEvent(&click), "Cannot queue ordinary topic click up");
                        clickedFrame = frames;
                        trace << "{\"stage\":\"queued-ordinary-topic-click\",\"topic\":" << (ending ? endingTopic : 950)
                            << ",\"pixel_x\":"
                            << pixelX << ",\"pixel_y\":" << pixelY << "}\n";
                        phase = ending ? Phase::EndingClicked : Phase::LearningClicked;
                    }
                    else if (phase == Phase::LearningClicked && frames >= clickedFrame + 6)
                    {
                        verifyLearning(screen().party()->snapshot());
                        recordBlaster("after-one-source-lesson", screen().party()->members());
                        require(game.quickSaveToPath(output / "learned-blaster.oysav", "160原话题榴弹枪入门"),
                            "Cannot save the actual Blaster lesson result");
                        const auto saved = loadGameDataFromPath(output / "learned-blaster.oysav", error);
                        require(saved && saved->mapFileName == source->mapFileName,
                            "Final lesson save is absent or changed maps: " + error);
                        verifyLearning(saved->party);
                        capture("after-one-source-blaster-lesson");
                        phase = Phase::LearningFinished;
                    }
                    else if (phase == Phase::LearningFinished)
                    {
                        require(captures == 2 && sourceLabelVisible && clickedFrame != 0,
                            "Blaster lesson evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    else if (phase == Phase::Warmup && frames >= 8)
                    {
                        checkGate();
                        capture("before-door");
                        phase = Phase::BeforeDoor;
                    }
                    else if (phase == Phase::BeforeDoor)
                    {
                        queueClick(width, height);
                        phase = Phase::Clicked;
                    }
                    else if (phase == Phase::Clicked && frames >= clickedFrame + 6)
                    {
                        checkGate();
                        const auto &dialog = screen().activeEventDialog();
                        require(dialog.isActive && dialog.isHouseDialog && dialog.sourceId == 220,
                            "Real event416 click did not enter house220 while MapVar6 remained 2");
                        const auto npc = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                            [](const auto &action)
                            {
                                return action.kind == EventDialogActionKind::HouseResident && action.id == 419
                                    && action.enabled && !action.textOnly && !action.label.empty();
                            });
                        require(npc != dialog.actions.end(), "House220 does not show the declared quest NPC419");
                        npcVisible = true;
                        trace << "{\"stage\":\"entered-house220\",\"title\":" << std::quoted(dialog.title)
                            << ",\"npc_id\":419,\"npc_label\":" << std::quoted(npc->label)
                            << ",\"npc_picture\":" << npc->participantPictureId << "}\n";
                        require(game.quickSaveToPath(output / "entered-house220.oysav", "157辉煌城堡王座入口"),
                            "Cannot save the actual door result");
                        const auto saved = loadGameDataFromPath(output / "entered-house220.oysav", error);
                        require(saved && saved->mapFileName == "7d30.blv" && saved->hasIndoorSceneState
                            && saved->indoorScene.eventRuntimeState
                            && saved->indoorScene.eventRuntimeState->mapVars[6] == 2
                            && saved->party.questBits.contains(611) && !saved->party.questBits.contains(612)
                            && saved->party.questBits.contains(642), "Saved gate state differs from the live result");
                        capture("house220-npc419");
                        phase = Phase::Entered;
                    }
                    else if (phase == Phase::Entered)
                    {
                        require(captures == 2 && npcVisible, "Door evidence is incomplete");
                        complete = true;
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot queue normal App exit");
                    }
                    trace.flush();
                },
                [&]() { game.shutdownApplication(); });
            require(app.run() == 0 && complete, "App ended before the single requested boundary completed");
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
    const bool blasterLearning = argc == 5 && std::string(argv[4]) == "--blaster-learning";
    const bool lightEnding = argc == 5 && std::string(argv[4]) == "--light-ending";
    const bool darkEnding = argc == 5 && std::string(argv[4]) == "--dark-ending";
    if (argc != 4 && !blasterLearning && !lightEnding && !darkEnding)
    {
        std::cerr << "lambent_castle_capture RUNTIME READONLY_INPUT_SAVE NEW_OUTPUT "
            "[--blaster-learning|--light-ending|--dark-ending]\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(
            argv[1], argv[2], argv[3], blasterLearning, lightEnding, darkEnding);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Lambent Castle capture: " << failure.what() << '\n';
        return 1;
    }
}
