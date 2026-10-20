#include "game/gameplay/GameplayHeldItemController.h"

#include "game/audio/SoundIds.h"
#include "game/events/EventRuntime.h"
#include "game/debug/GameplayDebugTrace.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/items/ItemGenerator.h"
#include "game/items/ItemRuntime.h"
#include "game/party/Party.h"
#include "game/tables/ItemTable.h"

#include <optional>
#include <string>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
void forceEventGrantedItemIdentificationState(InventoryItem &item, const ItemTable &itemTable)
{
    const ItemDefinition *pItemDefinition = itemTable.get(item.objectDescriptionId);

    if (pItemDefinition != nullptr && ItemRuntime::requiresIdentification(*pItemDefinition))
    {
        item.identified = false;
    }
}

std::string eventGrantedItemStatusName(const InventoryItem &item, const ItemTable &itemTable)
{
    const ItemDefinition *pItemDefinition = itemTable.get(item.objectDescriptionId);

    if (pItemDefinition == nullptr)
    {
        return "item";
    }

    if (!pItemDefinition->unidentifiedName.empty()
        && pItemDefinition->unidentifiedName != "0"
        && pItemDefinition->unidentifiedName != "N / A")
    {
        return pItemDefinition->unidentifiedName;
    }

    return !pItemDefinition->name.empty() ? pItemDefinition->name : "item";
}

void notifyEventGrantedItemReceived(
    GameplayScreenRuntime &runtime,
    const InventoryItem &item,
    const ItemTable &itemTable)
{
    Party *pParty = runtime.party();

    if (pParty != nullptr)
    {
        pParty->requestSound(SoundId::Gold);
    }

    runtime.setStatusBarEvent("You found something (" + eventGrantedItemStatusName(item, itemTable) + ")!");
}
} // namespace

void GameplayHeldItemController::setHeldInventoryItem(
    GameplayUiController::HeldInventoryItemState &heldInventoryItem,
    const InventoryItem &item)
{
    GAMEPLAY_DEBUG_TRACE(
        "held_item_changed active=true item_id=" + std::to_string(item.objectDescriptionId)
        + " quantity=" + std::to_string(item.quantity)
        + " grid=(" + std::to_string(item.gridX) + "," + std::to_string(item.gridY) + ")"
        + " source=held_controller");
    heldInventoryItem.active = true;
    heldInventoryItem.item = item;
    heldInventoryItem.grabCellOffsetX = 0;
    heldInventoryItem.grabCellOffsetY = 0;
    heldInventoryItem.grabOffsetX = 0.0f;
    heldInventoryItem.grabOffsetY = 0.0f;
}

void GameplayHeldItemController::clearHeldInventoryItem(
    GameplayUiController::HeldInventoryItemState &heldInventoryItem)
{
    if (heldInventoryItem.active)
    {
        GAMEPLAY_DEBUG_TRACE(
            "held_item_changed active=false item_id=" + std::to_string(heldInventoryItem.item.objectDescriptionId)
            + " source=held_controller");
    }

    heldInventoryItem = {};
}

bool GameplayHeldItemController::tryDisplaceHeldInventoryItem(
    GameplayScreenRuntime &runtime, bool sourceTakeOrDrop)
{
    GameplayUiController::HeldInventoryItemState &heldInventoryItem = runtime.heldInventoryItem();

    if (!heldInventoryItem.active)
    {
        return true;
    }

    Party *pParty = runtime.party();

    if (pParty != nullptr && pParty->tryGrantInventoryItem(heldInventoryItem.item))
    {
        GAMEPLAY_DEBUG_TRACE(
            "item_received destination=inventory source=held_displace item_id="
            + std::to_string(heldInventoryItem.item.objectDescriptionId)
            + gameplayDebugTraceItemSummary(heldInventoryItem.item.objectDescriptionId, runtime.itemTable()));
        clearHeldInventoryItem(heldInventoryItem);
        pParty->clearHeldItemForQueries();
        return true;
    }

    IGameplayWorldRuntime *pWorldRuntime = runtime.worldRuntime();
    const ItemTable *pItemTable = runtime.itemTable();
    const ItemDefinition *pItemDefinition =
        pItemTable != nullptr ? pItemTable->get(heldInventoryItem.item.objectDescriptionId) : nullptr;

    if (pWorldRuntime == nullptr
        || (!sourceTakeOrDrop && pItemDefinition != nullptr && !ItemRuntime::canDrop(*pItemDefinition)))
    {
        return false;
    }

    std::optional<GameplayHeldItemDropRequest> dropRequest = pWorldRuntime->buildHeldItemDropRequest();

    if (!dropRequest)
    {
        return false;
    }

    dropRequest->item = heldInventoryItem.item;

    if (!pWorldRuntime->dropHeldItemToWorld(*dropRequest))
    {
        return false;
    }

    clearHeldInventoryItem(heldInventoryItem);
    if (pParty != nullptr)
    {
        pParty->clearHeldItemForQueries();
    }
    return true;
}

