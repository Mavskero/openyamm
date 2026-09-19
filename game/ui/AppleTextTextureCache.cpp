#include "game/ui/AppleTextTextureCache.h"

#if defined(__APPLE__)
#include "engine/AppleText.h"
#include "engine/BgfxContext.h"
#include "game/render/TextureFiltering.h"

#include <algorithm>

namespace OpenYAMM::Game
{
AppleTextTextureCache::~AppleTextTextureCache()
{
    clear();
}

std::optional<AppleTextTextureCache::Entry> AppleTextTextureCache::ensure(
    const std::string &text, float size, uint32_t color)
{
    if (text.empty() || !Engine::BgfxContext::isBgfxInitialized())
    {
        return std::nullopt;
    }
    std::vector<Entry>::iterator found = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const Entry &entry)
        {
            return entry.text == text && entry.size == size && entry.color == color;
        });
    if (found == m_entries.end())
    {
        const std::optional<Engine::AppleTextImage> image = Engine::rasterizeAppleText(text, size, color);
        if (!image)
        {
            return std::nullopt;
        }
        size_t bytes = image->bgra.size();
        for (const Entry &entry : m_entries)
        {
            bytes += size_t(entry.width) * entry.height * 4;
        }
        while (!m_entries.empty() && (m_entries.size() >= 64 || bytes > 16 * 1024 * 1024))
        {
            const Entry &old = m_entries.front();
            bytes -= size_t(old.width) * old.height * 4;
            bgfx::destroy(old.handle);
            m_entries.erase(m_entries.begin());
        }
        const bgfx::TextureHandle handle = createBgraTexture2D(uint16_t(image->width), uint16_t(image->height),
            image->bgra.data(), uint32_t(image->bgra.size()), TextureFilterProfile::Text,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        if (!bgfx::isValid(handle))
        {
            return std::nullopt;
        }
        m_entries.push_back({text, size, color, image->width, image->height, handle});
        found = m_entries.end() - 1;
    }
    Entry entry = std::move(*found);
    m_entries.erase(found);
    m_entries.push_back(entry);
    return entry;
}

void AppleTextTextureCache::clear()
{
    if (Engine::BgfxContext::isBgfxInitialized())
    {
        for (const Entry &entry : m_entries)
        {
            bgfx::destroy(entry.handle);
        }
    }
    m_entries.clear();
}
}
#endif
