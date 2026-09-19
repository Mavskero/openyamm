#include "LodResources.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
constexpr size_t MaximumResourceBytes = 128 * 1024 * 1024;
using Bytes = std::vector<uint8_t>;

uint32_t number(const Bytes &bytes, size_t offset, size_t count = 4)
{
    if (offset > bytes.size() || count > bytes.size() - offset)
    {
        throw std::runtime_error("Truncated legacy resource header");
    }
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i)
    {
        value |= uint32_t(bytes[offset + i]) << (i * 8);
    }
    return value;
}

void putNumber(Bytes &bytes, size_t offset, uint32_t value, size_t count = 4)
{
    for (size_t i = 0; i < count; ++i)
    {
        bytes.at(offset + i) = uint8_t(value >> (i * 8));
    }
}

std::string lower(std::string value)
{
    for (char &character : value)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character += 'a' - 'A';
        }
    }
    return value;
}

std::string resourceName(const Bytes &bytes, size_t offset, size_t length)
{
    if (offset > bytes.size() || length > bytes.size() - offset)
    {
        throw std::runtime_error("Truncated archive name");
    }
    std::string name;
    for (size_t i = offset; i < offset + length && bytes[i] != 0; ++i)
    {
        const char character = char(bytes[i]);
        if (bytes[i] < 32 || bytes[i] >= 127 || character == '/' || character == '\\' || character == ':')
        {
            throw std::runtime_error("Invalid archive member name");
        }
        name.push_back(character);
    }
    if (name.empty() || name == "." || name == "..")
    {
        throw std::runtime_error("Empty or unsafe archive member name");
    }
    return lower(name);
}

Bytes readRange(const std::filesystem::path &path, uint64_t offset, size_t size)
{
    const uint64_t fileSize = std::filesystem::file_size(path);
    if (size > MaximumResourceBytes || offset > fileSize || size > fileSize - offset)
    {
        throw std::runtime_error("Archive range exceeds file bounds: " + path.filename().string());
    }
    std::ifstream stream(path, std::ios::binary);
    stream.seekg(std::streamoff(offset));
    Bytes result(size);
    if (!stream.read(reinterpret_cast<char *>(result.data()), std::streamsize(size)))
    {
        throw std::runtime_error("Unable to read archive: " + path.filename().string());
    }
    return result;
}

struct Entry
{
    uint64_t offset;
    uint32_t size;
};

struct Archive
{
    std::filesystem::path path;
    size_t nameSize;
    bool isMapArchive = false;
    std::map<std::string, Entry> entries;

    explicit Archive(const std::filesystem::path &source) : path(source)
    {
        const Bytes header = readRange(path, 0, 288);
        if (number(header, 0) != 0x00444f4c)
        {
            throw std::runtime_error("Invalid LOD signature");
        }
        const std::string version = resourceName(header, 4, 80);
        if (version != "mmvi" && version != "mmviii" && version != "gamemmvi")
        {
            throw std::runtime_error("Unsupported image LOD version");
        }
        isMapArchive = version == "gamemmvi";
        nameSize = version == "mmviii" ? 64 : 16;
        const size_t stride = nameSize == 64 ? 76 : 32;
        const uint64_t base = number(header, 272);
        const uint64_t size = number(header, 276);
        const uint32_t count = number(header, 284);
        const uint64_t fileSize = std::filesystem::file_size(path);
        if (count > 100000 || base < 288 || base > fileSize || size > fileSize - base
            || uint64_t(count) * stride > size)
        {
            throw std::runtime_error("Invalid LOD directory bounds");
        }
        const Bytes index = readRange(path, base, count * stride);
        std::vector<Entry> ranges;
        for (uint32_t i = 0; i < count; ++i)
        {
            const size_t start = i * stride;
            const std::string name = resourceName(index, start, nameSize);
            const uint64_t offset = number(index, start + nameSize);
            const uint32_t memberSize = number(index, start + nameSize + 4);
            if (offset < count * stride || offset > size || memberSize > size - offset
                || memberSize > MaximumResourceBytes
                || !entries.emplace(name, Entry{base + offset, memberSize}).second)
            {
                throw std::runtime_error("Invalid or duplicate LOD member: " + name);
            }
            ranges.push_back({offset, memberSize});
        }
        std::sort(ranges.begin(), ranges.end(), [](const Entry &left, const Entry &right)
        {
            return left.offset < right.offset;
        });
        uint64_t end = count * stride;
        for (const Entry &entry : ranges)
        {
            if (entry.offset < end)
            {
                throw std::runtime_error("Overlapping LOD members");
            }
            end = entry.offset + entry.size;
        }
    }
};

