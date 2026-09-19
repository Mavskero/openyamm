#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Game
{
inline std::optional<uint32_t> decodeUtf8Character(std::string_view text, size_t &byteCount)
{
    if (text.empty())
    {
        return std::nullopt;
    }
    const uint8_t first = uint8_t(text.front());
    byteCount = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
    if (first >= 0xf5 || (first >= 0x80 && first < 0xc2) || text.size() < byteCount)
    {
        return std::nullopt;
    }
    uint32_t value = byteCount == 1 ? first : first & (0x7f >> byteCount);
    for (size_t index = 1; index < byteCount; ++index)
    {
        const uint8_t next = uint8_t(text[index]);
        if ((next & 0xc0) != 0x80)
        {
            return std::nullopt;
        }
        value = (value << 6) | (next & 0x3f);
    }
    constexpr uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
    if (value < minimum[byteCount] || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
    {
        return std::nullopt;
    }
    return value;
}

inline std::optional<size_t> utf8CharacterCount(std::string_view text)
{
    size_t count = 0;
    while (!text.empty())
    {
        size_t byteCount = 0;
        const std::optional<uint32_t> character = decodeUtf8Character(text, byteCount);
        if (!character || *character == 0)
        {
            return std::nullopt;
        }
        text.remove_prefix(byteCount);
        ++count;
    }
    return count;
}

inline bool hasUtf8MultibyteText(std::string_view text)
{
    const std::optional<size_t> count = utf8CharacterCount(text);
    return count && *count < text.size();
}

// Labels have a character limit, independent of UTF-8 storage size. Reject malformed input as a whole.
inline void appendUtf8Label(std::string &buffer, std::string_view text, size_t maximumCharacters)
{
    const std::optional<size_t> count = utf8CharacterCount(buffer);
    if (!count || !utf8CharacterCount(text))
    {
        return;
    }
    size_t length = *count;
    while (!text.empty() && length < maximumCharacters)
    {
        size_t byteCount = 0;
        const uint32_t character = *decodeUtf8Character(text, byteCount);
        if (character >= 32 && !(character >= 127 && character <= 159)
            && character != 0x2028 && character != 0x2029)
        {
            buffer.append(text.data(), byteCount);
            ++length;
        }
        text.remove_prefix(byteCount);
    }
}

inline bool eraseLastUtf8Character(std::string &buffer)
{
    if (buffer.empty() || !utf8CharacterCount(buffer))
    {
        return false;
    }
    size_t start = buffer.size() - 1;
    while (start > 0 && (uint8_t(buffer[start]) & 0xc0) == 0x80)
    {
        --start;
    }
    buffer.resize(start);
    return true;
}
}
