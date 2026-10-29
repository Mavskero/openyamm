#include "game/gameplay/GameplayHudInputController.h"

#include "game/gameplay/GameplayItemService.h"
#include "game/gameplay/GameplayInputController.h"
#include "game/gameplay/NpcFollowerRuntime.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/party/SpellIds.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint64_t PartyPortraitDoubleClickWindowMs = 500;
constexpr size_t VisibleFollowerPanelSlots = 3;

struct HudPointerState
{
    float x = 0.0f;
    float y = 0.0f;
    bool leftButtonPressed = false;
};

template <typename Target, typename ResolveTargetFn, typename ActivateTargetFn>
void handlePointerClickRelease(
    const HudPointerState &pointerState,
    bool &clickLatch,
    Target &pressedTarget,
    const Target &noneTarget,
    ResolveTargetFn resolveTargetFn,
    ActivateTargetFn activateTargetFn)
{
    if (pointerState.leftButtonPressed)
    {
        if (!clickLatch)
        {
            pressedTarget = resolveTargetFn(pointerState.x, pointerState.y);
            clickLatch = true;
        }
    }
    else if (clickLatch)
    {
        const Target currentTarget = resolveTargetFn(pointerState.x, pointerState.y);

        if (currentTarget == pressedTarget)
        {
            activateTargetFn(currentTarget);
        }

        clickLatch = false;
        pressedTarget = noneTarget;
    }
    else
    {
        pressedTarget = noneTarget;
    }
}

const char *activeGameplayButtonLayoutId(
    const GameplayScreenRuntime &context,
    const char *pWideId,
    const char *pStandardId)
{
#if defined(__ANDROID__)
    (void)context;
    (void)pStandardId;
    return pWideId;
#else
    return context.settingsSnapshot().gameplayUiLayout == GameplayUiLayout::Standard ? pStandardId : pWideId;
#endif
}

void openDimensionDoorOverlay(GameplayScreenRuntime &context)
{
    const Party *pParty = context.partyReadOnly();
    const size_t casterMemberIndex = pParty != nullptr ? pParty->activeMemberIndex() : 0;
    context.openDimensionDoorOverlay(casterMemberIndex, spellIdValue(SpellId::TownPortal));
}

size_t hiredFollowerCount(const GameplayScreenRuntime &context)
{
    const IGameplayWorldRuntime *pWorldRuntime = context.worldRuntime();
    const EventRuntimeState *pEventRuntimeState =
        pWorldRuntime != nullptr ? pWorldRuntime->eventRuntimeState() : nullptr;
    const NpcDialogTable *pNpcDialogTable = context.npcDialogTable();
    const MergedNpcProfessionTable *pNpcProfessionTable = context.mergedNpcProfessionTable();
    const Party *pParty = context.partyReadOnly();

    if (pEventRuntimeState == nullptr || pNpcDialogTable == nullptr || pNpcProfessionTable == nullptr)
    {
        return 0;
    }

    return buildHiredNpcFollowerViews(
        *pEventRuntimeState,
        pParty,
        *pNpcDialogTable,
        *pNpcProfessionTable).size();
}

bool pointerInsideHudElement(
    const GameplayScreenRuntime &context,
    const GameplayHudButtonInputConfig &config,
    const char *pLayoutId,
    float pointerX,
    float pointerY)
{
    if (pLayoutId == nullptr || *pLayoutId == '\0')
    {
        return false;
    }

    const GameplayScreenRuntime::HudLayoutElement *pLayout = context.findHudLayoutElement(pLayoutId);

    if (pLayout == nullptr)
    {
        return false;
    }

    const std::optional<GameplayScreenRuntime::ResolvedHudLayoutElement> resolved =
        context.resolveHudLayoutElement(
            pLayoutId,
            config.screenWidth,
            config.screenHeight,
            pLayout->width,
            pLayout->height);

    return resolved && context.isPointerInsideResolvedElement(*resolved, pointerX, pointerY);
}
}

