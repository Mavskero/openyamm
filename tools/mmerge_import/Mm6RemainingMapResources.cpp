#include "Mm6RemainingMapResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumSourceBytes = 256 * 1024;

struct SourceFingerprint
{
    std::string_view relativePath;
    std::string_view sha256;
};

constexpr std::array<SourceFingerprint, 20> MapSources = {{
    {"Scripts/Maps/6d02.lua", "0562a88b8479936c25be77408644c972b0ac7b5be5f1951e368f9e34afaddebc"},
    {"Scripts/Maps/6d03.lua", "d130939c3a5c9e11bb73d742b19cf61521e8734efb24fe50c3defd533fa294bd"},
    {"Scripts/Maps/6t3.lua", "7d6c06f4218e80327fe155c6da707635b7acef1207d54f55bbd652c42144a08d"},
    {"Scripts/Maps/6t8.lua", "b0c950ec823a52c9df4b23dac3728f43b5bd5cda1276bd65b056314729544c2f"},
    {"Scripts/Maps/6d08.lua", "7ca9d31be514615f9d09bf8c2ffb6c9afd5560d82a3e9e8fc7da4eb4b805eccb"},
    {"Scripts/Maps/6d13.lua", "24cc648544bd7b1e599cbf0173e6c192a24ec52212e0b13e9583d3bf3df667c2"},
    {"Scripts/Maps/6t7.lua", "737624107df262f8edccc51c47d8d94237cb3947a62bd376ed8f646616668af4"},
    {"Scripts/Maps/cd2.lua", "6b12c52fa06115c40666618b7b29245505641a8c6fadc9780278ac0f0e8eda24"},
    {"Scripts/Maps/cd3.lua", "dc66d3fedc93d1d801e434174adf38dbf196add25172e25778474c3df2e14dc6"},
    {"Scripts/Maps/outa2.lua", "00929bb027d7c3b0adafcc5f399fc80713a6d25c153a13e1a131cb2104895257"},
    {"Scripts/Maps/outa3.lua", "3339531b2ed4bed66d74409c2c9de9aab133a15672b58eaaa07d208558e5df83"},
    {"Scripts/Maps/outb1.lua", "c3cff5f57a44085f5bb8cb4017ef4b5826611d2a1a299c287d2d8524c3ecf312"},
    {"Scripts/Maps/outb2.lua", "f7bea5436ce2366d07b201a831b0d0121ca7e09ceca8783465755b2e8f4dafa8"},
    {"Scripts/Maps/outb3.lua", "a90314e62c869d4672eea390dffa7ce1bd312652f2e0d6107d8133a40dad7448"},
    {"Scripts/Maps/outc1.lua", "b9964160284ae83fe3555cc9658cb8e5539b3e3d829a90e79d89cf8d1c9285fc"},
    {"Scripts/Maps/outc2.lua", "8551ac3062309de282c22470aabd4e27ea151d761a7985fda65b2cb119007b7f"},
    {"Scripts/Maps/outd1.lua", "1ee4775921cdc9d0b0cc94d899e199542b5cbc33fe1765ec2fc67edc338caecc"},
    {"Scripts/Maps/outd3.lua", "c2191191f244a0cead4a1337032cbcb0d3f9ee1c216820e4c8fad6559745fb28"},
    {"Scripts/Maps/oute2.lua", "3fa6cd65264130bdd354d357b9fcdd469c5a4b464b999a92f610bc06504b5a76"},
    {"Scripts/Maps/oute3.lua", "731955ac2b119b5ed6f32d7a01ff08ea82fac4cb7b60596b9fca80250f846e08"},
}};

constexpr std::array<SourceFingerprint, 3> SharedSources = {{
    {"Scripts/Global/StdQuestsFollowers.lua",
        "36c5e4a5830f975febe007042a8b52600bd97f0a53bc726d4f0193d94fa616a8"},
    {"Scripts/General/NPCFollowers.lua",
        "d1798480623d05d954b6b82de04761b8e54d2dfbce237d2a150549368810c107"},
    {"Scripts/Global/StdQuestsFunctions.lua",
        "201eed1ebb5b6ed02878eaf92268a4190b7de90eecedbffa7cda63b7b2754623"},
}};

struct TowerRule
{
    std::string_view mapStem;
    uint16_t timerEventId;
    uint16_t disableEventId;
    uint16_t restoreEventId;
    uint16_t questBit;
    int x;
    int y;
    int z;
    uint16_t modelIndex;
    uint16_t facetIndex;
};

