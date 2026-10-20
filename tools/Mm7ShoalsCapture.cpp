#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/scene/OutdoorSceneRuntime.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr float StepSeconds = 1.0f / 60.0f;
constexpr const char *MissingSuitMessage = "You must all be wearing your wetsuits!";
constexpr const char *CompletionKey = "MMerge.CrossContinents.QuestFinished";

void require(bool value, const std::string &message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

bool hasSuit(const Character &member)
{
    // This fixture deliberately carries suits in inventory, one of the two source alternatives.
    return std::any_of(member.inventory.begin(), member.inventory.end(), [](const InventoryItem &item)
    {
        return item.objectDescriptionId == 1406 && item.quantity > 0;
    });
}

void checkSavedParty(const GameSaveData &saved, const std::vector<uint64_t> &memberIds)
{
    const auto completed = saved.namedGlobalVars.find(CompletionKey);
    require(completed == saved.namedGlobalVars.end() || completed->second == 0, "Unexpected Merge completion");
    require(saved.party.members.size() == 5 && memberIds.size() == 5, "Five-member fixture changed");
    Party party;
    party.restoreSnapshot(saved.party);
    require(!party.hasPartyBuff(PartyBuffId::Fly), "Save gained a Fly buff");
    require(!saved.outdoorParty.partyMovementState.flying
        && !saved.outdoorParty.partyMovementState.activelyFlying, "Save confused swimming with flight");
    for (size_t index = 0; index < memberIds.size(); ++index)
    {
        require(saved.party.members[index].memberId == memberIds[index], "Member identity changed");
        require(hasSuit(saved.party.members[index]), "An inventory wetsuit disappeared");
    }
}
}