void GameplayHeldItemController::applyPendingQuestItemDeliveries(GameplayScreenRuntime &runtime)
{
    Party *pParty = runtime.party();
    const ItemTable *pTable = runtime.itemTable();
    if (pParty == nullptr || pTable == nullptr)
    {
        return;
    }
    while (!pParty->pendingItemDeliveries().empty())
    {
        const Party::PendingItemDelivery delivery = pParty->pendingItemDeliveries().front();
        if (pTable->get(delivery.item.objectDescriptionId) == nullptr)
        {
            return;
        }
        if (delivery.recipientMemberIndex >= 0
            && pParty->tryGrantInventoryItemToMember(size_t(delivery.recipientMemberIndex), delivery.item))
        {
            pParty->completeFirstPendingItemDelivery();
            notifyEventGrantedItemReceived(runtime, delivery.item, *pTable);
            continue;
        }
        if (!tryDisplaceHeldInventoryItem(runtime, delivery.allowDropProtectedHeldItem))
        {
            return; // Party retains the new item; the failed old held displacement retains its owner.
        }
        setHeldInventoryItem(runtime.heldInventoryItem(), delivery.item);
        pParty->setHeldItemForQueries(delivery.item);
        pParty->completeFirstPendingItemDelivery();
        notifyEventGrantedItemReceived(runtime, delivery.item, *pTable);
    }
}

void GameplayHeldItemController::applyGrantedEventItemsToHeldInventory(
    GameplayScreenRuntime &runtime,
    EventRuntimeState &eventRuntimeState,
    const ItemTable &itemTable)
{
    if (runtime.party() == nullptr)
    {
        return;
    }

    GameplayUiController::HeldInventoryItemState &heldInventoryItem = runtime.heldInventoryItem();

    if (eventRuntimeState.clearHeldItemRequest)
    {
        clearHeldInventoryItem(heldInventoryItem);
        runtime.party()->clearHeldItemForQueries();
        eventRuntimeState.clearHeldItemRequest = false;
    }

    applyPendingQuestItemDeliveries(runtime);
    if (eventRuntimeState.grantedItems.empty() && eventRuntimeState.grantedItemIds.empty())
    {
        return;
    }

    const auto receiveGrantedItem =
        [&runtime, &itemTable, &heldInventoryItem](InventoryItem grantedItem)
        {
            forceEventGrantedItemIdentificationState(grantedItem, itemTable);

#if defined(__ANDROID__)
            Party *pParty = runtime.party();
            size_t recipientMemberIndex = 0;

            if (pParty != nullptr && pParty->tryGrantInventoryItem(grantedItem, &recipientMemberIndex))
            {
                notifyEventGrantedItemReceived(runtime, grantedItem, itemTable);
                GAMEPLAY_DEBUG_TRACE(
                    "item_received destination=inventory source=event item_id="
                    + std::to_string(grantedItem.objectDescriptionId)
                    + gameplayDebugTraceItemSummary(grantedItem.objectDescriptionId, &itemTable)
                    + " member_index=" + std::to_string(recipientMemberIndex));
                return true;
            }
#endif

            if (!GameplayHeldItemController::tryDisplaceHeldInventoryItem(runtime))
            {
                return false;
            }

            GameplayHeldItemController::setHeldInventoryItem(heldInventoryItem, grantedItem);
            runtime.party()->setHeldItemForQueries(grantedItem);
            notifyEventGrantedItemReceived(runtime, grantedItem, itemTable);
            GAMEPLAY_DEBUG_TRACE(
                "item_received destination=held source=event item_id="
                + std::to_string(grantedItem.objectDescriptionId)
                + gameplayDebugTraceItemSummary(grantedItem.objectDescriptionId, &itemTable));
            return true;
        };

    std::vector<InventoryItem> undeliveredItems;
    std::vector<uint32_t> undeliveredItemIds;
    for (const InventoryItem &item : eventRuntimeState.grantedItems)
    {
        if (item.objectDescriptionId == 0)
        {
            continue;
        }

        if (!receiveGrantedItem(item)) undeliveredItems.push_back(item);
    }

    for (uint32_t itemId : eventRuntimeState.grantedItemIds)
    {
        if (itemId == 0)
        {
            continue;
        }

        InventoryItem item = ItemGenerator::makeInventoryItem(itemId, itemTable, ItemGenerationMode::Generic);
        if (!receiveGrantedItem(std::move(item))) undeliveredItemIds.push_back(itemId);
    }

    eventRuntimeState.grantedItems = std::move(undeliveredItems);
    eventRuntimeState.grantedItemIds = std::move(undeliveredItemIds);
}

bool GameplayHeldItemController::tryAutoPlaceHeldInventoryItemOnPartyMember(
    GameplayUiController::HeldInventoryItemState &heldInventoryItem,
    Party &party,
    size_t memberIndex,
    std::string &failureStatus)
{
    failureStatus.clear();

    if (!heldInventoryItem.active)
    {
        return false;
    }

    if (!party.tryAutoPlaceItemInMemberInventory(memberIndex, heldInventoryItem.item))
    {
        failureStatus = party.lastStatus().empty() || party.lastStatus() == "inventory full"
            ? "Pack is Full!"
            : party.lastStatus();
        return false;
    }

    clearHeldInventoryItem(heldInventoryItem);
    party.clearHeldItemForQueries();
    return true;
}
} // namespace OpenYAMM::Game
