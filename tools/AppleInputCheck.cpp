#include "game/app/GameInputSystem.h"
#include "game/app/GameSession.h"
#include "game/gameplay/GameplayOverlayInputController.h"
#include "game/gameplay/GameplayHudInputController.h"
#include "game/gameplay/GameplayInteractionController.h"
#include "game/gameplay/GameplayScreenController.h"
#include "game/ui/Utf8Text.h"
#include "game/ui/GameplayOverlayAdapters.h"
#include "game/ui/CharacterSkillsLayout.h"
#include "game/gameplay/GameplayPartyOverlayInputController.h"
#include "tests/PartySpellTestHarness.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace OpenYAMM::Game;
    if (!SDL_Init(SDL_INIT_EVENTS))
    {
        return 2;
    }
    try
    {
        GameInputSystem input;
        GameSettings settings = GameSettings::createDefault();
        const auto require = [](bool condition, const std::string &message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        };
        const auto finger = [&](SDL_EventType type, SDL_FingerID id, float x, float y)
        {
            SDL_Event event = {};
            event.type = type;
            event.tfinger.fingerID = id;
            event.tfinger.x = x;
            event.tfinger.y = y;
            event.tfinger.timestamp = SDL_GetTicksNS();
            input.handleSdlEvent(event);
        };
        const auto update = [&](bool block = false, bool touchEnabled = true)
        {
            input.updateFromEngineInput(1440, 1000, 0, settings, block, touchEnabled, true, false, false);
        };
        finger(SDL_EVENT_FINGER_DOWN, 1, 0.2f, 0.6f);
        update();
        finger(SDL_EVENT_FINGER_MOTION, 1, 0.2f, 0.4f);
        update();
        require(input.frame().action(KeyboardAction::Forward).held, "Touch joystick did not move forward");
        finger(SDL_EVENT_FINGER_DOWN, 2, 0.75f, 0.4f);
        update();
        finger(SDL_EVENT_FINGER_MOTION, 2, 0.80f, 0.42f);
        update();
        require(input.frame().action(KeyboardAction::Forward).held
            && std::abs(input.frame().relativeMouseX - 72.0f) < 0.01f,
            "Simultaneous camera and movement touch failed");
        finger(SDL_EVENT_FINGER_CANCELED, 1, 0.2f, 0.4f);
        finger(SDL_EVENT_FINGER_UP, 2, 0.80f, 0.42f);
        update();
        require(!input.frame().action(KeyboardAction::Forward).held && input.frame().relativeMouseX == 0,
            "Canceled/released fingers left input held");
        finger(SDL_EVENT_FINGER_DOWN, 3, 0.2f, 0.6f);
        update();
        finger(SDL_EVENT_FINGER_MOTION, 3, 0.2f, 0.4f);
        update(true);
        require(!input.frame().action(KeyboardAction::Forward).held, "Blocked gameplay still moved");
        finger(SDL_EVENT_FINGER_CANCELED, 3, 0.2f, 0.4f);
        update();
        finger(SDL_EVENT_FINGER_DOWN, 4, 0.7f, 0.3f);
        finger(SDL_EVENT_FINGER_UP, 4, 0.7f, 0.3f);
        update(false, false);
        require(input.frame().leftMouseButton.pressed && std::abs(input.frame().pointerX - 1008.0f) < 0.01f,
            "Menu tap was not translated into a scaled pointer click");
        update(false, false);
        require(input.frame().leftMouseButton.released, "Menu tap did not release");
        finger(SDL_EVENT_FINGER_DOWN, 57, 0.70f, 0.30f);
        finger(SDL_EVENT_FINGER_MOTION, 57, 0.705f, 0.30f);
        finger(SDL_EVENT_FINGER_UP, 57, 0.705f, 0.30f);
        update(false, false);
        require(input.frame().leftMouseButton.pressed
            && std::abs(input.frame().pointerX - 0.70f * 1440.0f) < 0.01f,
            "Quick touch replay replaced the true press position with its release position");
        update(false, false);
        require(input.frame().leftMouseButton.released
            && std::abs(input.frame().pointerX - 0.705f * 1440.0f) < 0.01f,
            "Quick touch replay lost the true release position");
        finger(SDL_EVENT_FINGER_DOWN, 58, 0.70f, 0.30f);
        update(false, false);
        finger(SDL_EVENT_FINGER_MOTION, 58, 0.705f, 0.30f);
        finger(SDL_EVENT_FINGER_MOTION, 58, 0.70f, 0.30f);
        finger(SDL_EVENT_FINGER_UP, 58, 0.70f, 0.30f);
        update(false, false);
        require(input.frame().leftMouseButton.released && !input.frame().mobileTouchDragReleased
            && !input.frame().mobileTouchPathIncomplete
            && std::any_of(input.frame().mobileTouchPath.begin(), input.frame().mobileTouchPath.end(),
                [](const SDL_FPoint &point) { return std::abs(point.x - 0.705f * 1440.0f) < 0.01f; }),
            "Below-threshold leave-and-return motion disappeared from the release frame");
        update(false, false);
        require(input.frame().mobileTouchPath.empty() && !input.frame().mobileTouchPathIncomplete,
            "Delivered touch trajectory leaked into a later frame");
        finger(SDL_EVENT_FINGER_DOWN, 59, 0.70f, 0.30f);
        finger(SDL_EVENT_FINGER_MOTION, 59, 0.71f, 0.30f);
        finger(SDL_EVENT_FINGER_MOTION, 59, 0.70f, 0.30f);
        update(false, false);
        require(input.frame().leftMouseButton.pressed && !input.frame().mobileTouchDragActive
            && std::any_of(input.frame().mobileTouchPath.begin(), input.frame().mobileTouchPath.end(),
                [](const SDL_FPoint &point) { return std::abs(point.x - 0.71f * 1440.0f) < 0.01f; }),
            "Motion before the first held touch frame disappeared from its press trajectory");
        finger(SDL_EVENT_FINGER_UP, 59, 0.70f, 0.30f);
        update(false, false);
        finger(SDL_EVENT_FINGER_DOWN, 60, 0.70f, 0.30f);
        for (int sample = 0; sample < 257; ++sample)
        {
            finger(SDL_EVENT_FINGER_MOTION, 60, sample % 2 == 0 ? 0.701f : 0.70f, 0.30f);
        }
        finger(SDL_EVENT_FINGER_UP, 60, 0.70f, 0.30f);
        update(false, false);
        require(input.frame().mobileTouchPathIncomplete && input.frame().mobileTouchPath.size() == 256,
            "Touch trajectory overflow silently discarded cancellation evidence");
        update(false, false);
        finger(SDL_EVENT_FINGER_DOWN, 50, 0.70f, 0.30f);
        finger(SDL_EVENT_FINGER_UP, 50, 0.70f, 0.30f);
        finger(SDL_EVENT_FINGER_DOWN, 51, 0.75f, 0.32f);
        finger(SDL_EVENT_FINGER_UP, 51, 0.75f, 0.32f);
        for (int tapIndex = 0; tapIndex < 2; ++tapIndex)
        {
            const float expectedX = (tapIndex == 0 ? 0.70f : 0.75f) * 1440.0f;
            update(false, false);
            require(input.frame().leftMouseButton.pressed
                && std::abs(input.frame().pointerX - expectedX) < 0.01f,
                "Batched touch taps lost their press order or position");
            update(false, false);
            require(input.frame().leftMouseButton.released
                && std::abs(input.frame().pointerX - expectedX) < 0.01f,
                "Batched touch taps lost their release order or position");
        }
        finger(SDL_EVENT_FINGER_DOWN, 52, 0.65f, 0.30f);
        update(false, false);
        require(input.frame().leftMouseButton.pressed, "Cannot begin held HUD touch");
        finger(SDL_EVENT_FINGER_UP, 52, 0.65f, 0.30f);
        finger(SDL_EVENT_FINGER_DOWN, 53, 0.80f, 0.32f);
        update(false, false);
        require(input.frame().leftMouseButton.released
            && std::abs(input.frame().pointerX - 0.65f * 1440.0f) < 0.01f,
            "New HUD touch hid the preceding release");
        update(false, false);
        require(input.frame().leftMouseButton.pressed, "New HUD touch did not follow the release");
        finger(SDL_EVENT_FINGER_UP, 53, 0.80f, 0.32f);
        update(false, false);
        require(input.frame().leftMouseButton.released, "Second held HUD touch did not release");
        finger(SDL_EVENT_FINGER_DOWN, 54, 0.7f, 0.3f);
        finger(SDL_EVENT_FINGER_CANCELED, 54, 0.7f, 0.3f);
        update(false, false);
        require(!input.frame().leftMouseButton.held, "Canceled between-frame tap created a click");
        finger(SDL_EVENT_FINGER_DOWN, 55, 0.7f, 0.3f);
        finger(SDL_EVENT_FINGER_UP, 55, 0.7f, 0.3f);
        SDL_Event touchFocusLost = {};
        touchFocusLost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        input.handleSdlEvent(touchFocusLost);
        update(false, false);
        update(false, false);
        require(!input.frame().leftMouseButton.held, "Focus loss replayed a queued touch tap");
        finger(SDL_EVENT_FINGER_DOWN, 56, 0.7f, 0.3f);
        finger(SDL_EVENT_FINGER_UP, 56, 0.7f, 0.3f);
        update(true, false);
        update(false, false);
        require(!input.frame().leftMouseButton.held, "Blocked touch replayed when input resumed");
        SDL_Event key = {};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.scancode = SDL_SCANCODE_W;
        input.handleSdlEvent(key);
        key.key.repeat = true;
        input.handleSdlEvent(key);
        update();
        require(input.frame().scancodePressCount(SDL_SCANCODE_W) == 1, "Keyboard press/repeat handling failed");
        require(input.frame().action(KeyboardAction::Forward).pressed
            && !input.frame().action(KeyboardAction::Forward).held, "Short key tap was lost or left held");
        update();
        require(!input.frame().action(KeyboardAction::Forward).pressed, "Short key tap repeated next frame");

        GameSession session;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        runtime.bindSettings(&settings);
        {
            GameSession skillsSession;
            const std::filesystem::path layoutPath = std::filesystem::path(__FILE__).parent_path().parent_path()
                / "assets_dev/engine/ui/gameplay/character.yml";
            std::ifstream layoutFile(layoutPath);
            require(layoutFile.good(), "Cannot open authored character layout");
            const std::string layoutText{std::istreambuf_iterator<char>(layoutFile), std::istreambuf_iterator<char>()};
            require(skillsSession.gameplayUiRuntime().layoutManager().loadLayoutText("character-skills", layoutText),
                "Cannot load authored character layout");
            GameplayScreenRuntime &skillsRuntime = skillsSession.gameplayScreenRuntime();
            for (const std::array<int, 2> size : {std::array{640, 480}, std::array{1210, 834}, std::array{2420, 1668}})
            {
                skillsRuntime.characterScreen().skillScrollOffset = 0;
                const CharacterSkillLayout top = layoutCharacterSkillRows(
                    skillsRuntime, size[0], size[1], 14, {10, 9, 5, 16});
                require(top.rowHeight == 18 && top.maximumScroll > 0,
                    "Crowded skills must scroll without shrinking their readable rows");
                const std::optional<GameplayResolvedHudLayoutElement> region =
                    skillsRuntime.resolveHudLayoutElement("CharacterSkillsMiscListRegion", size[0], size[1], 0, 0);
                require(region.has_value(), "Cannot resolve final skill section");
                GameplayResolvedHudLayoutElement last = *region;
                last.y += 15 * top.rowHeight * last.scale;
                last.height = top.rowHeight * last.scale;
                const GameplayResolvedHudLayoutElement originalLast = last;
                require(!top.placeVisible(last), "Offscreen skill row must not render or receive upgrade clicks");
                skillsRuntime.characterScreen().skillScrollOffset = 10000;
                const CharacterSkillLayout bottom = layoutCharacterSkillRows(
                    skillsRuntime, size[0], size[1], 14, {10, 9, 5, 16});
                require(bottom.scrollOffset == bottom.maximumScroll && bottom.rowHeight == top.rowHeight,
                    "Overscroll must clamp without changing font size");
                last = originalLast;
                require(bottom.placeVisible(last)
                    && last.y + last.height <= bottom.viewport.y + bottom.viewport.height + 0.01f,
                    "Scrolling must expose the last skill above the tabs");
                require(!bottom.contains(last.x, bottom.viewport.y + bottom.viewport.height + 1),
                    "Skill scroll input must exclude the tabs");
                const CharacterSkillLayout shortList = layoutCharacterSkillRows(
                    skillsRuntime, size[0], size[1], 14, {2, 1, 1, 1});
                require(shortList.rowHeight == top.rowHeight && shortList.maximumScroll == 0
                    && shortList.scrollOffset == 0, "Ordinary characters must keep the same readable font and reset scroll");
            }
            PartySeed skillSeed = Party::createDefaultSeed();
            skillSeed.members.resize(1);
            skillSeed.members.front().skillPoints = 100;
            for (auto &[name, skill] : skillSeed.members.front().skills)
            {
                skill.level = 1;
            }
            Party skillParty;
            skillParty.seed(skillSeed);
            OpenYAMM::Tests::PartySpellTestWorldRuntime skillWorld;
            skillWorld.bindParty(&skillParty);
            skillsSession.bindActiveWorldRuntime(&skillWorld);
            GameDataRepository skillData;
            skillsSession.bindDataRepository(&skillData);
            GameSettings touchSettings = settings;
            touchSettings.gameplayUiLayout = GameplayUiLayout::Standard;
            skillsRuntime.bindSettings(&touchSettings);
            skillsRuntime.characterScreen().open = true;
            skillsRuntime.characterScreen().page = GameplayUiController::CharacterPage::Skills;
            GameplayInputFrame scroll = {};
            scroll.screenWidth = 1210;
            scroll.screenHeight = 834;
            CharacterSkillLayout page = layoutCharacterSkillRows(skillsRuntime, 1210, 834, 11, {10, 9, 5, 16});
            scroll.pointerX = page.viewport.x + page.viewport.width / 2;
            scroll.pointerY = page.viewport.y + page.viewport.height / 2;
            scroll.mouseWheelDelta = -100;
            GameplayPartyOverlayInputController::handleCharacterOverlayInput(skillsRuntime, scroll);
            require(skillsRuntime.characterScreen().skillScrollOffset == page.maximumScroll,
                "Real character controller did not clamp wheel scroll to the last skill");
            page = layoutCharacterSkillRows(skillsRuntime, 1210, 834, 11, {10, 9, 5, 16});
            const std::optional<GameplayResolvedHudLayoutElement> misc =
                skillsRuntime.resolveHudLayoutElement("CharacterSkillsMiscListRegion", 1210, 834, 0, 0);
            require(misc.has_value(), "Cannot resolve scrolled skill target");
            GameplayResolvedHudLayoutElement last = *misc;
            last.y += 15 * page.rowHeight * last.scale;
            last.height = page.rowHeight * last.scale;
            require(page.placeVisible(last), "Last skill must be clickable after scrolling");
            GameplayInputFrame click = {};
            click.screenWidth = 1210;
            click.screenHeight = 834;
            click.pointerX = last.x + 10;
            click.pointerY = last.y + last.height / 2;
            click.leftMouseButton = {true, true, false};
            GameplayPartyOverlayInputController::handleCharacterOverlayInput(skillsRuntime, click);
            click.leftMouseButton = {false, false, true};
            GameplayPartyOverlayInputController::handleCharacterOverlayInput(skillsRuntime, click);
            require(skillParty.activeMember()->findSkill("VampireAbility")->level == 2
                && skillParty.activeMember()->findSkill("Regeneration")->level == 1
                && skillParty.activeMember()->skillPoints == 98,
                "Scrolled skill click must upgrade exactly the visible canonical skill");
            GameplayInputFrame drag = {};
            drag.screenWidth = 1210;
            drag.screenHeight = 834;
            drag.leftMouseButton.held = true;
            drag.mobileTouchDragStarted = true;
            drag.mobileTouchDragActive = true;
            drag.mobileTouchDragStartX = scroll.pointerX;
            drag.mobileTouchDragStartY = page.viewport.y + 10;
            drag.pointerX = drag.mobileTouchDragStartX;
            drag.pointerY = drag.mobileTouchDragStartY + (page.maximumScroll + 20) * page.viewport.scale;
            GameplayPartyOverlayInputController::handleCharacterOverlayInput(skillsRuntime, drag);
            drag.mobileTouchDragStarted = false;
            drag.mobileTouchDragActive = false;
            drag.mobileTouchDragReleased = true;
            drag.leftMouseButton.held = false;
            GameplayPartyOverlayInputController::handleCharacterOverlayInput(skillsRuntime, drag);
            require(skillsRuntime.characterScreen().skillScrollOffset == 0
                && !skillsRuntime.characterScreen().skillDragActive && skillParty.activeMember()->skillPoints == 98,
                "Touch scrolling must return to the top without spending skill points");

            require(!skillsRuntime.standardTouchButtonRect(1210, 834),
                "Desktop input unexpectedly enables the Standard touch button");
            skillsRuntime.enableStandardTouchControls();
            for (const GameplayUiController::CharacterPage characterPage : {
                GameplayUiController::CharacterPage::Stats,
                GameplayUiController::CharacterPage::Skills,
                GameplayUiController::CharacterPage::Inventory})
            {
                skillsRuntime.characterScreen().page = characterPage;
                const std::optional<GameplayResolvedHudLayoutElement> button =
                    skillsRuntime.standardTouchButtonRect(1210, 834);
                const std::optional<GameplayResolvedHudLayoutElement> inspect =
                    skillsRuntime.mobileInspectButtonRect(1210, 834);
                require(button && inspect && button->x == inspect->x && button->y == inspect->y
                    && button->width == inspect->width && button->height == inspect->height,
                    "Character touch inspection must use its visible button rectangle on every supported page");
                for (const auto &[id, element] : skillsSession.gameplayUiRuntime().layoutManager().elements())
                {
                    if (!element.visible || !element.interactive)
                    {
                        continue;
                    }
                    const std::optional<GameplayResolvedHudLayoutElement> other =
                        skillsRuntime.resolveHudLayoutElement(id, 1210, 834, element.width, element.height);
                    require(!other || button->x + button->width <= other->x || other->x + other->width <= button->x
                        || button->y + button->height <= other->y || other->y + other->height <= button->y,
                        "Standard inspect button overlaps authored control " + id);
                }
            }
            skillsRuntime.characterScreen().page = GameplayUiController::CharacterPage::Skills;
            skillsRuntime.characterScreen().skillScrollOffset = page.maximumScroll;
            const auto inspectTarget = [&](float targetX, float targetY, bool releaseModifierFirst)
            {
                const std::optional<GameplayResolvedHudLayoutElement> button =
                    skillsRuntime.mobileInspectButtonRect(1210, 834);
                require(button.has_value(), "Cannot resolve the visible character inspect modifier");
                const SDL_FRect inputRect = {button->x, button->y, button->width, button->height};
                const float modifierX = (button->x + button->width / 2) / 1210;
                const float modifierY = (button->y + button->height / 2) / 834;
                const auto inspectFrame = [&]()
                {
                    input.updateFromEngineInput(1210, 834, 0, touchSettings, false, false,
                        false, false, true, inputRect);
                    require(!input.frame().leftMouseButton.held && !input.frame().leftMouseButton.pressed
                        && !input.frame().leftMouseButton.released && input.frame().mobileTouchPath.empty(),
                        "Inspection replayed a target finger as a left click");
                    GameplayPartyOverlayInputController::handleCharacterOverlayInput(skillsRuntime, input.frame());
                };
                finger(SDL_EVENT_FINGER_DOWN, 701, modifierX, modifierY);
                inspectFrame();
                finger(SDL_EVENT_FINGER_DOWN, 702, targetX / 1210, targetY / 834);
                inspectFrame();
                require(input.frame().rightMouseButton.held
                    && std::abs(input.frame().pointerX - targetX) < 0.01f
                    && std::abs(input.frame().pointerY - targetY) < 0.01f,
                    "The visible inspect modifier did not inspect the second finger target");
                if (releaseModifierFirst)
                {
                    finger(SDL_EVENT_FINGER_UP, 701, modifierX, modifierY);
                    inspectFrame();
                    finger(SDL_EVENT_FINGER_UP, 702, targetX / 1210, targetY / 834);
                }
                else
                {
                    finger(SDL_EVENT_FINGER_UP, 702, targetX / 1210, targetY / 834);
                    inspectFrame();
                    require(input.frame().rightMouseButton.held,
                        "Releasing the target finger discarded inspection while the modifier remained held");
                    finger(SDL_EVENT_FINGER_UP, 701, modifierX, modifierY);
                }
                inspectFrame();
                inspectFrame();
                require(!input.frame().rightMouseButton.held, "Inspect release left the right button held");
            };
            inspectTarget(last.x + 10, last.y + last.height / 2, false);
            require(skillParty.activeMember()->skillPoints == 98
                && skillParty.activeMember()->findSkill("VampireAbility")->level == 2,
                "Touch skill inspection or target release spent skill points");
            skillsRuntime.characterScreen().page = GameplayUiController::CharacterPage::Inventory;
            const auto inventoryGrid = skillsRuntime.resolveHudLayoutElement("CharacterInventoryGrid", 1210, 834, 0, 0);
            require(inventoryGrid && !skillParty.activeMember()->inventory.empty(),
                "Default party must provide a real occupied inventory target");
            const InventoryItem inventoryItem = skillParty.activeMember()->inventory.front();
            const size_t inventoryCount = skillParty.activeMember()->inventory.size();
            const float inventoryX = inventoryGrid->x
                + (inventoryItem.gridX + 0.5f) * inventoryGrid->width / Character::InventoryWidth;
            const float inventoryY = inventoryGrid->y
                + (inventoryItem.gridY + 0.5f) * inventoryGrid->height / Character::InventoryHeight;
            inspectTarget(inventoryX, inventoryY, true);
            require(!skillsRuntime.heldInventoryItem().active
                && skillParty.activeMember()->inventory.size() == inventoryCount,
                "Touch inventory inspection or release picked up an item");

            const std::filesystem::path gameplayPath = layoutPath.parent_path() / "gameplay.yml";
            std::ifstream gameplayFile(gameplayPath);
            require(gameplayFile.good(), "Cannot open authored Standard HUD layout");
            const std::string gameplayText{std::istreambuf_iterator<char>(gameplayFile),
                std::istreambuf_iterator<char>()};
            require(skillsSession.gameplayUiRuntime().layoutManager().loadLayoutText("standard-touch", gameplayText),
                "Cannot load authored Standard HUD layout");
            skillsRuntime.characterScreen().open = false;
            const auto spellButton = skillsRuntime.standardTouchButtonRect(1210, 834);
            require(spellButton && !skillsRuntime.mobileInspectButtonRect(1210, 834),
                "Gameplay spell entry must not become an inspect modifier");
            for (const char *pId : {"OutdoorStandardButtonOptions", "OutdoorStandardButtonQuickReference",
                "OutdoorStandardButtonRest", "OutdoorStandardButtonBooks"})
            {
                const auto other = skillsRuntime.resolveHudLayoutElement(pId, 1210, 834, 0, 0);
                require(other && other->x + other->width <= spellButton->x,
                    std::string("Standard spell entry overlaps authored top bar control ") + pId);
            }
            GameplayHudButtonInputConfig spellClick = {};
            spellClick.screenWidth = 1210;
            spellClick.screenHeight = 834;
            spellClick.pointerX = spellButton->x + spellButton->width / 2;
            spellClick.pointerY = spellButton->y + spellButton->height / 2;
            spellClick.allowInput = true;
            require(GameplayHudInputController::resolveGameplayHudButtonTarget(skillsRuntime, spellClick).type
                == GameplayHudPointerTargetType::CastButton,
                "Visible Standard spell button has no matching HUD target");
            const auto spellFrame = [&]()
            {
                input.updateFromEngineInput(1210, 834, 0, touchSettings, false, true, false, false, false);
                spellClick.pointerX = input.frame().pointerX;
                spellClick.pointerY = input.frame().pointerY;
                spellClick.leftButtonPressed = input.frame().leftMouseButton.held;
                GameplayHudInputController::handleGameplayHudButtonInput(skillsRuntime, spellClick);
            };
            const float spellX = spellClick.pointerX / 1210;
            const float spellY = spellClick.pointerY / 834;
            finger(SDL_EVENT_FINGER_DOWN, 703, spellX, spellY);
            spellFrame();
            require(!skillsRuntime.spellbook().active, "Standard spell entry activated before release");
            finger(SDL_EVENT_FINGER_UP, 703, spellX, spellY);
            spellFrame();
            require(skillsRuntime.spellbook().active && !skillsRuntime.standardTouchButtonRect(1210, 834),
                "Standard spell entry did not open the real spellbook or remained visible above it");
        }
        require(!runtime.mobileInspectControlAvailable(),
            "An absent mobile inspect button still captures the classic character tabs");
        {
            GameSession mobileSession;
            require(mobileSession.gameplayUiRuntime().layoutManager().loadLayoutText("mobile-inspect", R"(
screen: OutdoorHud
elements:
  - id: OutdoorMobileInspectButton
    anchor: top_left
    width: 80
    height: 80
    interactive: true
    children:
      - id: OutdoorMobileInspectButtonIcon
        anchor: top_left
        width: 42
        height: 64
        asset:
          default: magnif-b
)"), "Cannot prepare mobile inspect layout");
            require(mobileSession.gameplayScreenRuntime().mobileInspectControlAvailable(),
                "The authored mobile inspect control was disabled");
        }
        const auto menuFrame = [&]()
        {
            GameplayStandardUiHotkeyConfig config = {};
            config.pInputFrame = &input.frame();
            config.pKeyboardState = input.frame().keyboardState();
            GameplayInputController::handleStandardUiHotkeys(runtime, config);
            GameplayOverlayInputController::handleMenuOverlayInput(runtime, input.frame());
        };
        const auto escapeTap = [&]()
        {
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = SDL_SCANCODE_ESCAPE;
            input.handleSdlEvent(event);
            event.type = SDL_EVENT_KEY_UP;
            input.handleSdlEvent(event);
        };
        escapeTap();
        update();
        menuFrame();
        require(runtime.menuScreenState().active, "Escape tap did not open the game menu");
        update();
        menuFrame();
        require(runtime.menuScreenState().active, "Opening Escape also closed the menu");
        escapeTap();
        update();
        menuFrame();
        require(!runtime.menuScreenState().active, "Second Escape tap did not close the menu");
        update();
        menuFrame();
        require(!runtime.menuScreenState().active, "Closing Escape reopened the menu");
        escapeTap();
        update(true);
        menuFrame();
        require(!runtime.menuScreenState().active, "Blocked Escape opened the game menu");
        const auto mouse = [&](SDL_EventType type, uint8_t button, float x, float y,
            SDL_MouseID source = 0)
        {
            SDL_Event event = {};
            event.type = type;
            event.button.which = source;
            event.button.button = button;
            event.button.x = x;
            event.button.y = y;
            input.handleSdlEvent(event);
        };
        const auto click = [&](uint8_t button, float downX, float downY, float upX, float upY)
        {
            mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, button, downX, downY);
            mouse(SDL_EVENT_MOUSE_BUTTON_UP, button, upX, upY);
        };
        click(SDL_BUTTON_LEFT, 100, 120, 200, 240);
        update();
        require(input.frame().leftMouseButton.pressed && input.frame().pointerX == 100
            && input.frame().pointerY == 120, "Mouse down edge or position was lost");
        update();
        require(input.frame().leftMouseButton.released && input.frame().pointerX == 200
            && input.frame().pointerY == 240, "Mouse release edge or position was lost");
        update();
        require(!input.frame().leftMouseButton.held && !input.frame().leftMouseButton.pressed
            && !input.frame().leftMouseButton.released, "Completed click repeated or stayed held");

        click(SDL_BUTTON_LEFT, 10, 20, 10, 20);
        click(SDL_BUTTON_LEFT, 30, 40, 30, 40);
        for (int edge = 0; edge < 4; ++edge)
        {
            update();
            require(input.frame().leftMouseButton.held == (edge % 2 == 0)
                && input.frame().pointerX == (edge < 2 ? 10 : 30), "Rapid clicks lost edge order/positions");
        }
        settings.keyboard.setBinding(KeyboardAction::Attack, mouseButtonInputBinding(SDL_BUTTON_RIGHT));
        click(SDL_BUTTON_RIGHT, 10, 20, 10, 20);
        update();
        require(input.frame().rightMouseButton.pressed && input.frame().action(KeyboardAction::Attack).pressed,
            "Quick right click did not reach its configured game action");
        update();
        require(input.frame().rightMouseButton.released && input.frame().action(KeyboardAction::Attack).released,
            "Right click action did not release");
        click(SDL_BUTTON_MIDDLE, 10, 20, 10, 20);
        update();
        require(input.frame().middleMouseButton.pressed, "Quick middle click was lost");
        update();
        require(input.frame().middleMouseButton.released, "Middle click did not release");
        click(SDL_BUTTON_LEFT, 10, 20, 10, 20);
        update(true);
        update();
        require(!input.frame().leftMouseButton.held, "Blocked click replayed after input was unblocked");
        click(SDL_BUTTON_LEFT, 10, 20, 10, 20);
        input.suppressMouseButtonsUntilReleased();
        update();
        require(!input.frame().leftMouseButton.held, "Screen transition replayed a pending click");
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, 10, 20, SDL_TOUCH_MOUSEID);
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, 10, 20, SDL_TOUCH_MOUSEID);
        update();
        require(!input.frame().leftMouseButton.held, "SDL touch mouse emulation duplicated a touch");

        GameSession hudSession;
        GameplayScreenRuntime &hudRuntime = hudSession.gameplayScreenRuntime();
        hudRuntime.bindSettings(&settings);
        require(hudSession.gameplayUiRuntime().layoutManager().loadLayoutText("touch-hud", R"(
screen: OutdoorHud
elements:
  - id: OutdoorStandardButtonOptions
    anchor: top_left
    offset_x: 520
    offset_y: 5
    width: 60
    height: 24
    interactive: true
  - id: OutdoorButtonOptions
    anchor: top_left
    offset_x: 520
    offset_y: 5
    width: 60
    height: 24
    interactive: true
)"), "Cannot prepare gameplay menu target");
        const auto hudMenu = hudRuntime.resolveHudLayoutElement("OutdoorStandardButtonOptions", 1440, 1000, 60, 24);
        require(hudMenu.has_value(), "Cannot resolve gameplay menu target");
        const float hudX = hudMenu->x + hudMenu->width / 2;
        const float hudY = hudMenu->y + hudMenu->height / 2;
        const auto hudFrame = [&](bool blocked = false)
        {
            const GameplayInputFrame &frame = input.frame();
            GameplayStandardUiInputConfig config = {};
            config.pInputFrame = &frame;
            config.width = frame.screenWidth;
            config.height = frame.screenHeight;
            config.pointerX = frame.pointerX;
            config.pointerY = frame.pointerY;
            config.leftButtonPressed = frame.leftMouseButton.held;
            config.allowGameplayPointerInput = false;
            config.blockHudButtonInput = blocked;
            GameplayScreenController::handleStandardUiInput(hudRuntime, config);
        };
        click(SDL_BUTTON_LEFT, hudX, hudY, hudX, hudY);
        update();
        hudFrame();
        update();
        hudFrame();
        require(!hudRuntime.menuScreenState().active, "Mouse-look mode unexpectedly enabled mouse HUD clicks");
        finger(SDL_EVENT_FINGER_DOWN, 41, hudX / 1440, hudY / 1000);
        finger(SDL_EVENT_FINGER_UP, 41, hudX / 1440, hudY / 1000);
        update();
        hudFrame();
        require(!hudRuntime.menuScreenState().active, "Touch menu activated before release");
        update();
        hudFrame();
        require(hudRuntime.menuScreenState().active, "Mouse-look mode blocked a touch on the real HUD menu");
        hudRuntime.closeMenuOverlay();
        update();
        require(!input.frame().pointerFromTouch, "Touch pointer ownership leaked into the next mouse frame");
        finger(SDL_EVENT_FINGER_DOWN, 42, hudX / 1440, hudY / 1000);
        finger(SDL_EVENT_FINGER_UP, 42, hudX / 1440, hudY / 1000);
        update();
        hudFrame(true);
        update();
        hudFrame(true);
        require(!hudRuntime.menuScreenState().active, "Touch bypassed the modal HUD input block");

        require(session.gameplayUiRuntime().layoutManager().loadLayoutText("input-menu", R"(
screen: Menu
elements:
  - id: MenuButtonReturn
    anchor: top_left
    offset_x: 100
    offset_y: 100
    width: 100
    height: 40
    interactive: true
)"), "Cannot prepare menu button layout");
        const auto returnButton = runtime.resolveHudLayoutElement("MenuButtonReturn", 1440, 1000, 100, 40);
        require(returnButton.has_value(), "Cannot resolve menu button target");
        const float buttonX = returnButton->x + returnButton->width / 2;
        const float buttonY = returnButton->y + returnButton->height / 2;
        escapeTap();
        update();
        menuFrame();
        require(runtime.menuScreenState().active, "Cannot open menu for mouse checks");
        click(SDL_BUTTON_LEFT, buttonX, buttonY, buttonX, buttonY);
        update();
        menuFrame();
        require(runtime.menuScreenState().active, "Menu activated on mouse down instead of release");
        update();
        menuFrame();
        require(!runtime.menuScreenState().active, "Quick click failed to activate the real menu controller");
        escapeTap();
        update();
        menuFrame();
        click(SDL_BUTTON_LEFT, buttonX, buttonY, -10, -10);
        update();
        menuFrame();
        update();
        menuFrame();
        require(runtime.menuScreenState().active, "Drag release outside the button activated it");
        click(SDL_BUTTON_LEFT, buttonX, buttonY, buttonX, buttonY);
        update();
        menuFrame();
        SDL_Event focusLost = {};
        focusLost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        input.handleSdlEvent(focusLost);
        update();
        menuFrame();
        update();
        menuFrame();
        require(runtime.menuScreenState().active, "Focus loss activated/replayed a pending click");
        click(SDL_BUTTON_LEFT, buttonX, buttonY, buttonX, buttonY);
        update();
        menuFrame();
        update();
        menuFrame();
        require(!runtime.menuScreenState().active, "Mouse input did not recover after cancellation");

        const auto keyTap = [&](SDL_Scancode scancode)
        {
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.scancode = scancode;
            input.handleSdlEvent(event);
            event.type = SDL_EVENT_KEY_UP;
            input.handleSdlEvent(event);
        };
        PartySeed seed = {};
        Character member = {};
        member.name = "Input check";
        member.health = 100;
        member.maxHealth = 100;
        seed.members.push_back(member);
        Party party;
        party.seed(seed);
        {
            PartySeed capacitySeed = seed;
            capacitySeed.gold = std::numeric_limits<int>::max() - 7;
            Party capacityParty;
            capacityParty.seed(capacitySeed);
            capacityParty.depositGoldToBank(capacitySeed.gold);
            capacityParty.addGold(100);
            require(capacityParty.depositGoldToBank(100) == 7 && capacityParty.gold() == 93
                && capacityParty.bankGold() == std::numeric_limits<int>::max(),
                "Bank deposit exceeded destination capacity or lost untransferred gold");
            require(capacityParty.depositGoldToBank(1) == 0 && capacityParty.gold() == 93,
                "A full bank consumed carried gold");
            capacityParty.addGold(std::numeric_limits<int>::max() - 98);
            require(capacityParty.withdrawBankGold(100) == 5
                && capacityParty.gold() == std::numeric_limits<int>::max()
                && capacityParty.bankGold() == std::numeric_limits<int>::max() - 5,
                "Bank withdrawal exceeded carried capacity or lost untransferred gold");
            require(capacityParty.withdrawBankGold(1) == 0,
                "Withdrawal into a full carried balance transferred gold");
        }
        OpenYAMM::Tests::PartySpellTestWorldRuntime world;
        world.bindParty(&party);
        session.bindActiveWorldRuntime(&world);

        settings.keyboard.setBinding(KeyboardAction::Attack, keyboardInputBinding(SDL_SCANCODE_F));
        struct AttackWorld : OpenYAMM::Tests::PartySpellTestWorldRuntime
        {
            bool worldInspectModeActive() const override { return true; }
        } attackWorld;
        attackWorld.bindParty(&party);
        GameSession attackSession;
        attackSession.bindActiveWorldRuntime(&attackWorld);
        GameplayScreenRuntime &attackRuntime = attackSession.gameplayScreenRuntime();
        attackRuntime.bindSettings(&settings);
        const auto worldFrame = [&](const GameplayInputFrame &frame, bool blocked = false)
        {
            return GameplayInteractionController::updateWorldInteractionFrame(
                attackSession.gameplayScreenState(), attackSession.overlayInteractionState(), attackRuntime,
                attackSession.gameplaySpellService(), frame, {}, blocked);
        };
        keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Attack));
        update();
        require(input.frame().action(KeyboardAction::Attack).pressed
            && !input.frame().action(KeyboardAction::Attack).held,
            "Completed Attack tap was not preserved as an edge");
        require(worldFrame(input.frame()).attackAttempted,
            "World interaction discarded a between-frame Attack tap");
        update();
        require(!worldFrame(input.frame()).attackAttempted,
            "Completed Attack tap repeated on an empty frame");
        keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Attack));
        update();
        require(!worldFrame(input.frame(), true).attackAttempted,
            "Blocked world accepted an Attack tap");
        update();
        require(!worldFrame(input.frame()).attackAttempted,
            "Blocked Attack tap replayed after the modal closed");
        GameplayInputFrame heldAttack;
        heldAttack.actions[keyboardActionIndex(KeyboardAction::Attack)].held = true;
        require(worldFrame(heldAttack).attackAttempted, "Held Attack no longer starts an attack");
        require(!worldFrame(heldAttack).attackAttempted, "Held Attack ignored the repeat debounce");
        worldFrame({});
        heldAttack.rightMouseButton.held = true;
        require(!worldFrame(heldAttack).attackAttempted, "Inspect pause accepted an attack");
        worldFrame({});
        std::cout << "World attack input passed: transient tap, release, modal cancellation, hold and inspect.\n";

        {
            SDL_Event yellFocusGained = {};
            yellFocusGained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
            input.handleSdlEvent(yellFocusGained);
            update();
            struct YellWorld : OpenYAMM::Tests::PartySpellTestWorldRuntime
            {
                int fleeCalls = 0;
                void fleeNearbyActorsFromParty() override { ++fleeCalls; }
            } yellWorld;
            PartySeed yellSeed;
            yellSeed.members.push_back(OpenYAMM::Tests::makeSpellRegressionPartyMember(
                "Yell input", "Sorcerer", "PC07-01", 7));
            Party yellParty;
            yellParty.seed(yellSeed);
            yellParty.activeMember()->recoverySecondsRemaining = 4.0f;
            yellWorld.bindParty(&yellParty);
            GameSession yellSession;
            yellSession.bindActiveWorldRuntime(&yellWorld);
            GameplayScreenRuntime &yellRuntime = yellSession.gameplayScreenRuntime();
            yellRuntime.bindSettings(&settings);
            const InputBinding originalYellBinding = settings.keyboard.binding(KeyboardAction::Yell);
            settings.keyboard.setBinding(KeyboardAction::Yell, keyboardInputBinding(SDL_SCANCODE_V));
            const auto makeInvisible = [&]()
            {
                yellParty.applyPartyBuff(PartyBuffId::Invisibility, 60.0f, 1, 19, 1, SkillMastery::Master, 0);
            };
            const auto isInvisible = [&]()
            {
                const PartyBuffState *pBuff = yellParty.partyBuff(PartyBuffId::Invisibility);
                return pBuff != nullptr && pBuff->active();
            };
            const auto yellFrame = [&](const GameplayInputFrame &frame)
            {
                GameplaySharedInputFrameConfig config;
                config.pInputFrame = &frame;
                config.pKeyboardState = frame.keyboardState();
                config.screenWidth = frame.screenWidth;
                config.screenHeight = frame.screenHeight;
                config.processStandardUiInput = false;
                config.processQuickCast = false;
                return GameplayInputController::updateSharedGameplayInputFrame(
                    yellSession.gameplayScreenState(), yellRuntime, yellSession.gameplaySpellService(), config);
            };
            const auto tapYell = [&]()
            {
                keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Yell));
                update();
                require(input.frame().action(KeyboardAction::Yell).pressed
                    && !input.frame().action(KeyboardAction::Yell).held,
                    "Configured Yell tap was not preserved as one input edge");
                yellFrame(input.frame());
            };
            // Establish the new session's focus generation before its first action.
            yellFrame(input.frame());
            makeInvisible();
            const int initialMana = yellParty.activeMember()->spellPoints;
            const float initialRecovery = yellParty.activeMember()->recoverySecondsRemaining;
            require(initialMana > 0 && initialRecovery > 0, "Yell fixture needs mana and nonzero recovery");
            tapYell();
            require(!isInvisible() && yellWorld.fleeCalls == 1
                && yellParty.activeMember()->spellPoints == initialMana
                && yellParty.activeMember()->recoverySecondsRemaining == initialRecovery,
                "Yell did not clear invisibility/flee once without spending mana or recovery");

            makeInvisible();
            // Like heldAttack above, this is an explicit held-frame fixture;
            // queued SDL key events do not change the native keyboard state.
            GameplayInputFrame heldYell = {};
            heldYell.actions[keyboardActionIndex(KeyboardAction::Yell)].held = true;
            heldYell.keyboardHeld[SDL_SCANCODE_V] = true;
            yellFrame(heldYell);
            require(isInvisible() && yellWorld.fleeCalls == 1, "Held Yell repeated without a new press");
            update();
            yellFrame(input.frame());

            yellRuntime.openMenuOverlay();
            tapYell();
            require(isInvisible() && yellWorld.fleeCalls == 1, "Modal UI accepted a Yell action");
            yellRuntime.closeMenuOverlay();
            yellParty.activeMember()->conditions.set(static_cast<size_t>(CharacterCondition::Asleep));
            tapYell();
            require(isInvisible() && yellWorld.fleeCalls == 1, "Incapacitated active member could yell");
            yellParty.activeMember()->conditions.reset(static_cast<size_t>(CharacterCondition::Asleep));

            TurnBasedCombatRuntime &yellCombat = yellRuntime.turnBasedCombatRuntime();
            require(yellCombat.begin(yellParty, nullptr), "Cannot begin turn-based Yell check");
            const TurnBasedCombatStage initialStage = yellCombat.stage();
            const int initialMovementPoints = yellCombat.movementActionPoints();
            const float turnRecovery = yellParty.activeMember()->recoverySecondsRemaining;
            const int turnMana = yellParty.activeMember()->spellPoints;
            tapYell();
            require(!isInvisible() && yellWorld.fleeCalls == 1
                && yellCombat.stage() == initialStage && yellCombat.movementActionPoints() == initialMovementPoints
                && yellParty.activeMember()->spellPoints == turnMana
                && yellParty.activeMember()->recoverySecondsRemaining == turnRecovery,
                "Turn-based Yell must only break invisibility without fleeing or spending the turn");
            yellCombat.end(&yellParty);

            // A held-frame fixture exercises the actual monotonic repeat timer.
            // SDL event injection above supplies taps, not native held-key polling.
            GameplayInputFrame repeatingYell = input.frame();
            repeatingYell.actions[keyboardActionIndex(KeyboardAction::Yell)] = {};
            repeatingYell.keyboardHeld[SDL_SCANCODE_V] = false;
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            const int beforeHoldCalls = yellWorld.fleeCalls;
            repeatingYell.actions[keyboardActionIndex(KeyboardAction::Yell)].pressed = true;
            repeatingYell.actions[keyboardActionIndex(KeyboardAction::Yell)].held = true;
            repeatingYell.keyboardHeld[SDL_SCANCODE_V] = true;
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            require(yellWorld.fleeCalls == beforeHoldCalls + 1, "Held Yell lost its initial press");
            repeatingYell.actions[keyboardActionIndex(KeyboardAction::Yell)].pressed = false;
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            require(yellWorld.fleeCalls == beforeHoldCalls + 1, "Held Yell repeated before its first delay");
            SDL_Delay(550);
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            require(yellWorld.fleeCalls == beforeHoldCalls + 2, "Held Yell missed its first repeat");
            yellFrame(repeatingYell);
            require(yellWorld.fleeCalls == beforeHoldCalls + 2, "Yell repeated twice for one input frame");
            SDL_Delay(90);
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            require(yellWorld.fleeCalls == beforeHoldCalls + 3, "Held Yell missed its subsequent repeat");
            input.handleSdlEvent(focusLost);
            input.handleSdlEvent(yellFocusGained);
            update();
            repeatingYell.keyboardFocusGeneration = input.frame().keyboardFocusGeneration;
            repeatingYell.keyboardFocusActive = input.frame().keyboardFocusActive;
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            SDL_Delay(90);
            ++repeatingYell.frameId;
            yellFrame(repeatingYell);
            require(yellWorld.fleeCalls == beforeHoldCalls + 3,
                "Focus loss and gain between frames resumed the old held Yell");
            settings.keyboard.setBinding(KeyboardAction::Yell, originalYellBinding);
            yellSession.bindActiveWorldRuntime(nullptr);
            std::cout << "Yell input passed: configured tap, held timer fixture, focus interruption, modal, incapacity and turn-based rules.\n";
        }

        const auto restFrame = [&]()
        {
            GameplayStandardUiHotkeyConfig config = {};
            config.pInputFrame = &input.frame();
            config.pKeyboardState = input.frame().keyboardState();
            config.canOpenRest = true;
            GameplayInputController::handleStandardUiHotkeys(runtime, config);
        };
        keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Rest));
        update();
        restFrame();
        require(runtime.restScreenState().active, "Short Rest tap did not open the real rest controller");
        runtime.closeRestOverlay();
        update();
        restFrame();
        require(!runtime.restScreenState().active, "Completed Rest tap reopened the overlay");

        const auto sharedHotkeyFrame = [&](bool allowed = true)
        {
            GameplaySharedGameplayHotkeyConfig config = {};
            config.pInputFrame = &input.frame();
            config.pKeyboardState = input.frame().keyboardState();
            config.canToggleAlwaysRun = allowed;
            GameplayInputController::handleSharedGameplayHotkeys(runtime, config);
        };
        const bool previousAlwaysRun = runtime.mutableSettings().alwaysRun;
        keyTap(settings.keyboard.keyboardBinding(KeyboardAction::AlwaysRun));
        update();
        sharedHotkeyFrame();
        require(runtime.mutableSettings().alwaysRun != previousAlwaysRun, "Short Always Run tap was lost");
        update();
        sharedHotkeyFrame();
        require(runtime.mutableSettings().alwaysRun != previousAlwaysRun, "Always Run tap toggled twice");

        TurnBasedCombatRuntime &combat = runtime.turnBasedCombatRuntime();
        for (SDL_Scancode scancode : {settings.keyboard.keyboardBinding(KeyboardAction::Combat), SDL_SCANCODE_KP_ENTER})
        {
            keyTap(scancode);
            update();
            sharedHotkeyFrame(false);
            require(!combat.active(), "Blocked Combat tap enabled turn-based combat");
            update();
            sharedHotkeyFrame();
            keyTap(scancode);
            update();
            sharedHotkeyFrame();
            require(combat.active(), "Short Combat tap did not enter turn-based combat");
            combat.update(&party, &world, 0.6f);
            require(combat.stage() == TurnBasedCombatStage::Attack, "Combat did not reach player attack phase");
            update();
            sharedHotkeyFrame();
            require(combat.active(), "Completed Combat tap toggled off again");
            keyTap(scancode);
            update();
            sharedHotkeyFrame();
            require(!combat.active(), "Second Combat tap did not leave turn-based combat");
            update();
            sharedHotkeyFrame();
        }

        require(combat.begin(party, nullptr), "Cannot begin movement input check");
        keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Forward));
        update();
        GameplayInputFrame movementFrame = input.frame();
        require(!combat.noteMovementInput(movementFrame)
            && !movementFrame.isActionActive(KeyboardAction::Forward),
            "Short movement tap escaped the turn-based wait phase");
        combat.update(&party, nullptr, 0.6f);
        require(combat.applyPlayerAction(party, 0, 5.0f), "Cannot complete player action for movement check");
        combat.update(&party, nullptr, 0.016f);
        require(combat.stage() == TurnBasedCombatStage::Movement, "Cannot enter movement phase");
        for (int step = 0; step < 5; ++step)
        {
            keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Forward));
            update();
            movementFrame = input.frame();
            require(!movementFrame.action(KeyboardAction::Forward).held
                && combat.noteMovementInput(movementFrame) && movementFrame.turnBasedMovementStep,
                "Short movement tap did not produce an allowed movement step");
            require(combat.movementActionPoints() == 130 - 26 * (step + 1),
                "Short movement tap spent the wrong number of action points");
            update();
            movementFrame = input.frame();
            require(!combat.noteMovementInput(movementFrame), "Completed movement tap spent a second step");
        }
        keyTap(settings.keyboard.keyboardBinding(KeyboardAction::Forward));
        update();
        movementFrame = input.frame();
        require(!combat.noteMovementInput(movementFrame)
            && !movementFrame.isActionActive(KeyboardAction::Forward),
            "Short movement tap bypassed exhausted action points");
        combat.end(&party);
        session.bindActiveWorldRuntime(nullptr);

        keyTap(SDL_SCANCODE_W);
        SDL_Event pendingText = {};
        pendingText.type = SDL_EVENT_TEXT_INPUT;
        pendingText.text.text = "canceled";
        input.handleSdlEvent(pendingText);
        input.handleSdlEvent(focusLost);
        update();
        require(!input.frame().isActionActive(KeyboardAction::Forward) && input.frame().textInput.empty(),
            "Focus loss replayed pending movement or text");
        SDL_Event focusGained = {};
        focusGained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        input.handleSdlEvent(focusGained);
        keyTap(SDL_SCANCODE_W);
        update();
        require(input.frame().isActionActive(KeyboardAction::Forward), "Keyboard did not recover after focus gain");
        update();
        GameplayUiController::SaveGameScreenState &saveScreen = runtime.saveGameScreenState();
        saveScreen.active = true;
        saveScreen.slots.resize(1);
        saveScreen.slots[0].fileLabel = "空槽";
        // Enable request preparation only. The scene adapter below captures requests without writing files.
        session.setSaveGameToPathCallback(
            [](const std::filesystem::path &, const std::string &, const std::vector<uint8_t> &, std::string &)
            {
                return false;
            });
        const std::optional<GameplayScreenRuntime::PreparedSaveGameRequest> emptySlotRequest =
            runtime.prepareSelectedSaveGameRequest();
        require(emptySlotRequest.has_value() && emptySlotRequest->saveName == "Save Game",
            "Localized empty slot label became the saved name");
        saveScreen.slots[0].fileLabel = "Empty";
        saveScreen.slots[0].populated = true;
        const std::optional<GameplayScreenRuntime::PreparedSaveGameRequest> populatedSlotRequest =
            runtime.prepareSelectedSaveGameRequest();
        require(populatedSlotRequest.has_value() && populatedSlotRequest->saveName == "Empty",
            "A populated save named Empty lost its name");
        saveScreen.slots[0].fileLabel = "空槽";
        saveScreen.slots[0].populated = false;
        require(session.gameplayUiRuntime().layoutManager().loadLayoutText("save-touch", R"(
screen: SaveGame
elements:
  - id: SaveGameSlotRow0
    anchor: top_left
    offset_x: 100
    offset_y: 100
    width: 200
    height: 40
    interactive: true
)"), "Cannot prepare save slot input layout");
        const auto saveSlot = runtime.resolveHudLayoutElement("SaveGameSlotRow0", 1440, 1000, 200, 40);
        require(saveSlot.has_value(), "Cannot resolve the save slot");
        const float saveX = (saveSlot->x + saveSlot->width / 2.0f) / 1440.0f;
        const float saveY = (saveSlot->y + saveSlot->height / 2.0f) / 1000.0f;
        for (SDL_FingerID id : {60, 61})
        {
            finger(SDL_EVENT_FINGER_DOWN, id, saveX, saveY);
            finger(SDL_EVENT_FINGER_UP, id, saveX, saveY);
        }
        for (int frameIndex = 0; frameIndex < 4; ++frameIndex)
        {
            update(false, false);
            GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
            require(saveScreen.editActive == (frameIndex == 3),
                "Real save controller did not enter editing on the second completed touch tap");
        }
        require(saveScreen.editBuffer.empty(), "Localized empty slot label was copied into the edit buffer");
        const auto textInput = [&](const char *pText)
        {
            SDL_Event event = {};
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.text = pText;
            input.handleSdlEvent(event);
            update();
            GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        };
        for (const bool eraseFirst : {true, false})
        {
            saveScreen.editBuffer = "12";
            if (eraseFirst)
            {
                keyTap(SDL_SCANCODE_BACKSPACE);
            }
            SDL_Event replacement = {};
            replacement.type = SDL_EVENT_TEXT_INPUT;
            replacement.text.text = "3";
            input.handleSdlEvent(replacement);
            if (!eraseFirst)
            {
                keyTap(SDL_SCANCODE_BACKSPACE);
            }
            update();
            GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
            require(saveScreen.editBuffer == (eraseFirst ? "13" : "12"),
                "Same-frame replacement lost the order of text and Backspace");
        }
        saveScreen.editBuffer.clear();
        textInput("新索匹格镇 MM678");
        require(saveScreen.editBuffer == "新索匹格镇 MM678", "Save controller rejected Chinese input");
        textInput("𠀀");
        require(saveScreen.editBuffer == "新索匹格镇 MM678𠀀",
            "Supplementary input was not appended: " + saveScreen.editBuffer);
        key = {};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.scancode = SDL_SCANCODE_BACKSPACE;
        input.handleSdlEvent(key);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(saveScreen.editBuffer == "新索匹格镇 MM678",
            "Backspace split a supplementary UTF-8 character: actual=" + saveScreen.editBuffer
                + " presses=" + std::to_string(input.frame().scancodePressCount(SDL_SCANCODE_BACKSPACE)));
        saveScreen.editBuffer.clear();
        for (int i = 0; i < 29; ++i)
        {
            saveScreen.editBuffer += "中";
        }
        textInput("文𠀀A");
        require(utf8CharacterCount(saveScreen.editBuffer) == 30 && saveScreen.editBuffer.ends_with("文"),
            "Save name limit counted bytes or split UTF-8");
        saveScreen.editBuffer.clear();
        textInput("A\nB\tC");
        require(saveScreen.editBuffer == "ABC", "Save label accepted control characters");
        textInput("\xe4\xb8");
        require(saveScreen.editBuffer == "ABC", "Save controller accepted a truncated UTF-8 sequence");
        for (const char *pInvalid : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80"})
        {
            textInput(pInvalid);
            require(saveScreen.editBuffer == "ABC", "Save controller accepted noncanonical Unicode");
        }
        struct SaveInputAdapter final : IGameplayOverlaySceneAdapter
        {
            GameplayScreenRuntime &m_runtime;
            std::string m_committedName;
            bool m_acceptSave = false;

            explicit SaveInputAdapter(GameplayScreenRuntime &screenRuntime) : m_runtime(screenRuntime)
            {
            }
            float gameplayCameraYawRadians() const override
            {
                return 0.0f;
            }
            void executeActiveDialogAction() override
            {
            }
            bool tryCastSpellRequest(const PartySpellCastRequest &, const std::string &) override
            {
                return false;
            }
            bool trySaveToSelectedGameSlot() override
            {
                return m_runtime.trySaveToSelectedGameSlot(
                    [this](const GameplayScreenRuntime::PreparedSaveGameRequest &request)
                    {
                        m_committedName = request.saveName;
                        return m_acceptSave;
                    });
            }
        } saveAdapter(runtime);
        runtime.bindSceneAdapter(&saveAdapter);
        const auto textEditing = [&](const char *text)
        {
            SDL_Event editing = {};
            editing.type = SDL_EVENT_TEXT_EDITING;
            editing.edit.text = text;
            input.handleSdlEvent(editing);
        };
        saveScreen.editBuffer = "前";
        textEditing("zhong");
        keyTap(SDL_SCANCODE_BACKSPACE);
        keyTap(SDL_SCANCODE_RETURN);
        keyTap(SDL_SCANCODE_ESCAPE);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(saveScreen.active && saveScreen.editActive && saveScreen.compositionActive
            && saveScreen.compositionText == "zhong" && saveScreen.editBuffer == "前"
            && saveAdapter.m_committedName.empty(),
            "IME keys changed or submitted a name before its composition was committed");
        // Explicit source records exercise the consumer; no native clipboard or IME is invoked.
        GameplayInputFrame pasteFrame = input.frame();
        pasteFrame.textInput = "剪贴板";
        pasteFrame.textEdits = std::vector<GameplayTextEditOperation>{
            {GameplayTextEditOperation::Kind::PasteText, "剪贴板"},
            {GameplayTextEditOperation::Kind::PasteText, ""},
            {GameplayTextEditOperation::Kind::Confirm, ""},
            {GameplayTextEditOperation::Kind::Cancel, ""}};
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, pasteFrame);
        require(saveScreen.active && saveScreen.compositionActive && saveScreen.compositionText == "zhong"
            && saveScreen.editBuffer == "前" && saveAdapter.m_committedName.empty(),
            "Manual or empty paste was mistaken for an IME commit in the save editor");
        textEditing("");
        SDL_Event compositionCommit = {};
        compositionCommit.type = SDL_EVENT_TEXT_INPUT;
        compositionCommit.text.text = "中";
        input.handleSdlEvent(compositionCommit);
        keyTap(SDL_SCANCODE_RETURN);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(!saveScreen.compositionActive && saveScreen.compositionText.empty()
            && saveScreen.editBuffer == "前中" && saveAdapter.m_committedName == "前中",
            "A real Return following the IME commit was delayed or saved preedit text");
        textEditing("wei");
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        input.handleSdlEvent(focusLost);
        input.handleSdlEvent(focusGained);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(!saveScreen.compositionActive && saveScreen.editBuffer == "前中",
            "Focus loss and gain in one frame left stale save composition");
        textEditing("bao");
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        update(true);
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(!saveScreen.compositionActive && saveScreen.editBuffer == "前中",
            "Blocked input retained save preedit or committed it");
        textEditing("liu");
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        keyTap(SDL_SCANCODE_BACKSPACE);
        // Console hides the visible preedit immediately; the queued clear owns its event boundary.
        saveScreen.compositionText.clear();
        input.clearTextComposition();
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(!saveScreen.compositionActive && saveScreen.editBuffer == "前中",
            "Queued composition clearing reordered an earlier IME Backspace");
        saveAdapter.m_committedName.clear();
        saveScreen.editBuffer = "连";
        SDL_Event committedText = {};
        committedText.type = SDL_EVENT_TEXT_INPUT;
        committedText.text.text = "续";
        input.handleSdlEvent(committedText);
        keyTap(SDL_SCANCODE_RETURN);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(saveAdapter.m_committedName == "连续", "Return saved before the same-frame IME text was applied");
        require(saveScreen.editActive && saveScreen.editBuffer == "连续" && saveScreen.slots[0].fileLabel == "空槽",
            "Rejected save discarded the editable name or changed the slot label");
        textInput("修正");
        saveAdapter.m_acceptSave = true;
        keyTap(SDL_SCANCODE_RETURN);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(saveAdapter.m_committedName == "连续修正" && saveScreen.editActive
            && saveScreen.editBuffer == "连续修正", "Corrected Chinese name was lost while the save was pending");
        runtime.clearSceneAdapter(&saveAdapter);
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        escapeTap();
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        require(!saveScreen.active, "Short Escape did not close the save overlay");
        saveScreen.active = true;
        saveScreen.editActive = false;
        saveScreen.scrollOffset = 0;
        saveScreen.selectedIndex = 0;
        saveScreen.slots.resize(11);
        for (size_t index = 0; index < saveScreen.slots.size(); ++index)
        {
            saveScreen.slots[index].path = std::filesystem::path("saves")
                / ("save" + std::to_string(index + 1) + ".oysav");
            saveScreen.slots[index].populated = index < 10;
            saveScreen.slots[index].fileLabel = index < 10 ? "Existing" : "空槽";
        }
        require(session.gameplayUiRuntime().layoutManager().loadLayoutText("save-touch-paging", R"(
screen: SaveGame
elements:
  - id: SaveGameSlotRow9
    anchor: top_left
    offset_x: 100
    offset_y: 500
    width: 200
    height: 40
    interactive: true
  - id: SaveGameScrollDownButton
    anchor: top_left
    offset_x: 320
    offset_y: 500
    width: 40
    height: 40
    interactive: true
  - id: SaveGameScrollUpButton
    anchor: top_left
    offset_x: 320
    offset_y: 100
    width: 40
    height: 40
    interactive: true
)"), "Cannot prepare save pagination input layout");
        const auto tapSaveElement = [&](const char *pId, SDL_FingerID id)
        {
            const auto element = runtime.resolveHudLayoutElement(pId, 1440, 1000, 40, 40);
            require(element.has_value(), "Cannot resolve save pagination target");
            const float x = (element->x + element->width / 2.0f) / 1440.0f;
            const float y = (element->y + element->height / 2.0f) / 1000.0f;
            finger(SDL_EVENT_FINGER_DOWN, id, x, y);
            finger(SDL_EVENT_FINGER_UP, id, x, y);
            for (int frame = 0; frame < 2; ++frame)
            {
                update(false, false);
                GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
            }
        };
        tapSaveElement("SaveGameScrollDownButton", 70);
        require(saveScreen.scrollOffset == 1, "Full save page cannot reach the next empty slot");
        tapSaveElement("SaveGameScrollDownButton", 71);
        require(saveScreen.scrollOffset == 1, "Save scrolling exceeded the last visible page");
        tapSaveElement("SaveGameSlotRow9", 72);
        tapSaveElement("SaveGameSlotRow9", 73);
        require(saveScreen.selectedIndex == 10 && saveScreen.editSlotIndex == 10
            && saveScreen.editActive && saveScreen.editBuffer.empty(),
            "Scrolled save row edited an occupied slot or copied the empty label");
        textInput("中文旅行");
        const auto scrolledSave = runtime.prepareSelectedSaveGameRequest();
        require(scrolledSave.has_value() && scrolledSave->path.filename() == "save11.oysav"
            && scrolledSave->saveName == "中文旅行" && saveScreen.slots[9].fileLabel == "Existing",
            "Scrolled save request lost its absolute slot or changed the previous save");
        tapSaveElement("SaveGameScrollUpButton", 74);
        require(saveScreen.scrollOffset == 0, "Save scroll up did not return to the first page");
        tapSaveElement("SaveGameScrollUpButton", 75);
        require(saveScreen.scrollOffset == 0, "Save scrolling underflowed the first page");
        escapeTap();
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        // No world is bound here: exercise the real bank input consumer without a transaction.
        auto &bankInput = runtime.houseBankState();
        bankInput.houseId = 281;
        bankInput.inputMode = GameplayUiController::HouseBankInputMode::Deposit;
        bankInput.inputText = "12";
        textEditing("3");
        keyTap(SDL_SCANCODE_BACKSPACE);
        keyTap(SDL_SCANCODE_RETURN);
        keyTap(SDL_SCANCODE_ESCAPE);
        update();
        GameplayOverlayInputController::handleDialogueOverlayInput(runtime, input.frame(), false);
        require(bankInput.inputActive() && bankInput.compositionActive && bankInput.inputText == "12",
            "Bank IME keys erased or submitted committed digits during composition");
        pasteFrame = input.frame();
        pasteFrame.textInput = "99";
        pasteFrame.textEdits = std::vector<GameplayTextEditOperation>{
            {GameplayTextEditOperation::Kind::PasteText, "99"},
            {GameplayTextEditOperation::Kind::PasteText, ""},
            {GameplayTextEditOperation::Kind::Confirm, ""},
            {GameplayTextEditOperation::Kind::Cancel, ""}};
        GameplayOverlayInputController::handleDialogueOverlayInput(runtime, pasteFrame, false);
        require(bankInput.inputActive() && bankInput.compositionActive
            && bankInput.compositionText == "3" && bankInput.inputText == "12",
            "Manual or empty bank paste cleared preedit and enabled a premature transaction");
        textEditing("");
        compositionCommit.text.text = "3";
        input.handleSdlEvent(compositionCommit);
        keyTap(SDL_SCANCODE_RETURN);
        compositionCommit.text.text = "4";
        input.handleSdlEvent(compositionCommit);
        update();
        GameplayOverlayInputController::handleDialogueOverlayInput(runtime, input.frame(), false);
        require(!bankInput.compositionActive && bankInput.inputText == "123",
            "Bank confirmation included a text event that arrived after Return");
        struct BankEscapeWorld final : OpenYAMM::Tests::PartySpellTestWorldRuntime
        {
            int escapeHookCalls = 0;
            bool rejectEscape = true;
            uint32_t lastScreen = 0;
            bool executeEventHooks(EventRuntimeHookKind kind) override
            {
                auto &hook = eventRuntimeState()->activeHookContext;
                if (kind == EventRuntimeHookKind::GameplayAction && hook
                    && hook->gameplayActionId == SourceActionEscape)
                {
                    ++escapeHookCalls;
                    lastScreen = hook->menuId;
                    hook->blocked = rejectEscape;
                    return true;
                }
                return false;
            }
        } bankEscapeWorld;
        session.bindActiveWorldRuntime(&bankEscapeWorld);
        keyTap(SDL_SCANCODE_ESCAPE);
        update();
        GameplayStandardUiHotkeyConfig bankHotkeys = {};
        bankHotkeys.pKeyboardState = input.frame().keyboardState();
        bankHotkeys.pInputFrame = &input.frame();
        GameplayInputController::handleStandardUiHotkeys(runtime, bankHotkeys);
        GameplayOverlayInputController::handleDialogueOverlayInput(runtime, input.frame(), false);
        require(bankInput.inputActive() && bankInput.inputText == "123" && bankEscapeWorld.escapeHookCalls == 1,
            "Bank ordered Escape bypassed or repeated the source action rejection");
        bankInput = {};
        saveScreen.active = true;
        saveScreen.editActive = true;
        saveScreen.editBuffer = "保留草稿";
        textEditing("cao");
        update();
        GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        const auto dispatchSaveEscape = [&]()
        {
            GameplayStandardUiHotkeyConfig config = {};
            config.pKeyboardState = input.frame().keyboardState();
            config.pInputFrame = &input.frame();
            GameplayInputController::handleStandardUiHotkeys(runtime, config);
            GameplayOverlayInputController::handleSaveGameOverlayInput(runtime, input.frame());
        };
        keyTap(SDL_SCANCODE_ESCAPE);
        update();
        dispatchSaveEscape();
        require(saveScreen.active && saveScreen.compositionActive && bankEscapeWorld.escapeHookCalls == 1,
            "Save composition-owned Escape reached the source close hook");
        textEditing("");
        keyTap(SDL_SCANCODE_ESCAPE);
        update();
        dispatchSaveEscape();
        require(saveScreen.active && saveScreen.editBuffer == "保留草稿"
            && bankEscapeWorld.escapeHookCalls == 2 && bankEscapeWorld.lastScreen == 11,
            "Save Escape bypassed or repeated a source rejection, or discarded the draft");
        bankEscapeWorld.rejectEscape = false;
        keyTap(SDL_SCANCODE_ESCAPE);
        update();
        dispatchSaveEscape();
        require(!saveScreen.active && bankEscapeWorld.escapeHookCalls == 3,
            "A fresh permitted Escape could not close the save overlay immediately");
        session.bindActiveWorldRuntime(nullptr);
        std::cout << "Touch, short movement/rest/combat input, focus, real menu clicks "
            "and Chinese save/bank editing/composition/pagination passed\n";
        SDL_Quit();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
