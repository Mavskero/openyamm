#include "Mm7RemainingMapResources.h"

#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumSourceBytes = 256 * 1024;
constexpr std::string_view FollowerApiPath = "Scripts/General/NPCFollowers.lua";
constexpr std::string_view FollowerApiSha256 =
    "d1798480623d05d954b6b82de04761b8e54d2dfbce237d2a150549368810c107";

struct SourceFingerprint
{
    std::string_view relativePath;
    std::string_view sha256;
};

constexpr std::array<SourceFingerprint, 16> MapSources = {{
    {"Scripts/Maps/7d05.lua", "a3b156f24dbe4dc588207ad5a7679f5cf292592ed5fe46f7e21151f7c506006c"},
    {"Scripts/Maps/7d08.lua", "2737a98b2b55e63d7d582513293481f033e89de23887e04ab0a4158872b95df3"},
    {"Scripts/Maps/7d25.lua", "bee5efadb2caa540706a0c25c14c16c1b9738e9ee2d37c258f12543cb233872d"},
    {"Scripts/Maps/7d27.lua", "18dda89718ffec927fd5d3a41edbc2076bde614308c967b38da90a4b71b7438e"},
    {"Scripts/Maps/7d29.lua", "238578473dbb5c73aca3e0676c923bd770a5b70c4737c4a95bdcb839778eab57"},
    {"Scripts/Maps/7d34.lua", "337b6a1ea224063902f1c4be28ffbf3033b03e515b07fe6d086171254ef9bd24"},
    {"Scripts/Maps/7d36.lua", "7c4edd7401adb6948e27badfa8c7428bdf8d1e5b8a3d5af325c9828ff60cc42e"},
    {"Scripts/Maps/7out01.lua", "3745eaa55307e086b103d564996f32065f23692d6409304eea3156d3207c84f0"},
    {"Scripts/Maps/7out02.lua", "95ddcdcfbfa41519d072652e02e6c94bd5365bdbe5041eadc00052ec6d718957"},
    {"Scripts/Maps/7out03.lua", "edf6af3e52f03fa8873ae6f5f216bb4c308da4243900c45cc1188668ee4595a8"},
    {"Scripts/Maps/7out05.lua", "648dc2a12561950eafcdd23e5189c19de28b6f32d8e5dec817559c736dc44db2"},
    {"Scripts/Maps/7out13.lua", "60080538d1153322bc2c8c55d19d177a99d6fc1ffaf4efc4f2c452370fca8793"},
    {"Scripts/Maps/7out15.lua", "3bdb718ed844ce2f5bdc59682962e1dc5673f7e7b61f1d68fc0f680b698ba483"},
    {"Scripts/Maps/out09.lua", "3c960b47020cc711d2d71cb04ad7632f8dc22625bb0408acba8fd8f4ffe81ce9"},
    {"Scripts/Maps/out11.lua", "fde19b1d7802bb934ec3ba1e4ebd651dd80a5b7f64e9763de596ee4b917b2896"},
    {"Scripts/Maps/out12.lua", "ca1e6b6bffe82644d4f7dc904ee6a5a33e45dda00a635b8606314f0af61b2cad"},
}};

constexpr std::array<SourceFingerprint, 2> GlobalSources = {{
    {"Scripts/Global/StdQuestsFollowers.lua",
        "36c5e4a5830f975febe007042a8b52600bd97f0a53bc726d4f0193d94fa616a8"},
    {"Scripts/Global/PromotionTopics.lua",
        "6266dd3fc45736d60901f0e3c4f98e7b4dcb0c2f4bfca6dd816dabc371d94014"},
}};

constexpr std::string_view GuildOverlay = R"OY_MM7_GUILD(
AppendMapEvent(376, function(continueStep)
    if continueStep ~= nil then return end
    if IsQBitSet(QBit(585)) or IsQBitSet(QBit(586)) then
        RemoveFollowerNpc(395)
    end
end)

