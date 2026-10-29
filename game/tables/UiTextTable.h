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
        Count,
    };

    // English defaults keep packages predating ui_text.txt usable.
    UiTextTable();
    // A present resource must declare every key exactly once.
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const std::string &text(Message message) const;
    static const char *key(Message message);

private:
    std::array<std::string, size_t(Message::Count)> m_texts;
};
}
