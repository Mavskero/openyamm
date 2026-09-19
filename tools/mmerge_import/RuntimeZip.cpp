#include "RuntimeZip.h"

#include <zlib.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
struct ZipEntry
{
    std::string name;
    uint32_t crc = 0;
    uint32_t size = 0;
    uint32_t offset = 0;
};

void word(std::ostream &stream, uint16_t value)
{
    stream.put(char(value & 255));
    stream.put(char(value >> 8));
}

void integer(std::ostream &stream, uint32_t value)
{
    word(stream, uint16_t(value & 65535));
    word(stream, uint16_t(value >> 16));
}

uint32_t offset(std::ostream &stream)
{
    const std::streampos position = stream.tellp();
    if (position < 0 || uint64_t(position) >= std::numeric_limits<uint32_t>::max())
    {
        throw std::runtime_error("Converted package exceeds ZIP32 archive size limit");
    }
    return uint32_t(position);
}
}

void writeRuntimeZip(const std::filesystem::path &root, const std::filesystem::path &destination,
    const ImportCancelCheck &cancelled)
{
    checkImportCancellation(cancelled);
    if (std::filesystem::exists(destination))
    {
        throw std::runtime_error("Runtime package already exists: " + destination.string());
    }
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_directory(root))
    {
        for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(root))
        {
            checkImportCancellation(cancelled);
            if (entry.is_symlink() || (!entry.is_directory() && !entry.is_regular_file()))
            {
                throw std::runtime_error("Unexpected nonregular converted resource: " + entry.path().string());
            }
            if (entry.is_regular_file())
            {
                files.push_back(entry.path());
            }
        }
    }
    std::sort(files.begin(), files.end());
    if (files.size() >= 65535)
    {
        throw std::runtime_error("Converted package exceeds ZIP32 entry limit");
    }
    std::filesystem::create_directories(destination.parent_path());
    std::ofstream stream(destination, std::ios::binary);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    std::vector<ZipEntry> entries;
    std::array<char, 65536> buffer;
    constexpr uint16_t flags = 0x0808; // UTF-8 names and a data descriptor after each streamed file.
    for (const std::filesystem::path &file : files)
    {
        checkImportCancellation(cancelled);
        ZipEntry entry;
        entry.name = file.lexically_relative(root).generic_string();
        const uintmax_t size = std::filesystem::file_size(file);
        if (entry.name.size() > 65535 || size >= std::numeric_limits<uint32_t>::max())
        {
            throw std::runtime_error("Converted resource exceeds ZIP32 entry size limit");
        }
        entry.size = uint32_t(size);
        entry.offset = offset(stream);
        integer(stream, 0x04034b50);
        word(stream, 20);
        word(stream, flags);
        word(stream, 0); // Stored data.
        word(stream, 0);
        word(stream, 33); // 1980-01-01.
        integer(stream, 0);
        integer(stream, 0);
        integer(stream, 0);
        word(stream, uint16_t(entry.name.size()));
        word(stream, 0);
        stream.write(entry.name.data(), entry.name.size());
        std::ifstream input(file, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("Cannot read converted resource: " + file.string());
        }
        uint64_t copied = 0;
        uLong checksum = crc32(0, Z_NULL, 0);
        while (input)
        {
            checkImportCancellation(cancelled);
            input.read(buffer.data(), buffer.size());
            const std::streamsize count = input.gcount();
            checksum = crc32(checksum, reinterpret_cast<const Bytef *>(buffer.data()), uInt(count));
            stream.write(buffer.data(), count);
            copied += uint64_t(count);
        }
        if (!input.eof() || copied != size)
        {
            throw std::runtime_error("Converted resource changed or could not be read completely");
        }
        entry.crc = uint32_t(checksum);
        integer(stream, 0x08074b50);
        integer(stream, entry.crc);
        integer(stream, entry.size);
        integer(stream, entry.size);
        entries.push_back(std::move(entry));
    }
    const uint32_t centralStart = offset(stream);
    for (const ZipEntry &entry : entries)
    {
        checkImportCancellation(cancelled);
        integer(stream, 0x02014b50);
        word(stream, 0x0314); // Unix, ZIP 2.0.
        word(stream, 20);
        word(stream, flags);
        word(stream, 0);
        word(stream, 0);
        word(stream, 33);
        integer(stream, entry.crc);
        integer(stream, entry.size);
        integer(stream, entry.size);
        word(stream, uint16_t(entry.name.size()));
        word(stream, 0);
        word(stream, 0);
        word(stream, 0);
        word(stream, 0);
        integer(stream, uint32_t(0100644) << 16);
        integer(stream, entry.offset);
        stream.write(entry.name.data(), entry.name.size());
    }
    const uint32_t centralSize = offset(stream) - centralStart;
    integer(stream, 0x06054b50);
    word(stream, 0);
    word(stream, 0);
    word(stream, uint16_t(entries.size()));
    word(stream, uint16_t(entries.size()));
    integer(stream, centralSize);
    integer(stream, centralStart);
    word(stream, 0);
    stream.close();
}
}