struct GameApplicationTestAccess
{
    static int run(std::filesystem::path root, std::filesystem::path seed,
        std::filesystem::path output, const std::string &mode)
    {
        const bool fresh = mode == "loop";
        const bool coldWater = mode == "reload-underwater";
        const bool coldReturn = mode == "reload-return";
        require(fresh || coldWater || coldReturn, "Unknown bounded Shoals mode");
        root = std::filesystem::canonical(root);
        seed = std::filesystem::canonical(seed);
        require(!std::filesystem::exists(output), "Output must be a new directory");
        require(std::filesystem::weakly_canonical(output).string().starts_with("/Volumes/NVMeSSD/"),
            "Evidence must stay on the NVMe volume");
        std::filesystem::create_directories(output);
        output = std::filesystem::canonical(output);
        std::filesystem::current_path(output);

        std::string error;
        GameSettings settings = GameSettings::createDefault();
        settings.assetRoot = root.string();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.startFlying = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.movementSpeedMultiplier = 1.0f;
        settings.windowMode = WindowMode::Windowed;
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot write isolated settings: " + error);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Shoals144 controlled movement";
        config.assetRoot = root.string();
        config.activeWorldId = "mm7";
        config.startupMapFileOverride = "out14.odm";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");

        std::ofstream(output / "fixture.json")
            << "{\"controlled\":true,\"natural_campaign\":false,\"mode\":" << std::quoted(mode)
            << ",\"seed\":" << std::quoted(seed.string())
            << ",\"renderer\":\"Metal\",\"audio\":\"dummy\",\"changes\":"
            << std::quoted(fresh
                ? "Require five-member seed; declared Avlee jump and west-boundary position at minX+96, y=-16562; "
                  "clear party/character buffs; clear conditions and restore HP; set SP0 and Q642; "
                  "remove all suits, grant inventory1406 to first four, later grant fifth after two real refusals. "
                  "No quest-completion mutation, actor changes, collision bypass or swimming position fixture."
                : "Cold load supplied save; no party, map, position, buff or quest mutation.")
            << ",\"input\":\"Synthetic SDL key-down/up taps every movement frame enter GameApplication and "
               "GameInputSystem; continuous logical action, not physical keyboard hold. Defaults W/S and J/K. "
               "Travel uses actual MapTransitionConfirm/Cancel actions.\","
               "\"timing\":\"Fixed 1/60 simulation seconds per active frame; screenshots freeze simulation; "
               "120-second in-process bound plus caller timeout required for synchronous loads.\","
               "\"limits\":[\"No Lincoln actual traversal\",\"No iPad or physical keyboard acceptance\","
               "\"Underwater action restrictions and temporary portraits remain unsupported\","
               "\"Surface timer has approved approximately one-second source Tick difference\"]}\n";

        GameApplication game(config);
        std::ofstream milestones(output / "milestones.jsonl");
        std::ofstream framesLog(output / "movement.jsonl");
        std::ofstream travelLog(output / "travel-resources.jsonl");
        std::vector<uint64_t> memberIds;
        std::string stage = "warmup";
        std::string pendingCapture;
        float stageSeconds = 0.0f;
        float stageX = 0.0f;
        float stageZ = 0.0f;
        float stageStatusSeconds = 0.0f;
        float contactX = 0.0f;
        float previousStatusSeconds = 0.0f;
        bool previousStatusMatched = false;
        unsigned missingNotifications = 0;
        unsigned frames = 0;
        unsigned captures = 0;
        unsigned boundaryContactFrames = 0;
        unsigned surfacePrompts = 0;
        uint64_t started = 0;
        bool complete = false;

        const auto scene = [&]() -> OutdoorSceneRuntime &
        {
            require(game.m_pMapSceneRuntime != nullptr && game.m_pMapSceneRuntime->kind() == SceneKind::Outdoor,
                "Expected outdoor scene at stage " + stage);
            return *static_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        };
        const auto state = [&]() -> EventRuntimeState & { return *scene().eventRuntimeState(); };
        const auto movement = [&]() -> const OutdoorMoveState & { return scene().partyRuntime().movementState(); };
        const auto checkNoFlight = [&]()
        {
            require(!game.m_settings.startFlying, "Debug startFlying enabled");
            require(!scene().party().hasPartyBuff(PartyBuffId::Fly), "Swimming granted Fly");
            require(!scene().partyRuntime().partyMovementState().flying
                && !scene().partyRuntime().partyMovementState().activelyFlying, "Swimming consumed flight mode");
            const auto found = state().namedGlobalVars.find(CompletionKey);
            require(found == state().namedGlobalVars.end() || found->second == 0, "Unexpected QuestFinished");
            require(std::any_of(scene().party().members().begin(), scene().party().members().end(),
                [](const Character &member) { return member.health > 0; }),
                "Controlled party lost all positive HP; inspect evidence instead of retrying");
        };
        const auto record = [&](const std::string &label)
        {
            const OutdoorMoveState &position = movement();
            milestones << "{\"stage\":" << std::quoted(label) << ",\"frame\":" << frames
                << ",\"map\":" << std::quoted(game.m_gameSession.currentMapFileName())
                << ",\"x\":" << position.x << ",\"y\":" << position.y << ",\"z\":" << position.footZ
                << ",\"underwater\":" << (scene().worldRuntime().isUnderwaterMap() ? "true" : "false")
                << ",\"missing_notifications\":" << missingNotifications
                << ",\"surface_prompts\":" << surfacePrompts << "}\n";
            milestones.flush();
        };
        const auto next = [&](const std::string &name)
        {
            stage = name;
            stageSeconds = 0.0f;
            stageX = movement().x;
            stageZ = movement().footZ;
            stageStatusSeconds = game.m_gameSession.gameplayScreenRuntime().statusBarEventRemainingSeconds();
            record(name);
        };
        const auto capture = [&](const std::string &name)
        {
            require(pendingCapture.empty(), "Screenshot overlap");
            pendingCapture = name;
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, name.c_str());
        };
        const auto save = [&](const std::string &name, bool allSuited)
        {
            require(game.quickSaveToPath(output / (name + ".oysav"), "144受控海域 " + name), "Save failed: " + name);
            const auto saved = loadGameDataFromPath(output / (name + ".oysav"), error);
            require(saved.has_value(), "Cannot inspect saved milestone: " + error);
            if (allSuited)
            {
                checkSavedParty(*saved, memberIds);
            }
        };
        const auto checkPrompt = [&](bool entering)
        {
            require(state().pendingDialogueContext.has_value(), "Missing real transition context");
            const auto &context = *state().pendingDialogueContext;
            require(context.kind == DialogueContextKind::MapTransition && context.transitionImageId == 8
                && context.transitionMapMove.has_value(), "Source Icon8 confirmation missing");
            const auto &move = *context.transitionMapMove;
            require(move.mapName == (entering ? "7out15.odm" : "out14.odm")
                && move.x == (entering ? 20096 : -18584) && move.y == (entering ? -16448 : -16562)
                && move.z == (entering ? 2404 : 1) && move.directionDegrees == (entering ? 177 : 50),
                "Original transition coordinates or direction changed");
            require(!state().pendingMapMove, "Travel occurred before confirmation");
            const auto &dialog = game.m_gameSession.gameplayScreenRuntime().activeEventDialog();
            require(!dialog.actions.empty(), "Real visible transition actions missing");
            require(std::none_of(dialog.lines.begin(), dialog.lines.end(), [](const std::string &line)
            {
                return line.starts_with("It will take ");
            }), "Scripted travel incorrectly displays a walking-route duration");
        };
        const auto choose = [&](bool confirm)
        {
            auto &screen = game.m_gameSession.gameplayScreenRuntime();
            const auto &actions = screen.activeEventDialog().actions;
            const EventDialogActionKind kind = confirm
                ? EventDialogActionKind::MapTransitionConfirm : EventDialogActionKind::MapTransitionCancel;
            for (size_t index = 0; index < actions.size(); ++index)
            {
                if (actions[index].kind == kind)
                {
                    const Party::Snapshot before = scene().party().snapshot();
                    const double beforeMinutes = scene().worldRuntime().gameMinutes();
                    screen.eventDialogSelectionIndex() = index;
                    screen.executeActiveDialogAction();
                    // Compare before the application loads the destination: no simulation frame intervenes.
                    const Party::Snapshot after = scene().party().snapshot();
                    const double afterMinutes = scene().worldRuntime().gameMinutes();
                    travelLog << "{\"stage\":" << std::quoted(stage)
                        << ",\"confirm\":" << (confirm ? "true" : "false")
                        << ",\"game_minutes\":[" << std::setprecision(17) << beforeMinutes << ',' << afterMinutes
                        << "],\"food\":[" << before.food << ',' << after.food << "],\"members\":[";
                    require(before.members.size() == after.members.size(), "Travel changed party size");
                    for (size_t member = 0; member < before.members.size(); ++member)
                    {
                        const Character &first = before.members[member];
                        const Character &last = after.members[member];
                        if (member != 0) travelLog << ',';
                        travelLog << "{\"member_id\":" << first.memberId
                            << ",\"hp\":[" << first.health << ',' << last.health
                            << "],\"sp\":[" << first.spellPoints << ',' << last.spellPoints << "]}";
                        require(first.memberId == last.memberId && first.health == last.health
                            && first.spellPoints == last.spellPoints && first.conditions == last.conditions
                            && first.levelModifier == last.levelModifier
                            && first.armorClassModifier == last.armorClassModifier,
                            "Scripted travel healed, starved or changed a member");
                    }
                    travelLog << "]}\n";
                    travelLog.flush();
                    require(beforeMinutes == afterMinutes && before.food == after.food,
                        "Scripted travel charged implicit time or food");
                    const auto buffFields = [](const auto &buff)
                    {
                        return std::tuple(buff.remainingSeconds, buff.spellId, buff.skillLevel,
                            buff.skillMastery, buff.power, buff.casterMemberIndex);
                    };
                    for (size_t buff = 0; buff < PartyBuffCount; ++buff)
                    {
                        require(buffFields(before.partyBuffs[buff]) == buffFields(after.partyBuffs[buff]),
                            "Scripted travel changed a party buff");
                    }
                    for (size_t member = 0; member < before.characterBuffs.size(); ++member)
                    {
                        for (size_t buff = 0; buff < CharacterBuffCount; ++buff)
                        {
                            const auto &first = before.characterBuffs[member][buff];
                            const auto &last = after.characterBuffs[member][buff];
                            require(buffFields(first) == buffFields(last)
                                && first.periodicAccumulatorSeconds == last.periodicAccumulatorSeconds
                                && first.casterMemberId == last.casterMemberId,
                                "Scripted travel changed a personal buff");
                        }
                    }
                    if (confirm)
                    {
                        require(game.m_gameSession.pendingMapMove().has_value(),
                            "Real confirm did not transfer travel to the session");
                        require(game.processPendingMapMove(), "Confirmed travel failed");
                    }
                    else
                    {
                        require(!state().pendingDialogueContext && !state().pendingMapMove
                            && !game.m_gameSession.pendingMapMove(),
                            "Real cancel did not clear transition");
                    }
                    return;
                }
            }
            throw std::runtime_error("Actual travel action missing");
        };
        const auto tap = [&](SDL_Scancode key)
        {
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = key;
            event.key.key = SDL_GetKeyFromScancode(key, SDL_KMOD_NONE, false);
            event.key.down = true;
            event.key.repeat = false;
            if (SDL_Window *pWindow = SDL_GetKeyboardFocus())
            {
                event.key.windowID = SDL_GetWindowID(pWindow);
            }
            game.handleSdlEvent(event);
            event.type = SDL_EVENT_KEY_UP;
            event.key.down = false;
            game.handleSdlEvent(event);
        };

