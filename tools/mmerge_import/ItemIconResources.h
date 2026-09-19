#pragma once

#include <filesystem>

namespace OpenYAMM::Game
{
class ItemTable;
}

namespace OpenYAMM::Import
{
// The inventory converter owns staging and removes it if this import fails.
void convertItemIconResources(const std::filesystem::path &source, const Game::ItemTable &items,
    const std::filesystem::path &destination);
}
