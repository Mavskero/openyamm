#include "MenuResources.h"
#include "LodResources.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
void convertMenuResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path sourceRoot = std::filesystem::canonical(source);
    const std::filesystem::path outputRoot = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = outputRoot.lexically_relative(sourceRoot);
    if (relative.empty() || (!relative.is_absolute() && *relative.begin() != ".."))
    {
        throw std::runtime_error("Conversion output must be outside the original source");
    }
    if (std::filesystem::exists(destination))
    {
        throw std::runtime_error("Conversion destination already exists");
    }
    std::vector<std::filesystem::path> archives = {source / "Data/mm8.D.lod", source / "Data/mmmerge.D.lod"};
    const std::filesystem::path chinese = source / "Data/10 LocZHCN.EnglishD.lod";
    if (std::filesystem::exists(chinese))
    {
        archives.push_back(chinese);
    }
    const std::array<std::string, 13> names = {"title.pcx", "t_new_up", "t_new_ht", "t_new_dn",
        "t_load_up", "t_load_ht", "t_load_dn", "t_cred_up", "t_cred_ht", "t_cred_dn",
        "t_quit_up", "t_quit_ht", "t_quit_dn"};
    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"menu-preview\",\"runtime_ready\":false,\"assets\":[\n";
    std::ostringstream layout;
    if (!std::filesystem::create_directories(destination))
    {
        throw std::runtime_error("Conversion destination was created by another operation");
    }
    try
    {
        const std::vector<ConvertedLodImage> images = convertLodImages(archives,
            std::vector<std::string>(names.begin(), names.end()), destination / "engine/icons");
        for (size_t i = 0; i < images.size(); ++i)
        {
            const ConvertedLodImage &converted = images[i];
            const std::string &fileName = converted.name;
            manifest << (i == 0 ? "" : ",\n") << "{\"name\":" << std::quoted(fileName)
                << ",\"archive\":" << std::quoted(converted.archive)
                << ",\"width\":" << converted.width << ",\"height\":" << converted.height << '}';
            if (i == 0)
            {
                layout << "screen: MainMenu\nelements:\n  - id: MainMenuRoot\n    anchor_space: screen\n"
                    << "    anchor: center\n    width: " << converted.width << "\n    height: " << converted.height
                    << "\n    children:\n      - id: MainMenuBackground\n        anchor: top_left\n"
                    << "        width: " << converted.width << "\n        height: " << converted.height
                    << "\n        asset:\n          default: title\n";
            }
        }
        const std::array<std::string, 4> ids = {"NewGame", "LoadGame", "Credits", "Quit"};
        const std::array<std::string, 4> prefixes = {"t_new", "t_load", "t_cred", "t_quit"};
        for (size_t i = 0; i < ids.size(); ++i)
        {
            layout << "      - id: MainMenu" << ids[i] << "Button\n        anchor: top_right\n"
                << "        offset_x: -20\n        offset_y: " << 123 + 38 * i
                << "\n        width: 106\n        height: 32\n        z_index: 20\n        interactive: true\n"
                << "        asset:\n          default: " << prefixes[i] << "_up\n          highlighted: "
                << prefixes[i] << "_ht\n          pressed: " << prefixes[i] << "_dn\n";
        }
        manifest << "\n]}\n";
        const std::string layoutText = layout.str();
        const std::string manifestText = manifest.str();
        writeConvertedResource(destination / "engine/ui/menu/main_menu.yml",
            std::vector<uint8_t>(layoutText.begin(), layoutText.end()));
        writeConvertedResource(destination / "conversion.json",
            std::vector<uint8_t>(manifestText.begin(), manifestText.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(destination);
        throw;
    }
}
}
