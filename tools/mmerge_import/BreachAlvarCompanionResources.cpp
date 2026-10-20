#include "BreachAlvarCompanionResources.h"
#include "BreachAlvarCompanionSource.h"

#include "engine/TextTable.h"
#include "game/content/BreachAlvarCompanionState.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/ClassMultiplierTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
struct SourceDefinition
{
    std::string_view path;
    std::string_view hash;
};

constexpr std::array<SourceDefinition, 4> Sources{{
    {BreachAlvarMapSourcePath, BreachAlvarMapSourceSha256},
    {"Scripts/Global/Quest_CrossContinents.lua", "0790af9b1b387b7db901f0b568593df725516e823a86fa0edb2d1fde459bce49"},
    {"Data/Tables/Character portraits.txt", "a15cef9da5d78ce9f7c58a3517b81675260b2f16ac73f850a55fc0dc9b5bd456"},
    {"Data/Tables/Class Extra.txt", "b3a27af317b8418644c90ca1a4209509553b1d6951e5a09c8d70fbf54650a48a"},
}};

constexpr std::array<std::string_view, Game::BreachAlvarGenerationSourceHashes.size()> GenerationSources{{
    "Scripts/General/NPCMercenaries.lua",
    "Scripts/General/MenuChooseCharacter.lua",
    "Scripts/Structs/RacialSkills.lua",
    "Scripts/Structs/After/DataTablesSupport.lua",
    "Scripts/Core/ConstAndBits.lua",
    "Scripts/Global/ExtraPotions.lua",
    "Data/Tables/Character selection.txt",
    "Data/Tables/Class Skills.txt",
    "Data/Tables/Class Starting Skills.txt",
    "Data/Tables/Class Starting Stats.txt",
    "Data/Tables/Class HP SP.txt",
    "Data/Tables/Race Skills.txt",
}};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string verifiedSource(const std::filesystem::path &root, const SourceDefinition &definition)
{
    const std::filesystem::path candidate = root / definition.path;
    require(std::filesystem::exists(candidate), "Incomplete Breach Alvar companion source: " + candidate.string());
    const std::filesystem::path path = std::filesystem::canonical(candidate);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_regular_file(path), "Breach Alvar companion source escapes source root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 256 * 1024, "Invalid Breach Alvar companion source size");
    std::string text(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(text.data(), std::streamsize(size))), "Cannot read Breach Alvar companion source");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(text.data(), CC_LONG(text.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    require(digest.str() == definition.hash,
        "Unsupported Breach Alvar companion source fingerprint: " + std::string(definition.path));
    return text;
}

uint32_t number(const std::string &text)
{
    uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size(),
        "Invalid Breach Alvar source table number");
    return value;
}

std::vector<std::vector<std::string>> rows(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid Breach Alvar source table");
    std::vector<std::vector<std::string>> result;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const auto &row = table->getRow(index);
        if (index + 1 == table->getRowCount() && row == std::vector<std::string>{""})
        {
            continue;
        }
        result.push_back(row);
    }
    return result;
}
}

