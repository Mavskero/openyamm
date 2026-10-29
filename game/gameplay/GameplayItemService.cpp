#include "game/gameplay/GameplayItemService.h"

#include "game/app/GameSession.h"
#include "game/audio/GameAudioSystem.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/GameMechanics.h"
#include "game/items/ItemRuntime.h"
#include "game/items/ConnectorStoneRuntime.h"
#include "game/tables/CombatTextTable.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/InventoryItemMixingRuntime.h"
#include "game/items/InventoryItemUseRuntime.h"
#include "game/party/SpellIds.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/PotionNoteTable.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *IdentifyFailedText = "Identify Failed";
constexpr const char *RepairFailedText = "Repair Failed";
constexpr const char *NwcDungeonMapName = "7nwc.blv";
constexpr const char *TempleInBottleReturnLocationName = "TempleInABottleReturn";
constexpr uint32_t DimensionDoorScrollItemId = 190;
constexpr int16_t FireballImpactObjectId = 1051;
constexpr float PotionExplosionForwardOffset = 64.0f;
constexpr float PotionExplosionHeightOffset = 96.0f;
constexpr uint32_t AutoNoteVariableTag = 0x00E1u;

enum class ActiveLootOperation
{
    ForceIdentify,
    IdentifyWithSkill,
    RepairWithSkill
};

bool applyLootOperation(
    GameplayChestItemState &lootItem,
    const ItemTable &itemTable,
    ActiveLootOperation operation,
    const Character *pInspector,
    std::string &statusText)
{
    statusText.clear();

    if (lootItem.isGold)
    {
        return false;
    }

    const ItemDefinition *pItemDefinition = itemTable.get(lootItem.item.objectDescriptionId);

    if (pItemDefinition == nullptr)
    {
        statusText = "Unavailable.";
        return false;
    }

    switch (operation)
    {
        case ActiveLootOperation::ForceIdentify:
            if (lootItem.item.identified || !ItemRuntime::requiresIdentification(*pItemDefinition))
            {
                statusText = "Already identified.";
                return false;
            }

            lootItem.item.identified = true;
            statusText = "Identified " + ItemRuntime::displayName(lootItem.item, *pItemDefinition) + ".";
            return true;

        case ActiveLootOperation::IdentifyWithSkill:
            if (lootItem.item.identified || !ItemRuntime::requiresIdentification(*pItemDefinition))
            {
                statusText = "Already identified.";
                return false;
            }

            if (pInspector == nullptr || !ItemRuntime::canCharacterIdentifyItem(*pInspector, *pItemDefinition))
            {
                statusText = IdentifyFailedText;
                return false;
            }

            lootItem.item.identified = true;
            statusText = "Identified " + ItemRuntime::displayName(lootItem.item, *pItemDefinition) + ".";
            return true;

        case ActiveLootOperation::RepairWithSkill:
            if (!lootItem.item.broken)
            {
                statusText = "Nothing to repair.";
                return false;
            }

            if (pInspector == nullptr || !ItemRuntime::canCharacterRepairItem(*pInspector, *pItemDefinition))
            {
                statusText = RepairFailedText;
                return false;
            }

            lootItem.item.broken = false;
            lootItem.item.identified = true;
            statusText = "Repaired " + ItemRuntime::displayName(lootItem.item, *pItemDefinition) + ".";
            return true;
    }

    return false;
}

Party *activeParty(GameSession &session)
{
    IGameplayWorldRuntime *pWorldRuntime = session.activeWorldRuntime();
    return pWorldRuntime != nullptr ? pWorldRuntime->party() : nullptr;
}

bool applyWorldItemOperation(
    IGameplayWorldRuntime &worldRuntime,
    size_t worldItemIndex,
    const ItemTable &itemTable,
    ActiveLootOperation operation,
    const Character *pInspector,
    std::string &statusText)
{
    GameplayWorldItemInspectState worldItemState = {};

    if (!worldRuntime.worldItemInspectState(worldItemIndex, worldItemState) || worldItemState.isGold)
    {
        return false;
    }

    GameplayChestItemState lootItem = {};
    lootItem.item = worldItemState.item;

    if (!applyLootOperation(lootItem, itemTable, operation, pInspector, statusText))
    {
        return false;
    }

    return worldRuntime.updateWorldItemInspectState(worldItemIndex, lootItem.item);
}

