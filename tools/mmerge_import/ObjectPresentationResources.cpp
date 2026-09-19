#include "ObjectPresentationResources.h"
#include "LodResources.h"
#include "SpriteFrameResources.h"
#include "engine/TextTable.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/SpriteTables.h"

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
using Path = std::filesystem::path;
using Rows = std::vector<std::vector<std::string>>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool outside(const Path &path, const Path &root)
{
    const Path relative = path.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() && *relative.begin() == "..";
}

std::string lower(std::string value)
{
    for (char &letter : value)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return value;
}

Rows readRows(const Path &path, const Path &root)
{
    const Path input = std::filesystem::canonical(path);
    require(!outside(input, root) && std::filesystem::is_regular_file(input)
        && std::filesystem::file_size(input) <= 16 * 1024 * 1024, "Invalid object presentation table path");
    std::ifstream file(input, std::ios::binary);
    require(file.good(), "Cannot read object presentation table");
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table && table->getRowCount() > 0, "Empty object presentation table");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        rows.push_back(table->getRow(index));
    }
    return rows;
}

struct Archives
{
    std::vector<Path> paths;
    std::map<std::string, std::string> owners;

    Archives(const Path &root, std::initializer_list<const char *> names)
    {
        for (const char *pName : names)
        {
            if (!std::filesystem::exists(root / "Data" / pName))
            {
                continue;
            }
            const Path path = std::filesystem::canonical(root / "Data" / pName);
            require(!outside(path, root), "Object presentation archive escapes source");
            paths.push_back(path);
            for (const std::string &member : listLodMembers(path))
            {
                // Match the explicit original-to-Merge layers already used by scene presentation.
                owners[lower(member)] = path.filename().string();
            }
        }
    }
};

void names(std::ostream &out, const std::vector<std::string> &values)
{
    out << '[';
    for (size_t index = 0; index < values.size(); ++index)
    {
        out << (index ? "," : "") << std::quoted(values[index]);
    }
    out << ']';
}
}

void convertObjectPresentationResources(const Path &source, const Path &convertedBase, const Path &destination,
    const ImportCancelCheck &cancelled)
{
    checkImportCancellation(cancelled);
    const Path root = std::filesystem::canonical(source);
    const Path base = std::filesystem::canonical(convertedBase);
    const Path output = std::filesystem::weakly_canonical(destination);
    require(!std::filesystem::exists(output) && outside(output, root) && outside(output, base),
        "Object presentation needs a new destination outside both inputs");
    Game::ObjectTable objects;
    require(objects.loadRows(readRows(base / "engine/data_tables/object_list.txt", base))
        && !objects.entries().empty() && objects.entries().size() <= 65536,
        "Cannot read global object descriptors");
    Rows rows = readRows(root / "Data/Tables/SFT.txt", root);
    require(rows.front().size() == 24 && rows.front()[1] == "GroupName", "Unknown SFT schema");
    rows.erase(rows.begin());
    const ConvertedSpriteFrames converted = convertSpriteFrameRows(rows);
    Game::SpriteFrameTable frames;
    std::string error;
    require(frames.loadFromYaml(converted.yaml, error), "Cannot read validated SFT: " + error);
    std::set<uint16_t> groupIds;
    std::vector<uint16_t> sentinelObjects;
    for (size_t index = 0; index < objects.entries().size(); ++index)
    {
        const Game::ObjectEntry &object = objects.entries()[index];
        const Game::SpriteFrameEntry *pFrame = frames.getFrame(object.spriteId, 0);
        require(object.spriteId < rows.size() && rows[object.spriteId][5] == "x" && pFrame,
            "Object descriptor refers to an inconsistent SFT group: " + std::to_string(index));
        if (object.spriteId == 0 && pFrame->spriteName == "null" && pFrame->scale == 0.0f
            && pFrame->paletteId == 0 && object.spriteName.empty())
        {
            sentinelObjects.push_back(uint16_t(index));
            continue;
        }
        require(lower(object.spriteName) == lower(rows[object.spriteId][1]),
            "Object descriptor SFT name differs from its index: " + std::to_string(index));
        groupIds.insert(object.spriteId);
    }
    std::set<std::string> textures;
    std::set<int16_t> palettes;
    for (uint16_t id : groupIds)
    {
        const std::vector<std::string> groupTextures = frames.collectTextureNames(id);
        textures.insert(groupTextures.begin(), groupTextures.end());
        for (size_t index = id; index < rows.size() && (index == id || rows[index][5] != "x"); ++index)
        {
            // All numbers were validated by convertSpriteFrameRows, including frames after time zero.
            palettes.insert(int16_t(std::stoi(rows[index][22])));
        }
    }
    const Archives sprites(root, {"sprites.lod", "mm6.sprites.lod", "mm7.sprites.lod", "mmmerge.sprites.lod"});
    const Archives bitmaps(root, {"bitmaps.lod", "mm6.bitmaps.lod", "mm7.bitmaps.lod", "mm8.bitmaps.lod",
        "mmmerge.bitmaps.lod", "00 patch.bitmaps.lod"});
    std::vector<std::string> presentTextures;
    std::vector<std::string> missingTextures;
    std::vector<int16_t> presentPalettes;
    std::vector<std::string> missingPalettes;
    for (const std::string &name : textures)
    {
        (sprites.owners.contains(name) ? presentTextures : missingTextures).push_back(name);
    }
    for (int16_t id : palettes)
    {
        std::ostringstream name;
        name << "pal" << std::setw(3) << std::setfill('0') << id;
        if (bitmaps.owners.contains(name.str()))
        {
            presentPalettes.push_back(id);
        }
        else
        {
            missingPalettes.push_back(name.str());
        }
    }
    require(std::filesystem::create_directories(output), "Cannot create object presentation output");
    try
    {
        checkImportCancellation(cancelled);
        const std::vector<ConvertedLodImage> images =
            convertLodSprites(sprites.paths, presentTextures, output / "engine/sprites");
        checkImportCancellation(cancelled);
        convertLodPalettes(bitmaps.paths, presentPalettes, output / "engine/textures");
        std::ostringstream report;
        report << "{\"schema\":1,\"stage\":\"global-object-presentation\",\"runtime_ready\":false,"
            "\"object_descriptors\":" << objects.entries().size() << ",\"sprite_groups\":" << groupIds.size()
            << ",\"sentinel_descriptors\":[";
        for (size_t index = 0; index < sentinelObjects.size(); ++index)
        {
            report << (index ? "," : "") << sentinelObjects[index];
        }
        report << "],\"sprite_group_ids\":[";
        size_t groupIndex = 0;
        for (uint16_t id : groupIds)
        {
            report << (groupIndex++ ? "," : "") << id;
        }
        report << "],\"images\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            checkImportCancellation(cancelled);
            const ConvertedLodImage &image = images[index];
            report << (index ? "," : "") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << '}';
        }
        report << "],\"palettes\":[";
        for (size_t index = 0; index < presentPalettes.size(); ++index)
        {
            std::ostringstream name;
            name << "pal" << std::setw(3) << std::setfill('0') << presentPalettes[index];
            report << (index ? "," : "") << "{\"name\":" << std::quoted(name.str() + ".act")
                << ",\"archive\":" << std::quoted(bitmaps.owners.at(name.str())) << '}';
        }
        report << "],\"missing_sprites\":";
        names(report, missingTextures);
        report << ",\"missing_palettes\":";
        names(report, missingPalettes);
        report << "}\n";
        const std::string text = report.str();
        writeConvertedResource(output / "object_presentation_conversion.json", {text.begin(), text.end()});
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove_all(output, ignored);
        throw;
    }
}
}
