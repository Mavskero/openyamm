#pragma once

#include "tools/LegacyLuaExport.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct Mm8EndgameProfile
{
    // Paths are relative to the selected original game source root.
    std::string globalEventArchive;
    std::string globalEventSha256;
    std::string answerArchive;
    std::string answerStrSha256;
    bool chineseAnswers = false;
    std::vector<Game::LegacyInputStringAnswerBinding> inputStringAnswerBindings;
};

// Bind only the audited Escaton input instructions to their map-local STR answers.
// Archive precedence is identical to the merged event importer. An absent Global.evt disables this profile;
// an available audited Global.evt requires the complete and verified answer resource.
std::optional<Mm8EndgameProfile> loadMm8EndgameProfile(const std::filesystem::path &source);
}
