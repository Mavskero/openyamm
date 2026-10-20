#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
struct MonsterProjectileEntry
{
    std::string token;
    std::string normalizedToken;
    int objectId = 0;
    int impactObjectId = 0;
    // Missing for legacy three-column packages. Zero is never a projectile entry.
    std::optional<uint8_t> sourceMissileId;
};

class MonsterProjectileTable
{
public:
    bool loadFromRows(const std::vector<std::vector<std::string>> &rows);
    const MonsterProjectileEntry *findByToken(const std::string &token) const;
    const MonsterProjectileEntry *findBySourceMissileId(uint8_t id) const;

private:
    std::vector<MonsterProjectileEntry> m_entries;
    std::unordered_map<std::string, size_t> m_entryIndexByNormalizedToken;
    std::unordered_map<uint8_t, size_t> m_entryIndexBySourceMissileId;
};
}
