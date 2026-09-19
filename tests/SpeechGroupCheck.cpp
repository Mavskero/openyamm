#include "game/tables/MergedBaseTables.h"
#include "game/tables/SpeechReactionTable.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace OpenYAMM::Game;
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

void require(bool value)
{
    if (!value)
    {
        throw std::runtime_error("Speech group assertion failed");
    }
}

void checkSelection()
{
    Rows rows = {{"Soundtype/Voicesetid", "0", "1"}};
    for (size_t slot = 0; slot < 100; ++slot)
    {
        rows.push_back({"RepeatedLabel", "0", "0"});
    }
    rows[1][1] = "100";
    rows[2][1] = "101";
    rows[4][1] = "200";
    rows[1][2] = "300";
    rows[3][2] = "400";
    rows[4][2] = "401";
    MergedCharacterVoiceTable voices;
    require(voices.loadFromRows(rows));
    const std::array<uint32_t, 8> expected = {100, 200, 101, 200, 100, 200, 101, 200};
    const std::array<uint32_t, 4> otherVoice = {300, 400, 300, 401};
    for (uint32_t seed = 0; seed < expected.size(); ++seed)
    {
        require(voices.selectSoundIdForGroups(0, {1, 2}, seed) == expected[seed]);
        require(voices.selectSoundIdForGroups(1, {1, 2}, seed) == otherVoice[seed % 4]);
    }
    require(voices.selectSoundIdForGroups(0, {1, 3}, 0) == 100);
    require(!voices.selectSoundIdForGroups(0, {1, 3}, 1));
    require(voices.selectSoundIdForGroups(0, {1, 3}, 2) == 101);
    require(!voices.selectSoundIdForGroups(0, {1, 3}, 3));
    require(!voices.selectSoundIdForGroups(2, {1}, 0));
    require(!voices.selectSoundIdForGroups(0, {0}, 0));
    require(!voices.selectSoundIdForGroups(0, {51}, 0));
    require(!voices.selectSoundIdForGroups(0, {}, 0));
    require(voices.soundIdsForTypes(0, {"RepeatedLabel"}) == std::vector<uint32_t>{100, 101, 200});
    rows.pop_back();
    require(voices.loadFromRows(rows) && !voices.selectSoundIdForGroups(0, {1}, 0));
}

void checkSchema()
{
    SpeechReactionTable table;
    const Rows rows = {{"#SpeechReactionMode", "MM8VoiceGroupsV1"},
        {"HelloDay", "Greeting", "32,33", "Hello", "22"},
        {"CastSpell", "Casting", "", "CastSpell", "49"}, {"Yes", "Affirmation", "42", "", ""}};
    require(table.loadFromRows(rows));
    require(table.find(SpeechId::HelloDay)->voiceGroups == std::vector<uint32_t>{32, 33});
    require(table.find(SpeechId::HelloDay)->soundTypes.empty());
    require(table.find(SpeechId::CastSpell)->voiceGroups.empty()
        && table.find(SpeechId::CastSpell)->faceAnimationId == FaceAnimationId::CastSpell);
    require(!table.find(SpeechId::Yes)->faceAnimationId);
    for (const std::string &invalid : {"0", "51", "-1", "1,", ",1", "1,,2", "1,2,3", "bad"})
    {
        Rows bad = rows;
        bad[1][2] = invalid;
        require(!table.loadFromRows(bad));
    }
    for (const std::pair<size_t, std::string> &fault : {
        std::pair<size_t, std::string>{0, "UnknownEvent"}, {1, ""}, {3, "UnknownFace"}, {4, "0"}, {4, "110"}})
    {
        Rows bad = rows;
        bad[1][fault.first] = fault.second;
        require(!table.loadFromRows(bad));
    }
    Rows bad = rows;
    bad.push_back(rows[1]);
    require(!table.loadFromRows(bad));
    bad = rows;
    bad[0][1] = "UnknownMode";
    require(!table.loadFromRows(bad));
    require(table.loadFromRows({{"HelloDay", "Legacy", "Hello1,Hello2", "Hello"}}));
    require(table.find(SpeechId::HelloDay)->voiceGroups.empty()
        && table.find(SpeechId::HelloDay)->soundTypes == std::vector<std::string>{"Hello1", "Hello2"});
}
}

int main()
{
    try
    {
        checkSelection();
        checkSchema();
        std::cout << "Speech group probability, silent variants and legacy schema checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
