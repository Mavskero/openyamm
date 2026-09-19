#include "engine/BgfxContext.h"
#include "tools/AppleMetalSmoke.h"

#include <SDL3/SDL.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace
{
bgfx::ShaderHandle loadShader(const char *pName)
{
    std::filesystem::path shaderRoot = OPENYAMM_SMOKE_SHADER_DIR;
    if (shaderRoot.is_relative() && SDL_GetBasePath() != nullptr)
    {
        shaderRoot = std::filesystem::path(SDL_GetBasePath()) / shaderRoot;
    }
    std::ifstream file(shaderRoot / pName, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty())
    {
        return BGFX_INVALID_HANDLE;
    }
    return bgfx::createShader(bgfx::copy(bytes.data(), static_cast<uint32_t>(bytes.size())));
}

int render(SDL_Window *pWindow, const std::filesystem::path &output)
{
    OpenYAMM::Engine::BgfxContext context;
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(pWindow, &width, &height);
    if (!context.initialize(pWindow, width, height, true)
        || context.getRendererType() != bgfx::RendererType::Metal)
    {
        std::cerr << "Metal initialization failed\n";
        return 2;
    }
    struct Vertex
    {
        float x;
        float y;
        float z;
        uint32_t color;
    };
    const std::array<Vertex, 3> vertices = {{
        {-0.7f, -0.65f, 0.5f, 0xff6270ff},
        { 0.7f, -0.65f, 0.5f, 0xffcce855},
        { 0.0f,  0.7f,  0.5f, 0xffffba54}
    }};
    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true).end();
    const bgfx::ShaderHandle vertexShader = loadShader("vs_cubes.bin");
    const bgfx::ShaderHandle fragmentShader = loadShader("fs_cubes.bin");
    if (!bgfx::isValid(vertexShader) || !bgfx::isValid(fragmentShader))
    {
        if (bgfx::isValid(vertexShader))
        {
            bgfx::destroy(vertexShader);
        }
        if (bgfx::isValid(fragmentShader))
        {
            bgfx::destroy(fragmentShader);
        }
        std::cerr << "Could not load Metal shaders\n";
        return 3;
    }
    const bgfx::ProgramHandle program = bgfx::createProgram(vertexShader, fragmentShader, true);
    if (!bgfx::isValid(program))
    {
        return 3;
    }
    const bgfx::VertexBufferHandle buffer = bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(), sizeof(vertices)), layout);
    if (!bgfx::isValid(buffer))
    {
        bgfx::destroy(program);
        return 3;
    }
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    int result = 4;
    for (int frame = 0; frame < 180; ++frame)
    {
        SDL_Event event;
        bool quit = false;
        while (SDL_PollEvent(&event))
        {
            quit = quit || event.type == SDL_EVENT_QUIT;
        }
        if (quit)
        {
            break;
        }
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x172435ff, 1.0f, 0);
        bgfx::setViewTransform(0, identity, identity);
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, buffer);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(0, program);
        if (frame == 30)
        {
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "apple-metal-smoke");
        }
        bgfx::frame();
        const std::optional<OpenYAMM::Engine::BgfxContext::ScreenshotCapture> capture =
            OpenYAMM::Engine::BgfxContext::consumeScreenshot("apple-metal-smoke");
        if (capture)
        {
            std::ofstream file(output, std::ios::binary);
            file << "P6\n" << capture->width << ' ' << capture->height << "\n255\n";
            for (size_t i = 0; i < capture->bgraPixels.size(); i += 4)
            {
                const char rgb[] = {static_cast<char>(capture->bgraPixels[i + 2]),
                    static_cast<char>(capture->bgraPixels[i + 1]), static_cast<char>(capture->bgraPixels[i])};
                file.write(rgb, 3);
            }
            result = file.good() ? 0 : 5;
            std::cout << "renderer=Metal capture=" << output << " width=" << capture->width
                      << " height=" << capture->height << '\n';
            break;
        }
    }
    bgfx::destroy(buffer);
    bgfx::destroy(program);
    return result;
}
}

int runAppleMetalSmoke(const std::filesystem::path &output)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window *pWindow = SDL_CreateWindow("OpenYAMM Metal validation", 640, 480,
        SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    int result = 1;
    if (pWindow != nullptr)
    {
        result = render(pWindow, output);
        SDL_DestroyWindow(pWindow);
    }
    SDL_Quit();
    return result;
}
