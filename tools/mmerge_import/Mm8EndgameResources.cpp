#include "Mm8EndgameResources.h"
#include "LodResources.h"
#include "TextResources.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OpenYAMM::Import
{
namespace
{
struct SelectedResource
{
    std::filesystem::path archive;
    bool chinese = false;
};

std::string digest(const std::vector<uint8_t> &bytes)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), hash.data());
    std::ostringstream result;
    for (unsigned char value : hash)
    {
        result << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return result.str();
}

std::filesystem::path checkedArchive(const std::filesystem::path &root, const char *pName)
{
    const std::filesystem::path path = std::filesystem::canonical(root / "Data" / pName);
    const std::filesystem::path relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == ".."
        || !std::filesystem::is_regular_file(path))
    {
        throw std::runtime_error("MM8 endgame source archive escapes the game source root");
    }
    return path;
}
}

std::optional<Mm8EndgameProfile> loadMm8EndgameProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    std::optional<SelectedResource> global;
    std::optional<SelectedResource> answers;
    for (const char *pName : {"mm6.T.lod", "mm7.T.lod", "mmmerge.T.lod", "10 LocZHCN.EnglishT.lod"})
    {
        if (!std::filesystem::exists(root / "Data" / pName))
        {
            continue;
        }
        const std::filesystem::path archive = checkedArchive(root, pName);
        const bool chinese = std::string_view(pName) == "10 LocZHCN.EnglishT.lod";
        const std::vector<std::string> members = listLodMembers(archive);
        if (std::find(members.begin(), members.end(), "global.evt") != members.end())
        {
            if (chinese)
            {
                throw std::runtime_error("MM8 endgame language archive must not replace Global.evt mechanics");
            }
            global = SelectedResource{archive, false};
        }
        if (std::find(members.begin(), members.end(), "d35.str") != members.end())
        {
            answers = SelectedResource{archive, chinese};
        }
    }
    if (!global)
    {
        return std::nullopt;
    }
    if (!answers)
    {
        throw std::runtime_error("MM8 endgame requires the Escaton d35.str answer resource");
    }

    const std::vector<uint8_t> globalBytes = readLodText(global->archive, "global.evt");
    const std::vector<uint8_t> answerBytes = readLodText(answers->archive, "d35.str");
    Mm8EndgameProfile profile;
    profile.globalEventArchive = global->archive.lexically_relative(root).generic_string();
    profile.globalEventSha256 = digest(globalBytes);
    profile.answerArchive = answers->archive.lexically_relative(root).generic_string();
    profile.answerStrSha256 = digest(answerBytes);
    profile.chineseAnswers = answers->chinese;
    if (profile.globalEventSha256 != "48dccfb27066fd84bc2c7849a462defb97fd34944251b1274e1c24595a8a7138")
    {
        throw std::runtime_error("Unsupported Global.evt fingerprint; review the MM8 endgame input instructions");
    }
    const std::string_view expectedAnswers = answers->chinese
        ? "b213fc4b3ca8a4d40cf60e1a05b5f8616d2a453afc2fe9495c31667e44412361"
        : "a04b6816cb806032c463670369fe034b64a99070488e5da7ce03f064d64b01d4";
    if (profile.answerStrSha256 != expectedAnswers)
    {
        throw std::runtime_error("Unsupported d35.str fingerprint for the selected MM8 endgame language archive");
    }

    Game::EvtProgram program;
    Game::StrTable strings;
    if (!program.loadFromBytes(globalBytes) || !strings.loadFromNullSeparatedBytes(answerBytes))
    {
        throw std::runtime_error("Malformed MM8 endgame source resource");
    }
    for (uint16_t eventId = 164; eventId <= 166; ++eventId)
    {
        const auto event = std::find_if(program.getEvents().begin(), program.getEvents().end(),
            [&](const Game::EvtEvent &candidate) { return candidate.eventId == eventId; });
        if (event == program.getEvents().end())
        {
            throw std::runtime_error("Missing Escaton input event");
        }
        const auto instruction = std::find_if(event->instructions.begin(), event->instructions.end(),
            [](const Game::EvtInstruction &candidate) { return candidate.step == 1; });
        const uint32_t firstAnswer = 100 + (eventId - 164) * 2;
        if (instruction == event->instructions.end() || instruction->opcode != Game::EvtOpcode::InputString
            || instruction->value1 != 595 + (eventId - 164) * 4
            || instruction->value2 != firstAnswer || instruction->value3 != firstAnswer + 1
            || instruction->targetStep != 4)
        {
            throw std::runtime_error("Escaton input instruction does not match its audited source binding");
        }
        Game::LegacyInputStringAnswerBinding binding;
        binding.scope = Game::LegacyLuaExportScope::Global;
        binding.eventId = eventId;
        binding.step = 1;
        for (uint32_t id : {firstAnswer, firstAnswer + 1})
        {
            const std::optional<std::string> text = strings.get(id);
            if (!text || text->empty())
            {
                throw std::runtime_error("Missing Escaton source answer");
            }
            size_t glyphs = 0;
            const std::string decoded = decodeLegacyText({text->begin(), text->end()}, answers->chinese, glyphs);
            if (decoded.empty())
            {
                throw std::runtime_error("Empty decoded Escaton source answer");
            }
            binding.answers.push_back(decoded);
        }
        profile.inputStringAnswerBindings.push_back(std::move(binding));
    }
    return profile;
}
}
