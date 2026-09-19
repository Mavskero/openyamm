#include "SurfaceResources.h"

#include <array>
#include <charconv>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
unsigned number(const std::string &text, unsigned limit)
{
    unsigned value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size() || value > limit)
    {
        throw std::runtime_error("Invalid TFT integer: " + text);
    }
    return value;
}
}

std::string convertHardwareWaterMaterials(const std::vector<std::vector<std::string>> &source,
    const std::string &miscTweaks)
{
    using Row = std::vector<std::string>;
    if (source.size() < 2 || source.size() > 65 || source.front() != Row{"SW texture", "HW texture prefix"})
    {
        throw std::runtime_error("Unrecognized hardware-water schema");
    }
    const auto patchOption = [&](const std::string &name, unsigned limit)
    {
        const std::string key = "Game.PatchOptions." + name;
        const std::regex assignment("\\s*Game\\.PatchOptions\\." + name + "\\s*=\\s*([0-9]+)\\s*");
        std::istringstream lines(miscTweaks);
        std::string line;
        unsigned value = 0;
        bool found = false;
        while (std::getline(lines, line))
        {
            line = line.substr(0, line.find("--"));
            if (line.find(key) == std::string::npos)
            {
                continue;
            }
            std::smatch match;
            if (found || !std::regex_match(line, match, assignment))
            {
                throw std::runtime_error("Hardware-water option is not one literal assignment: " + name);
            }
            found = true;
            value = number(match[1].str(), limit);
        }
        if (!found || value == 0)
        {
            throw std::runtime_error("Missing or zero hardware-water option: " + name);
        }
        return value;
    };
    const unsigned count = patchOption("HDWTRCount", 1000);
    const unsigned delay = patchOption("HDWTRDelay", 65535);
    const std::set<std::string> waterNames{"6wtrtyl", "7wtrtyl", "wtrtyl", "oiltyl", "tartyl"};
    std::set<std::string> seen;
    std::ostringstream yaml;
    yaml << "materials:\n";
    for (size_t index = 1; index < source.size(); ++index)
    {
        const Row &row = source[index];
        if (row.size() != 2 || (!waterNames.contains(row[0]) && row[0] != "lavtyl")
            || !seen.insert(row[0]).second || row[1].empty() || row[1].size() > 9
            || row[1].find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
        {
            throw std::runtime_error("Unsupported or duplicate hardware-water binding");
        }
        yaml << "  - id: merged_" << row[0] << "\n    semantic: " << (row[0] == "lavtyl" ? "lava" : "water")
             << "\n    applies_to: [terrain, face]\n    terrain_transition_overlay: true\n"
                "    match:\n      texture_names: [" << row[0] << "]\n    animation:\n      frames:\n";
        for (unsigned frame = 0; frame < count; ++frame)
        {
            yaml << "        - texture: " << row[1] << std::setw(3) << std::setfill('0') << frame
                 << "\n          ticks: " << delay << '\n';
        }
    }
    return yaml.str();
}

std::vector<std::vector<std::string>> convertTerrainRows(
    const std::vector<std::vector<std::string>> &source)
{
    using Row = std::vector<std::string>;
    const Row header{"#", "Name", "Id", "TileSet", "Section", "Burn", "Water", "Block", "Repulse", "Flat",
        "Wave", "NoDraw", "WaterTransition", "Transition", "ScrollDown", "ScrollUp", "ScrollLeft", "ScrollRight"};
    if (source.size() < 2 || source.size() > 65537 || source.front() != header)
    {
        throw std::runtime_error("Unrecognized MMMerge terrain schema");
    }
    std::vector<Row> result{{"index", "texture_name", "tile_id", "bitmap_id", "tileset", "variant", "flags"}};
    const std::array<unsigned, 13> flagBits{1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x200,
        0x400, 0x800, 0x1000, 0x2000};
    for (size_t index = 1; index < source.size(); ++index)
    {
        const Row &row = source[index];
        if (row.size() != header.size() || number(row[0], 65535) != index - 1
            || row[1].size() > 16)
        {
            throw std::runtime_error("Terrain rows must preserve valid names and contiguous IDs");
        }
        std::string name = row[1];
        for (char &letter : name)
        {
            if (letter >= 'A' && letter <= 'Z')
            {
                letter += 'a' - 'A';
            }
            if (!(letter >= 'a' && letter <= 'z') && !(letter >= '0' && letter <= '9')
                && letter != '_' && letter != '-')
            {
                throw std::runtime_error("Invalid terrain texture name");
            }
        }
        unsigned flags = 0;
        for (size_t column = 5; column < row.size(); ++column)
        {
            if (row[column] == "x" || row[column] == "X")
            {
                flags |= flagBits[column - 5];
            }
            else if (row[column] != "-")
            {
                throw std::runtime_error("Unrecognized terrain flag value");
            }
        }
        result.push_back({row[0], name, std::to_string(number(row[2], 65535)), "0",
            std::to_string(number(row[3], 65535)), std::to_string(number(row[4], 65535)), std::to_string(flags)});
    }
    return result;
}

std::vector<std::vector<std::string>> convertTextureFrameRows(
    const std::vector<std::vector<std::string>> &source)
{
    using Row = std::vector<std::string>;
    if (source.size() < 2 || source.size() > 32769 || source.front() != Row{"#", "Name", "Time", "GroupStart"})
    {
        throw std::runtime_error("Unrecognized MMMerge TFT schema");
    }
    std::vector<Row> result;
    size_t groupStart = 0;
    unsigned total = 0;
    for (size_t index = 1; index < source.size(); ++index)
    {
        const Row &row = source[index];
        if (row.size() != 4 || number(row[0], 32767) != index - 1 || row[1].empty()
            || row[1].size() > 12 || (row[3] != "x" && row[3] != "-") || (index == 1 && row[3] != "x"))
        {
            throw std::runtime_error("TFT rows must retain valid names, contiguous indices and group boundaries");
        }
        std::string name = row[1];
        for (char &letter : name)
        {
            if (letter >= 'A' && letter <= 'Z')
            {
                letter += 'a' - 'A';
            }
            if (!(letter >= 'a' && letter <= 'z') && !(letter >= '0' && letter <= '9')
                && letter != '_' && letter != '-')
            {
                throw std::runtime_error("Unsafe TFT texture name");
            }
        }
        if (row[3] == "x")
        {
            if (!result.empty())
            {
                result[groupStart][3] = std::to_string(total);
            }
            groupStart = result.size();
            total = 0;
        }
        const unsigned duration = number(row[2], 32767);
        total += duration;
        if (total > 32767)
        {
            throw std::runtime_error("TFT group duration exceeds source signed storage");
        }
        const unsigned flags = (row[3] == "x" ? 2 : 0)
            | (index + 1 < source.size() && source[index + 1].size() == 4 && source[index + 1][3] == "-" ? 1 : 0);
        // The legacy bitmap cache index is runtime-owned. Names are authoritative in the portable package.
        result.push_back({name, "0", std::to_string(duration), "0", std::to_string(flags)});
    }
    result[groupStart][3] = std::to_string(total);
    return result;
}
}
