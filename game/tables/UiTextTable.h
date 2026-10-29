#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class UiTextTable
{
public:
    enum class Message
    {
        EmptySlot,
        Menu,
        Submit,
        Cancel,
        BeaconSet,
        BeaconRecall,
        BeaconAvailable,
        Day,
        Days,
        Hour,
        Hours,
        Minute,
        Minutes,
        ArcomageResign,
        ArcomageTouchHint,
        ArcomageDiscardHint,
        ItemTypeFormat,
        ItemStolen,
        SpellSpCost,
        ItemPower,
        ItemAttack,
        ItemShoot,
        ItemDamage,
        ItemCharges,
        ItemArmor,
        ItemValue,
        Congratulations,
        TotalTime,
        ScoreFormat,
        CharacterFormat,
        Year,
        Years,
        Month,
        Months,
        WinEnding,
        BankBalance,
        BankDeposit,
        BankWithdraw,
        BankHowMuch,
        RestHealEightHours,
        RestWaitWithoutHealing,
        RestWaitUntilDawn,
        RestWaitFiveMinutes,
        RestWaitOneHour,
        RestExit,
        SkillNormal,
        SkillExpert,
        SkillMaster,
        SkillGrandmaster,
        None,
        WeekdayMonday,
        WeekdayTuesday,
        WeekdayWednesday,
        WeekdayThursday,
        WeekdayFriday,
        WeekdaySaturday,
        WeekdaySunday,
        MonthJanuary,
        MonthFebruary,
        MonthMarch,
        MonthApril,
        MonthMay,
        MonthJune,
        MonthJuly,
        MonthAugust,
        MonthSeptember,
        MonthOctober,
        MonthNovember,
        MonthDecember,
        SaveAutosave,
        SaveQuicksave,
        SaveSlot,
        KeyboardForward,
        KeyboardBackward,
        KeyboardLeft,
        KeyboardRight,
        KeyboardYell,
        KeyboardJump,
        KeyboardCombat,
        KeyboardCastReady,
        KeyboardAttack,
        KeyboardTrigger,
        KeyboardCast,
        KeyboardPass,
        KeyboardCharCycle,
        KeyboardQuest,
        KeyboardQuickRef,
        KeyboardRest,
        KeyboardHistory,
        KeyboardUse,
        KeyboardMapBook,
        KeyboardAlwaysRun,
        KeyboardLookUp,
        KeyboardLookDown,
        KeyboardCenterView,
        KeyboardZoomIn,
        KeyboardZoomOut,
        KeyboardFlyUp,
        KeyboardFlyDown,
        KeyboardLand,
        KeyboardDoubleSpeed,
        KeyboardPressInput,
        KeyboardBindingConflict,
        Count,
    };

    // English defaults keep packages predating ui_text.txt usable.
    UiTextTable();
    // Validate the current schema or migrate a complete 13-, 16-, 19-, 26-, 35-, 39-, 45- or 72-key schema.
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const std::string &text(Message message) const;
    std::string formatItemType(const std::string &type) const;
    std::string formatWinScore(uint64_t score) const;
    std::string formatWinCharacter(const std::string &name, uint32_t level, const std::string &className) const;
    std::string formatMasteryDescription(Message mastery, const std::string &description) const;
    static const char *key(Message message);

private:
    std::array<std::string, size_t(Message::Count)> m_texts;
};
}