constexpr std::array<TowerRule, 6> TowerRules = {{
    {"outb2", 210, 211, 213, 1184, -17921, 9724, 2742, 61, 42},
    {"outc1", 209, 210, 214, 1185, -6606, 15546, 2550, 114, 42},
    {"outc2", 209, 210, 214, 1183, 3823, 10974, 2700, 25, 55},
    {"outd1", 209, 210, 226, 1182, 11032, -8940, 2830, 117, 42},
    {"oute2", 210, 211, 213, 1181, 3039, -9201, 2818, 53, 42},
    {"oute3", 230, 231, 232, 1180, -6152, -9208, 2700, 84, 42},
}};

constexpr std::string_view GlobalOverlay = R"OY_MM6_GLOBAL(
-- StdQuestsFollowers.lua: these are AfterProcessEvent tails, preserving every original reward/topic transaction.
local function appendFollower(eventId, npcId, add, pendingQuest)
    AppendGlobalEvent(eventId, function(continueStep)
        if continueStep ~= nil then return end
        if pendingQuest ~= nil and IsQBitSet(QBit(pendingQuest)) then return end
        if add then AddFollowerNpc(npcId) else RemoveFollowerNpc(npcId) end
    end)
end
appendFollower(1642, 980, false)
appendFollower(1631, 893, false, 1702)
appendFollower(1634, 893, true)
appendFollower(1638, 978, false)
appendFollower(1640, 978, true)
appendFollower(1645, 940, false, 1705)
appendFollower(1646, 940, true)

-- The source Nicolai timer is re-created for the current map visit and starts one hour after registration.
RegisterGlobalEvent(65478, nil, function()
    RemoveFollowerNpc(798)
    ClearQBit(QBit(1114))
    ClearQBit(QBit(1700))
    evt.MoveNPC(798, 1595)
    SetQBit(QBit(1119))
    evt.SetNPCTopic(798, 0, 1334)
    evt.StatusText(evt.NPCText(1720), 5)
    -- RemoveTimer(callback) removes the first matching instance, not every registration of that callback.
    evt.StopMapTimer(65478)
end)

local function startNicolaiTimer()
    evt.StartMapTimer(65478, 5, nil, 60)
end
RegisterGlobalAfterLoadEvent(65479, nil, function()
    if IsQBitSet(QBit(1114)) and HasFollowerNpc(798) then startNicolaiTimer() end
end)

-- Only 1331 removes the raw Global EVT; 1333/1334 retain their base transactions and append follower state.
RegisterGlobalEvent(1331, nil, function()
    startNicolaiTimer()
    AddFollowerNpc(798)
    evt.MoveNPC(798, 0)
    SetQBit(QBit(1114))
    SetQBit(QBit(1700))
    evt.SetMessage(evt.NPCText(1718))
    evt.SetNPCTopic(798, 0, 1332)
end)
appendFollower(1333, 798, false)
appendFollower(1334, 798, true)
)OY_MM6_GLOBAL";

constexpr std::string_view AltarOverlay = R"OY_MM6_ALTAR(
RegisterEvent(24, mm6Texts[6], function()
    if IsQBitSet(QBit(1047)) then return end
    for _, facet in ipairs({-949, -947, -927, -928, -929, -948, -945, -946, -944, -943, -942}) do
        evt.SetTexture(facet, "d6flora")
    end
    evt.StatusText(mm6Texts[11])
    for _, player in ipairs(PartyMembers()) do
        evt.ForPlayer(player)
        -- Retain the source predicate, including its MapVar7 comparison (exported selector9).
        if IsAtLeast(ClassId, 4) or IsAtLeast(ClassId, 12) or not IsAtLeast(MapVar(9), 0) then
            AddValue(BasePersonality, 5)
        end
    end
    SetQBit(QBit(1047))
end, mm6Texts[6])
)OY_MM6_ALTAR";

constexpr std::string_view PerceptionOverlay = R"OY_MM6_GATE(
RegisterEvent(1, mm6Texts[1], function()
    evt.ForPlayer(Players.Current)
    if evt.CheckSkill(const.Skills.Perception, const.SkillCheck.Novice, 8) then
        evt.SetDoorState(1, DoorAction.Close)
    else
        evt.DamagePlayer(Players.Current, const.Damage.Fire, 50)
    end
end, mm6Texts[1])
)OY_MM6_GATE";

