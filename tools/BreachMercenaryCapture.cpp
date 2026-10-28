#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/StringUtils.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayDialogContextBuilder.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/MercenaryRecruitmentRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/screens/WinGameScreen.h"

#include <CommonCrypto/CommonDigest.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
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
constexpr uint64_t TargetMemberId = 2;
constexpr uint32_t TargetRosterId = 2;
constexpr uint32_t TargetNpcId = 263;
constexpr uint32_t BreachTravelTopic = 65517;
constexpr const char *QuestPrefix = "MMerge.CrossContinents.";
constexpr const char *DefaultInput =
    "/Volumes/NVMeSSD/Projects/openyamm-Ⅰpados/local-data/validation/"
    "normal-merge-admission-154/actual-004/declared-fixture.oysav";
constexpr const char *DefaultInputHash = "adfc5150b555b59ccd3e6c194cf175fa36b1d7138fce96b61a17cc37f2acbd80";

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string bytesHash(const std::vector<uint8_t> &bytes)
{
    require(bytes.size() <= 128 * 1024 * 1024, "Fingerprint payload exceeds the capture limit");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> digest{};
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), digest.data());
    std::ostringstream result;
    for (unsigned char value : digest)
        result << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    return result.str();
}

std::string fileHash(const std::filesystem::path &path)
{
    require(std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) <= 128 * 1024 * 1024,
        "Invalid read-only save size");
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot read input fingerprint");
    return bytesHash({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
}

std::map<uint64_t, Character> ownedCharacters(const Party::Snapshot &party)
{
    std::map<uint64_t, Character> result;
    const auto append = [&](const Character &member)
    {
        require(member.memberId != 0 && result.emplace(member.memberId, member).second,
            "Missing or duplicated owned memberId");
    };
    for (const Character &member : party.members) append(member);
    for (const AdventurersInnMember &entry : party.adventurersInnMembers) append(entry.character);
    for (const Party::DetachedPartyMember &entry : party.detachedMembers) append(entry.character);
    return result;
}

std::string characterHash(Character member)
{
    // These renderer animation fields are not portrait identity. Retention is checked independently.
    member.portraitState = PortraitId::Normal;
    member.portraitElapsedTicks = 0;
    member.portraitDurationTicks = 0;
    member.portraitSequenceCounter = 0;
    member.portraitImageIndex = 0;
    member.sourceMercenaryRetention = {};
    Party::Snapshot snapshot;
    snapshot.members.push_back(std::move(member));
    return bytesHash(encodePartySnapshotForComparison(snapshot));
}

std::string itemsHash(const Character &member)
{
    Character carrier;
    carrier.inventory = member.inventory;
    carrier.equipment = member.equipment;
    carrier.equipmentRuntime = member.equipmentRuntime;
    Party::Snapshot snapshot;
    snapshot.members.push_back(std::move(carrier));
    return bytesHash(encodePartySnapshotForComparison(snapshot));
}

void verifyCharacters(const Party::Snapshot &before, const Party::Snapshot &after, bool exact)
{
    require(before.nextMemberId == after.nextMemberId && before.gold == after.gold
        && before.bankGold == after.bankGold && before.food == after.food && before.fineGold == after.fineGold,
        "Owned identity allocator or Party resources changed");
    const auto expected = ownedCharacters(before);
    const auto actual = ownedCharacters(after);
    require(expected.size() == 5 && actual.size() == expected.size(), "Owned Character count changed");
    for (const auto &[id, member] : expected)
    {
        const auto found = actual.find(id);
        require(found != actual.end(), "An original owned Character disappeared");
        const Character &other = found->second;
        require(std::tie(member.memberId, member.rosterId, member.characterDataId, member.name, member.biography,
            member.role, member.className, member.portraitTextureName, member.portraitPictureId,
            member.voiceId, member.birthYear, member.sexId, member.raceId, member.level, member.experience,
            member.skillPoints, member.knownSpellIds)
            == std::tie(other.memberId, other.rosterId, other.characterDataId, other.name, other.biography,
                other.role, other.className, other.portraitTextureName, other.portraitPictureId,
                other.voiceId, other.birthYear, other.sexId, other.raceId, other.level, other.experience,
                other.skillPoints, other.knownSpellIds)
            && itemsHash(member) == itemsHash(other), "Original identity, gear, inventory or progression changed");
        // Check gameplay resources during zero-dt operation frames. Full encoded hashes remain observations:
        // the initial legacy-load hashes differed although independent inspection found every saved field equal.
        if (exact) require(std::tie(member.health, member.maxHealth, member.spellPoints, member.maxSpellPoints,
                member.might, member.intellect, member.personality, member.endurance, member.accuracy,
                member.speed, member.luck, member.conditions)
            == std::tie(other.health, other.maxHealth, other.spellPoints, other.maxSpellPoints,
                other.might, other.intellect, other.personality, other.endurance, other.accuracy,
                other.speed, other.luck, other.conditions), "Character resources or base attributes changed");
        if (id != TargetMemberId)
            require(member.sourceMercenaryRetention == other.sourceMercenaryRetention,
                "An unrelated Character acquired a retention fact");
    }
}
}

