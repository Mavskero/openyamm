#include "Mm8RemainingMapResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumSourceBytes = 256 * 1024;
constexpr uint16_t LoadHookEventId = 65480;

struct MapSource
{
    std::string_view mapStem;
    std::string_view sha256;
};

constexpr std::array<MapSource, 8> MapSources = {{
    {"d38", "b2efbaaa3de90656d045a200faadd6ccb040471884f20acd2d2250e18949fd1f"},
    {"d39", "f5635c3898d7edb1af6be7f62d57193640e0c30dfb4c6221587d4c3370ad1538"},
    {"d42", "c3d87326a811154bd465a820e9be922eda2c50fee35a7cda8aaa6684ec60c2c0"},
    {"out01", "5e5caae49ba76d8128181a695165384e4a8a526b56247fe58f4536659d2eb380"},
    {"out04", "c79e7683a6a00f19cb919ce3f54a63679f86e63655c79621749a01181aa6d8a6"},
    {"out06", "eb3c951ace5ab257497f262473fd6cbd1465a040ba30330101894993fad701aa"},
    {"out05", "65e487213f643cd9d876dc0173adf17fa0907a7e84670b93145b42908ece9505"},
    {"out07", "3eb167089254b740051b4242d031065e7c68dc3fd882eb9dd83d9d33677549c4"},
}};

constexpr std::string_view ClearLandOverlay = R"OY_MM8_CLEAR(
-- out05.lua does not remove the original event131. Core/evt.lua AfterProcessEvent invokes its Lua tail.
-- Preserve the original summons,75/200 checks and timer registration; complete155/158 on this same invocation.
AppendMapEvent(131, function(continueStep)
    if continueStep ~= nil then return end
    if not (IsQBitSet(QBit(22)) or IsQBitSet(QBit(155)))
        and evt.CheckMonstersKilled(ActorKillCheck.MonsterId, 189, 0, false)
        and evt.CheckMonstersKilled(ActorKillCheck.MonsterId, 190, 0, false)
        and evt.CheckMonstersKilled(ActorKillCheck.MonsterId, 191, 0, false) then
        SetQBit(QBit(155))
        evt.StatusText(mm8ClearLandTexts[1])
    end

    if not (IsQBitSet(QBit(21)) or IsQBitSet(QBit(158)))
        and evt.CheckMonstersKilled(ActorKillCheck.MonsterId, 42, 0, false)
        and evt.CheckMonstersKilled(ActorKillCheck.MonsterId, 43, 0, false)
        and evt.CheckMonstersKilled(ActorKillCheck.MonsterId, 44, 0, false) then
        SetQBit(QBit(158))
        evt.StatusText(mm8ClearLandTexts[2])
    end
end)
)OY_MM8_CLEAR";

constexpr std::string_view GemTreeOverlay = R"OY_MM8_GEMS(
-- out07.lua 42-116 replaces event455. Only the selected player's first matching inventory record is exchanged.
-- Source order follows Player.Items; it never searches another player, the mouse-held item or queued deliveries.
local mm8GemExchange = {
    [180] = 250, [2102] = 250,
    [181] = 183, [2060] = 183,
    [179] = 181,
    [997] = 656, [994] = 656, [186] = 656, [2056] = 656,
    [2064] = 655, [990] = 655, [183] = 655, [2061] = 655,
    [178] = 132,
    [998] = 271, [2059] = 271, [185] = 271,
    [2065] = -2000, [991] = -2000, [184] = -2000,
    [2058] = 0, [989] = 0, [182] = 0,
    [177] = 179,
}
local mm8GemIds = {
    177, 178, 179, 180, 181, 182, 183, 184, 185, 186,
    989, 990, 991, 994, 997, 998, 2056, 2058, 2059, 2060, 2061, 2064, 2065, 2102,
}
local mm8GemItemTypes = {20, 21, 22, 40, 43}