        Engine::EngineApplication app(config,
            [&](Engine::AssetFileSystem &files) { return game.loadGameData(files); },
            [&]()
            {
                started = SDL_GetTicks();
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Metal renderer required");
                require(game.initializeRenderer(), "Renderer initialization failed");
                require(game.loadSessionFromPath(seed), "Read-only seed load failed");
                game.m_gameSession.gameplayScreenRuntime().closeActiveEventDialog();
                if (fresh)
                {
                    const MapStatsEntry *pMap = game.m_gameDataLoader.getMapStats().findByFileName("out14.odm");
                    require(pMap != nullptr && pMap->outdoorBounds.enabled, "Avlee bounds unavailable");
                    require(!pMap->westTransition || pMap->westTransition->destinationMapFileName.empty(),
                        "Original Avlee west table must remain empty");
                    const int westStart = pMap->outdoorBounds.minX + 96;
                    game.m_pendingDebugMapJump = GameApplication::PendingDebugMapJump{.mapId = pMap->id};
                    require(game.processPendingDebugMapJump(), "Declared Avlee fixture jump failed");
                    Party &party = scene().party();
                    require(party.memberCount() == 5, "Use a declared five-member seed");
                    Party::Snapshot fixture = party.snapshot();
                    fixture.partyBuffs = {};
                    fixture.characterBuffs = {};
                    for (Character &member : fixture.members)
                    {
                        member.conditions.reset();
                        member.conditionStartGameMinutes = {};
                        member.health = member.maxHealth;
                        member.spellPoints = 0;
                    }
                    party.restoreSnapshot(fixture);
                    for (size_t index = 0; index < party.memberCount(); ++index)
                    {
                        while (party.removeItemFromMember(index, 1406)) {}
                        require(party.equippedItemId(index, EquipmentSlot::Armor) != 1406,
                            "Seed must not have equipped wetsuits; inventory-only fixture is intentional");
                        if (index < 4)
                        {
                            require(party.grantItemToMember(index, 1406), "Cannot grant declared inventory suit");
                        }
                    }
                    party.setQuestBit(642, true);
                    scene().partyRuntime().syncSpellMovementStatesFromPartyBuffs();
                    EventRuntimeState::PendingMapMove fixturePosition = {};
                    fixturePosition.x = westStart;
                    fixturePosition.y = -16562;
                    fixturePosition.z = 1;
                    fixturePosition.directionDegrees = 180;
                    fixturePosition.lookAngleDegrees = 0;
                    state().pendingMapMove = fixturePosition;
                    require(game.processPendingMapMove(), "Declared boundary position failed");
                    require(!scene().worldRuntime().isUnderwaterMap(), "Avlee incorrectly underwater");
                    require(!hasSuit(scene().party().members()[4]), "Fifth-member rejection fixture invalid");
                }
                else
                {
                    require(game.m_gameSession.currentMapFileName() == (coldWater ? "7out15.odm" : "out14.odm"),
                        "Cold input is not the requested underwater/return milestone");
                    require(scene().worldRuntime().isUnderwaterMap() == coldWater,
                        "Cold load did not derive swimming from current environment");
                    const auto loaded = loadGameDataFromPath(seed, error);
                    require(loaded.has_value() && loaded->hasOutdoorRuntimeState,
                        "Cold seed has no saved outdoor runtime: " + error);
                    const OutdoorMoveState &savedPosition = loaded->outdoorParty.movementState;
                    std::ofstream(output / "cold-position.json") << std::setprecision(9)
                        << "{\"saved\":[" << savedPosition.x << ',' << savedPosition.y << ',' << savedPosition.footZ
                        << "],\"actual\":[" << movement().x << ',' << movement().y << ',' << movement().footZ
                        << "]}\n";
                    require(std::abs(movement().x - savedPosition.x) < 0.5f
                        && std::abs(movement().y - savedPosition.y) < 0.5f
                        && std::abs(movement().footZ - savedPosition.footZ) < 0.5f,
                        "Cold load changed the saved swimming/return position");
                }
                for (const Character &member : scene().party().members())
                {
                    memberIds.push_back(member.memberId);
                }
                checkNoFlight();
                save("fixture", !fresh);
                next("warmup");
                return true;
            },
            [&](const SDL_Event &event)
            {
                // Movement comes from the declared synthetic input; retain quit and window lifecycle events.
                if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP
                    && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_WHEEL)
                {
                    game.handleSdlEvent(event);
                }
            },
            [&](int width, int height, float, float)
            {
                require(SDL_GetTicks() - started < 120000 && frames < 7200, "Bounded Shoals capture timeout: " + stage);
                ++frames;
                const bool capturing = !pendingCapture.empty();
                SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
                if (!capturing)
                {
                    if (stage.starts_with("approach") || stage.starts_with("hold_boundary")) key = SDL_SCANCODE_W;
                    if (stage.starts_with("inward")) key = SDL_SCANCODE_S;
                    if (stage == "swim_up" || stage.starts_with("surface_up") || stage == "hold_surface"
                        || stage == "land_up") key = SDL_SCANCODE_J;
                    if (stage == "swim_down" || stage == "surface_down") key = SDL_SCANCODE_K;
                    if (key != SDL_SCANCODE_UNKNOWN) tap(key);
                }
                game.renderFrame(width, height, 0.0f, capturing ? 0.0f : StepSeconds);
                checkNoFlight();
                if (key != SDL_SCANCODE_UNKNOWN)
                {
                    const KeyboardAction action = key == SDL_SCANCODE_W ? KeyboardAction::Forward
                        : key == SDL_SCANCODE_S ? KeyboardAction::Backward
                        : key == SDL_SCANCODE_J ? KeyboardAction::FlyUp : KeyboardAction::FlyDown;
                    require(game.m_gameInputSystem.frame().isActionActive(action),
                        "Synthetic key did not pass actual action mapping");
                }

                if (capturing)
                {
                    if (auto shot = Engine::BgfxContext::consumeScreenshot(pendingCapture))
                    {
                        SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(shot->width), int(shot->height),
                            SDL_PIXELFORMAT_BGRA32, shot->bgraPixels.data(), int(shot->width * 4));
                        require(pSurface != nullptr, "Cannot create capture surface");
                        const bool saved = SDL_SaveBMP(pSurface, (output / (pendingCapture + ".bmp")).string().c_str());
                        SDL_DestroySurface(pSurface);
                        require(saved, "Cannot save screenshot");
                        pendingCapture.clear();
                        ++captures;
                    }
                    return;
                }

