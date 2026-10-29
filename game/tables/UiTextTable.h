#pragma once

#include <array>
#include <cstddef>
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
        Count,
    };

    // English defaults keep packages predating ui_text.txt usable.
    UiTextTable();
    // Validate the current schema or explicitly migrate the complete 13-, 16- or 19-key schema.
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const std::string &text(Message message) const;
    std::string formatItemType(const std::string &type) const;
    static const char *key(Message message);

private:
    std::array<std::string, size_t(Message::Count)> m_texts;
};
}
