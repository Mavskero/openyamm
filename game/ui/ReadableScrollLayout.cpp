#include "game/ui/ReadableScrollLayout.h"

#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/Utf8Text.h"

#if defined(__APPLE__)
#include "engine/AppleText.h"
#endif

#include <algorithm>
#include <cmath>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
constexpr float ReferenceWidth = 640.0f;
constexpr float ReferenceHeight = 480.0f;
constexpr float ViewportPadding = 8.0f;
constexpr float BottomPadding = 15.0f;
constexpr float TitleBodyGap = 8.0f;
constexpr float FooterGap = 8.0f;

float renderedLineHeight(
    const GameplayHudFontHandle &font,
    const std::vector<std::string> &lines,
    float scale)
{
    float lineHeight = std::max(1.0f, static_cast<float>(font.fontHeight) * scale);
#if defined(__APPLE__)
    for (const std::string &line : lines)
    {
        if (hasUtf8MultibyteText(line))
        {
            const std::optional<Engine::AppleTextImageSize> imageSize =
                Engine::measureAppleTextImageSize(line, font.fontHeight * scale);
            if (imageSize)
            {
                lineHeight = std::max(lineHeight, float(imageSize->height) + std::max(1.0f, scale));
            }
        }
    }
#else
    static_cast<void>(lines);
#endif
    return lineHeight;
}

std::vector<std::string> wrapScrollText(
    GameplayScreenRuntime &context,
    const GameplayHudFontHandle &font,
    const std::string &text,
    float width,
    float scale)
{
#if defined(__APPLE__)
    if (hasUtf8MultibyteText(text))
    {
        width -= 4.0f + std::max(1.0f, scale);
    }
#endif
    const float wrapWidth = std::max(1.0f, width / scale);
    const std::vector<std::string> wrapped = context.wrapHudTextToWidth(font, text, wrapWidth);
    std::vector<std::string> lines;

    for (const std::string &line : wrapped)
    {
        if (context.measureHudTextWidth(font, line) <= wrapWidth)
        {
            lines.push_back(line);
            continue;
        }

        // The bitmap wrapper keeps an unbroken word together. Split it at UTF-8
        // boundaries so long codes and names remain reachable on narrow windows.
        std::string fragment;
        for (size_t offset = 0; offset < line.size();)
        {
            size_t byteCount = 0;
            if (!decodeUtf8Character(std::string_view(line).substr(offset), byteCount))
            {
                byteCount = 1;
            }
            const std::string character = line.substr(offset, byteCount);
            if (!fragment.empty()
                && context.measureHudTextWidth(font, fragment + character) > wrapWidth)
            {
                lines.push_back(fragment);
                fragment.clear();
            }
            fragment += character;
            offset += byteCount;
        }
        lines.push_back(fragment);
    }

    if (lines.empty())
    {
        lines.emplace_back();
    }
    return lines;
}
}

