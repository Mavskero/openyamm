#include "Mm8ReentryResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumSourceBytes = 256 * 1024;

struct MapSource
{
    std::string_view mapStem;
    std::string_view relativePath;
    std::string_view sha256;
};

constexpr std::array<MapSource, 2> MapSources = {{
    {"out02", "Scripts/Maps/out02.lua",
        "36ae1b344bed64ccdeca91d969b847247db18299dc501133758b8baa521c83cf"},
    {"pbp", "Scripts/Maps/pbp.lua",
        "417ee1d7c2767cc3888c8a42033a7378821a036065ac74c5eb4763729df0f5d3"},
}};

constexpr std::string_view CrystalOverlay = R"OY_CRYSTAL(
-- Source out02.lua 29-39: remember this entrance in the current map's saved state.
-- RegisterEvent retains the existing localized hint when no replacement hint is supplied.
RegisterEvent(504, nil, function()
    evt.ForPlayer(Players.All)
    if evt.GetMapVar("MMerge.CrystalOpened", 0) ~= 0 or HasItem(610) then
        evt.SetMapVar("MMerge.CrystalOpened", 1)
        evt.MoveToMap(-1024, -1626, 0, 520, 0, 0, 355, 1, "d10.blv")
    else
        evt.ForPlayer(Players.Current)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end)
)OY_CRYSTAL";

constexpr std::string_view PrisonOverlay = R"OY_PRISONS(
-- Source pbp.lua 4-50: each prison retains its own opened state until the map refills.
ReplaceMapEvent(502, prisonHints[1], function()
    evt.ForPlayer(Players.All)
    if evt.GetMapVar("MMerge.AirPrisonOpen", 0) ~= 0 or HasItem(629) then
        evt.SetMapVar("MMerge.AirPrisonOpen", 1)
        evt.MoveToMap(-733, -2563, -1051, 960, 0, 0, 341, 1, "d36.blv")
    else
        evt.ForPlayer(Players.Current)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end, prisonHints[1])

ReplaceMapEvent(503, prisonHints[2], function()
    evt.ForPlayer(Players.All)
    if evt.GetMapVar("MMerge.FirePrisonOpen", 0) ~= 0 or HasItem(629) then
        evt.SetMapVar("MMerge.FirePrisonOpen", 1)
        evt.MoveToMap(-128, 896, 1, 1536, 0, 0, 343, 1, "d37.blv")
    else
        evt.ForPlayer(Players.Current)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end, prisonHints[2])

ReplaceMapEvent(504, prisonHints[3], function()
    evt.ForPlayer(Players.All)
    if evt.GetMapVar("MMerge.WaterPrisonOpen", 0) ~= 0 or HasItem(629) then
        evt.SetMapVar("MMerge.WaterPrisonOpen", 1)
        evt.MoveToMap(2393, -10664, 1, 520, 0, 0, 342, 1, "d38.blv")
    else
        evt.ForPlayer(Players.Current)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end, prisonHints[3])

ReplaceMapEvent(505, prisonHints[4], function()
    evt.ForPlayer(Players.All)
    if evt.GetMapVar("MMerge.EarthPrisonOpen", 0) ~= 0 or HasItem(629) then
        evt.SetMapVar("MMerge.EarthPrisonOpen", 1)
        evt.MoveToMap(-2, 118, 1, 2047, 0, 0, 379, 1, "d39.blv")
    else
        evt.ForPlayer(Players.Current)
        evt.FaceAnimation(FaceAnimation.DoorLocked)
    end
end, prisonHints[4])
)OY_PRISONS";

std::optional<std::string> authenticateSource(const std::filesystem::path &root, const MapSource &source)
{
    const std::filesystem::path candidate = root / source.relativePath;
    if (std::filesystem::symlink_status(candidate).type() == std::filesystem::file_type::not_found)
    {
        return std::nullopt;
    }

    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Invalid MM8 reentry source path: " + std::string(source.relativePath));
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumSourceBytes)
    {
        throw std::runtime_error("Invalid MM8 reentry source size: " + std::string(source.relativePath));
    }

    std::string bytes(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(bytes.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM8 reentry source: " + std::string(source.relativePath));
    }

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != source.sha256)
    {
        throw std::runtime_error("Unsupported MM8 reentry source fingerprint: "
            + std::string(source.relativePath) + "; review this Merge version before enabling reentry");
    }
    return sourceSha256;
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

std::vector<Mm8ReentryProfile> loadMm8ReentryProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("MM8 reentry source root is not a directory");
    }

    std::vector<Mm8ReentryProfile> profiles;
    for (const MapSource &map : MapSources)
    {
        const std::optional<std::string> sourceSha256 = authenticateSource(root, map);
        if (sourceSha256)
        {
            profiles.push_back({std::string(map.mapStem), *sourceSha256});
        }
    }
    return profiles;
}

std::string makeMm8ReentryOverlay(const Mm8ReentryProfile &profile, const std::array<std::string, 4> &prisonHints)
{
    if (profile.mapStem == "out02")
    {
        return std::string(CrystalOverlay);
    }
    if (profile.mapStem != "pbp")
    {
        throw std::runtime_error("Unsupported MM8 reentry map: " + profile.mapStem);
    }

    std::string lua = "local prisonHints = {\n";
    for (const std::string &hint : prisonHints)
    {
        if (hint.empty())
        {
            throw std::runtime_error("Missing MM8 prison reentry source text from pbp.str slots 51-54");
        }
        lua += "    " + luaStringLiteral(hint) + ",\n";
    }
    return lua + "}\n" + std::string(PrisonOverlay);
}
}
