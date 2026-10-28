#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/FaceEnums.h"
#include "game/StringUtils.h"
#include "game/app/GameApplication.h"
#include "game/content/BreachMazeRuntimeProfile.h"
#include "game/gameplay/GameplayDialogContextBuilder.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/screens/CutsceneVideoScreen.h"

#include <CommonCrypto/CommonDigest.h>
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
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr float FrameSeconds = 1.0f / 60.0f;
constexpr const char *QuestPrefix = "MMerge.CrossContinents.";
constexpr uint32_t BreachTravelTopic = 65517;

void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

std::string hashBytes(const std::vector<uint8_t> &bytes)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash{};
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), hash.data());
    std::ostringstream result;
    for (unsigned char byte : hash) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string fileHash(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot hash read-only input");
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    std::array<char, 65536> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
        CC_SHA256_Update(&context, buffer.data(), CC_LONG(input.gcount()));
    require(input.eof(), "Cannot finish input hash");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash{};
    CC_SHA256_Final(hash.data(), &context);
    std::ostringstream result;
    for (unsigned char byte : hash) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string itemsHash(const Character &member)
{
    Character carrier;
    carrier.inventory = member.inventory;
    carrier.equipment = member.equipment;
    carrier.equipmentRuntime = member.equipmentRuntime;
    Party::Snapshot party;
    party.members.push_back(std::move(carrier));
    return hashBytes(encodePartySnapshotForComparison(party));
}

std::vector<uint8_t> retainedMemberBytes(const Party::Snapshot &party, size_t index, bool clearAwards)
{
    Party::Snapshot comparison;
    comparison.members.push_back(party.members.at(index));
    comparison.characterBuffs[0] = party.characterBuffs.at(index);
    comparison.nextMemberId = party.nextMemberId;
    Character &member = comparison.members.front();
    if (clearAwards) member.awards.clear();
    // Renderer initialization owns these five animation fields; portrait identity remains compared.
    member.portraitState = PortraitId::Normal;
    member.portraitElapsedTicks = 0;
    member.portraitDurationTicks = 0;
    member.portraitSequenceCounter = 0;
    member.portraitImageIndex = 0;
    return encodePartySnapshotForComparison(comparison);
}

uint32_t inventoryCount(const Party::Snapshot &party, uint32_t itemId)
{
    uint32_t count = 0;
    for (const Character &member : party.members)
        for (const InventoryItem &item : member.inventory)
            if (item.objectDescriptionId == itemId) count += item.quantity;
    return count;
}

size_t pendingDeliveryCount(const Party::Snapshot &party)
{
    size_t count = party.pendingItemDeliveries.size();
    for (const auto &member : party.detachedMembers) count += member.pendingItemDeliveries.size();
    return count;
}

void checkCustody(const Party::Snapshot &before, const Party::Snapshot &after)
{
    require(before.members.size() == 5 && after.members.size() == 5
        && before.detachedMembers.size() == after.detachedMembers.size()
        && before.adventurersInnMembers.size() == after.adventurersInnMembers.size()
        && before.nextMemberId == after.nextMemberId && before.gold == after.gold
        && before.bankGold == after.bankGold && before.food == after.food && before.questBits == after.questBits,
        "Normal continuation changed original Party custody, resources or quest bits");
    for (size_t index = 0; index < before.members.size(); ++index)
    {
        const Character &a = before.members[index];
        const Character &b = after.members[index];
        require(std::tie(a.memberId, a.rosterId, a.characterDataId, a.name, a.biography, a.role, a.className,
            a.portraitTextureName, a.portraitPictureId, a.voiceId, a.birthYear, a.sexId, a.raceId,
            a.level, a.experience, a.skillPoints)
            == std::tie(b.memberId, b.rosterId, b.characterDataId, b.name, b.biography, b.role, b.className,
                b.portraitTextureName, b.portraitPictureId, b.voiceId, b.birthYear, b.sexId, b.raceId,
                b.level, b.experience, b.skillPoints)
            && itemsHash(a) == itemsHash(b), "Original identity, XP or complete carried equipment changed");
    }
    // HP, SP, conditions, recovery, buff duration and AI advance normally and are recorded in the trace.
}

void writeMazeRunIdentity(std::ostream &output, const BreachMazeRunData &run)
{
    output << "{\"version\":" << run.version << ",\"seed_version\":" << run.seedVersion
        << ",\"template_fingerprint\":" << run.templateFingerprint << ",\"revision\":" << run.revision
        << ",\"root_seed\":" << run.rootSeed << ",\"epoch\":" << run.epoch
        << ",\"next_generation_serial\":" << run.nextGenerationSerial
        << ",\"current_level\":" << run.currentLevel << ",\"inside\":" << run.inside
        << ",\"direction\":" << int(run.direction) << ",\"floor_levels\":[";
    bool comma = false;
    for (const auto &entry : run.floors)
    {
        if (comma) output << ',';
        comma = true;
        output << entry.first;
    }
    output << "]}";
}

void compareFloors(const BreachMazeRunData &saved, const BreachMazeRunData &live,
    uint64_t expectedRevisionIncrease = 0)
{
    require(saved.inside && live.inside && saved.currentLevel == 4 && live.currentLevel == 4
        && std::tie(saved.version, saved.seedVersion, saved.templateFingerprint,
            saved.rootSeed, saved.epoch, saved.nextGenerationSerial, saved.direction)
            == std::tie(live.version, live.seedVersion, live.templateFingerprint,
                live.rootSeed, live.epoch, live.nextGenerationSerial, live.direction)
        && saved.floors.size() == live.floors.size(), "Cold/save changed the committed L4 run identity");
    // Codec comparisons require +0. App cold load captures the restored runtime at explicit synchronization points.
    require(live.revision >= saved.revision && live.revision - saved.revision == expectedRevisionIncrease,
        "Cold/save snapshot revision mismatch: before=" + std::to_string(saved.revision)
            + " after=" + std::to_string(live.revision)
            + " expected_increase=" + std::to_string(expectedRevisionIncrease));
    for (const auto &[level, a] : saved.floors)
    {
        const auto found = live.floors.find(level);
        require(found != live.floors.end(), "Cold/save lost a retained floor");
        const auto &b = found->second;
        require(a.descriptor.identity == b.descriptor.identity && a.descriptor.seeds == b.descriptor.seeds
            && a.descriptor.layout == b.descriptor.layout && a.population == b.population
            && a.runtime.interaction == b.runtime.interaction && a.runtime.actorRage == b.runtime.actorRage,
            "Cold/save rerolled population, layout, interactions or actor Rage");
        const auto &left = a.runtime.scene.worldRuntime.mapActorAiStates;
        const auto &right = b.runtime.scene.worldRuntime.mapActorAiStates;
        require(left.size() == right.size(), "Cold/save changed a retained actor count");
        for (size_t index = 0; index < left.size(); ++index)
            require(left[index].actorId == right[index].actorId, "Cold/save changed a persistent actor identity");
    }
}

struct PointerTarget
{
    size_t model = GameplayInvalidWorldIndex;
    size_t face = GameplayInvalidWorldIndex;
    uint16_t eventId = 0;
    std::vector<bx::Vec3> points;
};
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, const std::string &mode)
    {
        const bool cold = mode == "cold";
        const bool restart = mode == "restart";
        require(cold || restart || mode == "actual", "Expected actual, cold or restart mode");
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        require(std::filesystem::is_regular_file(root / "engine.zip")
            && std::filesystem::is_regular_file(root / "worlds/mmmerge.zip"), "Complete fresh runtime required");
        require(!std::filesystem::exists(output)
            && std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Use a new output directory on NVMeSSD");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        const std::string inputHash = fileHash(input);
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->party.members.size() == 5 && !source->heldInventoryItemActive,
            "Expected the reviewed five-member empty-held input: " + error);
        require(cold ? source->breachMaze && source->breachMaze->inside && source->breachMaze->currentLevel == 4
            : toLowerCopy(source->mapFileName) == "breach.odm", "Wrong continuation input map/floor");

        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"input\":" << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string()) << ",\"changes\":"
            << std::quoted(cold
                ? "Independent actual L4 cold load; no Party, pose, camera, event, quest, clock, RNG or AI writes."
                : restart
                ? "Completed actual003 Breach-return input. Declare exactly one source Telelocator666 in "
                  "member0 inventory. Empty held item and pending delivery are required. Supported pose/camera "
                  "at original pedestal model93/face2; actual mouse surface402 consumes the item and reveals "
                  "40 cog400 faces. Supported airborne pose above original model130/face9; ordinary gravity "
                  "landing triggers source81. SDL text y and Return queue the original once-random destination; "
                  "the next zero-dt App frame owns the campaign restart. No direct event execution, request "
                  "processing, TelelocatorSet/quest/RNG/AI writes. Explicit new-path save, ordinary SDL Escape "
                  "for the source MM7 intro when selected, then 0.5 seconds of normal continuation and one "
                  "gameplay capture. This does not test or claim automatic saving."
                : "Previously accepted controlled completed Breach input. Sole upstream quest fixture: "
                  "GotMainQuest=1, required for the ordinary root732/Hints branch. Original member2 learns31 "
                  "and WaterMagic10 Grandmaster only if needed. Preserve HP/SP, MetVerdant, completed flags, "
                  "XP, items, RNG, buffs and normal AI. Ordinary paid TownPortal/DimensionDoor leaves Breach; "
                  "one explicitly declared ordinary openNpcDialogue803 contact fixture in the normal world. "
                  "Select visible root732 and topic65517 through ordinary dialog actions. Source real-clock "
                  "message1024ms, close100ms and ordinary App map move return to Breach. Supported pose+camera "
                  "fixtures only at real Breach54 and generated L1/L2/L3 Down doors; each before/after is traced. "
                  "Actual typed surface pointers and normal transition confirmations own every level change. "
                  "No direct event execution, map/run/population writes, AI clearing or tactical retries.")
            << ",\"normal_ai\":true,\"new_test_cases\":0,\"bounds\":\"600 frames,10 simulation seconds,"
               "120 wall seconds after initialization\",\"visual_review_required\":true}\n";

        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Breach continuation155";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        trace << std::boolalpha << std::setprecision(17);
        Party::Snapshot beforeParty;
        Party::Snapshot restartBefore, restartAfter;
        std::array<std::string, 5> retainedBeforeHashes, retainedAfterHashes;
        PointerTarget pointer;
        std::string stage = "initialize";
        std::string captureLabel;
        std::string targetMap;
        std::string restartIntroMovie;
        uint64_t started = 0, topicStarted = 0, messageStarted = 0, closedAt = 0;
        unsigned frames = 0, phaseFrame = 0, captureFrame = 0, captures = 0;
        unsigned castAttempts = 0, confirmations = 0, pointerClicks = 0;
        bool captureRequested = false, complete = false, coldVerified = false, firstContactDismissed = false;
        bool messageVerified = false, closedVerified = false, returned = false;
        bool restartVerified = false;
        bool restartIntroDismissed = false, restartIntroResumed = false;
        uint32_t restartRandomBefore = 0, restartRandomAfter = 0;
        unsigned telelocatorVisibleBefore = 0, telelocatorVisibleAfter = 0;
        int appResult = -1, manaBefore = 0;
        uint32_t expectedLevel = 1;
        double simulationSeconds = 0.0;
        double restartContinueAt = 0.0;
        enum class Phase
        {
            Cast, Portal, NormalWorld, Hints, Travel, Delay, Door, Pointer, Offer, Floor, Continue, Done,
            RestartPedestal, RestartInstall, RestartLand, RestartCommit, RestartContinue
        };
        Phase phase = restart ? Phase::RestartPedestal : cold ? Phase::Continue : Phase::Cast;

        const auto screen = [&]() -> GameplayScreenRuntime &
            { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto world = [&]() -> IGameplayWorldRuntime &
        {
            require(game.m_gameSession.activeWorldRuntime() != nullptr, "No active world");
            return *game.m_gameSession.activeWorldRuntime();
        };
        const auto events = [&]() -> EventRuntimeState &
        {
            require(world().eventRuntimeState() != nullptr, "No active event runtime");
            return *world().eventRuntimeState();
        };
        const auto indoor = [&]() -> IndoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Indoor,
                "Expected the actual native indoor maze");
            return *static_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto quest = [&](const char *name)
            { return game.m_gameSession.namedGlobalVar(std::string(QuestPrefix) + name); };
        const auto record = [&](const std::string &label)
        {
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"wall_ms\":" << (started ? SDL_GetTicks() - started : 0)
                << ",\"simulation_seconds\":" << simulationSeconds << ",\"game_minutes\":" << world().gameMinutes()
                << ",\"map\":" << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"xyz\":["
                << screen().partyX() << ',' << screen().partyY() << ',' << screen().partyFootZ()
                << "],\"world_kind\":" << std::quoted(world().isIndoorMap() ? "indoor" : "outdoor")
                << ",\"quest_finished\":" << quest("QuestFinished") << ",\"got_end_card\":" << quest("GotEndCard")
                << ",\"got_main_quest\":" << quest("GotMainQuest") << ",\"met_verdant\":" << quest("MetVerdant")
                << ",\"dialogue\":" << screen().activeEventDialog().isActive
                << ",\"pending_delayed_move\":" << events().pendingDelayedMapMove.has_value()
                << ",\"event_rng\":" << events().eventRandomState << ",\"members\":[";
            for (size_t index = 0; index < screen().party()->memberCount(); ++index)
            {
                if (index) trace << ',';
                const Character &member = *screen().party()->member(index);
                trace << "{\"id\":" << member.memberId << ",\"hp\":" << member.health
                    << ",\"sp\":" << member.spellPoints << ",\"conditions\":" << member.conditions.to_ullong()
                    << ",\"recovery\":" << member.recoverySecondsRemaining << ",\"xp\":" << member.experience
                    << ",\"items_sha256\":" << std::quoted(itemsHash(member)) << '}';
            }
            trace << "],\"floors\":[";
            if (game.m_gameSession.breachMazeRunData())
            {
                bool comma = false;
                for (const auto &[level, floor] : game.m_gameSession.breachMazeRunData()->floors)
                {
                    if (comma) trace << ',';
                    comma = true;
                    trace << "{\"level\":" << level << ",\"serial\":" << floor.descriptor.identity.generationSerial
                        << ",\"population_seed\":" << floor.descriptor.seeds.population << ",\"actors\":"
                        << (floor.population ? floor.population->actors.size() : 0) << '}';
                }
            }
            trace << "]}\n";
            trace.flush();
        };
        const auto advance = [&](Phase next, const std::string &label)
        {
            phase = next;
            stage = label;
            phaseFrame = frames;
            record(label);
        };
        const auto capture = [&](const std::string &label)
        {
            require(captureLabel.empty(), "Overlapping screenshots");
            captureLabel = label;
            captureFrame = frames + 2;
            captureRequested = false;
        };
        const auto verifyParty = [&]()
        {
            checkCustody(beforeParty, screen().party()->snapshot());
            require(quest("QuestFinished") == 1 && quest("GotEndCard") == 1,
                "Return lost completion/endcard state");
        };
        const auto save = [&](const std::string &label)
        {
            verifyParty();
            require(game.quickSaveToPath(output / (label + ".oysav"), "155延续 " + label), "Save failed");
            const auto saved = loadGameDataFromPath(output / (label + ".oysav"), error);
            require(saved && saved->mapFileName == game.m_gameSession.currentMapFileName(), error);
            checkCustody(beforeParty, saved->party);
            if (world().isIndoorMap() && game.m_gameSession.breachMazeRunData()->currentLevel == 4)
            {
                require(saved->breachMaze.has_value(), "L4 save lost the maze");
                compareFloors(*game.m_gameSession.breachMazeRunData(), *saved->breachMaze);
            }
            record("saved:" + label);
        };
        const auto choose = [&](EventDialogActionKind kind, std::optional<uint32_t> id = std::nullopt)
        {
            const auto &dialog = screen().activeEventDialog();
            require(dialog.isActive, "Requested ordinary action has no dialogue");
            const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(), [&](const auto &action)
                { return action.kind == kind && (!id || action.id == *id) && action.enabled && !action.textOnly; });
            require(found != dialog.actions.end(), "Requested ordinary action is absent");
            screen().eventDialogSelectionIndex() = size_t(found - dialog.actions.begin());
            screen().executeActiveDialogAction();
        };
        const auto openVerdant = [&]()
        {
            auto &data = game.m_gameSession.data();
            auto context = buildGameplayDialogContext(game.m_gameSession.gameplayUiController(), events(),
                screen().activeEventDialog(), screen().eventDialogSelectionIndex(), screen().party(), &world(),
                world().globalEventProgram(), screen().houseTable(), screen().classSkillTable(),
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
                game.m_gameSession.gameplayDialogController().openNpcDialogue(context, 803));
            require(screen().activeEventDialog().isActive && screen().activeEventDialog().sourceId == 803,
                "Ordinary Verdant contact fixture did not present NPC803");
            record("declared-ordinary-npc-contact-fixture");
        };
        const auto aim = [&](const bx::Vec3 &center, float eyeZ)
        {
            const float dx = center.x - screen().partyX(), dy = center.y - screen().partyY();
            const float yaw = std::atan2(dy, dx), pitch = std::atan2(center.z - eyeZ, std::hypot(dx, dy));
            if (world().isIndoorMap()) game.m_indoorRenderer.setCameraAngles(yaw, pitch);
            else game.m_outdoorGameView.setCameraAngles(yaw, pitch);
        };
        const auto prepareDoor = [&]()
        {
            record("before-declared-door-pose");
            pointer = {};
            bx::Vec3 center{0, 0, 0};
            if (world().isIndoorMap())
            {
                const auto &run = *game.m_gameSession.breachMazeRunData();
                const auto &floor = run.floors.at(run.currentLevel);
                const auto &profile =
                    game.m_gameDataLoader.getOrCreateBreachMazeRuntimeProfile(*game.m_pAssetFileSystem);
                const auto &endpoint = floor.descriptor.layout.endpoints.exit;
                const auto &cell = profile.mazeTemplate.cells.at(endpoint.templateCellId - 1);
                const auto &selected = game.m_gameDataLoader.getSelectedMap();
                require(selected && selected->indoorMapData, "No actual generated geometry");
                const auto &map = *selected->indoorMapData;
                const auto &face = map.faces.at(endpoint.faceIndex);
                require(face.cogTriggered == 2 && !face.vertexIndices.empty(),
                    "Generated Down endpoint is not original event2");
                pointer.face = endpoint.faceIndex;
                pointer.eventId = 2;
                for (uint16_t index : face.vertexIndices)
                {
                    const auto &v = map.vertices.at(index);
                    pointer.points.push_back({float(v.x), float(v.y), float(v.z)});
                }
                indoor().partyRuntime().teleportPartyPosition(
                    601.5f * float(cell.x - 1), -601.5f * float(cell.y - 1), -200);
                for (const auto &point : pointer.points) center = bx::add(center, point);
                center = bx::mul(center, 1.0f / float(pointer.points.size()));
                aim(center, indoor().partyRuntime().movementState().eyeZ());
            }
            else
            {
                require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm",
                    "Outdoor door fixture is restricted to the real Breach54");
                const auto *pMap = game.m_pOutdoorWorldRuntime->mapData();
                require(pMap != nullptr, "No actual Breach geometry");
                const OutdoorBModelFace *pFace = nullptr;
                for (size_t model = 0; model < pMap->bmodels.size(); ++model)
                    for (size_t face = 0; face < pMap->bmodels[model].faces.size(); ++face)
                        if (pMap->bmodels[model].faces[face].cogTriggeredNumber == 54)
                        {
                            require(pFace == nullptr, "Ambiguous Breach54 source face");
                            pFace = &pMap->bmodels[model].faces[face];
                            pointer.model = model;
                            pointer.face = face;
                        }
                require(pFace && !pFace->vertexIndices.empty(), "No original Breach54 face");
                pointer.eventId = 54;
                float bottom = std::numeric_limits<float>::max();
                for (uint16_t index : pFace->vertexIndices)
                {
                    const auto &v = pMap->bmodels[pointer.model].vertices.at(index);
                    const bx::Vec3 point{float(v.x), float(v.y), float(v.z)};
                    pointer.points.push_back(point);
                    center = bx::add(center, point);
                    bottom = std::min(bottom, point.z);
                }
                center = bx::mul(center, 1.0f / float(pointer.points.size()));
                const float nx = float(pFace->planeNormalX), ny = float(pFace->planeNormalY);
                const float length = std::hypot(nx, ny);
                require(length > 0, "Breach54 has no horizontal door normal");
                game.m_pOutdoorPartyRuntime->teleportTo(
                    center.x + nx / length * 256, center.y + ny / length * 256, bottom + 1);
                const auto &pose = game.m_pOutdoorPartyRuntime->movementState();
                require(!pose.airborne && pose.supportKind != OutdoorSupportKind::None,
                    "Declared Breach54 door position lacks normal support");
                aim(center, pose.footZ + game.m_outdoorGameView.m_cameraEyeHeight);
            }
            for (auto &point : pointer.points) point = bx::add(center, bx::mul(bx::sub(point, center), 0.65f));
            pointer.points.insert(pointer.points.begin(), center);
            record("after-declared-door-pose-and-camera");
        };
        const auto clickDoor = [&](int width, int height)
        {
            const auto camera = world().buildWorldPickRequest({0, 0, width, height, false});
            float matrix[16];
            bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
            std::optional<std::array<float, 2>> pixel;
            for (const auto &point : pointer.points)
            {
                const auto projected = bx::mulH(point, matrix);
                const float x = (projected.x + 1) * width * 0.5f, y = (1 - projected.y) * height * 0.5f;
                if (x < 0 || y < 0 || x >= width || y >= height) continue;
                const auto hit = world().pickMouseInteractionTarget(
                    world().buildWorldPickRequest({x, y, width, height, true}));
                const bool accepted = hit.eventTarget
                    && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                    && hit.eventTarget->triggeredEventId == pointer.eventId
                    && (world().isIndoorMap() ? hit.eventTarget->targetIndex == pointer.face
                        : hit.eventTarget->targetIndex == pointer.model
                            && hit.eventTarget->secondaryIndex == pointer.face)
                    && world().canActivateWorldHit(hit, GameplayInteractionMethod::Mouse);
                trace << "{\"event\":\"typed-pointer-probe\",\"frame\":" << frames << ",\"source_event\":"
                    << pointer.eventId << ",\"pixel\":[" << x << ',' << y << "],\"accepted\":" << accepted
                    << ",\"hit_kind\":" << int(hit.kind) << ",\"hit_event\":"
                    << (hit.eventTarget ? hit.eventTarget->triggeredEventId : 0) << "}\n";
                if (accepted) { pixel = std::array<float, 2>{x, y}; break; }
            }
            require(pixel.has_value(), "No visible activatable sample on the actual source door");
            GameplayInputFrame frame;
            frame.screenWidth = width;
            frame.screenHeight = height;
            frame.pointerX = (*pixel)[0];
            frame.pointerY = (*pixel)[1];
            frame.rightMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(frame, 0);
            frame.rightMouseButton.pressed = false;
            frame.leftMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(frame, 0);
            frame.leftMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(frame, 0);
            frame.leftMouseButton = {};
            frame.rightMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(frame, 0);
            if (!restart)
                require(events().pendingBreachMazeTransitionRequest.has_value(),
                    "Actual source surface input did not queue its typed maze request");
            ++pointerClicks;
            record(restart ? "actual-source402-pointer-clicked" : "actual-source-pointer-queued");
        };
        const auto telelocatorVisibleFaces = [&]()
        {
            const auto *pMap = game.m_pOutdoorWorldRuntime->mapData();
            require(pMap != nullptr, "No actual Telelocator geometry");
            const auto snapshot = game.m_pOutdoorWorldRuntime->snapshot();
            size_t flat = 0;
            unsigned total = 0, visible = 0;
            for (const auto &model : pMap->bmodels)
                for (const auto &face : model.faces)
                {
                    const uint32_t attributes = snapshot.faceAttributes.at(flat++);
                    if (face.cogNumber != 400) continue;
                    ++total;
                    if (!hasFaceAttribute(attributes, FaceAttribute::Invisible)) ++visible;
                }
            require(total == 40, "Authentic Breach Telelocator must have exactly 40 cog400 faces");
            return visible;
        };
        const auto preparePedestal = [&]()
        {
            record("before-declared-pedestal402-pose");
            const auto *pMap = game.m_pOutdoorWorldRuntime->mapData();
            require(pMap && pMap->bmodels.size() > 130, "No authentic Breach models");
            const auto &model = pMap->bmodels.at(93);
            const auto &face = model.faces.at(2);
            require(face.cogTriggeredNumber == 402 && hasFaceAttribute(face.attributes, FaceAttribute::Clickable)
                && face.planeNormalX > 0 && !face.vertexIndices.empty(), "Original pedestal402 geometry differs");
            pointer = {};
            pointer.model = 93;
            pointer.face = 2;
            pointer.eventId = 402;
            bx::Vec3 center{0, 0, 0};
            float bottom = std::numeric_limits<float>::max();
            for (uint16_t index : face.vertexIndices)
            {
                const auto &vertex = model.vertices.at(index);
                const bx::Vec3 point{float(vertex.x), float(vertex.y), float(vertex.z)};
                pointer.points.push_back(point);
                center = bx::add(center, point);
                bottom = std::min(bottom, point.z);
            }
            center = bx::mul(center, 1.0f / float(pointer.points.size()));
            game.m_pOutdoorPartyRuntime->teleportTo(center.x + 256, center.y, bottom + 1);
            const auto &pose = game.m_pOutdoorPartyRuntime->movementState();
            require(!pose.airborne && pose.supportKind != OutdoorSupportKind::None,
                "Declared pedestal402 position lacks native floor support");
            aim(center, pose.footZ + game.m_outdoorGameView.m_cameraEyeHeight);
            for (auto &point : pointer.points) point = bx::add(center, bx::mul(bx::sub(point, center), 0.65f));
            pointer.points.insert(pointer.points.begin(), center);
            record("after-declared-pedestal402-pose-and-camera");
        };
        const auto saveRestart = [&](const std::string &label)
        {
            const auto path = output / (label + ".oysav");
            require(path != input && !std::filesystem::exists(path), "Restart save must use a fresh path");
            require(game.quickSaveToPath(path, "155重新开始 " + label), "Explicit restart save failed");
            const auto saved = loadGameDataFromPath(path, error);
            require(saved && toLowerCopy(saved->mapFileName) == targetMap, error);
            require(encodePartySnapshotForComparison(saved->party)
                    == encodePartySnapshotForComparison(screen().party()->snapshot()),
                "Explicit restart save changed the complete live Party record");
            const auto &selected = game.m_gameDataLoader.getSelectedMap();
            require(selected && !selected->map.canonicalId.empty(), "Restart has no canonical map identity");
            const std::string &canonical = selected->map.canonicalId;
            const size_t expectedCacheKeys = canonical == saved->mapFileName ? 1 : 2;
            trace << "{\"event\":\"restart-save-cache-keys\",\"map\":" << std::quoted(saved->mapFileName)
                << ",\"canonical\":" << std::quoted(canonical) << ",\"keys\":[";
            bool comma = false;
            for (const auto &[key, snapshot] : saved->outdoorWorldStates)
            {
                if (comma) trace << ',';
                comma = true;
                trace << std::quoted(key);
            }
            trace << "]}\n";
            trace.flush();
            require(saved->currentSceneKind == SceneKind::Outdoor && saved->hasOutdoorRuntimeState
                && !saved->hasIndoorSceneState && saved->indoorSceneStates.empty()
                && saved->outdoorWorldStates.size() == expectedCacheKeys
                && saved->outdoorWorldStates.contains(saved->mapFileName)
                && saved->outdoorWorldStates.contains(canonical)
                && !saved->breachMaze && !saved->breachAlvarCompanions && !saved->heldInventoryItemActive,
                "Restart save retained an old map cache, maze, companion receipt or held item");
            record("explicit-restart-saved:" + label);
        };
        const auto writeResult = [&](bool passed, const std::string &failure)
        {
            std::ofstream result(output / "result.json");
            result << std::boolalpha
                << "{\"status\":" << std::quoted(passed ? "passed" : "failed") << ",\"controlled\":true,"
                   "\"natural_campaign\":false,\"mode\":" << std::quoted(mode) << ",\"stage\":" << std::quoted(stage)
                << ",\"frames\":" << frames << ",\"simulation_seconds\":" << simulationSeconds
                << ",\"app_result\":" << appResult << ",\"captures\":" << captures
                << ",\"source_pointer_clicks\":" << pointerClicks << ",\"confirmations\":" << confirmations
                << ",\"first_contact_dismissed\":" << firstContactDismissed
                << ",\"message_verified\":" << messageVerified << ",\"close_delay_verified\":" << closedVerified
                << ",\"returned_to_breach\":" << returned << ",\"cold_verified\":" << coldVerified
                << ",\"normal_ai\":true,\"new_test_cases\":0,\"input_sha256\":" << std::quoted(inputHash)
                << ",\"error\":" << std::quoted(failure);
            if (restart)
            {
                result << ",\"restart_verified\":" << restartVerified
                    << ",\"source_random_target\":" << std::quoted(targetMap)
                    << ",\"source_rng_before\":" << restartRandomBefore
                    << ",\"source_rng_after_single_draw\":" << restartRandomAfter
                    << ",\"telelocator_visible_before\":" << telelocatorVisibleBefore
                    << ",\"telelocator_visible_after\":" << telelocatorVisibleAfter
                    << ",\"source_intro_movie\":" << std::quoted(restartIntroMovie)
                    << ",\"source_intro_sdl_escape\":" << restartIntroDismissed
                    << ",\"source_intro_resumed\":" << restartIntroResumed
                    << ",\"explicit_save_only\":true,\"retained_members\":[";
                for (size_t index = 0; index < retainedBeforeHashes.size(); ++index)
                {
                    if (index) result << ',';
                    result << "{\"index\":" << index << ",\"expected_sha256\":"
                        << std::quoted(retainedBeforeHashes[index]) << ",\"actual_sha256\":"
                        << std::quoted(retainedAfterHashes[index]) << '}';
                }
                result << ']';
            }
            result << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(input), "Cannot load input");
                    require(quest("QuestFinished") == 1 && quest("GotEndCard") == 1,
                        "Input must already have completed Breach and displayed its one-time endcard");
                    require(!screen().activeEventDialog().isActive, "Input contains an unfinished dialogue");
                    if (!cold && !restart)
                    {
                        events().namedGlobalVars[std::string(QuestPrefix) + "GotMainQuest"] = 1;
                        game.m_gameSession.setNamedGlobalVar(std::string(QuestPrefix) + "GotMainQuest", 1);
                        Character *pCaster = screen().party()->member(2);
                        require(pCaster && pCaster->health > 0 && pCaster->spellPoints >= 20,
                            "Original caster lacks its unmodified health/mana for one paid Town Portal");
                        if (!pCaster->knowsSpell(31)) require(pCaster->learnSpell(31), "Cannot declare learned31");
                        const auto *pWater = pCaster->findSkill("WaterMagic");
                        if (!pWater || pWater->level < 10 || pWater->mastery != SkillMastery::Grandmaster)
                            pCaster->skills["WaterMagic"] = {"WaterMagic", 10, SkillMastery::Grandmaster};
                    }
                    beforeParty = screen().party()->snapshot();
                    checkCustody(source->party, beforeParty);
                    started = SDL_GetTicks();
                    if (restart)
                    {
                        require(!screen().heldInventoryItem().active
                            && pendingDeliveryCount(beforeParty) == 0
                            && inventoryCount(beforeParty, 666) == 0,
                            "Restart fixture requires empty hands/deliveries and no existing Telelocator666");
                        const auto installed = events().namedMapVars.find("MMerge.Breach.TelelocatorSet");
                        require(installed == events().namedMapVars.end() || installed->second == 0,
                            "Input already has the source Telelocator installed");
                        telelocatorVisibleBefore = telelocatorVisibleFaces();
                        require(telelocatorVisibleBefore == 0, "Uninstalled source Telelocator is already visible");
                        const ItemDefinition *pItem = screen().itemTable()->get(666);
                        require(pItem != nullptr, "Source Telelocator666 item definition is missing");
                        InventoryItem fixture;
                        fixture.objectDescriptionId = 666;
                        fixture.width = pItem->inventoryWidth;
                        fixture.height = pItem->inventoryHeight;
                        fixture.rarity = pItem->rarity;
                        require(screen().party()->member(0)->addInventoryItem(fixture)
                            && inventoryCount(screen().party()->snapshot(), 666) == 1,
                            "Cannot declare exactly one Telelocator666 without replacing any carried item");
                        record("declared-single-source666-inventory-fixture");
                    }
                    else if (cold)
                    {
                        require(game.m_gameSession.breachMazeRunData().has_value(), "Cold lost the committed maze");
                        // AfterLoad's pre-apply and final synchronization each capture once;
                        // quickLoad's final apply captures the third snapshot.
                        constexpr uint64_t ColdLoadCaptureCount = 3;
                        const auto &savedRun = *source->breachMaze;
                        const auto &loadedRun = *game.m_gameSession.breachMazeRunData();
                        trace << "{\"event\":\"cold-run-identity-comparison\",\"expected_revision_increase\":"
                            << ColdLoadCaptureCount << ",\"expected_syncs\":[\"AfterLoad pre-apply\","
                               "\"AfterLoad final synchronization\",\"quickLoad final apply\"],\"saved\":";
                        writeMazeRunIdentity(trace, savedRun);
                        trace << ",\"live\":";
                        writeMazeRunIdentity(trace, loadedRun);
                        trace << "}\n";
                        trace.flush();
                        compareFloors(savedRun, loadedRun, ColdLoadCaptureCount);
                        // buildSaveData deliberately clears generic indoorScene for an active maze.
                        const auto &old = savedRun.floors.at(savedRun.currentLevel).runtime.scene.partyRuntime;
                        const auto now = indoor().partyRuntime().snapshot();
                        require(old.jumpState == now.jumpState
                            && old.movementAccumulatorSeconds == now.movementAccumulatorSeconds
                            && std::tie(old.movementState.x, old.movementState.y, old.movementState.footZ,
                                old.movementState.verticalVelocity, old.movementState.grounded)
                                == std::tie(now.movementState.x, now.movementState.y, now.movementState.footZ,
                                    now.movementState.verticalVelocity, now.movementState.grounded),
                            "Cold changed active Party pose/motion");
                        coldVerified = true;
                        save("restored");
                        capture("l4-cold-restored");
                        stage = "cold-before-normal-continuation";
                    }
                    else save("declared-fixture");
                    record("initialized");
                    return true;
                },
                [&](const SDL_Event &event)
                {
                    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                        && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                        && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                        game.handleSdlEvent(event);
                },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 600 && SDL_GetTicks() - started < 120000 && frames - phaseFrame < 240,
                        "Continuation exceeded its bounded frame/wall guard at " + stage);
                    const float dt = captureLabel.empty() && phase != Phase::Done && phase != Phase::RestartCommit
                        ? FrameSeconds : 0.0f;
                    game.renderFrame(width, height, wheel, dt);
                    simulationSeconds += dt;
                    require(simulationSeconds <= 10.01, "Continuation exceeded ten normal simulation seconds");
                    if (!captureLabel.empty())
                    {
                        if (!captureRequested && frames >= captureFrame)
                        {
                            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureLabel.c_str());
                            captureRequested = true;
                        }
                        if (captureRequested)
                            if (auto image = Engine::BgfxContext::consumeScreenshot(captureLabel))
                            {
                                SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                    SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                                require(pSurface != nullptr, "Cannot construct screenshot");
                                const bool saved = SDL_SaveBMP(pSurface, (output / (captureLabel + ".bmp")).c_str());
                                SDL_DestroySurface(pSurface);
                                require(saved, "Cannot write screenshot");
                                record("captured:" + captureLabel);
                                captureLabel.clear();
                                ++captures;
                            }
                        return;
                    }
                    if (!restart) verifyParty();
                    if (restart && phase == Phase::RestartContinue
                        && !restartIntroMovie.empty() && !restartIntroResumed)
                    {
                        if (IScreen *pActiveScreen = game.m_screenManager.activeScreen())
                        {
                            require(dynamic_cast<CutsceneVideoScreen *>(pActiveScreen) != nullptr,
                                "The original fresh-campaign intro opened an unexpected screen");
                            if (!restartIntroDismissed)
                            {
                                SDL_Event escape{};
                                escape.type = SDL_EVENT_KEY_DOWN;
                                escape.key.key = SDLK_ESCAPE;
                                escape.key.scancode = SDL_SCANCODE_ESCAPE;
                                escape.key.down = true;
                                game.handleSdlEvent(escape);
                                escape.type = SDL_EVENT_KEY_UP;
                                escape.key.down = false;
                                game.handleSdlEvent(escape);
                                restartIntroDismissed = true;
                                record("original-mm7-intro-ordinary-sdl-escape");
                            }
                            return;
                        }
                        require(restartIntroDismissed && !events().pendingMovie
                            && !game.m_gameAudioSystem.isBackgroundMusicPaused(),
                            "Original MM7 intro did not resume gameplay and background music after Escape");
                        restartIntroResumed = true;
                        restartContinueAt = simulationSeconds;
                        record("original-mm7-intro-closed-by-normal-app-frame");
                        return;
                    }
                    require(game.m_screenManager.activeScreen() == nullptr,
                        "Normal combat or another screen interrupted the bounded flow; no tactical retry");
                    if (restart)
                    {
                        if (phase == Phase::RestartPedestal)
                        {
                            preparePedestal();
                            advance(Phase::RestartInstall, "source402-pointer-ready");
                        }
                        else if (phase == Phase::RestartInstall)
                        {
                            require(inventoryCount(screen().party()->snapshot(), 666) == 1,
                                "Telelocator fixture changed before the actual source402 click");
                            clickDoor(width, height);
                            require(events().namedMapVars.contains("MMerge.Breach.TelelocatorSet")
                                && events().namedMapVars.at("MMerge.Breach.TelelocatorSet") == 1
                                && inventoryCount(screen().party()->snapshot(), 666) == 0,
                                "Actual source402 did not consume its single item and install the Telelocator");
                            checkCustody(beforeParty, screen().party()->snapshot());
                            telelocatorVisibleAfter = telelocatorVisibleFaces();
                            require(telelocatorVisibleAfter == 40, "Source402 did not reveal all 40 Telelocator faces");
                            trace << "{\"event\":\"source402-installation\",\"consumed_item\":666,\"quantity\":1,"
                                "\"visible_faces_before\":0,\"visible_faces_after\":40}\n";
                            record("before-declared-source81-airborne-pose");
                            const auto *pMap = game.m_pOutdoorWorldRuntime->mapData();
                            const auto &face = pMap->bmodels.at(130).faces.at(9);
                            require(face.cogTriggeredNumber == 81
                                && hasFaceAttribute(face.attributes, FaceAttribute::PressurePlate),
                                "Original central tower floor is not source81");
                            // This fixture is above the real triangle at z4640; only native landing enters it.
                            game.m_pOutdoorPartyRuntime->teleportTo(0, 0, 4705);
                            require(game.m_pOutdoorPartyRuntime->movementState().airborne,
                                "Source81 pose fixture must start airborne");
                            advance(Phase::RestartLand, "source81-awaiting-native-gravity-landing");
                        }
                        else if (phase == Phase::RestartLand && events().pendingInputPrompt)
                        {
                            const auto &trigger = events().lastPressurePlateTrigger;
                            const auto prompt = *events().pendingInputPrompt;
                            require(trigger && trigger->world == "outdoor" && trigger->eventId == 81
                                && trigger->bmodelIndex == 130 && trigger->faceIndex == 9
                                && !game.m_pOutdoorPartyRuntime->movementState().airborne
                                && prompt.eventId == 81 && prompt.continueStep == 1 && !prompt.trimWhitespace
                                && prompt.answers == std::vector<std::string>{"y"}
                                && prompt.answerContinueSteps == std::vector<uint8_t>{2},
                                "Native source81 landing did not present the original restart question");
                            require(!screen().heldInventoryItem().active
                                && pendingDeliveryCount(screen().party()->snapshot()) == 0,
                                "Restart confirmation requires empty held item and pending deliveries");
                            const auto beforePath = output / "restart-before-confirmation.oysav";
                            require(!std::filesystem::exists(beforePath)
                                && game.quickSaveToPath(beforePath, "155重新开始 确认前"),
                                "Cannot preserve the exact pre-confirmation record at a new save path");
                            restartBefore = screen().party()->snapshot();
                            restartRandomBefore = events().eventRandomState;
                            const auto variable = [&](EvtVariable id, int fallback)
                            {
                                const auto found = events().variables.find(uint32_t(id));
                                return found == events().variables.end() ? fallback : found->second;
                            };
                            const int minutes = ((std::max(1, variable(EvtVariable::DayOfYear, 1)) - 1) * 24
                                + std::max(0, variable(EvtVariable::Hour, 0))) * 60;
                            const uint32_t seed = restartRandomBefore ? restartRandomBefore
                                : 81u * 2654435761u ^ uint32_t(std::max(0, minutes));
                            const uint32_t expectedRandom = seed * 1664525u + 1013904223u;
                            record("source81-exact-before-sdl-confirmation");
                            SDL_Event text{};
                            text.type = SDL_EVENT_TEXT_INPUT;
                            text.text.text = "y";
                            game.handleSdlEvent(text);
                            SDL_Event key{};
                            key.type = SDL_EVENT_KEY_DOWN;
                            key.key.key = SDLK_RETURN;
                            key.key.scancode = SDL_SCANCODE_RETURN;
                            key.key.down = true;
                            game.handleSdlEvent(key);
                            key.type = SDL_EVENT_KEY_UP;
                            key.key.down = false;
                            game.handleSdlEvent(key);
                            require(!events().pendingInputPrompt && events().pendingCampaignRestartMap,
                                "SDL y/Return did not queue the source restart request");
                            restartRandomAfter = events().eventRandomState;
                            targetMap = toLowerCopy(*events().pendingCampaignRestartMap);
                            const std::array<std::string, 3> starts{"out01.odm", "7out01.odm", "oute3.odm"};
                            require(restartRandomAfter == expectedRandom && targetMap == starts[expectedRandom % 3],
                                "Source81 restart must draw exactly one of its original three start maps");
                            ++confirmations;
                            advance(Phase::RestartCommit, "source81-request-awaiting-normal-app-safe-point");
                        }
                        else if (phase == Phase::RestartCommit)
                        {
                            require(toLowerCopy(game.m_gameSession.currentMapFileName()) == targetMap
                                && !world().isIndoorMap() && !events().pendingCampaignRestartMap
                                && !game.m_gameSession.currentSavePath() && world().gameMinutes() == 540.0,
                                "Normal App safe point did not commit a fresh source start at day1 09:00");
                            const std::string expectedWorld = targetMap == "out01.odm" ? "mm8"
                                : targetMap == "7out01.odm" ? "mm7" : "mm6";
                            require(game.m_activeWorldManifest.id == expectedWorld,
                                "Source restart did not mount the ordinary destination world");
                            restartAfter = screen().party()->snapshot();
                            // Preserve a failed retention comparison for independent SaveInspect diagnosis too.
                            saveRestart("restart-complete");
                            require(restartAfter.members.size() == restartBefore.members.size()
                                && restartAfter.nextMemberId == restartBefore.nextMemberId,
                                "Restart changed the original active members or stable-ID allocator");
                            bool retained = true;
                            for (size_t index = 0; index < restartBefore.members.size(); ++index)
                            {
                                const auto expected = retainedMemberBytes(restartBefore, index, true);
                                const auto actual = retainedMemberBytes(restartAfter, index, false);
                                retainedBeforeHashes[index] = hashBytes(expected);
                                retainedAfterHashes[index] = hashBytes(actual);
                                const bool same = expected == actual && restartAfter.members[index].awards.empty();
                                retained = retained && same;
                                trace << "{\"event\":\"restart-complete-member-record\",\"index\":" << index
                                    << ",\"member_id\":" << restartAfter.members[index].memberId
                                    << ",\"awards_before\":" << restartBefore.members[index].awards.size()
                                    << ",\"awards_after\":" << restartAfter.members[index].awards.size()
                                    << ",\"active_personal_buffs_before\":"
                                    << std::count_if(restartBefore.characterBuffs[index].begin(),
                                        restartBefore.characterBuffs[index].end(),
                                        [](const auto &buff) { return buff.remainingSeconds > 0; })
                                    << ",\"expected_sha256\":" << std::quoted(retainedBeforeHashes[index])
                                    << ",\"actual_sha256\":" << std::quoted(retainedAfterHashes[index])
                                    << ",\"equal\":" << same << ",\"transaction_simulation_seconds\":0}\n";
                            }
                            require(retained,
                                "Restart changed a complete retained member/buff record beyond Awards/FX");
                            require(restartAfter.gold == 200 && restartAfter.food == 5 && restartAfter.bankGold == 0
                                && restartAfter.fineGold == 0 && restartAfter.adventurersInnMembers.empty()
                                && restartAfter.detachedMembers.empty()
                                && restartAfter.completedDetachmentOwners.empty()
                                && restartAfter.hiredNpcFollowers.empty() && restartAfter.pendingItemDeliveries.empty()
                                && !screen().heldInventoryItem().active && quest("QuestFinished") == 0
                                && quest("GotEndCard") == 0 && quest("CoughtChaos") == 0,
                                "Restart retained old campaign resources, custody or completion state");
                            Party::Snapshot partyBuffs;
                            partyBuffs.partyBuffs = restartAfter.partyBuffs;
                            require(encodePartySnapshotForComparison(partyBuffs)
                                == encodePartySnapshotForComparison(Party::Snapshot{}),
                                "Restart retained old party-wide buffs outside source player records");
                            for (const auto &[continent, history] : events().historyEventTimesByContinent)
                                for (const auto &[id, time] : history)
                                    require(time <= 540, "Restart retained a history timestamp from the old campaign");
                            trace << "{\"event\":\"fresh-source-onload-state\",\"world\":"
                                << std::quoted(game.m_activeWorldManifest.id) << ",\"quest_bits\":[";
                            bool comma = false;
                            for (uint32_t bit : restartAfter.questBits)
                            {
                                if (comma) trace << ',';
                                comma = true;
                                trace << bit;
                            }
                            trace << "],\"named_globals\":{";
                            comma = false;
                            for (const auto &[name, value] : events().namedGlobalVars)
                            {
                                if (comma) trace << ',';
                                comma = true;
                                trace << std::quoted(name) << ':' << value;
                            }
                            trace << "},\"history\":[";
                            comma = false;
                            for (const auto &[continent, history] : events().historyEventTimesByContinent)
                                for (const auto &[id, time] : history)
                                {
                                    if (comma) trace << ',';
                                    comma = true;
                                    trace << "{\"continent\":" << continent << ",\"event\":" << id
                                        << ",\"time\":" << time << '}';
                                }
                            trace << "]}\n";
                            if (events().pendingMovie)
                            {
                                restartIntroMovie = events().pendingMovie->movieName;
                                require(targetMap == "7out01.odm" && restartIntroMovie == "\"intro post\""
                                    && events().pendingMovie->restoreAfterPlayback,
                                    "Unexpected original movie queued by the fresh destination OnLoad");
                                trace << "{\"event\":\"source-onload-queued-intro\",\"name\":"
                                    << std::quoted(restartIntroMovie) << "}\n";
                            }
                            require(targetMap != "7out01.odm" || !restartIntroMovie.empty(),
                                "Fresh MM7 OnLoad did not queue its original introduction");
                            restartVerified = true;
                            restartContinueAt = simulationSeconds;
                            advance(Phase::RestartContinue, "fresh-campaign-before-short-normal-continuation");
                        }
                        else if (phase == Phase::RestartContinue && simulationSeconds - restartContinueAt >= 0.5)
                        {
                            checkCustody(restartAfter, screen().party()->snapshot());
                            saveRestart("continued");
                            capture("restart-continued");
                            advance(Phase::Done, "restart-and-normal-continuation-complete");
                        }
                        else if (phase == Phase::Done)
                        {
                            require(restartVerified && captures == 1 && pointerClicks == 1 && confirmations == 1
                                && fileHash(input) == inputHash, "Restart boundary incomplete or input changed");
                            complete = true;
                            record("complete");
                            SDL_Event quit{};
                            quit.type = SDL_EVENT_QUIT;
                            SDL_PushEvent(&quit);
                        }
                        return;
                    }
                    if (phase == Phase::Cast && frames >= 3)
                    {
                        require(!screen().activeEventDialog().isActive && castAttempts < 2
                            && world().townPortalUsesDimensionDoor(), "Source Breach TownPortal is not ready");
                        manaBefore = screen().party()->member(2)->spellPoints;
                        const auto *pSpell = screen().spellTable()->findById(31);
                        require(pSpell != nullptr, "No original TownPortal31");
                        const bool cast = screen().tryCastSpellFromMember(2, 31, pSpell->name);
                        ++castAttempts;
                        if (screen().activeEventDialog().isActive)
                        {
                            require(!firstContactDismissed && screen().activeEventDialog().sourceId == 803
                                && screen().party()->member(2)->spellPoints == manaBefore,
                                "Unexpected native cast/dialogue interruption");
                            GameplayInputFrame escape;
                            escape.screenWidth = width;
                            escape.screenHeight = height;
                            escape.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                            escape.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                            game.m_gameSession.updateGameplay(escape, 0);
                            game.m_gameSession.updateGameplay({}, 0);
                            firstContactDismissed = true;
                            record("ordinary-first-contact-dismissed-without-quest-write");
                        }
                        else
                        {
                            const auto &overlay = screen().utilitySpellOverlayReadOnly();
                            require(cast && overlay.active
                                && overlay.mode == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor
                                && overlay.spendMana && overlay.applyRecovery && !overlay.bypassRequiredMastery
                                && !overlay.bypassGameplayCasterValidation && !overlay.bypassTownPortalFailureChecks,
                                "Real source TownPortal did not open the paid native DimensionDoor UI");
                            advance(Phase::Portal, "paid-dimension-door-overlay");
                        }
                    }
                    else if (phase == Phase::Portal)
                    {
                        require(screen().ensureTownPortalDestinationsLoaded(), "No normal DimensionDoor destinations");
                        const auto &destinations = screen().townPortalDestinations();
                        const auto found = std::find_if(destinations.begin(), destinations.end(),
                            [](const auto &destination) { return destination.id == "TPGlobal:0"; });
                        require(found != destinations.end(), "No real TPGlobal:0 destination");
                        targetMap = found->mapName;
                        const auto *pLayout = screen().findHudLayoutElement("TownPortalBackground");
                        require(pLayout != nullptr, "No normal portal background");
                        const auto rect = screen().resolveHudLayoutElement(
                            "TownPortalBackground", width, height, pLayout->width, pLayout->height);
                        require(rect.has_value(), "Cannot resolve portal UI");
                        GameplayInputFrame click;
                        click.screenWidth = width;
                        click.screenHeight = height;
                        click.pointerX = rect->x + 37.0f * rect->width / 640;
                        click.pointerY = rect->y + 100.0f * rect->width / 640;
                        const auto hit = screen().resolveTownPortalDestinationAtPoint(
                            width, height, click.pointerX, click.pointerY);
                        require(hit && destinations[*hit].id == found->id, "Real portal UI missed its target");
                        const int mana = screen().party()->member(2)->spellPoints;
                        click.leftMouseButton = {true, true, false};
                        game.m_gameSession.updateGameplay(click, 0);
                        click.leftMouseButton = {false, false, true};
                        game.m_gameSession.updateGameplay(click, 0);
                        require(events().pendingMapMove && events().pendingMapMove->mapName == targetMap
                            && screen().party()->member(2)->spellPoints == mana - 20
                            && screen().party()->member(2)->recoverySecondsRemaining > 0,
                            "Ordinary DimensionDoor confirmation lost its normal cost/map request");
                        advance(Phase::NormalWorld, "ordinary-departure-map-requested");
                    }
                    else if (phase == Phase::NormalWorld
                        && toLowerCopy(game.m_gameSession.currentMapFileName()) == toLowerCopy(targetMap))
                    {
                        require(!world().isIndoorMap() && !world().townPortalUsesDimensionDoor(),
                            "Portal did not arrive in an ordinary world");
                        save("normal-world");
                        openVerdant();
                        advance(Phase::Hints, "ordinary-verdant-root");
                    }
                    else if (phase == Phase::Hints)
                    {
                        choose(EventDialogActionKind::NpcTopic, 732);
                        const auto *pMap = game.m_gameSession.data().mapStats().findByFileName("Breach.odm");
                        const auto &dialog = screen().activeEventDialog();
                        const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(), [](const auto &a)
                            { return a.kind == EventDialogActionKind::NpcTopic && a.id == BreachTravelTopic; });
                        const auto topic = screen().npcDialogTable()->getTopicById(BreachTravelTopic);
                        trace << "{\"event\":\"hints-source-diagnostic\",\"branch\":"
                            << std::quoted(events().dialogueState.scriptedBranch)
                            << ",\"branch_npc\":" << events().dialogueState.scriptedBranchNpcId
                            << ",\"source_map_label\":" << std::quoted(pMap ? pMap->name : "MISSING")
                            << ",\"source_topic_label\":" << std::quoted(topic ? topic->topic : "MISSING")
                            << ",\"actions\":[";
                        bool comma = false;
                        for (const auto &action : dialog.actions)
                        {
                            if (comma) trace << ',';
                            comma = true;
                            trace << "{\"id\":" << action.id << ",\"kind\":" << int(action.kind)
                                << ",\"label\":" << std::quoted(action.label) << '}';
                        }
                        trace << "]}\n";
                        trace.flush();
                        require(pMap && found != dialog.actions.end() && found->label == pMap->name,
                            "Hints slot3 does not display the source MapStats204 name");
                        advance(Phase::Travel, "source-hints-breach-topic-visible");
                    }
                    else if (phase == Phase::Travel)
                    {
                        const auto sourceMessage = screen().npcDialogTable()->getTopicById(186);
                        require(sourceMessage && !sourceMessage->topic.empty(), "No original NPCTopic186 message");
                        const Party::Snapshot previous = screen().party()->snapshot();
                        topicStarted = SDL_GetTicks();
                        messageStarted = topicStarted;
                        choose(EventDialogActionKind::NpcTopic, BreachTravelTopic);
                        require(events().pendingDelayedMapMove
                            && events().pendingDelayedMapMove->messageDelayMilliseconds == 1024
                            && events().pendingDelayedMapMove->afterCloseDelayMilliseconds == 100
                            && events().pendingDelayedMapMove->move.mapName
                            && toLowerCopy(*events().pendingDelayedMapMove->move.mapName) == "breach.odm"
                            && events().pendingDelayedMapMove->move.x == 0
                            && events().pendingDelayedMapMove->move.y == 0
                            && events().pendingDelayedMapMove->move.z == 0, "Wrong source delayed ordinary map move");
                        require(std::any_of(screen().activeEventDialog().lines.begin(),
                            screen().activeEventDialog().lines.end(), [&](const auto &line)
                                { return line.find(sourceMessage->topic) != std::string::npos; }),
                            "Actual dialogue did not display source NPCTopic186");
                        for (size_t index = 0; index < previous.members.size(); ++index)
                            require(previous.members[index].spellPoints == screen().party()->member(index)->spellPoints
                                && previous.members[index].recoverySecondsRemaining
                                    == screen().party()->member(index)->recoverySecondsRemaining,
                                "The free source BreachTravel topic spent mana/recovery");
                        messageVerified = true;
                        advance(Phase::Delay, "source-message-awaiting-real-clock-delay");
                        capture("source-finally-message");
                    }
                    else if (phase == Phase::Delay)
                    {
                        if (events().pendingDelayedMapMove)
                        {
                            const auto &pending = *events().pendingDelayedMapMove;
                            if (!pending.dialogClosed)
                            {
                                require(screen().activeEventDialog().isActive,
                                    "Source message closed before its delay");
                                if (pending.phaseStartedMilliseconds)
                                    messageStarted = *pending.phaseStartedMilliseconds;
                            }
                            else if (!closedAt)
                            {
                                closedAt = pending.phaseStartedMilliseconds.value_or(0);
                                require(closedAt && messageStarted && closedAt - messageStarted >= 1024
                                    && !screen().activeEventDialog().isActive,
                                    "Real message delay/close was not preserved");
                                record("message-closed-after-real1024ms");
                            }
                        }
                        else if (toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm")
                        {
                            require(closedAt && SDL_GetTicks() - closedAt >= 100
                                && SDL_GetTicks() - topicStarted >= 1124, "Post-close real100ms delay was skipped");
                            closedVerified = returned = true;
                            for (uint32_t id = 101; id <= 128; ++id)
                                require(events().outdoorModelMechanisms.contains(id),
                                    "A source Breach mechanism is absent");
                            require(events().namedMapVars.at("MMerge.Breach.DayState") == 3
                                && events().outdoorLightTimeOverride
                                && events().outdoorLightTimeOverride->hour == 11
                                && !events().outdoorLightTimeOverride->minute && world().townPortalUsesDimensionDoor(),
                                "Returned Breach lost source daylight3 or typed travel restrictions");
                            save("breach-returned");
                            advance(Phase::Door, "returned-breach-source28-daylight3");
                        }
                    }
                    else if (phase == Phase::Door)
                    {
                        prepareDoor();
                        advance(Phase::Pointer, "source-door-pointer-ready");
                    }
                    else if (phase == Phase::Pointer)
                    {
                        clickDoor(width, height);
                        advance(Phase::Offer, "source-maze-request-awaiting-normal-confirmation");
                    }
                    else if (phase == Phase::Offer && screen().activeEventDialog().isActive)
                    {
                        require(screen().activeEventDialog().presentation == EventDialogPresentation::Transition,
                            "Source door did not present the ordinary map confirmation");
                        choose(EventDialogActionKind::MapTransitionConfirm);
                        ++confirmations;
                        advance(Phase::Floor, "ordinary-maze-confirmation-selected");
                    }
                    else if (phase == Phase::Floor && world().isIndoorMap()
                        && game.m_gameSession.breachMazeRunData()
                        && game.m_gameSession.breachMazeRunData()->currentLevel == expectedLevel)
                    {
                        const auto &run = *game.m_gameSession.breachMazeRunData();
                        require(run.inside && run.floors.contains(expectedLevel)
                            && run.floors.at(expectedLevel).population
                            && indoor().worldRuntime().hasNativeEventHandler(),
                            "Normal confirmed floor did not install its population and native interactions");
                        record("actual-floor-arrived:" + std::to_string(expectedLevel));
                        if (expectedLevel == 4)
                        {
                            require(pointerClicks == 4 && confirmations == 4,
                                "L4 was not reached through all four actual surface/confirmation pairs");
                            save("l4-complete");
                            capture("l4-complete");
                            advance(Phase::Continue, "l4-before-short-normal-continuation");
                        }
                        else
                        {
                            ++expectedLevel;
                            advance(Phase::Door, "next-generated-floor-down-door");
                        }
                    }
                    else if (phase == Phase::Continue && frames - phaseFrame >= 30)
                    {
                        save("continued");
                        if (cold) capture("l4-cold-continued");
                        advance(Phase::Done, "continuation-complete");
                    }
                    else if (phase == Phase::Done)
                    {
                        require(fileHash(input) == inputHash && (cold ? coldVerified : returned && closedVerified),
                            "Input changed or the actual continuation boundary was not completed");
                        complete = true;
                        record("complete");
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        SDL_PushEvent(&quit);
                    }
                },
                [&]() { game.shutdownApplication(); });
            appResult = app.run();
            require(appResult == 0 && complete, "Continuation interrupted before completion");
            writeResult(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            try { if (game.m_pMapSceneRuntime) record("failure-live-state"); }
            catch (const std::exception &) { }
            trace << "{\"event\":\"failure\",\"stage\":" << std::quoted(stage)
                << ",\"error\":" << std::quoted(failure.what()) << "}\n";
            trace.flush();
            writeResult(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "breach_continuation_capture RUNTIME READONLY_SAVE NEW_OUTPUT actual|cold|restart\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Breach continuation capture: " << failure.what() << '\n';
        return 1;
    }
}
