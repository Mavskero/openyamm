#include "game/render/FramePresentation.h"

#include <SDL3/SDL.h>
#include <bx/math.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr bgfx::ViewId PreviewView = 251;
constexpr bgfx::ViewId PresentationView = 255;
constexpr std::array<bgfx::ViewId, 5> SceneViews = {0, 1, 2, 3, 250};
constexpr uint64_t SamplerFlags = BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
constexpr uint64_t ColorFlags = BGFX_TEXTURE_RT | SamplerFlags;
constexpr uint64_t DepthFlags = BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY;
constexpr uint64_t PreviewFlags = BGFX_TEXTURE_BLIT_DST | SamplerFlags;

struct PresentationVertex
{
    float x, y, z, u, v;
};

std::filesystem::path shaderPath(const char *name)
{
    const char *directory = nullptr;
    switch (bgfx::getRendererType())
    {
    case bgfx::RendererType::Direct3D11: directory = "dxbc"; break;
    case bgfx::RendererType::OpenGL: directory = "glsl"; break;
    case bgfx::RendererType::OpenGLES: directory = "essl"; break;
    case bgfx::RendererType::Metal: directory = "metal"; break;
    case bgfx::RendererType::Vulkan: directory = "spirv"; break;
    default: return {};
    }
    const std::filesystem::path root = OPENYAMM_BGFX_SHADER_DIR;
    const std::filesystem::path member = std::filesystem::path(directory) / (std::string(name) + ".bin");
    if (root.is_absolute())
    {
        return root / member;
    }
    if (const char *base = SDL_GetBasePath())
    {
        const std::filesystem::path executableRoot = base;
        const std::filesystem::path packaged = executableRoot / root / member;
        const std::filesystem::path buildTree = executableRoot / ".." / root / member;
        std::error_code error;
        if (std::filesystem::exists(packaged, error)) return packaged;
        error.clear();
        if (std::filesystem::exists(buildTree, error)) return buildTree;
        return packaged;
    }
    return root / member;
}

bgfx::ShaderHandle loadShader(const char *name)
{
    const std::filesystem::path path = shaderPath(name);
    if (path.empty())
    {
        return BGFX_INVALID_HANDLE;
    }
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    const std::streamsize size = stream ? stream.tellg() : std::streampos(-1);
    if (size <= 0 || size > 16 * 1024 * 1024)
    {
        std::cerr << "FramePresentation could not read shader: " << path << '\n';
        return BGFX_INVALID_HANDLE;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char *>(bytes.data()), size))
    {
        std::cerr << "FramePresentation could not read complete shader: " << path << '\n';
        return BGFX_INVALID_HANDLE;
    }
    return bgfx::createShader(bgfx::copy(bytes.data(), static_cast<uint32_t>(bytes.size())));
}

template <typename Handle>
void destroyHandle(Handle &handle)
{
    if (bgfx::isValid(handle))
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }
}

void bindSceneViews(bgfx::FrameBufferHandle target)
{
    for (bgfx::ViewId view : SceneViews)
    {
        bgfx::setViewFrameBuffer(view, target);
    }
}
}

