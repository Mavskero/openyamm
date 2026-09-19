#include "game/arcomage/ArcomageLoader.h"
#include "game/arcomage/ArcomageRules.h"

#include <iostream>
#include <stdexcept>

namespace
{
using namespace OpenYAMM::Game;
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
const Rows Rules = {{"1", "0", "0", "30", "10", "3", "3", "3", "100", "100", "100", "200", "500", "1"}};

void require(bool condition)
{
    if (!condition)
    {
        throw std::runtime_error("Arcomage binary rule assertion failed");
    }
}

Rows cards(bool binary)
{
    Rows rows;
    if (binary)
    {
        rows.push_back({"#ArcomageCardMode", "MM8BinaryV1"});
    }
    for (size_t id = 0; id < ArcomageLibrary::CardCount; ++id)
    {
        Row row(70, "0");
        row[0] = std::to_string(id);
        row[1] = "Card " + row[0];
        row[2] = row[0];
        row[3] = row[10] = row[11] = "1";
        rows.push_back(std::move(row));
    }
    return rows;
}

ArcomageState play(const Rows &rows, int playerWall, int enemyWall, int playerTower = 30)
{
    ArcomageLoader loader;
    ArcomageRules rules;
    ArcomageState state;
    require(loader.load(Rules, rows));
    require(rules.initializeMatch(loader.library(), 1, "A", "B", 1, state));
    state.players[0].wall = playerWall;
    state.players[0].tower = playerTower;
    state.players[1].wall = enemyWall;
    state.players[0].hand[0] = 0;
    require(rules.playCard(loader.library(), 0, state));
    return state;
}

void checkEffects()
{
    Rows rows = cards(true);
    Row &row = rows[1];
    row[1] = "Shift";
    row[7] = "17";
    row[21] = "98";
    ArcomageState state = play(rows, 10, 35);
    require(state.players[0].wall == 35 && state.players[1].wall == 10 && state.players[0].bricks == 83);
    row[1] = "Flood Water";
    row[21] = "0";
    row[7] = "6";
    row[11] = "15";
    row[16] = row[54] = "-1";
    row[22] = row[60] = "-2";
    for (int wall : {10, 20, 30})
    {
        state = play(rows, wall, 20);
        const size_t affected = wall < 20 ? 0 : 1;
        require(state.players[affected].dungeon == 2 && state.players[affected].tower == 28);
        require(state.players[1 - affected].dungeon == 3 && state.players[1 - affected].tower == 30);
        require(state.players[0].bricks == 94);
    }
    rows = cards(true);
    rows[1][11] = "25";
    rows[1][31] = "-8";
    rows[1][58] = "-8";
    for (int tower : {10, 20, 30})
    {
        state = play(rows, 10, 20, tower);
        require(state.players[1].wall == (tower > 20 ? 20 : 12));
        require(state.players[1].tower == (tower > 20 ? 22 : 30));
    }
    rows = cards(true);
    rows[1][12] = "1";
    rows[1][17] = "2";
    state = play(rows, 10, 10);
    require(state.currentPlayerIndex == 0 && !state.needsTurnStart && state.players[0].bricks == 102);

    // Existing unmarked tables retain their normalized, named special-card convention.
    rows = cards(false);
    rows[0][1] = "Shift";
    state = play(rows, 10, 35);
    require(state.players[0].wall == 35 && state.players[1].wall == 10);
    rows[0][1] = "Flood Water";
    state = play(rows, 20, 20);
    require(state.players[0].tower == 28 && state.players[1].tower == 28);
    rows[0][1] = "Legacy condition";
    rows[0][11] = "21";
    rows[0][31] = "-8";
    state = play(rows, 10, 20, 30);
    require(state.players[1].tower == 22);
}

void checkRejections()
{
    for (const std::pair<size_t, std::string> &fault : {
        std::pair<size_t, std::string>{0, "102"}, {2, "114"}, {3, "0"}, {7, "-1"}, {10, "2"},
        {11, "21"}, {11, "26"}, {11, "nan"}, {12, "2"}, {13, "-1"}, {14, "98"}, {16, "128"},
        {21, "9999999999999999"}})
    {
        Rows rows = cards(true);
        rows[1][fault.first] = fault.second;
        ArcomageLoader loader;
        require(!loader.load(Rules, rows));
    }
    for (int fault = 0; fault < 6; ++fault)
    {
        Rows rows = cards(true);
        if (fault == 0)
        {
            rows[0][1] = "Unknown";
        }
        if (fault == 1)
        {
            rows.push_back(rows[0]);
        }
        if (fault == 2)
        {
            rows[1].pop_back();
        }
        if (fault == 3)
        {
            rows[2][0] = "0";
        }
        if (fault == 4)
        {
            rows.pop_back();
        }
        if (fault == 5)
        {
            rows[1][1].clear();
        }
        ArcomageLoader loader;
        require(!loader.load(Rules, rows));
    }
}
}

int main()
{
    try
    {
        checkEffects();
        checkRejections();
        std::cout << "Arcomage binary effects, legacy compatibility and malformed tables passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
