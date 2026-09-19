#include "DimensionDoorResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumScriptBytes = 256 * 1024;

struct MapProfileSource
{
    std::string_view mapStem;
    std::string_view relativePath;
    std::string_view sha256;
    std::string_view luaSource;
};

constexpr std::string_view Out01Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge Dimension Door activation site only.
RegisterOutdoorStepHook(65490, nil, function(context)
    if context.tileX == 63 and context.tileY == 59 then
        evt.ActivateDimensionDoor()
    end
end)
)OY_DIM_DOOR";

constexpr std::string_view Out07Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge Dimension Door activation sites only.
local function activateDimensionDoor()
    evt.ActivateDimensionDoor()
end

ReplaceMapEvent(500, nil, activateDimensionDoor)
RegisterOutdoorStepHook(65490, nil, function(context)
    if context.tileX == 43 and context.tileY == 98 then
        activateDimensionDoor()
    end
end)
)OY_DIM_DOOR";

constexpr std::string_view Outb3Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge Dimension Door activation sites only.
-- The unrelated extra-terrain sound replacement is outside this selective profile.
local function activateDimensionDoor()
    evt.ActivateDimensionDoor()
end

ReplaceMapEvent(105, nil, activateDimensionDoor)
RegisterOutdoorStepHook(65490, nil, function(context)
    if context.tileX == 89 and context.tileY == 47 then
        activateDimensionDoor()
    end
end)
)OY_DIM_DOOR";

constexpr std::string_view SevenOut04Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge Dimension Door activation sites only.
local function activateDimensionDoor()
    evt.ActivateDimensionDoor()
end

ReplaceMapEvent(504, nil, activateDimensionDoor)
RegisterOutdoorStepHook(65490, nil, function(context)
    if context.tileX == 84 and context.tileY == 94 then
        activateDimensionDoor()
    end
end)
SetMapMetadata({
    outdoorModelEvents = {ClL1_W = 504},
})
)OY_DIM_DOOR";

constexpr std::string_view Out09Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge Dimension Door activation sites only.
local function inDimensionDoorRange()
    local x, y = evt.GetPartyPosition(true)
    local dx = -5121 - x
    local dy = 98 - y
    return dx * dx + dy * dy < 1500 * 1500
end

local function activateDimensionDoorInRange()
    if inDimensionDoorRange() then
        evt.ActivateDimensionDoor()
    end
end

ReplaceMapEvent(6, nil, function()
    evt.ActivateDimensionDoor()
end)
RegisterEvent(65490, nil, activateDimensionDoorInRange)
evt.meta.map.timers = evt.meta.map.timers or {}
table.insert(evt.meta.map.timers, {
    eventId = 65490,
    sourceEventId = 65490,
    origin = "native",
    scheduleKind = "interval",
    repeating = true,
    intervalGameMinutes = 1,
    startMinute = 3,
})
)OY_DIM_DOOR";

constexpr std::string_view Oute3Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge Dimension Door activation event only.
ReplaceMapEvent(140, nil, function()
    if not evt.Cmp(MapVar(50), 1) then
        evt.ActivateDimensionDoor()
    end
end)
)OY_DIM_DOOR";

constexpr std::string_view Out02Overlay = R"OY_DIM_DOOR(
-- OpenYAMM translation of the audited MMerge final-quest portal range only.
local prefix = "MMerge.CrossContinents."

local function inBreachRange()
    if evt.GetGlobalVar(prefix .. "GotFinalQuest", 0) == 0 then
        return false
    end

    local x, y = evt.GetPartyPosition(true)
    local dx = 15103 - x
    local dy = -9759 - y
    return dx * dx + dy * dy < 600 * 600
end

RegisterOutdoorStepHook(65490, nil, function()
    if inBreachRange() then
        evt.ActivateDimensionDoor()
    end
end)

RegisterTownPortalHook(65491, nil, function()
    if not inBreachRange() then
        return
    end

    evt.SetHookBlocked(true)
    local destination = "BrAlvar.odm"
    if evt.GetGlobalVar(prefix .. "QuestFinished", 0) ~= 0 then
        destination = "Breach.odm"
    end
    evt.MoveToMap(0, 0, 0, 0, 0, 0, 0, 0, destination)
end)
)OY_DIM_DOOR";