std::string lowerAscii(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool isNwcDungeonMapName(const std::string &mapFileName)
{
    const std::string normalized = lowerAscii(mapFileName);
    return normalized == NwcDungeonMapName || normalized == std::string("data/games/") + NwcDungeonMapName;
}

bool openDimensionDoorOverlay(GameplayScreenRuntime &runtime)
{
    const Party *pParty = runtime.partyReadOnly();
    const size_t casterMemberIndex = pParty != nullptr ? pParty->activeMemberIndex() : 0;
    runtime.activateDimensionDoor(DimensionDoorActivationSource::ItemScroll);
    if (!runtime.openDimensionDoorOverlay(casterMemberIndex, spellIdValue(SpellId::TownPortal)))
    {
        return false;
    }

    // Item 190 uses the original direct GM10 cast, independently of a learned Town Portal spell.
    GameplayUiController::UtilitySpellOverlayState &overlay = runtime.utilitySpellOverlay();
    overlay.skillLevelOverride = 10;
    overlay.skillMasteryOverride = SkillMastery::Grandmaster;
    overlay.spendMana = false;
    overlay.applyRecovery = false;
    // Source Direct31 resets hidden49 when the selection overlay opens, even if later cancelled.
    if (IGameplayWorldRuntime *world = runtime.worldRuntime(); world != nullptr)
        if (EventRuntimeState *state = world->eventRuntimeState(); state != nullptr)
            state->namedGlobalVars[HiddenCaster49CounterName] = 0;
    return true;
}

void saveTempleInBottleReturnLocation(GameplayScreenRuntime &runtime, GameSession &session)
{
    IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();
    EventRuntimeState *pEventRuntimeState =
        pWorldRuntime != nullptr ? pWorldRuntime->eventRuntimeState() : nullptr;

    if (pWorldRuntime == nullptr || pEventRuntimeState == nullptr)
    {
        return;
    }

    EventRuntimeState::SavedLocation location = {};
    location.x = static_cast<int32_t>(std::lround(runtime.partyX()));
    location.y = static_cast<int32_t>(std::lround(runtime.partyY()));
    location.z = static_cast<int32_t>(std::lround(runtime.partyFootZ()));
    location.continentId = pEventRuntimeState->activeHistoryContinentId;
    location.mapName = runtime.currentMapFileName();
    session.setSessionSavedLocation(TempleInBottleReturnLocationName, std::move(location));
}

InventoryItemUseContext buildInventoryItemUseContext(const GameplayScreenRuntime &runtime)
{
    InventoryItemUseContext context = {};
    const IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();

    if (pWorldRuntime != nullptr)
    {
        context.underwater = pWorldRuntime->isUnderwaterMap();
        context.gameMinutes = pWorldRuntime->gameMinutes();
    }

    return context;
}

void closeCharacterInventoryAfterPotionExplosion(GameplayScreenRuntime &runtime)
{
    GameplayUiController::CharacterScreenState &characterScreen = runtime.characterScreen();
    characterScreen.open = false;
    characterScreen.dollJewelryOverlayOpen = false;
    characterScreen.adventurersInnRosterOverlayOpen = false;
    runtime.closeInventoryNestedOverlay();
    runtime.itemInspectOverlay() = {};
    runtime.characterDetailOverlay() = {};
}

bool tryUseConnectorStone(GameplayScreenRuntime &runtime, GameplayUiController::HeldInventoryItemState &heldItem,
    GameplayUiController &ui)
{
    if (heldItem.item.objectDescriptionId != 624 && heldItem.item.objectDescriptionId != 625) return false;
    IGameplayWorldRuntime *world = runtime.worldRuntime();
    EventRuntimeState *state = world != nullptr ? world->eventRuntimeState() : nullptr;
    Party *party = runtime.party();
    if (state == nullptr || party == nullptr) return true;
    const auto [yellow, red] = EventRuntime::enemyDetectorState(*world);
    const EventDialogContent &dialog = ui.eventDialog().content;
    const bool houseScreen = !runtime.characterScreen().open && dialog.isActive
        && (dialog.isHouseDialog || (state->pendingDialogueContext
            && state->pendingDialogueContext->hostHouseId != 0));
    // Advance the existing persisted RNG only on the source branch which chooses a deadline.
    const bool choosesDeadline = heldItem.item.objectDescriptionId == 624
        && !houseScreen && !yellow && !red;
    const int32_t delay = choosesDeadline ? ConnectorMinimumRechargeTicks
        + int32_t(EventRuntime::randomValue(*state, 624) % (ConnectorMaximumRechargeTicks - ConnectorMinimumRechargeTicks + 1))
        : ConnectorMinimumRechargeTicks;
    const ConnectorUseResult result = useConnectorStone(*party, *state, heldItem.item,
        world->gameMinutes(), houseScreen, yellow || red, delay);
    if (result == ConnectorUseResult::Enemies)
    {
        party->requestSound(static_cast<SoundId>(142));
        const CombatTextTable *text = runtime.combatTextTable();
        const std::string message = text != nullptr
            ? text->format(CombatTextTable::Message::EnemyNearby, {}, {}, 0) : std::string{};
        runtime.setStatusBarEvent(message.empty() ? "Enemies nearby!" : message);
    }
    else if (result == ConnectorUseResult::Called)
    {
        party->setHeldItemForQueries(heldItem.item);
        // Close old presentation first; its close hooks must not erase the newly requested NPC.
        runtime.closeActiveEventDialog();
        ui.closeSpellbook();
        ui.closeUtilitySpellOverlay();
        ui.closeReadableScrollOverlay();
        ui.closeInventoryNestedOverlay();
        runtime.characterScreen() = {};
        runtime.restScreenState() = {};
        runtime.menuScreenState() = {};
        runtime.journalScreenState().active = false;
        runtime.quickReferenceScreenState().active = false;
        runtime.controlsScreenState().active = false;
        runtime.keyboardScreenState().active = false;
        runtime.videoOptionsScreenState().active = false;
        runtime.saveGameScreenState().active = false;
        runtime.loadGameScreenState().active = false;
        party->requestSound(static_cast<SoundId>(157));
        EventRuntimeState::PendingDialogueContext dialogue;
        dialogue.kind = DialogueContextKind::NpcTalk;
        dialogue.sourceId = 803;
        dialogue.npcEnterPending = true;
        state->pendingDialogueContext = std::move(dialogue);
    }
    else if (result == ConnectorUseResult::InvalidState)
        runtime.setStatusBarEvent("Invalid connector state.");
    return true;
}

void spawnPotionExplosionImpactVisual(GameSession &session, GameplayScreenRuntime &runtime)
{
    if (!session.hasDataRepository())
    {
        return;
    }

    GameplayProjectileService &projectileService = session.gameplayProjectileService();
    const ObjectTable &objectTable = session.data().objectTable();
    const std::optional<uint16_t> impactDescriptionId =
        objectTable.findDescriptionIdByObjectId(FireballImpactObjectId);

    if (!impactDescriptionId)
    {
        return;
    }

    const std::optional<GameplayProjectileService::ProjectileImpactVisualDefinition> impactDefinition =
        projectileService.buildProjectileImpactVisualDefinition(*impactDescriptionId, &objectTable, nullptr);

    if (!impactDefinition)
    {
        return;
    }

    const float yawRadians = runtime.gameplayCameraYawRadians();
    const float x = runtime.partyX() + std::cos(yawRadians) * PotionExplosionForwardOffset;
    const float y = runtime.partyY() + std::sin(yawRadians) * PotionExplosionForwardOffset;
    const float z = runtime.partyFootZ() + PotionExplosionHeightOffset;

    runtime.fxService().spawnImmediateSpellImpactVisual(
        *impactDefinition,
        static_cast<int>(spellIdValue(SpellId::Fireball)),
        "Fireball",
        "fire04",
        x,
        y,
        z,
        false,
        false);
}
}