-- The source removes the old counter-based event377, then installs this LoadMap policy instead.
-- Reusing its existing load registration preserves the other original castle load handlers.
RegisterEvent(377, nil, function()
    local invasion = IsQBitSet(QBit(526)) and IsQBitSet(QBit(695))
        and not (IsQBitSet(QBit(696)) or IsQBitSet(QBit(702)))
    evt.SetMonGroupBit(60, MonsterBits.Hostile, invasion)
    evt.SetMonGroupBit(60, MonsterBits.Invisible, not invasion)
    if invasion then
        SetValue(BankGold, 0)
        ClearQBit(QBit(693))
        ClearQBit(QBit(694))
    end
end)

RegisterMapOnLeaveEvent(65477, nil, function()
    -- The original fourth field is nonzero InvisibleAsDead in the merged MM8 event layout.
    if IsQBitSet(QBit(695)) and evt.CheckMonstersKilled(ActorKillCheck.Group, 60, 0, true) then
        SetQBit(QBit(696))
        if IsQBitSet(QBit(697)) then
            SetQBit(QBit(702))
            ClearQBit(QBit(695))
        else
            ClearQBit(QBit(702))
            SetQBit(QBit(695))
        end
    end
end)

RegisterRestFoodCostHook(65478, nil, function()
    if IsQBitSet(QBit(610)) then evt.SetHookRestFoodCost(0) end
end)
)OY_MM7_GUILD";

constexpr std::string_view HarmondaleOverlay = R"OY_MM7_HARM(
AppendMapEvent(37, function(continueStep)
    if continueStep ~= nil then return end
    RemoveFollowerNpc(416)
    RemoveFollowerNpc(417)
end)

local invasionTimeKey = "MMerge.Harmondale.InvasionTime."
local function invasionDeadline(now)
    -- Source mapvars.InvasionTime is a persistent int64 game-tick timestamp. Three int32 map variables
    -- retain its integer value without moving map-local state into the global save namespace.
    if evt.GetMapVar(invasionTimeKey .. "Initialized", 0) == 0 then
        local deadline = now + 14 * 24 * 60 * 256
        evt.SetMapVar(invasionTimeKey .. "Low", deadline % 2097152)
        evt.SetMapVar(invasionTimeKey .. "Middle", (deadline // 2097152) % 2097152)
        evt.SetMapVar(invasionTimeKey .. "High", deadline // 4398046511104)
        evt.SetMapVar(invasionTimeKey .. "Initialized", 1)
        return deadline
    end
    return evt.GetMapVar(invasionTimeKey .. "High", 0) * 4398046511104
        + evt.GetMapVar(invasionTimeKey .. "Middle", 0) * 2097152
        + evt.GetMapVar(invasionTimeKey .. "Low", 0)
end

local function setFactionBit()
    if IsQBitSet(QBit(611)) or IsQBitSet(QBit(612)) then
        SetQBit(QBit(608))
    else
        ClearQBit(QBit(608))
    end
end

local previousCastleArrival = evt.map[50]
RegisterEvent(50, nil, function(continueStep, ...)
    if continueStep == nil then setFactionBit() end
    if previousCastleArrival ~= nil then previousCastleArrival(continueStep, ...) end
    if continueStep ~= nil then return end

    -- Source keeps event50's original transaction and appends this strict two-week deadline.
    if (IsQBitSet(QBit(693)) or IsQBitSet(QBit(694)))
        and not (IsQBitSet(QBit(702)) or IsQBitSet(QBit(695))) then
        local now = evt.GameTimeTicks()
        if invasionDeadline(now) < now then
            SetQBit(QBit(695))
            evt.SetMonGroupBit(60, MonsterBits.Hostile, true)
            evt.SetMonGroupBit(60, MonsterBits.Invisible, false)
            SetValue(BankGold, 0)
            evt.SpeakNPC(437)
        end
    end
end)
)OY_MM7_HARM";

constexpr std::string_view ScavengerOverlay = R"OY_MM7_ADVERT(
local advertisementKey = "MMerge.Scavenger.GotAdvertisment"
RegisterEvent(65478, nil, function()
    local yellow, red = evt.GetEnemyDetectorState()
    if evt.IsPartyFlying() or yellow or red then return end
    local x, y = evt.GetPartyPosition()
    local dx, dy = advertisementX - x, advertisementY - y
    if dx * dx + dy * dy >= 4000 * 4000 then return end

    evt.SetMapVar(advertisementKey, 1)
    evt.StopCurrentMapTimer()
    evt.ForPlayer(0)
    AddValue(InventoryItem(774), 774)
    evt.SetNPCGreeting(649, 332)
    evt.SpeakNPC(649)
end)

RegisterMapAfterLoadEvent(65477, nil, function()
    if advertisementAtHarmondale then setFactionBit() end
    if evt.GetMapVar(advertisementKey, 0) ~= 0 or IsQBitSet(QBit(519)) then return end
    if advertisementAtHarmondale then
        evt.ForPlayer(Players.All)
        if HasItem(774) then return end
    end
    -- Original Timer(Catch, false, const.Minute * 3): default one-minute period, absolute minute3.
    evt.StartMapTimer(65478, 1, 3)
end)
)OY_MM7_ADVERT";

constexpr std::string_view VampireOverlay = R"OY_MM7_VAMPIRE(
RegisterMapAfterLoadEvent(65477, nil, function()
    -- Original HostileTxt row91 describes the three-monster family, not monster ID91.
    for monster = 271, 273 do evt.SetMonsterRelation(monster, 0, 0) end
    evt.SetMonGroupBit(56, MonsterBits.Hostile, true)
    evt.SetMonGroupBit(55, MonsterBits.Hostile, IsQBitSet(QBit(611)))
end)

RegisterNpcExitHook(65478, nil, function(context)
    if context.npcId ~= 461 or IsQBitSet(QBit(761)) then return end
    local x, y, z = evt.GetPartyPosition()
    evt.SummonMonsters(3, 3, 5, x, y, z + 400, 59)
    evt.SetMonGroupBit(59, MonsterBits.Hostile, true)
end)
)OY_MM7_VAMPIRE";

