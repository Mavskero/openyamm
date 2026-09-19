#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct EnchantmentResources
{
    std::vector<std::vector<std::string>> standard;
    std::vector<std::vector<std::string>> special;
    size_t preservedMechanicalCells = 0;
};

EnchantmentResources convertEnchantmentTables(const std::vector<std::vector<std::string>> &standard,
    const std::vector<std::vector<std::string>> &special,
    const std::vector<std::vector<std::string>> &localizedStandard,
    const std::vector<std::vector<std::string>> &localizedSpecial);
}
