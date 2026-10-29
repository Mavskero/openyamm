#pragma once

#include "game/gameplay/GameplaySpellActionController.h"
#include "game/gameplay/GameplayScreenState.h"
#include "game/party/PartySpellSystem.h"

#include <cstdint>

namespace OpenYAMM::Game
{
class GameplaySpellService;
class GameplayScreenRuntime;
struct GameplayInputFrame;

// Original discrete UI action IDs used by authenticated event Action hooks.
// Movement and simulation input do not pass through this UI action channel.
enum SourceGameplayActionId : uint32_t
{
    SourceActionWorldLeftClick = 10,
    SourceActionChestClickItem = 12,
    SourceActionOpenInventory = 20,
    SourceActionAttack = 23,
    SourceActionCastQuickSpell = 25,
    SourceActionClickBooks = 71,
    SourceActionWait5Minutes = 95,
    SourceActionWait1Hour = 96,
    SourceActionRest8Hours = 97,
    SourceActionRestWindow = 104,
    SourceActionSpellbookWindow = 105,
    SourceActionQuickReference = 106,
    SourceActionGameMenu = 107,
    SourceActionWaitUntilDawn = 109,
    SourceActionSelectCharacter = 110,
    SourceActionEscape = 113,
    SourceActionClickSkills = 114,
    SourceActionClickStats = 115,
    SourceActionClickInventory = 116,
    SourceActionClickAwards = 117,
    SourceActionInventoryLeftClick = 120,
    SourceActionSkillUp = 121,
    SourceActionReturnToGame = 123,
    SourceActionStartNewGame = 124,
    SourceActionOpenLoadGame = 125,
    SourceActionOpenSaveGame = 126,
    SourceActionOpenOptions = 127,
    SourceActionQuit = 132,
    SourceActionClickPaperdoll = 133,
    SourceActionStartNpcDialogue = 161,
    SourceActionExitRest = 167,
    SourceActionExitCharacterWindow = 168,
    SourceActionAwardsUp = 169,
    SourceActionAwardsDown = 170,
    SourceActionCycleCharacters = 176,
    SourceActionScrollNpcPanel = 188,
    SourceActionAwardScrollBar = 192,
    SourceActionOpenQuestBook = 200,
    SourceActionOpenAutonotes = 201,
    SourceActionOpenMapBook = 202,
    SourceActionOpenHistoryBook = 224,
    SourceActionToggleAlwaysRun = 225,
    SourceActionZoomOut = 367,
    SourceActionZoomIn = 368,
    // MMExtension const.Actions.CustomDialogButton; also used by this project's custom UI controls.
    SourceActionCustomDialogButton = 1000,
};

struct GameplayStandardUiHotkeyConfig
{
    const bool *pKeyboardState = nullptr;
    const GameplayInputFrame *pInputFrame = nullptr;
    bool canOpenRest = false;
    bool blockMenuToggle = false;
    bool blockSpellbookToggle = false;
    bool blockInventoryToggle = false;
    bool blockPartyCycle = false;
    bool requireGameplayReadyForPartySelection = false;
};

struct GameplayMouseLookPolicyConfig
{
    bool mouseLookAllowed = false;
    bool rightMousePressed = false;
};

struct GameplayMouseLookPolicyResult
{
    bool cursorModeActive = false;
    bool mouseLookActive = false;
    bool allowGameplayPointerInput = false;
};

struct GameplaySharedGameplayHotkeyConfig
{
    const bool *pKeyboardState = nullptr;
    const GameplayInputFrame *pInputFrame = nullptr;
    bool canToggleAlwaysRun = false;
    bool canToggleAdventurersInn = false;
};

struct GameplaySharedInputFrameConfig
{
    const bool *pKeyboardState = nullptr;
    const GameplayInputFrame *pInputFrame = nullptr;
    float mouseWheelDelta = 0.0f;
    int screenWidth = 0;
    int screenHeight = 0;
    float pointerX = 0.0f;
    float pointerY = 0.0f;
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    bool hasReadyMember = false;
    bool canBeginQuickCast = false;
    bool isUtilitySpellModalActive = false;
    bool isReadableScrollOverlayActive = false;
    bool processStandardUiInput = true;
    bool processSharedGameplayHotkeys = true;
    bool processQuickCast = true;
};

struct GameplaySharedInputFrameResult
{
    bool journalInputConsumed = false;
    bool worldInputBlocked = false;
    GameplayMouseLookPolicyResult mouseLookPolicy = {};
};

class GameplayInputController
{
public:
    static GameplayMouseLookPolicyResult updateGameplayMouseLookPolicy(
        GameplayScreenState::GameplayMouseLookState &state,
        const GameplayMouseLookPolicyConfig &config);
    static void handleStandardUiHotkeys(
        GameplayScreenRuntime &context,
        const GameplayStandardUiHotkeyConfig &config);
    static void handleSharedGameplayHotkeys(
        GameplayScreenRuntime &context,
        const GameplaySharedGameplayHotkeyConfig &config);
    static GameplaySharedInputFrameResult updateSharedGameplayInputFrame(
        GameplayScreenState &screenState,
        GameplayScreenRuntime &context,
        GameplaySpellService &spellService,
        const GameplaySharedInputFrameConfig &config);
};
} // namespace OpenYAMM::Game
