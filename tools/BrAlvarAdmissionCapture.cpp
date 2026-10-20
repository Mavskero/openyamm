#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/StringUtils.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/MapDeltaData.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <CommonCrypto/CommonDigest.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr float FrameSeconds = 1.0f / 60.0f;
constexpr const char *FinalQuest = "MMerge.CrossContinents.GotFinalQuest";
constexpr const char *QuestFinished = "MMerge.CrossContinents.QuestFinished";
constexpr const char *JourneyVersion = "MMerge.BrAlvar.SourceJourneyVersion";

void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

std::string digest(const std::vector<uint8_t> &bytes)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash{};
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), hash.data());
    std::ostringstream result;
    for (unsigned char byte : hash) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string fileDigest(const std::filesystem::path &path)
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

std::map<uint64_t, Character> ownedMembers(const Party::Snapshot &party)
{
    std::map<uint64_t, Character> members;
    const auto append = [&](const Character &member)
    {
        require(member.memberId != 0 && members.emplace(member.memberId, member).second,
            "Missing or duplicate owned character identity");
    };
    for (const Character &member : party.members) append(member);
    for (const auto &member : party.detachedMembers) append(member.character);
    for (const auto &member : party.adventurersInnMembers) append(member.character);
    return members;
}

std::string itemsDigest(const Character &member)
{
    Character carrier;
    carrier.inventory = member.inventory;
    carrier.equipment = member.equipment;
    carrier.equipmentRuntime = member.equipmentRuntime;
    Party::Snapshot comparison;
    comparison.members.push_back(std::move(carrier));
    return digest(encodePartySnapshotForComparison(comparison));
}

void checkCustody(const Party::Snapshot &before, const Party::Snapshot &after)
{
    const auto first = ownedMembers(before);
    const auto last = ownedMembers(after);
    require(first.size() == last.size() && before.nextMemberId == after.nextMemberId
        && before.adventurersInnMembers.size() == after.adventurersInnMembers.size()
        && before.gold == after.gold && before.bankGold == after.bankGold
        && before.food == after.food && before.fineGold == after.fineGold,
        "Source admission changed owned identities, allocator, inn custody or resources");
    require(before.pendingItemDeliveries.empty() && after.pendingItemDeliveries.empty(),
        "This bounded input must retain its empty pending item queue");
    for (const auto &[id, member] : first)
    {
        const auto found = last.find(id);
        require(found != last.end(), "An original character disappeared");
        const Character &actual = found->second;
        require(std::tie(member.rosterId, member.characterDataId, member.name, member.biography, member.role,
            member.className, member.portraitTextureName, member.portraitPictureId, member.voiceId,
            member.birthYear, member.sexId, member.raceId, member.level, member.experience, member.skillPoints)
            == std::tie(actual.rosterId, actual.characterDataId, actual.name, actual.biography, actual.role,
                actual.className, actual.portraitTextureName, actual.portraitPictureId, actual.voiceId,
                actual.birthYear, actual.sexId, actual.raceId, actual.level, actual.experience, actual.skillPoints),
            "Original character identity, biography, level or XP changed");
        require(itemsDigest(member) == itemsDigest(actual), "Original inventory or complete equipment payload changed");
    }
    // Normal AI, recovery, regeneration and elapsed buff time remain enabled and are recorded below.
}

const ScriptedEventTimerState &journeyTimer(const OutdoorWorldRuntime::Snapshot &world)
{
    const auto found = std::find_if(world.timers.begin(), world.timers.end(), [](const ScriptedEventTimerState &timer)
    {
        return timer.definition.scope == ScriptedEventScope::Map && timer.definition.eventId == 65457;
    });
    require(found != world.timers.end() && found->definition.repeating
        && std::abs(found->definition.intervalGameMinutes - 1.0) < 0.00001,
        "Source65457 sixty-game-second timer is absent or has the wrong interval");
    return *found;
}

