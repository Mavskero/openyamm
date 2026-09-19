#include "engine/BgfxContext.h"
#include "game/app/GameApplication.h"
#include "game/events/EvtEnums.h"
#include "game/gameplay/GameplayHudInputController.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/CorpseLootRuntime.h"
#include "game/gameplay/HouseServiceRuntime.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/gameplay/ReputationRuntime.h"
#include "game/gameplay/StoryTextFormatter.h"
#include "game/tables/MergedBaseTables.h"
#include "game/items/ItemRuntime.h"
#include "game/items/PriceCalculator.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayPartyOverlayInputController.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/scene/IndoorSceneRuntime.h"
#include "game/indoor/IndoorGeometryUtils.h"
#include "game/indoor/IndoorPathfindingBuilder.h"
#include "game/pathfinding/PathPlanner.h"
#include "game/ui/SpellbookUiLayout.h"
#include "game/ui/CharacterSkillRows.h"
#include "game/ui/CharacterSkillsLayout.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/screens/CutsceneVideoScreen.h"
#include "game/ui/screens/NewGameScreen.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace OpenYAMM::Game
{
struct NewGameScreenTestAccess
{
    static void prepareNormalMm8Knight(NewGameScreen &screen)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        screen.selectContinent("jadame");
        require(screen.m_stage == NewGameScreen::FlowStage::CharacterCreation && screen.candidateCount() > 0,
            "Normal Jadame character creation is unavailable");
        for (size_t i = 0; i < screen.selectedCandidate().availableClassIds.size()
            && screen.selectedClassName() != "Knight"; ++i)
        {
            screen.cycleClass(1);
        }
        require(screen.selectedClassName() == "Knight" && !screen.isGodLichSelected(),
            "Full replay requires a normal level-one Knight");
        for (size_t i = 0; i < size_t(NewGameScreen::StatId::Count); ++i)
        {
            while (screen.tryIncreaseStat(NewGameScreen::StatId(i))) {}
        }
        require(screen.currentBonusPool() == 0 && screen.m_state.optionalSkills.size() >= 2,
            "Normal creation could not allocate its original bonus pool");
        const std::vector<std::string> options = screen.m_state.optionalSkills;
        require(screen.tryToggleOptionalSkill(options[0]) && screen.tryToggleOptionalSkill(options[1]),
            "Normal creation could not select two original optional skills");
        screen.beginNameEditing();
        while (!screen.m_state.nameEditBuffer.empty())
        {
            SDL_Event key = {};
            key.type = SDL_EVENT_KEY_DOWN;
            key.key.key = SDLK_BACKSPACE;
            screen.handleSdlEvent(key);
        }
        SDL_Event release = {};
        release.type = SDL_EVENT_KEY_UP;
        release.key.key = SDLK_BACKSPACE;
        screen.handleSdlEvent(release);
        SDL_Event text = {};
        text.type = SDL_EVENT_TEXT_INPUT;
        text.text.text = "中文骑士";
        screen.handleSdlEvent(text);
        screen.endNameEditing(true);
        require(screen.m_state.name == "中文骑士", "Normal character creation lost committed Chinese text");
    }

    static void prepareNormalClassicParty(NewGameScreen &screen, bool mm6)
    {
        const auto require = [](bool ok, const char *message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        screen.selectContinent(mm6 ? "enroth" : "antagarich");
        require(screen.m_stage == NewGameScreen::FlowStage::CharacterCreation && screen.candidateCount() > 0,
            "Normal classic character creation is unavailable");
        constexpr std::array<const char *, 4> classes = {"Knight", "Cleric", "Sorcerer", "Archer"};
        constexpr std::array<const char *, 4> names = {"探路骑士", "晨光牧师", "火焰法师", "远行弓手"};
        for (size_t slot = 0; slot < classes.size(); ++slot)
        {
            if (slot != 0) screen.addPartySlot();
            require(screen.m_partySize == slot + 1 && screen.m_activePartySlot == slot,
                "Normal creation did not add and select the requested party slot");
            for (size_t i = 0; i < screen.candidateCount()
                && screen.selectedCandidate().characterDataId != 38 + slot; ++i)
                screen.cycleCandidate(1);
            require(screen.selectedCandidate().characterDataId == 38 + slot && !screen.isGodLichSelected(),
                "Original classic portrait is unavailable");
            for (size_t i = 0; i < screen.selectedCandidate().availableClassIds.size()
                && screen.selectedClassName() != classes[slot]; ++i)
                screen.cycleClass(1);
            require(screen.selectedClassName() == classes[slot], "Original classic starter class is unavailable");
            const auto primary = slot == 1 ? NewGameScreen::StatId::Personality
                : slot == 2 ? NewGameScreen::StatId::Intellect : NewGameScreen::StatId::Might;
            while (screen.tryIncreaseStat(primary)) {}
            for (size_t stat = 0; stat < size_t(NewGameScreen::StatId::Count); ++stat)
                while (screen.tryIncreaseStat(NewGameScreen::StatId(stat))) {}
            const std::vector<std::string> options = screen.m_state.optionalSkills;
            require(screen.currentBonusPool() == 0 && options.size() >= 2
                && screen.tryToggleOptionalSkill(options[0]) && screen.tryToggleOptionalSkill(options[1]),
                "Normal classic creation could not allocate its points and optional skills");
            screen.beginNameEditing();
            while (!screen.m_state.nameEditBuffer.empty())
            {
                SDL_Event key = {};
                key.type = SDL_EVENT_KEY_DOWN;
                key.key.key = SDLK_BACKSPACE;
                screen.handleSdlEvent(key);
            }
            SDL_Event release = {};
            release.type = SDL_EVENT_KEY_UP;
            release.key.key = SDLK_BACKSPACE;
            screen.handleSdlEvent(release);
            SDL_Event text = {};
            text.type = SDL_EVENT_TEXT_INPUT;
            text.text.text = names[slot];
            screen.handleSdlEvent(text);
            screen.endNameEditing(true);
            require(screen.m_state.name == names[slot], "Classic creation lost the committed Chinese name");
        }
    }

    static void confirm(NewGameScreen &screen)
    {
        screen.confirmCreation();
    }
};

struct ApplicationCaptureTestAccess
{
    static void pressQuestGameplay(GameSession &session, int width, int height,
        KeyboardAction action, float delta = 0.0f)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.actions[keyboardActionIndex(action)] = {true, true, false};
        session.updateGameplay(input, delta);
        session.updateGameplay({}, 0.0f);
    }

    static void selectQuestDialog(GameplayScreenRuntime &runtime, EventDialogActionKind kind, uint32_t id)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        const EventDialogContent &dialog = runtime.activeEventDialog();
        require(dialog.isActive, "Quest dialogue is not active");
        const auto action = std::find_if(dialog.actions.begin(), dialog.actions.end(),
            [&](const EventDialogAction &candidate) { return candidate.kind == kind && candidate.id == id; });
        if (action == dialog.actions.end())
        {
            for (const EventDialogAction &candidate : dialog.actions)
                std::cout << "Available quest action: kind=" << int(candidate.kind)
                    << " id=" << candidate.id << " label=" << candidate.label << '\n';
            throw std::runtime_error("Required original quest action is absent: " + std::to_string(id));
        }
        require(action->enabled, "Original quest action is disabled");
        std::cout << "Quest action: kind=" << int(kind) << " id=" << id << " label=" << action->label << '\n';
        runtime.eventDialogSelectionIndex() = size_t(action - dialog.actions.begin());
        runtime.executeActiveDialogAction();
    }

    struct Mm8StartCheckState
    {
        unsigned phase = 0;
        size_t waypoint = 0;
        uint32_t phaseFrame = 0;
        unsigned stalledFrames = 0;
        float walkedDistance = 0.0f;
        bool complete = false;
    };

    static void equipQuestInventoryItem(GameSession &session, int width, int height, uint32_t id,
        size_t memberIndex = 0)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        require(pParty != nullptr && memberIndex < pParty->members().size()
            && runtime.selectedCharacterScreenSourceIndex() == memberIndex,
            "Equipment needs the selected party member");
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto *pLayout = runtime.findHudLayoutElement("CharacterInventoryGrid");
        require(pLayout != nullptr, "Original inventory grid is absent");
        const auto grid = runtime.resolveHudLayoutElement(pLayout->id, width, height,
            pLayout->width, pLayout->height);
        require(grid.has_value(), "Original inventory grid cannot be placed");
        bool picked = false;
        for (const GameplayRenderedInspectableHudItem &image : runtime.renderedInspectableHudItems())
        {
            if (image.objectDescriptionId != id || image.sourceMemberIndex != memberIndex
                || image.sourceType != GameplayUiController::ItemInspectSourceType::Inventory) continue;
            for (float y = std::max(grid->y + 1, image.y + 2); y < image.y + image.height && !picked; y += 3)
            {
                for (float x = std::max(grid->x + 1, image.x + 2); x < image.x + image.width; x += 3)
                {
                    if (x >= grid->x + grid->width || y >= grid->y + grid->height) continue;
                    const auto *pItem = pParty->members()[memberIndex].inventoryItemAt(
                        uint8_t((x - grid->x) * Character::InventoryWidth / grid->width),
                        uint8_t((y - grid->y) * Character::InventoryHeight / grid->height));
                    if (!pItem || pItem->objectDescriptionId != id
                        || !runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                    click(x, y);
                    picked = true;
                    break;
                }
            }
            if (picked) break;
        }
        require(picked && runtime.heldInventoryItem().active
            && runtime.heldInventoryItem().item.objectDescriptionId == id,
            "Normal inventory click did not pick the original equipment");
        const auto body = runtime.resolveHudLayoutElement("CharacterDollPanel", width, height, 175, 378);
        require(body.has_value(), "Original equipment panel is absent");
        click(body->x + body->width * 0.35f, body->y + body->height * 0.5f);
        require(!runtime.heldInventoryItem().active, "Normal body click did not equip or use the item");
    }

    struct Mm7RouteCheckState
    {
        bool potion = false;
        bool recruitRevisit = false;
        bool specialStock = false;
        int expectedGold = 100;
        int buyPrice = 0;
        uint32_t boughtItemId = 0;
        bool needsMix = false;
        uint32_t guideOpenedFrame = 0;
        unsigned guideCount = 0;
        unsigned phase = 0;
        size_t waypoint = 0;
        uint32_t phaseFrame = 0;
        unsigned stalledFrames = 0;
        float walkedDistance = 0.0f;
        std::vector<std::array<float, 2>> forwardRoute;
        std::vector<std::array<float, 2>> returnRoute;
        bool complete = false;
        size_t preparationStep = 0;
    };

    static void checkMm7Temple(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm7RouteCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && ((pParty->members().size() == 2 && pParty->hasRosterMember(42))
            || pParty->members().size() == 4)
            && pParty->hasActableMember(), "Moon Temple requires an original surviving starter party");
        const bool fourMembers = pParty->members().size() == 4;
        constexpr std::array<std::array<uint32_t, 3>, 9> preparation = {{{1, 66, 0}, {1, 99, 0}, {1, 467, 68},
            {2, 84, 0}, {2, 21, 0}, {2, 401, 2}, {3, 1, 0}, {3, 56, 0}, {3, 412, 13}}};
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.stalledFrames = 0;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "月亮神庙正常装备与入口", {}), "Cannot preserve Moon Temple route state");
        };
        if (frame == 1)
        {
            require(fourMembers ? (pParty->gold() == 200 && pParty->members().front().experience == 0
                && pParty->hasQuestBit(513) && pParty->hasQuestBit(514))
                : (pParty->gold() == 92 && pParty->members().front().experience == 1000
                    && pParty->hasQuestBit(520) && pParty->hasQuestBit(521)),
                "Moon Temple input lost original resources or scavenger progress");
            if (reload)
            {
                require(session.currentMapFileName() == "7d06.blv", "Moon Temple reload needs the real entry save");
                next(7);
            }
            else
            {
                require(session.currentMapFileName() == "7out01.odm"
                    && pParty->members().front().equipment.mainHand == 0
                    && pParty->members().front().equipment.armor == 0,
                    "Moon Temple route must begin before the original Knight equips his items");
                std::ifstream stream(std::filesystem::current_path().parent_path() / "mm7-temple-forward.tsv");
                require(stream.good(), "Moon Temple requires its normal outdoor route");
                std::string line;
                while (std::getline(stream, line))
                {
                    if (line.empty() || line.front() == '#') continue;
                    std::istringstream values(line);
                    float x = 0, y = 0, z = 0;
                    require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                        && std::isfinite(z), "Invalid Moon Temple waypoint");
                    state.forwardRoute.push_back({x, y});
                    require(state.forwardRoute.size() <= 2000, "Moon Temple route exceeds waypoint limit");
                }
                require(state.forwardRoute.size() >= 2, "Moon Temple route is empty");
            }
        }
        if (state.phase == 4)
        {
            const auto &dialog = runtime.activeEventDialog();
            if (dialog.isActive)
            {
                if (dialog.presentation == EventDialogPresentation::Transition)
                {
                    next(6);
                    return;
                }
                require(dialog.sourceId >= 358 && dialog.sourceId <= 376 && !dialog.isHouseDialog,
                    "Unexpected dialogue on the normal Moon Temple approach");
                if (state.guideOpenedFrame == 0)
                {
                    state.guideOpenedFrame = frame;
                    require(++state.guideCount <= 30, "MM7 walking guide repeats without movement");
                    captures.push_back("mm7-temple-guide-" + std::to_string(state.guideCount));
                }
                if (frame >= state.guideOpenedFrame + 12) close();
                state.stalledFrames = 0;
                return;
            }
            state.guideOpenedFrame = 0;
            require(session.currentMapFileName() == "7out01.odm", "Temple approach left Emerald Island unexpectedly");
            const auto target = state.forwardRoute[state.waypoint];
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = target[0] - x, dy = target[1] - y;
            const float remaining = std::hypot(dx, dy);
            if (remaining < 5.0f)
            {
                if (++state.waypoint == state.forwardRoute.size()) next(5);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 30)
                std::cout << "MM7 temple walk: waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " target=" << target[0] << ',' << target[1] << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal temple approach stopped advancing");
            return;
        }
        if (frame == state.phaseFrame + 4)
        {
            if (state.phase == 3) captures.push_back("mm7-temple-equipped");
            if (state.phase == 6) captures.push_back("mm7-temple-door");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 0:
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "Original Knight portrait is unavailable");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned click = 0; click < 2; ++click)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == 0,
                "Normal portrait clicks did not open the Knight inventory");
            next(1);
            break;
        }
        case 1: equipQuestInventoryItem(session, width, height, 84); next(2); break;
        case 2: equipQuestInventoryItem(session, width, height, 1); next(3); break;
        case 3:
            require(pParty->members()[0].equipment.mainHand == 1 && pParty->members()[0].equipment.armor == 84,
                "Original sword and leather armor are not equipped");
            save("temple-equipped");
            close();
            require(!runtime.characterScreen().open, "Normal Escape did not close equipment");
            next(fourMembers ? 8 : 4);
            break;
        case 8:
        {
            const size_t member = preparation[state.preparationStep][0];
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, member);
            require(portrait.has_value(), "Normal party preparation has no selected portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned click = 0; click < 2; ++click)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == member,
                "Normal portrait clicks did not open the next party member");
            next(9);
            break;
        }
        case 9:
        {
            const auto action = preparation[state.preparationStep];
            const int before = pParty->inventoryItemCount(action[1]);
            equipQuestInventoryItem(session, width, height, action[1], action[0]);
            require(action[2] == 0 || (pParty->members()[action[0]].knowsSpell(action[2])
                && pParty->inventoryItemCount(action[1]) == before - 1),
                "The original book did not teach its actual spell and consume one item");
            std::cout << "MM7 preparation: member=" << action[0] << " item=" << action[1]
                << " learned_spell=" << action[2] << " remaining=" << pParty->inventoryItemCount(action[1]) << '\n';
            next(10);
            break;
        }
        case 10:
            close();
            require(!runtime.characterScreen().open, "Normal preparation did not close the inventory");
            if (++state.preparationStep == preparation.size())
            {
                require(pParty->members()[1].knowsSpell(68) && pParty->members()[2].knowsSpell(2)
                    && pParty->members()[3].equipment.bow == 56, "Original four-member preparation is incomplete");
                save("mm7-temple-prepared");
                captures.push_back("mm7-temple-prepared");
                next(4);
            }
            else next(8);
            break;
        case 5:
            game.m_outdoorGameView.setCameraAngles(0.0f, 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(6);
            break;
        case 6:
        {
            const auto &dialog = runtime.activeEventDialog();
            require(dialog.isActive && dialog.presentation == EventDialogPresentation::Transition,
                "Normal approach did not offer entry to the Moon Temple");
            const auto action = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                [](const auto &candidate) { return candidate.kind == EventDialogActionKind::MapTransitionConfirm; });
            require(action != dialog.actions.end(), "Moon Temple transition has no normal confirmation");
            save("temple-door");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, action->id);
            next(7);
            break;
        }
        case 7:
            require(session.currentMapFileName() == "7d06.blv"
                && dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get()) != nullptr,
                "Normal transition did not enter the original Moon Temple");
            require(pParty->members()[0].equipment.mainHand == 1 && pParty->members()[0].equipment.armor == 84
                && pParty->gold() == (fourMembers ? 200 : 92) && !runtime.heldInventoryItem().active,
                "Moon Temple entry changed the original equipment or resources");
            save("mm7-temple-complete");
            captures.push_back("mm7-temple-complete");
            state.complete = true;
            std::cout << "MM7 temple entry complete: reload=" << reload << " walked=" << state.walkedDistance
                << " gold=" << pParty->gold() << " members=" << pParty->members().size() << '\n';
            break;
        default:
            throw std::runtime_error("Unknown Moon Temple entry phase");
        }
    }

    static void checkMm7Recruit(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm7RouteCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        auto *pWorld = dynamic_cast<OutdoorWorldRuntime *>(session.activeWorldRuntime());
        require(pParty && pWorld && session.currentMapFileName() == "7out01.odm" && pParty->hasActableMember(),
            "MM7 recruitment needs the normal surviving Emerald Island party");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.stalledFrames = 0;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "翡翠岛正常招募", {}), "Cannot preserve MM7 recruitment state");
        };
        const auto requireParty = [&]()
        {
            require(pParty->members().size() == 2 && pParty->hasRosterMember(42)
                && pParty->members().back().rosterId == 42 && pParty->members().front().level == 1
                && pParty->members().front().experience == 1000 && pParty->gold() == 92
                && pParty->hasQuestBit(520) && pParty->hasQuestBit(521) && !pParty->hasQuestBit(519),
                "Recruitment changed the original party, resources or scavenger progress");
        };
        if (frame == 1)
        {
            if (reload)
            {
                requireParty();
                next(1);
            }
            else
            {
                require(pParty->members().size() == 1 && pParty->gold() == 92
                    && pParty->members().front().experience == 1000,
                    "MM7 recruitment must start from the normal post-potion single-character save");
                std::ifstream stream(std::filesystem::current_path().parent_path() / "mm7-recruit-forward.tsv");
                require(stream.good(), "MM7 recruitment requires its planned forward route");
                std::string line;
                while (std::getline(stream, line))
                {
                    if (line.empty() || line.front() == '#') continue;
                    std::istringstream values(line);
                    float x = 0, y = 0, z = 0;
                    require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                        && std::isfinite(z), "Invalid recruitment route waypoint");
                    state.forwardRoute.push_back({x, y});
                    require(state.forwardRoute.size() <= 2000, "Recruitment route exceeds waypoint limit");
                }
                require(state.forwardRoute.size() >= 2, "Recruitment route is empty");
            }
        }
        if (state.phase == 0)
        {
            const EventDialogContent &dialog = runtime.activeEventDialog();
            if (dialog.isActive)
            {
                require(dialog.sourceId >= 358 && dialog.sourceId <= 376 && !dialog.isHouseDialog,
                    "Unexpected dialogue on normal tavern route");
                if (state.guideOpenedFrame == 0)
                {
                    state.guideOpenedFrame = frame;
                    require(++state.guideCount <= 30, "MM7 walking guide repeats without movement");
                    captures.push_back("mm7-recruit-guide-" + std::to_string(state.guideCount));
                    for (const std::string &line : dialog.lines) std::cout << "MM7 guide: " << line << '\n';
                }
                if (frame >= state.guideOpenedFrame + 12) close();
                state.stalledFrames = 0;
                return;
            }
            state.guideOpenedFrame = 0;
            const auto target = state.forwardRoute[state.waypoint];
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = target[0] - x, dy = target[1] - y;
            const float remaining = std::hypot(dx, dy);
            if (remaining < 5.0f)
            {
                if (++state.waypoint == state.forwardRoute.size()) next(1);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 30)
                std::cout << "MM7 recruit walk: waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " target=" << target[0] << ',' << target[1] << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal recruitment walk stopped advancing");
            return;
        }
        if (frame == state.phaseFrame + 4)
        {
            if (state.phase == 4) captures.push_back("mm7-recruit-offer");
            if (state.phase == 5) captures.push_back("mm7-recruit-joined");
            if (state.phase == 8) captures.push_back("mm7-recruit-reopened");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            game.m_outdoorGameView.setCameraAngles(3.1415926536f, 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 239,
                "Normal Trigger did not open the original Emerald Island tavern");
            if (reload || state.recruitRevisit)
            {
                requireParty();
                save("recruit-reopened");
                const auto &actions = runtime.activeEventDialog().actions;
                require(std::none_of(actions.begin(), actions.end(),
                    [](const EventDialogAction &action)
                    { return action.kind == EventDialogActionKind::HouseResident && action.id == 303; }),
                    "Recruited companion is still available as a tavern resident");
                require(pWorld->eventRuntimeState()->generatedMercenaryRecruitsByNpcId.empty(),
                    "Tavern generated another mercenary before the original monthly refill");
                next(8);
            }
            else
            {
                save("recruit-before");
                selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 303);
                next(3);
            }
            break;
        case 3:
            selectQuestDialog(runtime, EventDialogActionKind::GeneratedMercenaryJoinOffer, 303);
            next(4);
            break;
        case 4:
        {
            for (const std::string &line : runtime.activeEventDialog().lines)
                std::cout << "MM7 recruit offer: " << line << '\n';
            const auto &actions = runtime.activeEventDialog().actions;
            const auto accept = std::find_if(actions.begin(), actions.end(), [](const EventDialogAction &action)
                { return action.kind == EventDialogActionKind::RosterJoinAccept; });
            require(accept != actions.end(), "Generated recruit has no normal acceptance action");
            selectQuestDialog(runtime, EventDialogActionKind::RosterJoinAccept, accept->id);
            next(5);
            break;
        }
        case 5:
            requireParty();
            save("recruit-joined");
            next(6);
            break;
        case 6:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                state.recruitRevisit = true;
                next(1);
            }
            break;
        case 8:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                requireParty();
                save("mm7-recruit-complete");
                captures.push_back("mm7-recruit-complete");
                state.complete = true;
                std::cout << "MM7 recruit route complete: reload=" << reload
                    << " walked=" << state.walkedDistance << " gold=" << pParty->gold()
                    << " members=" << pParty->members().size() << '\n';
            }
            break;
        default:
            throw std::runtime_error("Unknown MM7 recruitment phase");
        }
    }

    struct Mm7ContestCheckState : Mm7RouteCheckState
    {
        std::array<bool, 6> completedBefore = {};
        std::vector<uint64_t> experienceBefore;
        std::vector<size_t> deliveries;
        size_t delivered = 0;
        bool healFirst = false;
        bool healingComplete = false;
        size_t healingMember = 0;
        int healingPrice = 0;
        int foodBeforeWaiting = 0;
        float minutesBeforeWaiting = 0.0f;
        bool departing = false;
        bool departureComplete = false;
        uint32_t departureVideoFrames = 0;
        uint32_t departureVideoEndFrame = 0;
        bool claimAward = false;
        bool awardApplied = false;
        bool victoryBefore = false;
        bool craftRedPotion = false;
        int bottlesBeforeCrafting = 0;
        int reagentsBeforeCrafting = 0;
        uint32_t craftingReagent = 0;
        size_t craftingMember = 0;
        uint32_t purchaseItem = 0;
        uint32_t purchaseNpc = 0;
        uint32_t purchaseOffer = 0;
        uint32_t purchaseAccept = 0;
        int purchaseCost = 0;
        size_t purchaseTask = 0;
        bool purchaseComplete = false;
        uint32_t wellEvent = 0;
        std::array<float, 3> wellTarget = {};
        unsigned wellDrinks = 0;
        int wellInitialLuck = 0;
        std::array<uint8_t, 3> wellInitialVars = {};
    };

    static void checkMm7Contest(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm7ContestCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        constexpr std::array<uint32_t, 6> items = {222, 1437, 845, 1438, 1434, 1433};
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        auto *pWorld = dynamic_cast<OutdoorWorldRuntime *>(session.activeWorldRuntime());
        if (frame == 1 && reload && session.currentMapFileName() == "7out02.odm")
            state.departureComplete = true;
        require(pParty && pWorld && (session.currentMapFileName() == "7out01.odm"
                || ((state.departing || state.departureComplete) && session.currentMapFileName() == "7out02.odm"))
            && pParty->members().size() == 4 && pParty->hasActableMember(),
            "Contest delivery requires the original four-member Emerald Island party");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.stalledFrames = 0;
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "翡翠岛正常寻宝提交", {}), "Cannot preserve the original contest delivery");
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto clickButton = [&](const char *pId)
        {
            const auto *pLayout = runtime.findHudLayoutElement(pId);
            require(pLayout != nullptr, "The original rest button is missing");
            const auto rect = runtime.resolveHudLayoutElement(pId, width, height, pLayout->width, pLayout->height);
            require(rect.has_value(), "The original rest button has no screen rectangle");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = rect->x + rect->width * 0.5f;
            input.pointerY = rect->y + rect->height * 0.5f;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto verify = [&]()
        {
            require(pParty->gold() == state.expectedGold && !runtime.heldInventoryItem().active,
                "Contest delivery changed gold or left a held item");
            for (size_t index = 0; index < pParty->members().size(); ++index)
                require(pParty->members()[index].experience == state.experienceBefore[index] + state.delivered * 500
                        + (state.awardApplied ? 1000 : 0),
                    "Original contest reward did not give each member exactly 500 experience per item");
            for (size_t index = 0; index < items.size(); ++index)
            {
                const bool delivered = std::find(state.deliveries.begin(),
                    state.deliveries.begin() + state.delivered, index) != state.deliveries.begin() + state.delivered;
                const bool completed = state.completedBefore[index] || delivered;
                require(pParty->hasQuestBit(520 + index) == completed
                    && pParty->hasQuestBit(513 + index) == !completed,
                    "Contest completion bits differ from the original item order");
                if (delivered) require(pParty->inventoryItemCount(items[index]) == 0,
                    "The judge did not consume the delivered quest item");
            }
        };
        if (frame == 1)
        {
            state.victoryBefore = pParty->hasQuestBit(519);
            require(!state.victoryBefore || (pParty->hasQuestBit(529) && pParty->hasQuestBit(527)),
                "Victory revisit requires the normally claimed castle and departure arrangement");
            state.expectedGold = pParty->gold();
            for (const Character &member : pParty->members())
            {
                state.experienceBefore.push_back(member.experience);
            }
            for (size_t index = 0; index < items.size(); ++index)
            {
                state.completedBefore[index] = pParty->hasQuestBit(520 + index);
                require(pParty->hasQuestBit(513 + index) != state.completedBefore[index],
                    "Input is missing an original contest task state");
                if (!state.completedBefore[index] && pParty->inventoryItemCount(items[index]) > 0)
                {
                    require(!reload && pParty->inventoryItemCount(items[index]) == 1,
                        "Contest input must contain exactly one of each planned item");
                    state.deliveries.push_back(index);
                }
            }
            if (reload)
            {
                if (state.departureComplete)
                {
                    require(state.victoryBefore, "Harmondale reload requires the original contest victory");
                    verify();
                    save("mm7-contest-complete");
                    captures.push_back("mm7-contest-complete");
                    state.complete = true;
                    std::cout << "MM7 departure checkpoint reloaded in Harmondale without repeating travel\n";
                    return;
                }
                require(std::hypot(pWorld->partyX() - 16080.0f, pWorld->partyY() - 8560.0f) < 30.0f,
                    "Contest revisit must start at the saved headquarters entrance");
                next(1);
            }
            else
            {
                std::ifstream stream(std::filesystem::current_path().parent_path() / "mm7-contest-forward.tsv");
                require(stream.good(), "Contest delivery requires its normal walking route");
                std::string line;
                while (std::getline(stream, line))
                {
                    if (line.empty() || line.front() == '#') continue;
                    if (line == "heal-first") { state.healFirst = true; continue; }
                    if (line == "depart-only") { state.departing = true; continue; }
                    if (line == "claim-award") { state.claimAward = true; continue; }
                    if (line == "craft-red-potion") { state.craftRedPotion = true; continue; }
                    if (line == "buy-shell" || line == "buy-instrument")
                    {
                        const bool shell = line == "buy-shell";
                        state.purchaseItem = shell ? 1437 : 1434;
                        state.purchaseNpc = shell ? 344 : 343;
                        state.purchaseOffer = shell ? 765 : 762;
                        state.purchaseAccept = shell ? 766 : 763;
                        state.purchaseCost = shell ? 100 : 500;
                        state.purchaseTask = shell ? 1 : 4;
                        continue;
                    }
                    if (line.starts_with("well-at-end "))
                    {
                        std::istringstream values(line.substr(12));
                        require(bool(values >> state.wellEvent >> state.wellTarget[0]
                            >> state.wellTarget[1] >> state.wellTarget[2])
                            && (state.wellEvent == 114 || state.wellEvent == 115), "Invalid original well target");
                        continue;
                    }
                    std::istringstream values(line);
                    float x = 0, y = 0, z = 0;
                    require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                        && std::isfinite(z), "Invalid contest waypoint");
                    state.forwardRoute.push_back({x, y});
                    require(state.forwardRoute.size() <= 2000, "Contest route exceeds waypoint limit");
                }
                require(state.forwardRoute.size() >= 2, "Contest route is empty");
                require(!state.deliveries.empty() || state.craftRedPotion
                    || state.purchaseItem != 0 || state.wellEvent != 0 || state.departing,
                    "There are no acquired items, original ingredients or a planned original journey");
                if (state.departing)
                    require(state.victoryBefore && !state.claimAward && state.deliveries.empty()
                        && !state.craftRedPotion && state.purchaseItem == 0 && state.wellEvent == 0,
                        "Boat departure requires the previously completed original castle award");
                if (state.purchaseItem != 0 || state.healFirst)
                {
                    require(state.healFirst || (!state.completedBefore[state.purchaseTask]
                        && pParty->inventoryItemCount(state.purchaseItem) == 0
                        && state.expectedGold >= state.purchaseCost),
                        "The original quest-item transaction is not available");
                    std::ifstream returning(std::filesystem::current_path().parent_path() / "mm7-contest-return.tsv");
                    require(returning.good(), "The quest purchase needs its return route to the judge");
                    while (std::getline(returning, line))
                    {
                        if (line.empty() || line.front() == '#') continue;
                        std::istringstream values(line);
                        float x = 0, y = 0, z = 0;
                        require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                            && std::isfinite(z), "Invalid shell return waypoint");
                        state.returnRoute.push_back({x, y});
                        require(state.returnRoute.size() <= 2000, "Shell return exceeds the waypoint limit");
                    }
                    require(state.returnRoute.size() >= 2, "Shell return route is empty");
                }
                if (state.craftRedPotion)
                {
                    require(!state.completedBefore[0] && pParty->inventoryItemCount(222) == 0,
                        "Normal crafting requires an outstanding red-potion task");
                    for (size_t member = 0; member < pParty->members().size() && state.craftingReagent == 0; ++member)
                    {
                        if (pParty->inventoryItemCount(220, member) == 0) continue;
                        for (const InventoryItem &item : pParty->members()[member].inventory)
                        {
                            if (runtime.mergedReagentSettingTable()->resultItemIdForReagent(item.objectDescriptionId)
                                != 222) continue;
                            state.craftingMember = member;
                            state.craftingReagent = item.objectDescriptionId;
                            break;
                        }
                    }
                    require(state.craftingReagent != 0, "Normal red-potion crafting has no original reagent and bottle");
                    state.reagentsBeforeCrafting = pParty->inventoryItemCount(state.craftingReagent);
                    state.bottlesBeforeCrafting = pParty->inventoryItemCount(220);
                    next(20);
                }
            }
            save("mm7-contest-input");
        }
        if (frame < 24) return;
        if (state.phase == 0)
        {
            const EventDialogContent &dialog = runtime.activeEventDialog();
            if (dialog.isActive)
            {
                require(dialog.sourceId >= 358 && dialog.sourceId <= 376 && !dialog.isHouseDialog,
                    "Unexpected dialogue on the original contest route");
                if (state.guideOpenedFrame == 0)
                {
                    state.guideOpenedFrame = frame;
                    require(++state.guideCount <= 30, "Walking guide repeats without advancement");
                    captures.push_back("mm7-contest-guide-" + std::to_string(state.guideCount));
                }
                if (frame >= state.guideOpenedFrame + 12) close();
                return;
            }
            state.guideOpenedFrame = 0;
            const auto target = state.forwardRoute[state.waypoint];
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = target[0] - x, dy = target[1] - y;
            const float remaining = std::hypot(dx, dy);
            if (remaining < 5.0f)
            {
                if (++state.waypoint == state.forwardRoute.size())
                    next(state.departing ? 60 : state.wellEvent != 0 ? 40
                        : state.purchaseItem != 0 && !state.purchaseComplete ? 30 : 1);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 30)
                std::cout << "MM7 contest walk: waypoint=" << state.waypoint << " position="
                    << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << '\n';
            require(state.stalledFrames < 60, "Normal contest route stopped at collision geometry");
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
        {
            const bool healer = state.healFirst && !state.healingComplete;
            const HouseEntry *pHouse = runtime.houseTable()->get(healer ? 310 : 1163);
            require(pHouse != nullptr, "The original headquarters opening hours are absent");
            if (!isHouseOpenAtGameMinute(*pHouse, pWorld->gameMinutes()))
            {
                state.minutesBeforeWaiting = pWorld->gameMinutes();
                state.foodBeforeWaiting = pParty->food();
                save("mm7-contest-before-dawn");
                pressQuestGameplay(session, width, height, KeyboardAction::Rest);
                require(runtime.restScreenState().active, "Normal rest menu did not open before waiting for dawn");
                captures.push_back("mm7-contest-wait-menu");
                next(10);
                break;
            }
            if (healer)
            {
                const float dx = 10900.0f - pWorld->partyX();
                const float dy = 10160.0f - pWorld->partyY();
                const auto &movement = game.m_activeWorldManifest.partyMovement;
                const float eyeHeight = movement.declared ? movement.eyeHeight : 160.0f;
                game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                    std::atan2(224.0f - pWorld->partyFootZ() - eyeHeight, std::hypot(dx, dy)));
            }
            else game.m_outdoorGameView.setCameraAngles(3.1415926536f, 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(healer ? 70 : 2);
            break;
        }
        case 10:
            clickButton("RestButtonUntilDawn");
            require(runtime.restScreenState().mode == GameplayUiController::RestMode::Wait,
                "Normal wait-until-dawn button did not start waiting");
            next(11);
            break;
        case 11:
            require(frame < state.phaseFrame + 1800, "Normal dawn waiting did not finish");
            if (runtime.restScreenState().mode != GameplayUiController::RestMode::None) return;
            require(pParty->food() == state.foodBeforeWaiting
                && pWorld->gameMinutes() > state.minutesBeforeWaiting
                && isHouseOpenAtGameMinute(*runtime.houseTable()->get(
                    state.healFirst && !state.healingComplete ? 310 : 1163), pWorld->gameMinutes()),
                "Normal dawn waiting changed food or did not reach the original opening hours");
            if (runtime.restScreenState().active) clickButton("RestButtonExit");
            save("mm7-contest-after-dawn");
            captures.push_back("mm7-contest-after-dawn");
            next(1);
            break;
        case 20:
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, state.craftingMember);
            require(portrait.has_value(), "The original ingredient holder portrait is absent");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned click = 0; click < 2; ++click)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == state.craftingMember,
                "Normal portrait clicks did not open the selected member's original ingredients");
            next(21);
            break;
        }
        case 21:
        case 22:
        {
            const bool mix = state.phase == 22;
            const uint32_t itemId = mix ? 220 : state.craftingReagent;
            bool clicked = false;
            const auto *pGridLayout = runtime.findHudLayoutElement("CharacterInventoryGrid");
            require(pGridLayout != nullptr, "Crafting needs the original inventory grid");
            const auto grid = runtime.resolveHudLayoutElement("CharacterInventoryGrid", width, height,
                pGridLayout->width, pGridLayout->height);
            require(grid.has_value(), "Crafting inventory grid cannot be placed");
            for (const auto &image : runtime.renderedInspectableHudItems())
            {
                if (image.objectDescriptionId != itemId || image.sourceMemberIndex != state.craftingMember
                    || image.sourceType != GameplayUiController::ItemInspectSourceType::Inventory) continue;
                for (float y = std::max(grid->y + 1, image.y + 2); y < image.y + image.height && !clicked; y += 3)
                {
                    for (float x = std::max(grid->x + 1, image.x + 2); x < image.x + image.width; x += 3)
                    {
                        if (x >= grid->x + grid->width || y >= grid->y + grid->height) continue;
                        const auto *pItem = pParty->members()[state.craftingMember].inventoryItemAt(
                            uint8_t((x - grid->x) * Character::InventoryWidth / grid->width),
                            uint8_t((y - grid->y) * Character::InventoryHeight / grid->height));
                        if (!pItem || pItem->objectDescriptionId != itemId
                            || !runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                        GameplayInputFrame input;
                        input.screenWidth = width;
                        input.screenHeight = height;
                        input.pointerX = x;
                        input.pointerY = y;
                        if (mix) input.rightMouseButton = {true, true, false};
                        else input.leftMouseButton = {true, true, false};
                        session.updateGameplay(input, 0.0f);
                        if (mix) input.rightMouseButton = {false, false, true};
                        else input.leftMouseButton = {false, false, true};
                        session.updateGameplay(input, 0.0f);
                        clicked = true;
                        break;
                    }
                }
                if (clicked) break;
            }
            require(clicked, "Crafting input could not find the original visible inventory item");
            if (!mix)
            {
                require(runtime.heldInventoryItem().active
                    && runtime.heldInventoryItem().item.objectDescriptionId == state.craftingReagent,
                    "Normal inventory click did not pick up the original berry");
                next(22);
            }
            else
            {
                require(!runtime.heldInventoryItem().active
                    && pParty->inventoryItemCount(state.craftingReagent) == state.reagentsBeforeCrafting - 1
                    && pParty->inventoryItemCount(220) == state.bottlesBeforeCrafting - 1
                    && pParty->inventoryItemCount(222) == 1,
                    "Normal berry-on-bottle right click did not produce exactly one red potion");
                state.deliveries.insert(state.deliveries.begin(), 0);
                save("mm7-contest-potion-ready");
                captures.push_back("mm7-contest-potion-ready");
                next(23);
            }
            break;
        }
        case 23:
            if (runtime.characterScreen().open) close();
            else next(0);
            break;
        case 30:
        {
            const OutdoorWorldRuntime::MapActorState *pSeller = nullptr;
            for (size_t index = 0; index < pWorld->mapActorCount(); ++index)
            {
                const auto *pActor = pWorld->mapActorState(index);
                if (pActor && pActor->npcId == state.purchaseNpc) pSeller = pActor;
            }
            require(pSeller && pSeller->currentHp > 0, "The original quest-item seller is unavailable");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = pSeller->preciseX - x, dy = pSeller->preciseY - y;
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            if (std::hypot(dx, dy) > 190.0f)
            {
                pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.025f);
                const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
                state.walkedDistance += moved;
                state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
                require(state.stalledFrames < 60, "The normal approach to the original seller stopped");
                return;
            }
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(31);
            break;
        }
        case 31:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == state.purchaseNpc,
                "Normal interaction did not open the original seller");
            captures.push_back(state.purchaseItem == 1437 ? "mm7-contest-shell-seller"
                : "mm7-contest-instrument-seller");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, state.purchaseOffer);
            next(32);
            break;
        case 32:
            save(state.purchaseItem == 1437 ? "mm7-contest-before-shell-purchase"
                : "mm7-contest-before-instrument-purchase");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, state.purchaseAccept);
            next(33);
            break;
        case 33:
        {
            if (runtime.heldInventoryItem().active)
            {
                require(runtime.heldInventoryItem().item.objectDescriptionId == state.purchaseItem,
                    "The original seller granted a different item");
                const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
                require(portrait.has_value(), "The purchased item has no recipient portrait");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = portrait->x + portrait->width * 0.5f;
                input.pointerY = portrait->y + portrait->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            require(pParty->inventoryItemCount(state.purchaseItem) == 1 && !runtime.heldInventoryItem().active
                && pParty->gold() == state.expectedGold - state.purchaseCost,
                "Original quest purchase did not exchange its specified gold for exactly one item");
            state.expectedGold -= state.purchaseCost;
            state.deliveries.push_back(state.purchaseTask);
            std::sort(state.deliveries.begin(), state.deliveries.end());
            state.purchaseComplete = true;
            save(state.purchaseItem == 1437 ? "mm7-contest-shell-purchased"
                : "mm7-contest-instrument-purchased");
            captures.push_back(state.purchaseItem == 1437 ? "mm7-contest-shell-purchased"
                : "mm7-contest-instrument-purchased");
            next(34);
            break;
        }
        case 34:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                state.forwardRoute = std::move(state.returnRoute);
                state.waypoint = 0;
                next(0);
            }
            break;
        case 40:
        {
            require(state.purchaseItem == 0 && !state.craftRedPotion && state.deliveries.empty(),
                "Well visit must preserve the outstanding contest items");
            const auto *pEvents = pWorld->eventRuntimeState();
            require(pEvents != nullptr, "Original well charges are unavailable");
            state.wellInitialLuck = pParty->members()[0].luck;
            for (size_t index = 0; index < 3; ++index)
                state.wellInitialVars[index] = pEvents->mapVars[4 + index];
            require(state.wellEvent == 114
                ? state.wellInitialLuck == 11 && state.wellInitialVars[0] >= 2
                : state.wellInitialLuck >= 15 && pParty->gold() < 201
                    && state.wellInitialVars[1] == 0 && state.wellInitialVars[2] < 3,
                "The original well reward prerequisites are not met");
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "Cannot select the Knight for the original well");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton.held = true;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(pParty->activeMemberIndex() == 0 && !runtime.characterScreen().open,
                "Normal portrait click did not select the Knight");
            save("mm7-contest-before-well");
            next(41);
            break;
        }
        case 41:
        {
            const float dx = state.wellTarget[0] - pWorld->partyX();
            const float dy = state.wellTarget[1] - pWorld->partyY();
            const auto &movement = game.m_activeWorldManifest.partyMovement;
            // Match GameApplication's camera initialization when a world omits an eye-height override.
            const float eyeHeight = movement.declared ? movement.eyeHeight : 176.0f;
            const float dz = state.wellTarget[2] - pWorld->partyFootZ() - eyeHeight;
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), std::atan2(dz, std::hypot(dx, dy)));
            captures.push_back("mm7-contest-well-aimed-" + std::to_string(state.wellDrinks));
            next(43);
            break;
        }
        case 43:
            pressQuestGameplay(session, width, height, KeyboardAction::Use);
            next(42);
            break;
        case 42:
        {
            ++state.wellDrinks;
            const auto *pEvents = pWorld->eventRuntimeState();
            const unsigned granted = state.wellEvent == 114 ? std::min(state.wellDrinks, 2u) : 1;
            require(pEvents && pParty->members()[0].luck == state.wellInitialLuck
                    + (state.wellEvent == 114 ? int(granted * 2) : 0)
                && pParty->gold() == state.expectedGold + (state.wellEvent == 115 ? 1000 : 0),
                "Normal well interaction did not apply the original limited reward");
            for (size_t index = 0; index < 3; ++index)
            {
                const int delta = state.wellEvent == 114 ? (index == 0 ? -int(granted) : 0)
                    : (index > 0 ? 1 : 0);
                require(pEvents->mapVars[4 + index] == state.wellInitialVars[index] + delta,
                    "Original well charge or claim limits changed incorrectly");
            }
            const std::string token = "mm7-contest-well-" + std::to_string(state.wellEvent)
                + "-drink-" + std::to_string(state.wellDrinks);
            save(token);
            captures.push_back(token);
            std::cout << "MM7 original well: event=" << state.wellEvent << " drinks=" << state.wellDrinks
                << " luck=" << pParty->members()[0].luck << " gold=" << pParty->gold() << '\n';
            if (state.wellDrinks < (state.wellEvent == 114 ? 3u : 2u)) next(41);
            else
            {
                if (state.wellEvent == 115) state.expectedGold += 1000;
                next(6);
            }
            break;
        }
        case 2:
            require(runtime.activeEventDialog().isActive
                && (runtime.activeEventDialog().sourceId == 1163
                    || (pParty->hasQuestBit(519) && runtime.activeEventDialog().sourceId == 340)),
                "Normal Trigger did not open the original headquarters or its remaining resident");
            if (pParty->hasQuestBit(519))
            {
                require(state.claimAward || state.victoryBefore, "Victory requires the planned original award route");
                const auto &actions = runtime.activeEventDialog().actions;
                require(std::none_of(actions.begin(), actions.end(), [](const auto &action)
                    { return action.kind == EventDialogActionKind::HouseResident && action.id == 341; }),
                    "The completed contest judge remained available at headquarters");
                if (runtime.activeEventDialog().isHouseDialog)
                    selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 340);
                next(state.victoryBefore ? 54 : 51);
            }
            else
            {
                selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 341);
                next(3);
            }
            break;
        case 3:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 756);
            next(state.delivered < state.deliveries.size() ? 4 : 5);
            break;
        case 4:
        {
            const uint32_t item = items[state.deliveries[state.delivered++]];
            verify();
            const std::string token = "mm7-contest-delivered-" + std::to_string(item);
            save(token);
            captures.push_back(token);
            std::cout << "MM7 original contest delivery: item=" << item << " each_member_experience=500\n";
            next(3);
            break;
        }
        case 5:
            verify();
            save("mm7-contest-rechecked");
            captures.push_back("mm7-contest-rechecked");
            if (state.claimAward)
            {
                require(pParty->hasQuestBit(519), "All six original items did not finish the contest");
                next(50);
            }
            else next(6);
            break;
        case 50:
            if (runtime.activeEventDialog().isActive) close();
            else next(1);
            break;
        case 51:
            require(!pParty->hasQuestBit(529), "The castle award was already claimed");
            save("mm7-contest-before-award");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 753);
            state.awardApplied = true;
            next(52);
            break;
        case 52:
            verify();
            require(pParty->hasQuestBit(529) && !pParty->hasQuestBit(1683),
                "The original castle award did not update its quest state");
            for (size_t index = 0; index < pParty->members().size(); ++index)
                require(pParty->hasAward(index, 1), "The castle award is missing from an original party member");
            save("mm7-contest-castle-awarded");
            captures.push_back("mm7-contest-castle-awarded");
            next(53);
            break;
        case 53:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 754);
            next(54);
            break;
        case 54:
        {
            require(pParty->hasQuestBit(519) && pParty->hasQuestBit(529) && pParty->hasQuestBit(527),
                "Original contest, castle and departure state is incomplete");
            const auto &actions = runtime.activeEventDialog().actions;
            require(std::none_of(actions.begin(), actions.end(), [](const auto &action)
                { return action.kind == EventDialogActionKind::NpcTopic && (action.id == 753 || action.id == 754); }),
                "Original reward or ship topic is still offered after consumption");
            verify();
            save("mm7-contest-ship-arranged");
            captures.push_back("mm7-contest-ship-arranged");
            next(6);
            break;
        }
        case 60:
        {
            const float dx = 11008.0f - pWorld->partyX(), dy = 284.0f - pWorld->partyY();
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                std::atan2(320.0f - pWorld->partyFootZ() - 176.0f, std::hypot(dx, dy)));
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(61);
            break;
        }
        case 61:
            require(runtime.activeEventDialog().isActive
                && (runtime.activeEventDialog().sourceId == 1168 || runtime.activeEventDialog().sourceId == 350),
                "Normal ship-door interaction did not open the original captain");
            if (runtime.activeEventDialog().isHouseDialog)
                selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 350);
            save("mm7-departure-before-sailing");
            captures.push_back("mm7-departure-captain");
            next(62);
            break;
        case 62:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 783);
            next(63);
            break;
        case 70:
        {
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 310,
                "Normal interaction did not open the original healer tent");
            while (state.healingMember < pParty->members().size())
            {
                const Character &member = pParty->members()[state.healingMember];
                if (member.conditions.any() || member.health < Party::effectiveMaximumHealth(member)
                    || member.spellPoints < Party::effectiveMaximumSpellPoints(member)) break;
                ++state.healingMember;
            }
            if (state.healingMember == pParty->members().size())
            {
                state.healingComplete = true;
                save("mm7-healer-complete");
                captures.push_back("mm7-healer-complete");
                next(73);
                break;
            }
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, state.healingMember);
            require(portrait.has_value(), "The patient portrait is absent");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton.held = true;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(pParty->activeMemberIndex() == state.healingMember && !runtime.characterScreen().open,
                "Normal portrait input did not select the patient");
            next(71);
            break;
        }
        case 71:
        {
            const HouseEntry *pHouse = runtime.houseTable()->get(310);
            require(pHouse && pHouse->templeCanHeal, "The original tent cannot provide healing");
            state.healingPrice = PriceCalculator::templeHealPrice(pParty->activeMember(),
                *pHouse, pWorld->gameMinutes());
            require(state.healingPrice > 0 && pParty->gold() >= state.healingPrice,
                "Normal party cannot afford the original healer fee");
            save("mm7-healer-before-" + std::to_string(state.healingMember));
            captures.push_back("mm7-healer-before-" + std::to_string(state.healingMember));
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TempleHeal));
            next(72);
            break;
        }
        case 72:
        {
            const Character &member = pParty->members()[state.healingMember];
            require(member.conditions.none() && member.health == Party::effectiveMaximumHealth(member)
                && member.spellPoints == Party::effectiveMaximumSpellPoints(member)
                && pParty->gold() == state.expectedGold - state.healingPrice,
                "Original healer did not cure the selected patient for exactly the quoted fee");
            state.expectedGold -= state.healingPrice;
            std::cout << "MM7 original healer: member=" << state.healingMember
                << " price=" << state.healingPrice << " gold=" << pParty->gold() << '\n';
            save("mm7-healer-after-" + std::to_string(state.healingMember));
            captures.push_back("mm7-healer-after-" + std::to_string(state.healingMember));
            ++state.healingMember;
            next(70);
            break;
        }
        case 73:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                state.forwardRoute = std::move(state.returnRoute);
                state.waypoint = 0;
                next(0);
            }
            break;
        case 63:
            if (game.m_screenManager.activeScreen() != nullptr)
            {
                require(dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr,
                    "Original Harmondale arrival opened an unexpected screen");
                ++state.departureVideoFrames;
                if (state.departureVideoFrames == 1 || state.departureVideoFrames == 120
                    || state.departureVideoFrames == 240)
                    captures.push_back("mm7-departure-movie-" + std::to_string(state.departureVideoFrames));
                return;
            }
            require(state.departureVideoFrames > 1 && pParty->hasQuestBit(648)
                && !game.m_gameAudioSystem.isBackgroundMusicPaused(),
                "Original arrival movie did not finish naturally and restore background music");
            if (state.departureVideoEndFrame == 0)
            {
                state.departureVideoEndFrame = frame;
                std::cout << "MM7 arrival movie finished naturally: frames=" << state.departureVideoFrames << '\n';
                return;
            }
            if (frame < state.departureVideoEndFrame + 12) return;
            require(session.currentMapFileName() == "7out02.odm"
                && std::hypot(pWorld->partyX() + 17331.0f, pWorld->partyY() - 12547.0f) < 30.0f,
                "Original captain did not take the party to the Harmondale arrival point");
            verify();
            state.departureComplete = true;
            save("mm7-departure-arrived");
            captures.push_back("mm7-departure-arrived");
            next(6);
            break;
        case 6:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                verify();
                save("mm7-contest-complete");
                captures.push_back("mm7-contest-complete");
                state.complete = true;
                std::cout << "MM7 contest complete: deliveries=" << state.delivered << " reload=" << reload
                    << " walked=" << state.walkedDistance << " gold=" << pParty->gold() << '\n';
            }
            break;
        default: throw std::runtime_error("Unknown contest delivery phase");
        }
    }

    static void checkMm7Shell(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm7RouteCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        auto *pWorld = dynamic_cast<OutdoorWorldRuntime *>(session.activeWorldRuntime());
        require(pParty && pWorld && session.currentMapFileName() == "7out01.odm" && pParty->hasActableMember(),
            "MM7 shell route requires the surviving normal Emerald Island party");
        const std::string subject = state.potion ? "potion" : "shell";
        require(!state.potion || runtime.mergedReagentSettingTable() != nullptr,
            "Original potion reagent recipes are missing");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "翡翠岛贝壳任务", {}), "Cannot preserve normal MM7 shell route state");
        };
        const auto select = [&](EventDialogActionKind kind, uint32_t id, unsigned phase)
        {
            selectQuestDialog(runtime, kind, id);
            next(phase);
        };
        const auto clickInventoryItem = [&](uint32_t itemId, bool useHeldItem = false)
        {
            for (const auto &image : runtime.renderedInspectableHudItems())
            {
                if (image.objectDescriptionId != itemId
                    || image.sourceType != GameplayUiController::ItemInspectSourceType::Inventory) continue;
                for (float y = image.y + 2; y < image.y + image.height; y += 3)
                    for (float x = image.x + 2; x < image.x + image.width; x += 3)
                    {
                        if (!runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                        GameplayInputFrame input;
                        input.screenWidth = width;
                        input.screenHeight = height;
                        input.pointerX = x;
                        input.pointerY = y;
                        if (useHeldItem) input.rightMouseButton = {true, true, false};
                        else input.leftMouseButton = {true, true, false};
                        session.updateGameplay(input, 0.0f);
                        if (useHeldItem) input.rightMouseButton = {false, false, true};
                        else input.leftMouseButton = {false, false, true};
                        session.updateGameplay(input, 0.0f);
                        return;
                    }
            }
            throw std::runtime_error("Required crafting item is not visibly rendered: " + std::to_string(itemId));
        };
        const auto walkTo = [&](float targetX, float targetY, float distance = 5.0f)
        {
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            const float dx = targetX - x;
            const float dy = targetY - y;
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            const float remaining = std::hypot(dx, dy);
            if (remaining < distance) return true;
            // Keep at least one game tick; shorter pulses can round to zero movement.
            press(KeyboardAction::Forward, std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 30)
                std::cout << "MM7 walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " target=" << targetX << ',' << targetY << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal MM7 shell route stopped advancing");
            return false;
        };
        const auto walkRoute = [&](const auto &route)
        {
            if (!walkTo(route[state.waypoint][0], route[state.waypoint][1])) return false;
            std::cout << "MM7 waypoint: phase=" << state.phase << " index=" << state.waypoint
                << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << '\n';
            state.stalledFrames = 0;
            return ++state.waypoint == route.size();
        };
        const auto requireReward = [&]()
        {
            require(!pParty->hasQuestBit(514) && pParty->hasQuestBit(521) && !pParty->hasQuestBit(519)
                && pParty->inventoryItemCount(state.potion ? 222 : 1437) == 0 && !runtime.heldInventoryItem().active
                && pParty->gold() == state.expectedGold
                && pParty->members().front().experience == (state.potion ? 1000 : 500),
                "MM7 shell reward, consumed item or gold differs from the original event");
            require(state.potion ? (!pParty->hasQuestBit(513) && pParty->hasQuestBit(520)) : pParty->hasQuestBit(513),
                "Red potion quest state differs from the original event");
            for (uint32_t bit : {515u, 516u, 517u, 518u})
                require(pParty->hasQuestBit(bit), "An unrelated scavenger task was lost");
        };
        if (frame == 1)
        {
            require(pParty->members().size() == 1 && pParty->members().front().level == 1,
                "MM7 shell route needs the normal single level-one character");
            if (reload)
            {
                if (state.potion) state.expectedGold = pParty->gold();
                requireReward();
                next(8);
            }
            else
            {
                require(pParty->gold() == (state.potion ? 100 : 200)
                    && pParty->members().front().experience == (state.potion ? 500 : 0)
                    && pParty->inventoryItemCount(state.potion ? 222 : 1437) == 0
                    && (state.potion ? pParty->hasQuestBit(521) : !pParty->hasQuestBit(521)),
                    "MM7 shell route must begin before the purchase and delivery");
                for (uint32_t bit = 513; bit <= 518; ++bit)
                    require(pParty->hasQuestBit(bit) == !(state.potion && bit == 514),
                        "MM7 input omitted an outstanding scavenger task");
                const auto readRoute = [&](const char *pName)
                {
                    const auto path = std::filesystem::current_path().parent_path() / pName;
                    std::ifstream stream(path);
                    require(stream.good(), "MM7 shell mode requires route file: " + path.string());
                    std::vector<std::array<float, 2>> route;
                    std::string line;
                    while (std::getline(stream, line))
                    {
                        if (line.empty() || line.front() == '#') continue;
                        std::istringstream values(line);
                        float x = 0, y = 0, z = 0;
                        require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                            && std::isfinite(z), "Invalid MM7 route waypoint");
                        route.push_back({x, y});
                        require(route.size() <= 2000, "MM7 route exceeds the waypoint limit");
                    }
                    require(route.size() >= 2, "MM7 route requires at least two waypoints");
                    return route;
                };
                state.forwardRoute = readRoute(("mm7-" + subject + "-forward.tsv").c_str());
                state.returnRoute = readRoute(("mm7-" + subject + "-return.tsv").c_str());
            }
        }
        if (state.phase == 0 || state.phase == 1 || state.phase == 7)
        {
            const EventDialogContent &dialog = runtime.activeEventDialog();
            if (dialog.isActive)
            {
                require(dialog.sourceId >= 358 && dialog.sourceId <= 376 && !dialog.isHouseDialog,
                    "An unexpected dialogue interrupted the original MM7 route");
                if (state.guideOpenedFrame == 0)
                {
                    state.guideOpenedFrame = frame;
                    std::cout << "MM7 walking dialogue: npc=" << dialog.sourceId
                        << " title=" << dialog.title << '\n';
                    require(++state.guideCount <= 30, "MM7 guide repeats without allowing normal movement");
                    for (const std::string &line : dialog.lines) std::cout << "MM7 guide: " << line << '\n';
                    captures.push_back("mm7-guide-" + std::to_string(state.guideCount));
                }
                if (frame >= state.guideOpenedFrame + 12) close();
                state.stalledFrames = 0;
                return;
            }
            state.guideOpenedFrame = 0;
        }
        if (state.phase == 0)
        {
            // Waypoints are planned from original map geometry. Movement, collision and NPC
            // simulation all remain normal gameplay; no position or quest state is assigned.
            if (walkRoute(state.forwardRoute)) next(state.potion ? 20 : 1);
            return;
        }
        if (state.phase == 1)
        {
            const OutdoorWorldRuntime::MapActorState *pSeller = nullptr;
            for (size_t index = 0; index < pWorld->mapActorCount(); ++index)
            {
                const auto *pActor = pWorld->mapActorState(index);
                if (pActor != nullptr && pActor->npcId == 344) pSeller = pActor;
            }
            require(pSeller != nullptr && pSeller->currentHp > 0, "Original shell seller is unavailable");
            if (walkTo(pSeller->preciseX, pSeller->preciseY, 190.0f))
            {
                press(KeyboardAction::Trigger);
                next(2);
            }
            return;
        }
        if (state.phase == 7)
        {
            if (walkRoute(state.returnRoute)) next(8);
            return;
        }
        if (frame == state.phaseFrame + 4)
        {
            if (state.phase == 2) captures.push_back("mm7-shell-seller");
            if (state.phase == 4) captures.push_back("mm7-shell-purchased");
            if (state.phase == 11) captures.push_back("mm7-" + subject + (reload ? "-reopened" : "-delivered"));
            if (state.phase == 22 || state.phase == 25)
                captures.push_back(state.specialStock ? "mm7-potion-special-stock" : "mm7-potion-standard-stock");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 20:
            game.m_outdoorGameView.setCameraAngles(-1.5707963268f, 0.0f);
            press(KeyboardAction::Trigger);
            next(21);
            break;
        case 21:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 116,
                "Normal Trigger did not open the original Emerald Island alchemist");
            select(EventDialogActionKind::HouseService, uint32_t(HouseActionId::ShopBuyStandard), 22);
            break;
        case 24:
            select(EventDialogActionKind::HouseService, uint32_t(HouseActionId::ShopBuySpecial), 25);
            break;
        case 22:
        case 25:
        {
            require(runtime.houseShopOverlay().active && runtime.houseShopOverlay().houseId == 116,
                "Alchemist stock action did not open its shelf");
            const HouseEntry *pHouse = runtime.houseTable()->get(116);
            const Party::HouseStockState *pStock = pParty->houseStockState(116);
            require(pHouse && pStock, "Original alchemist stock is absent");
            const auto before = state.specialStock ? pStock->specialStock : pStock->standardStock;
            const int reputation = effectivePartyReputation(
                pWorld->currentLocationReputation(), pWorld->eventRuntimeState());
            for (size_t i = 0; i < before.size(); ++i)
            {
                const auto &item = before[i];
                if (!item.objectDescriptionId) continue;
                const int price = HouseServiceRuntime::buyPrice(*pParty, *runtime.itemTable(),
                    *runtime.standardItemEnchantTable(), *runtime.specialItemEnchantTable(), *pHouse, item, reputation);
                std::cout << "MM7 alchemist offer: special=" << state.specialStock << " slot=" << i
                    << " item=" << item.objectDescriptionId << " price=" << price << '\n';
            }
            save(state.specialStock ? "potion-special-before-buy" : "potion-standard-before-buy");
            bool bought = false;
            for (const auto &image : runtime.renderedInspectableHudItems())
            {
                if (!image.hasItemState) continue;
                const auto result =
                    runtime.mergedReagentSettingTable()->resultItemIdForReagent(image.objectDescriptionId);
                if (image.objectDescriptionId != 222 && result != std::optional<uint32_t>(222)) continue;
                const int price = HouseServiceRuntime::buyPrice(*pParty, *runtime.itemTable(),
                    *runtime.standardItemEnchantTable(), *runtime.specialItemEnchantTable(),
                    *pHouse, image.itemState, reputation);
                if (price <= 0 || price > pParty->gold()) continue;
                state.boughtItemId = image.objectDescriptionId;
                state.needsMix = state.boughtItemId != 222;
                require(!state.needsMix || pParty->inventoryItemCount(220) == 1,
                    "Normal reagent crafting requires the original empty bottle");
                bool visible = false;
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                for (float y = image.y + 2; y < image.y + image.height && !visible; y += 3)
                {
                    for (float x = image.x + 2; x < image.x + image.width; x += 3)
                    {
                        if (!runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                        input.pointerX = x;
                        input.pointerY = y;
                        visible = true;
                        break;
                    }
                }
                require(visible, "The offered red potion has no visible opaque pixels");
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
                state.buyPrice = price;
                state.expectedGold = 100 - price;
                require(pParty->gold() == state.expectedGold && pParty->inventoryItemCount(state.boughtItemId) == 1,
                    "Normal alchemist click did not purchase exactly one selected item at its offered price");
                const auto &after = state.specialStock ? pStock->specialStock : pStock->standardStock;
                size_t removed = 0;
                for (size_t i = 0; i < before.size(); ++i)
                    if (before[i].objectDescriptionId && !after[i].objectDescriptionId)
                    {
                        require(before[i].objectDescriptionId == state.boughtItemId,
                            "The purchase removed a different stock item");
                        ++removed;
                    }
                require(removed == 1, "The purchase did not remove exactly one stock slot");
                std::cout << "MM7 potion supply bought: item=" << state.boughtItemId
                    << " price=" << price << " gold=" << pParty->gold() << '\n';
                save(state.needsMix ? "potion-reagent-purchased" : "potion-ready");
                captures.push_back(state.needsMix ? "mm7-potion-reagent-purchased" : "mm7-potion-ready");
                bought = true;
                next(5);
                break;
            }
            if (!bought)
            {
                require(!state.specialStock, "Neither alchemist shelf offers an affordable red potion or its reagent");
                close();
                state.specialStock = true;
                next(24);
            }
            break;
        }
        case 26:
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "The normal Knight portrait is absent");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned count = 0; count < 2; ++count)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.leftMouseButton = {};
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open
                && runtime.characterScreen().page == GameplayUiController::CharacterPage::Inventory,
                "Normal portrait double click did not open the crafting backpack");
            next(27);
            break;
        }
        case 27:
            clickInventoryItem(state.boughtItemId);
            require(runtime.heldInventoryItem().active
                && runtime.heldInventoryItem().item.objectDescriptionId == state.boughtItemId,
                "Normal inventory click did not pick up the purchased reagent");
            save("potion-reagent-held");
            next(28);
            break;
        case 28:
            clickInventoryItem(220, true);
            require(!runtime.heldInventoryItem().active && pParty->inventoryItemCount(220) == 0
                && pParty->inventoryItemCount(state.boughtItemId) == 0 && pParty->inventoryItemCount(222) == 1
                && pParty->gold() == state.expectedGold && pParty->members().front().experience == 500,
                "Normal reagent-on-bottle right click did not create exactly one red potion");
            save("potion-ready");
            captures.push_back("mm7-potion-ready");
            next(29);
            break;
        case 29:
            if (runtime.characterScreen().open) close();
            else next(7);
            break;
        case 2:
            select(EventDialogActionKind::NpcTopic, 765, 3);
            break;
        case 3:
            select(EventDialogActionKind::NpcTopic, 766, 4);
            break;
        case 4:
            if (runtime.heldInventoryItem().active)
            {
                require(runtime.heldInventoryItem().item.objectDescriptionId == 1437,
                    "Shell purchase placed an unexpected item on the pointer");
                const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
                require(portrait.has_value(), "No visible portrait can receive the purchased shell");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = portrait->x + portrait->width * 0.5f;
                input.pointerY = portrait->y + portrait->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            require(pParty->gold() == 100 && pParty->inventoryItemCount(1437) == 1
                && !runtime.heldInventoryItem().active && pParty->members().front().experience == 0,
                "Shell purchase did not exchange exactly 100 gold for one original item");
            save("shell-purchased");
            next(5);
            break;
        case 5:
            if (runtime.activeEventDialog().isActive || runtime.houseShopOverlay().active) close();
            else next(state.needsMix ? 26 : 7);
            break;
        case 8:
            game.m_outdoorGameView.setCameraAngles(3.1415926536f, 0.0f);
            press(KeyboardAction::Trigger);
            next(9);
            break;
        case 9:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 1163,
                "Normal Trigger did not open Markham's Headquarters");
            select(EventDialogActionKind::HouseResident, 341, 10);
            break;
        case 10:
            select(EventDialogActionKind::NpcTopic, 756, 11);
            break;
        case 11:
            requireReward();
            save((subject + (reload ? "-reopened" : "-delivered")).c_str());
            if (reload) next(13);
            else select(EventDialogActionKind::NpcTopic, 756, 12);
            break;
        case 12:
            requireReward();
            next(13);
            break;
        case 13:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                requireReward();
                save(("mm7-" + subject + "-complete").c_str());
                captures.push_back("mm7-" + subject + "-complete");
                state.complete = true;
                std::cout << "MM7 " << subject << " route complete: reload=" << reload
                    << " walked=" << state.walkedDistance
                    << " gold=" << pParty->gold() << " experience=" << pParty->members().front().experience << '\n';
            }
            break;
        default:
            throw std::runtime_error("Unknown MM7 shell route phase");
        }
    }

    static void checkMm8Start(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm8StartCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && session.currentMapFileName() == "out01.odm" && pParty->hasActableMember(),
            "MM8 route requires the surviving normal Dagger Wound Island party");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto select = [&](EventDialogActionKind kind, uint32_t id, unsigned phase)
        {
            selectQuestDialog(runtime, kind, id);
            next(phase);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "匕伤岛任务验证", {}), "Cannot preserve normal MM8 route state");
        };
        const auto walk = [&](const auto &route)
        {
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x;
            const float dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 25.0f)
            {
                std::cout << "MM8 waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            // Original door geometry supplies direction candidates only. All movement uses
            // normal Forward input, collisions, terrain height and ongoing actor simulation.
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 30)
                std::cout << "MM8 walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " remaining=" << std::hypot(dx, dy) << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal MM8 route is blocked by scene geometry or an actor");
            return false;
        };
        const auto requireReward = [&]()
        {
            require(!pParty->hasQuestBit(7) && pParty->hasQuestBit(8)
                && pParty->inventoryItemCount(617) == 1 && pParty->gold() == 200
                && pParty->members().front().experience == 1500 && runtime.isAutonoteUnlocked(492),
                "Crystal quest reward, retained item or original autonote differs from the source");
        };
        if (frame == 1)
        {
            require(pParty->members().size() == (reload ? 2 : 1) && pParty->gold() == 200,
                "MM8 route input is not the normal party checkpoint");
            if (reload)
            {
                requireReward();
                require(pParty->hasRosterMember(2), "Reload lost Talimere's original roster identity");
                next(20);
            }
            else
            {
                require(!pParty->hasQuestBit(7) && !pParty->hasQuestBit(8)
                    && pParty->inventoryItemCount(617) == 0 && pParty->members().front().experience == 0,
                    "MM8 route must start before accepting the crystal quest");
            }
        }
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 5> route = {{{5600, 6400}, {7500, 5800},
                {9400, 5800}, {9400, 6150}, {9400, 6320}}};
            if (walk(route)) next(1);
            return;
        }
        if (state.phase == 7)
        {
            constexpr std::array<std::array<float, 2>, 6> route = {{{9400, 5900}, {10400, 5700},
                {12500, 4500}, {13800, 3600}, {15000, 3600}, {15260, 2872}}};
            if (walk(route)) next(8);
            return;
        }
        if (!reload && frame == state.phaseFrame + 4)
        {
            if (state.phase == 2) captures.push_back("mm8-chief");
            if (state.phase == 5) captures.push_back("mm8-crystal");
            if (state.phase == 11) captures.push_back("mm8-reward");
            if (state.phase == 12) captures.push_back("mm8-offer");
            if (state.phase == 13) captures.push_back("mm8-joined");
        }
        if (reload && state.phase == 21 && frame == state.phaseFrame + 4)
            captures.push_back("mm8-reopened");
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            game.m_outdoorGameView.setCameraAngles(1.5707963268f, -0.1f);
            press(KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 212,
                "Normal Trigger did not open the Lizardman Throne Room");
            select(EventDialogActionKind::HouseResident, 1, 3);
            break;
        case 3:
            select(EventDialogActionKind::NpcTopic, 8, 4);
            break;
        case 4:
            select(EventDialogActionKind::NpcTopic, 21, 5);
            break;
        case 5:
            if (runtime.heldInventoryItem().active)
            {
                require(runtime.heldInventoryItem().item.objectDescriptionId == 617,
                    "Chief dialogue placed an unexpected item on the pointer");
                const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
                require(portrait.has_value(), "Dialogue has no visible recipient portrait");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = portrait->x + portrait->width * 0.5f;
                input.pointerY = portrait->y + portrait->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
                require(!runtime.heldInventoryItem().active, "Portrait click did not store the original crystal");
            }
            require(pParty->hasQuestBit(7) && pParty->hasQuestBit(212)
                && pParty->inventoryItemCount(617) == 1 && pParty->members().front().experience == 0,
                "The original chief quest did not grant exactly one crystal");
            save("crystal-accepted");
            select(EventDialogActionKind::NpcTopic, 21, 6);
            break;
        case 6:
            require(pParty->inventoryItemCount(617) == 1, "Repeated chief dialogue duplicated the crystal");
            if (runtime.activeEventDialog().isActive)
            {
                close();
                state.phaseFrame = frame;
            }
            else
            {
                state.waypoint = 0;
                next(7);
            }
            break;
        case 8:
        case 20:
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.1f);
            press(KeyboardAction::Trigger);
            next(reload ? 21 : 9);
            break;
        case 9:
            // The shared house builder opens a lone resident directly as NpcTalk.
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 28,
                "Normal Trigger did not open Talimere's single-resident dialogue");
            next(10);
            break;
        case 10:
            select(EventDialogActionKind::NpcTopic, 12, 11);
            break;
        case 11:
            requireReward();
            require(std::none_of(runtime.activeEventDialog().actions.begin(), runtime.activeEventDialog().actions.end(),
                [](const auto &action) { return action.kind == EventDialogActionKind::NpcTopic && action.id == 12; }),
                "Completed crystal delivery is still offered as a reward topic");
            save("crystal-delivered");
            for (const std::string &line : runtime.activeEventDialog().lines)
                std::cout << "MM8 crystal response: " << line << '\n';
            select(EventDialogActionKind::RosterJoinOffer, 602, 12);
            break;
        case 12:
        {
            const auto &actions = runtime.activeEventDialog().actions;
            const auto accept = std::find_if(actions.begin(), actions.end(), [](const auto &action)
                { return action.kind == EventDialogActionKind::RosterJoinAccept; });
            require(accept != actions.end(), "Original roster offer has no accept action");
            select(EventDialogActionKind::RosterJoinAccept, accept->id, 13);
            break;
        }
        case 13:
            requireReward();
            require(pParty->members().size() == 2 && pParty->hasRosterMember(2)
                && pParty->members().back().level == 5 && pParty->members().back().experience == 10000,
                "Accepting Talimere did not recruit the original level-five roster member");
            save("talimere-recruited");
            next(14);
            break;
        case 21:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 866,
                "Reload did not reopen the original now-unoccupied hut");
            require(std::none_of(runtime.activeEventDialog().actions.begin(), runtime.activeEventDialog().actions.end(),
                [](const auto &action)
                { return action.kind == EventDialogActionKind::HouseResident && action.id == 28; }),
                "Recruited Talimere is still offered as a house resident");
            next(22);
            break;
        case 14:
        case 22:
            requireReward();
            require(pParty->members().size() == 2 && pParty->hasRosterMember(2),
                "Reopening the hut duplicated or lost the recruited companion");
            if (runtime.activeEventDialog().isActive)
            {
                close();
                state.phaseFrame = frame;
            }
            else
            {
                captures.push_back("mm8-complete");
                save("mm8-route-complete");
                state.complete = true;
                std::cout << "MM8 crystal and recruitment complete: reload=" << reload
                    << " walked=" << state.walkedDistance << " party=" << pParty->members().size()
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " gold=" << pParty->gold() << '\n';
            }
            break;
        default:
            throw std::runtime_error("Unknown MM8 route phase");
        }
    }

    static void checkMm8Inn(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm8StartCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && session.currentMapFileName() == "out01.odm" && pParty->hasActableMember(),
            "MM8 inn route requires the surviving normal island party");
        const auto next = [&](unsigned phase) { state.phase = phase; state.phaseFrame = frame; };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto hire = [&](uint32_t rosterId)
        {
            require(runtime.characterScreen().adventurersInnRosterOverlayOpen,
                "Inn roster overlay closed before normal recruitment");
            const auto &members = pParty->adventurersInnMembers();
            const auto found = std::find_if(members.begin(), members.end(), [&](const auto &member)
                { return member.character.rosterId == rosterId; });
            require(found != members.end(), "Original unlocked roster member is absent from the inn");
            const size_t index = size_t(found - members.begin());
            const size_t offset = runtime.characterScreen().adventurersInnScrollOffset;
            require(index >= offset && index < offset + 8, "Required original inn portrait is outside visible rows");
            const size_t visible = index - offset;
            const auto viewport = GameplayHudCommon::computeUiViewportRect(width, height);
            const float scale = std::min(viewport.width / 640.0f, viewport.height / 480.0f);
            click(viewport.x + (65.0f + float(visible % 2) * 65.0f) * scale,
                viewport.y + (83.0f + float(visible / 2) * 75.0f) * scale);
            require(runtime.characterScreen().sourceIndex == index, "Inn portrait click selected another companion");
            require(runtime.characterScreen().adventurersInnRosterOverlayOpen,
                "Clicking the next companion after hiring was mistaken for a portrait double-click");
            const auto button = runtime.resolveHudLayoutElement("AdventurersInnHireButton", width, height, 75, 26);
            require(button.has_value(), "Original inn hire button layout is missing");
            const size_t partySize = pParty->members().size();
            click(button->x + button->width * 0.5f, button->y + button->height * 0.5f);
            require(pParty->members().size() == partySize + 1
                && std::any_of(pParty->members().begin(), pParty->members().end(), [&](const Character &member)
                    { return member.rosterId == rosterId; }),
                "Normal inn hire button did not recruit the selected member into the active party");
            std::cout << "MM8 inn hired: roster=" << rosterId << " party=" << pParty->members().size() << '\n';
        };
        if (frame == 1)
        {
            require((reload ? (pParty->members().size() == 3 || pParty->members().size() == 4)
                    : pParty->members().size() == 2) && pParty->hasRosterMember(2)
                && pParty->hasQuestBit(8) && pParty->inventoryItemCount(617) == 1
                && pParty->inventoryItemCount(618) == 1 && pParty->gold() == 200,
                "Inn route must continue the normal completed crystal quest");
            if (reload) next(1);
        }
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 8> route = {{{15000, 3600}, {13800, 3600},
                {12490, 4500}, {12000, 4500}, {12000, 6600}, {13000, 7500}, {13380, 8500}, {13530, 9216}}};
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 25.0f)
            {
                std::cout << "Inn waypoint: " << state.waypoint << " position=" << x << ',' << y
                    << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                if (++state.waypoint == route.size()) next(1);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0)
                std::cout << "Inn walk: waypoint=" << state.waypoint << " position=" << pWorld->partyX()
                    << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal route to the inn is blocked");
            return;
        }
        if (frame == state.phaseFrame + 4)
        {
            if (state.phase == 2) captures.push_back("inn-arrived");
            if (!reload && state.phase == 4) captures.push_back("inn-first-hired");
            if (!reload && state.phase == 5) captures.push_back("inn-four-party");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.1f);
            press(KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            if (reload && pParty->members().size() == 4)
            {
                require(pParty->hasRosterMember(1) && pParty->hasRosterMember(7)
                    && pParty->adventurersInnMembers().empty()
                    && runtime.statusBarEventText() == "The Adventurer's Inn is empty.",
                    "Reopened inn regenerated already recruited companions or failed to open");
                next(6);
            }
            else
            {
                require(runtime.characterScreen().open && runtime.characterScreen().adventurersInnRosterOverlayOpen,
                    "Original Chinese inn entry did not open its roster overlay");
                next(reload ? 4 : 3);
            }
            break;
        case 3:
            require(game.quickSaveToPath(std::filesystem::current_path() / "inn-roster-available.oysav",
                "旅店待招募名单", {}), "Cannot preserve the original waiting roster");
            hire(1);
            next(4);
            break;
        case 4:
            require(pParty->members().size() == 3, "First inn recruitment changed the wrong number of members");
            require(game.quickSaveToPath(std::filesystem::current_path() / "inn-first-hired.oysav",
                "旅店首名队员加入", {}), "Cannot preserve the partially recruited roster");
            hire(7);
            next(5);
            break;
        case 5:
            require(pParty->members().size() == 4 && pParty->adventurersInnMembers().empty(),
                "Original two-member inn pool was not exhausted by two hires");
            next(6);
            break;
        case 6:
            require(pParty->hasRosterMember(1) && pParty->hasRosterMember(2) && pParty->hasRosterMember(7)
                && pParty->gold() == 200 && pParty->hasQuestBit(8), "Inn recruitment changed the completed quest");
            require(game.quickSaveToPath(std::filesystem::current_path() / "inn-route-complete.oysav",
                "匕伤岛旅店验证", {}), "Cannot save the normally recruited inn party");
            state.complete = true;
            captures.push_back("inn-complete");
            std::cout << "MM8 inn complete: reload=" << reload << " walked=" << state.walkedDistance
                << " party=" << pParty->members().size() << '\n';
            break;
        default:
            throw std::runtime_error("Unknown MM8 inn route phase");
        }
    }

    struct Mm8ShopCheckState : Mm8StartCheckState
    {
        size_t boughtSlot = 0;
        InventoryItem boughtItem;
        int buyPrice = 0;
        int sellPrice = 0;
        int startingGold = 0;
        size_t startingItemCount = 0;
        float pointerX = 0.0f;
        float pointerY = 0.0f;
    };

    static void checkMm8Shop(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm8ShopCheckState &state, std::vector<std::string> &captures, bool requireHammerFixture = true)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && session.currentMapFileName() == "out01.odm"
            && pParty->members().size() == 4 && pParty->hasActableMember(),
            "Shop route requires the original surviving four-member MM8 party");
        const HouseEntry *pHouse = runtime.houseTable()->get(1);
        require(pHouse && runtime.itemTable(), "Original MM8 weapon shop tables are absent");
        const int reputation = effectivePartyReputation(
            pWorld->currentLocationReputation(), pWorld->eventRuntimeState());
        const auto next = [&](unsigned phase) { state.phase = phase; state.phaseFrame = frame; };
        const auto close = [&]()
        {
            std::cout << "Shop close before: dialogue=" << runtime.activeEventDialog().isActive
                << " shelf=" << runtime.houseShopOverlay().active << '\n';
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            std::cout << "Shop close after press: dialogue=" << runtime.activeEventDialog().isActive
                << " shelf=" << runtime.houseShopOverlay().active
                << " latch=" << runtime.interactionState().closeOverlayLatch << '\n';
            const bool dialogueAfterPress = runtime.activeEventDialog().isActive;
            const bool shelfAfterPress = runtime.houseShopOverlay().active;
            const auto inventoryModeAfterPress = runtime.inventoryNestedOverlay().mode;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 0;
            session.updateGameplay(input, 0.0f);
            require(runtime.activeEventDialog().isActive == dialogueAfterPress
                && runtime.houseShopOverlay().active == shelfAfterPress
                && runtime.inventoryNestedOverlay().mode == inventoryModeAfterPress,
                "Holding Escape dismissed another shop layer");
            session.updateGameplay({}, 0.0f);
            require(!runtime.interactionState().closeOverlayLatch, "Escape release did not reset the shared latch");
            std::cout << "Shop close after release: dialogue=" << runtime.activeEventDialog().isActive
                << " shelf=" << runtime.houseShopOverlay().active << '\n';
        };
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto service = [&](HouseActionId action, unsigned phase)
        {
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(action));
            next(phase);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "匕伤岛商店验证", {}), "Cannot save normal shop transaction");
        };
        if (frame == 1)
        {
            require(pParty->hasQuestBit(8) && pParty->inventoryItemCount(617) == 1
                && pParty->inventoryItemCount(618) == 1 && pParty->activeMemberIndex() == 0,
                "Shop route must preserve the completed crystal quest and original selected member");
            state.startingGold = pParty->gold();
            state.startingItemCount = pParty->inventoryItemCount();
            require(reload || pParty->gold() == 200, "Normal shop route starts with the original 200 gold");
            if (reload || (pParty->houseStockState(1) != nullptr
                && std::hypot(pWorld->partyX() - 6630.8f, pWorld->partyY() - 7798.2f) < 25.0f))
            {
                require(pParty->houseStockState(1) != nullptr, "Reload lost visited shop stock");
                next(1);
            }
        }
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 11> route = {{{13380, 8500}, {13000, 7500},
                {12000, 6600}, {12000, 4500}, {12500, 4500}, {10400, 5700}, {9400, 5800},
                {7500, 5800}, {5800, 6400}, {5800, 7660}, {6640, 7800}}};
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 25.0f)
            {
                std::cout << "Shop waypoint: " << state.waypoint << " position=" << x << ',' << y
                    << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                if (++state.waypoint == route.size()) next(1);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0)
                std::cout << "Shop walk: waypoint=" << state.waypoint << " position=" << pWorld->partyX()
                    << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal walk to the weapon shop is blocked");
            return;
        }
        if (frame == state.phaseFrame + 4)
        {
            if (state.phase == 2) captures.push_back("shop-arrived");
            if (state.phase == 3) captures.push_back("shop-stock");
            if (state.phase == 4) captures.push_back("shop-bought");
            if (state.phase == 7) captures.push_back("shop-sell");
            if (state.phase == 8) captures.push_back("shop-sold");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.1f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 1,
                "Normal Trigger did not enter original MM8 weapon shop 1");
            service(HouseActionId::ShopBuyStandard, 3);
            break;
        case 3:
        {
            require(runtime.houseShopOverlay().active && runtime.houseShopOverlay().houseId == 1,
                "Original standard stock action did not open the shop shelf");
            const Party::HouseStockState *pStock = pParty->houseStockState(1);
            require(pStock && pStock->standardStock.size() == 6, "Original weapon shop must expose six stock slots");
            bool checkedHammer = false;
            for (const GameplayRenderedInspectableHudItem &image : runtime.renderedInspectableHudItems())
            {
                if (image.textureName != "hammer1") continue;
                int pixelWidth = 0, pixelHeight = 0;
                const std::vector<uint8_t> *pPixels = runtime.gameplayUiRuntime().hudTexturePixels(
                    image.textureName, pixelWidth, pixelHeight);
                require(pPixels && pixelWidth == 51 && pixelHeight == 151
                    && image.textureUsesItemIconTransparency && !image.rotatedCounterClockwise,
                    "Original hammer shelf texture dimensions or transparency mode changed");
                size_t transparentSourcePixels = 0, opaquePixels = 0;
                for (int y = 0; y < pixelHeight; ++y)
                {
                    for (int x = 0; x < pixelWidth; ++x)
                    {
                        const size_t pixel = size_t(y * pixelWidth + x) * 4;
                        const bool opaque = (*pPixels)[pixel + 3] != 0;
                        if (opaque) ++opaquePixels;
                        if ((*pPixels)[pixel] == 216 && (*pPixels)[pixel + 1] == 0
                            && (*pPixels)[pixel + 2] == 252 && !opaque) ++transparentSourcePixels;
                        const float hitX = image.x + (x + 0.5f) * image.width / pixelWidth;
                        const float hitY = image.y + (y + 0.5f) * image.height / pixelHeight;
                        require(runtime.isOpaqueHudPixelAtPoint(image, hitX, hitY) == opaque,
                            "Rendered hammer alpha and shared pointer hit testing disagree");
                    }
                }
                require(transparentSourcePixels == 5392 && opaquePixels == 2309,
                    "Original hammer palette-zero background was not preserved as transparent");
                std::cout << "Hammer transparency: source_zero=" << transparentSourcePixels
                    << " opaque=" << opaquePixels << " matching_hit_pixels=" << pixelWidth * pixelHeight << '\n';
                checkedHammer = true;
            }
            require(!requireHammerFixture || checkedHammer, "Original hammer is missing from the rendered shop shelf");
            save(reload ? "shop-reopened" : "shop-before-buy");
            if (reload)
            {
                require(pParty->gold() == state.startingGold
                    && pParty->inventoryItemCount() == state.startingItemCount,
                    "Reopening stock changed the restored party");
                next(9);
                break;
            }
            state.buyPrice = std::numeric_limits<int>::max();
            for (size_t i = 0; i < pStock->standardStock.size(); ++i)
            {
                const InventoryItem &item = pStock->standardStock[i];
                if (!item.objectDescriptionId) continue;
                const int price = HouseServiceRuntime::buyPrice(*pParty, *runtime.itemTable(),
                    *runtime.standardItemEnchantTable(), *runtime.specialItemEnchantTable(), *pHouse, item, reputation);
                std::cout << "Shop offer: slot=" << i << " item=" << item.objectDescriptionId
                    << " price=" << price << " quantity=" << item.quantity << '\n';
                if (price > 0 && price <= pParty->gold() && price < state.buyPrice
                    && pParty->inventoryItemCount(item.objectDescriptionId) == 0)
                {
                    state.buyPrice = price;
                    state.boughtItem = item;
                    state.boughtSlot = i;
                }
            }
            require(state.boughtItem.objectDescriptionId != 0,
                "No affordable distinct weapon is offered to the original 200-gold party");
            bool visiblePoint = false;
            for (const GameplayRenderedInspectableHudItem &image : runtime.renderedInspectableHudItems())
            {
                if (image.objectDescriptionId != state.boughtItem.objectDescriptionId) continue;
                for (float y = image.y + 2; y < image.y + image.height && !visiblePoint; y += 3)
                {
                    for (float x = image.x + 2; x < image.x + image.width; x += 3)
                    {
                        if (!runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                        state.pointerX = x;
                        state.pointerY = y;
                        visiblePoint = true;
                        break;
                    }
                }
                if (visiblePoint) break;
            }
            require(visiblePoint, "Purchased stock item has no actually rendered opaque pixels");
            click(state.pointerX, state.pointerY);
            require(pParty->gold() == state.startingGold - state.buyPrice
                && pParty->inventoryItemCount(state.boughtItem.objectDescriptionId) == state.boughtItem.quantity
                && pParty->inventoryItemCount() == state.startingItemCount + 1
                && pStock->standardStock[state.boughtSlot].objectDescriptionId == 0,
                "Normal shelf click did not buy exactly the selected weapon at its offered price");
            std::cout << "Shop bought: item=" << state.boughtItem.objectDescriptionId
                << " price=" << state.buyPrice << " gold=" << pParty->gold()
                << " status=" << runtime.statusBarEventText() << '\n';
            save("shop-after-buy");
            next(4);
            break;
        }
        case 4:
            click(state.pointerX, state.pointerY);
            require(pParty->gold() == state.startingGold - state.buyPrice
                && pParty->inventoryItemCount() == state.startingItemCount + 1,
                "Second click on empty stock duplicated a purchase");
            close();
            require(runtime.activeEventDialog().isActive && !runtime.houseShopOverlay().active,
                "One Escape should close the shelf and retain the shop dialogue");
            next(5);
            break;
        case 5:
            service(HouseActionId::OpenShopEquipmentMenu, 6);
            break;
        case 6:
            service(HouseActionId::ShopSell, 7);
            break;
        case 7:
        {
            require(runtime.inventoryNestedOverlay().active
                && runtime.inventoryNestedOverlay().mode == GameplayUiController::InventoryNestedOverlayMode::ShopSell,
                "Original sell action did not open the party inventory");
            const auto &items = pParty->members()[0].inventory;
            const auto item = std::find_if(items.begin(), items.end(), [&](const InventoryItem &entry)
                { return entry.objectDescriptionId == state.boughtItem.objectDescriptionId; });
            require(item != items.end(), "Purchased item did not reach the selected member's backpack");
            state.sellPrice = HouseServiceRuntime::sellPrice(*pParty, *runtime.itemTable(),
                *runtime.standardItemEnchantTable(), *runtime.specialItemEnchantTable(), *pHouse, *item, reputation);
            const ItemDefinition *pDefinition = runtime.itemTable()->get(item->objectDescriptionId);
            const std::string expectedStatus = "Sold " + ItemRuntime::displayName(*item, *pDefinition,
                runtime.standardItemEnchantTable(), runtime.specialItemEnchantTable())
                + " for " + std::to_string(state.sellPrice) + " gold.";
            const auto grid = runtime.resolveInventoryNestedOverlayGridArea(width, height);
            require(grid.has_value(), "Sell inventory grid is not visible");
            click(grid->x + (item->gridX + 0.5f) * grid->width / Character::InventoryWidth,
                grid->y + (item->gridY + 0.5f) * grid->height / Character::InventoryHeight);
            require(pParty->gold() == state.startingGold - state.buyPrice + state.sellPrice
                && pParty->inventoryItemCount() == state.startingItemCount
                && pParty->inventoryItemCount(state.boughtItem.objectDescriptionId) == 0,
                "Normal inventory click did not sell exactly the purchased weapon");
            require(runtime.statusBarEventText() == expectedStatus, "Sale status changed the sold item name");
            std::cout << "Shop sold: item=" << state.boughtItem.objectDescriptionId
                << " price=" << state.sellPrice << " gold=" << pParty->gold()
                << " status=" << runtime.statusBarEventText() << '\n';
            save("shop-after-sell");
            next(8);
            break;
        }
        case 8:
        case 9:
            if (runtime.activeEventDialog().isActive || runtime.inventoryNestedOverlay().active
                || runtime.houseShopOverlay().active)
            {
                close();
                next(9);
            }
            else
            {
                save("shop-route-complete");
                captures.push_back("shop-complete");
                state.complete = true;
                std::cout << "MM8 normal shop complete: reload=" << reload << " walked=" << state.walkedDistance
                    << " gold=" << pParty->gold() << '\n';
            }
            break;
        default:
            throw std::runtime_error("Unknown MM8 shop phase");
        }
    }

    struct Mm8PortalCheckState : Mm8StartCheckState
    {
        bool timerDialogueCaptured = false;
        bool reloadOnly = false;
        bool transitionCaptured = false;
        bool templeNpcCaptured = false;
        uint32_t templeNpcFrame = 0;
        uint32_t transitionFrame = 0;
        uint32_t entryFrame = 0;
        uint32_t timerDialogueFrame = 0;
        std::set<size_t> encounteredActors;
        std::set<size_t> defeatedActors;
        uint64_t experienceBeforeCombat = 0;
        uint64_t observedEnemyHealth = 0;
        uint32_t lastCombatProgressFrame = 0;
        bool combatCaptured = false;
        bool killCaptured = false;
        unsigned attackPresses = 0;
        unsigned mechanismButton = 0;
        int deliveryStartingGold = 0;
        int travelFare = 0;
        float departureMinutes = 0.0f;
        std::array<uint64_t, 5> deliveryStartingExperience = {};
        std::array<int, 5> restExpectedHealth = {};
        std::array<int, 5> restExpectedMana = {};
        std::array<uint64_t, 5> restExpectedConditions = {};
        std::vector<size_t> scenarioActors;
        std::array<size_t, 2> lootActors = {51, 55};
        bool selectedLootActors = false;
        size_t supplyTarget = 0;

    };

    static void rememberMm8RestExpectation(const Party &party, Mm8PortalCheckState &state)
    {
        if (party.members().size() != 5) throw std::runtime_error("Rest audit requires the five-member starter party");
        for (size_t i = 0; i < party.members().size(); ++i)
        {
            const Character &member = party.members()[i];
            const bool severePoison = member.conditions.test(size_t(CharacterCondition::PoisonSevere));
            if (!member.conditions.none() && !(member.conditions.count() == 1 && severePoison))
                throw std::runtime_error("Rest audit needs an expectation for this additional character condition");
            state.restExpectedHealth[i] = severePoison ? std::max(1, member.maxHealth / 4) : member.maxHealth;
            state.restExpectedMana[i] = severePoison ? member.maxSpellPoints / 4 : member.maxSpellPoints;
            state.restExpectedConditions[i] = member.conditions.to_ullong();
        }
    }

    static void verifyMm8RestRecovery(const Party &party, const Mm8PortalCheckState &state)
    {
        for (size_t i = 0; i < party.members().size(); ++i)
        {
            const Character &member = party.members()[i];
            if (member.health != state.restExpectedHealth[i] || member.spellPoints != state.restExpectedMana[i]
                || member.conditions.to_ullong() != state.restExpectedConditions[i])
                throw std::runtime_error("Normal rest recovery differs from the party's health and poison state");
        }
    }

    static void checkMm8Portals(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures, bool continueRecruitment = false)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember(),
            "MM8 portal route lost its normal surviving four-member party");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            std::cout << "Portal phase: " << phase << " position=" << pWorld->partyX() << ','
                << pWorld->partyY() << ',' << pWorld->partyFootZ() << '\n';
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "匕伤岛正常传送与废庙入口", {}), "Cannot preserve normal portal route checkpoint");
        };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.rightMouseButton.held = state.phase == 0;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto walk = [&](const auto &route)
        {
            require(state.waypoint < route.size(), "Walk reached a portal without activating its original event");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 25.0f)
            {
                std::cout << "Portal waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            if (moved < 500.0f) state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 30)
                std::cout << "Portal walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " moved=" << moved << " health=" << pParty->members()[0].health << '\n';
            require(state.stalledFrames < 60, "Normal portal route is blocked by terrain, geometry or an actor");
            return false;
        };
        if (frame == 1)
        {
            if (session.currentMapFileName() == "d05.blv")
            {
                require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(8)
                    && pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 0,
                    "Temple reload needs the completed original stone route");
                state.reloadOnly = true;
                next(12);
                return;
            }
            require(session.currentMapFileName() == "out01.odm" && pParty->hasQuestBit(8)
                && pParty->inventoryItemCount(618) == (pParty->hasQuestBit(2) ? 0 : 1),
                "Portal route requires the original MM8 party and consistent second portal state");
            if (pParty->hasQuestBit(1))
            {
                require(pParty->inventoryItemCount(617) == 0 && pParty->hasQuestBit(232)
                    && pParty->members()[0].equipment.mainHand == 1 && pParty->members()[0].equipment.armor == 84,
                    "Portal continuation requires the normally equipped, first-portal checkpoint");
                state.timerDialogueCaptured = true;
                next(pParty->hasQuestBit(2) ? 10 : 8);
                std::cout << "Resuming the saved island route; earlier phases are not replayed.\n";
            }
            else
            {
                require(pParty->inventoryItemCount(617) == 1
                    && pParty->members()[0].equipment.mainHand == 0 && pParty->members()[0].equipment.armor == 0,
                    "Full portal route requires the unchanged original shop-complete save");
            }
        }
        if (runtime.activeEventDialog().isActive)
        {
            const EventDialogContent &dialogue = runtime.activeEventDialog();
            if (state.phase == 12 && dialogue.presentation == EventDialogPresentation::Transition)
            {
                if (!state.transitionCaptured)
                {
                    std::cout << "Temple transition: title=" << dialogue.title
                        << " source=" << dialogue.sourceId << '\n';
                    for (const std::string &line : dialogue.lines)
                        std::cout << "Temple transition line: " << line << '\n';
                    captures.push_back("temple-confirm");
                    state.transitionCaptured = true;
                    state.transitionFrame = frame;
                    return;
                }
                if (frame < state.transitionFrame + 12) return;
                const auto confirm = std::find_if(dialogue.actions.begin(), dialogue.actions.end(),
                    [](const EventDialogAction &action)
                    {
                        return action.kind == EventDialogActionKind::MapTransitionConfirm;
                    });
                require(confirm != dialogue.actions.end() && confirm->enabled,
                    "Original temple transition has no enabled confirmation");
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, confirm->id);
                return;
            }
            const bool templeNpc = state.phase == 12 && session.currentMapFileName() == "d05.blv"
                && dialogue.sourceId == 264 && !pParty->hasQuestBit(403);
            require(templeNpc || (dialogue.sourceId == 27 && pParty->hasQuestBit(232)),
                "Portal route encountered an unexpected original dialogue");
            bool &captured = templeNpc ? state.templeNpcCaptured : state.timerDialogueCaptured;
            uint32_t &dialogueFrame = templeNpc ? state.templeNpcFrame : state.timerDialogueFrame;
            const char *pToken = templeNpc ? "temple-npc" : "timed-npc";
            if (!captured)
            {
                for (const std::string &line : dialogue.lines) std::cout << "MM8 NPC " << dialogue.sourceId
                    << ": " << line << '\n';
                save(pToken);
                if (!state.reloadOnly) captures.push_back(pToken);
                captured = true;
                dialogueFrame = frame;
                return;
            }
            if (frame < dialogueFrame + 12) return;
            if (templeNpc && continueRecruitment)
            {
                require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(8),
                    "Continuous temple entry lost the normal portal and crystal quest state");
                save("temple-entry");
                captures.push_back("temple-entry");
                state.complete = true;
                std::cout << "Normal temple entry preserves Simon's original greeting for recruitment\n";
                return;
            }
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(!runtime.activeEventDialog().isActive, "Normal Escape did not close the original NPC");
            if (templeNpc) state.entryFrame = frame;
            state.stalledFrames = 0;
            return;
        }
        if (frame < 20 || frame < state.phaseFrame + 12) return;
        if (state.phase == 4)
        {
            if (std::hypot(pWorld->partyX() + 480, pWorld->partyY() - 5432) < 400)
            {
                require(pParty->inventoryItemCount(617) == 1 && pParty->inventoryItemCount(618) == 1,
                    "The free town portal consumed a quest stone");
                next(5);
                return;
            }
            constexpr std::array<std::array<float, 2>, 7> route = {{{5800, 7800}, {5800, 6000},
                {7500, 5800}, {9500, 5800}, {9500, 4300}, {10112, 4300}, {10112, 4900}}};
            walk(route);
            return;
        }
        if (state.phase == 6)
        {
            if (std::hypot(pWorld->partyX() + 21528, pWorld->partyY() + 1384) < 400)
            {
                require(pParty->hasQuestBit(1) && pParty->hasQuestBit(8)
                    && pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 1,
                    "First crystal portal did not consume exactly its original stone and retain quest state");
                next(7);
                return;
            }
            constexpr std::array<std::array<float, 2>, 5> route = {{{-7424, -3072}, {-12032, -8448},
                {-12544, -9472}, {-12528, -9800}, {-12528, -10300}}};
            walk(route);
            return;
        }
        if ((state.phase == 8 || state.phase == 10) && game.m_pOutdoorWorldRuntime)
        {
            OutdoorWorldRuntime &world = *game.m_pOutdoorWorldRuntime;
            TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
            const auto experience = [&]()
            {
                uint64_t total = 0;
                for (const Character &member : pParty->members()) total += member.experience;
                return total;
            };
            for (size_t index : state.encounteredActors)
            {
                const auto *pPrevious = world.mapActorState(index);
                require(pPrevious, "An observed original combat actor disappeared");
                if (pPrevious->isDead && state.defeatedActors.insert(index).second)
                {
                    require(state.attackPresses > 0 && experience() > state.experienceBeforeCombat,
                        "Normal combat killed a hostile without granting experience");
                    std::cout << "Portal combat kill: actor=" << index << " xp=" << experience()
                        << " attack_presses=" << state.attackPresses << '\n';
                    if (!state.killCaptured)
                    {
                        save("route-first-kill");
                        captures.push_back("route-first-kill");
                        state.killCaptured = true;
                        return;
                    }
                }
            }
            size_t target = size_t(-1);
            float nearest = 650.0f;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                const auto *pActor = world.mapActorState(index);
                if (!pActor || !pActor->hostileToParty || pActor->isDead || pActor->isInvisible
                    || pActor->currentHp <= 0) continue;
                const float distance = std::hypot(pActor->preciseX - world.partyX(),
                    pActor->preciseY - world.partyY());
                if (distance < 2000.0f) state.encounteredActors.insert(index);
                if (distance < nearest && world.partyAttackActorHasLineOfSight(index))
                {
                    target = index;
                    nearest = distance;
                }
            }
            if (target != size_t(-1))
            {
                const auto *pActor = world.mapActorState(target);
                const auto &movement = game.m_activeWorldManifest.partyMovement;
                const float eyeHeight = movement.declared ? movement.eyeHeight : 176.0f;
                game.m_outdoorGameView.setCameraAngles(
                    std::atan2(pActor->preciseY - world.partyY(), pActor->preciseX - world.partyX()),
                    std::atan2(pActor->preciseZ + pActor->height * 0.5f - world.partyFootZ() - eyeHeight, nearest));
                if (!state.combatCaptured)
                {
                    state.experienceBeforeCombat = experience();
                    save("route-combat");
                    captures.push_back("route-combat");
                    state.combatCaptured = true;
                    return;
                }
                if (!turn.active())
                {
                    press(KeyboardAction::Combat);
                    require(turn.active(), "Normal Combat input did not enter turn mode");
                    return;
                }
                if (frame % 60 == 0)
                    std::cout << "Portal combat: actor=" << target << " hp=" << pActor->currentHp
                        << " distance=" << nearest << " party_hp=" << pParty->members()[0].health
                        << " active=" << pParty->activeMemberIndex() << " stage=" << int(turn.stage()) << '\n';
                state.stalledFrames = 0;
                if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait) return;
                if (turn.stage() == TurnBasedCombatStage::Movement)
                {
                    if (nearest > 200.0f)
                    {
                        const float x = world.partyX(), y = world.partyY();
                        press(KeyboardAction::Forward);
                        state.walkedDistance += std::hypot(world.partyX() - x, world.partyY() - y);
                    }
                    else press(KeyboardAction::Attack);
                    return;
                }
                if (!turn.playerActionReady(*pParty)) return;
                const size_t member = pParty->activeMemberIndex();
                const auto cast = [&](uint32_t spellId, std::optional<size_t> actor, std::optional<size_t> character)
                {
                    const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(spellId);
                    require(pSpell && runtime.tryCastSpellFromMember(member, spellId, pSpell->name),
                        "Cannot start the original learned route spell");
                    GameplaySpellService &spells = runtime.spellService();
                    PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                    request.targetActorIndex = actor;
                    request.targetCharacterIndex = character;
                    require(spells.validatePendingTargetSelectionRequest(runtime, request),
                        "Normal route spell target was rejected");
                    const int mana = pParty->members()[member].spellPoints;
                    const int hp = character ? pParty->members()[*character].health : 0;
                    const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                    require(result.castResult.succeeded() && pParty->members()[member].spellPoints < mana,
                        "Original learned route spell failed to consume mana");
                    if (character) require(pParty->members()[*character].health > hp,
                        "Original Heal did not restore the selected character");
                    spells.clearPendingTargetSelection(runtime);
                    session.updateGameplay({}, 0.0f);
                    std::cout << "Portal spell: caster=" << member << " id=" << spellId
                        << " target_actor=" << (actor ? int(*actor) : -1)
                        << " target_member=" << (character ? int(*character) : -1) << " mana=" << mana << "->"
                        << pParty->members()[member].spellPoints << '\n';
                };
                if (member == 1 && pParty->members()[member].knowsSpell(68)
                    && pParty->members()[member].spellPoints >= 2)
                {
                    size_t weakest = size_t(-1);
                    float fraction = 0.75f;
                    for (size_t index = 0; index < pParty->members().size(); ++index)
                    {
                        const Character &candidate = pParty->members()[index];
                        if (candidate.conditions.test(size_t(CharacterCondition::Dead))
                            || candidate.conditions.test(size_t(CharacterCondition::Eradicated))) continue;
                        const float health = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                        if (health < fraction) { weakest = index; fraction = health; }
                    }
                    if (weakest != size_t(-1))
                    {
                        cast(68, std::nullopt, weakest);
                        return;
                    }
                }
                if (member == 2 && pParty->members()[member].knowsSpell(2)
                    && pParty->members()[member].spellPoints >= 2)
                {
                    cast(2, target, std::nullopt);
                    ++state.attackPresses;
                    return;
                }
                press(KeyboardAction::Attack);
                ++state.attackPresses;
                return;
            }
            if (turn.active())
            {
                if (turn.canToggleOff(*pParty)) press(KeyboardAction::Combat);
                else if (turn.stage() == TurnBasedCombatStage::Movement) press(KeyboardAction::Attack);
                else if (turn.playerActionReady(*pParty)) press(KeyboardAction::Pass);
                return;
            }
        }
        if (state.phase == 8)
        {
            if (std::hypot(pWorld->partyX() + 13912, pWorld->partyY() - 14096) < 400)
            {
                require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2)
                    && pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 0,
                    "Second crystal portal did not activate and consume its original stone");
                next(9);
                return;
            }
            constexpr std::array<std::array<float, 2>, 4> route = {{{-20736, 3840}, {-20224, 4352},
                {-18880, 8500}, {-18880, 9030}}};
            walk(route);
            return;
        }
        if (state.phase == 10)
        {
            constexpr std::array<std::array<float, 2>, 3> route = {{{-12800, 16000},
                {-12800, 18432}, {-12800, 18540}}};
            if (walk(route)) next(11);
            return;
        }
        switch (state.phase)
        {
        case 0:
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "Original Knight portrait is absent");
            GameplayInputFrame pointer;
            pointer.screenWidth = width;
            pointer.screenHeight = height;
            pointer.rightMouseButton = {true, true, false};
            session.updateGameplay(pointer, 0.0f);
            click(portrait->x + portrait->width / 2, portrait->y + portrait->height / 2);
            click(portrait->x + portrait->width / 2, portrait->y + portrait->height / 2);
            pointer.rightMouseButton = {false, false, true};
            session.updateGameplay(pointer, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == 0,
                "Normal portrait clicks did not open the Knight inventory");
            next(1);
            break;
        }
        case 1: equipQuestInventoryItem(session, width, height, 84); next(2); break;
        case 2: equipQuestInventoryItem(session, width, height, 1); next(3); break;
        case 3:
        {
            require(pParty->members()[0].equipment.mainHand == 1 && pParty->members()[0].equipment.armor == 84,
                "Original sword and leather armor were not equipped");
            if (frame == state.phaseFrame + 12)
            {
                save("route-equipped");
                captures.push_back("route-equipped");
                return;
            }
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(!runtime.characterScreen().open, "Normal Escape did not close the equipment panel");
            next(4);
            break;
        }
        case 5: save("portal-town-landed"); captures.push_back("portal-town-landed"); next(6); break;
        case 7: save("portal-first-landed"); captures.push_back("portal-first-landed"); next(8); break;
        case 9: save("portal-second-landed"); captures.push_back("portal-second-landed"); next(10); break;
        case 11:
            if (frame == state.phaseFrame + 12)
            {
                save("temple-door");
                captures.push_back("temple-door");
                return;
            }
            game.m_outdoorGameView.setCameraAngles(1.5707963f, 0.0f);
            press(KeyboardAction::Trigger);
            next(12);
            break;
        case 12:
            require(session.currentMapFileName() == "d05.blv",
                "Normal original door did not enter the abandoned temple");
            if (state.entryFrame == 0)
            {
                state.entryFrame = frame;
                return;
            }
            // Entry autosave owns the first rendered preview. Give its readback time to finish
            // before requesting a diagnostic screenshot on the same framebuffer.
            if (frame < state.entryFrame + 20) return;
            require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(8),
                "Entering the temple lost original portal or quest state");
            save("temple-entry");
            captures.push_back("temple-entry");
            state.complete = true;
            std::cout << (state.reloadOnly ? "MM8 temple checkpoint restored; walked="
                : "Normal MM8 portal continuation and temple entry passed; walked=") << state.walkedDistance << '\n';
            break;
        default: throw std::runtime_error("Unknown MM8 portal route phase");
        }
    }

    static void checkMm8Temple(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        require(pParty && pParty->hasActableMember(), "Temple route lost its surviving original party");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.transitionCaptured = false;
            std::cout << "Temple phase: " << phase << " map=" << session.currentMapFileName() << '\n';
        };
        const auto press = [&](KeyboardAction action)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "废庙原始队员招募与往返", {}), "Cannot save the normal temple route");
        };
        const auto recruited = [&]()
        {
            const RosterEntry *pOriginal = game.m_gameDataLoader.getRosterTable().get(3);
            require(pOriginal && pParty->members().size() == 5 && pParty->members().back().rosterId == 3,
                "Simon did not join the fifth active party slot");
            const Character &member = pParty->members().back();
            require(member.name == pOriginal->name && member.level == pOriginal->level
                && member.experience == pOriginal->experience && member.might == pOriginal->might
                && member.intellect == pOriginal->intellect && member.personality == pOriginal->personality
                && member.endurance == pOriginal->endurance && member.accuracy == pOriginal->accuracy
                && member.speed == pOriginal->speed && member.luck == pOriginal->luck,
                "Simon differs from the original roster identity or attributes");
        };
        if (frame == 1)
        {
            require(session.currentMapFileName() == "d05.blv" && pParty->hasQuestBit(1)
                && pParty->hasQuestBit(2) && pParty->hasQuestBit(8), "Temple route needs the original portal result");
            state.reloadOnly = pParty->members().size() == 5;
            if (state.reloadOnly) { recruited(); next(9); }
            else require(pParty->members().size() == 4, "Temple recruitment needs one free active slot");
        }
        if (frame < 20 || frame < state.phaseFrame + 12) return;
        const EventDialogContent &dialogue = runtime.activeEventDialog();
        if ((state.phase == 1 || state.phase == 3) && dialogue.isActive
            && dialogue.presentation == EventDialogPresentation::Transition)
        {
            const char *pToken = state.phase == 1 ? "temple-exit-dialog" : "temple-reenter-dialog";
            if (!state.transitionCaptured)
            {
                captures.push_back(pToken);
                state.transitionCaptured = true;
                state.transitionFrame = frame;
                return;
            }
            if (frame < state.transitionFrame + 12) return;
            const auto confirm = std::find_if(dialogue.actions.begin(), dialogue.actions.end(),
                [](const EventDialogAction &action)
                {
                    return action.kind == EventDialogActionKind::MapTransitionConfirm;
                });
            require(confirm != dialogue.actions.end(), "Original map transition has no confirmation");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, confirm->id);
            return;
        }
        switch (state.phase)
        {
        case 0:
            require(dialogue.isActive && dialogue.sourceId == 264,
                "Original temple load did not present Simon's available greeting");
            save("temple-simon-greeting");
            captures.push_back("temple-simon-greeting");
            next(4);
            break;
        case 14:
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene, "Temple departure has no indoor scene");
            const IndoorMoveState &position = pScene->partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(-1728.0f - position.y, -3008.0f - position.x),
                std::atan2(2592.0f - position.eyeZ(), std::hypot(-3008.0f - position.x, -1728.0f - position.y)));
            next(13);
            break;
        }
        case 13:
            press(KeyboardAction::Trigger);
            next(1);
            break;
        case 1:
            require(session.currentMapFileName() == "out01.odm", "Normal temple exit did not return outdoors");
            if (state.entryFrame == 0) { state.entryFrame = frame; return; }
            if (frame < state.entryFrame + 20) return;
            save("temple-exited");
            captures.push_back("temple-exited");
            state.entryFrame = 0;
            next(2);
            break;
        case 2:
        {
            IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
            require(pWorld, "Temple exit has no outdoor world");
            game.m_outdoorGameView.setCameraAngles(
                std::atan2(18784.0f - pWorld->partyY(), -12800.0f - pWorld->partyX()), 0.0f);
            press(KeyboardAction::Trigger);
            next(3);
            break;
        }
        case 3:
            require(session.currentMapFileName() == "d05.blv", "Normal temple re-entry did not load the original map");
            if (state.entryFrame == 0) { state.entryFrame = frame; return; }
            if (frame < state.entryFrame + 20) return;
            require(!dialogue.isActive, "The already recruited Simon appeared again on temple re-entry");
            recruited();
            save("temple-reentered");
            captures.push_back("temple-reentered");
            next(9);
            break;
        case 4:
            selectQuestDialog(runtime, EventDialogActionKind::RosterJoinOffer, 603);
            next(5);
            break;
        case 5:
            require(dialogue.isActive && dialogue.sourceId == 264, "Simon's original invitation disappeared");
            for (const std::string &line : dialogue.lines) std::cout << "Simon invitation: " << line << '\n';
            captures.push_back("temple-simon-offer");
            next(6);
            break;
        case 6:
        {
            const auto accept = std::find_if(dialogue.actions.begin(), dialogue.actions.end(),
                [](const EventDialogAction &action) { return action.kind == EventDialogActionKind::RosterJoinAccept; });
            require(accept != dialogue.actions.end(), "Simon's invitation has no acceptance action");
            selectQuestDialog(runtime, EventDialogActionKind::RosterJoinAccept, accept->id);
            next(7);
            break;
        }
        case 7:
            recruited();
            save("temple-simon-joined");
            captures.push_back("temple-simon-joined");
            next(8);
            break;
        case 8:
            close();
            require(!runtime.activeEventDialog().isActive, "Normal Escape did not close the recruitment result");
            next(14);
            break;
        case 9:
        {
            recruited();
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 4);
            require(portrait.has_value(), "Fifth active member has no portrait hit rectangle");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (int tap = 0; tap < 2; ++tap)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == 4,
                "Normal portrait double-click did not open Simon's fifth-member inventory");
            next(10);
            break;
        }
        case 10:
            captures.push_back("temple-simon-sheet");
            next(11);
            break;
        case 11:
            close();
            require(!runtime.characterScreen().open, "Normal Escape did not close Simon's inventory");
            next(12);
            break;
        case 12:
            recruited();
            require(pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 0
                && pParty->hasQuestBit(1) && pParty->hasQuestBit(2), "Temple recruitment lost portal state");
            save("temple-roster-complete");
            captures.push_back("temple-five-party");
            state.complete = true;
            std::cout << (state.reloadOnly ? "Temple fifth-member reload passed" : "Temple normal recruitment passed")
                << "; members=" << pParty->members().size() << " gold=" << pParty->gold() << '\n';
            break;
        default:
            throw std::runtime_error("Unknown normal temple recruitment phase");
        }
    }

    enum class Mm8TempleMechanism
    {
        Stairs,
        Passage,
        FloorPuzzle
    };
    struct FloorPuzzleButton
    {
        uint32_t eventId;
        float faceX, faceY, approachX, approachY;
    };
    static constexpr std::array<FloorPuzzleButton, 8> floorPuzzleButtons = {{{105, 7296, 2880, 7184, 2880},
        {104, 7104, 3072, 7104, 2960}, {103, 6208, 3072, 6208, 2960}, {102, 6016, 2880, 6128, 2880},
        {109, 6016, 1984, 6128, 1984}, {108, 6208, 1792, 6208, 1904}, {107, 7104, 1792, 7104, 1904},
        {106, 7296, 1984, 7184, 1984}}};

    static void checkMm8TempleMechanisms(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures, Mm8TempleMechanism mode,
        bool normalReplay = false)
    {
        const bool passage = mode == Mm8TempleMechanism::Passage;
        const bool floorPuzzle = mode == Mm8TempleMechanism::FloorPuzzle;
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(pScene && pScene->currentMapFileName() == "d05.blv", "Stair puzzle needs the original temple");
        GameSession &session = game.m_gameSession;
        IndoorWorldRuntime &world = pScene->worldRuntime();
        const IndoorMoveState &position = pScene->partyRuntime().movementState();
        Party *pParty = session.gameplayScreenRuntime().party();
        const EventRuntimeState *pEvents = world.eventRuntimeState();
        require(pParty && pEvents && pParty->hasActableMember() && pParty->inventoryItemCount(741) == 1,
            "Stair puzzle lost the surviving party or undelivered letter");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
        };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM8 废庙原始按钮与楼梯", {}), "Cannot preserve normal stair puzzle");
        };
        const auto walk = [&](std::span<const std::array<float, 2>> route)
        {
            require(state.waypoint < route.size(), "Stair puzzle has no remaining approach point");
            const float dx = route[state.waypoint][0] - position.x, dy = route[state.waypoint][1] - position.y;
            if (std::hypot(dx, dy) < 20.0f)
            {
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            const float x = position.x, y = position.y;
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(position.x - x, position.y - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "Stair puzzle walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << position.x << ',' << position.y << ',' << position.footZ
                    << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Stair puzzle approach stopped at actual geometry");
            return false;
        };
        if (frame == 1)
        {
            require(pParty->members().size() == 5 && pParty->hasQuestBit(3) && pParty->hasQuestBit(221),
                "Stair puzzle source is not the original five-member letter party");
            size_t checked = 0;
            for (size_t index = 0; index < (normalReplay ? world.mapActorCount() : 11); ++index)
            {
                GameplayRuntimeActorState actor;
                require(world.actorRuntimeState(index, actor), "Original chamber actor is unavailable");
                if (normalReplay && !(actor.monsterId >= 7 && actor.monsterId <= 9
                    && std::abs(actor.preciseZ) < 100 && std::abs(actor.preciseX) < 1200
                    && actor.preciseY > -300 && actor.preciseY < 1500)) continue;
                require(actor.isDead, "Clear the original central chamber before solving its stairs");
                ++checked;
            }
            require(checked > 0, "Original central chamber encounter is absent");
            require(pEvents->mapVars[31] <= 4, "Original stair progress is outside its four-button sequence");
            if (passage) require(pEvents->mapVars[31] == 4, "Passage route needs the normally solved staircase");
            const bool rampContact = passage && position.x > 4288.0f;
            next(rampContact ? 19 : passage ? 14 : pEvents->mapVars[31] == 4 ? 13 : 3 * pEvents->mapVars[31]);
            if (floorPuzzle)
            {
                require(pEvents->mapVars[31] == 4, "Floor puzzle needs the completed earlier staircase");
                next(pEvents->mapVars[12] >= 15 ? 28 : pEvents->mapVars[12] > 0 ? 24
                    : position.x >= 6000.0f ? 23 : 20);
                while (state.mechanismButton < floorPuzzleButtons.size()
                    && pEvents->mapVars[floorPuzzleButtons[state.mechanismButton].eventId - 100] != 0)
                    ++state.mechanismButton;
            }
        }
        if (frame < 24) return;
        if (floorPuzzle)
        {
            constexpr std::array<std::array<float, 2>, 11> approach = {{{4448, 2272}, {4512, 2208},
                {4576, 2144}, {4640, 2080}, {4704, 2016}, {4768, 1952}, {4843, 1877}, {4992, 1930},
                {5436, 2176}, {5700, 2336}, {5780, 2432}}};
            constexpr std::array<std::array<float, 2>, 4> enter = {{{6080, 2432}, {6400, 2432},
                {6900, 2432}, {7160, 2432}}};
            const auto triggerAt = [&](float x, float y, float z)
            {
                const float dx = x - position.x, dy = y - position.y;
                game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
                    std::atan2(z - position.eyeZ(), std::hypot(dx, dy)));
                press(KeyboardAction::Trigger);
            };
            require(position.footZ > -760.0f, "Party fell below the original retracting floor platforms");
            if (state.phase == 20)
            {
                if (walk(approach)) next(21);
                return;
            }
            if (state.phase == 21)
            {
                if (frame < state.phaseFrame + 12) return;
                triggerAt(5936, 2432, -520);
                require(pEvents->mechanisms.at(108).state != 2,
                    "Normal Trigger did not open the floor puzzle entrance");
                next(22);
                return;
            }
            if (state.phase == 22)
            {
                if (pEvents->mechanisms.at(108).state != 0)
                {
                    require(frame - state.phaseFrame < 1200, "Floor puzzle entrance did not finish opening");
                    return;
                }
                if (!walk(enter)) return;
                save("floor-puzzle-entry");
                captures.push_back("floor-puzzle-entry");
                next(23);
                return;
            }
            if (state.phase == 23)
            {
                if (frame < state.phaseFrame + 12) return;
                triggerAt(7312, 2432, -520);
                require(pEvents->mapVars[12] == 1, "Original exit-door action did not start the floor puzzle");
                for (uint32_t id = 110; id <= 113; ++id)
                    require(pEvents->mechanisms.at(id).isMoving, "Original retracting floor did not start moving");
                save("floor-puzzle-started");
                captures.push_back("floor-puzzle-started");
                state.entryFrame = frame;
                next(24);
                return;
            }
            if (state.phase == 24)
            {
                while (state.mechanismButton < floorPuzzleButtons.size()
                    && pEvents->mapVars[floorPuzzleButtons[state.mechanismButton].eventId - 100] != 0)
                    ++state.mechanismButton;
                if (state.mechanismButton == floorPuzzleButtons.size())
                {
                    next(26);
                    return;
                }
                const FloorPuzzleButton &button = floorPuzzleButtons[state.mechanismButton];
                const std::array<std::array<float, 2>, 1> route = {{{button.approachX, button.approachY}}};
                if (walk(route)) next(25);
                return;
            }
            if (state.phase == 25)
            {
                if (frame < state.phaseFrame + 12) return;
                const FloorPuzzleButton &button = floorPuzzleButtons[state.mechanismButton];
                triggerAt(button.faceX, button.faceY, -480);
                require(pEvents->mapVars[button.eventId - 100] == 1,
                    "Normal Trigger did not toggle the selected floor puzzle button");
                const std::string token = "floor-button-" + std::to_string(button.eventId);
                save(token);
                captures.push_back(token);
                std::cout << "Original floor button pressed: event=" << button.eventId
                    << " count=" << int(pEvents->mapVars[12]) << '\n';
                ++state.mechanismButton;
                next(24);
                return;
            }
            if (state.phase == 26)
            {
                require(frame - state.phaseFrame < 1200, "Original floor puzzle timer did not unlock its exit");
                if (pEvents->mapVars[12] != 15) return;
                for (uint32_t id = 110; id <= 113; ++id)
                    require(!pEvents->mechanisms.at(id).isMoving, "Solved floor continued retracting");
                if (pEvents->mechanisms.at(108).state != 0 || pEvents->mechanisms.at(109).state != 0) return;
                save("floor-puzzle-solved");
                captures.push_back("floor-puzzle-solved");
                next(27);
                return;
            }
            if (state.phase == 27 || state.phase == 28)
            {
                constexpr std::array<std::array<float, 2>, 2> exit = {{{7184, 2432}, {7520, 2432}}};
                constexpr std::array<std::array<float, 2>, 2> reload = {{{7440, 2432}, {7520, 2432}}};
                const std::span<const std::array<float, 2>> route = state.phase == 27
                    ? std::span<const std::array<float, 2>>(exit) : std::span<const std::array<float, 2>>(reload);
                if (walk(route)) next(29);
                return;
            }
            require(state.phase == 29, "Unknown retracting-floor puzzle stage");
            if (frame < state.phaseFrame + 30) return;
            require(pEvents->mapVars[12] == 15 && position.x > 7376 && std::abs(position.footZ + 640.0f) < 1.0f,
                "Floor puzzle exit was not crossed normally");
            save("floor-puzzle-complete");
            captures.push_back("floor-puzzle-complete");
            state.complete = true;
            std::cout << "Normal MM8 retracting-floor puzzle passed: walked=" << state.walkedDistance
                << " progress=" << int(pEvents->mapVars[12]) << " height=" << position.footZ << '\n';
            return;
        }
        if (passage)
        {
            constexpr std::array<std::array<float, 2>, 4> corridor = {{{64, 2304}, {192, 2304},
                {1408, 2304}, {1960, 2304}}};
            constexpr std::array<std::array<float, 2>, 23> bridge = {{{2304, 2304}, {2448, 2256},
                {2581, 2240}, {2656, 2008}, {2832, 1784}, {3072, 1696}, {3280, 1776}, {3413, 1899},
                {3296, 2096}, {3176, 2400}, {3072, 2645}, {3176, 2784}, {3328, 2880}, {3499, 2944},
                {3632, 2784}, {3744, 2592}, {3808, 2416}, {3872, 2272}, {3925, 2133}, {4048, 2176},
                {4160, 2256}, {4256, 2304}, {4352, 2368}}};
            if (state.phase == 19)
            {
                if (frame < state.phaseFrame + 60) return;
                if (!state.killCaptured)
                {
                    save("passage-ramp-contact");
                    captures.push_back("passage-ramp-contact");
                    state.killCaptured = true;
                }
                constexpr std::array<std::array<float, 2>, 1> landing = {{{4352, 2368}}};
                if (walk(landing)) next(18);
                return;
            }
            if (state.phase == 14)
            {
                if (!walk(corridor)) return;
                require(std::abs(position.footZ + 352.0f) < 1.0f,
                    "Northern passage did not descend the original stairs");
                save("passage-door-closed");
                captures.push_back("passage-door-closed");
                next(15);
                return;
            }
            if (state.phase == 15)
            {
                if (frame < state.phaseFrame + 12) return;
                const float dx = 2112.0f - position.x, dy = 2304.0f - position.y;
                game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
                    std::atan2(-224.0f - position.eyeZ(), std::hypot(dx, dy)));
                press(KeyboardAction::Trigger);
                require(pEvents->mechanisms.contains(5) && pEvents->mechanisms.at(5).state != 2,
                    "Normal Trigger did not open passage door five");
                next(16);
                return;
            }
            if (state.phase == 16)
            {
                if (pEvents->mechanisms.at(5).state != 0)
                {
                    require(frame - state.phaseFrame < 1200, "Passage door animation did not complete");
                    return;
                }
                save("passage-door-open");
                captures.push_back("passage-door-open");
                next(17);
                return;
            }
            require(state.phase == 17 || state.phase == 18, "Unknown northern passage stage");
            require(position.footZ > -400.0f, "Party fell from the original winding bridge");
            if (state.phase == 17)
            {
                if (walk(bridge)) next(18);
                return;
            }
            if (frame < state.phaseFrame + 60) return;
            std::cout << "Passage landing: position=" << position.x << ',' << position.y << ',' << position.footZ
                << " support=" << position.supportFaceIndex << " grounded=" << position.grounded << '\n';
            require(position.x > 4288.0f && std::abs(position.footZ + 352.0f) < 1.0f,
                "Party has not crossed the original bridge to its eastern passage");
            save("passage-complete");
            captures.push_back("passage-complete");
            state.complete = true;
            std::cout << "Normal MM8 northern passage passed: walked=" << state.walkedDistance
                << " height=" << position.footZ << " gold=" << pParty->gold()
                << " food=" << pParty->food() << '\n';
            return;
        }
        constexpr std::array<std::array<float, 2>, 3> first = {{{-400, 1160}, {-400, 896}, {-620, 896}}};
        constexpr std::array<std::array<float, 2>, 2> second = {{{-400, 896}, {740, 896}}};
        constexpr std::array<std::array<float, 2>, 4> third = {{{500, 896}, {500, 448}, {400, 0}, {-620, 0}}};
        constexpr std::array<std::array<float, 2>, 1> fourth = {{{740, 0}}};
        constexpr std::array<std::array<float, 2>, 9> exitRoute = {{{500, 0}, {500, 448}, {500, 896},
            {320, 1160}, {-416, 1160}, {-416, 1344}, {-160, 1344}, {0, 1344}, {64, 1344}}};
        constexpr std::array<std::array<float, 2>, 6> reloadRoute = {{{0, 1344}, {-416, 1344},
            {-416, 1160}, {-416, 1344}, {-160, 1344}, {64, 1344}}};
        if (state.phase == 12 || state.phase == 13)
        {
            if (state.phase == 13 && state.waypoint == 3 && !state.killCaptured)
            {
                require(std::abs(position.footZ) < 1.0f, "Reloaded stair descent did not reach the chamber floor");
                save("stair-reload-ground");
                captures.push_back("stair-reload-ground");
                state.killCaptured = true;
            }
            const std::span<const std::array<float, 2>> route = state.phase == 12
                ? std::span<const std::array<float, 2>>(exitRoute)
                : std::span<const std::array<float, 2>>(reloadRoute);
            if (!walk(route)) return;
            require(pEvents->mapVars[31] == 4 && position.footZ >= 159.0f && position.footZ <= 161.0f,
                "Four original buttons did not yield a normally climbable exit staircase");
            save("stair-puzzle-complete");
            captures.push_back("stair-puzzle-complete");
            state.complete = true;
            std::cout << "Normal four-button stair puzzle passed: progress=" << int(pEvents->mapVars[31])
                << " height=" << position.footZ << " walked=" << state.walkedDistance << '\n';
            return;
        }
        const unsigned button = state.phase / 3;
        require(button < 4, "Unknown original stair puzzle stage");
        if (state.phase % 3 == 0)
        {
            const std::span<const std::array<float, 2>> route = button == 0
                ? std::span<const std::array<float, 2>>(first) : button == 1
                ? std::span<const std::array<float, 2>>(second) : button == 2
                ? std::span<const std::array<float, 2>>(third) : std::span<const std::array<float, 2>>(fourth);
            if (walk(route)) next(state.phase + 1);
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        if (state.phase % 3 == 1)
        {
            const float x = button % 2 == 0 ? -752.0f : 880.0f;
            const float y = button < 2 ? 896.0f : 0.0f;
            const float dx = x - position.x, dy = y - position.y;
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
                std::atan2(144.0f - position.eyeZ(), std::hypot(dx, dy)));
            press(KeyboardAction::Trigger);
            std::cout << "Original stair button pressed: event=" << 513 + button
                << " progress=" << int(pEvents->mapVars[31]) << '\n';
            next(state.phase + 1);
            return;
        }
        require(pEvents->mapVars[31] == button + 1, "Normal Trigger did not advance the original button sequence");
        const uint32_t stairId = 101 + 2 * button;
        require(pEvents->mechanisms.contains(stairId), "Original animated stair state is absent");
        if (pEvents->mechanisms.at(stairId).state != 0)
        {
            require(frame - state.phaseFrame < 1200, "Original stair animation did not finish");
            return;
        }
        const std::string token = "stair-button-" + std::to_string(button + 1);
        save(token);
        captures.push_back(token);
        next(state.phase + 1);
    }

    static void checkMm8Revisit(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pWorld && pParty && pParty->members().size() == 5 && pParty->hasActableMember(),
            "Temple revisit requires the normally resupplied five-member party");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            std::cout << "Revisit phase=" << phase << " map=" << session.currentMapFileName() << '\n';
        };
        const auto near = [&](float x, float y)
        {
            return std::hypot(pWorld->partyX() - x, pWorld->partyY() - y) < 350.0f;
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "MM8 带信返回废庙", {}), "Cannot preserve normal temple revisit");
        };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        const auto walk = [&](const auto &route, bool indoor = false)
        {
            require(state.waypoint < route.size(), "Revisit route ended without its normal portal activating");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            if (distance < 24.0f)
            {
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            if (indoor) game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            else game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, indoor && distance > 80.0f ? 0.05f : 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            if (moved < 500.0f) state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "Revisit walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal revisit approach stopped at actual geometry or an actor");
            return false;
        };
        if (frame == 1)
        {
            require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(3)
                && pParty->hasQuestBit(221) && pParty->inventoryItemCount(741) == 1
                && pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 0,
                "Revisit must retain the original accepted letter and consumed portal stones");
            if (session.currentMapFileName() == "d05.blv")
            {
                state.reloadOnly = true;
                next(pWorld->partyFootZ() < 1100 ? 7 : 6);
            }
            else require(session.currentMapFileName() == "out01.odm" && near(9400, 6320),
                "Revisit input is not the normally saved chief-hall departure");
        }
        if (frame < 24) return;
        if (state.phase == 0)
        {
            if (near(-480, 5432)) { next(1); return; }
            constexpr std::array<std::array<float, 2>, 5> route = {{{9400, 5800}, {10480, 5500},
                {10480, 5300}, {10112, 5200}, {10112, 4900}}};
            walk(route);
            return;
        }
        if (state.phase == 1)
        {
            if (near(-21528, -1384)) { save("revisit-island-two"); next(2); return; }
            constexpr std::array<std::array<float, 2>, 5> route = {{{-7424, -3072}, {-12032, -8448},
                {-12544, -9472}, {-12528, -9856}, {-12528, -10200}}};
            walk(route);
            return;
        }
        if (state.phase == 2)
        {
            if (near(-13912, 14096)) { save("revisit-temple-island"); next(3); return; }
            constexpr std::array<std::array<float, 2>, 4> route = {{{-20736, 3840}, {-20224, 4352},
                {-18880, 8500}, {-18880, 9030}}};
            walk(route);
            return;
        }
        if (state.phase == 3)
        {
            constexpr std::array<std::array<float, 2>, 3> route = {{{-12800, 16000},
                {-12800, 18432}, {-12800, 18540}}};
            if (walk(route)) next(4);
            return;
        }
        if (state.phase == 6)
        {
            require(session.currentMapFileName() == "d05.blv", "Revisit did not enter the original temple");
            constexpr std::array<std::array<float, 2>, 10> route = {{{-3008, -1536}, {-2752, -1536},
                {-2752, -1088}, {-3264, -1088}, {-3264, -1536}, {-2800, -1536}, {-2800, -224},
                {-4080, -224}, {-4080, -1880}, {-3300, -1880}}};
            if (walk(route, true)) next(7);
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        if (state.phase == 4)
        {
            game.m_outdoorGameView.setCameraAngles(1.5707963268f, 0.0f);
            press(KeyboardAction::Trigger);
            next(5);
            return;
        }
        if (state.phase == 5)
        {
            if (session.currentMapFileName() == "out01.odm")
            {
                const EventDialogContent &dialogue = runtime.activeEventDialog();
                const auto confirm = std::find_if(dialogue.actions.begin(), dialogue.actions.end(),
                    [](const EventDialogAction &action)
                    {
                        return action.kind == EventDialogActionKind::MapTransitionConfirm;
                    });
                require(confirm != dialogue.actions.end(), "Original temple door has no transition confirmation");
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, confirm->id);
                state.entryFrame = frame;
                return;
            }
            if (frame < state.entryFrame + 20) return;
            require(!runtime.activeEventDialog().isActive, "Temple revisit unexpectedly repeated an NPC dialogue");
            save("revisit-entry");
            captures.push_back("revisit-entry");
            next(6);
            return;
        }
        require(state.phase == 7 && session.currentMapFileName() == "d05.blv" && pWorld->partyFootZ() < 1100,
            "Revisit did not return to the lower temple");
        require(pParty->inventoryItemCount(741) == 1 && pParty->hasQuestBit(3) && pParty->hasQuestBit(221),
            "Normal travel lost the undelivered letter");
        save("revisit-lower");
        captures.push_back("revisit-lower");
        state.complete = true;
        std::cout << "Normal resupplied MM8 temple revisit passed: walked=" << state.walkedDistance
            << " gold=" << pParty->gold() << " food=" << pParty->food() << '\n';
    }

    static void checkMm8Resupply(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pWorld && pParty && session.currentMapFileName() == "out01.odm"
            && pParty->members().size() == 5 && pParty->hasActableMember(),
            "Resupply requires the normally returned five-member MM8 party");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            std::cout << "Resupply phase=" << phase << " gold=" << pParty->gold()
                << " food=" << pParty->food() << " minutes=" << pWorld->gameMinutes() << '\n';
        };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "MM8 正常补给与送信", {}), "Cannot preserve resupply checkpoint");
        };
        const auto walk = [&](const auto &route)
        {
            require(state.waypoint < route.size(), "Resupply route ran out of waypoints");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 24.0f)
            {
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "Resupply walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Resupply approach stopped at actual geometry or an actor");
            return false;
        };
        if (frame == 1)
        {
            require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(8)
                && !runtime.activeEventDialog().isActive, "Resupply input lost completed portal state");
            if (pParty->hasQuestBit(3))
            {
                require(pParty->hasQuestBit(221) && pParty->inventoryItemCount(741) == 1,
                    "Letter reload lost the original quest or item");
                state.reloadOnly = true;
                next(9);
            }
            else require(pParty->inventoryItemCount(741) == 0, "Resupply input already contains a letter");
        }
        if (frame < 24) return;
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 10> route = {{{10480, 4488}, {10480, 5600},
                {9400, 5800}, {7500, 5800}, {5600, 6400}, {4000, 6400}, {4000, 8500}, {2700, 8500},
                {2700, 10000}, {2700, 11264}}};
            if (walk(route)) next(1);
            return;
        }
        if (state.phase == 8)
        {
            constexpr std::array<std::array<float, 2>, 9> route = {{{2700, 10000}, {2700, 8500},
                {4000, 8500}, {4000, 6400}, {5600, 6400}, {7500, 5800}, {9400, 5800}, {9400, 6150},
                {9400, 6320}}};
            if (walk(route)) next(9);
            return;
        }
        if (frame == state.phaseFrame + 4)
        {
            if (state.phase == 20) captures.push_back("resupply-tavern");
            if (state.phase == 4) captures.push_back("resupply-rest");
            if (state.phase == 6) captures.push_back("resupply-rested");
            if (state.phase == 12) captures.push_back("resupply-letter");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.1f);
            press(KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 228,
                "Normal Trigger did not open the original MM8 tavern");
            selectQuestDialog(runtime, EventDialogActionKind::HouseProprietor, 228);
            next(20);
            break;
        case 20:
            save("resupply-before-services");
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TavernBuyFood));
            next(3);
            break;
        case 3:
            require(pParty->food() == 14, "Tavern did not fill provisions to its normal target");
            save("resupply-food");
            rememberMm8RestExpectation(*pParty, state);
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TavernRentRoom));
            next(4);
            break;
        case 4:
            require(runtime.restScreenState().active, "Paid room did not open the shared rest screen");
            // Escape is the normal player control that finishes the displayed rest animation.
            close();
            next(5);
            break;
        case 5:
            require(runtime.restScreenState().mode == GameplayUiController::RestMode::None,
                "Normal rest completion did not finish the paid stay");
            if (runtime.restScreenState().active) close();
            verifyMm8RestRecovery(*pParty, state);
            save("resupply-rested");
            next(6);
            break;
        case 6:
            if (runtime.activeEventDialog().isActive) close();
            else next(8);
            break;
        case 9:
            game.m_outdoorGameView.setCameraAngles(1.5707963268f, -0.1f);
            press(KeyboardAction::Trigger);
            next(10);
            break;
        case 10:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 212,
                "Normal Trigger did not reopen the chief's hall");
            selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 2);
            next(11);
            break;
        case 11:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 20);
            next(12);
            break;
        case 12:
            if (runtime.heldInventoryItem().active)
            {
                require(runtime.heldInventoryItem().item.objectDescriptionId == 741,
                    "Dadeross offered an unexpected held item");
                const auto rect = runtime.resolvePartyPortraitRect(width, height, 0);
                require(rect.has_value(), "Letter has no normal recipient portrait");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = rect->x + rect->width * 0.5f;
                input.pointerY = rect->y + rect->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            require(pParty->hasQuestBit(3) && pParty->hasQuestBit(221)
                && pParty->inventoryItemCount(741) == 1 && !runtime.heldInventoryItem().active,
                "Dadeross did not grant exactly one letter with the original quest flags");
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                save("resupply-letter-complete");
                state.complete = true;
                std::cout << "Normal MM8 resupply and letter accepted: gold=" << pParty->gold()
                    << " food=" << pParty->food() << " walked=" << state.walkedDistance << '\n';
            }
            break;
        default:
            throw std::runtime_error("Unknown resupply phase");
        }
    }

    static void checkMm8Return(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pWorld && pParty && pParty->members().size() == 5 && pParty->hasActableMember(),
            "Return route needs the surviving original five-member party");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "MM8 废庙正常返回城镇", {}), "Cannot preserve normal return route");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            std::cout << "Return phase=" << phase << " map=" << session.currentMapFileName() << '\n';
        };
        const auto near = [&](float x, float y)
        {
            return std::hypot(pWorld->partyX() - x, pWorld->partyY() - y) < 350.0f;
        };
        const auto walk = [&](const auto &route, bool indoor)
        {
            require(state.waypoint < route.size(), "Return walk ended without its original portal activating");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            if (distance < 24.0f)
            {
                std::cout << "Return waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            if (indoor) game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            else game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, indoor && distance > 80.0f ? 0.05f : 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            if (moved < 500.0f) state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "Return walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " moved=" << moved << " walked=" << state.walkedDistance << '\n';
            require(state.stalledFrames < 60, "Normal return route stopped at collision geometry or an actor");
            return false;
        };
        if (frame == 1)
        {
            require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(8)
                && pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 0,
                "Return route needs the already used original power stones");
            require(!runtime.activeEventDialog().isActive, "Return input has an unfinished dialogue");
            if (session.currentMapFileName() == "out01.odm")
            {
                require(near(10123, 4488), "Return reload is not at the normal village portal landing");
                state.reloadOnly = true;
                next(7);
            }
            else require(session.currentMapFileName() == "d05.blv" && pWorld->partyFootZ() < 1100,
                "Return needs the normally recovered lower temple party");
        }
        if (frame < 24) return;
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 10> route = {{{-4080, -1880}, {-4080, -224},
                {-2800, -224}, {-2800, -1536}, {-3264, -1536}, {-3264, -1088}, {-2752, -1088},
                {-2752, -1536}, {-3008, -1536}, {-3008, -1696}}};
            if (walk(route, true))
            {
                require(pWorld->partyFootZ() >= 2432, "Normal reverse stairs did not regain the temple entrance");
                save("return-stairs");
                captures.push_back("return-stairs");
                next(1);
            }
            return;
        }
        if (state.phase == 1)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene, "Return lost the original temple scene");
            const IndoorMoveState &position = pScene->partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(-1728.0f - position.y, -3008.0f - position.x),
                std::atan2(2592.0f - position.eyeZ(), std::hypot(-3008.0f - position.x, -1728.0f - position.y)));
            next(2);
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        if (state.phase == 2)
        {
            press(KeyboardAction::Trigger);
            next(3);
            return;
        }
        if (state.phase == 3)
        {
            const EventDialogContent &dialogue = runtime.activeEventDialog();
            if (dialogue.isActive && dialogue.presentation == EventDialogPresentation::Transition)
            {
                if (!state.transitionCaptured)
                {
                    captures.push_back("return-door");
                    state.transitionCaptured = true;
                    state.transitionFrame = frame;
                    return;
                }
                if (frame < state.transitionFrame + 12) return;
                const auto action = std::find_if(dialogue.actions.begin(), dialogue.actions.end(),
                    [](const EventDialogAction &candidate)
                    {
                        return candidate.kind == EventDialogActionKind::MapTransitionConfirm;
                    });
                require(action != dialogue.actions.end(), "Original temple exit has no confirmation");
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, action->id);
                return;
            }
            require(session.currentMapFileName() == "out01.odm", "Normal temple exit did not return outdoors");
            if (state.entryFrame == 0) { state.entryFrame = frame; return; }
            if (frame < state.entryFrame + 20) return;
            save("return-exited");
            captures.push_back("return-exited");
            next(4);
            return;
        }
        if (state.phase == 4)
        {
            if (near(-18952, 8608))
            {
                save("return-island-two");
                captures.push_back("return-island-two");
                next(5);
                return;
            }
            constexpr std::array<std::array<float, 2>, 5> route = {{{-12800, 18540}, {-12800, 18432},
                {-12800, 16000}, {-13912, 14096}, {-13840, 13600}}};
            walk(route, false);
            return;
        }
        if (state.phase == 5)
        {
            if (near(-12496, -9728))
            {
                save("return-island-one");
                captures.push_back("return-island-one");
                next(6);
                return;
            }
            constexpr std::array<std::array<float, 2>, 5> route = {{{-18880, 8500}, {-20224, 4352},
                {-20736, 3840}, {-21528, -1384}, {-21456, -1696}}};
            walk(route, false);
            return;
        }
        if (state.phase == 6)
        {
            if (near(10123, 4488)) { next(7); return; }
            constexpr std::array<std::array<float, 2>, 5> route = {{{-12544, -9472}, {-12032, -8448},
                {-7424, -3072}, {-480, 5432}, {-512, 4896}}};
            walk(route, false);
            return;
        }
        require(state.phase == 7 && session.currentMapFileName() == "out01.odm" && near(10123, 4488),
            "Return route did not reach the original village landing");
        require(pParty->hasQuestBit(1) && pParty->hasQuestBit(2) && pParty->hasQuestBit(8)
            && pParty->inventoryItemCount(617) == 0 && pParty->inventoryItemCount(618) == 0,
            "Activated return portals changed stone or quest state");
        save("return-town-complete");
        captures.push_back("return-town-complete");
        state.complete = true;
        std::cout << "Normal MM8 temple return passed: walked=" << state.walkedDistance
            << " gold=" << pParty->gold() << " members=" << pParty->members().size() << '\n';
    }

    static bool handleMm8DungeonCombat(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures, const std::string &prefix,
        bool useMm8StarterSpells = true, bool approachInCombat = false,
        float maximumTargetDistance = 1100.0f)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(pScene, "Dungeon combat requires an indoor scene");
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party &party = pScene->party();
        IndoorWorldRuntime &world = pScene->worldRuntime();
        const IndoorMoveState &position = pScene->partyRuntime().movementState();
        TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "地牢正常战斗", {}), "Cannot preserve normal dungeon combat");
        };
        const auto experience = [&]()
        {
            uint64_t result = 0;
            for (const Character &member : party.members()) result += member.experience;
            return result;
        };
        for (size_t index : state.encounteredActors)
        {
            GameplayRuntimeActorState actor;
            require(world.actorRuntimeState(index, actor), "Observed dungeon actor disappeared");
            if (actor.isDead && state.defeatedActors.insert(index).second)
            {
                require(state.attackPresses > 0 && experience() > state.experienceBeforeCombat,
                    "Observed dungeon kill has no normal attack or experience");
                std::cout << "Dungeon kill: actor=" << index << " xp=" << experience()
                    << " attacks=" << state.attackPresses << '\n';
                if (!state.killCaptured)
                {
                    const std::string token = prefix + "-first-kill";
                    save(token);
                    captures.push_back(token);
                    state.killCaptured = true;
                    return true;
                }
            }
        }
        uint64_t enemyHealth = 0;
        for (size_t index = 0; index < world.mapActorCount(); ++index)
        {
            GameplayRuntimeActorState actor;
            if (world.actorRuntimeState(index, actor) && !actor.isDead && actor.hostileToParty)
                enemyHealth += std::max(0, int(world.mapDeltaData()->actors[index].hp));
        }
        if (state.lastCombatProgressFrame == 0 || enemyHealth != state.observedEnemyHealth)
        {
            state.observedEnemyHealth = enemyHealth;
            state.lastCombatProgressFrame = frame;
        }
        size_t target = size_t(-1);
        float nearest = maximumTargetDistance;
        GameplayRuntimeActorState selected;
        for (size_t index = 0; index < world.mapActorCount(); ++index)
        {
            GameplayRuntimeActorState actor;
            if (!world.actorRuntimeState(index, actor) || actor.isDead || actor.isInvisible || !actor.hostileToParty
                || std::abs(actor.preciseZ - position.footZ) > 450.0f
                || !world.partyAttackActorHasLineOfSight(index)) continue;
            const float distance = std::hypot(actor.preciseX - position.x, actor.preciseY - position.y);
            if (distance < 1600.0f) state.encounteredActors.insert(index);
            if (distance < nearest) { target = index; nearest = distance; selected = actor; }
        }
        if (target != size_t(-1))
        {
            game.m_indoorRenderer.setCameraAngles(std::atan2(selected.preciseY - position.y,
                selected.preciseX - position.x),
                std::atan2(selected.preciseZ + selected.height * 0.5f - position.eyeZ(), nearest));
            if (!state.combatCaptured)
            {
                const std::string token = prefix + "-combat";
                save(token);
                captures.push_back(token);
                state.combatCaptured = true;
                return true;
            }
            if (!turn.active()) { press(KeyboardAction::Combat); return true; }
            state.stalledFrames = 0;
            if (frame % 120 == 0)
                std::cout << "Dungeon combat: actor=" << target
                    << " distance=" << nearest << " hp=" << world.mapDeltaData()->actors[target].hp
                    << " member=" << party.activeMemberIndex()
                    << " stage=" << int(turn.stage()) << " party_hp=" << party.members()[0].health << '\n';
            if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait) return true;
            if (turn.stage() == TurnBasedCombatStage::Movement)
            {
                const float edgeDistance = std::max(0.0f, nearest - float(selected.radius));
                bool canAttackAtRange = false;
                for (size_t i = 0; i < party.members().size(); ++i)
                {
                    const Character &member = party.members()[i];
                    if (!GameMechanics::canAct(member)) continue;
                    if (party.equippedItemId(i, EquipmentSlot::Bow) != 0
                        || (useMm8StarterSpells && i == 2 && member.spellPoints >= 2))
                        canAttackAtRange = true;
                }
                const bool rangeStalled = frame - state.lastCombatProgressFrame > 600;
                if (approachInCombat && canAttackAtRange && !rangeStalled && edgeDistance < 650.0f)
                {
                    // Keep the lightly equipped starter party at ordinary bow/spell range. This is
                    // normal turn-based movement; collision and action points still govern the retreat.
                    game.m_indoorRenderer.setCameraAngles(std::atan2(position.y - selected.preciseY,
                        position.x - selected.preciseX), 0.0f);
                    const float x = position.x, y = position.y;
                    press(KeyboardAction::Forward, 0.025f);
                    const float moved = std::hypot(position.x - x, position.y - y);
                    state.walkedDistance += moved;
                    if (moved < 0.5f) press(KeyboardAction::Pass);
                }
                else if (approachInCombat && (!canAttackAtRange || rangeStalled) && edgeDistance > 300.0f)
                {
                    const float x = position.x, y = position.y;
                    press(KeyboardAction::Forward, 0.025f);
                    const float moved = std::hypot(position.x - x, position.y - y);
                    state.walkedDistance += moved;
                    if (moved < 0.5f) press(KeyboardAction::Pass);
                }
                else press(KeyboardAction::Pass);
                return true;
            }
            if (!turn.playerActionReady(party)) return true;
            const size_t member = party.activeMemberIndex();
            const auto cast = [&](uint32_t id, std::optional<size_t> actor, std::optional<size_t> character)
            {
                const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(id);
                require(pSpell && runtime.tryCastSpellFromMember(member, id, pSpell->name),
                    "Original dungeon spell could not begin");
                GameplaySpellService &spells = runtime.spellService();
                PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                request.targetActorIndex = actor;
                request.targetCharacterIndex = character;
                require(spells.validatePendingTargetSelectionRequest(runtime, request), "Dungeon target rejected");
                const int mana = party.members()[member].spellPoints;
                const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                require(result.castResult.succeeded() && party.members()[member].spellPoints < mana,
                    "Original dungeon spell did not consume mana");
                spells.clearPendingTargetSelection(runtime);
                session.updateGameplay({}, 0.0f);
                std::cout << "Dungeon spell: caster=" << member << " id=" << id
                    << " actor=" << (actor ? int(*actor) : -1) << " member=" << (character ? int(*character) : -1)
                    << " mana=" << mana << "->" << party.members()[member].spellPoints << '\n';
            };
            if (useMm8StarterSpells && member == 1 && party.members()[member].spellPoints >= 2)
            {
                size_t weakest = size_t(-1);
                float fraction = 0.75f;
                for (size_t index = 0; index < party.members().size(); ++index)
                {
                    const Character &candidate = party.members()[index];
                    if (candidate.conditions.test(size_t(CharacterCondition::Dead))
                        || candidate.conditions.test(size_t(CharacterCondition::Eradicated))) continue;
                    const float health = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                    if (health < fraction) { weakest = index; fraction = health; }
                }
                if (weakest != size_t(-1)) { cast(68, std::nullopt, weakest); return true; }
            }
            if (useMm8StarterSpells && member == 2 && party.members()[member].spellPoints >= 2)
            {
                cast(2, target, std::nullopt);
                ++state.attackPresses;
                return true;
            }
            // Use the target's edge distance when deciding whether to attempt a normal melee input.
            // The shared action controller still decides reach, hit, damage and recovery.
            const float edgeDistance = std::max(0.0f,
                std::hypot(nearest, selected.preciseZ - position.footZ) - float(selected.radius));
            if (edgeDistance <= 400.0f || party.equippedItemId(member, EquipmentSlot::Bow) != 0)
            {
                press(KeyboardAction::Attack);
                ++state.attackPresses;
            }
            else press(KeyboardAction::Pass);
            return true;
        }
        if (turn.active())
        {
            if (turn.canToggleOff(party)) press(KeyboardAction::Combat);
            else if (turn.stage() == TurnBasedCombatStage::Movement) press(KeyboardAction::Pass);
            else if (turn.playerActionReady(party)) press(KeyboardAction::Pass);
            return true;
        }
        return false;
    }

    struct Mm7DungeonCheckState : Mm8PortalCheckState
    {
        std::string dungeonMap = "7d06.blv";
        std::vector<std::array<float, 3>> outdoorRoute;
        size_t outdoorWaypoint = 0;
        std::optional<std::array<float, 3>> entryTarget;
        std::array<float, 3> exitTarget = {-1056.0f, -4672.0f, 700.0f};
        bool avoidCombat = false;
        uint32_t guideFrame = 0;
        std::vector<std::array<float, 3>> route;
        bool restAtEnd = false;
        bool exitAtEnd = false;
        bool entering = false;
        bool entryTriggered = false;
        uint32_t entryTriggerFrame = 0;
        bool mobileCombat = false;
        std::map<size_t, std::array<float, 3>> triggers;
        std::set<size_t> triggeredWaypoints;
        uint32_t triggerWaitFrame = 0;
        uint32_t collectItemId = 0;
        std::optional<uint32_t> collectChestId;
        bool collectChestOpened = false;
        bool collectRevisit = false;
        std::array<float, 3> collectPosition = {};
        bool collectTriggered = false;
        uint32_t collectTriggerFrame = 0;
        int restStartingFood = 0;
        int restFoodCost = 0;
        float restStartingMinutes = 0.0f;
    };

    static void checkMm7Dungeon(GameApplication &game, uint32_t frame, int width, int height,
        Mm7DungeonCheckState &state, std::vector<std::string> &captures, bool reload)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        if (frame == 1 && pScene) state.dungeonMap = session.currentMapFileName();
        require(state.dungeonMap == "7d06.blv" || state.dungeonMap == "7d28.blv",
            "MM7 exploration requires the original temple or dragon cave");
        if (frame == 1 && session.currentMapFileName() == "7out01.odm")
        {
            state.exitAtEnd = reload;
            state.entering = !reload;
        }
        require(pWorld && runtime.party() && ((pScene && session.currentMapFileName() == state.dungeonMap)
            || ((state.entering || (state.exitAtEnd && state.waypoint == state.route.size()))
                && session.currentMapFileName() == "7out01.odm")), "Moon Temple route left its original maps");
        Party &party = *runtime.party();
        const bool fourMembers = party.members().size() == 4;
        require(((party.members().size() == 2 && party.hasRosterMember(42)) || fourMembers) && party.hasActableMember(),
            "Moon Temple exploration lost its original surviving starter party");
        require(!runtime.activeEventDialog().isActive || state.entering
            || (state.exitAtEnd && state.waypoint == state.route.size()),
            "Inspect the original dialogue before continuing exploration");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "月亮神庙正常行走与战斗", {}), "Cannot preserve Moon Temple exploration");
        };
        if (frame == 1)
        {
            require((fourMembers ? ((party.hasQuestBit(513) || party.hasQuestBit(520))
                && (party.hasQuestBit(514) || party.hasQuestBit(521)))
                : (party.hasQuestBit(520) && party.hasQuestBit(521)))
                && party.members()[0].equipment.mainHand == 1 && party.members()[0].equipment.armor == 84,
                "Exploration lost the original equipment or scavenger progress");
            require(!fourMembers || (party.members()[1].className == "Cleric" && party.members()[1].knowsSpell(68)
                && party.members()[2].className == "Sorcerer" && party.members()[2].knowsSpell(2)
                && party.members()[3].className == "Archer" && party.members()[3].equipment.bow == 56),
                "The four-member route requires its normally prepared healer, caster and archer");
            if (reload)
            {
                save("mm7-dungeon-complete");
                captures.push_back("mm7-dungeon-complete");
                state.complete = true;
                std::cout << "MM7 dungeon checkpoint reloaded without replaying movement or rest\n";
                return;
            }
            for (const Character &member : party.members()) state.experienceBeforeCombat += member.experience;
            std::ifstream stream(std::filesystem::current_path().parent_path() / "mm7-dungeon-forward.tsv");
            require(stream.good(), "Moon Temple exploration needs its geometry-derived walking route");
            std::string line;
            while (std::getline(stream, line))
            {
                if (line.empty() || line.front() == '#') continue;
                if (line.starts_with("dungeon "))
                {
                    state.dungeonMap = line.substr(8);
                    require(state.dungeonMap == "7d06.blv" || state.dungeonMap == "7d28.blv",
                        "Unknown original MM7 dungeon");
                    continue;
                }
                if (line == "avoid-combat") { state.avoidCombat = true; continue; }
                if (line.starts_with("outdoor ") || line.starts_with("entry-target ")
                    || line.starts_with("exit-target "))
                {
                    std::istringstream values(line.substr(line.find(' ') + 1));
                    std::array<float, 3> point = {};
                    require(bool(values >> point[0] >> point[1] >> point[2]) && std::isfinite(point[0])
                        && std::isfinite(point[1]) && std::isfinite(point[2]), "Invalid original doorway route");
                    if (line.starts_with("outdoor "))
                    {
                        state.outdoorRoute.push_back(point);
                        require(state.outdoorRoute.size() <= 2000, "Outdoor approach exceeds waypoint limit");
                    }
                    else if (line.starts_with("entry-target ")) state.entryTarget = point;
                    else state.exitTarget = point;
                    continue;
                }
                if (line == "rest-at-end") { state.restAtEnd = true; continue; }
                if (line == "exit-at-end") { state.exitAtEnd = true; continue; }
                if (line == "mobile-combat") { state.mobileCombat = true; continue; }
                if (line.starts_with("trigger "))
                {
                    std::istringstream values(line.substr(8));
                    std::array<float, 3> target = {};
                    require(bool(values >> target[0] >> target[1] >> target[2]), "Invalid original trigger target");
                    require(state.triggers.emplace(state.route.size(), target).second,
                        "Multiple triggers at the same walking step are ambiguous");
                    continue;
                }
                if (line.starts_with("chest-at-end ") || line.starts_with("chest-revisit-at-end "))
                {
                    state.collectRevisit = line.starts_with("chest-revisit-at-end ");
                    std::istringstream values(line.substr(line.find(' ') + 1));
                    uint32_t chestId = 0;
                    require(bool(values >> state.collectItemId >> chestId >> state.collectPosition[0]
                        >> state.collectPosition[1] >> state.collectPosition[2]) && state.collectItemId != 0,
                        "Invalid original quest chest target");
                    require(party.inventoryItemCount(state.collectItemId) == (state.collectRevisit ? 1 : 0),
                        "Quest chest input has an unexpected item count");
                    state.collectChestId = chestId;
                    continue;
                }
                if (line.starts_with("collect-at-end ") || line.starts_with("collect-revisit-at-end "))
                {
                    state.collectRevisit = line.starts_with("collect-revisit-at-end ");
                    std::istringstream values(line.substr(line.find(' ') + 1));
                    require(bool(values >> state.collectItemId >> state.collectPosition[0]
                        >> state.collectPosition[1] >> state.collectPosition[2]) && state.collectItemId != 0,
                        "Invalid original ground item target");
                    require(party.inventoryItemCount(state.collectItemId) == (state.collectRevisit ? 1 : 0),
                        "Ground item input has an unexpected quest item count");
                    continue;
                }
                std::istringstream values(line);
                std::array<float, 3> point = {};
                require(bool(values >> point[0] >> point[1] >> point[2]) && std::isfinite(point[0])
                    && std::isfinite(point[1]) && std::isfinite(point[2]), "Invalid dungeon waypoint");
                state.route.push_back(point);
                require(state.route.size() <= 2000, "Dungeon route exceeds waypoint limit");
            }
            require(state.route.size() >= 2, "Dungeon walking route is empty");
            save("mm7-dungeon-input");
        }
        if (frame < 24) return;
        if (state.entering)
        {
            if (pScene)
            {
                require(session.currentMapFileName() == state.dungeonMap, "Original dungeon door reached another map");
                if (state.entryFrame == 0) { state.entryFrame = frame; return; }
                if (frame < state.entryFrame + 20) return;
                state.entering = false;
                save("mm7-dungeon-reentered");
                captures.push_back("mm7-dungeon-reentered");
                return;
            }
            if (state.outdoorWaypoint < state.outdoorRoute.size())
            {
                const auto &dialog = runtime.activeEventDialog();
                if (dialog.isActive)
                {
                    require(dialog.sourceId >= 358 && dialog.sourceId <= 376 && !dialog.isHouseDialog,
                        "Unexpected dialogue on the original cave approach");
                    if (state.guideFrame == 0) state.guideFrame = frame;
                    if (frame >= state.guideFrame + 12)
                    {
                        GameplayInputFrame input;
                        input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                        input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                        session.updateGameplay(input, 0.0f);
                        session.updateGameplay({}, 0.0f);
                    }
                    return;
                }
                state.guideFrame = 0;
                const auto &point = state.outdoorRoute[state.outdoorWaypoint];
                const float x = pWorld->partyX(), y = pWorld->partyY();
                const float dx = point[0] - x, dy = point[1] - y, distance = std::hypot(dx, dy);
                if (distance < 5.0f) { ++state.outdoorWaypoint; return; }
                game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                    std::clamp(distance / 1500.0f, 1.0f / 128.0f, 0.025f));
                const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
                state.walkedDistance += moved;
                state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
                if (frame % 120 == 0 || state.stalledFrames == 30)
                    std::cout << "MM7 dungeon approach: waypoint=" << state.outdoorWaypoint << " position="
                        << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << '\n';
                require(state.stalledFrames < 60, "Original dungeon approach stopped at collision geometry");
                return;
            }
            if (!state.entryTarget)
                require(std::hypot(pWorld->partyX() - 15816.0f, pWorld->partyY() - 12161.0f) < 100.0f
                    && std::abs(pWorld->partyFootZ() - 1133.0f) < 5.0f,
                    "Temple reentry requires the real outdoor exit position");
            if (!state.entryTriggered)
            {
                if (state.entryTarget)
                {
                    const float dx = (*state.entryTarget)[0] - pWorld->partyX();
                    const float dy = (*state.entryTarget)[1] - pWorld->partyY();
                    game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                        std::atan2((*state.entryTarget)[2] - pWorld->partyFootZ() - 176.0f, std::hypot(dx, dy)));
                }
                else game.m_outdoorGameView.setCameraAngles(0.0f, 0.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                state.entryTriggered = true;
                state.entryTriggerFrame = frame;
                return;
            }
            if (frame < state.entryTriggerFrame + 12) return;
            const auto &dialog = runtime.activeEventDialog();
            require(dialog.isActive && dialog.presentation == EventDialogPresentation::Transition,
                "Original temple door did not offer reentry");
            const auto confirm = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                [](const auto &action) { return action.kind == EventDialogActionKind::MapTransitionConfirm; });
            require(confirm != dialog.actions.end(), "Original reentry has no confirmation");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, confirm->id);
            return;
        }
        if (state.waypoint == state.route.size())
        {
            if (state.collectItemId != 0)
            {
                require(pScene != nullptr, "Ground item collection requires the original indoor scene");
                if (!state.collectTriggered)
                {
                    const auto &position = pScene->partyRuntime().movementState();
                    const float dx = state.collectPosition[0] - position.x;
                    const float dy = state.collectPosition[1] - position.y;
                    game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
                        std::atan2(state.collectPosition[2] - position.eyeZ(), std::hypot(dx, dy)));
                    pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                    state.collectTriggered = true;
                    state.collectTriggerFrame = frame;
                    return;
                }
                if (frame < state.collectTriggerFrame + 12) return;
                if (state.collectChestId)
                {
                    GameplayChestViewState *pChest = pScene->worldRuntime().activeChestView();
                    require(pChest && pChest->chestId == *state.collectChestId,
                        "Normal world interaction did not open the original quest chest");
                    if (!state.collectChestOpened)
                    {
                        save("mm7-dungeon-chest-open");
                        captures.push_back("mm7-dungeon-chest-open");
                        state.collectChestOpened = true;
                        state.collectTriggerFrame = frame;
                        return;
                    }
                    const auto item = std::find_if(pChest->items.begin(), pChest->items.end(),
                        [&](const GameplayChestItemState &candidate)
                        { return candidate.itemId == state.collectItemId; });
                    if (state.collectRevisit)
                    {
                        require(item == pChest->items.end(), "Reopening the chest regenerated its quest item");
                    }
                    else
                    {
                        require(item != pChest->items.end() && !item->isGold && item->item.quantity == 1,
                            "The original chest does not contain the requested single quest item");
                        const size_t before = pChest->items.size();
                        const auto grid = runtime.resolveChestGridArea(width, height);
                        require(grid.has_value(), "The original quest chest has no rendered grid");
                        GameplayInputFrame input;
                        input.screenWidth = width;
                        input.screenHeight = height;
                        input.pointerX = grid->x + (item->gridX + 0.5f) * grid->width / pChest->gridWidth;
                        input.pointerY = grid->y + (item->gridY + 0.5f) * grid->height / pChest->gridHeight;
                        input.leftMouseButton = {true, true, false};
                        session.updateGameplay(input, 0.0f);
                        input.leftMouseButton = {false, false, true};
                        session.updateGameplay(input, 0.0f);
                        require(runtime.heldInventoryItem().active
                            && runtime.heldInventoryItem().item.objectDescriptionId == state.collectItemId
                            && pChest->items.size() == before - 1,
                            "Normal quest chest click did not transfer exactly the requested item");
                    }
                }
                if (runtime.heldInventoryItem().active)
                {
                    require(runtime.heldInventoryItem().item.objectDescriptionId == state.collectItemId,
                        "Ground interaction selected a different original item");
                    const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
                    require(portrait.has_value(), "Collected ground item has no recipient portrait");
                    GameplayInputFrame input;
                    input.screenWidth = width;
                    input.screenHeight = height;
                    input.pointerX = portrait->x + portrait->width * 0.5f;
                    input.pointerY = portrait->y + portrait->height * 0.5f;
                    input.leftMouseButton = {true, true, false};
                    session.updateGameplay(input, 0.0f);
                    input.leftMouseButton = {false, false, true};
                    session.updateGameplay(input, 0.0f);
                }
                require(party.inventoryItemCount(state.collectItemId) == 1 && !runtime.heldInventoryItem().active,
                    "Normal ground item interaction did not store exactly one original quest item");
                if (state.collectChestId)
                {
                    GameplayInputFrame input;
                    input.screenWidth = width;
                    input.screenHeight = height;
                    input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                    input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                    session.updateGameplay(input, 0.0f);
                    session.updateGameplay({}, 0.0f);
                    require(!pScene->worldRuntime().activeChestView(), "Normal Escape did not close the quest chest");
                }
                std::cout << "MM7 original item collected: id=" << state.collectItemId
                    << " chest=" << (state.collectChestId ? int(*state.collectChestId) : -1)
                    << " revisited=" << state.collectRevisit << '\n';
                save("mm7-dungeon-item-collected");
                captures.push_back("mm7-dungeon-item-collected");
            }
            if (state.exitAtEnd && pScene)
            {
                if (!state.transitionCaptured)
                {
                    const auto &position = pScene->partyRuntime().movementState();
                    const float dx = state.exitTarget[0] - position.x, dy = state.exitTarget[1] - position.y;
                    game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
                        std::atan2(state.exitTarget[2] - position.eyeZ(), std::hypot(dx, dy)));
                    pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                    state.transitionCaptured = true;
                    state.transitionFrame = frame;
                    save("mm7-dungeon-exit-door");
                    captures.push_back("mm7-dungeon-exit-door");
                    return;
                }
                if (frame < state.transitionFrame + 12) return;
                const auto &dialog = runtime.activeEventDialog();
                require(dialog.isActive && dialog.presentation == EventDialogPresentation::Transition,
                    "Original exit neither changed maps nor offered a confirmation after rendering");
                const auto confirm = std::find_if(dialog.actions.begin(), dialog.actions.end(),
                    [](const auto &action) { return action.kind == EventDialogActionKind::MapTransitionConfirm; });
                require(confirm != dialog.actions.end(), "Original Moon Temple exit has no confirmation");
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, confirm->id);
                return;
            }
            const auto click = [&](const char *pLayoutId)
            {
                const auto *pLayout = runtime.findHudLayoutElement(pLayoutId);
                require(pLayout != nullptr, "Original rest control is missing");
                const auto button = runtime.resolveHudLayoutElement(pLayoutId, width, height,
                    pLayout->width, pLayout->height);
                require(button.has_value(), "Original rest control cannot be placed");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = button->x + button->width * 0.5f;
                input.pointerY = button->y + button->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            };
            if (state.restAtEnd)
            {
                if (state.phase == 0)
                {
                    if (state.exitAtEnd && frame < state.transitionFrame + 30) return;
                    save("mm7-dungeon-before-rest");
                    state.restStartingFood = party.food();
                    state.restFoodCost = runtime.restFoodRequired();
                    state.restStartingMinutes = pWorld->gameMinutes();
                    for (size_t i = 0; i < party.members().size(); ++i)
                    {
                        const Character &member = party.members()[i];
                        auto remaining = member.conditions;
                        for (CharacterCondition condition : {CharacterCondition::Unconscious, CharacterCondition::Drunk,
                            CharacterCondition::Fear, CharacterCondition::Asleep, CharacterCondition::Weak})
                            remaining.reset(size_t(condition));
                        state.restExpectedConditions[i] = remaining.to_ullong();
                        int divisor = 1;
                        for (const auto &[condition, factor] : std::array<std::pair<CharacterCondition, int>, 6>{{
                            {CharacterCondition::PoisonWeak, 2}, {CharacterCondition::DiseaseWeak, 2},
                            {CharacterCondition::PoisonMedium, 3}, {CharacterCondition::DiseaseMedium, 3},
                            {CharacterCondition::PoisonSevere, 4}, {CharacterCondition::DiseaseSevere, 4}}})
                        {
                            if (remaining.test(size_t(condition))) divisor = std::max(divisor, factor);
                            remaining.reset(size_t(condition));
                        }
                        require(remaining.none(), "MM7 rest audit needs an expectation for this character condition");
                        state.restExpectedHealth[i] = std::max(1, member.maxHealth / divisor);
                        state.restExpectedMana[i] = member.maxSpellPoints / divisor;
                    }
                    pressQuestGameplay(session, width, height, KeyboardAction::Rest);
                    std::cout << "Moon Temple rest request: " << runtime.statusBarEventText() << '\n';
                    require(runtime.restScreenState().active, "Normal Rest was restricted at the planned location");
                    captures.push_back("mm7-dungeon-rest-menu");
                    state.phase = 1;
                    state.phaseFrame = frame;
                    return;
                }
                if (frame < state.phaseFrame + 12) return;
                if (state.phase == 1)
                {
                    click("RestButton8Hours");
                    require(runtime.restScreenState().mode == GameplayUiController::RestMode::Heal
                        && party.food() == state.restStartingFood - state.restFoodCost,
                        "Normal eight-hour rest did not start or consumed incorrect food");
                    state.phase = 2;
                    state.phaseFrame = frame;
                    return;
                }
                require(frame < state.phaseFrame + 1800, "Normal rest did not finish its animation");
                if (runtime.restScreenState().mode != GameplayUiController::RestMode::None) return;
                verifyMm8RestRecovery(party, state);
                require(pWorld->gameMinutes() >= state.restStartingMinutes + 480.0f,
                    "Rest did not advance the original eight hours");
                if (runtime.restScreenState().active) click("RestButtonExit");
                require(!runtime.restScreenState().active, "Normal rest exit did not close the overlay");
                save("mm7-dungeon-rested");
                captures.push_back("mm7-dungeon-rested");
            }
            save("mm7-dungeon-complete");
            captures.push_back("mm7-dungeon-complete");
            state.complete = true;
            std::cout << "MM7 dungeon walk complete: walked=" << state.walkedDistance
                << " kills=" << state.defeatedActors.size() << " attacks=" << state.attackPresses
                << " gold=" << party.gold() << " rested=" << state.restAtEnd << '\n';
            return;
        }
        if (!state.avoidCombat && handleMm8DungeonCombat(game, frame, width, height, state, captures,
            "mm7-dungeon", fourMembers, state.mobileCombat))
            return;
        const IndoorMoveState &position = pScene->partyRuntime().movementState();
        if (state.triggerWaitFrame != 0)
        {
            const EventRuntimeState *pEvents = pScene->eventRuntimeState();
            require(pEvents != nullptr && frame < state.triggerWaitFrame + 1200,
                "Original doorway mechanism did not settle");
            if (frame < state.triggerWaitFrame + 12
                || std::any_of(pEvents->mechanisms.begin(), pEvents->mechanisms.end(),
                    [](const auto &mechanism) { return mechanism.second.isMoving; })) return;
            state.triggerWaitFrame = 0;
        }
        if (const auto trigger = state.triggers.find(state.waypoint);
            trigger != state.triggers.end() && !state.triggeredWaypoints.contains(state.waypoint))
        {
            const float dx = trigger->second[0] - position.x, dy = trigger->second[1] - position.y;
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
                std::atan2(trigger->second[2] - position.eyeZ(), std::hypot(dx, dy)));
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            state.triggeredWaypoints.insert(state.waypoint);
            state.triggerWaitFrame = frame;
            const std::string token = "mm7-dungeon-trigger-" + std::to_string(state.waypoint);
            save(token);
            captures.push_back(token);
            return;
        }
        require(state.waypoint < state.route.size(), "Dungeon route has no next point");
        const auto &target = state.route[state.waypoint];
        const float dx = target[0] - position.x, dy = target[1] - position.y;
        const float remaining = std::hypot(dx, dy);
        if (remaining < 12.0f)
        {
            require(std::abs(position.footZ - target[2]) < 80.0f, "Dungeon route reached a different floor layer");
            std::cout << "Moon Temple waypoint=" << state.waypoint << " position="
                << position.x << ',' << position.y << ',' << position.footZ << '\n';
            state.stalledFrames = 0;
            if (++state.waypoint == state.route.size())
            {
                state.phase = 0;
                state.phaseFrame = frame;
            }
            else if (state.waypoint % 10 == 0)
            {
                const std::string token = "mm7-dungeon-waypoint-" + std::to_string(state.waypoint);
                save(token);
                captures.push_back(token);
            }
            return;
        }
        const float x = position.x, y = position.y;
        game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
        pressQuestGameplay(session, width, height, KeyboardAction::Forward,
            std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f));
        const float moved = std::hypot(position.x - x, position.y - y);
        state.walkedDistance += moved;
        state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
        if (frame % 120 == 0 || state.stalledFrames == 30)
            std::cout << "Moon Temple walk: waypoint=" << state.waypoint << " position="
                << position.x << ',' << position.y << ',' << position.footZ << " moved=" << moved << '\n';
        require(state.stalledFrames < 60, "Dungeon walking route stopped at actual collision geometry");
    }

    static void checkMm8Delivery(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pWorld && pParty && pParty->members().size() == 5 && pParty->hasActableMember(),
            "MM8 delivery requires the original surviving five-member party");
        require(session.currentMapFileName() == "out01.odm" || session.currentMapFileName() == "out02.odm",
            "MM8 delivery left its original island and Ravenshore route");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            std::cout << "Delivery phase=" << phase << " map=" << session.currentMapFileName()
                << " gold=" << pParty->gold() << " minutes=" << pWorld->gameMinutes() << '\n';
        };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM8 正常乘船与交信", {}), "Cannot preserve the normal MM8 letter route");
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto walk = [&](std::span<const std::array<float, 2>> route)
        {
            require(state.waypoint < route.size(), "Letter route ran out of approach points");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 20.0f)
            {
                state.stalledFrames = 0;
                std::cout << "Delivery waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                return ++state.waypoint == route.size();
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "Delivery walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Normal letter approach stopped at actual geometry");
            return false;
        };
        if (frame == 1)
        {
            state.reloadOnly = pParty->hasQuestBit(4);
            require(state.reloadOnly ? pParty->inventoryItemCount(741) == 0
                : pParty->inventoryItemCount(741) == 1 && pParty->hasQuestBit(3) && pParty->hasQuestBit(221),
                "Delivery input has unexpected original letter flags or item count");
            require(!runtime.activeEventDialog().isActive, "Delivery input has an unfinished dialogue");
            state.deliveryStartingGold = pParty->gold();
            for (size_t i = 0; i < 5; ++i)
                state.deliveryStartingExperience[i] = pParty->members()[i].experience;
            if (session.currentMapFileName() == "out02.odm") next(state.reloadOnly ? 7 : 6);
            else require(!state.reloadOnly && pWorld->partyX() > 21000.0f,
                "Boat departure must begin at the normally reached temple exit");
        }
        if (frame < 24) return;
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 13> pier = {{{21100, 21106}, {20500, 21106},
                {19800, 20224}, {19200, 20224}, {18900, 20224}, {17500, 20224}, {16720, 20224},
                {16720, 20800}, {16720, 21080}, {16720, 21290}, {16820, 21300}, {16880, 21554},
                {17040, 21554}}};
            if (walk(pier)) next(1);
            return;
        }
        if (state.phase == 6)
        {
            constexpr std::array<std::array<float, 2>, 11> town = {{{10000, -15616}, {10784, -15616},
                {11000, -15616}, {12000, -13100}, {12160, -11968}, {12480, -11968},
                {12480, -12580}, {12480, -12700}, {12864, -12700}, {12864, -12400}, {12864, -12224}}};
            if (!walk(town)) return;
            save("delivery-arrival");
            captures.push_back("delivery-arrival");
            next(7);
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        if (state.phase == 1)
        {
            game.m_outdoorGameView.setCameraAngles(std::atan2(21554.0f - pWorld->partyY(),
                17200.0f - pWorld->partyX()), 0.0f);
            press(KeyboardAction::Trigger);
            next(2);
            return;
        }
        if (state.phase == 2)
        {
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 479,
                "Normal ship door did not open the original boat service");
            next(3);
            return;
        }
        if (state.phase == 3)
        {
            const HouseEntry *pHouse = runtime.houseTable()->get(479);
            require(pHouse && pHouse->transportRoutes.size() == 1, "Original boat route is absent or ambiguous");
            const HouseEntry::TransportRoute &route = pHouse->transportRoutes.front();
            require(route.mapFileName == "Out02.odm" && route.travelDays == 4 && route.x == 8609
                && route.y == -15609 && route.z == 265 && route.requiredQBit == 0,
                "Boat route differs from the original four-day destination");
            const Character *pMerchant = pParty->bestPartyWideUtilitySkillMember("Merchant");
            if (!pMerchant) pMerchant = pParty->activeMember();
            const int reputation = effectivePartyReputation(
                pWorld->currentLocationReputation(), pWorld->eventRuntimeState());
            state.travelFare = PriceCalculator::transportPrice(pMerchant, *pHouse, true, reputation);
            state.deliveryStartingGold = pParty->gold();
            state.departureMinutes = pWorld->gameMinutes();
            rememberMm8RestExpectation(*pParty, state);
            save("boat-departure");
            captures.push_back("boat-service");
            next(4);
            return;
        }
        if (state.phase == 4)
        {
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TransportRoute));
            require(pParty->gold() == state.deliveryStartingGold - state.travelFare,
                "Normal boat boarding charged an unexpected fare");
            require(std::abs(pWorld->gameMinutes() - state.departureMinutes - 5760.0f) < 0.1f,
                "Normal boat route did not advance exactly four days");
            verifyMm8RestRecovery(*pParty, state);
            next(5);
            return;
        }
        if (state.phase == 5)
        {
            require(session.currentMapFileName() == "out02.odm"
                && std::hypot(pWorld->partyX() - 8609.0f, pWorld->partyY() + 15609.0f) < 1.0f,
                "Normal boat did not arrive at its original Ravenshore landing");
            if (frame < state.phaseFrame + 24) return;
            save("boat-arrived");
            captures.push_back("boat-arrived");
            std::cout << "Normal boat arrived: fare=" << state.travelFare
                << " gold=" << pParty->gold() << " minutes=" << pWorld->gameMinutes() << '\n';
            next(6);
            return;
        }
        if (state.phase == 7)
        {
            game.m_outdoorGameView.setCameraAngles(std::atan2(-12096.0f - pWorld->partyY(),
                12864.0f - pWorld->partyX()), 0.0f);
            press(KeyboardAction::Trigger);
            next(8);
            return;
        }
        if (state.phase == 8)
        {
            const EventDialogContent &dialog = runtime.activeEventDialog();
            require(dialog.isActive && dialog.sourceId == 3, "Normal front door did not speak to Elgar Fellmoon");
            if (state.reloadOnly)
            {
                require(std::none_of(dialog.actions.begin(), dialog.actions.end(), [](const EventDialogAction &a)
                    { return a.kind == EventDialogActionKind::NpcTopic && a.id == 28; }),
                    "Completed letter topic was offered again after restart");
                save("delivery-reopened");
                captures.push_back("delivery-reopened");
            }
            else
            {
                state.deliveryStartingGold = pParty->gold();
                save("delivery-before-reward");
                selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 28);
            }
            next(9);
            return;
        }
        if (state.phase == 9)
        {
            require(pParty->hasQuestBit(4) && !pParty->hasQuestBit(3) && !pParty->hasQuestBit(221)
                && pParty->inventoryItemCount(741) == 0, "Letter delivery did not update the original quest and item");
            require(pParty->gold() == state.deliveryStartingGold + (state.reloadOnly ? 0 : 2500),
                "Letter dialogue lost or repeated the original gold reward");
            for (size_t i = 0; i < 5; ++i)
                require(state.reloadOnly ? pParty->members()[i].experience == state.deliveryStartingExperience[i]
                    : pParty->members()[i].experience > state.deliveryStartingExperience[i],
                    "Letter dialogue lost or repeated experience");
            if (!state.reloadOnly)
            {
                save("delivery-reward");
                captures.push_back("delivery-response");
            }
            next(10);
            return;
        }
        if (state.phase == 10)
        {
            if (runtime.activeEventDialog().isActive) close();
            else next(11);
            return;
        }
        require(state.phase == 11, "Unknown original MM8 delivery phase");
        save("mm8-delivery-complete");
        captures.push_back("mm8-delivery-complete");
        state.complete = true;
        std::cout << "Normal MM8 letter delivery passed: walked=" << state.walkedDistance
            << " gold=" << pParty->gold() << " reload=" << state.reloadOnly << '\n';
    }

    struct Mm8BlackmailCheckState : Mm8PortalCheckState
    {
        std::vector<std::array<float, 3>> route;
        std::array<uint32_t, 5> rewardStartingExperience = {};
        uint32_t doorWaitUntil = 0;
        size_t lastBlockedWaypoint = size_t(-1);
        unsigned blockedAttempts = 0;
        unsigned indoorInteractions = 0;
        unsigned bypassSteps = 0;
        bool blockingCombat = false;
        bool reloadElgarVerified = false;
        int32_t deliveryHistoryTime = 0;
    };

    static void checkMm8Blackmail(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm8BlackmailCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 5 && pParty->hasActableMember(),
            "MM8 blackmail needs the surviving normal five-member party");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM8走私者黑函正常任务", {}), "Cannot preserve MM8 blackmail evidence");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            state.doorWaitUntil = 0;
            state.lastBlockedWaypoint = size_t(-1);
            state.blockedAttempts = 0;
            state.bypassSteps = 0;
            std::cout << "Blackmail phase=" << phase << " map=" << session.currentMapFileName()
                << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                << " item742=" << pParty->inventoryItemCount(742) << '\n';
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto hasAction = [&](EventDialogActionKind kind, uint32_t id)
        {
            const EventDialogContent &dialog = runtime.activeEventDialog();
            return std::any_of(dialog.actions.begin(), dialog.actions.end(), [&](const EventDialogAction &action)
            {
                return action.kind == kind && action.id == id && action.enabled;
            });
        };
        const auto aimOutdoor = [&](float x, float y, float z)
        {
            const float dx = x - pWorld->partyX(), dy = y - pWorld->partyY();
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                std::atan2(z - pWorld->partyFootZ() - 176.0f, std::hypot(dx, dy)));
        };
        const auto aimIndoor = [&](float x, float y, float z)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "MM8 blackmail indoor aim lost the d07 scene");
            const IndoorMoveState &position = pScene->partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(y - position.y, x - position.x),
                std::atan2(z - position.eyeZ(), std::hypot(x - position.x, y - position.y)));
        };
        const auto loadRoute = [&](const char *name, bool reverse)
        {
            state.route.clear();
            const std::filesystem::path path =
                std::filesystem::current_path().parent_path() / "routes" / name;
            std::ifstream stream(path);
            require(stream.good(), "Missing MM8 blackmail route: " + path.string());
            std::string line;
            while (std::getline(stream, line))
            {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream values(line);
                std::array<float, 3> point = {};
                require(bool(values >> point[0] >> point[1] >> point[2])
                    && std::isfinite(point[0]) && std::isfinite(point[1]) && std::isfinite(point[2]),
                    "Invalid MM8 blackmail route waypoint");
                state.route.push_back(point);
            }
            require(state.route.size() >= 2, "MM8 blackmail route is empty");
            if (std::string_view(name) == "landing-to-cove.tsv")
            {
                state.route.push_back({-22000.0f, -11239.0f, 2.0f});
                state.route.push_back({-22200.0f, -11239.0f, 2.0f});
            }
            if (reverse) std::reverse(state.route.begin(), state.route.end());
            state.waypoint = 0;
        };
        const auto loadTownRoute = [&](bool toTown)
        {
            state.route = {{
                {10000.0f, -15616.0f, 1.0f}, {10784.0f, -15616.0f, 1.0f},
                {11000.0f, -15616.0f, 1.0f}, {12000.0f, -13100.0f, 1.0f},
                {12160.0f, -11968.0f, 1.0f}, {12480.0f, -11968.0f, 1.0f},
                {12480.0f, -12580.0f, 1.0f}, {12480.0f, -12700.0f, 1.0f},
                {12864.0f, -12700.0f, 1.0f}, {12864.0f, -12400.0f, 1.0f},
                {12864.0f, -12224.0f, 1.0f},
            }};
            if (!toTown) std::reverse(state.route.begin(), state.route.end());
            state.waypoint = 0;
        };
        const auto walkOutdoor = [&]()
        {
            require(session.currentMapFileName() == "out02.odm" && !pWorld->isIndoorMap(),
                "MM8 blackmail outdoor route changed maps unexpectedly");
            require(!runtime.activeEventDialog().isActive, "MM8 blackmail walking retained an event dialogue");
            while (state.waypoint < state.route.size()
                && std::hypot(state.route[state.waypoint][0] - pWorld->partyX(),
                    state.route[state.waypoint][1] - pWorld->partyY()) < 28.0f)
            {
                ++state.waypoint;
                state.stalledFrames = 0;
            }
            if (state.waypoint == state.route.size()) return true;
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = state.route[state.waypoint][0] - x;
            const float dy = state.route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            const float yaw = std::atan2(dy, dx);
            if (state.bypassSteps > 0)
            {
                const float side = state.blockedAttempts % 2 == 0 ? 1.0f : -1.0f;
                game.m_outdoorGameView.setCameraAngles(yaw + side * 1.57079632679f, 0.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.025f);
                --state.bypassSteps;
                return false;
            }
            game.m_outdoorGameView.setCameraAngles(yaw, 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(distance / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.3f ? state.stalledFrames + 1 : 0;
            if (frame % 240 == 0)
                std::cout << "Blackmail outdoor: phase=" << state.phase << " waypoint=" << state.waypoint
                    << '/' << state.route.size() << " position=" << pWorld->partyX() << ',' << pWorld->partyY()
                    << " remaining=" << distance << '\n';
            if (state.stalledFrames == 20)
            {
                if (state.lastBlockedWaypoint != state.waypoint)
                {
                    state.lastBlockedWaypoint = state.waypoint;
                    state.blockedAttempts = 0;
                }
                ++state.blockedAttempts;
                state.stalledFrames = 0;
                state.bypassSteps = 12;
            }
            require(state.blockedAttempts < 8,
                "MM8 blackmail outdoor route stopped at collision geometry or an actor");
            return false;
        };
        const auto walkIndoor = [&]()
        {
            require(session.currentMapFileName() == "d07.blv" && pWorld->isIndoorMap(),
                "MM8 blackmail indoor route left the smuggler cove");
            require(!runtime.activeEventDialog().isActive, "MM8 blackmail indoor walk retained a dialogue");
            if (frame < state.doorWaitUntil) return false;
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "MM8 blackmail lost the indoor runtime");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            if (state.blockingCombat)
            {
                if (handleMm8DungeonCombat(game, frame, width, height, state, captures,
                        "blackmail", true, true, 600.0f)) return false;
                state.blockingCombat = false;
            }
            while (state.waypoint < state.route.size()
                && std::hypot(state.route[state.waypoint][0] - world.partyX(),
                    state.route[state.waypoint][1] - world.partyY()) < 18.0f
                && std::abs(state.route[state.waypoint][2] - world.partyFootZ()) < 160.0f)
            {
                ++state.waypoint;
                state.stalledFrames = 0;
                state.blockedAttempts = 0;
            }
            if (state.waypoint == state.route.size()) return true;
            const float x = world.partyX(), y = world.partyY();
            const float dx = state.route[state.waypoint][0] - x;
            const float dy = state.route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            const float yaw = std::atan2(dy, dx);
            if (state.bypassSteps > 0)
            {
                game.m_indoorRenderer.setCameraAngles(yaw + 1.57079632679f, 0.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.025f);
                --state.bypassSteps;
                return false;
            }
            game.m_indoorRenderer.setCameraAngles(yaw, 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(distance / 1500.0f, 1.0f / 256.0f, 0.025f));
            const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.1f ? state.stalledFrames + 1 : 0;
            if (frame % 180 == 0)
                std::cout << "Blackmail indoor: phase=" << state.phase << " waypoint=" << state.waypoint
                    << '/' << state.route.size() << " position=" << world.partyX() << ',' << world.partyY()
                    << ',' << world.partyFootZ() << " remaining=" << distance << '\n';
            if (state.stalledFrames == 5)
            {
                if (state.lastBlockedWaypoint != state.waypoint)
                {
                    state.lastBlockedWaypoint = state.waypoint;
                    state.blockedAttempts = 0;
                }
                ++state.blockedAttempts;
                state.stalledFrames = 0;
                bool hostileBlocker = false;
                for (size_t index = 0; index < world.mapActorCount(); ++index)
                {
                    GameplayRuntimeActorState actor;
                    if (!world.actorRuntimeState(index, actor) || actor.isDead || actor.isInvisible
                        || !actor.hostileToParty || !world.partyAttackActorHasLineOfSight(index)) continue;
                    if (std::hypot(actor.preciseX - world.partyX(), actor.preciseY - world.partyY()) < 600.0f)
                    {
                        hostileBlocker = true;
                        break;
                    }
                }
                if (state.blockedAttempts > 2 && hostileBlocker)
                {
                    // The quest only requires combat that actually blocks the authored route. Do not
                    // turn every visible hostile in the full cave into an artificial clear-all objective.
                    state.blockingCombat = true;
                    return false;
                }
                if (state.blockedAttempts > 2 && state.blockedAttempts % 3 == 0)
                {
                    state.bypassSteps = 5;
                }
                else
                {
                    game.m_indoorRenderer.setCameraAngles(yaw, 0.0f);
                    pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                    state.doorWaitUntil = frame + 45;
                    ++state.indoorInteractions;
                    if (state.indoorInteractions == 1)
                    {
                        save("blackmail-first-door");
                        captures.push_back("blackmail-first-door");
                    }
                }
                require(state.blockedAttempts < 15 && state.indoorInteractions < 80,
                    "MM8 blackmail could not pass an authored cove obstruction");
            }
            return false;
        };
        const auto enterArionDialogue = [&]()
        {
            require(runtime.activeEventDialog().isActive,
                "Smuggler cove room did not open an original house dialogue");
            const EventDialogContent &dialog = runtime.activeEventDialog();
            if (dialog.isHouseDialog && dialog.sourceId == 772)
            {
                require(hasAction(EventDialogActionKind::HouseResident, 4),
                    "Arion Hunter is absent from his original room");
                selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 4);
                return false;
            }
            require(dialog.sourceId == 4, "Smuggler cove room opened the wrong resident dialogue");
            return true;
        };
        const auto totalExperience = [&]()
        {
            uint64_t result = 0;
            for (const Character &member : pParty->members()) result += member.experience;
            return result;
        };

        if (frame == 1)
        {
            require(session.currentMapFileName() == "out02.odm" && runtime.activeHistoryContinentId() == 1,
                "MM8 blackmail input is not the normal Ravenshore continuation");
            require(pParty->hasQuestBit(4) && pParty->inventoryItemCount(742) == 0,
                "MM8 blackmail input lost the normally delivered Dadeross letter state");
            for (size_t i = 0; i < 5; ++i)
                state.deliveryStartingExperience[i] = pParty->members()[i].experience;
            if (reload)
            {
                require(pParty->hasQuestBit(10) && pParty->hasQuestBit(24)
                    && !pParty->hasQuestBit(9) && !pParty->hasQuestBit(222) && !pParty->hasQuestBit(284),
                    "MM8 blackmail reload input lost its completed reward state");
                save("blackmail-reload-input");
                captures.push_back("blackmail-reload-input");
                next(100);
            }
            else
            {
                require(!pParty->hasQuestBit(9) && !pParty->hasQuestBit(10)
                    && !pParty->hasQuestBit(24) && !pParty->hasQuestBit(222) && !pParty->hasQuestBit(284),
                    "MM8 blackmail main input has already advanced the quest");
                const Party::Snapshot snapshot = pParty->snapshot();
                require(snapshot.npcTopicOverrides.contains(3)
                    && snapshot.npcTopicOverrides.at(3).contains(0)
                    && snapshot.npcTopicOverrides.at(3).at(0) == 29,
                    "Elgar Fellmoon does not expose the original blackmail topic");
                save("blackmail-input");
                captures.push_back("blackmail-input");
            }
        }
        if (frame < 20) return;

        switch (state.phase)
        {
        case 0:
            aimOutdoor(12864.0f, -12096.0f, 320.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(1);
            break;
        case 1:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 3
                && hasAction(EventDialogActionKind::NpcTopic, 29),
                "Elgar Fellmoon did not offer the original blackmail assignment");
            save("blackmail-offer");
            captures.push_back("blackmail-offer");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 29);
            next(2);
            break;
        case 2:
        {
            require(pParty->hasQuestBit(9) && pParty->hasQuestBit(222) && !pParty->hasQuestBit(10),
                "Elgar blackmail acceptance did not set its original quest bits");
            if (runtime.heldInventoryItem().active)
            {
                require(runtime.heldInventoryItem().item.objectDescriptionId == 742,
                    "Elgar offered an unexpected held item");
                const auto rect = runtime.resolvePartyPortraitRect(width, height, pParty->activeMemberIndex());
                require(rect.has_value(), "Blackmail letter has no normal recipient portrait");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = rect->x + rect->width * 0.5f;
                input.pointerY = rect->y + rect->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
                break;
            }
            require(pParty->inventoryItemCount(742) == 1,
                "Elgar blackmail acceptance did not place exactly one letter in the normal inventory");
            const Party::Snapshot snapshot = pParty->snapshot();
            require(snapshot.npcGreetingOverrides.contains(3)
                && snapshot.npcGreetingOverrides.at(3) == 11,
                "Elgar blackmail acceptance lost the original greeting override");
            size_t holder = size_t(-1), held = 0;
            for (size_t i = 0; i < pParty->members().size(); ++i)
            {
                const size_t count = std::count_if(pParty->members()[i].inventory.begin(),
                    pParty->members()[i].inventory.end(), [](const InventoryItem &item)
                    {
                        return item.objectDescriptionId == 742;
                    });
                if (count != 0) holder = i;
                held += count;
            }
            require(held == 1 && holder != size_t(-1),
                "Blackmail inventory does not have one identifiable normal holder");
            save("blackmail-accepted");
            captures.push_back("blackmail-accepted");
            std::cout << "Blackmail holder=" << holder << " total_xp=" << totalExperience() << '\n';
            next(3);
            break;
        }
        case 3:
            if (runtime.activeEventDialog().isActive) close();
            else { loadTownRoute(false); next(4); }
            break;
        case 4:
            if (!walkOutdoor()) break;
            loadRoute("landing-to-cove.tsv", false);
            next(5);
            break;
        case 5:
            if (!walkOutdoor()) break;
            save("blackmail-cove-entrance");
            captures.push_back("blackmail-cove-entrance");
            next(6);
            break;
        case 6:
            aimOutdoor(-22527.0f, -11239.0f, 256.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(7);
            break;
        case 7:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Smuggler cove entrance did not open its original transition");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            next(8);
            break;
        case 8:
            if (frame < state.phaseFrame + 24) break;
            require(session.currentMapFileName() == "d07.blv" && pWorld->isIndoorMap(),
                "Normal blackmail route did not enter the smuggler cove");
            state.experienceBeforeCombat = totalExperience();
            loadRoute("d07-to-arion-open.tsv", false);
            save("blackmail-cove-entered");
            captures.push_back("blackmail-cove-entered");
            next(9);
            break;
        case 9:
            if (!walkIndoor()) break;
            next(10);
            break;
        case 10:
            aimIndoor(2840.0f, -16472.0f, 256.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(11);
            break;
        case 11:
            if (!enterArionDialogue()) break;
            require(hasAction(EventDialogActionKind::NpcTopic, 32),
                "Arion Hunter hid the blackmail delivery despite the carried letter");
            save("blackmail-delivery-offer");
            captures.push_back("blackmail-delivery-offer");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 32);
            next(12);
            break;
        case 12:
        {
            require(pParty->inventoryItemCount(742) == 0 && pParty->hasQuestBit(10)
                && pParty->hasQuestBit(284) && !pParty->hasQuestBit(9) && !pParty->hasQuestBit(222),
                "Arion delivery did not consume the blackmail exactly once and update its quest bits");
            const Party::Snapshot snapshot = pParty->snapshot();
            require(snapshot.npcTopicOverrides.contains(4)
                && snapshot.npcTopicOverrides.at(4).contains(0)
                && snapshot.npcTopicOverrides.at(4).at(0) == 33,
                "Arion delivery did not replace blackmail with the smuggler boats topic");
            const EventRuntimeState *events = pWorld->eventRuntimeState();
            require(events != nullptr && historyEventTimesForActiveContinent(*events).contains(5),
                "Arion delivery did not write original MM8 History5");
            state.deliveryHistoryTime = historyEventTimesForActiveContinent(*events).at(5);
            save("blackmail-delivered");
            captures.push_back("blackmail-delivered");
            next(13);
            break;
        }
        case 13:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                aimIndoor(2840.0f, -16472.0f, 256.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                next(14);
            }
            break;
        case 14:
            if (!enterArionDialogue()) break;
            require(hasAction(EventDialogActionKind::NpcTopic, 33)
                && !hasAction(EventDialogActionKind::NpcTopic, 32),
                "Arion delivery dialogue did not persist its one-time topic transition");
            save("blackmail-arion-reopened");
            captures.push_back("blackmail-arion-reopened");
            next(15);
            break;
        case 15:
            if (runtime.activeEventDialog().isActive) close();
            else { loadRoute("d07-to-arion-open.tsv", true); next(16); }
            break;
        case 16:
            if (!walkIndoor()) break;
            next(17);
            break;
        case 17:
            aimIndoor(-3968.0f, 627.2f, 486.4f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(18);
            break;
        case 18:
            if (session.currentMapFileName() == "d07.blv")
            {
                require(runtime.activeEventDialog().isActive
                    && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                    "Smuggler cove exit did not open its original transition");
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
                state.phaseFrame = frame;
                break;
            }
            if (frame < state.phaseFrame + 12) break;
            require(session.currentMapFileName() == "out02.odm" && !pWorld->isIndoorMap(),
                "Smuggler cove exit did not return to Ravenshore");
            loadRoute("landing-to-cove.tsv", true);
            save("blackmail-returned-outdoors");
            captures.push_back("blackmail-returned-outdoors");
            next(19);
            break;
        case 19:
            if (!walkOutdoor()) break;
            loadTownRoute(true);
            next(20);
            break;
        case 20:
            if (!walkOutdoor()) break;
            next(21);
            break;
        case 21:
            aimOutdoor(12864.0f, -12096.0f, 320.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(22);
            break;
        case 22:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 3
                && hasAction(EventDialogActionKind::NpcTopic, 29),
                "Elgar Fellmoon did not expose the blackmail return topic");
            for (size_t i = 0; i < 5; ++i)
                state.rewardStartingExperience[i] = pParty->members()[i].experience;
            save("blackmail-before-reward");
            captures.push_back("blackmail-return-offer");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 29);
            next(23);
            break;
        case 23:
        {
            require(pParty->hasQuestBit(24) && !pParty->hasQuestBit(284)
                && pParty->inventoryItemCount(742) == 0,
                "Elgar blackmail reward did not advance the Alvar clue state");
            for (size_t i = 0; i < 5; ++i)
                require(pParty->members()[i].experience == state.rewardStartingExperience[i] + 12000,
                    "Elgar blackmail reward was not exactly 12000 experience for every member");
            const EventRuntimeState *events = pWorld->eventRuntimeState();
            constexpr uint32_t BlackmailAutonoteRawId = (493u << 16) | 0x00e1u;
            require(events && events->variables.contains(BlackmailAutonoteRawId)
                && events->variables.at(BlackmailAutonoteRawId) != 0,
                "Elgar blackmail reward did not set original autonote493");
            require(historyEventTimesForActiveContinent(*events).contains(5)
                && historyEventTimesForActiveContinent(*events).at(5) == state.deliveryHistoryTime,
                "Returning to Elgar changed the original delivery history time");
            save("blackmail-reward");
            captures.push_back("blackmail-reward");
            next(24);
            break;
        }
        case 24:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                save("mm8-blackmail-main-complete");
                captures.push_back("blackmail-main-complete");
                state.complete = true;
                std::cout << "MM8 blackmail main passed: walked=" << state.walkedDistance
                    << " defeated=" << state.defeatedActors.size()
                    << " door_inputs=" << state.indoorInteractions << '\n';
            }
            break;
        case 100:
            aimOutdoor(12864.0f, -12096.0f, 320.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(101);
            break;
        case 101:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 3
                && hasAction(EventDialogActionKind::NpcTopic, 29),
                "Completed blackmail reload lost Elgar's next Alvar clue topic");
            require(pParty->inventoryItemCount(742) == 0,
                "Completed blackmail reload recreated Fellmoon's letter");
            save("blackmail-reload-elgar");
            captures.push_back("blackmail-reload-elgar");
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 29);
            next(102);
            break;
        case 102:
            if (!state.reloadElgarVerified)
            {
                require(pParty->hasQuestBit(11) && pParty->hasQuestBit(24)
                    && pParty->inventoryItemCount(742) == 0,
                    "Elgar reload visit did not open the next Alvar report clue");
                for (size_t i = 0; i < 5; ++i)
                    require(pParty->members()[i].experience == state.deliveryStartingExperience[i],
                        "Elgar reload visit repeated the blackmail experience reward");
                state.reloadElgarVerified = true;
            }
            if (runtime.activeEventDialog().isActive) close();
            else { loadTownRoute(false); next(103); }
            break;
        case 103:
            if (!walkOutdoor()) break;
            loadRoute("landing-to-cove.tsv", false);
            next(104);
            break;
        case 104:
            if (!walkOutdoor()) break;
            captures.push_back("blackmail-reload-cove");
            next(105);
            break;
        case 105:
            aimOutdoor(-22527.0f, -11239.0f, 256.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(106);
            break;
        case 106:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Blackmail reload did not reopen the smuggler cove transition");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            next(107);
            break;
        case 107:
            if (frame < state.phaseFrame + 24) break;
            require(session.currentMapFileName() == "d07.blv" && pWorld->isIndoorMap(),
                "Blackmail reload did not enter the smuggler cove");
            state.experienceBeforeCombat = totalExperience();
            loadRoute("d07-to-arion-open.tsv", false);
            next(108);
            break;
        case 108:
            if (!walkIndoor()) break;
            next(109);
            break;
        case 109:
            aimIndoor(2840.0f, -16472.0f, 256.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(110);
            break;
        case 110:
            if (!enterArionDialogue()) break;
            require(hasAction(EventDialogActionKind::NpcTopic, 33)
                && !hasAction(EventDialogActionKind::NpcTopic, 32)
                && pParty->inventoryItemCount(742) == 0,
                "Arion reload visit repeated or reverted the blackmail delivery");
            for (size_t i = 0; i < 5; ++i)
                require(pParty->members()[i].experience == state.deliveryStartingExperience[i],
                    "Arion reload visit changed the completed party experience");
            save("blackmail-reload-arion");
            captures.push_back("blackmail-reload-arion");
            next(111);
            break;
        case 111:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                save("mm8-blackmail-complete");
                captures.push_back("blackmail-reload-complete");
                state.complete = true;
                std::cout << "MM8 blackmail reload passed: walked=" << state.walkedDistance
                    << " q11=" << pParty->hasQuestBit(11) << '\n';
            }
            break;
        default: throw std::runtime_error("Invalid MM8 blackmail phase");
        }
    }

    static void checkMm8TempleExit(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pWorld && pParty && pParty->members().size() == 5 && pParty->hasActableMember()
            && pParty->inventoryItemCount(741) == 1 && pParty->hasQuestBit(3) && pParty->hasQuestBit(221),
            "Temple exit lost the original surviving party or undelivered letter");
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
        };
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM8 废庙另一出口", {}), "Cannot preserve the normal temple exit");
        };
        if (frame == 1)
        {
            for (const Character &member : pParty->members()) state.experienceBeforeCombat += member.experience;
            require(!runtime.activeEventDialog().isActive, "Exit input has an unfinished dialogue");
            state.reloadOnly = session.currentMapFileName() == "out01.odm";
            if (state.reloadOnly) next(4);
            else
            {
                auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
                require(pScene && pScene->currentMapFileName() == "d05.blv"
                    && pScene->eventRuntimeState()->mapVars[12] == 15
                    && pScene->eventRuntimeState()->mapVars[31] == 4,
                    "Exit route requires both normally completed earlier puzzles");
            }
        }
        if (frame < 24) return;
        if (state.phase == 4)
        {
            require(session.currentMapFileName() == "out01.odm"
                && std::hypot(pWorld->partyX() - 21519.0f, pWorld->partyY() - 21106.0f) < 100.0f,
                "Original exit 502 did not reach the boat island landing");
            if (frame < state.phaseFrame + 30) return;
            save("temple-exit-complete");
            captures.push_back("temple-exit-complete");
            state.complete = true;
            std::cout << "Normal MM8 temple exit passed: walked=" << state.walkedDistance
                << " new_kills=" << state.defeatedActors.size() << " gold=" << pParty->gold()
                << " food=" << pParty->food() << '\n';
            return;
        }
        if (state.phase == 3)
        {
            if (session.currentMapFileName() == "out01.odm") { next(4); return; }
            const EventDialogContent &dialogue = runtime.activeEventDialog();
            require(dialogue.isActive && dialogue.presentation == EventDialogPresentation::Transition,
                "Original exit door did not offer a map transition");
            if (!state.transitionCaptured)
            {
                captures.push_back("temple-exit-confirm");
                state.transitionCaptured = true;
                state.transitionFrame = frame;
                return;
            }
            if (frame < state.transitionFrame + 12) return;
            const auto action = std::find_if(dialogue.actions.begin(), dialogue.actions.end(),
                [](const EventDialogAction &candidate)
                {
                    return candidate.kind == EventDialogActionKind::MapTransitionConfirm;
                });
            require(action != dialogue.actions.end(), "Original exit has no confirmation action");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, action->id);
            return;
        }
        require(session.currentMapFileName() == "d05.blv", "Exit route left the temple before its original door");
        if (handleMm8DungeonCombat(game, frame, width, height, state, captures, "temple-exit")) return;
        const auto walk = [&](std::span<const std::array<float, 2>> route)
        {
            require(state.waypoint < route.size(), "Temple exit has no remaining route point");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) < 20.0f)
            {
                std::cout << "Exit waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "Exit walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " moved=" << moved << '\n';
            require(state.stalledFrames < 60, "Temple exit walk stopped at actual geometry");
            return false;
        };
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 17> hall = {{{8032, 2944}, {8992, 2944}, {9520, 2432},
                {9870, 2304}, {10752, 2304}, {10816, 2304}, {10944, 2048}, {11392, 2048}, {11584, 2048},
                {11968, 2048}, {12160, 2048}, {12608, 2304}, {12864, 2304}, {13440, 2304}, {13760, 2304},
                {13888, 2432}, {14080, 2432}}};
            if (!walk(hall)) return;
            save("temple-exit-hall");
            captures.push_back("temple-exit-hall");
            next(1);
            return;
        }
        if (state.phase == 1)
        {
            constexpr std::array<std::array<float, 2>, 9> stairs = {{{14080, 2208}, {14080, 1056}, {14080, 896},
                {13696, 896}, {13696, 1100}, {13696, 2210}, {13696, 2432}, {13080, 2432}, {12810, 2432}}};
            if (!walk(stairs)) return;
            require(std::abs(pWorld->partyFootZ() - 384.0f) < 1.0f,
                "Normal exit stair climb did not regain the upper floor");
            save("temple-exit-upper");
            captures.push_back("temple-exit-upper");
            next(2);
            return;
        }
        require(state.phase == 2, "Unknown temple exit phase");
        if (frame < state.phaseFrame + 12) return;
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(pScene, "Original exit lost its indoor camera");
        const float dx = 12672.0f - pWorld->partyX(), dy = 2432.0f - pWorld->partyY();
        game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx),
            std::atan2(512.0f - pScene->partyRuntime().movementState().eyeZ(), std::hypot(dx, dy)));
        press(KeyboardAction::Trigger);
        next(3);
    }

    static void checkMm8Dungeon(GameApplication &game, uint32_t frame, int width, int height,
        Mm8PortalCheckState &state, std::vector<std::string> &captures, bool chamber = false,
        bool normalReplay = false)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(pScene && pScene->currentMapFileName() == "d05.blv", "Dungeon route needs original d05");
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party &party = pScene->party();
        IndoorWorldRuntime &world = pScene->worldRuntime();
        const IndoorMoveState &position = pScene->partyRuntime().movementState();
        require(party.members().size() == 5 && party.members().back().rosterId == 3 && party.hasActableMember(),
            "Dungeon route lost the normal five-member party");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = width * 0.5f;
            input.pointerY = height * 0.5f;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "MM8 废庙正常探索", {}), "Cannot preserve normal dungeon checkpoint");
        };
        const auto walkTo = [&](float x, float y)
        {
            const float dx = x - position.x, dy = y - position.y;
            if (std::hypot(dx, dy) < 24.0f) { state.stalledFrames = 0; return true; }
            const float previousX = position.x, previousY = position.y;
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, 0.025f);
            const float moved = std::hypot(position.x - previousX, position.y - previousY);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0)
                std::cout << "Dungeon flank: phase=" << state.phase << " waypoint=" << state.entryFrame
                    << " position=" << position.x << ',' << position.y << ',' << position.footZ
                    << " walked=" << state.walkedDistance << '\n';
            require(state.stalledFrames < 60, "Dungeon flanking route stopped at actual geometry");
            return false;
        };
        const auto experience = [&]()
        {
            uint64_t result = 0;
            for (const Character &member : party.members()) result += member.experience;
            return result;
        };
        if (frame == 1)
        {
            if (normalReplay)
            {
                for (size_t i = 0; i < world.mapActorCount(); ++i)
                {
                    GameplayRuntimeActorState actor;
                    if (!world.actorRuntimeState(i, actor) || actor.monsterId < 7 || actor.monsterId > 9) continue;
                    const bool inRegion = chamber
                        ? std::abs(actor.preciseZ) < 100 && std::abs(actor.preciseX) < 1200
                            && actor.preciseY > -300 && actor.preciseY < 1500
                        : actor.preciseZ > 850 && actor.preciseZ < 1100;
                    if (inRegion) state.scenarioActors.push_back(i);
                }
                require(!state.scenarioActors.empty(), "No original winged serpents in the current dungeon region");
                std::cout << "Replay dungeon region: chamber=" << chamber << " actors=";
                for (size_t i : state.scenarioActors) std::cout << i << ',';
                std::cout << '\n';
            }
            else
            {
                if (chamber)
                    for (size_t i = 0; i < 11; ++i) state.scenarioActors.push_back(i);
                else state.scenarioActors = {46, 53};
            }
            require(party.hasQuestBit(1) && party.hasQuestBit(2) && party.hasQuestBit(8)
                && !runtime.activeEventDialog().isActive, "Dungeon input has an unfinished entrance dialogue");
            state.experienceBeforeCombat = experience();
            state.reloadOnly = !chamber && position.footZ < 1100.0f;
            if (chamber)
            {
                require(party.hasQuestBit(3) && party.hasQuestBit(221) && party.inventoryItemCount(741) == 1,
                    "The next temple chamber must retain the undelivered original letter");
                if (position.footZ < 100.0f)
                {
                    state.phase = 30;
                    state.combatCaptured = true;
                    state.killCaptured = true;
                }
            }
            if (state.reloadOnly)
            {
                const auto snapshot = world.snapshot();
                const bool searched = !normalReplay && snapshot.mapActorCorpseViews.size() > 55
                    && isConsumedCorpseView(snapshot.mapActorCorpseViews[51])
                    && isConsumedCorpseView(snapshot.mapActorCorpseViews[55]);
                const bool supplies = party.inventoryItemCount(222) + party.inventoryItemCount(223)
                    + party.inventoryItemCount(226) > 0;
                state.phase = searched ? 24 : supplies ? 10 : 15;
            }
            save(chamber ? "chamber-input" : "dungeon-input");
        }
        if (frame < 24) return;
        if (state.reloadOnly && state.phase != 16)
        {
            const auto click = [&](float x, float y)
            {
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = x;
                input.pointerY = y;
                input.rightMouseButton.held = true;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
                session.updateGameplay({}, 0.0f);
            };
            const auto portrait = [&](size_t member)
            {
                const auto rect = runtime.resolvePartyPortraitRect(width, height, member);
                require(rect.has_value(), "Normal potion recipient portrait is missing");
                click(rect->x + rect->width * 0.5f, rect->y + rect->height * 0.5f);
            };
            const auto close = [&]()
            {
                GameplayInputFrame input;
                input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                session.updateGameplay(input, 0.0f);
                session.updateGameplay({}, 0.0f);
            };
            const auto next = [&](unsigned phase)
            {
                state.phase = phase;
                state.phaseFrame = frame;
            };
            constexpr std::array<std::array<uint32_t, 3>, 5> supplies = {{{2, 222, 0}, {2, 223, 1},
                {1, 226, 4}, {3, 222, 2}, {3, 223, 1}}};
            if (frame < state.phaseFrame + 12) return;
            if (state.phase == 10)
            {
                const auto supply = supplies[state.waypoint];
                const auto poisoned = [](const Character &member)
                {
                    return member.conditions.test(size_t(CharacterCondition::PoisonWeak))
                        || member.conditions.test(size_t(CharacterCondition::PoisonMedium))
                        || member.conditions.test(size_t(CharacterCondition::PoisonSevere));
                };
                state.supplyTarget = supply[2];
                if (normalReplay && supply[1] == 222 && party.members()[1].health <= 0)
                    state.supplyTarget = 1;
                if (normalReplay && supply[1] == 226)
                {
                    if (poisoned(party.members()[1])) state.supplyTarget = 1;
                    else
                        for (size_t i = 0; i < party.members().size(); ++i)
                            if (poisoned(party.members()[i])) { state.supplyTarget = i; break; }
                }
                const Character &recipient = party.members()[state.supplyTarget];
                const bool alreadyRestored = supply[1] == 222 ? recipient.health >= recipient.maxHealth
                    : supply[1] == 223 ? recipient.spellPoints >= recipient.maxSpellPoints
                    : !poisoned(recipient);
                if (normalReplay && alreadyRestored)
                {
                    std::cout << "Replay conserves unneeded potion: item=" << supply[1]
                        << " target=" << state.supplyTarget << '\n';
                    ++state.waypoint;
                    next(state.waypoint == supplies.size() ? 15 : 10);
                    return;
                }
                const size_t owner = supply[0];
                const bool unconsciousOwner = normalReplay && party.members()[owner].health <= 0;
                const size_t beforeActive = party.activeMemberIndex();
                portrait(owner);
                if (unconsciousOwner)
                    require(!runtime.characterScreen().open && party.activeMemberIndex() == beforeActive,
                        "Single unconscious portrait click incorrectly selected or opened the member");
                portrait(owner);
                require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == owner,
                    "Normal portrait input did not open the potion owner's inventory");
                if (unconsciousOwner)
                {
                    require(party.activeMemberIndex() == beforeActive,
                        "Inspecting an unconscious member changed the active actor");
                    const std::string token = "unconscious-inventory-open-" + std::to_string(state.waypoint);
                    save(token.c_str());
                    captures.push_back(token);
                    std::cout << "Unconscious inventory passed: owner=" << owner
                        << " hp=" << party.members()[owner].health << " active=" << beforeActive << '\n';
                }
                next(11);
                return;
            }
            if (state.phase == 11)
            {
                if (state.waypoint == 0) captures.push_back("dungeon-supplies");
                const auto supply = supplies[state.waypoint];
                const auto *pGridLayout = runtime.findHudLayoutElement("CharacterInventoryGrid");
                require(pGridLayout != nullptr, "Normal potion inventory grid is unavailable");
                const auto grid = runtime.resolveHudLayoutElement(pGridLayout->id, width, height,
                    pGridLayout->width, pGridLayout->height);
                require(grid.has_value(), "Normal potion inventory grid cannot be placed");
                const Character &owner = party.members()[supply[0]];
                const auto item = std::find_if(owner.inventory.begin(), owner.inventory.end(),
                    [&](const InventoryItem &candidate) { return candidate.objectDescriptionId == supply[1]; });
                require(item != owner.inventory.end(), "Original potion is absent from its owner's backpack");
                const float x = grid->x + (item->gridX + item->width * 0.5f)
                    * grid->width / Character::InventoryWidth;
                const float y = grid->y + (item->gridY + item->height * 0.5f)
                    * grid->height / Character::InventoryHeight;
                click(x, y);
                require(runtime.heldInventoryItem().active
                    && runtime.heldInventoryItem().item.objectDescriptionId == supply[1],
                    "Normal inventory-cell click did not pick the original potion");
                next(12);
                return;
            }
            if (state.phase == 12)
            {
                auto supply = supplies[state.waypoint];
                supply[2] = uint32_t(state.supplyTarget);
                const int count = party.inventoryItemCount(supply[1]);
                const Character before = party.members()[supply[2]];
                portrait(supply[2]);
                const Character &after = party.members()[supply[2]];
                require(!runtime.heldInventoryItem().active && party.inventoryItemCount(supply[1]) == count,
                    "Normal portrait potion use did not consume its held original item");
                require(supply[1] == 222 ? after.health > before.health
                    : supply[1] == 223 ? after.spellPoints > before.spellPoints : after.conditions != before.conditions,
                    "Original potion had no corresponding health, mana or condition effect");
                std::cout << "Dungeon potion: item=" << supply[1] << " owner=" << supply[0]
                    << " target=" << supply[2] << " hp=" << before.health << "->" << after.health
                    << " mana=" << before.spellPoints << "->" << after.spellPoints
                    << " conditions=" << before.conditions.to_ullong() << "->" << after.conditions.to_ullong() << '\n';
                close();
                require(!runtime.characterScreen().open, "Normal Escape did not close the potion inventory");
                const std::string token = "dungeon-potion-" + std::to_string(state.waypoint);
                save(token.c_str());
                ++state.waypoint;
                next(state.waypoint == supplies.size() ? 15 : 10);
                return;
            }
            if (state.phase == 15)
            {
                for (size_t index : state.scenarioActors)
                {
                    GameplayRuntimeActorState actor;
                    require(world.actorRuntimeState(index, actor), "Remaining bat is absent from the original map");
                    if (!actor.isDead) { next(16); return; }
                }
                size_t weakest = size_t(-1);
                float fraction = 0.6f;
                for (size_t i = 0; i < party.members().size(); ++i)
                {
                    const Character &member = party.members()[i];
                    const float health = float(member.health) / float(std::max(1, member.maxHealth));
                    if (health < fraction) { weakest = i; fraction = health; }
                }
                if (weakest != size_t(-1))
                {
                    if (party.members()[1].recoverySecondsRemaining > 0.0f) return;
                    if (party.activeMemberIndex() != 1) portrait(1);
                    const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(68);
                    require(pSpell && runtime.tryCastSpellFromMember(1, 68, pSpell->name),
                        "Original recovered healer cannot start Heal");
                    GameplaySpellService &spells = runtime.spellService();
                    PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                    request.targetCharacterIndex = weakest;
                    require(spells.validatePendingTargetSelectionRequest(runtime, request), "Healing target rejected");
                    const int hp = party.members()[weakest].health, mana = party.members()[1].spellPoints;
                    const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                    require(result.castResult.succeeded() && party.members()[weakest].health > hp
                        && party.members()[1].spellPoints == mana - 2, "Normal Heal has wrong health or mana effect");
                    spells.clearPendingTargetSelection(runtime);
                    session.updateGameplay({}, 0.0f);
                    std::cout << "Dungeon recovery Heal: target=" << weakest << " hp=" << hp << "->"
                        << party.members()[weakest].health << " mana=" << mana << "->"
                        << party.members()[1].spellPoints << '\n';
                    state.phaseFrame = frame;
                    return;
                }
                save("dungeon-recovered");
                captures.push_back("dungeon-recovered");
                state.waypoint = 0;
                state.entryFrame = 0;
                next(20);
                return;
            }
            const std::array<size_t, 2> &corpses = state.lootActors;
            if (state.phase == 20)
            {
                constexpr std::array<std::array<float, 2>, 2> returnRoute = {{{-3840, -1984}, {-3300, -1880}}};
                if (state.entryFrame < returnRoute.size())
                {
                    if (walkTo(returnRoute[state.entryFrame][0], returnRoute[state.entryFrame][1])) ++state.entryFrame;
                    return;
                }
                if (normalReplay && !state.selectedLootActors)
                {
                    std::vector<std::pair<float, size_t>> candidates;
                    const auto snapshot = world.snapshot();
                    for (size_t i : state.scenarioActors)
                    {
                        GameplayRuntimeActorState actor;
                        if (!world.actorRuntimeState(i, actor) || !actor.isDead) continue;
                        if (i < snapshot.mapActorCorpseViews.size()
                            && isConsumedCorpseView(snapshot.mapActorCorpseViews[i])) continue;
                        candidates.emplace_back(std::hypot(actor.preciseX - position.x,
                            actor.preciseY - position.y), i);
                    }
                    std::sort(candidates.begin(), candidates.end());
                    require(candidates.size() >= 2, "Normal lower encounter has fewer than two unsearched corpses");
                    state.lootActors = {candidates[0].second, candidates[1].second};
                    state.selectedLootActors = true;
                    std::cout << "Replay corpse selection: " << state.lootActors[0] << ',' << state.lootActors[1]
                        << " distance=" << candidates[0].first << ',' << candidates[1].first << '\n';
                }
                GameplayRuntimeActorState corpse;
                require(world.actorRuntimeState(corpses[state.waypoint], corpse) && corpse.isDead,
                    "Normal search requires the actual defeated bat");
                const float distance = std::hypot(corpse.preciseX - position.x, corpse.preciseY - position.y);
                if (normalReplay && distance >= 350.0f)
                {
                    walkTo(corpse.preciseX, corpse.preciseY);
                    return;
                }
                require(distance < 400.0f, "Actual corpse is beyond the nearby normal search range");
                game.m_indoorRenderer.setCameraAngles(std::atan2(corpse.preciseY - position.y,
                    corpse.preciseX - position.x),
                    std::atan2(corpse.preciseZ + corpse.height * 0.25f - position.eyeZ(), distance));
                next(21);
                return;
            }
            if (state.phase == 21)
            {
                press(KeyboardAction::Trigger);
                next(22);
                return;
            }
            if (state.phase == 22)
            {
                const auto snapshot = world.snapshot();
                const size_t index = corpses[state.waypoint];
                require(index < snapshot.mapActorCorpseViews.size()
                    && isConsumedCorpseView(snapshot.mapActorCorpseViews[index]),
                    "Normal Trigger did not search and consume the expected bat corpse");
                require(!runtime.heldInventoryItem().active, "Normal bat search still has a held item");
                std::cout << "Dungeon corpse searched: actor=" << index << " gold=" << party.gold()
                    << " result=" << pScene->eventRuntimeState()->lastActivationResult.value_or("none") << '\n';
                if (++state.waypoint < corpses.size()) { next(20); return; }
                next(24);
                return;
            }
            if (state.phase == 24)
            {
                portrait(2);
                portrait(2);
                require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == 2,
                    "Normal portrait input did not open the loot recipient inventory");
                next(25);
                return;
            }
            if (state.phase == 25)
            {
                captures.push_back("dungeon-loot-inventory");
                next(26);
                return;
            }
            if (state.phase == 26)
            {
                close();
                require(!runtime.characterScreen().open, "Normal Escape did not close the loot inventory");
                const auto snapshot = world.snapshot();
                for (size_t i : corpses)
                    require(i < snapshot.mapActorCorpseViews.size()
                        && isConsumedCorpseView(snapshot.mapActorCorpseViews[i]), "Searched corpse state was lost");
                save("dungeon-loot-complete");
                captures.push_back("dungeon-loot-complete");
                state.complete = true;
                std::cout << "Dungeon recovery, corpse searches and inventory continuation passed\n";
                return;
            }
            throw std::runtime_error("Unknown normal dungeon recovery phase");
        }
        if (handleMm8DungeonCombat(game, frame, width, height, state, captures,
            chamber ? "chamber" : state.reloadOnly ? "recovery" : "dungeon")) return;
        if (state.reloadOnly && state.phase == 16)
        {
            for (size_t index : state.scenarioActors)
            {
                GameplayRuntimeActorState actor;
                require(world.actorRuntimeState(index, actor), "Remaining bat disappeared during combat");
                if (!actor.isDead)
                {
                    constexpr std::array<std::array<float, 2>, 3> oldFlank = {{{-3840, -1984},
                        {-4352, -1984}, {-4352, -1696}}};
                    constexpr std::array<std::array<float, 2>, 15> patrol = {{{-3840, -1984}, {-4352, -1984},
                        {-4352, -3300}, {-3200, -3300}, {-2000, -3300}, {-1200, -3300}, {-1200, -1984},
                        {-1200, -640}, {-1200, 384}, {-2304, 384}, {-3840, 384}, {-4700, 384},
                        {-4700, -1984}, {-3840, -1984}, {-3300, -1880}}};
                    const std::span<const std::array<float, 2>> flank = normalReplay
                        ? std::span<const std::array<float, 2>>(patrol)
                        : std::span<const std::array<float, 2>>(oldFlank);
                    require(state.entryFrame < flank.size(), "Flanking the original stairwall did not expose the bat");
                    if (walkTo(flank[state.entryFrame][0], flank[state.entryFrame][1])) ++state.entryFrame;
                    require(state.walkedDistance < (normalReplay ? 20000.0f : 2500.0f),
                        "Winged-serpent approach exceeded the lower-floor route");
                    return;
                }
            }
            state.phase = 15;
            state.phaseFrame = frame;
            return;
        }
        constexpr std::array<std::array<float, 2>, 11> landingRoute = {{{-3008, -1536}, {-2752, -1536},
            {-2752, -1088}, {-3264, -1088}, {-3264, -1536}, {-2800, -1536}, {-2800, -224},
            {-4080, -224}, {-4080, -1880}, {-3700, -1880}, {-3008, -1880}}};
        constexpr std::array<std::array<float, 2>, 21> chamberRoute = {{{-2800, -1880}, {-2800, -1344},
            {-2560, -1344}, {-1850, -1344}, {-1650, -1344}, {-1472, -1560}, {-1100, -1728},
            {-850, -1728}, {-580, -1610}, {-440, -1470}, {-400, -1320}, {-230, -1200}, {-75, -1100},
            {32, -950}, {64, -760}, {64, -480}, {-360, -320}, {-400, 64}, {-400, 640},
            {-400, 1120}, {0, 1160}}};
        const std::span<const std::array<float, 2>> route = chamber
            ? std::span<const std::array<float, 2>>(chamberRoute)
            : std::span<const std::array<float, 2>>(landingRoute);
        if (chamber && (state.phase == 30 || state.waypoint == route.size()))
        {
            for (size_t index : state.scenarioActors)
            {
                GameplayRuntimeActorState actor;
                require(world.actorRuntimeState(index, actor) && actor.isDead,
                    "The original central chamber still has a surviving bat");
            }
            require(position.footZ < 100.0f && party.inventoryItemCount(741) == 1,
                "Normal chamber route did not reach its original lower floor with the letter");
            save("chamber-complete");
            captures.push_back("chamber-complete");
            state.complete = true;
            std::cout << "Normal MM8 central chamber passed: walked=" << state.walkedDistance
                << " new_kills=" << state.defeatedActors.size() << " xp=" << experience() << '\n';
            return;
        }
        if (state.waypoint == route.size())
        {
            require(position.footZ < 1100.0f && state.defeatedActors.size() >= 5,
                "Normal dungeon route has not descended and defeated its lower encounter");
            save("dungeon-landing-complete");
            captures.push_back("dungeon-landing-complete");
            state.complete = true;
            std::cout << "Dungeon landing complete: walked=" << state.walkedDistance
                << " defeated=" << state.defeatedActors.size() << " xp=" << experience() << '\n';
            return;
        }
        const float dx = route[state.waypoint][0] - position.x;
        const float dy = route[state.waypoint][1] - position.y;
        const float distance = std::hypot(dx, dy);
        if (distance < 24.0f)
        {
            if (chamber && state.waypoint == 16)
            {
                const EventRuntimeState *pEvents = world.eventRuntimeState();
                require(pEvents && pEvents->mechanisms.contains(2), "Central chamber gate state is absent");
                if (state.entryFrame == 0)
                {
                    state.entryFrame = frame;
                    std::cout << "Waiting for the original side-room gate to finish opening\n";
                }
                if (pEvents->mechanisms.at(2).state != 0)
                {
                    require(frame - state.entryFrame < 1200, "Original side-room gate did not finish opening");
                    return;
                }
                save("chamber-gate-open");
                captures.push_back("chamber-gate-open");
            }
            std::cout << "Dungeon waypoint: index=" << state.waypoint << " position=" << position.x << ','
                << position.y << ',' << position.footZ << " walked=" << state.walkedDistance << '\n';
            if (!chamber && state.waypoint == 4)
            {
                save("dungeon-upper-stairs");
                captures.push_back("dungeon-upper-stairs");
            }
            ++state.waypoint;
            state.stalledFrames = 0;
            return;
        }
        const float x = position.x, y = position.y;
        game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
        press(KeyboardAction::Forward, distance < 80.0f ? 0.025f : 0.05f);
        const float moved = std::hypot(position.x - x, position.y - y);
        state.walkedDistance += moved;
        state.stalledFrames = moved < 0.2f ? state.stalledFrames + 1 : 0;
        if (frame % 120 == 0 || state.stalledFrames == 30)
            std::cout << "Dungeon walk: waypoint=" << state.waypoint << " position=" << position.x << ','
                << position.y << ',' << position.footZ << " remaining=" << distance << " moved=" << moved << '\n';
        require(state.stalledFrames < 60, "Normal dungeon route stopped at actual collision");
    }

    struct EquipmentCheckState
    {
        std::array<GameplayRenderedInspectableHudItem, 2> originalItems;
    };

    static void checkPaperdolls(GameApplication &game, uint32_t frame, int width, int height,
        bool &complete, EquipmentCheckState &equipment, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        require(pParty && pParty->members().size() == 4,
            "Paperdoll capture needs the normal four-member party");
        if (frame == 1) require(!runtime.heldInventoryItem().active, "Paperdoll run starts with a held item");
        // Let the actual world/HUD establish its viewport before generating pointer coordinates.
        if (frame <= 20) return;
        const uint32_t inspectionFrame = frame - 20;
        if (frame == 21)
        {
            const auto panel = runtime.resolveHudLayoutElement("CharacterDollPanel", width, height, 175, 378);
            require(panel.has_value(), "Paperdoll panel is absent");
            std::cout << "Paperdoll panel: x=" << panel->x << " y=" << panel->y
                << " width=" << panel->width << " height=" << panel->height << '\n';
        }
        if (inspectionFrame <= 180)
        {
            const size_t memberIndex = (inspectionFrame - 1) / 45;
            if ((inspectionFrame - 1) % 45 == 0)
            {
                const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
                require(portrait.has_value(), "The active companion has no visible portrait");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = portrait->x + portrait->width * 0.5f;
                input.pointerY = portrait->y + portrait->height * 0.5f;
                // Normal world mouse-look uses right-button hold to expose pointer interaction.
                // Modal character screens already allow the pointer without that hold.
                if (!runtime.characterScreen().open)
                {
                    input.rightMouseButton = {true, true, false};
                    session.updateGameplay(input, 0.0f);
                    input.rightMouseButton.pressed = false;
                }
                for (unsigned click = 0; click < 2; ++click)
                {
                    input.leftMouseButton = {true, true, false};
                    session.updateGameplay(input, 0.0f);
                    input.leftMouseButton = {false, false, true};
                    session.updateGameplay(input, 0.0f);
                }
                input.leftMouseButton = {};
                input.rightMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
                require(runtime.characterScreen().open
                    && runtime.characterScreen().page == GameplayUiController::CharacterPage::Inventory
                    && runtime.selectedCharacterScreenSourceIndex() == memberIndex,
                    "Normal portrait clicks did not open the selected companion's inventory");
                const Character &member = pParty->members()[memberIndex];
                const auto *pDoll = game.m_gameDataLoader.getCharacterDollTable().getCharacter(member.characterDataId);
                require(pDoll != nullptr, "Selected companion has no authoritative paperdoll entry");
                for (const std::string *pName : {&pDoll->backgroundAsset, &pDoll->bodyAsset, &pDoll->headAsset,
                    &pDoll->leftHandClosedAsset, &pDoll->leftHandHoldAsset, &pDoll->leftHandOpenAsset,
                    &pDoll->rightHandFingersAsset, &pDoll->rightHandOpenAsset, &pDoll->rightHandHoldAsset})
                {
                    if (!pName->empty() && *pName != "none")
                    {
                        std::cout << "Paperdoll image: member=" << memberIndex << " name=" << *pName << '\n';
                        require(runtime.gameplayUiRuntime().ensureHudTextureLoaded(*pName).has_value(),
                            "Selected companion's original body/hand/background image is missing");
                    }
                }
                const std::array<std::pair<EquipmentSlot, uint32_t>, 5> worn = {{
                    {EquipmentSlot::Armor, member.equipment.armor}, {EquipmentSlot::Helm, member.equipment.helm},
                    {EquipmentSlot::Belt, member.equipment.belt}, {EquipmentSlot::Boots, member.equipment.boots},
                    {EquipmentSlot::Cloak, member.equipment.cloak}}};
                for (const auto &[slot, itemId] : worn)
                {
                    if (itemId == 0) continue;
                    const ItemDefinition *pItem = runtime.itemTable()->get(itemId);
                    require(pItem != nullptr, "Equipped item has no definition");
                    for (EquipmentPicturePart part : {EquipmentPicturePart::Body, EquipmentPicturePart::Collar,
                        EquipmentPicturePart::CollarFront})
                    {
                        const std::string name = runtime.resolveEquippedItemHudTextureName(*pItem,
                            pDoll->dollTypeId, member.equipment.mainHand != 0, slot, part);
                        if (name.empty()) continue;
                        std::cout << "Fitted equipment image: member=" << memberIndex << " item=" << itemId
                            << " name=" << name << '\n';
                        require(name != pItem->iconName
                            && runtime.gameplayUiRuntime().ensureItemIconTextureLoaded(name).has_value(),
                            "Original fitted equipment image is missing or replaced by the inventory icon");
                    }
                }
                std::cout << "Paperdoll selected: member=" << memberIndex << " character_id=" << member.characterDataId
                    << " name=" << member.name << '\n';
            }
            if ((inspectionFrame - 1) % 45 == 0)
            {
                captures.push_back("paperdoll-first-" + std::to_string(memberIndex));
            }
            if ((inspectionFrame - 1) % 45 == 12)
            {
                captures.push_back("paperdoll-" + std::to_string(memberIndex));
            }
            return;
        }
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / pName,
                "正常穿戴装备验证", {}), "Cannot preserve equipment checkpoint");
        };
        if (frame == 201)
        {
            save("paperdolls-complete.oysav");
            return;
        }
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto pickItem = [&](uint32_t itemId, GameplayUiController::ItemInspectSourceType source)
        {
            for (const GameplayRenderedInspectableHudItem &item : runtime.renderedInspectableHudItems())
            {
                if (item.objectDescriptionId != itemId || item.sourceMemberIndex != 0 || item.sourceType != source)
                    continue;
                const GameplayRenderedInspectableHudItem original = item;
                for (float y = item.y + 2; y < item.y + item.height; y += 3)
                {
                    for (float x = item.x + 2; x < item.x + item.width; x += 3)
                    {
                        if (!runtime.isOpaqueHudPixelAtPoint(item, x, y)) continue;
                        if (source == GameplayUiController::ItemInspectSourceType::Inventory)
                        {
                            const auto *pLayout = runtime.findHudLayoutElement("CharacterInventoryGrid");
                            require(pLayout != nullptr, "Inventory grid layout is absent");
                            const auto grid = runtime.resolveHudLayoutElement(pLayout->id, width, height,
                                pLayout->width, pLayout->height);
                            require(grid.has_value(), "Inventory grid has no placement");
                            if (x < grid->x || y < grid->y || x >= grid->x + grid->width
                                || y >= grid->y + grid->height) continue;
                            const int cellX = static_cast<int>((x - grid->x) * Character::InventoryWidth / grid->width);
                            const int cellY = static_cast<int>(
                                (y - grid->y) * Character::InventoryHeight / grid->height);
                            const auto *pAtCell = pParty->members()[0].inventoryItemAt(cellX, cellY);
                            if (pAtCell == nullptr || pAtCell->objectDescriptionId != itemId) continue;
                        }
                        else
                        {
                            const auto panel = runtime.resolveHudLayoutElement("CharacterDollPanel",
                                width, height, 175, 378);
                            require(panel.has_value(), "Visible equipment panel is absent");
                            if (x < panel->x || y < panel->y || x >= panel->x + panel->width
                                || y >= panel->y + panel->height) continue;
                        }
                        std::cout << "Equipment pointer: frame=" << frame << " item=" << itemId
                            << " source=" << static_cast<int>(source) << " x=" << x << " y=" << y << '\n';
                        click(x, y);
                        require(runtime.heldInventoryItem().active
                            && runtime.heldInventoryItem().item.objectDescriptionId == itemId,
                            "Normal opaque item click did not pick up the expected equipment");
                        return original;
                    }
                }
            }
            throw std::runtime_error("Expected equipment is absent from actual inspectable HUD items");
        };
        const auto equipHeld = [&]()
        {
            const auto panel = runtime.resolveHudLayoutElement("CharacterDollPanel", width, height, 175, 378);
            require(panel.has_value(), "Paperdoll panel has no resolved placement");
            click(panel->x + panel->width * 0.35f, panel->y + panel->height * 0.5f);
            require(!runtime.heldInventoryItem().active, "Normal body click did not equip held item");
        };
        const auto replaceInBackpack = [&](size_t index)
        {
            const auto held = runtime.heldInventoryItem();
            require(held.active, "No equipment held for backpack return");
            const auto &original = equipment.originalItems[index];
            const auto *pLayout = runtime.findHudLayoutElement("CharacterInventoryGrid");
            require(pLayout != nullptr, "Inventory return grid is absent");
            const auto grid = runtime.resolveHudLayoutElement(pLayout->id, width, height,
                pLayout->width, pLayout->height);
            require(grid.has_value(), "Inventory return grid has no placement");
            // Original item graphics can overhang their occupied first-row cells.
            const float x = std::max(grid->x + 1.0f, original.x + held.grabOffsetX + 1.0f);
            const float y = std::max(grid->y + 1.0f, original.y + held.grabOffsetY + 1.0f);
            std::cout << "Equipment return: frame=" << frame << " item=" << held.item.objectDescriptionId
                << " x=" << x << " y=" << y << '\n';
            click(x, y);
            require(!runtime.heldInventoryItem().active, "Normal backpack click did not return equipment");
        };
        if (frame == 202)
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "Original knight portrait is absent");
            click(portrait->x + portrait->width / 2, portrait->y + portrait->height / 2);
            click(portrait->x + portrait->width / 2, portrait->y + portrait->height / 2);
            require(runtime.selectedCharacterScreenSourceIndex() == 0, "Cannot select original knight");
        }
        if (frame == 220)
        {
            equipment.originalItems[0] = pickItem(84, GameplayUiController::ItemInspectSourceType::Inventory);
        }
        if (frame == 221)
        {
            equipHeld();
            require(pParty->members()[0].equipment.armor == 84, "Original leather armor was not equipped");
        }
        if (frame == 232) { captures.push_back("equipped-armor-open"); save("equipped-armor-open.oysav"); }
        if (frame == 240)
        {
            equipment.originalItems[1] = pickItem(1, GameplayUiController::ItemInspectSourceType::Inventory);
        }
        if (frame == 241)
        {
            equipHeld();
            require(pParty->members()[0].equipment.mainHand == 1, "Original sword was not equipped");
        }
        if (frame == 252) { captures.push_back("equipped-armor-sword"); save("equipped-armor-sword.oysav"); }
        if (frame == 260) pickItem(1, GameplayUiController::ItemInspectSourceType::Equipment);
        if (frame == 261) replaceInBackpack(1);
        if (frame == 272) captures.push_back("equipped-armor-open-again");
        if (frame == 280) pickItem(84, GameplayUiController::ItemInspectSourceType::Equipment);
        if (frame == 281) replaceInBackpack(0);
        if (frame == 292) captures.push_back("equipment-restored");
        if (frame < 301) return;
        require(pParty->members()[0].equipment.mainHand == 0 && pParty->members()[0].equipment.armor == 0
            && !runtime.heldInventoryItem().active, "Equipment round trip did not restore empty slots");
        save("equipment-complete.oysav");
        complete = true;
        std::cout << "Four original paperdolls and normal armor/sword equip/unequip round trip passed\n";
    }

    struct CandlestickCheckState
    {
        unsigned phase = 0;
        size_t waypoint = 0;
        uint32_t phaseFrame = 0;
        unsigned stalledFrames = 0;
        float walkedDistance = 0.0f;
        int goldBefore = 0;
        std::vector<uint64_t> experienceBefore;
        std::vector<std::array<float, 2>> outdoorRoute;
        bool trainedMm6 = false;
        bool complete = false;
    };

    static void checkCandlestick(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, CandlestickCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        Party *pParty = runtime.party();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember(),
            "Candlestick route requires the surviving original four-member party");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto save = [&](const char *name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(name) + ".oysav"),
                "蜡烛台任务验证", {}), "Cannot preserve candlestick checkpoint");
        };
        const auto select = [&](EventDialogActionKind kind, uint32_t id)
        {
            selectQuestDialog(runtime, kind, id);
            state.phaseFrame = frame;
        };
        const auto walk = [&](const auto &route, bool indoor)
        {
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x;
            const float dy = route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            // A normal turn-based step travels about 156 units. Its waypoint radius must exceed
            // half a step, otherwise the route can oscillate across the waypoint without reaching it.
            if (distance < (indoor ? (state.trainedMm6 ? 85.0f : 55.0f) : 25.0f))
            {
                std::cout << "Candle waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            if (indoor) game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            else game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, distance < 80.0f ? 0.025f : 0.05f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.1f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 60)
            {
                std::cout << "Candle walk: frame=" << frame << " phase=" << state.phase
                    << " waypoint=" << state.waypoint << " position=" << pWorld->partyX() << ','
                    << pWorld->partyY() << ',' << pWorld->partyFootZ() << " remaining=" << distance
                    << " walked=" << state.walkedDistance << '\n';
            }
            require(state.stalledFrames < 120, "Candlestick route stopped at real collision geometry");
            return false;
        };
        if (frame == 1)
        {
            require(pParty->inventoryItemCount(2069) == (reload ? 0 : 1), "Unexpected candlestick inventory");
            require(!reload || !pParty->hasQuestBit(1150), "Reload still has an unfinished candle quest");
            if (state.trainedMm6)
            {
                require(reload || pParty->hasQuestBit(1150), "Trained return lost the previously accepted candle task");
                std::ifstream stream(std::filesystem::current_path().parent_path() / "mm6-candle-return.tsv");
                require(stream.good(), "Candle return has no actual walking route");
                std::string line;
                while (std::getline(stream, line))
                {
                    if (line.empty() || line[0] == '#') continue;
                    std::istringstream values(line);
                    float x, y, z;
                    require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y) && std::isfinite(z),
                        "Invalid candle return waypoint");
                    state.outdoorRoute.push_back({x, y});
                    require(state.outdoorRoute.size() <= 2000, "Candle return exceeds its waypoint bound");
                }
                require(state.outdoorRoute.size() >= 2, "Candle return route is empty");
            }
            if (session.currentMapFileName() == "oute3.odm")
            {
                state.phase = std::hypot(pWorld->partyX() + 8878.0f, pWorld->partyY() + 8697.0f) < 100.0f ? 3 : 2;
            }
            else
            {
                require(!reload && session.currentMapFileName() == "6d02.blv", "Unexpected candle input map");
                if (pWorld->activeChestView())
                {
                    GameplayInputFrame input;
                    input.screenWidth = width;
                    input.screenHeight = height;
                    input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                    session.updateGameplay(input, 0.0f);
                    session.updateGameplay({}, 0.0f);
                }
            }
            state.goldBefore = pParty->gold();
            for (const Character &member : pParty->members()) state.experienceBefore.push_back(member.experience);
            save("candle-input");
            return;
        }
        require(!state.trainedMm6 || frame < 10000, "Trained candle return exceeded its normal-input frame bound");
        if (state.phase == 0)
        {
            TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
            if (!turn.active())
            {
                press(KeyboardAction::Combat);
                return;
            }
            if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait) return;
            if (turn.stage() != TurnBasedCombatStage::Movement)
            {
                if (!turn.playerActionReady(*pParty)) return;
                const size_t member = pParty->activeMemberIndex();
                const auto cast = [&](uint32_t spellId, std::optional<size_t> actor, std::optional<size_t> character)
                {
                    const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(spellId);
                    require(pSpell && runtime.tryCastSpellFromMember(member, spellId, pSpell->name),
                        "Cannot start original retreat spell");
                    GameplaySpellService &spells = runtime.spellService();
                    PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                    request.targetActorIndex = actor;
                    request.targetCharacterIndex = character;
                    require(spells.validatePendingTargetSelectionRequest(runtime, request), "Retreat spell target rejected");
                    const int mana = pParty->members()[member].spellPoints;
                    const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                    require(result.castResult.succeeded() && pParty->members()[member].spellPoints == mana - 2,
                        "Retreat spell did not consume its original mana cost");
                    spells.clearPendingTargetSelection(runtime);
                    session.updateGameplay({}, 0.0f);
                    std::cout << "Candle retreat spell: id=" << spellId << " caster=" << member
                        << " target=" << (character ? int(*character) : -1) << " mana=" << mana << "->"
                        << pParty->members()[member].spellPoints << '\n';
                };
                if (member == 1 && pParty->members()[member].spellPoints >= 2)
                {
                    size_t weakest = size_t(-1);
                    float fraction = 1.0f;
                    for (size_t i = 0; i < pParty->members().size(); ++i)
                    {
                        const Character &candidate = pParty->members()[i];
                        if (candidate.conditions.test(size_t(CharacterCondition::Dead))
                            || candidate.conditions.test(size_t(CharacterCondition::Eradicated))
                            || candidate.maxHealth - candidate.health < 7) continue;
                        const float health = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                        if (health < fraction)
                        {
                            weakest = i;
                            fraction = health;
                        }
                    }
                    if (pParty->members()[1].health < 18) weakest = 1;
                    if (weakest != size_t(-1))
                    {
                        cast(68, std::nullopt, weakest);
                        return;
                    }
                }
                auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
                require(pScene, "Retreat lost its indoor scene");
                IndoorWorldRuntime &world = pScene->worldRuntime();
                size_t target = size_t(-1);
                float distance = 2000.0f;
                GameplayRuntimeActorState selected;
                for (size_t i = 0; i < world.mapActorCount(); ++i)
                {
                    GameplayRuntimeActorState actor;
                    if (!world.actorRuntimeState(i, actor) || actor.isDead || actor.isInvisible
                        || !actor.hostileToParty || !world.partyAttackActorHasLineOfSight(i)) continue;
                    const float next = std::hypot(actor.preciseX - world.partyX(), actor.preciseY - world.partyY());
                    if (next < distance)
                    {
                        distance = next;
                        target = i;
                        selected = actor;
                    }
                }
                if (target != size_t(-1))
                {
                    const IndoorMoveState &position = pScene->partyRuntime().movementState();
                    game.m_indoorRenderer.setCameraAngles(
                        std::atan2(selected.preciseY - position.y, selected.preciseX - position.x),
                        std::atan2(selected.preciseZ + selected.height * 0.5f - position.eyeZ(), distance));
                    if (member == 2 && pParty->members()[2].spellPoints >= 2)
                    {
                        cast(2, target, std::nullopt);
                        return;
                    }
                    if (distance <= 400.0f || pParty->equippedItemId(member, EquipmentSlot::Bow) != 0)
                    {
                        press(KeyboardAction::Attack);
                        return;
                    }
                }
                press(KeyboardAction::Pass);
                return;
            }
            constexpr std::array<std::array<float, 2>, 14> route{{
                {17890, -15220}, {17890, -16400}, {17120, -16400}, {17120, -16000},
                {16820, -16000}, {16640, -16000}, {16384, -16000}, {16384, -16440},
                {16384, -17000}, {16384, -17600}, {16384, -18400}, {16384, -19000},
                {16384, -19400}, {16384, -19600}}};
            if (walk(route, true))
            {
                save("candle-exit-door");
                game.m_indoorRenderer.setCameraAngles(-1.5707963f, 0.0f);
                press(KeyboardAction::Trigger);
                state.phase = 1;
                state.phaseFrame = frame;
            }
        }
        else if (state.phase == 1 && frame > state.phaseFrame + 10)
        {
            if (runtime.activeEventDialog().presentation == EventDialogPresentation::Transition)
            {
                const auto &actions = runtime.activeEventDialog().actions;
                const auto confirm = std::find_if(actions.begin(), actions.end(), [](const EventDialogAction &action)
                    { return action.kind == EventDialogActionKind::MapTransitionConfirm; });
                require(confirm != actions.end(), "Original temple exit has no normal confirmation");
                save("candle-exit-prompt");
                captures.push_back("candle-exit-prompt");
                select(EventDialogActionKind::MapTransitionConfirm, confirm->id);
                return;
            }
            require(session.currentMapFileName() == "oute3.odm", "Normal temple exit did not return to town");
            require(pParty->inventoryItemCount(2069) == 1, "Leaving the temple lost the candlestick");
            save("candle-outside");
            captures.push_back("candle-outside");
            state.phase = 2;
            state.waypoint = 0;
        }
        else if (state.phase == 2)
        {
            constexpr std::array<std::array<float, 2>, 8> route{{
                {-21400, -2500}, {-19500, -5200}, {-16400, -8300}, {-11400, -8800},
                {-9700, -9100}, {-9400, -8750}, {-9150, -8700}, {-8878, -8697}}};
            if (state.trainedMm6 ? walk(state.outdoorRoute, false) : walk(route, false)) state.phase = 3;
        }
        else if (state.phase == 3)
        {
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
            press(KeyboardAction::Trigger);
            state.phase = 4;
            state.phaseFrame = frame;
        }
        else if (state.phase == 4 && frame > state.phaseFrame + 10)
        {
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                "Normal interaction did not open the quest giver's tavern");
            select(EventDialogActionKind::HouseResident, 786);
            state.phase = reload ? 8 : 5;
        }
        else if (state.phase == 5 && frame > state.phaseFrame + 20)
        {
            state.goldBefore = pParty->gold();
            state.experienceBefore.clear();
            for (const Character &member : pParty->members()) state.experienceBefore.push_back(member.experience);
            save("candle-before");
            if (!pParty->hasQuestBit(1150)) select(EventDialogActionKind::NpcTopic, 1609);
            require(pParty->hasQuestBit(1150) && pParty->inventoryItemCount(2069) == 1,
                "Accepting the original quest consumed the item or failed to add the journal bit");
            save("candle-accepted");
            captures.push_back("candle-accepted");
            state.phase = 6;
        }
        else if (state.phase == 6 && frame > state.phaseFrame + 30)
        {
            select(EventDialogActionKind::NpcTopic, 1610);
            require(pParty->gold() == state.goldBefore + 1000 && pParty->inventoryItemCount(2069) == 0
                && !pParty->hasQuestBit(1150), "Original candle hand-in has wrong gold, item or journal state");
            for (size_t i = 0; i < pParty->members().size(); ++i)
            {
                require(pParty->members()[i].experience == state.experienceBefore[i] + 2000
                    && pParty->members()[i].awards.contains(81), "Candle reward missed a party member");
            }
            for (const std::string &line : runtime.activeEventDialog().lines)
                std::cout << "Candle response: " << line << '\n';
            save("candle-complete");
            captures.push_back("candle-reward");
            state.phase = 7;
        }
        else if (state.phase == 7 && frame > state.phaseFrame + 30)
        {
            // The reward leaves the NPC topic page open. Return through the normal back/leave
            // input before choosing the resident again; it is not a topic on the NPC page.
            const EventDialogContent &dialog = runtime.activeEventDialog();
            if (!dialog.isActive)
            {
                game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
                press(KeyboardAction::Trigger);
                state.phaseFrame = frame;
                return;
            }
            if (std::none_of(dialog.actions.begin(), dialog.actions.end(), [](const EventDialogAction &action)
                { return action.kind == EventDialogActionKind::HouseResident && action.id == 786; }))
            {
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                session.updateGameplay(input, 0.0f);
                session.updateGameplay({}, 0.0f);
                state.phaseFrame = frame;
                return;
            }
            select(EventDialogActionKind::HouseResident, 786);
            state.phase = 8;
        }
        else if (state.phase == 8 && frame > state.phaseFrame + 20)
        {
            const auto &actions = runtime.activeEventDialog().actions;
            require(std::none_of(actions.begin(), actions.end(), [](const EventDialogAction &action)
                { return action.kind == EventDialogActionKind::NpcTopic && (action.id == 1609 || action.id == 1610); }),
                "Completed candle quest can still be selected");
            require(pParty->gold() == state.goldBefore + (reload ? 0 : 1000)
                && pParty->inventoryItemCount(2069) == 0 && !pParty->hasQuestBit(1150),
                "Revisiting the quest giver changed the reward");
            for (size_t i = 0; i < pParty->members().size(); ++i)
            {
                require(pParty->members()[i].experience == state.experienceBefore[i] + (reload ? 0 : 2000)
                    && pParty->members()[i].awards.contains(81), "Repeated candle dialogue changed member rewards");
            }
            save("candle-repeat");
            captures.push_back("candle-repeat");
            state.complete = true;
            std::cout << "Candlestick quest passed: frame=" << frame << " reload=" << reload
                << " walked=" << state.walkedDistance << " gold=" << pParty->gold()
                << " candle=0 q1150=0 award81=all repeat_reward=0\n";
        }
    }

    struct FirstLetterCheckState
    {
        unsigned phase = 0;
        size_t waypoint = 0;
        uint32_t phaseFrame = 0;
        int startingGold = 0;
        float walkedDistance = 0.0f;
        bool complete = false;
    };

    static void checkFirstLetter(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, FirstLetterCheckState &state, std::vector<std::string> &captureRequests)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && session.currentMapFileName() == "oute3.odm",
            "First letter route needs New Sorpigal");
        require(pParty->members().size() == 4 && pParty->hasActableMember()
            && std::all_of(pParty->members().begin(), pParty->members().end(),
                [](const Character &member) { return member.level == 1 && member.experience == 0; }),
            "First letter route needs the normal level-one four-member party");
        require(pParty->inventoryItemCount(2125) == 1, "First letter must remain in the original inventory");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto select = [&](EventDialogActionKind kind, uint32_t id)
        {
            selectQuestDialog(runtime, kind, id);
            state.phaseFrame = frame;
        };
        if (frame == 1)
        {
            state.startingGold = pParty->gold();
            require(state.startingGold == (reload ? 1200 : 200)
                && pParty->hasQuestBit(1105) == !reload && pParty->hasQuestBit(1106) == reload,
                "First letter input has unexpected quest or reward state");
            if (reload) state.phase = 1;
            std::cout << "First letter input: reload=" << reload << " gold=" << state.startingGold
                << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << '\n';
        }
        if (state.phase == 0)
        {
            // Navigation waypoints supply directions only. Every position change goes through
            // normal session Forward input, terrain/building collision and actor simulation.
            constexpr std::array<std::array<float, 2>, 3> route = {{
                {-9700.0f, -9100.0f}, {-9150.0f, -8700.0f}, {-8878.0f, -8697.0f}}};
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x;
            const float dy = route[state.waypoint][1] - y;
            if (std::hypot(dx, dy) <= 25.0f)
            {
                std::cout << "Quest waypoint: index=" << state.waypoint << " position=" << x << ',' << y
                    << ',' << pWorld->partyFootZ() << " walked=" << state.walkedDistance << '\n';
                if (++state.waypoint == route.size()) state.phase = 1;
            }
            else
            {
                game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
                press(KeyboardAction::Forward, 0.025f);
                state.walkedDistance += std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
                if (frame % 60 == 0)
                    std::cout << "Quest walk: frame=" << frame << " waypoint=" << state.waypoint
                        << " position=" << pWorld->partyX() << ',' << pWorld->partyY()
                        << ',' << pWorld->partyFootZ() << " remaining=" << std::hypot(dx, dy) << '\n';
            }
        }
        else if (state.phase == 1)
        {
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
            press(KeyboardAction::Trigger);
            state.phaseFrame = frame;
            state.phase = 2;
        }
        else if (state.phase == 2 && frame > state.phaseFrame + 10)
        {
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                "Normal interaction did not open the Lonely Knight tavern");
            select(EventDialogActionKind::HouseResident, 786);
            state.phase = 3;
        }
        else if (state.phase == 3)
        {
            if (frame == state.phaseFrame + 2) captureRequests.push_back("letter-resident");
            if (frame > state.phaseFrame + 20)
            {
                select(EventDialogActionKind::NpcTopic, reload ? 1315 : 1314);
                state.phase = 4;
            }
        }
        else if (state.phase == 4)
        {
            require(pParty->gold() == 1200 && !pParty->hasQuestBit(1105) && pParty->hasQuestBit(1106),
                "First letter reward or follow-up quest is incorrect");
            if (frame == state.phaseFrame + 2)
            {
                captureRequests.push_back("letter-reward");
                for (const std::string &line : runtime.activeEventDialog().lines)
                    std::cout << "Quest response: " << line << '\n';
            }
            if (frame > state.phaseFrame + 20)
            {
                const auto &actions = runtime.activeEventDialog().actions;
                require(std::none_of(actions.begin(), actions.end(), [](const EventDialogAction &action)
                    { return action.kind == EventDialogActionKind::NpcTopic && action.id == 1314; }),
                    "Reward topic remained selectable after delivery");
                select(EventDialogActionKind::NpcTopic, 1315);
                state.phase = 5;
            }
        }
        else if (state.phase == 5)
        {
            require(pParty->gold() == 1200 && !pParty->hasQuestBit(1105) && pParty->hasQuestBit(1106),
                "Repeated conversation duplicated or lost the letter reward");
            if (frame == state.phaseFrame + 2) captureRequests.push_back("letter-repeat");
            if (frame > state.phaseFrame + 20)
            {
                require(game.quickSaveToPath(std::filesystem::current_path() / "letter-complete.oysav",
                    "舒尔曼的信·酒馆交付", {}), "Cannot save the first letter result");
                state.complete = true;
                std::cout << "First letter stage passed: frame=" << frame << " walked=" << state.walkedDistance
                    << " gold=" << pParty->gold() << " letter=1 q1105=0 q1106=1 repeat_reward=0\n";
            }
        }
    }

    struct CastleLetterCheckState
    {
        unsigned phase = 0;
        size_t waypoint = 0;
        uint32_t phaseFrame = 0;
        uint32_t stalledFrames = 0;
        int startingGold = 0;
        float departureMinutes = 0.0f;
        float walkedDistance = 0.0f;
        bool complete = false;
    };

    static void checkCastleLetter(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, CastleLetterCheckState &state, std::vector<std::string> &captureRequests)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember(),
            "Castle route needs the normal four-member party");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            pressQuestGameplay(session, width, height, action, delta);
        };
        const auto select = [&](EventDialogActionKind kind, uint32_t id)
        {
            selectQuestDialog(runtime, kind, id);
            state.phaseFrame = frame;
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                pName, {}), "Cannot preserve castle quest checkpoint");
        };
        const auto walk = [&](const auto &route)
        {
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x;
            const float dy = route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            if (distance <= 25.0f)
            {
                std::cout << "Castle waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ()
                    << " walked=" << state.walkedDistance << '\n';
                state.stalledFrames = 0;
                return ++state.waypoint == route.size();
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            press(KeyboardAction::Forward, distance < 60.0f ? 0.025f : 0.05f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 60 == 0 || state.stalledFrames == 60)
                std::cout << "Castle walk: frame=" << frame << " phase=" << state.phase
                    << " waypoint=" << state.waypoint << " position=" << pWorld->partyX() << ','
                    << pWorld->partyY() << ',' << pWorld->partyFootZ() << " remaining=" << distance
                    << " moved=" << moved << '\n';
            require(state.stalledFrames < 90, "Normal castle walking is blocked; inspect the saved collision position");
            return false;
        };
        if (frame == 1)
        {
            state.startingGold = pParty->gold();
            require(pParty->hasQuestBit(1106) == !reload && !pParty->hasQuestBit(1105)
                && pParty->inventoryItemCount(2125) == (reload ? 0 : 1),
                "Castle route input has unexpected letter/quest state");
            require(std::all_of(pParty->members().begin(), pParty->members().end(), [&](const Character &member)
                { return member.level == 1 && member.experience == (reload ? 3000u : 0u); }),
                "Castle route input has unexpected levels/experience");
            if (session.currentMapFileName() == "outd3.odm")
            {
                state.phase = reload ? 5 : 4;
            }
            else require(!reload && session.currentMapFileName() == "oute3.odm" && state.startingGold == 1200,
                "Castle route requires the actual tavern or coach arrival checkpoint");
            std::cout << "Castle input: map=" << session.currentMapFileName() << " gold=" << state.startingGold
                << " reload=" << reload << " phase=" << state.phase << '\n';
        }
        if (state.phase == 0)
        {
            constexpr std::array<std::array<float, 2>, 2> route = {{
                {-9700.0f, -9200.0f}, {-10188.0f, -9294.12f}}};
            if (walk(route)) state.phase = 1;
        }
        else if (state.phase == 1)
        {
            game.m_outdoorGameView.setCameraAngles(-2.6109f, -0.2175f);
            press(KeyboardAction::Trigger);
            state.phaseFrame = frame;
            state.phase = 2;
        }
        else if (state.phase == 2)
        {
            if (frame == state.phaseFrame + 2) captureRequests.push_back("castle-route");
            if (frame > state.phaseFrame + 20)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 470,
                    "Normal interaction did not open New Sorpigal stables");
                state.departureMinutes = pWorld->gameMinutes();
                save("coach-departure");
                select(EventDialogActionKind::HouseService, uint32_t(HouseActionId::TransportRoute));
                require(pParty->gold() == 1175 && std::abs(pWorld->gameMinutes()
                    - state.departureMinutes - 2880.0f) < 0.01f, "Coach fare or two-day duration is incorrect");
                state.phase = 3;
            }
        }
        else if (state.phase == 3 && frame > state.phaseFrame + 2)
        {
            require(session.currentMapFileName() == "outd3.odm" && pParty->gold() == 1175
                && pParty->hasQuestBit(1106) && pParty->inventoryItemCount(2125) == 1,
                "Coach did not arrive in Ironfist with the letter and task");
            require(std::hypot(pWorld->partyX() - 14317.0f, pWorld->partyY() - 2696.0f) < 1.0f,
                "Coach did not use its original destination coordinates");
            save("coach-arrived");
            captureRequests.push_back("castle-arrived");
            std::cout << "Coach arrival: position=" << pWorld->partyX() << ',' << pWorld->partyY()
                << ',' << pWorld->partyFootZ() << " game_minutes=" << pWorld->gameMinutes()
                << " gold=" << pParty->gold() << " food=" << pParty->food() << '\n';
            state.waypoint = 0;
            state.phase = 4;
        }
        else if (state.phase == 4)
        {
            constexpr std::array<std::array<float, 2>, 27> route = {{
                {14208.0f, 2688.0f}, {14208.0f, 3072.0f}, {12672.0f, 4608.0f},
                {12672.0f, 6144.0f}, {13184.0f, 6656.0f}, {13184.0f, 7168.0f},
                {14976.0f, 8960.0f}, {14976.0f, 9088.0f}, {15104.0f, 9216.0f},
                {15104.0f, 9344.0f}, {15232.0f, 9472.0f}, {15232.0f, 9600.0f},
                {15360.0f, 9984.0f}, {15488.0f, 10112.0f},
                {15488.0f, 10240.0f}, {15616.0f, 10368.0f}, {15616.0f, 10496.0f},
                {15744.0f, 10624.0f}, {15744.0f, 10752.0f}, {15872.0f, 10880.0f},
                {15872.0f, 11136.0f}, {16000.0f, 11264.0f}, {16128.0f, 12150.0f},
                {16000.0f, 13380.0f}, {17132.8f, 13700.0f}, {17132.8f, 15680.0f},
                {17132.8f, 16769.0f}}};
            if (walk(route)) state.phase = 5;
        }
        else if (state.phase == 5)
        {
            game.m_outdoorGameView.setCameraAngles(1.49011f, -0.2175f);
            press(KeyboardAction::Trigger);
            state.phaseFrame = frame;
            state.phase = 6;
        }
        else if (state.phase == 6 && frame > state.phaseFrame + 10)
        {
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 222,
                "Normal interaction did not open the Ironfist throne room");
            select(EventDialogActionKind::HouseResident, 789);
            state.phase = 7;
        }
        else if (state.phase == 7)
        {
            if (frame == state.phaseFrame + 2) captureRequests.push_back("castle-resident");
            if (frame > state.phaseFrame + 20)
            {
                select(EventDialogActionKind::NpcTopic, reload ? 1323 : 1322);
                state.phase = 8;
            }
        }
        else if (state.phase == 8)
        {
            require(pParty->gold() == 6175 && !pParty->hasQuestBit(1106) && !pParty->hasQuestBit(1105)
                && pParty->inventoryItemCount(2125) == 0, "Castle delivery reward or letter consumption is incorrect");
            require(std::all_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                { return member.experience == 3000 && member.awards.contains(100); }),
                "Castle delivery did not grant original XP and award to all four members");
            const auto &actions = runtime.activeEventDialog().actions;
            require(std::none_of(actions.begin(), actions.end(), [](const EventDialogAction &action)
                { return action.kind == EventDialogActionKind::NpcTopic && action.id == 1322; }),
                "Castle reward topic remained selectable after delivery");
            if (frame == state.phaseFrame + 2)
            {
                save("castle-delivered");
                captureRequests.push_back("castle-reward");
                for (const std::string &line : runtime.activeEventDialog().lines)
                    std::cout << "Castle response: " << line << '\n';
            }
            if (frame > state.phaseFrame + 20)
            {
                if (!reload) select(EventDialogActionKind::NpcTopic, 1323);
                state.phaseFrame = frame;
                state.phase = 9;
            }
        }
        else if (state.phase == 9)
        {
            if (frame == state.phaseFrame + 4 || frame == state.phaseFrame + 6)
                selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 1324);
            require(pParty->gold() == 6175 && pParty->hasQuestBit(1110)
                && pParty->inventoryItemCount(2125) == 0, "Next quest duplicated or lost the letter reward");
            require(std::all_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                { return member.experience == 3000 && member.awards.contains(100); }),
                "Next quest duplicated or lost character rewards");
            if (frame == state.phaseFrame + 2) captureRequests.push_back("castle-followup");
            if (frame > state.phaseFrame + 20)
            {
                save("castle-followup");
                state.complete = true;
                std::cout << "Castle letter passed: frame=" << frame << " walked=" << state.walkedDistance
                    << " gold=6175 xp_each=3000 award_each=100 letter=0 q1106=0 q1110=1\n";
            }
        }
    }

    struct IndoorCombatCheckState
    {
        size_t target = size_t(-1);
        bool entireEncounter = false;
        bool searchCorpses = true;
        std::set<size_t> encounterActors;
        std::set<size_t> searchedActors;
        int startingHealth = 0;
        int lastHealth = 0;
        int startingTargetHp = 0;
        uint32_t startingExperience = 0;
        uint32_t nearFrame = 0;
        uint32_t hitFrame = 0;
        uint32_t finishedFrame = 0;
        unsigned attackPresses = 0;
        unsigned emptyCorpses = 0;
        bool damaged = false;
        bool complete = false;
        unsigned spellsCast = 0;
        unsigned resolutionFrames = 0;
        unsigned resolutionImpacts = 0;
        unsigned resolutionPhases = 0;
        uint32_t lastTargetProgressFrame = 0;
        int lastObservedTargetHp = -1;
    };

    static void learnExistingSpellBook(GameApplication &game, Party &party,
        size_t memberIndex, uint32_t bookId, uint32_t spellId)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        const Character &member = party.members()[memberIndex];
        require(!member.knowsSpell(spellId), "Expected an unread original spellbook");
        const auto book = std::find_if(member.inventory.begin(), member.inventory.end(),
            [&](const InventoryItem &item) { return item.objectDescriptionId == bookId; });
        require(book != member.inventory.end(), "Original spellbook is missing");
        const int before = party.inventoryItemCount(bookId);
        InventoryItem held;
        require(party.takeItemFromMemberInventoryCell(memberIndex, book->gridX, book->gridY, held),
            "Cannot pick up the original spellbook");
        GameplayScreenRuntime &runtime = game.m_gameSession.gameplayScreenRuntime();
        GameplayHeldItemController::setHeldInventoryItem(runtime.heldInventoryItem(), held);
        require(runtime.itemService().tryUseHeldItemOnPartyMember(runtime, memberIndex, false)
            && member.knowsSpell(spellId) && !runtime.heldInventoryItem().active
            && party.inventoryItemCount(bookId) == before - 1,
            "Normal book use did not teach its spell and consume one item");
    }

    static bool equipExistingSlot(GameApplication &game, Party &party, size_t memberIndex, EquipmentSlot slot)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        const Character &member = party.members()[memberIndex];
        const GameDataLoader &data = game.m_gameDataLoader;
        const auto *doll = data.getCharacterDollTable().getCharacter(member.characterDataId);
        require(doll != nullptr, "Missing equipment/doll data");
        const auto *type = data.getCharacterDollTable().getDollType(doll->dollTypeId);
        require(type != nullptr, "Missing doll type");
        for (const InventoryItem &candidate : member.inventory)
        {
            const uint32_t itemId = candidate.objectDescriptionId;
            const auto *definition = data.getItemTable().get(itemId);
            require(definition != nullptr, "Undefined starter item");
            const auto plan = GameMechanics::resolveCharacterEquipPlan(
                member, *definition, &data.getItemTable(), type, std::nullopt, false);
            if (!plan || plan->targetSlot != slot) continue;
            const int originalCount = party.inventoryItemCount(itemId, memberIndex);
            InventoryItem item;
            require(party.takeItemFromMemberInventoryCell(memberIndex, candidate.gridX, candidate.gridY, item),
                "Cannot pick up starter item");
            std::optional<InventoryItem> replacement;
            require(party.tryEquipItemOnMember(memberIndex, slot, item, plan->displacedSlot,
                plan->autoStoreDisplacedItem, replacement), "Cannot equip the original item");
            if (replacement)
            {
                require(party.tryAutoPlaceItemInMemberInventory(memberIndex, *replacement),
                    "Cannot return displaced original equipment to the inventory");
            }
            require(party.equippedItemId(memberIndex, slot) == itemId
                && party.inventoryItemCount(itemId, memberIndex) == originalCount,
                "Equipping lost or duplicated starter item");
            std::cout << "Original equipment: member=" << memberIndex
                << " slot=" << int(slot) << " item=" << itemId << '\n';
            return true;
        }
        return false;
    }

    static void checkIndoorCombat(GameApplication &game, uint32_t frame, IndoorCombatCheckState &state,
        bool trainedMm6 = false, int width = 0, int height = 0)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        auto *scene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(scene != nullptr, "Indoor combat requires a real indoor scene");
        Party &party = scene->party();
        IndoorWorldRuntime &world = scene->worldRuntime();
        require(scene->mapDeltaData().has_value(), "Indoor combat has no persistent actor data");
        require((party.members().size() == 1 || party.members().size() == 4)
            && party.members().front().className == "Knight"
            && std::all_of(party.members().begin(), party.members().end(),
                [&](const Character &member) { return member.level == (trainedMm6 ? 3 : 1); }),
            "Indoor route requires the expected normal Knight-led party");
        if (trainedMm6)
            require(party.members().size() == 4 && scene->currentMapFileName() == "6d02.blv"
                && party.hasQuestBit(1110) && party.hasQuestBit(1150),
                "Trained indoor party lost its original MM6 quest state");
        int health = 0;
        uint32_t experience = 0;
        for (const Character &member : party.members())
        {
            health += std::max(0, member.health);
            experience += member.experience;
        }
        const auto press = [&](KeyboardAction action)
        {
            if (trainedMm6)
            {
                const bool walking = action == KeyboardAction::Forward
                    && !game.m_gameSession.turnBasedCombatRuntime().active();
                pressQuestGameplay(game.m_gameSession, width, height, action, walking ? 0.025f : 0.0f);
                return;
            }
            // Deliver a complete tap before the next normal application frame. This exercises
            // GameInputSystem's event latch; it is diagnostic input, not physical keyboard acceptance.
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = game.m_settings.keyboard.keyboardBinding(action);
            require(event.key.scancode != SDL_SCANCODE_UNKNOWN, "Diagnostic tap needs a keyboard binding");
            event.key.down = true;
            game.handleSdlEvent(event);
            event.type = SDL_EVENT_KEY_UP;
            event.key.down = false;
            game.handleSdlEvent(event);
        };
        if (frame == 1)
        {
            for (size_t index = 0; index < party.members().size(); ++index)
            {
                if (trainedMm6)
                {
                    require(party.equippedItemId(index, EquipmentSlot::MainHand) != 0,
                        "Trained MM6 party lost its previously equipped weapon");
                    continue;
                }
                require(equipExistingSlot(game, party, index, EquipmentSlot::MainHand),
                    "Party member has no usable starting weapon");
                equipExistingSlot(game, party, index, EquipmentSlot::Armor);
                if (state.entireEncounter) equipExistingSlot(game, party, index, EquipmentSlot::Bow);
            }
            if (state.entireEncounter)
            {
                const IndoorMoveState &start = scene->partyRuntime().movementState();
                for (size_t index = 0; index < world.mapActorCount(); ++index)
                {
                    GameplayRuntimeActorState actor;
                    if (world.actorRuntimeState(index, actor) && actor.hostileToParty && !actor.isDead
                        && !actor.isInvisible
                        && std::hypot(actor.preciseX - start.x, actor.preciseY - start.y) < 2500.0f)
                    {
                        state.encounterActors.insert(index);
                        const MapDeltaActor &original = scene->mapDeltaData()->actors[index];
                        std::cout << "Encounter member: actor=" << index << " monster=" << original.monsterInfoId
                            << " hp=" << original.hp << '\n';
                    }
                }
                require(state.encounterActors.size() > 1, "Entrance encounter needs multiple original actors");
            }
            state.startingHealth = state.lastHealth = health;
            state.startingExperience = experience;
            std::cout << "Indoor party equipped: members=" << party.members().size() << " health=" << health << '\n';
            if (state.entireEncounter)
            {
                require(party.members().size() == 4, "Encounter needs the original four-member party");
                if (!party.members()[2].knowsSpell(2)) learnExistingSpellBook(game, party, 2, 401, 2);
                press(KeyboardAction::Combat);
                return;
            }
        }
        require(party.hasActableMember(), "Party fell during the indoor encounter");
        const IndoorMoveState &position = scene->partyRuntime().movementState();
        if (state.entireEncounter || state.target == size_t(-1))
        {
            const size_t previousTarget = state.target;
            state.target = size_t(-1);
            int bestPriority = 3;
            float nearest = 6000.0f;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                GameplayRuntimeActorState actor;
                if (state.entireEncounter
                    && (!state.encounterActors.contains(index) || state.searchedActors.contains(index))) continue;
                if (!world.actorRuntimeState(index, actor) || !actor.hostileToParty
                    || (!state.entireEncounter && actor.isDead) || actor.isInvisible
                    || (!state.entireEncounter && !world.partyAttackActorHasLineOfSight(index))) continue;
                if (!state.entireEncounter && scene->mapDeltaData()->actors[index].hp <= 0) continue;
                const float distance = std::hypot(actor.preciseX - position.x, actor.preciseY - position.y);
                const int priority = !state.entireEncounter ? 0 : actor.isDead ? 2
                    : world.partyAttackActorHasLineOfSight(index) ? 0 : 1;
                if (priority < bestPriority || (priority == bestPriority && distance < nearest))
                {
                    bestPriority = priority;
                    nearest = distance;
                    state.target = index;
                }
            }
            require(state.target != size_t(-1), "No visible hostile reachable from the actual dungeon entrance");
            require(scene->mapDeltaData() && state.target < scene->mapDeltaData()->actors.size(),
                "Target has no persistent actor record");
            if (previousTarget != state.target)
            {
                state.startingTargetHp = scene->mapDeltaData()->actors[state.target].hp;
                state.lastTargetProgressFrame = frame;
                state.lastObservedTargetHp = state.startingTargetHp;
                std::cout << "Indoor target: actor=" << state.target << " hp=" << state.startingTargetHp
                    << " distance=" << nearest << " party=" << position.x << ',' << position.y << '\n';
            }
        }
        GameplayRuntimeActorState actor;
        require(world.actorRuntimeState(state.target, actor), "Indoor target disappeared");
        const int hp = scene->mapDeltaData()->actors[state.target].hp;
        if (state.lastObservedTargetHp != hp)
        {
            state.lastObservedTargetHp = hp;
            state.lastTargetProgressFrame = frame;
        }
        const float distance = std::hypot(actor.preciseX - position.x, actor.preciseY - position.y);
        game.m_indoorRenderer.setCameraAngles(
            std::atan2(actor.preciseY - position.y, actor.preciseX - position.x),
            std::atan2(actor.preciseZ + actor.height * 0.5f - position.eyeZ(), distance));
        if (health != state.lastHealth)
        {
            state.damaged = state.damaged || health < state.lastHealth;
            std::cout << "Indoor health: " << state.lastHealth << "->" << health << '\n';
            state.lastHealth = health;
        }
        if (frame % 120 == 0)
        {
            std::cout << "Indoor state: frame=" << frame << " distance=" << distance << " actor_hp=" << hp
                << " party_hp=" << health << " position=" << position.x << ',' << position.y
                << ',' << position.footZ << " sector=" << position.sectorId
                << " target=" << state.target << " los=" << world.partyAttackActorHasLineOfSight(state.target)
                << " actor_position=" << actor.preciseX << ',' << actor.preciseY << ',' << actor.preciseZ
                << " actor_height=" << actor.height << '\n';
        }
        require(!trainedMm6 || frame - state.lastTargetProgressFrame < 2400,
            "MM6 entrance automation made no target HP or target-selection progress for 2400 frames");
        if (distance < 400.0f && state.nearFrame == 0) state.nearFrame = frame;
        if (hp < state.startingTargetHp && state.hitFrame == 0)
        {
            state.hitFrame = frame;
            std::cout << "Indoor target damaged: hp=" << state.startingTargetHp << "->" << hp << '\n';
        }
        TurnBasedCombatRuntime &turn = game.m_gameSession.turnBasedCombatRuntime();
        if (trainedMm6 && state.entireEncounter && !state.searchCorpses && !turn.hasPendingActions()
            && std::all_of(state.encounterActors.begin(), state.encounterActors.end(), [&](size_t index)
                { return scene->mapDeltaData()->actors[index].hp <= 0; }))
        {
            require(state.attackPresses > 0 && experience > state.startingExperience,
                "MM6 entrance battle ended without normal attacks and experience");
            // Looting is separately exercised by the encounter check. The continuous quest route
            // leaves these corpses behind instead of chasing them through surrounding live enemies.
            state.complete = true;
            state.finishedFrame = frame;
            std::cout << "MM6 quest-route combat passed: defeated=" << state.encounterActors.size()
                << " corpse_searches=0 attack_presses=" << state.attackPresses << " experience=" << experience
                << " health=" << health << " turn_active=" << turn.active() << '\n';
            return;
        }
        if (state.entireEncounter && actor.isDead && turn.active())
        {
            if (turn.canToggleOff(party)) press(KeyboardAction::Combat);
            return;
        }
        if (actor.isDead && distance <= 250.0f)
        {
            require(state.attackPresses > 0 && experience > state.startingExperience,
                "Indoor kill lacks actual attack or experience");
            GameplayCorpseAutoLootResult loot;
            if (!world.openMapActorCorpseView(state.target))
            {
                const auto snapshot = world.snapshot();
                require(state.target < snapshot.mapActorCorpseViews.size()
                    && isConsumedCorpseView(snapshot.mapActorCorpseViews[state.target]),
                    "Dead indoor target has neither a loot view nor a consumed empty corpse");
                const int gold = party.gold();
                require(!world.openMapActorCorpseView(state.target) && party.gold() == gold,
                    "Empty corpse generated loot on the second search");
                ++state.emptyCorpses;
                std::cout << "Indoor empty corpse searched: actor=" << state.target << " experience=" << experience
                    << " gold=" << gold << " searches=" << state.emptyCorpses << '\n';
                require(game.quickSaveToPath(std::filesystem::current_path()
                    / ("empty-corpse-" + std::to_string(state.target) + ".oysav"),
                    "空尸体搜索验证", {}), "Cannot preserve actual empty-corpse state");
                loot.empty = true;
            }
            else
            {
                loot = autoLootActiveCorpseView(world, party, &game.m_gameDataLoader.getItemTable());
                require(!loot.blockedByInventory, "Original inventory cannot hold indoor loot");
                std::cout << "Indoor nonempty loot: actor=" << state.target << " gold=" << loot.goldAmount
                    << " first_item=" << loot.firstItemName << '\n';
            }
            const int gold = party.gold();
            world.closeActiveCorpseView();
            if (world.openMapActorCorpseView(state.target))
            {
                require(!autoLootActiveCorpseView(world, party, &game.m_gameDataLoader.getItemTable()).lootedAny
                    && party.gold() == gold, "Indoor corpse duplicated loot");
                world.closeActiveCorpseView();
            }
            if (state.entireEncounter)
            {
                state.searchedActors.insert(state.target);
                require(game.quickSaveToPath(std::filesystem::current_path()
                    / ("encounter-" + std::to_string(state.searchedActors.size()) + ".oysav"),
                    "神庙入口战斗", {}), "Cannot save encounter progress");
                std::cout << "Encounter progress: searched=" << state.searchedActors.size()
                    << '/' << state.encounterActors.size() << " last_actor=" << state.target
                    << " frame=" << frame << '\n';
                if (state.searchedActors.size() != state.encounterActors.size())
                {
                    state.target = size_t(-1);
                    return;
                }
                for (size_t index : state.encounterActors)
                    require(scene->mapDeltaData()->actors[index].hp <= 0, "Encounter retained a living target");
            }
            state.complete = true;
            state.finishedFrame = frame;
            std::cout << "Indoor combat passed: attack_presses=" << state.attackPresses << " health=" << health
                << " experience=" << experience << " gold=" << gold << " loot_gold=" << loot.goldAmount
                << " empty_corpses=" << state.emptyCorpses << " incoming_damage=" << state.damaged << '\n';
            for (size_t index = 0; index < party.members().size(); ++index)
            {
                const Character &member = party.members()[index];
                std::cout << "Indoor final member " << index << ": health=" << member.health
                    << " experience=" << member.experience << " recovery=" << member.recoverySecondsRemaining << '\n';
            }
            return;
        }
        if (state.entireEncounter && turn.active())
        {
            if (turn.hasPendingActions()) return;
            if (turn.stage() == TurnBasedCombatStage::Movement)
            {
                bool rangedMemberCanAct = false;
                for (size_t index = 0; trainedMm6 && index < party.members().size(); ++index)
                    if (GameMechanics::canAct(party.members()[index])
                        && (party.equippedItemId(index, EquipmentSlot::Bow) != 0
                            || (index == 2 && party.members()[index].spellPoints >= 2))) rangedMemberCanAct = true;
                const bool holdEntrance = trainedMm6 && rangedMemberCanAct
                    && world.partyAttackActorHasLineOfSight(state.target)
                    && frame - state.lastTargetProgressFrame < 600;
                const bool needsApproach = distance > 300.0f
                    || (trainedMm6 && !world.partyAttackActorHasLineOfSight(state.target) && distance > 80.0f);
                press(!holdEntrance && needsApproach && turn.movementActionPoints() >= 26
                    ? KeyboardAction::Forward : KeyboardAction::Pass);
                return;
            }
            if (!turn.playerActionReady(party)) return;
            const size_t member = party.activeMemberIndex();
            const bool visible = world.partyAttackActorHasLineOfSight(state.target);
            GameplayScreenRuntime &runtime = game.m_gameSession.gameplayScreenRuntime();
            if (trainedMm6 && member == 1 && party.members()[member].knowsSpell(68)
                && party.members()[member].spellPoints >= 2)
            {
                size_t weakest = size_t(-1);
                float lowestFraction = 0.7f;
                for (size_t index = 0; index < party.members().size(); ++index)
                {
                    const Character &candidate = party.members()[index];
                    if (candidate.conditions.test(size_t(CharacterCondition::Dead))
                        || candidate.conditions.test(size_t(CharacterCondition::Eradicated))) continue;
                    const float fraction = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                    if (fraction < lowestFraction) { lowestFraction = fraction; weakest = index; }
                }
                if (weakest != size_t(-1))
                {
                    const SpellEntry *pHeal = game.m_gameDataLoader.getSpellTable().findById(68);
                    require(pHeal && runtime.tryCastSpellFromMember(member, 68, pHeal->name),
                        "Learned normal Heal could not begin indoors");
                    GameplaySpellService &spells = runtime.spellService();
                    PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                    request.targetCharacterIndex = weakest;
                    require(spells.validatePendingTargetSelectionRequest(runtime, request), "Indoor Heal target rejected");
                    const int mana = party.members()[member].spellPoints;
                    const int health = party.members()[weakest].health;
                    const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                    require(result.castResult.succeeded() && party.members()[member].spellPoints == mana - 2
                        && party.members()[weakest].health == std::min(party.members()[weakest].maxHealth, health + 13),
                        "Learned level-four novice Heal has wrong cost or restoration");
                    spells.clearPendingTargetSelection(runtime);
                    game.m_gameSession.updateGameplay({}, 0.0f);
                    std::cout << "MM6 indoor Heal: member=" << weakest << " health=" << health << "->"
                        << party.members()[weakest].health << " mana=" << mana << "->"
                        << party.members()[member].spellPoints << '\n';
                    return;
                }
            }
            if (member == 2 && party.members()[2].spellPoints >= 2 && visible)
            {
                const SpellEntry *pBolt = game.m_gameDataLoader.getSpellTable().findById(2);
                require(pBolt && runtime.tryCastSpellFromMember(2, 2, pBolt->name),
                    "Original Fire Bolt could not begin");
                GameplaySpellService &spells = runtime.spellService();
                PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                request.targetActorIndex = state.target;
                require(spells.validatePendingTargetSelectionRequest(runtime, request), "Encounter target rejected");
                const int mana = party.members()[2].spellPoints;
                const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                require(result.castResult.succeeded() && party.members()[2].spellPoints == mana - 2,
                    "Encounter spell did not consume normal mana");
                spells.clearPendingTargetSelection(runtime);
                game.m_gameSession.updateGameplay({}, 0.0f);
                ++state.spellsCast;
                std::cout << "Encounter spell: target=" << state.target << " mana=" << mana
                    << "->" << party.members()[2].spellPoints << '\n';
            }
            else if (visible && (distance <= 400.0f || party.equippedItemId(member, EquipmentSlot::Bow) != 0))
            {
                press(KeyboardAction::Attack);
                ++state.attackPresses;
            }
            else
            {
                press(KeyboardAction::Pass);
            }
            return;
        }
        if (distance > 180.0f) press(KeyboardAction::Forward);
        bool ready = std::any_of(party.members().begin(), party.members().end(),
            [](const Character &member) { return GameMechanics::canTakeGameplayAction(member); });
        if (state.entireEncounter && distance > 250.0f)
        {
            ready = false;
            for (size_t index = 0; index < party.members().size(); ++index)
            {
                if (party.equippedItemId(index, EquipmentSlot::Bow) != 0
                    && GameMechanics::canTakeGameplayAction(party.members()[index])
                    && game.m_gameSession.gameplayScreenRuntime().trySelectPartyMember(index, true))
                {
                    ready = world.partyAttackActorHasLineOfSight(state.target);
                    break;
                }
            }
        }
        if (hp > 0 && (distance <= 250.0f || state.entireEncounter) && ready)
        {
            press(KeyboardAction::Attack);
            ++state.attackPresses;
        }
    }

    struct TempleChestCheckState
    {
        size_t waypoint = 0;
        unsigned phase = 0;
        uint32_t phaseFrame = 0;
        bool doorPressed = false;
        bool complete = false;
        float walkedDistance = 0.0f;
        float previousX = 0.0f;
        float previousY = 0.0f;
        unsigned heals = 0;
        unsigned itemsTaken = 0;
        unsigned potions = 0;
        unsigned chestAttempts = 0;
        bool reloadEmpty = false;
        bool pointerTransfer = false;
        bool trainedMm6 = false;
        int goldAfterLoot = 0;
        int goldTaken = 0;
    };

    static void checkTempleChest(GameApplication &game, uint32_t frame, int width, int height,
        TempleChestCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok)
            {
                throw std::runtime_error(message);
            }
        };
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(pScene && pScene->currentMapFileName() == "6d02.blv", "Chest route needs the actual temple");
        Party &party = pScene->party();
        IndoorWorldRuntime &world = pScene->worldRuntime();
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
        const auto press = [&](KeyboardAction action)
        {
            pressQuestGameplay(session, width, height, action);
        };
        const auto key = [&](SDL_Scancode code)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[code] = true;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto save = [&](const char *pName)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (std::string(pName) + ".oysav"),
                "神庙宝箱流程", {}), "Cannot preserve chest route");
        };
        const auto aim = [&](float x, float y, float z)
        {
            const IndoorMoveState &position = pScene->partyRuntime().movementState();
            const float distance = std::hypot(x - position.x, y - position.y);
            game.m_indoorRenderer.setCameraAngles(std::atan2(y - position.y, x - position.x),
                std::atan2(z - position.eyeZ(), distance));
        };
        const auto cast = [&](size_t member, uint32_t spellId, std::optional<size_t> actor,
            std::optional<size_t> character)
        {
            const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(spellId);
            require(pSpell && runtime.tryCastSpellFromMember(member, spellId, pSpell->name),
                "Cannot start the original spell");
            GameplaySpellService &spells = runtime.spellService();
            PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
            request.targetActorIndex = actor;
            request.targetCharacterIndex = character;
            require(spells.validatePendingTargetSelectionRequest(runtime, request), "Spell target rejected");
            const int mana = party.members()[member].spellPoints;
            const int hp = character ? party.members()[*character].health : 0;
            const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
            require(result.castResult.succeeded() && party.members()[member].spellPoints == mana - 2,
                "Normal spell did not consume its original mana cost");
            if (character)
            {
                const Character &target = party.members()[*character];
                const int restoration = state.trainedMm6 ? 13 : 7;
                require(target.health == std::min(target.maxHealth, hp + restoration),
                    "Original novice Heal amount differs from the learned Body Magic skill");
                ++state.heals;
                std::cout << "Chest route Heal: member=" << *character << " hp=" << hp << "->" << target.health
                    << " mana=" << mana << "->" << party.members()[member].spellPoints << '\n';
            }
            spells.clearPendingTargetSelection(runtime);
            session.updateGameplay({}, 0.0f);
        };
        const IndoorMoveState &position = pScene->partyRuntime().movementState();
        if (frame == 1)
        {
            require(party.members().size() == 4 && party.members()[1].className == "Cleric",
                "Chest route requires the original four-member party");
            if (state.trainedMm6)
            {
                const CharacterSkill *pBody = party.members()[1].findSkillByCanonicalName("BodyMagic");
                require(pBody && pBody->level == 4 && party.hasQuestBit(1150)
                    && party.inventoryItemCount(2069) == 0,
                    "Trained chest route requires the accepted quest and learned Body Magic");
            }
            if (!party.members()[1].knowsSpell(68))
            {
                learnExistingSpellBook(game, party, 1, 467, 68);
            }
            if (party.equippedItemId(1, EquipmentSlot::OffHand) == 0)
            {
                require(equipExistingSlot(game, party, 1, EquipmentSlot::OffHand), "Original shield is unavailable");
            }
            if (party.equippedItemId(1, EquipmentSlot::MainHand) == 0)
            {
                require(equipExistingSlot(game, party, 1, EquipmentSlot::MainHand), "Original mace is unavailable");
            }
            state.previousX = position.x;
            state.previousY = position.y;
            if (std::hypot(position.x - 18016.0f, position.y + 15180.0f) < 180.0f)
            {
                state.waypoint = 10;
                state.doorPressed = true;
            }
            else if (position.x > 16600.0f && std::abs(position.y + 16000.0f) < 150.0f)
            {
                state.waypoint = 4;
                const auto &mechanisms = pScene->eventRuntimeState()->mechanisms;
                state.doorPressed = mechanisms.contains(3)
                    && (mechanisms.at(3).isMoving || mechanisms.at(3).currentDistance > 0.0f);
            }
            if (!turn.active())
            {
                press(KeyboardAction::Combat);
            }
            save("route-start");
            if (state.reloadEmpty)
            {
                const GameplayChestViewState *pChest = world.activeChestView();
                require(pChest && pChest->chestId == 2 && pChest->items.empty() && pChest->hiddenItems.empty(),
                    "Reload requires the actual depleted temple chest");
                state.phase = 2;
                state.phaseFrame = frame;
                state.goldAfterLoot = party.gold();
                captures.push_back("chest-reloaded");
            }
            else if (state.doorPressed)
            {
                captures.push_back("chest-door");
            }
            return;
        }
        state.walkedDistance += std::hypot(position.x - state.previousX, position.y - state.previousY);
        state.previousX = position.x;
        state.previousY = position.y;
        require(party.hasActableMember(), "Party fell on the normal chest route");
        require(!state.trainedMm6 || frame < 8000, "Trained chest route exceeded its normal-input frame bound");
        if (frame % 120 == 0)
        {
            std::cout << "Chest route: frame=" << frame << " waypoint=" << state.waypoint
                << " position=" << position.x << ',' << position.y << ',' << position.footZ
                << " sector=" << position.sectorId << " walked=" << state.walkedDistance
                << " hp=";
            for (const Character &member : party.members())
            {
                std::cout << member.health << ',';
            }
            std::cout << '\n';
        }
        if (GameplayChestViewState *pChest = world.activeChestView())
        {
            require(pChest->chestId == 2, "Route opened a different original chest");
            if (state.phase == 3)
            {
                require(pChest->items.empty() && pChest->hiddenItems.empty() && party.gold() == state.goldAfterLoot,
                    "Reopening the actual depleted chest regenerated loot");
                require((pChest->flags & uint16_t(EvtChestFlag::Opened)) != 0,
                    "Reopening the actual chest lost its Opened flag");
                save("chest-reopened");
                captures.push_back("chest-reopened");
                state.phaseFrame = frame;
                state.complete = true;
                std::cout << "Temple chest reopen passed: items=" << state.itemsTaken << " gold=" << state.goldTaken
                    << " walked=" << state.walkedDistance << " heals=" << state.heals
                    << " potions=" << state.potions << " reloaded=" << state.reloadEmpty << '\n';
                return;
            }
            if (state.phase == 2)
            {
                if (frame < state.phaseFrame + 45)
                {
                    return;
                }
                key(SDL_SCANCODE_ESCAPE);
                require(!world.activeChestView(), "Normal Escape did not close the depleted chest");
                save("chest-closed");
                state.phase = 3;
                return;
            }
            if (state.phase == 0)
            {
                require(!pChest->items.empty(), "Original chest has no visible loot");
                state.phase = 1;
                state.phaseFrame = frame;
                std::cout << "Original chest opened: id=" << pChest->chestId << " items=" << pChest->items.size()
                    << " hidden=" << pChest->hiddenItems.size() << " flags=" << pChest->flags << '\n';
                save("chest-open");
                return;
            }
            if (frame == state.phaseFrame + 10)
            {
                if (state.pointerTransfer)
                {
                    require(runtime.renderedInspectableHudItems().size() == pChest->items.size(),
                        "Chest item images are absent from the rendered UI");
                }
                captures.push_back("chest-open");
            }
            if (state.pointerTransfer && frame >= state.phaseFrame + 20 && frame <= state.phaseFrame + 23)
            {
                const auto &rendered = runtime.renderedInspectableHudItems();
                const auto icon = std::find_if(rendered.begin(), rendered.end(), [](const auto &value)
                {
                    return value.objectDescriptionId == 2069
                        && value.sourceType == GameplayUiController::ItemInspectSourceType::Chest;
                });
                require(icon != rendered.end(), "Original candlestick has no inspectable rendered image");
                float hitX = 0.0f;
                float hitY = 0.0f;
                bool opaque = false;
                for (float y = icon->y + 1.0f; y < icon->y + icon->height && !opaque; y += 2.0f)
                {
                    for (float x = icon->x + 1.0f; x < icon->x + icon->width && !opaque; x += 2.0f)
                    {
                        if (runtime.isOpaqueHudPixelAtPoint(*icon, x, y))
                        {
                            hitX = x;
                            hitY = y;
                            opaque = true;
                        }
                    }
                }
                require(opaque, "Candlestick image contains no opaque inspect target");
                int windowCount = 0;
                SDL_Window **pWindows = SDL_GetWindows(&windowCount);
                SDL_Window *pWindow = pWindows && windowCount == 1 ? pWindows[0] : nullptr;
                SDL_free(pWindows);
                require(pWindow != nullptr, "SDL pointer validation needs the single capture window");
                int pointWidth = 0;
                int pointHeight = 0;
                require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight), "Cannot get SDL window point size");
                SDL_Event event = {};
                const bool down = frame != state.phaseFrame + 23;
                event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID = SDL_GetWindowID(pWindow);
                event.button.button = SDL_BUTTON_RIGHT;
                event.button.down = down;
                event.button.x = hitX * pointWidth / width;
                event.button.y = hitY * pointHeight / height;
                game.m_gameInputSystem.handleSdlEvent(event);
                if (frame == state.phaseFrame + 22)
                {
                    const auto &inspection = runtime.itemInspectOverlayReadOnly();
                    const ItemDefinition *pItem = runtime.itemTable()->get(2069);
                    const GameplayInputFrame *pInput = runtime.currentGameplayInputFrame();
                    std::cout << "Chest inspection receipt: target=" << hitX << ',' << hitY
                        << " input=" << (pInput ? pInput->pointerX : -1.0f) << ','
                        << (pInput ? pInput->pointerY : -1.0f)
                        << " right=" << (pInput && pInput->rightMouseButton.held)
                        << " overlay=" << inspection.active << ':' << inspection.objectDescriptionId
                        << " name=" << (pItem ? pItem->name : "missing") << '\n';
                    require(inspection.active && inspection.objectDescriptionId == 2069
                        && pItem && pItem->name == "蜡烛台", "SDL right click did not inspect the Chinese candlestick");
                    captures.push_back("chest-inspect");
                }
            }
            if (frame < state.phaseFrame + 45)
            {
                return;
            }
            if (!pChest->items.empty())
            {
                const GameplayChestItemState item = pChest->items.front();
                const size_t count = pChest->items.size();
                const int before = party.inventoryItemCount(item.item.objectDescriptionId);
                const int gold = party.gold();
                if (state.pointerTransfer)
                {
                    const bool touch = !item.isGold && state.itemsTaken == 1;
                    const auto click = [&](float x, float y)
                    {
                        GameplayInputFrame input;
                        input.screenWidth = width;
                        input.screenHeight = height;
                        input.pointerX = x;
                        input.pointerY = y;
                        input.pointerFromTouch = touch;
                        input.leftMouseButton = {true, true, false};
                        session.updateGameplay(input, 0.0f);
                        input.leftMouseButton = {false, false, true};
                        session.updateGameplay(input, 0.0f);
                    };
                    const auto grid = runtime.resolveChestGridArea(width, height);
                    require(grid.has_value(), "Rendered chest has no pointer grid");
                    const float cellWidth = grid->width / pChest->gridWidth;
                    const float cellHeight = grid->height / pChest->gridHeight;
                    click(grid->x + (item.gridX + 0.5f) * cellWidth,
                        grid->y + (item.gridY + 0.5f) * cellHeight);
                    if (!item.isGold)
                    {
                        require(runtime.heldInventoryItem().active
                            && runtime.heldInventoryItem().item.objectDescriptionId == item.itemId,
                            "Pointer click did not pick up the visible chest item");
                        const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
                        require(portrait.has_value(), "Chest transfer has no recipient portrait");
                        click(portrait->x + portrait->width * 0.5f, portrait->y + portrait->height * 0.5f);
                        require(!runtime.heldInventoryItem().active,
                            "Clicking the recipient portrait did not store the picked-up item");
                    }
                    std::cout << "Chest pointer transfer: touch=" << touch << " item=" << item.itemId << '\n';
                }
                else
                {
                    runtime.interactionState().chestSelectionIndex = 0;
                    key(SDL_SCANCODE_RETURN);
                }
                pChest = world.activeChestView();
                require(pChest && pChest->items.size() == count - 1, "Chest transfer did not remove exactly one item");
                if (item.isGold)
                {
                    require(party.gold() == gold + int(item.goldAmount), "Chest gold transfer differs");
                    state.goldTaken += int(item.goldAmount);
                }
                else
                {
                    require(party.inventoryItemCount(item.item.objectDescriptionId) == before + int(item.item.quantity),
                        "Chest transfer lost or duplicated the original item");
                    ++state.itemsTaken;
                }
                std::cout << "Chest transfer: item=" << item.item.objectDescriptionId
                    << " quantity=" << item.item.quantity << " gold=" << item.goldAmount << '\n';
                return;
            }
            require(pChest->hiddenItems.empty(), "Chest retains hidden overflow items");
            require(state.itemsTaken > 0, "Nonempty item loot has not been demonstrated");
            save("chest-taken");
            captures.push_back("chest-taken");
            state.phase = 2;
            state.phaseFrame = frame;
            state.goldAfterLoot = party.gold();
            std::cout << "Temple chest loot passed: items=" << state.itemsTaken << " gold=" << state.goldTaken
                << " walked=" << state.walkedDistance << " heals=" << state.heals << '\n';
            return;
        }
        if (state.phase == 3)
        {
            aim(18016.0f, -15056.0f, 256.0f);
            press(KeyboardAction::Trigger);
            return;
        }
        require(state.phase == 0, "Chest unexpectedly closed during transfer");
        const std::array<std::array<float, 2>, 10> route{{
            {16384.0f, -17000.0f}, {16384.0f, -16440.0f}, {16384.0f, -16000.0f},
            {16640.0f, -16000.0f}, {16820.0f, -16000.0f},
            {17120.0f, -16000.0f}, {17120.0f, -16400.0f}, {17890.0f, -16400.0f},
            {17890.0f, -15220.0f}, {18016.0f, -15180.0f}}};
        // A trained turn-based step is about 156 units. Accept its nearest endpoint
        // instead of oscillating across a waypoint whose radius is below half a step.
        const float waypointRadius = state.trainedMm6 ? 85.0f : 70.0f;
        while (state.waypoint < route.size()
            && std::hypot(route[state.waypoint][0] - position.x,
                route[state.waypoint][1] - position.y) < waypointRadius)
        {
            std::cout << "Chest waypoint reached: " << state.waypoint << '\n';
            ++state.waypoint;
        }
        if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait)
        {
            return;
        }
        if (state.waypoint >= 5 && !state.doorPressed)
        {
            aim(17008.0f, -16000.0f, 352.0f);
            press(KeyboardAction::Trigger);
            state.doorPressed = true;
            save("door-pressed");
            captures.push_back("chest-door");
            return;
        }
        if (state.waypoint == route.size())
        {
            // The original temple trap rolls 5 + 1d20 before resistance. Use the party's
            // remaining starting reagents through the normal item service before opening it.
            if (party.members()[0].health <= 25 && state.chestAttempts == 0)
            {
                bool mixed = false;
                for (size_t owner = 0; owner < party.members().size() && !mixed; ++owner)
                {
                    const auto &inventory = party.members()[owner].inventory;
                    const auto berry = std::find_if(inventory.begin(), inventory.end(),
                        [](const InventoryItem &item) { return item.objectDescriptionId == 200; });
                    const auto bottle = std::find_if(inventory.begin(), inventory.end(),
                        [](const InventoryItem &item) { return item.objectDescriptionId == 220; });
                    if (berry == inventory.end() || bottle == inventory.end())
                    {
                        continue;
                    }
                    const uint8_t bottleX = bottle->gridX;
                    const uint8_t bottleY = bottle->gridY;
                    const int berries = party.inventoryItemCount(200);
                    const int bottles = party.inventoryItemCount(220);
                    const int potions = party.inventoryItemCount(222);
                    InventoryItem reagent;
                    require(party.takeItemFromMemberInventoryCell(owner, berry->gridX, berry->gridY, reagent),
                        "Cannot pick up an original healing reagent");
                    GameplayHeldItemController::setHeldInventoryItem(runtime.heldInventoryItem(), reagent);
                    require(runtime.itemService().tryUseHeldItemOnInventoryItem(runtime, owner, bottleX, bottleY)
                        && !runtime.heldInventoryItem().active
                        && party.inventoryItemCount(200) == berries - 1
                        && party.inventoryItemCount(220) == bottles - 1
                        && party.inventoryItemCount(222) == potions + 1,
                        "Original berry/bottle recipe failed conservation");
                    InventoryItem potion;
                    require(party.takeItemFromMemberInventoryCell(owner, bottleX, bottleY, potion)
                        && potion.objectDescriptionId == 222, "Cannot pick up the mixed healing potion");
                    const int hp = party.members()[0].health;
                    GameplayHeldItemController::setHeldInventoryItem(runtime.heldInventoryItem(), potion);
                    require(runtime.itemService().tryUseHeldItemOnPartyMember(runtime, 0, false)
                        && !runtime.heldInventoryItem().active && party.members()[0].health > hp
                        && party.inventoryItemCount(222) == potions,
                        "Normal potion use did not heal and consume its item");
                    ++state.potions;
                    mixed = true;
                    std::cout << "Chest preparation potion: owner=" << owner << " target=0 hp=" << hp
                        << "->" << party.members()[0].health << " power=" << potion.standardEnchantPower << '\n';
                }
                require(mixed, "Original healing supplies cannot prepare the party for this trap");
                return;
            }
            aim(18016.0f, -15056.0f, 256.0f);
            if (frame % 15 == 0)
            {
                if (state.chestAttempts == 0)
                {
                    save("before-chest");
                }
                ++state.chestAttempts;
                press(KeyboardAction::Trigger);
                save("after-chest-trigger");
                std::cout << "Chest trigger: attempt=" << state.chestAttempts << " hp=";
                for (const Character &member : party.members())
                {
                    std::cout << member.health << ',';
                }
                std::cout << '\n';
                require(party.hasActableMember(), "Original chest trap defeated the party");
            }
            return;
        }
        if (turn.stage() == TurnBasedCombatStage::Movement)
        {
            aim(route[state.waypoint][0], route[state.waypoint][1], position.eyeZ());
            press(KeyboardAction::Forward);
            return;
        }
        if (!turn.playerActionReady(party))
        {
            return;
        }
        const size_t member = party.activeMemberIndex();
        if (member == 1 && party.members()[member].spellPoints >= 2)
        {
            size_t weakest = size_t(-1);
            float lowestHealthFraction = 1.0f;
            for (size_t i = 0; i < party.members().size(); ++i)
            {
                const Character &candidate = party.members()[i];
                if (candidate.conditions.test(size_t(CharacterCondition::Dead))
                    || candidate.conditions.test(size_t(CharacterCondition::Eradicated))
                    || candidate.maxHealth - candidate.health < 7)
                {
                    continue;
                }
                const float fraction = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                if (fraction < lowestHealthFraction)
                {
                    weakest = i;
                    lowestHealthFraction = fraction;
                }
            }
            if (party.members()[1].health < 18) weakest = 1;
            if (weakest != size_t(-1))
            {
                cast(1, 68, std::nullopt, weakest);
                return;
            }
        }
        size_t target = size_t(-1);
        float distance = 2000.0f;
        GameplayRuntimeActorState selected;
        for (size_t i = 0; i < world.mapActorCount(); ++i)
        {
            GameplayRuntimeActorState actor;
            if (!world.actorRuntimeState(i, actor) || actor.isDead || actor.isInvisible || !actor.hostileToParty
                || !world.partyAttackActorHasLineOfSight(i))
            {
                continue;
            }
            const float next = std::hypot(actor.preciseX - position.x, actor.preciseY - position.y);
            if (next < distance)
            {
                target = i;
                distance = next;
                selected = actor;
            }
        }
        if (target != size_t(-1))
        {
            aim(selected.preciseX, selected.preciseY, selected.preciseZ + selected.height * 0.5f);
            if (member == 2 && party.members()[2].spellPoints >= 2)
            {
                cast(2, 2, target, std::nullopt);
                return;
            }
            if (distance <= 400.0f || party.equippedItemId(member, EquipmentSlot::Bow) != 0)
            {
                press(KeyboardAction::Attack);
                return;
            }
        }
        press(KeyboardAction::Pass);
    }

    struct IndoorTurnSaveCheckState
    {
        size_t target = size_t(-1);
        int startingTargetHp = 0;
        int manaBeforeCast = 0;
        std::vector<int> hpBeforeCast;
        bool cast = false;
        bool hit = false;
        bool complete = false;
        unsigned passes = 0;
    };

    static void checkIndoorTurnSaves(GameApplication &game, uint32_t frame, int width, int height,
        IndoorTurnSaveCheckState &state)
    {
        const auto require = [](bool ok, const char *pMessage)
        {
            if (!ok) throw std::runtime_error(pMessage);
        };
        auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
        require(pScene && pScene->mapDeltaData(), "Indoor turn check requires original indoor actor data");
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
        Party &party = pScene->party();
        IndoorWorldRuntime &world = pScene->worldRuntime();
        require(party.members().size() == 4 && party.members()[2].className == "Sorcerer",
            "Indoor turn check requires the normal four-member party");
        require(party.hasActableMember(), "Party fell during indoor turn save check");
        const auto press = [&](KeyboardAction action)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.actions[keyboardActionIndex(action)] = {true, true, false};
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        if (frame == 1)
        {
            for (size_t index = 0; index < party.members().size(); ++index)
            {
                require(equipExistingSlot(game, party, index, EquipmentSlot::MainHand), "No original usable weapon");
                equipExistingSlot(game, party, index, EquipmentSlot::Armor);
            }
            learnExistingSpellBook(game, party, 2, 401, 2);
            float nearest = 6000.0f;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                GameplayRuntimeActorState actor;
                if (!world.actorRuntimeState(index, actor) || actor.isDead || actor.isInvisible
                    || !actor.hostileToParty || !world.partyAttackActorHasLineOfSight(index)) continue;
                const float distance = std::hypot(actor.preciseX - world.partyX(), actor.preciseY - world.partyY());
                if (distance < nearest)
                {
                    nearest = distance;
                    state.target = index;
                }
            }
            require(state.target != size_t(-1), "No visible original indoor target");
            state.startingTargetHp = pScene->mapDeltaData()->actors[state.target].hp;
            std::cout << "Indoor turn setup: target=" << state.target << " hp=" << state.startingTargetHp
                << " distance=" << nearest << " caster=2 learned_from_book=401\n";
            press(KeyboardAction::Combat);
            require(turn.active(), "Normal Combat input did not enter indoor turn mode");
            return;
        }
        GameplayRuntimeActorState actor;
        require(world.actorRuntimeState(state.target, actor), "Indoor saved target disappeared");
        const float distance = std::hypot(actor.preciseX - world.partyX(), actor.preciseY - world.partyY());
        game.m_indoorRenderer.setCameraAngles(
            std::atan2(actor.preciseY - world.partyY(), actor.preciseX - world.partyX()),
            std::atan2(actor.preciseZ + actor.height * 0.5f - pScene->partyRuntime().movementState().eyeZ(), distance));
        if (turn.hasPendingActions()) return;
        if (state.cast && !state.hit)
        {
            const auto &actors = pScene->mapDeltaData()->actors;
            require(actors.size() == state.hpBeforeCast.size(), "Indoor cast changed the actor registry");
            size_t actualTarget = size_t(-1);
            unsigned damaged = 0;
            for (size_t index = 0; index < actors.size(); ++index)
            {
                if (actors[index].hp < state.hpBeforeCast[index])
                {
                    actualTarget = index;
                    ++damaged;
                }
            }
            require(damaged == 1, "Indoor single-target Fire Bolt did not hit exactly one original actor");
            const int hp = actors[actualTarget].hp;
            require(party.members()[2].spellPoints == state.manaBeforeCast - 2, "Indoor Fire Bolt mana changed twice");
            state.hit = true;
            std::cout << "Indoor turn hit: selected=" << state.target << " actual=" << actualTarget
                << " hp=" << state.hpBeforeCast[actualTarget] << "->" << hp
                << " caster_mana=" << party.members()[2].spellPoints << '\n';
        }
        if (turn.stage() == TurnBasedCombatStage::Movement)
        {
            const float x = world.partyX();
            const float y = world.partyY();
            press(KeyboardAction::Forward);
            const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
            require(turn.movementActionPoints() == 104 && moved > 1.0f,
                "Normal indoor movement did not spend one step and move through collision");
            require(state.hit, "Movement checkpoint reached before the actual cast/hit");
            std::cout << "Indoor turn moved: distance=" << moved << " movement_ap=104 passes=" << state.passes << '\n';
            state.complete = true;
            return;
        }
        if (turn.playerActionReady(party))
        {
            const size_t member = party.activeMemberIndex();
            if (member == 2 && !state.cast)
            {
                GameplaySpellService &spells = runtime.spellService();
                const SpellEntry *pBolt = game.m_gameDataLoader.getSpellTable().findById(2);
                state.manaBeforeCast = party.members()[2].spellPoints;
                require(pBolt && runtime.tryCastSpellFromMember(2, 2, pBolt->name)
                    && session.gameplayScreenState().pendingSpellTarget().active,
                    "Normal caster turn did not open target selection");
                PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                request.targetActorIndex = state.target;
                require(spells.validatePendingTargetSelectionRequest(runtime, request), "Indoor target rejected");
                for (const auto &candidate : pScene->mapDeltaData()->actors)
                {
                    state.hpBeforeCast.push_back(candidate.hp);
                }
                const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                require(result.castResult.succeeded() && result.castResult.manaCost == 2
                    && party.members()[2].spellPoints == state.manaBeforeCast - 2,
                    "Normal indoor cast did not spend two mana");
                spells.clearPendingTargetSelection(runtime);
                session.updateGameplay({}, 0.0f);
                require(turn.hasPendingActions() && !turn.playerActionReady(party), "Indoor projectile not tracked");
                state.cast = true;
                std::cout << "Indoor turn cast: member=" << member << " pending=" << turn.pendingActions()
                    << " mana=" << state.manaBeforeCast << "->" << party.members()[2].spellPoints << '\n';
            }
            else
            {
                require(++state.passes <= 16, "Party pass actions did not advance the turn");
                press(KeyboardAction::Pass);
                std::cout << "Indoor turn pass: member=" << member << " next=" << party.activeMemberIndex()
                    << " stage=" << int(turn.stage()) << '\n';
            }
        }
    }

    struct HostileTurnCheckState
    {
        struct ObservedActor
        {
            uint32_t id = 0;
            int hp = 0;
            bool hostile = false;
        };
        size_t target = 0;
        unsigned casts = 0;
        unsigned passes = 0;
        uint32_t targetFrame = 0;
        uint32_t projectileCaptureFrame = 0;
        uint32_t finishedFrame = 0;
        int healthBefore = 0;
        std::vector<ObservedActor> actorsBeforeCast;
        unsigned interceptedCasts = 0;
        unsigned approachSteps = 0;
        float approachDistance = 0.0f;
        int manaBeforeCast = 0;
        int experienceBefore = 0;
        bool started = false;
        bool damaged = false;
        bool waitingForImpact = false;
        bool complete = false;
    };

    static void checkHostileTurns(GameApplication &game, uint32_t frame, int width, int height,
        HostileTurnCheckState &state)
    {
        const auto require = [](bool condition, const char *pMessage)
        {
            if (!condition)
            {
                throw std::runtime_error(pMessage);
            }
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
        require(game.m_pOutdoorWorldRuntime && runtime.party(), "Hostile check requires an outdoor party");
        OutdoorWorldRuntime &world = *game.m_pOutdoorWorldRuntime;
        Party &party = *runtime.party();
        require(party.members().size() == 1 && party.members().front().level == 1,
            "Hostile check needs the normal level-one solo caster");
        const Character &member = party.members().front();
        if (frame == 1)
        {
            require(equipExistingSlot(game, party, 0, EquipmentSlot::Armor),
                "Normal caster has no usable original armor");
        }
        const auto press = [&](KeyboardAction action)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.actions[keyboardActionIndex(action)].held = true;
            input.actions[keyboardActionIndex(action)].pressed = true;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto queued = [&](size_t index)
        {
            return std::any_of(turn.queue().begin(), turn.queue().end(), [index](const auto &entry)
            {
                return entry.kind == TurnBasedCombatRuntime::QueueEntry::Kind::Actor && entry.id == index;
            });
        };
        if (frame == 5)
        {
            float nearest = 600.0f;
            bool found = false;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                const auto *pActor = world.mapActorState(index);
                if (pActor && pActor->hostileToParty && !pActor->isDead && pActor->currentHp > 0)
                {
                    const float distance = std::hypot(pActor->preciseX - world.partyX(),
                        pActor->preciseY - world.partyY());
                    if (distance < nearest)
                    {
                        state.target = index;
                        nearest = distance;
                        found = true;
                    }
                }
            }
            require(found && member.knowsSpell(2) && member.spellPoints >= 4,
                "Input must contain a learned Fire Bolt caster naturally within 600 units of an enemy");
            state.healthBefore = member.health;
            state.experienceBefore = member.experience;
            press(KeyboardAction::Combat);
            require(turn.active() && queued(state.target), "Nearby hostile was not queued by actual application input");
            state.started = true;
            std::cout << "Hostile turn entered: actor=" << state.target << " distance=" << nearest
                << " actor_hp=" << world.mapActorState(state.target)->currentHp
                << " party_hp=" << member.health << " queue=" << turn.queue().size() << '\n';
        }
        if (!state.started)
        {
            return;
        }
        require(GameMechanics::canSelectInGameplay(member), "Normal caster fell during turn-based combat");
        if (!state.damaged && member.health < state.healthBefore)
        {
            state.damaged = true;
            std::cout << "Incoming damage in turn mode: " << state.healthBefore << "->" << member.health << '\n';
        }
        const auto *pActor = world.mapActorState(state.target);
        require(pActor != nullptr, "Original hostile actor disappeared");
        float distance = std::hypot(pActor->preciseX - world.partyX(), pActor->preciseY - world.partyY());
        if (frame % 120 == 0)
        {
            std::cout << "Hostile state: frame=" << frame << " stage=" << int(turn.stage())
                << " pending=" << turn.pendingActions() << " actor_hp=" << pActor->currentHp
                << " party_hp=" << member.health << " casts=" << state.casts << " distance=" << distance << '\n';
        }
        if (state.waitingForImpact)
        {
            if (world.turnBasedPendingWorldActionCount() != 0)
            {
                require(turn.hasPendingActions() && !turn.playerActionReady(party),
                    "Flying turn-based projectile did not block another player action");
                return;
            }
            require(state.actorsBeforeCast.size() == world.mapActorCount(),
                "Actor registry changed while observing a single Fire Bolt");
            size_t hitActor = size_t(-1);
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                const auto *pCandidate = world.mapActorState(index);
                require(pCandidate != nullptr && pCandidate->actorId == state.actorsBeforeCast[index].id,
                    "Actor identity changed during projectile observation");
                if (pCandidate->currentHp >= state.actorsBeforeCast[index].hp) continue;
                require(hitActor == size_t(-1), "Single-target Fire Bolt damaged multiple observed actors");
                require(state.actorsBeforeCast[index].hostile && state.actorsBeforeCast[index].hp > 0,
                    "Observed Fire Bolt damage was not on a live hostile");
                hitActor = index;
            }
            require(hitActor != size_t(-1), "Fire Bolt expired without observable actor damage");
            const auto *pHitActor = world.mapActorState(hitActor);
            std::cout << "Turn projectile impact: cast=" << state.casts << " selected_actor=" << state.target
                << " actual_actor=" << hitActor << " actor_id=" << pHitActor->actorId
                << " hp=" << state.actorsBeforeCast[hitActor].hp << "->" << pHitActor->currentHp
                << " mana=" << state.manaBeforeCast << "->" << member.spellPoints
                << " pending=" << turn.pendingActions() << '\n';
            if (hitActor != state.target)
            {
                // A normal projectile can hit a nearer actor before its selected target. Continue
                // fighting that actual blocker; do not redirect the projectile or alter actor state.
                ++state.interceptedCasts;
                state.target = hitActor;
                pActor = pHitActor;
                distance = std::hypot(pActor->preciseX - world.partyX(), pActor->preciseY - world.partyY());
            }
            state.waitingForImpact = false;
        }
        game.m_outdoorGameView.setCameraAngles(
            std::atan2(pActor->preciseY - world.partyY(), pActor->preciseX - world.partyX()), 0.0f);
        if (pActor->isDead)
        {
            require(state.casts > 0 && state.damaged && member.experience > state.experienceBefore,
                "Kill did not include normal incoming damage, a player cast and experience");
            require(!queued(state.target), "Defeated enemy remained in the application turn queue");
            if (distance <= 250.0f && !turn.hasPendingActions() && turn.canToggleOff(party))
            {
                press(KeyboardAction::Combat);
                require(!turn.active() && world.openMapActorCorpseView(state.target),
                    "Cannot return to realtime and open the nearby defeated enemy's corpse");
                const GameplayCorpseAutoLootResult loot = autoLootActiveCorpseView(
                    world, party, &game.m_gameDataLoader.getItemTable());
                require(!loot.blockedByInventory, "Normal inventory could not hold corpse loot");
                const int gold = party.gold();
                world.closeActiveCorpseView();
                if (world.openMapActorCorpseView(state.target))
                {
                    require(!autoLootActiveCorpseView(world, party, &game.m_gameDataLoader.getItemTable()).lootedAny
                        && party.gold() == gold, "Repeated corpse interaction duplicated loot");
                    world.closeActiveCorpseView();
                }
                state.complete = true;
                state.finishedFrame = frame;
                std::cout << "Hostile turn combat passed: casts=" << state.casts << " health=" << member.health
                    << " mana=" << member.spellPoints << " experience=" << member.experience
                    << " loot_gold=" << loot.goldAmount << " total_gold=" << party.gold()
                    << " interceptions=" << state.interceptedCasts << " approach_steps=" << state.approachSteps
                    << " approach_distance=" << state.approachDistance << '\n';
                return;
            }
        }
        if (turn.stage() == TurnBasedCombatStage::Movement)
        {
            if (distance > 210.0f)
            {
                const float beforeX = world.partyX();
                const float beforeY = world.partyY();
                const int beforePoints = turn.movementActionPoints();
                press(KeyboardAction::Forward);
                const float moved = std::hypot(world.partyX() - beforeX, world.partyY() - beforeY);
                require(turn.movementActionPoints() < beforePoints,
                    "Normal approach did not consume a turn movement step");
                ++state.approachSteps;
                state.approachDistance += moved;
                std::cout << "Turn approach: frame=" << frame << " from=" << beforeX << ',' << beforeY
                    << " to=" << world.partyX() << ',' << world.partyY() << " moved=" << moved
                    << " target_distance=" << distance << " points=" << beforePoints
                    << "->" << turn.movementActionPoints() << '\n';
            }
            else
            {
                press(KeyboardAction::Pass);
            }
            return;
        }
        GameplaySpellService &spells = runtime.spellService();
        if (session.gameplayScreenState().pendingSpellTarget().active)
        {
            require(member.spellPoints == state.manaBeforeCast, "Waiting for a target already consumed mana");
            if (frame < state.targetFrame + 5)
            {
                return;
            }
            PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
            request.targetActorIndex = state.target;
            require(spells.validatePendingTargetSelectionRequest(runtime, request), "Original hostile target rejected");
            state.actorsBeforeCast.clear();
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                const auto *pCandidate = world.mapActorState(index);
                require(pCandidate != nullptr, "Cannot snapshot actor HP before casting");
                state.actorsBeforeCast.push_back(
                    {pCandidate->actorId, pCandidate->currentHp, pCandidate->hostileToParty});
            }
            const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
            require(result.castResult.succeeded() && result.castResult.manaCost == 2
                && member.spellPoints == state.manaBeforeCast - 2 && world.turnBasedPendingWorldActionCount() > 0,
                "Actual turn-based Fire Bolt did not spend normal mana and spawn a pending projectile");
            spells.clearPendingTargetSelection(runtime);
            session.updateGameplay({}, 0.0f);
            require(turn.hasPendingActions() && !turn.playerActionReady(party), "Spawned projectile was not tracked");
            state.waitingForImpact = true;
            ++state.casts;
            if (state.casts == 1)
            {
                state.projectileCaptureFrame = frame + 1;
            }
            return;
        }
        if (turn.playerActionReady(party))
        {
            if (!state.damaged || pActor->currentHp <= 0)
            {
                require(++state.passes < 80, "Enemy turns did not progress to incoming damage or a corpse");
                press(KeyboardAction::Pass);
                return;
            }
            require(state.casts < 8, "Normal Fire Bolt casts did not defeat this enemy");
            const SpellEntry *pBolt = game.m_gameDataLoader.getSpellTable().findById(2);
            state.manaBeforeCast = member.spellPoints;
            require(pBolt && runtime.tryCastSpellFromMember(0, 2, pBolt->name)
                && session.gameplayScreenState().pendingSpellTarget().active,
                "Actual scene-adapter Fire Bolt did not enter target selection");
            state.targetFrame = frame;
        }
    }

    struct TurnCheckState
    {
        unsigned phase = 0;
        unsigned actions = 0;
        unsigned movementSteps = 0;
        uint32_t movementFrame = 0;
        uint32_t dialogFrame = 0;
        bool dialogClosed = false;
        float startGameMinutes = 0.0f;
        float movementX = 0.0f;
        float movementY = 0.0f;
        bool complete = false;
    };

    static void checkTurnBasedControls(GameApplication &game, uint32_t frame, int width, int height,
        TurnCheckState &state, std::vector<std::string> &frameCaptureRequests)
    {
        const auto require = [](bool condition, const char *pMessage)
        {
            if (!condition)
            {
                throw std::runtime_error(pMessage);
            }
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 1, "Turn check requires a normal solo caster");
        const auto press = [&](KeyboardAction action, float delta = 0.0f)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.actions[keyboardActionIndex(action)].held = true;
            input.actions[keyboardActionIndex(action)].pressed = true;
            session.updateGameplay(input, delta);
            session.updateGameplay({}, 0.0f);
        };
        if (frame == 35)
        {
            state.startGameMinutes = pWorld->gameMinutes();
            press(KeyboardAction::Combat);
            require(turn.active(), "Shared Combat input did not enter turn-based mode");
            std::cout << "Turn mode entered: queue=" << turn.queue().size() << '\n';
            state.phase = 1;
        }
        if (state.phase == 1 && turn.playerActionReady(*pParty))
        {
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            press(KeyboardAction::Forward, 0.05f);
            require(std::hypot(pWorld->partyX() - x, pWorld->partyY() - y) < 0.1f,
                "Attack phase allowed forward movement");
            const SpellEntry *pTorch = game.m_gameDataLoader.getSpellTable().findById(1);
            const Character &member = pParty->members().front();
            const int mana = member.spellPoints;
            require(pTorch && member.knowsSpell(1) && mana > 0, "Turn check requires learned Torch Light");
            require(runtime.tryCastSpellFromMember(0, 1, pTorch->name)
                && member.spellPoints == mana - 1 && pParty->hasPartyBuff(PartyBuffId::TorchLight),
                "Actual turn-based spell did not apply its cost and buff");
            if (turn.stage() == TurnBasedCombatStage::Attack && !turn.playerActionReady(*pParty))
            {
                require(!runtime.tryCastSpellFromMember(0, 1, pTorch->name) && member.spellPoints == mana - 1,
                    "A spell bypassed turn readiness");
            }
            std::cout << "Turn spell applied: mana=" << mana << "->" << member.spellPoints
                << " stage=" << int(turn.stage()) << '\n';
            frameCaptureRequests.push_back("turn-action");
            state.phase = 2;
        }
        else if (state.phase == 2)
        {
            if (turn.stage() == TurnBasedCombatStage::Movement)
            {
                require(turn.movementActionPoints() == 130, "Movement phase did not start with its action budget");
                state.movementFrame = frame;
                state.movementX = pWorld->partyX();
                state.movementY = pWorld->partyY();
                frameCaptureRequests.push_back("turn-movement");
                state.phase = 3;
            }
            else if (turn.playerActionReady(*pParty))
            {
                require(++state.actions <= 12, "Pass actions failed to advance the turn");
                press(KeyboardAction::Pass);
            }
        }
        else if (state.phase == 3)
        {
            if (frame <= state.movementFrame + 30)
            {
                require(turn.stage() == TurnBasedCombatStage::Movement && turn.movementActionPoints() == 130,
                    "Idle movement phase consumed its action budget");
            }
            else
            {
                press(KeyboardAction::Forward, 0.05f);
                ++state.movementSteps;
                if (state.movementSteps < 5)
                {
                    require(turn.stage() == TurnBasedCombatStage::Movement
                        && turn.movementActionPoints() == 130 - int(state.movementSteps) * 26,
                        "Forward input spent an incorrect number of movement points");
                }
                else
                {
                    const float moved = std::hypot(pWorld->partyX() - state.movementX,
                        pWorld->partyY() - state.movementY);
                    require(turn.stage() == TurnBasedCombatStage::Wait && moved > 1.0f,
                        "Five movement steps failed to move and finish the phase");
                    std::cout << "Movement passed: five steps, distance=" << moved << ", idle frames=30\n";
                    state.phase = 4;
                }
            }
        }
        else if (state.phase == 4 && runtime.activeEventDialog().isActive)
        {
            require(session.currentMapFileName() == "7out01.odm" && !state.dialogClosed,
                "Unexpected repeated dialogue interrupted the turn check");
            require(turn.stage() == TurnBasedCombatStage::Wait && !turn.hasPendingActions(),
                "Guide dialogue did not keep the wait phase paused");
            if (state.dialogFrame == 0)
            {
                state.dialogFrame = frame;
                frameCaptureRequests.push_back("turn-dialogue");
            }
            if (frame > state.dialogFrame + 20)
            {
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
                input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
                session.updateGameplay(input, 0.0f);
                session.updateGameplay({}, 0.0f);
                require(!runtime.activeEventDialog().isActive, "Shared Escape input did not close the guide dialogue");
                state.dialogClosed = true;
                std::cout << "Guide dialogue paused the turn for 20 frames and closed through shared input.\n";
            }
        }
        else if (state.phase == 4 && turn.playerActionReady(*pParty))
        {
            require(pWorld->gameMinutes() > state.startGameMinutes, "Completed turn did not advance game time");
            press(KeyboardAction::Combat);
            require(!turn.active(), "Shared Combat input did not return to realtime mode");
            require(session.currentMapFileName() != "7out01.odm" || state.dialogClosed,
                "MM7 route did not exercise the original guide dialogue");
            state.phase = 5;
            state.complete = true;
            std::cout << "Turn controls passed: second player turn, returned to realtime, game_minutes="
                << pWorld->gameMinutes() - state.startGameMinutes << '\n';
        }
    }

    static void checkSpellbookControls(GameApplication &game, uint32_t frame, int width, int height,
        uint32_t &targetFrame, bool &complete, std::vector<std::string> &frameCaptureRequests)
    {
        const auto require = [](bool condition, const char *pMessage)
        {
            if (!condition)
            {
                throw std::runtime_error(pMessage);
            }
        };
        GameplayScreenRuntime &runtime = game.m_gameSession.gameplayScreenRuntime();
        const Party *pParty = runtime.partyReadOnly();
        require(pParty && pParty->members().size() == 1, "Controls need the normal single caster");
        const Character &member = pParty->members().front();
        const SpellEntry *pBolt = game.m_gameDataLoader.getSpellTable().findById(2);
        require(pBolt && member.knowsSpell(1) && member.knowsSpell(2)
            && member.knowsSpell(12) && member.knowsSpell(23), "Controls require the normal three-school caster");
        const auto click = [&](const std::string &id, bool fromTouch = true, bool releaseOutside = false)
        {
            const auto *pLayout = runtime.findHudLayoutElement(id);
            require(pLayout != nullptr, "Missing authored text control");
            const auto rect = runtime.resolveHudLayoutElement(id, width, height, pLayout->width, pLayout->height);
            require(rect && rect->x >= 0 && rect->y >= 0 && rect->x + rect->width <= width
                && rect->y + rect->height <= height, "Text control escaped the rendered viewport");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = rect->x + rect->width * 0.5f;
            input.pointerY = rect->y + rect->height * 0.5f;
            input.pointerFromTouch = fromTouch;
            input.leftMouseButton = {true, true, false};
            GameplayPartyOverlayInputController::handleSpellbookOverlayInput(runtime, input);
            input.leftMouseButton = {false, false, true};
            if (releaseOutside)
            {
                input.pointerX = 0;
                input.pointerY = 0;
            }
            GameplayPartyOverlayInputController::handleSpellbookOverlayInput(runtime, input);
        };
        const std::string torch = spellbookSpellLayoutId(SpellbookSchool::Fire, 1);
        const std::string bolt = spellbookSpellLayoutId(SpellbookSchool::Fire, 2);
        if (frame == 60)
        {
            size_t checked = 0;
            for (const auto &[viewWidth, viewHeight] : {std::pair{640, 480}, std::pair{1210, 834},
                     std::pair{2420, 1660}, std::pair{1366, 1024}})
            {
                for (const SpellbookSchoolUiDefinition &school : spellbookSchoolUiDefinitions())
                {
                    std::vector<std::string> ids{"SpellbookCastButton", "SpellbookInspectButton",
                        "SpellbookQuickCastButton", "SpellbookAttackCastButton", "SpellbookCloseButton",
                        "SpellbookSelectLabel", "SpellbookCasterName", "SpellbookInstructions"};
                    for (const SpellbookSchoolUiDefinition &tab : spellbookSchoolUiDefinitions())
                    {
                        ids.emplace_back(tab.pButtonLayoutId);
                    }
                    for (uint32_t ordinal = 1; ordinal <= school.spellCount; ++ordinal)
                    {
                        ids.push_back(spellbookSpellLayoutId(school.school, ordinal));
                    }
                    std::vector<GameplayScreenRuntime::ResolvedHudLayoutElement> rects;
                    for (const std::string &id : ids)
                    {
                        const auto *pLayout = runtime.findHudLayoutElement(id);
                        require(pLayout && pLayout->primaryAsset.empty() && !pLayout->fontName.empty(),
                            "List presentation still requires icon artwork");
                        const auto rect = runtime.resolveHudLayoutElement(id, viewWidth, viewHeight,
                            pLayout->width, pLayout->height);
                        require(rect && rect->width > 0 && rect->height > 0 && rect->x >= 0 && rect->y >= 0
                            && rect->x + rect->width <= viewWidth + 0.01f
                            && rect->y + rect->height <= viewHeight + 0.01f, "Spell layout escaped viewport");
                        for (const auto &prior : rects)
                        {
                            require(rect->x + rect->width <= prior.x + 0.01f
                                || prior.x + prior.width <= rect->x + 0.01f
                                || rect->y + rect->height <= prior.y + 0.01f
                                || prior.y + prior.height <= rect->y + 0.01f, "Spell layout elements overlap");
                        }
                        rects.push_back(*rect);
                        ++checked;
                    }
                }
            }
            std::cout << "Spellbook layout passed: 12 schools, 4 viewport sizes, "
                << checked << " resolved rectangles.\n";
        }
        if (frame == 65)
        {
            click(torch, true, true);
            require(runtime.spellbookReadOnly().selectedSpellId == 0, "Release outside activated a spell row");
        }
        if (frame == 70)
        {
            click(torch);
            require(runtime.spellbookReadOnly().selectedSpellId == 1 && member.spellPoints == 32,
                "Single touch did not select without casting");
        }
        if (frame == 75)
        {
            click("SpellbookInspectButton");
            require(runtime.spellbookReadOnly().inspectSelected, "Touch inspect action did not persist");
        }
        if (frame == 85)
        {
            const auto &inspect = runtime.spellInspectOverlayReadOnly();
            require(inspect.active && inspect.spellId == 1 && !inspect.body.empty(),
                "Selected spell detail did not reach the formal overlay");
            frameCaptureRequests.push_back("spell-details");
        }
        if (frame == 90)
        {
            click("SpellbookCastButton");
            require(!runtime.spellbookReadOnly().inspectSelected && runtime.spellbookReadOnly().active
                && member.spellPoints == 32, "Dismissing details activated the button underneath");
        }
        if (frame == 95)
        {
            click(bolt, false);
            require(runtime.spellbookReadOnly().selectedSpellId == 2, "Mouse did not select the second learned spell");
        }
        if (frame == 100)
        {
            click("SpellbookQuickCastButton");
            require(member.quickSpellName == pBolt->name, "Quick-spell binding failed");
        }
        if (frame == 105)
        {
            click("SpellbookQuickCastButton");
            require(member.quickSpellName.empty(), "Quick-spell toggle did not clear binding");
            click(bolt);
            click("SpellbookQuickCastButton");
            click(bolt);
            require(runtime.spellbookReadOnly().active && runtime.spellbookReadOnly().selectedSpellId == 2
                && !game.m_gameSession.gameplayScreenState().pendingSpellTarget().active,
                "Intervening button left a stale double-click that cast the spell");
        }
        if (frame == 115)
        {
            click("SpellbookAttackCastButton");
            require(member.attackSpellName == pBolt->name, "Attack-spell binding failed");
        }
        if (frame == 125 || frame == 130)
        {
            const bool air = frame == 125;
            const SpellbookSchool school = air ? SpellbookSchool::Air : SpellbookSchool::Water;
            click(air ? "SpellbookSchoolButtonAir" : "SpellbookSchoolButtonWater");
            require(runtime.spellbookReadOnly().school == school, "Learned school did not switch");
            click(spellbookSpellLayoutId(school, 1));
            require(runtime.spellbookReadOnly().selectedSpellId == (air ? 12 : 23), "School selected wrong spell ID");
        }
        if (frame == 135)
        {
            click("SpellbookSchoolButtonFire");
            click(spellbookSpellLayoutId(SpellbookSchool::Fire, 3));
            click("SpellbookSchoolButtonDark");
            require(runtime.spellbookReadOnly().selectedSpellId == 0
                && runtime.spellbookReadOnly().school == SpellbookSchool::Fire,
                "Unlearned spell/school was selectable");
        }
        if (frame == 140)
        {
            click(torch);
        }
        if (frame == 145)
        {
            click("SpellbookCastButton");
            require(!runtime.spellbookReadOnly().active && member.spellPoints == 31,
                "Touch Cast did not use the normal scene-adapter spell path");
        }
        if (frame == 150)
        {
            runtime.openSpellbookOverlay();
            click(torch);
            click("SpellbookCastButton");
            require(runtime.spellbookReadOnly().active && member.spellPoints == 31,
                "Recovering caster spent mana or lost the open spellbook");
            click("SpellbookCloseButton");
        }
        if (frame >= 230 && targetFrame == 0 && GameMechanics::canTakeGameplayAction(member))
        {
            runtime.openSpellbookOverlay();
            click(bolt, false);
            click("SpellbookCastButton", false);
            require(!runtime.spellbookReadOnly().active && member.spellPoints == 31
                && game.m_gameSession.gameplayScreenState().pendingSpellTarget().active,
                "Mouse Cast did not enter target selection without premature mana cost");
            targetFrame = frame;
        }
        if (targetFrame != 0 && frame == targetFrame + 10)
        {
            frameCaptureRequests.push_back("spell-targeting");
        }
        if (targetFrame != 0 && frame == targetFrame + 20)
        {
            runtime.spellService().clearPendingTargetSelection(runtime);
            runtime.openSpellbookOverlay();
            click("SpellbookCloseButton");
            require(!runtime.spellbookReadOnly().active && member.quickSpellName == pBolt->name
                && member.attackSpellName == pBolt->name, "Close lost spell bindings");
            complete = true;
            std::cout << "Spellbook controls passed: touch/mouse frames, release outside, details/dismiss, "
                << "quick toggle, attack binding, schools, unknown guards, normal cast, recovery and targeting.\n";
        }
    }

    struct Mm8ReplayState
    {
        size_t stage = 0;
        uint32_t firstFrame = 1;
        bool complete = false;
        bool fromNewGame = true;
        float walkedDistance = 0;
        Mm8StartCheckState start;
        Mm8StartCheckState inn;
        Mm8ShopCheckState shop;
        Mm8PortalCheckState portal;
    };

    static constexpr std::array<const char *, 16> mm8ReplayStages = {"00-crystal", "01-inn", "02-shop",
        "03-portals", "04-simon", "05-dungeon", "06-loot", "07-return", "08-resupply", "09-revisit",
        "10-chamber", "11-stairs", "12-passage", "13-floor-puzzle", "14-exit", "15-delivery"};

    static void checkMm8Replay(GameApplication &game, uint32_t frame, int width, int height,
        const std::filesystem::path &output, Mm8ReplayState &state, std::vector<std::string> &captures)
    {
        if (state.stage >= mm8ReplayStages.size()) throw std::runtime_error("Invalid full replay stage");
        const std::string name = mm8ReplayStages[state.stage];
        const uint32_t localFrame = frame - state.firstFrame + 1;
        if (localFrame == 1)
        {
            std::filesystem::create_directories(output / name);
            std::filesystem::current_path(output / name);
            const Party &party = *game.m_gameSession.gameplayScreenRuntime().party();
            std::cout << "Replay stage begin: name=" << name << " global_frame=" << frame
                << " map=" << game.m_gameSession.currentMapFileName() << " members=" << party.members().size()
                << " gold=" << party.gold() << '\n';
        }
        Mm8StartCheckState *pCheck = &state.portal;
        switch (state.stage)
        {
        case 0:
            pCheck = &state.start;
            checkMm8Start(game, localFrame, width, height, false, state.start, captures);
            break;
        case 1:
            pCheck = &state.inn;
            checkMm8Inn(game, localFrame, width, height, false, state.inn, captures);
            break;
        case 2:
            pCheck = &state.shop;
            checkMm8Shop(game, localFrame, width, height, false, state.shop, captures, false);
            break;
        case 3:
            checkMm8Portals(game, localFrame, width, height, state.portal, captures, true);
            break;
        case 4: checkMm8Temple(game, localFrame, width, height, state.portal, captures); break;
        case 5:
        case 6:
            checkMm8Dungeon(game, localFrame, width, height, state.portal, captures, false, true);
            break;
        case 7: checkMm8Return(game, localFrame, width, height, state.portal, captures); break;
        case 8: checkMm8Resupply(game, localFrame, width, height, state.portal, captures); break;
        case 9: checkMm8Revisit(game, localFrame, width, height, state.portal, captures); break;
        case 10:
            checkMm8Dungeon(game, localFrame, width, height, state.portal, captures, true, true);
            break;
        case 11:
            checkMm8TempleMechanisms(game, localFrame, width, height, state.portal, captures,
                Mm8TempleMechanism::Stairs, true);
            break;
        case 12:
            checkMm8TempleMechanisms(game, localFrame, width, height, state.portal, captures,
                Mm8TempleMechanism::Passage, true);
            break;
        case 13:
            checkMm8TempleMechanisms(game, localFrame, width, height, state.portal, captures,
                Mm8TempleMechanism::FloorPuzzle, true);
            break;
        case 14: checkMm8TempleExit(game, localFrame, width, height, state.portal, captures); break;
        case 15: checkMm8Delivery(game, localFrame, width, height, state.portal, captures); break;
        }
        for (std::string &capture : captures) capture = name + "/" + capture;
        if (!pCheck->complete) return;
        if (!game.quickSaveToPath(output / name / "stage-complete.oysav", "MM8 完整开局阶段记录", {}))
            throw std::runtime_error("Cannot preserve a completed stage of the normal new-game replay");
        state.walkedDistance += pCheck->walkedDistance;
        std::cout << "Replay stage complete: name=" << name << " global_frame=" << frame
            << " walked_total=" << state.walkedDistance << '\n';
        ++state.stage;
        state.firstFrame = frame + 1;
        state.portal = {};
        if (state.stage == mm8ReplayStages.size())
        {
            state.complete = true;
            const char *pResult = state.fromNewGame ? "full-starter-complete" : "starter-continuation-complete";
            if (!game.quickSaveToPath(output / (std::string(pResult) + ".oysav"), "MM8 正常开局路线", {}))
                throw std::runtime_error("Cannot save completed normal starter route");
            captures.push_back(pResult);
        }
    }

    struct Mm6TrainingCheckState : Mm8StartCheckState
    {
        unsigned training = 0;
        int expectedGold = 0;
        int originalFood = 0;
        float previousMinutes = 0.0f;
        std::array<uint32_t, 4> startingSkillPoints = {};
    };

    static void checkMm6Training(GameApplication &game, uint32_t frame, int width, int height,
        Mm6TrainingCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool condition, const std::string &message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && session.currentMapFileName() == "outd3.odm"
            && pParty->members().size() == 4 && pParty->hasActableMember(),
            "MM6 training needs the surviving four-member Ironfist party");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM6 原训练馆升级记录", {}), "Cannot preserve normal MM6 training");
        };
        const auto next = [&](unsigned phase) { state.phase = phase; state.phaseFrame = frame; };
        if (frame == 1)
        {
            state.expectedGold = pParty->gold();
            state.originalFood = pParty->food();
            require(state.expectedGold == 6175 && pParty->hasQuestBit(1110)
                && !pParty->hasQuestBit(1106) && pParty->inventoryItemCount(2125) == 0,
                "MM6 training input did not preserve the actual completed letter quest");
            for (size_t i = 0; i < pParty->members().size(); ++i)
            {
                const Character &member = pParty->members()[i];
                require(member.level == 1 && member.experience == 3000 && member.awards.contains(100)
                    && !member.hasSkill("Merchant"), "MM6 training input is not the original rewarded starter party");
                state.startingSkillPoints[i] = member.skillPoints;
            }
        }
        if (state.phase == 0)
        {
            // The trainer door is recessed in the east courtyard of the original H-shaped building.
            constexpr std::array<std::array<float, 2>, 5> route = {{{17132.8f, 15680.0f},
                {17132.8f, 13700.0f}, {17530.0f, 13700.0f}, {17530.0f, 12820.0f}, {17180.0f, 12820.0f}}};
            const float x = pWorld->partyX();
            const float y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x;
            const float dy = route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            if (distance <= 25.0f)
            {
                save("mm6-training-waypoint-" + std::to_string(state.waypoint));
                std::cout << "MM6 training waypoint: index=" << state.waypoint << " position=" << x << ',' << y
                    << ',' << pWorld->partyFootZ() << '\n';
                if (++state.waypoint == route.size()) next(1);
                state.stalledFrames = 0;
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward, distance < 60 ? 0.025f : 0.05f);
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (state.stalledFrames == 60 || frame % 120 == 0)
                std::cout << "MM6 training walk: waypoint=" << state.waypoint << " position=" << pWorld->partyX()
                    << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ() << " remaining=" << distance << '\n';
            require(state.stalledFrames < 90, "Normal MM6 training walk is blocked");
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            game.m_outdoorGameView.setCameraAngles(-1.5707963f, -0.2175f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 1589,
                "Normal interaction did not open the original Ironfist training hall");
            save("mm6-training-arrived");
            captures.push_back("mm6-training-arrived");
            next(3);
            break;
        case 3:
        {
            const size_t memberIndex = state.training % 4;
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
            require(portrait.has_value(), "Training hall has no visible student portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton.held = true;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(pParty->activeMemberIndex() == memberIndex && !runtime.characterScreen().open,
                "Portrait input did not select the student");
            next(4);
            break;
        }
        case 4:
        {
            const size_t memberIndex = state.training % 4;
            const uint32_t priorLevel = 1 + state.training / 4;
            const HouseEntry *pHouse = runtime.houseTable()->get(1589);
            require(pHouse != nullptr && pParty->activeMember()->level == priorLevel,
                "Original training hall or expected student level is absent");
            const int reputation = effectivePartyReputation(pWorld->currentLocationReputation(),
                pWorld->eventRuntimeState());
            const int price = PriceCalculator::trainingPrice(pParty->activeMember(), *pHouse, reputation);
            require(price > 0 && pParty->gold() >= price, "Original party cannot afford the training fee");
            state.previousMinutes = pWorld->gameMinutes();
            save("mm6-training-before-" + std::to_string(state.training));
            selectQuestDialog(runtime, EventDialogActionKind::HouseService,
                uint32_t(HouseActionId::TrainingTrainActiveMember));
            state.expectedGold -= price;
            require(pParty->gold() == state.expectedGold && pParty->food() == state.originalFood,
                "Training fee or food changed unexpectedly");
            for (size_t i = 0; i < pParty->members().size(); ++i)
            {
                const Character &member = pParty->members()[i];
                const uint32_t trained = state.training / 4 + (i <= memberIndex ? 1 : 0);
                require(member.level == 1 + trained && member.experience == 3000
                    && member.skillPoints == state.startingSkillPoints[i] + 5 * trained
                    && member.awards.contains(100) && member.conditions.none(),
                    "Training changed another student or did not grant the original level and skill points");
            }
            const float elapsed = pWorld->gameMinutes() - state.previousMinutes;
            require(memberIndex == 0 ? elapsed >= 7 * 1440 && elapsed <= 9 * 1440 : std::abs(elapsed) < 0.01f,
                "Training time did not follow one weekly advance per party training round");
            std::cout << "MM6 normal training: index=" << state.training << " member=" << memberIndex
                << " level=" << pParty->activeMember()->level << " price=" << price
                << " gold=" << pParty->gold() << " elapsed_minutes=" << elapsed
                << " game_minutes=" << pWorld->gameMinutes() << '\n';
            save("mm6-training-after-" + std::to_string(state.training));
            captures.push_back("mm6-training-after-" + std::to_string(state.training));
            ++state.training;
            next(state.training == 8 ? 5 : 3);
            break;
        }
        case 5:
        {
            const auto &actions = runtime.activeEventDialog().actions;
            const auto train = std::find_if(actions.begin(), actions.end(), [](const EventDialogAction &action)
            {
                return action.kind == EventDialogActionKind::HouseService
                    && action.id == uint32_t(HouseActionId::TrainingTrainActiveMember);
            });
            require(train != actions.end() && !train->enabled,
                "Training to level four must be unavailable with only 3000 experience");
            save("mm6-training-complete");
            captures.push_back("mm6-training-complete");
            state.complete = true;
            std::cout << "MM6 training passed: four normal level-three members, eight paid lessons, "
                << "unchanged quest XP, gold=" << pParty->gold() << '\n';
            break;
        }
        default: throw std::runtime_error("Invalid MM6 training phase");
        }
    }

    struct Mm6ReturnCheckState : Mm8StartCheckState
    {
        size_t preparationStep = 0;
        unsigned streetDialogs = 0;
        int expectedGold = 0;
        int foodPrice = 0;
        float departureMinutes = 0.0f;
        std::vector<std::array<float, 2>> ironfistRoute;
        std::vector<std::array<float, 2>> townRoute;
    };

    static void checkMm6Return(GameApplication &game, uint32_t frame, int width, int height,
        Mm6ReturnCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool condition, const std::string &message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember(),
            "MM6 return needs the original surviving four-member party");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM6 正常步行返乡补给", {}), "Cannot preserve MM6 return state");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.stalledFrames = 0;
            state.waypoint = 0;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        constexpr std::array<std::array<uint32_t, 3>, 11> preparation = {{{0, 84, 0}, {0, 1, 0},
            {1, 66, 0}, {1, 99, 0}, {1, 467, 68}, {2, 84, 0}, {2, 21, 0}, {2, 401, 2},
            {3, 1, 0}, {3, 56, 0}, {3, 412, 13}}};
        const auto prepared = [&]()
        {
            return pParty->members()[0].equipment.mainHand == 1 && pParty->members()[0].equipment.armor == 84
                && pParty->members()[1].knowsSpell(68) && pParty->members()[2].knowsSpell(2)
                && pParty->members()[3].equipment.bow == 56 && pParty->members()[3].knowsSpell(13);
        };
        if (frame == 1)
        {
            require(pParty->gold() == 5815 && pParty->hasQuestBit(1110) && !pParty->hasQuestBit(1106)
                && pParty->inventoryItemCount(2125) == 0
                && std::all_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                    { return member.level == 3 && member.experience == 3000 && member.awards.contains(100); }),
                "MM6 return input did not preserve the original letter rewards and training");
            state.expectedGold = pParty->gold();
            const auto readRoute = [&](const char *name)
            {
                const auto path = std::filesystem::current_path().parent_path() / name;
                std::ifstream stream(path);
                require(stream.good(), "Missing MM6 return route: " + path.string());
                std::vector<std::array<float, 2>> route;
                std::string line;
                while (std::getline(stream, line))
                {
                    if (line.empty() || line.front() == '#') continue;
                    std::istringstream values(line);
                    float x = 0, y = 0, z = 0;
                    require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                        && std::isfinite(z), "Invalid MM6 return waypoint");
                    route.push_back({x, y});
                    require(route.size() <= 2000, "MM6 return has too many waypoints");
                }
                require(route.size() >= 2, "MM6 return route is empty");
                return route;
            };
            state.ironfistRoute = readRoute("mm6-return-ironfist.tsv");
            state.townRoute = readRoute("mm6-return-town.tsv");
            if (session.currentMapFileName() == "oute3.odm")
            {
                require(prepared() && pParty->food() == 0, "Return continuation is not the original overland arrival");
                next(6);
            }
            else
            {
                require(session.currentMapFileName() == "outd3.odm" && pParty->food() == 5,
                    "MM6 return must start in Ironfist with the original provisions");
                if (prepared()) next(3);
            }
        }
        if (state.phase == 3 || state.phase == 6)
        {
            const bool ironfist = state.phase == 3;
            require(session.currentMapFileName() == (ironfist ? "outd3.odm" : "oute3.odm"),
                "MM6 return walk changed maps before normal travel confirmation");
            const auto &dialog = runtime.activeEventDialog();
            if (dialog.isActive)
            {
                if (ironfist && dialog.presentation == EventDialogPresentation::Transition)
                {
                    require(std::any_of(dialog.lines.begin(), dialog.lines.end(), [](const std::string &line)
                    {
                        return line.starts_with("It will take 5 days to travel to ");
                    }), "MM6 normal overland prompt omitted the five-day duration");
                    for (const std::string &line : dialog.lines)
                        std::cout << "MM6 return travel prompt: " << line << '\n';
                    save("mm6-return-travel-prompt");
                    captures.push_back("mm6-return-travel-prompt");
                    next(4);
                    return;
                }
                require(!dialog.isHouseDialog && dialog.presentation != EventDialogPresentation::Transition,
                    "Unexpected building/transition during MM6 return walk");
                require(++state.streetDialogs <= 30, "MM6 return repeatedly reopened a street conversation");
                save("mm6-return-street-" + std::to_string(state.streetDialogs));
                close();
                return;
            }
            const auto &route = ironfist ? state.ironfistRoute : state.townRoute;
            require(state.waypoint < route.size(), "MM6 return exhausted the route before the boundary prompt");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = route[state.waypoint][0] - x, dy = route[state.waypoint][1] - y;
            const float remaining = std::hypot(dx, dy);
            if (remaining < 5.0f)
            {
                if (state.waypoint % 10 == 0 || state.waypoint + 1 == route.size())
                    save("mm6-return-walk-" + std::to_string(state.phase) + '-' + std::to_string(state.waypoint));
                std::cout << "MM6 return waypoint: phase=" << state.phase << " index=" << state.waypoint
                    << " position=" << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                if (++state.waypoint == route.size() && !ironfist) next(7);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(remaining / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 120 == 0 || state.stalledFrames == 30)
                std::cout << "MM6 return walk: frame=" << frame << " phase=" << state.phase
                    << " waypoint=" << state.waypoint << " position=" << pWorld->partyX() << ','
                    << pWorld->partyY() << ',' << pWorld->partyFootZ() << " remaining=" << remaining << '\n';
            require(state.stalledFrames < 90, "MM6 return normal walking stopped at geometry or an actor");
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 0:
        {
            const size_t memberIndex = preparation[state.preparationStep][0];
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
            require(portrait.has_value(), "MM6 preparation has no student portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned click = 0; click < 2; ++click)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == memberIndex,
                "MM6 normal portrait clicks did not open the student's inventory");
            next(1);
            break;
        }
        case 1:
        {
            const auto action = preparation[state.preparationStep];
            const int before = pParty->inventoryItemCount(action[1]);
            equipQuestInventoryItem(session, width, height, action[1], action[0]);
            require(action[2] == 0 || (pParty->members()[action[0]].knowsSpell(action[2])
                && pParty->inventoryItemCount(action[1]) == before - 1),
                "MM6 original spell book did not teach its spell and consume one copy");
            save("mm6-return-preparation-" + std::to_string(state.preparationStep));
            captures.push_back("mm6-return-preparation-" + std::to_string(state.preparationStep));
            next(2);
            break;
        }
        case 2:
            close();
            require(!runtime.characterScreen().open, "MM6 preparation did not close its inventory");
            if (++state.preparationStep == preparation.size())
            {
                require(prepared() && pParty->gold() == 5815 && pParty->food() == 5,
                    "MM6 original equipment and book preparation is incomplete");
                save("mm6-return-prepared");
                next(3);
            }
            else next(0);
            break;
        case 4:
            require(runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "MM6 overland travel confirmation disappeared");
            state.departureMinutes = pWorld->gameMinutes();
            save("mm6-return-before-travel");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            require(pParty->food() == 0 && pParty->gold() == state.expectedGold
                && std::abs(pWorld->gameMinutes() - state.departureMinutes - 7200.0f) < 0.01f,
                "MM6 overland travel did not consume five provisions and advance exactly five days");
            next(5);
            break;
        case 5:
            require(session.currentMapFileName() == "oute3.odm" && pParty->food() == 0
                && pParty->gold() == state.expectedGold && pParty->hasQuestBit(1110)
                && !pParty->hasQuestBit(1106) && pParty->inventoryItemCount(2125) == 0,
                "MM6 normal overland travel lost the trained party or completed letter quest");
            save("mm6-return-arrived");
            captures.push_back("mm6-return-arrived");
            next(6);
            break;
        case 7:
        case 11:
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(state.phase == 7 ? 8 : 12);
            break;
        case 8:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                "MM6 return did not reach the original Lonely Knight tavern");
            selectQuestDialog(runtime, EventDialogActionKind::HouseProprietor, 260);
            save("mm6-return-tavern");
            captures.push_back("mm6-return-tavern");
            next(9);
            break;
        case 9:
        {
            const HouseEntry *pHouse = runtime.houseTable()->get(260);
            require(pHouse != nullptr, "Original return tavern is absent");
            const int reputation = effectivePartyReputation(pWorld->currentLocationReputation(),
                pWorld->eventRuntimeState());
            state.foodPrice = PriceCalculator::tavernFoodPrice(pParty->activeMember(), *pHouse, reputation);
            save("mm6-return-before-food");
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TavernBuyFood));
            state.expectedGold -= state.foodPrice;
            require(pParty->gold() == state.expectedGold && pParty->food() == 14,
                "Original New Sorpigal tavern did not sell fourteen provisions for the quoted fee");
            save("mm6-return-provisions");
            captures.push_back("mm6-return-provisions");
            next(10);
            break;
        }
        case 10:
            if (runtime.activeEventDialog().isActive) close();
            else next(11);
            break;
        case 12:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                "MM6 old tavern did not reopen");
            selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 786);
            next(13);
            break;
        case 13:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 1315);
            save("mm6-return-old-letter-topic");
            captures.push_back("mm6-return-old-letter-topic");
            next(14);
            break;
        case 14:
            require(pParty->gold() == state.expectedGold && pParty->food() == 14 && pParty->hasQuestBit(1110)
                && !pParty->hasQuestBit(1105) && !pParty->hasQuestBit(1106)
                && pParty->inventoryItemCount(2125) == 0
                && std::all_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                    { return member.level == 3 && member.experience == 3000 && member.awards.contains(100); }),
                "Returning to the old tavern repeated a completed quest or lost the original party");
            save("mm6-return-complete");
            state.complete = true;
            std::cout << "MM6 return passed: original equipment/books, five-day overland journey, "
                << "tavern food and old quest persistence; gold=" << pParty->gold()
                << " food=" << pParty->food() << '\n';
            break;
        default: throw std::runtime_error("Invalid MM6 return phase");
        }
    }

    struct Mm6SkillsCheckState : Mm8StartCheckState
    {
        size_t memberIndex = 0;
        unsigned upgrades = 0;
    };

    static constexpr std::array<const char *, 4> mm6StarterSkills = {"Sword", "BodyMagic", "FireMagic", "Bow"};

    static void checkMm6Skills(GameApplication &game, uint32_t frame, int width, int height,
        Mm6SkillsCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool condition, const std::string &message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        require(pParty && pParty->members().size() == 4 && pParty->gold() == 5813 && pParty->food() == 14
            && session.currentMapFileName() == "oute3.odm" && pParty->hasQuestBit(1110),
            "Skill spending requires the actual returned MM6 party and provisions");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM6 正常技能点分配", {}), "Cannot preserve normal skill spending");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
        };
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto resolve = [&](const char *id)
        {
            const auto *pLayout = runtime.findHudLayoutElement(id);
            require(pLayout != nullptr, std::string("Missing character control: ") + id);
            const auto rect = runtime.resolveHudLayoutElement(id, width, height, pLayout->width, pLayout->height);
            require(rect.has_value(), std::string("Cannot place character control: ") + id);
            return *rect;
        };
        const auto clickSkill = [&]()
        {
            require(runtime.characterScreen().open
                && runtime.characterScreen().page == GameplayUiController::CharacterPage::Skills
                && runtime.selectedCharacterScreenSourceIndex() == state.memberIndex,
                "Skill click needs the selected character's visible skill page");
            const CharacterSkillUiData rows = buildCharacterSkillUiData(
                pParty->member(state.memberIndex), runtime.characterInspectTable());
            const auto font = runtime.findHudFont("Lucida");
            const float naturalHeight = font ? float(std::max(1, font->fontHeight - 3)) : 11.0f;
            const CharacterSkillLayout layout = layoutCharacterSkillRows(runtime, width, height, naturalHeight,
                {rows.weaponRows.size(), rows.magicRows.size(), rows.armorRows.size(), rows.miscRows.size()});
            const bool magic = state.memberIndex == 1 || state.memberIndex == 2;
            const auto &group = magic ? rows.magicRows : rows.weaponRows;
            const auto row = std::find_if(group.begin(), group.end(), [&](const CharacterSkillUiRow &entry)
                { return entry.canonicalName == mm6StarterSkills[state.memberIndex]; });
            require(row != group.end(), "Original starter skill is absent from the visible skill group");
            const auto region = resolve(magic ? "CharacterSkillsMagicListRegion" : "CharacterSkillsWeaponsListRegion");
            const auto level = resolve(magic ? "CharacterSkillsMagicLevelHeader" : "CharacterSkillsWeaponsLevelHeader");
            const float rowHeight = layout.rowHeight * region.scale;
            GameplayResolvedHudLayoutElement rect = {region.x, region.y + float(row - group.begin()) * rowHeight,
                level.x + level.width - region.x, rowHeight, region.scale};
            require(layout.placeVisible(rect), "Starter skill row is outside the actual scroll viewport");
            click(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
        };
        if (frame == 1)
        {
            for (size_t index = 0; index < pParty->members().size(); ++index)
            {
                const Character &member = pParty->members()[index];
                const CharacterSkill *pSkill = member.findSkillByCanonicalName(mm6StarterSkills[index]);
                require(member.level == 3 && member.experience == 3000 && member.skillPoints == 10
                    && pSkill && pSkill->level == 1, "Skill input is not the normally earned training points");
            }
            save("mm6-skills-before");
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 0:
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, state.memberIndex);
            require(portrait.has_value(), "Skill student has no actual portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned press = 0; press < 2; ++press)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.leftMouseButton = {};
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == state.memberIndex,
                "Normal portrait clicks did not open the correct character");
            next(1);
            break;
        }
        case 1:
        {
            const auto rect = resolve("CharacterSkillsButton");
            click(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
            require(runtime.characterScreen().page == GameplayUiController::CharacterPage::Skills,
                "Normal skills tab click failed");
            next(2);
            break;
        }
        case 2:
            captures.push_back("mm6-skills-before-" + std::to_string(state.memberIndex));
            next(3);
            break;
        case 3:
        {
            const unsigned beforeLevel = 1 + state.upgrades;
            const Character &before = pParty->members()[state.memberIndex];
            const uint32_t points = before.skillPoints;
            require(before.findSkillByCanonicalName(mm6StarterSkills[state.memberIndex])->level == beforeLevel,
                "Unexpected skill level before normal click");
            clickSkill();
            const Character &after = pParty->members()[state.memberIndex];
            require(after.findSkillByCanonicalName(mm6StarterSkills[state.memberIndex])->level == beforeLevel + 1
                && after.skillPoints + beforeLevel + 1 == points,
                "Skill click did not charge the next level's normal point cost exactly once");
            std::cout << "MM6 normal skill upgrade: member=" << state.memberIndex
                << " skill=" << mm6StarterSkills[state.memberIndex] << " level=" << beforeLevel + 1
                << " points=" << points << "->" << after.skillPoints << '\n';
            save("mm6-skill-" + std::to_string(state.memberIndex) + '-' + std::to_string(beforeLevel + 1));
            next(++state.upgrades == 3 ? 4 : 3);
            break;
        }
        case 4:
        {
            clickSkill();
            const Character &member = pParty->members()[state.memberIndex];
            require(member.skillPoints == 1
                && member.findSkillByCanonicalName(mm6StarterSkills[state.memberIndex])->level == 4,
                "Insufficient points still upgraded a skill or spent the remaining point");
            save("mm6-skills-finished-" + std::to_string(state.memberIndex));
            captures.push_back("mm6-skills-finished-" + std::to_string(state.memberIndex));
            next(5);
            break;
        }
        case 5:
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(!runtime.characterScreen().open, "Normal close did not leave the skills page");
            if (++state.memberIndex == 4)
            {
                save("mm6-skills-complete");
                state.complete = true;
                std::cout << "MM6 normal skill spending passed: twelve upgrades, nine points each, "
                    << "four insufficient-point clicks preserved the remaining point\n";
            }
            else
            {
                state.upgrades = 0;
                next(0);
            }
            break;
        }
        default: throw std::runtime_error("Invalid MM6 skill spending phase");
        }
    }

    struct Mm6TempleEntryCheckState : Mm8StartCheckState
    {
        std::vector<std::array<float, 2>> route;
        unsigned streetDialogs = 0;
        float entryMinutes = 0.0f;
        std::set<size_t> encounteredActors;
        std::set<size_t> defeatedActors;
        unsigned attackPresses = 0;
        unsigned heals = 0;
        bool combatCaptured = false;
        uint32_t lastCombatProgressFrame = 0;
        size_t lastTarget = size_t(-1);
        int lastTargetHp = 0;
    };

    static void checkMm6TempleEntry(GameApplication &game, uint32_t frame, int width, int height,
        Mm6TempleEntryCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool condition, const std::string &message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember()
            && pParty->gold() == 5813 && pParty->food() == 14 && pParty->hasQuestBit(1110),
            "MM6 temple approach lost the normal returned party or provisions");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM6 烛台任务与正常废庙入口", {}), "Cannot preserve the original temple approach");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.stalledFrames = 0;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto guardRoute = [&]()
        {
            OutdoorWorldRuntime *pOutdoor = game.m_pOutdoorWorldRuntime.get();
            require(pOutdoor != nullptr, "Temple road combat requires its actual outdoor world");
            TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
            uint64_t totalExperience = 0;
            for (const Character &member : pParty->members()) totalExperience += member.experience;
            for (size_t index : state.encounteredActors)
            {
                const auto *pActor = pOutdoor->mapActorState(index);
                require(pActor != nullptr, "Observed temple road enemy disappeared");
                if (pActor->isDead && state.defeatedActors.insert(index).second)
                {
                    require(state.attackPresses > 0 && totalExperience > 12000,
                        "Temple road enemy died without a normal attack and experience award");
                    std::cout << "MM6 temple road defeated: actor=" << index << " total_xp=" << totalExperience << '\n';
                    save("mm6-temple-road-kill-" + std::to_string(state.defeatedActors.size()));
                    if (state.defeatedActors.size() == 1) captures.push_back("mm6-temple-road-first-kill");
                }
            }
            size_t target = size_t(-1);
            float nearest = 1200.0f;
            for (size_t index = 0; index < pOutdoor->mapActorCount(); ++index)
            {
                const auto *pActor = pOutdoor->mapActorState(index);
                if (!pActor || !pActor->hostileToParty || pActor->isDead || pActor->isInvisible
                    || pActor->currentHp <= 0) continue;
                const float distance = std::hypot(pActor->preciseX - pWorld->partyX(),
                    pActor->preciseY - pWorld->partyY());
                if (distance < nearest && pOutdoor->partyAttackActorHasLineOfSight(index))
                {
                    target = index;
                    nearest = distance;
                }
            }
            const auto press = [&](KeyboardAction action) { pressQuestGameplay(session, width, height, action); };
            if (target == size_t(-1))
            {
                state.lastCombatProgressFrame = frame;
                if (!turn.active()) return false;
                if (turn.canToggleOff(*pParty)) press(KeyboardAction::Combat);
                else if (turn.stage() == TurnBasedCombatStage::Movement || turn.playerActionReady(*pParty))
                    press(KeyboardAction::Pass);
                return true;
            }
            state.encounteredActors.insert(target);
            const auto *pActor = pOutdoor->mapActorState(target);
            const auto &movement = game.m_activeWorldManifest.partyMovement;
            const float eyeHeight = movement.declared ? movement.eyeHeight : 176.0f;
            game.m_outdoorGameView.setCameraAngles(
                std::atan2(pActor->preciseY - pWorld->partyY(), pActor->preciseX - pWorld->partyX()),
                std::atan2(pActor->preciseZ + pActor->height * 0.5f - pWorld->partyFootZ() - eyeHeight, nearest));
            if (state.lastCombatProgressFrame == 0 || state.lastTarget != target
                || state.lastTargetHp != pActor->currentHp)
            {
                state.lastCombatProgressFrame = frame;
                state.lastTarget = target;
                state.lastTargetHp = pActor->currentHp;
            }
            require(frame - state.lastCombatProgressFrame < 1800, "Temple road combat stopped damaging its target");
            state.stalledFrames = 0;
            if (!turn.active())
            {
                press(KeyboardAction::Combat);
                require(turn.active(), "Normal combat input did not enter turn mode on the temple road");
                if (!state.combatCaptured)
                {
                    save("mm6-temple-road-combat");
                    captures.push_back("mm6-temple-road-combat");
                    state.combatCaptured = true;
                }
                return true;
            }
            if (frame % 120 == 0)
                std::cout << "MM6 temple road combat: actor=" << target << " hp=" << pActor->currentHp
                    << " distance=" << nearest << " stage=" << int(turn.stage())
                    << " active=" << pParty->activeMemberIndex() << '\n';
            if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait) return true;
            if (turn.stage() == TurnBasedCombatStage::Movement)
            {
                press(KeyboardAction::Pass);
                return true;
            }
            if (!turn.playerActionReady(*pParty)) return true;
            const size_t member = pParty->activeMemberIndex();
            const auto cast = [&](uint32_t spellId, std::optional<size_t> actor, std::optional<size_t> character)
            {
                const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(spellId);
                require(pSpell && runtime.tryCastSpellFromMember(member, spellId, pSpell->name),
                    "Cannot begin the original temple road spell");
                GameplaySpellService &spells = runtime.spellService();
                PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
                request.targetActorIndex = actor;
                request.targetCharacterIndex = character;
                require(spells.validatePendingTargetSelectionRequest(runtime, request),
                    "Temple road spell target rejected");
                const int mana = pParty->members()[member].spellPoints;
                const int health = character ? pParty->members()[*character].health : 0;
                const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
                require(result.castResult.succeeded() && pParty->members()[member].spellPoints == mana - 2,
                    "Temple road spell did not consume its normal two mana");
                if (character)
                {
                    require(pParty->members()[*character].health > health, "Normal road Heal did not restore health");
                    ++state.heals;
                }
                else ++state.attackPresses;
                spells.clearPendingTargetSelection(runtime);
                session.updateGameplay({}, 0.0f);
                std::cout << "MM6 temple road spell: caster=" << member << " spell=" << spellId
                    << " actor=" << (actor ? int(*actor) : -1) << " member=" << (character ? int(*character) : -1)
                    << " mana=" << mana << "->" << pParty->members()[member].spellPoints << '\n';
            };
            if (member == 1 && pParty->members()[member].knowsSpell(68)
                && pParty->members()[member].spellPoints >= 2)
            {
                size_t weakest = size_t(-1);
                float fraction = 0.7f;
                for (size_t index = 0; index < pParty->members().size(); ++index)
                {
                    const Character &candidate = pParty->members()[index];
                    if (candidate.conditions.test(size_t(CharacterCondition::Dead))
                        || candidate.conditions.test(size_t(CharacterCondition::Eradicated))) continue;
                    const float health = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                    if (health < fraction) { fraction = health; weakest = index; }
                }
                if (weakest != size_t(-1))
                {
                    cast(68, std::nullopt, weakest);
                    return true;
                }
            }
            if (member == 2 && pParty->members()[member].knowsSpell(2)
                && pParty->members()[member].spellPoints >= 2)
            {
                cast(2, target, std::nullopt);
                return true;
            }
            if (nearest < 400.0f || pParty->equippedItemId(member, EquipmentSlot::Bow) != 0)
            {
                press(KeyboardAction::Attack);
                ++state.attackPresses;
            }
            else press(KeyboardAction::Pass);
            return true;
        };
        if (frame == 1)
        {
            require(session.currentMapFileName() == "oute3.odm" && !pParty->hasQuestBit(1150)
                && pParty->inventoryItemCount(2069) == 0,
                "Temple approach requires the unaccepted original candle task");
            for (size_t index = 0; index < pParty->members().size(); ++index)
            {
                const Character &member = pParty->members()[index];
                const CharacterSkill *pSkill = member.findSkillByCanonicalName(mm6StarterSkills[index]);
                require(member.level == 3 && member.experience == 3000 && member.skillPoints == 1
                    && pSkill && pSkill->level == 4, "Temple party did not retain the normally spent skill points");
            }
            std::ifstream stream(std::filesystem::current_path().parent_path() / "mm6-temple-forward.tsv");
            require(stream.good(), "MM6 temple has no walking route");
            std::string line;
            while (std::getline(stream, line))
            {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream values(line);
                float x, y, z;
                require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y) && std::isfinite(z),
                    "Invalid MM6 temple waypoint");
                state.route.push_back({x, y});
                require(state.route.size() <= 2000, "MM6 temple route exceeds the waypoint bound");
            }
            require(state.route.size() >= 2, "MM6 temple route is empty");
            save("mm6-temple-quest-input");
        }
        if (state.phase == 4)
        {
            require(session.currentMapFileName() == "oute3.odm", "Temple walk changed maps without confirmation");
            if (runtime.activeEventDialog().isActive)
            {
                require(!runtime.activeEventDialog().isHouseDialog
                    && runtime.activeEventDialog().presentation != EventDialogPresentation::Transition
                    && ++state.streetDialogs <= 30,
                    "Unexpected building or repeated street dialogue on temple approach");
                close();
                return;
            }
            if (guardRoute()) return;
            require(state.waypoint < state.route.size(), "Temple route exhausted before its doorway");
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = state.route[state.waypoint][0] - x, dy = state.route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            if (distance < 5.0f)
            {
                std::cout << "MM6 temple waypoint: index=" << state.waypoint << " position="
                    << x << ',' << y << ',' << pWorld->partyFootZ() << '\n';
                if (state.waypoint % 10 == 0) save("mm6-temple-walk-" + std::to_string(state.waypoint));
                if (++state.waypoint == state.route.size()) next(5);
                return;
            }
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(distance / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            require(state.stalledFrames < 90, "MM6 normal temple approach stopped at collision geometry or an actor");
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 0:
            game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(1);
            break;
        case 1:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                "Temple quest requires the original Lonely Knight tavern");
            selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 786);
            next(2);
            break;
        case 2:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 1609);
            require(pParty->hasQuestBit(1150) && pParty->inventoryItemCount(2069) == 0,
                "Normal candle quest acceptance gave an item or failed to add its journal entry");
            save("mm6-temple-candle-accepted");
            captures.push_back("mm6-temple-candle-accepted");
            next(3);
            break;
        case 3:
            if (runtime.activeEventDialog().isActive)
            {
                require(frame < state.phaseFrame + 120, "Temple quest dialogue did not close normally");
                close();
            }
            else next(4);
            break;
        case 5:
        {
            const float dx = -21456.0f - pWorld->partyX(), dy = -136.0f - pWorld->partyY();
            const auto &movement = game.m_activeWorldManifest.partyMovement;
            const float eyeHeight = movement.declared ? movement.eyeHeight : 176.0f;
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                std::atan2(320.0f - pWorld->partyFootZ() - eyeHeight, std::hypot(dx, dy)));
            save("mm6-temple-before-trigger");
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(6);
            break;
        }
        case 6:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Actual temple door did not offer its normal entry confirmation");
            require(std::none_of(runtime.activeEventDialog().lines.begin(), runtime.activeEventDialog().lines.end(),
                [](const std::string &line) { return line.starts_with("It will take "); }),
                "Dungeon entrance incorrectly showed outdoor multi-day travel");
            save("mm6-temple-entry-prompt");
            captures.push_back("mm6-temple-entry-prompt");
            next(7);
            break;
        case 7:
            state.entryMinutes = pWorld->gameMinutes();
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            next(8);
            break;
        case 8:
            if (frame < state.phaseFrame + 30) return;
            require(session.currentMapFileName() == "6d02.blv" && pWorld->isIndoorMap()
                && pParty->hasQuestBit(1150) && pParty->inventoryItemCount(2069) == 0,
                "Temple confirmation lost the party, destination or active candle quest");
            require(pWorld->gameMinutes() - state.entryMinutes < 1.0f,
                "Entering the dungeon unexpectedly charged outdoor travel time");
            save("mm6-temple-entered");
            captures.push_back("mm6-temple-entered");
            state.complete = true;
            std::cout << "MM6 temple entry passed: normal quest acceptance and door confirmation, walked="
                << state.walkedDistance << " gold=" << pParty->gold() << " food=" << pParty->food()
                << " kills=" << state.defeatedActors.size() << " heals=" << state.heals << '\n';
            break;
        default: throw std::runtime_error("Invalid MM6 temple approach phase");
        }
    }

    struct Mm6LetterReplayState
    {
        size_t stage = 0;
        uint32_t firstFrame = 1;
        bool openingComplete = false;
        uint32_t openingVideoFrames = 0;
        uint32_t openingEndFrame = 0;
        uint32_t dialogClosedFrame = 0;
        bool complete = false;
        float walkedDistance = 0.0f;
        FirstLetterCheckState tavern;
        CastleLetterCheckState castle;
        Mm6TrainingCheckState training;
        Mm6ReturnCheckState returning;
        Mm6SkillsCheckState skills;
        Mm6TempleEntryCheckState temple;
        IndoorCombatCheckState encounter;
        TempleChestCheckState chest;
        CandlestickCheckState candle;
        size_t endStage = 2;
    };

    static constexpr std::array<const char *, 9> mm6LetterReplayStages = {
        "00-tavern", "01-castle", "02-training", "03-return", "04-skills", "05-temple", "06-encounter", "07-chest",
        "08-candle"};

    static void checkMm6LetterReplay(GameApplication &game, uint32_t frame, int width, int height,
        const std::filesystem::path &output, Mm6LetterReplayState &state, std::vector<std::string> &captures)
    {
        if (!state.openingComplete)
        {
            if (dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr)
            {
                ++state.openingVideoFrames;
                if (state.openingVideoFrames == 120) captures.push_back("mm6-opening-movie");
                return;
            }
            // MM6's original New Sorpigal entry need not queue an opening movie.
            if (frame <= 30) return;
            if (state.openingEndFrame == 0) state.openingEndFrame = frame;
            if (frame < state.openingEndFrame + 12) return;
            if (game.m_gameAudioSystem.isBackgroundMusicPaused())
                throw std::runtime_error("MM6 opening left background music paused");
            GameSession &session = game.m_gameSession;
            GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
            Party *pParty = runtime.party();
            if (!pParty || pParty->inventoryItemCount(2125) != 0 || !runtime.heldInventoryItem().active
                || runtime.heldInventoryItem().item.objectDescriptionId != 2125)
                throw std::runtime_error("Original MM6 opening did not present exactly its starting letter");
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            if (!portrait) throw std::runtime_error("Starting letter has no recipient portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            // Modern controls use held right mouse for cursor mode. Engage it away from
            // portraits before moving to the recipient, so the press does not read the scroll.
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton = {false, false, true};
            input.leftMouseButton = {};
            session.updateGameplay(input, 0.0f);
            if (runtime.heldInventoryItem().active || pParty->inventoryItemCount(2125) != 1
                || pParty->gold() != 200 || !pParty->hasQuestBit(1104) || !pParty->hasQuestBit(1105))
                throw std::runtime_error("Portrait click did not store the original MM6 letter without other rewards");
            if (!game.quickSaveToPath(output / "mm6-start-ready.oysav", "MM6 收好原开局信件", {}))
                throw std::runtime_error("Cannot record the original MM6 starting letter");
            state.openingComplete = true;
            state.firstFrame = frame + 1;
            captures.push_back("mm6-opening-complete");
            std::cout << "MM6 opening ready: video_frames=" << state.openingVideoFrames << '\n';
            return;
        }
        if (state.stage == state.endStage && state.endStage >= 3)
        {
            const bool returned = state.endStage >= 4;
            const Party *pParty = game.m_gameSession.gameplayScreenRuntime().party();
            if (!pParty || pParty->members().size() != 4
                || (state.endStage >= 7 ? pParty->gold() < 5813 : pParty->gold() != (returned ? 5813 : 5815))
                || pParty->food() != (returned ? 14 : 5)
                || game.m_gameSession.currentMapFileName()
                    != (state.endStage == 9 ? "oute3.odm"
                        : state.endStage >= 6 ? "6d02.blv" : returned ? "oute3.odm" : "outd3.odm")
                || !pParty->hasQuestBit(1110) || pParty->hasQuestBit(1106)
                || pParty->inventoryItemCount(2125) != 0
                || !std::all_of(pParty->members().begin(), pParty->members().end(), [&](const Character &member)
                    { return member.level == 3 && (state.endStage >= 6 ? member.experience >= 3000
                        : member.experience == 3000) && member.awards.contains(100)
                        && (state.endStage >= 7 ? member.health > 0 : member.conditions.none()); }))
                throw std::runtime_error("Independent MM6 reload lost the actual trained party or letter quest");
            if (state.endStage >= 5)
                for (size_t index = 0; index < pParty->members().size(); ++index)
                {
                    const Character &member = pParty->members()[index];
                    const CharacterSkill *pSkill = member.findSkillByCanonicalName(mm6StarterSkills[index]);
                    if (member.skillPoints != 1 || !pSkill || pSkill->level != 4)
                        throw std::runtime_error("Independent MM6 reload lost normal skill spending");
                }
            if (state.endStage >= 6 && state.endStage < 9 && !pParty->hasQuestBit(1150))
                throw std::runtime_error("Independent temple reload lost the candle quest");
            if (state.endStage == 8 && pParty->inventoryItemCount(2069) != 1)
                throw std::runtime_error("Independent chest reload lost its original candlestick");
            if (state.endStage == 9 && (pParty->hasQuestBit(1150) || pParty->inventoryItemCount(2069) != 0
                || !std::all_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                    { return member.awards.contains(81) && member.experience >= 5000; })))
                throw std::runtime_error("Independent candle reload lost the original quest reward");
            const std::string result = state.endStage == 9 ? "mm6-candlestick-return-complete"
                : state.endStage == 8 ? "mm6-temple-chest-complete"
                : state.endStage == 7 ? "mm6-entrance-encounter-complete"
                : state.endStage == 6 ? "mm6-temple-party-entered"
                : state.endStage == 5 ? "mm6-skilled-party-complete"
                : returned ? "mm6-returned-party-complete" : "mm6-trained-party-complete";
            if (!game.quickSaveToPath(output / (result + ".oysav"), "MM6 正常队伍重启读档", {}))
                throw std::runtime_error("Cannot preserve the independently loaded MM6 trained party");
            captures.push_back(result);
            state.complete = true;
            std::cout << "MM6 " << (returned ? "returned" : "trained")
                << " party independently reloaded without repeating quests or training\n";
            return;
        }
        if (state.stage >= mm6LetterReplayStages.size()) throw std::runtime_error("Invalid MM6 letter replay stage");
        const std::string name = mm6LetterReplayStages[state.stage];
        const uint32_t localFrame = frame - state.firstFrame + 1;
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        if (localFrame == 1)
        {
            std::filesystem::create_directories(output / name / "run");
            if (state.stage == 3 || state.stage == 5 || state.stage == 8)
            {
                const auto routes = output.parent_path() / "routes" / name;
                if (!std::filesystem::is_directory(routes)) throw std::runtime_error("Missing MM6 return routes");
                for (const auto &entry : std::filesystem::directory_iterator(routes))
                    if (entry.is_regular_file() && entry.path().extension() == ".tsv")
                        std::filesystem::copy_file(entry.path(), output / name / entry.path().filename());
            }
            std::filesystem::current_path(output / name / "run");
            const Party &party = *runtime.party();
            std::cout << "MM6 replay stage begin: name=" << name << " global_frame=" << frame
                << " map=" << session.currentMapFileName() << " members=" << party.members().size()
                << " gold=" << party.gold() << '\n';
        }
        if (state.stage == 0 && !state.tavern.complete)
            checkFirstLetter(game, localFrame, width, height, false, state.tavern, captures);
        if (state.stage == 1 && !state.castle.complete)
            checkCastleLetter(game, localFrame, width, height, false, state.castle, captures);
        if (state.stage == 2 && !state.training.complete)
            checkMm6Training(game, localFrame, width, height, state.training, captures);
        if (state.stage == 3 && !state.returning.complete)
            checkMm6Return(game, localFrame, width, height, state.returning, captures);
        if (state.stage == 4 && !state.skills.complete)
            checkMm6Skills(game, localFrame, width, height, state.skills, captures);
        if (state.stage == 5 && !state.temple.complete)
            checkMm6TempleEntry(game, localFrame, width, height, state.temple, captures);
        if (state.stage == 6 && !state.encounter.complete)
        {
            if (localFrame == 1)
            {
                state.encounter.entireEncounter = true;
                state.encounter.searchCorpses = state.endStage != 9;
                captures.push_back("mm6-indoor-encounter-start");
            }
            checkIndoorCombat(game, localFrame, state.encounter, true, width, height);
            if (state.encounter.nearFrame == localFrame) captures.push_back("mm6-indoor-encounter-near");
            if (state.encounter.hitFrame == localFrame) captures.push_back("mm6-indoor-encounter-hit");
            if (state.encounter.complete) captures.push_back("mm6-indoor-encounter-finished");
        }
        if (state.stage == 7 && !state.chest.complete)
        {
            state.chest.trainedMm6 = true;
            state.chest.pointerTransfer = true;
            checkTempleChest(game, localFrame, width, height, state.chest, captures);
        }
        if (state.stage == 8 && !state.candle.complete)
        {
            state.candle.trainedMm6 = true;
            checkCandlestick(game, localFrame, width, height, false, state.candle, captures);
        }
        for (std::string &capture : captures) capture = name + "/run/" + capture;
        const bool complete = state.stage == 0 ? state.tavern.complete
            : state.stage == 1 ? state.castle.complete : state.stage == 2 ? state.training.complete
            : state.stage == 3 ? state.returning.complete
            : state.stage == 4 ? state.skills.complete
            : state.stage == 5 ? state.temple.complete
            : state.stage == 6 ? state.encounter.complete
            : state.stage == 7 ? state.chest.complete : state.candle.complete;
        if (!complete) return;
        if (state.stage == 7 && session.activeWorldRuntime()->activeChestView())
        {
            if (localFrame < state.chest.phaseFrame + 12) return;
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            if (session.activeWorldRuntime()->activeChestView())
                throw std::runtime_error("MM6 chest stage did not close through normal Escape");
            return;
        }
        // Finish through the ordinary dialog back/leave action, so the next walk is unblocked.
        if (runtime.activeEventDialog().isActive)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            return;
        }
        if (state.dialogClosedFrame == 0) state.dialogClosedFrame = frame;
        if (frame < state.dialogClosedFrame + 12) return;
        if (!game.quickSaveToPath(output / name / "stage-complete.oysav", "MM6 连续送信阶段", {}))
            throw std::runtime_error("Cannot preserve MM6 continuous letter stage");
        state.walkedDistance += state.stage == 0 ? state.tavern.walkedDistance
            : state.stage == 1 ? state.castle.walkedDistance
            : state.stage == 2 ? state.training.walkedDistance : state.stage == 3 ? state.returning.walkedDistance
            : state.stage == 5 ? state.temple.walkedDistance
            : state.stage == 7 ? state.chest.walkedDistance
            : state.stage == 8 ? state.candle.walkedDistance : 0.0f;
        std::cout << "MM6 replay stage complete: name=" << name << " global_frame=" << frame
            << " walked_total=" << state.walkedDistance << '\n';
        ++state.stage;
        state.firstFrame = frame + 1;
        state.dialogClosedFrame = 0;
        if (state.stage == state.endStage)
        {
            state.complete = true;
            const std::string result = state.endStage == 2 ? "mm6-letter-chain-complete"
                : state.endStage == 3 ? "mm6-trained-party-complete"
                : state.endStage == 4 ? "mm6-returned-party-complete"
                : state.endStage == 5 ? "mm6-skilled-party-complete"
                : state.endStage == 6 ? "mm6-temple-party-entered"
                : state.endStage == 7 ? "mm6-entrance-encounter-complete"
                : state.endStage == 8 ? "mm6-temple-chest-complete" : "mm6-candlestick-return-complete";
            if (!game.quickSaveToPath(output / (result + ".oysav"), "MM6 原任务与训练结果", {}))
                throw std::runtime_error("Cannot preserve MM6 continuous result");
            captures.push_back(result);
        }
    }

    struct Mm7ReplayState
    {
        size_t stage = 0;
        uint32_t firstFrame = 1;
        bool complete = false;
        bool openingComplete = false;
        uint32_t openingVideoFrames = 0;
        uint32_t openingEndFrame = 0;
        float walkedDistance = 0.0f;
        Mm7RouteCheckState temple;
        Mm7DungeonCheckState dungeon;
        Mm7ContestCheckState contest;
    };

    static constexpr std::array<const char *, 14> mm7ReplayStages = {
        "00-temple-entry", "01-temple-tile", "02-temple-hat", "03-temple-exit-rest",
        "04-temple-delivery", "05-potion", "06-shell", "07-luck-well", "08-money-well", "09-instrument",
        "10-dragon-bow", "11-dragon-exit-rest", "12-contest-award", "13-departure"};

    static void checkMm7Replay(GameApplication &game, uint32_t frame, int width, int height,
        const std::filesystem::path &output, Mm7ReplayState &state, std::vector<std::string> &captures)
    {
        // A real new MM7 session queues its original opening movie from the map's first-load event.
        // Let normal frame processing start and finish it before sending gameplay input.
        if (!state.openingComplete)
        {
            if (dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr)
            {
                ++state.openingVideoFrames;
                if (state.openingVideoFrames == 1 || state.openingVideoFrames == 120 || state.openingVideoFrames == 240)
                    captures.push_back("mm7-opening-movie-" + std::to_string(state.openingVideoFrames));
                return;
            }
            if (state.openingVideoFrames == 0)
            {
                if (frame > 30) throw std::runtime_error("Original MM7 opening movie did not start");
                return;
            }
            if (state.openingEndFrame == 0) state.openingEndFrame = frame;
            if (frame < state.openingEndFrame + 12) return;
            if (game.m_gameAudioSystem.isBackgroundMusicPaused())
                throw std::runtime_error("MM7 opening movie left background music paused");
            std::cout << "MM7 opening movie finished naturally: frames=" << state.openingVideoFrames << '\n';
            state.openingComplete = true;
            state.firstFrame = frame + 1;
            captures.push_back("mm7-opening-complete");
            return;
        }
        if (state.stage >= mm7ReplayStages.size()) throw std::runtime_error("Invalid MM7 replay stage");
        const std::string name = mm7ReplayStages[state.stage];
        const uint32_t localFrame = frame - state.firstFrame + 1;
        if (localFrame == 1)
        {
            const auto routes = output.parent_path() / "routes" / name;
            if (!std::filesystem::is_directory(routes)) throw std::runtime_error("Missing MM7 stage route: " + name);
            std::filesystem::create_directories(output / name / "run");
            for (const auto &entry : std::filesystem::directory_iterator(routes))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".tsv")
                    std::filesystem::copy_file(entry.path(), output / name / entry.path().filename());
            }
            std::filesystem::current_path(output / name / "run");
            const Party &party = *game.m_gameSession.gameplayScreenRuntime().party();
            std::cout << "MM7 replay stage begin: name=" << name << " global_frame=" << frame
                << " map=" << game.m_gameSession.currentMapFileName() << " members=" << party.members().size()
                << " gold=" << party.gold() << '\n';
        }
        bool complete = false;
        float walked = 0.0f;
        if (state.stage == 0)
        {
            checkMm7Temple(game, localFrame, width, height, false, state.temple, captures);
            complete = state.temple.complete;
            walked = state.temple.walkedDistance;
        }
        else if (state.stage <= 3 || state.stage == 10 || state.stage == 11)
        {
            checkMm7Dungeon(game, localFrame, width, height, state.dungeon, captures, false);
            complete = state.dungeon.complete;
            walked = state.dungeon.walkedDistance;
        }
        else
        {
            checkMm7Contest(game, localFrame, width, height, false, state.contest, captures);
            complete = state.contest.complete;
            walked = state.contest.walkedDistance;
        }
        for (std::string &capture : captures) capture = name + "/run/" + capture;
        if (!complete) return;
        if (!game.quickSaveToPath(output / name / "stage-complete.oysav", "MM7 连续开局阶段记录", {}))
            throw std::runtime_error("Cannot preserve MM7 continuous stage");
        state.walkedDistance += walked;
        std::cout << "MM7 replay stage complete: name=" << name << " global_frame=" << frame
            << " walked_total=" << state.walkedDistance << '\n';
        ++state.stage;
        state.firstFrame = frame + 1;
        state.dungeon = {};
        state.contest = {};
        if (state.stage == mm7ReplayStages.size())
        {
            state.complete = true;
            if (!game.quickSaveToPath(output / "mm7-full-starter-complete.oysav", "MM7 正常连续开局", {}))
                throw std::runtime_error("Cannot preserve MM7 continuous starter result");
            captures.push_back("mm7-full-starter-complete");
        }
    }

    struct PausedMovementCheckState
    {
        bool complete = false;
        int startingAp = 0;
        float x = 0.0f;
        float y = 0.0f;
        float footZ = 0.0f;
    };

    static void checkPausedMovement(GameApplication &game, uint32_t frame, int width, int height,
        PausedMovementCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const char *message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        IGameplayWorldRuntime *world = session.activeWorldRuntime();
        TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
        require(world && turn.active() && turn.stage() == TurnBasedCombatStage::Movement,
            "Paused movement check needs an actual saved movement phase");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "暂停界面行动点验证", {}), "Cannot save paused movement evidence");
        };
        const auto escape = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto blockedSteps = [&](const char *name, bool inspect = false)
        {
            save(std::string(name) + "-before");
            for (KeyboardAction action : {KeyboardAction::Forward, KeyboardAction::Backward,
                     KeyboardAction::Left, KeyboardAction::Right})
            {
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.actions[keyboardActionIndex(action)] = {true, true, false};
                input.rightMouseButton = {inspect, inspect, false};
                session.updateGameplay(input, 0.05f);
                input.actions = {};
                input.rightMouseButton.pressed = false;
                session.updateGameplay(input, 0.0f);
                std::cout << "Paused movement: overlay=" << name << " action=" << int(action)
                    << " ap=" << state.startingAp << "->" << turn.movementActionPoints()
                    << " moved=" << std::hypot(world->partyX() - state.x, world->partyY() - state.y) << '\n';
                require(turn.movementActionPoints() == state.startingAp,
                    "Blocked movement consumed turn-based action points");
                require(std::hypot(world->partyX() - state.x, world->partyY() - state.y) < 0.01f
                    && std::abs(world->partyFootZ() - state.footZ) < 0.01f,
                    "Blocked movement changed the saved party position");
            }
            session.updateGameplay({}, 0.0f);
            save(std::string(name) + "-after");
        };
        if (frame == 1)
        {
            require(turn.movementActionPoints() >= 52 && !runtime.menuScreenState().active
                && !runtime.characterScreen().open && !world->activeChestView(),
                "Paused movement needs a normal closed-interface save with at least two remaining steps");
            state.startingAp = turn.movementActionPoints();
            state.x = world->partyX();
            state.y = world->partyY();
            state.footZ = world->partyFootZ();
            save("pause-input");
        }
        if (frame == 20)
        {
            escape();
            require(runtime.menuScreenState().active, "Normal Escape did not open the pause menu");
            captures.push_back("pause-menu");
        }
        if (frame == 40) blockedSteps("pause-menu");
        if (frame == 60)
        {
            escape();
            require(!runtime.menuScreenState().active, "Normal Escape did not resume from the pause menu");
        }
        if (frame == 80)
        {
            pressQuestGameplay(session, width, height, KeyboardAction::History);
            require(runtime.journalScreenState().active, "Normal History action did not open the journal");
            captures.push_back("pause-journal");
        }
        if (frame == 100) blockedSteps("pause-journal");
        if (frame == 120)
        {
            escape();
            require(!runtime.journalScreenState().active, "Normal Escape did not close the journal");
        }
        if (frame == 140) blockedSteps("pause-inspect", true);
        if (frame == 160)
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "Character pause check has no actual portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned click = 0; click < 2; ++click)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open, "Normal portrait clicks did not open the character screen");
            captures.push_back("pause-character");
        }
        if (frame == 180) blockedSteps("pause-character");
        if (frame == 200)
        {
            escape();
            require(!runtime.characterScreen().open, "Normal Escape did not close the character screen");
        }
        if (frame == 220)
        {
            // This direction is the first normal retreat leg from the accepted temple chest save.
            const float yaw = std::atan2(-15220.0f - world->partyY(), 17890.0f - world->partyX());
            game.m_indoorRenderer.setCameraAngles(yaw, 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.05f);
            const float moved = std::hypot(world->partyX() - state.x, world->partyY() - state.y);
            require(turn.movementActionPoints() == state.startingAp - 26 && moved > 100.0f && moved < 180.0f,
                "Resumed normal movement did not consume exactly one step and move the party");
            save("pause-resumed");
            captures.push_back("pause-resumed");
            state.complete = true;
            std::cout << "Paused movement passed: overlays=4 directions_each=4 blocked_ap_cost=0 resumed_ap_cost=26"
                << " resumed_distance=" << moved << '\n';
        }
    }

    struct Mm6GoblinwatchCheckState
    {
        unsigned phase = 0;
        uint32_t phaseFrame = 0;
        size_t waypoint = 0;
        unsigned stalledFrames = 0;
        unsigned streetDialogs = 0;
        unsigned healingMember = 0;
        unsigned indoorInteractions = 0;
        size_t combinationStep = 0;
        float walkedDistance = 0.0f;
        int expectedGold = 0;
        int goldBeforeReward = 0;
        std::vector<uint64_t> experienceBeforeReward;
        std::vector<std::array<float, 2>> route;
        std::vector<PathPoint> indoorRoute;
        PathPlanStatus indoorRouteStatus = PathPlanStatus::NotRequested;
        bool complete = false;
        bool combatCaptured = false;
    };

    static void checkMm6Goblinwatch(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, Mm6GoblinwatchCheckState &state, std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember(),
            "Goblinwatch needs the surviving normal four-member MM6 party");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM6大耳怪前哨正常任务", {}), "Cannot preserve Goblinwatch quest evidence");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            state.indoorRoute.clear();
            state.indoorRouteStatus = PathPlanStatus::NotRequested;
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto selectMember = [&](size_t memberIndex)
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
            require(portrait.has_value(), "Goblinwatch patient has no visible portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton.held = true;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(pParty->activeMemberIndex() == memberIndex && !runtime.characterScreen().open,
                "Normal portrait input did not select the Goblinwatch party member");
        };
        const auto loadRoute = [&](const char *name)
        {
            state.route.clear();
            const std::filesystem::path path = std::filesystem::current_path().parent_path()
                / "routes" / "outdoor" / name;
            std::ifstream stream(path);
            require(stream.good(), "Missing Goblinwatch outdoor route: " + path.string());
            std::string line;
            while (std::getline(stream, line))
            {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream values(line);
                float x = 0.0f, y = 0.0f, z = 0.0f;
                require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                    && std::isfinite(z), "Invalid Goblinwatch outdoor waypoint");
                state.route.push_back({x, y});
            }
            require(state.route.size() >= 2, "Goblinwatch outdoor route is empty");
            state.waypoint = 0;
        };
        const auto walkOutdoor = [&]()
        {
            require(session.currentMapFileName() == "oute3.odm" && !pWorld->isIndoorMap(),
                "Goblinwatch outdoor walk changed maps unexpectedly");
            if (runtime.activeEventDialog().isActive)
            {
                require(!runtime.activeEventDialog().isHouseDialog
                    && runtime.activeEventDialog().presentation != EventDialogPresentation::Transition
                    && ++state.streetDialogs <= 40, "Unexpected building or repeated dialogue on Goblinwatch route");
                close();
                return false;
            }
            while (state.waypoint < state.route.size()
                && std::hypot(state.route[state.waypoint][0] - pWorld->partyX(),
                    state.route[state.waypoint][1] - pWorld->partyY()) < 25.0f)
            {
                ++state.waypoint;
            }
            if (state.waypoint == state.route.size()) return true;
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = state.route[state.waypoint][0] - x;
            const float dy = state.route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(distance / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 180 == 0)
                std::cout << "Goblinwatch outdoor walk: phase=" << state.phase << " waypoint=" << state.waypoint
                    << " position=" << pWorld->partyX() << ',' << pWorld->partyY() << ',' << pWorld->partyFootZ()
                    << " remaining=" << distance << '\n';
            require(state.stalledFrames < 120, "Goblinwatch outdoor route stopped at collision geometry or an actor");
            return false;
        };
        const auto aimOutdoor = [&](float x, float y, float z)
        {
            const float dx = x - pWorld->partyX(), dy = y - pWorld->partyY();
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                std::atan2(z - pWorld->partyFootZ() - 176.0f, std::hypot(dx, dy)));
        };
        const auto aimIndoor = [&](float x, float y, float z)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Goblinwatch indoor aim lost the scene runtime");
            const IndoorMoveState &position = pScene->partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(y - position.y, x - position.x),
                std::atan2(z - position.eyeZ(), std::hypot(x - position.x, y - position.y)));
        };
        const auto cast = [&](size_t member, uint32_t spellId, std::optional<size_t> actor,
            std::optional<size_t> character)
        {
            const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(spellId);
            require(pSpell && runtime.tryCastSpellFromMember(member, spellId, pSpell->name),
                "Cannot start a normal Goblinwatch combat spell");
            GameplaySpellService &spells = runtime.spellService();
            PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
            request.targetActorIndex = actor;
            request.targetCharacterIndex = character;
            require(spells.validatePendingTargetSelectionRequest(runtime, request),
                "Goblinwatch combat spell target was rejected");
            const int mana = pParty->members()[member].spellPoints;
            const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
            require(result.castResult.succeeded() && pParty->members()[member].spellPoints == mana - 2,
                "Goblinwatch combat spell did not consume its normal mana cost");
            spells.clearPendingTargetSelection(runtime);
            session.updateGameplay({}, 0.0f);
        };
        const auto fightIndoor = [&]()
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Goblinwatch combat lost its indoor scene");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            size_t target = size_t(-1);
            float nearest = 1000.0f;
            GameplayRuntimeActorState selected;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                GameplayRuntimeActorState actor;
                if (!world.actorRuntimeState(index, actor) || actor.isDead || actor.isInvisible
                    || !actor.hostileToParty || !world.partyAttackActorHasLineOfSight(index)) continue;
                const float distance = std::hypot(actor.preciseX - world.partyX(), actor.preciseY - world.partyY());
                if (distance < nearest)
                {
                    nearest = distance;
                    target = index;
                    selected = actor;
                }
            }
            TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
            if (target == size_t(-1))
            {
                if (!turn.active()) return false;
                if (turn.canToggleOff(*pParty)) pressQuestGameplay(session, width, height, KeyboardAction::Combat);
                else if (turn.stage() == TurnBasedCombatStage::Movement || turn.playerActionReady(*pParty))
                    pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                return true;
            }
            aimIndoor(selected.preciseX, selected.preciseY, selected.preciseZ + selected.height * 0.5f);
            if (!turn.active())
            {
                pressQuestGameplay(session, width, height, KeyboardAction::Combat);
                if (!state.combatCaptured)
                {
                    captures.push_back("goblinwatch-combat");
                    state.combatCaptured = true;
                }
                return true;
            }
            if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait) return true;
            if (turn.stage() == TurnBasedCombatStage::Movement)
            {
                pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                return true;
            }
            if (!turn.playerActionReady(*pParty)) return true;
            const size_t member = pParty->activeMemberIndex();
            if (member == 1 && pParty->members()[member].knowsSpell(68)
                && pParty->members()[member].spellPoints >= 2)
            {
                size_t weakest = size_t(-1);
                float fraction = 0.6f;
                for (size_t index = 0; index < pParty->members().size(); ++index)
                {
                    const Character &candidate = pParty->members()[index];
                    const float health = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                    if (candidate.health > 0 && health < fraction) { fraction = health; weakest = index; }
                }
                if (weakest != size_t(-1))
                {
                    cast(member, 68, std::nullopt, weakest);
                    return true;
                }
            }
            if (member == 2 && pParty->members()[member].knowsSpell(2)
                && pParty->members()[member].spellPoints >= 2)
            {
                cast(member, 2, target, std::nullopt);
                return true;
            }
            if (nearest < 425.0f || pParty->equippedItemId(member, EquipmentSlot::Bow) != 0)
                pressQuestGameplay(session, width, height, KeyboardAction::Attack);
            else pressQuestGameplay(session, width, height, KeyboardAction::Pass);
            return true;
        };
        const auto walkIndoor = [&](float targetX, float targetY, float targetZ)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene && session.currentMapFileName() == "6d01.blv", "Goblinwatch indoor walk left 6d01");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            if (fightIndoor()) return false;
            // Authored MM6 doors can need almost three seconds to reach state 1.  Replanning or
            // re-triggering while the same door is moving only restarts the close command and
            // leaves the route proposal blocked by its intermediate geometry.
            if (state.indoorInteractions > 0 && frame < state.phaseFrame + 512) return false;
            const float x = world.partyX(), y = world.partyY();
            if (std::hypot(targetX - x, targetY - y) < 150.0f
                && std::abs(targetZ - world.partyFootZ()) < 192.0f) return true;
            if (state.indoorRoute.empty())
            {
                const auto &map = game.m_gameDataLoader.getSelectedMap();
                require(map && map->indoorMapData, "Goblinwatch path planner lost the authored indoor map");
                const std::vector<IndoorVertex> vertices = buildIndoorMechanismAdjustedVertices(
                    *map->indoorMapData, world.mapDeltaData(), world.eventRuntimeState());
                const IndoorPathMapBuildResult built = IndoorPathfindingBuilder::buildPathMap(
                    *map->indoorMapData, vertices, world.mapDeltaData());
                PathPlanRequest request;
                request.source = {x, y, world.partyFootZ()};
                request.target = {targetX, targetY, targetZ};
                request.object.radius = 37.0f;
                request.object.stepHeight = 128.0f;
                request.object.stepLength = 128.0f;
                request.nodeLimit = 200000;
                request.allowPartialPath = true;
                PathPlanner planner;
                const PathPlanResult result = planner.plan(built.pathMap, request);
                require(result.status == PathPlanStatus::Success || result.status == PathPlanStatus::Partial,
                    "Goblinwatch indoor route has no navigable proposal");
                state.indoorRoute = result.waypoints;
                state.indoorRouteStatus = result.status;
                state.waypoint = 0;
                while (state.waypoint < state.indoorRoute.size()
                    && std::hypot(state.indoorRoute[state.waypoint].x - x,
                        state.indoorRoute[state.waypoint].y - y) < 90.0f) ++state.waypoint;
                std::cout << "Goblinwatch indoor plan: phase=" << state.phase
                    << " status=" << int(result.status) << " points=" << result.waypoints.size()
                    << " best_distance=" << result.debug.bestDistance2d << '\n';
            }
            while (state.waypoint < state.indoorRoute.size()
                && std::hypot(state.indoorRoute[state.waypoint].x - world.partyX(),
                    state.indoorRoute[state.waypoint].y - world.partyY()) < 90.0f) ++state.waypoint;
            if (state.waypoint == state.indoorRoute.size())
            {
                if (state.indoorRouteStatus == PathPlanStatus::Success)
                {
                    state.indoorRoute.clear();
                    return std::hypot(targetX - world.partyX(), targetY - world.partyY()) < 220.0f;
                }
                require(state.indoorInteractions < 20,
                    "Goblinwatch indoor route repeatedly stopped at an unhandled mechanism");
                bool foundDoor = false;
                uint16_t doorEvent = 0;
                for (float pitch : {-0.55f, -0.3f, -0.05f, 0.2f, 0.45f})
                {
                    for (size_t direction = 0; direction < 64; ++direction)
                    {
                        const float yaw = float(direction) * 6.28318530718f / 64.0f;
                        game.m_indoorRenderer.setCameraAngles(yaw, pitch);
                        GameplayWorldPickRequest pick = world.buildWorldPickRequest({
                            .screenX = width * 0.5f,
                            .screenY = height * 0.5f,
                            .screenWidth = width,
                            .screenHeight = height,
                            .includeRay = true,
                        });
                        pick.ignoreActors = true;
                        const GameplayWorldHit hit = world.pickKeyboardInteractionTarget(pick);
                        if (!hit.eventTarget || !world.canActivateWorldHit(hit, GameplayInteractionMethod::Keyboard))
                            continue;
                        const uint16_t eventId = hit.eventTarget->triggeredEventId;
                        if (!((eventId >= 1 && eventId <= 18) || (eventId >= 60 && eventId <= 63))) continue;
                        foundDoor = true;
                        doorEvent = eventId;
                        break;
                    }
                    if (foundDoor) break;
                }
                require(foundDoor, "Goblinwatch partial route has no nearby activatable authored door");
                pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                ++state.indoorInteractions;
                state.indoorRoute.clear();
                state.phaseFrame = frame;
                std::cout << "Goblinwatch mechanism input: phase=" << state.phase
                    << " attempt=" << state.indoorInteractions << " event=" << doorEvent
                    << " position=" << world.partyX() << ','
                    << world.partyY() << ',' << world.partyFootZ() << '\n';
                return false;
            }
            const PathPoint point = state.indoorRoute[state.waypoint];
            const float dx = point.x - world.partyX(), dy = point.y - world.partyY();
            const float distance = std::hypot(dx, dy);
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                distance < 120.0f ? 0.025f : 0.05f);
            const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.1f ? state.stalledFrames + 1 : 0;
            if (state.stalledFrames == 90)
            {
                state.indoorRoute.clear();
                state.stalledFrames = 0;
            }
            return false;
        };
        const auto takeChestItem = [&](uint32_t itemId)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Goblinwatch chest lost its indoor scene");
            GameplayChestViewState *pChest = pScene->worldRuntime().activeChestView();
            require(pChest && pChest->chestId == 1, "Goblinwatch opened the wrong original chest");
            const auto item = std::find_if(pChest->items.begin(), pChest->items.end(), [&](const auto &entry)
                { return entry.item.objectDescriptionId == itemId; });
            require(item != pChest->items.end(), "Goblinwatch code scroll is absent from original chest 1");
            const auto grid = runtime.resolveChestGridArea(width, height);
            require(grid.has_value(), "Goblinwatch chest has no rendered grid");
            const int before = pParty->inventoryItemCount(itemId);
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = grid->x + (item->gridX + 0.5f) * grid->width / pChest->gridWidth;
            input.pointerY = grid->y + (item->gridY + 0.5f) * grid->height / pChest->gridHeight;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.heldInventoryItem().active
                && runtime.heldInventoryItem().item.objectDescriptionId == itemId,
                "Normal chest pointer did not pick up Goblinwatch code scroll");
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
            require(portrait.has_value(), "Goblinwatch scroll has no recipient portrait");
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(!runtime.heldInventoryItem().active && pParty->inventoryItemCount(itemId) == before + 1,
                "Normal portrait transfer lost or duplicated Goblinwatch code scroll");
        };
        const auto pressButton = [&](float x, float y, float z)
        {
            aimIndoor(x, y, z);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
        };

        if (frame == 1)
        {
            state.expectedGold = pParty->gold();
            if (reload)
            {
                require(session.currentMapFileName() == "oute3.odm" && pParty->hasQuestBit(1324)
                    && !pParty->hasQuestBit(1107) && pParty->inventoryItemCount(2163) == 1
                    && std::all_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                        { return member.awards.contains(95); }),
                    "Goblinwatch reload input lost its completed quest state");
                state.goldBeforeReward = pParty->gold();
                for (const Character &member : pParty->members())
                    state.experienceBeforeReward.push_back(member.experience);
                captures.push_back("goblinwatch-reload-input");
                return;
            }
            require(session.currentMapFileName() == "oute3.odm" && pParty->gold() == 7107
                && pParty->food() == 14 && pParty->hasQuestBit(1104) && pParty->hasQuestBit(1110)
                && !pParty->hasQuestBit(1107) && !pParty->hasQuestBit(1324)
                && pParty->inventoryItemCount(2109) == 0 && pParty->inventoryItemCount(2163) == 0,
                "Goblinwatch input is not the fixed normal checkpoint113 continuation");
            require(pParty->members()[2].conditions.to_ullong() == 64,
                "Goblinwatch input did not retain the original sorcerer poison");
            loadRoute("01-tavern-to-temple.tsv");
            save("goblinwatch-input");
            captures.push_back("goblinwatch-input");
        }
        if (reload)
        {
            if (frame < state.phaseFrame + 12) return;
            switch (state.phase)
            {
            case 0:
                aimOutdoor(-11240.0f, -6536.0f, 304.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                next(1);
                break;
            case 1:
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 208,
                    "Goblinwatch reload did not reopen New Sorpigal Town Hall");
                selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 1076);
                next(2);
                break;
            case 2:
            {
                const auto &actions = runtime.activeEventDialog().actions;
                require(std::none_of(actions.begin(), actions.end(), [](const EventDialogAction &action)
                    { return action.kind == EventDialogActionKind::NpcTopic && action.id == 1317; }),
                    "Goblinwatch reload still offers the consumed reward topic");
                require(std::any_of(actions.begin(), actions.end(), [](const EventDialogAction &action)
                    { return action.kind == EventDialogActionKind::NpcTopic && action.id == 1318; }),
                    "Goblinwatch reload lost the original next-quest topic");
                require(pParty->gold() == state.goldBeforeReward,
                    "Goblinwatch reload repeated the gold reward");
                for (size_t index = 0; index < pParty->members().size(); ++index)
                    require(pParty->members()[index].experience == state.experienceBeforeReward[index],
                        "Goblinwatch reload repeated the experience reward");
                save("mm6-goblinwatch-reload-complete");
                captures.push_back("goblinwatch-reload-complete");
                state.complete = true;
                std::cout << "Goblinwatch independent reload passed: gold=" << pParty->gold()
                    << " item2163=" << pParty->inventoryItemCount(2163) << '\n';
                break;
            }
            default: throw std::runtime_error("Invalid Goblinwatch reload phase");
            }
            return;
        }

        if (state.phase == 0 && walkOutdoor()) next(1);
        else if (state.phase == 7 && walkOutdoor()) next(8);
        else if (state.phase == 15 && walkOutdoor()) next(16);
        else if (state.phase == 39 && walkOutdoor()) next(40);
        if (state.phase == 0 || state.phase == 7 || state.phase == 15 || state.phase == 39) return;
        if (state.phase == 22 && state.indoorInteractions == 1)
        {
            // The first lower room is intentionally offset from the chest corridor.  A partial
            // path aimed directly at the chest prefers the nearer south wall, so walk around the
            // authored corner to door 3 before resuming target-directed planning.
            if (!walkIndoor(-1320.0f, 5750.0f, -512.0f)) return;
            aimIndoor(-1584.0f, 5759.0f, -384.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            ++state.indoorInteractions;
            state.indoorRoute.clear();
            state.phaseFrame = frame;
            std::cout << "Goblinwatch mechanism input: phase=22 attempt=" << state.indoorInteractions
                << " event=3 position=" << dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get())
                    ->worldRuntime().partyX() << ','
                << dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get())
                    ->worldRuntime().partyY() << ",-512\n";
            return;
        }
        if (state.phase == 22 && state.indoorInteractions == 2)
        {
            if (!walkIndoor(-1850.0f, 4544.0f, -512.0f)) return;
            aimIndoor(-2112.0f, 4544.0f, -384.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            ++state.indoorInteractions;
            state.indoorRoute.clear();
            state.phaseFrame = frame;
            std::cout << "Goblinwatch mechanism input: phase=22 attempt=" << state.indoorInteractions
                << " event=9\n";
            return;
        }
        if ((state.phase == 22 && !walkIndoor(-1940.0f, 4544.0f, -512.0f))
            || (state.phase == 28 && !walkIndoor(1950.0f, 4864.0f, 0.0f))
            || (state.phase == 34 && !walkIndoor(5200.0f, 4864.0f, 0.0f))
            || (state.phase == 36 && !walkIndoor(576.0f, 6800.0f, 177.0f))) return;
        if (state.phase == 22) next(23);
        else if (state.phase == 28) next(29);
        else if (state.phase == 34) next(35);
        else if (state.phase == 36) next(37);
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            aimOutdoor(-9744.0f, -1712.0f, 742.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 325,
                "Normal Goblinwatch preparation did not enter New Sorpigal Temple");
            if (std::any_of(runtime.activeEventDialog().actions.begin(), runtime.activeEventDialog().actions.end(),
                [](const EventDialogAction &action)
                    { return action.kind == EventDialogActionKind::HouseProprietor && action.id == 325; }))
                selectQuestDialog(runtime, EventDialogActionKind::HouseProprietor, 325);
            next(3);
            break;
        case 3:
            while (state.healingMember < pParty->members().size())
            {
                const Character &member = pParty->members()[state.healingMember];
                if (member.conditions.any() || member.health < Party::effectiveMaximumHealth(member)
                    || member.spellPoints < Party::effectiveMaximumSpellPoints(member)) break;
                ++state.healingMember;
            }
            if (state.healingMember == pParty->members().size())
            {
                save("goblinwatch-party-healed");
                captures.push_back("goblinwatch-party-healed");
                next(6);
            }
            else
            {
                selectMember(state.healingMember);
                next(4);
            }
            break;
        case 4:
        {
            const HouseEntry *pHouse = runtime.houseTable()->get(325);
            require(pHouse && pHouse->templeCanHeal, "New Sorpigal Temple cannot heal the normal party");
            const int price = PriceCalculator::templeHealPrice(pParty->activeMember(), *pHouse, pWorld->gameMinutes());
            require(price > 0 && pParty->gold() >= price, "Normal party cannot afford New Sorpigal healing");
            state.expectedGold -= price;
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TempleHeal));
            next(5);
            break;
        }
        case 5:
        {
            const Character &member = pParty->members()[state.healingMember];
            require(member.conditions.none() && member.health == Party::effectiveMaximumHealth(member)
                && member.spellPoints == Party::effectiveMaximumSpellPoints(member)
                && pParty->gold() == state.expectedGold, "Temple healing changed more than the quoted normal service");
            ++state.healingMember;
            next(3);
            break;
        }
        case 6:
            if (runtime.activeEventDialog().isActive) close();
            else { loadRoute("02-temple-to-town-hall.tsv"); next(7); }
            break;
        case 8:
            aimOutdoor(-11240.0f, -6536.0f, 304.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(9);
            break;
        case 9:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 208,
                "Normal route did not enter New Sorpigal Town Hall");
            selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 1076);
            next(10);
            break;
        case 10:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 1316);
            require(pParty->hasQuestBit(1107) && runtime.heldInventoryItem().active
                    && runtime.heldInventoryItem().item.objectDescriptionId == 2109
                    && !pParty->hasQuestBit(1324),
                "Town Hall did not grant the original quest and Goblinwatch key");
            {
                const auto portrait = runtime.resolvePartyPortraitRect(width, height, 0);
                require(portrait.has_value(), "The Goblinwatch key has no recipient portrait");
                GameplayInputFrame input;
                input.screenWidth = width;
                input.screenHeight = height;
                input.pointerX = portrait->x + portrait->width * 0.5f;
                input.pointerY = portrait->y + portrait->height * 0.5f;
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            require(!runtime.heldInventoryItem().active && pParty->inventoryItemCount(2109) == 1,
                "Normal portrait delivery did not store the original Goblinwatch key");
            state.goldBeforeReward = pParty->gold();
            state.experienceBeforeReward.clear();
            for (const Character &member : pParty->members()) state.experienceBeforeReward.push_back(member.experience);
            save("goblinwatch-quest-accepted");
            captures.push_back("goblinwatch-quest-accepted");
            next(11);
            break;
        case 11:
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 1317);
            require(pParty->hasQuestBit(1107) && pParty->gold() == state.goldBeforeReward
                && pParty->inventoryItemCount(2163) == 0,
                "Unmet Goblinwatch hand-in completed or paid early");
            for (size_t index = 0; index < pParty->members().size(); ++index)
                require(pParty->members()[index].experience == state.experienceBeforeReward[index]
                    && !pParty->members()[index].awards.contains(95),
                    "Unmet Goblinwatch hand-in changed member rewards");
            save("goblinwatch-early-handin-rejected");
            captures.push_back("goblinwatch-early-handin-rejected");
            next(12);
            break;
        case 12:
            if (runtime.activeEventDialog().isActive) close(); else next(13);
            break;
        case 13:
            if (runtime.activeEventDialog().isActive) close();
            else { loadRoute("03-town-hall-to-goblinwatch.tsv"); next(15); }
            break;
        case 16:
            aimOutdoor(-18432.0f, -15200.0f, 1728.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(17);
            break;
        case 17:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Actual Goblinwatch entrance did not open its normal confirmation");
            require(pParty->hasQuestBit(1324) && pParty->inventoryItemCount(2109) == 0,
                "Goblinwatch entrance did not consume its original key and set Q1324");
            save("goblinwatch-entry-confirmation");
            captures.push_back("goblinwatch-entry-confirmation");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            next(18);
            break;
        case 18:
            if (frame < state.phaseFrame + 30) return;
            require(session.currentMapFileName() == "6d01.blv" && pWorld->isIndoorMap()
                && pParty->hasQuestBit(1107) && pParty->hasQuestBit(1324),
                "Goblinwatch transition lost the quest or destination");
            save("goblinwatch-entered");
            captures.push_back("goblinwatch-entered");
            next(22);
            break;
        case 23:
            aimIndoor(-2192.0f, 4544.0f, -416.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(24);
            break;
        case 24:
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene && pScene->worldRuntime().activeChestView(),
                "Normal Goblinwatch interaction did not open original chest 1");
            takeChestItem(2163);
            require(pParty->inventoryItemCount(2163) == 1 && pParty->hasQuestBit(1107),
                "Goblinwatch code pickup lost the scroll or completed the quest early");
            save("goblinwatch-scroll-taken");
            captures.push_back("goblinwatch-scroll-taken");
            next(25);
            break;
        }
        case 25:
            close();
            next(26);
            break;
        case 26:
            if (dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get())->worldRuntime().activeChestView())
                return;
            next(28);
            break;
        case 29:
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene && pScene->eventRuntimeState(), "Goblinwatch panel lost event state");
            pressButton(2184.0f, 4856.0f, 136.0f); // G is deliberately wrong before reset.
            require(pScene->eventRuntimeState()->mapVars[7] == 1 && pParty->hasQuestBit(1107)
                && !pParty->members()[0].awards.contains(95),
                "Wrong Goblinwatch input completed the quest or failed to change its original map variable");
            save("goblinwatch-wrong-button");
            captures.push_back("goblinwatch-wrong-button");
            next(30);
            break;
        }
        case 30:
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            pressButton(2152.0f, 4840.0f, 104.0f); // P reset.
            for (size_t index = 2; index <= 14; ++index)
                require(pScene->eventRuntimeState()->mapVars[index] == 0,
                    "Goblinwatch reset did not clear the original button variables");
            next(31);
            break;
        }
        case 31:
        {
            static constexpr std::array<std::array<float, 3>, 6> buttons = {{{2152, 4872, 104},
                {2168, 4888, 120}, {2168, 4840, 120}, {2200, 4872, 152}, {2152, 4856, 104},
                {2184, 4856, 136}}}; // N I L B O G
            static constexpr std::array<size_t, 6> vars = {13, 9, 12, 2, 14, 7};
            require(state.combinationStep < buttons.size(), "Goblinwatch combination step overflow");
            pressButton(buttons[state.combinationStep][0], buttons[state.combinationStep][1],
                buttons[state.combinationStep][2]);
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene->eventRuntimeState()->mapVars[vars[state.combinationStep]] == 1,
                "Goblinwatch NILBOG button did not set its original map variable");
            ++state.combinationStep;
            state.phaseFrame = frame;
            if (state.combinationStep == buttons.size())
            {
                save("goblinwatch-nilbog-entered");
                captures.push_back("goblinwatch-nilbog-entered");
                next(32);
            }
            break;
        }
        case 32:
            if (frame < state.phaseFrame + 180) return;
            next(34);
            break;
        case 35:
            require(pWorld->partyX() > 3500.0f && pParty->hasQuestBit(1107)
                && pParty->inventoryItemCount(2163) == 1,
                "NILBOG did not admit the normal party through the six-door vault");
            save("goblinwatch-vault-entered");
            captures.push_back("goblinwatch-vault-entered");
            next(36);
            break;
        case 37:
            aimIndoor(576.0f, 6944.0f, 320.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(38);
            break;
        case 38:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Goblinwatch exit did not open its normal return confirmation");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            loadRoute("04-goblinwatch-to-town-hall.tsv");
            next(39);
            break;
        case 40:
            aimOutdoor(-11240.0f, -6536.0f, 304.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(41);
            break;
        case 41:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 208,
                "Goblinwatch return did not enter New Sorpigal Town Hall");
            selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 1076);
            next(42);
            break;
        case 42:
            state.goldBeforeReward = pParty->gold();
            state.experienceBeforeReward.clear();
            for (const Character &member : pParty->members()) state.experienceBeforeReward.push_back(member.experience);
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 1317);
            next(43);
            break;
        case 43:
            require(!pParty->hasQuestBit(1107) && pParty->hasQuestBit(1324)
                && pParty->inventoryItemCount(2163) == 1 && pParty->gold() == state.goldBeforeReward + 2000,
                "Goblinwatch hand-in changed the original item, quest bits or 2000-gold reward");
            for (size_t index = 0; index < pParty->members().size(); ++index)
                require(pParty->members()[index].experience == state.experienceBeforeReward[index] + 2000
                    && pParty->members()[index].awards.contains(95),
                    "Goblinwatch hand-in changed the original member experience or award95 reward");
            save("mm6-goblinwatch-complete");
            captures.push_back("goblinwatch-reward-complete");
            state.complete = true;
            std::cout << "Goblinwatch continuous quest passed: walked=" << state.walkedDistance
                << " gold=" << pParty->gold() << " item2163=" << pParty->inventoryItemCount(2163) << '\n';
            break;
        default: throw std::runtime_error("Invalid Goblinwatch quest phase");
        }
    }

    struct Mm7CastleCheckState
    {
        unsigned phase = 0;
        unsigned wallRecoverySteps = 0;
        unsigned bypassSteps = 0;
        unsigned bypassTurns = 0;
        size_t templeMember = 0;
        unsigned rangedRecoveryMoves = 0;
        unsigned rangedBlockedDirections = 0;
        int templeGoldBefore = 0;
        int templePrice = 0;
        float bypassYaw = 0.0f;
        unsigned resupplyTrips = 0;
        bool resupplying = false;
        bool approachingDoor = false;
        std::set<uint16_t> openedDoors;
        int lodgingGoldBefore = 0;
        int provisionGoldBefore = 0;
        int provisionFoodBefore = 0;
        uint32_t phaseFrame = 0;
        uint32_t routePlannedFrame = 0;
        uint32_t doorTriggeredFrame = 0;
        size_t waypoint = 0;
        unsigned stalledFrames = 0;
        unsigned streetDialogs = 0;
        unsigned indoorInteractions = 0;
        unsigned rests = 0;
        unsigned restPhase = 0;
        size_t training = 0;
        size_t skillMember = 0;
        unsigned skillUpgrades = 0;
        float walkedDistance = 0.0f;
        int goldBeforeReward = 0;
        int restStartingFood = 0;
        int restFoodCost = 0;
        size_t initialTargets = 0;
        size_t actorRouteTarget = size_t(-1);
        std::array<uint64_t, 4> preparationExperience = {};
        std::array<uint32_t, 4> preparationSkillPoints = {};
        std::vector<uint64_t> experienceBeforeReward;
        std::unordered_map<uint32_t, int32_t> historyBefore;
        std::vector<std::array<float, 2>> route;
        std::vector<PathPoint> indoorRoute;
        PathPlanStatus indoorRouteStatus = PathPlanStatus::NotRequested;
        bool complete = false;
        bool combatCaptured = false;
        bool restCaptured = false;
    };

    static void checkMm7Castle(GameApplication &game, uint32_t frame, int width, int height,
        bool reload, bool continuation, unsigned historyMode, Mm7CastleCheckState &state,
        std::vector<std::string> &captures)
    {
        const auto require = [](bool ok, const std::string &message)
        {
            if (!ok) throw std::runtime_error(message);
        };
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        require(pParty && pWorld && pParty->members().size() == 4 && pParty->hasActableMember(),
            "Harmondale Castle needs the surviving normal four-member MM7 party");
        const auto save = [&](const std::string &name)
        {
            require(game.quickSaveToPath(std::filesystem::current_path() / (name + ".oysav"),
                "MM7哈蒙代尔城堡清理", {}), "Cannot preserve Harmondale Castle quest evidence");
        };
        const auto next = [&](unsigned phase)
        {
            state.phase = phase;
            state.phaseFrame = frame;
            state.waypoint = 0;
            state.stalledFrames = 0;
            state.approachingDoor = false;
            state.bypassSteps = 0;
            state.indoorRoute.clear();
            state.indoorRouteStatus = PathPlanStatus::NotRequested;
            state.actorRouteTarget = size_t(-1);
        };
        const auto close = [&]()
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
        };
        const auto click = [&](float x, float y)
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = x;
            input.pointerY = y;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto resolve = [&](const char *id)
        {
            const auto *pLayout = runtime.findHudLayoutElement(id);
            require(pLayout != nullptr, std::string("Missing Harmondale character control: ") + id);
            const auto rect = runtime.resolveHudLayoutElement(id, width, height, pLayout->width, pLayout->height);
            require(rect.has_value(), std::string("Cannot place Harmondale character control: ") + id);
            return *rect;
        };
        static constexpr std::array<const char *, 4> preparationSkills =
            {"LeatherArmor", "BodyMagic", "FireMagic", "Bow"};
        const auto clickPreparationSkill = [&]()
        {
            require(runtime.characterScreen().open
                && runtime.characterScreen().page == GameplayUiController::CharacterPage::Skills
                && runtime.selectedCharacterScreenSourceIndex() == state.skillMember,
                "Harmondale skill click needs the selected character's visible skill page");
            const CharacterSkillUiData rows = buildCharacterSkillUiData(
                pParty->member(state.skillMember), runtime.characterInspectTable());
            const auto font = runtime.findHudFont("Lucida");
            const float naturalHeight = font ? float(std::max(1, font->fontHeight - 3)) : 11.0f;
            const CharacterSkillLayout layout = layoutCharacterSkillRows(runtime, width, height, naturalHeight,
                {rows.weaponRows.size(), rows.magicRows.size(), rows.armorRows.size(), rows.miscRows.size()});
            const std::vector<CharacterSkillUiRow> *pGroup = &rows.weaponRows;
            const char *pRegionId = "CharacterSkillsWeaponsListRegion";
            const char *pLevelId = "CharacterSkillsWeaponsLevelHeader";
            if (state.skillMember == 0)
            {
                pGroup = &rows.armorRows;
                pRegionId = "CharacterSkillsArmorListRegion";
                pLevelId = "CharacterSkillsArmorLevelHeader";
            }
            else if (state.skillMember == 1 || state.skillMember == 2)
            {
                pGroup = &rows.magicRows;
                pRegionId = "CharacterSkillsMagicListRegion";
                pLevelId = "CharacterSkillsMagicLevelHeader";
            }
            const auto row = std::find_if(pGroup->begin(), pGroup->end(), [&](const CharacterSkillUiRow &entry)
                { return entry.canonicalName == preparationSkills[state.skillMember]; });
            require(row != pGroup->end(), "Harmondale preparation skill is absent from the visible group");
            const auto region = resolve(pRegionId);
            const auto level = resolve(pLevelId);
            const float rowHeight = layout.rowHeight * region.scale;
            GameplayResolvedHudLayoutElement rect = {region.x,
                region.y + float(row - pGroup->begin()) * rowHeight,
                level.x + level.width - region.x, rowHeight, region.scale};
            require(layout.placeVisible(rect), "Harmondale preparation skill is outside the visible viewport");
            click(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
        };
        const auto clickRest = [&](const char *pLayoutId)
        {
            const auto *pLayout = runtime.findHudLayoutElement(pLayoutId);
            require(pLayout != nullptr, "Harmondale rest control is missing");
            const auto button = runtime.resolveHudLayoutElement(pLayoutId, width, height,
                pLayout->width, pLayout->height);
            require(button.has_value(), "Harmondale rest control cannot be placed");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = button->x + button->width * 0.5f;
            input.pointerY = button->y + button->height * 0.5f;
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        };
        const auto loadRoute = [&](const char *name)
        {
            state.route.clear();
            const std::filesystem::path path = std::filesystem::current_path().parent_path()
                / "routes" / "outdoor" / name;
            std::ifstream stream(path);
            require(stream.good(), "Missing Harmondale outdoor route: " + path.string());
            std::string line;
            while (std::getline(stream, line))
            {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream values(line);
                float x = 0.0f, y = 0.0f, z = 0.0f;
                require(bool(values >> x >> y >> z) && std::isfinite(x) && std::isfinite(y)
                    && std::isfinite(z), "Invalid Harmondale outdoor waypoint");
                state.route.push_back({x, y});
            }
            require(state.route.size() >= 2, "Harmondale outdoor route is empty");
            state.waypoint = 0;
        };
        const auto walkOutdoor = [&]()
        {
            require(session.currentMapFileName() == "7out02.odm" && !pWorld->isIndoorMap(),
                "Harmondale outdoor walk changed maps unexpectedly");
            if (runtime.activeEventDialog().isActive)
            {
                require(!runtime.activeEventDialog().isHouseDialog
                    && runtime.activeEventDialog().presentation != EventDialogPresentation::Transition
                    && ++state.streetDialogs <= 40,
                    "Unexpected building or repeated dialogue on Harmondale route");
                close();
                return false;
            }
            while (state.waypoint < state.route.size()
                && std::hypot(state.route[state.waypoint][0] - pWorld->partyX(),
                    state.route[state.waypoint][1] - pWorld->partyY()) < 25.0f)
            {
                ++state.waypoint;
            }
            if (state.waypoint == state.route.size()) return true;
            const float x = pWorld->partyX(), y = pWorld->partyY();
            const float dx = state.route[state.waypoint][0] - x;
            const float dy = state.route[state.waypoint][1] - y;
            const float distance = std::hypot(dx, dy);
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(distance / 1500.0f, 1.0f / 128.0f, 0.025f));
            const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.5f ? state.stalledFrames + 1 : 0;
            if (frame % 180 == 0)
                std::cout << "Harmondale outdoor walk: phase=" << state.phase
                    << " waypoint=" << state.waypoint << " position=" << pWorld->partyX() << ','
                    << pWorld->partyY() << ',' << pWorld->partyFootZ() << " remaining=" << distance << '\n';
            require(state.stalledFrames < 120,
                "Harmondale outdoor route stopped at collision geometry or an actor");
            return false;
        };
        const auto aimOutdoor = [&](float x, float y, float z)
        {
            const float dx = x - pWorld->partyX(), dy = y - pWorld->partyY();
            game.m_outdoorGameView.setCameraAngles(std::atan2(dy, dx),
                std::atan2(z - pWorld->partyFootZ() - 176.0f, std::hypot(dx, dy)));
        };
        const auto useCastleEntrance = [&]()
        {
            // Trigger chooses a nearby interactable, which can be a guard. The ordinary Use action
            // selects the precise crosshair context target; aim at an unobscured part of the door.
            const float yaw = std::atan2(12544.0f - pWorld->partyY(), -18368.0f - pWorld->partyX());
            for (float pitch : {0.5f, 0.25f, 0.0f, -0.35f, -0.6f})
            {
                for (float offset : {0.0f, -0.25f, 0.25f, -0.5f, 0.5f})
                {
                    game.m_outdoorGameView.setCameraAngles(yaw + offset, pitch);
                    const GameplayWorldPickRequest request = pWorld->buildWorldPickRequest({
                        .screenX = width * 0.5f, .screenY = height * 0.5f,
                        .screenWidth = width, .screenHeight = height, .includeRay = true,
                    });
                    const GameplayWorldHit hit = pWorld->pickMouseInteractionTarget(request);
                    if (!hit.eventTarget || hit.eventTarget->triggeredEventId != 301
                        || !pWorld->canActivateWorldHit(hit, GameplayInteractionMethod::Keyboard)) continue;
                    pressQuestGameplay(session, width, height, KeyboardAction::Use);
                    std::cout << "Harmondale precision entrance Use: yaw=" << yaw + offset
                        << " pitch=" << pitch << '\n';
                    return;
                }
            }
            throw std::runtime_error("Harmondale entrance has no reachable visible door aim");
        };
        const auto aimIndoor = [&](float x, float y, float z)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Harmondale indoor aim lost the scene runtime");
            const IndoorMoveState &position = pScene->partyRuntime().movementState();
            game.m_indoorRenderer.setCameraAngles(std::atan2(y - position.y, x - position.x),
                std::atan2(z - position.eyeZ(), std::hypot(x - position.x, y - position.y)));
        };
        struct ActorCounts
        {
            size_t targets = 0;
            size_t targetsAlive = 0;
            size_t golems = 0;
            size_t golemsVisible = 0;
            size_t invasion = 0;
            size_t invasionVisible = 0;
        };
        const auto actorCounts = [&]()
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Harmondale actor count lost the indoor scene");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            const MapDeltaData *pDelta = world.mapDeltaData();
            require(pDelta && pDelta->actors.size() == world.mapActorCount(),
                "Harmondale actor delta and runtime counts differ");
            ActorCounts counts;
            for (size_t index = 0; index < pDelta->actors.size(); ++index)
            {
                GameplayRuntimeActorState actor;
                require(world.actorRuntimeState(index, actor), "Cannot inspect Harmondale runtime actor");
                const uint32_t group = pDelta->actors[index].group;
                if (group == 56)
                {
                    ++counts.targets;
                    if (pDelta->actors[index].hp > 0) ++counts.targetsAlive;
                }
                else if (group == 57)
                {
                    ++counts.golems;
                    if (!actor.isInvisible) ++counts.golemsVisible;
                }
                else if (group == 60)
                {
                    ++counts.invasion;
                    if (!actor.isInvisible) ++counts.invasionVisible;
                }
            }
            return counts;
        };
        const auto cast = [&](size_t member, uint32_t spellId, std::optional<size_t> actor,
            std::optional<size_t> character)
        {
            const SpellEntry *pSpell = game.m_gameDataLoader.getSpellTable().findById(spellId);
            require(pSpell && runtime.tryCastSpellFromMember(member, spellId, pSpell->name),
                "Cannot start a normal Harmondale combat spell");
            GameplaySpellService &spells = runtime.spellService();
            PartySpellCastRequest request = spells.makePendingTargetSelectionRequest();
            request.targetActorIndex = actor;
            request.targetCharacterIndex = character;
            require(spells.validatePendingTargetSelectionRequest(runtime, request),
                "Harmondale combat spell target was rejected");
            const int mana = pParty->members()[member].spellPoints;
            const auto result = spells.resolvePendingTargetSelectionCast(runtime, request);
            require(result.castResult.succeeded() && pParty->members()[member].spellPoints < mana,
                "Harmondale combat spell did not consume normal mana");
            spells.clearPendingTargetSelection(runtime);
            session.updateGameplay({}, 0.0f);
        };
        const auto healWhileWalking = [&]()
        {
            const Character &cleric = pParty->members()[1];
            if (session.turnBasedCombatRuntime().active() || !pParty->canSelectMemberInGameplay(1)
                || cleric.recoverySecondsRemaining > 0.0f || cleric.spellPoints < 2) return false;
            size_t weakest = size_t(-1);
            float fraction = 0.85f;
            for (size_t index = 0; index < pParty->members().size(); ++index)
            {
                const Character &member = pParty->members()[index];
                const float health = float(member.health) / float(std::max(1, member.maxHealth));
                if (health < fraction) { fraction = health; weakest = index; }
            }
            if (cleric.health < 18) weakest = 1;
            if (weakest == size_t(-1)) return false;
            cast(1, 68, std::nullopt, weakest);
            return true;
        };
        const auto fightIndoor = [&](bool preserveLast)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Harmondale combat lost its indoor scene");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            const MapDeltaData *pDelta = world.mapDeltaData();
            require(pDelta != nullptr, "Harmondale combat lost its actor delta");
            size_t alive = 0;
            size_t target = size_t(-1);
            float nearest = std::numeric_limits<float>::max();
            float bestScore = std::numeric_limits<float>::max();
            GameplayRuntimeActorState selected;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                if (index >= pDelta->actors.size() || pDelta->actors[index].group != 56) continue;
                GameplayRuntimeActorState actor;
                if (pDelta->actors[index].hp <= 0 || !world.actorRuntimeState(index, actor) || actor.isDead) continue;
                ++alive;
                if (actor.isInvisible || !actor.hostileToParty
                    || !world.partyAttackActorHasLineOfSight(index)) continue;
                const float distance = std::hypot(actor.preciseX - world.partyX(),
                    actor.preciseY - world.partyY());
                // Prefer nearby threats; low-HP rats are only a tie-break within a local encounter.
                const int priority = actor.monsterId == 385 ? 0 : actor.monsterId == 386 ? 1 : 2;
                const float score = distance + float(priority) * 125.0f;
                if (score < bestScore)
                {
                    bestScore = score;
                    nearest = distance;
                    target = index;
                    selected = actor;
                }
            }
            TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
            if ((preserveLast && alive <= 1) || target == size_t(-1))
            {
                if (!turn.active()) return false;
                if (turn.canToggleOff(*pParty)) pressQuestGameplay(session, width, height, KeyboardAction::Combat);
                else if (turn.stage() == TurnBasedCombatStage::Movement || turn.playerActionReady(*pParty))
                    pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                return true;
            }
            aimIndoor(selected.preciseX, selected.preciseY, selected.preciseZ + selected.height * 0.5f);
            const auto rangedLaneClear = [&](size_t member, float x, float y)
            {
                // The party-center LOS is insufficient near a corner: ordinary missiles start
                // at each member's lateral offset, with different spell/bow heights and radius16.
                const float yaw = std::atan2(selected.preciseY - y, selected.preciseX - x);
                const float rightX = -std::sin(yaw), rightY = std::cos(yaw);
                const float offset = (1.5f - float(member)) * 28.0f;
                const auto &position = pScene->partyRuntime().movementState();
                for (float margin : {-16.0f, 0.0f, 16.0f})
                {
                    const GameplayWorldPoint source{x + rightX * (offset + margin),
                        y + rightY * (offset + margin), world.partyFootZ() + (member == 2 ? 96.0f : 64.0f)};
                    const GameplayWorldPoint destination{selected.preciseX + rightX * margin,
                        selected.preciseY + rightY * margin,
                        selected.preciseZ + std::max(48.0f, float(selected.height) * 0.6f)};
                    if (!world.hasIndoorCombatLineOfSight(source, position.eyeSectorId,
                            destination, pDelta->actors[target].sectorId)) return false;
                }
                return true;
            };
            const bool mageLane = rangedLaneClear(2, world.partyX(), world.partyY());
            const bool bowLane = rangedLaneClear(3, world.partyX(), world.partyY());
            if (mageLane && bowLane)
            {
                state.rangedRecoveryMoves = 0;
                state.rangedBlockedDirections = 0;
            }
            if (!turn.active())
            {
                pressQuestGameplay(session, width, height, KeyboardAction::Combat);
                if (!state.combatCaptured)
                {
                    captures.push_back("castle-combat");
                    state.combatCaptured = true;
                }
                return true;
            }
            if (turn.hasPendingActions() || turn.stage() == TurnBasedCombatStage::Wait) return true;
            if (turn.stage() == TurnBasedCombatStage::Movement)
            {
                if ((!mageLane || !bowLane) && turn.movementActionPoints() >= 26)
                {
                    require(++state.rangedRecoveryMoves <= 128,
                        "Harmondale ordinary combat movement could not clear the missile lanes");
                    const float x = world.partyX(), y = world.partyY();
                    const float facing = std::atan2(selected.preciseY - y, selected.preciseX - x);
                    const std::array<float, 4> offsets{1.57079632679f, -1.57079632679f, 0.0f, 3.14159265359f};
                    float moveYaw = facing + offsets[state.rangedBlockedDirections % offsets.size()];
                    int bestLanes = -1;
                    for (size_t candidate = 0; candidate < offsets.size(); ++candidate)
                    {
                        const size_t direction = (candidate + state.rangedBlockedDirections) % offsets.size();
                        const float yaw = facing + offsets[direction];
                        const float nextX = x + std::cos(yaw) * 96.0f;
                        const float nextY = y + std::sin(yaw) * 96.0f;
                        const int lanes = int(rangedLaneClear(2, nextX, nextY))
                            + int(rangedLaneClear(3, nextX, nextY));
                        if (lanes > bestLanes) { bestLanes = lanes; moveYaw = yaw; }
                    }
                    game.m_indoorRenderer.setCameraAngles(moveYaw, 0.0f);
                    pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.025f);
                    const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
                    state.walkedDistance += moved;
                    if (moved < 0.5f) ++state.rangedBlockedDirections;
                    std::cout << "Harmondale missile-lane movement: actor=" << target
                        << " mage_clear=" << mageLane << " bow_clear=" << bowLane
                        << " moved=" << moved << " position=" << world.partyX() << ',' << world.partyY() << '\n';
                }
                else if (nearest < 425.0f && turn.movementActionPoints() >= 26)
                {
                    const float x = world.partyX(), y = world.partyY();
                    pressQuestGameplay(session, width, height, KeyboardAction::Backward, 0.025f);
                    if (std::hypot(world.partyX() - x, world.partyY() - y) < 0.5f)
                        pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                }
                else pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                return true;
            }
            if (!turn.playerActionReady(*pParty)) return true;
            const size_t member = pParty->activeMemberIndex();
            if (member == 1 && pParty->members()[member].knowsSpell(68)
                && pParty->members()[member].spellPoints >= 2)
            {
                size_t weakest = size_t(-1);
                float fraction = pParty->members()[member].health < 18 ? 1.01f : 0.85f;
                for (size_t index = 0; index < pParty->members().size(); ++index)
                {
                    const Character &candidate = pParty->members()[index];
                    const float health = float(candidate.health) / float(std::max(1, candidate.maxHealth));
                    if (health < fraction) { fraction = health; weakest = index; }
                }
                if (pParty->members()[member].health < 18) weakest = member;
                if (weakest != size_t(-1))
                {
                    cast(member, 68, std::nullopt, weakest);
                    return true;
                }
            }
            if (member == 2 && pParty->members()[member].knowsSpell(2)
                && pParty->members()[member].spellPoints >= 2)
            {
                if (mageLane) cast(member, 2, target, std::nullopt);
                else pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                return true;
            }
            // The archer's learned Air spell is Feather Fall, not an offensive actor-targeted spell.
            // Use the normally equipped bow instead of submitting an invalid spell target.
            if (nearest < 425.0f || (pParty->equippedItemId(member, EquipmentSlot::Bow) != 0 && bowLane))
                pressQuestGameplay(session, width, height, KeyboardAction::Attack);
            else pressQuestGameplay(session, width, height, KeyboardAction::Pass);
            return true;
        };
        const auto nearestTarget = [&]() -> std::optional<std::array<float, 4>>
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene != nullptr, "Harmondale target search lost the indoor scene");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            const MapDeltaData *pDelta = world.mapDeltaData();
            require(pDelta != nullptr, "Harmondale target search lost its actor delta");
            float nearest = std::numeric_limits<float>::max();
            std::optional<std::array<float, 4>> result;
            for (size_t index = 0; index < world.mapActorCount(); ++index)
            {
                if (index >= pDelta->actors.size() || pDelta->actors[index].group != 56) continue;
                GameplayRuntimeActorState actor;
                if (pDelta->actors[index].hp <= 0 || !world.actorRuntimeState(index, actor)
                    || actor.isDead || actor.isInvisible) continue;
                if (state.actorRouteTarget == index)
                    return std::array<float, 4>{float(index), actor.preciseX, actor.preciseY, actor.preciseZ};
                const float distance = std::hypot(actor.preciseX - world.partyX(),
                    actor.preciseY - world.partyY());
                if (distance < nearest)
                {
                    nearest = distance;
                    result = std::array<float, 4>{float(index), actor.preciseX, actor.preciseY, actor.preciseZ};
                }
            }
            return result;
        };
        const auto walkIndoor = [&](float targetX, float targetY, float targetZ, size_t actorTarget)
        {
            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
            require(pScene && session.currentMapFileName() == "7d29.blv",
                "Harmondale indoor walk left 7d29");
            IndoorWorldRuntime &world = pScene->worldRuntime();
            if (state.doorTriggeredFrame != 0 && frame < state.doorTriggeredFrame + 384) return false;
            const float x = world.partyX(), y = world.partyY();
            if (state.phase == 55 && (state.stalledFrames > 0 || state.bypassSteps > 0))
            {
                // A living actor can seal the corridor. Use normal real-time combat to clear it;
                // repeatedly walking into its body or restarting a full encounter cannot escape.
                for (size_t index = 0; index < world.mapActorCount(); ++index)
                {
                    GameplayRuntimeActorState actor;
                    if (!world.actorRuntimeState(index, actor) || actor.isDead || actor.isInvisible
                        || !actor.hostileToParty || !world.partyAttackActorHasLineOfSight(index)
                        || std::hypot(actor.preciseX - x, actor.preciseY - y) > 180.0f) continue;
                    aimIndoor(actor.preciseX, actor.preciseY, actor.preciseZ + actor.height * 0.5f);
                    const Character &mage = pParty->members()[2];
                    if (pParty->canSelectMemberInGameplay(2) && mage.recoverySecondsRemaining <= 0.0f
                        && mage.spellPoints >= 2) cast(2, 2, index, std::nullopt);
                    else pressQuestGameplay(session, width, height, KeyboardAction::Attack);
                    std::cout << "Harmondale retreat obstruction attack: actor=" << index << '\n';
                    break;
                }
            }
            if (state.bypassSteps > 0)
            {
                game.m_indoorRenderer.setCameraAngles(state.bypassYaw, 0.0f);
                if (continuation) pScene->partyRuntime().setCollisionTraceEnabled(true, "7d29.blv");
                pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.05f);
                const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
                state.walkedDistance += moved;
                std::cout << "Harmondale sidestep result: yaw=" << state.bypassYaw
                    << " moved=" << moved << " position=" << world.partyX() << ',' << world.partyY() << '\n';
                if (moved < 0.1f)
                {
                    require(++state.bypassTurns < 8, "Harmondale ordinary sidestep cannot clear the obstacle");
                    state.bypassYaw += 1.57079632679f;
                }
                else --state.bypassSteps;
                state.indoorRoute.clear();
                return false;
            }
            if (std::hypot(targetX - x, targetY - y) < 170.0f
                && std::abs(targetZ - world.partyFootZ()) < 256.0f) return true;
            if (!state.approachingDoor && (state.actorRouteTarget != actorTarget
                || (actorTarget != size_t(-1) && frame > state.routePlannedFrame + 1800)))
            {
                state.indoorRoute.clear();
                state.actorRouteTarget = actorTarget;
            }
            if (state.indoorRoute.empty())
            {
                const auto &map = game.m_gameDataLoader.getSelectedMap();
                require(map && map->indoorMapData, "Harmondale path planner lost the authored indoor map");
                const std::vector<IndoorVertex> vertices = buildIndoorMechanismAdjustedVertices(
                    *map->indoorMapData, world.mapDeltaData(), world.eventRuntimeState());
                IndoorPathMapBuildResult built = IndoorPathfindingBuilder::buildPathMap(
                    *map->indoorMapData, vertices, world.mapDeltaData());
                if (state.phase == 55)
                {
                    // Include current hostile bodies in the driver's proposal only. Real party
                    // movement still uses the unchanged collision controller.
                    auto facets = built.pathMap.facets();
                    for (const auto &actor : world.actorMovementCollidersForPartyMovement())
                    {
                        const float radius = actor.radius / std::cos(0.3926990817f);
                        for (unsigned side = 0; side < 8; ++side)
                        {
                            const float a = float(side) * 0.78539816339f;
                            const float b = float(side + 1) * 0.78539816339f;
                            const float ax = actor.x + radius * std::cos(a);
                            const float ay = actor.y + radius * std::sin(a);
                            const float bx = actor.x + radius * std::cos(b);
                            const float by = actor.y + radius * std::sin(b);
                            PathFacet face;
                            face.kind = PathFacetKind::Wall;
                            face.dynamic = true;
                            face.vertices = {{ax, ay, actor.z}, {bx, by, actor.z},
                                {bx, by, actor.z + actor.height}, {ax, ay, actor.z + actor.height}};
                            facets.push_back(std::move(face));
                        }
                    }
                    built.pathMap.setFacets(std::move(facets));
                }
                PathPlanRequest request;
                request.source = {x, y, world.partyFootZ()};
                request.target = {targetX, targetY, targetZ};
                request.object.radius = 37.0f;
                request.object.stepHeight = 128.0f;
                request.object.stepLength = 128.0f;
                request.nodeLimit = 200000;
                request.allowPartialPath = true;
                PathPlanner planner;
                PathPlanResult result = planner.plan(built.pathMap, request);
                // The castle has doorways narrower than the coarse 128-unit search grid.
                // Refine a failed proposal without changing the real party radius or collision rules.
                for (float step : {48.0f, 24.0f})
                {
                    if (result.status == PathPlanStatus::Success) break;
                    request.object.stepLength = step;
                    const PathPlanResult refined = planner.plan(built.pathMap, request);
                    if (refined.status == PathPlanStatus::Success
                        || refined.debug.bestDistance3d < result.debug.bestDistance3d) result = refined;
                    std::cout << "Harmondale refined plan: step=" << step
                        << " status=" << int(refined.status) << " nodes=" << refined.analyzedNodeCount
                        << " best_distance=" << refined.debug.bestDistance2d << '\n';
                }
                if ((result.status == PathPlanStatus::NoRoute
                        || (result.status == PathPlanStatus::Partial && result.analyzedNodeCount < 128))
                    && result.debug.sourceValid && result.debug.targetValid)
                {
                    require(state.wallRecoverySteps < 8,
                        "Harmondale normal wall recovery could not find a navigable starting point");
                    // A wall-adjacent start can also produce a short partial path along that wall.
                    // It is not evidence that the destination requires opening another door.
                    // Try short ordinary walking inputs; never substitute the planner's position into the game.
                    const float yaw = std::atan2(targetY - y, targetX - x)
                        + float(state.wallRecoverySteps++) * 0.78539816339f;
                    game.m_indoorRenderer.setCameraAngles(yaw, 0.0f);
                    pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.025f);
                    const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
                    state.walkedDistance += moved;
                    std::cout << "Harmondale wall recovery: step=" << state.wallRecoverySteps
                        << " nodes=" << result.analyzedNodeCount
                        << " moved=" << moved << " position=" << world.partyX() << ','
                        << world.partyY() << ',' << world.partyFootZ() << '\n';
                    return false;
                }
                require(result.status == PathPlanStatus::Success || result.status == PathPlanStatus::Partial,
                    "Harmondale indoor route has no navigable proposal");
                state.wallRecoverySteps = 0;
                state.indoorRoute = result.waypoints;
                state.indoorRouteStatus = result.status;
                state.routePlannedFrame = frame;
                state.waypoint = 0;
                while (state.waypoint < state.indoorRoute.size()
                    && std::hypot(state.indoorRoute[state.waypoint].x - x,
                        state.indoorRoute[state.waypoint].y - y) < 4.0f) ++state.waypoint;
                std::cout << "Harmondale indoor plan: phase=" << state.phase
                    << " actor=" << actorTarget << " status=" << int(result.status)
                    << " points=" << result.waypoints.size()
                    << " best_distance=" << result.debug.bestDistance2d << '\n';
            }
            while (state.waypoint < state.indoorRoute.size()
                && std::hypot(state.indoorRoute[state.waypoint].x - world.partyX(),
                    state.indoorRoute[state.waypoint].y - world.partyY()) < 4.0f) ++state.waypoint;
            if (state.waypoint == state.indoorRoute.size())
            {
                if (state.indoorRouteStatus == PathPlanStatus::Success && !state.approachingDoor)
                {
                    state.indoorRoute.clear();
                    return std::hypot(targetX - world.partyX(), targetY - world.partyY()) < 240.0f;
                }
                require(state.indoorInteractions < 40,
                    "Harmondale indoor route repeatedly stopped at an unhandled mechanism");
                bool foundDoor = false;
                uint16_t doorEvent = 0;
                for (float pitch : {-0.55f, -0.3f, -0.05f, 0.2f, 0.45f})
                {
                    for (size_t direction = 0; direction < 64; ++direction)
                    {
                        const float yaw = float(direction) * 6.28318530718f / 64.0f;
                        game.m_indoorRenderer.setCameraAngles(yaw, pitch);
                        GameplayWorldPickRequest pick = world.buildWorldPickRequest({
                            .screenX = width * 0.5f,
                            .screenY = height * 0.5f,
                            .screenWidth = width,
                            .screenHeight = height,
                            .includeRay = true,
                        });
                        pick.ignoreActors = true;
                        const GameplayWorldHit hit = world.pickKeyboardInteractionTarget(pick);
                        if (!hit.eventTarget || !world.canActivateWorldHit(
                            hit, GameplayInteractionMethod::Keyboard)) continue;
                        const uint16_t eventId = hit.eventTarget->triggeredEventId;
                        if (eventId < 3 || eventId > 34) continue;
                        foundDoor = true;
                        doorEvent = eventId;
                        break;
                    }
                    if (foundDoor) break;
                }
                if (!foundDoor && !state.approachingDoor)
                {
                    // A partial route ends at the closest wall, which need not be the room's doorway.
                    // Find a reachable standing point beside an authored door using its current geometry.
                    const auto &map = game.m_gameDataLoader.getSelectedMap();
                    const auto &indoor = *map->indoorMapData;
                    const auto vertices = buildIndoorMechanismAdjustedVertices(
                        indoor, world.mapDeltaData(), world.eventRuntimeState());
                    const auto built = IndoorPathfindingBuilder::buildPathMap(
                        indoor, vertices, world.mapDeltaData());
                    float bestScore = std::numeric_limits<float>::max();
                    std::vector<PathPoint> bestRoute;
                    uint16_t approachEvent = 0;
                    for (const IndoorFace &face : indoor.faces)
                    {
                        const uint16_t event = face.cogTriggered;
                        if (event < 3 || event > 34 || event == 10 || event == 15 || event == 16
                            || state.openedDoors.contains(event) || face.vertexIndices.size() < 3) continue;
                        PathPoint center{};
                        float minX = 1.0e9f, maxX = -1.0e9f, minY = 1.0e9f, maxY = -1.0e9f;
                        float minZ = 1.0e9f, maxZ = -1.0e9f;
                        for (uint16_t index : face.vertexIndices)
                        {
                            const IndoorVertex &v = vertices[index];
                            center.x += float(v.x); center.y += float(v.y); center.z += float(v.z);
                            minX = std::min(minX, float(v.x)); maxX = std::max(maxX, float(v.x));
                            minY = std::min(minY, float(v.y)); maxY = std::max(maxY, float(v.y));
                            minZ = std::min(minZ, float(v.z)); maxZ = std::max(maxZ, float(v.z));
                        }
                        const float count = float(face.vertexIndices.size());
                        center.x /= count; center.y /= count; center.z /= count;
                        // Raised doors can retain their event on a strip near the ceiling. They
                        // no longer obstruct the party and must not replace a closed-door approach.
                        if (minZ > world.partyFootZ() + 160.0f || maxZ < world.partyFootZ()
                            || std::hypot(center.x - x, center.y - y) > 3000.0f) continue;
                        for (float side : {-160.0f, 160.0f})
                        {
                            PathPoint standing{center.x, center.y, world.partyFootZ()};
                            if (maxX - minX < maxY - minY) standing.x += side;
                            else standing.y += side;
                            PathPlanRequest request;
                            request.source = {world.partyX(), world.partyY(), world.partyFootZ()};
                            request.target = standing;
                            request.object.radius = 37.0f;
                            request.object.stepHeight = 128.0f;
                            request.object.stepLength = 64.0f;
                            request.nodeLimit = 200000;
                            PathPlanner planner;
                            const PathPlanResult proposed = planner.plan(built.pathMap, request);
                            if (proposed.status != PathPlanStatus::Success) continue;
                            float length = 0.0f;
                            PathPoint previous = request.source;
                            for (const PathPoint &point : proposed.waypoints)
                            {
                                length += std::hypot(point.x - previous.x, point.y - previous.y);
                                previous = point;
                            }
                            const float score = length + std::hypot(center.x - targetX, center.y - targetY);
                            if (score >= bestScore) continue;
                            bestScore = score;
                            bestRoute = proposed.waypoints;
                            approachEvent = event;
                        }
                    }
                    require(!bestRoute.empty(), "Harmondale partial route has no reachable authored door approach");
                    state.indoorRoute = std::move(bestRoute);
                    state.waypoint = 0;
                    state.indoorRouteStatus = PathPlanStatus::Success;
                    state.approachingDoor = true;
                    std::cout << "Harmondale door approach: event=" << approachEvent
                        << " waypoints=" << state.indoorRoute.size() << '\n';
                    return false;
                }
                require(foundDoor, "Harmondale reached a door approach but normal picking could not activate it");
                state.approachingDoor = false;
                state.openedDoors.insert(doorEvent);
                pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                ++state.indoorInteractions;
                state.indoorRoute.clear();
                state.doorTriggeredFrame = frame;
                std::cout << "Harmondale mechanism input: phase=" << state.phase
                    << " attempt=" << state.indoorInteractions << " event=" << doorEvent
                    << " position=" << world.partyX() << ',' << world.partyY() << ','
                    << world.partyFootZ() << '\n';
                return false;
            }
            const PathPoint point = state.indoorRoute[state.waypoint];
            const float dx = point.x - world.partyX(), dy = point.y - world.partyY();
            const float distance = std::hypot(dx, dy);
            game.m_indoorRenderer.setCameraAngles(std::atan2(dy, dx), 0.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Forward,
                std::clamp(distance / 1500.0f, 1.0f / 256.0f, 0.025f));
            const float moved = std::hypot(world.partyX() - x, world.partyY() - y);
            state.walkedDistance += moved;
            state.stalledFrames = moved < 0.1f ? state.stalledFrames + 1 : 0;
            if (state.stalledFrames == 3)
            {
                if (state.phase == 55)
                {
                    state.indoorRoute.clear();
                    state.stalledFrames = 0;
                    std::cout << "Harmondale replan around current hostile bodies\n";
                    return false;
                }
                // Static paths do not include moving actor bodies. Walk sideways under real collision
                // before replanning; do not turn a momentary obstruction into an exposed combat stop.
                if (continuation && !std::filesystem::exists("castle-obstructed.oysav"))
                {
                    save("castle-obstructed");
                    captures.push_back("castle-obstructed");
                }
                state.bypassYaw = std::atan2(dy, dx) + 1.57079632679f;
                state.bypassSteps = 6;
                state.bypassTurns = 0;
                state.indoorRoute.clear();
                state.stalledFrames = 0;
                std::cout << "Harmondale ordinary sidestep: phase=" << state.phase
                    << " position=" << world.partyX() << ',' << world.partyY() << '\n';
            }
            return false;
        };
        const auto verifyNpcDestination = [&](uint32_t house, uint32_t topic, uint32_t greeting)
        {
            const Party::Snapshot snapshot = pParty->snapshot();
            const auto houseIt = snapshot.npcHouseOverrides.find(397);
            const auto topicIt = snapshot.npcTopicOverrides.find(397);
            const auto greetingIt = snapshot.npcGreetingOverrides.find(397);
            require(houseIt != snapshot.npcHouseOverrides.end() && houseIt->second == house,
                "Butler house override does not match the original quest stage");
            if (topic == 856 && greeting == 200)
            {
                require((topicIt == snapshot.npcTopicOverrides.end() || !topicIt->second.contains(0))
                    && greetingIt == snapshot.npcGreetingOverrides.end(),
                    "Castle acceptance unexpectedly overrode the butler's static topic or greeting");
                return;
            }
            require(topicIt != snapshot.npcTopicOverrides.end() && topicIt->second.contains(0)
                && topicIt->second.at(0) == topic,
                "Butler topic override does not match the original quest stage");
            require(greetingIt != snapshot.npcGreetingOverrides.end() && greetingIt->second == greeting,
                "Butler greeting override does not match the original quest stage");
        };

        if (frame == 1)
        {
            state.goldBeforeReward = pParty->gold();
            for (size_t index = 0; index < pParty->members().size(); ++index)
            {
                const Character &member = pParty->members()[index];
                state.experienceBeforeReward.push_back(member.experience);
                state.preparationExperience[index] = member.experience;
                state.preparationSkillPoints[index] = member.skillPoints;
            }
            if (historyMode != 0)
            {
                const EventRuntimeState *pEvents = pWorld->eventRuntimeState();
                require(pEvents != nullptr && runtime.activeHistoryContinentId() == 2,
                    "Harmondale history compatibility needs the active MM7 history state");
                state.historyBefore = historyEventTimesForActiveContinent(*pEvents);
                require(state.historyBefore.contains(3) && state.historyBefore.contains(4),
                    "Harmondale history compatibility input lost the checkpoint107 history");
                if (historyMode == 1)
                {
                    require(session.currentMapFileName() == "7out02.odm" && pParty->gold() == 560
                        && pParty->food() == 1 && pParty->hasQuestBit(519) && !pParty->hasQuestBit(587)
                        && !pParty->hasQuestBit(610) && !pParty->hasQuestBit(644)
                        && !pParty->hasQuestBit(647) && !pParty->hasQuestBit(658)
                        && !state.historyBefore.contains(5),
                        "Harmondale history entrance input is not the normal checkpoint107 continuation");
                    loadRoute("01-arrival-to-castle.tsv");
                    save("history-input");
                    captures.push_back("history-input");
                    return;
                }
                if (historyMode == 2)
                {
                    require(session.currentMapFileName() == "7d29.blv" && pParty->hasQuestBit(587)
                        && pParty->hasQuestBit(644) && !pParty->hasQuestBit(647)
                        && !pParty->hasQuestBit(658) && !state.historyBefore.contains(5),
                        "Harmondale history reload input lost the newly entered castle state");
                    captures.push_back("history-reload-input");
                    next(50);
                    return;
                }
                require(historyMode == 3 && session.currentMapFileName() == "7out02.odm"
                    && !pParty->hasQuestBit(587) && pParty->hasQuestBit(644)
                    && pParty->hasQuestBit(647) && pParty->hasQuestBit(658)
                    && state.historyBefore.contains(5),
                    "Harmondale legacy history input lost its sealed checkpoint128 state");
                captures.push_back("history-legacy-input");
                next(50);
                return;
            }
            if (reload)
            {
                require(session.currentMapFileName() == "7out02.odm" && !pParty->hasQuestBit(587)
                    && pParty->hasQuestBit(644) && pParty->hasQuestBit(647) && pParty->hasQuestBit(658)
                    && std::all_of(pParty->members().begin(), pParty->members().end(),
                        [](const Character &member) { return member.awards.contains(3); }),
                    "Harmondale reload input lost its completed castle quest state");
                verifyNpcDestination(1169, 0, 201);
                captures.push_back("castle-reload-input");
                next(50);
                return;
            }
            if (continuation)
            {
                const bool inside = session.currentMapFileName() == "7d29.blv";
                const bool entrance = session.currentMapFileName() == "7out02.odm"
                    && std::hypot(pWorld->partyX() + 18368.0f, pWorld->partyY() - 12544.0f) < 512.0f;
                const bool inn = session.currentMapFileName() == "7out02.odm"
                    && std::hypot(pWorld->partyX() + 5184.0f, pWorld->partyY() - 13264.0f) < 512.0f;
                require((inside || entrance || inn) && pParty->hasQuestBit(587)
                    && !pParty->hasQuestBit(647) && !pParty->hasQuestBit(658),
                    "Harmondale diagnostic continuation requires an unfinished normal castle save");
                state.initialTargets = inside ? actorCounts().targets : 0;
                save("castle-diagnostic-input");
                captures.push_back("castle-diagnostic-input");
                std::cout << "Harmondale DIAGNOSTIC continuation; not a continuous checkpoint107 run\n";
                if (inn)
                {
                    state.resupplying = true;
                    state.resupplyTrips = 1;
                    state.lodgingGoldBefore = pParty->gold();
                    loadRoute("08-hospitality-to-temple.tsv");
                    next(60);
                }
                else next(inside ? 8 : 5);
                return;
            }
            require(session.currentMapFileName() == "7out02.odm" && pParty->gold() == 560
                && pParty->food() == 1 && pParty->hasQuestBit(519) && !pParty->hasQuestBit(587)
                && !pParty->hasQuestBit(610) && !pParty->hasQuestBit(644)
                && !pParty->hasQuestBit(647) && !pParty->hasQuestBit(658),
                "Harmondale input is not the fixed normal checkpoint107 continuation");
            loadRoute("01-arrival-to-castle.tsv");
            save("castle-input");
            captures.push_back("castle-input");
        }
        if (historyMode != 0)
        {
            if (state.phase == 0)
            {
                require(historyMode == 1, "Only the normal checkpoint107 history run may approach the castle");
                if (!walkOutdoor()) return;
                next(5);
                return;
            }
            if (frame < state.phaseFrame + 3) return;
            switch (state.phase)
            {
            case 5:
                aimOutdoor(-18368.0f, 12544.0f, 608.0f);
                if (std::hypot(pWorld->partyX() + 18368.0f, pWorld->partyY() - 12544.0f) > 160.0f)
                {
                    pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.015f);
                    require(frame < state.phaseFrame + 600,
                        "Harmondale history entrance approach is obstructed");
                    break;
                }
                useCastleEntrance();
                next(6);
                break;
            case 6:
                require(runtime.activeEventDialog().isActive
                    && runtime.activeEventDialog().presentation != EventDialogPresentation::Transition
                    && runtime.activeEventDialog().sourceId == 397,
                    "Harmondale history run did not open the original first-arrival butler dialogue");
                captures.push_back("history-entrance-dialog");
                next(62);
                break;
            case 62:
                close();
                useCastleEntrance();
                next(63);
                break;
            case 63:
                require(runtime.activeEventDialog().isActive
                    && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                    "Harmondale history run did not reopen the castle transition after the butler dialogue");
                captures.push_back("history-transition-dialog");
                next(61);
                break;
            case 61:
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
                next(7);
                break;
            case 7:
            {
                if (frame < state.phaseFrame + 30) return;
                require(session.currentMapFileName() == "7d29.blv" && pWorld->isIndoorMap()
                    && pParty->hasQuestBit(587) && pParty->hasQuestBit(644)
                    && !pParty->hasQuestBit(647) && !pParty->hasQuestBit(658),
                    "Harmondale history run did not enter the normal unfinished castle state");
                const EventRuntimeState *pEvents = pWorld->eventRuntimeState();
                require(pEvents != nullptr
                    && historyEventTimesForActiveContinent(*pEvents) == state.historyBefore
                    && !state.historyBefore.contains(5),
                    "Harmondale entrance changed an existing history time or wrote the war record");
                save("mm7-history-entry-complete");
                captures.push_back("history-castle-interior");
                next(50);
                break;
            }
            case 50:
            {
                pressQuestGameplay(session, width, height, KeyboardAction::History);
                require(runtime.journalScreenState().active
                    && runtime.journalScreenState().view == GameplayUiController::JournalView::Story,
                    "Normal History action did not open the Harmondale story journal");
                const EventRuntimeState *pEvents = pWorld->eventRuntimeState();
                const JournalHistoryTable *pTable = runtime.journalHistoryTable();
                require(pEvents != nullptr && pTable != nullptr
                    && historyEventTimesForActiveContinent(*pEvents) == state.historyBefore,
                    "Opening the Harmondale journal changed its saved history state");
                const JournalHistoryEntry *pCastle = pTable->getForContinent(2, 3);
                require(pCastle != nullptr && pCastle->pageTitle == "极大挑战"
                    && pCastle->text.find("哈蒙代尔的新主人今天来到了他的城堡") != std::string::npos,
                    "Harmondale history3 is not mapped to its original title and text");
                if (historyMode == 3)
                {
                    const JournalHistoryEntry *pWar = pTable->getForContinent(2, 5);
                    require(state.historyBefore.contains(5) && pWar != nullptr
                        && pWar->pageTitle == "一触即发",
                        "The sealed checkpoint128 history5 record no longer maps to its original war text");
                }
                else require(!state.historyBefore.contains(5),
                    "The corrected Harmondale entrance exposed a history5 record");

                const auto *pTextLayout = runtime.findHudLayoutElement("JournalTextViewport");
                const auto textRect = pTextLayout != nullptr
                    ? runtime.resolveHudLayoutElement("JournalTextViewport", width, height,
                        pTextLayout->width, pTextLayout->height)
                    : std::nullopt;
                const auto font = runtime.findHudFont("AUTONOTE");
                require(textRect.has_value() && font.has_value(),
                    "Harmondale history view cannot resolve its original page layout");
                const float fontScale = GameplayHudCommon::snappedHudFontScale(textRect->scale);
                const size_t linesPerPage = std::max<size_t>(1, static_cast<size_t>(std::floor(
                    textRect->height / std::max(1.0f, float(font->fontHeight) * fontScale))));
                size_t castlePage = 0;
                bool foundCastlePage = false;
                for (const JournalHistoryEntry &entry : pTable->entriesForContinent(2))
                {
                    const auto timeIt = state.historyBefore.find(entry.id);
                    if (timeIt == state.historyBefore.end()) continue;
                    if (entry.id == 3)
                    {
                        foundCastlePage = true;
                        break;
                    }
                    const std::vector<std::string> lines = runtime.wrapHudTextToWidth(
                        *font, StoryTextFormatter::format(entry.text, *pParty, timeIt->second),
                        textRect->width / std::max(0.1f, fontScale));
                    castlePage += std::max<size_t>(1, (lines.size() + linesPerPage - 1) / linesPerPage);
                }
                require(foundCastlePage, "Harmondale history3 has no visible story page");
                runtime.journalScreenState().storyPage = castlePage;
                next(51);
                break;
            }
            case 51:
                if (frame < state.phaseFrame + 10) return;
                captures.push_back(historyMode == 1 ? "history-journal"
                    : historyMode == 2 ? "history-reload-journal" : "history-legacy-journal");
                next(52);
                break;
            case 52:
                close();
                next(53);
                break;
            case 53:
            {
                require(!runtime.journalScreenState().active,
                    "Normal Escape did not close the Harmondale story journal");
                const EventRuntimeState *pEvents = pWorld->eventRuntimeState();
                require(pEvents != nullptr
                    && historyEventTimesForActiveContinent(*pEvents) == state.historyBefore,
                    "Harmondale history round trip changed an existing record or time");
                save(historyMode == 1 ? "mm7-history-view-complete"
                    : historyMode == 2 ? "mm7-history-reload-complete"
                    : "mm7-history-legacy-roundtrip");
                captures.push_back(historyMode == 1 ? "history-complete"
                    : historyMode == 2 ? "history-reload-complete" : "history-legacy-complete");
                state.complete = true;
                std::cout << "Harmondale history compatibility passed: mode=" << historyMode
                    << " history3_time=" << state.historyBefore.at(3)
                    << " history5=" << state.historyBefore.contains(5) << '\n';
                break;
            }
            default: throw std::runtime_error("Invalid Harmondale history compatibility phase");
            }
            return;
        }
        if (reload)
        {
            if (state.phase == 0)
            {
                if (!walkOutdoor()) return;
                loadRoute("03-inn-to-castle.tsv");
                next(4);
                return;
            }
            if (state.phase == 4)
            {
                if (!walkOutdoor()) return;
                next(1);
                return;
            }
            if (frame < state.phaseFrame + 12) return;
            switch (state.phase)
            {
            case 50:
                aimOutdoor(-5184.0f, 13120.0f, 128.0f);
                pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
                next(51);
                break;
            case 51:
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 240,
                    "Harmondale independent revisit did not reopen the original reward inn");
                require(std::none_of(runtime.activeEventDialog().actions.begin(),
                    runtime.activeEventDialog().actions.end(), [](const EventDialogAction &action)
                    {
                        return action.kind == EventDialogActionKind::HouseResident && action.id == 397;
                    }), "Harmondale completed butler still offers a resident action at the old reward inn");
                captures.push_back("castle-reload-inn-revisit");
                next(52);
                break;
            case 52:
                if (runtime.activeEventDialog().isActive) close();
                else
                {
                    loadRoute("05-hospitality-to-castle.tsv");
                    next(0);
                }
                break;
            case 1:
                aimOutdoor(-18368.0f, 12544.0f, 608.0f);
                if (std::hypot(pWorld->partyX() + 18368.0f, pWorld->partyY() - 12544.0f) > 160.0f)
                {
                    pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.015f);
                    require(frame < state.phaseFrame + 600, "Harmondale reload entrance approach is obstructed");
                    break;
                }
                useCastleEntrance();
                next(2);
                break;
            case 2:
                require(runtime.activeEventDialog().isActive
                    && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                    "Harmondale reload did not reopen the castle transition");
                selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
                next(3);
                break;
            case 3:
            {
                if (frame < state.phaseFrame + 30) return;
                require(session.currentMapFileName() == "7d29.blv" && pWorld->isIndoorMap(),
                    "Harmondale reload did not re-enter the castle");
                const ActorCounts counts = actorCounts();
                require(counts.targets > 0 && counts.targetsAlive == 0,
                    "Harmondale castle targets respawned after independent reload and re-entry");
                require(counts.golems == 1 && counts.golemsVisible == 0
                    && counts.invasion == 62 && counts.invasionVisible == 0,
                    "Harmondale reload exposed unrelated future-stage actors");
                require(pParty->gold() == state.goldBeforeReward,
                    "Harmondale reload repeated the quest gold state");
                for (size_t index = 0; index < pParty->members().size(); ++index)
                    require(pParty->members()[index].experience == state.experienceBeforeReward[index]
                        && pParty->members()[index].awards.contains(3),
                        "Harmondale reload repeated or lost a member reward");
                verifyNpcDestination(1169, 0, 201);
                save("mm7-castle-clear-reload-complete");
                captures.push_back("castle-reentered-cleared");
                state.complete = true;
                std::cout << "Harmondale independent reload passed: targets=" << counts.targets
                    << " alive=" << counts.targetsAlive << " gold=" << pParty->gold() << '\n';
                break;
            }
            default: throw std::runtime_error("Invalid Harmondale reload phase");
            }
            return;
        }

        if (state.phase == 0)
        {
            if (!walkOutdoor()) return;
            next(1);
            return;
        }
        if (state.phase == 11)
        {
            if (!walkIndoor(-5120.0f, -2784.0f, 0.0f, size_t(-1))) return;
            next(12);
            return;
        }
        if (state.phase == 55)
        {
            if (healWhileWalking()) return;
            if (actorCounts().targetsAlive <= 1)
            {
                state.resupplying = false;
                next(8);
                return;
            }
            TurnBasedCombatRuntime &turn = session.turnBasedCombatRuntime();
            if (turn.active())
            {
                if (turn.canToggleOff(*pParty)) pressQuestGameplay(session, width, height, KeyboardAction::Combat);
                else if (turn.stage() == TurnBasedCombatStage::Movement || turn.playerActionReady(*pParty))
                    pressQuestGameplay(session, width, height, KeyboardAction::Pass);
                return;
            }
            if (walkIndoor(-5120.0f, -2784.0f, 0.0f, size_t(-1))) next(12);
            return;
        }
        if (state.phase == 60)
        {
            if (!walkOutdoor()) return;
            next(61);
            return;
        }
        if (state.phase == 30)
        {
            if (!walkOutdoor()) return;
            next(31);
            return;
        }
        if (state.phase == 36)
        {
            if (!walkOutdoor()) return;
            next(21);
            return;
        }
        if (state.phase == 15)
        {
            if (!walkOutdoor()) return;
            loadRoute("04-castle-to-hospitality.tsv");
            next(28);
            return;
        }
        if (state.phase == 20)
        {
            if (!walkOutdoor()) return;
            loadRoute("04-castle-to-hospitality.tsv");
            next(27);
            return;
        }
        if (state.phase == 27)
        {
            if (!walkOutdoor()) return;
            next(21);
            return;
        }
        if (state.phase == 28)
        {
            if (!walkOutdoor()) return;
            next(16);
            return;
        }
        if (state.phase == 29)
        {
            if (!walkOutdoor()) return;
            loadRoute("03-inn-to-castle.tsv");
            next(26);
            return;
        }
        if (state.phase == 26)
        {
            if (!walkOutdoor()) return;
            next(5);
            return;
        }
        if (state.phase == 8 || state.phase == 9)
        {
            if (state.restPhase == 1)
            {
                if (frame < state.phaseFrame + 12) return;
                clickRest("RestButton8Hours");
                require(runtime.restScreenState().mode == GameplayUiController::RestMode::Heal
                    && pParty->food() == state.restStartingFood - state.restFoodCost,
                    "Normal castle rest did not start or consumed incorrect food");
                state.restPhase = 2;
                state.phaseFrame = frame;
                return;
            }
            if (state.restPhase == 2)
            {
                require(frame < state.phaseFrame + 1800, "Normal castle rest did not finish its animation");
                if (runtime.restScreenState().mode != GameplayUiController::RestMode::None) return;
                if (runtime.restScreenState().active) clickRest("RestButtonExit");
                require(!runtime.restScreenState().active && pParty->hasActableMember(),
                    "Normal castle rest did not return to the surviving party");
                ++state.rests;
                if (!state.restCaptured)
                {
                    save("castle-rested");
                    captures.push_back("castle-rested");
                    state.restCaptured = true;
                }
                std::cout << "Harmondale normal rest complete: count=" << state.rests
                    << " food=" << pParty->food() << '\n';
                state.restPhase = 0;
                state.phaseFrame = frame;
                return;
            }
            const ActorCounts counts = actorCounts();
            const bool preserveLast = state.phase == 8;
            if (preserveLast && counts.targetsAlive == 1)
            {
                require(!pParty->hasQuestBit(647),
                    "Harmondale castle completed before the last living group56 target died");
                // Snapshot before any turn toggle or input advances queued projectiles. Death
                // animation state is not evidence of positive HP; actorCounts reads actual HP.
                save("castle-last-target-alive");
                captures.push_back("castle-last-target-alive");
                next(9);
                return;
            }
            if ((preserveLast && counts.targetsAlive > 1) || (!preserveLast && counts.targetsAlive > 0))
            {
                if (healWhileWalking()) return;
                const Character &cleric = pParty->members()[1];
                const bool lowMana = cleric.spellPoints < 31 || pParty->members()[2].spellPoints < 31;
                // A ready healer needs the ordinary next turn, even when their current HP is low.
                // Cancelling that turn to withdraw can expose the party to a much larger following pack.
                // Preserve the resource reserve for normal healing on the return journey.
                if (counts.targetsAlive > 1 && lowMana)
                {
                    require(state.resupplyTrips < 60, "Harmondale normal resupply trip limit exceeded");
                    state.resupplying = true;
                    ++state.resupplyTrips;
                    save("castle-retreat-" + std::to_string(state.resupplyTrips));
                    std::cout << "Harmondale normal retreat: trip=" << state.resupplyTrips
                        << " alive=" << counts.targetsAlive << " cleric_mana=" << pParty->members()[1].spellPoints
                        << " sorcerer_mana=" << pParty->members()[2].spellPoints << '\n';
                    next(55);
                    return;
                }
                if (fightIndoor(preserveLast)) return;
                bool needsRest = pParty->members()[1].spellPoints < 8
                    || pParty->members()[2].spellPoints < 8;
                for (const Character &member : pParty->members())
                    needsRest = needsRest || member.health * 4 < std::max(1, member.maxHealth) * 3;
                if (needsRest && frame >= state.phaseFrame + 600)
                {
                    state.restStartingFood = pParty->food();
                    state.restFoodCost = runtime.restFoodRequired();
                    if (state.restStartingFood >= state.restFoodCost)
                    {
                        pressQuestGameplay(session, width, height, KeyboardAction::Rest);
                        if (runtime.restScreenState().active)
                        {
                            state.restPhase = 1;
                            state.phaseFrame = frame;
                            return;
                        }
                        std::cout << "Harmondale normal rest restricted: "
                            << runtime.statusBarEventText() << '\n';
                        state.phaseFrame = frame;
                    }
                }
                const auto target = nearestTarget();
                require(target.has_value(), "Harmondale has living group56 targets but none can be explored");
                const size_t index = size_t((*target)[0]);
                walkIndoor((*target)[1], (*target)[2], (*target)[3], index);
                return;
            }
            if (fightIndoor(preserveLast)) return;
            if (preserveLast)
            {
                throw std::runtime_error("Harmondale missed the positive-HP last-target snapshot");
            }
            else
            {
                require(counts.targets == state.initialTargets && counts.targetsAlive == 0
                    && !pParty->hasQuestBit(647),
                    "Harmondale indoor clear changed target identity or completed before leaving");
                save("castle-cleared-inside");
                captures.push_back("castle-cleared-inside");
                next(11);
            }
            return;
        }
        if (frame < state.phaseFrame + 12) return;
        switch (state.phase)
        {
        case 1:
            aimOutdoor(-18368.0f, 12544.0f, 608.0f);
            useCastleEntrance();
            next(2);
            break;
        case 2:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 397
                && pParty->hasQuestBit(587) && pParty->hasQuestBit(644)
                && !pParty->hasQuestBit(647) && !pParty->hasQuestBit(658),
                "Castle entrance did not accept the original butler clear quest");
            verifyNpcDestination(240, 856, 200);
            state.goldBeforeReward = pParty->gold();
            state.experienceBeforeReward.clear();
            for (const Character &member : pParty->members())
                state.experienceBeforeReward.push_back(member.experience);
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 856);
            next(3);
            break;
        case 3:
            require(pParty->hasQuestBit(587) && !pParty->hasQuestBit(647)
                && !pParty->hasQuestBit(658) && pParty->gold() == state.goldBeforeReward,
                "Unmet Harmondale hand-in completed or paid early");
            for (size_t index = 0; index < pParty->members().size(); ++index)
                require(pParty->members()[index].experience == state.experienceBeforeReward[index]
                    && !pParty->members()[index].awards.contains(3),
                    "Unmet Harmondale hand-in changed member rewards");
            save("castle-early-rejected");
            captures.push_back("castle-early-rejected");
            next(4);
            break;
        case 4:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                loadRoute("06-castle-to-training.tsv");
                next(30);
            }
            break;
        case 31:
            aimOutdoor(-10480.0f, 12096.0f, 128.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(32);
            break;
        case 32:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 1571,
                "Harmondale preparation did not enter the original Basic Training Hall house1571");
            next(33);
            break;
        case 33:
        {
            const size_t memberIndex = state.training % 4;
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, memberIndex);
            require(portrait.has_value(), "Harmondale training has no visible student portrait");
            click(portrait->x + portrait->width * 0.5f, portrait->y + portrait->height * 0.5f);
            require(pParty->activeMemberIndex() == memberIndex,
                "Harmondale training portrait did not select the expected student");
            next(34);
            break;
        }
        case 34:
        {
            const size_t memberIndex = state.training % 4;
            const uint32_t priorLevel = 1 + uint32_t(state.training / 4);
            const HouseEntry *pHouse = runtime.houseTable()->get(1571);
            require(pHouse && pParty->activeMemberIndex() == memberIndex
                && pParty->activeMember()->level == priorLevel,
                "Harmondale training hall or expected student level is absent");
            const int reputation = effectivePartyReputation(pWorld->currentLocationReputation(),
                pWorld->eventRuntimeState());
            const int price = PriceCalculator::trainingPrice(pParty->activeMember(), *pHouse, reputation);
            const int gold = pParty->gold();
            const uint32_t points = pParty->activeMember()->skillPoints;
            require(price > 0 && gold >= price, "Harmondale party cannot afford its normal training fee");
            selectQuestDialog(runtime, EventDialogActionKind::HouseService,
                uint32_t(HouseActionId::TrainingTrainActiveMember));
            const Character &trained = pParty->members()[memberIndex];
            require(trained.level == priorLevel + 1 && trained.experience == state.preparationExperience[memberIndex]
                && trained.skillPoints == points + 5 && pParty->gold() == gold - price,
                "Harmondale training changed experience, skill points or fee unexpectedly");
            std::cout << "Harmondale normal training: index=" << state.training << " member=" << memberIndex
                << " level=" << trained.level << " price=" << price << " gold=" << pParty->gold() << '\n';
            ++state.training;
            next(state.training == 8 ? 35 : 33);
            break;
        }
        case 35:
            require(std::all_of(pParty->members().begin(), pParty->members().end(),
                    [](const Character &member) { return member.level == 3 && member.skillPoints == 10; }),
                "Harmondale normal training did not produce four level-three students with earned points");
            close();
            require(!runtime.activeEventDialog().isActive, "Harmondale training dialog did not close normally");
            next(40);
            break;
        case 40:
        {
            const auto portrait = runtime.resolvePartyPortraitRect(width, height, state.skillMember);
            require(portrait.has_value(), "Harmondale skill student has no actual portrait");
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.pointerX = portrait->x + portrait->width * 0.5f;
            input.pointerY = portrait->y + portrait->height * 0.5f;
            input.rightMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.rightMouseButton.pressed = false;
            for (unsigned press = 0; press < 2; ++press)
            {
                input.leftMouseButton = {true, true, false};
                session.updateGameplay(input, 0.0f);
                input.leftMouseButton = {false, false, true};
                session.updateGameplay(input, 0.0f);
            }
            input.leftMouseButton = {};
            input.rightMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
            require(runtime.characterScreen().open
                && runtime.selectedCharacterScreenSourceIndex() == state.skillMember,
                "Harmondale portrait clicks did not open the correct character");
            next(41);
            break;
        }
        case 41:
        {
            const auto rect = resolve("CharacterSkillsButton");
            click(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
            require(runtime.characterScreen().page == GameplayUiController::CharacterPage::Skills,
                "Harmondale normal skills tab click failed");
            next(42);
            break;
        }
        case 42:
        {
            const Character &before = pParty->members()[state.skillMember];
            const CharacterSkill *pBeforeSkill = before.findSkillByCanonicalName(preparationSkills[state.skillMember]);
            const uint32_t beforeLevel = 1 + state.skillUpgrades;
            const uint32_t points = before.skillPoints;
            require(pBeforeSkill && pBeforeSkill->level == beforeLevel,
                "Harmondale preparation skill has an unexpected level before the UI click");
            clickPreparationSkill();
            const Character &after = pParty->members()[state.skillMember];
            const CharacterSkill *pAfterSkill = after.findSkillByCanonicalName(preparationSkills[state.skillMember]);
            require(pAfterSkill && pAfterSkill->level == beforeLevel + 1
                && after.skillPoints + beforeLevel + 1 == points,
                "Harmondale skill UI did not charge the next level's normal point cost");
            std::cout << "Harmondale normal skill upgrade: member=" << state.skillMember
                << " skill=" << preparationSkills[state.skillMember] << " level=" << pAfterSkill->level
                << " points=" << points << "->" << after.skillPoints << '\n';
            next(++state.skillUpgrades == 3 ? 43 : 42);
            break;
        }
        case 43:
        {
            GameplayInputFrame input;
            input.screenWidth = width;
            input.screenHeight = height;
            input.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
            input.keyboardPressCounts[SDL_SCANCODE_ESCAPE] = 1;
            session.updateGameplay(input, 0.0f);
            session.updateGameplay({}, 0.0f);
            require(!runtime.characterScreen().open, "Harmondale normal close did not leave the skills page");
            if (++state.skillMember < pParty->members().size())
            {
                state.skillUpgrades = 0;
                next(40);
                break;
            }
            for (size_t index = 0; index < pParty->members().size(); ++index)
            {
                const Character &member = pParty->members()[index];
                const CharacterSkill *pSkill = member.findSkillByCanonicalName(preparationSkills[index]);
                require(member.level == 3 && member.experience == state.preparationExperience[index]
                    && member.skillPoints == 1 && pSkill && pSkill->level == 4,
                    "Harmondale prepared party did not preserve normal training and skill spending");
            }
            save("castle-trained");
            captures.push_back("castle-trained");
            loadRoute("07-training-to-hospitality.tsv");
            next(36);
            break;
        }
        case 21:
            aimOutdoor(-5184.0f, 13120.0f, 128.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(22);
            break;
        case 22:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 240,
                "Harmondale provisioning did not enter the original Hospitality Inn house240");
            selectQuestDialog(runtime, EventDialogActionKind::HouseProprietor, 240);
            next(23);
            break;
        case 23:
            state.provisionGoldBefore = pParty->gold();
            state.provisionFoodBefore = pParty->food();
            if (pParty->food() < 14)
                selectQuestDialog(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TavernBuyFood));
            next(24);
            break;
        case 24:
            require(pParty->food() == 14 && (state.provisionFoodBefore < 14
                ? pParty->gold() < state.provisionGoldBefore : pParty->gold() == state.provisionGoldBefore),
                "Hospitality Inn did not sell the normal target provisions");
            save(state.resupplying ? "castle-provisions-" + std::to_string(state.resupplyTrips)
                : "castle-provisions");
            if (!state.resupplying) captures.push_back("castle-provisions");
            if (state.resupplying)
            {
                state.lodgingGoldBefore = pParty->gold();
                selectQuestDialog(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TavernRentRoom));
                next(54);
            }
            else next(25);
            break;
        case 54:
            require(runtime.restScreenState().active && pParty->gold() < state.lodgingGoldBefore,
                "Harmondale paid resupply did not open the ordinary inn rest screen and charge a fee");
            close();
            next(56);
            break;
        case 56:
            require(runtime.restScreenState().mode == GameplayUiController::RestMode::None,
                "Harmondale paid rest did not finish normally");
            if (runtime.restScreenState().active) close();
            save("castle-lodged-" + std::to_string(state.resupplyTrips));
            if (std::any_of(pParty->members().begin(), pParty->members().end(), [](const Character &member)
                {
                    return member.conditions.any() || member.health < Party::effectiveMaximumHealth(member)
                        || member.spellPoints < Party::effectiveMaximumSpellPoints(member);
                }))
            {
                if (runtime.activeEventDialog().isActive) close();
                loadRoute("08-hospitality-to-temple.tsv");
                next(60);
                break;
            }
            save("castle-resupplied-" + std::to_string(state.resupplyTrips));
            if (!state.restCaptured)
            {
                save("castle-rested");
                captures.push_back("castle-rested");
                state.restCaptured = true;
            }
            std::cout << "Harmondale paid resupply complete: trip=" << state.resupplyTrips
                << " gold=" << state.lodgingGoldBefore << "->" << pParty->gold() << '\n';
            next(25);
            break;
        case 61:
            aimOutdoor(-13050.0f, 10752.0f, 128.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(62);
            break;
        case 62:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 311,
                "Harmondale normal recovery did not reach Welnin Cathedral house311");
            selectQuestDialog(runtime, EventDialogActionKind::HouseProprietor, 311);
            state.templeMember = 0;
            save("castle-temple-before-" + std::to_string(state.resupplyTrips));
            captures.push_back("castle-temple-before-" + std::to_string(state.resupplyTrips));
            next(63);
            break;
        case 63:
            while (state.templeMember < pParty->members().size())
            {
                const Character &member = pParty->members()[state.templeMember];
                if (member.conditions.any() || member.health < Party::effectiveMaximumHealth(member)
                    || member.spellPoints < Party::effectiveMaximumSpellPoints(member)) break;
                ++state.templeMember;
            }
            if (state.templeMember == pParty->members().size()) next(66);
            else
            {
                const auto portrait = runtime.resolvePartyPortraitRect(width, height, state.templeMember);
                require(portrait.has_value(), "Harmondale Temple patient has no visible portrait");
                click(portrait->x + portrait->width * 0.5f, portrait->y + portrait->height * 0.5f);
                require(pParty->activeMemberIndex() == state.templeMember,
                    "Harmondale Temple portrait selected the wrong patient");
                next(64);
            }
            break;
        case 64:
        {
            const HouseEntry *house = runtime.houseTable()->get(311);
            require(house && house->templeCanHeal, "Welnin Cathedral cannot heal");
            state.templePrice = PriceCalculator::templeHealPrice(pParty->activeMember(), *house,
                pWorld->gameMinutes());
            state.templeGoldBefore = pParty->gold();
            require(state.templePrice > 0 && state.templeGoldBefore >= state.templePrice,
                "Normal Harmondale party cannot afford the quoted Temple healing");
            selectQuestDialog(runtime, EventDialogActionKind::HouseService, uint32_t(HouseActionId::TempleHeal));
            next(65);
            break;
        }
        case 65:
        {
            const Character &member = pParty->members()[state.templeMember];
            require(member.conditions.none() && member.health == Party::effectiveMaximumHealth(member)
                && member.spellPoints == Party::effectiveMaximumSpellPoints(member)
                && pParty->gold() == state.templeGoldBefore - state.templePrice,
                "Harmondale Temple healing differs from the normal quoted service");
            std::cout << "Harmondale normal Temple healing: trip=" << state.resupplyTrips
                << " member=" << state.templeMember << " price=" << state.templePrice
                << " gold=" << state.templeGoldBefore << "->" << pParty->gold() << '\n';
            ++state.templeMember;
            next(63);
            break;
        }
        case 66:
            save("castle-temple-healed-" + std::to_string(state.resupplyTrips));
            captures.push_back("castle-temple-healed-" + std::to_string(state.resupplyTrips));
            save("castle-resupplied-" + std::to_string(state.resupplyTrips));
            if (!state.restCaptured)
            {
                save("castle-rested");
                captures.push_back("castle-rested");
                state.restCaptured = true;
            }
            next(67);
            break;
        case 67:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                loadRoute("09-temple-to-castle.tsv");
                next(26);
            }
            break;
        case 25:
            if (runtime.activeEventDialog().isActive) close();
            else
            {
                loadRoute("05-hospitality-to-castle.tsv");
                next(29);
            }
            break;
        case 5:
            aimOutdoor(-18368.0f, 12544.0f, 608.0f);
            if (std::hypot(pWorld->partyX() + 18368.0f, pWorld->partyY() - 12544.0f) > 160.0f)
            {
                const float x = pWorld->partyX(), y = pWorld->partyY();
                pressQuestGameplay(session, width, height, KeyboardAction::Forward, 0.015f);
                const float moved = std::hypot(pWorld->partyX() - x, pWorld->partyY() - y);
                state.walkedDistance += moved;
                state.stalledFrames = moved < 0.1f ? state.stalledFrames + 1 : 0;
                require(state.stalledFrames < 120, "Harmondale castle entrance approach is obstructed");
                break;
            }
            useCastleEntrance();
            next(6);
            break;
        case 6:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Actual Harmondale Castle entrance did not open its normal confirmation");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            next(7);
            break;
        case 7:
        {
            if (frame < state.phaseFrame + 30) return;
            require(session.currentMapFileName() == "7d29.blv" && pWorld->isIndoorMap()
                && pParty->hasQuestBit(587) && !pParty->hasQuestBit(647),
                "Harmondale Castle transition lost the active quest or destination");
            const ActorCounts counts = actorCounts();
            require(counts.targets >= 29 && (continuation && state.initialTargets == 0
                ? counts.targetsAlive < counts.targets : state.resupplying
                ? counts.targets == state.initialTargets && counts.targetsAlive < counts.targets
                : counts.targetsAlive == counts.targets),
                "Harmondale Castle did not materialize the exact live group56 target set");
            require(counts.golems == 1 && counts.golemsVisible == 0
                && counts.invasion == 62 && counts.invasionVisible == 0,
                "Harmondale Castle exposed unrelated future-stage actors");
            state.initialTargets = counts.targets;
            save(state.resupplying ? "castle-reentered-" + std::to_string(state.resupplyTrips) : "castle-entered");
            if (!state.resupplying) captures.push_back("castle-entered");
            std::cout << "Harmondale target inventory: group56=" << counts.targets
                << " group57=" << counts.golems << " group60=" << counts.invasion << '\n';
            state.resupplying = false;
            next(8);
            break;
        }
        case 12:
            aimIndoor(-5120.0f, -2880.0f, 128.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(13);
            break;
        case 13:
            require(runtime.activeEventDialog().isActive
                && runtime.activeEventDialog().presentation == EventDialogPresentation::Transition,
                "Harmondale Castle exit did not open its normal confirmation");
            selectQuestDialog(runtime, EventDialogActionKind::MapTransitionConfirm, 0);
            next(14);
            break;
        case 14:
            if (frame < state.phaseFrame + 30) return;
            if (state.resupplying)
            {
                require(session.currentMapFileName() == "7out02.odm" && pParty->hasQuestBit(587)
                    && !pParty->hasQuestBit(647) && !pParty->hasQuestBit(658),
                    "Leaving the unfinished castle completed the quest early");
                save("castle-retreat-outside-" + std::to_string(state.resupplyTrips));
                loadRoute("02-castle-to-inn.tsv");
                next(20);
                break;
            }
            require(session.currentMapFileName() == "7out02.odm" && !pWorld->isIndoorMap()
                && pParty->hasQuestBit(587) && pParty->hasQuestBit(647) && !pParty->hasQuestBit(658),
                "Leaving the cleared castle did not set only the original Q647 completion flag");
            save("castle-cleared-return");
            captures.push_back("castle-cleared-return");
            loadRoute("02-castle-to-inn.tsv");
            next(15);
            break;
        case 16:
            aimOutdoor(-5184.0f, 13120.0f, 128.0f);
            pressQuestGameplay(session, width, height, KeyboardAction::Trigger);
            next(17);
            break;
        case 17:
            require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 240,
                "Harmondale return did not enter the original Hospitality Inn house240");
            selectQuestDialog(runtime, EventDialogActionKind::HouseResident, 397);
            next(18);
            break;
        case 18:
            state.goldBeforeReward = pParty->gold();
            state.experienceBeforeReward.clear();
            for (const Character &member : pParty->members())
                state.experienceBeforeReward.push_back(member.experience);
            selectQuestDialog(runtime, EventDialogActionKind::NpcTopic, 856);
            next(19);
            break;
        case 19:
            require(!pParty->hasQuestBit(587) && pParty->hasQuestBit(644)
                && pParty->hasQuestBit(647) && pParty->hasQuestBit(658)
                && pParty->gold() == state.goldBeforeReward,
                "Harmondale hand-in changed the original quest bits or gold");
            for (size_t index = 0; index < pParty->members().size(); ++index)
                require(pParty->members()[index].experience == state.experienceBeforeReward[index] + 5000
                    && pParty->members()[index].awards.contains(3),
                    "Harmondale hand-in changed the original 5000 experience or award3 reward");
            verifyNpcDestination(1169, 0, 201);
            save(continuation ? "mm7-castle-diagnostic-complete" : "mm7-castle-clear-complete");
            captures.push_back("castle-reward-complete");
            state.complete = true;
            std::cout << (continuation ? "Harmondale diagnostic quest passed: targets="
                : "Harmondale continuous quest passed: targets=") << state.initialTargets
                << " walked=" << state.walkedDistance << " rests=" << state.rests
                << " gold=" << pParty->gold() << '\n';
            break;
        default: throw std::runtime_error("Invalid Harmondale Castle quest phase");
        }
    }

    static int run(const std::filesystem::path &root, const std::filesystem::path &save,
        const std::filesystem::path &output, size_t fixtureSize, bool requireMinimap, bool checkNavigation,
        bool checkPartyAudio, bool checkSpellbook, bool spellbookControls, bool checkTurns, bool hostileTurns,
        bool indoorCombat, bool checkDefeat, uint32_t defeatSkipFrame, bool checkTurnSaves, bool checkTurnReload,
        bool indoorTurnSaves, bool firstLetter, bool firstLetterReload, bool castleLetter, bool castleLetterReload,
        bool indoorEncounter, bool templeChest, bool indoorDoor, bool templeChestReload,
        bool candlestick, bool candlestickReload, bool chestPointer, bool mm8Start, bool mm8StartReload,
        bool mm8Inn, bool mm8InnReload, bool paperdolls, bool mm8Shop, bool mm8ShopReload,
        bool mm8Portals, bool mm8Temple, bool mm8Dungeon, bool mm8Return, bool mm8Resupply, bool mm8Revisit,
        bool mm8Chamber, bool mm8Stairs, bool mm8Passage, bool mm8FloorPuzzle, bool mm8Exit, bool mm8Delivery,
        bool mm8Blackmail, bool mm8BlackmailReload,
        bool mm8Replay, size_t replayFirstStage, bool mm7Shell, bool mm7ShellReload, bool mm7Potion,
        bool mm7Recruit, bool mm7Temple, bool mm7Dungeon, bool mm7Contest, bool mm7Replay,
        bool mm6Replay, size_t mm6ReplayEndStage, bool mm6Goblinwatch, bool mm6GoblinwatchReload,
        bool mm7Castle, bool mm7CastleReload, bool mm7CastleContinue, unsigned mm7CastleHistoryMode,
        bool pausedMovement)
    {
        const auto require = [](bool condition, const std::string &message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        };
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        std::string error;
        std::optional<GameSaveData> expected = (mm6Replay && replayFirstStage == 0)
            || mm7Replay || (mm8Replay && replayFirstStage == 0)
            ? std::optional<GameSaveData>(GameSaveData{}) : loadGameDataFromPath(save, error);
        require(expected.has_value(), "Cannot read capture input save: " + error);
        if ((mm6Replay && replayFirstStage == 0) || mm7Replay || (mm8Replay && replayFirstStage == 0))
            expected->mapFileName = mm6Replay ? "oute3.odm" : mm7Replay ? "7out01.odm" : "out01.odm";
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        std::filesystem::path input = save;
        if (fixtureSize != 0)
        {
            // Load the fixture through the real save path so both session and world receive the same party.
            require(!expected->party.members.empty(), "HUD fixture needs a source character");
            const Character member = expected->party.members.front();
            expected->party.members.resize(fixtureSize, member);
            const std::array<const char *, 5> portraits{"malea01", "girla01", "maleb01", "girlb01", "malec01"};
            for (size_t index = 0; index < fixtureSize; ++index)
            {
                Character &fixture = expected->party.members[index];
                fixture.name = "界面测试" + std::to_string(index + 1);
                fixture.portraitTextureName = portraits[index];
                fixture.characterDataId = uint32_t(38 + index);
                fixture.portraitPictureId = uint32_t(37 + index);
                fixture.portraitState = PortraitId::Normal;
                fixture.health = member.maxHealth * int(fixtureSize - index) / int(fixtureSize);
            }
            input = output / "hud-fixture-input.oysav";
            require(saveGameDataToPath(input, *expected, error), "Cannot write explicit HUD fixture");
            std::cout << "Explicit visual fixture with " << fixtureSize << " source portraits; not a playthrough.\n";
        }
        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.combatTrace = hostileTurns || indoorCombat || indoorTurnSaves || checkTurnReload || templeChest
            || mm6Goblinwatch || mm7Castle || mm8Blackmail;
        settings.collisionTrace = mm7CastleContinue;
        settings.gameplayTrace = castleLetter || indoorEncounter || templeChest || mm8Start || mm8Inn || mm8Shop
            || mm8Portals || mm8Replay || mm7Shell || mm7Replay || mm6Replay || mm6Goblinwatch
            || mm7Castle || mm8Blackmail || pausedMovement;
        if (mm8Replay || mm7Replay || mm6Replay)
        {
            settings.newGameGodLich = false;
            settings.allowIncompleteCharacterCreation = false;
        }
        settings.gameplayTraceAppend = mm7Castle;
        settings.gameplayTraceFile = (output / "gameplay.trace").string();
        settings.combatTraceAppend = mm7Castle;
        settings.combatTraceFile = (output / "combat.trace").string();
        settings.assetRoot = root.string();
        settings.windowMode = WindowMode::Windowed;
        if (indoorCombat || templeChest)
        {
            settings.keyboard.setBinding(KeyboardAction::Attack, keyboardInputBinding(SDL_SCANCODE_F));
        }
        require(saveGameSettings(output / "settings.ini", settings, error), "Cannot write isolated capture settings");
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM Mac scene validation";
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;
        GameApplication game(config);
        uint32_t frames = 0;
        uint32_t captures = 0;
        uint64_t started = 0;
        bool saved = false;
        std::set<std::string> savedTurnPhases;
        bool turnReloadComplete = false;
        bool doorComplete = false;
        bool paperdollsComplete = false;
        EquipmentCheckState equipmentCheck;
        uint32_t turnReloadImpactFrame = 0;
        int turnReloadImpactDamage = 0;
        float turnReloadSimulatedSeconds = 0.0f;
        PausedMovementCheckState pausedMovementCheck;
        bool controlsComplete = false;
        uint32_t controlsTargetFrame = 0;
        TurnCheckState turnCheck;
        HostileTurnCheckState hostileCheck;
        IndoorCombatCheckState indoorCheck;
        TempleChestCheckState chestCheck;
        chestCheck.reloadEmpty = templeChestReload;
        chestCheck.pointerTransfer = chestPointer;
        indoorCheck.entireEncounter = indoorEncounter;
        IndoorTurnSaveCheckState indoorTurnCheck;
        Mm7RouteCheckState mm7ShellCheck;
        Mm7DungeonCheckState mm7DungeonCheck;
        Mm7ContestCheckState mm7ContestCheck;
        mm7ShellCheck.potion = mm7Potion;
        Mm8StartCheckState mm8Check;
        Mm8StartCheckState innCheck;
        Mm8ShopCheckState shopCheck;
        Mm8PortalCheckState portalCheck;
        Mm8BlackmailCheckState blackmailCheck;
        Mm6LetterReplayState mm6ReplayCheck;
        mm6ReplayCheck.stage = mm6Replay ? replayFirstStage : 0;
        mm6ReplayCheck.endStage = mm6ReplayEndStage;
        mm6ReplayCheck.openingComplete = mm6Replay && replayFirstStage != 0;
        Mm6GoblinwatchCheckState mm6GoblinwatchCheck;
        Mm7CastleCheckState mm7CastleCheck;
        Mm7ReplayState mm7ReplayCheck;
        Mm8ReplayState replayCheck;
        replayCheck.stage = replayFirstStage;
        replayCheck.fromNewGame = replayFirstStage == 0;
        CandlestickCheckState candleCheck;
        FirstLetterCheckState firstLetterCheck;
        CastleLetterCheckState castleLetterCheck;
        std::string lastCombatStatus;
        unsigned combatStatusCaptures = 0;
        uint32_t revivedFrame = 0;
        bool sawDefeatScreen = false;
        const uint16_t deathVariable = uint16_t(EvtVariable::NumDeaths);
        const int startingDeaths = expected->party.eventVariables.contains(deathVariable)
            ? expected->party.eventVariables.at(deathVariable) : 0;
        std::vector<std::string> captureTokens{"scene-first", "scene-later"};
        std::vector<std::string> frameCaptureRequests;
        struct PendingSceneCapture
        {
            uint32_t requestedFrame = 0;
            std::vector<std::string> aliases;
        };
        std::map<std::string, PendingSceneCapture> pendingCaptureAliases;
        if (mm7Castle)
        {
            if (mm7CastleHistoryMode == 1)
                captureTokens.insert(captureTokens.end(), {"history-input", "history-entrance-dialog",
                    "history-transition-dialog", "history-castle-interior", "history-journal",
                    "history-complete"});
            else if (mm7CastleHistoryMode == 2)
                captureTokens.insert(captureTokens.end(), {"history-reload-input", "history-reload-journal",
                    "history-reload-complete"});
            else if (mm7CastleHistoryMode == 3)
                captureTokens.insert(captureTokens.end(), {"history-legacy-input", "history-legacy-journal",
                    "history-legacy-complete"});
            else if (mm7CastleReload)
                captureTokens.insert(captureTokens.end(), {"castle-reload-input", "castle-reload-inn-revisit",
                    "castle-reentered-cleared"});
            else if (mm7CastleContinue)
                captureTokens.insert(captureTokens.end(), {"castle-diagnostic-input",
                    "castle-last-target-alive", "castle-cleared-inside",
                    "castle-cleared-return", "castle-reward-complete"});
            else
                captureTokens.insert(captureTokens.end(), {"castle-input", "castle-early-rejected",
                    "castle-trained", "castle-provisions", "castle-entered", "castle-combat", "castle-rested",
                    "castle-last-target-alive", "castle-cleared-inside", "castle-cleared-return",
                    "castle-reward-complete"});
        }
        if (mm7Shell)
        {
            const std::string prefix = mm7Contest ? "mm7-contest" : mm7Dungeon ? "mm7-dungeon" : mm7Temple ? "mm7-temple"
                : mm7Recruit ? "mm7-recruit" : mm7Potion ? "mm7-potion" : "mm7-shell";
            captureTokens.push_back(prefix + "-complete");
            if (mm7Temple)
            {
                if (!mm7ShellReload)
                    captureTokens.insert(captureTokens.end(), {prefix + "-equipped", prefix + "-door"});
            }
            else if (mm7ShellReload && !mm7Dungeon && !mm7Contest) captureTokens.push_back(prefix + "-reopened");
            else if (mm7Recruit)
            {
                captureTokens.insert(captureTokens.end(),
                    {prefix + "-offer", prefix + "-joined", prefix + "-reopened"});
            }
            else if (!mm7Dungeon && !mm7Contest)
            {
                captureTokens.insert(captureTokens.end(),
                    {prefix + (mm7Potion ? "-ready" : "-purchased"), prefix + "-delivered"});
                if (!mm7Potion) captureTokens.push_back("mm7-shell-seller");
            }
        }
        if (mm8Blackmail)
        {
            if (mm8BlackmailReload)
                captureTokens.insert(captureTokens.end(), {"blackmail-reload-input", "blackmail-reload-elgar",
                    "blackmail-reload-cove", "blackmail-reload-arion", "blackmail-reload-complete"});
            else
                captureTokens.insert(captureTokens.end(), {"blackmail-input", "blackmail-offer",
                    "blackmail-accepted", "blackmail-cove-entrance", "blackmail-cove-entered",
                    "blackmail-first-door", "blackmail-delivery-offer", "blackmail-delivered",
                    "blackmail-arion-reopened", "blackmail-returned-outdoors", "blackmail-return-offer",
                    "blackmail-reward", "blackmail-main-complete"});
        }
        if (mm8Delivery)
        {
            captureTokens.push_back("mm8-delivery-complete");
            if (expected->mapFileName == "out01.odm")
                captureTokens.insert(captureTokens.end(), {"boat-service", "boat-arrived"});
            if (expected->party.questBits.contains(4)) captureTokens.push_back("delivery-reopened");
            else captureTokens.insert(captureTokens.end(), {"delivery-arrival", "delivery-response"});
        }
        else if (mm8Exit)
        {
            captureTokens.push_back("temple-exit-complete");
            if (expected->mapFileName == "d05.blv")
                captureTokens.insert(captureTokens.end(), {"temple-exit-combat", "temple-exit-first-kill",
                    "temple-exit-hall", "temple-exit-upper", "temple-exit-confirm"});
        }
        else if (mm8FloorPuzzle)
        {
            captureTokens.push_back("floor-puzzle-complete");
            const auto &events = expected->indoorScene.eventRuntimeState;
            if (!events || events->mapVars[12] < 15)
            {
                if (!events || events->mapVars[12] == 0)
                    captureTokens.insert(captureTokens.end(), {"floor-puzzle-entry", "floor-puzzle-started"});
                captureTokens.push_back("floor-puzzle-solved");
                for (const FloorPuzzleButton &button : floorPuzzleButtons)
                    if (!events || events->mapVars[button.eventId - 100] == 0)
                        captureTokens.push_back("floor-button-" + std::to_string(button.eventId));
            }
        }
        else if (mm8Passage)
        {
            captureTokens.push_back("passage-complete");
            if (expected->indoorScene.partyRuntime.movementState.x > 4288.0f)
                captureTokens.push_back("passage-ramp-contact");
            else captureTokens.insert(captureTokens.end(), {"passage-door-closed", "passage-door-open"});
        }
        else if (mm8Stairs)
        {
            captureTokens.push_back("stair-puzzle-complete");
            const unsigned progress = expected->indoorScene.eventRuntimeState
                ? expected->indoorScene.eventRuntimeState->mapVars[31] : 0;
            if (progress == 4) captureTokens.push_back("stair-reload-ground");
            for (unsigned index = progress + 1; index <= 4; ++index)
                captureTokens.push_back("stair-button-" + std::to_string(index));
        }
        else if (mm8Chamber)
        {
            captureTokens.push_back("chamber-complete");
            if (expected->indoorScene.partyRuntime.movementState.footZ >= 100.0f)
                captureTokens.insert(captureTokens.end(),
                    {"chamber-gate-open", "chamber-combat", "chamber-first-kill"});
        }
        else if (mm8Revisit)
        {
            captureTokens.push_back("revisit-lower");
            if (expected->mapFileName == "out01.odm") captureTokens.push_back("revisit-entry");
        }
        else if (mm8Resupply)
        {
            captureTokens.push_back("resupply-letter");
            if (!expected->party.questBits.contains(3))
                captureTokens.insert(captureTokens.end(), {"resupply-tavern", "resupply-rest", "resupply-rested"});
        }
        else if (mm8Return)
        {
            captureTokens.push_back("return-town-complete");
            if (expected->mapFileName == "d05.blv")
                captureTokens.insert(captureTokens.end(), {"return-stairs", "return-door", "return-exited",
                    "return-island-two", "return-island-one"});
        }
        else if (mm8Dungeon)
        {
            if (expected->indoorScene.partyRuntime.movementState.footZ < 1100.0f)
            {
                captureTokens.insert(captureTokens.end(), {"dungeon-loot-complete", "dungeon-loot-inventory"});
                const auto &corpses = expected->indoorScene.worldRuntime.mapActorCorpseViews;
                if (corpses.size() <= 55 || !isConsumedCorpseView(corpses[51]) || !isConsumedCorpseView(corpses[55]))
                {
                    captureTokens.insert(captureTokens.end(), {"dungeon-recovered", "recovery-combat",
                        "recovery-first-kill"});
                    const bool supplies = std::any_of(expected->party.members.begin(), expected->party.members.end(),
                        [](const Character &member)
                        {
                            return std::any_of(member.inventory.begin(), member.inventory.end(),
                                [](const InventoryItem &item)
                                {
                                    return item.objectDescriptionId == 222 || item.objectDescriptionId == 223
                                        || item.objectDescriptionId == 226;
                                });
                        });
                    if (supplies) captureTokens.push_back("dungeon-supplies");
                }
            }
            else
                captureTokens.insert(captureTokens.end(), {"dungeon-upper-stairs", "dungeon-combat",
                    "dungeon-first-kill", "dungeon-landing-complete"});
        }
        else if (mm8Temple)
        {
            captureTokens.insert(captureTokens.end(), {"temple-simon-sheet", "temple-five-party"});
            if (expected->party.members.size() == 4)
                captureTokens.insert(captureTokens.end(), {"temple-exit-dialog", "temple-reenter-dialog",
                    "temple-exited", "temple-reentered", "temple-simon-greeting", "temple-simon-offer",
                    "temple-simon-joined"});
        }
        else if (mm8Portals)
        {
            captureTokens.push_back("temple-entry");
            if (expected->mapFileName != "d05.blv")
                captureTokens.insert(captureTokens.end(), {"temple-door", "temple-confirm", "temple-npc"});
            if (!expected->party.questBits.contains(2))
                captureTokens.insert(captureTokens.end(), {"portal-second-landed", "route-combat", "route-first-kill"});
            if (!expected->party.questBits.contains(1))
                captureTokens.insert(captureTokens.end(), {"route-equipped", "portal-town-landed",
                    "portal-first-landed", "timed-npc"});
        }
        if (mm8Shop)
        {
            captureTokens.insert(captureTokens.end(), {"shop-arrived", "shop-stock", "shop-complete"});
            if (!mm8ShopReload)
                captureTokens.insert(captureTokens.end(), {"shop-bought", "shop-sell", "shop-sold"});
        }
        if (paperdolls)
        {
            captureTokens.insert(captureTokens.end(), {"paperdoll-0", "paperdoll-1", "paperdoll-2", "paperdoll-3"});
            for (unsigned member = 0; member < 4; ++member)
            {
                captureTokens.push_back("paperdoll-first-" + std::to_string(member));
            }
            captureTokens.insert(captureTokens.end(), {"equipped-armor-open", "equipped-armor-sword",
                "equipped-armor-open-again", "equipment-restored"});
        }
        if (mm8Inn)
        {
            captureTokens.insert(captureTokens.end(), {"inn-arrived", "inn-complete"});
            if (!mm8InnReload)
                captureTokens.insert(captureTokens.end(), {"inn-first-hired", "inn-four-party"});
        }
        if (mm8Start)
        {
            captureTokens.push_back("mm8-complete");
            if (mm8StartReload) captureTokens.push_back("mm8-reopened");
            if (!mm8StartReload)
                captureTokens.insert(captureTokens.end(), {"mm8-chief", "mm8-crystal", "mm8-reward",
                    "mm8-offer", "mm8-joined"});
        }
        if (candlestick)
        {
            captureTokens.push_back("candle-repeat");
            if (!candlestickReload)
            {
                captureTokens.insert(captureTokens.end(), {"candle-accepted", "candle-reward"});
                if (expected->mapFileName == "6d02.blv") captureTokens.push_back("candle-outside");
            }
        }
        if (checkDefeat) captureTokens.push_back("defeat-revived");
        if (templeChestReload)
        {
            captureTokens.insert(captureTokens.end(), {"chest-reloaded", "chest-reopened"});
        }
        else if (templeChest)
        {
            captureTokens.insert(captureTokens.end(), {"chest-door", "chest-open", "chest-taken", "chest-reopened"});
            if (chestPointer)
            {
                captureTokens.push_back("chest-inspect");
            }
        }
        if (firstLetter)
            captureTokens.insert(captureTokens.end(), {"letter-resident", "letter-reward", "letter-repeat"});
        if (castleLetter)
        {
            captureTokens.insert(captureTokens.end(), {"castle-resident", "castle-reward", "castle-followup"});
            if (expected->mapFileName == "oute3.odm")
                captureTokens.insert(captureTokens.end(), {"castle-route", "castle-arrived"});
        }
        if (indoorCombat)
        {
            captureTokens.insert(captureTokens.end(), {"indoor-near", "indoor-hit", "indoor-loot"});
        }
        if (spellbookControls)
        {
            captureTokens.insert(captureTokens.end(), {"spell-details", "spell-targeting"});
        }
        if (checkTurns)
        {
            captureTokens.insert(captureTokens.end(), {"turn-action", "turn-movement"});
            if (expected->mapFileName == "7out01.odm")
            {
                captureTokens.push_back("turn-dialogue");
            }
        }
        if (hostileTurns)
        {
            captureTokens.insert(captureTokens.end(), {"turn-enemy", "turn-projectile", "turn-corpse"});
        }
        std::ofstream navigationLog;
        GameplayMinimapState beforeMovement;
        float startX = 0.0f;
        float startY = 0.0f;
        if (checkNavigation)
        {
            require(expected->mapFileName.ends_with(".odm"), "Navigation capture needs an outdoor save");
            for (int direction = 0; direction < 8; ++direction)
            {
                captureTokens.push_back("direction-" + std::to_string(direction));
            }
            captureTokens.insert(captureTokens.end(), {"zoom-out", "zoom-in", "moved"});
            navigationLog.open(output / "navigation.tsv");
            navigationLog.exceptions(std::ios::badbit | std::ios::failbit);
            navigationLog << "capture\tyaw\tzoom\tparty_u\tparty_v\tmap_x\tmap_y\tmap_width\tmap_height\n";
        }
        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                return game.loadGameData(files);
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Capture requires the Metal backend");
                if (!game.initializeRenderer()) return false;
                if ((mm6Replay && replayFirstStage == 0) || mm7Replay || (mm8Replay && replayFirstStage == 0))
                {
                    game.openNewGameScreen("normal_starter_replay");
                    auto *pCreation = dynamic_cast<NewGameScreen *>(game.m_screenManager.activeScreen());
                    require(pCreation != nullptr, "Normal application did not open character creation");
                    if (mm7Replay || mm6Replay)
                        NewGameScreenTestAccess::prepareNormalClassicParty(*pCreation, mm6Replay);
                    else NewGameScreenTestAccess::prepareNormalMm8Knight(*pCreation);
                    NewGameScreenTestAccess::confirm(*pCreation);
                    const Party *pParty = game.m_gameSession.gameplayScreenRuntime().party();
                    require(pParty && pParty->members().size() == (mm7Replay || mm6Replay ? 4 : 1)
                        && pParty->members()[0].level == 1
                        && pParty->members()[0].experience == 0 && pParty->gold() == 200,
                        "New-game replay did not begin with its original normal party");
                    input = output / "newgame-created.oysav";
                    require(game.quickSaveToPath(input,
                        mm6Replay ? "正常新建 MM6 开局" : mm7Replay ? "正常新建 MM7 开局" : "正常新建 MM8 开局", {}),
                        "Cannot record normal creation");
                    expected = loadGameDataFromPath(input, error);
                    require(expected.has_value(), "Cannot inspect the newly created normal party");
                }
                else if (!game.loadSessionFromPath(input)) return false;
                if (mm8Replay)
                    std::cout << "Replay origin: " << (replayFirstStage == 0 ? "normal-new-game" : "saved-stage")
                        << " first_stage=" << replayFirstStage << '\n';
                require(game.m_gameSession.currentMapFileName() == expected->mapFileName,
                    "Rendered application loaded a different map");
                if (mm6Replay && replayFirstStage != 0)
                {
                    require(game.quickSaveToPath(output / "cold-restored.oysav", "MM6 冷读档", {}),
                        "Cannot preserve cold MM6 continuation restore");
                    require(game.quickLoadFromPath(input, true), "Warm MM6 continuation reload failed");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "MM6 同会话读档", {}),
                        "Cannot preserve warm MM6 continuation restore");
                }
                if (mm8StartReload || mm8InnReload || mm8ShopReload || mm8Portals
                    || mm8BlackmailReload || mm7Shell)
                {
                    require(game.quickSaveToPath(output / "cold-restored.oysav", "匕伤岛冷读档", {}),
                        "Cannot save cold-restored MM8 result");
                    require(game.quickLoadFromPath(input, true), "In-session MM8 result reload failed");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "匕伤岛同会话读档", {}),
                        "Cannot save warm-restored MM8 result");
                }
                if (candlestickReload)
                {
                    require(game.quickSaveToPath(output / "cold-restored.oysav", "蜡烛台任务冷读档", {}),
                        "Cannot save cold-restored candle state");
                    require(game.quickLoadFromPath(input, true), "In-session candle result reload failed");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "蜡烛台任务同会话读档", {}),
                        "Cannot save warm-restored candle state");
                }
                if (firstLetterReload || castleLetterReload)
                {
                    require(game.quickSaveToPath(output / "cold-restored.oysav", "送信结果冷读档", {}),
                        "Cannot save cold-restored first letter state");
                }
                if (checkDefeat)
                {
                    require(expected->mapFileName == "6d02.blv" && expected->party.gold > 0,
                        "Defeat check needs the original temple encounter with carried gold");
                    require(game.m_pMapSceneRuntime && !game.m_pMapSceneRuntime->party().hasActableMember(),
                        "Defeat check must start from an actually unactable party");
                    std::cout << "Defeat input: gold=" << expected->party.gold << " deaths=" << startingDeaths
                        << " movie=" << game.resolvePartyDefeatCutsceneStem() << '\n';
                }
                if (checkTurnReload || templeChestReload)
                {
                    require(expected->turnBasedCombat.active, "Turn reload requires an active-turn save");
                    require(game.m_gameSession.turnBasedCombatRuntime().snapshot() == expected->turnBasedCombat,
                        "Cold load did not restore the exact turn state");
                    require(game.quickSaveToPath(output / "cold-restored.oysav", "回合冷读档", {}),
                        "Cannot save cold-restored turn state");
                    // Load again with an active old scene to test scene teardown/rebind as well as cold start.
                    require(game.quickLoadFromPath(input, true), "In-session active-turn reload failed");
                    require(game.m_gameSession.turnBasedCombatRuntime().snapshot() == expected->turnBasedCombat,
                        "In-session load changed the turn state");
                    require(game.quickSaveToPath(output / "warm-restored.oysav", "回合同会话读档", {}),
                        "Cannot save warm-restored turn state");
                    std::cout << "Cold and warm turn restore matched: stage=" << int(expected->turnBasedCombat.stage)
                        << " pending=" << expected->turnBasedCombat.pendingActions
                        << " movement_ap=" << expected->turnBasedCombat.movementActionPoints << '\n';
                }
                started = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                // Scripted route actions use the shared gameplay handlers below. Do not let
                // unrelated physical desktop input steer this automated run; keep window/quit events.
                if ((mm8Start || mm8Inn || paperdolls || mm8Shop || mm8Portals || mm8Blackmail
                        || mm8Replay || mm7Shell || mm7Replay || mm6Replay || mm6Goblinwatch
                        || mm7Castle || pausedMovement)
                    && ((event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_KEYMAP_CHANGED)
                    || (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
                    || (event.type >= SDL_EVENT_FINGER_DOWN && event.type <= SDL_EVENT_FINGER_CANCELED)))
                    return;
                game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                frameCaptureRequests.clear();
                if (pausedMovement && !pausedMovementCheck.complete)
                {
                    try
                    {
                        checkPausedMovement(game, frames + 1, width, height,
                            pausedMovementCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests) captureTokens.push_back(token);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "paused-movement-failed.oysav", "暂停行动点测试失败", {});
                        throw;
                    }
                }
                if (mm7Shell && !mm7ShellCheck.complete)
                {
                    try
                    {
                        if (mm7Contest)
                        {
                            checkMm7Contest(game, frames + 1, width, height,
                                mm7ShellReload, mm7ContestCheck, frameCaptureRequests);
                            mm7ShellCheck.complete = mm7ContestCheck.complete;
                        }
                        else if (mm7Dungeon)
                        {
                            checkMm7Dungeon(game, frames + 1, width, height,
                                mm7DungeonCheck, frameCaptureRequests, mm7ShellReload);
                            mm7ShellCheck.complete = mm7DungeonCheck.complete;
                        }
                        else if (mm7Temple)
                            checkMm7Temple(game, frames + 1, width, height,
                                mm7ShellReload, mm7ShellCheck, frameCaptureRequests);
                        else if (mm7Recruit)
                            checkMm7Recruit(game, frames + 1, width, height,
                                mm7ShellReload, mm7ShellCheck, frameCaptureRequests);
                        else
                            checkMm7Shell(game, frames + 1, width, height,
                                mm7ShellReload, mm7ShellCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            if (std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end())
                                captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(
                            output / (mm7Contest ? "mm7-contest-failed.oysav" : mm7Dungeon ? "mm7-dungeon-failed.oysav"
                                : mm7Temple ? "mm7-temple-failed.oysav" : mm7Recruit ? "mm7-recruit-failed.oysav"
                                : mm7Potion ? "mm7-potion-failed.oysav" : "mm7-shell-failed.oysav"),
                            "MM7 寻宝路线中止", {});
                        throw;
                    }
                }
                if (mm6Goblinwatch && !mm6GoblinwatchCheck.complete)
                {
                    try
                    {
                        checkMm6Goblinwatch(game, frames + 1, width, height, mm6GoblinwatchReload,
                            mm6GoblinwatchCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            if (std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end())
                                captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "goblinwatch-failed.oysav", "MM6大耳怪前哨中止", {});
                        throw;
                    }
                }
                if (mm7Castle && !mm7CastleCheck.complete)
                {
                    try
                    {
                        checkMm7Castle(game, frames + 1, width, height, mm7CastleReload, mm7CastleContinue,
                            mm7CastleHistoryMode, mm7CastleCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            if (std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end())
                                captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "mm7-castle-failed.oysav", "MM7哈蒙代尔城堡中止", {});
                        throw;
                    }
                }
                if (mm8Blackmail && !blackmailCheck.complete)
                {
                    try
                    {
                        checkMm8Blackmail(game, frames + 1, width, height, mm8BlackmailReload,
                            blackmailCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            if (std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end())
                                captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "mm8-blackmail-failed.oysav", "MM8走私者黑函中止", {});
                        throw;
                    }
                }
                if (mm6Replay && !mm6ReplayCheck.complete)
                {
                    try
                    {
                        checkMm6LetterReplay(game, frames + 1, width, height, output,
                            mm6ReplayCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            require(std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end(),
                                "MM6 replay requested a duplicate evidence capture");
                            captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "mm6-replay-failed.oysav", "MM6 连续送信中止", {});
                        throw;
                    }
                }
                if (mm7Replay && !mm7ReplayCheck.complete)
                {
                    try
                    {
                        checkMm7Replay(game, frames + 1, width, height, output, mm7ReplayCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            require(std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end(),
                                "MM7 replay requested a duplicate evidence capture");
                            captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "mm7-replay-failed.oysav", "MM7 连续开局中止", {});
                        throw;
                    }
                }
                if (mm8Replay && !replayCheck.complete)
                {
                    try
                    {
                        checkMm8Replay(game, frames + 1, width, height, output, replayCheck, frameCaptureRequests);
                        for (const std::string &token : frameCaptureRequests)
                        {
                            require(std::find(captureTokens.begin(), captureTokens.end(), token) == captureTokens.end(),
                                "Full replay requested a duplicate evidence capture");
                            captureTokens.push_back(token);
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "replay-failed.oysav", "MM8 完整开局中止", {});
                        throw;
                    }
                }
                if (mm8Portals && !portalCheck.complete)
                {
                    try
                    {
                        if (mm8Delivery)
                            checkMm8Delivery(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else if (mm8Exit)
                            checkMm8TempleExit(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else if (mm8Stairs)
                            checkMm8TempleMechanisms(game, frames + 1, width, height, portalCheck, frameCaptureRequests,
                                mm8FloorPuzzle ? Mm8TempleMechanism::FloorPuzzle : mm8Passage
                                ? Mm8TempleMechanism::Passage : Mm8TempleMechanism::Stairs);
                        else if (mm8Chamber)
                            checkMm8Dungeon(game, frames + 1, width, height, portalCheck, frameCaptureRequests, true);
                        else if (mm8Revisit)
                            checkMm8Revisit(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else if (mm8Resupply)
                            checkMm8Resupply(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else if (mm8Return)
                            checkMm8Return(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else if (mm8Dungeon)
                            checkMm8Dungeon(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else if (mm8Temple)
                            checkMm8Temple(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                        else checkMm8Portals(game, frames + 1, width, height, portalCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "portal-failed.oysav", "传送与废庙路线中止", {});
                        throw;
                    }
                }
                if (paperdolls && !paperdollsComplete)
                {
                    checkPaperdolls(game, frames + 1, width, height, paperdollsComplete,
                        equipmentCheck, frameCaptureRequests);
                }
                if (mm8Shop && !shopCheck.complete)
                {
                    try
                    {
                        checkMm8Shop(game, frames + 1, width, height, mm8ShopReload, shopCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "shop-failed.oysav", "商店流程中止", {});
                        throw;
                    }
                }
                if (mm8Inn && !innCheck.complete)
                {
                    try
                    {
                        checkMm8Inn(game, frames + 1, width, height, mm8InnReload, innCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "inn-failed.oysav", "旅店流程中止", {});
                        throw;
                    }
                }
                if (mm8Start && !mm8Check.complete)
                {
                    try
                    {
                        checkMm8Start(game, frames + 1, width, height, mm8StartReload,
                            mm8Check, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "mm8-failed.oysav", "匕伤岛流程中止", {});
                        throw;
                    }
                }
                if (candlestick && !candleCheck.complete)
                {
                    try
                    {
                        checkCandlestick(game, frames + 1, width, height, candlestickReload,
                            candleCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "candle-failed.oysav", "蜡烛台任务检查中止", {});
                        throw;
                    }
                }
                if (templeChest && !chestCheck.complete)
                {
                    try
                    {
                        checkTempleChest(game, frames + 1, width, height, chestCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "chest-failed.oysav", "宝箱流程中止", {});
                        throw;
                    }
                }
                if (indoorCombat && !indoorCheck.complete)
                {
                    try
                    {
                        checkIndoorCombat(game, frames + 1, indoorCheck);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "indoor-failed.oysav", "室内战斗检查中止", {});
                        throw;
                    }
                }
                if (checkDefeat && defeatSkipFrame != 0 && frames == defeatSkipFrame
                    && sawDefeatScreen && revivedFrame == 0)
                {
                    SDL_Event event = {};
                    event.type = SDL_EVENT_KEY_DOWN;
                    event.key.scancode = SDL_SCANCODE_ESCAPE;
                    event.key.key = SDLK_ESCAPE;
                    event.key.down = true;
                    game.handleSdlEvent(event);
                    event.type = SDL_EVENT_KEY_UP;
                    event.key.down = false;
                    game.handleSdlEvent(event);
                    std::cout << "Defeat cutscene skipped through normal Escape input after 120 frames\n";
                }
                // Once the asserted encounter and loot interaction finish, render its evidence
                // without advancing world time while asynchronous screenshot readbacks complete.
                const bool capturingCompletedEncounter = (hostileTurns && hostileCheck.complete)
                    || (indoorCombat && indoorCheck.complete) || (checkTurnReload && turnReloadComplete)
                    || (indoorTurnSaves && indoorTurnCheck.complete) || (firstLetter && firstLetterCheck.complete)
                    || (castleLetter && castleLetterCheck.complete) || (templeChest && chestCheck.complete)
                    || (indoorDoor && doorComplete) || (candlestick && candleCheck.complete)
                    || (mm8Start && mm8Check.complete) || (mm8Inn && innCheck.complete)
                    || (paperdolls && paperdollsComplete) || (mm8Shop && shopCheck.complete)
                    || (mm8Portals && portalCheck.complete) || (mm8Replay && replayCheck.complete)
                    || (mm6Replay && mm6ReplayCheck.complete)
                    || (mm7Replay && mm7ReplayCheck.complete)
                    || (mm7Shell && mm7ShellCheck.complete);
                auto *pIndoorScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
                const bool resolvingActorActions = (indoorEncounter || checkTurnReload) && pIndoorScene != nullptr
                    && game.m_gameSession.turnBasedCombatRuntime().snapshot().waitingForActorActionResolution;
                std::vector<uint32_t> attackDecisionsBefore;
                std::vector<bool> impactsBefore;
                if (resolvingActorActions)
                {
                    if (indoorCheck.resolutionFrames == 0)
                    {
                        require(game.quickSaveToPath(output / "resolution-wait.oysav", "怪物攻击收尾中", {}),
                            "Cannot save the real actor resolution phase");
                    }
                    ++indoorCheck.resolutionFrames;
                    const IndoorWorldRuntime &world = pIndoorScene->worldRuntime();
                    for (size_t index = 0; index < world.mapActorCount(); ++index)
                    {
                        const auto *pActor = world.mapActorAiState(index);
                        attackDecisionsBefore.push_back(pActor != nullptr ? pActor->attackDecisionCount : 0);
                        impactsBefore.push_back(pActor != nullptr && pActor->attackImpactTriggered);
                    }
                }
                game.renderFrame(width, height, wheel, capturingCompletedEncounter ? 0.0f : delta);
                if (resolvingActorActions)
                {
                    const IndoorWorldRuntime &world = pIndoorScene->worldRuntime();
                    for (size_t index = 0; index < attackDecisionsBefore.size(); ++index)
                    {
                        const auto *pActor = world.mapActorAiState(index);
                        if (pActor != nullptr && pActor->attackImpactTriggered && !impactsBefore[index])
                            ++indoorCheck.resolutionImpacts;
                        if (pActor == nullptr || pActor->attackDecisionCount == attackDecisionsBefore[index]) continue;
                        game.quickSaveToPath(output / "resolution-failed.oysav", "怪物回合收尾检查失败", {});
                        throw std::runtime_error("New monster combat decision during action resolution: actor="
                            + std::to_string(index) + " decisions=" + std::to_string(attackDecisionsBefore[index])
                            + "->" + std::to_string(pActor->attackDecisionCount));
                    }
                }
                if (resolvingActorActions
                    && game.m_gameSession.turnBasedCombatRuntime().stage() == TurnBasedCombatStage::Attack)
                {
                    ++indoorCheck.resolutionPhases;
                    std::cout << "Actor resolution passed: phases=" << indoorCheck.resolutionPhases
                        << " frames=" << indoorCheck.resolutionFrames
                        << " impacts=" << indoorCheck.resolutionImpacts << " new_decisions=0\n";
                    if (indoorCheck.resolutionPhases == 1)
                        require(game.quickSaveToPath(output / "resolution-complete.oysav", "怪物攻击收尾完成", {}),
                            "Cannot save completed actor resolution");
                }
                require(checkDefeat || game.m_screenManager.activeScreen() == nullptr
                    || (mm6Replay && !mm6ReplayCheck.openingComplete
                        && dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr)
                    || (mm7Replay && (!mm7ReplayCheck.openingComplete
                            || (mm7ReplayCheck.stage == 13 && mm7ReplayCheck.contest.phase == 63))
                        && dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr)
                    || (mm7Contest && mm7ContestCheck.departing && mm7ContestCheck.phase == 63
                        && dynamic_cast<CutsceneVideoScreen *>(game.m_screenManager.activeScreen()) != nullptr),
                    "Capture unexpectedly rendered a menu or cutscene instead of gameplay: frame=" + std::to_string(frames + 1)
                        + " mode=" + std::to_string(int(game.m_screenManager.currentMode())));
                ++frames;
                if (indoorDoor && !doorComplete && frames >= 150)
                {
                    auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
                    require(pScene && expected->indoorScene.eventRuntimeState,
                        "Door validation needs the actual indoor mechanism state");
                    const auto &before = expected->indoorScene.eventRuntimeState->mechanisms.at(3);
                    const auto &after = pScene->eventRuntimeState()->mechanisms.at(3);
                    std::cout << "Door progress in player turn: distance=" << before.currentDistance
                        << "->" << after.currentDistance << " moving=" << after.isMoving
                        << " game_minutes=" << expected->savedGameMinutes
                        << "->" << pScene->worldRuntime().gameMinutes()
                        << '\n';
                    require(game.quickSaveToPath(output / "door-observed.oysav", "回合中机关检查", {}),
                        "Cannot save observed door movement");
                    require(before.isMoving && !after.isMoving && after.currentDistance >= 251.9f,
                        "Door remained blocked while player turn was paused");
                    require(game.m_gameSession.turnBasedCombatRuntime().snapshot() == expected->turnBasedCombat,
                        "Advancing the door changed the saved combat turn");
                    require(pScene->worldRuntime().gameMinutes() == expected->savedGameMinutes,
                        "Advancing the door advanced game time during player selection");
                    doorComplete = true;
                }
                if (castleLetter && !castleLetterCheck.complete)
                {
                    try
                    {
                        checkCastleLetter(game, frames, width, height, castleLetterReload,
                            castleLetterCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "castle-failed.oysav", "城堡送信检查中止", {});
                        throw;
                    }
                }
                if (firstLetter && !firstLetterCheck.complete)
                {
                    try
                    {
                        checkFirstLetter(game, frames, width, height, firstLetterReload,
                            firstLetterCheck, frameCaptureRequests);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "letter-failed.oysav", "送信检查中止", {});
                        throw;
                    }
                }
                if (indoorCombat || hostileTurns || indoorTurnSaves)
                {
                    const std::string &status = game.m_gameSession.gameplayScreenRuntime().statusBarEventText();
                    if (status != lastCombatStatus)
                    {
                        lastCombatStatus = status;
                        if (!status.empty())
                        {
                            std::cout << "Rendered combat status: frame=" << frames << " text=" << status << '\n';
                            if (combatStatusCaptures < 16)
                            {
                                const std::string token = "combat-status-" + std::to_string(++combatStatusCaptures);
                                captureTokens.push_back(token);
                                std::ofstream record(output / (token + ".txt"));
                                record.exceptions(std::ios::badbit | std::ios::failbit);
                                record << status << '\n';
                                frameCaptureRequests.push_back(token.c_str());
                            }
                        }
                    }
                }
                if (checkDefeat)
                {
                    const IScreen *screen = game.m_screenManager.activeScreen();
                    require(screen == nullptr || dynamic_cast<const CutsceneVideoScreen *>(screen),
                        "Defeat flow opened an unexpected screen");
                    sawDefeatScreen = sawDefeatScreen || screen != nullptr;
                    if (game.m_gameSession.currentMapFileName() != expected->mapFileName)
                    {
                        require(game.m_gameSession.currentMapFileName() == "oute3.odm"
                            && dynamic_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get()),
                            "MM6 defeat did not return to New Sorpigal");
                        const Party &party = game.m_pMapSceneRuntime->party();
                        require(screen == nullptr && party.hasActableMember() && party.gold() == 0
                            && party.eventVariableValue(deathVariable) == startingDeaths + 1,
                            "Defeat penalty or recovered gameplay is incorrect");
                        require(!game.m_gameAudioSystem.isBackgroundMusicPaused(),
                            "Defeat cutscene left gameplay background music paused");
                        require(party.members().size() == expected->party.members.size(),
                            "Defeat changed party size");
                        for (const Character &member : party.members())
                        {
                            require(member.conditions.none() && member.health == member.maxHealth
                                && member.spellPoints == member.maxSpellPoints,
                                "Defeat did not clear conditions and restore health/mana");
                        }
                        if (revivedFrame == 0)
                        {
                            revivedFrame = frames;
                            std::cout << "Actual defeat recovery: frame=" << frames << " map=oute3.odm gold=0 deaths="
                                << party.eventVariableValue(deathVariable)
                                << " visible_cutscene=" << sawDefeatScreen << '\n';
                        }
                        if (frames == revivedFrame + 30)
                        {
                            frameCaptureRequests.push_back("defeat-revived");
                        }
                    }
                }
                if (indoorCombat)
                {
                    if (frames == indoorCheck.nearFrame) frameCaptureRequests.push_back("indoor-near");
                    if (frames == indoorCheck.hitFrame) frameCaptureRequests.push_back("indoor-hit");
                    if (frames == indoorCheck.finishedFrame)
                    {
                        frameCaptureRequests.push_back("indoor-loot");
                    }
                }
                if (hostileTurns)
                {
                    if (frames == 6)
                    {
                        frameCaptureRequests.push_back("turn-enemy");
                    }
                    if (frames == hostileCheck.projectileCaptureFrame)
                    {
                        frameCaptureRequests.push_back("turn-projectile");
                    }
                    if (hostileCheck.complete && frames == hostileCheck.finishedFrame + 1)
                    {
                        frameCaptureRequests.push_back("turn-corpse");
                    }
                }
                if (hostileTurns && !hostileCheck.complete)
                {
                    try
                    {
                        checkHostileTurns(game, frames, width, height, hostileCheck);
                        if (hostileCheck.complete)
                        {
                            require(game.quickSaveToPath(output / "combat-complete.oysav", "战斗拾取检查点", {}),
                                "Cannot save completed encounter before screenshot readback");
                            std::cout << "Encounter checkpoint saved; world time paused for screenshot readback.\n";
                        }
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "combat-failed.oysav", "战斗检查中止", {});
                        throw;
                    }
                }
                if (indoorTurnSaves && !indoorTurnCheck.complete)
                {
                    try
                    {
                        checkIndoorTurnSaves(game, frames, width, height, indoorTurnCheck);
                    }
                    catch (...)
                    {
                        game.quickSaveToPath(output / "indoor-turn-failed.oysav", "室内回合检查中止", {});
                        throw;
                    }
                }
                if (checkTurnSaves)
                {
                    const TurnBasedCombatRuntime &turn = game.m_gameSession.turnBasedCombatRuntime();
                    std::string label;
                    if (turn.active())
                    {
                        if (turn.pendingActions() > 0) label = "pending";
                        else if (turn.stage() == TurnBasedCombatStage::Wait) label = "wait";
                        else if (turn.playerActionReady(game.m_pMapSceneRuntime->party())
                            && (!indoorTurnSaves || game.m_pMapSceneRuntime->party().activeMemberIndex() != 0))
                            label = "ready";
                        else if (turn.stage() == TurnBasedCombatStage::Movement && turn.movementActionPoints() < 130)
                            label = "movement";
                    }
                    if (!label.empty() && savedTurnPhases.insert(label).second)
                    {
                        require(game.quickSaveToPath(output / ("turn-" + label + ".oysav"), "回合状态检查", {}),
                            "Cannot save active turn checkpoint");
                        const auto written = loadGameDataFromPath(output / ("turn-" + label + ".oysav"), error);
                        require(written && written->turnBasedCombat == turn.snapshot(),
                            "Written active turn state differs from live state");
                        std::cout << "Turn checkpoint saved: " << label << " frame=" << frames
                            << " pending=" << turn.pendingActions() << " movement_ap=" << turn.movementActionPoints()
                            << " queue=" << turn.queue().size() << '\n';
                    }
                }
                if (checkTurnReload && !turnReloadComplete)
                {
                    turnReloadSimulatedSeconds += delta;
                    TurnBasedCombatRuntime &turn = game.m_gameSession.turnBasedCombatRuntime();
                    const Party &party = game.m_pMapSceneRuntime->party();
                    require(turn.active(), "Reload silently left turn mode");
                    require(party.members().size() == expected->party.members.size(), "Reload changed party size");
                    for (size_t index = 0; index < party.members().size(); ++index)
                    {
                        require(party.members()[index].spellPoints == expected->party.members[index].spellPoints,
                            "Reload or projectile continuation spent mana twice");
                    }
                    if (turn.hasPendingActions())
                    {
                        require(!turn.playerActionReady(party), "Restored pending projectile allowed an early action");
                    }
                    if (expected->turnBasedCombat.pendingActions > 0 && !turn.hasPendingActions())
                    {
                        size_t damaged = 0;
                        int damage = 0;
                        if (game.m_pOutdoorWorldRuntime != nullptr)
                        {
                            const auto worldSnapshot = game.m_pOutdoorWorldRuntime->snapshot();
                            const auto &actors = worldSnapshot.mapActors;
                            require(actors.size() == expected->outdoorWorld.mapActors.size(), "Changed actor registry");
                            for (size_t index = 0; index < actors.size(); ++index)
                            {
                                require(actors[index].actorId == expected->outdoorWorld.mapActors[index].actorId,
                                    "Reload changed actor identity");
                                const int loss = expected->outdoorWorld.mapActors[index].currentHp
                                    - actors[index].currentHp;
                                if (loss > 0) { ++damaged; damage += loss; }
                            }
                        }
                        else
                        {
                            auto *pScene = dynamic_cast<IndoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
                            require(pScene && pScene->mapDeltaData() && expected->indoorScene.mapDeltaData,
                                "Indoor impact comparison lacks original actor records");
                            const auto &actors = pScene->mapDeltaData()->actors;
                            const auto &before = expected->indoorScene.mapDeltaData->actors;
                            require(actors.size() == before.size(), "Indoor reload changed actor registry");
                            for (size_t index = 0; index < actors.size(); ++index)
                            {
                                require(actors[index].monsterInfoId == before[index].monsterInfoId,
                                    "Indoor reload changed actor identity");
                                const int loss = before[index].hp - actors[index].hp;
                                if (loss > 0) { ++damaged; damage += loss; }
                            }
                        }
                        require(damaged == 1 && damage > 0, "Restored single-target Fire Bolt did not resolve one hit");
                        if (turnReloadImpactFrame == 0)
                        {
                            turnReloadImpactFrame = frames;
                            turnReloadImpactDamage = damage;
                            std::cout << "Restored projectile impact: damaged=" << damaged << " hp_loss=" << damage
                                << " frame=" << frames << '\n';
                        }
                        require(damage == turnReloadImpactDamage,
                            "Restored projectile caused more damage after its first resolution");
                        if (frames >= turnReloadImpactFrame + 30)
                        {
                            std::cout << "Restored projectile stayed resolved for 30 further normal frames\n";
                            turnReloadComplete = true;
                        }
                    }
                    else if (expected->turnBasedCombat.pendingActions == 0
                        && expected->turnBasedCombat.stage == TurnBasedCombatStage::Wait
                        && turn.stage() == TurnBasedCombatStage::Wait)
                    {
                        require(turnReloadSimulatedSeconds < 2.0f,
                            "Restored enemy wait exceeded its wait/action timer budget");
                    }
                    else if (expected->turnBasedCombat.pendingActions == 0 && frames >= 45)
                    {
                        if (expected->turnBasedCombat.stage == TurnBasedCombatStage::Movement)
                        {
                            require(turn.movementActionPoints() == expected->turnBasedCombat.movementActionPoints,
                                "Idle reload refilled spent movement points");
                        }
                        else if (expected->turnBasedCombat.stage == TurnBasedCombatStage::Wait)
                        {
                            require(turn.stage() != TurnBasedCombatStage::Wait, "Restored enemy wait did not advance");
                        }
                        else
                        {
                            require(turn.playerActionReady(party), "Restored player turn did not remain ready");
                        }
                        if (party.members().size() > 1)
                        {
                            GameplayInputFrame input;
                            input.screenWidth = width;
                            input.screenHeight = height;
                            const bool movement = expected->turnBasedCombat.stage == TurnBasedCombatStage::Movement;
                            const size_t actingMember = party.activeMemberIndex();
                            const auto action = movement ? KeyboardAction::Forward : KeyboardAction::Pass;
                            input.actions[keyboardActionIndex(action)] = {true, true, false};
                            const float beforeX = game.m_gameSession.activeWorldRuntime()->partyX();
                            const float beforeY = game.m_gameSession.activeWorldRuntime()->partyY();
                            game.m_gameSession.updateGameplay(input, 0.0f);
                            game.m_gameSession.updateGameplay({}, 0.0f);
                            if (movement)
                            {
                                const float moved = std::hypot(
                                    game.m_gameSession.activeWorldRuntime()->partyX() - beforeX,
                                    game.m_gameSession.activeWorldRuntime()->partyY() - beforeY);
                                const int expectedPoints = expected->turnBasedCombat.movementActionPoints - 26;
                                require(turn.movementActionPoints() == expectedPoints
                                    && moved > 1.0f, "Restored indoor movement did not consume one remaining step");
                                std::cout << "Restored party moved: distance=" << moved << '\n';
                            }
                            else
                            {
                                const auto entry = std::find_if(turn.queue().begin(), turn.queue().end(),
                                    [actingMember](const auto &item)
                                    {
                                        return item.kind == TurnBasedCombatRuntime::QueueEntry::Kind::Character
                                            && item.id == actingMember;
                                    });
                                require(entry != turn.queue().end() && entry->initiative > 0
                                    && party.activeMemberIndex() != actingMember,
                                    "Restored party action did not advance the saved member's recovery and selection");
                                std::cout << "Restored party pass: acted=" << actingMember
                                    << " next=" << party.activeMemberIndex() << " recovery_ticks=" << entry->initiative
                                    << '\n';
                            }
                        }
                        turnReloadComplete = true;
                    }
                    if (turnReloadComplete)
                    {
                        std::cout << "Turn continuation passed: stage=" << int(turn.stage())
                            << " simulated_seconds=" << turnReloadSimulatedSeconds
                            << " movement_ap=" << turn.movementActionPoints() << '\n';
                        require(game.quickSaveToPath(output / "turn-continued.oysav", "回合继续验证", {}),
                            "Cannot save continued turn");
                    }
                }
                if (checkTurns && !turnCheck.complete)
                {
                    checkTurnBasedControls(game, frames, width, height, turnCheck, frameCaptureRequests);
                }
                if (checkSpellbook && !spellbookControls && frames == 35)
                {
                    GameplayScreenRuntime &runtime = game.m_gameSession.gameplayScreenRuntime();
                    const Party *pParty = runtime.partyReadOnly();
                    const SpellEntry *pTorch = game.m_gameDataLoader.getSpellTable().findById(1);
                    require(pParty && pTorch && pParty->members().size() == 1, "No caster for formal spell check");
                    const Character &member = pParty->members().front();
                    const int manaBefore = member.spellPoints;
                    require(member.knowsSpell(1) && member.knowsSpell(2) && manaBefore > 0,
                        "Formal spell check requires learned spells and normal mana");
                    require(runtime.tryCastSpellFromMember(0, 1, pTorch->name)
                        && pParty->hasPartyBuff(PartyBuffId::TorchLight) && member.spellPoints == manaBefore - 1
                        && !runtime.tryCastSpellFromMember(0, 1, pTorch->name)
                        && member.spellPoints == manaBefore - 1,
                        "Formal scene-adapter cast failed its buff, cost or recovery check");
                    std::cout << "Formal Torch Light cast passed: mana=" << manaBefore << "->"
                        << member.spellPoints << ", buff active, immediate repeat rejected.\n";
                }
                if (checkSpellbook && frames == 60)
                {
                    GameplayScreenRuntime &runtime = game.m_gameSession.gameplayScreenRuntime();
                    const Party *pParty = runtime.partyReadOnly();
                    require(pParty && pParty->members().size() == 1
                        && pParty->members().front().knowsSpell(1) && pParty->members().front().knowsSpell(2),
                        "Spellbook capture requires the normal Sorcerer's learned-spell save");
                    runtime.openSpellbookOverlay();
                    require(runtime.spellbookReadOnly().active, "Formal gameplay could not open the spellbook");
                    std::cout << "Diagnostic shared spellbook open for the actual learned-spell save.\n";
                }
                if (spellbookControls)
                {
                    checkSpellbookControls(game, frames, width, height, controlsTargetFrame, controlsComplete,
                        frameCaptureRequests);
                }
                if (checkPartyAudio && frames == 60)
                {
                    Party *pParty = game.m_gameSession.gameplayScreenRuntime().party();
                    require(pParty != nullptr && !pParty->members().empty(), "No party for audio queue check");
                    pParty->requestSpeech(0, SpeechId::Yes);
                    std::cout << "Diagnostic party speech request queued through Party; no physical input.\n";
                }
                if (checkPartyAudio && frames == 65)
                {
                    const Party *pParty = game.m_gameSession.gameplayScreenRuntime().partyReadOnly();
                    require(pParty != nullptr && pParty->pendingAudioRequests().empty(),
                        "Formal gameplay frame did not drain the party audio queue");
                }
                if (checkNavigation)
                {
                    GameplayScreenRuntime &hud = game.m_gameSession.gameplayScreenRuntime();
                    const auto clickZoom = [&](bool zoomIn)
                    {
                        const auto rect = hud.resolveHudLayoutElement(zoomIn ? "OutdoorMinimapZoomIn"
                            : "OutdoorMinimapZoomOut", width, height, 0, 0);
                        require(rect.has_value(), "Cannot resolve minimap zoom button");
                        GameplayHudButtonInputConfig click;
                        click.screenWidth = width;
                        click.screenHeight = height;
                        click.pointerX = rect->x + rect->width * 0.5f;
                        click.pointerY = rect->y + rect->height * 0.5f;
                        click.allowInput = true;
                        click.leftButtonPressed = true;
                        GameplayHudInputController::handleGameplayHudButtonInput(hud, click);
                        click.leftButtonPressed = false;
                        GameplayHudInputController::handleGameplayHudButtonInput(hud, click);
                    };
                    const auto requestNavigationCapture = [&](const std::string &token)
                    {
                        GameplayMinimapState state;
                        require(hud.tryGetGameplayMinimapState(state), "Missing navigation state");
                        const auto rect = hud.resolveHudLayoutElement("OutdoorMinimap", width, height, 0, 0);
                        require(rect.has_value(), "Missing minimap rectangle");
                        navigationLog << token << '\t' << hud.gameplayCameraYawRadians() << '\t' << state.zoom
                            << '\t' << state.partyU << '\t' << state.partyV << '\t' << rect->x << '\t' << rect->y
                            << '\t' << rect->width << '\t' << rect->height << '\n';
                        frameCaptureRequests.push_back(token.c_str());
                    };
                    if (frames >= 20 && frames <= 160 && frames % 20 == 0)
                    {
                        const float yaw = float((frames - 20) / 20) * 0.7853981633974483f;
                        game.m_outdoorGameView.setCameraAngles(yaw, 0.0f);
                    }
                    if (frames >= 30 && frames <= 170 && frames % 20 == 10)
                    {
                        const int direction = int((frames - 30) / 20);
                        require(std::abs(std::remainder(hud.gameplayCameraYawRadians()
                            - float(direction) * 0.7853981633974483f, 6.283185307179586f)) < 0.0001f,
                            "Camera angle did not reach the requested navigation direction");
                        const auto arrow = hud.gameplayUiRuntime().minimapArrowTextureName(
                            direction == 0 ? 7 : direction - 1);
                        require(arrow && hud.gameplayUiRuntime().ensureHudTextureLoaded(*arrow).has_value(),
                            "Missing original direction arrow");
                        requestNavigationCapture("direction-" + std::to_string(direction));
                    }
                    if (frames == 175)
                    {
                        clickZoom(false);
                        clickZoom(false);
                        require(hud.gameplayMinimapZoomScale() == 0.5f, "Zoom-out click or minimum limit failed");
                    }
                    if (frames == 190)
                    {
                        requestNavigationCapture("zoom-out");
                    }
                    if (frames == 195)
                    {
                        clickZoom(true);
                        clickZoom(true);
                        clickZoom(true);
                        require(hud.gameplayMinimapZoomScale() == 2.0f, "Zoom-in click or maximum limit failed");
                    }
                    if (frames == 210)
                    {
                        requestNavigationCapture("zoom-in");
                    }
                    if (frames == 215)
                    {
                        clickZoom(false);
                        require(hud.gameplayMinimapZoomScale() == 1.0f, "Zoom did not return to default");
                        require(hud.tryGetGameplayMinimapState(beforeMovement), "Missing initial movement state");
                        startX = hud.partyX();
                        startY = hud.partyY();
                        game.m_outdoorGameView.setCameraAngles(0.0f, 0.0f);
                    }
                    if (frames >= 216 && frames <= 235)
                    {
                        OutdoorSceneRuntime *pScene =
                            dynamic_cast<OutdoorSceneRuntime *>(game.m_pMapSceneRuntime.get());
                        require(pScene != nullptr, "Missing outdoor movement runtime");
                        OutdoorMovementInput movement;
                        movement.forward = true;
                        movement.yawRadians = 0.0f;
                        pScene->advanceFrame(movement, 0.05f);
                    }
                    if (frames == 245)
                    {
                        GameplayMinimapState moved;
                        require(hud.tryGetGameplayMinimapState(moved), "Missing moved minimap state");
                        const float dx = hud.partyX() - startX;
                        const float dy = hud.partyY() - startY;
                        require(std::hypot(dx, dy) > 1.0f && std::abs(moved.partyU - beforeMovement.partyU
                            - dx / 65536.0f) < 0.00001f && std::abs(moved.partyV - beforeMovement.partyV
                            + dy / 65536.0f) < 0.00001f, "Minimap UV did not follow actual party movement");
                        std::cout << "Navigation movement: " << dx << ',' << dy << "; UV delta "
                            << moved.partyU - beforeMovement.partyU << ','
                            << moved.partyV - beforeMovement.partyV << '\n';
                        requestNavigationCapture("moved");
                    }
                }
                if (frames == (checkNavigation ? 35u : 30u)
                    || frames == (checkNavigation ? 155u : spellbookControls ? 120u : 150u))
                {
                    if (requireMinimap)
                    {
                        GameplayScreenRuntime &hud = game.m_gameSession.gameplayScreenRuntime();
                        GameplayMinimapState state;
                        require(hud.tryGetGameplayMinimapState(state) && !state.vectorBackground
                            && state.textureName == std::filesystem::path(expected->mapFileName).stem().string(),
                            "Capture did not resolve the original outdoor minimap name");
                        const auto texture = hud.gameplayUiRuntime().ensureHudTextureLoaded(state.textureName);
                        require(texture && texture->width > 0 && texture->height > 0,
                            "Original minimap failed to load through the shared HUD");
                        std::cout << "Minimap " << state.textureName << ": " << texture->width << 'x'
                            << texture->height << ", party UV=" << state.partyU << ',' << state.partyV << '\n';
                    }
                    if (fixtureSize != 0)
                    {
                        GameplayScreenRuntime &hud = game.m_gameSession.gameplayScreenRuntime();
                        for (size_t index = 0; index < fixtureSize; ++index)
                        {
                            const auto rect = hud.resolvePartyPortraitRect(width, height, index);
                            if (rect)
                            {
                                std::cout << "Portrait " << index << ": " << rect->x << ',' << rect->y << ' '
                                    << rect->width << 'x' << rect->height << " in " << width << 'x' << height << '\n';
                            }
                            require(rect && rect->x >= 0 && rect->y >= 0 && rect->x + rect->width <= width
                                && rect->y + rect->height <= height, "HUD portrait escaped screen bounds");
                            require(hud.resolvePartyPortraitIndexAtPoint(width, height,
                                rect->x + rect->width * 0.5f, rect->y + rect->height * 0.5f) == index,
                                "HUD portrait hit geometry differs from its visible slot");
                            for (const char *pId : {"OutdoorOptionsBar", "OutdoorBuffBodyPanel",
                                     "OutdoorBuffSkullPanel", "OutdoorMinimapFrame"})
                            {
                                const auto panel = hud.resolveHudLayoutElement(pId, width, height, 0, 0);
                                require(panel && (rect->x + rect->width <= panel->x
                                    || panel->x + panel->width <= rect->x || rect->y + rect->height <= panel->y
                                    || panel->y + panel->height <= rect->y), "Another HUD panel covers a portrait");
                            }
                            const Character &member = game.m_gameSession.partyState()->members()[index];
                            require(hud.gameplayUiRuntime()
                                .ensureHudTextureLoaded(hud.resolvePortraitTextureName(member)).has_value(),
                                "HUD fixture portrait image is missing");
                        }
                    }
                    frameCaptureRequests.push_back(frames < 100 ? "scene-first" : "scene-later");
                }
                // bgfx accepts one screenshot per framebuffer per frame. All labels requested
                // for this rendered frame share that one readback and its exact pixels.
                if (!frameCaptureRequests.empty())
                {
                    const std::string &primary = frameCaptureRequests.front();
                    require(pendingCaptureAliases.emplace(primary,
                            PendingSceneCapture{frames, frameCaptureRequests}).second,
                        "A screenshot token was requested twice before consumption");
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, primary.c_str());
                }
                for (auto pending = pendingCaptureAliases.begin(); pending != pendingCaptureAliases.end();)
                {
                    std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                        Engine::BgfxContext::consumeScreenshot(pending->first);
                    if (!capture)
                    {
                        if (frames - pending->second.requestedFrame > 300)
                        {
                            game.quickSaveToPath(output / "capture-timeout.oysav", "截图回读超时现场", {});
                            throw std::runtime_error("Screenshot readback did not arrive: " + pending->first);
                        }
                        ++pending;
                        continue;
                    }
                    SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                        SDL_PIXELFORMAT_BGRA32, capture->bgraPixels.data(), int(capture->width * 4));
                    require(pSurface != nullptr, "Cannot wrap captured Metal pixels");
                    bool written = true;
                    for (const std::string &token : pending->second.aliases)
                    {
                        written = SDL_SaveBMP(pSurface, (output / (token + ".bmp")).c_str()) && written;
                        ++captures;
                        std::cout << "Captured " << token << ": " << capture->width << 'x' << capture->height
                            << " readback=" << pending->first << '\n';
                    }
                    SDL_DestroySurface(pSurface);
                    require(written, "Cannot write captured scene bitmap");
                    pending = pendingCaptureAliases.erase(pending);
                }
                if ((captures == captureTokens.size() && (!spellbookControls || controlsComplete)
                    && (!checkTurns || turnCheck.complete) && (!hostileTurns || hostileCheck.complete)
                    && (!indoorCombat || indoorCheck.complete)
                    && (!firstLetter || firstLetterCheck.complete)
                    && (!castleLetter || castleLetterCheck.complete)
                    && (!templeChest || chestCheck.complete)
                    && (!candlestick || candleCheck.complete)
                    && (!mm8Start || mm8Check.complete)
                    && (!mm8Inn || innCheck.complete)
                    && (!paperdolls || paperdollsComplete)
                    && (!mm8Shop || shopCheck.complete)
                    && (!mm8Portals || portalCheck.complete)
                    && (!mm8Replay || replayCheck.complete)
                    && (!mm6Replay || mm6ReplayCheck.complete)
                    && (!mm6Goblinwatch || mm6GoblinwatchCheck.complete)
                    && (!mm7Castle || mm7CastleCheck.complete)
                    && (!mm8Blackmail || blackmailCheck.complete)
                    && (!pausedMovement || pausedMovementCheck.complete)
                    && (!mm7Replay || mm7ReplayCheck.complete)
                    && (!mm7Shell || mm7ShellCheck.complete)
                    && (!checkDefeat || (revivedFrame != 0 && frames >= revivedFrame + 180))
                    && frames >= (checkNavigation || spellbookControls ? 260u : 180u))
                    || frames >= (mm8Blackmail ? (mm8BlackmailReload ? 180000u : 300000u)
                        : mm7Castle ? (mm7CastleHistoryMode != 0 ? 12000u
                            : mm7CastleReload ? 30000u : 360000u)
                        : mm6Goblinwatch ? (mm6GoblinwatchReload ? 12000u : 120000u)
                        : mm6Replay ? 30000u : mm7Replay ? 150000u : mm8Replay ? 100000u : mm7Dungeon ? 60000u
                        : castleLetter || templeChest || candlestick
                        || mm8Start || mm8Inn || mm8Shop
                        || mm8Portals || mm7Shell ? 12000u
                        : hostileTurns || indoorCombat || checkDefeat ? 3600u : 900u)
                    || SDL_GetTicks() - started >= (mm8Blackmail ? (mm8BlackmailReload ? 1200000u : 2400000u)
                        : mm7Castle ? (mm7CastleHistoryMode != 0 ? 180000u
                            : mm7CastleReload ? 300000u : 3600000u)
                        : mm6Goblinwatch ? (mm6GoblinwatchReload ? 150000u : 900000u)
                        : mm6Replay ? 600000u : mm7Replay ? 1800000u
                        : mm8Replay || mm7Dungeon ? 900000u
                        : mm7Contest && mm7ContestCheck.departing ? 300000u : castleLetter || templeChest || candlestick
                        || mm8Start || mm8Inn || mm8Shop || mm8Portals || mm7Shell ? 150000u
                        : hostileTurns || indoorCombat || checkDefeat ? 60000u : 30000u))
                {
                    if ((mm8Dungeon || mm8Return || mm8Resupply || mm8Revisit || mm8Chamber || mm8Stairs)
                        && !portalCheck.complete)
                        game.quickSaveToPath(output / "dungeon-timeout.oysav", "废庙自动路线超时", {});
                    if (mm7Replay && !mm7ReplayCheck.complete)
                        game.quickSaveToPath(output / "mm7-replay-timeout.oysav", "MM7 连续开局超时", {});
                    require(!mm7Replay || mm7ReplayCheck.complete, "Continuous MM7 new-game replay did not complete");
                    if (mm6Replay && !mm6ReplayCheck.complete)
                        game.quickSaveToPath(output / "mm6-replay-timeout.oysav", "MM6 连续送信超时", {});
                    require(!mm6Replay || mm6ReplayCheck.complete, "Continuous MM6 letter replay did not complete");
                    if (mm6Goblinwatch && !mm6GoblinwatchCheck.complete)
                        game.quickSaveToPath(output / "goblinwatch-timeout.oysav", "MM6大耳怪前哨超时", {});
                    require(!mm6Goblinwatch || mm6GoblinwatchCheck.complete,
                        "Continuous MM6 Goblinwatch quest did not complete");
                    if (mm7Castle && !mm7CastleCheck.complete)
                        game.quickSaveToPath(output / "mm7-castle-timeout.oysav", "MM7哈蒙代尔城堡超时", {});
                    require(!mm7Castle || mm7CastleCheck.complete,
                        "Continuous MM7 Harmondale Castle quest did not complete");
                    if (mm8Blackmail && !blackmailCheck.complete)
                        game.quickSaveToPath(output / "mm8-blackmail-timeout.oysav", "MM8走私者黑函超时", {});
                    require(!mm8Blackmail || blackmailCheck.complete,
                        "Continuous MM8 blackmail quest did not complete");
                    if (mm8Replay && !replayCheck.complete)
                        game.quickSaveToPath(output / "replay-timeout.oysav", "MM8 完整开局超时", {});
                    if (mm7Shell && !mm7ShellCheck.complete)
                        game.quickSaveToPath(
                            output / (mm7Contest ? "mm7-contest-timeout.oysav" : mm7Dungeon ? "mm7-dungeon-timeout.oysav"
                                : mm7Temple ? "mm7-temple-timeout.oysav" : mm7Recruit ? "mm7-recruit-timeout.oysav"
                                : mm7Potion ? "mm7-potion-timeout.oysav" : "mm7-shell-timeout.oysav"),
                            "MM7 寻宝路线超时", {});
                    require(!mm7Shell || mm7ShellCheck.complete,
                        mm7Dungeon ? "MM7 dungeon route did not complete" : "MM7 scavenger route did not complete");
                    require(!mm8Replay || replayCheck.complete, "Normal new-game starter replay did not complete");
                    require(!mm8Portals || portalCheck.complete, "MM8 portal and temple route did not complete");
                    require(!mm8Shop || shopCheck.complete, "MM8 shop route did not complete");
                    require(!mm8Start || mm8Check.complete, "MM8 starting quest and recruitment did not complete");
                    require(!mm8Inn || innCheck.complete, "MM8 inn route did not complete");
                    require(!paperdolls || paperdollsComplete, "Paperdoll inspection did not complete");
                    require(!candlestick || candleCheck.complete, "Candlestick route did not complete");
                    require(!templeChest || chestCheck.complete, "Temple chest route did not finish before timeout");
                    require(!castleLetter || castleLetterCheck.complete, "Castle letter route did not complete");
                    require(!firstLetter || firstLetterCheck.complete, "First letter route did not complete");
                    require(!checkTurnSaves || savedTurnPhases.size() == 4,
                        "Not all four active-turn phases were saved");
                    require(!indoorTurnSaves || indoorTurnCheck.complete, "Indoor turn save route did not complete");
                    require(!checkTurnReload || turnReloadComplete, "Restored turn did not continue correctly");
                    require(!indoorCombat || indoorCheck.complete, "Indoor combat did not finish before timeout");
                    require(!hostileTurns || hostileCheck.complete,
                        "Hostile turn combat did not complete before timeout");
                    require(!checkTurns || turnCheck.complete,
                        "Turn controls did not complete before timeout");
                    require(!pausedMovement || pausedMovementCheck.complete, "Paused movement check did not finish");
                    require(!spellbookControls || controlsComplete,
                        "Spellbook controls did not complete before timeout");
                    require(!checkDefeat || revivedFrame != 0, "Defeat recovery did not complete");
                    require(game.m_gameSession.currentMapFileName()
                            == (mm7Dungeon ? (mm7DungeonCheck.exitAtEnd ? "7out01.odm" : mm7DungeonCheck.dungeonMap)
                                : mm6Replay ? (mm6ReplayEndStage == 9 ? "oute3.odm"
                                    : mm6ReplayEndStage >= 6 ? "6d02.blv"
                                    : mm6ReplayEndStage >= 4 ? "oute3.odm" : "outd3.odm")
                                : mm8Blackmail ? (mm8BlackmailReload ? "d07.blv" : "out02.odm")
                                : mm7Castle ? (mm7CastleHistoryMode == 3 ? "7out02.odm"
                                    : mm7CastleHistoryMode != 0 || mm7CastleReload ? "7d29.blv" : "7out02.odm")
                                : mm6Goblinwatch ? "oute3.odm"
                                : mm7Replay ? "7out02.odm"
                                : mm7Contest ? (mm7ContestCheck.departureComplete ? "7out02.odm" : "7out01.odm")
                                : mm7Temple ? "7d06.blv" : (mm8Delivery || mm8Replay) ? "out02.odm"
                                : (mm8Return || mm8Resupply || mm8Exit) ? "out01.odm"
                                : mm8Portals ? "d05.blv"
                                : castleLetter ? "outd3.odm" : checkDefeat || candlestick ? "oute3.odm"
                                : expected->mapFileName),
                        "Scene changed during unattended capture");
                    saved = game.quickSaveToPath(output / "after-render.oysav", "画面验证", {});
                    const Engine::AudioSystem::CacheStats audio = game.m_gameAudioSystem.cacheStats();
                    std::cout << "Application audio cache: clips=" << audio.clipCount
                        << " pcm_bytes=" << audio.sampleBytes << " transient_pcm_bytes=" << audio.transientSampleBytes
                        << " evictions=" << audio.evictedClipCount << '\n';
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                game.shutdownApplication();
            },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });
        const int result = application.run();
        require(result == 0 && captures == captureTokens.size() && saved,
            "Application capture did not complete all screenshots and a save");
        std::cout << "Actual Metal application rendered " << frames << " frames and saved "
            << (mm7Dungeon ? (mm7DungeonCheck.exitAtEnd ? "7out01.odm" : mm7DungeonCheck.dungeonMap)
                                : mm6Replay ? (mm6ReplayEndStage == 9 ? "oute3.odm"
                                    : mm6ReplayEndStage >= 6 ? "6d02.blv"
                                    : mm6ReplayEndStage >= 4 ? "oute3.odm" : "outd3.odm")
                                : mm8Blackmail ? (mm8BlackmailReload ? "d07.blv" : "out02.odm")
                                : mm7Castle ? (mm7CastleHistoryMode == 3 ? "7out02.odm"
                                    : mm7CastleHistoryMode != 0 || mm7CastleReload ? "7d29.blv" : "7out02.odm")
                                : mm6Goblinwatch ? "oute3.odm"
                                : mm7Replay ? "7out02.odm"
                                : mm7Contest ? (mm7ContestCheck.departureComplete ? "7out02.odm" : "7out01.odm")
                                : mm7Temple ? "7d06.blv" : (mm8Delivery || mm8Replay) ? "out02.odm"
                : (mm8Return || mm8Resupply || mm8Exit) ? "out01.odm"
                : mm8Portals ? "d05.blv"
                : castleLetter ? "outd3.odm" : checkDefeat ? "oute3.odm" : expected->mapFileName)
            << "; no physical iPad or accessory validation.\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 4 || argc > 12)
        {
            throw std::runtime_error(
                "Usage: application_scene_capture RUNTIME INPUT_SAVE NEW_OUTPUT "
                "[--hud-fixture=1|5] [--require-minimap] [--check-navigation] [--check-party-audio] "
                "[--check-mm6-letter-replay|--check-mm6-training-replay|--check-mm6-training|--check-mm6-training-reload] "
                "[--check-spellbook] [--check-spellbook-controls] [--check-turn-based] [--check-hostile-turns] "
                "[--check-indoor-combat|--check-indoor-encounter] [--check-defeat|--check-defeat-complete] "
                "[--check-turn-saves|--check-turn-reload|--check-indoor-turn-saves] "
                "[--check-first-letter|--check-first-letter-reload] "
                "[--check-castle-letter|--check-castle-letter-reload] "
                "[--check-temple-chest|--check-temple-chest-reload|--check-chest-pointer|--check-indoor-door] "
                "[--check-candlestick|--check-candlestick-reload] [--check-mm8-start|--check-mm8-start-reload] "
                "[--check-mm6-skills|--check-mm6-skills-reload|--check-mm6-temple|--check-mm6-temple-reload] "
                "[--check-mm6-encounter|--check-mm6-encounter-reload] "
                "[--check-mm6-chest|--check-mm6-chest-reload] "
                "[--check-mm6-candle|--check-mm6-candle-reload|--check-mm6-candle-replay] "
                "[--check-mm6-dungeon-quest] [--check-mm6-goblinwatch|--check-mm6-goblinwatch-reload] "
                "[--check-paused-movement] "
                "[--check-mm8-inn|--check-mm8-inn-reload] [--check-paperdolls] "
                "[--check-mm7-shell|--check-mm7-shell-reload|--check-mm7-potion|--check-mm7-potion-reload] "
                "[--check-mm7-recruit|--check-mm7-recruit-reload] "
                "[--check-mm7-temple|--check-mm7-temple-reload|--check-mm7-dungeon|--check-mm7-dungeon-reload] "
                "[--check-mm7-castle|--check-mm7-castle-reload|--check-mm7-castle-continue|"
                "--check-mm7-history|--check-mm7-history-reload|--check-mm7-history-legacy] "
                "(MM7 walking modes read named route TSVs beside NEW_OUTPUT; reload modes need none.) "
                "[--check-mm8-shop|--check-mm8-shop-reload] "
                "[--check-mm8-portals|--check-mm8-temple|--check-mm8-dungeon|--check-mm8-return|--check-mm8-resupply] "
                "[--check-mm8-revisit|--check-mm8-chamber|--check-mm8-stairs|--check-mm8-passage] "
                "[--check-mm8-floor-puzzle|--check-mm8-exit|--check-mm8-delivery|"
                "--check-mm8-blackmail|--check-mm8-blackmail-reload|--check-mm8-replay|"
                "--check-mm8-replay-from=1..15]");
        }
        size_t fixtureSize = 0;
        bool requireMinimap = false;
        bool checkNavigation = false;
        bool checkPartyAudio = false;
        bool checkSpellbook = false;
        bool pausedMovement = false;
        bool spellbookControls = false;
        bool checkTurns = false;
        bool hostileTurns = false;
        bool checkTurnSaves = false;
        bool checkTurnReload = false;
        bool indoorTurnSaves = false;
        bool firstLetter = false;
        bool firstLetterReload = false;
        bool castleLetter = false;
        bool castleLetterReload = false;
        bool indoorCombat = false;
        bool indoorEncounter = false;
        bool templeChest = false;
        bool templeChestReload = false;
        bool chestPointer = false;
        bool indoorDoor = false;
        bool candlestick = false;
        bool candlestickReload = false;
        bool mm8Start = false;
        bool mm8StartReload = false;
        bool mm8Inn = false;
        bool mm8InnReload = false;
        bool paperdolls = false;
        bool mm8Shop = false;
        bool mm8ShopReload = false;
        bool mm8Portals = false;
        bool mm8Temple = false;
        bool mm8Dungeon = false;
        bool mm8Return = false;
        bool mm8Resupply = false;
        bool mm8Revisit = false;
        bool mm8Chamber = false;
        bool mm8Stairs = false;
        bool mm8Passage = false;
        bool mm8FloorPuzzle = false;
        bool mm8Exit = false;
        bool mm8Delivery = false;
        bool mm8Blackmail = false;
        bool mm8BlackmailReload = false;
        bool mm7Shell = false;
        bool mm7ShellReload = false;
        bool mm7Potion = false;
        bool mm7Recruit = false;
        bool mm7Temple = false;
        bool mm7Dungeon = false;
        bool mm7Contest = false;
        bool mm6Replay = false;
        size_t mm6ReplayEndStage = 2;
        bool mm6Goblinwatch = false;
        bool mm6GoblinwatchReload = false;
        bool mm7Castle = false;
        bool mm7CastleReload = false;
        bool mm7CastleContinue = false;
        unsigned mm7CastleHistoryMode = 0;
        bool mm7Replay = false;
        bool mm8Replay = false;
        size_t replayFirstStage = 0;
        bool checkDefeat = false;
        uint32_t defeatSkipFrame = 120;
        for (int index = 4; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--require-minimap" && !requireMinimap)
            {
                requireMinimap = true;
            }
            else if (option == "--check-paused-movement" && !pausedMovement)
            {
                pausedMovement = true;
            }
            else if ((option == "--check-mm6-goblinwatch"
                || option == "--check-mm6-goblinwatch-reload") && !mm6Goblinwatch)
            {
                mm6Goblinwatch = true;
                mm6GoblinwatchReload = option.ends_with("-reload");
            }
            else if ((option == "--check-mm7-history"
                || option == "--check-mm7-history-reload"
                || option == "--check-mm7-history-legacy") && !mm7Castle)
            {
                mm7Castle = true;
                mm7CastleHistoryMode = option == "--check-mm7-history" ? 1
                    : option == "--check-mm7-history-reload" ? 2 : 3;
            }
            else if ((option == "--check-mm7-castle"
                || option == "--check-mm7-castle-reload"
                || option == "--check-mm7-castle-continue") && !mm7Castle)
            {
                mm7Castle = true;
                mm7CastleReload = option.ends_with("-reload");
                mm7CastleContinue = option.ends_with("-continue");
            }
            else if ((option == "--check-mm6-letter-replay" || option == "--check-mm6-training-replay"
                || option == "--check-mm6-training" || option == "--check-mm6-training-reload"
                || option == "--check-mm6-return" || option == "--check-mm6-return-replay"
                || option == "--check-mm6-return-reload" || option == "--check-mm6-skills"
                || option == "--check-mm6-skills-reload" || option == "--check-mm6-temple"
                || option == "--check-mm6-temple-reload" || option == "--check-mm6-encounter"
                || option == "--check-mm6-encounter-reload" || option == "--check-mm6-chest"
                || option == "--check-mm6-chest-reload" || option == "--check-mm6-candle"
                || option == "--check-mm6-candle-reload" || option == "--check-mm6-candle-replay"
                || option == "--check-mm6-dungeon-quest") && !mm6Replay)
            {
                mm6Replay = true;
                replayFirstStage = option == "--check-mm6-candle-reload" ? 9
                    : option == "--check-mm6-candle" || option == "--check-mm6-chest-reload" ? 8
                    : option == "--check-mm6-chest" || option == "--check-mm6-encounter-reload" ? 7
                    : option == "--check-mm6-encounter" || option == "--check-mm6-temple-reload"
                        || option == "--check-mm6-dungeon-quest" ? 6
                    : option == "--check-mm6-temple" || option == "--check-mm6-skills-reload" ? 5
                    : option == "--check-mm6-skills" || option == "--check-mm6-return-reload" ? 4
                    : option == "--check-mm6-training-reload" || option == "--check-mm6-return" ? 3
                    : option == "--check-mm6-training" ? 2 : 0;
                mm6ReplayEndStage = option.starts_with("--check-mm6-candle")
                    || option == "--check-mm6-dungeon-quest" ? 9
                    : option.starts_with("--check-mm6-chest") ? 8
                    : option.starts_with("--check-mm6-encounter") ? 7
                    : option.starts_with("--check-mm6-temple") ? 6
                    : option.starts_with("--check-mm6-skills") ? 5
                    : option.starts_with("--check-mm6-return") ? 4
                    : option == "--check-mm6-letter-replay" ? 2 : 3;
            }
            else if (option == "--check-mm7-replay" && !mm7Replay)
            {
                mm7Replay = true;
            }
            else if ((option == "--check-mm7-shell" || option == "--check-mm7-shell-reload"
                || option == "--check-mm7-potion" || option == "--check-mm7-potion-reload"
                || option == "--check-mm7-recruit" || option == "--check-mm7-recruit-reload"
                || option == "--check-mm7-temple" || option == "--check-mm7-temple-reload"
                || option == "--check-mm7-dungeon" || option == "--check-mm7-dungeon-reload"
                || option == "--check-mm7-contest" || option == "--check-mm7-contest-reload") && !mm7Shell)
            {
                mm7Shell = true;
                mm7ShellReload = option.ends_with("-reload");
                mm7Potion = option.starts_with("--check-mm7-potion");
                mm7Recruit = option.starts_with("--check-mm7-recruit");
                mm7Temple = option.starts_with("--check-mm7-temple");
                mm7Dungeon = option.starts_with("--check-mm7-dungeon");
                mm7Contest = option.starts_with("--check-mm7-contest");
            }
            else if (option == "--check-navigation" && !checkNavigation)
            {
                checkNavigation = true;
            }
            else if (option == "--check-party-audio" && !checkPartyAudio)
            {
                checkPartyAudio = true;
            }
            else if (option == "--check-spellbook" && !checkSpellbook)
            {
                checkSpellbook = true;
            }
            else if (option == "--check-spellbook-controls" && !spellbookControls)
            {
                spellbookControls = true;
            }
            else if (option == "--check-turn-based" && !checkTurns)
            {
                checkTurns = true;
            }
            else if (option == "--check-hostile-turns" && !hostileTurns)
            {
                hostileTurns = true;
            }
            else if (option == "--check-turn-saves" && !checkTurnSaves && !hostileTurns)
            {
                checkTurnSaves = true;
                hostileTurns = true;
            }
            else if (option == "--check-indoor-turn-saves" && !checkTurnSaves)
            {
                checkTurnSaves = true;
                indoorTurnSaves = true;
            }
            else if (option == "--check-turn-reload" && !checkTurnReload)
            {
                checkTurnReload = true;
            }
            else if ((option == "--check-first-letter" || option == "--check-first-letter-reload") && !firstLetter)
            {
                firstLetter = true;
                firstLetterReload = option == "--check-first-letter-reload";
            }
            else if ((option == "--check-castle-letter" || option == "--check-castle-letter-reload") && !castleLetter)
            {
                castleLetter = true;
                castleLetterReload = option == "--check-castle-letter-reload";
            }
            else if ((option == "--check-mm8-blackmail" || option == "--check-mm8-blackmail-reload")
                && !mm8Blackmail)
            {
                mm8Blackmail = true;
                mm8BlackmailReload = option == "--check-mm8-blackmail-reload";
            }
            else if (option.starts_with("--check-mm8-replay-from=") && argc == 5)
            {
                replayFirstStage = std::stoul(option.substr(std::string("--check-mm8-replay-from=").size()));
                if (replayFirstStage == 0 || replayFirstStage >= 16)
                    throw std::runtime_error("Saved continuation requires stage 1 through 15");
                mm8Replay = true;
            }
            else if (option == "--check-mm8-replay" && argc == 5)
            {
                mm8Replay = true;
            }
            else if ((option == "--check-mm8-portals" || option == "--check-mm8-temple"
                || option == "--check-mm8-dungeon" || option == "--check-mm8-return"
                || option == "--check-mm8-resupply" || option == "--check-mm8-revisit"
                || option == "--check-mm8-chamber" || option == "--check-mm8-stairs"
                || option == "--check-mm8-passage" || option == "--check-mm8-floor-puzzle"
                || option == "--check-mm8-exit" || option == "--check-mm8-delivery") && !mm8Portals)
            {
                mm8Portals = true;
                mm8Temple = option == "--check-mm8-temple";
                mm8Dungeon = option == "--check-mm8-dungeon";
                mm8Return = option == "--check-mm8-return";
                mm8Resupply = option == "--check-mm8-resupply";
                mm8Revisit = option == "--check-mm8-revisit";
                mm8Chamber = option == "--check-mm8-chamber";
                mm8Passage = option == "--check-mm8-passage";
                mm8FloorPuzzle = option == "--check-mm8-floor-puzzle";
                mm8Exit = option == "--check-mm8-exit";
                mm8Delivery = option == "--check-mm8-delivery";
                mm8Stairs = option == "--check-mm8-stairs" || mm8Passage || mm8FloorPuzzle;
            }
            else if ((option == "--check-mm8-shop" || option == "--check-mm8-shop-reload") && !mm8Shop)
            {
                mm8Shop = true;
                mm8ShopReload = option == "--check-mm8-shop-reload";
            }
            else if (option == "--check-paperdolls" && !paperdolls)
            {
                paperdolls = true;
            }
            else if ((option == "--check-mm8-inn" || option == "--check-mm8-inn-reload") && !mm8Inn)
            {
                mm8Inn = true;
                mm8InnReload = option == "--check-mm8-inn-reload";
            }
            else if ((option == "--check-mm8-start" || option == "--check-mm8-start-reload") && !mm8Start)
            {
                mm8Start = true;
                mm8StartReload = option == "--check-mm8-start-reload";
            }
            else if ((option == "--check-candlestick" || option == "--check-candlestick-reload") && !candlestick)
            {
                candlestick = true;
                candlestickReload = option == "--check-candlestick-reload";
            }
            else if (option == "--check-indoor-door" && !indoorDoor)
            {
                indoorDoor = true;
            }
            else if ((option == "--check-temple-chest" || option == "--check-temple-chest-reload"
                || option == "--check-chest-pointer") && !templeChest)
            {
                templeChest = true;
                templeChestReload = option == "--check-temple-chest-reload";
                chestPointer = option == "--check-chest-pointer";
            }
            else if (option == "--check-indoor-encounter" && !indoorCombat)
            {
                indoorCombat = true;
                indoorEncounter = true;
            }
            else if (option == "--check-indoor-combat" && !indoorCombat)
            {
                indoorCombat = true;
            }
            else if (option == "--check-defeat" && !checkDefeat)
            {
                checkDefeat = true;
            }
            else if (option == "--check-defeat-complete" && !checkDefeat)
            {
                checkDefeat = true;
                defeatSkipFrame = 0;
            }
            else if ((option == "--hud-fixture=1" || option == "--hud-fixture=5") && fixtureSize == 0)
            {
                fixtureSize = option.back() == '1' ? 1 : 5;
            }
            else
            {
                throw std::runtime_error("Unsupported or repeated capture option");
            }
        }
        if ((checkTurns || hostileTurns)
            && (fixtureSize != 0 || checkNavigation || checkSpellbook || spellbookControls
                || (checkTurns && hostileTurns)))
        {
            throw std::runtime_error("Turn controls need an independent normal-party capture");
        }
        if (indoorCombat && (checkTurns || hostileTurns || fixtureSize != 0 || checkNavigation
            || checkPartyAudio || checkSpellbook || spellbookControls || requireMinimap))
        {
            throw std::runtime_error("Indoor combat needs an independent normal-party capture");
        }
        if ((checkDefeat || checkTurnReload || checkTurnSaves || firstLetter || castleLetter
            || templeChest || indoorDoor || candlestick || mm8Start || mm8Inn || paperdolls || mm8Shop
            || mm8Portals || mm8Blackmail || mm7Shell || mm7Replay || mm6Replay || mm6Goblinwatch || mm7Castle
            || pausedMovement) && argc != 5)
        {
            throw std::runtime_error("This validation needs an independent capture with exactly one mode option");
        }
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(std::filesystem::canonical(argv[1]),
            (mm6Replay && replayFirstStage == 0) || mm7Replay || (mm8Replay && replayFirstStage == 0)
                ? std::filesystem::path{} : std::filesystem::canonical(argv[2]),
            std::filesystem::absolute(argv[3]), fixtureSize,
            requireMinimap || checkNavigation, checkNavigation, checkPartyAudio,
            checkSpellbook || spellbookControls, spellbookControls, checkTurns, hostileTurns, indoorCombat,
            checkDefeat, defeatSkipFrame, checkTurnSaves, checkTurnReload, indoorTurnSaves,
            firstLetter, firstLetterReload, castleLetter, castleLetterReload, indoorEncounter,
            templeChest, indoorDoor, templeChestReload, candlestick, candlestickReload, chestPointer,
            mm8Start, mm8StartReload, mm8Inn, mm8InnReload, paperdolls, mm8Shop, mm8ShopReload,
            mm8Portals, mm8Temple, mm8Dungeon, mm8Return, mm8Resupply, mm8Revisit, mm8Chamber,
            mm8Stairs, mm8Passage, mm8FloorPuzzle, mm8Exit, mm8Delivery,
            mm8Blackmail, mm8BlackmailReload, mm8Replay, replayFirstStage,
            mm7Shell, mm7ShellReload, mm7Potion, mm7Recruit, mm7Temple, mm7Dungeon, mm7Contest,
            mm7Replay, mm6Replay, mm6ReplayEndStage, mm6Goblinwatch, mm6GoblinwatchReload,
            mm7Castle, mm7CastleReload, mm7CastleContinue, mm7CastleHistoryMode, pausedMovement);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
