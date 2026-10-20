#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/FaceEnums.h"
#include "game/StringUtils.h"
#include "game/events/EventDialogContent.h"
#include "game/events/EvtEnums.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/GameplayWorldInteraction.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/screens/WinGameScreen.h"

#include <CommonCrypto/CommonDigest.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *QuestPrefix = "MMerge.CrossContinents.";

void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}

std::string hex(const std::vector<uint8_t> &bytes)
{
    std::ostringstream result;
    for (uint8_t byte : bytes) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}

std::string fileSha256(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot hash input: " + path.string());
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    std::array<char, 65536> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0)
    {
        CC_SHA256_Update(&context, buffer.data(), CC_LONG(input.gcount()));
    }
    require(input.eof(), "Cannot finish input hash");
    std::vector<uint8_t> digest(CC_SHA256_DIGEST_LENGTH);
    CC_SHA256_Final(digest.data(), &context);
    return hex(digest);
}

// Same-build canonical item evidence; never restored or passed to a gameplay transaction.
std::string itemSignature(const Character &member)
{
    Party::Snapshot comparison;
    Character carrier;
    carrier.inventory = member.inventory;
    carrier.equipment = member.equipment;
    carrier.equipmentRuntime = member.equipmentRuntime;
    comparison.members.push_back(std::move(carrier));
    return hex(encodePartySnapshotForComparison(comparison));
}

std::map<uint64_t, Character> allMembers(const Party::Snapshot &party)
{
    std::map<uint64_t, Character> result;
    const auto append = [&](const Character &member)
    {
        require(member.memberId != 0 && result.emplace(member.memberId, member).second,
            "Duplicate or unassigned Party identity");
    };
    for (const Character &member : party.members) append(member);
    for (const auto &member : party.detachedMembers) append(member.character);
    for (const auto &member : party.adventurersInnMembers) append(member.character);
    return result;
}

std::multiset<std::string> pendingOwnership(const Party::Snapshot &party)
{
    std::multiset<std::string> result;
    const auto append = [&](const Party::PendingItemDelivery &delivery, uint64_t recipient)
    {
        result.insert(std::to_string(recipient) + ":" + std::to_string(delivery.allowDropProtectedHeldItem)
            + ":" + hex(encodeInventoryItemForComparison(delivery.item)));
    };
    for (const auto &delivery : party.pendingItemDeliveries)
    {
        require(delivery.recipientMemberIndex >= -1
            && (delivery.recipientMemberIndex < 0 || size_t(delivery.recipientMemberIndex) < party.members.size()),
            "Invalid pending item recipient");
        append(delivery, delivery.recipientMemberIndex < 0 ? 0
            : party.members[size_t(delivery.recipientMemberIndex)].memberId);
    }
    for (const auto &member : party.detachedMembers)
    {
        for (const auto &delivery : member.pendingItemDeliveries)
        {
            require(delivery.recipientMemberIndex == 0, "Detached pending item lost its fixed recipient");
            append(delivery, member.character.memberId);
        }
    }
    return result;
}

void checkCustody(const Party::Snapshot &before, const Party::Snapshot &after, bool checkVitals)
{
    const auto first = allMembers(before);
    const auto last = allMembers(after);
    require(first.size() == last.size(), "Party custody gained or lost a character");
    require(pendingOwnership(before) == pendingOwnership(after), "Pending item owner or full payload changed");
    for (const auto &[id, member] : first)
    {
        const auto found = last.find(id);
        require(found != last.end(), "Stable member identity disappeared");
        const Character &current = found->second;
        require(member.rosterId == current.rosterId && member.characterDataId == current.characterDataId
            && member.name == current.name && member.level == current.level && member.experience == current.experience,
            "Character identity, level or source-zero XP changed");
        require(itemSignature(member) == itemSignature(current), "Owned inventory/equipment payload changed");
        if (checkVitals)
        {
            require(member.health == current.health && member.spellPoints == current.spellPoints
                && member.conditions == current.conditions, "Frozen interaction changed HP/SP/conditions");
        }
    }
}

std::vector<uint8_t> memberPayload(const Party::Snapshot &party, uint64_t memberId)
{
    // Comparison copies only: presentation ticks use wall time even when gameplay delta is zero.
    Party::Snapshot comparison;
    for (size_t index = 0; index < party.members.size(); ++index)
    {
        if (party.members[index].memberId != memberId) continue;
        comparison.members.push_back(party.members[index]);
        comparison.characterBuffs[0] = party.characterBuffs[index];
    }
    for (const auto &member : party.detachedMembers)
    {
        if (member.character.memberId != memberId) continue;
        comparison.members.push_back(member.character);
        comparison.characterBuffs[0] = member.characterBuffs;
    }
    for (const auto &member : party.adventurersInnMembers)
    {
        if (member.character.memberId == memberId) comparison.members.push_back(member.character);
    }
    require(comparison.members.size() == 1, "Full payload comparison lost a unique owned member");
    Character &member = comparison.members.front();
    member.portraitState = PortraitId::Normal;
    member.portraitElapsedTicks = 0;
    member.portraitDurationTicks = 0;
    member.portraitSequenceCounter = 0;
    member.portraitImageIndex = 0;
    for (CharacterBuffState &buff : comparison.characterBuffs[0])
    {
        // Custody changes active indices; the persisted caster identity remains authoritative.
        if (buff.casterMemberId != 0) buff.casterMemberIndex = 0;
    }
    return encodePartySnapshotForComparison(comparison);
}

void checkReserveCustody(const Party::Snapshot &before, const Party::Snapshot &after)
{
    checkCustody(before, after, true);
    require(before.adventurersInnMembers.size() == after.adventurersInnMembers.size(),
        "Reserve custody changed the preserved inn count");
    for (const auto &member : before.adventurersInnMembers)
    {
        require(std::any_of(after.adventurersInnMembers.begin(), after.adventurersInnMembers.end(),
            [&](const auto &current)
            {
                return current.character.memberId == member.character.memberId
                    && current.portraitPictureId == member.portraitPictureId;
            }), "Reserve custody moved or changed an original inn member");
    }
    for (const auto &[id, member] : allMembers(before))
    {
        require(memberPayload(before, id) == memberPayload(after, id),
            "Persisted character/buff payload changed for member " + std::to_string(id) + " (" + member.name + ")");
    }
}

void writeReserveReceipt(std::ostream &stream, const BreachAlvarCompanionReceipt &receipt)
{
    stream << "{\"profile_version\":" << receipt.profileVersion
        << ",\"initial_active_count\":" << receipt.initialActiveCount
        << ",\"leader_level_at_split\":" << receipt.leaderLevelAtSplit
        << ",\"leader_member_id\":" << receipt.leaderMemberId << ",\"seed\":" << receipt.seed
        << ",\"final_random_state\":" << receipt.finalRandomState
        << ",\"random_draw_count\":" << receipt.randomDrawCount << ",\"companions\":[";
    for (size_t index = 0; index < receipt.companions.size(); ++index)
    {
        if (index) stream << ',';
        const auto &entry = receipt.companions[index];
        stream << "{\"npc_id\":" << 772 + index << ",\"member_id\":" << entry.memberId
            << ",\"generation\":";
        if (entry.generation)
        {
            const auto &generation = *entry.generation;
            stream << "{\"request\":{\"roster_id\":" << generation.request.rosterId
                << ",\"level\":" << generation.request.level << ",\"seed\":" << generation.request.seed
                << "},\"final_random_state\":" << generation.finalRandomState
                << ",\"random_draw_count\":" << generation.randomDrawCount << '}';
        }
        else stream << "null";
        stream << '}';
    }
    stream << "]}";
}

using FriendBlood = std::multiset<std::tuple<float, float, float, float>>;

FriendBlood friendBlood(const OutdoorWorldRuntime::Snapshot &world, uint32_t actorId)
{
    FriendBlood result;
    for (const auto &splat : world.bloodSplats)
    {
        if (splat.sourceActorId == actorId) result.emplace(splat.x, splat.y, splat.z, splat.radius);
    }
    return result;
}

void checkColdFriendActor(const OutdoorWorldRuntime::MapActorState &saved,
    const OutdoorWorldRuntime::MapActorState &actual)
{
    require(std::tie(saved.actorId, saved.monsterId, saved.npcId, saved.currentHp, saved.combatState)
        == std::tie(actual.actorId, actual.monsterId, actual.npcId, actual.currentHp, actual.combatState),
        "Cold load changed companion identity or HP/maxHP");
    require(std::tie(saved.breachAlvarCompanion, saved.breachAlvarCompanionRemoved,
        saved.breachAlvarNextMaintenanceGameMinute, saved.canReceiveDamage, saved.isDead, saved.isInvisible,
        saved.proceduralDeathLoot, saved.specialItemId)
        == std::tie(actual.breachAlvarCompanion, actual.breachAlvarCompanionRemoved,
            actual.breachAlvarNextMaintenanceGameMinute, actual.canReceiveDamage, actual.isDead, actual.isInvisible,
            actual.proceduralDeathLoot, actual.specialItemId), "Cold load changed companion lifecycle or loot state");
    // Berserk/rage is an ActorControlMode, with its duration in controlRemainingSeconds.
    require(std::tie(saved.aiType, saved.aiState, saved.hostilityType, saved.hostileToParty, saved.hasDetectedParty,
        saved.alertStatusBit, saved.controlMode, saved.controlRemainingSeconds, saved.fearRemainingSeconds,
        saved.stunRemainingSeconds, saved.paralyzeRemainingSeconds, saved.blindRemainingSeconds)
        == std::tie(actual.aiType, actual.aiState, actual.hostilityType, actual.hostileToParty, actual.hasDetectedParty,
            actual.alertStatusBit, actual.controlMode, actual.controlRemainingSeconds, actual.fearRemainingSeconds,
            actual.stunRemainingSeconds, actual.paralyzeRemainingSeconds, actual.blindRemainingSeconds),
        "Cold load changed companion AI, hostility, control/rage or disabling effects");
    require(saved.bloodSplatSpawned == actual.bloodSplatSpawned, "Cold load changed companion blood-spawn marker");
}

struct Step
{
    std::string name;
    std::function<void()> action;
    bool screenshot = true;
    std::function<bool()> advance;
};
}

