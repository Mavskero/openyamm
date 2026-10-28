#include "LichPromotionResources.h"

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
constexpr std::string_view SourcePath = "Scripts/Global/PromotionTopics.lua";
constexpr std::string_view SourceSha256 =
    "6266dd3fc45736d60901f0e3c4f98e7b4dcb0c2f4bfca6dd816dabc371d94014";
constexpr std::string_view MiscTweaksPath = "Scripts/General/MiscTweaks.lua";
constexpr std::string_view MiscTweaksSha256 =
    "7e15ed60a94c3ffafc018aa0dcaf9754e3828b8db67eb88ed16ec8f902b5b51d";

constexpr std::string_view Overlay = R"OY_LICH(
-- Audited PromotionTopics.lua 1561-1638 and 1665-1688, including its helpers at 18-72.
-- Authenticated MiscTweaks.lua 22-33 disables automatic EVT Lich class effects.
-- The native final helper owns appearance and the source resistance/skill changes.
-- Source resistance indices 7/8 are Mind/Body, as declared by Scripts/Structs/00 structs.lua.
SetGlobalMetadata({sourceManagedLichEffects = true})

local function promoteDarkMages()
    for _, player in ipairs(PartyMembers()) do
        if evt.GetPlayerClass(player) == 47
            and (evt.PartyMemberHasItem(player, 1417) or evt.PartyMemberHasItem(player, 628)) then
            if not evt.RemovePartyMemberItem(player, 1417, 1) then
                evt.RemovePartyMemberItem(player, 628, 1)
            end
            evt.AssignPlayerClass(player, 45)
            evt.ForPlayer(player)
            AddValue(Experience, 0)
        end
    end

    -- Process all final Liches, including pre-existing members, in party order.
    evt.ApplyMergedLichAppearance()
end

-- Keep the original book quest transaction and append its source sequence-zero tail.
AppendGlobalEvent(89, function(continueStep)
    if continueStep ~= nil then return end
    promoteDarkMages()
    evt.ForPlayer(Players.Current)
end)

ReplaceGlobalEvent(738, nil, function()
    local promotedLich = false
    for _, player in ipairs(PartyMembers()) do
        local playerClass = evt.GetPlayerClass(player)
        if (playerClass == 43 or playerClass == 44) and evt.PartyMemberHasItem(player, 628) then
            evt.RemovePartyMemberItem(player, 628, 1)
            evt.ForPlayer(player)
            AddValue(Experience, 0)
            evt.AssignPlayerClass(player, 45)
            promotedLich = true
        end
    end

    -- A Dark Archmage conversion alone does not select the source's successful-jar message.
    promoteDarkMages()
    evt.SetMessage(evt.NPCText(promotedLich and 925 or 114))
    evt.ForPlayer(Players.Current)
end)

ReplaceGlobalEvent(846, nil, function()
    local currentClass = evt.GetPlayerClass()
    local allowed = IsQBitSet(QBit(1619)) or IsQBitSet(QBit(1620))
        or currentClass == 44 or currentClass == 45
    if not allowed then
        evt.SetMessage(evt.NPCText(1147))
    elseif IsQBitSet(QBit(612)) then
        evt.SetMessage(evt.NPCText(1146))
        SetQBit(QBit(560))
        evt.SetNPCTopic(388, 0, 847)
    elseif IsQBitSet(QBit(611)) then
        evt.SetMessage(evt.NPCText(1149))
    else
        evt.SetMessage(evt.NPCText(1148))
    end
end)

ReplaceGlobalEvent(847, nil, function()
    local jarHolders = 0
    local promotedLiches = 0
    local honoraryRewardGiven = IsQBitSet(QBit(1624))
    for _, player in ipairs(PartyMembers()) do
        evt.ForPlayer(player)
        if HasItem(1417) then
            jarHolders = jarHolders + 1
            local playerClass = evt.GetPlayerClass(player)
            if playerClass == 43 or playerClass == 44 then
                evt.AssignPlayerClass(player, 45)
                AddValue(Experience, 40000)
                promotedLiches = promotedLiches + 1
            else
                if not honoraryRewardGiven then
                    AddValue(Experience, 40000)
                end
                AddValue(Gold, 1500)
            end
        end
    end

    -- Inventory Subtract consumes one party item per call, independently of ForPlayer.
    -- Count holders before consuming: stacked jars on one member count only once.
    for _ = 1, jarHolders do
        RemoveItem(1417)
    end

    if jarHolders == 0 then
        evt.SetMessage(evt.NPCText(1151))
    else
        SetQBit(QBit(1624))
        SetQBit(QBit(1623))
        ClearQBit(QBit(560))
        ClearQBit(QBit(741))
        evt.SetMessage(evt.NPCText(promotedLiches == 0 and 2698 or 1150))
    end

    -- This second pass uses the inventory remaining after the counted ritual consumption.
    -- It also runs after the no-1417 message; a Dark Archmage may instead carry jar 628.
    promoteDarkMages()
    evt.ForPlayer(Players.Current)
end)
)OY_LICH";

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string authenticatedSourceHash(const std::filesystem::path &root,
    std::string_view relativePath, std::string_view expectedSha256)
{
    const std::filesystem::path candidate = root / relativePath;
    require(std::filesystem::exists(candidate), "Missing Lich promotion source: " + std::string(relativePath));
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path), "Lich promotion source script escapes the game source root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= MaximumScriptBytes, "Invalid Lich promotion source script size");
    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(script.data(), static_cast<std::streamsize>(size))),
        "Cannot read Lich promotion source script");

    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(script.data(), static_cast<CC_LONG>(script.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    const std::string sourceSha256 = digest.str();
    require(sourceSha256 == expectedSha256,
        "Unsupported " + std::string(relativePath)
            + " fingerprint; review this Merge version before enabling source-managed Lich effects");
    return sourceSha256;
}
}

std::optional<LichPromotionProfile> loadLichPromotionProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    require(std::filesystem::is_directory(root), "Lich promotion source root is not a directory");
    if (!std::filesystem::exists(root / SourcePath))
    {
        return std::nullopt;
    }

    const std::string sourceSha256 = authenticatedSourceHash(root, SourcePath, SourceSha256);
    const std::string miscTweaksSha256 = authenticatedSourceHash(root, MiscTweaksPath, MiscTweaksSha256);
    return LichPromotionProfile{sourceSha256, miscTweaksSha256, std::string(Overlay)};
}
}
