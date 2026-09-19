#include "engine/AssetFileSystem.h"
#include "game/ui/GameplayHudCommon.h"

#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::runtime_error(pMessage);
    }
}
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    using namespace OpenYAMM::Game;
    try
    {
        require(argc == 2, "Usage: ui_texture_region_check RUNTIME");
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        Engine::AssetFileSystem files;
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount runtime");
        UiLayoutManager layouts;
        require(layouts.loadLayoutFile(files, "Data/ui/gameplay/gameplay.yml"), "Cannot load gameplay layout");
        require(layouts.textureRegions().size() == 5, "Expected five authored source regions");
        GameplayAssetLoadCache cache;
        size_t compared = 0;
        for (const auto &[name, region] : layouts.textureRegions())
        {
            int sourceWidth = 0;
            int sourceHeight = 0;
            int width = 0;
            int height = 0;
            const auto source = GameplayHudCommon::loadHudBitmapPixelsBgraCached(
                &files, cache, region.source, sourceWidth, sourceHeight);
            const auto pixels = GameplayHudCommon::loadHudBitmapPixelsBgraCached(
                &files, cache, name, width, height, GameplayHudBitmapTransparencyMode::HudColorKey, &region);
            require(source && pixels && width == region.width && height == region.height,
                "Runtime could not load the declared source region");
            require(region.pixelScaleForSource(sourceWidth * 2, sourceHeight * 2) == 2
                && region.pixelScaleForSource(sourceWidth * 2, sourceHeight * 2 + 1) == 0
                && region.pixelScaleForSource(sourceWidth - 1, sourceHeight) == 0,
                "Region accepted a different source shape or lost asset scaling");
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const size_t expected = (size_t(y + region.y) * sourceWidth + x + region.x) * 4;
                    const size_t actual = (size_t(y) * width + x) * 4;
                    for (size_t channel = 0; channel < 4; ++channel)
                    {
                        require((*source)[expected + channel] == (*pixels)[actual + channel],
                            "Region pixels or source transparency changed");
                    }
                    ++compared;
                }
            }
            std::cout << name << ": " << width << 'x' << height << " original RGBA pixels retained\n";
        }
        std::cout << "Five runtime regions, scaled source bounds and " << compared << " pixels passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