void GameplayHudInputController::activatePartyPortrait(
    GameplayScreenRuntime &context,
    size_t memberIndex,
    const GameplayPartyPortraitInputConfig &config)
{
    if (!context.allowSourceGameplayAction(SourceActionSelectCharacter))
    {
        return;
    }

    if (config.onPortraitActivated && config.onPortraitActivated(memberIndex))
    {
        return;
    }

    if (context.heldInventoryItem().active)
    {
        const bool switchCharacterOnFailedPlacement =
            context.characterScreenReadOnly().open
            && context.characterScreenReadOnly().page == GameplayUiController::CharacterPage::Inventory;

        if (context.tryAutoPlaceHeldInventoryItemOnPartyMember(memberIndex, !switchCharacterOnFailedPlacement))
        {
            context.interactionState().lastPartyPortraitClickedIndex = std::nullopt;
        }
        else if (switchCharacterOnFailedPlacement)
        {
            GameplayUiController::CharacterScreenState &characterScreen = context.characterScreen();
            characterScreen.source = GameplayUiController::CharacterScreenSource::Party;
            characterScreen.sourceIndex = memberIndex;
            context.trySelectPartyMember(memberIndex, config.requireGameplayReady);
        }

        return;
    }

    const Party *pParty = context.partyReadOnly();
    if (pParty == nullptr || pParty->member(memberIndex) == nullptr)
    {
        return;
    }

    const uint64_t nowTicks = SDL_GetTicks();
    const bool isGameplayInventoryDoubleClick =
        config.requireGameplayReady
        && context.interactionState().lastPartyPortraitClickedIndex.has_value()
        && *context.interactionState().lastPartyPortraitClickedIndex == memberIndex
        && nowTicks >= context.interactionState().lastPartyPortraitClickTicks
        && nowTicks - context.interactionState().lastPartyPortraitClickTicks <= PartyPortraitDoubleClickWindowMs;
    const bool isChestInventoryDoubleClick =
        config.hasActiveLootView
        && context.interactionState().lastPartyPortraitClickedIndex.has_value()
        && *context.interactionState().lastPartyPortraitClickedIndex == memberIndex
        && nowTicks >= context.interactionState().lastPartyPortraitClickTicks
        && nowTicks - context.interactionState().lastPartyPortraitClickTicks <= PartyPortraitDoubleClickWindowMs;

    if (context.characterScreenReadOnly().open && !context.isAdventurersInnCharacterSourceActive())
    {
        GameplayUiController::CharacterScreenState &characterScreen = context.characterScreen();
        characterScreen.source = GameplayUiController::CharacterScreenSource::Party;
        characterScreen.sourceIndex = memberIndex;
        context.trySelectPartyMember(memberIndex, config.requireGameplayReady);
        context.interactionState().lastPartyPortraitClickTicks = nowTicks;
        context.interactionState().lastPartyPortraitClickedIndex = memberIndex;
        return;
    }

    const bool selected = context.trySelectPartyMember(memberIndex, config.requireGameplayReady);
    // A member who cannot act still needs a remembered first click for inventory inspection.
    context.interactionState().lastPartyPortraitClickTicks = nowTicks;
    context.interactionState().lastPartyPortraitClickedIndex = memberIndex;

    if (!selected && !isGameplayInventoryDoubleClick)
    {
        return;
    }

    if (isGameplayInventoryDoubleClick)
    {
        GameplayUiController::CharacterScreenState &characterScreen = context.characterScreen();
        characterScreen = {};
        characterScreen.open = true;
        characterScreen.page = GameplayUiController::CharacterPage::Inventory;
        characterScreen.source = GameplayUiController::CharacterScreenSource::Party;
        characterScreen.sourceIndex = memberIndex;
    }
    else if (isChestInventoryDoubleClick)
    {
        context.openChestTransferInventoryOverlay();
    }

}