std::optional<BreachAlvarCompanionResources> loadBreachAlvarCompanionResources(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::exists(root / Sources[0].path) && !std::filesystem::exists(root / Sources[1].path))
    {
        return std::nullopt;
    }
    std::array<std::string, Sources.size()> sourceText;
    BreachAlvarCompanionResources resources;
    Game::BreachAlvarCompanionProfile &profile = resources.profile;
    for (size_t index = 0; index < Sources.size(); ++index)
    {
        sourceText[index] = verifiedSource(root, Sources[index]);
        profile.sourceSha256[index] = Sources[index].hash;
    }
    for (size_t index = 0; index < GenerationSources.size(); ++index)
    {
        verifiedSource(root, {GenerationSources[index], Game::BreachAlvarGenerationSourceHashes[index]});
        profile.generationSourceSha256.emplace_back(Game::BreachAlvarGenerationSourceHashes[index]);
    }
    profile.version = Game::BreachAlvarCompanionContentVersion;
    profile.minimumActiveCount = 1;
    profile.maximumActiveCount = 5;
    profile.canonicalMapKey = Game::BreachAlvarCompanionMap;
    profile.requiredPartySize = 5;
    profile.npcIds = {772, 773, 774, 775};
    profile.houseIds = {0, 705, 706, 707};
    profile.initialTopicId = 1789;
    profile.hintTopicId = 1793;
    profile.hintExperience = 0;
    profile.hintsFollowEncounterOrder = true;
    profile.defaultMonsterId = 261;
    // BrAlvar.lua 145–182: portrait race takes precedence over these class-kind branches.
    profile.raceRules = {{1, 417}, {3, 66}, {4, 60}, {5, 72}, {6, 290}};
    profile.classRules = {
        {1, 204, 477, std::nullopt, 0}, {2, 219, 219, 7, 216}, {5, 633, 519, std::nullopt, 0},
        {6, 261, 261, std::nullopt, 0}, {9, 261, 261, std::nullopt, 0}, {8, 585, 585, std::nullopt, 0},
        {10, 258, 258, std::nullopt, 0}, {11, 408, 15, std::nullopt, 0},
        {14, 293, 420, std::nullopt, 0}, {15, 293, 420, std::nullopt, 0},
    };

    const auto portraitRows = rows(sourceText[2]);
    require(portraitRows.size() == 78 && portraitRows[0].size() == 27
        && portraitRows[0][1] == "Paperdoll type" && portraitRows[0][24] == "NPCPic",
        "Unsupported Breach Alvar original portrait layout");
    Game::CharacterDollTable dolls;
    require(dolls.loadCharacterRows(portraitRows), "Cannot load Breach Alvar original portraits");
    for (size_t index = 1; index < portraitRows.size(); ++index)
    {
        // CharacterOutfits.lua 461–504 indexes portraits by row, starting at Face 0.
        require(portraitRows[index].size() == 27 && number(portraitRows[index][0]) == index - 1,
            "Breach Alvar original portrait Face/order mismatch");
        const Game::CharacterDollEntry *pDoll = dolls.getCharacter(uint32_t(index));
        require(pDoll != nullptr, "Missing Breach Alvar typed portrait");
        profile.portraits.push_back({pDoll->id, pDoll->raceId, pDoll->defaultSex, pDoll->npcPictureId});
    }

    const auto classRows = rows(sourceText[3]);
    require(classRows.size() == 54 && classRows[0] == std::vector<std::string>{"#", "Kind", "Step", "Note"},
        "Unsupported Breach Alvar original class layout");
    Game::ClassMultiplierTable classes;
    require(classes.applyClassExtraRows(classRows), "Cannot load Breach Alvar class metadata");
    for (size_t index = 1; index < classRows.size(); ++index)
    {
        const auto &row = classRows[index];
        require(row.size() == 4 && number(row[0]) == index - 1, "Breach Alvar class ID/order mismatch");
        const uint32_t classKind = number(row[1]);
        if (classKind == 0)
        {
            require((index - 1 == 49 || index - 1 == 52) && row[3] == "n/u",
                "Unknown Breach Alvar inactive class");
            continue;
        }
        const Game::ClassMultiplierEntry *pClass = classes.get(row[3]);
        require(pClass && pClass->classId == index - 1 && pClass->classKind == classKind,
            "Breach Alvar typed class identity mismatch");
        profile.classes.push_back({pClass->classId, pClass->classKind});
    }

    // Source 184–199 and ControlTimer 428–449, registered with const.Minute at 691.
    profile.friendActor = {772, -20633, -14746, 4864, 50, 9999, 5, false, 1, 15, true, true, true,
        {{0, 0}, {25, 4}, {150, 4}, {217, 4}}};
    std::string error;
    require(Game::validBreachAlvarCompanionProfile(profile, error), error);
    resources.mapLuaSource = std::string(BreachAlvarCompanionMapSource);
    resources.globalLuaSource = std::string(BreachAlvarCompanionGlobalSource);
    return resources;
}
}
