#include "DecorationResources.h"
#include "LodResources.h"
#include "SpriteFrameResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/maps/MapDecorationDirection.h"
#include "game/tables/SpriteTables.h"

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
using Owners = std::map<std::string, std::filesystem::path>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t number(const std::string &text, uint32_t maximum = 65535)
{
    uint32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value <= maximum,
        "Invalid decoration integer: " + text);
    return value;
}

std::string lower(std::string text)
{
    for (char &letter : text)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return text;
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Decoration input escapes root");
    return path;
}

Bytes read(const std::filesystem::path &path)
{
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid decoration source size");
    Bytes bytes(size);
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read decoration source");
    return bytes;
}

Rows parse(const Bytes &bytes, const Row &header)
{
    size_t glyphs = 0;
    const std::optional<Engine::TextTable> table =
        Engine::TextTable::parseTabSeparated(decodeLegacyText(bytes, false, glyphs));
    require(table && table->getRowCount() > 1 && table->getRow(0) == header, "Unknown decoration/SFT header");
    Rows rows;
    for (size_t index = 1; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        require(row.size() == header.size() && number(row[0]) == index - 1,
            "Decoration/SFT source index or width changed");
        rows.push_back(row);
    }
    require(rows.size() <= 65536, "Decoration registry exceeds runtime ID range");
    return rows;
}

void write(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, Bytes(text.begin(), text.end()));
}

Owners archiveOwners(const std::filesystem::path &root, const std::vector<std::string> &names,
    std::vector<std::filesystem::path> &archives)
{
    Owners owners;
    for (const std::string &name : names)
    {
        if (!std::filesystem::exists(root / "Data" / name))
        {
            continue;
        }
        const std::filesystem::path archive = checkedPath(root, "Data/" + name);
        archives.push_back(archive);
        for (const std::string &member : listLodMembers(archive))
        {
            owners[lower(member)] = archive;
        }
    }
    return owners;
}
}

void convertDecorationResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Decoration output must be outside source");
    require(!std::filesystem::exists(output), "Decoration output already exists");
    const Bytes decorationBytes = read(checkedPath(root, "Data/Tables/DecList.txt"));
    const Bytes frameBytes = read(checkedPath(root, "Data/Tables/SFT.txt"));
    const Rows sourceRows = parse(decorationBytes, {"#", "Name", "GameName", "Type", "Height", "Radius",
        "LightRadius", "SFTGroup", "NoBlockMovement", "NoDraw", "FlickerSlow", "FlickerMedium", "FlickerFast",
        "Marker", "SlowLoop", "EmitFire", "SoundOnDawn", "SoundOnDusk", "EmitSmoke", "SoundId", "Red", "Green",
        "Blue"});
    const Rows frameRows = parse(frameBytes, {"#", "GroupName", "SpriteName", "Scale", "Luminous", "GroupStart",
        "Image1", "Center", "Fidget", "Loaded", "Mirror0", "Mirror1", "Mirror2", "Mirror3", "Mirror4", "Mirror5",
        "Mirror6", "Mirror7", "Images3", "Glow", "Transparent", "LightRadius", "PaletteId", "Time"});
    const ConvertedSpriteFrames frames = convertSpriteFrameRows(frameRows);
    Rows rows{{"#", "InternalName", "Hint", "Type", "Radius", "Height", "LightRadius", "Red", "Green", "Blue",
        "SoundId", "Flags", "SpriteId", "SourceSFTGroup", "MapAliases", "SourceSoundId"}};
    std::set<std::string> names;
    size_t signedSounds = 0;
    for (size_t index = 0; index < sourceRows.size(); ++index)
    {
        const Row &row = sourceRows[index];
        const std::string name = lower(row[1]);
        require(!name.empty() && name.size() <= 32 && names.insert(name).second,
            "Invalid or duplicate decoration name");
        const std::string group = lower(row[7]);
        require(frames.groupIds.contains(group), "Unknown decoration SFT group: " + group);
        for (size_t field = 3; field <= 6; ++field)
        {
            number(row[field], 32767);
        }
        uint16_t flags = 0;
        for (size_t field = 8; field <= 18; ++field)
        {
            require(row[field] == "x" || row[field] == "-", "Invalid decoration flag");
            if (row[field] == "x")
            {
                flags |= uint16_t(1 << (field - 8));
            }
        }
        for (size_t field = 20; field <= 22; ++field)
        {
            number(row[field], 255);
        }
        const uint32_t sourceSound = number(row[19]);
        // MMExtension DecListItem defines SoundId as i2. Preserve its stored bit pattern;
        // retain the unsigned TXT value separately, without claiming ambient audio support.
        const int32_t storedSound = sourceSound <= 32767 ? int32_t(sourceSound) : int32_t(sourceSound) - 65536;
        signedSounds += sourceSound > 32767;
        rows.push_back({std::to_string(index + 1), name, row[2], row[3], row[5], row[4], row[6],
            row[20], row[21], row[22], std::to_string(storedSound), std::to_string(flags),
            std::to_string(frames.groupIds.at(group)), row[7], "", row[19]});
    }
    DecorationTable decorations;
    require(decorations.loadRows(rows), "Converted decoration registry failed runtime validation");
    SpriteFrameTable sprites;
    std::string error;
    require(sprites.loadFromYaml(frames.yaml, error), "Cannot load converted decoration frames: " + error);
    const Bytes geometry = readLodMap(checkedPath(root, "Data/mm6.games.lod"), "oute3.odm");
    const std::optional<OutdoorMapData> map = OutdoorMapDataLoader().loadFromBytes(geometry);
    require(map.has_value(), "Cannot read oute3 decoration instances");
    std::set<uint16_t> visibleGroups;
    std::set<std::string> unresolvedNames;
    std::vector<size_t> startIndices;
    size_t namedResolutions = 0;
    for (size_t index = 0; index < map->entities.size(); ++index)
    {
        const OutdoorEntity &entity = map->entities[index];
        const DecorationLookupResult resolved = decorations.resolveMapDecoration(entity.decorationListId, entity.name);
        const DecorationEntry *pEntry = resolved.pEntry;
        if (!pEntry || (!entity.name.empty() && pEntry->internalName != lower(entity.name)))
        {
            unresolvedNames.insert(entity.name);
            continue;
        }
        namedResolutions += !entity.name.empty();
        if (pEntry->internalName == "party start")
        {
            startIndices.push_back(index);
        }
        if (pEntry->spriteId != 0 && !hasDecorationFlag(pEntry->flags, DecorationDescFlag::DontDraw))
        {
            visibleGroups.insert(pEntry->spriteId);
        }
    }
    std::set<std::string> textures;
    std::set<int16_t> palettes;
    for (uint16_t group : visibleGroups)
    {
        const std::vector<std::string> names = sprites.collectTextureNames(group);
        textures.insert(names.begin(), names.end());
        for (size_t index = group; index < frameRows.size(); ++index)
        {
            if (index != group && frameRows[index][5] == "x")
            {
                break;
            }
            palettes.insert(int16_t(number(frameRows[index][22], 32767)));
        }
    }
    std::vector<std::filesystem::path> spriteArchives;
    std::vector<std::filesystem::path> bitmapArchives;
    const Owners spriteOwners = archiveOwners(root,
        {"sprites.lod", "mm6.sprites.lod", "mm7.sprites.lod", "mmmerge.sprites.lod"}, spriteArchives);
    const Owners paletteOwners = archiveOwners(root,
        {"bitmaps.lod", "mm6.bitmaps.lod", "mm7.bitmaps.lod", "mm8.bitmaps.lod", "mmmerge.bitmaps.lod",
            "00 patch.bitmaps.lod"}, bitmapArchives);
    std::vector<std::string> presentTextures;
    std::vector<std::string> missingTextures;
    std::vector<int16_t> presentPalettes;
    std::vector<int16_t> missingPalettes;
    for (const std::string &texture : textures)
    {
        (spriteOwners.contains(texture) ? presentTextures : missingTextures).push_back(texture);
    }
    for (int16_t palette : palettes)
    {
        std::ostringstream name;
        name << "pal" << std::setw(3) << std::setfill('0') << palette;
        (paletteOwners.contains(name.str()) ? presentPalettes : missingPalettes).push_back(palette);
    }
    require(std::filesystem::create_directories(output), "Cannot create decoration output");
    try
    {
        const std::vector<ConvertedLodImage> images = convertLodSprites(spriteArchives, presentTextures,
            output / "worlds/mm6/sprites");
        convertLodPalettes(bitmapArchives, presentPalettes, output / "worlds/mm6/textures");
        write(output / "engine/data_tables/decoration_data.txt", serializeTextRows(rows));
        write(output / "engine/rendering/sprite_frame_data_common.yml", frames.yaml);
        writeConvertedResource(output / "import_sources/declist.txt", decorationBytes);
        writeConvertedResource(output / "import_sources/sft.txt", frameBytes);
        std::ostringstream report;
        report << "{\"schema\":1,\"stage\":\"decoration-registry-and-oute3-art\",\"runtime_ready\":false,"
            << "\"ambient_audio_validated\":false,\"localized_hints_validated\":false,\"decoration_rows\":"
            << sourceRows.size() << ",\"signed_sound_storage_rows\":" << signedSounds
            << ",\"sft_frames\":" << frames.frameCount << ",\"sft_groups\":" << frames.groupCount
            << ",\"map\":\"oute3.odm\",\"map_entities\":" << map->entities.size()
            << ",\"named_resolutions\":" << namedResolutions << ",\"visible_sft_groups\":" << visibleGroups.size()
            << ",\"sprites_exported\":" << images.size() << ",\"palettes_exported\":" << presentPalettes.size()
            << ",\"missing_sprites\":[";
        for (size_t index = 0; index < missingTextures.size(); ++index)
        {
            report << (index ? "," : "") << std::quoted(missingTextures[index]);
        }
        report << "],\"missing_palettes\":[";
        for (size_t index = 0; index < missingPalettes.size(); ++index)
        {
            report << (index ? "," : "") << missingPalettes[index];
        }
        report << "],\"unresolved_entity_names\":[";
        size_t count = 0;
        for (const std::string &name : unresolvedNames)
        {
            report << (count++ ? "," : "") << std::quoted(name);
        }
        report << "],\"party_starts\":[";
        for (size_t index = 0; index < startIndices.size(); ++index)
        {
            const OutdoorEntity &entity = map->entities[startIndices[index]];
            report << (index ? "," : "") << "{\"entity_index\":" << startIndices[index]
                << ",\"x\":" << entity.x << ",\"y\":" << entity.y << ",\"z\":" << entity.z
                << ",\"direction_yaw_units\":" << entity.facing << ",\"facing_degrees\":"
                << mapDecorationFacingDegrees(entity.facing, entity.variableSecondary) << '}';
        }
        report << "],\"sprite_owners\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            const ConvertedLodImage &image = images[index];
            report << (index ? "," : "") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << '}';
        }
        report << "]}\n";
        write(output / "decoration_conversion.json", report.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
