#include "AppleMenuPreview.h"
#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameInputSystem.h"
#include "game/ui/screens/MainMenuScreen.h"

#include <fstream>
#include <iostream>
#include <memory>

int runAppleMenuPreview(const std::filesystem::path &assets, const std::filesystem::path &capturePath)
{
    using namespace OpenYAMM;
    Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
    config.appName = "OpenYAMM resource preview";
    config.assetRoot = assets.string();
    config.activeWorldId = "mm8";
    config.verticalSync = true;
    config.fpsTrace = false;
    std::unique_ptr<Game::MainMenuScreen> pMenu;
    Game::GameInputSystem input;
    const Game::GameSettings settings = Game::GameSettings::createDefault();
    uint64_t started = 0;
    unsigned frames = 0;
    bool captured = false;
    const auto quit = []()
    {
        SDL_Event event = {};
        event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&event);
    };
    Engine::EngineApplication application(config,
        [&](Engine::AssetFileSystem &fileSystem)
        {
            pMenu = std::make_unique<Game::MainMenuScreen>(fileSystem, nullptr, quit, quit, quit);
            return true;
        },
        [&]()
        {
            if (bgfx::getRendererType() != bgfx::RendererType::Metal)
            {
                return false;
            }
            pMenu->prepareForFirstFrame();
            started = SDL_GetTicks();
            return true;
        },
        [&](const SDL_Event &event)
        {
            input.handleSdlEvent(event);
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                quit();
            }
        },
        [&](int width, int height, float wheel, float delta)
        {
            input.updateFromEngineInput(width, height, wheel, settings, true, false);
            pMenu->renderFrame(width, height, input.frame(), delta);
            if (++frames == 30)
            {
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "apple-menu-preview");
            }
            const std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                Engine::BgfxContext::consumeScreenshot("apple-menu-preview");
            if (capture)
            {
                std::ofstream stream(capturePath, std::ios::binary);
                stream << "P6\n" << capture->width << ' ' << capture->height << "\n255\n";
                for (size_t i = 0; i < capture->bgraPixels.size(); i += 4)
                {
                    const char rgb[] = {char(capture->bgraPixels[i + 2]), char(capture->bgraPixels[i + 1]),
                        char(capture->bgraPixels[i])};
                    stream.write(rgb, 3);
                }
                captured = stream.good();
                std::cout << "menu_preview renderer=Metal width=" << width << " height=" << height
                    << " captured=" << captured << std::endl;
            }
            if (SDL_GetTicks() - started >= 8000)
            {
                quit();
            }
        },
        [&]()
        {
            pMenu.reset();
            Game::MenuScreenBase::shutdownSharedResources();
        });
    const int result = application.run();
    return result != 0 ? result : (captured ? 0 : 4);
}