GameplayItemService::GameplayItemService(GameSession &session)
    : m_session(session)
{
}

bool GameplayItemService::tryUseHeldItemOnPartyMember(
    GameplayScreenRuntime &runtime,
    size_t memberIndex,
    bool keepCharacterScreenOpen)
{
    GameplayUiController &uiController = m_session.gameplayUiController();
    GameplayUiController::HeldInventoryItemState &heldItem = uiController.heldInventoryItem();
    Party *pParty = runtime.party();
    const ItemTable *pItemTable = m_session.hasDataRepository() ? &m_session.data().itemTable() : nullptr;
    const ReadableScrollTable *pReadableScrollTable =
        m_session.hasDataRepository() ? &m_session.data().readableScrollTable() : nullptr;
    const MergedPotionSettingTable *pPotionSettingTable =
        m_session.hasDataRepository() ? &m_session.data().mergedPotionSettingTable() : nullptr;
    const MergedReagentSettingTable *pReagentSettingTable =
        m_session.hasDataRepository() ? &m_session.data().mergedReagentSettingTable() : nullptr;

    if (!heldItem.active || pParty == nullptr || pItemTable == nullptr)
    {
        return false;
    }

    if (tryUseConnectorStone(runtime, heldItem, uiController))
    {
        return true;
    }

    const InventoryItemUseResult useResult =
        InventoryItemUseRuntime::useItemOnMember(
            *pParty,
            memberIndex,
            heldItem.item,
            *pItemTable,
            pReadableScrollTable,
            pPotionSettingTable,
            pReagentSettingTable,
            buildInventoryItemUseContext(runtime));

    if (!useResult.handled)
    {
        return false;
    }

    if (useResult.action == InventoryItemUseAction::CastScroll)
    {
        if (!useResult.consumed)
        {
            if (runtime.audioSystem() != nullptr)
            {
                runtime.audioSystem()->playCommonSound(SoundId::Error, GameAudioSystem::PlaybackGroup::Ui);
            }
        }
        else
        {
            if (heldItem.item.objectDescriptionId == DimensionDoorScrollItemId)
            {
                if (openDimensionDoorOverlay(runtime))
                {
                    heldItem = {};
                    pParty->clearHeldItemForQueries();
                }

                GameplayUiController::CharacterScreenState &characterScreen =
                    m_session.gameplayScreenState().characterScreen();
                characterScreen.open = false;
                characterScreen.dollJewelryOverlayOpen = false;
                return true;
            }

            const SpellTable *pSpellTable = m_session.hasDataRepository() ? &m_session.data().spellTable() : nullptr;

            if (pSpellTable == nullptr)
            {
                runtime.setStatusBarEvent("Spell data missing");
                return true;
            }

            const SpellEntry *pSpellEntry = pSpellTable->findById(useResult.spellId);

            if (pSpellEntry == nullptr)
            {
                runtime.setStatusBarEvent("Unknown scroll spell");
                return true;
            }

            PartySpellCastRequest request = {};
            request.casterMemberIndex = memberIndex;
            request.spellId = useResult.spellId;
            request.skillLevelOverride = useResult.spellSkillLevelOverride;
            request.skillMasteryOverride = useResult.spellSkillMasteryOverride;
            request.bypassRequiredMastery = useResult.spellBypassRequiredMastery;
            request.spendMana = false;
            request.applyRecovery = true;

            if (!runtime.tryCastSpellRequest(request, pSpellEntry->name))
            {
                return true;
            }

            heldItem = {};
            pParty->clearHeldItemForQueries();
        }
    }
    else if (useResult.action == InventoryItemUseAction::UseDimensionDoorScroll)
    {
        if (useResult.consumed && openDimensionDoorOverlay(runtime))
        {
            heldItem = {};
            pParty->clearHeldItemForQueries();
        }

        GameplayUiController::CharacterScreenState &characterScreen =
            m_session.gameplayScreenState().characterScreen();
        characterScreen.open = false;
        characterScreen.dollJewelryOverlayOpen = false;
        return true;
    }
    else if (useResult.action == InventoryItemUseAction::ReadMessageScroll)
    {
        GameplayUiController::ReadableScrollOverlayState &overlay = uiController.readableScrollOverlay();
        overlay = {};
        overlay.active = true;
        overlay.title = useResult.readableTitle;
        overlay.body = useResult.readableBody;
    }
    else
    {
        if (useResult.consumed)
        {
            heldItem = {};
            pParty->clearHeldItemForQueries();
        }

        if (useResult.action == InventoryItemUseAction::ConsumePotion
            && useResult.consumed
            && runtime.audioSystem() != nullptr)
        {
            runtime.audioSystem()->playCommonSound(SoundId::Drink, GameAudioSystem::PlaybackGroup::Ui);
            runtime.triggerPortraitFaceAnimation(memberIndex, FaceAnimationId::DrinkPotion);
        }

        if (useResult.action == InventoryItemUseAction::UseHorseshoe && useResult.consumed)
        {
            m_session.gameplayFxService().triggerPortraitEventFxWithoutSpeech(
                runtime,
                memberIndex,
                PortraitFxEventKind::QuestComplete);
        }
        else if (useResult.action == InventoryItemUseAction::UseGenieLamp && useResult.consumed)
        {
            m_session.gameplayFxService().triggerPortraitEventFxWithoutSpeech(
                runtime,
                memberIndex,
                PortraitFxEventKind::QuestComplete);
        }
        else if (useResult.action == InventoryItemUseAction::LearnSpell
                 && !useResult.consumed
                 && useResult.alreadyKnown
                 && runtime.audioSystem() != nullptr)
        {
            runtime.audioSystem()->playCommonSound(SoundId::Error, GameAudioSystem::PlaybackGroup::Ui);
        }

        if (useResult.speechId.has_value())
        {
            runtime.playSpeechReaction(memberIndex, *useResult.speechId, true);
        }

        if (useResult.soundId.has_value() && runtime.audioSystem() != nullptr)
        {
            runtime.audioSystem()->playCommonSound(*useResult.soundId, GameAudioSystem::PlaybackGroup::Ui);
        }

        if (useResult.action == InventoryItemUseAction::UseTempleInABottle)
        {
            if (!isNwcDungeonMapName(m_session.currentMapFileName()))
            {
                saveTempleInBottleReturnLocation(runtime, m_session);
            }

            EventRuntimeState::PendingMapMove pendingMapMove = {};
            pendingMapMove.mapName = NwcDungeonMapName;
            pendingMapMove.directionDegrees = 0;
            pendingMapMove.lookAngleDegrees = 0;
            m_session.setPendingMapMove(std::move(pendingMapMove));
        }
    }

    if (!useResult.statusText.empty())
    {
        runtime.setStatusBarEvent(useResult.statusText);
    }

    const bool closeCharacterScreen =
        !keepCharacterScreenOpen
        || (useResult.action == InventoryItemUseAction::CastScroll && useResult.consumed)
        || useResult.action == InventoryItemUseAction::UseTempleInABottle;

    if (closeCharacterScreen)
    {
        GameplayUiController::CharacterScreenState &characterScreen = m_session.gameplayScreenState().characterScreen();
        characterScreen.open = false;
        characterScreen.dollJewelryOverlayOpen = false;
    }

    return true;
}

