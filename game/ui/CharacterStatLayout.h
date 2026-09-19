#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Game
{
class CharacterInspectTable;

struct CharacterStatRowDefinition
{
    const char *pStatName;
    const char *pLabelId;
    const char *pValueId;
};

const std::array<CharacterStatRowDefinition, 26> &characterStatRows();
std::optional<std::string> characterStatDisplayLabel(
    const CharacterInspectTable *pTable,
    std::string_view normalizedLayoutId);
}
