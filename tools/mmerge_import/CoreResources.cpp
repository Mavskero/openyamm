#include "CoreResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/MapStats.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/CombatTextTable.h"
#include "game/tables/HouseServiceTextTable.h"
#include "game/tables/UiTextTable.h"

#include <charconv>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
using IndexedRows = std::map<uint32_t, Row>;

uint32_t integer(const std::string &text, uint32_t limit = 65535)
{
    uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() || value > limit)
    {
        throw std::runtime_error("Invalid core table integer: " + text);
    }
    return value;
}

bool numeric(const std::string &text)
{
    return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
}

std::string lower(std::string text)
{
    for (char &letter : text)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return text;
}

std::vector<uint8_t> readLoose(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const auto relative = path.lexically_relative(root);
    if (relative.empty() || *relative.begin() == ".." || relative.is_absolute())
    {
        throw std::runtime_error("Source file escapes import root: " + name);
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size > 16 * 1024 * 1024)
    {
        throw std::runtime_error("Core source text exceeds size limit: " + name);
    }
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    if (!stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read core source text: " + name);
    }
    return bytes;
}

Rows parse(const std::vector<uint8_t> &bytes, bool chinese, size_t &glyphs)
{
    const auto table = Engine::TextTable::parseTabSeparated(decodeLegacyText(bytes, chinese, glyphs));
    if (!table || table->getRowCount() == 0)
    {
        throw std::runtime_error("Invalid or empty core text table");
    }
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (!row.empty() && !(row.size() == 1 && row[0].empty()))
        {
            rows.push_back(row);
        }
    }
    return rows;
}

IndexedRows indexed(const Rows &rows, size_t columns, const std::string &name)
{
    IndexedRows result;
    for (const Row &row : rows)
    {
        if (row.empty() || !numeric(row[0]))
        {
            continue;
        }
        if (row.size() != columns || !result.emplace(integer(row[0]), row).second)
        {
            throw std::runtime_error("Wrong column count or duplicate ID in " + name + ": " + row[0]);
        }
    }
    if (result.empty())
    {
        throw std::runtime_error("No numeric records in " + name);
    }
    return result;
}

Rows values(const IndexedRows &table)
{
    Rows rows;
    for (const auto &[id, row] : table)
    {
        rows.push_back(row);
    }
    return rows;
}

void requireHeader(const Rows &rows, const std::string &header)
{
    const auto expected = Engine::TextTable::parseTabSeparated(header);
    if (rows.empty() || rows.front() != expected->getRow(0))
    {
        throw std::runtime_error("Unrecognized core source table header");
    }
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}

// MonList uses byte fields in the supplied MMExtension structure. Preserve its
// fixed-width storage conversion (including legacy 999 values), and report it.
uint32_t color(const Row &row, size_t first, bool bgr, size_t &wrappedChannels)
{
    uint32_t channels[4] = {};
    for (size_t channel = 0; channel < 4; ++channel)
    {
        const uint32_t value = integer(row[first + channel]);
        wrappedChannels += value > 255 ? 1 : 0;
        channels[channel] = value & 255;
    }
    return bgr ? channels[0] | channels[1] << 8 | channels[2] << 16 | channels[3] << 24
        : channels[2] | channels[1] << 8 | channels[0] << 16 | channels[3] << 24;
}

