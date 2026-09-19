#include "EnchantmentResources.h"
#include "game/items/ItemEnchantTables.h"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string_view>

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
        throw std::runtime_error("Enchantment conversion: " + message);
    }
}

void validateNumber(const std::string &text)
{
    std::string_view trimmed = text;
    const size_t begin = trimmed.find_first_not_of(" \t\r\n");
    require(begin != std::string_view::npos, "empty weight/value");
    const size_t end = trimmed.find_last_not_of(" \t\r\n");
    trimmed = trimmed.substr(begin, end - begin + 1);
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == trimmed.data() + trimmed.size() && value <= 32767,
        "invalid weight/value: " + text);
}

Rows entryBlock(const Rows &source, bool special)
{
    const size_t columns = special ? 17 : 11;
    const size_t paddedColumns = special ? 20 : 16;
    const Row expectedHeader = special
        ? Row{"Bonus Stat", "Name Add", "W1", "W2", "Miss", "Arm", "Shld", "Helm", "Belt", "Cape",
            "Gaunt", "Boot", "Ring", "Amul", "Value", "Lvl"}
        : Row{"Bonus Stat", "Of Name", "Arm", "Shld", "Helm", "Belt", "Cape", "Gaunt", "Boot", "Ring", "Amul"};
    const auto header = std::find_if(source.begin(), source.end(), [](const Row &row)
    {
        return !row.empty() && row[0] == "Bonus Stat";
    });
    require(header != source.end() && header->size() >= expectedHeader.size()
        && std::equal(expectedHeader.begin(), expectedHeader.end(), header->begin()), "unrecognized source header");
    Rows entries;
    for (auto position = header + 1; position != source.end(); ++position)
    {
        const Row &row = *position;
        if (row.empty() || row[0].empty())
        {
            if (!entries.empty())
            {
                break;
            }
            continue;
        }
        require(row.size() >= columns && row.size() <= paddedColumns && !row[1].empty(),
            "invalid entry at slot " + std::to_string(entries.size() + 1));
        require(std::all_of(row.begin() + columns, row.end(), [](const std::string &cell) { return cell.empty(); }),
            "unexpected populated extension columns");
        for (size_t column = 2; column < (special ? 14 : 11); ++column)
        {
            validateNumber(row[column]);
        }
        if (special)
        {
            std::string price = row[14];
            const size_t begin = price.find_first_not_of(" \t\r\n");
            require(begin != std::string::npos, "empty price modifier");
            if (price[begin] == 'x' || price[begin] == 'X')
            {
                price.erase(0, begin + 1);
            }
            validateNumber(price);
            require(row[15].size() == 1 && row[15][0] >= 'A' && row[15][0] <= 'D', "invalid rarity level");
        }
        entries.push_back(Row(row.begin(), row.begin() + columns));
    }
    // This versioned MMerge profile has 24/72 ordinary slots followed by one
    // zero-weight extension each. Footers describe treasure levels, not items.
    require(entries.size() == (special ? 73 : 25), "unsupported slot count or interrupted entry block");
    return entries;
}

Rows localize(const Rows &base, const Rows &translation, bool special, size_t &preservedMechanicalCells)
{
    Rows output = base;
    for (size_t index = 0; index < output.size(); ++index)
    {
        Row &row = output[index];
        row.resize(special ? 20 : 16);
        row.push_back(base[index][special ? 1 : 0]);
        if (!translation.empty())
        {
            // Neither source has an ID column. Preserve the validated profile's
            // positional identity; only the original visible strings are overlaid.
            row[0] = translation[index][0];
            row[1] = translation[index][1];
            if (special)
            {
                // Column 16 is an English design note in the old language pack.
                // The game's actual visible bonus description is column zero.
                row[16] = translation[index][0];
            }
            for (size_t column = 2; column < (special ? 16 : 11); ++column)
            {
                preservedMechanicalCells += base[index][column] != translation[index][column] ? 1 : 0;
            }
        }
    }
    return output;
}