constexpr std::string_view ShoalsOverlay = R"OY_MM7_SHOALS(
-- Rendering overrides are map-visit state; they never replace the Character portrait in a save.
RegisterMapAfterLoadEvent(65477, nil, function()
    for _, player in ipairs(PartyMembers()) do evt.SetTemporaryPartyPortrait(player, 30) end
end)
RegisterMapOnLeaveEvent(65478, nil, function()
    for _, player in ipairs(PartyMembers()) do evt.SetTemporaryPartyPortrait(player, nil) end
end)
RegisterGameplayActionHook(65479, nil, function(context)
    local action = context.actionId
    if action == 133 or action == 105
        or context.heldItemId == 1406 and action ~= 120 and action ~= 12 then
        evt.PlaySound(27)
        evt.StatusText(shoalsRestrictedText)
        evt.SetHookBlocked(true)
    end
end)
-- The original Z>3900 exit is already provided by the separate authenticated Shoals map overlay.
)OY_MM7_SHOALS";

constexpr std::string_view ObeliskOverlay = R"OY_MM7_OBELISK(
local obeliskTreasureIndex = -1
RegisterEvent(65478, nil, function()
    if obeliskTreasureIndex >= 0 then
        evt.SetMapDecorationVisible(obeliskTreasureIndex, evt.GetRuntimeVariable(Hour) == 0)
    end
end)
RegisterMapAfterLoadEvent(65477, nil, function()
    obeliskTreasureIndex = evt.FindMapDecorationByEventId(170)
    if obeliskTreasureIndex < 0 then return end
    for bit = 676, 689 do
        if not IsQBitSet(QBit(bit)) then return end
    end
    -- Source Timer(SetTrViz, const.Hour / 2, true) fires once immediately, then every half hour.
    evt.StartMapTimer(65478, 30, true)
end)
)OY_MM7_OBELISK";

