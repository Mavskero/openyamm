#include "game/gameplay/GameplaySpellService.h"

#include "game/app/GameSession.h"
#include "game/audio/GameAudioSystem.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/GameplayHeldItemController.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/gameplay/TurnBasedCombatRuntime.h"
#include "game/party/LloydsBeaconRuntime.h"
#include "game/party/SpellIds.h"
#include "game/tables/UiTextTable.h"

#include <SDL3/SDL_platform.h>

namespace OpenYAMM::Game
{
namespace
{
using UiMessage = UiTextTable::Message;

bool isUtilitySelectionRequest(const PartySpellCastRequest &request)
{
    return request.utilityAction != PartySpellUtilityActionKind::None
        || request.targetInventoryGridX.has_value()
        || request.targetEquipmentSlot.has_value();
}

void preserveUtilitySpellCastContext(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request)
{
    GameplayUiController::UtilitySpellOverlayState &overlay = runtime.utilitySpellOverlay();
    overlay.skillLevelOverride = request.skillLevelOverride;
    overlay.skillMasteryOverride = request.skillMasteryOverride;
    overlay.spendMana = request.spendMana;
    overlay.applyRecovery = request.applyRecovery;
    overlay.bypassRequiredMastery = request.bypassRequiredMastery;
    overlay.bypassGameplayCasterValidation = request.bypassGameplayCasterValidation;
    overlay.bypassTownPortalFailureChecks = request.bypassTownPortalFailureChecks;
}

bool isFollowupSpellUiActive(
    const GameplayScreenState &screenState,
    const GameplayScreenRuntime &runtime)
{
    return screenState.pendingSpellTarget().active || runtime.utilitySpellOverlayReadOnly().active;
}

SkillMastery dragonAbilityMasteryForCaster(const GameplayScreenRuntime &runtime, size_t casterMemberIndex)
{
    const Party *pParty = runtime.partyReadOnly();
    const Character *pCaster = pParty != nullptr ? pParty->member(casterMemberIndex) : nullptr;
    const CharacterSkill *pDragonAbility = pCaster != nullptr ? pCaster->findSkill("DragonAbility") : nullptr;
    return pDragonAbility != nullptr ? pDragonAbility->mastery : SkillMastery::None;
}
}

GameplaySpellService::GameplaySpellService(GameSession &session)
    : m_session(session)
{
}

GameplaySpellService::QuickSpellStartResolution GameplaySpellService::tryStartQuickSpellCast(
    GameplayScreenRuntime &runtime,
    const TargetQueries &targetQueries)
{
    QuickSpellStartResolution resolution = {};
    const UiTextTable &uiText = runtime.gameplayUiRuntime().uiTextTable();

    if (m_session.gameplayScreenState().pendingSpellTarget().active)
    {
        showPendingTargetSelectionPrompt(runtime);
        return resolution;
    }

    IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();
    const bool hasActiveLootView =
        pWorldRuntime != nullptr
        && (pWorldRuntime->activeChestView() != nullptr || pWorldRuntime->activeCorpseView() != nullptr);

    if (runtime.heldInventoryItem().active
        || runtime.characterScreenReadOnly().open
        || runtime.spellbookReadOnly().active
        || runtime.controlsScreenState().active
        || runtime.keyboardScreenState().active
        || runtime.menuScreenState().active
        || runtime.saveGameScreenState().active
        || runtime.loadGameScreenState().active
        || runtime.activeEventDialog().isActive
        || hasActiveLootView)
    {
        runtime.setStatusBarEvent(uiText.text(UiMessage::FinishCurrentAction));
        return resolution;
    }

    Party *pParty = runtime.party();

    if (pParty == nullptr)
    {
        return resolution;
    }

    const Character *pCaster = pParty->activeMember();

    if (pCaster == nullptr || !GameMechanics::canTakeGameplayAction(*pCaster))
    {
        return resolution;
    }

    if (pCaster->quickSpellName.empty())
    {
        resolution.disposition = QuickSpellStartDisposition::AttackFallback;
        return resolution;
    }

    const SpellTable *pSpellTable = runtime.spellTable();

    if (pSpellTable == nullptr)
    {
        return resolution;
    }

    const SpellEntry *pSpellEntry = pSpellTable->findByName(pCaster->quickSpellName);

    if (pSpellEntry == nullptr)
    {
        runtime.setStatusBarEvent(uiText.text(UiMessage::SpellUnknownQuick));
        return resolution;
    }

    if (tryCastSpellFromMember(
            runtime,
            pParty->activeMemberIndex(),
            uint32_t(pSpellEntry->id),
            pSpellEntry->name,
            true,
            &targetQueries))
    {
        resolution.disposition = QuickSpellStartDisposition::CastStarted;
    }

    return resolution;
}

GameplaySpellService::MemberSpellCastResolution GameplaySpellService::castSpellFromMember(
    GameplayScreenRuntime &runtime,
    size_t casterMemberIndex,
    uint32_t spellId,
    const std::string &spellName,
    bool quickCast,
    const TargetQueries *pTargetQueries)
{
    MemberSpellCastResolution resolution = {};
    Party *pParty = runtime.party();

    if (pParty != nullptr)
    {
        const Character *pCaster = pParty->member(casterMemberIndex);

        if (pCaster == nullptr || !pCaster->knowsSpell(spellId))
        {
            runtime.setStatusBarEvent(runtime.gameplayUiRuntime().uiTextTable().text(UiMessage::SpellNotLearned));
            return resolution;
        }
    }

    PartySpellCastRequest request = {};
    request.casterMemberIndex = casterMemberIndex;
    request.spellId = spellId;

    if (quickCast
        && pTargetQueries != nullptr
        && !tryPrepareQuickCastRequest(runtime, request, spellName, *pTargetQueries))
    {
        return resolution;
    }

    resolution.castStarted = runtime.tryCastSpellRequest(request, spellName);
    resolution.followupUiActive =
        resolution.castStarted && isFollowupSpellUiActive(m_session.gameplayScreenState(), runtime);
    return resolution;
}

bool GameplaySpellService::tryCastSpellFromMember(
    GameplayScreenRuntime &runtime,
    size_t casterMemberIndex,
    uint32_t spellId,
    const std::string &spellName,
    bool quickCast,
    const TargetQueries *pTargetQueries)
{
    return castSpellFromMember(
        runtime,
        casterMemberIndex,
        spellId,
        spellName,
        quickCast,
        pTargetQueries).castStarted;
}

bool GameplaySpellService::tryPrepareQuickCastRequest(
    GameplayScreenRuntime &runtime,
    PartySpellCastRequest &request,
    const std::string &spellName,
    const TargetQueries &targetQueries) const
{
    const UiTextTable &uiText = runtime.gameplayUiRuntime().uiTextTable();
    if (spellIdFromValue(request.spellId) == SpellId::Fear)
    {
        request.quickCast = true;

        if (dragonAbilityMasteryForCaster(runtime, request.casterMemberIndex) >= SkillMastery::Grandmaster)
        {
            return true;
        }

        const PartySpellDescriptor fearTargetDescriptor = {
            request.spellId,
            PartySpellCastTargetKind::Actor,
            PartySpellCastEffectKind::ActorEffect
        };

        if (tryResolveQuickCastRequest(runtime, request, fearTargetDescriptor, targetQueries))
        {
            return true;
        }

        runtime.setStatusBarEvent(uiText.text(UiMessage::SpellNoValidTarget), 3.0f);
        return false;
    }

    const std::optional<PartySpellDescriptor> descriptor = PartySpellSystem::describeSpell(request.spellId);

    if (!descriptor || !isQuickCastable(*descriptor))
    {
        return true;
    }

    request.quickCast = true;

    if (tryResolveQuickCastRequest(runtime, request, *descriptor, targetQueries))
    {
        return true;
    }

    runtime.setStatusBarEvent(uiText.text(UiMessage::SpellNoValidTarget), 3.0f);
    return false;
}

bool GameplaySpellService::tryResolveQuickCastRequest(
    GameplayScreenRuntime &runtime,
    PartySpellCastRequest &request,
    const PartySpellDescriptor &descriptor,
    const TargetQueries &targetQueries)
{
    IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();

    if (descriptor.targetKind == PartySpellCastTargetKind::None)
    {
        return true;
    }

    if (descriptor.targetKind == PartySpellCastTargetKind::Actor)
    {
        if (pWorldRuntime != nullptr)
        {
            request.targetActorIndex = pWorldRuntime->spellActionHoveredActorIndex();

            if (!request.targetActorIndex)
            {
                request.targetActorIndex = pWorldRuntime->spellActionClosestVisibleHostileActorIndex();
            }
        }

        if (request.targetActorIndex)
        {
            return true;
        }

        if (descriptor.effectKind == PartySpellCastEffectKind::Projectile)
        {
            const std::optional<bx::Vec3> groundTargetPoint =
                resolveCursorTargetPoint(targetQueries, pWorldRuntime);

            if (!groundTargetPoint)
            {
                return false;
            }

            request.hasTargetPoint = true;
            request.targetX = groundTargetPoint->x;
            request.targetY = groundTargetPoint->y;
            request.targetZ = groundTargetPoint->z;
            return true;
        }

        return false;
    }

    if (descriptor.targetKind == PartySpellCastTargetKind::GroundPoint)
    {
        std::optional<bx::Vec3> targetPoint;

        if (pWorldRuntime != nullptr)
        {
            const std::optional<size_t> hoveredActorIndex = pWorldRuntime->spellActionHoveredActorIndex();

            if (hoveredActorIndex)
            {
                targetPoint = pWorldRuntime->spellActionActorTargetPoint(*hoveredActorIndex);
            }
        }

        if (!targetPoint)
        {
            targetPoint = resolveCursorTargetPoint(targetQueries, pWorldRuntime);
        }

        if (!targetPoint)
        {
            return false;
        }

        request.hasTargetPoint = true;
        request.targetX = targetPoint->x;
        request.targetY = targetPoint->y;
        request.targetZ = targetPoint->z;
        return true;
    }

    return false;
}

std::optional<bx::Vec3> GameplaySpellService::resolveCursorTargetPoint(
    const TargetQueries &targetQueries,
    const IGameplayWorldRuntime *pWorldRuntime)
{
    if (pWorldRuntime == nullptr)
    {
        return std::nullopt;
    }

    if (targetQueries.useCrosshairTarget && targetQueries.screenWidth > 0.0f && targetQueries.screenHeight > 0.0f)
    {
        return pWorldRuntime->spellActionGroundTargetPoint(
            targetQueries.screenWidth * 0.5f,
            targetQueries.screenHeight * 0.5f);
    }

    return pWorldRuntime->spellActionGroundTargetPoint(targetQueries.cursorX, targetQueries.cursorY);
}

GameplaySpellService::SpellRequestResolution GameplaySpellService::resolveSpellRequest(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request,
    const std::string &spellName)
{
    SpellRequestResolution resolution = {};

    if (!request.bypassGameplayCasterValidation
        && !tryValidateCasterForGameplayCast(runtime, request.casterMemberIndex))
    {
        return resolution;
    }

    resolution.castResult = castSpell(runtime, request);

    if (resolution.castResult.succeeded())
    {
        applySuccessFeedback(runtime, request, spellName, resolution.castResult);
        resolution.disposition = SpellRequestDisposition::CastSucceeded;
        return resolution;
    }

    if (tryOpenSelectionUi(runtime, request, spellName, resolution.castResult))
    {
        Party *pParty = runtime.party();

        if (pParty != nullptr)
        {
            pParty->setActiveMemberIndex(request.casterMemberIndex);
        }

        resolution.disposition = SpellRequestDisposition::OpenedSelectionUi;
        return resolution;
    }

    if (requiresTargetSelection(resolution.castResult))
    {
        if (isSpellId(request.spellId, SpellId::Telekinesis)
            && !GameplayHeldItemController::tryDisplaceHeldInventoryItem(runtime))
        {
            runtime.setStatusBarEvent(runtime.gameplayUiRuntime().uiTextTable().text(UiMessage::FinishCurrentAction));
            return resolution;
        }

        resolution.disposition = SpellRequestDisposition::NeedsTargetSelection;
        return resolution;
    }

    applyFailureFeedback(runtime, request, resolution.castResult);
    return resolution;
}

void GameplaySpellService::armPendingTargetSelection(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request,
    PartySpellCastTargetKind targetKind,
    const std::string &spellName) const
{
    GameplayScreenState::PendingSpellTargetState &pendingTargetState =
        m_session.gameplayScreenState().pendingSpellTarget();
    pendingTargetState.clear();
    pendingTargetState.active = true;
    pendingTargetState.casterMemberIndex = request.casterMemberIndex;
    pendingTargetState.spellId = request.spellId;
    pendingTargetState.skillLevelOverride = request.skillLevelOverride;
    pendingTargetState.skillMasteryOverride = request.skillMasteryOverride;
    pendingTargetState.spendMana = request.spendMana;
    pendingTargetState.applyRecovery = request.applyRecovery;
    pendingTargetState.bypassRequiredMastery = request.bypassRequiredMastery;
    pendingTargetState.targetKind = targetKind;
    pendingTargetState.spellName = spellName;
    showPendingTargetSelectionPrompt(runtime);
}

void GameplaySpellService::clearPendingTargetSelection(
    GameplayScreenRuntime &runtime,
    const std::string &statusText) const
{
    m_session.gameplayScreenState().pendingSpellTarget().clear();

    if (!statusText.empty())
    {
        runtime.setStatusBarEvent(statusText);
    }
}

PartySpellCastRequest GameplaySpellService::makePendingTargetSelectionRequest() const
{
    const GameplayScreenState::PendingSpellTargetState &pendingTargetState =
        m_session.gameplayScreenState().pendingSpellTarget();

    PartySpellCastRequest request = {};
    request.casterMemberIndex = pendingTargetState.casterMemberIndex;
    request.spellId = pendingTargetState.spellId;
    request.skillLevelOverride = pendingTargetState.skillLevelOverride;
    request.skillMasteryOverride = pendingTargetState.skillMasteryOverride;
    request.spendMana = pendingTargetState.spendMana;
    request.applyRecovery = pendingTargetState.applyRecovery;
    request.bypassRequiredMastery = pendingTargetState.bypassRequiredMastery;
    return request;
}

std::string GameplaySpellService::pendingTargetSelectionPromptText(bool includeControls) const
{
    const GameplayScreenState::PendingSpellTargetState &pendingTargetState =
        m_session.gameplayScreenState().pendingSpellTarget();
    const UiTextTable &uiText = m_session.gameplayUiRuntime().uiTextTable();

    const UiMessage promptMessage =
        pendingTargetState.targetKind == PartySpellCastTargetKind::Actor
            ? UiMessage::SpellSelectActorFormat
            : pendingTargetState.targetKind == PartySpellCastTargetKind::Character
            ? UiMessage::SpellSelectCharacterFormat
            : pendingTargetState.targetKind == PartySpellCastTargetKind::GroundPoint
            ? UiMessage::SpellSelectGroundFormat
            : UiMessage::SpellSelectTargetFormat;
    std::string prompt = uiText.formatSpellPrompt(promptMessage, pendingTargetState.spellName);

    if (includeControls)
    {
        UiMessage controlsMessage = UiMessage::SpellTargetControlsMouse;
#if defined(__ANDROID__) || defined(SDL_PLATFORM_IOS)
        controlsMessage = pendingTargetState.targetKind == PartySpellCastTargetKind::Character
            ? UiMessage::SpellTargetControlsTouchPortrait : UiMessage::SpellTargetControlsTouchTarget;
#if defined(__ANDROID__)
        const bool mobileGroundTargetConfirmSpell =
            isSpellId(pendingTargetState.spellId, SpellId::MeteorShower)
            || isSpellId(pendingTargetState.spellId, SpellId::Starburst);

        if (mobileGroundTargetConfirmSpell)
        {
            controlsMessage = UiMessage::SpellTargetControlsTouchGround;
        }
#endif
#endif
        prompt += "  " + uiText.text(controlsMessage);
    }

    return prompt;
}

void GameplaySpellService::showPendingTargetSelectionPrompt(
    GameplayScreenRuntime &runtime,
    bool includeControls,
    float durationSeconds) const
{
    const GameplayScreenState::PendingSpellTargetState &pendingTargetState =
        m_session.gameplayScreenState().pendingSpellTarget();

    if (!pendingTargetState.active)
    {
        return;
    }

    runtime.setStatusBarEvent(pendingTargetSelectionPromptText(includeControls), durationSeconds);
}

bool GameplaySpellService::validatePendingTargetSelectionRequest(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request) const
{
    const GameplayScreenState::PendingSpellTargetState &pendingTargetState =
        m_session.gameplayScreenState().pendingSpellTarget();

    bool targetResolved = true;

    switch (pendingTargetState.targetKind)
    {
        case PartySpellCastTargetKind::Actor:
            targetResolved = request.targetActorIndex.has_value();
            break;

        case PartySpellCastTargetKind::Character:
            targetResolved = request.targetCharacterIndex.has_value();
            break;

        case PartySpellCastTargetKind::CharacterOrWorldTarget:
            targetResolved = request.targetCharacterIndex.has_value() || request.targetWorldHit.has_value();
            break;

        case PartySpellCastTargetKind::ActorOrCharacter:
            targetResolved = request.targetActorIndex.has_value() || request.targetCharacterIndex.has_value();
            break;

        case PartySpellCastTargetKind::GroundPoint:
            targetResolved = request.hasTargetPoint;
            break;

        case PartySpellCastTargetKind::TelekinesisTarget:
            targetResolved = request.telekinesisTarget.has_value();
            break;

        default:
            break;
    }

    if (targetResolved)
    {
        return true;
    }

    showPendingTargetSelectionPrompt(runtime);
    return false;
}

GameplaySpellService::PendingTargetResolution GameplaySpellService::resolvePendingTargetSelectionCast(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request) const
{
    PendingTargetResolution resolution = {};
    resolution.castResult = castSpell(runtime, request);

    if (resolution.castResult.succeeded())
    {
        const std::string spellName = m_session.gameplayScreenState().pendingSpellTarget().spellName;
        applySuccessFeedback(runtime, request, spellName, resolution.castResult);
        resolution.disposition = PendingTargetResolutionDisposition::CastSucceeded;
        return resolution;
    }

    if (requiresTargetSelection(resolution.castResult))
    {
        showPendingTargetSelectionPrompt(runtime);
        resolution.disposition = PendingTargetResolutionDisposition::NeedsTargetSelection;
        return resolution;
    }

    applyFailureFeedback(runtime, request, resolution.castResult);
    clearPendingTargetSelection(runtime);
    return resolution;
}

bool GameplaySpellService::tryValidateCasterForGameplayCast(GameplayScreenRuntime &runtime, size_t casterMemberIndex)
{
    Party *pParty = runtime.party();

    if (pParty == nullptr)
    {
        return false;
    }

    const Character *pCaster = pParty->member(casterMemberIndex);

    if (pCaster == nullptr || !GameMechanics::canTakeGameplayAction(*pCaster))
    {
        return false;
    }

    return true;
}

PartySpellCastResult GameplaySpellService::castSpell(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request) const
{
    Party *pParty = runtime.party();
    IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();
    const SpellTable *pSpellTable = runtime.spellTable();

    if (pParty == nullptr || pWorldRuntime == nullptr || pSpellTable == nullptr)
    {
        return {};
    }

    PartySpellCastRequest effectiveRequest = request;
    TurnBasedCombatRuntime &turnBasedCombatRuntime = runtime.turnBasedCombatRuntime();
    const bool turnBasedRecovery =
        turnBasedCombatRuntime.active()
        && request.applyRecovery
        && turnBasedCombatRuntime.stage() == TurnBasedCombatStage::Attack;

    if (turnBasedRecovery)
    {
        effectiveRequest.applyRecovery = false;
        effectiveRequest.turnBasedPendingAction = true;
    }

    PartySpellCastResult result = PartySpellSystem::castSpell(*pParty, *pWorldRuntime, *pSpellTable, effectiveRequest);

    if (turnBasedRecovery && result.recoverySeconds > 0.0f)
    {
        turnBasedCombatRuntime.storeMemberTurnRecovery(request.casterMemberIndex, result.recoverySeconds);

        turnBasedCombatRuntime.applyPlayerAction(*pParty, request.casterMemberIndex, result.recoverySeconds);
    }

    return result;
}

bool GameplaySpellService::isQuickCastable(const PartySpellDescriptor &descriptor)
{
    if (spellIdFromValue(descriptor.spellId) == SpellId::MeteorShower
        || spellIdFromValue(descriptor.spellId) == SpellId::Starburst)
    {
        return false;
    }

    if (descriptor.targetKind == PartySpellCastTargetKind::None)
    {
        return true;
    }

    if (descriptor.targetKind == PartySpellCastTargetKind::Actor)
    {
        return descriptor.effectKind == PartySpellCastEffectKind::Projectile
            || descriptor.effectKind == PartySpellCastEffectKind::ActorEffect;
    }

    if (descriptor.targetKind == PartySpellCastTargetKind::GroundPoint)
    {
        return descriptor.effectKind == PartySpellCastEffectKind::Projectile
            || descriptor.effectKind == PartySpellCastEffectKind::MultiProjectile
            || descriptor.effectKind == PartySpellCastEffectKind::AreaEffect;
    }

    return false;
}

bool GameplaySpellService::requiresTargetSelection(const PartySpellCastResult &result)
{
    return result.status == PartySpellCastStatus::NeedActorTarget
        || result.status == PartySpellCastStatus::NeedCharacterTarget
        || result.status == PartySpellCastStatus::NeedCharacterOrWorldTarget
        || result.status == PartySpellCastStatus::NeedActorOrCharacterTarget
        || result.status == PartySpellCastStatus::NeedGroundPoint
        || result.status == PartySpellCastStatus::NeedTelekinesisTarget;
}

bool GameplaySpellService::tryOpenSelectionUi(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request,
    const std::string &spellName,
    const PartySpellCastResult &result)
{
    const UiTextTable &uiText = runtime.gameplayUiRuntime().uiTextTable();
    if (result.status == PartySpellCastStatus::NeedInventoryItemTarget)
    {
        runtime.closeSpellbookOverlay();
        runtime.characterScreen().open = true;
        runtime.characterScreen().dollJewelryOverlayOpen = false;
        runtime.characterScreen().adventurersInnRosterOverlayOpen = false;
        runtime.characterScreen().source = GameplayUiController::CharacterScreenSource::Party;
        runtime.characterScreen().sourceIndex = request.casterMemberIndex;
        runtime.characterScreen().page = GameplayUiController::CharacterPage::Inventory;
        runtime.openUtilitySpellOverlay(
            GameplayUiController::UtilitySpellOverlayMode::InventoryTarget,
            request.spellId,
            request.casterMemberIndex);
        preserveUtilitySpellCastContext(runtime, request);
        runtime.setStatusBarEvent(spellName + ": " + uiText.text(UiMessage::InventoryTargetHint), 4.0f);
        return true;
    }

    if (result.status != PartySpellCastStatus::NeedUtilityUi)
    {
        return false;
    }

    runtime.closeSpellbookOverlay();

    if (isSpellId(request.spellId, SpellId::TownPortal))
    {
        if (runtime.tryRunTownPortalHook())
        {
            return true;
        }
        const bool dimensionDoorEligible = runtime.shouldTownPortalCastOpenDimensionDoor();
        if (runtime.tryInterruptTownPortalForVerdant(dimensionDoorEligible))
        {
            return true;
        }

        if (dimensionDoorEligible)
        {
            if (runtime.openDimensionDoorOverlay(
                request.casterMemberIndex,
                request.spellId,
                uiText.text(UiMessage::HighMagicPresence)))
            {
                preserveUtilitySpellCastContext(runtime, request);
            }
            return true;
        }

        if (!runtime.ensureTownPortalDestinationsLoaded())
        {
            runtime.setStatusBarEvent(uiText.text(UiMessage::TownPortalDataMissing));
            return true;
        }

        runtime.openUtilitySpellOverlay(
            GameplayUiController::UtilitySpellOverlayMode::TownPortal,
            request.spellId,
            request.casterMemberIndex);
        preserveUtilitySpellCastContext(runtime, request);
        runtime.resetUtilitySpellOverlayInteractionState();
        runtime.setStatusBarEvent(uiText.text(UiMessage::TownPortalChooseDestination), 4.0f);
        return true;
    }

    if (isSpellId(request.spellId, SpellId::LloydsBeacon))
    {
        const Party *pParty = runtime.partyReadOnly();
        const Character *pCaster = pParty != nullptr ? pParty->member(request.casterMemberIndex) : nullptr;
        bool hasRecallableBeacon = lloydsBeaconHasRecallableBeacon(pCaster);
        if (pCaster != nullptr && request.skillLevelOverride > 0 && request.skillMasteryOverride != SkillMastery::None)
        {
            hasRecallableBeacon = false;
            const size_t slotCount = lloydsBeaconMaxSlotsForMastery(request.skillMasteryOverride);
            for (size_t index = 0; index < slotCount && index < pCaster->lloydsBeacons.size(); ++index)
            {
                const std::optional<LloydBeacon> &beacon = pCaster->lloydsBeacons[index];
                if (beacon.has_value() && beacon->remainingSeconds > 0.0f)
                {
                    hasRecallableBeacon = true;
                    break;
                }
            }
        }

        runtime.openUtilitySpellOverlay(
            GameplayUiController::UtilitySpellOverlayMode::LloydsBeacon,
            request.spellId,
            request.casterMemberIndex,
            hasRecallableBeacon);
        preserveUtilitySpellCastContext(runtime, request);
        runtime.resetUtilitySpellOverlayInteractionState();
        runtime.setStatusBarEvent(
            uiText.text(UiMessage::BeaconSet) + " / " + uiText.text(UiMessage::BeaconRecall), 4.0f);
        return true;
    }

    return false;
}

void GameplaySpellService::applySuccessFeedback(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request,
    const std::string &spellName,
    const PartySpellCastResult &result) const
{
    if (isUtilitySelectionRequest(request))
    {
        runtime.closeUtilitySpellOverlayAfterSpellResolution(request.spellId);
    }

    runtime.triggerPortraitFaceAnimation(request.casterMemberIndex, FaceAnimationId::CastSpell);
    runtime.playSpeechReaction(request.casterMemberIndex, SpeechId::CastSpell, false);

    if (isSpellId(request.spellId, SpellId::Awaken) && !result.affectedCharacterIndices.empty())
    {
        runtime.playSpeechReaction(result.affectedCharacterIndices.front(), SpeechId::Awaken, true);
    }

    m_session.gameplayFxService().triggerPortraitSpellFx(result);
    if (result.screenOverlayRequest.has_value())
    {
        m_session.gameplayFxService().triggerGameplayScreenOverlay(*result.screenOverlayRequest);
    }

    if (runtime.audioSystem() != nullptr && runtime.spellTable() != nullptr)
    {
        const SpellEntry *pSpellEntry = runtime.spellTable()->findById(request.spellId);

        if (result.effectKind == PartySpellCastEffectKind::CharacterRestore
            || result.effectKind == PartySpellCastEffectKind::PartyRestore)
        {
            runtime.audioSystem()->playCommonSound(SoundId::Heal, GameAudioSystem::PlaybackGroup::Ui);
        }
        else if (pSpellEntry != nullptr
            && pSpellEntry->effectSoundId > 0
            && result.effectKind != PartySpellCastEffectKind::Projectile
            && result.effectKind != PartySpellCastEffectKind::MultiProjectile)
        {
            runtime.audioSystem()->playSound(
                pSpellEntry->effectSoundId,
                GameAudioSystem::PlaybackGroup::Ui);
        }
    }

    if (!spellName.empty() && result.effectKind != PartySpellCastEffectKind::Telekinesis)
    {
        runtime.setStatusBarEvent(
            runtime.gameplayUiRuntime().uiTextTable().formatSpellPrompt(UiMessage::SpellCastFormat, spellName));
    }
}

void GameplaySpellService::applyFailureFeedback(
    GameplayScreenRuntime &runtime,
    const PartySpellCastRequest &request,
    const PartySpellCastResult &result) const
{
    if (isUtilitySelectionRequest(request))
    {
        runtime.closeUtilitySpellOverlayAfterSpellResolution(request.spellId);
    }

    UiMessage message = UiMessage::Count;
    switch (result.failureReason)
    {
        case PartySpellFailureReason::GenericSpellFailed:
            message = UiMessage::SpellFailed;
            break;
        case PartySpellFailureReason::DivineInterventionBlockedByGear:
            message = UiMessage::SpellDivineInterventionBlockedByGear;
            break;
        case PartySpellFailureReason::MeteorShowerIndoors:
            message = UiMessage::SpellMeteorShowerIndoors;
            break;
        case PartySpellFailureReason::StarburstIndoors:
            message = UiMessage::SpellStarburstIndoors;
            break;
        case PartySpellFailureReason::FlyIndoors:
            message = UiMessage::SpellFlyIndoors;
            break;
        case PartySpellFailureReason::ArmageddonIndoors:
            message = UiMessage::SpellArmageddonIndoors;
            break;
        case PartySpellFailureReason::InfernoOutdoors:
            message = UiMessage::SpellInfernoOutdoors;
            break;
        case PartySpellFailureReason::PrismaticLightOutdoors:
            message = UiMessage::SpellPrismaticLightOutdoors;
            break;
        case PartySpellFailureReason::HostileCreaturesNearby:
            message = UiMessage::SpellHostileCreaturesNearby;
            break;
        case PartySpellFailureReason::WandAlreadyCharged:
            message = UiMessage::SpellWandAlreadyCharged;
            break;
        case PartySpellFailureReason::ItemQualityTooLow:
            message = UiMessage::SpellItemQualityTooLow;
            break;
        case PartySpellFailureReason::SummonLimitReached:
            message = UiMessage::SpellSummonLimitReached;
            break;
        case PartySpellFailureReason::ArmageddonAlreadyActive:
            message = UiMessage::SpellArmageddonAlreadyActive;
            break;
        case PartySpellFailureReason::ArmageddonDailyLimit:
            message = UiMessage::SpellArmageddonDailyLimit;
            break;
        case PartySpellFailureReason::None:
            break;
    }

    if (message == UiMessage::Count)
    {
        switch (result.status)
        {
            case PartySpellCastStatus::NotEnoughSpellPoints:
                message = UiMessage::SpellNotEnoughSpellPoints;
                break;
            case PartySpellCastStatus::NotSkilledEnough:
                message = UiMessage::SpellNotSkilledEnough;
                break;
            case PartySpellCastStatus::InvalidCaster:
            case PartySpellCastStatus::InvalidSpell:
                message = UiMessage::SpellFailed;
                break;
            default:
                break;
        }
    }

    const std::string &statusText = message != UiMessage::Count
        ? runtime.gameplayUiRuntime().uiTextTable().text(message) : result.statusText;
    runtime.triggerSpellFailureFeedback(request.casterMemberIndex, statusText);
}
}