void GameplayHudInputController::handlePartyPortraitInput(
    GameplayScreenRuntime &context,
    const GameplayPartyPortraitInputConfig &config)
{
    if (!config.allowInput || config.screenWidth <= 0 || config.screenHeight <= 0)
    {
        context.interactionState().partyPortraitClickLatch = false;
        context.interactionState().partyPortraitRightClickItemUseLatch = false;
        context.interactionState().partyPortraitPressedIndex = std::nullopt;
        return;
    }

    if (!config.rightButtonPressed)
    {
        context.interactionState().partyPortraitRightClickItemUseLatch = false;
    }
    else if (!context.interactionState().partyPortraitRightClickItemUseLatch
             && context.heldInventoryItem().active)
    {
        const std::optional<size_t> memberIndex =
            context.resolvePartyPortraitIndexAtPoint(
                config.screenWidth,
                config.screenHeight,
                config.pointerX,
                config.pointerY);

        // A touch inspect modifier can precede the second finger selecting a portrait.
        // Only a real portrait attempt consumes the gesture.
        if (memberIndex)
        {
            context.interactionState().partyPortraitRightClickItemUseLatch = true;
        }

        const bool keepCharacterScreenOpen =
            context.characterScreenReadOnly().open
            && context.characterScreenReadOnly().page == GameplayUiController::CharacterPage::Inventory;

        if (memberIndex
            && context.itemService().tryUseHeldItemOnPartyMember(
                context,
                *memberIndex,
                keepCharacterScreenOpen))
        {
            context.interactionState().partyPortraitClickLatch = false;
            context.interactionState().partyPortraitPressedIndex = std::nullopt;
            context.interactionState().lastPartyPortraitClickedIndex = std::nullopt;
            return;
        }
    }

    const HudPointerState pointerState = {
        config.pointerX,
        config.pointerY,
        config.leftButtonPressed
    };

    handlePointerClickRelease(
        pointerState,
        context.interactionState().partyPortraitClickLatch,
        context.interactionState().partyPortraitPressedIndex,
        std::optional<size_t>{},
        [&context, &config](float x, float y) -> std::optional<size_t>
        {
            return context.resolvePartyPortraitIndexAtPoint(config.screenWidth, config.screenHeight, x, y);
        },
        [&context, &config](const std::optional<size_t> &memberIndex)
        {
            if (!memberIndex)
            {
                return;
            }

            GameplayHudInputController::activatePartyPortrait(context, *memberIndex, config);
        });
}