void validateConverted(const Rows &standard, const Rows &special, const EnchantmentResources &converted)
{
    Game::StandardItemEnchantTable baseStandard;
    Game::StandardItemEnchantTable localizedStandard;
    Game::SpecialItemEnchantTable baseSpecial;
    Game::SpecialItemEnchantTable localizedSpecial;
    require(baseStandard.load(standard) && localizedStandard.load(converted.standard)
        && baseSpecial.load(special) && localizedSpecial.load(converted.special), "typed loader rejected data");
    require(localizedStandard.entries().size() == 25 && localizedSpecial.entries().size() == 73,
        "typed loader changed slot coverage");
    for (uint16_t id = 1; id <= 25; ++id)
    {
        const Game::StandardItemEnchantEntry *pBase = baseStandard.get(id);
        const Game::StandardItemEnchantEntry *pLocalized = localizedStandard.get(id);
        require(pBase && pLocalized && (id == 25 || static_cast<uint16_t>(pBase->kind) == id),
            "unsupported standard effect or reordered source slot " + std::to_string(id));
        require(pLocalized->kind == pBase->kind && pLocalized->slotValues == pBase->slotValues
            && pLocalized->statName == converted.standard[id - 1][0]
            && pLocalized->suffix == converted.standard[id - 1][1], "localized standard effect changed");
    }
    for (uint16_t id = 1; id <= 73; ++id)
    {
        const Game::SpecialItemEnchantEntry *pBase = baseSpecial.get(id);
        const Game::SpecialItemEnchantEntry *pLocalized = localizedSpecial.get(id);
        require(pBase && pLocalized && (id == 73 || static_cast<uint16_t>(pBase->kind) == id),
            "unsupported special effect or reordered source slot " + std::to_string(id));
        require(pLocalized->kind == pBase->kind && pLocalized->slotWeights == pBase->slotWeights
            && pLocalized->priceModifier.amount == pBase->priceModifier.amount
            && pLocalized->priceModifier.multiplicative == pBase->priceModifier.multiplicative
            && pLocalized->rarityLevel == pBase->rarityLevel
            && pLocalized->displayAsPrefix == pBase->displayAsPrefix
            && pLocalized->name == converted.special[id - 1][0]
            && pLocalized->suffix == converted.special[id - 1][1]
            && pLocalized->description == converted.special[id - 1][16], "localized special effect changed");
    }
    require(standard.back()[0] == "Great might" && special.back()[1] == "of Sparking",
        "unknown extension effect");
    require(baseStandard.get(25)->kind == Game::StandardItemEnchantKind::None
        && baseSpecial.get(73)->kind == Game::SpecialItemEnchantKind::None, "extension support profile changed");
    for (const int value : baseStandard.get(25)->slotValues)
    {
        require(value == 0, "unsupported standard extension now has random generation weight");
    }
    for (const int value : baseSpecial.get(73)->slotWeights)
    {
        require(value == 0, "unsupported special extension now has random generation weight");
    }
}
}

EnchantmentResources convertEnchantmentTables(const Rows &standard, const Rows &special,
    const Rows &localizedStandard, const Rows &localizedSpecial)
{
    require(localizedStandard.empty() == localizedSpecial.empty(), "incomplete localization pair");
    const Rows baseStandard = entryBlock(standard, false);
    const Rows baseSpecial = entryBlock(special, true);
    const Rows translationStandard = localizedStandard.empty() ? Rows{} : entryBlock(localizedStandard, false);
    const Rows translationSpecial = localizedSpecial.empty() ? Rows{} : entryBlock(localizedSpecial, true);
    EnchantmentResources result;
    result.standard = localize(baseStandard, translationStandard, false, result.preservedMechanicalCells);
    result.special = localize(baseSpecial, translationSpecial, true, result.preservedMechanicalCells);
    validateConverted(baseStandard, baseSpecial, result);
    return result;
}
}