RegisterEvent(455, nil, function()
    local player = evt.GetCurrentPlayerIndex()
    if player < 0 then return end
    local itemId = evt.FindPartyMemberInventoryItem(player, mm8GemIds)
    if itemId == 0 then return end
    local value = mm8GemExchange[itemId]
    if value == nil then error("Gem tree selected an unregistered source gem") end

    evt.ForPlayer(player)
    if value > 0 then
        evt.Add(InventoryItem(value), value)
    elseif value < 0 then
        evt.Add(Gold, -value)
    else
        evt.GiveItem(3, mm8GemItemTypes[evt.RandomBetween(1, #mm8GemItemTypes)], 0)
    end
    if not RemovePlayerItem(player, itemId, 1) then
        error("Gem tree source inventory record disappeared")
    end
end)
)OY_MM8_GEMS";

std::optional<std::string> authenticateSource(const std::filesystem::path &root, const MapSource &source)
{
    const std::filesystem::path relativePath = std::filesystem::path("Scripts/Maps")
        / (std::string(source.mapStem) + ".lua");
    const std::filesystem::path candidate = root / relativePath;
    if (std::filesystem::symlink_status(candidate).type() == std::filesystem::file_type::not_found)
    {
        return std::nullopt;
    }
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("MM8 remaining-map source escapes the game source root: " + relativePath.string());
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumSourceBytes)
    {
        throw std::runtime_error("Invalid MM8 remaining-map source size: " + relativePath.string());
    }
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM8 remaining-map source: " + relativePath.string());
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != source.sha256)
    {
        throw std::runtime_error("Unsupported MM8 remaining-map fingerprint: " + relativePath.string()
            + "; review this Merge version before enabling its projection");
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

Mm8RemainingMapProfile makeProfile(const MapSource &source, const std::string &sha256)
{
    Mm8RemainingMapProfile profile;
    profile.mapStem = source.mapStem;
    profile.sourceSha256 = sha256;
    if (source.mapStem == "d38" || source.mapStem == "d39")
    {
        profile.addedAfterLoadEventIds = {LoadHookEventId};
    }
    else if (source.mapStem == "out01")
    {
        profile.addedOnLoadEventIds = {LoadHookEventId};
    }
    else if (source.mapStem == "out05")
    {
        profile.requiredEventIds = {131};
        profile.requiredStringIds = {70, 71};
    }
    else if (source.mapStem == "out07")
    {
        profile.requiredEventIds = {455};
    }
    else if (source.mapStem == "out04")
    {
        profile.tileSetFootsteps = {{0, 91, 52}, {5, 101, 62}, {6, 90, 51}};
    }
    else if (source.mapStem == "out06")
    {
        profile.tileSetFootsteps = {{5, 101, 62}};
        profile.terrainBitmap = Mm8TerrainBitmapRule{1, 12, "dirt", "gdtyl"};
    }
    else if (source.mapStem == "d42")
    {
        profile.deferredReason = "Source exit501 uses TownPortalControls.GetCurrentSwitch to select MM7/MM6/MM8. "
            "No ordinary cross-world ingress or retained origin-switch contract was found in the finite171 audit; "
            "the native GetCurrentContinent reports the active arena map and cannot invent that origin.";
    }
    return profile;
}
}

std::vector<Mm8RemainingMapProfile> loadMm8RemainingMapProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("MM8 remaining-map source root is not a directory");
    }
    std::vector<Mm8RemainingMapProfile> profiles;
    for (const MapSource &map : MapSources)
    {
        const std::optional<std::string> hash = authenticateSource(root, map);
        if (hash) profiles.push_back(makeProfile(map, *hash));
    }
    return profiles;
}

std::string makeMm8RemainingMapOverlay(const Mm8RemainingMapProfile &profile,
    const std::unordered_map<uint16_t, std::string> &strings)
{
    if (profile.mapStem == "d38" || profile.mapStem == "d39")
    {
        const bool water = profile.mapStem == "d38";
        return "-- Source AfterLoadMap repairs only the still-unreleased elemental lord's NPC residence.\n"
            "RegisterMapAfterLoadEvent(" + std::to_string(LoadHookEventId) + ", nil, function()\n"
            "    if not IsQBitSet(QBit(" + std::to_string(water ? 53 : 51) + ")) then\n"
            "        evt.MoveNPC(" + std::to_string(water ? 24 : 25) + ", "
            + std::to_string(water ? 662 : 663) + ")\n    end\nend)\n";
    }
    if (profile.mapStem == "out01")
    {
        return "-- Source out01.lua LoadMap refreshes this MM8 Town Portal visit on every ordinary entry.\n"
            "RegisterMapOnLoadEvent(" + std::to_string(LoadHookEventId)
            + ", nil, function() SetQBit(QBit(185)) end)\n";
    }
    if (profile.mapStem == "out05")
    {
        std::string overlay = "local mm8ClearLandTexts = {\n";
        for (uint16_t id : {uint16_t(70), uint16_t(71)})
        {
            const auto found = strings.find(id);
            if (found == strings.end() || found->second.empty())
                throw std::runtime_error("Missing MM8 out05 original localized STR" + std::to_string(id));
            overlay += "    " + luaStringLiteral(found->second) + ",\n";
        }
        return overlay + "}\n" + std::string(ClearLandOverlay);
    }
    if (profile.mapStem == "out07") return std::string(GemTreeOverlay);
    if (profile.mapStem == "out04" || profile.mapStem == "out06" || profile.mapStem == "d42") return {};
    throw std::runtime_error("Unsupported MM8 remaining-map projection: " + profile.mapStem);
}
}