GameplayHudPointerTarget GameplayHudInputController::resolveGameplayHudButtonTarget(
    const GameplayScreenRuntime &context,
    const GameplayHudButtonInputConfig &config)
{
    if (!config.allowInput || config.screenWidth <= 0 || config.screenHeight <= 0)
    {
        return {};
    }

    const float pointerX = config.pointerX;
    const float pointerY = config.pointerY;
    const GameplayContextActionState &contextActionState = context.contextActionStateReadOnly();
    const bool hasDropHeldItemAction =
        contextActionState.visible
        && contextActionState.primaryIndex < contextActionState.actions.size()
        && contextActionState.actions[contextActionState.primaryIndex].kind
            == GameplayContextActionKind::DropHeldItem;
    const bool contextActionButtonEnabled =
        context.settingsSnapshot().contextActionPopup || hasDropHeldItemAction;

    if (context.interactionState().gameplayHudPressedTarget.type
            == GameplayHudPointerTargetType::ContextActionButton
        && contextActionButtonEnabled
        && pointerInsideHudElement(context, config, "OutdoorMobileContextActionButton", pointerX, pointerY))
    {
        return context.interactionState().gameplayHudPressedTarget;
    }

    if (pointerInsideHudElement(context, config, "OutdoorFollowerToggle", pointerX, pointerY))
    {
        return {GameplayHudPointerTargetType::FollowerPanelToggleButton};
    }

    if (context.interactionState().followerPanelOpen)
    {
        static constexpr std::array<const char *, VisibleFollowerPanelSlots> FollowerSlots = {{
            "OutdoorFollowerSlot_1",
            "OutdoorFollowerSlot_2",
            "OutdoorFollowerSlot_3"
        }};

        for (size_t slotIndex = 0; slotIndex < FollowerSlots.size(); ++slotIndex)
        {
            if (pointerInsideHudElement(context, config, FollowerSlots[slotIndex], pointerX, pointerY))
            {
                return {GameplayHudPointerTargetType::FollowerPanelPortrait, slotIndex};
            }
        }

        if (pointerInsideHudElement(context, config, "OutdoorFollowerScrollUp", pointerX, pointerY))
        {
            return {GameplayHudPointerTargetType::FollowerPanelScrollUpButton};
        }

        if (pointerInsideHudElement(context, config, "OutdoorFollowerScrollDown", pointerX, pointerY))
        {
            return {GameplayHudPointerTargetType::FollowerPanelScrollDownButton};
        }
    }

    if (contextActionButtonEnabled
        && contextActionState.visible
        && contextActionState.primaryIndex < contextActionState.actions.size()
        && pointerInsideHudElement(context, config, "OutdoorMobileContextActionButton", pointerX, pointerY))
    {
        return {GameplayHudPointerTargetType::ContextActionButton, contextActionState.primaryIndex};
    }

    const std::pair<const char *, GameplayHudPointerTargetType> targets[] = {
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorButtonOptions",
                "OutdoorStandardButtonOptions"),
            GameplayHudPointerTargetType::MenuButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMobileButtonPause",
                ""),
            GameplayHudPointerTargetType::TurnBasedToggleButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorButtonRest",
                "OutdoorStandardButtonRest"),
            GameplayHudPointerTargetType::RestButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorButtonBooks",
                "OutdoorStandardButtonBooks"),
            GameplayHudPointerTargetType::BooksButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMobileButtonInventory",
                ""),
            GameplayHudPointerTargetType::InventoryButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorButtonDimensionDoor",
                "OutdoorStandardButtonDimensionDoor"),
            GameplayHudPointerTargetType::DimensionDoorButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorButtonQuickReference",
                "OutdoorStandardButtonQuickReference"),
            GameplayHudPointerTargetType::QuickReferenceButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMobileButtonQuickCast",
                ""),
            GameplayHudPointerTargetType::QuickCastButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMobileButtonAttack",
                ""),
            GameplayHudPointerTargetType::AttackButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMobileButtonCast",
                ""),
            GameplayHudPointerTargetType::CastButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMinimapZoomIn",
                "OutdoorStandardMinimapZoomIn"),
            GameplayHudPointerTargetType::MinimapZoomInButton
        },
        {
            activeGameplayButtonLayoutId(
                context,
                "OutdoorMinimapZoomOut",
                "OutdoorStandardMinimapZoomOut"),
            GameplayHudPointerTargetType::MinimapZoomOutButton
        }
    };

    for (const auto &[pLayoutId, targetType] : targets)
    {
        if (pLayoutId == nullptr || *pLayoutId == '\0')
        {
            continue;
        }

        const GameplayScreenRuntime::HudLayoutElement *pLayout = context.findHudLayoutElement(pLayoutId);

        if (pLayout == nullptr)
        {
            continue;
        }

        const std::optional<GameplayScreenRuntime::ResolvedHudLayoutElement> resolved =
            context.resolveHudLayoutElement(
                pLayoutId,
                config.screenWidth,
                config.screenHeight,
                pLayout->width,
                pLayout->height);

        if (resolved && context.isPointerInsideResolvedElement(*resolved, pointerX, pointerY))
        {
            return {targetType};
        }
    }

    return {};
}

