#include "mmerge_import/LodResources.h"
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        std::cerr << "Usage: sprite_import SOURCE_DIRECTORY SPRITE_NAME PALETTE_ID NEW_DESTINATION\n";
        return 2;
    }
    std::filesystem::path destination;
    bool ownsOutput = false;
    try
    {
        const std::filesystem::path source = std::filesystem::canonical(argv[1]);
        destination = std::filesystem::weakly_canonical(argv[4]);
        const std::filesystem::path relative = destination.lexically_relative(source);
        if (relative.empty() || *relative.begin() != ".." || std::filesystem::exists(destination))
        {
            throw std::runtime_error("Output must be new and outside the source directory");
        }
        int palette = 0;
        const std::string_view number = argv[3];
        const std::from_chars_result parsed = std::from_chars(number.data(), number.data() + number.size(), palette);
        if (parsed.ec != std::errc() || parsed.ptr != number.data() + number.size() || palette < 1 || palette > 32767)
        {
            throw std::runtime_error("Invalid palette ID");
        }
        ownsOutput = std::filesystem::create_directories(destination);
        if (!ownsOutput)
        {
            throw std::runtime_error("Output was created by another operation");
        }
        OpenYAMM::Import::convertLodSprites({source / "sprites.lod"}, {argv[2]}, destination);
        OpenYAMM::Import::convertLodPalettes({source / "bitmaps.lod"}, {int16_t(palette)}, destination);
        return 0;
    }
    catch (const std::exception &error)
    {
        if (ownsOutput)
        {
            std::filesystem::remove_all(destination);
        }
        std::cerr << error.what() << '\n';
        return 1;
    }
}
