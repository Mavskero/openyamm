#include "tools/legacy_events/StrTable.h"

#include <string>
#include <vector>

namespace OpenYAMM::Game
{
bool StrTable::loadFromNullSeparatedBytes(const std::vector<uint8_t> &bytes)
{
    m_entries.clear();
    size_t start = 0;
    for (size_t index = 0; index < bytes.size(); ++index)
    {
        if (bytes[index] == 0)
        {
            m_entries.emplace_back(bytes.begin() + start, bytes.begin() + index);
            start = index + 1;
        }
    }
    if (start != bytes.size())
    {
        m_entries.clear();
        return false;
    }
    return true;
}

bool StrTable::loadFromBytes(const std::vector<uint8_t> &bytes)
{
    m_entries.clear();

    std::string content(bytes.begin(), bytes.end());
    size_t position = 0;

    while (position <= content.size())
    {
        size_t lineEnd = content.find('\n', position);

        if (lineEnd == std::string::npos)
        {
            lineEnd = content.size();
        }

        std::string line = content.substr(position, lineEnd - position);

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        m_entries.push_back(std::move(line));

        if (lineEnd == content.size())
        {
            break;
        }

        position = lineEnd + 1;
    }

    return true;
}

const std::vector<std::string> &StrTable::getEntries() const
{
    return m_entries;
}

std::optional<std::string> StrTable::get(size_t index) const
{
    if (index >= m_entries.size())
    {
        return std::nullopt;
    }

    return m_entries[index];
}
}
