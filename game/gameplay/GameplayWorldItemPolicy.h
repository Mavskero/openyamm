#pragma once

#include "game/maps/MapItemSourceData.h"

#include <string>

namespace OpenYAMM::Game
{
class Party;
class ItemTable;
class UiTextTable;
struct InventoryItem;

bool worldItemPolicyAvailable(const MapWorldItemSource &source, const Party &party);
bool applyWorldItemPolicyActions(
    const MapWorldItemSource &source,
    const InventoryItem &randomPoolItem,
    Party &party,
    const ItemTable &itemTable);
std::string formatWorldItemPickupStatusText(
    const InventoryItem &item, const ItemTable &itemTable, const UiTextTable &uiText);
}
