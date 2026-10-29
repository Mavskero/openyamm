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
        Count,
    };

    // English defaults keep packages predating ui_text.txt usable.
    UiTextTable();
    // Validate the current schema or explicitly migrate a complete 13-, 16-, 19-, 26- or 35-key schema.
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const std::string &text(Message message) const;
    std::string formatItemType(const std::string &type) const;
    std::string formatWinScore(uint64_t score) const;
    std::string formatWinCharacter(const std::string &name, uint32_t level, const std::string &className) const;
    static const char *key(Message message);

private:
    std::array<std::string, size_t(Message::Count)> m_texts;
};
}