bool GameplayItemService::tryUseHeldItemOnInventoryItem(
    GameplayScreenRuntime &runtime,
    size_t memberIndex,
    uint8_t targetGridX,
    uint8_t targetGridY)
{
    GameplayUiController::HeldInventoryItemState &heldItem = m_session.gameplayUiController().heldInventoryItem();
    Party *pParty = runtime.party();
    const ItemTable *pItemTable = m_session.hasDataRepository() ? &m_session.data().itemTable() : nullptr;
    const PotionMixingTable *pPotionMixingTable =
        m_session.hasDataRepository() ? &m_session.data().potionMixingTable() : nullptr;
    const MergedPotionSettingTable *pPotionSettingTable =
        m_session.hasDataRepository() ? &m_session.data().mergedPotionSettingTable() : nullptr;
    const MergedReagentSettingTable *pReagentSettingTable =
        m_session.hasDataRepository() ? &m_session.data().mergedReagentSettingTable() : nullptr;
    const PotionNoteTable *pPotionNoteTable =
        m_session.hasDataRepository() ? &m_session.data().potionNoteTable() : nullptr;

    if (!heldItem.active
        || pParty == nullptr
        || pItemTable == nullptr
        || pPotionMixingTable == nullptr
        || pPotionSettingTable == nullptr
        || pReagentSettingTable == nullptr
        || pPotionNoteTable == nullptr)
    {
        return false;
    }

    const InventoryItemMixResult mixResult = InventoryItemMixingRuntime::tryApplyHeldItemToInventoryItem(
        *pParty,
        memberIndex,
        heldItem.item,
        targetGridX,
        targetGridY,
        *pItemTable,
        *pPotionMixingTable,
        *pPotionSettingTable,
        *pReagentSettingTable,
        pPotionNoteTable);

    if (!mixResult.handled)
    {
        return false;
    }

    if (mixResult.heldItemReplacement.has_value())
    {
        heldItem.item = *mixResult.heldItemReplacement;
        heldItem.grabCellOffsetX = 0;
        heldItem.grabCellOffsetY = 0;
        heldItem.grabOffsetX = 0.0f;
        heldItem.grabOffsetY = 0.0f;
        pParty->setHeldItemForQueries(heldItem.item);
    }
    else if (mixResult.heldItemConsumed)
    {
        heldItem = {};
        pParty->clearHeldItemForQueries();
    }

    if (mixResult.unlockedAutonoteId != 0)
    {
        IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();
        EventRuntimeState *pEventRuntimeState =
            pWorldRuntime != nullptr ? pWorldRuntime->eventRuntimeState() : nullptr;

        if (pEventRuntimeState != nullptr)
        {
            const uint32_t rawId = (mixResult.unlockedAutonoteId << 16) | AutoNoteVariableTag;
            const EventRuntime::VariableRef variable = EventRuntime::decodeVariable(rawId);
            const std::vector<size_t> targetMemberIndices = {memberIndex};
            EventRuntime::setVariableValue(*pEventRuntimeState, variable, 1, pParty, targetMemberIndices);
        }
    }

    const bool potionExplosion =
        mixResult.action == InventoryItemMixAction::PotionMix
        && !mixResult.success
        && mixResult.failureDamageLevel != 0;

    if (potionExplosion)
    {
        pParty->applyPotionExplosionToMember(memberIndex, mixResult.failureDamageLevel);
        closeCharacterInventoryAfterPotionExplosion(runtime);
        spawnPotionExplosionImpactVisual(m_session, runtime);
    }

    if (runtime.audioSystem() != nullptr)
    {
        if (mixResult.success && mixResult.action == InventoryItemMixAction::PotionMix)
        {
            runtime.audioSystem()->playCommonSound(SoundId::MixPotion, GameAudioSystem::PlaybackGroup::Ui);
        }
        else if (potionExplosion)
        {
            runtime.audioSystem()->playCommonSound(SoundId::Fireball, GameAudioSystem::PlaybackGroup::Ui);
        }
        else if (!mixResult.success)
        {
            runtime.audioSystem()->playCommonSound(SoundId::Error, GameAudioSystem::PlaybackGroup::Ui);
        }
    }

    if (mixResult.action == InventoryItemMixAction::PotionMix
        || mixResult.action == InventoryItemMixAction::ReagentBottleMix)
    {
        Character *pMember = pParty->member(memberIndex);
        const bool canPlayPotionReaction =
            mixResult.success
            || !potionExplosion
            || (pMember != nullptr && GameMechanics::canAct(*pMember));

        if (canPlayPotionReaction)
        {
            runtime.triggerPortraitFaceAnimation(
                memberIndex,
                mixResult.success ? FaceAnimationId::MixPotion : FaceAnimationId::PotionExplode);
            runtime.playSpeechReaction(
                memberIndex,
                mixResult.success ? SpeechId::PotionSuccess : SpeechId::PotionFail,
                true);
        }
    }
    else if (mixResult.action == InventoryItemMixAction::RechargePotion && !mixResult.success)
    {
        runtime.playSpeechReaction(memberIndex, SpeechId::PotionFail, true);
    }

    if (!mixResult.statusText.empty())
    {
        runtime.setStatusBarEvent(mixResult.statusText);
    }

    return true;
}

