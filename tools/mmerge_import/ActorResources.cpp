#include "ActorResources.h"
#include "LodResources.h"
#include "SpriteFrameResources.h"
#include "engine/TextTable.h"
#include "game/maps/MapDeltaData.h"
#include "game/tables/SpriteTables.h"

#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Rows = std::vector<std::vector<std::string>>;

uint32_t integer(const std::string &text, uint32_t limit = 65535)
{
    uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() || value > limit)
    {
        throw std::runtime_error("Invalid actor table integer: " + text);
    }
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

Rows readTable(const std::filesystem::path &path, const std::string &header)
{
    if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
    {
        throw std::runtime_error("Actor table exceeds import limit");
    }
    std::ifstream stream(path, std::ios::binary);
    const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    const std::optional<Engine::TextTable> expected = Engine::TextTable::parseTabSeparated(header);
    if (!table || table->getRowCount() < 2 || table->getRow(0) != expected->getRow(0))
    {
        throw std::runtime_error("Unrecognized actor table schema: " + path.filename().string());
    }
    Rows rows;
    for (size_t i = 1; i < table->getRowCount(); ++i)
    {
        const std::vector<std::string> &row = table->getRow(i);
        if (row.size() != table->getRow(0).size() || integer(row[0]) != i - 1)
        {
            throw std::runtime_error("Actor table rows must preserve contiguous source indices");
        }
        rows.push_back(row);
    }
    return rows;
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertActorResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::vector<uint8_t> geometry = readLodMap(source / "Data/mm6.games.lod", "oute3.odm");
    const std::optional<Game::OutdoorMapData> map = Game::OutdoorMapDataLoader().loadFromBytes(geometry);
    const std::vector<uint8_t> companion = readLodMap(source / "Data/mm6.games.lod", "oute3.ddm");
    const std::optional<Game::MapDeltaData> state = map
        ? Game::MapDeltaDataLoader().loadOutdoorFromBytes(companion, *map) : std::nullopt;
    if (!state || state->actors.empty())
    {
        throw std::runtime_error("Outdoor companion has no readable actors");
    }
    const Rows monsters = readTable(source / "Data/Tables/MonList.txt",
        "#\tHeight\tRadius\tVelocity\tRadius2\tTintB\tTintG\tTintR\tTintA\tSoundAttack\tSoundDie"
        "\tSoundGetHit\tSoundFidget\tName\tFramesStand\tFramesWalk\tFramesAttack\tFramesShoot"
        "\tFramesStun\tFramesDie\tFramesDead\tFramesFidget");
    const Rows frames = readTable(source / "Data/Tables/SFT.txt",
        "#\tGroupName\tSpriteName\tScale\tLuminous\tGroupStart\tImage1\tCenter\tFidget\tLoaded"
        "\tMirror0\tMirror1\tMirror2\tMirror3\tMirror4\tMirror5\tMirror6\tMirror7\tImages3"
        "\tGlow\tTransparent\tLightRadius\tPaletteId\tTime");
    std::set<int16_t> actorIds;
    std::set<std::string> groupNames;
    for (const Game::MapDeltaActor &actor : state->actors)
    {
        // MMMerge stores the shared monster registry ID separately from the old
        // visual/cache ID in the companion. MonList rows are zero based, IDs are +1.
        if (actor.monsterInfoId <= 0 || size_t(actor.monsterInfoId) > monsters.size())
        {
            throw std::runtime_error("Actor references an unknown merged monster ID");
        }
        actorIds.insert(actor.monsterInfoId);
        const std::vector<std::string> &row = monsters[actor.monsterInfoId - 1];
        for (size_t i = 14; i < 22; ++i)
        {
            groupNames.insert(lower(row[i]));
        }
    }
    std::ostringstream descriptors;
    std::ostringstream names;
    descriptors << "#\tInternalName\tHeight\tRadius\tMovementSpeed\tToHitRadius\tTintColor\t"
        "AttackSoundId\tDeathSoundId\tWinceSoundId\tAwareSoundId\tSpriteStanding\tSpriteWalking\t"
        "SpriteAttackMelee\tSpriteAttackRanged\tSpriteGotHit\tSpriteDying\tSpriteDead\tSpriteBored\n";
    names << "#\tName\tPicture\n";
    for (int16_t id : actorIds)
    {
        const std::vector<std::string> &row = monsters[id - 1];
        // This presentation diagnostic uses SFT palette variants, not descriptor tint effects.
        descriptors << id << '\t' << row[13] << '\t' << integer(row[1]) << '\t' << integer(row[2])
            << '\t' << integer(row[3]) << '\t' << integer(row[4], 32767) << "\t0";
        for (size_t i = 9; i < 13; ++i)
        {
            descriptors << '\t' << integer(row[i]);
        }
        for (size_t i = 14; i < 22; ++i)
        {
            descriptors << '\t' << lower(row[i]);
        }
        descriptors << '\n';
        names << id << '\t' << row[13] << '\t' << row[13] << '\n';
    }
    const ConvertedSpriteFrames convertedFrames = convertSpriteFrameRows(frames, &groupNames);
    const std::set<int16_t> &paletteIds = convertedFrames.paletteIds;
    if (paletteIds.contains(0))
    {
        throw std::runtime_error("Actor frame has no palette");
    }
    Game::SpriteFrameTable spriteTable;
    std::string error;
    if (!spriteTable.loadFromYaml(convertedFrames.yaml, error))
    {
        throw std::runtime_error("Converted SFT failed runtime validation: " + error);
    }
    std::set<std::string> textures;
    for (const std::string &name : groupNames)
    {
        const uint16_t id = *spriteTable.findFrameIndexBySpriteName(name);
        for (const std::string &texture : spriteTable.collectTextureNames(id))
        {
            textures.insert(texture);
        }
    }
    const std::filesystem::path data = source / "Data";
    const std::vector<ConvertedLodImage> images = convertLodSprites(
        {data / "mm6.sprites.lod", data / "mm7.sprites.lod", data / "mmmerge.sprites.lod"},
        std::vector<std::string>(textures.begin(), textures.end()), destination / "worlds/mm6/sprites");
    convertLodPalettes({data / "mm6.bitmaps.lod", data / "mm7.bitmaps.lod", data / "mm8.bitmaps.lod",
        data / "mmmerge.bitmaps.lod", data / "00 patch.bitmaps.lod"},
        std::vector<int16_t>(paletteIds.begin(), paletteIds.end()), destination / "worlds/mm6/textures");
    writeConvertedResource(destination / "worlds/mm6/maps/oute3.ddm", companion);
    writeText(destination / "engine/data_tables/preview_monster_descriptors.txt", descriptors.str());
    writeText(destination / "engine/data_tables/preview_monster_names.txt", names.str());
    writeText(destination / "engine/rendering/sprite_frame_data_common.yml", convertedFrames.yaml);
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"actor-presentation\",\"runtime_ready\":false,\"actors\":"
        << state->actors.size() << ",\"monster_types\":" << actorIds.size() << ",\"animation_groups\":"
        << groupNames.size() << ",\"palette_count\":" << paletteIds.size()
        << ",\"id_policy\":\"MonList zero-based row + 1 equals companion monster_info_id\","
        << "\"omitted\":[\"AI\",\"combat\",\"dialogue\",\"descriptor tint\"],\"sprites\":[";
    for (size_t i = 0; i < images.size(); ++i)
    {
        manifest << (i ? "," : "") << "{\"name\":" << std::quoted(images[i].name)
            << ",\"archive\":" << std::quoted(images[i].archive) << ",\"width\":" << images[i].width
            << ",\"height\":" << images[i].height << '}';
    }
    manifest << "]}\n";
    writeText(destination / "actor_conversion.json", manifest.str());
}
}
