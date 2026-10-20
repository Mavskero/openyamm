#include "game/tables/MonsterProjectileTable.h"

#include <cctype>
#include <charconv>
#include <limits>

namespace OpenYAMM::Game
{
namespace
{
std::string normalizeToken(std::string value)
{
    for (char &character : value)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    return value;
}

bool parseNonnegative(const std::string &text, int &value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value >= 0;
}
}

bool MonsterProjectileTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    MonsterProjectileTable result;

    for (const std::vector<std::string> &row : rows)
    {
        if (row.empty() || (row.size() == 1 && row[0].empty()))
        {
            continue;
        }
        if (row.size() < 2 || row.size() > 4 || row[0].empty())
        {
            return false;
        }

        MonsterProjectileEntry entry = {};
        entry.token = row[0];
        entry.normalizedToken = normalizeToken(entry.token);
        if (!parseNonnegative(row[1], entry.objectId) || entry.objectId == 0
            || (row.size() > 2 && !parseNonnegative(row[2], entry.impactObjectId))
            || result.m_entryIndexByNormalizedToken.contains(entry.normalizedToken))
        {
            return false;
        }
        if (row.size() == 4)
        {
            int sourceId = 0;
            if (!parseNonnegative(row[3], sourceId) || sourceId == 0
                || sourceId > std::numeric_limits<uint8_t>::max())
            {
                return false;
            }
            entry.sourceMissileId = uint8_t(sourceId);
            const MonsterProjectileEntry *pPrevious = result.findBySourceMissileId(*entry.sourceMissileId);
            if (pPrevious && (pPrevious->objectId != entry.objectId
                || pPrevious->impactObjectId != entry.impactObjectId))
            {
                return false;
            }
            // FireAr/ArrowF and Rock/Earth are source aliases, not conflicting mappings.
            result.m_entryIndexBySourceMissileId.emplace(*entry.sourceMissileId, result.m_entries.size());
        }
        result.m_entryIndexByNormalizedToken.emplace(entry.normalizedToken, result.m_entries.size());
        result.m_entries.push_back(std::move(entry));
    }

    if (result.m_entries.empty())
    {
        return false;
    }
    *this = std::move(result);
    return true;
}

const MonsterProjectileEntry *MonsterProjectileTable::findByToken(const std::string &token) const
{
    const auto entryIt = m_entryIndexByNormalizedToken.find(normalizeToken(token));

    if (entryIt == m_entryIndexByNormalizedToken.end())
    {
        return nullptr;
    }

    return &m_entries[entryIt->second];
}

const MonsterProjectileEntry *MonsterProjectileTable::findBySourceMissileId(uint8_t id) const
{
    const auto found = m_entryIndexBySourceMissileId.find(id);
    return found == m_entryIndexBySourceMissileId.end() ? nullptr : &m_entries[found->second];
}
}