constexpr std::string_view EarlyBlasterOverlay = R"OY_MM7_BLASTER(
RemoveMapEvent(1)
RegisterEvent(1, nil, function()
    if IsQBitSet(QBit(775)) or not (IsQBitSet(QBit(616)) or IsQBitSet(QBit(635))) then return end
    SetQBit(QBit(775))
    evt.SetNPCGreeting(462, IsQBitSet(QBit(616)) and 316 or 317)
    -- Release the previous mouse item through ordinary custody, then publish the identified source item.
    evt.ReplaceMouseItem(866, {identified = true})
    evt.SpeakNPC(462)
end)
RegisterMapAfterLoadEvent(65477, nil, function()
    evt.DelayMapEvent(1, 1)
end)
)OY_MM7_BLASTER";

constexpr std::string_view FollowerOverlay = R"OY_MM7_FOLLOWERS(
local function removeRescuedDwarves()
    if IsQBitSet(QBit(610)) then
        for npc = 399, 405 do RemoveFollowerNpc(npc) end
    end
end
AppendGlobalEvent(858, function(continueStep)
    if continueStep == nil then removeRescuedDwarves() end
end)
for eventId = 859, 865 do
    local npc = 399 + eventId - 859
    AppendGlobalEvent(eventId, function(continueStep)
        if continueStep == nil then AddFollowerNpc(npc) end
    end)
end
RegisterGlobalNpcEnterHook(65526, nil, function(context)
    if context.npcId == 398 then removeRescuedDwarves() end
end)

local function removeDeliveredLoren()
    if not IsQBitSet(QBit(1695)) then RemoveFollowerNpc(410) end
    if not IsQBitSet(QBit(1696)) then RemoveFollowerNpc(411) end
end
for _, eventId in ipairs({875, 876, 885, 886}) do
    AppendGlobalEvent(eventId, function(continueStep)
        if continueStep == nil then removeDeliveredLoren() end
    end)
end
AppendGlobalEvent(884, function(continueStep)
    if continueStep == nil and IsQBitSet(QBit(1696)) then AddFollowerNpc(411) end
end)
AppendGlobalEvent(891, function(continueStep)
    if continueStep ~= nil then return end
    RemoveFollowerNpc(416)
    AddFollowerNpc(417)
end)
AppendGlobalEvent(893, function(continueStep)
    if continueStep ~= nil then return end
    RemoveFollowerNpc(417)
    AddFollowerNpc(416)
end)
)OY_MM7_FOLLOWERS";

constexpr std::string_view GolemOverlay = R"OY_MM7_GOLEM(
AppendGlobalEvent(842, function(continueStep)
    if continueStep == nil then AddFollowerNpc(395) end
end)
)OY_MM7_GOLEM";

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path checkedPath(const std::filesystem::path &root, std::string_view relativePath)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relativePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path),
        "MM7 remaining-map source escapes the game source root: " + std::string(relativePath));
    return path;
}

std::string sha256(const void *pBytes, size_t size)
{
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid MM7 remaining-map fingerprint payload size");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(pBytes, static_cast<CC_LONG>(size), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return digest.str();
}

std::optional<std::string> authenticateSource(const std::filesystem::path &root, const SourceFingerprint &source)
{
    const std::filesystem::path candidate = root / source.relativePath;
    if (std::filesystem::symlink_status(candidate).type() == std::filesystem::file_type::not_found)
    {
        return std::nullopt;
    }
    const std::filesystem::path path = checkedPath(root, source.relativePath);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= MaximumSourceBytes,
        "Invalid MM7 remaining-map source size: " + std::string(source.relativePath));
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(script.data(), static_cast<std::streamsize>(size))),
        "Cannot read MM7 remaining-map source: " + std::string(source.relativePath));
    const std::string digest = sha256(script.data(), script.size());
    require(digest == source.sha256, "Unsupported MM7 remaining-map source fingerprint: "
        + std::string(source.relativePath) + "; review this Merge version before enabling its projection");
    return digest;
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