// The declared ascent fixture freezes combat scheduling, never actor/Party content or collision.
struct BreachAlvarWorldActorTestAccess
{
    static void discardQueuedAi(OutdoorWorldRuntime &world)
    {
        world.m_actorAiUpdateQueued = false;
        world.m_queuedActorAiDeltaSeconds = 0.0f;
        world.m_queuedActorAiPartyX = 0.0f;
        world.m_queuedActorAiPartyY = 0.0f;
        world.m_queuedActorAiPartyZ = 0.0f;
    }
};

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path seed,
        std::filesystem::path output, const std::string &mode)
    {
        const bool reserveOneRoute = mode == "reserve1";
        const bool ascentResumeRoute = mode == "ascent-ending-resume";
        const bool ascentTopRoute = mode == "ascent-ending-top";
        const bool ascentCheckpointRoute = ascentResumeRoute || ascentTopRoute;
        const bool ascentEndingRoute = mode == "ascent-ending" || ascentCheckpointRoute;
        const bool ascentRoute = mode == "ascent" || ascentEndingRoute;
        const bool reserveRoute = mode == "reserve" || reserveOneRoute;
        const uint32_t reserveInitialActiveCount = reserveOneRoute ? 1 : 4;
        const uint32_t reserveRetainedCount = reserveInitialActiveCount - 1;
        const uint32_t reserveNewCount = 4 - reserveRetainedCount;
        const bool doorRoute = mode == "doors" || reserveRoute;
        const bool chaosRoute = mode == "chaos";
        const bool fresh = mode == "run" || doorRoute;
        const bool coldSplit = mode == "reload-split";
        const bool coldReturned = mode == "reload-returned";
        require(fresh || coldSplit || coldReturned || chaosRoute || ascentRoute,
            "Unknown bounded companion capture mode");
        root = std::filesystem::canonical(root);
        seed = std::filesystem::canonical(seed);
        require(std::filesystem::is_regular_file(root / "worlds/mmmerge.zip"),
            "Explicit controlled complete runtime with inspection mmmerge package required");
        require(!std::filesystem::exists(output), "Output must be a new directory");
        require(std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Evidence must stay on the NVMe volume");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);
        const std::string seedHash = fileSha256(seed);
        std::string error;
        const auto inputSave = loadGameDataFromPath(seed, error);
        require(inputSave.has_value(), "Read-only input codec failed: " + error);

        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"seed\":" << std::quoted(seed.string()) << ",\"seed_sha256\":" << std::quoted(seedHash)
            << ",\"controlled_runtime\":" << std::quoted(root.string())
            << ",\"renderer\":\"Metal\",\"audio\":\"dummy\",\"changes\":"
            << std::quoted(chaosRoute
                ? "Cold-load the completed four-companion return save. Only declared party position/camera "
                  "navigation at six source event5 doors with distinct ceil(X/500)+ceil(Y/500) keys and the "
                  "real OrMatt1 event81 entity. Normal world pointer/house/topic UI, five source answers via "
                  "SDL text and Return, then 1794/1792 capture. No quest, follower, seed, Party item or XP writes. "
                  "App owns return/reward, Breach ending and the one-time certificate; dismiss it to continue."
                : ascentTopRoute
                ? "Cold-load prior actual005/top-arrived.oysav without changing its Party or position. "
                  "All three floor flags are set;106/107/108/114/115/116/128 are fully open and the other18 "
                  "source104..128 mechanisms are closed and static. Preserve every member and actor ID. "
                  "Restore the already-observed supported TopFloor landing at4410; no teleport, stair/door "
                  "replay, source128 click or physical launch in this run. Save top-cold-restored, then normal "
                  "walk/collision to the real orb81, App return/reward/card and continued gameplay. "
                  "Normal1/60 physics/world clock starts accumulating from the saved time. Synthetic camera "
                  "steering and transient combat-AI queue freeze remain declared fixtures; no driver Party, "
                  "quest, actor, pose or seed writes. Three remaining captures are combined with the prior "
                  "actual lower-route and physical-landing evidence. A real floor81 return keeps two labeled "
                  "certificate views, without claiming an orb click. This is not natural campaign completion."
                : ascentResumeRoute
                ? "Cold-load the prior actual third-floor-arrived.oysav, preserving its Party and position. "
                  "Only BrSecFloor is set among the three floor flags;107/108 are fully open and the other23 "
                  "source104..128 mechanisms are closed. No teleport or replay of the accepted two stair flights. "
                  "Continue normal walk/collision through real114/115/116/106 doors,128 launch and TopFloor/orb81. "
                  "Normal1/60 physics/world clock restarts accumulation from this input's saved time. "
                  "Synthetic camera steering and transient combat-AI queue freeze remain declared fixtures; "
                  "no Party, position, quest, actor, seed, generation or capture writes. Existing five solved "
                  "riddles and Chaos capture are read from the input; App owns the new return/reward/card. "
                  "A real floor81 return keeps two labeled certificate views without claiming an orb click. "
                  "This is continuation of prior actual controlled movement evidence, not natural campaign completion."
                : ascentRoute
                ? "Read-only five-returned-member input with no tower/ending progress. ascent-ending resumes "
                  "the accepted Chaos-caught save; existing riddle answers and capture are read, "
                  "never replayed or edited. "
                  "One initial navigation fixture at SubTower bottom (-2200,-910,856), guarded by real low support. "
                  "All later positions come from normal GameplayInputFrame movement and native collision/physics: "
                  "SubTower stairs, real source108/107 faces, the unlocked114/115/116 museum-door route to106, "
                  "source128 within its strict100 XY radius; original locked112/113/117 remain closed. "
                  "physical pure-vertical source launch with apex4410, TopFloor walk and actual OrMatt1/event81 pick. "
                  "Synthetic camera steering and fixed1/60 movement frames; normal world timers/clock and mechanism "
                  "interpolation advance, while the existing test friend discards only the transient actor-AI queue. "
                  "Render/UI/App safe points remain real with delta0; captures freeze simulation. "
                  "No post-start teleport, flight, actor/HP/Party/quest/seed edits or new Chaos capture. "
                  "ascent-ending clicks the reached orb and lets the App own return/reward/card/continued play. "
                  "An earlier original floor81 return instead keeps its real ending and two labeled certificate "
                  "views, without repeating or claiming an orb click. "
                  "This controlled movement/collision fixture is not natural ascent/combat/campaign completion."
                : reserveOneRoute
                ? "Read-only NORMAL MM7 four-member input. Controlled leader-only preparation calls standard "
                  "Party.dismissMemberToAdventurersInn(1) exactly three times, retaining the original three inn "
                  "members. This is not natural MM7 progression or UI dismissal coverage. Standard dismissal may "
                  "change buffs; preservation begins with the actual post-fixture active+inn snapshot before "
                  "the declared BrAlvar map jump. Real OnLoad65469 generates source roster38..41/NPC772..775, "
                  "without driver generation requests, seed, quest, XP or roster writes. Q1714 must retain its "
                  "pre-split value. Actual NPC772 pointer and source50/51/52 doors use declared position/camera "
                  "navigation, then normal Hint/Join. Preserve all eight owned members from the real split "
                  "baseline, excluding portrait animation and stable-ID caster index remapping. "
                  "Only split/complete saves and three captures."
                : reserveRoute
                ? "Read-only NORMAL MM7 four-member save; declared map jump to BrAlvar and real OnLoad65469. "
                  "Only Party position/camera navigation at the real NPC772 and source50/51/52 house faces. "
                  "Actual pointer/house Hint/Join returns the original three companions and source-generated "
                  "NPC775 roster41. No generation request, seed, roster, quest, item or XP injection. "
                  "Preserve all four original persisted character/buff payloads, allowing only portrait animation "
                  "and stable-ID caster index remapping; use the actual split five-member custody as the later "
                  "preservation baseline. Only split/complete saves and three captures."
                : fresh
                ? "Require five existing active members, empty held/pending items, "
                  "no prior split/hints or QuestFinished. "
                  "Debug map jump to explicitly mounted BrAlvar; first real MapOnLoad65469 requests native split. "
                  "Only party position/camera moves near the actual spawned friend. NPC772 uses rendered opaque pixel "
                  "pick and pointer input. The doors mode uses real source50/51/52 faces with declared nearby "
                  "position/camera placement; run mode keeps historical house-entry fixtures. Residents and "
                  "Hint/Join use actual UI actions, Lua and App safe point. "
                  "No receipt, actor, Party item, XP or Qbit injection."
                : "Independent process cold-loads supplied split/returned save; no actor/Party/quest injection. "
                  "reload-split advances exactly one game minute via the actual shared scene time API after "
                  "initial cold equality checks; reload-returned remains frozen.")
            << ",\"timing\":\"Render/UI delta0; ascent movement/world step1/60s "
               "with normal0.5game-minute/second clock; "
               "reload-split advances one game minute; "
               "60-second reserve/chaos or 180-second companion bound; external watchdog also bounds init/load.\","
               "\"limits\":[\"No ordinary Merge admission\",\"No continuous obstacle/navigation acceptance\","
               "\"No natural campaign or iPad acceptance\",\"No nonempty-held or pending-queue stress fixture\","
               "\"Screenshots require independent human review\"]}\n";
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
        config.appName = chaosRoute ? "OpenYAMM Breach Alvar149 Chaos capture"
            : ascentRoute ? "OpenYAMM Breach Alvar149 controlled tower ascent"
            : reserveRoute ? "OpenYAMM Breach Alvar149 source reserve capture"
            : "OpenYAMM Breach Alvar149 controlled companions";
        config.assetRoot = root.string();
        config.activeWorldId = "mm8";
        config.startupMapFileOverride = "out02.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        GameApplication game(config);
        std::ofstream trace(output / "trace.jsonl");
        std::ofstream ownership(output / "ownership.jsonl");
        std::ofstream pointerProbes(output / "pointer-probes.jsonl");
        std::ofstream ascentMovement;
        if (ascentRoute) ascentMovement.open(output / "movement.jsonl");
        std::vector<Step> steps;
        std::string stage = "initialize";
        std::string failureDomain = "initialization";
        std::string pendingCapture;
        std::string scheduledCapture;
        size_t stepIndex = 0;
        unsigned frames = 0;
        unsigned captures = 0;
        unsigned nextStep = fresh || chaosRoute || ascentRoute ? 1 : 36;
        unsigned captureFrame = 0;
        int renderWidth = config.windowWidth;
        int renderHeight = config.windowHeight;
        const uint64_t started = SDL_GetTicks();
        bool complete = false;
        int applicationResult = -1;
        unsigned endCardShows = 0;
        bool endCardVisible = false;
        std::set<int> usedDoorKeys;
        std::set<int> answeredRiddles;
        size_t returnEntityIndex = GameplayInvalidWorldIndex;
        Party::Snapshot beforeParty;
        std::optional<Party::Snapshot> preservationBaseline;
        std::optional<BreachAlvarCompanionState> originalReceipt;
        std::vector<uint8_t> heldBytes;
        size_t originalActorCount = 0;
        double originalDeadline = 0.0;
        bool ascentStepStarted = false;
        double ascentSimulationSeconds = 0.0;
        double ascentStepStartSeconds = 0.0;
        double ascentInitialGameMinutes = 0.0;
        std::vector<bx::Vec3> ascentLowerStairs;
        std::vector<bx::Vec3> ascentUpperStairs;
        std::vector<bx::Vec3> ascentWaypoints;
        size_t ascentWaypointIndex = 0;
        float ascentLaunchZ = 0.0f;
        float ascentMaximumZ = 0.0f;
        bool ascentAirborneSeen = false;
        bool ascentLanded = false;
        bool ascentOriginalFloorReturned = false;
        bool ascentExplicitOrbClick = false;
        std::array<float, 2> ascentOrbPixel{};
        PartyBuffState ascentFeatherFallBefore;

        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Expected real outdoor runtime at " + stage);
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto events = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto screen = [&]() -> GameplayScreenRuntime & { return game.m_gameSession.gameplayScreenRuntime(); };
        const auto profile = [&]() -> const BreachAlvarCompanionProfile &
        {
            const auto &value = game.m_gameDataLoader.getBreachAlvarCompanionProfile();
            require(value.has_value(), "Imported source companion profile unavailable");
            require(validBreachAlvarCompanionProfile(*value, error), "Invalid source profile: " + error);
            require(value->hintExperience == 0, "Driver expects the audited zero-XP source hint");
            return *value;
        };
        const auto receipt = [&]() -> const BreachAlvarCompanionState &
        {
            const auto &value = game.m_gameSession.breachAlvarCompanions();
            require(value.has_value(), "App did not publish the real companion receipt");
            return *value;
        };
        const auto friendIndex = [&]()
        {
            const auto &binding = receipt().friendActor;
            const auto snapshot = scene().worldRuntime().snapshot();
            for (size_t index = 0; index < snapshot.mapActors.size(); ++index)
            {
                if (snapshot.mapActors[index].actorId == binding.actorId) return index;
            }
            throw std::runtime_error("Receipt actor missing from actual persistent world");
        };
        const auto friendActor = [&]()
        {
            // The public mapActorState intentionally hides removed actors from gameplay consumers.
            // Read a value from the persistent snapshot for returned/cold-returned lifecycle evidence.
            const auto snapshot = scene().worldRuntime().snapshot();
            const auto found = std::find_if(snapshot.mapActors.begin(), snapshot.mapActors.end(), [&](const auto &actor)
            {
                return actor.actorId == receipt().friendActor.actorId;
            });
            require(found != snapshot.mapActors.end(), "Persistent companion actor record is absent");
            return *found;
        };
        const auto record = [&](const std::string &label)
        {
            trace << "{\"stage\":" << std::quoted(stage) << ",\"event\":" << std::quoted(label)
                << ",\"frame\":" << frames << ",\"elapsed_ms\":" << SDL_GetTicks() - started
                << ",\"map\":" << std::quoted(game.m_gameSession.currentMapFileName());
            if (game.m_pMapSceneRuntime)
            {
                const Party::Snapshot party = game.m_pMapSceneRuntime->party().snapshot();
                trace << ",\"active\":" << party.members.size() << ",\"detached\":" << party.detachedMembers.size()
                    << ",\"inn\":" << party.adventurersInnMembers.size()
                    << ",\"q1714\":" << (party.questBits.contains(1714) ? "true" : "false")
                    << ",\"pending_items\":" << pendingOwnership(party).size();
                ownership << "{\"stage\":" << std::quoted(stage) << ",\"event\":" << std::quoted(label)
                    << ",\"members\":[";
                bool first = true;
                for (const auto &[id, member] : allMembers(party))
                {
                    if (!first) ownership << ',';
                    first = false;
                    ownership << "{\"id\":" << id << ",\"name\":" << std::quoted(member.name)
                        << ",\"roster\":" << member.rosterId << ",\"character_data\":" << member.characterDataId
                        << ",\"level\":" << member.level
                        << ",\"hp\":" << member.health << ",\"sp\":" << member.spellPoints
                        << ",\"xp\":" << member.experience << ",\"conditions\":" << member.conditions.to_ullong()
                        << ",\"items_canonical_hex\":" << std::quoted(itemSignature(member));
                    if (reserveRoute && (label == "before-declared-map-jump"
                        || label == "source-reserve-publication-validated"))
                        ownership << ",\"persisted_character_buff_payload_hex\":"
                            << std::quoted(hex(memberPayload(party, id)));
                    ownership << '}';
                }
                ownership << "]}\n";
                ownership.flush();
            }
            if (game.m_gameSession.breachAlvarCompanions())
            {
                const auto &value = receipt();
                trace << ",\"actor_id\":" << value.friendActor.actorId
                    << ",\"monster_id\":" << value.friendActor.spawnMonsterId << ",\"returned\":[";
                for (size_t index = 0; index < value.receipt.companions.size(); ++index)
                {
                    if (index) trace << ',';
                    trace << (value.receipt.companions[index].returned ? "true" : "false");
                }
                trace << ']';
                if (reserveRoute)
                {
                    trace << ",\"reserve_receipt\":";
                    writeReserveReceipt(trace, value.receipt);
                    trace << ",\"source_generation_flags\":[";
                    for (uint32_t npc = 772; npc <= 775; ++npc)
                    {
                        if (npc != 772) trace << ',';
                        trace << game.m_gameSession.namedGlobalVar(
                            std::string(QuestPrefix) + "FQMercRnd." + std::to_string(npc));
                    }
                    trace << ']';
                }
                if (game.m_pMapSceneRuntime && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor
                    && toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm")
                {
                    const auto actor = friendActor();
                    trace << ",\"game_minutes\":" << std::setprecision(17) << scene().worldRuntime().gameMinutes()
                        << ",\"maintenance_deadline\":" << actor.breachAlvarNextMaintenanceGameMinute
                        << ",\"actor_hp\":" << actor.currentHp
                        << ",\"removed\":" << (actor.breachAlvarCompanionRemoved ? "true" : "false");
                    const auto &position = scene().partyRuntime().movementState();
                    trace << ",\"pose\":[" << position.x << ',' << position.y << ',' << position.footZ << ']';
                }
            }
            if (chaosRoute && game.m_pMapSceneRuntime)
            {
                const auto value = [&](const std::string &suffix)
                {
                    const auto found = events().namedGlobalVars.find(std::string(QuestPrefix) + suffix);
                    return found == events().namedGlobalVars.end() ? 0 : found->second;
                };
                trace << ",\"riddles_answered\":" << value("RiddlesAnswered")
                    << ",\"pending_riddle\":" << value("PendingRiddle")
                    << ",\"cought_chaos\":" << value("CoughtChaos")
                    << ",\"quest_finished\":" << value("QuestFinished")
                    << ",\"got_end_card\":" << value("GotEndCard")
                    << ",\"door_keys\":" << usedDoorKeys.size() << ",\"end_cards\":" << endCardShows;
            }
            trace << "}\n";
            trace.flush();
        };
        const auto refreshKeyboardCandidates = [&]()
        {
            auto &world = scene().worldRuntime();
            const auto &view = game.m_outdoorGameView;
            const size_t target = friendIndex();
            const size_t beforeCount = view.m_keyboardInteractionBillboardCandidates.size();
            const auto request = world.buildWorldPickRequest(
                {float(renderWidth) * 0.5f, float(renderHeight) * 0.5f, renderWidth, renderHeight, true});
            require(request.hasRay && request.viewWidth > 0 && request.viewHeight > 0,
                "Cannot refresh keyboard candidates without an actual camera pick request");
            // This production query lazily rebuilds the billboard cache. Rendering alone does not.
            // It selects a hit without activating it; pointer dialogue still uses mouse input below.
            const auto hit = world.pickKeyboardInteractionTarget(request);
            size_t targetCandidates = 0;
            for (const auto &candidate : view.m_keyboardInteractionBillboardCandidates)
            {
                if (candidate.inspectHit.kind == "actor" && candidate.inspectHit.runtimeActorIndex == target)
                    ++targetCandidates;
            }
            pointerProbes << "{\"phase\":\"keyboard-refresh\",\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"target_index\":" << target
                << ",\"candidate_count_before\":" << beforeCount
                << ",\"candidate_count_after\":" << view.m_keyboardInteractionBillboardCandidates.size()
                << ",\"target_candidates\":" << targetCandidates
                << ",\"has_hit\":" << (hit.hasHit ? "true" : "false")
                << ",\"hit_kind\":" << int(hit.kind);
            if (hit.actor)
                pointerProbes << ",\"hit_actor_index\":" << hit.actor->actorIndex
                    << ",\"hit_npc_id\":" << hit.actor->npcId
                    << ",\"hit_distance\":" << hit.actor->distance;
            pointerProbes << "}\n";
            pointerProbes.flush();
        };
        const auto verify = [&](size_t returned, bool checkVitals)
        {
            require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm", "Unexpected actual map");
            const auto &value = receipt();
            const Party::Snapshot party = scene().party().snapshot();
            if (reserveRoute)
            {
                require(preservationBaseline.has_value(), "Actual reserve split baseline is absent");
                checkReserveCustody(*preservationBaseline, party);
            }
            else checkCustody(beforeParty, party, checkVitals);
            require(party.members.size() == 1 + returned && party.detachedMembers.size() == 4 - returned,
                "Actual Party return count changed");
            if (reserveOneRoute)
                require(scene().party().hasQuestBit(1714) == beforeParty.questBits.contains(1714),
                    "Source leader-only split changed the pre-existing Q1714 value");
            else require(scene().party().hasQuestBit(1714), "Source split Q1714 missing");
            require(!events().pendingBreachAlvarCompanionRequest, "App left native request queued");
            require(game.m_gameSession.namedGlobalVar(std::string(QuestPrefix) + "QuestFinished") == 0,
                "Unexpected whole Merge completion");
            require(scene().worldRuntime().mapActorCount() == originalActorCount, "Duplicate actor spawn or erase");
            require(OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(scene().worldRuntime().snapshot(),
                value.friendActor, returned != 0, error, &profile().friendActor), error);
            if (returned != 0 && !reserveRoute)
            {
                // Give the actual scene time to render, then query the lazy cache afresh for this check.
                require(frames >= 35, "Removed actor presentation checked before renderer refresh");
                const size_t index = friendIndex();
                require(scene().worldRuntime().mapActorState(index) == nullptr,
                    "Removed companion is still exposed by the gameplay actor query");
                refreshKeyboardCandidates();
                const auto &candidates = game.m_outdoorGameView.m_keyboardInteractionBillboardCandidates;
                require(std::none_of(candidates.begin(), candidates.end(), [index](const auto &candidate)
                {
                    return candidate.inspectHit.kind == "actor" && candidate.inspectHit.runtimeActorIndex == index;
                }), "Removed companion remains in the actual renderer interaction candidates");
                trace << "{\"stage\":" << std::quoted(stage) << ",\"removed_actor_index\":" << index
                    << ",\"public_hidden\":true,\"render_candidate_absent\":true,\"cache_refreshed\":true"
                    << ",\"candidate_count\":" << candidates.size() << ",\"frame\":" << frames << "}\n";
                trace.flush();
            }
            const auto &held = game.m_gameSession.gameplayUiController().heldInventoryItem();
            require(!held.active && encodeInventoryItemForComparison(held.item) == heldBytes,
                "Held-item state changed");
            require(originalReceipt && value.friendActor == originalReceipt->friendActor
                && value.receipt.seed == originalReceipt->receipt.seed
                && value.receipt.finalRandomState == originalReceipt->receipt.finalRandomState
                && value.receipt.randomDrawCount == originalReceipt->receipt.randomDrawCount,
                "Real publication regenerated its seed or actor binding");
            if (reserveRoute)
            {
                require(value.receipt.profileVersion == 2
                    && value.receipt.initialActiveCount == reserveInitialActiveCount
                    && value.receipt.leaderLevelAtSplit == beforeParty.members[0].level,
                    "Reserve receipt changed its immutable source split facts");
                require(party.adventurersInnMembers.size() == beforeParty.adventurersInnMembers.size(),
                    "Reserve route changed the preserved inn custody");
            }
            for (size_t index = 0; index < 4; ++index)
            {
                require(value.receipt.companions[index].returned == (index < returned), "Return receipt order changed");
                require(value.receipt.companions[index].memberId == originalReceipt->receipt.companions[index].memberId,
                    "Companion receipt changed member identity");
                require(game.m_gameSession.isBreachAlvarCompanionAvailable(uint32_t(772 + index))
                    == (index >= returned), "Actual NPC availability disagrees with custody");
                if (reserveRoute)
                {
                    require(value.receipt.companions[index].generation
                        == originalReceipt->receipt.companions[index].generation,
                        "Join changed the actual source generation receipt");
                    require(game.m_gameSession.namedGlobalVar(std::string(QuestPrefix) + "FQMercRnd."
                        + std::to_string(772 + index)) == (index >= reserveRetainedCount ? 1 : 0),
                        "Source reserve/retained-companion generation flag changed");
                }
            }
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(output / (name + ".oysav"), "149受控同伴 " + name),
                "Milestone save failed: " + name);
            const auto saved = loadGameDataFromPath(output / (name + ".oysav"), error);
            require(saved && validateSavedBreachAlvarCompanions(*saved, error, &profile().friendActor), error);
            require(saved->breachAlvarCompanions == game.m_gameSession.breachAlvarCompanions(),
                "Saved receipt differs from actual Session");
            if (reserveRoute) checkReserveCustody(scene().party().snapshot(), saved->party);
            else checkCustody(scene().party().snapshot(), saved->party, true);
            if (ascentRoute)
            {
                require(saved->savedGameMinutes == scene().worldRuntime().gameMinutes()
                    && saved->outdoorWorld.eventRuntimeState.has_value(),
                    "Ascent save lost its real clock or outdoor event state");
                if (toLowerCopy(saved->mapFileName) == "bralvar.odm")
                {
                    for (uint32_t id = 104; id <= 128; ++id)
                    {
                        const auto &live = events().mechanisms.at(id);
                        const auto &readback = saved->outdoorWorld.eventRuntimeState->mechanisms.at(id);
                        require(std::tie(live.state, live.timeSinceTriggeredMs, live.currentDistance,
                            live.rotationDirection, live.isMoving) == std::tie(readback.state,
                                readback.timeSinceTriggeredMs, readback.currentDistance,
                                readback.rotationDirection, readback.isMoving),
                            "Ascent save changed a tower mechanism");
                    }
                }
                const auto live = scene().party().snapshot().partyBuffs[size_t(PartyBuffId::FeatherFall)];
                const auto readback = saved->party.partyBuffs[size_t(PartyBuffId::FeatherFall)];
                require(std::tie(live.remainingSeconds, live.spellId, live.skillLevel, live.skillMastery,
                    live.power, live.casterMemberIndex) == std::tie(readback.remainingSeconds, readback.spellId,
                        readback.skillLevel, readback.skillMastery, readback.power, readback.casterMemberIndex),
                    "Ascent save changed source Feather Fall duration or metadata");
            }
            require(!saved->heldInventoryItemActive
                && encodeInventoryItemForComparison(saved->heldInventoryItem) == heldBytes, "Saved held item changed");
            record("saved:" + name);
        };
        const auto choose = [&](EventDialogActionKind kind, uint32_t id)
        {
            const auto &dialog = screen().activeEventDialog();
            require(dialog.isActive, "No actual dialog for requested action");
            const auto found = std::find_if(dialog.actions.begin(), dialog.actions.end(), [&](const auto &action)
            {
                return action.kind == kind && action.id == id && action.enabled && !action.textOnly;
            });
            require(found != dialog.actions.end(), "Actual dialog action absent: " + std::to_string(id));
            screen().eventDialogSelectionIndex() = size_t(found - dialog.actions.begin());
            screen().executeActiveDialogAction();
        };
        const auto add = [&](const std::string &name, std::function<void()> action, bool screenshot = true)
        {
            if (reserveRoute)
            {
                screenshot = name == "first-map-onload-published" || name == "source-hint-775"
                    || name == "all-companions-returned";
            }
            steps.push_back({name, std::move(action), screenshot, {}});
        };
        struct DoorPointerTarget
        {
            size_t modelIndex = 0;
            size_t faceIndex = 0;
            uint16_t eventId = 0;
            uint32_t houseId = 0;
            bx::Vec3 center = {0.0f, 0.0f, 0.0f};
            int key = 0;
        };
        std::optional<DoorPointerTarget> doorTarget;
        const auto placeAtHouseDoor = [&](uint16_t eventId, uint32_t houseId)
        {
            screen().closeActiveEventDialog();
            const auto &selected = game.m_gameDataLoader.getSelectedMap();
            require(selected && selected->outdoorMapData, "House door has no outdoor geometry");
            const auto &models = selected->outdoorMapData->bmodels;
            for (size_t modelIndex = 0; modelIndex < models.size(); ++modelIndex)
            {
                const auto &model = models[modelIndex];
                for (size_t faceIndex = 0; faceIndex < model.faces.size(); ++faceIndex)
                {
                    const auto &face = model.faces[faceIndex];
                    if (face.cogTriggeredNumber != eventId || face.vertexIndices.empty()
                        || (face.attributes & faceAttributeBit(FaceAttribute::Clickable)) == 0
                        || (face.attributes & (faceAttributeBit(FaceAttribute::Invisible)
                            | faceAttributeBit(FaceAttribute::HasHint))) != 0) continue;
                    const float normalX = float(face.planeNormalX);
                    const float normalY = float(face.planeNormalY);
                    const float length = std::hypot(normalX, normalY);
                    if (length == 0.0f) continue;
                    bx::Vec3 center = {0.0f, 0.0f, 0.0f};
                    float bottom = float(model.vertices.at(face.vertexIndices.front()).z);
                    for (uint16_t index : face.vertexIndices)
                    {
                        const auto &vertex = model.vertices.at(index);
                        center.x += float(vertex.x);
                        center.y += float(vertex.y);
                        center.z += float(vertex.z);
                        bottom = std::min(bottom, float(vertex.z));
                    }
                    const float count = float(face.vertexIndices.size());
                    center = {center.x / count, center.y / count, center.z / count};
                    const float x = center.x + normalX / length * 256.0f;
                    const float y = center.y + normalY / length * 256.0f;
                    const int key = int(std::ceil(x / 500.0f) + std::ceil(y / 500.0f));
                    if (chaosRoute && usedDoorKeys.contains(key)) continue;
                    scene().partyRuntime().teleportTo(x, y, bottom + 1.0f);
                    const auto &pose = scene().partyRuntime().movementState();
                    const float dx = center.x - pose.x;
                    const float dy = center.y - pose.y;
                    game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                        std::atan2(center.z - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                            std::hypot(dx, dy)));
                    doorTarget = DoorPointerTarget{modelIndex, faceIndex, eventId, houseId, center, key};
                    if (chaosRoute) record("controlled-door-navigation-key:" + std::to_string(key));
                    return;
                }
            }
            throw std::runtime_error("Source house door face is absent for event " + std::to_string(eventId));
        };
        const auto clickWorldPixel = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = renderWidth;
            input.screenHeight = renderHeight;
            input.pointerX = x;
            input.pointerY = y;
            input.rightMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            input.leftMouseButton = {true, true, false};
            game.m_gameSession.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(input, 0.0f);
            input.leftMouseButton = {};
            input.rightMouseButton = {false, false, true};
            game.m_gameSession.updateGameplay(input, 0.0f);
        };
        const auto clickHouseDoor = [&]()
        {
            require(doorTarget.has_value(), "House door pose was not prepared");
            auto &world = scene().worldRuntime();
            const auto camera = world.buildWorldPickRequest(
                {float(renderWidth) * 0.5f, float(renderHeight) * 0.5f, renderWidth, renderHeight, true});
            float matrix[16];
            bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
            const auto &model = world.mapData()->bmodels.at(doorTarget->modelIndex);
            const auto &face = model.faces.at(doorTarget->faceIndex);
            std::vector<bx::Vec3> points{doorTarget->center};
            for (uint16_t index : face.vertexIndices)
            {
                if (points.size() == 9) break;
                const auto &vertex = model.vertices.at(index);
                points.push_back(bx::add(doorTarget->center,
                    bx::mul(bx::sub({float(vertex.x), float(vertex.y), float(vertex.z)}, doorTarget->center), 0.65f)));
            }
            std::optional<std::array<float, 2>> pixel;
            for (const bx::Vec3 &point : points)
            {
                const bx::Vec3 projected = bx::mulH(point, matrix);
                const float x = (projected.x + 1.0f) * float(renderWidth) * 0.5f;
                const float y = (1.0f - projected.y) * float(renderHeight) * 0.5f;
                if (x < 0.0f || y < 0.0f || x >= renderWidth || y >= renderHeight) continue;
                const auto request = world.buildWorldPickRequest({x, y, renderWidth, renderHeight, true});
                const auto hit = world.pickMouseInteractionTarget(request);
                if (hit.eventTarget && hit.eventTarget->targetKind == GameplayWorldEventTargetKind::Surface
                    && hit.eventTarget->targetIndex == doorTarget->modelIndex
                    && hit.eventTarget->secondaryIndex == doorTarget->faceIndex
                    && hit.eventTarget->triggeredEventId == doorTarget->eventId
                    && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse))
                {
                    pixel = std::array<float, 2>{x, y};
                    break;
                }
            }
            require(pixel.has_value(), "Real pointer did not expose the source house door event "
                + std::to_string(doorTarget->eventId));
            const float x = (*pixel)[0];
            const float y = (*pixel)[1];
            pointerProbes << "{\"phase\":\"house-door\",\"event\":" << doorTarget->eventId
                << ",\"house\":" << doorTarget->houseId << ",\"model\":" << doorTarget->modelIndex
                << ",\"face\":" << doorTarget->faceIndex << ",\"pixel\":[" << x << ',' << y << "]}\n";
            pointerProbes.flush();
            if (chaosRoute)
            {
                const auto &pose = scene().partyRuntime().movementState();
                const int key = int(std::ceil(pose.x / 500.0f) + std::ceil(pose.y / 500.0f));
                require(key == doorTarget->key && usedDoorKeys.insert(key).second,
                    "Door navigation reused or changed its source key");
            }
            clickWorldPixel(x, y);
            if (chaosRoute) return; // Native event5 publication occurs at the next App safe point.
            const auto &dialog = screen().activeEventDialog();
            const auto &context = events().pendingDialogueContext;
            const uint32_t resident = uint32_t(773 + doorTarget->eventId - 50);
            // Ordinary houses with one resident and no services open that resident directly.
            require(dialog.isActive && context && context->hostHouseId == doorTarget->houseId
                && ((dialog.isHouseDialog && dialog.sourceId == doorTarget->houseId)
                    || (!dialog.isHouseDialog && dialog.sourceId == resident)),
                "Actual door did not open its source house dialog");
        };
        const auto quest = [&](const std::string &suffix)
        {
            const auto found = events().namedGlobalVars.find(std::string(QuestPrefix) + suffix);
            return found == events().namedGlobalVars.end() ? 0 : found->second;
        };
        const auto tapKey = [&](SDL_Keycode key, SDL_Scancode scancode)
        {
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.key = key;
            event.key.scancode = scancode;
            event.key.down = true;
            game.handleSdlEvent(event);
            event.type = SDL_EVENT_KEY_UP;
            event.key.down = false;
            game.handleSdlEvent(event);
        };
        const auto verifyChaosProgress = [&](int count, bool caught, bool finished)
        {
            require(quest("RiddlesAnswered") == count && answeredRiddles.size() == size_t(count),
                "Source riddle count differs from the actual submitted answers");
            int solved = 0;
            for (int id = 1; id <= 11; ++id)
            {
                const bool present = quest("RiddleSolved." + std::to_string(id)) != 0;
                require(present == answeredRiddles.contains(id), "Source solved set gained or lost a riddle");
                solved += present ? 1 : 0;
            }
            require(solved == count && quest("PendingRiddle") == 0, "Riddle continuation was not consumed");
            require(quest("CoughtChaos") == int(caught) && quest("QuestFinished") == int(finished)
                && quest("GotEndCard") == int(finished), "Chaos capture or completion flags differ from the phase");
            const Party::Snapshot actual = scene().party().snapshot();
            Party::Snapshot expected = beforeParty;
            if (finished)
            {
                // Expected evidence only; never restore this comparison snapshot into the running Party.
                for (Character &member : expected.members) member.experience += 100000;
            }
            checkCustody(expected, actual, !ascentEndingRoute);
            require(actual.members.size() == 5 && actual.detachedMembers.empty(), "Chaos route changed Party custody");
            const auto isChaos = [](const auto &follower) { return follower.npcId == 1092; };
            const int expectedFollowers = caught && !finished ? 1 : 0;
            require(std::count_if(actual.hiredNpcFollowers.begin(), actual.hiredNpcFollowers.end(), isChaos)
                    == expectedFollowers
                && std::count_if(events().hiredNpcFollowers.begin(), events().hiredNpcFollowers.end(), isChaos)
                    == expectedFollowers, "Real Party/runtime Chaos follower does not match the phase");
            for (uint32_t bit : {1713u, 1714u, 1715u})
            {
                require(finished ? !scene().party().hasQuestBit(bit)
                    : scene().party().hasQuestBit(bit) == beforeParty.questBits.contains(bit),
                    "Breach ending did not preserve/clear its source quest bits");
            }
            for (int hint = 1; hint <= 4; ++hint)
                require(quest("GotFQHint" + std::to_string(hint)) == 1, "Previously earned source hint changed");
        };
        const auto selectChaos = [&]()
        {
            const auto &context = events().pendingDialogueContext;
            require(context && context->hostHouseId == 712 && screen().activeEventDialog().isActive,
                "Real event5 did not publish its House712 encounter");
            if (screen().activeEventDialog().isHouseDialog || screen().activeEventDialog().sourceId != 1092)
                choose(EventDialogActionKind::HouseResident, 1092);
            require(screen().activeEventDialog().sourceId == 1092
                && !screen().activeEventDialog().sourceActorIndex, "House712 did not select actual NPC1092");
        };
        const auto answerRiddle = [&](int ordinal)
        {
            require(events().pendingInputPrompt.has_value(), "Actual riddle text input is missing");
            const auto prompt = *events().pendingInputPrompt;
            const int id = quest("PendingRiddle");
            require(id >= 1 && id <= 11 && !answeredRiddles.contains(id), "Random riddle was already solved");
            const auto answer = game.m_gameDataLoader.getNpcDialogTable().getText(2217 + id * 2);
            require(prompt.eventId == 1790 && !prompt.trimWhitespace && answer && !answer->empty()
                && prompt.answers.size() == 1 && prompt.answers.front() == *answer,
                "Actual prompt answer/policy differs from the imported source riddle");
            SDL_Event text = {};
            text.type = SDL_EVENT_TEXT_INPUT;
            text.text.text = answer->c_str();
            game.handleSdlEvent(text);
            tapKey(SDLK_RETURN, SDL_SCANCODE_RETURN);
            require(!events().pendingInputPrompt, "SDL Return did not consume the real input prompt");
            answeredRiddles.insert(id);
            verifyChaosProgress(ordinal, false, false);
            require(events().npcHouseOverrides.at(1092) == 0, "Answered Chaos did not leave the actual house");
            pointerProbes << "{\"phase\":\"source-riddle-answer\",\"riddle\":" << id
                << ",\"submitted\":" << std::quoted(*answer) << ",\"count\":" << ordinal << "}\n";
            pointerProbes.flush();
        };

        constexpr float AscentStepSeconds = 1.0f / 60.0f;
        const std::array<uint32_t, 12> towerEvents = {104, 105, 106, 107, 108, 109, 114, 115, 116, 126, 127, 128};
        const auto verifyAscentPopulation = [&]()
        {
            const size_t actual = scene().worldRuntime().mapActorCount();
            const std::array<int, 3> flags = {quest("BrFirstFloor"), quest("BrSecFloor"), quest("BrThirdFloor")};
            const size_t expected = originalActorCount + (flags[0] != 0 ? 9 : 0)
                + (flags[1] != 0 ? 12 : 0) + (flags[2] != 0 ? 5 : 0);
            if (actual == expected) return;
            failureDomain = "population-invariant";
            std::ostringstream message;
            message << "Tower population mismatch: actual=" << actual << "; expected=" << expected
                << "; session_map=" << game.m_gameSession.currentMapFileName()
                << "; event_map=" << events().mapFileName
                << "; flags=[" << flags[0] << ',' << flags[1] << ',' << flags[2] << ']';
            try
            {
                save("population-stop");
            }
            catch (const std::exception &saveFailure)
            {
                message << "; population-stop save failed: " << saveFailure.what();
            }
            throw std::runtime_error(message.str());
        };
        const auto advanceAscentPhysics = [&](GameplayInputFrame input)
        {
            require(!screen().activeEventDialog().isActive && game.m_screenManager.activeScreen() == nullptr,
                "Ascent movement reached a modal screen");
            input.screenWidth = renderWidth;
            input.screenHeight = renderHeight;
            auto &world = scene().worldRuntime();
            const double beforeClock = world.gameMinutes();
            world.updateWorldMovement(input, AscentStepSeconds, true);
            BreachAlvarWorldActorTestAccess::discardQueuedAi(world);
            world.updateWorld(AscentStepSeconds); // Real interpolation and geometry/collision refresh.
            BreachAlvarWorldActorTestAccess::discardQueuedAi(world);
            ascentSimulationSeconds += AscentStepSeconds;
            require(std::abs(world.gameMinutes() - beforeClock - double(AscentStepSeconds) * 0.5) < 0.00001,
                "Ascent did not advance the original half-game-minute per simulation-second clock");
            require(std::abs(world.gameMinutes() - ascentInitialGameMinutes - ascentSimulationSeconds * 0.5) < 0.001,
                "Ascent accumulated an implicit time jump");
            const auto &pose = scene().partyRuntime().movementState();
            require(!scene().partyRuntime().partyMovementState().flying, "Ascent unexpectedly used Fly");
            verifyAscentPopulation();
            const auto buff = scene().party().snapshot().partyBuffs[size_t(PartyBuffId::FeatherFall)];
            ascentMovement << std::setprecision(17) << "{\"stage\":" << std::quoted(stage)
                << ",\"frame\":" << frames << ",\"simulation_seconds\":" << ascentSimulationSeconds
                << ",\"game_minutes\":" << world.gameMinutes() << ",\"initial_game_minutes\":"
                << ascentInitialGameMinutes << ",\"xyz\":[" << pose.x << ',' << pose.y << ',' << pose.footZ
                << "],\"vz\":" << pose.verticalVelocity << ",\"airborne\":" << (pose.airborne ? "true" : "false")
                << ",\"support\":[" << int(pose.supportKind) << ',' << pose.supportBModelIndex << ','
                << pose.supportFaceIndex << "],\"actor_count\":" << world.mapActorCount()
                << ",\"flags\":[" << quest("BrFirstFloor") << ',' << quest("BrSecFloor") << ','
                << quest("BrThirdFloor") << "],\"feather_fall_game_seconds\":" << buff.remainingSeconds
                << ",\"waypoint\":" << ascentWaypointIndex << ",\"ai_queue_discarded\":true,\"mechanisms\":[";
            for (size_t index = 0; index < towerEvents.size(); ++index)
            {
                if (index) ascentMovement << ',';
                const auto &state = events().mechanisms.at(towerEvents[index]);
                ascentMovement << '[' << towerEvents[index] << ',' << state.state << ',' << state.timeSinceTriggeredMs
                    << ',' << state.currentDistance << ',' << (state.isMoving ? "true" : "false") << ']';
            }
            ascentMovement << "]}\n";
            ascentMovement.flush();
        };
        const auto addAscentNavigation = [&](const std::string &name,
            std::function<std::vector<bx::Vec3>()> points, float arrivalRadius = 24.0f)
        {
            steps.push_back({name, [&, points]()
            {
                ascentWaypoints = points();
                ascentWaypointIndex = 0;
                require(!ascentWaypoints.empty(), "Ascent has no real geometry waypoints");
            }, false, [&, name, arrivalRadius]()
            {
                if (ascentEndingRoute && name == "top-floor-to-orb81"
                    && toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm")
                {
                    failureDomain = "source-ending-invariant";
                    require(ascentLanded, "Original floor81 returned before a verified TopFloor landing");
                    verifyChaosProgress(5, true, true);
                    ascentOriginalFloorReturned = true;
                    record("source81-original-floor-return");
                    trace << "{\"stage\":" << std::quoted(stage)
                        << ",\"source81_original_floor_return\":true,\"explicit_orb_click\":false}\n";
                    trace.flush();
                    return true;
                }
                failureDomain = "navigation-or-collision-stall";
                if (ascentSimulationSeconds - ascentStepStartSeconds >= 35.0)
                {
                    save("navigation-stop");
                    throw std::runtime_error("Bounded normal movement stalled; inspect movement.jsonl at " + stage);
                }
                const auto &pose = scene().partyRuntime().movementState();
                while (ascentWaypointIndex < ascentWaypoints.size())
                {
                    const auto &point = ascentWaypoints[ascentWaypointIndex];
                    if (std::hypot(point.x - pose.x, point.y - pose.y) > arrivalRadius) break;
                    require(std::abs(pose.footZ - point.z) < 128.0f,
                        "Navigation reached the XY waypoint on a different level");
                    ++ascentWaypointIndex;
                }
                if (ascentWaypointIndex == ascentWaypoints.size()) return true;
                const auto &point = ascentWaypoints[ascentWaypointIndex];
                const float dx = point.x - pose.x;
                const float dy = point.y - pose.y;
                game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
                GameplayInputFrame input;
                input.actions[keyboardActionIndex(KeyboardAction::Forward)].held = true;
                input.keyboardHeld[SDL_SCANCODE_W] = true;
                input.keyboardHeld[SDL_SCANCODE_LSHIFT] = scene().partyRuntime().partyMovementState().running;
                // Keep each native 1/128-second collision step above its 0.5-unit movement threshold.
                input.movementSpeedScale = std::clamp(std::hypot(dx, dy) / 64.0f, 0.25f, 1.0f);
                advanceAscentPhysics(input);
                return false;
            }});
        };
        const auto aimAscentMechanism = [&](uint32_t eventId)
        {
            const auto &definition = events().outdoorModelMechanisms.at(eventId);
            auto &world = scene().worldRuntime();
            std::vector<size_t> faces;
            world.collectOutdoorFaceCandidates(-3500.0f, -3400.0f, 3000.0f, 3000.0f, faces);
            const auto &pose = scene().partyRuntime().movementState();
            const bx::Vec3 eye = {pose.x, pose.y, pose.footZ + game.m_outdoorGameView.m_cameraEyeHeight};
            float distance = 1.0e20f;
            std::optional<bx::Vec3> selected;
            for (size_t index : faces)
            {
                const auto *pFace = world.outdoorFace(index);
                if (!pFace || pFace->bModelIndex != definition.bmodelIndex || pFace->vertices.empty()) continue;
                bx::Vec3 center = {0.0f, 0.0f, 0.0f};
                for (const auto &vertex : pFace->vertices) center = bx::add(center, vertex);
                center = bx::mul(center, 1.0f / float(pFace->vertices.size()));
                const bx::Vec3 delta = bx::sub(center, eye);
                const float candidate = bx::dot(delta, delta);
                if (candidate < distance)
                {
                    distance = candidate;
                    selected = center;
                }
            }
            require(selected.has_value(), "Live mechanism geometry is absent for event " + std::to_string(eventId));
            game.m_outdoorGameView.setCameraAngles(std::atan2(selected->y - eye.y, selected->x - eye.x),
                std::atan2(selected->z - eye.z, std::hypot(selected->x - eye.x, selected->y - eye.y)));
        };
        const auto clickAscentMechanism = [&](uint32_t eventId)
        {
            failureDomain = "world-pointer-or-navigation-integration";
            auto &world = scene().worldRuntime();
            const auto &definition = events().outdoorModelMechanisms.at(eventId);
            const auto camera = world.buildWorldPickRequest(
                {float(renderWidth) * 0.5f, float(renderHeight) * 0.5f, renderWidth, renderHeight, true});
            float matrix[16];
            bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
            std::vector<size_t> faces;
            world.collectOutdoorFaceCandidates(-3500.0f, -3400.0f, 3000.0f, 3000.0f, faces);
            unsigned probes = 0;
            for (size_t index : faces)
            {
                const auto *pFace = world.outdoorFace(index);
                if (!pFace || pFace->bModelIndex != definition.bmodelIndex || pFace->vertices.empty()) continue;
                bx::Vec3 center = {0.0f, 0.0f, 0.0f};
                for (const auto &vertex : pFace->vertices) center = bx::add(center, vertex);
                center = bx::mul(center, 1.0f / float(pFace->vertices.size()));
                std::vector<bx::Vec3> points{center};
                for (const auto &vertex : pFace->vertices)
                    points.push_back(bx::add(center, bx::mul(bx::sub(vertex, center), 0.65f)));
                for (const auto &point : points)
                {
                    require(++probes <= 128, "Bounded mechanism pointer probes exhausted");
                    const bx::Vec3 projected = bx::mulH(point, matrix);
                    const float x = (projected.x + 1.0f) * float(renderWidth) * 0.5f;
                    const float y = (1.0f - projected.y) * float(renderHeight) * 0.5f;
                    if (x < 0 || y < 0 || x >= renderWidth || y >= renderHeight) continue;
                    const auto hit = world.pickMouseInteractionTarget(
                        world.buildWorldPickRequest({x, y, renderWidth, renderHeight, true}));
                    pointerProbes << "{\"phase\":\"ascent-pointer-probe\",\"frame\":" << frames
                        << ",\"wanted_event\":" << eventId << ",\"ordinal\":" << probes
                        << ",\"pixel\":[" << x << ',' << y << "],\"hit_kind\":" << int(hit.kind);
                    if (hit.eventTarget)
                        pointerProbes << ",\"target_kind\":" << int(hit.eventTarget->targetKind)
                            << ",\"model\":" << hit.eventTarget->targetIndex
                            << ",\"face_or_event\":" << hit.eventTarget->secondaryIndex
                            << ",\"event_id\":" << hit.eventTarget->triggeredEventId
                            << ",\"distance\":" << hit.eventTarget->distance;
                    if (hit.actor) pointerProbes << ",\"occluding_actor\":" << hit.actor->actorIndex;
                    pointerProbes << "}\n";
                    pointerProbes.flush();
                    if (!hit.eventTarget || hit.eventTarget->targetKind != GameplayWorldEventTargetKind::Surface
                        || hit.eventTarget->targetIndex != definition.bmodelIndex
                        || hit.eventTarget->triggeredEventId != eventId
                        || !world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse)) continue;
                    pointerProbes << "{\"phase\":\"ascent-mechanism\",\"event_id\":" << eventId
                        << ",\"model\":" << hit.eventTarget->targetIndex
                        << ",\"face\":" << hit.eventTarget->secondaryIndex << ",\"pixel\":[" << x << ',' << y
                        << "],\"distance\":" << hit.eventTarget->distance << "}\n";
                    pointerProbes.flush();
                    clickWorldPixel(x, y);
                    return;
                }
            }
            throw std::runtime_error("Real pointer cannot reach tower event " + std::to_string(eventId));
        };
        const auto addAscentDoor = [&](uint32_t eventId, bool screenshot)
        {
            add("aim-source" + std::to_string(eventId), [&, eventId]() { aimAscentMechanism(eventId); }, false);
            add("click-source" + std::to_string(eventId), [&, eventId]()
            {
                clickAscentMechanism(eventId);
                require(events().mechanisms.at(eventId).isMoving,
                    "Real source door did not start native interpolation");
                verifyAscentPopulation();
            }, false);
            steps.push_back({"source" + std::to_string(eventId) + "-opened", []() {}, screenshot, [&, eventId]()
            {
                require(ascentSimulationSeconds - ascentStepStartSeconds < 4.0,
                    "Real source door failed to finish its two-second interpolation");
                advanceAscentPhysics({});
                const auto &state = events().mechanisms.at(eventId);
                return !state.isMoving && state.currentDistance >= 0.999f;
            }});
        };

        if (ascentRoute)
        {
            if (!ascentTopRoute)
            {
                if (!ascentResumeRoute)
                {
                    addAscentNavigation("subtower-first-lap", [&]()
                    {
                        std::vector<bx::Vec3> points = ascentLowerStairs;
                        points.push_back({-1862.0f, -1363.0f, 1991.0f});
                        points.push_back({-1950.0f, -1211.0f, 1991.0f});
                        points.push_back({-1850.0f, -1200.0f, 1991.0f});
                        return points;
                    });
                    addAscentDoor(108, false);
                    addAscentNavigation("subtower-second-lap", [&]()
                    {
                        std::vector<bx::Vec3> points = {{-2084.0f, -1138.0f, 1991.0f}, {-2200.0f, -910.0f, 1991.0f}};
                        points.insert(points.end(), ascentUpperStairs.begin(), ascentUpperStairs.end());
                        points.push_back({-1862.0f, -1363.0f, 3127.0f});
                        points.push_back({-1950.0f, -1211.0f, 3127.0f});
                        points.push_back({-1850.0f, -1200.0f, 3127.0f});
                        return points;
                    });
                    addAscentDoor(107, false); // Same source family as 108: must not summon its twelve actors twice.
                    add("third-floor-arrived", [&]() { save("third-floor-arrived"); }, false);
                }
                addAscentNavigation("museum-south-to-source114", []()
                {
                    return std::vector<bx::Vec3>{{-1850, -970, 3127}, {-1800, -900, 3127},
                        {-1750, -900, 3127}, {-1750, -875, 3127}, {-1700, -875, 3127},
                        {-1700, -850, 3127}, {-1650, -850, 3127}, {-1650, -825, 3127},
                        {-1625, -825, 3127}, {-1625, -800, 3127}, {-900, -800, 3127}};
                }, 8.0f);
                addAscentDoor(114, false);
                addAscentNavigation("museum-south-to-source115", []()
                {
                    return std::vector<bx::Vec3>{{-250, -800, 3127}, {350, -800, 3127}};
                }, 8.0f);
                addAscentDoor(115, false);
                addAscentNavigation("museum-east-to-source116", []()
                {
                    return std::vector<bx::Vec3>{{550, -800, 3127}, {550, -775, 3127},
                        {575, -775, 3127}, {575, -725, 3127}, {600, -725, 3127}, {600, -350, 3127},
                        {625, -350, 3127}, {625, -300, 3127}, {650, -300, 3127}, {650, -250, 3127},
                        {675, -250, 3127}, {675, -200, 3127}, {700, -200, 3127}, {700, -175, 3127},
                        {725, -175, 3127}, {725, -125, 3127}, {750, -125, 3127}, {750, -75, 3127},
                        {775, -75, 3127}, {775, -25, 3127}, {800, -25, 3127}, {800, 25, 3127},
                        {775, 25, 3127}, {775, 75, 3127}, {750, 75, 3127}, {750, 125, 3127},
                        {725, 125, 3127}, {725, 150, 3127}};
                }, 8.0f);
                addAscentDoor(116, false);
                addAscentNavigation("museum-north-to-source106", []()
                {
                    return std::vector<bx::Vec3>{{725, 400, 3127}, {700, 400, 3127}, {700, 450, 3127},
                        {550, 450, 3127}, {550, 475, 3127}, {525, 475, 3127}, {525, 525, 3127},
                        {500, 525, 3127}, {500, 575, 3127}, {475, 575, 3127}, {475, 625, 3127},
                        {450, 625, 3127}, {450, 650, 3127}, {425, 650, 3127}, {425, 675, 3127},
                        {400, 675, 3127}, {400, 700, 3127}, {-525, 700, 3127}, {-525, 775, 3127},
                        {-550, 775, 3127}, {-550, 850, 3127}, {-575, 850, 3127}, {-575, 875, 3127},
                        {-600, 875, 3127}, {-600, 900, 3127}, {-650, 900, 3127}, {-650, 925, 3127},
                        {-1400, 925, 3127}};
                }, 8.0f);
                addAscentDoor(106, true);
                addAscentNavigation("source128-launch-radius", []()
                {
                    return std::vector<bx::Vec3>{{-1630, 940, 3127}, {-1850, 1060, 3127}, {-1962, 1137, 3127}};
                });
                steps.push_back({"source128-supported-ready", []() {}, false, [&]()
                {
                    failureDomain = "driver-launch-settle-precondition";
                    require(ascentSimulationSeconds - ascentStepStartSeconds < 2.0,
                        "Normal source128 approach did not settle onto its support");
                    advanceAscentPhysics({});
                    const auto &pose = scene().partyRuntime().movementState();
                    if (pose.airborne || std::abs(pose.verticalVelocity) > 0.01f) return false;
                    require(pose.supportKind == OutdoorSupportKind::BModelFace
                        && pose.footZ >= 3120.0f && pose.footZ < 3140.0f
                        && std::hypot(pose.x + 1962.0f, pose.y - 1137.0f) < 100.0f,
                        "Settled source128 approach has the wrong original floor or radius");
                    save("launch-arrived");
                    return true;
                }});
                add("aim-source128", [&]() { aimAscentMechanism(128); }, false);
                add("click-source128", [&]()
                {
                    const auto &pose = scene().partyRuntime().movementState();
                    require(pose.footZ >= 3120.0f && pose.footZ < 3140.0f
                        && std::hypot(pose.x + 1962.0f, pose.y - 1137.0f) < 100.0f,
                        "Normal navigation missed the source128 floor/radius precondition");
                    require(quest("BrThirdFloor") == 0, "Source128 was already completed before this launch");
                    ascentLaunchZ = pose.footZ;
                    ascentMaximumZ = pose.footZ;
                    ascentFeatherFallBefore = scene().party().snapshot().partyBuffs[size_t(PartyBuffId::FeatherFall)];
                    clickAscentMechanism(128);
                    const auto buff = scene().party().snapshot().partyBuffs[size_t(PartyBuffId::FeatherFall)];
                    require(buff.remainingSeconds >= std::max(240.0f, ascentFeatherFallBefore.remainingSeconds)
                        && std::tie(buff.spellId, buff.skillLevel, buff.skillMastery,
                            buff.power, buff.casterMemberIndex)
                            == std::tie(ascentFeatherFallBefore.spellId, ascentFeatherFallBefore.skillLevel,
                                ascentFeatherFallBefore.skillMastery, ascentFeatherFallBefore.power,
                                ascentFeatherFallBefore.casterMemberIndex),
                        "Source Feather Fall extension changed metadata");
                    require(quest("BrFirstFloor") == 1 && quest("BrSecFloor") == 1 && quest("BrThirdFloor") == 1,
                        "Real tower clicks did not publish all three source floor flags");
                    verifyAscentPopulation();
                    trace << "{\"stage\":" << std::quoted(stage) << ",\"source_jump\":[0,512,"
                        << std::sqrt(2.0f * 800.0f * (4410.0f - ascentLaunchZ))
                        << "],\"source_apex\":4410,\"launch_z\":" << ascentLaunchZ
                        << ",\"feather_fall_game_seconds\":" << buff.remainingSeconds << "}\n";
                    trace.flush();
                }, false);
                steps.push_back({"source128-physical-landing", []() {}, true, [&]()
                {
                    failureDomain = "source-jump-physics-invariant";
                    require(ascentSimulationSeconds - ascentStepStartSeconds < 5.0,
                        "Source jump did not land on TopFloor");
                    advanceAscentPhysics({});
                    const auto &pose = scene().partyRuntime().movementState();
                    ascentAirborneSeen = ascentAirborneSeen || pose.airborne;
                    ascentMaximumZ = std::max(ascentMaximumZ, pose.footZ);
                    const float physicsStepRise = std::sqrt(2.0f * 1280.0f * (4410.0f - ascentLaunchZ)) / 128.0f;
                    require(ascentMaximumZ <= 4411.0f + physicsStepRise,
                        "Source jump exceeded its original apex by more than one native physics step");
                    if (pose.airborne || pose.footZ < 4300.0f) return false;
                    require(ascentAirborneSeen && pose.supportKind == OutdoorSupportKind::BModelFace
                        && std::abs(pose.footZ - 4410.0f) < 3.0f
                        && scene().worldRuntime().mapData()->bmodels.at(pose.supportBModelIndex).name == "TopFloor",
                        "Source jump did not physically land on the original TopFloor support");
                    ascentLanded = true;
                    save("top-arrived");
                    return true;
                }});
            }
            else
                add("top-cold-restored", [&]() { save("top-cold-restored"); }, false);
            addAscentNavigation("top-floor-to-orb81", []()
            {
                return std::vector<bx::Vec3>{{-1765, 1025, 4410}, {-1568, 914, 4470}, {-1371, 802, 4501},
                    {-1174, 690, 4501}, {-978, 579, 4501}, {-781, 467, 4501}, {-584, 355, 4501},
                    {-387, 243, 4524}, {-190, 132, 4641}, {-200, 20, 4641}};
            });
            add("aim-orb81", [&]()
            {
                if (ascentOriginalFloorReturned)
                {
                    stage = "source81-floor-return-skip-orb-aim";
                    return;
                }
                const auto &entity = scene().worldRuntime().mapData()->entities.at(returnEntityIndex);
                const auto &pose = scene().partyRuntime().movementState();
                const float dx = float(entity.x) - pose.x;
                const float dy = float(entity.y) - pose.y;
                game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                    std::atan2(float(entity.z) + 16.0f - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight,
                        std::hypot(dx, dy)));
            }, false);
            add("orb81-reached", [&]()
            {
                if (ascentOriginalFloorReturned)
                {
                    stage = "source81-floor-returned";
                    verifyChaosProgress(5, true, true);
                    save("source81-floor-returned");
                    return;
                }
                failureDomain = "orb-pointer-or-navigation-integration";
                auto &world = scene().worldRuntime();
                const auto &pose = scene().partyRuntime().movementState();
                require(ascentLanded && pose.supportKind == OutdoorSupportKind::BModelFace && !pose.airborne
                    && std::abs(pose.footZ - 4641.0f) < 3.0f, "Normal ascent did not reach the orb's TopFloor");
                const auto &entity = world.mapData()->entities.at(returnEntityIndex);
                const auto camera = world.buildWorldPickRequest(
                    {float(renderWidth) * 0.5f, float(renderHeight) * 0.5f, renderWidth, renderHeight, true});
                float matrix[16];
                bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
                const bx::Vec3 projected = bx::mulH(
                    {float(entity.x), float(entity.y), float(entity.z) + 16.0f}, matrix);
                const float x = (projected.x + 1.0f) * float(renderWidth) * 0.5f;
                const float y = (1.0f - projected.y) * float(renderHeight) * 0.5f;
                require(x >= 0 && y >= 0 && x < renderWidth && y < renderHeight,
                    "Reached source orb projects outside the actual drawable");
                const auto hit = world.pickMouseInteractionTarget(
                    world.buildWorldPickRequest({x, y, renderWidth, renderHeight, true}));
                require(hit.eventTarget && hit.eventTarget->eventIdSecondary == 81
                    && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse),
                    "Actual pointer cannot reach the real OrMatt1/event81");
                const auto &target = *hit.eventTarget;
                const auto &billboards = game.m_outdoorGameView.m_outdoorDecorationBillboardSet;
                require((target.targetKind == GameplayWorldEventTargetKind::Entity
                        && target.targetIndex == returnEntityIndex)
                    || (target.targetKind == GameplayWorldEventTargetKind::Decoration && billboards
                        && target.targetIndex < billboards->billboards.size()
                        && billboards->billboards[target.targetIndex].entityIndex == returnEntityIndex),
                    "Orb pointer resolved a different source entity");
                checkCustody(beforeParty, scene().party().snapshot(), false);
                require(game.m_gameSession.breachAlvarCompanions() == originalReceipt
                    && quest("CoughtChaos") == int(ascentEndingRoute)
                    && quest("QuestFinished") == 0 && quest("GotEndCard") == 0,
                    "Ascent invented a companion/Chaos/ending transaction");
                verifyAscentPopulation();
                pointerProbes << "{\"phase\":\"ascent-orb81\",\"entity_index\":" << returnEntityIndex
                    << ",\"target_kind\":" << int(target.targetKind) << ",\"target_index\":" << target.targetIndex
                    << ",\"distance\":" << target.distance << ",\"source_return_requires_caught_chaos\":true}\n";
                pointerProbes.flush();
                ascentOrbPixel = {x, y};
                save(ascentEndingRoute ? "orb-arrived" : "complete");
            });
            if (ascentEndingRoute)
            {
                add("ascent-activate-orb81", [&]()
                {
                    if (ascentOriginalFloorReturned)
                    {
                        stage = "source81-floor-return-skip-orb-click";
                        verifyChaosProgress(5, true, true);
                        return;
                    }
                    verifyChaosProgress(5, true, false);
                    clickWorldPixel(ascentOrbPixel[0], ascentOrbPixel[1]);
                    ascentExplicitOrbClick = true;
                }, false);
                add("ascent-breach-endcard", [&]()
                {
                    require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm",
                        "Normal ascent did not commit the actual source81 return");
                    verifyChaosProgress(5, true, true);
                    require(dynamic_cast<WinGameScreen *>(game.m_screenManager.activeScreen()) != nullptr,
                        "Normal ascent return did not present its completion card");
                });
                add("ascent-dismiss-endcard", [&]() { tapKey(SDLK_RETURN, SDL_SCANCODE_RETURN); }, false);
                add("ascent-continued-gameplay", [&]()
                {
                    require(game.m_screenManager.activeScreen() == nullptr && endCardShows == 1,
                        "Normal ascent completion did not return to playable gameplay exactly once");
                    verifyChaosProgress(5, true, true);
                    save("complete");
                });
            }
        }
        else if (chaosRoute)
        {
            add("chaos-input-ready", [&]()
            {
                verifyChaosProgress(0, false, false);
                require(toLowerCopy(events().mapFileName) == "bralvar.odm",
                    "Chaos input source map context changed");
                require(scene().localEventProgram() && scene().localEventProgram()->hasEvent(5)
                    && scene().localEventProgram()->hasEvent(81),
                    "Controlled runtime lacks source5/81 production handlers");
            }, false);
            for (int ordinal = 1; ordinal <= 5; ++ordinal)
            {
                add("chaos-door-navigation-" + std::to_string(ordinal), [&]() { placeAtHouseDoor(5, 712); }, false);
                add("chaos-door-click-" + std::to_string(ordinal), clickHouseDoor, false);
                add(ordinal == 1 ? "first-riddle-input" : "riddle-input-" + std::to_string(ordinal), [&]()
                {
                    selectChaos();
                    choose(EventDialogActionKind::NpcTopic, 1790);
                    require(events().pendingInputPrompt.has_value(), "Source topic1790 did not open an input prompt");
                }, ordinal == 1);
                add("riddle-answer-" + std::to_string(ordinal), [&, ordinal]() { answerRiddle(ordinal); }, false);
            }
            add("capture-door-navigation", [&]() { placeAtHouseDoor(5, 712); }, false);
            add("capture-door-click", clickHouseDoor, false);
            add("capture-offer", [&]()
            {
                selectChaos();
                verifyChaosProgress(5, false, false);
                choose(EventDialogActionKind::NpcTopic, 1794);
            }, false);
            add("capture-request", [&]() { choose(EventDialogActionKind::NpcTopic, 1792); }, false);
            add("chaos-caught", [&]()
            {
                screen().closeActiveEventDialog();
                verifyChaosProgress(5, true, false);
                require(usedDoorKeys.size() == 6, "Actual search did not use six distinct source door keys");
                save("chaos-caught");
            });
            add("return-orb-navigation", [&]()
            {
                const auto *pMap = scene().worldRuntime().mapData();
                require(pMap != nullptr, "Return orb has no live outdoor data");
                for (size_t index = 0; index < pMap->entities.size(); ++index)
                {
                    const auto &entity = pMap->entities[index];
                    if (toLowerCopy(entity.name) != "ormatt1" || entity.scriptEventId() != 81) continue;
                    require(returnEntityIndex == GameplayInvalidWorldIndex, "Ambiguous source return orb");
                    returnEntityIndex = index;
                }
                require(returnEntityIndex != GameplayInvalidWorldIndex, "Real OrMatt1 event81 entity is absent");
                const auto &entity = pMap->entities[returnEntityIndex];
                // Source OrMatt1 is above the original TopFloor at Z4640; navigation only, no flight/stat fixture.
                scene().partyRuntime().teleportTo(float(entity.x) - 256.0f, float(entity.y), 4641.0f);
                const auto &pose = scene().partyRuntime().movementState();
                require(pose.supportKind == OutdoorSupportKind::BModelFace && !pose.airborne,
                    "Return navigation is not supported by the real TopFloor");
                const float targetZ = float(entity.z) + 16.0f;
                game.m_outdoorGameView.setCameraAngles(0.0f,
                    std::atan2(targetZ - pose.footZ - game.m_outdoorGameView.m_cameraEyeHeight, 256.0f));
                record("controlled-OrMatt1-navigation");
            }, false);
            add("return-event81-pointer", [&]()
            {
                require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "bralvar.odm",
                    "Return navigation triggered a transition before the pointer gesture");
                auto &world = scene().worldRuntime();
                const auto &entity = world.mapData()->entities.at(returnEntityIndex);
                const auto camera = world.buildWorldPickRequest(
                    {float(renderWidth) * 0.5f, float(renderHeight) * 0.5f, renderWidth, renderHeight, true});
                float matrix[16];
                bx::mtxMul(matrix, camera.viewMatrix.data(), camera.projectionMatrix.data());
                const bx::Vec3 point = {float(entity.x), float(entity.y), float(entity.z) + 16.0f};
                const bx::Vec3 projected = bx::mulH(point, matrix);
                const float x = (projected.x + 1.0f) * float(renderWidth) * 0.5f;
                const float y = (1.0f - projected.y) * float(renderHeight) * 0.5f;
                require(x >= 0.0f && y >= 0.0f && x < renderWidth && y < renderHeight,
                    "Actual return orb projects outside the drawable");
                const auto hit = world.pickMouseInteractionTarget(
                    world.buildWorldPickRequest({x, y, renderWidth, renderHeight, true}));
                require(hit.eventTarget && hit.eventTarget->eventIdSecondary == 81,
                    "Actual pointer did not pick the source event81 entity");
                const auto &target = *hit.eventTarget;
                const auto &billboards = game.m_outdoorGameView.m_outdoorDecorationBillboardSet;
                const bool sourceEntity = target.targetKind == GameplayWorldEventTargetKind::Entity
                    && target.targetIndex == returnEntityIndex;
                const bool sourceDecoration = target.targetKind == GameplayWorldEventTargetKind::Decoration
                    && billboards && target.targetIndex < billboards->billboards.size()
                    && billboards->billboards[target.targetIndex].entityIndex == returnEntityIndex;
                require((sourceEntity || sourceDecoration)
                    && world.canActivateWorldHit(hit, GameplayInteractionMethod::Mouse),
                    "Actual event81 pointer identity/range changed");
                pointerProbes << "{\"phase\":\"return-event81\",\"entity\":" << returnEntityIndex
                    << ",\"target_kind\":" << int(target.targetKind) << ",\"target_index\":" << target.targetIndex
                    << ",\"pixel\":[" << x << ',' << y << "],\"distance\":" << target.distance << "}\n";
                pointerProbes.flush();
                clickWorldPixel(x, y);
            }, false);
            add("breach-endcard", [&]()
            {
                require(toLowerCopy(game.m_gameSession.currentMapFileName()) == "breach.odm",
                    "App did not commit the actual source81 return");
                verifyChaosProgress(5, true, true);
                require(dynamic_cast<WinGameScreen *>(game.m_screenManager.activeScreen()) != nullptr,
                    "Breach arrival did not present its actual completion card");
            });
            add("dismiss-breach-endcard", [&]() { tapKey(SDLK_RETURN, SDL_SCANCODE_RETURN); }, false);
            add("continued-gameplay", [&]()
            {
                require(game.m_screenManager.activeScreen() == nullptr,
                    "Completion card did not return to playable gameplay");
                verifyChaosProgress(5, true, true);
                require(endCardShows == 1, "Breach completion card was missing or repeated");
                save("completed");
            });
        }
        else if (fresh)
        {
            add("first-map-onload-published", [&]() { verify(0, true); save("split"); });
            add("friend-position-fixture", [&]()
            {
                screen().closeActiveEventDialog();
                const auto *pActor = scene().worldRuntime().mapActorState(friendIndex());
                require(pActor != nullptr, "Active companion is hidden before pointer placement");
                const float x = pActor->preciseX - 320.0f;
                const float y = pActor->preciseY;
                scene().partyRuntime().teleportTo(x, y, pActor->preciseZ);
                const auto &position = scene().partyRuntime().movementState();
                require(std::abs(position.footZ - pActor->preciseZ) < 320.0f,
                    "Declared nearby party pose resolved to another floor");
                const float pitch = std::atan2(pActor->preciseZ + float(pActor->height) * 0.5f
                    - position.footZ - game.m_outdoorGameView.m_cameraEyeHeight, 320.0f);
                game.m_outdoorGameView.setCameraAngles(0.0f, pitch);
            });
            add("friend772-pointer-dialogue", [&]()
            {
                refreshKeyboardCandidates();
                const size_t target = friendIndex();
                const auto *pTarget = scene().worldRuntime().mapActorState(target);
                require(pTarget != nullptr, "Actual companion disappeared before pointer diagnostics");
                const auto &view = game.m_outdoorGameView;
                const auto &position = scene().partyRuntime().movementState();
                pointerProbes << std::setprecision(17)
                    << "{\"phase\":\"pose\",\"frame\":" << frames << ",\"target_index\":" << target
                    << ",\"actor_id\":" << pTarget->actorId << ",\"live_npc_id\":" << pTarget->npcId
                    << ",\"monster_id\":" << pTarget->monsterId << ",\"friendly\":"
                    << (!pTarget->hostileToParty ? "true" : "false")
                    << ",\"actor_xyz\":[" << pTarget->preciseX << ',' << pTarget->preciseY << ','
                    << pTarget->preciseZ << "],\"radius\":" << pTarget->radius << ",\"height\":" << pTarget->height
                    << ",\"party_xyz\":[" << position.x << ',' << position.y << ',' << position.footZ
                    << "],\"camera_xyz\":[" << view.m_cameraTargetX << ',' << view.m_cameraTargetY << ','
                    << view.m_cameraTargetZ << "],\"yaw\":" << view.cameraYawRadians()
                    << ",\"pitch\":" << view.cameraPitchRadians() << ",\"screen\":[" << renderWidth << ','
                    << renderHeight << "],\"billboard_candidates\":"
                    << view.m_keyboardInteractionBillboardCandidates.size() << "}\n";
                size_t targetCandidates = 0;
                size_t targetCandidateNpcMismatches = 0;
                size_t nearbyCandidates = 0;
                for (const auto &candidate : view.m_keyboardInteractionBillboardCandidates)
                {
                    const auto &hit = candidate.inspectHit;
                    const bool isTarget = hit.kind == "actor" && hit.runtimeActorIndex == target;
                    if (isTarget) ++targetCandidates;
                    if (isTarget && hit.npcId != 772) ++targetCandidateNpcMismatches;
                    // Keep diagnostic output bounded while recording every candidate for the target itself.
                    if (!isTarget && (hit.kind != "actor" || nearbyCandidates >= 12)) continue;
                    if (!isTarget) ++nearbyCandidates;
                    pointerProbes << "{\"phase\":\"candidate\",\"target\":" << (isTarget ? "true" : "false")
                        << ",\"runtime_index\":" << hit.runtimeActorIndex << ",\"preview_index\":" << hit.bModelIndex
                        << ",\"npc_id\":" << hit.npcId << ",\"name\":" << std::quoted(hit.name)
                        << ",\"camera_depth\":" << candidate.cameraDepth;
                    const auto *pLive = scene().worldRuntime().mapActorState(hit.runtimeActorIndex);
                    if (pLive != nullptr)
                        pointerProbes << ",\"live_actor_id\":" << pLive->actorId << ",\"live_npc_id\":" << pLive->npcId;
                    pointerProbes << ",\"sample_points\":[";
                    for (size_t index = 0; index < candidate.samplePointCount; ++index)
                    {
                        if (index != 0) pointerProbes << ',';
                        pointerProbes << '[' << candidate.samplePoints[index].x << ','
                            << candidate.samplePoints[index].y << ']';
                    }
                    pointerProbes << "]}\n";
                }
                pointerProbes.flush();
                require(targetCandidates != 0 && targetCandidateNpcMismatches == 0,
                    "Fresh production keyboard cache did not expose the actual NPC772 identity; see pointer-probes.jsonl");
                std::optional<std::array<float, 2>> pixel;
                unsigned probeCount = 0;
                unsigned targetHits = 0;
                unsigned targetNpcMismatches = 0;
                const auto testPixel = [&](float x, float y, const char *source)
                {
                    require(probeCount < 128, "Bounded pointer probe budget exhausted");
                    ++probeCount;
                    const auto request = scene().worldRuntime().buildWorldPickRequest(
                        {x, y, renderWidth, renderHeight, true});
                    const auto hit = scene().worldRuntime().pickMouseInteractionTarget(request);
                    pointerProbes << "{\"phase\":\"probe\",\"ordinal\":" << probeCount
                        << ",\"source\":" << std::quoted(source) << ",\"pixel\":[" << x << ',' << y
                        << "],\"has_ray\":" << (request.hasRay ? "true" : "false")
                        << ",\"ray_origin\":[" << request.rayOrigin.x << ',' << request.rayOrigin.y << ','
                        << request.rayOrigin.z << "],\"ray_direction\":[" << request.rayDirection.x << ','
                        << request.rayDirection.y << ',' << request.rayDirection.z
                        << "],\"has_hit\":" << (hit.hasHit ? "true" : "false")
                        << ",\"hit_kind\":" << int(hit.kind);
                    if (hit.actor)
                    {
                        pointerProbes << ",\"hit_actor_index\":" << hit.actor->actorIndex
                            << ",\"hit_npc_id\":" << hit.actor->npcId
                            << ",\"hit_name\":" << std::quoted(hit.actor->displayName)
                            << ",\"hit_distance\":" << hit.actor->distance;
                        const auto *pHitActor = scene().worldRuntime().mapActorState(hit.actor->actorIndex);
                        if (pHitActor != nullptr)
                            pointerProbes << ",\"hit_live_actor_id\":" << pHitActor->actorId
                                << ",\"hit_live_npc_id\":" << pHitActor->npcId;
                        pointerProbes << ",\"can_activate_mouse\":"
                            << (scene().worldRuntime().canActivateWorldHit(hit, GameplayInteractionMethod::Mouse)
                                ? "true" : "false");
                    }
                    if (hit.eventTarget)
                        pointerProbes << ",\"event_kind\":" << int(hit.eventTarget->targetKind)
                            << ",\"event_target_index\":" << hit.eventTarget->targetIndex
                            << ",\"event_secondary_index\":" << hit.eventTarget->secondaryIndex
                            << ",\"event_name\":" << std::quoted(hit.eventTarget->name)
                            << ",\"event_distance\":" << hit.eventTarget->distance;
                    if (hit.ground)
                        pointerProbes << ",\"ground_distance\":" << hit.ground->distance;
                    pointerProbes << "}\n";
                    pointerProbes.flush();
                    if (hit.kind == GameplayWorldHitKind::Actor && hit.actor && hit.actor->actorIndex == target)
                    {
                        ++targetHits;
                        if (hit.actor->npcId != 772) ++targetNpcMismatches;
                        else pixel = std::array<float, 2>{x, y};
                    }
                };
                // A strict real ray hit at the reticle is a valid mouse pixel, just like a sampled billboard point.
                testPixel(float(renderWidth) * 0.5f, float(renderHeight) * 0.5f, "reticle");
                for (const auto &candidate : view.m_keyboardInteractionBillboardCandidates)
                {
                    if (candidate.inspectHit.kind != "actor" || candidate.inspectHit.runtimeActorIndex != target)
                        continue;
                    for (size_t index = 0; index < candidate.samplePointCount && !pixel; ++index)
                        testPixel(candidate.samplePoints[index].x, candidate.samplePoints[index].y, "sample");
                    if (!pixel && candidate.samplePointCount >= 5)
                    {
                        // Bounded read-only opaque-pixel search inside the renderer's actual projected rectangle.
                        const auto first = candidate.samplePoints[1];
                        const auto last = candidate.samplePoints[4];
                        for (unsigned row = 1; row < 10 && !pixel; ++row)
                            for (unsigned col = 1; col < 10 && !pixel; ++col)
                                testPixel(first.x + (last.x - first.x) * float(col) / 10.0f,
                                    first.y + (last.y - first.y) * float(row) / 10.0f, "grid");
                    }
                    if (pixel) break;
                }
                pointerProbes << "{\"phase\":\"summary\",\"target_candidates\":" << targetCandidates
                    << ",\"probe_count\":" << probeCount << ",\"target_hits\":" << targetHits
                    << ",\"target_npc_mismatches\":" << targetNpcMismatches
                    << ",\"selected\":" << (pixel ? "true" : "false") << "}\n";
                pointerProbes.flush();
                require(pixel.has_value(), targetNpcMismatches != 0
                    ? "Actual companion was picked but hit lost NPC772 identity; see pointer-probes.jsonl"
                    : targetCandidates == 0
                        ? "Actual companion absent from renderer candidates; see pointer-probes.jsonl"
                        : "No target pixel passed actual picking; inspect pointer-probes.jsonl for occlusion");
                trace << "{\"pointer_npc\":772,\"actor_index\":" << target
                    << ",\"x\":" << (*pixel)[0] << ",\"y\":" << (*pixel)[1] << "}\n";
                trace.flush();
                GameplayInputFrame input;
                input.screenWidth = renderWidth;
                input.screenHeight = renderHeight;
                input.pointerX = (*pixel)[0];
                input.pointerY = (*pixel)[1];
                const auto recordPointerInput = [&](const char *phase)
                {
                    const auto &state = game.m_gameSession.gameplayScreenState();
                    const auto &mouse = state.gameplayMouseLookState();
                    const auto &dialog = screen().activeEventDialog();
                    pointerProbes << "{\"phase\":\"pointer-input\",\"step\":" << std::quoted(phase)
                        << ",\"right_held\":" << (input.rightMouseButton.held ? "true" : "false")
                        << ",\"left_held\":" << (input.leftMouseButton.held ? "true" : "false")
                        << ",\"mouse_look\":" << (mouse.mouseLookActive ? "true" : "false")
                        << ",\"cursor_mode\":" << (mouse.cursorModeActive ? "true" : "false")
                        << ",\"world_click_armed\":"
                        << (state.worldInteractionInputState().inspectMouseActivateLatch ? "true" : "false")
                        << ",\"last_activation\":" << std::quoted(events().lastActivationResult.value_or(""))
                        << ",\"dialog_active\":" << (dialog.isActive ? "true" : "false")
                        << ",\"dialog_source\":" << dialog.sourceId;
                    if (dialog.sourceActorIndex)
                        pointerProbes << ",\"dialog_actor_index\":" << *dialog.sourceActorIndex;
                    pointerProbes << "}\n";
                    pointerProbes.flush();
                };
                // Modern controls use held right mouse for the normal cursor-mode click gesture.
                input.rightMouseButton = {true, true, false};
                game.m_gameSession.updateGameplay(input, 0.0f);
                recordPointerInput("cursor-enter");
                require(game.m_gameSession.gameplayScreenState().gameplayMouseLookState().cursorModeActive,
                    "Real right-button input did not enter cursor mode");
                input.rightMouseButton.pressed = false;
                input.leftMouseButton = {true, true, false};
                game.m_gameSession.updateGameplay(input, 0.0f);
                recordPointerInput("left-down");
                input.leftMouseButton = {false, false, true};
                game.m_gameSession.updateGameplay(input, 0.0f);
                recordPointerInput("left-up");
                input.leftMouseButton = {};
                input.rightMouseButton = {false, false, true};
                game.m_gameSession.updateGameplay(input, 0.0f);
                recordPointerInput("cursor-release");
                require(screen().activeEventDialog().sourceId == 772
                    && screen().activeEventDialog().sourceActorIndex == target,
                    "Real pointer did not open the spawned NPC772 dialog");
            });
            for (uint32_t npc = 772; npc <= 775; ++npc)
            {
                if (npc != 772)
                {
                    if (doorRoute)
                    {
                        add("door-position-" + std::to_string(npc), [&, npc]()
                        {
                            placeAtHouseDoor(uint16_t(50 + npc - 773), profile().houseIds[npc - 772]);
                        }, false);
                        add("house-door-" + std::to_string(npc), clickHouseDoor);
                    }
                    else add("house-entry-fixture-" + std::to_string(npc), [&, npc]()
                    {
                        screen().closeActiveEventDialog();
                        const uint32_t house = profile().houseIds[npc - 772];
                        const auto *pHouse = game.m_gameDataLoader.getHouseTable().get(house);
                        require(pHouse != nullptr, "Imported source house unavailable");
                        const auto residents = collectSelectableResidentNpcIds(*pHouse,
                            game.m_gameDataLoader.getNpcDialogTable(), events());
                        require(std::find(residents.begin(), residents.end(), npc) != residents.end(),
                            "Expected companion is not an actual house resident");
                        EventRuntimeState::PendingDialogueContext context;
                        context.kind = DialogueContextKind::HouseService;
                        context.sourceId = context.hostHouseId = house;
                        events().dialogueState.hostHouseId = house;
                        events().pendingDialogueContext = context;
                        screen().presentPendingEventDialog(events().messages.size(), true);
                    });
                    add("resident-selected-" + std::to_string(npc), [&, npc]()
                    {
                        if (screen().activeEventDialog().isHouseDialog || screen().activeEventDialog().sourceId != npc)
                            choose(EventDialogActionKind::HouseResident, npc);
                        require(screen().activeEventDialog().sourceId == npc
                            && !screen().activeEventDialog().sourceActorIndex, "House selected wrong NPC context");
                    });
                }
                add("source-hint-" + std::to_string(npc), [&, npc]()
                {
                    require(screen().activeEventDialog().sourceId == npc, "Hint actor context changed");
                    choose(EventDialogActionKind::NpcTopic, profile().hintTopicId);
                    const std::string key = std::string(QuestPrefix) + "HintByNPC." + std::to_string(npc);
                    require(events().namedGlobalVars.at(key) == int32_t(npc - 771), "Source hint order changed");
                    if (reserveRoute)
                    {
                        checkReserveCustody(*preservationBaseline, scene().party().snapshot());
                        if (npc - 772 >= reserveRetainedCount)
                        {
                            const auto &texts = game.m_gameDataLoader.getNpcDialogTable();
                            const uint32_t placeIndex = npc - 771;
                            const uint32_t greetingId = placeIndex % 2 == 1 ? 2245 : 2246;
                            const uint32_t placeId = 2246 + placeIndex;
                            const uint32_t hintId = 2250 + placeIndex;
                            const auto greeting = texts.getText(greetingId);
                            const auto place = texts.getText(placeId);
                            const auto hint = texts.getText(hintId);
                            require(greeting && place && hint, "Imported source stranger hint texts are absent");
                            const std::string message = *greeting + "\n" + *place + "\n" + *hint;
                            require(std::find(events().messages.begin(), events().messages.end(), message)
                                != events().messages.end(), "Generated NPC did not use its source stranger hint text");
                            trace << "{\"stage\":" << std::quoted(stage)
                                << ",\"source_message_text_ids\":[" << greetingId << ',' << placeId
                                << ',' << hintId << ']'
                                << ",\"stranger_greeting_verified\":true}\n";
                            trace.flush();
                            std::cout << "reserve NPC" << npc << " source stranger greeting" << greetingId
                                << " verified\n";
                        }
                    }
                    else checkCustody(beforeParty, scene().party().snapshot(), true);
                });
                add("join-request-" + std::to_string(npc), [&, npc]()
                {
                    choose(EventDialogActionKind::NpcTopic, profile().initialTopicId);
                    require(events().pendingBreachAlvarCompanionRequest.has_value(),
                        "Real Lua Join did not queue a request");
                    require(!receipt().receipt.companions[npc - 772].returned,
                        "Lua published before the actual App safe point");
                }, false);
                add("returned-" + std::to_string(npc), [&, npc]()
                {
                    verify(npc - 771, true);
                    if (npc == 772)
                    {
                        require(!screen().activeEventDialog().isActive, "Street return did not close actual dialogue");
                    }
                    else
                    {
                        const uint32_t house = profile().houseIds[npc - 772];
                        const auto &context = events().pendingDialogueContext;
                        const auto &dialog = screen().activeEventDialog();
                        require(context && context->kind == DialogueContextKind::HouseService
                            && context->sourceId == house && context->hostHouseId == house
                            && dialog.isActive && dialog.isHouseDialog && dialog.sourceId == house,
                            "House return did not reload its actual parent house");
                        const auto *pHouse = game.m_gameDataLoader.getHouseTable().get(house);
                        require(pHouse != nullptr, "Returned companion house disappeared");
                        const auto residents = collectSelectableResidentNpcIds(*pHouse,
                            game.m_gameDataLoader.getNpcDialogTable(), events());
                        require(std::find(residents.begin(), residents.end(), npc) == residents.end(),
                            "Returned companion remains a selectable house resident");
                        require(std::none_of(dialog.actions.begin(), dialog.actions.end(), [npc](const auto &action)
                            { return action.kind == EventDialogActionKind::HouseResident && action.id == npc; }),
                            "Actual house UI still offers the returned companion");
                    }
                    if (!reserveRoute) save("returned-" + std::to_string(npc));
                });
            }
            add("all-companions-returned", [&]()
            {
                screen().closeActiveEventDialog();
                verify(4, true);
                save("complete");
            });
        }
        else
        {
            add("cold-state-equality", [&]()
            {
                verify(coldReturned ? 4 : 0, true);
                require(game.m_gameSession.breachAlvarCompanions() == originalReceipt, "Cold receipt changed");
                require(friendActor().breachAlvarNextMaintenanceGameMinute == originalDeadline,
                    "Cold load restarted maintenance phase");
                require(scene().worldRuntime().gameMinutes() == inputSave->savedGameMinutes,
                    "Cold load changed global game clock");
                const auto &position = scene().partyRuntime().movementState();
                const auto &saved = inputSave->outdoorParty.movementState;
                require(std::abs(position.x - saved.x) < 0.5f && std::abs(position.y - saved.y) < 0.5f
                    && std::abs(position.footZ - saved.footZ) < 0.5f, "Cold load changed saved position");
                save("cold-preserved");
            });
            if (coldSplit)
            {
                add("cold-shared-minute-maintenance", [&]()
                {
                    const double before = scene().worldRuntime().gameMinutes();
                    const auto beforeActor = friendActor();
                    scene().advanceTurnBasedGameMinutes(1.0f);
                    game.synchronizeSessionFromRuntime();
                    verify(0, false);
                    const auto actor = friendActor();
                    require(scene().worldRuntime().gameMinutes() == before + 1.0,
                        "Shared scheduler did not advance exactly one minute");
                    require(actor.breachAlvarNextMaintenanceGameMinute == originalDeadline + 1.0,
                        "Source maintenance phase did not advance exactly once");
                    require(actor.currentHp == std::min(beforeActor.combatState.maximumHitPoints(),
                        beforeActor.currentHp + int(profile().friendActor.restoreHealth)),
                        "Source minute did not restore the expected capped HP");
                    save("cold-minute");
                });
            }
            add("cold-no-duplicate-publication", [&]()
            {
                verify(coldReturned ? 4 : 0, coldReturned);
                require(game.m_gameSession.breachAlvarCompanions() == originalReceipt,
                    "Cold verification changed receipt or returned history");
            });
        }

        try
        {
            Engine::EngineApplication app(config,
                [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
                [&]()
                {
                    record("begin-renderer");
                    require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual capture requires Metal");
                    require(game.initializeRenderer(), "Renderer init failed");
                    require(game.loadSessionFromPath(seed), "Actual read-only seed load failed");
                    screen().closeActiveEventDialog();
                    std::ofstream(output / "source-profile.txt") << serializeBreachAlvarCompanionProfile(profile());
                    const auto &held = game.m_gameSession.gameplayUiController().heldInventoryItem();
                    require(!held.active, "Use empty-held seed for real pointer dialogue; no silent item placement");
                    heldBytes = encodeInventoryItemForComparison(held.item);
                    beforeParty = game.m_pMapSceneRuntime->party().snapshot();
                    require(pendingOwnership(beforeParty).empty(),
                        "Use seed with no pending deliveries for this scope");
                    if (fresh)
                    {
                        if (reserveRoute) stage = "reserve-input-admission";
                        failureDomain = "input-precondition";
                        require(beforeParty.members.size() == (reserveRoute ? 4 : 5)
                            && beforeParty.detachedMembers.empty(),
                            "Fresh scope has the wrong active count or pre-existing detached custody");
                        if (reserveRoute)
                        {
                            require(toLowerCopy(inputSave->mapFileName) == "7d29.blv"
                                && beforeParty.adventurersInnMembers.empty() && beforeParty.members[0].level == 3,
                                "Reserve scope requires the normal MM7 four-member castle save (leader level3)");
                            checkReserveCustody(inputSave->party, beforeParty);
                            for (const Character &member : beforeParty.members)
                            {
                                require(member.rosterId < 38 || member.rosterId > 41,
                                    "Reserve template is already instantiated in the input Party");
                            }
                            for (uint32_t rosterId = 38; rosterId <= 41; ++rosterId)
                            {
                                const auto *pRoster = game.m_gameDataLoader.getRosterTable().get(rosterId);
                                require(pRoster && pRoster->experience == 100000 && pRoster->level == 10,
                                    "Runtime lacks the original reserve generation marker for roster"
                                        + std::to_string(rosterId));
                                trace << "{\"stage\":" << std::quoted(stage) << ",\"source_roster_id\":" << rosterId
                                    << ",\"template_level\":" << pRoster->level
                                    << ",\"template_xp\":" << pRoster->experience << "}\n";
                            }
                            trace.flush();
                            if (reserveOneRoute)
                            {
                                stage = "controlled-leader-only-inn-fixture";
                                failureDomain = "controlled-input-preparation";
                                const auto originalMembers = allMembers(beforeParty);
                                const uint64_t leaderMemberId = beforeParty.members[0].memberId;
                                const uint64_t nextMemberId = beforeParty.nextMemberId;
                                record("before-three-standard-inn-dismissals");
                                for (unsigned count = 0; count < 3; ++count)
                                {
                                    require(game.m_pMapSceneRuntime->party().dismissMemberToAdventurersInn(1),
                                        "Standard controlled inn preparation rejected a dismissal");
                                }
                                game.synchronizeSessionFromRuntime();
                                beforeParty = game.m_pMapSceneRuntime->party().snapshot();
                                const auto preparedMembers = allMembers(beforeParty);
                                require(beforeParty.members.size() == 1 && beforeParty.adventurersInnMembers.size() == 3
                                    && beforeParty.detachedMembers.empty() && preparedMembers.size() == 4
                                    && beforeParty.members[0].memberId == leaderMemberId
                                    && beforeParty.nextMemberId == nextMemberId,
                                    "Controlled leader-only preparation changed ownership or allocated an identity");
                                for (const auto &member : originalMembers)
                                {
                                    require(preparedMembers.contains(member.first),
                                        "Controlled inn preparation lost an original ID");
                                }
                                // Standard dismissal owns any buff effects; later comparisons start here.
                                record("post-fixture-active-plus-three-inn-preservation-baseline");
                                std::cout << "reserve1 controlled preparation: three standard "
                                    "dismissMemberToAdventurersInn(1) "
                                    "calls; one active plus three inn; preservation starts after this fixture; "
                                    "no natural progression or UI dismissal claim\n" << std::flush;
                            }
                        }
                        require(!game.m_gameSession.breachAlvarCompanions()
                            && (reserveOneRoute || !beforeParty.questBits.contains(1714)), "Seed already split");
                        const auto requireNoCompanionProgress = [&](const std::string &suffix)
                        {
                            const std::string key = std::string(QuestPrefix) + suffix;
                            require(game.m_gameSession.namedGlobalVar(key) == 0,
                                "Use a seed without prior companion/ending progress: " + key);
                        };
                        // Ordinary AfterLoad initializes invitation/contact timestamps. They are valid
                        // pre-existing progress, unrelated to this first split; preserve them unchanged.
                        for (const std::string suffix : {"BreachSplit", "QuestFinished", "GotFQHints"})
                        {
                            requireNoCompanionProgress(suffix);
                        }
                        for (uint32_t index = 0; index < 4; ++index)
                        {
                            requireNoCompanionProgress("GotFQHint" + std::to_string(index + 1));
                            requireNoCompanionProgress("HintByNPC." + std::to_string(772 + index));
                            requireNoCompanionProgress("FQMercRnd." + std::to_string(772 + index));
                        }
                        record("before-declared-map-jump");
                        const auto *pMap = game.m_gameDataLoader.getMapStats().findByFileName("bralvar.odm");
                        require(pMap != nullptr && pMap->worldId == "mmmerge", "Controlled source map unavailable");
                        game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                        if (reserveRoute) stage = "source-onload65469";
                        failureDomain = "source-onload-publication";
                        require(game.processPendingDebugMapJump(), "Controlled map jump/init failed");
                        require(receipt().receipt.leaderMemberId == beforeParty.members[0].memberId,
                            "Source split replaced the original leader");
                        originalReceipt = game.m_gameSession.breachAlvarCompanions();
                        if (reserveRoute)
                        {
                            stage = "source-reserve-publication";
                            failureDomain = "source-generation-contract";
                            const Party::Snapshot split = scene().party().snapshot();
                            const auto &value = receipt().receipt;
                            require(value.profileVersion == 2
                                && value.initialActiveCount == reserveInitialActiveCount
                                && value.leaderLevelAtSplit == 3 && split.members.size() == 1
                                && split.detachedMembers.size() == 4
                                && split.adventurersInnMembers.size() == beforeParty.adventurersInnMembers.size(),
                                "Source reserve OnLoad did not publish the expected V2 split/inn custody");
                            require(validBreachAlvarCompanionReceipt(value, split, error), error);
                            const auto originalMembers = allMembers(beforeParty);
                            const auto splitMembers = allMembers(split);
                            require(originalMembers.size() == 4 && splitMembers.size() == 4 + reserveNewCount,
                                "Reserve publication added the wrong number of owned characters");
                            Party::Snapshot originalProjection = split;
                            uint32_t generatedCount = 0;
                            for (size_t index = 0; index < value.companions.size(); ++index)
                            {
                                const auto &entry = value.companions[index];
                                if (index < reserveRetainedCount)
                                {
                                    require(originalMembers.contains(entry.memberId) && !entry.generation
                                        && entry.memberId != value.leaderMemberId,
                                        "Retained NPC772/773/774 did not preserve an original nonleader character");
                                    continue;
                                }
                                require(entry.generation && !originalMembers.contains(entry.memberId)
                                    && splitMembers.contains(entry.memberId),
                                    "Source reserve lacks its new identity and actual generation receipt");
                                const auto &generation = *entry.generation;
                                const Character &generated = splitMembers.at(entry.memberId);
                                const uint32_t minimumLevel = value.leaderLevelAtSplit > 5
                                    ? value.leaderLevelAtSplit - 5 : 1;
                                require(generation.request.rosterId == 38 + index && generated.rosterId == 38 + index
                                    && generated.level == generation.request.level
                                    && generated.level >= minimumLevel
                                    && generated.level <= value.leaderLevelAtSplit + 8
                                    && generated.experience == generated.level * (generated.level - 1) * 500,
                                    "Reserve roster generation differs from source max(leader3+[-5,8],1)/CountExp");
                                require(entry.memberId == beforeParty.nextMemberId + generatedCount,
                                    "Source reserve generation allocated an unexpected stable identity");
                                ++generatedCount;
                                const auto found = std::find_if(originalProjection.detachedMembers.begin(),
                                    originalProjection.detachedMembers.end(), [&](const auto &member)
                                    {
                                        return member.ownerKey == BreachAlvarSplitOwner
                                            && member.slotId == 772 + index
                                            && member.character.memberId == entry.memberId;
                                    });
                                require(found != originalProjection.detachedMembers.end(),
                                    "Generated reserve is not in its actual source detached slot");
                                // Comparison copy only; never applied to Party.
                                originalProjection.detachedMembers.erase(found);
                                std::cout << "{\"source_generated_npc\":" << 772 + index
                                    << ",\"member_id\":" << entry.memberId << ",\"roster_id\":" << generated.rosterId
                                    << ",\"name\":" << std::quoted(generated.name) << ",\"level\":" << generated.level
                                    << ",\"xp\":" << generated.experience << "}\n";
                            }
                            require(generatedCount == reserveNewCount
                                && split.nextMemberId == beforeParty.nextMemberId + reserveNewCount,
                                "Reserve publication allocated identities beyond its expected source generations");
                            checkReserveCustody(beforeParty, originalProjection);
                            preservationBaseline = split;
                            std::cout << "{\"source_reserve_publication\":";
                            writeReserveReceipt(std::cout, value);
                            std::cout << ",\"generated_count\":" << generatedCount
                                << ",\"preservation_baseline\":"
                                << std::quoted(reserveOneRoute
                                    ? "after-standard-inn-fixture" : "normal-four-member-input")
                                << ",\"original_four_payloads_preserved\":true}\n" << std::flush;
                            record("source-reserve-publication-validated");
                        }
                        const auto *pActor = scene().worldRuntime().mapActorState(friendIndex());
                        require(pActor != nullptr, "Real fresh companion is not gameplay-visible");
                        const auto &rules = profile().friendActor;
                        require(pActor->preciseX == float(rules.x) && pActor->preciseY == float(rules.y)
                            && pActor->preciseZ == float(rules.z) && pActor->currentHp == pActor->combatState.maximumHitPoints(),
                            "Real source spawn changed position or initial health");
                    }
                    else if (ascentRoute)
                    {
                        stage = ascentTopRoute ? "ascent-top-cold-checkpoint"
                            : ascentResumeRoute ? "ascent-resume-checkpoint" : "ascent-input-and-bottom-fixture";
                        failureDomain = "input-or-map-definition-precondition";
                        if (ascentCheckpointRoute)
                        {
                            beforeParty = inputSave->party;
                            checkCustody(beforeParty, scene().party().snapshot(), true);
                        }
                        require(inputSave->breachAlvarCompanions
                            && toLowerCopy(inputSave->mapFileName) == "bralvar.odm"
                            && beforeParty.members.size() == 5 && beforeParty.detachedMembers.empty(),
                            "Ascent requires the completed normal-doors five-member input");
                        originalReceipt = inputSave->breachAlvarCompanions;
                        require(std::all_of(originalReceipt->receipt.companions.begin(),
                            originalReceipt->receipt.companions.end(), [](const auto &entry)
                            {
                                return entry.returned;
                            }),
                            "Ascent input has not returned all four companions");
                        for (const std::string suffix : {"BrFirstFloor", "BrSecFloor", "BrThirdFloor",
                            "QuestFinished", "GotEndCard"})
                        {
                            const bool floorFlag = suffix == "BrFirstFloor" || suffix == "BrSecFloor"
                                || suffix == "BrThirdFloor";
                            const bool expected = (ascentTopRoute && floorFlag)
                                || (ascentResumeRoute && suffix == "BrSecFloor");
                            require(quest(suffix) == int(expected),
                                "Ascent input has unexpected progress: " + suffix);
                        }
                        require(quest("CoughtChaos") == int(ascentEndingRoute),
                            "Ascent input capture state differs from its requested route");
                        if (ascentEndingRoute)
                        {
                            for (int id = 1; id <= 11; ++id)
                            {
                                if (quest("RiddleSolved." + std::to_string(id)) != 0) answeredRiddles.insert(id);
                            }
                            verifyChaosProgress(5, true, false);
                        }
                        const std::array<uint32_t, 7> topOpenEvents = {106, 107, 108, 114, 115, 116, 128};
                        for (uint32_t eventId = 104; eventId <= 128; ++eventId)
                        {
                            const bool open = (ascentResumeRoute && (eventId == 107 || eventId == 108))
                                || (ascentTopRoute && std::find(topOpenEvents.begin(), topOpenEvents.end(), eventId)
                                    != topOpenEvents.end());
                            const auto found = events().outdoorModelMechanisms.find(eventId);
                            require(found != events().outdoorModelMechanisms.end()
                                && found->second.interactionEventId == eventId
                                && events().mechanisms.contains(eventId)
                                && !events().mechanisms.at(eventId).isMoving
                                && events().mechanisms.at(eventId).state
                                    == uint16_t(open ? EvtMechanismState::Open : EvtMechanismState::Closed)
                                && events().mechanisms.at(eventId).currentDistance == (open ? 1.0f : 0.0f)
                                && scene().localEventProgram() && scene().localEventProgram()->hasEvent(eventId),
                                "Cold input has a missing or unexpected source mechanism definition/state/handler: "
                                    + std::to_string(eventId));
                        }
                        const auto *pMap = scene().worldRuntime().mapData();
                        require(pMap != nullptr, "Ascent has no authoritative outdoor geometry");
                        std::map<int, std::vector<bx::Vec3>> treads;
                        std::vector<size_t> faces;
                        if (!ascentTopRoute)
                            scene().worldRuntime().collectOutdoorFaceCandidates(-3400, -2200, -1500, -600, faces);
                        for (size_t index : faces)
                        {
                            const auto *pFace = scene().worldRuntime().outdoorFace(index);
                            if (!pFace || pFace->modelName != "SubTower" || !pFace->isWalkable
                                || pFace->normal.z < 0.999f || pFace->notAStep || pFace->vertices.size() < 3
                                || pFace->maxZ - pFace->minZ > 0.1f
                                || pFace->minZ < 870.0f || pFace->minZ > 3110.0f) continue;
                            float twiceArea = 0.0f;
                            bx::Vec3 center = {0.0f, 0.0f, 0.0f};
                            for (size_t vertex = 0; vertex < pFace->vertices.size(); ++vertex)
                            {
                                const auto &first = pFace->vertices[vertex];
                                const auto &next = pFace->vertices[(vertex + 1) % pFace->vertices.size()];
                                twiceArea += first.x * next.y - next.x * first.y;
                                center = bx::add(center, first);
                            }
                            const float area = std::abs(twiceArea) * 0.5f;
                            if (area < 25000.0f || area > 30000.0f) continue;
                            center = bx::mul(center, 1.0f / float(pFace->vertices.size()));
                            center.z = pFace->minZ + 1.0f;
                            treads[int(std::lround(pFace->minZ))].push_back(center);
                        }
                        bx::Vec3 previous = {-2200, -910, 856};
                        for (const auto &[height, candidates] : treads)
                        {
                            if (height >= 1975 && height < 2000) continue;
                            if (height >= 2000 && ascentUpperStairs.empty()) previous = {-2200, -910, 1991};
                            const auto point = std::min_element(candidates.begin(), candidates.end(),
                                [&](const auto &first, const auto &second)
                                {
                                    return std::hypot(first.x - previous.x, first.y - previous.y)
                                        < std::hypot(second.x - previous.x, second.y - previous.y);
                                });
                            require(std::hypot(point->x - previous.x, point->y - previous.y) < 300.0f
                                && point->z - previous.z < 90.0f,
                                "Static SubTower tread candidates do not form the bounded source spiral");
                            (height < 2000 ? ascentLowerStairs : ascentUpperStairs).push_back(*point);
                            previous = *point;
                        }
                        require(ascentTopRoute || (ascentLowerStairs.size() >= 45 && ascentUpperStairs.size() >= 45),
                            "SubTower source geometry lacks the two expected stair flights");
                        for (size_t index = 0; index < pMap->entities.size(); ++index)
                        {
                            const auto &entity = pMap->entities[index];
                            if (toLowerCopy(entity.name) != "ormatt1" || entity.scriptEventId() != 81) continue;
                            require(returnEntityIndex == GameplayInvalidWorldIndex && entity.x == 7
                                && entity.y == 20 && entity.z == 4750, "Ambiguous or moved source OrMatt1/event81");
                            returnEntityIndex = index;
                        }
                        require(returnEntityIndex != GameplayInvalidWorldIndex, "Actual source orb81 is absent");
                        if (!ascentCheckpointRoute)
                            scene().partyRuntime().teleportTo(-2200, -910, 856); // The only ascent position fixture.
                        const auto &pose = scene().partyRuntime().movementState();
                        if (ascentCheckpointRoute)
                        {
                            const auto &savedPose = inputSave->outdoorParty.movementState;
                            require(pose.supportKind == OutdoorSupportKind::BModelFace && !pose.airborne
                                && !scene().partyRuntime().partyMovementState().flying
                                && std::abs(pose.footZ - (ascentTopRoute ? 4410.0f : 3127.0f)) < 0.25f
                                && std::abs(pose.x - savedPose.x) < 0.01f
                                && std::abs(pose.y - savedPose.y) < 0.01f
                                && std::abs(pose.footZ - savedPose.footZ) < 0.25f,
                                "Resume changed the real checkpoint pose or lost its supported landing");
                            if (ascentTopRoute)
                            {
                                require(pMap->bmodels.at(pose.supportBModelIndex).name == "TopFloor",
                                    "Top checkpoint is not supported by the real TopFloor geometry");
                                ascentLanded = true; // Prior actual005 proved this landing; no new launch is claimed.
                            }
                        }
                        else
                            require(pose.supportKind == OutdoorSupportKind::BModelFace && !pose.airborne
                                && pose.footZ > 850.0f && pose.footZ < 865.0f
                                && pMap->bmodels.at(pose.supportBModelIndex).name == "SubTower",
                                "Initial SubTower navigation fixture did not resolve to the real bottom landing");
                        ascentInitialGameMinutes = scene().worldRuntime().gameMinutes();
                        BreachAlvarWorldActorTestAccess::discardQueuedAi(scene().worldRuntime());
                        record(ascentTopRoute ? "cold-prior-actual-top-landing-preserved"
                            : ascentResumeRoute ? "cold-actual-third-floor-checkpoint-preserved"
                                : "single-bottom-navigation-fixture-and-real-treads-ready");
                    }
                    else if (chaosRoute)
                    {
                        require(inputSave->breachAlvarCompanions
                            && toLowerCopy(inputSave->mapFileName) == "bralvar.odm",
                            "Chaos input must be the real completed BrAlvar return save");
                        beforeParty = inputSave->party;
                        originalReceipt = inputSave->breachAlvarCompanions;
                        require(beforeParty.members.size() == 5 && beforeParty.detachedMembers.empty()
                            && std::all_of(originalReceipt->receipt.companions.begin(),
                                originalReceipt->receipt.companions.end(),
                                [](const auto &member) { return member.returned; }),
                            "Chaos input has not reunited the original five-member Party");
                    }
                    else
                    {
                        require(inputSave->breachAlvarCompanions
                            && toLowerCopy(inputSave->mapFileName) == "bralvar.odm",
                            "Cold input lacks an actual BrAlvar receipt");
                        beforeParty = inputSave->party;
                        originalReceipt = inputSave->breachAlvarCompanions;
                        const auto found = std::find_if(inputSave->outdoorWorld.mapActors.begin(),
                            inputSave->outdoorWorld.mapActors.end(), [&](const auto &actor)
                            { return actor.actorId == originalReceipt->friendActor.actorId; });
                        require(found != inputSave->outdoorWorld.mapActors.end(), "Cold input binding absent");
                        originalDeadline = found->breachAlvarNextMaintenanceGameMinute;
                        const auto actualActor = friendActor();
                        const auto actualWorld = scene().worldRuntime().snapshot();
                        std::ofstream(output / "cold-actor-equality.json") << std::setprecision(17)
                            << "{\"stage\":\"immediately-after-actual-load\",\"actor_id\":" << found->actorId
                            << ",\"hp\":[" << found->currentHp << ',' << actualActor.currentHp
                            << "],\"max_hp\":[" << found->combatState.maximumHitPoints() << ',' << actualActor.combatState.maximumHitPoints()
                            << "],\"ai_state\":[" << int(found->aiState) << ',' << int(actualActor.aiState)
                            << "],\"control_mode\":[" << int(found->controlMode) << ',' << int(actualActor.controlMode)
                            << "],\"control_seconds\":[" << found->controlRemainingSeconds << ','
                            << actualActor.controlRemainingSeconds << "],\"blood_marks\":"
                            << friendBlood(actualWorld, found->actorId).size()
                            << ",\"assertion_result\":\"see-trace-and-result\"}\n";
                        checkColdFriendActor(*found, actualActor);
                        require(friendBlood(inputSave->outdoorWorld, found->actorId)
                            == friendBlood(actualWorld, found->actorId), "Cold load changed companion blood marks");
                        record("cold-actor-selected-fields-equal");
                        require(encodeInventoryItemForComparison(inputSave->heldInventoryItem) == heldBytes
                            && !inputSave->heldInventoryItemActive, "Cold held-item data differs");
                    }
                    originalActorCount = scene().worldRuntime().mapActorCount();
                    if (!fresh)
                    {
                        const auto coldWorld = scene().worldRuntime().snapshot();
                        require(originalActorCount == inputSave->outdoorWorld.mapActors.size()
                            && coldWorld.nextActorId == inputSave->outdoorWorld.nextActorId,
                            "Cold OnLoad duplicated or allocated a world actor");
                        if (ascentCheckpointRoute)
                            require(std::equal(coldWorld.mapActors.begin(), coldWorld.mapActors.end(),
                                inputSave->outdoorWorld.mapActors.begin(), [](const auto &actual, const auto &saved)
                                {
                                    return actual.actorId == saved.actorId;
                                }), "Cold checkpoint changed or reordered a world actor ID");
                        require(scene().party().memberCount() == inputSave->party.members.size(),
                            "Cold load changed active party size");
                        for (size_t index = 0; index < inputSave->party.members.size(); ++index)
                        {
                            require(scene().party().members()[index].memberId
                                == inputSave->party.members[index].memberId,
                                "Cold load changed active member ordering");
                        }
                    }
                    if (ascentCheckpointRoute)
                    {
                        const size_t existingSummons = ascentTopRoute ? 26 : 12;
                        require(originalActorCount >= existingSummons, "Resume lacks its existing source summons");
                        originalActorCount -= existingSummons;
                        verifyAscentPopulation();
                        trace << "{\"stage\":" << std::quoted(stage) << ",\"resumed_prior_actual\":true"
                            << ",\"loaded_actor_count\":" << scene().worldRuntime().mapActorCount()
                            << ",\"source_population_baseline\":" << originalActorCount
                            << ",\"existing_source_summons\":" << existingSummons << ",\"initial_game_minutes\":"
                            << std::setprecision(17) << ascentInitialGameMinutes << "}\n";
                        trace.flush();
                    }
                    record("actual-runtime-ready");
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
                    require(SDL_GetTicks() - started < (chaosRoute || reserveRoute ? 60000u : 180000u),
                        "Bounded companion capture timed out at " + stage);
                    ++frames;
                    renderWidth = width;
                    renderHeight = height;
                    if (frames >= nextStep && stepIndex < steps.size()
                        && pendingCapture.empty() && scheduledCapture.empty())
                    {
                        const Step &step = steps[stepIndex];
                        if (!ascentRoute || !ascentStepStarted)
                        {
                            stage = step.name;
                            if (reserveRoute)
                            {
                                failureDomain = stage.find("pointer") != std::string::npos
                                    || stage.starts_with("door-position") || stage.starts_with("house-door")
                                    ? "pointer-or-navigation-integration"
                                    : stage.starts_with("source-hint") || stage.starts_with("resident-selected")
                                        || stage.starts_with("join-request")
                                        ? "source-dialogue-integration" : "source-custody-or-save-invariant";
                            }
                            record("begin");
                            ascentStepStartSeconds = ascentSimulationSeconds;
                            step.action();
                            ascentStepStarted = ascentRoute;
                        }
                        if (!step.advance || step.advance())
                        {
                            ++stepIndex;
                            ascentStepStarted = false;
                            record("action-complete");
                            nextStep = frames + (ascentRoute ? 3 : chaosRoute || reserveRoute ? 5 : 35);
                            if (step.screenshot)
                            {
                                scheduledCapture = ascentOriginalFloorReturned && step.name == "orb81-reached"
                                    ? "source81-floor-returned" : step.name;
                                captureFrame = frames + (chaosRoute || reserveRoute || ascentRoute ? 2 : 12);
                            }
                        }
                    }
                    // All publication remains in the actual App drain, after the real Lua/UI action has returned.
                    game.renderFrame(width, height, wheel, 0.0f);
                    if (chaosRoute || ascentEndingRoute)
                    {
                        require(!game.m_pendingWinGameCertificateAfterMovie,
                            "Chaos completion unexpectedly entered the ordinary MM8 movie route");
                        const bool visible =
                            dynamic_cast<WinGameScreen *>(game.m_screenManager.activeScreen()) != nullptr;
                        if (visible && !endCardVisible) ++endCardShows;
                        endCardVisible = visible;
                        require(endCardShows <= 1, "Actual Breach completion card repeated");
                    }
                    if (!scheduledCapture.empty() && frames >= captureFrame)
                    {
                        pendingCapture = std::move(scheduledCapture);
                        scheduledCapture.clear();
                        bgfx::requestScreenShot(BGFX_INVALID_HANDLE, pendingCapture.c_str());
                    }
                    if (!pendingCapture.empty())
                    {
                        if (auto screenshot = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                        {
                            SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(screenshot->width),
                                int(screenshot->height), SDL_PIXELFORMAT_BGRA32, screenshot->bgraPixels.data(),
                                int(screenshot->width * 4));
                            require(pSurface != nullptr, "Cannot construct screenshot surface");
                            const bool written = SDL_SaveBMP(pSurface,
                                (output / (pendingCapture + ".bmp")).string().c_str());
                            SDL_DestroySurface(pSurface);
                            require(written, "Cannot persist screenshot");
                            record("captured:" + pendingCapture);
                            pendingCapture.clear();
                            ++captures;
                        }
                    }
                    if (stepIndex == steps.size() && frames >= nextStep
                        && pendingCapture.empty() && scheduledCapture.empty())
                    {
                        screen().closeActiveEventDialog();
                        require(fileSha256(seed) == seedHash, "Read-only input changed");
                        if (chaosRoute)
                        {
                            require(captures == 4 && endCardShows == 1,
                                "Chaos route did not finish its four captures and one completion card");
                            verifyChaosProgress(5, true, true);
                        }
                        if (reserveRoute)
                            require(captures == 3, "Reserve route did not finish its three necessary captures");
                        if (ascentRoute)
                        {
                            require(captures == (ascentTopRoute ? 3u : ascentEndingRoute ? 5u : 3u)
                                && ascentLanded && quest("BrThirdFloor") == 1
                                && quest("CoughtChaos") == int(ascentEndingRoute)
                                && quest("QuestFinished") == int(ascentEndingRoute),
                                "Controlled ascent did not finish its landing/orb evidence");
                            if (ascentEndingRoute) verifyChaosProgress(5, true, true);
                            else verifyAscentPopulation();
                        }
                        complete = true;
                        record("complete");
                        SDL_Event quit{};
                        quit.type = SDL_EVENT_QUIT;
                        SDL_PushEvent(&quit);
                    }
                },
                [&]() { game.shutdownApplication(); });
            applicationResult = app.run();
            if (applicationResult != 0 || !complete)
            {
                failureDomain = applicationResult == 0 && !complete ? "external-interruption" : "application-exit";
                throw std::runtime_error("Actual companion run incomplete at " + stage
                    + "; app_result=" + std::to_string(applicationResult) + "; frames=" + std::to_string(frames)
                    + "; captures=" + std::to_string(captures));
            }
            std::ofstream(output / "result.json")
                << "{\"status\":\"passed\",\"controlled\":true,\"mode\":" << std::quoted(mode)
                << ",\"seed_sha256_before\":" << std::quoted(seedHash)
                << ",\"seed_sha256_after\":" << std::quoted(fileSha256(seed))
                << ",\"frames\":" << frames << ",\"captures\":" << captures
                << ",\"app_result\":" << applicationResult << ",\"complete\":true"
                << ",\"reserve_route\":" << (reserveRoute ? "true" : "false")
                << ",\"ascent_route\":" << (ascentRoute ? "true" : "false")
                << ",\"ascent_ending\":" << (ascentEndingRoute ? "true" : "false")
                << ",\"resumed_prior_actual\":" << (ascentCheckpointRoute ? "true" : "false")
                << ",\"restored_prior_top_landing\":" << (ascentTopRoute && ascentLanded ? "true" : "false")
                << ",\"source_jump_observed_in_this_run\":" << (ascentAirborneSeen ? "true" : "false")
                << ",\"source81_original_floor_return\":" << (ascentOriginalFloorReturned ? "true" : "false")
                << ",\"ascent_explicit_orb_pointer_click\":" << (ascentExplicitOrbClick ? "true" : "false")
                << ",\"floor_return_capture_repeats_certificate\":"
                << (ascentOriginalFloorReturned ? "true" : "false")
                << ",\"riddles_reused_from_input\":" << (ascentEndingRoute ? answeredRiddles.size() : 0)
                << ",\"combat_ai_frozen_fixture\":" << (ascentRoute ? "true" : "false")
                << ",\"ascent_simulation_seconds\":" << ascentSimulationSeconds
                << ",\"source_jump_maximum_z\":" << ascentMaximumZ
                << ",\"leader_only_inn_fixture\":" << (reserveOneRoute ? "true" : "false")
                << ",\"chaos_route\":" << (chaosRoute ? "true" : "false")
                << ",\"riddles_answered\":" << answeredRiddles.size()
                << ",\"door_keys\":" << usedDoorKeys.size() << ",\"end_cards\":" << endCardShows
                << ",\"natural_campaign\":false,\"normal_merge_admission\":false,"
                   "\"visual_review_required\":true,\"house_door_clicks\":"
                << (doorRoute || chaosRoute ? "true" : "false") << "}\n";
            return 0;
        }
        catch (const std::exception &failure)
        {
            trace << "{\"stage\":" << std::quoted(stage) << ",\"frame\":" << frames
                << ",\"failure_domain\":" << std::quoted(failureDomain)
                << ",\"failure\":" << std::quoted(failure.what()) << "}\n";
            trace.flush();
            std::cerr << "companion capture " << failureDomain << " at " << stage << ", frame " << frames
                << ": " << failure.what() << '\n';
            std::ofstream(output / "result.json")
                << "{\"status\":" << std::quoted(failureDomain == "external-interruption" ? "interrupted" : "failed")
                << ",\"controlled\":true,\"stage\":" << std::quoted(stage)
                << ",\"mode\":" << std::quoted(mode) << ",\"frames\":" << frames
                << ",\"resumed_prior_actual\":" << (ascentCheckpointRoute ? "true" : "false")
                << ",\"restored_prior_top_landing\":" << (ascentTopRoute && ascentLanded ? "true" : "false")
                << ",\"source_jump_observed_in_this_run\":" << (ascentAirborneSeen ? "true" : "false")
                << ",\"source81_original_floor_return\":" << (ascentOriginalFloorReturned ? "true" : "false")
                << ",\"ascent_explicit_orb_pointer_click\":" << (ascentExplicitOrbClick ? "true" : "false")
                << ",\"captures\":" << captures << ",\"app_result\":" << applicationResult
                << ",\"complete\":" << (complete ? "true" : "false")
                << ",\"failure_domain\":" << std::quoted(failureDomain)
                << ",\"error\":" << std::quoted(failure.what()) << ",\"seed_sha256_before\":"
                << std::quoted(seedHash) << ",\"seed_sha256_after\":" << std::quoted(fileSha256(seed)) << "}\n";
            throw;
        }
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "breach_alvar_capture CONTROLLED_RUNTIME READONLY_SEED NEW_OUTPUT "
            "run|doors|reserve|reserve1|ascent|ascent-ending|ascent-ending-resume|ascent-ending-top|"
            "reload-split|reload-returned|chaos\n";
        return 1;
    }
    try
    {
        return OpenYAMM::Game::GameApplicationTestAccess::run(argv[1], argv[2], argv[3], argv[4]);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
