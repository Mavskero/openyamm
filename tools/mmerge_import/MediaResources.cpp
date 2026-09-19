#include "MediaResources.h"
#include "LodResources.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Bytes = std::vector<uint8_t>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &relativeName)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relativeName);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Media source escapes input root: " + relativeName);
    require(std::filesystem::is_regular_file(path), "Media source is not a file: " + relativeName);
    return path;
}

std::optional<std::string> findVideoArchive(const std::filesystem::path &root, const std::string &requested)
{
    const std::filesystem::path directory = root / "Anims";
    if (!std::filesystem::exists(directory))
    {
        return std::nullopt;
    }
    const std::string filename = std::filesystem::path(requested).filename().string();
    std::optional<std::string> result;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        std::string name = entry.path().filename().string();
        for (char &character : name)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character += 'a' - 'A';
            }
        }
        if (name != filename)
        {
            continue;
        }
        require(!result, "Ambiguous video archive filename casing: " + requested);
        result = "Anims/" + entry.path().filename().string();
    }
    return result;
}

Bytes read(std::ifstream &stream, uint64_t offset, size_t size)
{
    stream.seekg(static_cast<std::streamoff>(offset));
    Bytes bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Truncated VID archive");
    return bytes;
}

uint32_t number(const Bytes &bytes, size_t offset)
{
    require(offset <= bytes.size() && bytes.size() - offset >= 4, "Truncated VID integer");
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8)
        | (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
}

struct Clip
{
    std::string name;
    uint32_t offset;
    uint64_t size;
};

std::vector<Clip> readIndex(std::ifstream &stream, uint64_t archiveSize)
{
    require(archiveSize >= 4 && archiveSize <= 1024ULL * 1024 * 1024, "Invalid VID archive size");
    const uint32_t count = number(read(stream, 0, 4), 0);
    const uint64_t dataStart = 4 + uint64_t(count) * 44;
    require(count > 0 && count <= 4096 && dataStart < archiveSize, "Invalid VID entry count");
    const Bytes index = read(stream, 4, count * 44);
    std::vector<Clip> clips;
    std::set<std::string> names;
    for (uint32_t slot = 0; slot < count; ++slot)
    {
        const size_t start = size_t(slot) * 44;
        std::string name;
        size_t length = 0;
        for (; length < 40 && index[start + length] != 0; ++length)
        {
            char character = char(index[start + length]);
            require(character >= 32 && character < 127 && character != '/' && character != '\\'
                && character != ':' && character != '"', "Invalid VID member name");
            if (character >= 'A' && character <= 'Z')
            {
                character += 'a' - 'A';
            }
            name += character;
        }
        require(length > 4 && length < 40 && (name.ends_with(".smk") || name.ends_with(".bik")),
            "Unsupported or unterminated VID member name");
        require(names.insert(name).second, "Duplicate VID member name: " + name);
        const uint32_t offset = number(index, start + 40);
        require(offset >= dataStart && offset < archiveSize, "VID offset outside payload area: " + name);
        clips.push_back({name, offset, 0});
    }
    std::sort(clips.begin(), clips.end(), [](const Clip &left, const Clip &right)
    {
        return left.offset < right.offset;
    });
    for (size_t index = 0; index < clips.size(); ++index)
    {
        Clip &clip = clips[index];
        const uint64_t end = index + 1 < clips.size() ? clips[index + 1].offset : archiveSize;
        require(end > clip.offset, "Overlapping VID offsets: " + clip.name);
        clip.size = end - clip.offset;
        require(clip.size >= 4 && clip.size <= 128 * 1024 * 1024, "Invalid VID clip size: " + clip.name);
    }
    return clips;
}

bool isNpcPortrait(const std::string &name)
{
    return name.size() == 7 && name.starts_with("npc")
        && std::all_of(name.begin() + 3, name.end(), [](char character)
        {
            return character >= '0' && character <= '9';
        });
}

struct MusicImport
{
    size_t count = 0;
    uint64_t bytes = 0;
    bool directoryMissing = false;
    std::string entries;
};

