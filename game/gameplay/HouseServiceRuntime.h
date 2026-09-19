#pragma once

#include "game/tables/HouseTable.h"
#include "game/gameplay/HouseInteraction.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct InventoryItem;
struct ItemDefinition;
class ItemTable;
class IGameplayWorldRuntime;
class HouseServiceTextTable;
class Party;
class StandardItemEnchantTable;
class SpecialItemEnchantTable;
struct EventRuntimeState;

enum class HouseStockMode : uint8_t
{
    ShopStandard = 0,
    ShopSpecial,
    GuildSpellbooks
};

class HouseServiceRuntime
{
public:
    enum class ShopItemServiceResult
    {
        None,
        Success,
        NoItem,
        Unavailable,
        WrongShop,
        AlreadyIdentified,
        NothingToRepair,
        NotEnoughGold,
        InventoryFull,
        Stolen,
        TheftCaught,
        Failed,
    };

    static bool supportsGeneratedStock(const HouseEntry &houseEntry);
    static bool supportsEquipmentSell(const HouseEntry &houseEntry);
    static bool supportsIdentify(const HouseEntry &houseEntry);
    static bool supportsRepair(const HouseEntry &houseEntry);
    static size_t slotCountForStockMode(const HouseEntry &houseEntry, HouseStockMode mode);
    static const std::vector<InventoryItem> &ensureStock(
        Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        float gameMinutes,
        HouseStockMode mode);
    static int buyPrice(
        const Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        const InventoryItem &item,
        int effectiveReputation = 0);
    static int sellPrice(
        const Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        const InventoryItem &item,
        int effectiveReputation = 0);
    static bool canSellItemToHouse(const ItemTable &itemTable, const HouseEntry &houseEntry, const InventoryItem &item);
    static std::string buildBuyHoverText(
        const Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        const InventoryItem &item,
        int effectiveReputation = 0,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static std::string buildSellHoverText(
        const Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        const InventoryItem &item,
        int effectiveReputation = 0,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static std::string buildIdentifyHoverText(
        const Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        const InventoryItem &item,
        int effectiveReputation = 0,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static std::string buildRepairHoverText(
        const Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        const InventoryItem &item,
        int effectiveReputation = 0,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static bool tryBuyStockItem(
        Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        float gameMinutes,
        HouseStockMode mode,
        size_t slotIndex,
        std::string &statusText,
        ShopItemServiceResult *pResult = nullptr,
        int effectiveReputation = 0,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static bool tryStealStockItem(
        Party &party,
        IGameplayWorldRuntime &worldRuntime,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        float gameMinutes,
        HouseStockMode mode,
        size_t slotIndex,
        uint32_t successRoll,
        uint32_t caughtRoll,
        std::string &statusText,
        ShopItemServiceResult *pResult = nullptr,
        int effectiveReputation = 0);
    static bool trySellInventoryItem(
        Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        std::string &statusText,
        ShopItemServiceResult *pResult = nullptr,
        int effectiveReputation = 0,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static bool tryIdentifyInventoryItem(
        Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        std::string &statusText,
        ShopItemServiceResult *pResult = nullptr,
        int effectiveReputation = 0,
        const EventRuntimeState *pEventRuntimeState = nullptr,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
    static bool tryRepairInventoryItem(
        Party &party,
        const ItemTable &itemTable,
        const StandardItemEnchantTable &standardItemEnchantTable,
        const SpecialItemEnchantTable &specialItemEnchantTable,
        const HouseEntry &houseEntry,
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        std::string &statusText,
        ShopItemServiceResult *pResult = nullptr,
        int effectiveReputation = 0,
        const EventRuntimeState *pEventRuntimeState = nullptr,
        const HouseServiceTextTable *pHouseServiceTextTable = nullptr);
};
}