struct GameApplicationTestAccess
{
    static int runRelease(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, const std::string &mode)
    {
        const bool cold = mode == "release-cold";
        constexpr const char *ReleaseInput =
            "/Volumes/NVMeSSD/Projects/openyamm-Ⅰpados/local-data/validation/"
            "breach-mercenaries-156/actual-005/dismissed.oysav";
        constexpr const char *ReleaseInputHash = "6176fc74923eb02057294eef1cf142869d4da0a5d8c3a2c42cc22732b52cd923";
        if (input == "-")
        {
            require(!cold, "release-cold requires the new released.oysav input");
            input = ReleaseInput;
        }
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(root / "engine.zip")
            && std::filesystem::is_regular_file(root / "worlds/mmmerge.zip")
            && !std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(root).generic_string().starts_with("../"),
            "Release capture requires a complete runtime and new NVMe output outside it");
        const std::string inputHash = fileHash(input);
        require(cold || inputHash == ReleaseInputHash, "Release mode requires the preserved156 dismissed save");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->currentSceneKind == SceneKind::Outdoor
            && toLowerCopy(source->mapFileName) == "out02.odm" && source->party.members.size() == 4
            && source->party.adventurersInnMembers.size() == (cold ? 0 : 1)
            && source->party.detachedMembers.empty() && source->party.pendingItemDeliveries.empty()
            && !source->heldInventoryItemActive && !source->breachAlvarCompanions,
            "Expected the scoped four-active out02 release input: " + error);
        if (!cold)
        {
            const Character &target = source->party.adventurersInnMembers.front().character;
            require(target.memberId == TargetMemberId && target.rosterId == TargetRosterId
                && (!target.sourceMercenaryRetention.sourceRosterId
                    || target.sourceMercenaryRetention.sourceRosterId == TargetRosterId),
                "The preserved Inn member has no authenticated target identity");
        }
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"checkpoint\":177,\"mode\":" << std::quoted(mode)
            << ",\"controlled\":true,\"natural_campaign\":false,\"input\":" << std::quoted(input.string())
            << ",\"input_sha256\":" << std::quoted(inputHash) << ",\"runtime\":" << std::quoted(root.string())
            << ",\"changes\":" << std::quoted(cold
                ? "Independent App load of the new released Save103. No Party, NPC, quest, pose, AI, clock or RNG "
                  "fixture; verify restored reusable slot and absent old Character/template, continue30 normal frames."
                : "Preserved156 dismissed Save101, inheriting its154/156 declared upstream and pose fixtures. "
                  "One ordinary shared P keyboard-input action opens the Inn; this is not physical-key coverage. "
                  "Two real SDL left-button clicks at the resolved rendered Dismiss layout. No direct release call, "
                  "Party/NPC/quest/pose/AI/clock/RNG fixture in the live App. After release, isolated Party and "
                  "EventRuntime copies alone use current time plus40320minutes for one production refill and "
                  "production recruitCharacter admission; that is pure logic evidence, not an actual Join.")
            << ",\"timing\":\"Input, inspection and capture frames use delta0; normal AI remains enabled and "
               "advances only during the final30 frames at1/60 second. Confirmation uses real SDL wall time.\","
               "\"new_unit_cases\":0,\"old_route_replays\":0,\"device_or_simulator\":false,"
               "\"audio_driver\":\"dummy\",\"subjective_audio_claim\":false,"
               "\"bounds\":\"240frames/60wallseconds/at most3images\",\"visual_review_required\":true}\n";
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = settings.immortal = settings.unlimitedMana = settings.startFlying = false;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Inn release177";
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
        enum class Phase { Warmup, Inn, FirstReady, FirstQueued, SecondReady, SecondQueued, Continue, Done };
        Phase phase = Phase::Warmup;
        std::string stage = "initialize", captureLabel;
        unsigned frames = 0, phaseFrame = 0, normalFrames = 0, captures = 0, clicks = 0;
        size_t savedCacheStatesChecked = 0;
        uint64_t started = 0, isolatedNewMemberId = 0;
        bool complete = false, firstClickVerified = false, saveVerified = false, isolatedRefillVerified = false;
        int appResult = -1;
        Party::Snapshot baseline;
        std::optional<int32_t> baselineRefillMonth;
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                && toLowerCopy(game.m_gameSession.currentMapFileName()) == "out02.odm", "Release left source out02");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto events = [&]() -> EventRuntimeState &
        {
            require(scene().eventRuntimeState() != nullptr, "Missing current event runtime");
            return *scene().eventRuntimeState();
        };
        const auto refillMonth = [](const EventRuntimeState &state) -> std::optional<int32_t>
        {
            const auto found = state.namedMapVars.find("LastMercsRefill");
            return found == state.namedMapVars.end() ? std::nullopt : std::optional<int32_t>(found->second);
        };
        const auto verifyParty = [&](const Party::Snapshot &party, bool released)
        {
            require(party.members.size() == 4 && party.detachedMembers.empty()
                && party.adventurersInnMembers.size() == (released ? 0 : 1)
                && party.nextMemberId == baseline.nextMemberId, "Release ownership or monotonic allocator changed");
            for (size_t index = 0; index < baseline.members.size(); ++index)
                require(party.members[index].memberId == baseline.members[index].memberId
                    && party.members[index].rosterId == baseline.members[index].rosterId,
                    "An unrelated active member identity changed");
            auto expectedPool = baseline.reusableMercenaryRosterIds;
            if (released) expectedPool.insert(TargetRosterId);
            require(party.reusableMercenaryRosterIds == expectedPool, "Unexpected reusable-slot set");
            if (released)
            {
                require(!ownedCharacters(party).contains(TargetMemberId) && !party.questBits.contains(402)
                    && !party.unavailableNpcIds.contains(TargetNpcId)
                    && party.npcHouseOverrides.contains(TargetNpcId)
                    && party.npcHouseOverrides.at(TargetNpcId) == 0,
                    "Released Character, sourceQ402 or source NPC263 state survived");
                for (const auto &[id, member] : ownedCharacters(party))
                    require(member.rosterId != TargetRosterId, "Old source template reappeared in owned custody");
            }
            else
            {
                const Character &target = party.adventurersInnMembers.front().character;
                require(target.memberId == TargetMemberId && target.rosterId == TargetRosterId
                    && target.sourceMercenaryRetention == baseline.adventurersInnMembers.front()
                        .character.sourceMercenaryRetention
                    && party.questBits == baseline.questBits && party.npcHouseOverrides == baseline.npcHouseOverrides
                    && party.gold == baseline.gold && party.food == baseline.food,
                    "First confirmation changed ownership, source state or Party resources");
            }
        };
        const auto verifyEventState = [](const EventRuntimeState &state)
        {
            require(state.npcHouseOverrides.contains(TargetNpcId) && state.npcHouseOverrides.at(TargetNpcId) == 0
                && !state.unavailableNpcIds.contains(TargetNpcId)
                && !state.generatedMercenaryRecruitsByNpcId.contains(TargetNpcId)
                && !state.npcNameOverrides.contains(TargetNpcId) && !state.npcPictureOverrides.contains(TargetNpcId)
                && !state.npcProfessionOverrides.contains(TargetNpcId), "A cached target NPC presentation survived");
            require(!state.dialogueState.currentOffer || (state.dialogueState.currentOffer->npcId != TargetNpcId
                && state.dialogueState.currentOffer->rosterId != TargetRosterId),
                "A cached generic target offer survived");
            require((!state.dialogueState.ownedMercenaryOffer
                    || state.dialogueState.ownedMercenaryOffer->binding.npcId != TargetNpcId)
                && (!state.dialogueState.pendingOwnedMercenaryJoin
                    || state.dialogueState.pendingOwnedMercenaryJoin->binding.npcId != TargetNpcId)
                && (!state.pendingDialogueContext || state.pendingDialogueContext->sourceId != TargetNpcId),
                "A cached target dialogue request survived");
        };
        const auto verifySaved = [&](const GameSaveData &saved)
        {
            verifyParty(saved.party, true);
            size_t count = 0;
            const auto inspect = [&](const auto &snapshot)
            {
                if (snapshot.eventRuntimeState) { verifyEventState(*snapshot.eventRuntimeState); ++count; }
            };
            if (saved.hasOutdoorRuntimeState) inspect(saved.outdoorWorld);
            if (saved.hasIndoorSceneState) inspect(saved.indoorScene);
            for (const auto &[key, snapshot] : saved.outdoorWorldStates) inspect(snapshot);
            for (const auto &[key, snapshot] : saved.indoorSceneStates) inspect(snapshot);
            if (saved.breachMaze)
                for (const auto &[level, floor] : saved.breachMaze->floors) inspect(floor.runtime.scene);
            require(count > 0, "Saved release has no event runtime to inspect");
            savedCacheStatesChecked = count;
        };
        const auto record = [&](const std::string &label)
        {
            const auto party = scene().party().snapshot();
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"wall_ms\":" << (started ? SDL_GetTicks() - started : 0)
                << ",\"normal_frames\":" << normalFrames << ",\"active\":" << party.members.size()
                << ",\"inn\":" << party.adventurersInnMembers.size() << ",\"next_member_id\":" << party.nextMemberId
                << ",\"old_member_present\":" << ownedCharacters(party).contains(TargetMemberId)
                << ",\"pool2\":" << party.reusableMercenaryRosterIds.contains(TargetRosterId)
                << ",\"q402\":" << party.questBits.contains(402)
                << ",\"game_minutes\":" << scene().worldRuntime().gameMinutes()
                << ",\"pending_confirmation\":" << screen().interactionState().pendingAdventurersInnRelease.has_value()
                << ",\"confirmation_text\":" << std::quoted(screen().statusBarEventText()) << "}\n";
            trace.flush();
        };
        const auto capture = [&](const std::string &label)
        {
            require(captureLabel.empty() && captures < 3, "Overlapping or excessive release captures");
            captureLabel = label;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, label.c_str());
        };
        const auto clickDismiss = [&](int width, int height)
        {
            const auto *pLayout = screen().findHudLayoutElement("AdventurersInnDismissButton");
            require(pLayout && pLayout->visible && screen().selectedAdventurersInnReleaseIdentity(),
                "Authenticated visible Inn Dismiss button is absent");
            const auto rect = screen().resolveHudLayoutElement(
                "AdventurersInnDismissButton", width, height, pLayout->width, pLayout->height);
            require(rect.has_value(), "Cannot resolve the rendered Inn Dismiss layout");
            int count = 0, pointWidth = 0, pointHeight = 0;
            SDL_Window **pWindows = SDL_GetWindows(&count);
            SDL_Window *pWindow = pWindows && count == 1 ? pWindows[0] : nullptr;
            SDL_free(pWindows);
            require(pWindow && SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Missing capture window");
            for (bool down : {true, false})
            {
                SDL_Event event{};
                event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID = SDL_GetWindowID(pWindow);
                event.button.button = SDL_BUTTON_LEFT;
                event.button.down = down;
                event.button.clicks = 1;
                event.button.x = (rect->x + rect->width * 0.5f) * float(pointWidth) / width;
                event.button.y = (rect->y + rect->height * 0.5f) * float(pointHeight) / height;
                require(SDL_PushEvent(&event), "Cannot queue real SDL Inn click");
            }
            ++clicks;
            phaseFrame = frames;
            record("queued-real-dismiss-click");
        };
        const auto isolatedRefill = [&]()
        {
            const double liveTime = scene().worldRuntime().gameMinutes();
            const double sessionTime = game.m_gameSession.gameMinutes();
            Party copiedParty = scene().party();
            EventRuntimeState copiedEvents = events();
            const auto &data = game.m_gameSession.data();
            const MapStatsEntry *pMap = data.mapStats().findByFileName(game.m_gameSession.currentMapFileName());
            require(pMap != nullptr, "Missing original refill map");
            const MercenaryRecruitmentTables tables{
                &data.houseTable(), &data.mergedNpcNameTable(), &data.mergedCharacterSelectionTable(),
                &data.characterDollTable(), &data.classSkillTable(), &data.classMultiplierTable(),
                &data.raceStartingStatsTable(), &data.itemTable(), &data.standardItemEnchantTable(),
                &data.specialItemEnchantTable(), &data.spellTable()};
            require(!refreshMercenaryRecruitmentForCurrentMap(*pMap, copiedParty, copiedEvents, tables, float(liveTime)),
                "Released copy bypassed its existing same-month refill gate");
            const float later = float(liveTime) + 28 * 24 * 60;
            require(refreshMercenaryRecruitmentForCurrentMap(*pMap, copiedParty, copiedEvents, tables, later)
                && isAvailableGeneratedMercenaryRecruit(copiedParty, copiedEvents, TargetNpcId),
                "Next-month production refill did not reuse released source slot2");
            const Character replacement = copiedEvents.generatedMercenaryRecruitsByNpcId.at(TargetNpcId).character;
            require(replacement.memberId == 0 && replacement.rosterId == TargetRosterId
                && replacement.sourceMercenaryRetention.sourceRosterId == TargetRosterId,
                "Replacement is not a fresh unowned source template");
            require(copiedParty.recruitCharacter(replacement), "Production admission of isolated replacement failed");
            isolatedNewMemberId = copiedParty.members().back().memberId;
            require(isolatedNewMemberId == baseline.nextMemberId && isolatedNewMemberId != TargetMemberId
                && copiedParty.snapshot().nextMemberId == baseline.nextMemberId + 1
                && !ownedCharacters(copiedParty.snapshot()).contains(TargetMemberId), "Replacement reused old memberId");
            verifyParty(scene().party().snapshot(), true);
            verifyEventState(events());
            require(scene().worldRuntime().gameMinutes() == liveTime && game.m_gameSession.gameMinutes() == sessionTime
                && refillMonth(events()) == baselineRefillMonth,
                "Isolated refill changed the live App clock or month gate");
            isolatedRefillVerified = true;
            trace << "{\"event\":\"isolated-production-refill-and-admission\",\"actual_join\":false,"
                   "\"copied_game_minutes\":" << later << ",\"live_game_minutes\":" << liveTime
                << ",\"new_member_id\":" << isolatedNewMemberId << ",\"source_roster_id\":2,\"npc_id\":263}\n";
        };
        const auto save = [&]()
        {
            const auto path = output / (cold ? "continued.oysav" : "released.oysav");
            require(game.quickSaveToPath(path, cold ? "177 release cold" : "177 Inn release"), "Release save failed");
            const auto saved = loadGameDataFromPath(path, error);
            require(saved.has_value(), "Cannot independently decode new release save: " + error);
            verifySaved(*saved);
            std::ifstream file(path, std::ios::binary);
            std::array<unsigned char, 12> header{};
            require(bool(file.read(reinterpret_cast<char *>(header.data()), header.size())), "Missing save header");
            const uint32_t version = uint32_t(header[8]) | uint32_t(header[9]) << 8
                | uint32_t(header[10]) << 16 | uint32_t(header[11]) << 24;
            require(version == 103, "Release did not write explicit Save103");
            saveVerified = true;
            trace << "{\"save\":" << std::quoted(path.filename().string()) << ",\"version\":" << version
                << ",\"cache_event_states_checked\":" << savedCacheStatesChecked
                << ",\"sha256\":" << std::quoted(fileHash(path)) << "}\n";
        };
        const auto writeResult = [&](bool passed, const std::string &failure)
        {
            std::ofstream(output / "result.json") << std::boolalpha
                << "{\"checkpoint\":177,\"passed\":" << passed << ",\"mode\":" << std::quoted(mode)
                << ",\"controlled\":true,\"natural_campaign\":false,\"stage\":" << std::quoted(stage)
                << ",\"frames\":" << frames << ",\"normal_frames\":" << normalFrames
                << ",\"normal_simulation_seconds\":" << double(normalFrames) / 60.0
                << ",\"captures\":" << captures << ",\"real_SDL_clicks\":" << clicks
                << ",\"first_click_owner_unchanged\":" << firstClickVerified
                << ",\"save103_verified\":" << saveVerified
                << ",\"cache_event_states_checked\":" << savedCacheStatesChecked
                << ",\"isolated_refill_and_new_identity\":" << isolatedRefillVerified
                << ",\"isolated_new_member_id\":" << isolatedNewMemberId
                << ",\"actual_join_claim\":false,\"app_exit\":" << appResult
                << ",\"input_delta_seconds\":0,\"normal_ai\":true,\"new_unit_cases\":0,"
                   "\"old_route_replays\":0,\"device_or_simulator\":false,\"error\":" << std::quoted(failure)
                << ",\"input_sha256\":" << std::quoted(inputHash) << "}\n";
        };
        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal && game.initializeRenderer()
                        && game.loadSessionFromPath(input), "Release capture cannot load the read-only input");
                    require(screen().mercenaryReleaseProfile() && !screen().activeEventDialog().isActive,
                        "Release content is absent or the input has an active dialogue");
                    baseline = scene().party().snapshot();
                    require(baseline.nextMemberId == source->party.nextMemberId, "Load changed identity allocator");
                    baselineRefillMonth = refillMonth(events());
                    require(baselineRefillMonth.has_value(), "Normal source map has no existing monthly refill gate");
                    started = SDL_GetTicks();
                    verifyParty(baseline, cold);
                    if (cold) { verifySaved(*source); verifyEventState(events()); }
                    record("loaded-readonly-input");
                    return true;
                },
                [&](const SDL_Event &event) { game.handleSdlEvent(event); },
                [&](int width, int height, float wheel, float)
                {
                    require(++frames <= 240 && SDL_GetTicks() - started < 60000, "Bounded release capture timed out");
                    const bool normal = phase == Phase::Continue && captureLabel.empty();
                    game.renderFrame(width, height, wheel, normal ? 1.0f / 60.0f : 0.0f);
                    if (normal) ++normalFrames;
                    require(game.m_screenManager.activeScreen() == nullptr && !screen().activeEventDialog().isActive,
                        "Release flow was interrupted; no route replay");
                    if (!captureLabel.empty())
                    {
                        if (auto image = Engine::BgfxContext::consumeScreenshot(captureLabel))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                            require(pSurface != nullptr, "Cannot create release screenshot surface");
                            const bool written = SDL_SaveBMP(pSurface, (output / (captureLabel + ".bmp")).c_str());
                            SDL_DestroySurface(pSurface);
                            require(written, "Cannot save release screenshot");
                            record("captured:" + captureLabel);
                            captureLabel.clear();
                            ++captures;
                        }
                        if (!captureLabel.empty()) return;
                    }
                    if (phase == Phase::Warmup && frames >= 8)
                    {
                        if (cold)
                        {
                            verifyParty(scene().party().snapshot(), true);
                            phase = Phase::Continue; stage = "cold-normal-continuation";
                        }
                        else
                        {
                            // Establish the comparison after ordinary load callbacks have settled at zero dt.
                            baseline = scene().party().snapshot();
                            require(baseline.nextMemberId == source->party.nextMemberId,
                                "Warmup changed the original identity allocator");
                            baselineRefillMonth = refillMonth(events());
                            verifyParty(baseline, false);
                            GameplayInputFrame key;
                            key.screenWidth = width; key.screenHeight = height;
                            key.keyboardHeld[SDL_SCANCODE_P] = true;
                            key.keyboardPressCounts[SDL_SCANCODE_P] = 1;
                            game.m_gameSession.updateGameplay(key, 0);
                            key.keyboardHeld[SDL_SCANCODE_P] = false;
                            key.keyboardPressCounts[SDL_SCANCODE_P] = 0;
                            game.m_gameSession.updateGameplay(key, 0);
                            const auto identity = screen().selectedAdventurersInnReleaseIdentity();
                            require(identity && identity->memberId == TargetMemberId
                                && identity->sourceRosterId == TargetRosterId, "Ordinary P action did not open target Inn");
                            phase = Phase::Inn; phaseFrame = frames; stage = "ordinary-P-Inn";
                            record(stage);
                        }
                    }
                    else if (phase == Phase::Inn && frames >= phaseFrame + 2)
                    {
                        verifyParty(scene().party().snapshot(), false);
                        capture("inn-before-release"); phase = Phase::FirstReady;
                    }
                    else if (phase == Phase::FirstReady)
                    {
                        clickDismiss(width, height); phase = Phase::FirstQueued; stage = "first-SDL-click";
                    }
                    else if (phase == Phase::FirstQueued)
                    {
                        if (!screen().interactionState().pendingAdventurersInnRelease && frames < phaseFrame + 3) return;
                        verifyParty(scene().party().snapshot(), false);
                        const auto identity = screen().selectedAdventurersInnReleaseIdentity();
                        require(identity && screen().interactionState().pendingAdventurersInnRelease == identity
                            && screen().statusBarEventText() == screen().mercenaryReleaseProfile()->confirmationText,
                            "First SDL click did not show source confirmation without releasing");
                        firstClickVerified = true;
                        record("first-click-confirmation-owner-unchanged");
                        capture("inn-source-confirmation"); phase = Phase::SecondReady;
                    }
                    else if (phase == Phase::SecondReady)
                    {
                        require(SDL_GetTicks() < screen().interactionState().pendingAdventurersInnReleaseExpiresTicks,
                            "Capture driver exhausted the two-second window before its second click");
                        clickDismiss(width, height); phase = Phase::SecondQueued; stage = "second-SDL-click";
                    }
                    else if (phase == Phase::SecondQueued)
                    {
                        if (scene().party().adventurersInnMembers().size() == 1 && frames < phaseFrame + 3) return;
                        verifyParty(scene().party().snapshot(), true);
                        verifyEventState(events());
                        require(refillMonth(events()) == baselineRefillMonth
                            && !screen().characterScreenReadOnly().open,
                            "Release reset the month gate or left an empty Inn open");
                        record("real-release-committed");
                        isolatedRefill();
                        phase = Phase::Continue; stage = "normal-continuation";
                    }
                    else if (phase == Phase::Continue && normalFrames == 30)
                    {
                        verifyParty(scene().party().snapshot(), true);
                        verifyEventState(events());
                        save();
                        capture(cold ? "release-cold-continued" : "released-continued");
                        phase = Phase::Done; stage = "saved-after30-normal-frames";
                    }
                    else if (phase == Phase::Done)
                    {
                        require(saveVerified && normalFrames == 30 && captures == (cold ? 1u : 3u)
                            && clicks == (cold ? 0u : 2u) && (cold || isolatedRefillVerified)
                            && fileHash(input) == inputHash, "Release evidence is incomplete or original input changed");
                        complete = true;
                        record("complete");
                        SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
                        require(SDL_PushEvent(&quit), "Cannot quit completed release capture");
                    }
                },
                [&]() { game.shutdownApplication(); });
            appResult = app.run();
            require(appResult == 0 && complete, "Release capture ended before its requested boundary");
            writeResult(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            trace << "{\"failure\":" << std::quoted(failure.what()) << "}\n";
            trace.flush();
            writeResult(false, failure.what());
            throw;
        }
    }

    static int run(std::filesystem::path root, std::filesystem::path input,
        std::filesystem::path output, const std::string &mode)
    {
        if (mode == "release" || mode == "release-cold")
            return runRelease(std::move(root), std::move(input), std::move(output), mode);
        const bool cold = mode == "cold";
        require(cold || mode == "actual", "Expected actual or cold mode");
        if (input == "-")
        {
            require(!cold, "Cold mode requires the actual run's joined.oysav");
            input = DefaultInput;
        }
        root = std::filesystem::canonical(root);
        input = std::filesystem::canonical(input);
        output = std::filesystem::weakly_canonical(output);
        require(std::filesystem::is_regular_file(root / "engine.zip")
            && std::filesystem::is_regular_file(root / "worlds/mmmerge.zip"), "Complete fresh runtime required");
        require(!std::filesystem::exists(output) && output.string().starts_with("/Volumes/NVMeSSD/")
            && output.lexically_relative(root).generic_string().starts_with("../"),
            "Use a new NVMe output outside the read-only runtime");
        const std::string inputHash = fileHash(input);
        require(cold || inputHash == DefaultInputHash, "Actual mode requires the reviewed 154 Save100 input");
        std::string error;
        const auto source = loadGameDataFromPath(input, error);
        require(source && source->party.members.size() == 5 && !source->heldInventoryItemActive
            && source->party.adventurersInnMembers.empty() && source->party.detachedMembers.empty()
            && source->party.pendingItemDeliveries.empty() && !source->breachAlvarCompanions,
            "Expected five owned active members, empty hands/custody and no companion receipt: " + error);
        require(source->currentSceneKind == SceneKind::Outdoor
            && toLowerCopy(source->mapFileName) == (cold ? "breach.odm" : "out02.odm"),
            "Wrong source map for the selected mode");
        const auto originalMembers = ownedCharacters(source->party);
        require(originalMembers.contains(TargetMemberId)
            && originalMembers.at(TargetMemberId).rosterId == TargetRosterId,
            "Reviewed target memberId/source roster is missing");
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);

        std::ofstream(output / "fixture.json") << std::boolalpha
            << "{\"checkpoint\":156,\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"input\":" << std::quoted(input.string()) << ",\"input_sha256\":" << std::quoted(inputHash)
            << ",\"runtime\":" << std::quoted(root.string()) << ",\"changes\":" << std::quoted(cold
                ? "Independent App load of the actual joined Save101. No Character, quest, pose, camera, "
                  "actor or RNG fixture. Restored identity/custody/projection check, then 30 frames "
                  "at 1/60s; HP/SP, recovery, buffs and clock may advance normally."
                : "Read-only 154 declared-fixture Save100, inheriting its published upstream/caster/pose fixtures. "
                  "One normal Party.dismissMemberToAdventurersInn(1) action entry, not dismissal-button coverage. "
                  "Declare GotMainQuest=1, QuestFinished=1 and StartedAt=3 for this MM8 source input; "
                  "ordinary openNpcDialogue803 contact fixture. "
                  "Visible root732/65517 actions, real1024ms+100ms delays and normal App Breach OnLoad. "
                  "Dismiss a source endcard with SDL Return if presented. One nearby actor pose/camera fixture; "
                  "actual rendered sprite picking and cursor mouse gesture opens NPC263, then visible Join/Yes. "
                  "No roster/Hired/gear/XP/HP/SP/AI/RNG or direct Join/session transaction writes.")
            << ",\"timing\":\"Operation/render/capture frames use zero dt; source delays use the real SDL clock. "
               "Only cold continuation uses 30 normal 1/60s frames.\","
               "\"comparison_limit\":\"Full encoded Character hashes are observations, not acceptance checks; "
               "initial Save100 hashes differed despite all decoded persistent Character fields matching. "
               "Actual checks identity, custody, equipment, inventory, progression, HP/SP and base attributes.\","
               "\"new_unit_tests\":0,"
               "\"bounds\":\"600 frames,120 wall seconds; at most128 read-only pointer probes\","
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
        config.appName = "OpenYAMM Breach mercenaries156";
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
        enum class Phase { Contact, Hints, Travel, Delay, Place, Pointer, Offer, Accept, Joined, Cold, Done };
        Phase phase = cold ? Phase::Cold : Phase::Contact;
        std::string stage = "initialize", captureLabel;
        uint64_t started = 0, topicStarted = 0, messageStarted = 0, closedAt = 0;
        unsigned frames = 0, phaseFrame = 0, captureFrame = 0, captures = 0, normalFrames = 0;
        unsigned pointerProbes = 0, pointerClicks = 0, joins = 0, endCards = 0;
        bool captureRequested = false, complete = false, delayVerified = false, coldVerified = false;
        int appResult = -1;
        SourceMercenaryRetention expectedRetention;
        std::optional<OwnedMercenaryActorBinding> expectedBinding;
        std::optional<BreachMercenaryProjectionState> coldProjection;
        size_t projectedActorCount = 0;
        uint32_t projectedNextActorId = 0;

        const auto screen = [&]() -> GameplayScreenRuntime &
            { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Expected an ordinary outdoor scene");
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto world = [&]() -> OutdoorWorldRuntime & { return scene().worldRuntime(); };
        const auto events = [&]() -> EventRuntimeState &
        {
            require(world().eventRuntimeState() != nullptr, "Missing live event state");
            return *world().eventRuntimeState();
        };
        const auto profile = [&]() -> const BreachMercenaryProfile &
        {
            const auto &value = game.m_gameDataLoader.getBreachMercenaryProfile();
            require(value.has_value(), "Fresh runtime does not contain the Breach mercenary profile");
            require(validBreachMercenaryProfile(*value, error), "Invalid Breach profile: " + error);
            return *value;
        };
        const auto verifyParty = [&](bool joined, bool exact)
        {
            const Party::Snapshot party = screen().party()->snapshot();
            verifyCharacters(source->party, party, exact);
            require(party.members.size() == (joined ? 5 : 4) && party.detachedMembers.empty()
                && party.adventurersInnMembers.size() == (joined ? 0 : 1)
                && party.pendingItemDeliveries.empty() && !screen().heldInventoryItem().active,
                "Ordinary dismissal/Join custody or pending ownership changed");
            const auto members = ownedCharacters(party);
            require(members.at(TargetMemberId).sourceMercenaryRetention == expectedRetention
                && expectedRetention.kind == SourceMercenaryRetentionKind::Retained
                && expectedRetention.provenance == SourceMercenaryProvenance::OrdinaryDismissal,
                "Target lost its exact ordinary-dismissal provenance");
            if (joined)
            {
                require(party.members.back().memberId == TargetMemberId,
                    "Original member did not return through the normal appended Party slot");
            }
            else
            {
                require(party.adventurersInnMembers.front().character.memberId == TargetMemberId,
                    "Dismissed original member is not retained in the existing Inn container");
            }
        };
        const auto verifyProjection = [&](bool joined)
        {
            const auto snapshot = world().snapshot();
            require(snapshot.breachMercenaries && snapshot.breachMercenaries->bindings.size() == 1
                && snapshot.breachMercenaries->canonicalMapKey == profile().canonicalMapKey
                && snapshot.breachMercenaries->profileVersion == profile().version
                && snapshot.breachMercenaries->sourceSha256 == profile().sourceSha256,
                "Missing or changed authenticated one-member projection");
            const auto &binding = snapshot.breachMercenaries->bindings.front();
            require(binding.memberId == TargetMemberId && binding.sourceRosterId == TargetRosterId
                && binding.npcId == TargetNpcId && binding.sourceSpotIndex == 1,
                "Projection does not bind the original dismissed member");
            if (!expectedBinding)
            {
                expectedBinding = binding;
                projectedActorCount = snapshot.mapActors.size();
                projectedNextActorId = snapshot.nextActorId;
            }
            require(binding == *expectedBinding && snapshot.mapActors.size() == projectedActorCount
                && snapshot.nextActorId == projectedNextActorId, "Projection regenerated an actor or binding");
            const auto found = std::find_if(snapshot.mapActors.begin(), snapshot.mapActors.end(),
                [&](const auto &actor) { return actor.actorId == binding.actorId; });
            require(found != snapshot.mapActors.end(), "Persistent projection actor is missing");
            const auto &actor = *found;
            require(actor.npcId == TargetNpcId && actor.monsterId == 368 && actor.sourceRemoved == joined
                && actor.isSourceRemoved() == joined && actor.levelOverride == originalMembers.at(TargetMemberId).level
                && actor.combatState.maximumHitPoints() == originalMembers.at(TargetMemberId).maxHealth
                && actor.ally == profile().actor.ally && !actor.hostileToParty,
                "Derived source actor fields or terminal removal changed");
            const size_t index = size_t(found - snapshot.mapActors.begin());
            require((world().mapActorState(index) == nullptr) == joined,
                "Removed projection is still exposed to gameplay or the live NPC is hidden");
            if (!joined)
            {
                const auto &spot = profile().spots.front();
                require(actor.currentHp == originalMembers.at(TargetMemberId).maxHealth
                    && actor.preciseX == spot.x && actor.preciseY == spot.y && actor.preciseZ == spot.z
                    && actor.wanderRadius == float(profile().actor.guardRadius),
                    "Fresh actor does not use source HP/position/guard radius");
            }
            if (cold) require(snapshot.breachMercenaries == coldProjection, "Cold projection receipt changed");
            if (cold && normalFrames == 0)
            {
                const auto savedActor = std::find_if(source->outdoorWorld.mapActors.begin(),
                    source->outdoorWorld.mapActors.end(),
                    [&](const auto &value) { return value.actorId == binding.actorId; });
                require(savedActor != source->outdoorWorld.mapActors.end()
                    && std::tie(actor.currentHp, actor.combatState, actor.levelOverride, actor.sourceRemoved,
                        actor.npcId, actor.monsterId, actor.preciseX, actor.preciseY, actor.preciseZ)
                    == std::tie(savedActor->currentHp, savedActor->combatState, savedActor->levelOverride,
                        savedActor->sourceRemoved, savedActor->npcId, savedActor->monsterId,
                        savedActor->preciseX, savedActor->preciseY, savedActor->preciseZ),
                    "Cold changed the persisted removed actor identity/combat/pose");
            }
            return index;
        };
        const auto record = [&](const std::string &label)
        {
            trace << "{\"event\":" << std::quoted(label) << ",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"wall_ms\":" << (started ? SDL_GetTicks() - started : 0)
                << ",\"normal_frames\":" << normalFrames << ",\"map\":"
                << std::quoted(game.m_gameSession.currentMapFileName()) << ",\"game_minutes\":" << world().gameMinutes()
                << ",\"pose\":[" << screen().partyX() << ',' << screen().partyY() << ',' << screen().partyFootZ()
                << "],\"members\":[";
            bool comma = false;
            for (const auto &[id, member] : ownedCharacters(screen().party()->snapshot()))
            {
                if (comma) trace << ',';
                comma = true;
                trace << "{\"member_id\":" << id << ",\"roster_id\":" << member.rosterId
                    << ",\"retention\":" << int(member.sourceMercenaryRetention.kind)
                    << ",\"provenance\":" << int(member.sourceMercenaryRetention.provenance)
                    << ",\"hp\":" << member.health << ",\"sp\":" << member.spellPoints
                    << ",\"xp\":" << member.experience << ",\"recovery\":" << member.recoverySecondsRemaining
                    << ",\"character_sha256\":" << std::quoted(characterHash(member))
                    << ",\"items_sha256\":" << std::quoted(itemsHash(member)) << '}';
            }
            trace << "],\"dialogue\":" << screen().activeEventDialog().isActive
                << ",\"dialogue_source\":" << screen().activeEventDialog().sourceId
                << ",\"pending_join\":" << events().dialogueState.pendingOwnedMercenaryJoin.has_value()
                << ",\"pending_delayed_move\":" << events().pendingDelayedMapMove.has_value();
            if (const auto snapshot = world().snapshot(); snapshot.breachMercenaries)
                trace << ",\"projection_bindings\":" << snapshot.breachMercenaries->bindings.size()
                    << ",\"actor_count\":" << snapshot.mapActors.size()
                    << ",\"next_actor_id\":" << snapshot.nextActorId;
            trace << "}\n";
            trace.flush();
        };
        const auto advance = [&](Phase next, const std::string &label)
        {
            phase = next;
            phaseFrame = frames;
            stage = label;
            record(label);
        };
        const auto capture = [&](const std::string &label)
        {
            require(captureLabel.empty(), "Overlapping captures");
            captureLabel = label;
            captureFrame = frames + 2;
            captureRequested = false;
        };
        const auto save = [&](const std::string &label, bool joined, bool exact)
        {
            verifyParty(joined, exact);
            require(game.quickSaveToPath(output / (label + ".oysav"), "156雇佣兵 " + label), "Save failed");
            const auto saved = loadGameDataFromPath(output / (label + ".oysav"), error);
            require(saved && saved->mapFileName == game.m_gameSession.currentMapFileName(), "Saved map mismatch");
            verifyCharacters(source->party, saved->party, exact);
            require(ownedCharacters(saved->party).at(TargetMemberId).sourceMercenaryRetention == expectedRetention
                && saved->party.members.size() == (joined ? 5 : 4)
                && saved->party.adventurersInnMembers.size() == (joined ? 0 : 1)
                && saved->party.detachedMembers.empty(), "Saved retention or custody differs from the live result");
            if (toLowerCopy(saved->mapFileName) == "breach.odm")
            {
                require(validateSavedBreachMercenaries(*saved, error, &profile(), true), error);
                require(saved->outdoorWorld.breachMercenaries == world().snapshot().breachMercenaries,
                    "Saved projection differs from the actual live world");
            }
            trace << "{\"save\":" << std::quoted(label + ".oysav")
                << ",\"sha256\":" << std::quoted(fileHash(output / (label + ".oysav"))) << "}\n";
            record("saved:" + label);
        };
        const auto choose = [&](EventDialogActionKind kind, uint32_t id, const std::string &label)
        {
            const auto &dialog = screen().activeEventDialog();
            const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(), [&](const auto &action)
            {
                return action.kind == kind && action.id == id && action.enabled && !action.textOnly;
            });
            require(dialog.isActive && found != dialog.actions.end()
                && (label.empty() || found->label == label), "Expected visible source dialogue action is absent");
            trace << "{\"dialogue_action\":" << int(kind) << ",\"id\":" << id
                << ",\"label\":" << std::quoted(found->label) << "}\n";
            screen().eventDialogSelectionIndex() = size_t(found - dialog.actions.begin());
            screen().executeActiveDialogAction();
        };
        const auto openVerdant = [&]()
        {
            auto &data = game.m_gameSession.data();
            auto context = buildGameplayDialogContext(game.m_gameSession.gameplayUiController(), events(),
                screen().activeEventDialog(), screen().eventDialogSelectionIndex(), screen().party(), &world(),
                world().globalEventProgram(), screen().houseTable(), screen().classSkillTable(),
                screen().npcDialogTable(), &data.mm9RudeDialogueTable(), &data.mm9MapTransitionTable(),
                &data.mm9SkillTrainerTable(), &data.mm9TransportRouteTable(), &data.transitionTable(),
                data.mapStats().findByFileName(game.m_gameSession.currentMapFileName()), &data.mapEntries(),
                screen().rosterTable(), &data.arcomageLibrary(), false, &screen(), &data.mergedNpcProfessionTable(),
                &data.mergedNewsProfessionTopicTable(), &data.mergedNpcBtbTable(), &data.mergedBolsterMapTable(),
                &data.mergedContinentSettingTable(), &data.mergedTeacherTopicTable(),
                &data.mergedTeacherAutonoteTable(), &data.spellTable(), &data.characterInspectTable(),
                &data.houseServiceTextTable());
            screen().presentPendingDialogActionResult(
                game.m_gameSession.gameplayDialogController().openNpcDialogue(context, 803));
            require(screen().activeEventDialog().isActive && screen().activeEventDialog().sourceId == 803,
                "Ordinary Verdant contact did not present NPC803");
        };
        const auto clickActor = [&](int width, int height)
        {
            const size_t target = verifyProjection(false);
            const auto request = world().buildWorldPickRequest(
                {float(width) * 0.5f, float(height) * 0.5f, width, height, true});
            require(request.hasRay, "Actual camera cannot form a sprite ray");
            world().pickKeyboardInteractionTarget(request); // Read-only production billboard-cache refresh.
            std::optional<std::array<float, 2>> pixel;
            const auto probe = [&](float x, float y)
            {
                require(++pointerProbes <= 128, "Bounded pointer probe limit reached");
                const auto hit = world().pickMouseInteractionTarget(
                    world().buildWorldPickRequest({x, y, width, height, true}));
                trace << "{\"pointer_probe\":" << pointerProbes << ",\"pixel\":[" << x << ',' << y
                    << "],\"hit_kind\":" << int(hit.kind);
                if (hit.actor)
                    trace << ",\"actor_index\":" << hit.actor->actorIndex << ",\"npc_id\":" << hit.actor->npcId;
                trace << "}\n";
                if (hit.kind == GameplayWorldHitKind::Actor && hit.actor && hit.actor->actorIndex == target
                    && hit.actor->npcId == TargetNpcId
                    && world().canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                    pixel = std::array<float, 2>{x, y};
            };
            bool hasCandidate = false;
            for (const auto &candidate : game.m_outdoorGameView.m_keyboardInteractionBillboardCandidates)
            {
                if (candidate.inspectHit.kind != "actor" || candidate.inspectHit.runtimeActorIndex != target) continue;
                hasCandidate = true;
                require(candidate.inspectHit.npcId == TargetNpcId, "Rendered sprite lost its NPC identity");
                probe(float(width) * 0.5f, float(height) * 0.5f);
                for (size_t index = 0; index < candidate.samplePointCount && !pixel; ++index)
                    probe(candidate.samplePoints[index].x, candidate.samplePoints[index].y);
                if (!pixel && candidate.samplePointCount >= 5)
                {
                    const auto first = candidate.samplePoints[1], last = candidate.samplePoints[4];
                    for (unsigned row = 1; row < 10 && !pixel; ++row)
                        for (unsigned column = 1; column < 10 && !pixel; ++column)
                            probe(first.x + (last.x - first.x) * float(column) / 10.0f,
                                first.y + (last.y - first.y) * float(row) / 10.0f);
                }
                break;
            }
            require(hasCandidate && pixel, "No rendered target pixel passed real NPC263 picking");
            GameplayInputFrame mouse;
            mouse.screenWidth = width;
            mouse.screenHeight = height;
            mouse.pointerX = (*pixel)[0];
            mouse.pointerY = (*pixel)[1];
            mouse.rightMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(mouse, 0);
            require(game.m_gameSession.gameplayScreenState().gameplayMouseLookState().cursorModeActive,
                "Right-button input did not enter ordinary cursor mode");
            mouse.rightMouseButton.pressed = false;
            mouse.leftMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(mouse, 0);
            mouse.leftMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(mouse, 0);
            mouse.leftMouseButton = {};
            mouse.rightMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(mouse, 0);
            require(screen().activeEventDialog().isActive && screen().activeEventDialog().sourceId == TargetNpcId
                && screen().activeEventDialog().sourceActorIndex == target
                && screen().activeEventDialog().title == originalMembers.at(TargetMemberId).name
                && screen().activeEventDialog().participantPictureId == 1060,
                "Actual sprite mouse gesture did not open NPC263");
            ++pointerClicks;
        };
        const auto writeResult = [&](bool passed, const std::string &failure)
        {
            std::ofstream result(output / "result.json");
            result << std::boolalpha << "{\"checkpoint\":156,\"passed\":" << passed
                << ",\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
                << ",\"stage\":" << std::quoted(stage) << ",\"error\":" << std::quoted(failure)
                << ",\"app_exit\":" << appResult << ",\"frames\":" << frames << ",\"normal_frames\":" << normalFrames
                << ",\"captures\":" << captures << ",\"pointer_probes\":" << pointerProbes
                << ",\"pointer_clicks\":" << pointerClicks << ",\"accepted_join_actions\":" << joins
                << ",\"end_cards\":" << endCards << ",\"real_delay_verified\":" << delayVerified
                << ",\"cold_verified\":" << coldVerified << ",\"member_id\":2,\"source_roster_id\":2,\"npc_id\":263,"
                   "\"save\":" << std::quoted(cold ? "continued.oysav" : "joined.oysav")
                << ",\"input_sha256\":" << std::quoted(inputHash) << ",\"new_unit_tests\":0,"
                   "\"limit\":\"One ordinary dismissal and owned NPC Join, then one independent cold run; "
                   "not natural campaign, button-dismissal, device or full mercenary acceptance.\"}\n";
        };

        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                    require(game.initializeRenderer() && game.loadSessionFromPath(input),
                        "Cannot load read-only input");
                    profile();
                    require(!screen().activeEventDialog().isActive, "Input contains an unfinished dialogue");
                    require(game.quickSaveToPath(output / "loaded-baseline.oysav", "156 loaded baseline"),
                        "Cannot preserve the loaded Character baseline before comparison");
                    verifyCharacters(source->party, screen().party()->snapshot(), true);
                    started = SDL_GetTicks();
                    if (cold)
                    {
                        expectedRetention = originalMembers.at(TargetMemberId).sourceMercenaryRetention;
                        require(source->outdoorWorld.breachMercenaries
                            && source->outdoorWorld.breachMercenaries->bindings.size() == 1,
                            "Cold input lacks its saved removed actor receipt");
                        coldProjection = source->outdoorWorld.breachMercenaries;
                        expectedBinding = coldProjection->bindings.front();
                        projectedActorCount = source->outdoorWorld.mapActors.size();
                        projectedNextActorId = source->outdoorWorld.nextActorId;
                        verifyParty(true, true);
                        verifyProjection(true);
                        require(!events().dialogueState.ownedMercenaryOffer
                            && !events().dialogueState.pendingOwnedMercenaryJoin,
                            "Cold restored a transient Join dialogue");
                        coldVerified = true;
                        save("restored", true, true);
                        advance(Phase::Cold, "exact-cold-restored");
                    }
                    else
                    {
                        require(screen().party()->member(1)->memberId == TargetMemberId,
                            "Reviewed ordinary-dismiss index has changed");
                        require(screen().party()->dismissMemberToAdventurersInn(1), "Ordinary dismissal failed");
                        expectedRetention = ownedCharacters(screen().party()->snapshot())
                            .at(TargetMemberId).sourceMercenaryRetention;
                        verifyParty(false, true);
                        for (const char *pName : {"GotMainQuest", "QuestFinished"})
                        {
                            const std::string name = std::string(QuestPrefix) + pName;
                            events().namedGlobalVars[name] = 1;
                            game.m_gameSession.setNamedGlobalVar(name, 1);
                        }
                        // A completed introduction already knows its original continent. The old154 input
                        // bypassed that introduction; the normal AfterLoad cannot infer it inside continent4.
                        const std::string startedAt = std::string(QuestPrefix) + "StartedAt";
                        events().namedGlobalVars[startedAt] = 3;
                        game.m_gameSession.setNamedGlobalVar(startedAt, 3);
                        save("dismissed", false, true);
                        record("ordinary-dismissal-and-declared-upstream-flags");
                    }
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
                    require(++frames <= 600 && SDL_GetTicks() - started < 120000 && frames - phaseFrame < 300,
                        "Capture exceeded its bounded frame/wall limit at " + stage);
                    const bool normal = cold && phase == Phase::Cold && captureLabel.empty() && normalFrames < 30;
                    game.renderFrame(width, height, wheel, normal ? 1.0f / 60.0f : 0.0f);
                    if (normal) ++normalFrames;
                    if (!captureLabel.empty())
                    {
                        if (!captureRequested && frames >= captureFrame)
                        {
                            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureLabel.c_str());
                            captureRequested = true;
                        }
                        if (captureRequested)
                        {
                            if (auto image = Engine::BgfxContext::consumeScreenshot(captureLabel))
                            {
                                SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(image->width), int(image->height),
                                    SDL_PIXELFORMAT_BGRA32, image->bgraPixels.data(), int(image->width * 4));
                                require(pSurface != nullptr, "Cannot create screenshot surface");
                                const bool saved = SDL_SaveBMP(pSurface, (output / (captureLabel + ".bmp")).c_str());
                                SDL_DestroySurface(pSurface);
                                require(saved, "Cannot write screenshot");
                                record("captured:" + captureLabel);
                                captureLabel.clear();
                                ++captures;
                            }
                        }
                        return;
                    }
                    if (IScreen *pActive = game.m_screenManager.activeScreen())
                    {
                        require(!cold && (phase == Phase::Delay || phase == Phase::Place) && endCards == 0
                            && toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm"
                            && dynamic_cast<WinGameScreen *>(pActive), "Unexpected active screen; no tactical retry");
                        SDL_Event key{};
                        key.type = SDL_EVENT_KEY_DOWN;
                        key.key.key = SDLK_RETURN;
                        key.key.scancode = SDL_SCANCODE_RETURN;
                        key.key.down = true;
                        game.handleSdlEvent(key);
                        key.type = SDL_EVENT_KEY_UP;
                        key.key.down = false;
                        game.handleSdlEvent(key);
                        ++endCards;
                        record("source-endcard-ordinary-sdl-return");
                        return;
                    }
                    if (phase == Phase::Contact)
                    {
                        openVerdant();
                        advance(Phase::Hints, "ordinary-verdant-contact-fixture");
                    }
                    else if (phase == Phase::Hints)
                    {
                        choose(EventDialogActionKind::NpcTopic, 732, "");
                        advance(Phase::Travel, "visible-hints-branch");
                    }
                    else if (phase == Phase::Travel)
                    {
                        const auto *pMap = game.m_gameSession.data().mapStats().findByFileName("Breach.odm");
                        require(pMap != nullptr, "Missing source Breach map label");
                        topicStarted = SDL_GetTicks();
                        messageStarted = topicStarted;
                        choose(EventDialogActionKind::NpcTopic, BreachTravelTopic, pMap->name);
                        require(events().pendingDelayedMapMove
                            && events().pendingDelayedMapMove->messageDelayMilliseconds == 1024
                            && events().pendingDelayedMapMove->afterCloseDelayMilliseconds == 100
                            && events().pendingDelayedMapMove->move.mapName
                            && toLowerCopy(*events().pendingDelayedMapMove->move.mapName) == "breach.odm",
                            "Visible Breach topic did not queue the original delayed move");
                        advance(Phase::Delay, "source-real-clock-delays");
                    }
                    else if (phase == Phase::Delay)
                    {
                        if (events().pendingDelayedMapMove)
                        {
                            const auto &pending = *events().pendingDelayedMapMove;
                            if (!pending.dialogClosed)
                            {
                                require(screen().activeEventDialog().isActive, "Source message closed too early");
                                if (pending.phaseStartedMilliseconds)
                                    messageStarted = *pending.phaseStartedMilliseconds;
                            }
                            else if (!closedAt)
                            {
                                closedAt = pending.phaseStartedMilliseconds.value_or(0);
                                require(closedAt && closedAt - messageStarted >= 1024
                                    && !screen().activeEventDialog().isActive, "Source message delay was skipped");
                            }
                        }
                        else if (toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm")
                        {
                            require(closedAt && SDL_GetTicks() - closedAt >= 100
                                && SDL_GetTicks() - topicStarted >= 1124, "Source after-close delay was skipped");
                            delayVerified = true;
                            verifyParty(false, true);
                            verifyProjection(false);
                            save("projected", false, true);
                            advance(Phase::Place, "normal-breach-owned-projection");
                        }
                    }
                    else if (phase == Phase::Place)
                    {
                        const size_t index = verifyProjection(false);
                        const auto *pActor = world().mapActorState(index);
                        require(pActor != nullptr, "Live owned actor vanished before placement");
                        record("before-declared-near-actor-pose");
                        scene().partyRuntime().teleportTo(
                            pActor->preciseX - 320.0f, pActor->preciseY, pActor->preciseZ);
                        const auto &pose = scene().partyRuntime().movementState();
                        require(std::abs(pose.footZ - pActor->preciseZ) < 320.0f,
                            "Nearby fixture resolved to another floor");
                        const float pitch = std::atan2(pActor->preciseZ + float(pActor->height) * 0.5f
                            - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight, 320.0f);
                        game.m_outdoorGameView.setCameraAngles(0, pitch);
                        advance(Phase::Pointer, "declared-near-actor-pose");
                    }
                    else if (phase == Phase::Pointer && frames - phaseFrame >= 2)
                    {
                        clickActor(width, height);
                        advance(Phase::Offer, "actual-sprite-npc263-dialogue");
                    }
                    else if (phase == Phase::Offer)
                    {
                        const auto topic = screen().npcDialogTable()->getTopicById(profile().joinTopicId);
                        require(topic.has_value(), "Missing source Join label");
                        choose(EventDialogActionKind::OwnedMercenaryJoinOffer, TargetNpcId, topic->topic);
                        require(events().dialogueState.ownedMercenaryOffer
                            && events().dialogueState.ownedMercenaryOffer->binding == *expectedBinding,
                            "Visible Join did not retain the exact owned binding");
                        const auto &actions = screen().activeEventDialog().actions;
                        require(std::any_of(actions.begin(), actions.end(), [&](const auto &action)
                            {
                                return action.kind == EventDialogActionKind::OwnedMercenaryJoinDecline
                                    && action.id == TargetNpcId && action.label == profile().noText && action.enabled;
                            }), "Source localized No action is absent");
                        capture("join-confirmation");
                        advance(Phase::Accept, "source-yes-no-confirmation");
                    }
                    else if (phase == Phase::Accept)
                    {
                        verifyParty(false, true);
                        choose(EventDialogActionKind::OwnedMercenaryJoinAccept, TargetNpcId, profile().yesText);
                        require(events().dialogueState.pendingOwnedMercenaryJoin
                            && screen().party()->memberCount() == 4,
                            "Yes did not queue a transaction for the normal App safe point");
                        ++joins;
                        advance(Phase::Joined, "visible-yes-awaiting-normal-app-commit");
                    }
                    else if (phase == Phase::Joined)
                    {
                        verifyParty(true, true);
                        verifyProjection(true);
                        require(!events().dialogueState.pendingOwnedMercenaryJoin
                            && !events().dialogueState.ownedMercenaryOffer && !screen().activeEventDialog().isActive,
                            "Successful owned Join did not close its transient dialogue");
                        save("joined", true, true);
                        capture("rejoined");
                        advance(Phase::Done, "original-character-returned-and-actor-removed");
                    }
                    else if (phase == Phase::Cold)
                    {
                        verifyParty(true, false);
                        verifyProjection(true);
                        if (normalFrames == 30)
                        {
                            save("continued", true, false);
                            capture("cold-continued");
                            advance(Phase::Done, "cold-normal-half-second-complete");
                        }
                    }
                    else if (phase == Phase::Done)
                    {
                        verifyParty(true, !cold);
                        verifyProjection(true);
                        require(fileHash(input) == inputHash && (cold
                            ? coldVerified && normalFrames == 30 && captures == 1
                            : delayVerified && pointerClicks == 1 && joins == 1 && captures == 2),
                            "Input changed or bounded actual/cold evidence is incomplete");
                        complete = true;
                        record("complete");
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        SDL_PushEvent(&quit);
                    }
                },
                [&]() { game.shutdownApplication(); });
            appResult = app.run();
            require(appResult == 0 && complete, "Capture ended before completing its requested boundary");
            writeResult(true, "");
            return 0;
        }
        catch (const std::exception &failure)
        {
            try { if (game.m_pMapSceneRuntime) record("failure-live-state"); }
            catch (const std::exception &) { }
            trace << "{\"failure\":" << std::quoted(failure.what()) << "}\n";
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
        std::cerr << "breach_mercenary_capture RUNTIME READONLY_SAVE NEW_OUTPUT actual|cold|release|release-cold\n"
            "Use '-' for actual154 declared-fixture or release156 dismissed; cold modes require an explicit save.\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &failure)
    {
        std::cerr << "Breach mercenary capture: " << failure.what() << '\n';
        return 1;
    }
}