constexpr std::string_view SarcophagusOverlay = R"OY_MM6_TOMBS(
local itemTypes = {[61] = 35, [62] = 36, [63] = 39}
for eventId = 61, 63 do
    local id = eventId
    RegisterEvent(id, mm6Texts[18], function(continueStep)
        local counter = MapVar(id - 50) -- Source MapVar(id-52), using the existing exported selectors.
        if continueStep == 4 then
            if IsAtLeast(counter, 1) then return end
            SetValue(counter, 1)
            evt.GiveItem(6, itemTypes[id], 0)
            AddValue(ReputationInCurrentLocation, 200)
            return
        end
        if continueStep ~= nil or IsAtLeast(counter, 1) then return end
        evt.AskQuestion(id, 3, 21, 4, 22, 23,
            mm6Texts[20] .. "\n" .. mm6Texts[21], {mm6Texts[22], mm6Texts[23]})
    end, mm6Texts[18])
end
)OY_MM6_TOMBS";

constexpr std::string_view CuratorOverlay = R"OY_MM6_CURATOR(
RegisterEvent(27, mm6Texts[14], function(continueStep)
    if continueStep == 4 then
        if not IsAtLeast(Gold, 10000) then
            evt.StatusText(mm6Texts[13])
            return
        end
        evt.ForPlayer(0)
        SubtractValue(Gold, 10000)
        AddValue(ReputationInCurrentLocation, 50)
        evt.ForPlayer(Players.All)
        SetValue(MajorCondition, 0)
        AddValue(MaxHealth, 0)
        AddValue(MaxSpellPoints, 0)
        return
    elseif continueStep == 2 then
        evt.StatusText(mm6Texts[13])
        return
    elseif continueStep ~= nil then
        return
    end
    evt.AskQuestion(27, 2, 10, 4, 11, 12, mm6Texts[15], {mm6Texts[11], mm6Texts[12]})
end, mm6Texts[14])
)OY_MM6_CURATOR";

constexpr std::string_view ShrineOverlay = R"OY_MM6_SHRINE(
RegisterEvent(103, mm6Texts[16], function()
    local player = evt.GetCurrentPlayerIndex()
    if player < 0 then return end
    local rosterId = evt.GetPartyMemberRosterId(player)
    if rosterId < 0 then return end
    evt.ForPlayer(player)
    -- Source Party.PlayersIndexes, rather than the current party slot or a regenerated Character identity.
    -- This one-map ledger survives source Map.Refilled and follows the same campaign lifetime as GotBless.
    local key = "MMerge.MM6.ShrineOfGods.Roster." .. tostring(rosterId)
    local blessed = evt.GetGlobalVar(key, 0) ~= 0
    if not blessed and IsPlayerBitSet(PlayerBit(70)) then
        -- Explicit migration of the previous exported once-only reward; never grant a second+20.
        evt.SetGlobalVar(key, 1)
        blessed = true
    end
    if blessed then
        SetValue(MajorCondition, 0)
        return
    end
    evt.SetGlobalVar(key, 1)
    for _, stat in ipairs({FireResistance, AirResistance, WaterResistance, EarthResistance,
        SpiritResistance, MindResistance, BodyResistance, BaseMight, BaseIntellect, BasePersonality,
        BaseEndurance, BaseSpeed, BaseAccuracy, BaseLuck}) do
        AddValue(stat, 20)
    end
    evt.PlaySound(42797, 0, 0)
    evt.StatusText(mm6Texts[7])
end, mm6Texts[16])
)OY_MM6_SHRINE";

constexpr std::string_view NicolaiDoorOverlay = R"OY_MM6_NICOLAI(
RegisterEvent(43, nil, function()
    if IsQBitSet(QBit(1119)) then
        if HasFollowerNpc(798) then
            evt.MoveNPC(798, 222)
            ClearQBit(QBit(1700))
            ClearQBit(QBit(1119))
            RemoveFollowerNpc(798)
            evt.ForPlayer(Players.All)
            AddValue(Experience, 7500)
            evt.SetNPCTopic(798, 0, 1337)
            evt.SimpleMessage(evt.NPCText(1723))
        else
            evt.SimpleMessage(mm6Texts[9])
        end
        return
    end
    evt.MoveToMap(0, 0, 0, 0, 0, 0, 335, 2)
    evt.EnterHouse(222)
end)
)OY_MM6_NICOLAI";

