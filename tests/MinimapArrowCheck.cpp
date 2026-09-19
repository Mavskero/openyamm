#include "engine/AssetFileSystem.h"
#include "engine/ImageAssetLoader.h"
#include "game/ui/GameplayUiRuntime.h"

#include <iostream>
#include <stdexcept>
#include <array>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    try
    {
        require(argc == 3, "Usage: minimap_arrow_check RUNTIME world|original|legacy|rejected|missing");
        const std::string mode = argv[2];
        require(mode == "world" || mode == "original" || mode == "legacy" || mode == "rejected" || mode == "missing",
            "Unknown mode");
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        Engine::AssetFileSystem files;
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount test package");
        Game::GameplayUiRuntime ui;
        for (const std::string world : {"mm6", "mm7", "mm6"})
        {
            require(files.switchActiveWorld(world), "Cannot switch test world");
            ui.bindAssetFileSystem(&files);
            Game::GameplayAssetLoadCache pixelsCache;
            const std::string streamPath = "worlds/" + world + "/namespace.bin";
            if (files.exists(streamPath))
            {
                const auto activeBytes = files.readBinaryFile("namespace.bin");
                const auto qualifiedBytes = files.readBinaryFile(streamPath);
                const std::string otherWorld = world == "mm6" ? "mm7" : "mm6";
                const auto otherBytes = files.readBinaryFile("worlds/" + otherWorld + "/namespace.bin");
                require(activeBytes && qualifiedBytes && otherBytes && *activeBytes == *qualifiedBytes
                    && otherBytes->front() == uint8_t(otherWorld.back() - '0'),
                    "Root precedence or an inactive world's namespace changed");
                auto first = files.openReadStream(streamPath);
                auto second = files.openReadStream(streamPath);
                require(first && second && first->length() == 65537, "Cannot open independent namespace streams");
                std::array<uint8_t, 17> bytes;
                for (const uint64_t offset : {0u, 32760u, 65520u, 12u})
                {
                    require(first->seek(offset) && first->read(bytes.data(), bytes.size()) == int64_t(bytes.size()),
                        "Compressed namespace seek/read failed");
                    for (size_t index = 0; index < bytes.size(); ++index)
                    {
                        require(bytes[index] == uint8_t((offset + index) * 37 + int(world.back() - '0')),
                            "Namespace stream returned another world's bytes");
                    }
                    require(second->tell() == 0, "Independent streams share a file position");
                }
                require(first->seek(65537) && first->read(bytes.data(), 1) == 0, "Namespace EOF handling changed");
            }
            for (size_t index = 0; index < 8; ++index)
            {
                const std::optional<std::string> texture = ui.minimapArrowTextureName(index);
                if (mode == "rejected")
                {
                    require(!texture, "Malformed world binding fell back to an image");
                    continue;
                }
                const std::string source = world.substr(2) + "mapdir" + std::to_string(index + 1) + ".bmp";
                const std::string expected = mode == "legacy" ? "MAPDIR" + std::to_string(index + 1)
                    : "worlds/" + world + "/icons/" + source;
                require(texture == expected, "Direction binding or cache retained the wrong world");
                if (mode == "missing")
                {
                    require(files.exists("Data/icons/" + source), "Missing-image fixture has no competing image");
                    require(!files.exists(*texture), "Missing image resolved from a different content owner");
                }
                else if (mode == "world" || mode == "original")
                {
                    require(files.exists(*texture), "Declared image could not resolve within its world");
                }
                if (mode == "original")
                {
                    const auto bytes = files.readBinaryFile(*texture);
                    require(bytes.has_value(), "Original arrow bytes unavailable");
                    const auto original = Engine::decodeImagePixelsBgra(*bytes, *texture);
                    int width = 0;
                    int height = 0;
                    const auto pixels = Game::GameplayHudCommon::loadHudBitmapPixelsBgraCached(
                        &files, pixelsCache, *texture, width, height);
                    require(original && pixels && width == 7 && height == 7, "Original arrow did not decode");
                    size_t transparent = 0;
                    for (size_t offset = 0; offset < pixels->size(); offset += 4)
                    {
                        const bool black = original->pixels[offset] == 0 && original->pixels[offset + 1] == 0
                            && original->pixels[offset + 2] == 0;
                        require((*pixels)[offset + 3] == (black ? 0 : 255), "Original arrow transparency was lost");
                        transparent += black ? 1 : 0;
                        for (size_t channel = 0; channel < 3; ++channel)
                        {
                            require((*pixels)[offset + channel] == original->pixels[offset + channel],
                                "Original arrow RGB colors changed");
                        }
                    }
                    require(transparent > 0 && transparent < 49, "Arrow has no visible pixels or no transparent edge");
                }
            }
            require(!ui.minimapArrowTextureName(8), "Out-of-range direction accepted");
        }
        ui.clear();
        require(!ui.minimapArrowTextureName(0), "Cleared runtime retained arrow bindings");
        std::cout << "Eight direction bindings, mm6/mm7/mm6 cache changes and " << mode << " checks passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
