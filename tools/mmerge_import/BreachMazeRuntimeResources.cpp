#include "BreachMazeRuntimeResources.h"

#include "BreachMazePopulationResources.h"
#include "game/content/BreachMazeRuntimeProfile.h"

#include <fstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
bool writeBreachMazeRuntimeResources(const std::filesystem::path &source, const std::filesystem::path &assetRoot)
{
    const std::optional<BreachMazePopulationProfile> imported = loadBreachMazePopulationProfile(source);
    if (!imported) return false;
    const Game::BreachMazeRuntimeProfile profile{imported->mazeTemplate, imported->catalog};
    const std::vector<uint8_t> bytes = Game::encodeBreachMazeRuntimeProfile(profile);
    const std::filesystem::path output = assetRoot / "engine" / Game::BreachMazeRuntimeProfileAsset;
    std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()))
        || !stream.flush())
    {
        throw std::runtime_error("Cannot write Breach maze runtime profile: " + output.string());
    }
    return true;
}
}
