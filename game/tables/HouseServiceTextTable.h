#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class HouseServiceTextTable
{
public:
    struct Arguments
    {
        std::string member;
        std::string skill;
        std::string item;
        std::string destination;
        std::string open;
        std::string close;
        std::string price;
        std::string days;
        std::string level;
        std::string experience;
        std::string points;
        std::string food;
    };

    enum class Message
    {
        TempleHealAction,
        TempleDonateAction,
        TempleLearnSkillsAction,
        TempleLearnSkillAction,
        TempleNoSkills,
        TempleHealUnavailable,
        TempleNoCharacter,
        TempleCannotTreat,
        TempleAlreadyWell,
        TempleHealInsufficientGold,
        TempleHealSuccess,
        TempleDonateUnavailable,
        TempleDonateInsufficientGold,
        TempleDonateSuccess,
        TempleLessonUnavailable,
        TempleCannotLearn,
        TempleLearnInsufficientGold,
        TempleLearnSuccess,
        CommonClosedHours,
        CommonNotEnoughGold,
        TavernRentAction,
        TavernFoodAction,
        TavernPacksFull,
        TavernRentInsufficientGold,
        TavernFoodInsufficientGold,
        TavernFoodSuccess,
        TrainingAction,
        TrainingActionWithLevelPrice,
        TrainingMaxLevel,
        TrainingExperienceShortfall,
        TrainingNoCharacter,
        TrainingInsufficientGold,
        TrainingUnavailable,
        TrainingSuccess,
        ShopBuyStandardAction,
        ShopBuySpecialAction,
        ShopDisplayEquipmentAction,
        ShopSellAction,
        ShopIdentifyAction,
        ShopRepairAction,
        ShopSelectBuyStandard,
        ShopSelectBuySpecial,
        ShopSelectSell,
        ShopSelectIdentify,
        ShopSelectRepair,
        ShopNothingForSale,
        ShopUnavailable,
        ShopInventoryFull,
        ShopBought,
        ShopNoItem,
        ShopCannotSell,
        ShopSold,
        ShopAlreadyIdentified,
        ShopWrongIdentifyShop,
        ShopIdentifyFailed,
        ShopIdentifySuccess,
        ShopNothingToRepair,
        ShopWrongRepairShop,
        ShopRepairFailed,
        ShopRepairSuccess,
        ShopBuyHover,
        ShopSellHover,
        ShopIdentifyHover,
        ShopRepairHover,
        TransportRouteActionSingle,
        TransportRouteActionPlural,
        TransportNoRouteToday,
        TransportRouteUnavailable,
        TransportTravelUnavailable,
        TransportInsufficientGold,
        TransportSuccessSingle,
        TransportSuccessPlural,
        Count,
    };

    // Each semantic key is required exactly once. Templates may contain only
    // the declared named placeholders for that key.
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    std::string format(Message message, const Arguments &arguments) const;
    std::string format(
        Message message,
        const std::string &member = {},
        const std::string &skill = {},
        int price = -1) const;
    static const char *key(Message message);

private:
    std::array<std::string, size_t(Message::Count)> m_templates;
};
}
