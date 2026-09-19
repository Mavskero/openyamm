#include "TextResources.h"

#include <iconv.h>
#include <stdexcept>

namespace OpenYAMM::Import
{
std::string decodeLegacyText(const std::vector<uint8_t> &bytes, bool chinese, size_t &restoredGlyphs)
{
    if (bytes.size() > 16 * 1024 * 1024)
    {
        throw std::runtime_error("Story table exceeds the text size limit");
    }
    std::string utf8(bytes.size() * 4 + 4, '\0');
    iconv_t converter = iconv_open("UTF-8", chinese ? "GB2312" : "WINDOWS-1252");
    if (converter == reinterpret_cast<iconv_t>(-1))
    {
        throw std::runtime_error("Legacy text codec is unavailable");
    }
    char *pInput = reinterpret_cast<char *>(const_cast<uint8_t *>(bytes.data()));
    size_t inputLeft = bytes.size();
    char *pOutput = utf8.data();
    size_t outputLeft = utf8.size();
    const size_t result = iconv(converter, &pInput, &inputLeft, &pOutput, &outputLeft);
    iconv_close(converter);
    if (result == size_t(-1) || inputLeft != 0)
    {
        throw std::runtime_error("Invalid legacy story text encoding");
    }
    utf8.resize(utf8.size() - outputLeft);
    std::string output;
    for (size_t index = 0; index < utf8.size();)
    {
        size_t cursor = index;
        std::string run;
        size_t glyphs = 0;
        if (chinese && utf8[cursor] == '\x0e')
        {
            ++cursor;
            if (cursor < utf8.size() && utf8[cursor] == ' ')
            {
                ++cursor;
            }
            while (cursor + 2 < utf8.size() && utf8[cursor] == '\x0e')
            {
                const uint8_t lead = uint8_t(utf8[cursor + 1]);
                const size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
                if (lead < 32 || cursor + length + 1 >= utf8.size() || utf8[cursor + length + 1] != '\x07')
                {
                    break;
                }
                run.append(utf8, cursor + 1, length);
                ++glyphs;
                cursor += length + 2;
                if (cursor < utf8.size() && utf8[cursor] == ' ')
                {
                    ++cursor;
                }
            }
        }
        if (glyphs > 0 && cursor < utf8.size() && utf8[cursor] == '\x0f')
        {
            output += run;
            restoredGlyphs += glyphs;
            index = cursor + 1;
        }
        else
        {
            output += utf8[index++];
        }
    }
    // Partial glyph runs must be repaired explicitly, not passed to the UI.
    if (output.find_first_of(std::string("\0\x0e\x0f\x07", 4)) != std::string::npos)
    {
        throw std::runtime_error("Unresolved legacy glyph controls in story text");
    }
    return output;
}

std::string serializeTextRows(const std::vector<std::vector<std::string>> &rows)
{
    std::string text;
    for (const std::vector<std::string> &row : rows)
    {
        for (size_t column = 0; column < row.size(); ++column)
        {
            if (column > 0)
            {
                text += '\t';
            }
            text += '"';
            for (char character : row[column])
            {
                text += character;
                if (character == '"')
                {
                    text += '"';
                }
            }
            text += '"';
        }
        text += '\n';
    }
    return text;
}

}
