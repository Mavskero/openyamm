#include "VoiceResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
// CharacterOutfits.lua indexes the first 100 rows by position. Repeated labels are separate sound slots.
const Row SoundTypes = {"Disarmed", "Disarmed", "Closed1", "Closed2", "FailedDisarm1", "FailedDisarm2",
    "ChooseMe1", "ChooseMe2", "BadItem1", "BadItem2", "GoodItem1", "GoodItem2", "CantIdent1", "CantIdent2",
    "Repaired1", "Repaired2", "CantRep1", "CantRep2", "WeakMonster1", "WeakMonster2", "BigMonster1", "BigMonster2",
    "CantIdMon1", "CantIdMon2", "QuickSpell1", "QuickSpell2", "Hungry1", "Hungry2", "SoftInjure1", "SoftInjure1",
    "HardInjured1", "HardInjured2", "Dying1", "Dying2", "Drunk1", "Drunk2", "Insane1", "Insane2", "Poison1",
    "Poison2", "Cursed1", "Cursed2", "Fall1", "Fall2", "CantRestHere1", "CantRestHere2", "NeedMoreGold1",
    "NeedMoreGold2", "InventoryFull1", "InventoryFull2", "PotionMixed1", "PotionMixed2", "FailMixing1", "FailMixing2",
    "NeedAKey1", "NeedAKey2", "LearnSpell1", "LearnSpell2", "CantLearn1", "CantLearn2", "CantEquip1", "CantEquip2",
    "Hello1", "Hello2", "Hello3", "Hello4", "Hello5", "Hello6", "n/uCurse1", "n/uCurse2", "n/uLaughter1",
    "n/uLaughter2", "Win!1", "Win!2", "Heh1", "Heh2", "LastStanding1", "LastStanding2", "Leave1", "Leave2",
    "Enter1", "Enter2", "Yes1", "Yes2", "Thanks1", "Thanks2", "Indignant", "Indignant", "n/uObligatory",
    "n/uObligatory", "n/uShutUp1", "n/uShutUp2", "Move!1", "Move!2", "n/u", "n/u", "n/u", "n/u", "n/u", "n/u"};
const Row Categories = {"Man", "Woman", "Dragon", "Undead Man", "Undead Woman", "Undead Dragon"};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t number(const std::string &text, uint32_t maximum = 65535)
{
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value <= maximum,
        "Invalid voice numeric field: " + text);
    return value;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid voice table quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 != table->getRowCount() || row != Row{""})
        {
            rows.push_back(row);
        }
    }
    return rows;
}

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Voice input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 4 * 1024 * 1024, "Invalid voice input size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read voice dependency");
    if (!legacy)
    {
        return parse(text);
    }
    size_t glyphs = 0;
    text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
    // The source reader splits literal lines/tabs; quote characters in Notes have no CSV meaning.
    Rows rows;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        Row row;
        size_t start = 0;
        while (true)
        {
            const size_t end = line.find('\t', start);
            row.push_back(line.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}
}

void convertVoiceResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Voice output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Voice output already exists");
    const Rows rows = readRows(root, "Data/Tables/Character voices.txt", true);
    require(rows.size() == 110 && rows[0].size() >= 2 && rows[0].size() <= 257
        && rows[0][0] == "Soundtype/Voicesetid", "Unsupported character voice header or row count");
    const size_t voices = rows[0].size() - 1;
    for (const Row &row : rows)
    {
        require(row.size() == voices + 1, "Invalid character voice row width");
    }
    for (size_t voice = 0; voice < voices; ++voice)
    {
        require(number(rows[0][voice + 1], 255) == voice, "Voice set identity/order changed");
    }
    for (size_t index : {101, 108})
    {
        require(rows[index] == Row(voices + 1), "Voice separator contains nonempty fields");
    }
    require(rows[109][0] == "Notes", "Missing voice notes footer");
    std::array<std::vector<uint32_t>, 6> available;
    for (size_t category = 0; category < Categories.size(); ++category)
    {
        const Row &row = rows[102 + category];
        require(row[0] == Categories[category], "Voice availability category/order changed");
        for (size_t voice = 0; voice < voices; ++voice)
        {
            require(row[voice + 1] == "x" || row[voice + 1] == "-", "Invalid voice availability flag");
            if (row[voice + 1] == "x")
            {
                available[category].push_back(static_cast<uint32_t>(voice));
            }
        }
    }
    Rows exported = {{"#CharacterVoiceMode", "Complete"}};
    exported.insert(exported.end(), rows.begin(), rows.end());
    const std::string text = serializeTextRows(exported);
    require(parse(text) == exported, "Voice serialization changed source fields");
    MergedCharacterVoiceTable table;
    require(table.loadFromRows(parse(text)) && table.entries().size() == 100,
        "Voice loader changed sound slot boundaries");
    std::set<uint32_t> soundIds;
    size_t zeroSlots = 0;
    size_t queryChecks = 0;
    for (size_t voice = 0; voice < voices; ++voice)
    {
        std::map<std::string, std::vector<uint32_t>> expected;
        for (size_t slot = 0; slot < SoundTypes.size(); ++slot)
        {
            const Row &row = rows[slot + 1];
            require(row[0] == SoundTypes[slot], "Voice sound slot label/order changed");
            const uint32_t sound = number(row[voice + 1]);
            const MergedCharacterVoiceEntry &entry = table.entries()[slot];
            require(entry.soundType == row[0] && entry.soundIdsByVoiceSetId.size() == voices
                && entry.soundIdsByVoiceSetId[voice] == sound, "Voice sound identity changed");
            std::vector<uint32_t> &choices = expected[row[0]];
            if (sound != 0)
            {
                choices.push_back(sound);
                soundIds.insert(sound);
            }
            else
            {
                ++zeroSlots;
            }
        }
        for (const auto &[label, choices] : expected)
        {
            require(table.soundIdsForTypes(voice, {label}) == choices, "Voice reaction order/zero handling changed");
            ++queryChecks;
        }
        require(table.soundIdsForTypes(voice, Categories).empty()
            && table.soundIdsForTypes(voice, {"Notes"}).empty(), "Voice metadata was interpreted as a sound");
    }
    require(table.soundIdsForTypes(voices, SoundTypes).empty(), "Out-of-range voice yielded sounds");
    size_t availabilityChecks = 0;
    for (uint32_t portrait : {0, 66, 67, 68, 69})
    {
        for (int32_t race : {0, 5, 6, 10})
        {
            for (uint32_t sex : {0, 1})
            {
                size_t category = sex;
                if (race == 5)
                {
                    category = 2;
                }
                else if (race == 6 || race == 10)
                {
                    category += 3;
                }
                if (portrait == 67 || portrait == 68)
                {
                    category = 5;
                }
                const std::optional<std::vector<uint32_t>> choices =
                    table.availableVoiceIds(portrait, race, sex);
                require(choices && *choices == available[category], "Voice category/portrait priority changed");
                ++availabilityChecks;
            }
        }
    }
    CharacterDollTable dolls;
    require(dolls.loadCharacterRows(readRows(assets, "engine/data_tables/character_data.txt", false)),
        "Invalid voice portrait dependency");
    std::vector<uint32_t> defaultsOutsideAvailability;
    for (const auto &[id, doll] : dolls.characters())
    {
        require(id > 0 && doll.defaultVoiceId < voices, "Portrait default voice is outside the source voice array");
        const std::optional<std::vector<uint32_t>> choices =
            table.availableVoiceIds(id - 1, doll.raceId, doll.defaultSex);
        require(choices.has_value(), "Missing complete voice availability");
        if (std::find(choices->begin(), choices->end(), doll.defaultVoiceId) == choices->end())
        {
            defaultsOutsideAvailability.push_back(id - 1);
        }
    }
    std::sort(defaultsOutsideAvailability.begin(), defaultsOutsideAvailability.end());
    MergedCharacterVoiceTable legacy;
    require(legacy.loadFromRows({{"Soundtype/Voicesetid", "0"}, {"Disarmed", "1"}, {"Disarmed", "1"}})
        && !legacy.availableVoiceIds(0, 0, 0).has_value()
        && legacy.soundIdsForTypes(0, {"Disarmed"}) == std::vector<uint32_t>{1, 1},
        "Legacy voice table semantics changed");
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"character_voices\",\"runtime_ready\":false,\"voice_sets\":" << voices
        << ",\"sound_slots_per_voice\":100,\"zero_sound_slots\":" << zeroSlots
        << ",\"distinct_nonzero_sound_ids\":" << soundIds.size() << ",\"reaction_query_checks\":" << queryChecks
        << ",\"availability_checks\":" << availabilityChecks << ",\"availability_by_category\":[";
    for (size_t category = 0; category < available.size(); ++category)
    {
        report << (category == 0 ? "" : ",") << available[category].size();
    }
    report << "],\"portrait_default_voices_checked\":" << dolls.characters().size()
        << ",\"source_portraits_with_default_outside_availability\":[";
    for (size_t index = 0; index < defaultsOutsideAvailability.size(); ++index)
    {
        report << (index == 0 ? "" : ",") << defaultsOutsideAvailability[index];
    }
    report << "],\"typed_validation\":true,\"legacy_table_behavior_preserved\":true"
        << ",\"audio_assets_exported\":0,\"voice_playback_validated\":false"
        << ",\"rendered_voice_selection_validated\":false,\"full_gameplay_loop\":false}\n";
    const std::string reportText = report.str();
    require(std::filesystem::create_directory(output), "Cannot create fresh voice output");
    try
    {
        writeConvertedResource(output / "engine/data_tables/character_voices.txt",
            std::vector<uint8_t>(text.begin(), text.end()));
        writeConvertedResource(output / "voice_conversion.json",
            std::vector<uint8_t>(reportText.begin(), reportText.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