bool FramePresentation::initialize()
{
    if (m_initialized)
    {
        return true;
    }
    m_noop = bgfx::getRendererType() == bgfx::RendererType::Noop;
    if (m_noop)
    {
        m_initialized = true;
        return true;
    }
    const bgfx::Caps *caps = bgfx::getCaps();
    if (caps == nullptr || caps->limits.maxViews <= PresentationView
        || (caps->supported & BGFX_CAPS_TEXTURE_BLIT) == 0
        || !bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::BGRA8, ColorFlags)
        || !bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, DepthFlags)
        || !bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::BGRA8, PreviewFlags))
    {
        std::cerr << "FramePresentation requires views 0..255, texture blits and neutral BGRA8/D24S8 targets\n";
        return false;
    }
    const bgfx::ShaderHandle vertex = loadShader("vs_shadowmaps_texture");
    const bgfx::ShaderHandle fragment = loadShader("fs_presentation_gamma");
    if (!bgfx::isValid(vertex) || !bgfx::isValid(fragment))
    {
        if (bgfx::isValid(vertex)) bgfx::destroy(vertex);
        if (bgfx::isValid(fragment)) bgfx::destroy(fragment);
        return false;
    }
    m_program = bgfx::createProgram(vertex, fragment, false);
    bgfx::destroy(vertex);
    bgfx::destroy(fragment);
    m_sampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    m_gamma = bgfx::createUniform("u_presentationGamma", bgfx::UniformType::Vec4);
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    const float topV = caps->originBottomLeft ? 1.0f : 0.0f;
    const float bottomV = caps->originBottomLeft ? 0.0f : 1.0f;
    const std::array<PresentationVertex, 4> vertices{{
        {0.0f, 0.0f, 0.0f, 0.0f, topV},
        {1.0f, 0.0f, 0.0f, 1.0f, topV},
        {1.0f, 1.0f, 0.0f, 1.0f, bottomV},
        {0.0f, 1.0f, 0.0f, 0.0f, bottomV},
    }};
    constexpr std::array<uint16_t, 6> indices = {0, 1, 2, 0, 2, 3};
    m_vertices = bgfx::createVertexBuffer(bgfx::copy(vertices.data(), sizeof(vertices)), layout);
    m_indices = bgfx::createIndexBuffer(bgfx::copy(indices.data(), sizeof(indices)));
    if (!bgfx::isValid(m_program) || !bgfx::isValid(m_sampler) || !bgfx::isValid(m_gamma)
        || !bgfx::isValid(m_vertices) || !bgfx::isValid(m_indices))
    {
        std::cerr << "FramePresentation could not create presentation resources\n";
        shutdown();
        return false;
    }
    m_initialized = true;
    return true;
}

bool FramePresentation::resize(int width, int height)
{
    m_previewValid = false;
    const bgfx::Caps *caps = bgfx::getCaps();
    if (width <= 0 || height <= 0 || caps == nullptr
        || uint32_t(width) > caps->limits.maxTextureSize || uint32_t(height) > caps->limits.maxTextureSize
        || width > std::numeric_limits<uint16_t>::max() || height > std::numeric_limits<uint16_t>::max())
    {
        std::cerr << "FramePresentation invalid render dimensions: " << width << 'x' << height << '\n';
        return false;
    }
    bgfx::TextureHandle color = bgfx::createTexture2D(
        uint16_t(width), uint16_t(height), false, 1, bgfx::TextureFormat::BGRA8, ColorFlags);
    bgfx::TextureHandle depth = bgfx::createTexture2D(
        uint16_t(width), uint16_t(height), false, 1, bgfx::TextureFormat::D24S8, DepthFlags);
    bgfx::TextureHandle preview = bgfx::createTexture2D(
        uint16_t(width), uint16_t(height), false, 1, bgfx::TextureFormat::BGRA8, PreviewFlags);
    bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(color) && bgfx::isValid(depth) && bgfx::isValid(preview))
    {
        std::array<bgfx::Attachment, 2> attachments{};
        attachments[0].init(color);
        attachments[1].init(depth);
        if (bgfx::isFrameBufferValid(uint8_t(attachments.size()), attachments.data()))
        {
            target = bgfx::createFrameBuffer(uint8_t(attachments.size()), attachments.data(), false);
        }
    }
    if (!bgfx::isValid(target))
    {
        destroyHandle(color);
        destroyHandle(depth);
        destroyHandle(preview);
        std::cerr << "FramePresentation could not allocate render targets: " << width << 'x' << height << '\n';
        return false;
    }
    destroyTargets();
    m_color = color;
    m_depth = depth;
    m_preview = preview;
    m_frameBuffer = target;
    m_width = width;
    m_height = height;
    bgfx::setName(m_color, "Neutral complete frame");
    bgfx::setName(m_depth, "Complete frame depth/stencil");
    bgfx::setName(m_preview, "Neutral world preview");
    return true;
}

