#include "EmeraldIslandTravelResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumScriptBytes = 256 * 1024;
constexpr size_t MaximumTableBytes = 4 * 1024 * 1024;
constexpr std::string_view RelativePath = "Scripts/Maps/7out03.lua";
constexpr std::string_view ExpectedSha256 =
    "edf6af3e52f03fa8873ae6f5f216bb4c308da4243900c45cc1188668ee4595a8";

constexpr std::string_view Overlay = R"OY_EMERALD(
-- Source 7out03.lua:4-8 selects location 101 until the party has completed Emerald Island.
-- House rules Boats[8] has four distinct locations, so source slot 4 is runtime route 4 at house 486.
RegisterMapOnLoadEvent(65489, nil, function()
    if IsQBitSet(QBit(519)) then
        evt.ClearTransportRouteOverride(486, 4)
    else
        evt.SetTransportRouteOverride(486, 4, {
            mapFileName = "7out01.odm",
            destinationName = emeraldIslandName,
            daysAvailable = {false, false, true, false, false, false, false},
            travelDays = 6,
            x = 12552,
            y = 800,
            z = 193,
            directionDegrees = 90,
            requiredQBit = 0,
            useMapStartPosition = false
        })
    end
end)
)OY_EMERALD";

std::string readSource(const std::filesystem::path &root, std::string_view relativePath, size_t maximumBytes)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relativePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Emerald Island travel source escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > maximumBytes)
    {
        throw std::runtime_error("Invalid Emerald Island travel source size: " + std::string(relativePath));
    }

    std::string contents(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(contents.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Emerald Island travel source: " + std::string(relativePath));
    }
    return contents;
}

void requireSourceRecord(const std::string &contents, std::string_view header, std::string_view key,
    std::string_view expectedRecord)
{
    // These audited records contain only ASCII. Match literal TSV fields, as the source table reader does.
    std::istringstream lines(contents);
    std::string line;
    size_t headerCount = 0;
    size_t recordCount = 0;
    bool inSection = false;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line == header)
        {
            ++headerCount;
            inSection = true;
            continue;
        }

        const std::string_view rowKey = std::string_view(line).substr(0, line.find('\t'));
        if (rowKey.empty() || rowKey.find_first_not_of("0123456789") != std::string_view::npos)
        {
            inSection = false;
        }
        if (inSection && rowKey == key)
        {
            ++recordCount;
            if (line != expectedRecord)
            {
                throw std::runtime_error("Unsupported Emerald Island transport source record: " + line);
            }
        }
    }
    if (headerCount != 1 || recordCount != 1)
    {
        throw std::runtime_error("Missing or ambiguous Emerald Island transport source record: " + std::string(key));
    }
}

std::string luaStringLiteral(std::string_view text)
{
    std::string literal = "\"";
    for (unsigned char character : text)
    {
        if (character == '\\' || character == '"')
        {
            literal += '\\';
            literal += character;
        }
        else if (character < 32 || character == 127)
        {
            literal += '\\';
            literal += char('0' + character / 100);
            literal += char('0' + (character / 10) % 10);
            literal += char('0' + character % 10);
        }
        else
        {
            literal += character;
        }
    }
    literal += '"';
    return literal;
}
}

std::optional<EmeraldIslandTravelProfile> loadEmeraldIslandTravelProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::exists(root / RelativePath))
    {
        return std::nullopt;
    }

    const std::string script = readSource(root, RelativePath, MaximumScriptBytes);
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != ExpectedSha256)
    {
        throw std::runtime_error("Unsupported Scripts/Maps/7out03.lua fingerprint; "
            "review this Merge version before enabling the Emerald Island travel overlay");
    }

    const std::string houseRules = readSource(root, "Data/Tables/House rules.txt", MaximumTableBytes);
    requireSourceRecord(houseRules, "Boats\tLoc1\tLoc2\tLoc3\tLoc4", "8", "8\t41\t42\t43\t44");

    const std::string locations = readSource(root, "Data/Tables/Transport Locations.txt", MaximumTableBytes);
    constexpr std::string_view LocationHeader = "#\tMap\tMonday\tTuesday\tWednesday\tThursday\tFriday\tSaturday"
        "\tSunday\tDaysCount\tX\tY\tZ\tDirection\tQBit";
    requireSourceRecord(locations, LocationHeader, "44",
        "44\t7Out06.odm\t-\t-\tx\t-\t-\t-\t-\t6\t19171\t-19722\t193\t1024\t0");
    requireSourceRecord(locations, LocationHeader, "101",
        "101\t7Out01.odm\t-\t-\tx\t-\t-\t-\t-\t6\t12552\t800\t193\t512\t0");

    return EmeraldIslandTravelProfile{"7out03", sourceSha256};
}

std::string makeEmeraldIslandTravelOverlay(std::string_view destinationName)
{
    if (destinationName.empty())
    {
        throw std::runtime_error("Missing Emerald Island source destination name");
    }
    return "local emeraldIslandName = " + luaStringLiteral(destinationName) + "\n" + std::string(Overlay);
}
}