void GameplayItemService::updateReadableScrollOverlayForHeldItem(
    size_t memberIndex,
    const GameplayCharacterPointerTarget &pointerTarget,
    bool isLeftMousePressed)
{
    GameplayUiController &uiController = m_session.gameplayUiController();
    GameplayUiController::ReadableScrollOverlayState &overlay = uiController.readableScrollOverlay();
    overlay = {};

    const ItemTable *pItemTable = m_session.hasDataRepository() ? &m_session.data().itemTable() : nullptr;
    Party *pParty = activeParty(m_session);
    const GameplayUiController::HeldInventoryItemState &heldItem = uiController.heldInventoryItem();
    const ReadableScrollTable *pReadableScrollTable =
        m_session.hasDataRepository() ? &m_session.data().readableScrollTable() : nullptr;

    if (!isLeftMousePressed
        || !heldItem.active
        || pItemTable == nullptr
        || pParty == nullptr
        || (pointerTarget.type != GameplayCharacterPointerTargetType::EquipmentSlot
            && pointerTarget.type != GameplayCharacterPointerTargetType::DollPanel))
    {
        return;
    }

    const InventoryItemUseAction useAction =
        InventoryItemUseRuntime::classifyItemUse(
            heldItem.item,
            *pItemTable,
            &m_session.data().mergedPotionSettingTable(),
            &m_session.data().mergedReagentSettingTable());

    if (useAction != InventoryItemUseAction::ReadMessageScroll)
    {
        return;
    }

    const InventoryItemUseResult useResult =
        InventoryItemUseRuntime::useItemOnMember(
            *pParty,
            memberIndex,
            heldItem.item,
            *pItemTable,
            pReadableScrollTable,
            &m_session.data().mergedPotionSettingTable(),
            &m_session.data().mergedReagentSettingTable());

    if (!useResult.handled || useResult.action != InventoryItemUseAction::ReadMessageScroll)
    {
        return;
    }

    overlay.active = true;
    overlay.title = useResult.readableTitle;
    overlay.body = useResult.readableBody;
}

