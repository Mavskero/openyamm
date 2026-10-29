#include "UiResources.h"
#include "LodResources.h"
#include "engine/ImageAssetLoader.h"
#include "engine/TextTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/ui/UiLayoutManager.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace OpenYAMM::Import
{
namespace
{
using Bytes = std::vector<uint8_t>;
using Owners = std::map<std::string, std::filesystem::path>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
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

bool safeName(const std::string &value)
{
    return !value.empty() && value != "." && value != ".."
        && value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.+ ")
            == std::string::npos;
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relative);
    const std::filesystem::path resolved = path.lexically_relative(root);
    require(!resolved.empty() && !resolved.is_absolute() && *resolved.begin() != "..", "UI input escapes root");
    return path;
}

Bytes read(const std::filesystem::path &path)
{
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 32 * 1024 * 1024, "Invalid UI file size");
    Bytes bytes(size);
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read UI file");
    return bytes;
}

void addArchive(Owners &owners, const std::filesystem::path &root, const std::string &relative, bool required = false)
{
    if (!required && !std::filesystem::exists(root / relative))
    {
        return;
    }
    const std::filesystem::path path = checkedPath(root, relative);
    for (const std::string &name : listLodMembers(path))
    {
        owners[lower(name)] = path;
    }
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, Bytes(text.begin(), text.end()));
}

std::set<std::string> dimensionDoorImages(const std::filesystem::path &root)
{
    const Bytes bytes = read(checkedPath(root, "Data/Tables/TownPortalSwitch.txt"));
    const std::optional<Engine::TextTable> text =
        Engine::TextTable::parseTabSeparated(std::string(bytes.begin(), bytes.end()));
    require(text.has_value(), "Invalid TownPortalSwitch table");

    std::vector<std::vector<std::string>> rows;
    for (size_t index = 0; index < text->getRowCount(); ++index)
    {
        rows.push_back(text->getRow(index));
    }

    Game::MergedTownPortalSwitchTable table;
    require(table.loadFromRows(rows), "Invalid TownPortalSwitch table");
    const Game::MergedTownPortalSwitchGroup *pGlobal = nullptr;
    for (const Game::MergedTownPortalSwitchGroup &group : table.groups())
    {
        if (lower(group.name) == "tpglobal")
        {
            require(pGlobal == nullptr, "Duplicate TPGlobal group");
            pGlobal = &group;
        }
    }
    require(pGlobal != nullptr && pGlobal->topicId == 309 && pGlobal->destinations.size() == 6,
        "Unsupported TPGlobal group");

    const std::vector<std::string> expectedIcons = {
        "tpjadam1", "none", "tpantag1", "none", "tpenroth1", "none"};
    const std::vector<std::string> expectedDescriptions = {
        "jadame", "", "antagarich", "", "enroth", ""};
    const std::vector<std::pair<int32_t, int32_t>> expectedIconPositions = {
        {0, 46}, {0, 0}, {180, 46}, {0, 0}, {180, 0}, {0, 0}};
    for (size_t index = 0; index < pGlobal->destinations.size(); ++index)
    {
        const Game::MergedTownPortalDestination &destination = pGlobal->destinations[index];
        require(destination.id == index
                && destination.mapId == 0
                && destination.x == 0
                && destination.y == 0
                && destination.z == 0
                && destination.direction == 0
                && destination.lookAngle == 0
                && destination.qbitIndex == 309
                && destination.iconX == expectedIconPositions[index].first
                && destination.iconY == expectedIconPositions[index].second
                && !destination.iconWidth.has_value()
                && !destination.iconHeight.has_value()
                && lower(destination.iconName) == expectedIcons[index]
                && (expectedDescriptions[index].empty()
                    || lower(destination.description) == expectedDescriptions[index]),
            "Unsupported TPGlobal destination profile");
    }

    std::set<std::string> images = {lower(pGlobal->name)};
    const std::vector<std::pair<std::string, size_t>> families = {
        {"tpjadam", 8}, {"tpantag", 8}, {"tpenroth", 5}};
    for (const std::pair<std::string, size_t> &family : families)
    {
        for (size_t index = 1; index <= family.second; ++index)
        {
            images.insert(family.first + std::to_string(index));
        }
    }
    return images;
}
}