std::optional<std::string> authenticateSource(
    const std::filesystem::path &root, const SourceFingerprint &source)
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
        throw std::runtime_error("MM6 remaining-map source escapes the source root: "
            + std::string(source.relativePath));
    }
    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumSourceBytes)
    {
        throw std::runtime_error("Invalid MM6 remaining-map source size: " + std::string(source.relativePath));
    }
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read MM6 remaining-map source: " + std::string(source.relativePath));
    }
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sha256 = digest.str();
    if (sha256 != source.sha256)
    {
        throw std::runtime_error("Unsupported MM6 remaining-map fingerprint: "
            + std::string(source.relativePath) + "; review this Merge version before enabling its projection");
    }
    return sha256;
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
    return literal + '"';
}

const TowerRule *towerRule(std::string_view stem)
{
    for (const TowerRule &rule : TowerRules)
    {
        if (rule.mapStem == stem)
        {
            return &rule;
        }
    }
    return nullptr;
}

void addTextureMetadata(std::ostringstream &lua, std::string_view texture)
{
    lua << "do\n    local textures = evt.meta.map.textureNames or {}\n"
        << "    local found = false\n    for _, texture in ipairs(textures) do\n"
        << "        if texture == " << luaStringLiteral(texture) << " then found = true; break end\n    end\n"
        << "    if not found then table.insert(textures, " << luaStringLiteral(texture) << ") end\n"
        << "    evt.meta.map.textureNames = textures\nend\n";
}

void writeRelations(std::ostringstream &lua, std::string_view stem)
{
    // HostileTxt rows identify three-monster families. The native relation API takes concrete monster IDs.
    std::vector<std::array<int, 3>> relations;
    if (stem == "outb2") relations = {{196, 0, 0}};
    else if (stem == "outc2" || stem == "outd3")
        relations = {{159, 0, 0}, {196, 0, 0}, {196, 191, 2}, {191, 196, 2}, {196, 205, 2}, {205, 196, 1}};
    else if (stem == "outd1")
        relations = {{211, 0, 0}, {211, 173, 2}, {211, 181, 2}, {173, 211, 1}, {181, 211, 1}};
    else if (stem == "oute2")
        relations = {{211, 0, 0}, {204, 0, 0}, {211, 205, 2}, {205, 211, 1},
            {211, 201, 2}, {201, 211, 1}, {211, 202, 2}, {202, 211, 1}};
    else if (stem == "oute3") relations = {{185, 0, 0}};

    for (const std::array<int, 3> &relation : relations)
    {
        const int firstFrom = relation[0] == 0 ? 0 : relation[0] * 3 - 2;
        const int firstTo = relation[1] == 0 ? 0 : relation[1] * 3 - 2;
        for (int from = firstFrom; from <= relation[0] * 3; ++from)
        {
            for (int to = firstTo; to <= relation[1] * 3; ++to)
            {
                lua << "    evt.SetLocalMonsterRelation(" << from << ", " << to << ", " << relation[2] << ")\n";
            }
        }
    }
}

