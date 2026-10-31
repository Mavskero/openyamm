#include "EquipmentPictureResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/items/ItemEnchantRuntime.h"
#include "game/items/EquipmentPictureVariants.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
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
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : static_cast<char>(c);
    });
    return value;
}

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int32_t number(const std::string &text, int32_t minimum, int32_t maximum)
{
    int32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size() && value >= minimum && value <= maximum,
        "Invalid equipment numeric field: " + text);
    return value;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid equipment table quoting");
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

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Equipment input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 4 * 1024 * 1024, "Invalid equipment input size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read equipment dependency");
    if (!legacy)
    {
        return parse(text);
    }
    size_t glyphs = 0;
    text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
    // The source reader splits literal lines/tabs; quote characters in Notes have no CSV meaning.
    Rows rows;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
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
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<ConvertedLodImage> exportEquipmentImages(const std::filesystem::path &root,
    const std::map<std::string, std::set<uint32_t>> &references, const std::filesystem::path &output)
{
    std::map<std::string, std::filesystem::path> owners;
    const auto supersedes = [](const std::string &patch, const std::string &base)
    {
        return (patch == "mm8.icons.lod" && base == "icons.lod")
            || (patch == "mmmerge.icons.lod" && base == "mm6.icons.lod");
    };
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root / "Data"))
    {
        const std::string name = lower(entry.path().filename().string());
        if (!(name == "icons.lod" || name.ends_with(".icons.lod") || name.ends_with(".d.lod")
            || name == "icons.ui.lod" || name.ends_with(".icons.ui.lod")))
        {
            continue;
        }
        const std::filesystem::path archive = std::filesystem::canonical(entry.path());
        const std::filesystem::path relative = archive.lexically_relative(root);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
            && std::filesystem::is_regular_file(archive), "Equipment archive escapes source or is not a file");
        for (const std::string &member : listLodMembers(archive))
        {
            if (!references.contains(member))
            {
                continue;
            }
            const auto [owner, inserted] = owners.emplace(member, archive);
            if (!inserted)
            {
                const std::string previous = lower(owner->second.filename().string());
                if (supersedes(name, previous))
                {
                    owner->second = archive;
                }
                else
                {
                    require(supersedes(previous, name), "Ambiguous equipment image ownership: " + member);
                }
            }
        }
    }
    std::map<std::filesystem::path, std::vector<std::string>> byArchive;
    for (const auto &[name, archive] : owners)
    {
        byArchive[archive].push_back(name);
    }
    std::vector<ConvertedLodImage> images;
    for (const auto &[archive, names] : byArchive)
    {
        const std::vector<ConvertedLodImage> converted =
            convertLodImages({archive}, names, output / "engine/icons", LodImagePixelFormat::Indexed8);
        require(converted.size() == names.size(), "Equipment conversion omitted selected images");
        images.insert(images.end(), converted.begin(), converted.end());
    }
    std::sort(images.begin(), images.end(), [](const ConvertedLodImage &left, const ConvertedLodImage &right)
    {
        return left.name < right.name;
    });
    return images;
}
}

void convertEquipmentPictureResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
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
            "Equipment output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Equipment output already exists");
    const Rows pictures = readRows(root, "Data/Tables/Complex item pictures.txt", true);
    const Rows offsets = readRows(root, "Data/Tables/Complex item pictures offsets.txt", true);
    Row firstHeader = {"", "", "Armors", "Settings"};
    Row secondHeader = {"Id", "Item id", "Notes", ""};
    for (size_t index = 0; index < 12; ++index)
    {
        firstHeader.insert(firstHeader.end(), {"t" + std::to_string(index % 6), ""});
        secondHeader.insert(secondHeader.end(), {"X", "Y"});
    }
    firstHeader.pop_back();
    require(pictures.size() > 2 && pictures[0] == firstHeader && pictures[1] == secondHeader,
        "Unsupported equipment picture headers");
    require(offsets.size() > 1 && offsets[0] == Row{"Portrait id", "Item id", "X", "Y"},
        "Unsupported equipment offset header");
    const auto assetRows = [&assets](const std::string &name)
    {
        return readRows(assets, "engine/data_tables/" + name, false);
    };
    Engine::AssetFileSystem fileSystem;
    ItemTable items;
    CharacterDollTable dolls;
    require(items.load(fileSystem, assetRows("items.txt"), {})
        && dolls.loadCharacterRows(assetRows("character_data.txt"))
        && dolls.loadDollTypeRows(assetRows("doll_types.txt")), "Invalid equipment registry dependency");
    for (uint32_t type = 0; type < 6; ++type)
    {
        require(dolls.getDollType(type), "Equipment profile requires all six doll types");
    }
    MergedComplexItemPictureTable pictureTable;
    MergedComplexItemPictureOffsetTable offsetTable;
    require(pictureTable.loadFromRows(pictures) && offsetTable.loadFromRows(offsets),
        "Equipment typed table loading failed");
    const Row sections = {"Armors", "Helms", "Belts", "Boots", "Cloaks"};
    const Row equipTypes = {"Armor", "Helm", "Belt", "Boots", "Cloak"};
    const std::array<ItemEnchantCategory, 5> enchantCategories = {ItemEnchantCategory::Armor,
        ItemEnchantCategory::Helm, ItemEnchantCategory::Belt, ItemEnchantCategory::Boots, ItemEnchantCategory::Cloak};
    const auto coordinate = [](const Row &row, size_t column)
    {
        return column >= row.size() || row[column].empty() ? 0 : number(row[column], -32768, 32767);
    };
    size_t section = 0;
    std::array<size_t, 5> counts = {};
    std::array<size_t, 5> complexCounts = {};
    std::map<uint32_t, Row> rowsByItem;
    size_t pointChecks = 0;
    std::map<std::string, std::set<uint32_t>> imageReferences;
    for (size_t index = 2; index < pictures.size(); ++index)
    {
        const Row &row = pictures[index];
        if (row == Row{""})
        {
            continue;
        }
        require(row.size() >= 4, "Invalid equipment record width");
        if (row[0].empty())
        {
            require(section + 1 < sections.size() && counts[section] > 0 && row[1].empty()
                && row[2] == sections[section + 1], "Equipment section order changed");
            ++section;
            require(row[3] == (section == 3 ? "n/u" : "Complex"), "Equipment section flag meaning changed");
            for (size_t column = 4; column < row.size(); ++column)
            {
                require(row[column].empty() || (section == 4 && column == 16 && row[column] == "Collars"),
                    "Unknown equipment section metadata");
            }
            continue;
        }
        require(row.size() == (section == 4 ? 26 : 14) || row.size() == (section == 4 ? 28 : 16),
            "Equipment coordinates differ from the six-doll profile");
        const uint32_t id = number(row[0], 1, 65535);
        const uint32_t itemId = number(row[1], 1, 65535);
        require(id == ++counts[section] && rowsByItem.emplace(itemId, row).second,
            "Duplicate item or invalid section-local equipment ID");
        const ItemDefinition *pItem = items.get(itemId);
        const MergedComplexItemPictureEntry *pEntry = pictureTable.get(itemId);
        require(pItem && pItem->equipStat == equipTypes[section] && pEntry && pEntry->id == id
            && pEntry->notes == row[2], "Equipment category, identity or source note changed");
        require(ItemEnchantRuntime::categoryForItem(*pItem) == enchantCategories[section],
            "Equipment case normalization did not reach the enchantment category lookup");
        require(row[3].empty() || (row[3] == "x" && section != 0 && section != 3),
            "Invalid complex equipment flag");
        require(pEntry->complex == (row[3] == "x"), "Equipment complex flag was discarded");
        complexCounts[section] += pEntry->complex;
        require(pEntry->points.size() == 6 && pEntry->collarPoints.size() == (section == 4 ? 6 : 0),
            "Equipment body/collar coordinate sets changed");
        for (uint32_t type = 0; type < 6; ++type)
        {
            const CharacterDollTypeEntry &doll = *dolls.getDollType(type);
            const std::array<bool, 5> allowed = {doll.canEquipArmor, doll.canEquipHelm, doll.canEquipBelt,
                doll.canEquipBoots, doll.canEquipCloak};
            if (allowed[section])
            {
                require(!pItem->iconName.empty() && pItem->iconName.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == std::string::npos,
                    "Invalid equipment icon reference");
                for (bool holding : {false, true})
                {
                    for (EquipmentPicturePart part : {EquipmentPicturePart::Body, EquipmentPicturePart::Collar,
                        EquipmentPicturePart::CollarFront})
                    {
                        const std::string name = equipmentPictureName(pItem->iconName, pItem->equipStat,
                            type, pEntry->complex, holding, part);
                        if (!name.empty())
                        {
                            imageReferences[name].insert(itemId);
                        }
                    }
                }
            }
            for (size_t layer = 0; layer < (section == 4 ? 2 : 1); ++layer)
            {
                const size_t column = 4 + type * 2 + layer * 12;
                const std::optional<MergedPoint> point =
                    pictureTable.resolvePoint(itemId, type, offsetTable, std::nullopt, layer == 1);
                require(point && point->x == coordinate(row, column) && point->y == coordinate(row, column + 1)
                    && point->z == 0, "Equipment coordinate value or missing-zero semantics changed");
                ++pointChecks;
            }
        }
        require(!pictureTable.resolvePoint(itemId, 6, offsetTable), "Out-of-range doll type yielded coordinates");
    }
    require(section == 4 && counts.back() > 0 && pictureTable.entries().size() == rowsByItem.size(),
        "Incomplete equipment sections or discarded item records");
    std::set<std::pair<uint32_t, uint32_t>> offsetKeys;
    for (size_t index = 1; index < offsets.size(); ++index)
    {
        const Row &row = offsets[index];
        require(row.size() == 4, "Invalid equipment offset width");
        const uint32_t portrait = number(row[0], 0, 255);
        const uint32_t itemId = number(row[1], 1, 65535);
        const int32_t x = number(row[2], -32768, 32767);
        const int32_t y = number(row[3], -32768, 32767);
        require(dolls.getCharacter(portrait + 1) && rowsByItem.contains(itemId)
            && offsetKeys.emplace(portrait, itemId).second, "Invalid/duplicate equipment offset identity");
        const MergedComplexItemPictureOffsetEntry *pOffset = offsetTable.get(portrait, itemId);
        require(pOffset && pOffset->portraitId == portrait && pOffset->itemId == itemId
            && pOffset->x == x && pOffset->y == y, "Equipment offset identity/value changed");
        const Row &picture = rowsByItem.at(itemId);
        for (const auto &[characterId, doll] : dolls.characters())
        {
            for (uint32_t type = 0; type < 6; ++type)
            {
                for (size_t layer = 0; layer < (picture.size() > 16 ? 2 : 1); ++layer)
                {
                    const size_t column = 4 + type * 2 + layer * 12;
                    const Rows::const_iterator expectedOffset = std::find_if(offsets.begin() + 1, offsets.end(),
                        [characterId, itemId](const Row &candidate)
                        {
                            return candidate.size() == 4 && number(candidate[0], 0, 255) == characterId - 1
                                && number(candidate[1], 1, 65535) == itemId;
                        });
                    const int32_t expectedX = coordinate(picture, column)
                        + (expectedOffset == offsets.end() ? 0 : number((*expectedOffset)[2], -32768, 32767));
                    const int32_t expectedY = coordinate(picture, column + 1)
                        + (expectedOffset == offsets.end() ? 0 : number((*expectedOffset)[3], -32768, 32767));
                    const std::optional<MergedPoint> point =
                        pictureTable.resolvePoint(itemId, type, offsetTable, characterId - 1, layer == 1);
                    require(point && point->x == expectedX && point->y == expectedY,
                        "Equipment offset was lost or leaked into another portrait");
                    ++pointChecks;
                }
            }
        }
    }
    std::vector<uint32_t> missingItems;
    for (const ItemDefinition &item : items.entries())
    {
        if (std::any_of(equipTypes.begin(), equipTypes.end(), [&item](const std::string &type)
            { return lower(type) == lower(item.equipStat); })
            && !rowsByItem.contains(item.itemId))
        {
            missingItems.push_back(item.itemId);
        }
    }
    std::sort(missingItems.begin(), missingItems.end());
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"equipment_pictures\",\"runtime_ready\":false,\"items\":"
        << rowsByItem.size() << ",\"portrait_item_offsets\":" << offsetKeys.size()
        << ",\"point_checks\":" << pointChecks << ",\"items_by_section\":[";
    for (size_t index = 0; index < counts.size(); ++index)
    {
        report << (index == 0 ? "" : ",") << counts[index];
    }
    report << "],\"complex_flags_by_section\":[";
    for (size_t index = 0; index < counts.size(); ++index)
    {
        report << (index == 0 ? "" : ",") << complexCounts[index];
    }
    report << "],\"equipment_ids_without_source_coordinates\":[";
    for (size_t index = 0; index < missingItems.size(); ++index)
    {
        report << (index == 0 ? "" : ",") << missingItems[index];
    }
    report << "],\"typed_validation\":true,\"rendered_equipment_validated\":false"
        << ",\"collar_layers_rendered\":false,\"full_gameplay_loop\":false";
    std::map<std::string, std::string> files;
    for (const auto &[name, rows] : std::map<std::string, Rows>{{"complex_item_pictures.txt", pictures},
        {"complex_item_picture_offsets.txt", offsets}})
    {
        const std::string text = serializeTextRows(rows);
        require(parse(text) == rows, "Equipment serialization changed original cells");
        files.emplace("engine/data_tables/" + name, text);
    }
    require(std::filesystem::create_directory(output), "Cannot create fresh equipment output");
    try
    {
        for (const auto &[name, text] : files)
        {
            writeConvertedResource(output / name, std::vector<uint8_t>(text.begin(), text.end()));
        }
        const std::vector<ConvertedLodImage> images = exportEquipmentImages(root, imageReferences, output);
        std::set<std::string> missing;
        for (const auto &[name, ids] : imageReferences)
        {
            missing.insert(name);
        }
        report << ",\"image_references\":" << imageReferences.size()
            << ",\"images_exported\":" << images.size() << ",\"images\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            const ConvertedLodImage &image = images[index];
            const std::string name = std::filesystem::path(image.name).stem().string();
            missing.erase(name);
            report << (index ? "," : "") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << ",\"item_ids\":[";
            size_t itemIndex = 0;
            for (uint32_t id : imageReferences.at(name))
            {
                report << (itemIndex++ ? "," : "") << id;
            }
            report << "]}";
        }
        report << "],\"missing_images\":[";
        size_t missingIndex = 0;
        for (const std::string &name : missing)
        {
            report << (missingIndex++ ? "," : "") << std::quoted(name);
        }
        report << "]}\n";
        const std::string text = report.str();
        writeConvertedResource(output / "equipment_picture_conversion.json",
            std::vector<uint8_t>(text.begin(), text.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
