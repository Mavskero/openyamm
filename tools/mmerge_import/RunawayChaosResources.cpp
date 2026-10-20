#include "RunawayChaosResources.h"
#include "RunawayChaosSource.h"
#include "CrossContinentsResources.h"

#include <stdexcept>

namespace OpenYAMM::Import
{
std::optional<RunawayChaosProfile> loadRunawayChaosProfile(
    const std::filesystem::path &source,
    const std::unordered_map<uint32_t, std::string> &npcNames,
    const std::unordered_map<uint32_t, std::string> &npcTopicNames,
    const std::unordered_map<uint32_t, std::string> &npcTexts)
{
    // Both profiles translate distinct sections of the same audited original script.
    const std::optional<CrossContinentsIntroProfile> original = loadCrossContinentsIntroProfile(source);
    if (!original)
    {
        return std::nullopt;
    }
    const auto requireText = [](const std::unordered_map<uint32_t, std::string> &table,
        uint32_t id, const char *pTable)
    {
        const auto found = table.find(id);
        if (found == table.end() || found->second.empty())
        {
            throw std::runtime_error(std::string("Missing Runaway Chaos ") + pTable + " " + std::to_string(id));
        }
    };
    for (uint32_t id : {772u, 773u, 774u, 775u, 1092u})
    {
        requireText(npcNames, id, "NPC");
    }
    for (uint32_t id : {1789u, 1790u, 1792u, 1793u, 1794u})
    {
        requireText(npcTopicNames, id, "topic");
    }
    for (uint32_t id = 2218; id <= 2239; ++id)
    {
        requireText(npcTexts, id, "riddle text/answer");
    }
    for (uint32_t id = 2241; id <= 2265; ++id)
    {
        requireText(npcTexts, id, "hint/dialogue text");
    }
    for (uint32_t id = 2709; id <= 2713; ++id)
    {
        requireText(npcTexts, id, "disguise name");
    }
    return RunawayChaosProfile{original->sourceSha256, std::string(RunawayChaosSource)};
}
}
