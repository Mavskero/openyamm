#include "FreeHavenTempleResources.h"

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
constexpr std::string_view RelativePath = "Scripts/Maps/outc2.lua";
constexpr std::string_view ExpectedSha256 =
    "8551ac3062309de282c22470aabd4e27ea151d761a7985fda65b2cb119007b7f";
constexpr std::string_view CouncilGlobalPath = "Scripts/Global/StdQuestsFunctions.lua";
constexpr std::string_view CouncilGlobalSha256 =
    "201eed1ebb5b6ed02878eaf92268a4190b7de90eecedbffa7cda63b7b2754623";

constexpr std::string_view Overlay = R"OY_FREE_HAVEN(
-- Translate the source follower transaction; hiring, fees and custody remain native operations.
ReplaceMapEvent(19, templeHint, function()
    if IsQBitSet(QBit(1131)) then
        evt.ForPlayer(Players.All)
        if HasItem(2054) then
            RemoveItem(2054)
            ClearQBit(QBit(1212))
            SetQBit(QBit(1132))
            evt.SimpleMessage(chaliceText)
        else
            evt.EnterHouse(326)
        end
    elseif IsQBitSet(QBit(1130)) then
        if IsQBitSet(QBit(1129)) then
            evt.EnterHouse(1442)
        else
            evt.EnterHouse(326)
        end
    elseif HasFollowerProfession(63) and HasFollowerProfession(64) then
        evt.SimpleMessage(repairedText)
        SetQBit(QBit(1130))
        RemoveFollowerProfession(63)
        RemoveFollowerProfession(64)
    else
        evt.EnterHouse(1442)
    end
end, templeHint)

RegisterEvent(140, innHint, function()
    evt.EnterHouse(1607)
end, innHint)
)OY_FREE_HAVEN";

constexpr std::string_view CouncilMapOverlay = R"OY_COUNCIL_MAP(
-- Audited outc2.lua 28-69: the original door reveals the evidence before opening the Council lobby.
local function enterCouncil()
    evt.ForPlayer(Players.All)
    if HasItem(2122) and evt.GetNPCHouse(1089) == 209 then
        evt.ShowMovie("citytrtr", false)
        evt.MoveNPC(1089, 0)
        RemoveItem(2122)
        evt.Add(0x00EB, 20) -- Source ReputationIs improves the stored local reputation by 20.
        SetQBit(QBit(1192))
        ClearQBit(QBit(1225))
        SetAward(Award(63))

        local allEndorsements = true
        for awardId = 57, 62 do
            if not evt.HasOwnedAward(awardId) then
                allEndorsements = false
                break
            end
        end
        if allEndorsements then
            SetQBit(QBit(1191))
        end
    end

    if IsQBitSet(QBit(1192)) then
        ClearQBit(QBit(1225))
        for slot = 0, 5 do
            if evt.GetNPCTopic(789, slot) == 1415 then
                evt.SetNPCTopic(789, slot, 0)
            end
        end
    end

    evt.EnterHouse(209)
end

RegisterEvent(49, nil, enterCouncil)
RegisterEvent(50, nil, enterCouncil)
)OY_COUNCIL_MAP";

constexpr std::string_view CouncilGlobalOverlay = R"OY_COUNCIL_GLOB(
-- StdQuestsFunctions.lua 67-73 is an AfterProcessEvent callback; retain the original EVT transaction.
AppendGlobalEvent(1693, function(continueStep)
    if continueStep ~= nil then return end
    if IsQBitSet(QBit(1192)) then
        evt.ShowMovie("citytrtr", false)
        -- Rebuild the lobby and its live quest-gated extra exit after leaving the NPC conversation.
        evt.EnterHouse(209)
    end
end)
)OY_COUNCIL_GLOB";

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

std::string authenticatedSourceHash(const std::filesystem::path &root, std::string_view relativePath,
    std::string_view expectedSha256)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relativePath);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("Free Haven source script escapes the game source root");
    }

    const uintmax_t size = std::filesystem::file_size(path);
    if (size == 0 || size > MaximumScriptBytes)
    {
        throw std::runtime_error("Invalid Free Haven source script size");
    }

    std::string script(size, '\0');
    std::ifstream input(path, std::ios::binary);
    if (!input.read(script.data(), static_cast<std::streamsize>(size)))
    {
        throw std::runtime_error("Cannot read Free Haven source script");
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
        throw std::runtime_error("Unsupported " + std::string(relativePath) + " fingerprint; "
            "review this Merge version before enabling the Free Haven projection");
    }
    return sourceSha256;
}
}

std::optional<FreeHavenTempleProfile> loadFreeHavenTempleProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::exists(root / RelativePath))
    {
        return std::nullopt;
    }
    return FreeHavenTempleProfile{"outc2", authenticatedSourceHash(root, RelativePath, ExpectedSha256)};
}

std::optional<FreeHavenCouncilProfile> loadFreeHavenCouncilProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::exists(root / RelativePath) || !std::filesystem::exists(root / CouncilGlobalPath))
    {
        return std::nullopt;
    }
    return FreeHavenCouncilProfile{
        authenticatedSourceHash(root, RelativePath, ExpectedSha256),
        authenticatedSourceHash(root, CouncilGlobalPath, CouncilGlobalSha256),
        std::string(CouncilMapOverlay), std::string(CouncilGlobalOverlay)};
}

std::string makeFreeHavenTempleOverlay(std::string_view templeHint, std::string_view repairedText,
    std::string_view chaliceText, std::string_view innHint)
{
    if (templeHint.empty() || repairedText.empty() || chaliceText.empty() || innHint.empty())
    {
        throw std::runtime_error("Missing Free Haven temple or inn source text");
    }
    return "local templeHint = " + luaStringLiteral(templeHint)
        + "\nlocal repairedText = " + luaStringLiteral(repairedText)
        + "\nlocal chaliceText = " + luaStringLiteral(chaliceText)
        + "\nlocal innHint = " + luaStringLiteral(innHint) + "\n" + std::string(Overlay);
}
}