void GameplayHudInputController::handleGameplayHudButtonInput(
    GameplayScreenRuntime &context,
    const GameplayHudButtonInputConfig &config)
{
    if (!config.allowInput || config.screenWidth <= 0 || config.screenHeight <= 0)
    {
        context.interactionState().gameplayHudClickLatch = false;
        context.interactionState().gameplayHudPressedTarget = {};
        context.interactionState().gameplayHudPressedContextActionActive = false;
        context.interactionState().gameplayHudPressedContextActionHit = {};
        return;
    }

    const HudPointerState pointerState = {
        config.pointerX,
        config.pointerY,
        config.leftButtonPressed
    };

    handlePointerClickRelease(
        pointerState,
        context.interactionState().gameplayHudClickLatch,
        context.interactionState().gameplayHudPressedTarget,
        GameplayHudPointerTarget{},
        [&context, &config, &pointerState](float, float) -> GameplayHudPointerTarget
        {
            const GameplayHudPointerTarget target = resolveGameplayHudButtonTarget(context, config);

            if (target.type == GameplayHudPointerTargetType::ContextActionButton
                && context.interactionState().gameplayHudPressedTarget.type
                    != GameplayHudPointerTargetType::ContextActionButton
                && pointerState.leftButtonPressed
                && !context.interactionState().gameplayHudClickLatch)
            {
                const GameplayContextActionState &contextActionState = context.contextActionStateReadOnly();
                context.interactionState().gameplayHudPressedContextActionActive = true;
                context.interactionState().gameplayHudPressedContextActionHit =
                    contextActionState.actions[target.index].worldHit;
            }

            return target;
        },
        [&context](const GameplayHudPointerTarget &target)
        {
            uint32_t sourceAction = 0;
            switch (target.type)
            {
            case GameplayHudPointerTargetType::MenuButton: sourceAction = SourceActionGameMenu; break;
            case GameplayHudPointerTargetType::RestButton: sourceAction = SourceActionRestWindow; break;
            case GameplayHudPointerTargetType::BooksButton: sourceAction = SourceActionClickBooks; break;
            case GameplayHudPointerTargetType::InventoryButton: sourceAction = SourceActionOpenInventory; break;
            case GameplayHudPointerTargetType::QuickReferenceButton: sourceAction = SourceActionQuickReference; break;
            case GameplayHudPointerTargetType::AttackButton: sourceAction = SourceActionAttack; break;
            case GameplayHudPointerTargetType::CastButton:
                sourceAction = context.pendingSpellTargetActive()
                    ? SourceActionCustomDialogButton : SourceActionSpellbookWindow;
                break;
            case GameplayHudPointerTargetType::DimensionDoorButton:
            case GameplayHudPointerTargetType::TurnBasedToggleButton:
            case GameplayHudPointerTargetType::FollowerPanelToggleButton:
                sourceAction = SourceActionCustomDialogButton;
                break;
            case GameplayHudPointerTargetType::MinimapZoomInButton: sourceAction = SourceActionZoomIn; break;
            case GameplayHudPointerTargetType::MinimapZoomOutButton: sourceAction = SourceActionZoomOut; break;
            case GameplayHudPointerTargetType::FollowerPanelPortrait:
                sourceAction = SourceActionStartNpcDialogue;
                break;
            case GameplayHudPointerTargetType::FollowerPanelScrollUpButton:
            case GameplayHudPointerTargetType::FollowerPanelScrollDownButton:
                sourceAction = SourceActionScrollNpcPanel;
                break;
            default: break;
            }
            if (sourceAction != 0 && !context.allowSourceGameplayAction(sourceAction))
            {
                return;
            }

            switch (target.type)
            {
            case GameplayHudPointerTargetType::MenuButton:
                if (context.pendingSpellTargetActive())
                {
                    context.interactionState().gameplayHudSpellTargetCancelRequested = true;
                }
                else
                {
                    context.openMenuOverlay();
                }
                break;
            case GameplayHudPointerTargetType::RestButton:
                context.openRestOverlay();
                break;
            case GameplayHudPointerTargetType::BooksButton:
                context.openJournalOverlay();
                break;
            case GameplayHudPointerTargetType::InventoryButton:
                context.toggleCharacterInventoryScreen();
                break;
            case GameplayHudPointerTargetType::DimensionDoorButton:
                openDimensionDoorOverlay(context);
                break;
            case GameplayHudPointerTargetType::QuickReferenceButton:
                context.openQuickReferenceOverlay();
                break;
            case GameplayHudPointerTargetType::QuickCastButton:
                context.interactionState().gameplayHudQuickCastRequested = true;
                break;
            case GameplayHudPointerTargetType::AttackButton:
                context.interactionState().gameplayHudAttackRequested = true;
                break;
            case GameplayHudPointerTargetType::TriggerButton:
                context.interactionState().gameplayHudTriggerRequested = true;
                break;
            case GameplayHudPointerTargetType::CastButton:
                if (context.pendingSpellTargetActive())
                {
                    context.interactionState().gameplayHudSpellTargetConfirmRequested = true;
                }
                else
                {
                    context.openSpellbookOverlay();
                }
                break;
            case GameplayHudPointerTargetType::TurnBasedToggleButton:
                if (context.pendingSpellTargetActive())
                {
                    context.interactionState().gameplayHudSpellTargetCancelRequested = true;
                }
                else
                {
                    context.toggleTurnBasedMode();
                }
                break;
            case GameplayHudPointerTargetType::ContextActionButton:
            {
                GameplayContextActionState &contextActionState = context.contextActionState();
                IGameplayWorldRuntime *pWorldRuntime = context.worldRuntime();
                const GameplayContextAction *pSelectedAction =
                    target.index < contextActionState.actions.size()
                        ? &contextActionState.actions[target.index]
                        : nullptr;
                const bool dropHeldItemAction =
                    pSelectedAction != nullptr
                    && pSelectedAction->kind == GameplayContextActionKind::DropHeldItem;
                const bool contextActionPopupEnabled =
                    context.settingsSnapshot().contextActionPopup || dropHeldItemAction;
                const bool hasPressedContextAction =
                    contextActionPopupEnabled && context.interactionState().gameplayHudPressedContextActionActive;
                const GameplayWorldHit pressedWorldHit =
                    context.interactionState().gameplayHudPressedContextActionHit;
                GameplayWorldHit activationHit = {};

                if (hasPressedContextAction)
                {
                    activationHit = pressedWorldHit;
                }
                else if (contextActionPopupEnabled
                    && contextActionState.visible
                    && target.index < contextActionState.actions.size())
                {
                    activationHit = contextActionState.actions[target.index].worldHit;
                }

                if (dropHeldItemAction)
                {
                    context.interactionState().gameplayHudDropHeldItemRequested = true;
                }
                else if (pWorldRuntime != nullptr
                    && activationHit.hasHit
                    && pWorldRuntime->canActivateWorldHit(activationHit, GameplayInteractionMethod::Keyboard)
                    && context.allowSourceGameplayAction(SourceActionWorldLeftClick))
                {
                    pWorldRuntime->activateWorldHit(activationHit);
                }

                context.interactionState().gameplayHudPressedContextActionActive = false;
                context.interactionState().gameplayHudPressedContextActionHit = {};
                context.clearContextActionState();
                break;
            }
            case GameplayHudPointerTargetType::MinimapZoomInButton:
                context.zoomGameplayMinimapIn();
                break;
            case GameplayHudPointerTargetType::MinimapZoomOutButton:
                context.zoomGameplayMinimapOut();
                break;
            case GameplayHudPointerTargetType::FollowerPanelToggleButton:
                context.interactionState().followerPanelOpen = !context.interactionState().followerPanelOpen;
                break;
            case GameplayHudPointerTargetType::FollowerPanelPortrait:
                context.openFollowerNpcDialogue(target.index);
                break;
            case GameplayHudPointerTargetType::FollowerPanelScrollUpButton:
                if (context.interactionState().followerPanelScrollOffset > 0)
                {
                    --context.interactionState().followerPanelScrollOffset;
                }
                break;
            case GameplayHudPointerTargetType::FollowerPanelScrollDownButton:
            {
                const size_t followerCount = hiredFollowerCount(context);
                const size_t maxOffset =
                    followerCount > VisibleFollowerPanelSlots ? followerCount - VisibleFollowerPanelSlots : 0;
                context.interactionState().followerPanelScrollOffset =
                    std::min(context.interactionState().followerPanelScrollOffset + 1u, maxOffset);
                break;
            }
            case GameplayHudPointerTargetType::None:
                break;
        }
        });

    if (!context.interactionState().gameplayHudClickLatch)
    {
        context.interactionState().gameplayHudPressedContextActionActive = false;
        context.interactionState().gameplayHudPressedContextActionHit = {};
    }
}
} // namespace OpenYAMM::Game
