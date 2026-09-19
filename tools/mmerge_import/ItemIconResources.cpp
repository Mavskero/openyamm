#include "ItemIconResources.h"
#include "LodResources.h"
#include "game/StringUtils.h"
#include "game/tables/ItemTable.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool isIconArchive(const std::string &name)
{
    return name == "icons.lod" || name.ends_with(".icons.lod") || name.ends_with(".d.lod")
        || name.ends_with("englishd.lod") || name == "icons.ui.lod" || name.ends_with(".icons.ui.lod");
}
}

void convertItemIconResources(const std::filesystem::path &source, const Game::ItemTable &items,
    const std::filesystem::path &destination)
{
    std::map<std::string, std::vector<uint32_t>> references;
    std::vector<uint32_t> blankIconItems;
    for (const Game::ItemDefinition &item : items.entries())
    {
        if (item.iconName.empty())
        {
            blankIconItems.push_back(item.itemId);
            continue;
        }
        const std::string name = Game::toLowerCopy(item.iconName);
        require(name != "." && name != ".." && name.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyz0123456789_.-") == std::string::npos,
            "Invalid item icon member name: " + name);
        references[name].push_back(item.itemId);
    }
    // These gold heap images are selected by the shared chest renderer rather than a loot item ID.
    for (const char *pName : {"item204", "item205", "item206"})
    {
        references.try_emplace(pName);
    }

    std::map<std::string, std::filesystem::path> owners;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(source / "Data"))
    {
        if (!isIconArchive(Game::toLowerCopy(entry.path().filename().string())))
        {
            continue;
        }
        const std::filesystem::path archive = std::filesystem::canonical(entry.path());
        const std::filesystem::path relative = archive.lexically_relative(source);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
            && std::filesystem::is_regular_file(archive), "Item icon archive escapes source or is not a file");
        for (const std::string &name : listLodMembers(archive))
        {
            if (references.contains(name))
            {
                // The supplied merged item registry has unique image owners across all base/patch archives.
                // Reject unknown collisions instead of borrowing another world's same-named item art.
                require(owners.emplace(name, archive).second, "Ambiguous item icon ownership: " + name);
            }
        }
    }

    std::map<std::filesystem::path, std::vector<std::string>> archiveImages;
    for (const auto &[name, archive] : owners)
    {
        archiveImages[archive].push_back(name);
    }
    std::map<std::string, ConvertedLodImage> images;
    for (const auto &[archive, names] : archiveImages)
    {
        const std::vector<ConvertedLodImage> converted =
            convertLodImages({archive}, names, destination / "engine/icons", LodImagePixelFormat::Indexed8);
        require(converted.size() == names.size(), "Item icon conversion omitted an archive member");
        for (size_t i = 0; i < names.size(); ++i)
        {
            images.emplace(names[i], converted[i]);
        }
    }
    require(images.size() == owners.size(), "Item icon conversion omitted a selected image");

    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"shared-item-icons\",\"runtime_ready\":false,"
        "\"visual_gameplay_validated\":false,\"requested_images\":" << references.size()
        << ",\"imported_images\":" << images.size() << ",\"references\":[";
    size_t index = 0;
    for (const auto &[name, ids] : references)
    {
        report << (index++ ? "," : "") << "{\"name\":" << std::quoted(name) << ",\"item_ids\":[";
        for (size_t i = 0; i < ids.size(); ++i)
        {
            report << (i ? "," : "") << ids[i];
        }
        report << ']';
        const auto image = images.find(name);
        if (image != images.end())
        {
            report << ",\"path\":" << std::quoted("engine/icons/" + image->second.name)
                << ",\"archive\":" << std::quoted(owners.at(name).lexically_relative(source).generic_string())
                << ",\"width\":" << image->second.width << ",\"height\":" << image->second.height;
        }
        report << '}';
    }
    report << "],\"missing_images\":[";
    index = 0;
    for (const auto &[name, ids] : references)
    {
        if (!owners.contains(name))
        {
            report << (index++ ? "," : "") << std::quoted(name);
        }
    }
    report << "],\"blank_icon_item_ids\":[";
    for (size_t i = 0; i < blankIconItems.size(); ++i)
    {
        report << (i ? "," : "") << blankIconItems[i];
    }
    report << "]}\n";
    const std::string text = report.str();
    writeConvertedResource(destination / "item_icon_conversion.json", std::vector<uint8_t>(text.begin(), text.end()));
}
}