constexpr std::array<MapProfileSource, 7> MapSources = {{
    {"out01", "Scripts/Maps/out01.lua",
        "5e5caae49ba76d8128181a695165384e4a8a526b56247fe58f4536659d2eb380", Out01Overlay},
    {"out07", "Scripts/Maps/out07.lua",
        "3eb167089254b740051b4242d031065e7c68dc3fd882eb9dd83d9d33677549c4", Out07Overlay},
    {"outb3", "Scripts/Maps/outb3.lua",
        "a90314e62c869d4672eea390dffa7ce1bd312652f2e0d6107d8133a40dad7448", Outb3Overlay},
    {"7out04", "Scripts/Maps/7out04.lua",
        "a3c2c6008c3cd49ee99548b1adae5af8f792e83d6cfb77adc63815a1e9311cb3", SevenOut04Overlay},
    {"out09", "Scripts/Maps/out09.lua",
        "3c960b47020cc711d2d71cb04ad7632f8dc22625bb0408acba8fd8f4ffe81ce9", Out09Overlay},
    {"oute3", "Scripts/Maps/oute3.lua",
        "731955ac2b119b5ed6f32d7a01ff08ea82fac4cb7b60596b9fca80250f846e08", Oute3Overlay},
    {"out02", "Scripts/Maps/out02.lua",
        "36ae1b344bed64ccdeca91d969b847247db18299dc501133758b8baa521c83cf", Out02Overlay},
}};

bool pathEscapesRoot(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const std::filesystem::path relative = path.lexically_relative(root);
    return relative.empty() || relative.is_absolute() || *relative.begin() == "..";
}

std::string readValidatedScript(
    const std::filesystem::path &root,
    const std::filesystem::path &relativePath,
    std::string_view expectedSha256)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relativePath);
    if (pathEscapesRoot(path, root))
    {
        throw std::runtime_error("Dimension Door source script escapes game source root: " + relativePath.string());
    }
    if (!std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Dimension Door source is not a regular file: " + relativePath.string());
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Dimension Door source script size: " + relativePath.string());
    }

    std::string script(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Dimension Door source script: " + relativePath.string());
    }

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    if (sourceSha256 != expectedSha256)
    {
        throw std::runtime_error(
            "Unsupported Dimension Door source fingerprint: " + relativePath.string()
            + "; review this Merge version before enabling the activation overlay");
    }
    return sourceSha256;
}
}

std::optional<DimensionDoorProfile> loadDimensionDoorProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path travelPath = root / "Scripts/General/2_OutdoorTravels.lua";
    if (!std::filesystem::exists(travelPath))
    {
        return std::nullopt;
    }

    DimensionDoorProfile profile;
    profile.sourceSha256 = readValidatedScript(
        root,
        "Scripts/General/2_OutdoorTravels.lua",
        "0691bb4fc1b5684000753ad6bea86e98cfc5f371cd8a9e346cd88f702de810d2");

    const std::filesystem::path timerPath = root / "Scripts/Core/timers.lua";
    if (!std::filesystem::exists(timerPath))
    {
        throw std::runtime_error("Missing Scripts/Core/timers.lua for Dimension Door activation profile");
    }
    profile.timerSourceSha256 = readValidatedScript(
        root,
        "Scripts/Core/timers.lua",
        "04108d079d9605cdeb90601e8e9acf096f697e17e19ccaec8c15eb78380f2213");

    for (const MapProfileSource &mapSource : MapSources)
    {
        const std::filesystem::path mapPath = root / mapSource.relativePath;
        if (!std::filesystem::exists(mapPath))
        {
            continue;
        }
        profile.maps.push_back({
            std::string(mapSource.mapStem),
            readValidatedScript(root, mapSource.relativePath, mapSource.sha256),
            std::string(mapSource.luaSource),
        });
    }

    return profile;
}
}
