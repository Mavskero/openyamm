#include "SoundResources.h"
#include "SoundPayloadResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/audio/SoundCatalog.h"
#include "game/tables/MergedBaseTables.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Bytes = std::vector<uint8_t>;
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
constexpr char ExecutableDigest[] = "97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b";

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Bytes read(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Sound input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid sound input size");
    Bytes bytes(size);
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read sound source");
    return bytes;
}

std::string digest(const Bytes &bytes)
{
    std::array<uint8_t, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), hash.data());
    std::ostringstream result;
    for (uint8_t value : hash)
    {
        result << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return result.str();
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid sound table quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 != table->getRowCount() || row != Row{""})
        {
            rows.push_back(row);
        }
    }
    return rows;
}

uint32_t number(const std::string &text)
{
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(), "Invalid sound ID");
    return value;
}
}

void convertSoundResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Sound output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Sound output already exists");
    const Bytes executable = read(root, "mm8.exe");
    require(executable.size() == 1155072 && digest(executable) == ExecutableDigest,
        "Sound lookup behavior needs a verified MM8 executable profile");
    const Bytes sourceBytes = read(root, "Data/Tables/Sounds.txt");
    size_t chineseGlyphs = 0;
    const std::string sourceText = decodeLegacyText(sourceBytes, false, chineseGlyphs);
    // The source StructsArray reader uses literal tabs and lines, not CSV quoting.
    Rows sourceRows;
    std::istringstream lines(sourceText);
    for (std::string line; std::getline(lines, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        Row row;
        size_t start = 0;
        while (true)
        {
            const size_t end = line.find('\t', start);
            row.push_back(line.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        sourceRows.push_back(std::move(row));
    }
    require(sourceRows.size() > 1 && sourceRows[0] == Row{"Name", "Id", "Type", "Is3D"},
        "Unsupported source sound header");
    std::map<uint32_t, Row> firstRows;
    size_t positionalRows = 0;
    size_t zeroRows = 0;
    size_t duplicateRows = 0;
    std::ostringstream duplicates;
    duplicates << '[';
    for (size_t index = 1; index < sourceRows.size(); ++index)
    {
        const Row &row = sourceRows[index];
        require(row.size() == 4 && !row[0].empty() && row[0][0] != '/'
            && row[0].find_first_of("\r\n\t") == std::string::npos
            && row[0].find('\0') == std::string::npos
            && (row[2] == "0" || row[2] == "system" || row[2] == "swap" || row[2] == "lock")
            && (row[3] == "x" || row[3] == "-"), "Invalid source sound row");
        const uint32_t id = number(row[1]);
        positionalRows += row[3] == "x";
        zeroRows += id == 0;
        if (!firstRows.emplace(id, row).second)
        {
            duplicates << (duplicateRows++ == 0 ? "" : ",") << "{\"id\":" << id
                << ",\"source_row\":" << index << ",\"selected\":" << std::quoted(firstRows.at(id)[0])
                << ",\"shadowed\":" << std::quoted(row[0]) << '}';
        }
    }
    duplicates << ']';
    require(firstRows.contains(0) && firstRows.at(0) == Row{"null", "0", "system", "-"},
        "Missing original sound-zero sentinel");
    Rows exported = {{"#SoundCatalogMode", "MM8OrderedV1"}};
    exported.insert(exported.end(), sourceRows.begin(), sourceRows.end());
    const std::string text = serializeTextRows(exported);
    require(parse(text) == exported, "Sound serialization changed original fields");
    SoundCatalog catalog;
    std::string error;
    require(catalog.loadFromScopedRows(parse(text), {}, error), "Sound catalog loading failed: " + error);
    for (const auto &[id, row] : firstRows)
    {
        const SoundCatalogEntry *pEntry = catalog.findById(id);
        const uint32_t type = row[2] == "system" ? 1 : row[2] == "swap" ? 2 : row[2] == "lock" ? 4 : 0;
        require(pEntry && pEntry->name == row[0] && pEntry->scope == SoundScope::Engine
            && pEntry->positional == (row[3] == "x") && pEntry->loadType == type,
            "Sound lookup changed source order, type or positioning");
    }
    const Bytes voices = read(assets, "engine/data_tables/character_voices.txt");
    MergedCharacterVoiceTable voiceTable;
    require(voiceTable.loadFromRows(parse(std::string(voices.begin(), voices.end()))), "Invalid voice dependency");
    std::set<uint32_t> missingVoiceIds;
    size_t voiceReferences = 0;
    for (const MergedCharacterVoiceEntry &entry : voiceTable.entries())
    {
        for (uint32_t id : entry.soundIdsByVoiceSetId)
        {
            if (id == 0)
            {
                continue;
            }
            ++voiceReferences;
            if (!catalog.findById(id))
            {
                missingVoiceIds.insert(id);
            }
        }
    }
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"sound_catalog\",\"runtime_ready\":false,\"source_rows\":"
        << sourceRows.size() - 1 << ",\"lookup_ids\":" << firstRows.size() << ",\"zero_rows\":" << zeroRows
        << ",\"positional_rows\":" << positionalRows << ",\"duplicates\":" << duplicates.str()
        << ",\"source_sha256\":\"" << digest(sourceBytes) << "\",\"exe_sha256\":\"" << ExecutableDigest
        << "\",\"typed_validation\":true,\"voice_references\":" << voiceReferences << ",\"missing_voice_ids\":[";
    bool first = true;
    for (uint32_t id : missingVoiceIds)
    {
        report << (first ? "" : ",") << id;
        first = false;
    }
    require(std::filesystem::create_directory(output), "Cannot create fresh sound output");
    try
    {
        writeConvertedResource(output / "engine/data_tables/sounds.txt", Bytes(text.begin(), text.end()));
        const SoundPayloadImport payloads = importSoundPayloadResources(root, output);
        report << "],\"audio_files_exported\":" << payloads.names.size()
            << ",\"audio_bytes_exported\":" << payloads.bytes << ",\"missing_sample_ids\":[";
        first = true;
        for (const auto &[id, row] : firstRows)
        {
            if (id == 0)
            {
                continue;
            }
            std::string name = catalog.findById(id)->name + ".wav";
            for (char &character : name)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    character += 'a' - 'A';
                }
            }
            if (!payloads.names.contains(name))
            {
                report << (first ? "" : ",") << id;
                first = false;
            }
        }
        report << "],\"audio_playback_validated\":false,\"executable_executed\":false,"
            << "\"original_cache_policy_applied\":false,\"patched_windows_parity\":false}\n";
        const std::string reportText = report.str();
        writeConvertedResource(output / "sound_conversion.json", Bytes(reportText.begin(), reportText.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