std::optional<ReadableScrollLayout> resolveReadableScrollLayout(
    GameplayScreenRuntime &context,
    int width,
    int height)
{
    const GameplayUiController::ReadableScrollOverlayState &overlay = context.readableScrollOverlayReadOnly();
    const UiLayoutManager::LayoutElement *pRoot = context.findHudLayoutElement("SpellInspectRoot");
    const UiLayoutManager::LayoutElement *pTitle = context.findHudLayoutElement("SpellInspectTitle");
    const UiLayoutManager::LayoutElement *pBody = context.findHudLayoutElement("SpellInspectBody");
    const std::optional<GameplayHudFontHandle> bodyFont = context.findHudFont("SMALLNUM");

    if (!overlay.active || width <= 0 || height <= 0
        || pRoot == nullptr || pTitle == nullptr || pBody == nullptr || !bodyFont
        || pRoot->width <= 0.0f || pRoot->height <= 0.0f)
    {
        return std::nullopt;
    }

    const std::optional<GameplayHudFontHandle> titleFont =
        context.findHudFont(pTitle->fontName.empty() ? "Create" : pTitle->fontName);
    if (!titleFont)
    {
        return std::nullopt;
    }

    const GameplayUiViewportRect viewport = GameplayHudCommon::computeUiViewportRect(width, height);
    const float baseScale = std::min(viewport.width / ReferenceWidth, viewport.height / ReferenceHeight);
    const float scale = std::min({
        std::clamp(baseScale, pRoot->minScale, pRoot->maxScale),
        viewport.width / (pRoot->width + 2.0f * ViewportPadding),
        viewport.height / (pRoot->height + 2.0f * ViewportPadding)});
    if (scale <= 0.0f)
    {
        return std::nullopt;
    }

    const GameplayResolvedHudLayoutElement provisionalRoot = {
        0.0f, 0.0f, pRoot->width * scale, pRoot->height * scale, scale};
    const auto resolveChild = [&provisionalRoot, scale](const UiLayoutManager::LayoutElement &element)
    {
        return GameplayHudCommon::resolveAttachedHudLayoutRect(
            element.attachTo, provisionalRoot, element.width * scale, element.height * scale,
            element.gapX, element.gapY, scale);
    };
    const GameplayResolvedHudLayoutElement authoredTitle = resolveChild(*pTitle);
    const GameplayResolvedHudLayoutElement authoredBody = resolveChild(*pBody);
    const float titlePadX = std::abs(pTitle->textPadX) * scale;
    const float titlePadY = std::abs(pTitle->textPadY) * scale;
    const float bodyPadX = std::abs(pBody->textPadX) * scale;
    const float bodyPadY = std::abs(pBody->textPadY) * scale;
    const float titleScale = scale * std::max(0.1f, pTitle->textScale);

    ReadableScrollLayout layout = {};
    // The shared spell frame reserves a left icon column. A letter has no icon,
    // so use equal reading margins while retaining its frame, fonts and vertical spacing.
    const float readingMargin = 20.0f * scale;
    layout.titleRect = {
        readingMargin + titlePadX, authoredTitle.y + titlePadY,
        provisionalRoot.width - 2.0f * (readingMargin + titlePadX), 0.0f, titleScale};
    layout.bodyRect = {
        readingMargin + bodyPadX, authoredBody.y + bodyPadY,
        provisionalRoot.width - 2.0f * (readingMargin + bodyPadX), 0.0f, scale};
    if (layout.titleRect.width <= 0.0f || layout.bodyRect.width <= 0.0f)
    {
        return std::nullopt;
    }

    layout.titleLines = wrapScrollText(context, *titleFont, overlay.title, layout.titleRect.width, titleScale);
    layout.bodyLines = wrapScrollText(context, *bodyFont, overlay.body, layout.bodyRect.width, scale);
    layout.titleLineHeight = renderedLineHeight(*titleFont, layout.titleLines, titleScale);
    layout.lineHeight = renderedLineHeight(*bodyFont, layout.bodyLines, scale);
    layout.titleRect.height = std::max(
        authoredTitle.height - 2.0f * titlePadY,
        layout.titleLineHeight * static_cast<float>(layout.titleLines.size()));
    layout.bodyRect.y = std::max(
        layout.bodyRect.y, layout.titleRect.y + layout.titleRect.height + TitleBodyGap * scale);

    const float maximumHeight = viewport.height - 2.0f * ViewportPadding * scale;
    const float bodyHeight = layout.lineHeight * static_cast<float>(layout.bodyLines.size());
    const float fullHeight = layout.bodyRect.y + bodyHeight + BottomPadding * scale;
    const bool showControls = overlay.persistent || fullHeight > maximumHeight;
    const float buttonHeight = std::max(24.0f * scale, bodyFont->fontHeight * scale + 8.0f * scale);
    const float footerHeight = showControls ? buttonHeight + FooterGap * scale : 0.0f;
    const float rootHeight = std::min(
        maximumHeight, std::max(provisionalRoot.height, fullHeight + footerHeight));
    const float availableBodyHeight =
        rootHeight - layout.bodyRect.y - BottomPadding * scale - footerHeight;
    if (availableBodyHeight < layout.lineHeight)
    {
        return std::nullopt;
    }

    layout.linesPerPage = std::max(size_t(1), size_t(std::floor(availableBodyHeight / layout.lineHeight)));
    layout.pageCount = (layout.bodyLines.size() + layout.linesPerPage - 1) / layout.linesPerPage;
    if (layout.pageCount > 1)
    {
        // Balance the pages so the final signature is not stranded on an otherwise empty sheet.
        layout.linesPerPage = (layout.bodyLines.size() + layout.pageCount - 1) / layout.pageCount;
    }
    layout.bodyRect.height = layout.lineHeight * static_cast<float>(layout.linesPerPage);
    layout.rootRect = {
        std::round(viewport.x + (viewport.width - provisionalRoot.width) * 0.5f),
        std::round(viewport.y + (viewport.height - rootHeight) * 0.5f),
        provisionalRoot.width, rootHeight, scale};
    layout.titleRect.x += layout.rootRect.x;
    layout.titleRect.y += layout.rootRect.y;
    layout.bodyRect.x += layout.rootRect.x;
    layout.bodyRect.y += layout.rootRect.y;

    if (showControls)
    {
        const float footerY = layout.rootRect.y + rootHeight - BottomPadding * scale - buttonHeight;
        layout.previousButton = {
            layout.rootRect.x + 20.0f * scale, footerY, 44.0f * scale, buttonHeight, scale};
        layout.nextButton = {
            layout.rootRect.x + 72.0f * scale, footerY, 44.0f * scale, buttonHeight, scale};
        layout.closeButton = {
            layout.rootRect.x + layout.rootRect.width - 84.0f * scale,
            footerY, 64.0f * scale, buttonHeight, scale};
    }
    return layout;
}
}
