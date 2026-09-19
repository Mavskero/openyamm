#pragma once

#include <bgfx/bgfx.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct SDL_Window;

namespace OpenYAMM::Engine
{
class BgfxContext
{
public:
    struct ScreenshotCapture
    {
        std::string token;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> bgraPixels;
    };

    BgfxContext();
    ~BgfxContext();

    BgfxContext(const BgfxContext &) = delete;
    BgfxContext &operator=(const BgfxContext &) = delete;

    bool initialize(SDL_Window *pWindow, int windowWidth, int windowHeight, bool verticalSync);
    void resize(SDL_Window *pWindow, int windowWidth, int windowHeight) const;
    void shutdown();

    bool isInitialized() const;
    bgfx::RendererType::Enum getRendererType() const;
    static bool isBgfxInitialized();
    static std::optional<ScreenshotCapture> consumeScreenshot(const std::string &token);

private:
    class Callback;

    static bgfx::PlatformData resolvePlatformData(SDL_Window *pWindow);

    bool m_isInitialized;
    bool m_verticalSync;
    bgfx::RendererType::Enum m_rendererType;
    std::unique_ptr<Callback> m_pCallback;
    void *m_pMetalView = nullptr;
};
}
