#pragma once

#include "engine/AssetFileSystem.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
class UiLayoutManager
{
public:
    struct TextureRegion
    {
        std::string source;
        int sourceWidth = 0;
        int sourceHeight = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        // Zero rejects a different source shape, out-of-bounds region or non-integral asset scale.
        int pixelScaleForSource(int physicalWidth, int physicalHeight) const;
    };

    enum class LayoutAnchor
    {
        TopLeft,
        TopCenter,
        TopRight,
        Left,
        Center,
        Right,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    enum class LayoutAttachMode
    {
        None,
        RightOf,
        LeftOf,
        Above,
        Below,
        CenterAbove,
        CenterBelow,
        InsideLeft,
        InsideRight,
        InsideTopCenter,
        InsideTopLeft,
        InsideTopRight,
        InsideBottomLeft,
        InsideBottomCenter,
        InsideBottomRight,
        CenterIn
    };

    enum class LayoutAnchorSpace
    {
        Viewport,
        Screen
    };

    enum class TextAlignX
    {
        Left,
        Center,
        Right
    };

    enum class TextAlignY
    {
        Top,
        Middle,
        Bottom
    };

    struct LayoutElement
    {
        std::string id;
        std::string normalizedId;
        std::string screen;
        std::string normalizedScreen;
        LayoutAnchor anchor = LayoutAnchor::TopLeft;
        LayoutAnchorSpace anchorSpace = LayoutAnchorSpace::Viewport;
        std::string parentId;
        LayoutAttachMode attachTo = LayoutAttachMode::None;
        float gapX = 0.0f;
        float gapY = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        std::string bottomToId;
        float bottomGap = 0.0f;
        float minScale = 1.0f;
        float maxScale = 8.0f;
        float scaleReferenceWidth = 0.0f;
        float scaleReferenceHeight = 0.0f;
        bool hasExplicitScale = false;
        int zIndex = 0;
        bool visible = true;
        bool interactive = false;
        std::string primaryAsset;
        std::string hoverAsset;
        std::string pressedAsset;
        std::string secondaryAsset;
        std::string tertiaryAsset;
        std::string quaternaryAsset;
        std::string quinaryAsset;
        std::string fontName;
        std::string labelText;
        uint32_t textColorAbgr = 0xffffffffu;
        TextAlignX textAlignX = TextAlignX::Left;
        TextAlignY textAlignY = TextAlignY::Top;
        float textScale = 1.0f;
        bool textFitHeight = false;
        float textPadX = 0.0f;
        float textPadY = 0.0f;
        std::string notes;
    };

    void clear();
    bool loadLayoutFile(const Engine::AssetFileSystem &assetFileSystem, const std::string &path);
    bool loadLayoutText(const std::string &sourceName, const std::string &text);

    const LayoutElement *findElement(const std::string &layoutId) const;
    const std::vector<std::string> &sortedLayoutIdsForScreenCached(const std::string &screen) const;
    std::vector<std::string> sortedLayoutIdsForScreen(const std::string &screen) const;
    int maxZIndexForScreen(const std::string &screen) const;
    static int defaultZIndexForScreen(const std::string &screen);

    const std::unordered_map<std::string, LayoutElement> &elements() const;
    const std::unordered_map<std::string, TextureRegion> &textureRegions() const;
    const TextureRegion *findTextureRegion(const std::string &name) const;

private:
    std::vector<std::string> m_layoutOrder;
    std::unordered_map<std::string, LayoutElement> m_layoutElements;
    std::unordered_map<std::string, TextureRegion> m_textureRegions;
    std::unordered_map<std::string, const LayoutElement *> m_layoutElementByLookupId;
    mutable std::unordered_map<std::string, std::vector<std::string>> m_sortedLayoutIdsByScreen;
};
}
