#include "SoundPayloadResources.h"
#include "LodResources.h"

#include <array>
#include <iomanip>
#include <map>
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

uint32_t number(const Bytes &bytes, size_t offset, size_t count = 4)
{
    require(offset <= bytes.size() && count <= bytes.size() - offset, "Truncated WAV integer");
    uint32_t value = 0;
    for (size_t index = 0; index < count; ++index)
    {
        value |= uint32_t(bytes[offset + index]) << (index * 8);
    }
    return value;
}

struct Wave
{
    std::string archive;
    size_t bytes = 0;
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t sourceFrames = 0;
};

Wave inspectWave(const Bytes &bytes)
{
    require(bytes.size() >= 12 && bytes.size() <= 16 * 1024 * 1024
        && number(bytes, 0) == 0x46464952 && number(bytes, 8) == 0x45564157
        && uint64_t(number(bytes, 4)) + 8 == bytes.size(), "Invalid original WAV/RIFF size or signature");
    Wave wave;
    wave.bytes = bytes.size();
    bool hasFormat = false;
    bool hasData = false;
    bool hasFact = false;
    for (size_t position = 12; position < bytes.size();)
    {
        require(bytes.size() - position >= 8, "Truncated WAV chunk header");
        const uint32_t kind = number(bytes, position);
        const uint32_t size = number(bytes, position + 4);
        position += 8;
        const uint64_t paddedSize = uint64_t(size) + (size & 1);
        require(paddedSize <= bytes.size() - position, "WAV chunk outside RIFF payload");
        if (kind == 0x20746d66) // fmt
        {
            require(!hasFormat && size >= 20 && number(bytes, position, 2) == 17,
                "Original sound profile requires one IMA ADPCM format chunk");
            wave.channels = number(bytes, position + 2, 2);
            wave.sampleRate = number(bytes, position + 4);
            const uint32_t blockBytes = number(bytes, position + 12, 2);
            require((wave.channels == 1 || wave.channels == 2) && wave.sampleRate >= 8000
                && wave.sampleRate <= 192000 && number(bytes, position + 14, 2) == 4
                && number(bytes, position + 16, 2) >= 2
                && uint64_t(number(bytes, position + 16, 2)) + 18 <= size
                && blockBytes > 4 * wave.channels && blockBytes % (4 * wave.channels) == 0
                && number(bytes, position + 18, 2) == 1 + (blockBytes - 4 * wave.channels) * 2 / wave.channels,
                "Unsupported original IMA ADPCM layout");
            hasFormat = true;
        }
        else if (kind == 0x61746164) // data
        {
            require(!hasData && size > 0, "Missing or duplicate WAV sample data");
            hasData = true;
        }
        else if (kind == 0x74636166) // fact
        {
            require(!hasFact && size >= 4, "Invalid or duplicate WAV fact chunk");
            wave.sourceFrames = number(bytes, position);
            require(wave.sourceFrames > 0, "Empty original WAV sample count");
            hasFact = true;
        }
        position += size_t(paddedSize);
    }
    require(hasFormat && hasData && hasFact, "Original WAV missing format, samples or fact");
    return wave;
}
}

SoundPayloadImport importSoundPayloadResources(const std::filesystem::path &root,
    const std::filesystem::path &output)
{
    // MM8's original EnglishD supplies shared effects. The prefixed MM8 layer replaces its voices;
    // the named language archive is the explicit Simplified Chinese overlay.
    const std::array<const char *, 6> profile = {
        "EnglishD.lod", "mm6.D.lod", "mm7.D.lod", "mm8.D.lod", "mmmerge.D.lod", "10 LocZHCN.EnglishD.lod"
    };
    std::map<std::string, Wave> selected;
    std::vector<std::string> missing;
    std::ostringstream overrides;
    size_t overrideCount = 0;
    uint64_t inspectedBytes = 0;
    for (size_t archiveIndex = 0; archiveIndex < profile.size(); ++archiveIndex)
    {
        const std::string archiveName = "Data/" + std::string(profile[archiveIndex]);
        const std::filesystem::path input = root / archiveName;
        if (!std::filesystem::exists(std::filesystem::symlink_status(input)))
        {
            missing.push_back(archiveName);
            continue;
        }
        const std::filesystem::path path = std::filesystem::canonical(input);
        const std::filesystem::path relative = path.lexically_relative(root);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
            && std::filesystem::is_regular_file(path), "Sound archive escapes input root or is not a file");
        std::vector<std::string> names;
        for (const std::string &name : listLodMembers(path))
        {
            if (name.ends_with(".wav"))
            {
                require(name.size() > 4, "Empty WAV resource name");
                const bool mm8Override = archiveName == "Data/mm8.D.lod" && selected.contains(name)
                    && selected.at(name).archive == "Data/EnglishD.lod";
                require(archiveIndex == profile.size() - 1 || !selected.contains(name) || mm8Override,
                    "Ambiguous base sound ownership requires an explicit profile: " + name);
                names.push_back(name);
            }
        }
        require(selected.size() + names.size() <= 20000, "Too many original sound records");
        visitLodTextResources(path, names, [&](const std::string &name, const Bytes &bytes)
        {
            Wave wave;
            try
            {
                wave = inspectWave(bytes);
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error(archiveName + "/" + name + ": " + error.what());
            }
            inspectedBytes += bytes.size();
            require(inspectedBytes <= 512 * 1024 * 1024, "Original sound payloads exceed 512 MiB");
            wave.archive = archiveName;
            if (selected.contains(name))
            {
                overrides << (overrideCount++ == 0 ? "" : ",") << "{\"name\":" << std::quoted(name)
                    << ",\"selected\":" << std::quoted(archiveName)
                    << ",\"shadowed\":" << std::quoted(selected.at(name).archive) << "}";
            }
            selected[name] = wave;
            writeConvertedResource(output / "engine/audio" / name, bytes);
        });
    }
    SoundPayloadImport result;
    std::ostringstream report;
    report << "{\"profile\":\"MMergeOriginalWavZHCNV2\",\"payloads_transcoded\":false,"
        "\"audio_playback_validated\":false,\"runtime_ready\":false,\"files\":[";
    for (const auto &[name, wave] : selected)
    {
        report << (result.names.empty() ? "" : ",\n") << "{\"name\":" << std::quoted(name)
            << ",\"archive\":" << std::quoted(wave.archive) << ",\"bytes\":" << wave.bytes
            << ",\"format\":17,\"channels\":" << wave.channels << ",\"sample_rate\":" << wave.sampleRate
            << ",\"source_fact_frames\":" << wave.sourceFrames << "}";
        result.names.insert(name);
        result.bytes += wave.bytes;
    }
    report << "],\"exported_count\":" << result.names.size() << ",\"exported_bytes\":" << result.bytes
        << ",\"override_count\":" << overrideCount << ",\"overrides\":[" << overrides.str()
        << "],\"missing_profile_archives\":[";
    for (size_t index = 0; index < missing.size(); ++index)
    {
        report << (index == 0 ? "" : ",") << std::quoted(missing[index]);
    }
    report << "]}\n";
    const std::string text = report.str();
    writeConvertedResource(output / "sound_payloads.json", Bytes(text.begin(), text.end()));
    return result;
}
}
