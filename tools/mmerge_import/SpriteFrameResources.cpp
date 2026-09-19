#include "SpriteFrameResources.h"
#include "game/tables/SpriteTables.h"

#include <array>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
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
        "Invalid SFT integer: " + text);
    return value;
}

std::string name(std::string text)
{
    require(text.size() <= 12 && text.find_first_not_of(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos,
        "Unsafe SFT name");
    for (char &letter : text)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return text;
}
}

ConvertedSpriteFrames convertSpriteFrameRows(const std::vector<std::vector<std::string>> &rows,
    const std::set<std::string> *pSelectedGroups)
{
    require(!rows.empty() && rows.size() <= 65536, "Invalid SFT row count");
    for (size_t index = 0; index < rows.size(); ++index)
    {
        require(rows[index].size() == 24 && number(rows[index][0]) == index, "SFT index/width changed");
        for (size_t field = 4; field <= 20; ++field)
        {
            // DataTablesSupport explicitly treats an empty flag cell as false.
            require(rows[index][field] == "x" || rows[index][field] == "-" || rows[index][field].empty(),
                "Unknown SFT flag value");
        }
    }
    ConvertedSpriteFrames result;
    std::set<std::string> found;
    std::ostringstream yaml;
    yaml << "sprites:\n";
    const std::array<uint32_t, 17> bits = {2, 4, 16, 32, 64, 128, 256, 512, 1024, 2048,
        4096, 8192, 16384, 32768, 65536, 131072, 262144};
    for (size_t begin = 0; begin < rows.size();)
    {
        require(rows[begin][5] == "x", "SFT animation must begin with GroupStart");
        size_t end = begin + 1;
        while (end < rows.size() && rows[end][5] != "x")
        {
            ++end;
        }
        const std::string groupName = name(rows[begin][1]);
        require(groupName.empty() || !result.groupIds.contains(groupName), "Duplicate named SFT group");
        // DataTablesSupport builds this lookup in source order, including unnamed groups.
        result.groupIds[groupName] = uint16_t(begin);
        const bool selected = !pSelectedGroups || pSelectedGroups->contains(groupName);
        uint32_t duration = 0;
        for (size_t index = begin; index < end; ++index)
        {
            duration += number(rows[index][23]);
        }
        require(duration <= 268435455, "SFT animation duration overflows runtime ticks");
        if (selected)
        {
            found.insert(groupName);
            ++result.groupCount;
            result.frameCount += end - begin;
            yaml << "  - sprite_id: " << begin << "\n    sprite_name: " << std::quoted(groupName)
                << "\n    animation_length_raw: " << duration << "\n    frames:\n";
        }
        for (size_t index = begin; index < end; ++index)
        {
            const std::vector<std::string> &row = rows[index];
            const std::string texture = name(row[2]);
            require(!texture.empty(), "Empty SFT texture name");
            const uint32_t scale = number(row[3], 16777216);
            const uint32_t light = number(row[21], 32767);
            const int16_t palette = int16_t(number(row[22], 32767));
            uint32_t flags = index + 1 < end ? 1 : 0;
            for (size_t field = 4; field <= 20; ++field)
            {
                if (row[field] == "x")
                {
                    flags |= bits[field - 4];
                }
            }
            if (selected)
            {
                result.paletteIds.insert(palette);
                yaml << "      - texture_name: " << std::quoted(texture)
                    << "\n        frame_length_raw: " << number(row[23])
                    << "\n        flags: " << flags
                    << "\n        scale: " << std::setprecision(9) << double(scale) / 65536.0
                    << "\n        palette_id: " << palette
                    << "\n        glow_radius: " << light << '\n';
            }
        }
        begin = end;
    }
    require(!pSelectedGroups || found == *pSelectedGroups, "Selected SFT groups are missing");
    result.yaml = result.frameCount == 0 ? "sprites: []\n" : yaml.str();
    if (result.frameCount != 0)
    {
        Game::SpriteFrameTable table;
        std::string error;
        require(table.loadFromYaml(result.yaml, error), "Converted SFT failed runtime validation: " + error);
    }
    // An empty scene subset contributes no frames to the shared nonempty table. The source rows
    // and requested group identities are still validated above, even when no group was selected.
    return result;
}
}