MusicImport importMusic(const std::filesystem::path &root, const std::filesystem::path &output)
{
    MusicImport result;
    const std::filesystem::path directory = root / "Music";
    result.directoryMissing = !std::filesystem::exists(std::filesystem::symlink_status(directory));
    if (result.directoryMissing)
    {
        return result;
    }
    const std::filesystem::path resolved = std::filesystem::canonical(directory);
    const std::filesystem::path relative = resolved.lexically_relative(root);
    require(!relative.empty() && relative != "." && !relative.is_absolute() && *relative.begin() != ".."
        && std::filesystem::is_directory(resolved), "Music directory escapes input root or is not a directory");
    std::map<int, std::string> tracks;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory))
    {
        std::string extension = entry.path().extension().string();
        for (char &character : extension)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character += 'a' - 'A';
            }
        }
        if (extension != ".mp3")
        {
            continue;
        }
        const std::string stem = entry.path().stem().string();
        require(!stem.empty() && stem.size() <= 6 && stem.front() != '0'
            && std::all_of(stem.begin(), stem.end(), [](char character)
            {
                return character >= '0' && character <= '9';
            }), "Unsupported merged music track name: " + entry.path().filename().string());
        require(tracks.emplace(std::stoi(stem), "Music/" + entry.path().filename().string()).second,
            "Ambiguous merged music track: " + stem);
        require(tracks.size() <= 512, "Too many merged music tracks");
    }
    std::ostringstream entries;
    for (const auto &[track, sourceName] : tracks)
    {
        const std::filesystem::path path = checkedPath(root, sourceName);
        const uint64_t size = std::filesystem::file_size(path);
        require(size >= 10 && size <= 128 * 1024 * 1024, "Invalid MP3 payload size: " + sourceName);
        result.bytes += size;
        require(result.bytes <= 1024ULL * 1024 * 1024, "Merged music exceeds 1 GiB");
        std::ifstream stream(path, std::ios::binary);
        Bytes bytes(size);
        require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), size)), "Truncated MP3: " + sourceName);
        // Signature screening only. Full decoding belongs to the shared FFmpeg player.
        const bool id3 = bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3';
        const bool frame = bytes[0] == 0xff && (bytes[1] & 0xe6) == 0xe2
            && ((bytes[1] >> 3) & 3) != 1 && ((bytes[2] >> 2) & 3) != 3;
        require(id3 || frame, "Unsupported MP3 signature: " + sourceName);
        const std::string outputName = "engine/music/" + std::to_string(track) + ".mp3";
        writeConvertedResource(output / outputName, bytes);
        entries << (result.count++ == 0 ? "" : ",\n") << "{\"track\":" << track
            << ",\"source\":" << std::quoted(sourceName) << ",\"path\":" << std::quoted(outputName)
            << ",\"bytes\":" << size << "}";
    }
    result.entries = entries.str();
    return result;
}
}

void convertMediaResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Media output must be outside input root");
    require(!std::filesystem::exists(output), "Media destination already exists");
    require(std::filesystem::create_directory(output), "Cannot create fresh media destination");
    try
    {
        const std::array<std::pair<const char *, const char *>, 5> owners = {{
            {"", "worlds/mm8"}, {"mm6", "worlds/mm6"}, {"mm7", "worlds/mm7"},
            {"mm8", "worlds/mm8"}, {"mmmerge", "engine"}
        }};
        struct SelectedVideo
        {
            std::string owner;
            std::string outputOwner;
            std::string archiveName;
            std::filesystem::path archive;
            Clip clip;
        };
        std::map<std::string, size_t> videoNames;
        std::vector<SelectedVideo> selectedVideos;
        std::ostringstream videoOverrides;
        size_t videoOverrideCount = 0;
        std::set<std::string> portraitNames;
        std::vector<std::string> missingArchives;
        std::ostringstream videos;
        std::ostringstream portraits;
        uint64_t videoBytes = 0;
        for (const auto &[pSourceOwner, pOutputOwner] : owners)
        {
            const std::filesystem::path package = output / pOutputOwner;
            for (const char *pArchiveKind : {"magicdod", "mightdod"})
            {
                const std::string prefix = *pSourceOwner == '\0' ? "" : std::string(pSourceOwner) + ".";
                const std::string requestedName = "Anims/" + prefix + pArchiveKind + ".vid";
                const std::optional<std::string> sourceName = findVideoArchive(root, requestedName);
                if (!sourceName)
                {
                    missingArchives.push_back(requestedName);
                    continue;
                }
                const std::string &archiveName = *sourceName;
                const std::filesystem::path archive = checkedPath(root, archiveName);
                std::ifstream stream(archive, std::ios::binary);
                const std::vector<Clip> clips = readIndex(stream, std::filesystem::file_size(archive));
                for (const Clip &clip : clips)
                {
                    const SelectedVideo selection{pSourceOwner, pOutputOwner, archiveName, archive, clip};
                    const auto previous = videoNames.find(clip.name);
                    if (previous == videoNames.end())
                    {
                        videoNames.emplace(clip.name, selectedVideos.size());
                        selectedVideos.push_back(selection);
                    }
                    else
                    {
                        SelectedVideo &shadowed = selectedVideos[previous->second];
                        require(shadowed.owner.empty()
                            && (selection.owner == "mm8" || selection.owner == "mmmerge"),
                            "Ambiguous native video ownership requires an explicit override profile: " + clip.name);
                        videoOverrides << (videoOverrideCount++ == 0 ? "" : ",") << "{\"name\":"
                            << std::quoted(clip.name) << ",\"selected\":" << std::quoted(archiveName)
                            << ",\"shadowed\":" << std::quoted(shadowed.archiveName) << '}';
                        shadowed = selection;
                    }
                }
            }
            // MMerge's prefixed icon archive already remaps MM8 portraits into the merged NPC registry.
            if (*pSourceOwner == '\0')
            {
                continue;
            }
            const std::string archiveName = "Data/" + std::string(pSourceOwner) + ".icons.lod";
            if (!std::filesystem::exists(root / archiveName))
            {
                missingArchives.push_back(archiveName);
                continue;
            }
            const std::filesystem::path archive = checkedPath(root, archiveName);
            std::vector<std::string> selected;
            for (const std::string &name : listLodMembers(archive))
            {
                if (isNpcPortrait(name))
                {
                    require(portraitNames.insert(name).second,
                        "Ambiguous NPC portrait ownership requires an explicit override profile: " + name);
                    selected.push_back(name);
                }
            }
            const std::vector<ConvertedLodImage> images = convertLodImages({archive}, selected, package / "icons");
            for (const ConvertedLodImage &image : images)
            {
                portraits << (portraits.tellp() == 0 ? "" : ",\n") << "{\"path\":"
                    << std::quoted(std::string(pOutputOwner) + "/icons/" + image.name)
                    << ",\"archive\":" << std::quoted(archiveName)
                    << ",\"width\":" << image.width << ",\"height\":" << image.height << "}";
            }
        }
        for (const SelectedVideo &selection : selectedVideos)
        {
            std::ifstream stream(selection.archive, std::ios::binary);
            const Clip &clip = selection.clip;
            const Bytes bytes = read(stream, clip.offset, clip.size);
            const std::string signature(bytes.begin(), bytes.begin() + 4);
            require((clip.name.ends_with(".smk") && (signature == "SMK2" || signature == "SMK4"))
                || (clip.name.ends_with(".bik") && (signature == "BIKb" || signature == "BIKf"
                    || signature == "BIKg" || signature == "BIKh" || signature == "BIKi")),
                "VID clip signature does not match supported format: " + clip.name);
            const std::string outputName = selection.outputOwner + "/videos/" + clip.name;
            writeConvertedResource(output / outputName, bytes);
            videos << (videos.tellp() == 0 ? "" : ",\n") << "{\"path\":" << std::quoted(outputName)
                << ",\"archive\":" << std::quoted(selection.archiveName) << ",\"offset\":" << clip.offset
                << ",\"bytes\":" << clip.size << ",\"signature\":" << std::quoted(signature) << "}";
            videoBytes += clip.size;
        }
        const MusicImport music = importMusic(root, output);
        require(!videoNames.empty() || !portraitNames.empty() || music.count > 0, "No supported original media found");
        std::ostringstream manifest;
        manifest << "{\"profile\":\"MMergeNativeMediaV2\",\"runtime_ready\":false,"
            "\"playback_validated\":false,\"video_payloads_transcoded\":false,\"video_count\":" << videoNames.size()
            << ",\"video_bytes\":" << videoBytes << ",\"portrait_count\":" << portraitNames.size()
            << ",\"music_payloads_transcoded\":false,\"music_count\":" << music.count
            << ",\"music_bytes\":" << music.bytes << ",\"music\":[" << music.entries
            << "],\"missing_music_directory\":" << (music.directoryMissing ? "true" : "false")
            << ",\"videos\":[" << videos.str() << "],\"portraits\":[" << portraits.str()
            << "],\"video_override_count\":" << videoOverrideCount
            << ",\"video_overrides\":[" << videoOverrides.str() << "],\"missing_profile_archives\":[";
        for (size_t index = 0; index < missingArchives.size(); ++index)
        {
            manifest << (index == 0 ? "" : ",") << std::quoted(missingArchives[index]);
        }
        manifest << "]}\n";
        const std::string text = manifest.str();
        writeConvertedResource(output / "media_conversion.json", Bytes(text.begin(), text.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