Bytes unpack(const Bytes &resource, size_t offset, size_t stored, size_t expanded)
{
    if (offset > resource.size() || stored > resource.size() - offset || expanded > MaximumResourceBytes)
    {
        throw std::runtime_error("Invalid compressed resource bounds");
    }
    if (expanded == 0)
    {
        return Bytes(resource.begin() + offset, resource.begin() + offset + stored);
    }
    Bytes result(expanded);
    z_stream stream = {};
    stream.next_in = const_cast<uint8_t *>(resource.data() + offset);
    stream.avail_in = uint32_t(stored);
    stream.next_out = result.data();
    stream.avail_out = uint32_t(result.size());
    if (inflateInit(&stream) != Z_OK)
    {
        throw std::runtime_error("Cannot initialize resource decompressor");
    }
    const int status = inflate(&stream, Z_FINISH);
    const bool valid = status == Z_STREAM_END && stream.total_out == expanded && stream.total_in == stored;
    inflateEnd(&stream);
    if (!valid)
    {
        throw std::runtime_error("Corrupt or oversized compressed image");
    }
    return result;
}

struct Image
{
    Bytes bytes;
    uint32_t width;
    uint32_t height;
    std::string extension;
};

Bytes readWrappedResource(const Archive &archive, const std::string &name)
{
    if (archive.isMapArchive || !archive.entries.contains(name))
    {
        throw std::runtime_error("Missing language resource: " + name);
    }
    const Entry &entry = archive.entries.at(name);
    const Bytes resource = readRange(archive.path, entry.offset, entry.size);
    const size_t headerSize = archive.nameSize + 32;
    if (resource.size() < headerSize || resourceName(resource, 0, archive.nameSize) != name)
    {
        throw std::runtime_error("Language resource header/name mismatch: " + name);
    }
    const uint32_t stored = number(resource, archive.nameSize + 4);
    const uint32_t expanded = number(resource, archive.nameSize + 24);
    if (headerSize + stored != resource.size())
    {
        throw std::runtime_error("Language resource length mismatch: " + name);
    }
    return unpack(resource, headerSize, stored, expanded);
}

Image decode(const Archive &archive, const std::string &name, LodImagePixelFormat pixelFormat)
{
    const Entry &entry = archive.entries.at(name);
    const Bytes resource = readRange(archive.path, entry.offset, entry.size);
    const size_t headerSize = archive.nameSize + 32;
    if (resource.size() < headerSize || resourceName(resource, 0, archive.nameSize) != name)
    {
        throw std::runtime_error("Image header/name mismatch: " + name);
    }
    const uint32_t stored = number(resource, archive.nameSize + 4);
    const uint32_t expanded = number(resource, archive.nameSize + 24);
    const uint32_t width = number(resource, archive.nameSize + 8, 2);
    const uint32_t height = number(resource, archive.nameSize + 10, 2);
    const Bytes pixels = unpack(resource, headerSize, stored, expanded);
    if (name.ends_with(".pcx"))
    {
        if (headerSize + stored != resource.size() || pixels.size() < 128 || pixels[0] != 10
            || number(pixels, 8, 2) < number(pixels, 4, 2) || number(pixels, 10, 2) < number(pixels, 6, 2))
        {
            throw std::runtime_error("Invalid wrapped PCX image");
        }
        return {pixels, number(pixels, 8, 2) - number(pixels, 4, 2) + 1,
            number(pixels, 10, 2) - number(pixels, 6, 2) + 1, ".pcx"};
    }
    const uint64_t baseSize = uint64_t(width) * height;
    const uint64_t mipSize = baseSize + uint64_t(width / 2) * (height / 2)
        + uint64_t(width / 4) * (height / 4) + uint64_t(width / 8) * (height / 8);
    if (width == 0 || height == 0 || (baseSize != pixels.size() && mipSize != pixels.size())
        || headerSize + uint64_t(stored) + 768 != resource.size())
    {
        throw std::runtime_error("Unsupported indexed image layout: " + name);
    }
    const bool indexed = pixelFormat == LodImagePixelFormat::Indexed8;
    const size_t pixelOffset = indexed ? 1078 : 54;
    const size_t rowSize = (width * (indexed ? 1 : 3) + 3) & ~size_t(3);
    Bytes bitmap(pixelOffset + rowSize * height, 0);
    bitmap[0] = 'B';
    bitmap[1] = 'M';
    putNumber(bitmap, 2, uint32_t(bitmap.size()));
    putNumber(bitmap, 10, uint32_t(pixelOffset));
    putNumber(bitmap, 14, 40);
    putNumber(bitmap, 18, width);
    putNumber(bitmap, 22, height);
    putNumber(bitmap, 26, 1, 2);
    putNumber(bitmap, 28, indexed ? 8 : 24, 2);
    putNumber(bitmap, 34, uint32_t(rowSize * height));
    if (indexed)
    {
        putNumber(bitmap, 46, 256);
        for (size_t i = 0; i < 256; ++i)
        {
            const size_t palette = headerSize + stored + i * 3;
            bitmap[54 + i * 4] = resource[palette + 2];
            bitmap[55 + i * 4] = resource[palette + 1];
            bitmap[56 + i * 4] = resource[palette];
        }
    }
    for (uint32_t y = 0; y < height; ++y)
    {
        if (indexed)
        {
            std::copy_n(pixels.begin() + y * width, width,
                bitmap.begin() + pixelOffset + (height - y - 1) * rowSize);
            continue;
        }
        for (uint32_t x = 0; x < width; ++x)
        {
            const size_t palette = headerSize + stored + size_t(pixels[y * width + x]) * 3;
            const size_t target = 54 + (height - y - 1) * rowSize + x * 3;
            bitmap[target] = resource[palette + 2];
            bitmap[target + 1] = resource[palette + 1];
            bitmap[target + 2] = resource[palette];
        }
    }
    return {std::move(bitmap), width, height, ".bmp"};
}

