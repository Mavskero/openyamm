#pragma once

#include <bgfx/bgfx.h>

#include <optional>

namespace OpenYAMM::Game
{
struct PreviewTexture
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    int width = 0;
    int height = 0;
};

// Owns a neutral full-frame BGRA8/D24S8 target for views 0, 1, 2, 3 and 250.
// View 251 copies the world after views 0/1 and before HUD view 2; view 255
// presents the complete frame to the backbuffer with one display-gamma pass.
// The preview is borrowed, retains the last captured world and expires on resize/shutdown.
// Render-target texture UVs follow bgfx::Caps::originBottomLeft.
class FramePresentation
{
public:
    FramePresentation() = default;
    FramePresentation(const FramePresentation &) = delete;
    FramePresentation &operator=(const FramePresentation &) = delete;

    // Call after bgfx initialization; shutdown before bgfx shutdown.
    // Noop initialization/begin succeed without allocating or presenting GPU images.
    bool initialize();
    // False means no frame was begun. The caller must explicitly handle the failure.
    bool beginFrame(int width, int height);
    void captureWorldPreview();
    void submit(float displayGamma);
    void shutdown();
    std::optional<PreviewTexture> worldPreviewTexture() const;

private:
    bool resize(int width, int height);
    void destroyTargets();

    bgfx::FrameBufferHandle m_frameBuffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_color = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_depth = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_preview = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle m_vertices = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_indices = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_sampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_gamma = BGFX_INVALID_HANDLE;
    int m_width = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_noop = false;
    bool m_frameActive = false;
    bool m_previewValid = false;
    bool m_previewCapturedThisFrame = false;
};
}