void convertUiResources(const std::filesystem::path &source, const std::filesystem::path &layoutDirectory,
    uint32_t uiSetId, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path layoutsRoot = std::filesystem::canonical(layoutDirectory);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, layoutsRoot})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "UI output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "UI output already exists");
    const Bytes uiBytes = read(checkedPath(root, "Data/Tables/Additional UI.txt"));
    const std::optional<Engine::TextTable> uiText =
        Engine::TextTable::parseTabSeparated(std::string(uiBytes.begin(), uiBytes.end()));
    require(uiText.has_value(), "Invalid Additional UI table");
    std::vector<std::vector<std::string>> rows;
    for (size_t index = 0; index < uiText->getRowCount(); ++index)
    {
        rows.push_back(uiText->getRow(index));
    }
    Game::MergedAdditionalUiTable uiTable;
    require(uiTable.loadFromRows(rows) && uiSetId > 0 && uiSetId <= uiTable.entries().size(),
        "Unknown UI set");
    const Game::MergedAdditionalUiEntry &ui = uiTable.entries()[uiSetId - 1];
    require(ui.id == uiSetId && safeName(ui.lodName) && (ui.dLodName.empty() || safeName(ui.dLodName)),
        "UI set order or archive name changed");

    std::map<std::string, Bytes> layouts;
    std::map<std::string, Game::UiLayoutManager::TextureRegion> textureRegions;
    std::set<std::string> images;
    std::set<std::string> fonts;
    size_t elementCount = 0;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(layoutsRoot))
    {
        if (entry.path().extension() != ".yml")
        {
            continue;
        }
        const std::string name = entry.path().filename().string();
        require(safeName(name) && lower(name) == name, "Unsafe gameplay layout name");
        const Bytes bytes = read(checkedPath(layoutsRoot, name));
        Game::UiLayoutManager layout;
        require(layout.loadLayoutText(name, std::string(bytes.begin(), bytes.end())) && !layout.elements().empty(),
            "Invalid gameplay layout: " + name);
        for (const auto &[regionName, region] : layout.textureRegions())
        {
            require(textureRegions.emplace(regionName, region).second, "Duplicate UI texture region: " + regionName);
            images.insert(region.source);
        }
        elementCount += layout.elements().size();
        for (const auto &[id, element] : layout.elements())
        {
            (void)id;
            for (const std::string *pName : {&element.primaryAsset, &element.hoverAsset, &element.pressedAsset,
                &element.secondaryAsset, &element.tertiaryAsset, &element.quaternaryAsset, &element.quinaryAsset})
            {
                if (!pName->empty())
                {
                    require(safeName(*pName), "Unsafe layout image reference");
                    images.insert(lower(*pName));
                }
            }
            if (!element.fontName.empty())
            {
                require(safeName(element.fontName), "Unsafe layout font reference");
                fonts.insert(lower(element.fontName) + ".fnt");
            }
        }
        layouts.emplace(name, bytes);
    }
    require(layouts.contains("gameplay.yml"), "Main gameplay layout is missing");
    // These OpenYAMM images are owned by the UI, including context/resume icons selected at runtime.
    std::map<std::string, std::filesystem::path> ownedImages;
    const std::filesystem::path ownedManifest = layoutsRoot / "owned_images.txt";
    require(!layouts.contains("gameplay_mobile.yml") || std::filesystem::exists(ownedManifest),
        "Mobile UI requires owned_images.txt");
    if (std::filesystem::exists(ownedManifest))
    {
        const Bytes manifestBytes = read(checkedPath(layoutsRoot, "owned_images.txt"));
        std::istringstream manifest(std::string(manifestBytes.begin(), manifestBytes.end()));
        const std::filesystem::path iconsRoot =
            std::filesystem::canonical(layoutsRoot.parent_path().parent_path() / "icons");
        const std::filesystem::path relativeOutput = output.lexically_relative(iconsRoot);
        require(!relativeOutput.empty() && !relativeOutput.is_absolute() && *relativeOutput.begin() == "..",
            "UI output must be outside owned images");
        std::string fileName;
        while (std::getline(manifest, fileName))
        {
            if (!fileName.empty() && fileName.back() == '\r') fileName.pop_back();
            require(safeName(fileName) && lower(fileName) == fileName && fileName.ends_with(".png"),
                "Invalid owned UI image: " + fileName);
            const std::string name = fileName.substr(0, fileName.size() - 4);
            require(ownedImages.emplace(name, checkedPath(iconsRoot, fileName)).second,
                "Duplicate owned UI image: " + name);
            images.insert(name);
        }
        require(!ownedImages.empty(), "Owned UI image manifest is empty");
    }
    for (const auto &[name, region] : textureRegions)
    {
        require(!textureRegions.contains(region.source), "UI texture regions must reference original images");
        images.erase(name);
    }
    std::set<std::string> dimensionDoorImageReferences;
    if (layouts.contains("town_portal.yml"))
    {
        dimensionDoorImageReferences = dimensionDoorImages(root);
        images.insert(dimensionDoorImageReferences.begin(), dimensionDoorImageReferences.end());
    }

    // Menu and gameplay overlays select this original side-panel image outside the YAML layouts.
    const std::set<std::string> requiredDynamicImageReferences = {"ui-parch"};
    images.insert(requiredDynamicImageReferences.begin(), requiredDynamicImageReferences.end());

    // LoadingOverlayScreen selects these original images dynamically, outside the YAML layouts.
    std::set<std::string> loadingImageReferences = {
        "loading1.pcx", "loading2.pcx", "loading3.pcx", "loading4.pcx", "loading5.pcx", "loadprog", "bardata"};
    for (unsigned frame = 1; frame <= 10; ++frame)
    {
        std::ostringstream name;
        name << "ia02-" << std::setfill('0') << std::setw(3) << frame;
        loadingImageReferences.insert(name.str());
    }
    images.insert(loadingImageReferences.begin(), loadingImageReferences.end());

    // WinGameScreen references ENDGAME directly, outside the YAML layouts.
    fonts.insert("endgame.fnt");
    // Lloyd's Beacon draws slot labels directly, outside its YAML layout.
    const std::set<std::string> requiredDynamicFontReferences = {"spell.fnt"};
    fonts.insert(requiredDynamicFontReferences.begin(), requiredDynamicFontReferences.end());

    // Separate original icon and language-image namespaces. Conditional UI packs override their own namespace.
    Owners iconOwners;
    for (const std::string &name : {"icons.lod", "mmmerge.icons.lod", "00 add.icons.lod",
        "00 patch.icons.lod", "icons.UI.lod", "z10 LocZHCN.icons.lod"})
    {
        addArchive(iconOwners, root, "Data/" + name);
    }
    Owners imageOwners;
    for (const std::string &name : {"mm8.D.lod", "mmmerge.D.lod", "10 LocZHCN.EnglishD.lod"})
    {
        addArchive(imageOwners, root, "Data/" + name);
    }
    // This import profile uses the original "UI" layout suffix, matching the base icons.UI.lod.
    if (ui.lodName != "default")
    {
        std::filesystem::path archive = "Data/Additional UI/" + ui.lodName;
        addArchive(iconOwners, root, archive.generic_string(), true);
        archive.replace_extension(".UI.lod");
        addArchive(iconOwners, root, archive.generic_string());
    }
    if (!ui.dLodName.empty())
    {
        std::filesystem::path archive = "Data/Additional UI/" + ui.dLodName;
        addArchive(imageOwners, root, archive.generic_string(), true);
        archive.replace_extension(".UI.lod");
        addArchive(imageOwners, root, archive.generic_string());
    }
    Owners fontOwners;
    addArchive(fontOwners, root, "Data/mmmerge.T.lod");
    addArchive(fontOwners, root, "Data/10 LocZHCN.EnglishT.lod");

    require(std::filesystem::create_directory(output), "Cannot create UI output");
    try
    {
        for (const auto &[name, bytes] : layouts)
        {
            writeConvertedResource(output / "engine/ui/gameplay" / name, bytes);
        }
        std::ostringstream report;
        report << "{\"schema\":1,\"stage\":\"gameplay_ui\",\"runtime_ready\":false,\"ui_set_id\":" << uiSetId
            << ",\"layouts\":" << layouts.size() << ",\"layout_elements\":" << elementCount
            << ",\"image_references\":" << images.size() << ",\"font_references\":" << fonts.size()
            << ",\"dimension_door_image_references\":" << dimensionDoorImageReferences.size()
            << ",\"loading_image_references\":" << loadingImageReferences.size()
            << ",\"owned_image_references\":" << ownedImages.size()
            << ",\"images\":[";
        bool first = true;
        std::set<std::string> missingImages;
        std::set<std::string> missingFonts;
        std::map<std::string, std::pair<int, int>> imageSizes;
        for (const std::string &name : images)
        {
            const auto owned = ownedImages.find(name);
            if (owned != ownedImages.end())
            {
                const Bytes bytes = read(owned->second);
                const std::string fileName = owned->second.filename().string();
                const auto decoded = Engine::decodeImagePixelsBgra(bytes, fileName);
                require(decoded.has_value(), "Owned UI image decoding failed: " + name);
                writeConvertedResource(output / "engine/icons" / fileName, bytes);
                imageSizes[name] = {decoded->width, decoded->height};
                report << (first ? "" : ",") << "{\"name\":" << std::quoted(fileName)
                    << ",\"owner\":\"openyamm_ui\",\"width\":" << decoded->width
                    << ",\"height\":" << decoded->height << '}';
                first = false;
                continue;
            }
            // Layouts use image stems; original LOD members can retain their PCX extension (e.g. makeme.pcx).
            const std::string sourceName = iconOwners.contains(name) || imageOwners.contains(name)
                || name.ends_with(".pcx") ? name : name + ".pcx";
            require(!(iconOwners.contains(sourceName) && imageOwners.contains(sourceName)),
                "Ambiguous icon/language image: " + sourceName);
            const Owners &owners = iconOwners.contains(sourceName) ? iconOwners : imageOwners;
            const Owners::const_iterator owner = owners.find(sourceName);
            if (owner == owners.end())
            {
                require(!requiredDynamicImageReferences.contains(name), "Missing required dynamic UI image: " + name);
                require(!loadingImageReferences.contains(name), "Missing required loading image: " + name);
                missingImages.insert(name);
                continue;
            }
            const ConvertedLodImage converted =
                convertLodImages({owner->second}, {sourceName}, output / "engine/icons")[0];
            const std::optional<Engine::ImagePixelsBgra> decoded = Engine::decodeImagePixelsBgra(
                read(output / "engine/icons" / converted.name), converted.name);
            require(decoded && decoded->width == converted.width && decoded->height == converted.height,
                "Runtime UI image decoding failed: " + name);
            imageSizes[name] = {decoded->width, decoded->height};
            report << (first ? "" : ",") << "{\"name\":" << std::quoted(converted.name)
                << ",\"archive\":" << std::quoted(owner->second.lexically_relative(root).generic_string())
                << ",\"width\":" << converted.width << ",\"height\":" << converted.height << '}';
            first = false;
        }
        report << "],\"texture_regions\":[";
        first = true;
        for (const auto &[name, region] : textureRegions)
        {
            const auto size = imageSizes.find(region.source);
            const bool available = size != imageSizes.end();
            require(!available || region.pixelScaleForSource(size->second.first, size->second.second) != 0,
                "UI region source dimensions changed: " + name);
            if (!available)
            {
                missingImages.insert(name);
            }
            report << (first ? "" : ",") << "{\"name\":" << std::quoted(name)
                << ",\"source\":" << std::quoted(region.source)
                << ",\"x\":" << region.x << ",\"y\":" << region.y
                << ",\"width\":" << region.width << ",\"height\":" << region.height
                << ",\"source_width\":" << region.sourceWidth << ",\"source_height\":" << region.sourceHeight
                << ",\"available\":" << (available ? "true" : "false") << '}';
            first = false;
        }
        report << "],\"fonts\":[";
        first = true;
        for (const std::string &name : fonts)
        {
            const Owners::const_iterator owner = fontOwners.find(name);
            if (owner == fontOwners.end())
            {
                require(!requiredDynamicFontReferences.contains(name), "Missing required dynamic UI font: " + name);
                missingFonts.insert(name);
                continue;
            }
            const Bytes bytes = readLodText(owner->second, name);
            require(!bytes.empty(), "Empty font payload");
            writeConvertedResource(output / "engine/icons" / name, bytes);
            report << (first ? "" : ",") << "{\"name\":" << std::quoted(name)
                << ",\"archive\":" << std::quoted(owner->second.lexically_relative(root).generic_string())
                << ",\"bytes\":" << bytes.size() << '}';
            first = false;
        }
        for (const auto &[name, missing] : {std::pair{"missing_images", missingImages},
            std::pair{"missing_fonts", missingFonts}})
        {
            report << "],\"" << name << "\":[";
            first = true;
            for (const std::string &entry : missing)
            {
                report << (first ? "" : ",") << std::quoted(entry);
                first = false;
            }
        }
        std::set<std::string> missingDimensionDoorImages;
        std::set_intersection(
            dimensionDoorImageReferences.begin(),
            dimensionDoorImageReferences.end(),
            missingImages.begin(),
            missingImages.end(),
            std::inserter(missingDimensionDoorImages, missingDimensionDoorImages.end()));
        report << "],\"missing_dimension_door_images\":[";
        first = true;
        for (const std::string &entry : missingDimensionDoorImages)
        {
            report << (first ? "" : ",") << std::quoted(entry);
            first = false;
        }
        report << "],\"runtime_images_decoded\":true,\"complete_hud_validated\":false,"
            << "\"font_rendering_validated\":false,\"chinese_layout_validated\":false,"
            << "\"original_ui_patch_parity\":false}\n";
        writeText(output / "ui_conversion.json", report.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
