#include "SupportResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/AssetFileSystem.h"
#include "engine/TextTable.h"
#include "game/items/InventoryItemMixingRuntime.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/PotionMixingTable.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string trim(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \r\n\t");
    return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \r\n\t") - first + 1);
}

int32_t number(const std::string &text, int32_t minimum = 0, int32_t maximum = 65535)
{
    const std::string value = trim(text);
    int32_t result = 0;
    const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size()
        && result >= minimum && result <= maximum, "Invalid support numeric field: " + text);
    return result;
}

bool marker(const std::string &text)
{
    require(text.empty() || text == "-" || text == "x" || text == "X", "Unsupported interface marker");
    return text == "x" || text == "X";
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Support input escapes root");
    return path;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid support quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 != table->getRowCount() || row != Row{""})
        {
            rows.push_back(row);
        }
    }
    return rows;
}

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid support dependency size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read support dependency");
    if (legacy)
    {
        size_t glyphs = 0;
        text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
    }
    return parse(text);
}

void checkReagentMix(const Game::ItemTable &items, const Game::MergedPotionSettingTable &potions,
    const Game::MergedReagentSettingTable &reagents, uint32_t reagentId, uint32_t resultId, bool reverse)
{
    using namespace Game;
    const uint32_t bottleId = potions.emptyBottleItemId();
    const uint32_t targetId = reverse ? reagentId : bottleId;
    const uint32_t heldId = reverse ? bottleId : reagentId;
    Party party;
    party.setItemTable(&items);
    Character member;
    member.name = "炼金检查";
    member.className = "Knight";
    member.health = member.maxHealth = 40;
    member.skills["Alchemy"] = {"Alchemy", 10, SkillMastery::Grandmaster};
    PartySeed seed;
    seed.members.push_back(member);
    party.seed(seed);
    InventoryItem target;
    target.objectDescriptionId = targetId;
    target.width = target.height = target.quantity = 1;
    target.standardEnchantPower = 123;
    require(party.tryAutoPlaceItemInMemberInventory(0, target), "Cannot prepare reagent mixture");
    InventoryItem held;
    held.objectDescriptionId = heldId;
    held.width = held.height = held.quantity = 1;
    const PotionMixingTable emptyMixing;
    const InventoryItemMixResult result = InventoryItemMixingRuntime::tryApplyHeldItemToInventoryItem(
        party, 0, held, 0, 0, items, emptyMixing, potions, reagents);
    const InventoryItem *pAfter = party.memberInventoryItemMutable(0, 0, 0);
    const uint32_t expectedPower = std::min(65535, 10 + number(items.get(reagentId)->mod1));
    require(result.handled && result.success && result.action == InventoryItemMixAction::ReagentBottleMix
        && result.heldItemConsumed && result.targetItemChanged && !result.targetItemRemoved
        && !result.heldItemReplacement && pAfter && pAfter->objectDescriptionId == resultId
        && pAfter->standardEnchantPower == expectedPower && pAfter->identified && pAfter->quantity == 1
        && held.objectDescriptionId == heldId && held.quantity == 1,
        "Reagent mixing changed the recipe, power or consumption contract");
}
}

void convertSupportResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Support output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Support output already exists");
    const auto sourceRows = [&root](const std::string &name) { return readRows(root, "Data/Tables/" + name, true); };
    const auto assetRows = [&assets](const std::string &name)
    {
        return readRows(assets, "engine/data_tables/" + name, false);
    };
    const Rows portraits = sourceRows("MonPortraits.txt");
    const Rows reagents = sourceRows("Reagent settings.txt");
    const Rows interfaces = sourceRows("Additional UI.txt");
    require(portraits.size() > 1 && portraits[0] == Row{"#", "Pic index", "Note"}
        && reagents.size() > 1 && reagents[0] == Row{"Reagent Id", "Item Id", "Result item id", "Notes"},
        "Unsupported portrait/reagent header");
    require(interfaces.size() > 1 && interfaces[0] == Row{"#", "Lod name", "D Lod name",
        "Show blank hostile indicator", "Hostile indicator Y", "Hostile indicator X offset",
        "Selring on top", "Selring Y", "Selring X offset", "Notes"}, "Unsupported additional interface header");
    Engine::AssetFileSystem fileSystem;
    ItemTable items;
    MonsterTable monsters;
    MergedPotionSettingTable potions;
    require(items.load(fileSystem, assetRows("items.txt"), {})
        && monsters.loadStatsFromRows(assetRows("monster_data.txt"))
        && potions.loadFromRows(assetRows("potion_settings.txt")), "Invalid support registry dependency");
    MergedMonsterPortraitTable portraitTable;
    MergedReagentSettingTable reagentTable;
    MergedAdditionalUiTable interfaceTable;
    require(portraitTable.loadFromRows(portraits) && reagentTable.loadFromRows(reagents)
        && interfaceTable.loadFromRows(interfaces), "Support typed loading failed");
    std::set<uint32_t> pictureIds;
    size_t portraitChoices = 0;
    size_t zeroGroups = 0;
    size_t portraitChecks = 0;
    for (size_t index = 1; index < portraits.size(); ++index)
    {
        const Row &row = portraits[index];
        require(row.size() == 3 && number(row[0], 1, 10922) == index,
            "Monster portrait group differs from source row order");
        std::vector<uint32_t> choices;
        std::istringstream parts(row[1]);
        std::string part;
        require(!row[1].empty() && row[1].back() != ',', "Empty monster portrait choice");
        while (std::getline(parts, part, ','))
        {
            choices.push_back(number(part, 0, 9999));
            if (choices.back() != 0)
            {
                pictureIds.insert(choices.back());
            }
        }
        require(choices.size() == 1 || std::find(choices.begin(), choices.end(), 0) == choices.end(),
            "Mixed zero/nonzero portrait choices need explicit semantics");
        zeroGroups += choices == std::vector<uint32_t>{0};
        portraitChoices += choices.size() - (choices == std::vector<uint32_t>{0} ? 1 : 0);
        for (uint32_t tier = 0; tier < 3; ++tier)
        {
            const uint32_t monsterId = (index - 1) * 3 + tier + 1;
            require(monsters.findStatsById(monsterId), "Portrait group references missing monster stats");
            for (size_t choice = 0; choice < choices.size(); ++choice)
            {
                const std::optional<uint32_t> expected = choices[choice] == 0
                    ? std::nullopt : std::optional<uint32_t>(choices[choice]);
                require(portraitTable.portraitForMonsterId(monsterId, choice) == expected,
                    "Monster portrait choice order or ID changed");
                ++portraitChecks;
            }
        }
    }
    std::set<uint32_t> reagentItems;
    for (size_t index = 1; index < reagents.size(); ++index)
    {
        const Row &row = reagents[index];
        require(row.size() == 4 && number(row[0], 1) == index, "Reagent identity differs from source row order");
        const uint32_t itemId = number(row[1], 1);
        const uint32_t resultId = number(row[2], 1);
        const ItemDefinition *pItem = items.get(itemId);
        const ItemDefinition *pResult = items.get(resultId);
        require(reagentItems.insert(itemId).second && pItem && pResult && potions.getByItemId(resultId)
            && (pItem->equipStat == "Reagent" || pItem->equipStat == "Herb") && pResult->equipStat == "Bottle",
            "Invalid/duplicate reagent item or unregistered potion result");
        const MergedReagentSettingEntry &entry = reagentTable.entries().at(index - 1);
        require(entry.reagentId == index && entry.itemId == itemId && entry.resultItemId == resultId
            && entry.note == trim(row[3]) && reagentTable.resultItemIdForReagent(itemId) == resultId,
            "Reagent identity or result changed");
        checkReagentMix(items, potions, reagentTable, itemId, resultId, false);
        checkReagentMix(items, potions, reagentTable, itemId, resultId, true);
    }
    std::set<std::string> archiveNames;
    size_t availableArchives = 0;
    for (size_t index = 1; index < interfaces.size(); ++index)
    {
        const Row &row = interfaces[index];
        require(row.size() == 10 && number(row[0], 1) == index, "Interface identity differs from source row order");
        const MergedAdditionalUiEntry &entry = interfaceTable.entries().at(index - 1);
        require(entry.id == index && entry.lodName == row[1] && entry.dLodName == row[2]
            && entry.showBlankHostileIndicator == marker(row[3]) && entry.selectionRingOnTop == marker(row[6])
            && entry.hostileIndicatorY == number(row[4], -32768, 32767)
            && entry.hostileIndicatorXOffset == number(row[5], -32768, 32767)
            && entry.selectionRingY == number(row[7], -32768, 32767)
            && entry.selectionRingXOffset == number(row[8], -32768, 32767) && entry.notes == trim(row[9]),
            "Interface metadata changed");
        for (const std::string &name : {row[1], row[2]})
        {
            if (name.empty() || name == "default")
            {
                continue;
            }
            require(name.find_first_of("/\\\r\n\t") == std::string::npos && name.ends_with(".lod"),
                "Invalid interface archive name");
            if (archiveNames.insert(name).second && std::filesystem::exists(root / "Data/Additional UI" / name))
            {
                require(std::filesystem::is_regular_file(checkedPath(root, "Data/Additional UI/" + name)),
                    "Invalid interface archive file");
                ++availableArchives;
            }
        }
    }
    require(portraitTable.groupCount() == portraits.size() - 1 && reagentTable.entries().size() == reagents.size() - 1
        && interfaceTable.entries().size() == interfaces.size() - 1, "Support loader lost records");
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"support_tables\",\"runtime_ready\":false,\"portrait_groups\":"
        << portraitTable.groupCount() << ",\"portrait_choices\":" << portraitChoices
        << ",\"zero_portrait_groups\":" << zeroGroups << ",\"distinct_portrait_ids\":" << pictureIds.size()
        << ",\"portrait_selection_checks\":" << portraitChecks << ",\"reagents\":" << reagentItems.size()
        << ",\"reagent_mix_checks\":" << reagentItems.size() * 2 << ",\"interface_sets\":" << interfaces.size() - 1
        << ",\"declared_interface_archives\":" << archiveNames.size()
        << ",\"present_interface_archive_files\":" << availableArchives
        << ",\"typed_validation\":true,\"portrait_images_exported\":0,\"ui_images_exported\":0"
        << ",\"ui_art_validated\":false,\"portrait_rendering_validated\":false,\"alchemy_ui_validated\":false"
        << ",\"original_alchemy_power_parity\":false,\"full_gameplay_loop\":false}\n";
    const std::map<std::string, Rows> tables = {{"monster_portraits.txt", portraits},
        {"reagent_settings.txt", reagents}, {"additional_ui.txt", interfaces}};
    std::map<std::string, std::string> files;
    for (const auto &[name, rows] : tables)
    {
        const std::string text = serializeTextRows(rows);
        require(parse(text) == rows, "Support serialization changed source cells");
        files.emplace("engine/data_tables/" + name, text);
    }
    files.emplace("support_conversion.json", report.str());
    require(std::filesystem::create_directory(output), "Cannot create fresh support output");
    try
    {
        for (const auto &[name, text] : files)
        {
            writeConvertedResource(output / name, std::vector<uint8_t>(text.begin(), text.end()));
        }
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
