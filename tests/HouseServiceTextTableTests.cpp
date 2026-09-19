#include "game/tables/HouseServiceTextTable.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
using Message = OpenYAMM::Game::HouseServiceTextTable::Message;

std::vector<std::vector<std::string>> validRows()
{
    return {
        {"temple.heal_action", "治疗（{price} 金币）"},
        {"temple.donate_action", "捐赠（{price} 金币）"},
        {"temple.learn_skills_action", "学习技能"},
        {"temple.learn_skill_action", "学习{skill}（{price} 金币）"},
        {"temple.no_skills", "这里没有可学技能。"},
        {"temple.heal_unavailable", "这里不提供治疗服务。"},
        {"temple.no_character", "未选择角色。"},
        {"temple.cannot_treat", "无法治疗{member}。"},
        {"temple.already_well", "{member}已经很健康。"},
        {"temple.heal_insufficient_gold", "治疗需要{price}金币。"},
        {"temple.heal_success", "已为{member}恢复，花费{price}金币。"},
        {"temple.donate_unavailable", "这里不接受捐赠。"},
        {"temple.donate_insufficient_gold", "捐赠需要{price}金币。"},
        {"temple.donate_success", "谢谢你！"},
        {"temple.lesson_unavailable", "这里不提供该课程。"},
        {"temple.cannot_learn", "{member}无法学习{skill}。"},
        {"temple.learn_insufficient_gold", "你的金币不够"},
        {"temple.learn_success", "{member}学习了{skill}，花费{price}金币。"},
        {"common.closed_hours", "此处在{open}到{close}开放"},
        {"common.not_enough_gold", "你的金币不够"},
        {"tavern.rent_action", "花费{price}个金币租房间"},
        {"tavern.food_action", "购买{food}天的食物，花费{price}个金币"},
        {"tavern.packs_full", "你的行囊已经装满了！"},
        {"tavern.rent_insufficient_gold", "租房间需要{price}个金币。"},
        {"tavern.food_insufficient_gold", "购买食物需要{price}个金币。"},
        {"tavern.food_success", "旅店老板把你的食物补充到{food}天。"},
        {"training.action", "训练"},
        {"training.action_with_level_price", "训练至等级{level}，花费{price}个金币"},
        {"training.max_level", "依你的水平，都可以在此当老师了。"},
        {"training.experience_shortfall", "你还需要{experience}点经验值才能训练至等级{level}"},
        {"training.no_character", "未选择要训练的角色。"},
        {"training.insufficient_gold", "训练需要{price}个金币。"},
        {"training.unavailable", "现在无法训练。"},
        {"training.success", "{member}已升至{level}级，并获得{points}个技能点！"},
        {"shop.buy_standard_action", "购买标准物品"},
        {"shop.buy_special_action", "购买特殊物品"},
        {"shop.display_equipment_action", "显示行囊"},
        {"shop.sell_action", "出售"},
        {"shop.identify_action", "鉴定"},
        {"shop.repair_action", "修理"},
        {"shop.select_buy_standard", "选择要购买的物品"},
        {"shop.select_buy_special", "选择要购买的特殊物品"},
        {"shop.select_sell", "选择要出售的物品"},
        {"shop.select_identify", "选择要鉴定的物品"},
        {"shop.select_repair", "选择要修理的物品"},
        {"shop.nothing_for_sale", "该位置没有出售的物品。"},
        {"shop.unavailable", "该物品不可用。"},
        {"shop.inventory_full", "行囊已满"},
        {"shop.bought", "购买了{item}，花费{price}个金币。"},
        {"shop.no_item", "该位置没有物品。"},
        {"shop.cannot_sell", "该物品不能出售。"},
        {"shop.sold", "出售了{item}，获得{price}个金币。"},
        {"shop.already_identified", "该物品已经鉴定。"},
        {"shop.wrong_identify_shop", "这家商店不能鉴定该物品。"},
        {"shop.identify_failed", "鉴定物品失败"},
        {"shop.identify_success", "完成"},
        {"shop.nothing_to_repair", "该物品无需修理。"},
        {"shop.wrong_repair_shop", "这家商店不能修理该物品。"},
        {"shop.repair_failed", "修理物品失败"},
        {"shop.repair_success", "完好如新"},
        {"shop.buy_hover", "购买{item}需要{price}个金币。"},
        {"shop.sell_hover", "出售{item}可得{price}个金币。"},
        {"shop.identify_hover", "鉴定{item}需要{price}个金币。"},
        {"shop.repair_hover", "修理{item}需要{price}个金币。"},
        {"transport.route_action_single", "用{days}天到{destination}，旅费{price}个金币"},
        {"transport.route_action_plural", "用{days}天到{destination}，旅费{price}个金币"},
        {"transport.no_route_today", "抱歉，请改天再来"},
        {"transport.route_unavailable", "该路线不可用。"},
        {"transport.travel_unavailable", "现在无法旅行。"},
        {"transport.insufficient_gold", "旅行需要{price}个金币。"},
        {"transport.success_single", "要用{days}天才能到达{destination}。"},
        {"transport.success_plural", "要用{days}天才能到达{destination}。"},
    };
}
}

TEST_CASE("house service text accepts semantic row order and formats declared placeholders")
{
    std::vector<std::vector<std::string>> rows = validRows();
    std::reverse(rows.begin(), rows.end());
    OpenYAMM::Game::HouseServiceTextTable table;

    REQUIRE(table.loadFromRows(rows));
    CHECK_EQ(table.format(Message::TempleHealAction, {}, {}, 37), "治疗（37 金币）");
    CHECK_EQ(
        table.format(Message::TempleLearnSuccess, "火焰法师", "经商术", 240),
        "火焰法师学习了经商术，花费240金币。");
    CHECK_EQ(
        table.format(Message::TransportRouteActionPlural,
            {.destination = "泰拉瑞尔", .price = "50", .days = "3"}),
        "用3天到泰拉瑞尔，旅费50个金币");
}

TEST_CASE("house service text rejects incomplete duplicate and unsafe templates")
{
    SUBCASE("missing semantic row")
    {
        std::vector<std::vector<std::string>> rows = validRows();
        rows.pop_back();
        OpenYAMM::Game::HouseServiceTextTable table;
        CHECK_FALSE(table.loadFromRows(rows));
    }

    SUBCASE("duplicate semantic key")
    {
        std::vector<std::vector<std::string>> rows = validRows();
        rows.back() = rows.front();
        OpenYAMM::Game::HouseServiceTextTable table;
        CHECK_FALSE(table.loadFromRows(rows));
    }

    SUBCASE("missing required placeholder")
    {
        std::vector<std::vector<std::string>> rows = validRows();
        rows[0][1] = "治疗金币";
        OpenYAMM::Game::HouseServiceTextTable table;
        CHECK_FALSE(table.loadFromRows(rows));
    }

    SUBCASE("unknown placeholder")
    {
        std::vector<std::vector<std::string>> rows = validRows();
        rows[0][1] = "治疗（{cost} 金币）";
        OpenYAMM::Game::HouseServiceTextTable table;
        CHECK_FALSE(table.loadFromRows(rows));
    }

    SUBCASE("duplicate placeholder")
    {
        std::vector<std::vector<std::string>> rows = validRows();
        rows[0][1] = "{price} + {price}";
        OpenYAMM::Game::HouseServiceTextTable table;
        CHECK_FALSE(table.loadFromRows(rows));
    }
}