void addOutdoorOverlay(std::ostringstream &lua, const Mm6RemainingMapProfile &profile)
{
    const std::string &stem = profile.mapStem;
    const TowerRule *pTower = towerRule(stem);
    if (pTower == nullptr && stem != "outd3")
    {
        return;
    }

    if (pTower != nullptr)
    {
        addTextureMetadata(lua, "t1swbu");
        lua << "RemoveMapEvent(" << pTower->timerEventId << ")\nRegisterEvent(" << pTower->timerEventId
            << ", nil, function()\n    if not IsQBitSet(QBit(" << pTower->questBit
            << ")) and IsAtLeast(IsFlying, 0) and not IsAtLeast(Invisible, 0) then\n"
            // Source CastSpell arguments are spell/mastery/skill. The native API uses spell/skill/mastery.
            << "        evt.CastSpell(6, 5, 2, " << pTower->x << ", " << pTower->y << ", " << pTower->z
            << ", 0, 0, 0)\n    end\nend)\nRegisterEvent(" << pTower->disableEventId
            << ", nil, function()\n    evt.ForPlayer(Players.All)\n    if not IsQBitSet(QBit("
            << pTower->questBit << ")) and HasItem(2106) then\n        SetQBit(QBit(" << pTower->questBit
            << "))\n        evt.SetOutdoorModelFacetTexture(" << pTower->modelIndex << ", " << pTower->facetIndex
            << ", \"t1swbu\")\n    end\nend)\nAppendMapEvent(" << pTower->restoreEventId
            << ", function(continueStep)\n    if continueStep ~= nil then return end\n    if IsQBitSet(QBit("
            << pTower->questBit << ")) then\n        evt.SetOutdoorModelFacetTexture(" << pTower->modelIndex << ", "
            << pTower->facetIndex << ", \"t1swbu\")\n    end\nend)\n";
    }

    lua << "RegisterMapAfterLoadEvent(65477, nil, function()\n";
    writeRelations(lua, stem);
    if (stem == "outb2" || stem == "outc1")
    {
        lua << "    evt.ChangeGroupAlly(39, 9999)\n    evt.SetMonGroupBit(39, MonsterBits.Hostile, false)\n";
    }
    if (stem == "outc1")
    {
        lua << "    if IsQBitSet(QBit(1252)) then\n        evt.SetOutdoorWeatherEnabled(true)\n    else\n"
            << "        evt.SetOutdoorWeatherEnabled(false)\n        evt.SetOutdoorSky(\"sky04\")\n"
            << "        evt.SetOutdoorFog(100, 1000)\n        evt.DelayMapEvent(65484, 0.1)\n    end\n";
    }
    if (pTower != nullptr)
    {
        lua << "    if not IsQBitSet(QBit(" << pTower->questBit << ")) then\n        evt.StartMapTimer("
            << pTower->timerEventId << ", 5)\n    end\n";
    }
    lua << "end)\n";
    if (stem == "outc1")
    {
        lua << "RegisterEvent(65484, nil, function() evt.SetSnow(0, true) end)\n";
    }
}
}