struct SourceLabel
{
    std::string text;
    std::string hash;
};

SourceLabel loadShoalsLabel(const std::filesystem::path &root, bool chineseText)
{
    const std::filesystem::path archive = checkedPath(root,
        chineseText ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod");
    const std::vector<uint8_t> bytes = readLodText(archive, "global.txt");
    SourceLabel label;
    label.hash = sha256(bytes.data(), bytes.size());
    size_t restoredGlyphs = 0;
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(
        decodeLegacyText(bytes, chineseText, restoredGlyphs));
    require(table && table->getRowCount() > 2
        && table->getRow(0) == std::vector<std::string>{"Global Text", ""},
        "Unsupported Shoals global text header");
    std::set<uint32_t> ids;
    for (size_t index = 1; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (row.empty() || row == std::vector<std::string>{""}) continue;
        require(row.size() >= 2, "Incomplete Shoals global text row");
        std::string_view idText = row[0];
        while (!idText.empty() && idText.front() == ' ') idText.remove_prefix(1);
        while (!idText.empty() && idText.back() == ' ') idText.remove_suffix(1);
        uint32_t id = 0;
        const std::from_chars_result parsed = std::from_chars(idText.data(), idText.data() + idText.size(), id);
        require(!idText.empty() && parsed.ec == std::errc() && parsed.ptr == idText.data() + idText.size(),
            "Invalid Shoals global text ID");
        require(ids.insert(id).second, "Duplicate Shoals global text ID");
        if (id == 652)
        {
            require(row.size() == 2 && row[1].find_first_not_of(' ') != std::string::npos
                && std::none_of(row[1].begin(), row[1].end(), [](unsigned char value)
                    { return value < 32 || value == 127; }), "Invalid Shoals action text global.txt:652");
            label.text = row[1];
        }
    }
    require(ids.contains(652), "Missing Shoals action text global.txt:652");
    return label;
}

Mm7RemainingMapProfile makeProfile(const SourceFingerprint &source, const std::string &digest)
{
    Mm7RemainingMapProfile profile;
    profile.mapStem = std::filesystem::path(source.relativePath).stem().string();
    profile.sourceSha256 = digest;
    const std::string &stem = profile.mapStem;
    if (stem == "7d05" || stem == "7d25")
    {
        profile.addedEventIds = {65477};
        profile.addedAfterLoadEventIds = {65477};
    }
    else if (stem == "7d08" || stem == "7d27")
    {
        profile.requiredEventIds = {376};
    }
    else if (stem == "7d29")
    {
        profile.requiredEventIds = {376, 377};
        profile.addedEventIds = {65477, 65478};
        profile.addedOnLeaveEventIds = {65477};
        profile.addedRestFoodCostHookEventIds = {65478};
    }
    else if (stem == "7d34")
    {
        profile.requiredEventIds = {376, 377, 378, 379, 380, 381, 382};
    }
    else if (stem == "7d36" || stem == "out11")
    {
        profile.requiredEventIds = {501};
        if (stem == "out11") profile.requiredStringIds = {30};
    }
    else if (stem == "7out01")
    {
        profile.addedEventIds = {65477};
        profile.addedHouseTopicFilterHookEventIds = {65477};
    }
    else if (stem == "7out02" || stem == "7out03" || stem == "7out05" || stem == "out09")
    {
        profile.addedEventIds = {65477, 65478};
        profile.addedAfterLoadEventIds = {65477};
        if (stem == "7out02") profile.requiredEventIds = {37, 50};
        if (stem == "7out05") profile.addedNpcExitHookEventIds = {65478};
    }
    else if (stem == "7out13")
    {
        profile.addedEventIds = {82};
    }
    else if (stem == "7out15")
    {
        profile.requiredGlobalTextIds = {652};
        profile.addedEventIds = {65477, 65478, 65479};
        profile.addedAfterLoadEventIds = {65477};
        profile.addedOnLeaveEventIds = {65478};
        profile.addedGameplayActionHookEventIds = {65479};
    }
    else if (stem == "out12")
    {
        profile.requiredEventIds = {1};
        profile.addedEventIds = {65477};
        profile.addedAfterLoadEventIds = {65477};
        profile.removedOnLoadEventIds = {1};
    }
    return profile;
}
}

