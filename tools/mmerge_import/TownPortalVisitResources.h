#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct TownPortalVisitProfile
{
    std::string mapStem;
    std::string sourceSha256;
    uint16_t eventId = 0;
    uint32_t legacyQBit = 0;
    uint32_t unlockQBit = 0;
    bool afterLoad = false;
    std::string luaSource;
};

// Authenticate the six map scripts and the source slot/continent/table mapping before translating visits.
// A missing map omits its profile; missing mapping support disables all profiles. Present but changed or
// invalid source is rejected. Other map behavior and the source engine's shared display slots are untouched.
std::vector<TownPortalVisitProfile> loadTownPortalVisitProfiles(const std::filesystem::path &source);
}