Rows deathDrops(const std::vector<uint8_t> &bytes, const IndexedRows &monsters, const IndexedRows &items)
{
    const std::string script(bytes.begin(), bytes.end());
    // Recognize the complete supported drop handler, not arbitrary Lua or a
    // partial formula. Other handlers in this script remain outside this table.
    const std::regex prefix(
        R"(^local random, floor, ceil = math\.random, math\.floor, math\.ceil\s+local MonsterItems = \{([^}]+)\}\s+)"
        R"(function events\.MonsterKilled\(mon\)\s+if mon\.Ally == 9999 then -- no drop from reanimated monsters\s+)"
        R"(return\s+end\s+local monKind = ceil\(mon\.Id/3\)\s+local Mul = mon\.Id - monKind\*3 \+ 3\s+)"
        R"(local ItemId = MonsterItems\[monKind\]\s+if ItemId and random\(100\) \+ Mul\*5 > 80 then\s+)"
        R"(local t = \{MonsterIndex = mon:GetIndex\(\), Monster = mon, ItemId = ItemId, Handled = false\}\s+)"
        R"(events\.Call\("MonsterDropItem", t\)\s+if not t\.Handled then\s+)"
        R"(evt\.SummonObject\(ItemId, mon\.X, mon\.Y, mon\.Z \+ 100, 100\)\s+end\s+end\s+end\s*)");
    std::smatch match;
    if (!std::regex_search(script, match, prefix))
    {
        throw std::runtime_error("Unsupported MonsterItems drop handler; no Lua was executed");
    }
    std::string entries = match[1];
    const std::regex entry(R"(^\s*\[([0-9]+)\]\s*=\s*([0-9]+)\s*,?\s*)");
    std::set<uint32_t> families;
    Rows rows;
    while (!entries.empty())
    {
        if (!std::regex_search(entries, match, entry))
        {
            throw std::runtime_error("Unsupported MonsterItems mapping");
        }
        const uint32_t family = integer(match[1]);
        const uint32_t item = integer(match[2]);
        if (family == 0 || !families.insert(family).second || !items.contains(item))
        {
            throw std::runtime_error("Invalid MonsterItems family/item reference");
        }
        for (uint32_t rank = 1; rank <= 3; ++rank)
        {
            const uint32_t id = (family - 1) * 3 + rank;
            if (!monsters.contains(id))
            {
                throw std::runtime_error("Drop references absent monster ID: " + std::to_string(id));
            }
            rows.push_back({std::to_string(id), monsters.at(id)[1], std::to_string(item), items.at(item)[2],
                std::to_string(20 + rank * 5), "MonsterItems.lua authored additional drop"});
        }
        entries = match.suffix();
    }
    return rows;
}
}

void convertCoreResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    if (relative.empty() || (!relative.is_absolute() && *relative.begin() != ".."))
    {
        throw std::runtime_error("Core output must be outside the original source");
    }
    if (std::filesystem::exists(destination))
    {
        throw std::runtime_error("Core destination already exists");
    }
    size_t glyphs = 0;
    const auto base = root / "Data/mmmerge.T.lod";
    const auto localized = root / "Data/10 LocZHCN.EnglishT.lod";
    const bool chinese = std::filesystem::exists(localized);
    const auto archiveTable = [&](const std::string &name, bool translated = false)
    {
        return parse(readLodText(translated ? localized : base, name), translated, glyphs);
    };
    const auto looseTable = [&](const std::string &name, bool translated = false)
    {
        return parse(readLoose(root, name), translated, glyphs);
    };
    std::map<std::string, Rows> tables;
    const IndexedRows globalText = indexed(archiveTable("global.txt", chinese), 2, "global.txt");
    using UiMessage = Game::UiTextTable::Message;
    constexpr std::array<std::pair<UiMessage, uint32_t>, 38> uiTextSources{{
        {UiMessage::Cancel, 34},
        {UiMessage::BeaconSet, 375},
        {UiMessage::BeaconRecall, 523},
        {UiMessage::BeaconAvailable, 19},
        {UiMessage::Day, 56},
        {UiMessage::Days, 57},
        {UiMessage::Hour, 109},
        {UiMessage::Hours, 110},
        {UiMessage::Minute, 437},
        {UiMessage::Minutes, 436},
        {UiMessage::ItemTypeFormat, 463},
        {UiMessage::ItemStolen, 187},
        {UiMessage::SpellSpCost, 522},
        {UiMessage::ItemPower, 449},
        {UiMessage::ItemAttack, 18},
        {UiMessage::ItemShoot, 203},
        {UiMessage::ItemDamage, 53},
        {UiMessage::ItemCharges, 464},
        {UiMessage::ItemArmor, 11},
        {UiMessage::ItemValue, 465},
        {UiMessage::Congratulations, 9},
        {UiMessage::TotalTime, 37},
        {UiMessage::ScoreFormat, 94},
        {UiMessage::CharacterFormat, 129},
        {UiMessage::Year, 245},
        {UiMessage::Years, 132},
        {UiMessage::Month, 146},
        {UiMessage::Months, 148},
        {UiMessage::BankBalance, 25},
        {UiMessage::BankDeposit, 60},
        {UiMessage::BankWithdraw, 244},
        {UiMessage::BankHowMuch, 112},
        {UiMessage::RestHealEightHours, 183},
        {UiMessage::RestWaitWithoutHealing, 236},
        {UiMessage::RestWaitUntilDawn, 237},
        {UiMessage::RestWaitFiveMinutes, 238},
        {UiMessage::RestWaitOneHour, 239},
        {UiMessage::RestExit, 81},
    }};
    // These captions and the MM8 ending paragraph have no equivalents in the selected GlobalTxt.
    // Their Chinese text is a project translation, recorded separately in the manifest.
    constexpr std::array<std::pair<UiMessage, const char *>, 7> projectUiTranslations{{
        {UiMessage::EmptySlot, "空槽"}, {UiMessage::Menu, "菜单"}, {UiMessage::Submit, "提交"},
        {UiMessage::ArcomageResign, "认输"},
        {UiMessage::ArcomageTouchHint, "轻点出牌，上划弃牌"},
        {UiMessage::ArcomageDiscardHint, "请选择要弃掉的牌"},
        {UiMessage::WinEnding,
            "干得好！你挫败了世界毁灭者，将你的世界从永恒毁灭的边缘拯救了回来。"
            "生命得以延续，贾达姆也迎来了新的和平。"
            "你缔结的强大联盟将确保这片土地得到重建，并最终走向繁荣。"},
    }};
    const Game::UiTextTable englishUiText;
    Rows uiText;
    for (const auto &[message, translation] : projectUiTranslations)
    {
        uiText.push_back({Game::UiTextTable::key(message), chinese ? translation : englishUiText.text(message)});
    }
    for (const auto &[message, id] : uiTextSources)
    {
        if (!globalText.contains(id))
        {
            throw std::runtime_error("Missing UI text in global.txt: " + std::to_string(id));
        }
        uiText.push_back({Game::UiTextTable::key(message), globalText.at(id)[1]});
    }
    Game::UiTextTable uiTextCheck;
    if (!uiTextCheck.loadFromRows(uiText))
    {
        throw std::runtime_error("Unsupported UI text in global.txt conversion");
    }
    const size_t uiTextCount = uiText.size();
    tables["ui_text.txt"] = std::move(uiText);
    const std::array<std::pair<uint32_t, const char *>, 6> combatTextIds{{
        {164, "combat.melee_hit"}, {189, "combat.projectile_hit"}, {175, "combat.kill"},
        {635, "combat.stun"}, {636, "combat.paralyze"}, {480, "combat.enemy_nearby"}}};
    Rows combatText;
    for (const auto &[id, key] : combatTextIds)
    {
        if (!globalText.contains(id))
        {
            throw std::runtime_error("Missing combat text in global.txt: " + std::to_string(id));
        }
        combatText.push_back({key, globalText.at(id)[1]});
    }
    Game::CombatTextTable combatTextCheck;
    if (!combatTextCheck.loadFromRows(combatText))
    {
        throw std::runtime_error("Unsupported combat text argument signature in global.txt");
    }
    tables["combat_text.txt"] = combatText;

    constexpr std::array<uint32_t, 41> houseServiceSourceIds{
        56, 57, 68, 86, 97, 104, 113, 128, 134, 140, 152, 155, 159, 160, 168, 178, 179,
        195, 196, 197, 198, 199, 200, 404, 405, 414, 446, 448, 527, 529, 536, 537, 538,
        540, 541, 542, 561, 563, 569, 570, 663};
    for (uint32_t id : houseServiceSourceIds)
    {
        if (!globalText.contains(id))
        {
            throw std::runtime_error("Missing house service text in global.txt: " + std::to_string(id));
        }
    }

    const std::string &donate = globalText.at(68)[1];
    const std::string &gold = globalText.at(97)[1];
    const std::string &heal = globalText.at(104)[1];
    Rows houseServiceText{
        {"temple.heal_action", heal + (chinese ? "（{price} " : " {price} ") + gold + (chinese ? "）" : "")},
        {"temple.donate_action", donate + (chinese ? "（{price} " : " {price} ") + gold + (chinese ? "）" : "")},
        {"temple.learn_skills_action", globalText.at(160)[1]},
        {"temple.learn_skill_action", chinese ? "学习{skill}（{price} 金币）" : "Learn {skill} for {price} gold"},
        {"temple.no_skills", chinese
            ? "这里没有适合该角色学习的技能。"
            : "No skills are available here for this character."},
        {"temple.heal_unavailable", chinese ? "这里不提供治疗服务。" : "Healing is not offered here."},
        {"temple.no_character", chinese ? "未选择角色。" : "No character is selected."},
        {"temple.cannot_treat", chinese
            ? "神殿人员无法治疗{member}的当前状态。"
            : "The temple staff cannot treat {member}'s condition."},
        {"temple.already_well", chinese
            ? "神殿人员说{member}已经很健康。"
            : "The temple staff says {member} is already well."},
        {"temple.heal_insufficient_gold", chinese
            ? "治疗需要{price}金币。"
            : "You need {price} gold for healing."},
        {"temple.heal_success", chinese
            ? "神殿已为{member}恢复健康，花费{price}金币。"
            : "The temple restores {member} for {price} gold."},
        {"temple.donate_unavailable", chinese ? "这里不接受捐赠。" : "Donations are not accepted here."},
        {"temple.donate_insufficient_gold", chinese
            ? "在这里捐赠需要{price}金币。"
            : "You need {price} gold to donate here."},
        {"temple.donate_success", globalText.at(527)[1]},
        {"temple.lesson_unavailable", chinese ? "这里不提供该课程。" : "That lesson is not available."},
        {"temple.cannot_learn", chinese
            ? "{member}无法在这里学习{skill}。"
            : "{member} cannot learn {skill} here."},
        {"temple.learn_insufficient_gold", globalText.at(155)[1]},
        {"temple.learn_success", chinese
            ? "{member}学习了{skill}，花费{price}金币。"
            : "{member} learns {skill} for {price} gold."},
        {"common.closed_hours", chinese
            ? "此处在{open}到{close}开放"
            : "This place is open from {open} to {close}"},
        {"common.not_enough_gold", globalText.at(155)[1]},
        {"tavern.rent_action", chinese
            ? "花费{price}个金币租房间"
            : "Rent Room for {price} gold"},
        {"tavern.food_action", chinese
            ? "购买{food}天的食物，花费{price}个金币"
            : "Fill Packs to {food} days for {price} gold"},
        {"tavern.packs_full", globalText.at(140)[1]},
        {"tavern.rent_insufficient_gold", chinese
            ? "租房间需要{price}个金币。"
            : "You need {price} gold to rent a room."},
        {"tavern.food_insufficient_gold", chinese
            ? "购买食物需要{price}个金币。"
            : "You need {price} gold for provisions."},
        {"tavern.food_success", chinese
            ? "旅店老板把你的食物补充到{food}天。"
            : "The innkeeper fills your packs to {food} days."},
        {"training.action", chinese ? "训练" : "Train"},
        {"training.action_with_level_price", chinese
            ? "训练至等级{level}，花费{price}个金币"
            : "Train to level {level} for {price} gold"},
        {"training.max_level", chinese
            ? "依你的水平，都可以在此当老师了。对不起，我们不能训练你。"
            : "With your skills, you should be working here as a teacher. Sorry, but we are unable to train you."},
        {"training.experience_shortfall", chinese
            ? "你还需要{experience}点经验值才能训练至等级{level}"
            : "You need {experience} more experience to train to level {level}"},
        {"training.no_character", chinese ? "未选择要训练的角色。" : "No character is selected for training."},
        {"training.insufficient_gold", chinese
            ? "训练需要{price}个金币。"
            : "You need {price} gold for training."},
        {"training.unavailable", chinese ? "现在无法训练。" : "Training is not available right now."},
        {"training.success", chinese
            ? "{member}已升至{level}级，并获得{points}个技能点！"
            : "{member} is now level {level} and has earned {points} skill points!"},
        {"shop.buy_standard_action", globalText.at(134)[1]},
        {"shop.buy_special_action", globalText.at(152)[1]},
        {"shop.display_equipment_action", globalText.at(159)[1]},
        {"shop.sell_action", globalText.at(200)[1]},
        {"shop.identify_action", globalText.at(113)[1]},
        {"shop.repair_action", globalText.at(179)[1]},
        {"shop.select_buy_standard", globalText.at(195)[1]},
        {"shop.select_buy_special", globalText.at(196)[1]},
        {"shop.select_sell", globalText.at(199)[1]},
        {"shop.select_identify", globalText.at(197)[1]},
        {"shop.select_repair", globalText.at(198)[1]},
        {"shop.nothing_for_sale", chinese ? "该位置没有出售的物品。" : "Nothing is for sale in that slot."},
        {"shop.unavailable", chinese ? "该物品不可用。" : "That item is unavailable."},
        {"shop.inventory_full", globalText.at(563)[1]},
        {"shop.bought", chinese
            ? "购买了{item}，花费{price}个金币。"
            : "Bought {item} for {price} gold."},
        {"shop.no_item", chinese ? "该位置没有物品。" : "No item there."},
        {"shop.cannot_sell", chinese ? "该物品不能出售。" : "That item cannot be sold."},
        {"shop.sold", chinese
            ? "出售了{item}，获得{price}个金币。"
            : "Sold {item} for {price} gold."},
        {"shop.already_identified", chinese ? "该物品已经鉴定。" : "That item is already identified."},
        {"shop.wrong_identify_shop", chinese
            ? "这家商店不能鉴定该物品。"
            : "This shop cannot identify that item."},
        {"shop.identify_failed", globalText.at(446)[1]},
        {"shop.identify_success", globalText.at(569)[1]},
        {"shop.nothing_to_repair", chinese ? "该物品无需修理。" : "That item does not need repair."},
        {"shop.wrong_repair_shop", chinese
            ? "这家商店不能修理该物品。"
            : "This shop cannot repair that item."},
        {"shop.repair_failed", globalText.at(448)[1]},
        {"shop.repair_success", globalText.at(570)[1]},
        {"shop.buy_hover", chinese
            ? "购买{item}需要{price}个金币。"
            : "Buy {item} for {price} gold."},
        {"shop.sell_hover", chinese
            ? "出售{item}可得{price}个金币。"
            : "Sell {item} for {price} gold."},
        {"shop.identify_hover", chinese
            ? "鉴定{item}需要{price}个金币。"
            : "Identify {item} for {price} gold."},
        {"shop.repair_hover", chinese
            ? "修理{item}需要{price}个金币。"
            : "Repair {item} for {price} gold."},
        {"transport.route_action_single", chinese
            ? "用{days}天到{destination}，旅费{price}个金币"
            : "{days} day to {destination} for {price} gold"},
        {"transport.route_action_plural", chinese
            ? "用{days}天到{destination}，旅费{price}个金币"
            : "{days} days to {destination} for {price} gold"},
        {"transport.no_route_today", globalText.at(561)[1]},
        {"transport.route_unavailable", chinese ? "该路线不可用。" : "That route is not available."},
        {"transport.travel_unavailable", chinese ? "现在无法旅行。" : "Travel is unavailable right now."},
        {"transport.insufficient_gold", chinese
            ? "旅行需要{price}个金币。"
            : "You need {price} gold for travel."},
        {"transport.success_single", chinese
            ? "要用{days}天才能到达{destination}。"
            : "It will take {days} day to travel to {destination}."},
        {"transport.success_plural", chinese
            ? "要用{days}天才能到达{destination}。"
            : "It will take {days} days to travel to {destination}."},
    };
    Game::HouseServiceTextTable houseServiceTextCheck;
    if (!houseServiceTextCheck.loadFromRows(houseServiceText))
    {
        throw std::runtime_error("Unsupported house service template in global.txt conversion");
    }
    const size_t houseServiceTextCount = houseServiceText.size();
    tables["house_service_text.txt"] = std::move(houseServiceText);
    IndexedRows maps = indexed(archiveTable("mapstats.txt"), 34, "mapstats.txt");
    const IndexedRows translatedMaps = chinese
        ? indexed(archiveTable("mapstats.txt", true), 34, "localized mapstats") : IndexedRows{};
    const IndexedRows bolsterMaps = indexed(looseTable("Data/Tables/Bolster - maps.txt"), 10, "bolster maps");
    const IndexedRows continents = indexed(looseTable("Data/Tables/Continent settings.txt"), 23, "continents");
    const std::map<std::string, std::string> continentWorlds = {
        {"Jadam", "mm8"}, {"Antagarich", "mm7"}, {"Enroth", "mm6"}, {"Breach", "mmmerge"}};
    std::map<std::string, std::string> mapOwners;
    std::set<std::string> patchedMaps;
    for (const std::string world : {"mm6", "mm7", "mm8", "breach", "patch"})
    {
        const auto archive = root / "Data" / (world + ".games.lod");
        if (!std::filesystem::exists(archive))
        {
            continue;
        }
        for (const std::string &member : listLodMembers(archive))
        {
            if (!member.ends_with(".odm") && !member.ends_with(".blv"))
            {
                continue;
            }
            if (world == "patch")
            {
                patchedMaps.insert(member);
            }
            else if (!mapOwners.emplace(member, world == "breach" ? "mmmerge" : world).second)
            {
                throw std::runtime_error("Ambiguous map archive ownership: " + member);
            }
        }
    }
    std::ostringstream missingMaps;
    size_t missingCount = 0;
    size_t mapTranslations = 0;
    size_t mapTranslationMismatches = 0;
    std::set<std::string> registeredMaps;
    for (auto &[id, row] : maps)
    {
        const std::string file = lower(row[2]);
        if (file.empty() || file.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-") != std::string::npos
            || (!file.ends_with(".odm") && !file.ends_with(".blv")) || !registeredMaps.insert(file).second)
        {
            throw std::runtime_error("Invalid map file name");
        }
        std::string world;
        if (mapOwners.contains(file))
        {
            world = mapOwners.at(file);
        }
        else
        {
            const auto map = bolsterMaps.find(id);
            if (map == bolsterMaps.end() || !continents.contains(integer(map->second[2])))
            {
                throw std::runtime_error("Cannot establish missing map continent: " + file);
            }
            const std::string name = continents.at(integer(map->second[2]))[1];
            if (!continentWorlds.contains(name))
            {
                throw std::runtime_error("Unknown continent: " + name);
            }
            world = continentWorlds.at(name);
            if (!patchedMaps.contains(file))
            {
                missingMaps << (missingCount++ == 0 ? "" : ",") << "{\"id\":" << id
                    << ",\"file\":" << std::quoted(file) << ",\"world\":" << std::quoted(world) << "}";
            }
        }
        if (translatedMaps.contains(id))
        {
            if (lower(translatedMaps.at(id)[2]) == file)
            {
                row[1] = translatedMaps.at(id)[1];
                ++mapTranslations;
            }
            else
            {
                ++mapTranslationMismatches;
            }
        }
        row.push_back(world);
    }
    tables["map_stats.txt"] = values(maps);

    IndexedRows monsters = indexed(archiveTable("monsters.txt"), 39, "monsters.txt");
    size_t monsterTranslations = 0;
    std::set<uint32_t> unusedMonsterTranslations;
    std::set<uint32_t> conflictingMonsterTranslations;
    if (chinese)
    {
        const Rows translations = looseTable("Data/Text localization/ZHCN_Monsters.txt", true);
        requireHeader(translations, "Table (of Game struct)\tId\tField\tNew text");
        std::map<uint32_t, std::string> names;
        for (size_t index = 1; index < translations.size(); ++index)
        {
            const Row &row = translations[index];
            if (row.size() != 4 || row[0] != "MonstersTxt" || row[2] != "Name")
            {
                throw std::runtime_error("Unsupported monster name overlay");
            }
            const uint32_t id = integer(row[1]);
            if (!names.emplace(id, row[3]).second && names.at(id) != row[3])
            {
                conflictingMonsterTranslations.insert(id);
            }
        }
        for (const auto &[id, name] : names)
        {
            if (conflictingMonsterTranslations.contains(id))
            {
                continue;
            }
            if (monsters.contains(id))
            {
                monsters.at(id)[1] = name;
                ++monsterTranslations;
            }
            else
            {
                unusedMonsterTranslations.insert(id);
            }
        }
    }
    tables["monster_data.txt"] = values(monsters);
    const Rows monList = looseTable("Data/Tables/MonList.txt");
    requireHeader(monList, "#\tHeight\tRadius\tVelocity\tRadius2\tTintB\tTintG\tTintR\tTintA\tSoundAttack"
        "\tSoundDie\tSoundGetHit\tSoundFidget\tName\tFramesStand\tFramesWalk\tFramesAttack\tFramesShoot"
        "\tFramesStun\tFramesDie\tFramesDead\tFramesFidget");
    Rows descriptors;
    size_t wrappedTintChannels = 0;
    for (size_t index = 1; index < monList.size(); ++index)
    {
        const Row &row = monList[index];
        if (row.size() != 22 || integer(row[0]) != index - 1 || !monsters.contains(index)
            || lower(monsters.at(index)[2]) != lower(row[13]))
        {
            throw std::runtime_error("MonList index/picture does not match the merged monster ID");
        }
        Row converted = {std::to_string(index), row[13], row[1], row[2], row[3], row[4],
            std::to_string(color(row, 5, true, wrappedTintChannels))};
        for (size_t column = 1; column <= 4; ++column)
        {
            integer(row[column], column == 4 ? 32767 : 65535);
        }
        for (size_t column = 9; column <= 12; ++column)
        {
            integer(row[column]);
        }
        converted.insert(converted.end(), row.begin() + 9, row.begin() + 13);
        for (size_t column = 14; column < 22; ++column)
        {
            converted.push_back(lower(row[column]));
        }
        descriptors.push_back(converted);
    }
    if (descriptors.size() != monsters.size())
    {
        throw std::runtime_error("Monster stats/descriptors have different coverage");
    }
    tables["monster_descriptors.txt"] = descriptors;
    IndexedRows uniqueNames = indexed(archiveTable("placemon.txt"), 2, "placemon.txt");
    if (chinese)
    {
        for (const auto &[id, row] : indexed(archiveTable("placemon.txt", true), 2, "localized placemon"))
        {
            if (uniqueNames.contains(id))
            {
                uniqueNames.at(id)[1] = row[1];
            }
        }
    }
    tables["english/place_mon.txt"] = values(uniqueNames);
    const Rows hostile = archiveTable("hostile.txt");
    if (hostile.size() < 2 || hostile.front().size() != hostile.size())
    {
        throw std::runtime_error("Hostile matrix is not square");
    }
    for (size_t index = 1; index < hostile.size(); ++index)
    {
        if (hostile[index].size() != hostile.size() || hostile[index][0] != hostile[0][index])
        {
            throw std::runtime_error("Hostile matrix row/column labels disagree");
        }
        for (size_t column = 1; column < hostile.size(); ++column)
        {
            if (!hostile[index][column].empty())
            {
                integer(hostile[index][column], 4);
            }
        }
    }
    tables["hostile.txt"] = hostile;
    tables["monster_death_drops.txt"] = deathDrops(readLoose(root, "Scripts/General/MonsterItems.lua"),
        monsters, indexed(archiveTable("items.txt"), 17, "items.txt"));

    const Rows frames = looseTable("Data/Tables/SFT.txt");
    requireHeader(frames, "#\tGroupName\tSpriteName\tScale\tLuminous\tGroupStart\tImage1\tCenter\tFidget"
        "\tLoaded\tMirror0\tMirror1\tMirror2\tMirror3\tMirror4\tMirror5\tMirror6\tMirror7\tImages3"
        "\tGlow\tTransparent\tLightRadius\tPaletteId\tTime");
    std::map<std::string, uint32_t> spriteIndices;
    size_t unnamedSpriteGroups = 0;
    for (size_t index = 1; index < frames.size(); ++index)
    {
        const Row &row = frames[index];
        if (row.size() != 24 || integer(row[0]) != index - 1)
        {
            throw std::runtime_error("Invalid global SFT index");
        }
        if (row[5] == "x" && row[1].empty())
        {
            // The supplied DataTables.UpdateSFTGroups stores groupId[name] = i,
            // including the empty name. Its last unnamed slot wins. Preserve
            // that index and report blank object names; do not invent artwork.
            ++unnamedSpriteGroups;
            spriteIndices[""] = integer(row[0]);
        }
        else if (row[5] == "x" && !spriteIndices.emplace(lower(row[1]), integer(row[0])).second)
        {
            throw std::runtime_error("Duplicate SFT group: " + row[1]);
        }
    }
    const Rows objects = looseTable("Data/Tables/ObjList.txt");
    requireHeader(objects, "Name\tId\tRadius\tHeight\tInvisible\tUntouchable\tTemporary\tLifetimeInSFT"
        "\tNoPickup\tNoGravity\tInterceptAction\tBounce\tTrailParticles\tTrailFire\tTrailLine\tSFTGroup"
        "\tLifeTime\tSpeed\tParticleR\tParticleG\tParticleB\tParticleA");
    Rows objectRows;
    size_t wrappedParticleChannels = 0;
    std::set<uint32_t> objectIds;
    std::vector<uint32_t> unnamedObjectSprites;
    for (size_t index = 1; index < objects.size(); ++index)
    {
        const Row &row = objects[index];
        if (row.size() != 22 || !objectIds.insert(integer(row[1], 32767)).second
            || !spriteIndices.contains(lower(row[15])))
        {
            throw std::runtime_error("Invalid object ID or missing global SFT group");
        }
        uint32_t flags = 0;
        if (row[15].empty())
        {
            unnamedObjectSprites.push_back(integer(row[1]));
        }
        for (size_t column = 4; column <= 14; ++column)
        {
            if (row[column] == "x")
            {
                flags |= 1 << (column - 4);
            }
            else if (row[column] != "-")
            {
                throw std::runtime_error("Unknown object flag: " + row[column]);
            }
        }
        objectRows.push_back({row[0], lower(row[15]), row[1], row[2], row[3], row[16], row[17],
            std::to_string(spriteIndices.at(lower(row[15]))),
            std::to_string(color(row, 18, false, wrappedParticleChannels)), row[18], row[19], row[20],
            std::to_string(flags)});
    }
    tables["object_list.txt"] = objectRows;

    Game::MapStats mapCheck;
    Game::MonsterTable monsterCheck;
    Game::ObjectTable objectCheck;
    if (!mapCheck.loadFromRows(tables.at("map_stats.txt"))
        || mapCheck.getEntries().size() != maps.size()
        || !monsterCheck.loadEntriesFromRows(descriptors)
        || !monsterCheck.loadDisplayNamesFromRows(tables.at("monster_data.txt"))
        || !monsterCheck.loadStatsFromRows(tables.at("monster_data.txt"))
        || monsterCheck.statsEntries().size() != monsters.size()
        || !monsterCheck.loadDeathDropsFromRows(tables.at("monster_death_drops.txt"))
        || !monsterCheck.loadUniqueNamesFromRows(tables.at("english/place_mon.txt"))
        || !monsterCheck.loadRelationsFromRows(hostile)
        || !objectCheck.loadRows(objectRows) || objectCheck.entries().size() != objectRows.size())
    {
        throw std::runtime_error("Runtime typed loaders rejected converted core tables");
    }
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"shared-core-tables\",\"runtime_ready\":false,"
        << "\"combat_text_count\":" << combatText.size() << ",\"combat_text_source\":"
        << std::quoted(chinese ? "Data/10 LocZHCN.EnglishT.lod::global.txt" : "Data/mmmerge.T.lod::global.txt") << ','
        << "\"house_service_text_count\":" << houseServiceTextCount << ",\"house_service_text_source\":"
        << std::quoted(chinese ? "Data/10 LocZHCN.EnglishT.lod::global.txt" : "Data/mmmerge.T.lod::global.txt") << ','
        << "\"house_service_source_ids\":[56,57,68,86,97,104,113,128,134,140,152,155,159,160,168,178,179,195,196,197,198,199,200,404,405,414,446,448,527,529,536,537,538,540,541,542,561,563,569,570,663],"
        << "\"ui_text_count\":" << uiTextCount << ",\"ui_text_source\":"
        << std::quoted(chinese ? "Data/10 LocZHCN.EnglishT.lod::global.txt" : "Data/mmmerge.T.lod::global.txt")
        << ",\"ui_text_source_ids\":[";
    for (size_t index = 0; index < uiTextSources.size(); ++index)
    {
        manifest << (index == 0 ? "" : ",") << uiTextSources[index].second;
    }
    manifest << "],\"ui_text_project_translations\":[";
    for (size_t index = 0; index < projectUiTranslations.size(); ++index)
    {
        const auto &[message, translation] = projectUiTranslations[index];
        manifest << (index == 0 ? "" : ",") << "{\"key\":" << std::quoted(Game::UiTextTable::key(message))
            << ",\"en\":" << std::quoted(englishUiText.text(message))
            << ",\"zh-CN\":" << std::quoted(translation) << "}";
    }
    manifest << "],\"typed_validation\":true,\"map_count\":" << maps.size()
        << ",\"map_name_translations\":" << mapTranslations
        << ",\"map_translation_identity_mismatches\":" << mapTranslationMismatches
        << ",\"missing_map_geometry\":[" << missingMaps.str() << "]"
        << ",\"monster_count\":" << monsters.size() << ",\"monster_name_translations\":" << monsterTranslations
        << ",\"unused_monster_translation_count\":" << unusedMonsterTranslations.size()
        << ",\"conflicting_monster_translation_ids\":[";
    bool first = true;
    for (uint32_t id : conflictingMonsterTranslations)
    {
        manifest << (first ? "" : ",") << id;
        first = false;
    }
    manifest << "]"
        << ",\"wrapped_tint_channels\":" << wrappedTintChannels
        << ",\"wrapped_particle_channels\":" << wrappedParticleChannels
        << ",\"unnamed_sft_group_slots\":" << unnamedSpriteGroups
        << ",\"unnamed_object_sprite_ids\":[";
    first = true;
    for (uint32_t id : unnamedObjectSprites)
    {
        manifest << (first ? "" : ",") << id;
        first = false;
    }
    manifest << "]"
        << ",\"restored_glyphs\":" << glyphs << ",\"object_count\":" << objectRows.size()
        << ",\"death_drop_count\":" << tables.at("monster_death_drops.txt").size()
        << ",\"drop_scope\":\"declarative mapping and chance only; Lua hooks/summoning not ported\""
        << ",\"tables\":[";
    first = true;
    for (const auto &[name, rows] : tables)
    {
        manifest << (first ? "" : ",") << "{\"output\":" << std::quoted(name) << ",\"rows\":"
            << rows.size() << "}";
        first = false;
    }
    manifest << "]}\n";
    if (!std::filesystem::create_directory(destination))
    {
        throw std::runtime_error("Cannot create fresh core output directory");
    }
    try
    {
        for (const auto &[name, rows] : tables)
        {
            writeText(destination / "engine/data_tables" / name, serializeTextRows(rows));
        }
        writeText(destination / "core_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(destination);
        throw;
    }
}
}