std::vector<Mm7RemainingMapProfile> loadMm7RemainingMapProfiles(
    const std::filesystem::path &source, bool chineseText)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "MM7 remaining-map source root is not a directory");
    std::vector<Mm7RemainingMapProfile> profiles;
    std::optional<std::string> followerApiHash;
    for (const SourceFingerprint &map : MapSources)
    {
        const std::optional<std::string> digest = authenticateSource(root, map);
        if (!digest) continue;
        Mm7RemainingMapProfile profile = makeProfile(map, *digest);
        const std::string &stem = profile.mapStem;
        if (stem == "7d08" || stem == "7d29" || stem == "7d34" || stem == "7out02")
        {
            if (!followerApiHash) followerApiHash = authenticateSource(root, {FollowerApiPath, FollowerApiSha256});
            if (!followerApiHash) continue;
            profile.followerApiSourceSha256 = *followerApiHash;
        }
        if (stem == "7out15")
        {
            const SourceLabel baseLabel = loadShoalsLabel(root, false);
            const SourceLabel label = chineseText ? loadShoalsLabel(root, true) : baseLabel;
            profile.globalTexts.emplace(652, label.text);
            profile.globalTextSha256 = label.hash;
            profile.baseGlobalTextSha256 = baseLabel.hash;
        }
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::string makeMm7RemainingMapOverlay(const Mm7RemainingMapProfile &profile,
    const std::unordered_map<uint16_t, std::string> &strings)
{
    const std::string &stem = profile.mapStem;
    std::string lua = "-- Authenticated Merge MM7 finite171 projection: " + stem + ".lua\n";
    if (stem == "7d05")
    {
        lua += R"OY_MM7_ARENA(
RegisterMapAfterLoadEvent(65477, nil, function()
    evt.SetNPCTopic(639, 0, 704)
    evt.SetNPCTopic(639, 2, 0)
end)
-- NoSave/NoLloyd/IsArena and door flags are projected by the authenticated indoor rules sidecar.
)OY_MM7_ARENA";
    }
    else if (stem == "7d08")
    {
        lua += R"OY_MM7_LOREN(
AppendMapEvent(376, function(continueStep)
    if continueStep == nil and IsQBitSet(QBit(1695)) then AddFollowerNpc(410) end
end)
)OY_MM7_LOREN";
    }
    else if (stem == "7d25")
    {
        lua += R"OY_MM7_DARK(
RegisterMapAfterLoadEvent(65477, nil, function()
    if IsQBitSet(QBit(612)) then
        for _, group in ipairs({57, 56, 55}) do evt.SetMonGroupBit(group, MonsterBits.Hostile, true) end
    end
end)
)OY_MM7_DARK";
    }
    else if (stem == "7d27")
    {
        lua += R"OY_MM7_LOST(
RegisterEvent(376, nil, function()
    evt.ForPlayer(Players.All)
    if not HasItem(1463) and evt.GetHeldItemId() ~= 1463 then
        evt.ForPlayer(0)
        AddValue(InventoryItem(1463), 1463)
        evt.SetSprite(20, true, "0")
        SetQBit(QBit(752))
        evt.SetFacetBit(1, FacetBits.Untouchable, true)
        evt.SetFacetBit(1, FacetBits.Invisible, true)
    end
    evt.SpeakNPC(626)
end)
)OY_MM7_LOST";
    }
    else if (stem == "7d29")
    {
        lua += GuildOverlay;
    }
    else if (stem == "7d34")
    {
        lua += R"OY_MM7_DWARVES(
for eventId = 376, 382 do
    local npc = eventId == 382 and 399 or 400 + eventId - 376
    AppendMapEvent(eventId, function(continueStep)
        if continueStep ~= nil then return end
        evt.ForPlayer(Players.All)
        if HasItem(1431) then AddFollowerNpc(npc) end
    end)
end
)OY_MM7_DWARVES";
    }
    else if (stem == "7d36")
    {
        lua += R"OY_MM7_TUNNELS(
RegisterEvent(501, nil, function()
    evt.MoveToMap(20890, -3119, 5, 896, 0, 0, 0, 3, "out12.odm")
end)
)OY_MM7_TUNNELS";
    }
    else if (stem == "7out01")
    {
        lua += R"OY_MM7_TAVERN(
RegisterHouseTopicFilter(65477, nil, function(context)
    if context.houseServiceType == 4 then
        evt.SetHookHouseTopics({HouseAction.TavernRentRoom, HouseAction.TavernBuyFood,
            HouseAction.OpenLearnSkillsMenu})
    end
end)
)OY_MM7_TAVERN";
    }
    else if (stem == "7out02" || stem == "7out03")
    {
        if (stem == "7out02") lua += HarmondaleOverlay;
        lua += stem == "7out02"
            ? "\nlocal advertisementAtHarmondale = true\nlocal advertisementX, advertisementY = -13115, 12497\n"
            : "\nlocal advertisementAtHarmondale = false\nlocal advertisementX, advertisementY = -10511, 6119\n";
        lua += ScavengerOverlay;
    }
    else if (stem == "7out05")
    {
        lua += VampireOverlay;
    }
    else if (stem == "7out13")
    {
        lua += R"OY_MM7_INN(
RegisterEvent(82, nil, function() evt.EnterHouse(1607) end)
SetMapContextAction(82, {kind = "enter_house", source = "merge_house", houseId = 1607})
)OY_MM7_INN";
    }
    else if (stem == "7out15")
    {
        const auto found = profile.globalTexts.find(652);
        require(found != profile.globalTexts.end() && !found->second.empty(), "Missing Shoals GlobalTxt652");
        lua += "local shoalsRestrictedText = " + luaStringLiteral(found->second) + "\n";
        lua += ShoalsOverlay;
    }
    else if (stem == "out09")
    {
        lua += ObeliskOverlay;
    }
    else if (stem == "out11")
    {
        const auto found = strings.find(30);
        require(found != strings.end() && !found->second.empty(), "Missing Stone City entrance STR30");
        lua += "RegisterEvent(501, nil, function()\n"
            "    evt.MoveToMap(179, -5386, 33, 240, 0, 0, 408, 2, \"7d24.blv\")\n"
            "end, " + luaStringLiteral(found->second) + ")\n";
    }
    else if (stem == "out12")
    {
        lua += EarlyBlasterOverlay;
    }
    else
    {
        throw std::runtime_error("Unknown MM7 remaining-map profile: " + stem);
    }
    return lua;
}

std::vector<Mm7RemainingGlobalProfile> loadMm7RemainingGlobalProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "MM7 remaining-global source root is not a directory");
    const std::optional<std::string> apiHash = authenticateSource(root, {FollowerApiPath, FollowerApiSha256});
    std::vector<Mm7RemainingGlobalProfile> profiles;
    for (const SourceFingerprint &global : GlobalSources)
    {
        const std::optional<std::string> digest = authenticateSource(root, global);
        if (!digest || !apiHash) continue;
        Mm7RemainingGlobalProfile profile;
        profile.sourcePath = global.relativePath;
        profile.sourceSha256 = *digest;
        profile.followerApiSourceSha256 = *apiHash;
        if (global.relativePath == GlobalSources[0].relativePath)
        {
            profile.requiredEventIds = {858, 859, 860, 861, 862, 863, 864, 865,
                875, 876, 884, 885, 886, 891, 893};
            profile.addedNpcEnterHookEventIds = {65526};
            profile.luaSource = FollowerOverlay;
        }
        else
        {
            profile.requiredEventIds = {842};
            profile.luaSource = GolemOverlay;
        }
        profiles.push_back(std::move(profile));
    }
    return profiles;
}
}