bool FramePresentation::beginFrame(int width, int height)
{
    m_frameActive = false;
    m_previewCapturedThisFrame = false;
    if (!m_initialized)
    {
        std::cerr << "FramePresentation beginFrame called before initialization\n";
        return false;
    }
    if (m_noop)
    {
        m_width = width;
        m_height = height;
        m_frameActive = true;
        return true;
    }
    const bool newTarget = width != m_width || height != m_height || !bgfx::isValid(m_frameBuffer);
    if (newTarget && !resize(width, height))
    {
        return false;
    }
    bindSceneViews(m_frameBuffer);
    std::array<bgfx::ViewId, PresentationView + 1> order{};
    size_t next = 0;
    order[next++] = 0;
    order[next++] = 1;
    order[next++] = PreviewView;
    for (bgfx::ViewId view = 2; view <= PresentationView; ++view)
    {
        if (view != PreviewView) order[next++] = view;
    }
    bgfx::setViewOrder(0, uint16_t(order.size()), order.data());
    bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
    if (newTarget)
    {
        // A loading frame can end inside the outer application frame. Rebinding
        // its target must retain that image until the next screen/world draws.
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 0x000000ff, 1.0f, 0);
        bgfx::touch(0);
    }
    bgfx::setViewRect(PreviewView, 0, 0, uint16_t(width), uint16_t(height));
    bgfx::setViewClear(PreviewView, BGFX_CLEAR_NONE);
    bgfx::setViewFrameBuffer(PreviewView, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(PresentationView, 0, 0, uint16_t(width), uint16_t(height));
    bgfx::setViewScissor(PresentationView, 0, 0, 0, 0);
    bgfx::setViewClear(PresentationView, BGFX_CLEAR_NONE);
    bgfx::setViewFrameBuffer(PresentationView, BGFX_INVALID_HANDLE);
    bgfx::setViewMode(PresentationView, bgfx::ViewMode::Sequential);
    m_frameActive = true;
    return true;
}

void FramePresentation::captureWorldPreview()
{
    if (!m_frameActive || m_noop || m_previewCapturedThisFrame)
    {
        return;
    }
    bgfx::blit(PreviewView, m_preview, 0, 0, m_color, 0, 0, uint16_t(m_width), uint16_t(m_height));
    m_previewCapturedThisFrame = true;
    m_previewValid = true;
}

void FramePresentation::submit(float displayGamma)
{
    if (!m_frameActive)
    {
        return;
    }
    m_frameActive = false;
    if (m_noop)
    {
        return;
    }
    const bgfx::Caps *caps = bgfx::getCaps();
    float projection[16];
    bx::mtxOrtho(projection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, caps->homogeneousDepth);
    bgfx::setViewTransform(PresentationView, nullptr, projection);
    float identity[16];
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);
    bgfx::setScissor(UINT16_MAX);
    bgfx::setStencil(BGFX_STENCIL_NONE);
    bgfx::setVertexBuffer(0, m_vertices);
    bgfx::setIndexBuffer(m_indices);
    bgfx::setTexture(0, m_sampler, m_color, uint32_t(SamplerFlags));
    const float inverseGamma = std::isfinite(displayGamma) && displayGamma > 0.0f ? 1.0f / displayGamma : 1.0f;
    const std::array<float, 4> gamma = {inverseGamma, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(m_gamma, gamma.data());
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(PresentationView, m_program);
}

void FramePresentation::destroyTargets()
{
    if (bgfx::isValid(m_frameBuffer))
    {
        bindSceneViews(BGFX_INVALID_HANDLE);
    }
    destroyHandle(m_frameBuffer);
    destroyHandle(m_color);
    destroyHandle(m_depth);
    destroyHandle(m_preview);
    m_width = 0;
    m_height = 0;
    m_previewValid = false;
}

void FramePresentation::shutdown()
{
    m_frameActive = false;
    if (m_initialized && !m_noop)
    {
        bgfx::setViewOrder();
    }
    destroyTargets();
    destroyHandle(m_vertices);
    destroyHandle(m_indices);
    destroyHandle(m_program);
    destroyHandle(m_sampler);
    destroyHandle(m_gamma);
    m_initialized = false;
    m_noop = false;
    m_previewCapturedThisFrame = false;
}

std::optional<PreviewTexture> FramePresentation::worldPreviewTexture() const
{
    if (!m_previewValid || !bgfx::isValid(m_preview))
    {
        return std::nullopt;
    }
    return PreviewTexture{m_preview, m_width, m_height};
}
}
