#pragma once

#if defined(__APPLE__)
#include <bgfx/bgfx.h>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class AppleTextTextureCache
{
public:
    struct Entry
    {
        std::string text;
        float size = 0;
        uint32_t color = 0;
        int width = 0;
        int height = 0;
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    };

    AppleTextTextureCache() = default;
    ~AppleTextTextureCache();
    AppleTextTextureCache(const AppleTextTextureCache &) = delete;
    AppleTextTextureCache &operator=(const AppleTextTextureCache &) = delete;

    std::optional<Entry> ensure(const std::string &text, float size, uint32_t color);
    void clear();

private:
    std::vector<Entry> m_entries;
};
}
#endif