                stageSeconds += StepSeconds;
                require(stageSeconds < 8.0f, "Movement stalled at " + stage + "; inspect movement.jsonl");
                auto &screen = game.m_gameSession.gameplayScreenRuntime();
                const bool statusMatched = screen.statusBarEventText() == MissingSuitMessage
                    && screen.statusBarEventRemainingSeconds() > 0.0f;
                const float statusSeconds = screen.statusBarEventRemainingSeconds();
                if (statusMatched && (!previousStatusMatched || statusSeconds > previousStatusSeconds + 0.05f))
                {
                    ++missingNotifications;
                    record("observed-missing-suit-status");
                }
                previousStatusMatched = statusMatched;
                previousStatusSeconds = statusSeconds;
                if (scene().partyRuntime().movementEvents().blockedBoundaryEdge == MapBoundaryEdge::West)
                {
                    ++boundaryContactFrames;
                }
                framesLog << "{\"frame\":" << frames << ",\"stage\":" << std::quoted(stage)
                    << ",\"key\":" << int(key) << ",\"x\":" << movement().x << ",\"y\":" << movement().y
                    << ",\"z\":" << movement().footZ << ",\"status_seconds\":" << statusSeconds
                    << ",\"sp\":[";
                for (size_t index = 0; index < scene().party().memberCount(); ++index)
                {
                    if (index != 0) framesLog << ',';
                    framesLog << scene().party().members()[index].spellPoints;
                }
                framesLog << "],\"prompt\":" << (state().pendingDialogueContext ? "true" : "false") << "}\n";
                framesLog.flush();

