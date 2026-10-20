#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/ui/screens/LoadingOverlayScreen.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    try
    {
        require(argc == 3, "Usage: loading_ui_capture RUNTIME NEW_OUTPUT");
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        const std::filesystem::path output = std::filesystem::absolute(argv[2]);
        require(!std::filesystem::exists(output), "Loading capture output must be new");
        std::filesystem::create_directories(output);
        std::filesystem::current_path(output);
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM loading UI validation";
        config.assetRoot = root.string();
        config.activeWorldId = "mm6";
        config.windowWidth = 1210;
        config.windowHeight = 834;
        config.verticalSync = true;
        config.fpsTrace = false;
        std::unique_ptr<Game::LoadingOverlayScreen> pScreen;
        struct Stage
        {
            std::string texture;
            int percent;
            bool dungeon;
        };
        const std::array<Stage, 9> stages = {{
            {"loading1", 65, false}, {"loading2", 65, false}, {"loading3", 65, false},
            {"loading4", 65, false}, {"loading5", 65, false}, {"loading1", 0, false},
            {"loading1", 100, false}, {"bardata", 25, true}, {"bardata", 100, true}}};
        size_t stage = 0;
        unsigned frames = 0;
        uint64_t started = 0;
        bool pending = false;
        bool complete = false;
        std::string token;
        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                pScreen = std::make_unique<Game::LoadingOverlayScreen>(files);
                return true;
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal, "Actual Metal is required");
                started = SDL_GetTicks();
                return true;
            },
            [](const SDL_Event &) {},
            [&](int width, int height, float, float delta)
            {
                if (complete) return;
                require(SDL_GetTicks() - started < 30000, "Loading capture exceeded finite guard");
                const Stage &current = stages[stage];
                pScreen->setPresentation(current.dungeon
                    ? Game::LoadingOverlayScreen::Presentation::DungeonTransition
                    : Game::LoadingOverlayScreen::Presentation::Fullscreen);
                pScreen->setBackgroundTextureName(current.texture);
                pScreen->setProgressPercent(current.percent);
                // A neutral backdrop makes the controlled dungeon overlay's full extent visible.
                if (current.dungeon)
                {
                    bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x202020ff);
                    bgfx::touch(0);
                }
                pScreen->renderFrame(width, height, {}, delta);
                if (++frames == 30)
                {
                    token = current.texture + "-" + std::to_string(current.percent);
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, token.c_str());
                    pending = true;
                }
                if (!pending) return;
                const auto capture = Engine::BgfxContext::consumeScreenshot(token);
                if (!capture) return;
                SDL_Surface *pSurface = SDL_CreateSurfaceFrom(int(capture->width), int(capture->height),
                    SDL_PIXELFORMAT_BGRA32, const_cast<uint8_t *>(capture->bgraPixels.data()), int(capture->width * 4));
                require(pSurface != nullptr, "Cannot construct screenshot surface");
                const bool saved = SDL_SaveBMP(pSurface, (output / (token + ".bmp")).string().c_str());
                SDL_DestroySurface(pSurface);
                require(saved, "Cannot save loading screenshot");
                std::cout << "Actual loading UI capture: " << token << std::endl;
                pending = false;
                frames = 0;
                if (++stage == stages.size())
                {
                    complete = true;
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                pScreen.reset();
                Game::MenuScreenBase::shutdownSharedResources();
            });
        require(application.run() == 0 && complete, "Loading component capture did not complete normally");
        std::ofstream(output / "result.json")
            << "{\"status\":\"passed\",\"renderer\":\"Metal\",\"captures\":9,"
            << "\"scope\":\"Controlled production loading component; actual map transitions checked separately\"}\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
