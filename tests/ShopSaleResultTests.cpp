#include "doctest/doctest.h"
#include "engine/AssetFileSystem.h"
#include "game/gameplay/HouseServiceRuntime.h"
#include "game/items/ItemEnchantTables.h"
#include "game/party/Party.h"
#include "game/tables/ItemTable.h"

using namespace OpenYAMM::Game;

TEST_CASE("sale result retains the sold item's identification after backpack compaction")
{
    OpenYAMM::Engine::AssetFileSystem files;
    std::vector<std::string> row(31);
    row[0] = "1";
    row[2] = "Named sword";
    row[3] = "100";
    row[4] = "Weapon";
    row[5] = "Sword";
    row[10] = "Unknown sword";
    row[29] = "1";
    row[30] = "1";
    ItemTable table;
    REQUIRE(table.load(files, {row}, {}));
    StandardItemEnchantTable standard;
    SpecialItemEnchantTable special;
    HouseEntry shop;
    shop.id = 1;
    shop.type = "Weapon Shop";
    shop.priceMultiplier = 2.0f;

    for (bool identified : {false, true})
    {
        INFO("sold identification: " << identified);
        Party party;
        party.setItemTable(&table);
        party.setItemEnchantTables(&standard, &special);
        Party::Snapshot snapshot;
        snapshot.gold = 200;
        snapshot.members.resize(1);
        Character &member = snapshot.members.front();
        member.className = "Knight";
        member.health = member.maxHealth = 40;
        InventoryItem sold;
        sold.objectDescriptionId = 1;
        sold.quantity = 1;
        sold.identified = identified;
        InventoryItem retained = sold;
        retained.gridX = 1;
        retained.identified = !identified;
        member.inventory = {sold, retained};
        party.restoreSnapshot(snapshot);
        std::string status;
        HouseServiceRuntime::ShopItemServiceResult result;
        REQUIRE(HouseServiceRuntime::trySellInventoryItem(
            party, table, standard, special, shop, 0, 0, 0, status, &result));
        CHECK(result == HouseServiceRuntime::ShopItemServiceResult::Success);
        CHECK(party.gold() == 225);
        REQUIRE(party.members().front().inventory.size() == 1);
        CHECK(party.members().front().inventory.front().gridX == 1);
        CHECK(party.members().front().inventory.front().identified == !identified);
        CHECK(status == std::string("Sold ") + (identified ? "Named sword" : "Unknown sword") + " for 25 gold.");
    }
}
