#include "OutdoorResources.h"
#include "ActorResources.h"
#include "LodResources.h"
#include "SurfaceResources.h"
#include "engine/TextTable.h"
#include "game/outdoor/OutdoorMapData.h"

#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
std::string textureName(std::string name)
{
    for (char &character : name)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character += 'a' - 'A';
        }
        if (!(character >= 'a' && character <= 'z') && !(character >= '0' && character <= '9')
            && character != '_' && character != '-')
        {
            throw std::runtime_error("Invalid terrain texture name");
        }
    }
    return name;
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}
}

void convertOutdoorResources(const std::filesystem::path &source, const std::filesystem::path &destination,
    bool includeActors)
{
    const std::filesystem::path relative = std::filesystem::weakly_canonical(destination)
        .lexically_relative(std::filesystem::canonical(source));
    if (relative.empty() || *relative.begin() != ".." || std::filesystem::exists(destination))
    {
        throw std::runtime_error("Scene output must be new and outside original resources");
    }
    const std::vector<uint8_t> geometry = readLodMap(source / "Data/mm6.games.lod", "oute3.odm");
    const std::optional<Game::OutdoorMapData> map = Game::OutdoorMapDataLoader().loadFromBytes(geometry);
    if (!map || map->masterTile != 2)
    {
        throw std::runtime_error("Expected the MMMerge MM6 outdoor map and terrain table");
    }
    const std::filesystem::path tablePath = source / "Data/Tables/Tile3.txt";
    if (std::filesystem::file_size(tablePath) > 4 * 1024 * 1024)
    {
        throw std::runtime_error("Terrain table exceeds the import limit");
    }
    std::ifstream tableFile(tablePath, std::ios::binary);
    const std::string originalTable{std::istreambuf_iterator<char>(tableFile), std::istreambuf_iterator<char>()};
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(originalTable);
    if (!table)
    {
        throw std::runtime_error("Cannot parse terrain table");
    }
    std::vector<std::vector<std::string>> rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    const std::vector<std::vector<std::string>> convertedRows = convertTerrainRows(rows);
    std::vector<std::string> tileNames;
    std::ostringstream convertedTable;
    for (size_t index = 0; index < convertedRows.size(); ++index)
    {
        const std::vector<std::string> &row = convertedRows[index];
        for (size_t column = 0; column < row.size(); ++column)
        {
            convertedTable << (column ? "\t" : "") << row[column];
        }
        convertedTable << '\n';
        if (index != 0)
        {
            tileNames.push_back(row[1]);
        }
    }
    std::set<std::string> textures;
    // A fixed original MM6 sky for the scene diagnostic; weather is not imported.
    textures.insert("6sky05");
    for (size_t tile = 0; tile < 234; ++tile)
    {
        const size_t index = tile < 90 ? tile : map->tileSetLookupIndices[(tile - 90) / 36] + (tile - 90) % 36;
        if (index >= tileNames.size())
        {
            throw std::runtime_error("Map references a terrain row outside Tile3.txt");
        }
        if (!tileNames[index].empty() && tileNames[index] != "pending")
        {
            textures.insert(tileNames[index]);
        }
    }
    for (const Game::OutdoorBModel &model : map->bmodels)
    {
        for (const Game::OutdoorBModelFace &face : model.faces)
        {
            if (!face.textureName.empty())
            {
                textures.insert(textureName(face.textureName));
            }
        }
    }
    std::vector<std::filesystem::path> archives;
    for (const char *pName : {"mm6.bitmaps.lod", "mm7.bitmaps.lod", "mm8.bitmaps.lod",
            "mmmerge.bitmaps.lod", "00 patch.bitmaps.lod"})
    {
        archives.push_back(source / "Data" / pName);
    }
    if (!std::filesystem::create_directories(destination))
    {
        throw std::runtime_error("Scene output was created by another operation");
    }
    try
    {
        writeConvertedResource(destination / "worlds/mm6/maps/oute3.odm", geometry);
        writeText(destination / "engine/data_tables/terrain_tile_data_3.txt", convertedTable.str());
        const std::vector<ConvertedLodImage> images = convertLodImages(archives,
            std::vector<std::string>(textures.begin(), textures.end()), destination / "worlds/mm6/textures");
        std::ostringstream manifest;
        manifest << "{\"schema\":1,\"stage\":\"outdoor-preview\",\"runtime_ready\":false,"
            << "\"map\":\"mm6:oute3.odm\",\"preview_sky\":\"6sky05\","
            << "\"geometry_archive\":\"mm6.games.lod\","
            << "\"terrain_table\":\"Data/Tables/Tile3.txt\",\"terrain_rows\":" << tileNames.size()
            << ",\"buildings\":" << map->bmodels.size() << ",\"textures\":[\n";
        for (size_t i = 0; i < images.size(); ++i)
        {
            const ConvertedLodImage &image = images[i];
            manifest << (i == 0 ? "" : ",\n") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << '}';
        }
        manifest << "\n],\"actor_presentation\":" << (includeActors ? "true" : "false")
            << ",\"omitted\":[" << (includeActors ? "" : "\"actors\",")
            << "\"decoration sprites\",\"events\",\"saves\","
            << "\"audio\",\"weather simulation\"]}\n";
        if (includeActors)
        {
            convertActorResources(source, destination);
        }
        writeText(destination / "conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(destination);
        throw;
    }
}
}