                if (stage == "warmup" && stageSeconds >= 0.25f)
                {
                    if (fresh) next("approach_missing_first");
                    else if (coldWater) next("swim_up");
                    else next("land_settle");
                }
                else if (stage == "approach_missing_first" && missingNotifications > 0)
                {
                    require(missingNotifications == 1 && boundaryContactFrames > 0 && movement().x < stageX - 8,
                        "First refusal did not follow real boundary movement");
                    require(!state().pendingDialogueContext, "Fifth member without suit was admitted");
                    contactX = movement().x;
                    capture("fifth-member-missing-suit");
                    next("hold_boundary_missing_first");
                }
                else if (stage == "hold_boundary_missing_first")
                {
                    require(missingNotifications == 1 && !state().pendingDialogueContext,
                        "Held boundary spammed refusal");
                    if (stageSeconds >= 1.0f)
                    {
                        require(statusSeconds <= stageStatusSeconds - 0.8f,
                            "Held boundary continuously refreshed identical status text");
                        next("inward_missing_retry");
                    }
                }
                else if (stage.starts_with("inward") && movement().x > contactX + 64.0f)
                {
                    if (stage == "inward_missing_retry") next("approach_missing_second");
                    else if (stage == "inward_suited") next("approach_entry_first");
                    else next("approach_entry_second");
                }
                else if (stage == "approach_missing_second" && missingNotifications > 1)
                {
                    require(missingNotifications == 2 && !state().pendingDialogueContext, "Missing-suit retry failed");
                    require(scene().party().grantItemToMember(4, 1406), "Cannot grant declared fifth suit");
                    contactX = movement().x;
                    save("all-five-suited", true);
                    next("inward_suited");
                }
                else if (stage == "approach_entry_first" && state().pendingDialogueContext)
                {
                    checkPrompt(true);
                    contactX = movement().x;
                    capture("source-entry-icon8");
                    next("cancel_entry");
                }
                else if (stage == "cancel_entry")
                {
                    choose(false);
                    next("hold_boundary_cancelled");
                }
                else if (stage == "hold_boundary_cancelled")
                {
                    require(!state().pendingDialogueContext && missingNotifications == 2,
                        "Cancelled boundary prompt reopened without moving away");
                    if (stageSeconds >= 1.0f) next("inward_entry_retry");
                }
                else if (stage == "approach_entry_second" && state().pendingDialogueContext)
                {
                    checkPrompt(true);
                    choose(true);
                    require(game.m_gameSession.currentMapFileName() == "7out15.odm"
                        && scene().worldRuntime().isUnderwaterMap(), "Source entry did not establish underwater map");
                    save("underwater-arrival", true);
                    next("water_arrival");
                }
                else if (stage == "water_arrival" && stageSeconds >= 0.25f)
                {
                    capture("underwater-source-arrival");
                    next("swim_up");
                }
                else if (stage == "swim_up" && movement().footZ > stageZ + 220.0f)
                {
                    require(!state().pendingDialogueContext, "Initial controlled swim unexpectedly reached surface");
                    capture("normal-input-swim-up");
                    next("swim_down");
                }
                else if (stage == "swim_down" && movement().footZ < stageZ - 120.0f)
                {
                    save("underwater-swimming", true);
                    next("surface_up_first");
                }
                else if (stage == "surface_up_first" && state().pendingDialogueContext)
                {
                    require(movement().footZ > 3900.0f, "Surface prompt appeared below source threshold");
                    checkPrompt(false);
                    ++surfacePrompts;
                    capture("source-surface-icon8");
                    next("cancel_surface");
                }
                else if (stage == "cancel_surface")
                {
                    choose(false);
                    next("hold_surface");
                }
                else if (stage == "hold_surface")
                {
                    require(!state().pendingDialogueContext, "Cancelled surface prompt reopened above threshold");
                    if (stageSeconds >= 1.25f) next("surface_down");
                }
                else if (stage == "surface_down" && movement().footZ <= 3600.0f)
                {
                    next("surface_rearm_wait");
                }
                else if (stage == "surface_rearm_wait" && stageSeconds >= 1.25f)
                {
                    require(movement().footZ <= 3900 && !state().pendingDialogueContext, "Surface rearm drifted");
                    next("surface_up_second");
                }
                else if (stage == "surface_up_second" && state().pendingDialogueContext)
                {
                    checkPrompt(false);
                    ++surfacePrompts;
                    choose(true);
                    require(game.m_gameSession.currentMapFileName() == "out14.odm"
                        && !scene().worldRuntime().isUnderwaterMap(), "Surface return retained underwater environment");
                    next("land_settle");
                }
                else if (stage == "land_settle" && stageSeconds >= 0.5f)
                {
                    capture(coldReturn ? "cold-return-avlee" : "surface-return-avlee");
                    next("land_up");
                }
                else if (stage == "land_up")
                {
                    require(movement().footZ <= stageZ + 8.0f, "Swim ascent leaked into ordinary Avlee");
                    if (stageSeconds >= 1.0f)
                    {
                        require(coldReturn || surfacePrompts == 2, "Surface cancellation/rearm incomplete");
                        require(!fresh || (missingNotifications == 2 && boundaryContactFrames > 2),
                            "Missing real boundary/hold evidence");
                        save("final", true);
                        record("complete");
                        complete = true;
                        SDL_Event quit = {};
                        quit.type = SDL_EVENT_QUIT;
                        SDL_PushEvent(&quit);
                    }
                }
            },
            [&]() { game.shutdownApplication(); });
        require(app.run() == 0 && complete && pendingCapture.empty(), "Shoals capture incomplete");
        std::ofstream(output / "result.json")
            << "{\"status\":\"passed\",\"controlled\":true,\"natural_campaign\":false,\"mode\":"
            << std::quoted(mode) << ",\"frames\":" << frames << ",\"captures\":" << captures
            << ",\"missing_suit_notifications\":" << missingNotifications
            << ",\"boundary_contact_frames\":" << boundaryContactFrames
            << ",\"surface_prompts\":" << surfacePrompts
            << ",\"actual_lincoln_traversal\":false,\"physical_keyboard_hold\":false,\"QuestFinished\":false}\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "mm7_shoals_capture RUNTIME READONLY_SEED NEW_OUTPUT loop|reload-underwater|reload-return\n";
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
