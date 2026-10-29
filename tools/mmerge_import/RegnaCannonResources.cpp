#include "RegnaCannonResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumScriptBytes = 256 * 1024;
constexpr std::string_view RelativePath = "Scripts/Maps/out13.lua";
constexpr std::string_view ExpectedSha256 =
    "590359a843d8d70929fc212450008b8e999b34123c2af5c2d8c0d8a6195403ac";

constexpr std::string_view Overlay = R"OY_REGNA_CANNON(
local prefix = "MMerge.Regna."
local function flag(name)
    return evt.GetMapVar(prefix .. name, 0) ~= 0
end
local function setFlag(name, value)
    evt.SetMapVar(prefix .. name, value and 1 or 0)
end

local fireballTargets = {
    {-13371, 13740, 2793}, {-13600, 13740, 2793}, {-13250, 13740, 2793}, {-13371, 13600, 2793},
    {-13371, 13820, 3000}, {-13371, 13740, 2250}, {-13371, 13800, 2500}, {-13100, 13740, 3000},
}
local meteorTargets = {
    {-19692, 14204}, {-16984, 15783}, {-12333, 18364}, {-13102, 20346}, {-15489, 18406},
    {-19300, 18374}, {-17229, 20297}, {-13235, 20616}, {-16787, 13839}, {-12748, 14383}, {-15107, 13092},
}
local deathBlossomTargets = {
    {-15743, 15989, 2731}, {-12022, 19402, 2728}, {-13168, 15608, 2725}, {-14622, 15778, 2724},
}

local function castFromCannon(spellId, targets)
    for _, target in ipairs(targets) do
        evt.CastSpell(spellId, 3, 4, -13312, 12864, 2432, target[1], target[2], target[3])
    end
end

local function showFleetEffects()
    for _, target in ipairs(meteorTargets) do
        evt.CastSpell(9, 3, 4, target[1], target[2], 4000, target[1], target[2], 0)
    end
    -- The source lightning targets are entries 2 through 8 of the meteor list, in the same order.
    for index = 2, 8 do
        local target = meteorTargets[index]
        evt.CastSpell(18, 3, 4, target[1], target[2], 4000, target[1], target[2], 0)
    end
    castFromCannon(43, deathBlossomTargets)
end

-- Remove the old permanent EVT timer before registering its replacement callback.
RemoveMapEvent(452)
RegisterEvent(452, nil, function()
    if not flag("FireSequenceOngoing") then
        evt.StopCurrentMapTimer()
        return
    end

    if not flag("CannonFired") then
        castFromCannon(6, fireballTargets)
        evt.Jump(1736, 256, 2000)
        evt.PlaySound(472, -13305, 12958)
        setFlag("CannonFired", true)
    elseif not flag("EffectsShown") then
        showFleetEffects()
        setFlag("EffectsShown", true)
    else
        if not IsQBitSet(QBit(37)) then
            SetQBit(QBit(37))
            evt.MoveNPC(64, 899)
            evt.MoveNPC(20, 900)
            evt.MoveNPC(21, 900)
            AddValue(History(13), 0)
        end
        evt.SetFacetBit(31, FacetBits.Invisible, 0)
        evt.SetFacetBit(31, FacetBits.Untouchable, 0)
        evt.SetFacetBit(30, FacetBits.Invisible, 1)
        evt.SetFacetBit(30, FacetBits.Untouchable, 1)
        setFlag("FireSequenceOngoing", false)
        setFlag("CannonFired", false)
        setFlag("EffectsShown", false)
    end
end)

ReplaceMapEvent(451, cannonHint, function()
    evt.ForPlayer(Players.All)
    if HasItem(662) then
        RemoveItem(662)
        evt.ForPlayer(Players.Current)
        evt.StatusText(firingText)
        evt.PlaySound(473, -12945, 12015)
        ClearQBit(QBit(224))
        setFlag("FireSequenceOngoing", true)
        -- Each source Timer call appends an independent instance, including repeated clicks during a sequence.
        evt.StartMapTimer(452, 1)
        return
    end
    evt.StatusText(noAmmoText)
end, cannonHint)
-- Missing named flags read as false; existing values survive a load exactly as the source AfterLoadMap defaults.
)OY_REGNA_CANNON";

constexpr std::string_view MigrationHandler = R"OY_REGNA_MIG(, nil, function()
    if evt.GetMapVar(prefix .. "SequenceVersion", 0) ~= 0 then
        return
    end
    if not IsQBitSet(QBit(37)) and IsAtLeast(MapVar(41), 1) then
        setFlag("FireSequenceOngoing", true)
        setFlag("CannonFired", IsAtLeast(MapVar(41), 2))
        setFlag("EffectsShown", IsAtLeast(MapVar(41), 3))
        -- The old in-flight ball resumes after one new minute; its previous timer alarm is not preserved.
        evt.StartMapTimer(452, 1)
    end
    SetValue(MapVar(41), 0)
    evt.SetMapVar(prefix .. "SequenceVersion", 1)
end)
)OY_REGNA_MIG";

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

std::optional<RegnaCannonProfile> loadRegnaCannonProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path candidate = root / RelativePath;
    if (!std::filesystem::exists(candidate))
    {
        return std::nullopt;
    }

    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Regna cannon source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Regna cannon source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Regna cannon source script");
    }

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
        throw std::runtime_error("Unsupported Scripts/Maps/out13.lua fingerprint; "
            "review this Merge version before enabling the Regna cannon overlay");
    }

    return RegnaCannonProfile{"out13", sourceSha256};
}

std::string makeRegnaCannonOverlay(std::string_view cannonHint, std::string_view firingText,
    std::string_view noAmmoText)
{
    if (cannonHint.empty() || firingText.empty() || noAmmoText.empty())
    {
        throw std::runtime_error("Missing Regna cannon source hint or status text");
    }
    return "local cannonHint = " + luaStringLiteral(cannonHint)
        + "\nlocal firingText = " + luaStringLiteral(firingText)
        + "\nlocal noAmmoText = " + luaStringLiteral(noAmmoText) + "\n" + std::string(Overlay)
        + "\nRegisterMapAfterLoadEvent(" + std::to_string(RegnaCannonMigrationHookEventId)
        + std::string(MigrationHandler);
}
}