bool GameplayItemService::identifyInspectedItem(
    const GameplayUiController::ItemInspectOverlayState &overlay,
    std::string &statusText)
{
    statusText.clear();

    Party *pParty = activeParty(m_session);
    IGameplayWorldRuntime *pWorldRuntime = m_session.activeWorldRuntime();
    const ItemTable *pItemTable = m_session.hasDataRepository() ? &m_session.data().itemTable() : nullptr;

    if (pParty == nullptr || pItemTable == nullptr)
    {
        return false;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Inventory)
    {
        return pParty->identifyMemberInventoryItem(
            overlay.sourceMemberIndex,
            overlay.sourceGridX,
            overlay.sourceGridY,
            statusText);
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Equipment)
    {
        return pParty->identifyEquippedItem(
            overlay.sourceMemberIndex,
            overlay.sourceEquipmentSlot,
            statusText);
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Chest && pWorldRuntime != nullptr)
    {
        GameplayChestViewState *pChestView = pWorldRuntime->activeChestView();

        if (pChestView == nullptr || overlay.sourceLootItemIndex >= pChestView->items.size())
        {
            return false;
        }

        if (!applyLootOperation(
                pChestView->items[overlay.sourceLootItemIndex],
                *pItemTable,
                ActiveLootOperation::ForceIdentify,
                nullptr,
                statusText))
        {
            return false;
        }

        pWorldRuntime->commitActiveChestView();
        return true;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Corpse && pWorldRuntime != nullptr)
    {
        GameplayCorpseViewState *pCorpseView = pWorldRuntime->activeCorpseView();

        if (pCorpseView == nullptr || overlay.sourceLootItemIndex >= pCorpseView->items.size())
        {
            return false;
        }

        if (!applyLootOperation(
                pCorpseView->items[overlay.sourceLootItemIndex],
                *pItemTable,
                ActiveLootOperation::ForceIdentify,
                nullptr,
                statusText))
        {
            return false;
        }

        pWorldRuntime->commitActiveCorpseView();
        return true;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::WorldItem && pWorldRuntime != nullptr)
    {
        return applyWorldItemOperation(
            *pWorldRuntime,
            overlay.sourceWorldItemIndex,
            *pItemTable,
            ActiveLootOperation::ForceIdentify,
            nullptr,
            statusText);
    }

    return false;
}