void write(const std::filesystem::path &path, const Bytes &bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    if (!stream.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size())))
    {
        throw std::runtime_error("Unable to write converted image");
    }
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("Unable to finish converted resource");
    }
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
        | std::filesystem::perms::group_read | std::filesystem::perms::others_read);
}
}

std::vector<uint8_t> readLodMap(const std::filesystem::path &path, const std::string &name)
{
    const Archive archive(path);
    if (!archive.isMapArchive)
    {
        throw std::runtime_error("Expected a map LOD archive");
    }
    const Entry &entry = archive.entries.at(lower(name));
    const Bytes data = readRange(path, entry.offset, entry.size);
    if (data.size() < 16 || number(data, 4) != 0x6969766d || number(data, 8) != data.size() - 16
        || number(data, 12) == 0)
    {
        throw std::runtime_error("Invalid compressed map wrapper");
    }
    return unpack(data, 16, number(data, 8), number(data, 12));
}

std::vector<ConvertedLodImage> convertLodSprites(const std::vector<std::filesystem::path> &paths,
    const std::vector<std::string> &names, const std::filesystem::path &destination)
{
    std::vector<Archive> archives;
    for (const std::filesystem::path &path : paths)
    {
        archives.emplace_back(path);
    }
    std::vector<ConvertedLodImage> images;
    for (const std::string &requested : names)
    {
        const std::string name = lower(requested);
        const Archive *pOwner = nullptr;
        for (const Archive &archive : archives)
        {
            if (archive.entries.contains(name))
            {
                pOwner = &archive;
            }
        }
        if (pOwner == nullptr)
        {
            throw std::runtime_error("Missing sprite: " + name);
        }
        const Entry &entry = pOwner->entries.at(name);
        const Bytes data = readRange(pOwner->path, entry.offset, entry.size);
        const uint32_t width = number(data, 16, 2);
        const uint32_t height = number(data, 18, 2);
        const uint32_t stored = number(data, 12);
        const uint32_t expanded = number(data, 28);
        const size_t header = 32 + height * 8;
        if (resourceName(data, 0, 12) != name || width == 0 || height == 0
            || width > 4096 || height > 4096 || header + uint64_t(stored) != data.size())
        {
            throw std::runtime_error("Invalid sprite header: " + name);
        }
        const Bytes pixels = unpack(data, header, stored, expanded);
        const size_t stride = (width + 3) & ~size_t(3);
        // Preserve indices: the renderer applies SFT palette variants and keys index zero.
        Bytes bitmap(1078 + stride * height, 0);
        bitmap[0] = 'B';
        bitmap[1] = 'M';
        putNumber(bitmap, 2, uint32_t(bitmap.size()));
        putNumber(bitmap, 10, 1078);
        putNumber(bitmap, 14, 40);
        putNumber(bitmap, 18, width);
        putNumber(bitmap, 22, height);
        putNumber(bitmap, 26, 1, 2);
        putNumber(bitmap, 28, 8, 2);
        putNumber(bitmap, 34, uint32_t(stride * height));
        putNumber(bitmap, 46, 256);
        for (size_t i = 0; i < 256; ++i)
        {
            bitmap[54 + i * 4] = bitmap[55 + i * 4] = bitmap[56 + i * 4] = uint8_t(i);
        }
        for (uint32_t y = 0; y < height; ++y)
        {
            const int16_t left = int16_t(number(data, 32 + y * 8, 2));
            const int16_t right = int16_t(number(data, 34 + y * 8, 2));
            const uint32_t offset = number(data, 36 + y * 8);
            if (left == -1 && right == -1)
            {
                continue;
            }
            if (left < 0 || right < left || uint32_t(right) >= width
                || uint64_t(offset) + uint32_t(right - left + 1) > pixels.size())
            {
                throw std::runtime_error("Invalid sprite scanline: " + name);
            }
            std::copy_n(pixels.begin() + offset, right - left + 1,
                bitmap.begin() + 1078 + (height - 1 - y) * stride + left);
        }
        write(destination / (name + ".bmp"), bitmap);
        images.push_back({name + ".bmp", pOwner->path.filename().string(), width, height});
    }
    return images;
}