std::vector<Mm6RemainingMapProfile> loadMm6RemainingMapProfiles(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::optional<std::string> followerApi = authenticateSource(root, SharedSources[1]);
    const std::optional<std::string> functions = authenticateSource(root, SharedSources[2]);
    std::vector<Mm6RemainingMapProfile> profiles;
    for (const SourceFingerprint &map : MapSources)
    {
        const std::optional<std::string> sha256 = authenticateSource(root, map);
        if (!sha256)
        {
            continue;
        }
        Mm6RemainingMapProfile profile;
        profile.mapStem = std::filesystem::path(map.relativePath).stem().string();
        profile.sourceSha256 = *sha256;
        const std::string &stem = profile.mapStem;
        if (stem == "6d02" || stem == "6d03" || stem == "6t3" || stem == "6t8" || stem == "outd3")
        {
            if (!followerApi)
            {
                continue;
            }
        }
        if (const TowerRule *pTower = towerRule(stem))
        {
            if (!functions)
            {
                continue;
            }
            profile.requiredEventIds = {pTower->timerEventId, pTower->disableEventId, pTower->restoreEventId};
            profile.addedEventIds = {Mm6RemainingMapAfterLoadEventId};
            profile.addedAfterLoadEventIds = profile.addedEventIds;
            profile.removedLegacyTimerEventIds = {pTower->timerEventId};
        }
        if (stem == "6d02") profile.requiredEventIds = {14};
        else if (stem == "6d03") profile.requiredEventIds = {22};
        else if (stem == "6t3" || stem == "6t8")
        {
            profile.requiredEventIds = {uint16_t(stem == "6t3" ? 30 : 25)};
            profile.bolsterExcludedUniqueNameIds = {123};
        }
        else if (stem == "6d08") profile.indoorFacetTypes = {{373, 5}};
        else if (stem == "6d13")
        {
            profile.requiredEventIds = {24};
            profile.requiredStringIds = {6, 11};
        }
        else if (stem == "6t7")
        {
            profile.requiredEventIds = {1};
            profile.requiredStringIds = {1};
        }
        else if (stem == "cd2")
        {
            profile.requiredEventIds = {61, 62, 63};
            profile.requiredStringIds = {18, 20, 21, 22, 23};
        }
        else if (stem == "cd3")
        {
            profile.requiredEventIds = {27};
            profile.requiredStringIds = {11, 12, 13, 14, 15};
        }
        else if (stem == "outa2") profile.tileSetFootsteps = {{0, 91, 52}};
        else if (stem == "outa3") profile.tileSetFootsteps = {{0, 91, 52}, {6, 90, 51}};
        else if (stem == "outb1") profile.tileSetFootsteps = {{6, 90, 51}};
        else if (stem == "outb2") profile.tileSetFootsteps = {{6, 91, 52}};
        else if (stem == "outb3")
        {
            profile.requiredEventIds = {103};
            profile.requiredStringIds = {7, 16};
            profile.tileSetFootsteps = {{6, 91, 52}};
        }
        else if (stem == "outc1") profile.addedEventIds.push_back(Mm6WinterSnowEventId);
        else if (stem == "outd3")
        {
            profile.requiredEventIds = {43};
            profile.requiredStringIds = {9};
            profile.addedEventIds = {Mm6RemainingMapAfterLoadEventId};
            profile.addedAfterLoadEventIds = profile.addedEventIds;
        }
        else if (stem == "oute3")
        {
            profile.tileSetFootsteps = {{6, 90, 51}};
            profile.unresolvedSourceNote = "Volcano220 uses reversed math.random bounds before its first spell; "
                "no source override of math.random was found. The source firework sequence is not certified.";
        }
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::optional<Mm6RemainingGlobalProfile> loadMm6RemainingGlobalProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    std::array<std::optional<std::string>, SharedSources.size()> hashes;
    bool complete = true;
    for (size_t index = 0; index < SharedSources.size(); ++index)
    {
        hashes[index] = authenticateSource(root, SharedSources[index]);
        complete = hashes[index].has_value() && complete;
    }
    if (!complete)
    {
        return std::nullopt;
    }
    return Mm6RemainingGlobalProfile{*hashes[0], *hashes[1], *hashes[2], std::string(GlobalOverlay),
        {Mm6NicolaiKidnapEventId, Mm6NicolaiAfterLoadEventId}, {Mm6NicolaiAfterLoadEventId}};
}

std::string makeMm6RemainingMapOverlay(const Mm6RemainingMapProfile &profile,
    const std::unordered_map<uint16_t, std::string> &strings)
{
    std::ostringstream lua;
    if (!profile.requiredStringIds.empty())
    {
        lua << "local mm6Texts = {\n";
        for (uint16_t id : profile.requiredStringIds)
        {
            const auto found = strings.find(id);
            if (found == strings.end() || found->second.empty())
            {
                throw std::runtime_error("Missing MM6 " + profile.mapStem + " STR" + std::to_string(id));
            }
            lua << "    [" << id << "] = " << luaStringLiteral(found->second) << ",\n";
        }
        lua << "}\n";
    }
    const std::string &stem = profile.mapStem;
    if (stem == "6d02" || stem == "6d03")
    {
        const bool angela = stem == "6d02";
        const int event = angela ? 14 : 22;
        const int onceBit = angela ? 1056 : 1036;
        const int pendingBit = angela ? 1704 : 1703;
        const int npc = angela ? 980 : 978;
        lua << "RegisterEvent(" << event << ", nil, function()\n    if IsQBitSet(QBit(" << onceBit
            << ")) then return end\n    SetQBit(QBit(" << onceBit << "))\n    SetQBit(QBit(" << pendingBit
            << "))\n    AddFollowerNpc(" << npc << ")\n    evt.SpeakNPC(" << npc << ")\nend)\n";
    }
    else if (stem == "6t3" || stem == "6t8")
    {
        const bool sherell = stem == "6t3";
        lua << "AppendMapEvent(" << (sherell ? 30 : 25)
            << ", function(continueStep)\n    if continueStep ~= nil then return end\n    if IsQBitSet(QBit("
            << (sherell ? 1705 : 1702) << ")) then AddFollowerNpc(" << (sherell ? 940 : 893)
            << ") end\nend)\n";
    }
    else if (stem == "6d13")
    {
        addTextureMetadata(lua, "d6flora");
        lua << AltarOverlay;
    }
    else if (stem == "6t7") lua << PerceptionOverlay;
    else if (stem == "cd2") lua << SarcophagusOverlay;
    else if (stem == "cd3") lua << CuratorOverlay;
    else if (stem == "outb3") lua << ShrineOverlay;
    else if (stem == "outd3") lua << NicolaiDoorOverlay;
    addOutdoorOverlay(lua, profile);
    return lua.str();
}
}