bool GameplayItemService::tryIdentifyInspectedItem(
    const GameplayUiController::ItemInspectOverlayState &overlay,
    size_t inspectorMemberIndex,
    std::string &statusText)
{
    statusText.clear();

    Party *pParty = activeParty(m_session);
    IGameplayWorldRuntime *pWorldRuntime = m_session.activeWorldRuntime();
    const ItemTable *pItemTable = m_session.hasDataRepository() ? &m_session.data().itemTable() : nullptr;

    if (pParty == nullptr || pItemTable == nullptr)
    {
        return false;
    }

    const Character *pRequestedInspector = pParty->member(inspectorMemberIndex);
    const Character *pInspector = pParty->bestPartyWideUtilitySkillMember("IdentifyItem");

    if (pRequestedInspector == nullptr)
    {
        return false;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Inventory)
    {
        return pParty->tryIdentifyMemberInventoryItem(
            overlay.sourceMemberIndex,
            overlay.sourceGridX,
            overlay.sourceGridY,
            inspectorMemberIndex,
            statusText);
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Equipment)
    {
        return pParty->tryIdentifyEquippedItem(
            overlay.sourceMemberIndex,
            overlay.sourceEquipmentSlot,
            inspectorMemberIndex,
            statusText);
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Chest && pWorldRuntime != nullptr)
    {
        GameplayChestViewState *pChestView = pWorldRuntime->activeChestView();

        if (pChestView == nullptr || overlay.sourceLootItemIndex >= pChestView->items.size())
        {
            return false;
        }

        if (!applyLootOperation(
                pChestView->items[overlay.sourceLootItemIndex],
                *pItemTable,
                ActiveLootOperation::IdentifyWithSkill,
                pInspector,
                statusText))
        {
            return false;
        }

        pWorldRuntime->commitActiveChestView();
        return true;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Corpse && pWorldRuntime != nullptr)
    {
        GameplayCorpseViewState *pCorpseView = pWorldRuntime->activeCorpseView();

        if (pCorpseView == nullptr || overlay.sourceLootItemIndex >= pCorpseView->items.size())
        {
            return false;
        }

        if (!applyLootOperation(
                pCorpseView->items[overlay.sourceLootItemIndex],
                *pItemTable,
                ActiveLootOperation::IdentifyWithSkill,
                pInspector,
                statusText))
        {
            return false;
        }

        pWorldRuntime->commitActiveCorpseView();
        return true;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::WorldItem && pWorldRuntime != nullptr)
    {
        return applyWorldItemOperation(
            *pWorldRuntime,
            overlay.sourceWorldItemIndex,
            *pItemTable,
            ActiveLootOperation::IdentifyWithSkill,
            pInspector,
            statusText);
    }

    return false;
}