void checkJourneySnapshot(const OutdoorWorldRuntime::Snapshot &saved, const OutdoorWorldRuntime::Snapshot &live)
{
    require(saved.decorationFlags == live.decorationFlags && saved.eventRuntimeState && live.eventRuntimeState,
        "Saved/cold full indexed decoration flags differ");
    const auto &a = *saved.eventRuntimeState;
    const auto &b = *live.eventRuntimeState;
    require(a.namedMapVars.contains(JourneyVersion) && b.namedMapVars.contains(JourneyVersion)
        && a.namedMapVars.at(JourneyVersion) == 1 && b.namedMapVars.at(JourneyVersion) == 1
        && a.outdoorLightTimeOverride == b.outdoorLightTimeOverride,
        "Saved/cold source journey version or typed daylight differs");
    for (uint32_t eventId : {101u, 102u, 103u})
    {
        require(a.mechanisms.contains(eventId) && b.mechanisms.contains(eventId), "A Time mechanism disappeared");
        const RuntimeMechanismState &left = a.mechanisms.at(eventId);
        const RuntimeMechanismState &right = b.mechanisms.at(eventId);
        require(std::tie(left.state, left.timeSinceTriggeredMs, left.currentDistance,
            left.rotationDirection, left.isMoving) == std::tie(right.state, right.timeSinceTriggeredMs,
                right.currentDistance, right.rotationDirection, right.isMoving),
            "Saved/cold complete Time mechanism motion differs");
    }
    const auto &left = journeyTimer(saved);
    const auto &right = journeyTimer(live);
    require(sameScriptedEventTimerIdentity(left.definition, right.definition)
        && std::tie(left.nextAlarmGameMinutes, left.eligibleGameMinutes, left.hasFired,
            left.initialCalendarFirePending, left.migratedFromLegacySave, left.migratedRemainingGameMinutes)
            == std::tie(right.nextAlarmGameMinutes, right.eligibleGameMinutes, right.hasFired,
                right.initialCalendarFirePending, right.migratedFromLegacySave, right.migratedRemainingGameMinutes),
        "Saved/cold65457 persistent timer phase differs");
    // lastNativePollTime is deliberately transient and is re-established by ordinary native polling.
}
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, const std::string &mode)
    {
        const bool cold = mode == "cold";
        require(cold || mode == "actual", "Expected actual or cold mode");
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        require(std::filesystem::is_regular_file(root / "engine.zip")
            && std::filesystem::is_regular_file(root / "worlds/mmmerge.zip"), "Complete normal runtime required");
        require(!std::filesystem::exists(output)
            && std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Use a new output directory on NVMeSSD");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        const std::string inputHash = fileDigest(input);
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->hasOutdoorRuntimeState && source->currentSceneKind == SceneKind::Outdoor
            && toLowerCopy(source->mapFileName) == (cold ? "bralvar.odm" : "out02.odm"),
            "Wrong source outdoor input: " + error);
        require(!source->heldInventoryItemActive, "Use the original empty-held input");
        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"input\":" << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string()) << ",\"changes\":"
            << std::quoted(cold
                ? "Independent cold restore of the actual split save; no Party, position, camera, event, quest, "
                  "spell, RNG, HP/SP, buff or AI injection. Save restored state before short normal continuation."
                : "Declared upstream quest-state fixture only: GotFinalQuest=1 and Q1713=true. Declared caster "
                  "availability fixture: original member2 learns31 and WaterMagic becomes10 Grandmaster. "
                  "One supported position fixture at source out02(15103,-9759), with sampled terrain altitude. "
                  "Real tryCastSpellFromMember31 retains native learned/mastery/canCast checks; existing65491 "
                  "queues the map move and ordinary App OnLoad owns split. No HP/SP, items, XP, RNG, buff or "
                  "AI writes, direct event execution, direct map jump, retries, stairs or riddles.")
            << ",\"normal_ai\":true,\"timing\":\"Normal App frames use1/60; screenshot/save frames pause "
               "simulation. Bounds5 simulation seconds,180 frames,60 wall seconds after initialization.\","
               "\"visual_review_required\":true}\n";

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
        config.appName = "OpenYAMM normal source admission";
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
        std::vector<uint16_t> originalDecorationFlags;
        std::optional<BreachAlvarCompanionState> committed;
        std::string stage = "initialize";
        std::string failureDomain = "initialization";
        std::string captureLabel;
        unsigned frames = 0;
        unsigned captures = 0;
        unsigned captureFrame = 0;
        unsigned requestFrame = 0;
        bool captureRequested = false;
        bool complete = false;
        bool hookVerified = false;
        bool coldVerified = false;
        bool naturalVitalsChanged = false;
        int appResult = -1;
        double initialGameMinutes = 0.0;
        double simulationSeconds = 0.0;
        double continuationStartSeconds = 0.0;
        uint64_t readyTicks = 0;
        enum class Phase { Cast, Arrival, Continue, Done };
        Phase phase = cold ? Phase::Continue : Phase::Cast;

        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Admission left the expected outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto events = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto record = [&](const std::string &label)
        {
            const auto &world = scene().worldRuntime();
            const auto &pose = scene().partyRuntime().movementState();
            const Party::Snapshot party = scene().party().snapshot();
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"simulation_seconds\":" << simulationSeconds
                << ",\"game_minutes\":" << world.gameMinutes() << ",\"map\":"
                << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"xyz\":["
                << pose.x << ',' << pose.y << ',' << pose.footZ << "],\"support\":["
                << int(pose.supportKind) << ',' << pose.supportBModelIndex << ',' << pose.supportFaceIndex
                << "],\"active\":" << party.members.size() << ",\"detached\":" << party.detachedMembers.size()
                << ",\"q1713\":" << party.questBits.contains(1713)
                << ",\"q1714\":" << party.questBits.contains(1714)
                << ",\"q1715\":" << party.questBits.contains(1715)
                << ",\"got_final_quest\":" << game.m_gameSession.namedGlobalVar(FinalQuest)
                << ",\"quest_finished\":" << game.m_gameSession.namedGlobalVar(QuestFinished)
                << ",\"entered_breach\":" << game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBreach")
                << ",\"allows_lloyd\":" << world.allowsLloydsBeacon()
                << ",\"town_portal_dimension_route\":" << world.townPortalUsesDimensionDoor()
                << ",\"timer_count\":" << world.snapshot().timers.size()
                << ",\"event_rng\":" << events().eventRandomState
                << ",\"pending_split_request\":" << events().pendingBreachAlvarCompanionRequest.has_value()
                << ",\"pending_map_move\":" << events().pendingMapMove.has_value() << ",\"receipt\":";
            if (game.m_gameSession.breachAlvarCompanions())
            {
                const auto &state = *game.m_gameSession.breachAlvarCompanions();
                trace << "{\"leader_id\":" << state.receipt.leaderMemberId
                    << ",\"seed\":" << state.receipt.seed << ",\"final_rng\":" << state.receipt.finalRandomState
                    << ",\"draw_count\":" << state.receipt.randomDrawCount
                    << ",\"friend_actor_id\":" << state.friendActor.actorId << ",\"companions\":[";
                for (size_t index = 0; index < state.receipt.companions.size(); ++index)
                {
                    if (index) trace << ',';
                    const auto &entry = state.receipt.companions[index];
                    trace << '[' << 772 + index << ',' << entry.memberId << ',' << entry.returned << ']';
                }
                trace << "]}";
            }
            else trace << "null";
            if (toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm")
            {
                const auto snapshot = world.snapshot();
                const auto marker = events().namedMapVars.find(JourneyVersion);
                trace << ",\"source_journey_version\":"
                    << (marker == events().namedMapVars.end() ? 0 : marker->second) << ",\"decorations\":[";
                bool separator = false;
                for (size_t index : {381u, 676u, 792u, 10u, 146u, 382u, 829u})
                {
                    if (separator) trace << ',';
                    separator = true;
                    trace << '[' << index << ',' << snapshot.decorationFlags.at(index) << ','
                        << world.isMapDecorationVisible(index) << ']';
                }
                const auto &timer = journeyTimer(snapshot);
                trace << "],\"control_timer\":{\"event\":65457,\"interval_game_minutes\":"
                    << timer.definition.intervalGameMinutes << ",\"next_alarm\":" << timer.nextAlarmGameMinutes
                    << ",\"eligible_minutes\":" << timer.eligibleGameMinutes
                    << ",\"has_fired\":" << timer.hasFired << "},\"time_motion\":[";
                for (uint32_t eventId : {101u, 102u, 103u})
                {
                    if (eventId != 101) trace << ',';
                    const auto &motion = events().mechanisms.at(eventId);
                    trace << '[' << eventId << ',' << motion.state << ',' << motion.timeSinceTriggeredMs
                        << ',' << motion.currentDistance << ',' << int(motion.rotationDirection)
                        << ',' << motion.isMoving << ']';
                }
                trace << ']';
            }
            trace << ",\"members\":[";
            const auto baseline = ownedMembers(beforeParty);
            bool separator = false;
            for (const auto &[id, member] : ownedMembers(party))
            {
                if (separator) trace << ',';
                separator = true;
                const auto found = baseline.find(id);
                const bool changed = found != baseline.end() && (found->second.health != member.health
                    || found->second.spellPoints != member.spellPoints
                    || found->second.conditions != member.conditions);
                naturalVitalsChanged = naturalVitalsChanged || changed;
                trace << "{\"id\":" << id << ",\"name\":" << std::quoted(member.name)
                    << ",\"roster\":" << member.rosterId << ",\"character_data\":" << member.characterDataId
                    << ",\"hp\":" << member.health << ",\"sp\":" << member.spellPoints
                    << ",\"conditions\":" << member.conditions.to_ullong() << ",\"recovery\":"
                    << member.recoverySecondsRemaining << ",\"xp\":" << member.experience
                    << ",\"items_sha256\":" << std::quoted(itemsDigest(member))
                    << ",\"natural_vitals_changed\":" << changed << '}';
            }
            trace << "]}\n";
            trace.flush();
        };
        const auto verifySplit = [&]()
        {
            failureDomain = "normal-onload-split-publication";
            require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm"
                && game.m_gameSession.breachAlvarCompanions(), "Ordinary OnLoad did not publish a split receipt");
            const auto &map = game.m_gameDataLoader.getSelectedMap();
            require(map && map->map.worldId == "mmmerge", "Arrival did not use the ordinary mmmerge world package");
            const OutdoorMapData *pGeometry = scene().worldRuntime().mapData();
            if (originalDecorationFlags.empty())
            {
                require(pGeometry && map->companionPath && game.m_pAssetFileSystem,
                    "Original BrAlvar DDM baseline is unavailable");
                const auto bytes = game.m_pAssetFileSystem->readBinaryFile(*map->companionPath);
                const MapDeltaDataLoader loader;
                const auto raw = bytes ? loader.loadOutdoorFromBytes(*bytes, *pGeometry) : std::nullopt;
                require(raw && raw->decorationFlags.size() > 829, "Cannot read the original indexed DDM baseline");
                originalDecorationFlags = raw->decorationFlags;
            }
            const auto worldSnapshot = scene().worldRuntime().snapshot();
            require(worldSnapshot.decorationFlags.size() == originalDecorationFlags.size()
                && events().namedMapVars.contains(JourneyVersion)
                && events().namedMapVars.at(JourneyVersion) == 1, "Source journey initialization is incomplete");
            for (size_t index : {381u, 676u, 792u})
                require(worldSnapshot.decorationFlags[index] == uint16_t(originalDecorationFlags[index] | 0x20)
                    && !scene().worldRuntime().isMapDecorationVisible(index),
                    "An indexed source sprite stayed visible");
            for (size_t index : {10u, 146u, 382u, 829u})
                require(pGeometry->entities.at(index).spriteOverrideKey(index) == 10
                    && worldSnapshot.decorationFlags[index] == originalDecorationFlags[index]
                    && scene().worldRuntime().isMapDecorationVisible(index)
                        == ((originalDecorationFlags[index] & 0x20) == 0),
                    "Indexed hiding changed another source decoration sharing key10");
            journeyTimer(worldSnapshot);
            require(!scene().worldRuntime().allowsLloydsBeacon()
                && scene().worldRuntime().townPortalUsesDimensionDoor(),
                "Normal BrAlvar lost its source typed travel restrictions");
            const auto &state = *game.m_gameSession.breachAlvarCompanions();
            const Party::Snapshot party = scene().party().snapshot();
            checkCustody(beforeParty, party);
            require(party.members.size() == 1 && party.detachedMembers.size() == 4
                && state.receipt.initialActiveCount == 5 && state.receipt.leaderMemberId == party.members[0].memberId
                && party.members[0].memberId == beforeParty.members[0].memberId
                && !events().pendingBreachAlvarCompanionRequest, "Split custody or publication boundary differs");
            require(validBreachAlvarCompanionState(game.m_gameSession.breachAlvarCompanions(), party, error), error);
            std::set<uint64_t> detached;
            for (size_t index = 0; index < state.receipt.companions.size(); ++index)
            {
                const auto &entry = state.receipt.companions[index];
                require(!entry.returned && !entry.generation && detached.insert(entry.memberId).second,
                    "Existing-five-member admission generated or returned a companion");
                const auto found = std::find_if(party.detachedMembers.begin(), party.detachedMembers.end(),
                    [&](const Party::DetachedPartyMember &member)
                    {
                        return member.character.memberId == entry.memberId && member.ownerKey == BreachAlvarSplitOwner
                            && member.slotId == 772 + index && member.pendingItemDeliveries.empty();
                    });
                require(found != party.detachedMembers.end(), "Receipt lost its original detached owner/slot");
            }
            const auto &profile = game.m_gameDataLoader.getBreachAlvarCompanionProfile();
            require(profile && OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(
                scene().worldRuntime().snapshot(), state.friendActor, false, error, &profile->friendActor),
                "Real NPC772 actor binding failed: " + error);
            std::unordered_set<uint32_t> expectedQuestBits = beforeParty.questBits;
            expectedQuestBits.insert(1714);
            expectedQuestBits.insert(1715);
            require(party.questBits == expectedQuestBits && party.questBits.contains(1713)
                && game.m_gameSession.namedGlobalVar(FinalQuest) == 1
                && game.m_gameSession.namedGlobalVar(QuestFinished) == 0
                && game.m_gameSession.namedGlobalVar("MMerge.CrossContinents.EnteredBreach") == 0,
                "First-journey source quest flags differ");
            if (committed) require(state == *committed, "Receipt or source selection changed after publication");
            else committed = state;
        };
        const auto save = [&](const std::string &label)
        {
            failureDomain = "admission-save-persistence";
            const auto expectedWorld = scene().worldRuntime().snapshot();
            require(game.quickSaveToPath(output / (label + ".oysav"), "原始准入 " + label), "Actual save failed");
            const auto saved = loadGameDataFromPath(output / (label + ".oysav"), error);
            require(saved && saved->breachAlvarCompanions == committed
                && toLowerCopy(saved->mapFileName) == "bralvar.odm"
                && saved->savedGameMinutes == scene().worldRuntime().gameMinutes(),
                "Save lost the admitted map, receipt or source clock: " + error);
            checkCustody(scene().party().snapshot(), saved->party);
            checkJourneySnapshot(expectedWorld, saved->outdoorWorld);
            require(saved->party.members.size() == 1 && saved->party.detachedMembers.size() == 4,
                "Save lost detached custody");
            record("saved:" + label);
        };
        const auto capture = [&](const std::string &label)
        {
            require(captureLabel.empty(), "Overlapping screenshots");
            captureLabel = label;
            captureFrame = frames + 2;
            captureRequested = false;
        };
        const auto result = [&](bool passed, const std::string &message)
        {
            std::ofstream(output / "result.json") << std::boolalpha << std::setprecision(17)
                << "{\"status\":" << std::quoted(passed ? "passed" : "failed")
                << ",\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
                << ",\"stage\":" << std::quoted(stage) << ",\"complete\":" << complete
                << ",\"app_result\":" << appResult << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"simulation_seconds\":" << simulationSeconds << ",\"source65491_verified\":" << hookVerified
                << ",\"cold_state_verified\":" << coldVerified << ",\"normal_ai\":true"
                << ",\"natural_vitals_changed\":" << naturalVitalsChanged << ",\"failure_domain\":"
                << std::quoted(failureDomain) << ",\"error\":" << std::quoted(message)
                << ",\"input_sha256_before\":" << std::quoted(inputHash)
                << ",\"input_sha256_after\":" << std::quoted(fileDigest(input))
                << ",\"visual_review_required\":true}\n";
        };

        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(input), "Input load failed");
                    game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                    require(!game.m_gameSession.gameplayUiController().heldInventoryItem().active,
                        "Input unexpectedly owns a held item");
                    if (cold)
                    {
                        failureDomain = "admission-cold-restoration";
                        beforeParty = source->party;
                        committed = source->breachAlvarCompanions;
                        require(committed && beforeParty.members.size() == 1 && beforeParty.detachedMembers.size() == 4,
                            "Cold input must be the actual admitted split");
                        require(source->outdoorWorld.eventRuntimeState
                            && source->outdoorWorld.eventRuntimeState->eventRandomState == events().eventRandomState,
                            "Cold load changed the source RNG");
                        const auto &old = source->outdoorParty;
                        const auto restored = scene().partyRuntime().snapshot();
                        require(restored.jumpState == old.jumpState
                            && restored.movementAccumulatorSeconds == old.movementAccumulatorSeconds
                            && std::tie(restored.movementState.x, restored.movementState.y,
                                restored.movementState.footZ, restored.movementState.verticalVelocity,
                                restored.movementState.supportKind, restored.movementState.supportBModelIndex,
                                restored.movementState.supportFaceIndex, restored.movementState.airborne)
                                == std::tie(old.movementState.x, old.movementState.y, old.movementState.footZ,
                                    old.movementState.verticalVelocity, old.movementState.supportKind,
                                    old.movementState.supportBModelIndex, old.movementState.supportFaceIndex,
                                    old.movementState.airborne), "Cold load changed the saved Party pose/motion");
                        const auto world = scene().worldRuntime().snapshot();
                        checkJourneySnapshot(source->outdoorWorld, world);
                        require(world.mapActors.size() == source->outdoorWorld.mapActors.size()
                            && world.nextActorId == source->outdoorWorld.nextActorId,
                            "Cold load allocated another actor");
                        for (size_t index = 0; index < world.mapActors.size(); ++index)
                            require(world.mapActors[index].actorId == source->outdoorWorld.mapActors[index].actorId,
                                "Cold load changed persistent actor identity/order");
                        verifySplit();
                        coldVerified = true;
                        stage = "cold-restored-before-continuation";
                        record("cold-receipt-custody-and-actor-equal");
                        save("restored");
                        capture("restored");
                    }
                    else
                    {
                        failureDomain = "declared-input-fixture";
                        require(!game.m_gameSession.breachAlvarCompanions()
                            && scene().party().memberCount() == 5 && scene().party().snapshot().detachedMembers.empty()
                            && game.m_gameSession.namedGlobalVar(QuestFinished) == 0,
                            "Use the reviewed ordinary five-member out02 input before first admission");
                        Character *pCaster = scene().party().member(2);
                        require(pCaster && pCaster->name == "阿肯那斯" && pCaster->health > 0
                            && pCaster->spellPoints == 55 && pCaster->recoverySecondsRemaining == 0.0f,
                            "Reviewed original member2 is not ready with its unmodified55 SP");
                        events().namedGlobalVars[FinalQuest] = 1;
                        game.m_gameSession.setNamedGlobalVar(FinalQuest, 1);
                        scene().party().setQuestBit(1713, true);
                        pCaster->skills["WaterMagic"] = {"WaterMagic", 10, SkillMastery::Grandmaster};
                        require(pCaster->learnSpell(31), "Cannot declare the required learned spell31 fixture");
                        beforeParty = scene().party().snapshot();
                        checkCustody(source->party, beforeParty);
                        const OutdoorMapData *pMap = scene().worldRuntime().mapData();
                        require(pMap && scene().localEventProgram() && scene().localEventProgram()->hasEvent(65491),
                            "Normal out02 lacks its source Town Portal hook65491");
                        const float ground = sampleOutdoorRenderedTerrainHeight(*pMap, 15103.0f, -9759.0f);
                        scene().partyRuntime().teleportTo(15103.0f, -9759.0f, ground);
                        const auto &pose = scene().partyRuntime().movementState();
                        record("declared-source-range-position");
                        require(!pose.airborne && pose.supportKind != OutdoorSupportKind::None
                            && std::abs(pose.x - 15103.0f) < 0.1f && std::abs(pose.y + 9759.0f) < 0.1f,
                            "Declared source portal position has no ordinary support");
                        require(game.quickSaveToPath(output / "declared-fixture.oysav", "154声明源入口前置"),
                            "Cannot save the concrete declared fixture");
                        stage = "ordinary-out02-source-range";
                        record("declared-upstream-and-caster-fixture");
                    }
                    initialGameMinutes = scene().worldRuntime().gameMinutes();
                    require(initialGameMinutes == source->savedGameMinutes,
                        "Input load changed the saved actual clock");
                    readyTicks = SDL_GetTicks();
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
                    require(++frames <= 180 && SDL_GetTicks() - readyTicks < 60000,
                        "Bounded admission exceeded its frame/wall guard at " + stage);
                    if (captureLabel.empty() && phase == Phase::Cast && frames >= 3)
                    {
                        failureDomain = "native-town-portal-cast-or-normal-interruption";
                        GameplayScreenRuntime &screen = game.m_gameSession.gameplayScreenRuntime();
                        require(!screen.activeEventDialog().isActive && !screen.utilitySpellOverlayReadOnly().active,
                            "Normal world activity interrupted the one cast attempt");
                        const Character *pCaster = scene().party().member(2);
                        const int mana = pCaster->spellPoints;
                        const float recovery = pCaster->recoverySecondsRemaining;
                        const SpellEntry *pSpell = screen.spellTable() ? screen.spellTable()->findById(31) : nullptr;
                        require(pSpell && screen.tryCastSpellFromMember(2, 31, pSpell->name),
                            "The real learned/mastery/canCast pipeline did not start Town Portal");
                        require(events().pendingMapMove && events().pendingMapMove->mapName
                            && toLowerCopy(*events().pendingMapMove->mapName) == "bralvar.odm"
                            && events().pendingMapMove->x == 0 && events().pendingMapMove->y == 0
                            && events().pendingMapMove->z == 0 && !screen.utilitySpellOverlayReadOnly().active
                            && scene().party().member(2)->spellPoints == mana
                            && scene().party().member(2)->recoverySecondsRemaining == recovery,
                            "Source hook did not replace the ordinary portal UI without mana/recovery cost");
                        hookVerified = true;
                        stage = "source65491-map-move-queued";
                        trace << "{\"event\":\"source-map-move\",\"trace_event_id\":"
                            << events().pendingMapMove->traceEventId << ",\"trace_source_kind\":"
                            << std::quoted(events().pendingMapMove->traceSourceKind) << ",\"map\":"
                            << std::quoted(*events().pendingMapMove->mapName) << ",\"mana\":" << mana
                            << ",\"recovery\":" << recovery << "}\n";
                        record("native-cast-source-hook-no-cost");
                        phase = Phase::Arrival;
                        requestFrame = frames;
                    }
                    const float dt = captureLabel.empty() && !complete && phase != Phase::Done ? FrameSeconds : 0.0f;
                    if (dt > 0.0f)
                    {
                        failureDomain = "normal-world-interruption";
                        require(game.m_screenManager.activeScreen() == nullptr
                            && !game.m_gameSession.gameplayScreenRuntime().activeEventDialog().isActive
                            && !game.m_gameSession.turnBasedCombatRuntime().active(),
                            "Normal combat/dialogue interrupted the one admission flow; no retry performed");
                    }
                    game.renderFrame(width, height, wheel, dt);
                    simulationSeconds += dt;
                    require(simulationSeconds <= 5.0
                        && std::abs(scene().worldRuntime().gameMinutes() - initialGameMinutes
                            - simulationSeconds * 0.5) < 0.001,
                        "Source admission changed actual time outside normal0.5game-minute/second updates");
                    if (captureLabel.empty() && phase == Phase::Arrival)
                    {
                        const bool arrived = toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm";
                        record(arrived ? "ordinary-app-arrived" : "ordinary-app-awaiting-map-load");
                        if (arrived && game.m_gameSession.breachAlvarCompanions())
                        {
                            verifySplit();
                            stage = "ordinary-onload-split-committed";
                            record("source-split-committed-once");
                            save("admitted-split");
                            capture("admitted-split");
                            phase = Phase::Continue;
                            continuationStartSeconds = simulationSeconds;
                        }
                        else
                        {
                            failureDomain = arrived ? "normal-onload-split-publication" : "ordinary-map-load";
                            require(!arrived || events().pendingBreachAlvarCompanionRequest.has_value(),
                                "Source OnLoad split was rejected/consumed; no event replay or AI clearing performed");
                            require(frames - requestFrame < 8,
                                "Ordinary load/split did not reach its safe publication point");
                        }
                    }
                    else if (captureLabel.empty() && phase == Phase::Continue)
                    {
                        verifySplit();
                        record("normal-short-continuation");
                        if (simulationSeconds - continuationStartSeconds >= 0.5)
                        {
                            stage = "normal-continuation-saved";
                            save("continued");
                            capture("continued");
                            phase = Phase::Done;
                        }
                    }
                    else if (captureLabel.empty() && phase == Phase::Done && !complete)
                    {
                        verifySplit();
                        require(captures == 2 && fileDigest(input) == inputHash,
                            "Missing images or altered original input");
                        complete = true;
                        stage = "complete";
                        record("complete");
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        SDL_PushEvent(&quit);
                    }
                    if (!captureLabel.empty() && frames >= captureFrame && !captureRequested)
                    {
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureLabel.c_str());
                        captureRequested = true;
                    }
                    if (captureRequested)
                    {
                        if (auto screenshot = Engine::BgfxContext::consumeScreenshot(captureLabel))
                        {
                            SDL_Surface *surface = SDL_CreateSurfaceFrom(int(screenshot->width),
                                int(screenshot->height), SDL_PIXELFORMAT_BGRA32, screenshot->bgraPixels.data(),
                                int(screenshot->width * 4));
                            require(surface != nullptr, "Cannot construct captured surface");
                            const bool written =
                                SDL_SaveBMP(surface, (output / (captureLabel + ".bmp")).string().c_str());
                            SDL_DestroySurface(surface);
                            require(written, "Cannot save actual admission image");
                            record("captured:" + captureLabel);
                            captureLabel.clear();
                            captureRequested = false;
                            ++captures;
                        }
                    }
                },
                [&]() { game.shutdownApplication(); });
            appResult = app.run();
            require(appResult == 0 && complete, "Actual admission was interrupted before completion");
            result(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            try
            {
                if (game.m_pMapSceneRuntime) record("failure-live-state");
            }
            catch (const std::exception &)
            {
                // The renderer may already have shut down; retain the original failure below.
            }
            trace << "{\"event\":\"failure\",\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"failure_domain\":" << std::quoted(failureDomain)
                << ",\"error\":" << std::quoted(failure.what()) << "}\n";
            trace.flush();
            result(false, failure.what());
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "br_alvar_admission_capture NORMAL_RUNTIME READONLY_SAVE NEW_OUTPUT actual|cold\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "BrAlvar admission capture: " << failure.what() << '\n';
        return 1;
    }
}
