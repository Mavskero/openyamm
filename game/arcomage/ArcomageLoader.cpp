#include "game/arcomage/ArcomageLoader.h"

#include "game/tables/HouseTable.h"
#include "game/tables/MergedBaseTables.h"

#include <cstdlib>
#include <charconv>
#include <iostream>
#include <string>
#include <unordered_set>

namespace OpenYAMM::Game
{
namespace
{
bool parseInt(const std::string &text, int &value)
{
    if (text.empty())
    {
        return false;
    }

    char *pEnd = nullptr;
    const long parsed = std::strtol(text.c_str(), &pEnd, 10);

    if (pEnd == text.c_str() || *pEnd != '\0')
    {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool parseUnsigned(const std::string &text, uint32_t &value)
{
    if (text.empty())
    {
        return false;
    }

    char *pEnd = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &pEnd, 10);

    if (pEnd == text.c_str() || *pEnd != '\0')
    {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

int intAt(const std::vector<std::string> &row, size_t index, int defaultValue = 0)
{
    if (index >= row.size())
    {
        return defaultValue;
    }

    int value = defaultValue;
    return parseInt(row[index], value) ? value : defaultValue;
}

uint32_t unsignedAt(const std::vector<std::string> &row, size_t index, uint32_t defaultValue = 0)
{
    if (index >= row.size())
    {
        return defaultValue;
    }

    uint32_t value = defaultValue;
    return parseUnsigned(row[index], value) ? value : defaultValue;
}

ArcomageResourceType resourceTypeAt(const std::vector<std::string> &row, size_t index)
{
    const int value = intAt(row, index);

    switch (value)
    {
        case 1:
            return ArcomageResourceType::Bricks;

        case 2:
            return ArcomageResourceType::Gems;

        case 3:
            return ArcomageResourceType::Recruits;

        default:
            return ArcomageResourceType::None;
    }
}

ArcomageCondition conditionAt(const std::vector<std::string> &row, size_t index)
{
    const int value = intAt(row, index, 1);

    if (value < 0 || value > 21)
    {
        return ArcomageCondition::AlwaysPrimary;
    }

    return static_cast<ArcomageCondition>(value);
}

bool isDataRow(const std::vector<std::string> &row)
{
    if (row.empty() || row.front().empty())
    {
        return false;
    }

    for (char character : row.front())
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
    }

    return true;
}

bool validBinaryCardRow(const std::vector<std::string> &row)
{
    if (row.size() != 70)
    {
        return false;
    }
    for (size_t column = 0; column < row.size(); ++column)
    {
        if (column == 1)
        {
            continue;
        }
        int value = 0;
        const std::string &text = row[column];
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
        {
            return false;
        }
        if (column == 0 && (value < 0 || value >= ArcomageLibrary::CardCount))
        {
            return false;
        }
        if (column == 2 && (value < 0 || value > 113))
        {
            return false;
        }
        if (column == 3 && (value < 1 || value > 3))
        {
            return false;
        }
        if (column >= 4 && column <= 9 && (value < 0 || value > 127))
        {
            return false;
        }
        if ((column == 10 || column == 12 || column == 41) && value != 0 && value != 1)
        {
            return false;
        }
        // MM8 25 means player tower > enemy wall. Legacy OpenYAMM assigns that meaning to 21.
        if (column == 11 && !(value >= 0 && value <= 20) && value != 25)
        {
            return false;
        }
        if (column >= 12 && (value < -128 || value > 127))
        {
            return false;
        }
        if ((column == 13 || column == 42) && value < 0)
        {
            return false;
        }
        // This version of the binary profile supports the observed player-wall swap sentinel.
        if (column >= 14 && value == 98 && column != 21 && column != 50)
        {
            return false;
        }
    }
    return true;
}

void decodeBinaryWallEffect(ArcomageEffectBundle &bundle)
{
    if (bundle.playerWall == 98)
    {
        bundle.playerWall = 0;
        bundle.swapWalls = true;
    }
}
}

bool ArcomageLoader::load(
    const std::vector<std::vector<std::string>> &ruleRows,
    const std::vector<std::vector<std::string>> &cardRows
)
{
    m_library = {};

    if (!loadRules(ruleRows))
    {
        return false;
    }

    if (!loadCards(cardRows))
    {
        return false;
    }

    return true;
}

bool ArcomageLoader::loadFromHouseRules(
    const MergedHouseRuleTable &houseRules,
    const HouseTable &houseTable,
    const std::vector<std::vector<std::string>> &cardRows
)
{
    (void)houseRules;
    m_library = {};

    std::unordered_set<uint32_t> seenHouseIds;

    for (const auto &[houseId, houseEntry] : houseTable.entries())
    {
        if (!houseEntry.arcomageRule.has_value())
        {
            continue;
        }

        if (!seenHouseIds.insert(houseId).second)
        {
            std::cerr << "Duplicate Arcomage tavern rule row\n";
            return false;
        }

        const HouseEntry::ArcomageRule &sourceRule = *houseEntry.arcomageRule;
        ArcomageTavernRule rule = {};
        rule.houseId = houseId;
        rule.victoryTextId = sourceRule.rulesTextId;
        rule.firstWinAwardId = houseEntry.typeIndex <= 11 ? 63u + houseEntry.typeIndex : 0u;
        rule.startTower = sourceRule.towerAtStart;
        rule.startWall = sourceRule.wallAtStart;
        rule.startQuarry = sourceRule.quarry;
        rule.startMagic = sourceRule.magic;
        rule.startDungeon = sourceRule.dungeon;
        rule.startBricks = sourceRule.bricks;
        rule.startGems = sourceRule.gems;
        rule.startRecruits = sourceRule.recruits;
        rule.winTower = sourceRule.towerToWin;
        rule.winResources = sourceRule.resourceToWin;
        rule.aiMastery = sourceRule.ai;
        m_library.tavernRulesByHouseId[houseId] = rule;
    }

    if (m_library.tavernRulesByHouseId.empty())
    {
        std::cerr << "No Arcomage tavern rules resolved from house rules\n";
        return false;
    }

    return loadCards(cardRows);
}

const ArcomageLibrary &ArcomageLoader::library() const
{
    return m_library;
}

bool ArcomageLoader::loadRules(const std::vector<std::vector<std::string>> &rows)
{
    std::unordered_set<uint32_t> seenHouseIds;

    for (const std::vector<std::string> &row : rows)
    {
        if (!isDataRow(row))
        {
            continue;
        }

        ArcomageTavernRule rule = {};
        rule.houseId = unsignedAt(row, 0);
        rule.victoryTextId = unsignedAt(row, 1);
        rule.firstWinAwardId = unsignedAt(row, 2);
        rule.startTower = intAt(row, 3);
        rule.startWall = intAt(row, 4);
        rule.startQuarry = intAt(row, 5);
        rule.startMagic = intAt(row, 6);
        rule.startDungeon = intAt(row, 7);
        rule.startBricks = intAt(row, 8);
        rule.startGems = intAt(row, 9);
        rule.startRecruits = intAt(row, 10);
        rule.winTower = intAt(row, 11);
        rule.winResources = intAt(row, 12);
        rule.aiMastery = intAt(row, 13);

        if (rule.houseId == 0 || !seenHouseIds.insert(rule.houseId).second)
        {
            std::cerr << "Invalid Arcomage tavern rule row\n";
            return false;
        }

        m_library.tavernRulesByHouseId[rule.houseId] = rule;
    }

    return !m_library.tavernRulesByHouseId.empty();
}

bool ArcomageLoader::loadCards(const std::vector<std::vector<std::string>> &rows)
{
    std::unordered_set<uint32_t> seenCardIds;
    bool binaryEffects = false;
    bool hasMode = false;

    for (const std::vector<std::string> &row : rows)
    {
        if (!row.empty() && row[0] == "#ArcomageCardMode")
        {
            if (hasMode || !seenCardIds.empty() || row.size() != 2 || row[1] != "MM8BinaryV1")
            {
                std::cerr << "Invalid Arcomage card mode\n";
                return false;
            }
            hasMode = true;
            binaryEffects = true;
            continue;
        }
        if (!isDataRow(row))
        {
            continue;
        }
        if (binaryEffects && !validBinaryCardRow(row))
        {
            std::cerr << "Invalid MM8 binary Arcomage card row\n";
            return false;
        }

        ArcomageCardDefinition card = {};
        card.id = unsignedAt(row, 0);
        card.name = row.size() > 1 ? row[1] : std::string {};
        card.slot = unsignedAt(row, 2);
        card.resourceType = resourceTypeAt(row, 3);
        card.needQuarry = intAt(row, 4);
        card.needMagic = intAt(row, 5);
        card.needDungeon = intAt(row, 6);
        card.needBricks = intAt(row, 7);
        card.needGems = intAt(row, 8);
        card.needRecruits = intAt(row, 9);
        card.discardable = intAt(row, 10, 1) != 0;
        card.usesMm8BinaryEffects = binaryEffects;
        card.condition = binaryEffects && intAt(row, 11) == 25
            ? ArcomageCondition::TowerGreaterThanEnemyWall : conditionAt(row, 11);

        card.primary.playAgain = intAt(row, 12) != 0;
        card.primary.extraDraw = intAt(row, 13);
        card.primary.playerQuarry = intAt(row, 14);
        card.primary.playerMagic = intAt(row, 15);
        card.primary.playerDungeon = intAt(row, 16);
        card.primary.playerBricks = intAt(row, 17);
        card.primary.playerGems = intAt(row, 18);
        card.primary.playerRecruits = intAt(row, 19);
        card.primary.playerBuildings = intAt(row, 20);
        card.primary.playerWall = intAt(row, 21);
        card.primary.playerTower = intAt(row, 22);
        card.primary.enemyQuarry = intAt(row, 23);
        card.primary.enemyMagic = intAt(row, 24);
        card.primary.enemyDungeon = intAt(row, 25);
        card.primary.enemyBricks = intAt(row, 26);
        card.primary.enemyGems = intAt(row, 27);
        card.primary.enemyRecruits = intAt(row, 28);
        card.primary.enemyBuildings = intAt(row, 29);
        card.primary.enemyWall = intAt(row, 30);
        card.primary.enemyTower = intAt(row, 31);
        card.primary.bothQuarry = intAt(row, 32);
        card.primary.bothMagic = intAt(row, 33);
        card.primary.bothDungeon = intAt(row, 34);
        card.primary.bothBricks = intAt(row, 35);
        card.primary.bothGems = intAt(row, 36);
        card.primary.bothRecruits = intAt(row, 37);
        card.primary.bothBuildings = intAt(row, 38);
        card.primary.bothWall = intAt(row, 39);
        card.primary.bothTower = intAt(row, 40);

        card.secondary.playAgain = intAt(row, 41) != 0;
        card.secondary.extraDraw = intAt(row, 42);
        card.secondary.playerQuarry = intAt(row, 43);
        card.secondary.playerMagic = intAt(row, 44);
        card.secondary.playerDungeon = intAt(row, 45);
        card.secondary.playerBricks = intAt(row, 46);
        card.secondary.playerGems = intAt(row, 47);
        card.secondary.playerRecruits = intAt(row, 48);
        card.secondary.playerBuildings = intAt(row, 49);
        card.secondary.playerWall = intAt(row, 50);
        card.secondary.playerTower = intAt(row, 51);
        card.secondary.enemyQuarry = intAt(row, 52);
        card.secondary.enemyMagic = intAt(row, 53);
        card.secondary.enemyDungeon = intAt(row, 54);
        card.secondary.enemyBricks = intAt(row, 55);
        card.secondary.enemyGems = intAt(row, 56);
        card.secondary.enemyRecruits = intAt(row, 57);
        card.secondary.enemyBuildings = intAt(row, 58);
        card.secondary.enemyWall = intAt(row, 59);
        card.secondary.enemyTower = intAt(row, 60);
        card.secondary.bothQuarry = intAt(row, 61);
        card.secondary.bothMagic = intAt(row, 62);
        card.secondary.bothDungeon = intAt(row, 63);
        card.secondary.bothBricks = intAt(row, 64);
        card.secondary.bothGems = intAt(row, 65);
        card.secondary.bothRecruits = intAt(row, 66);
        card.secondary.bothBuildings = intAt(row, 67);
        card.secondary.bothWall = intAt(row, 68);
        card.secondary.bothTower = intAt(row, 69);

        if (binaryEffects)
        {
            decodeBinaryWallEffect(card.primary);
            decodeBinaryWallEffect(card.secondary);
        }

        if (card.name.empty() || !seenCardIds.insert(card.id).second)
        {
            std::cerr << "Invalid Arcomage card row\n";
            return false;
        }

        m_library.cardIndexById[card.id] = m_library.cards.size();
        m_library.cards.push_back(std::move(card));
    }

    if (m_library.cards.size() != ArcomageLibrary::CardCount)
    {
        std::cerr << "Arcomage card table has unexpected card count: " << m_library.cards.size() << '\n';
        return false;
    }

    return true;
}
}