bool GameplayItemService::tryRepairInspectedItem(
    const GameplayUiController::ItemInspectOverlayState &overlay,
    size_t inspectorMemberIndex,
    std::string &statusText)
{
    statusText.clear();

    Party *pParty = activeParty(m_session);
    IGameplayWorldRuntime *pWorldRuntime = m_session.activeWorldRuntime();
    const ItemTable *pItemTable = m_session.hasDataRepository() ? &m_session.data().itemTable() : nullptr;

    if (pParty == nullptr || pItemTable == nullptr)
    {
        return false;
    }

    const Character *pRequestedInspector = pParty->member(inspectorMemberIndex);
    const Character *pInspector = pParty->bestPartyWideUtilitySkillMember("RepairItem");

    if (pRequestedInspector == nullptr)
    {
        return false;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Inventory)
    {
        return pParty->tryRepairMemberInventoryItem(
            overlay.sourceMemberIndex,
            overlay.sourceGridX,
            overlay.sourceGridY,
            inspectorMemberIndex,
            statusText);
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Equipment)
    {
        return pParty->tryRepairEquippedItem(
            overlay.sourceMemberIndex,
            overlay.sourceEquipmentSlot,
            inspectorMemberIndex,
            statusText);
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Chest && pWorldRuntime != nullptr)
    {
        GameplayChestViewState *pChestView = pWorldRuntime->activeChestView();

        if (pChestView == nullptr || overlay.sourceLootItemIndex >= pChestView->items.size())
        {
            return false;
        }

        if (!applyLootOperation(
                pChestView->items[overlay.sourceLootItemIndex],
                *pItemTable,
                ActiveLootOperation::RepairWithSkill,
                pInspector,
                statusText))
        {
            return false;
        }

        pWorldRuntime->commitActiveChestView();
        return true;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::Corpse && pWorldRuntime != nullptr)
    {
        GameplayCorpseViewState *pCorpseView = pWorldRuntime->activeCorpseView();

        if (pCorpseView == nullptr || overlay.sourceLootItemIndex >= pCorpseView->items.size())
        {
            return false;
        }

        if (!applyLootOperation(
                pCorpseView->items[overlay.sourceLootItemIndex],
                *pItemTable,
                ActiveLootOperation::RepairWithSkill,
                pInspector,
                statusText))
        {
            return false;
        }

        pWorldRuntime->commitActiveCorpseView();
        return true;
    }

    if (overlay.sourceType == GameplayUiController::ItemInspectSourceType::WorldItem && pWorldRuntime != nullptr)
    {
        return applyWorldItemOperation(
            *pWorldRuntime,
            overlay.sourceWorldItemIndex,
            *pItemTable,
            ActiveLootOperation::RepairWithSkill,
            pInspector,
            statusText);
    }

    return false;
}

void GameplayItemService::closeReadableScrollOverlay()
{
    m_session.gameplayUiController().closeReadableScrollOverlay();
}
}