void convertLodPalettes(const std::vector<std::filesystem::path> &paths,
    const std::vector<int16_t> &paletteIds, const std::filesystem::path &destination)
{
    std::vector<Archive> archives;
    for (const std::filesystem::path &path : paths)
    {
        archives.emplace_back(path);
    }
    for (int16_t id : paletteIds)
    {
        std::ostringstream nameStream;
        nameStream << "pal" << std::setw(3) << std::setfill('0') << id;
        const std::string name = nameStream.str();
        const Archive *pOwner = nullptr;
        for (const Archive &archive : archives)
        {
            if (archive.entries.contains(name))
            {
                pOwner = &archive;
            }
        }
        if (pOwner == nullptr)
        {
            throw std::runtime_error("Missing palette: " + name);
        }
        const Entry &entry = pOwner->entries.at(name);
        const Bytes data = readRange(pOwner->path, entry.offset, entry.size);
        const size_t header = pOwner->nameSize + 32;
        const uint32_t stored = number(data, pOwner->nameSize + 4);
        if (resourceName(data, 0, pOwner->nameSize) != name || header + uint64_t(stored) + 768 != data.size())
        {
            throw std::runtime_error("Invalid palette: " + name);
        }
        write(destination / (name + ".act"), Bytes(data.end() - 768, data.end()));
    }
}

std::vector<ConvertedLodImage> convertLodImages(const std::vector<std::filesystem::path> &paths,
    const std::vector<std::string> &names, const std::filesystem::path &destination, LodImagePixelFormat pixelFormat)
{
    std::vector<Archive> archives;
    for (const std::filesystem::path &path : paths)
    {
        archives.emplace_back(path);
        if (archives.back().isMapArchive)
        {
            throw std::runtime_error("Expected an image LOD archive");
        }
    }
    std::vector<ConvertedLodImage> images;
    for (const std::string &requestedName : names)
    {
        const std::string name = lower(requestedName);
        const Archive *pOwner = nullptr;
        for (const Archive &archive : archives)
        {
            if (archive.entries.contains(name))
            {
                pOwner = &archive;
            }
        }
        if (pOwner == nullptr)
        {
            throw std::runtime_error("Missing image: " + name);
        }
        const Image converted = decode(*pOwner, name, pixelFormat);
        const std::string fileName = name.ends_with(".pcx") ? name : name + converted.extension;
        write(destination / fileName, converted.bytes);
        images.push_back({fileName, pOwner->path.filename().string(), converted.width, converted.height});
    }
    return images;
}

void writeConvertedResource(const std::filesystem::path &path, const std::vector<uint8_t> &bytes)
{
    write(path, bytes);
}

std::vector<std::string> listLodMembers(const std::filesystem::path &path)
{
    const Archive archive(path);
    std::vector<std::string> names;
    for (const auto &[name, entry] : archive.entries)
    {
        names.push_back(name);
    }
    return names;
}

std::vector<uint8_t> readLodText(const std::filesystem::path &path, const std::string &requestedName)
{
    const Archive archive(path);
    return readWrappedResource(archive, lower(requestedName));
}

void visitLodTextResources(const std::filesystem::path &path, const std::vector<std::string> &names,
    const std::function<void(const std::string &, const std::vector<uint8_t> &)> &visitor)
{
    const Archive archive(path);
    for (const std::string &requestedName : names)
    {
        const std::string name = lower(requestedName);
        visitor(name, readWrappedResource(archive, name));
    }
}
}
