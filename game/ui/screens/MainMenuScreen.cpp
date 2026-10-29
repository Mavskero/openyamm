#include "game/ui/screens/MainMenuScreen.h"

#include "game/audio/GameAudioSystem.h"
#include "game/ui/Utf8Text.h"

#if defined(__APPLE__)
#include "engine/AppleText.h"
#endif

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr float ReferenceWidth = 640.0f;
constexpr float ReferenceHeight = 480.0f;
constexpr float MaxUiScale = 8.0f;
constexpr const char *MainMenuLayoutPath = "Data/ui/menu/main_menu.yml";
constexpr const char *CreditsTextPath = "Data/data_tables/credits.txt";
constexpr const char *CreditsFont = "SMALLNUM";

struct ResolvedLayoutElement
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float scale = 1.0f;
};

ResolvedLayoutElement resolveAttachedLayoutRect(
    UiLayoutManager::LayoutAttachMode attachTo,
    const ResolvedLayoutElement &parent,
    float width,
    float height,
    float gapX,
    float gapY,
    float scale,
    std::optional<float> visibleRightEdge = std::nullopt)
{
    ResolvedLayoutElement resolved = {};
    resolved.width = width;
    resolved.height = height;
    resolved.scale = scale;
    const float parentRight = visibleRightEdge.has_value()
        ? std::min(parent.x + parent.width, *visibleRightEdge)
        : parent.x + parent.width;

    switch (attachTo)
    {
        case UiLayoutManager::LayoutAttachMode::None:
            resolved.x = parent.x + gapX * scale;
            resolved.y = parent.y + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::RightOf:
            resolved.x = parent.x + parent.width + gapX * scale;
            resolved.y = parent.y + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::LeftOf:
            resolved.x = parent.x - resolved.width + gapX * scale;
            resolved.y = parent.y + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::Above:
            resolved.x = parent.x + gapX * scale;
            resolved.y = parent.y - resolved.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::Below:
            resolved.x = parent.x + gapX * scale;
            resolved.y = parent.y + parent.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::CenterAbove:
            resolved.x = parent.x + (parent.width - resolved.width) * 0.5f + gapX * scale;
            resolved.y = parent.y - resolved.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::CenterBelow:
            resolved.x = parent.x + (parent.width - resolved.width) * 0.5f + gapX * scale;
            resolved.y = parent.y + parent.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideLeft:
            resolved.x = parent.x + gapX * scale;
            resolved.y = parent.y + (parent.height - resolved.height) * 0.5f + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideRight:
            resolved.x = parentRight - resolved.width + gapX * scale;
            resolved.y = parent.y + (parent.height - resolved.height) * 0.5f + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideTopCenter:
            resolved.x = parent.x + (parent.width - resolved.width) * 0.5f + gapX * scale;
            resolved.y = parent.y + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideTopLeft:
            resolved.x = parent.x + gapX * scale;
            resolved.y = parent.y + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideTopRight:
            resolved.x = parentRight - resolved.width + gapX * scale;
            resolved.y = parent.y + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideBottomLeft:
            resolved.x = parent.x + gapX * scale;
            resolved.y = parent.y + parent.height - resolved.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideBottomCenter:
            resolved.x = parent.x + (parent.width - resolved.width) * 0.5f + gapX * scale;
            resolved.y = parent.y + parent.height - resolved.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::InsideBottomRight:
            resolved.x = parentRight - resolved.width + gapX * scale;
            resolved.y = parent.y + parent.height - resolved.height + gapY * scale;
            break;

        case UiLayoutManager::LayoutAttachMode::CenterIn:
            resolved.x = parent.x + (parent.width - resolved.width) * 0.5f + gapX * scale;
            resolved.y = parent.y + (parent.height - resolved.height) * 0.5f + gapY * scale;
            break;
    }

    return resolved;
}

std::optional<ResolvedLayoutElement> resolveLayoutElementRecursive(
    const UiLayoutManager &layoutManager,
    const std::string &layoutId,
    int screenWidth,
    int screenHeight,
    float fallbackWidth,
    float fallbackHeight,
    std::unordered_set<std::string> &visited)
{
    if (visited.contains(layoutId))
    {
        return std::nullopt;
    }

    visited.insert(layoutId);
    const UiLayoutManager::LayoutElement *pElement = layoutManager.findElement(layoutId);

    if (pElement == nullptr)
    {
        visited.erase(layoutId);
        return std::nullopt;
    }

    const UiLayoutManager::LayoutElement &element = *pElement;
    const float baseScale = std::min(
        std::min(
            static_cast<float>(screenWidth) / ReferenceWidth,
            static_cast<float>(screenHeight) / ReferenceHeight),
        MaxUiScale);
    const float viewportWidth = ReferenceWidth * baseScale;
    const float viewportHeight = ReferenceHeight * baseScale;
    const float viewportX = (static_cast<float>(screenWidth) - viewportWidth) * 0.5f;
    const float viewportY = (static_cast<float>(screenHeight) - viewportHeight) * 0.5f;
    const float anchorX = element.anchorSpace == UiLayoutManager::LayoutAnchorSpace::Screen ? 0.0f : viewportX;
    const float anchorY = element.anchorSpace == UiLayoutManager::LayoutAnchorSpace::Screen ? 0.0f : viewportY;
    const float anchorWidth = element.anchorSpace == UiLayoutManager::LayoutAnchorSpace::Screen
        ? static_cast<float>(screenWidth)
        : viewportWidth;
    const float anchorHeight = element.anchorSpace == UiLayoutManager::LayoutAnchorSpace::Screen
        ? static_cast<float>(screenHeight)
        : viewportHeight;
    ResolvedLayoutElement resolved = {};

    if (!element.parentId.empty())
    {
        const UiLayoutManager::LayoutElement *pParent = layoutManager.findElement(element.parentId);

        if (pParent == nullptr)
        {
            visited.erase(layoutId);
            return std::nullopt;
        }

        const std::optional<ResolvedLayoutElement> parent = resolveLayoutElementRecursive(
            layoutManager,
            element.parentId,
            screenWidth,
            screenHeight,
            pParent->width,
            pParent->height,
            visited);

        if (!parent.has_value())
        {
            visited.erase(layoutId);
            return std::nullopt;
        }

        resolved.scale = element.hasExplicitScale
            ? std::clamp(baseScale, element.minScale, element.maxScale)
            : parent->scale;
        resolved.width = (element.width > 0.0f ? element.width : fallbackWidth) * resolved.scale;
        resolved.height = (element.height > 0.0f ? element.height : fallbackHeight) * resolved.scale;
        resolved = resolveAttachedLayoutRect(
            element.attachTo,
            *parent,
            resolved.width,
            resolved.height,
            element.gapX,
            element.gapY,
            resolved.scale,
            element.anchorSpace == UiLayoutManager::LayoutAnchorSpace::Screen
                ? std::optional<float>(static_cast<float>(screenWidth))
                : std::nullopt);
        visited.erase(layoutId);
        return resolved;
    }

    resolved.scale = std::clamp(baseScale, element.minScale, element.maxScale);
    resolved.width = (element.width > 0.0f ? element.width : fallbackWidth) * resolved.scale;
    resolved.height = (element.height > 0.0f ? element.height : fallbackHeight) * resolved.scale;

    switch (element.anchor)
    {
        case UiLayoutManager::LayoutAnchor::TopLeft:
            resolved.x = anchorX + element.offsetX * resolved.scale;
            resolved.y = anchorY + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::TopCenter:
            resolved.x = anchorX + anchorWidth * 0.5f - resolved.width * 0.5f + element.offsetX * resolved.scale;
            resolved.y = anchorY + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::TopRight:
            resolved.x = anchorX + anchorWidth - resolved.width + element.offsetX * resolved.scale;
            resolved.y = anchorY + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::Left:
            resolved.x = anchorX + element.offsetX * resolved.scale;
            resolved.y = anchorY + anchorHeight * 0.5f - resolved.height * 0.5f + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::Center:
            resolved.x = anchorX + anchorWidth * 0.5f - resolved.width * 0.5f + element.offsetX * resolved.scale;
            resolved.y = anchorY + anchorHeight * 0.5f - resolved.height * 0.5f + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::Right:
            resolved.x = anchorX + anchorWidth - resolved.width + element.offsetX * resolved.scale;
            resolved.y = anchorY + anchorHeight * 0.5f - resolved.height * 0.5f + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::BottomLeft:
            resolved.x = anchorX + element.offsetX * resolved.scale;
            resolved.y = anchorY + anchorHeight - resolved.height + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::BottomCenter:
            resolved.x = anchorX + anchorWidth * 0.5f - resolved.width * 0.5f + element.offsetX * resolved.scale;
            resolved.y = anchorY + anchorHeight - resolved.height + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::BottomRight:
            resolved.x = anchorX + anchorWidth - resolved.width + element.offsetX * resolved.scale;
            resolved.y = anchorY + anchorHeight - resolved.height + element.offsetY * resolved.scale;
            break;
    }

    visited.erase(layoutId);
    return resolved;
}
}

MainMenuScreen::MainMenuScreen(
    const Engine::AssetFileSystem &assetFileSystem,
    GameAudioSystem *pGameAudioSystem,
    Action newGameAction,
    Action loadGameAction,
    Action quitAction)
    : MenuScreenBase(assetFileSystem)
    , m_pGameAudioSystem(pGameAudioSystem)
    , m_newGameAction(std::move(newGameAction))
    , m_loadGameAction(std::move(loadGameAction))
    , m_quitAction(std::move(quitAction))
{
}

AppMode MainMenuScreen::mode() const
{
    return AppMode::MainMenu;
}

void MainMenuScreen::prepareForFirstFrame()
{
    ensureLayoutLoaded();
    preloadLayoutAssets(m_layoutManager);
}

void MainMenuScreen::drawScreen(float deltaSeconds)
{
    static_cast<void>(deltaSeconds);
    ensureLayoutLoaded();

    const Rect backgroundRect = resolveLayoutRect("MainMenuBackground", ReferenceWidth, ReferenceHeight).value_or(
        Rect{0.0f, 0.0f, ReferenceWidth, ReferenceHeight});
    drawTexture(resolveAssetName("MainMenuBackground", "title"), backgroundRect);

    if (m_creditsVisible)
    {
        drawCredits();
        return;
    }

    const ButtonState newGameButton = drawButton(
        resolveButtonVisuals("MainMenuNewGameButton", ButtonVisualSet{"T_new_up", "T_new_ht", "T_new_dn"}),
        resolveLayoutRect("MainMenuNewGameButton", 135.0f, 41.0f).value_or(Rect{}));
    const ButtonState loadGameButton = drawButton(
        resolveButtonVisuals("MainMenuLoadGameButton", ButtonVisualSet{"T_load_up", "T_load_ht", "T_load_dn"}),
        resolveLayoutRect("MainMenuLoadGameButton", 135.0f, 41.0f).value_or(Rect{}));
    const ButtonState creditsButton = drawButton(
        resolveButtonVisuals("MainMenuCreditsButton", ButtonVisualSet{"T_cred_up", "T_cred_ht", "T_cred_dn"}),
        resolveLayoutRect("MainMenuCreditsButton", 135.0f, 41.0f).value_or(Rect{}));
    const ButtonState quitButton = drawButton(
        resolveButtonVisuals("MainMenuQuitButton", ButtonVisualSet{"T_quit_up", "T_quit_ht", "T_quit_dn"}),
        resolveLayoutRect("MainMenuQuitButton", 135.0f, 41.0f).value_or(Rect{}));

    if (newGameButton.clicked && m_newGameAction)
    {
        playUiClickSound(SoundId::SelectingNewCharacter);
        m_newGameAction();
        return;
    }

    if (loadGameButton.clicked && m_loadGameAction)
    {
        playUiClickSound(SoundId::SelectingNewCharacter);
        m_loadGameAction();
        return;
    }

    if (creditsButton.clicked)
    {
        playUiClickSound(SoundId::ClickIn);
        openCredits();
        return;
    }

    if (quitButton.clicked && m_quitAction)
    {
        playUiClickSound(SoundId::ClickIn);
        m_quitAction();
        return;
    }
}

void MainMenuScreen::openCredits()
{
    m_creditsText = "OpenYAMM\nOpen Yet Another Might and Magic\n";
    const std::optional<std::string> originalCredits = assetFileSystem().readTextFile(CreditsTextPath);
    if (originalCredits && !originalCredits->empty())
    {
        m_creditsText += "\n原版制作人员\n\n" + *originalCredits;
    }
    else
    {
        m_creditsText += "\n原版制作信息尚未导入\n";
    }
    // Project acknowledgements and library names are documented in README.md.
    m_creditsText += "\nOpenYAMM 社区致谢\n"
        "OpenEnroth development team\nRodril and all MMMerge contributors\n"
        "\nBuilt with SDL3, bgfx, PhysicsFS, FFmpeg and Lua.\n";
    m_creditsPageIndex = 0;
    m_creditsLayoutWidth = 0;
    m_creditsLayoutHeight = 0;
    m_creditsKeysHeld = inputFrame().keyboardHeld;
    m_creditsVisible = true;
}

bool MainMenuScreen::creditsKeyPressed(SDL_Scancode scancode)
{
    const bool held = isScancodeHeld(scancode);
    const bool pressed = inputFrame().scancodePressCount(scancode) != 0
        || (held && !m_creditsKeysHeld[scancode]);
    m_creditsKeysHeld[scancode] = held;
    return pressed;
}

void MainMenuScreen::rebuildCreditsLayout(const Rect &bodyRect, float scale)
{
    m_creditsLines.clear();
    const float fontSize = static_cast<float>(fontHeight(CreditsFont)) * scale;
    const auto lineWidth = [this, scale, fontSize](const std::string &line)
    {
        float width = measureTextWidth(CreditsFont, line, scale);
#if defined(__APPLE__)
        if (hasUtf8MultibyteText(line))
        {
            const std::optional<Engine::AppleTextImageSize> image =
                Engine::measureAppleTextImageSize(line, fontSize);
            if (image)
            {
                width = std::max(width, static_cast<float>(image->width) + 1.0f);
            }
        }
#else
        static_cast<void>(fontSize);
#endif
        return width;
    };
    std::istringstream paragraphs(m_creditsText);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph))
    {
        std::istringstream words(paragraph);
        std::string line;
        std::string word;
        while (words >> word)
        {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (lineWidth(candidate) <= bodyRect.width)
            {
                line = candidate;
                continue;
            }
            if (!line.empty())
            {
                m_creditsLines.push_back(std::move(line));
                line.clear();
            }
            // An unbroken translated heading or name can span more than one row.
            for (size_t offset = 0; offset < word.size();)
            {
                size_t bytes = 0;
                if (!decodeUtf8Character(std::string_view(word).substr(offset), bytes))
                {
                    bytes = 1;
                }
                const std::string character = word.substr(offset, bytes);
                if (!line.empty() && lineWidth(line + character) > bodyRect.width)
                {
                    m_creditsLines.push_back(std::move(line));
                    line.clear();
                }
                line += character;
                offset += bytes;
            }
        }
        m_creditsLines.push_back(std::move(line));
    }
    m_creditsLineHeight = std::max(1.0f, fontSize) + std::max(1.0f, scale);
#if defined(__APPLE__)
    for (const std::string &line : m_creditsLines)
    {
        if (hasUtf8MultibyteText(line))
        {
            const std::optional<Engine::AppleTextImageSize> image =
                Engine::measureAppleTextImageSize(line, fontSize);
            if (image)
            {
                m_creditsLineHeight = std::max(m_creditsLineHeight, static_cast<float>(image->height) + 1.0f);
            }
        }
    }
#endif
    m_creditsLineHeight += 2.0f * scale;
    m_creditsLinesPerPage = std::max<size_t>(1, size_t(std::floor(bodyRect.height / m_creditsLineHeight)));
    m_creditsLayoutWidth = frameWidth();
    m_creditsLayoutHeight = frameHeight();
}

void MainMenuScreen::drawCredits()
{
    if (frameWidth() <= 0 || frameHeight() <= 0)
    {
        return;
    }
    const float scale = std::min({static_cast<float>(frameWidth()) / ReferenceWidth,
        static_cast<float>(frameHeight()) / ReferenceHeight, MaxUiScale});
    const Rect rootRect = {(frameWidth() - 560.0f * scale) * 0.5f,
        (frameHeight() - 424.0f * scale) * 0.5f, 560.0f * scale, 424.0f * scale};
    const Rect bodyRect = {rootRect.x + 20.0f * scale, rootRect.y + 44.0f * scale,
        rootRect.width - 40.0f * scale, rootRect.height - 96.0f * scale};
    if (m_creditsLayoutWidth != frameWidth() || m_creditsLayoutHeight != frameHeight())
    {
        rebuildCreditsLayout(bodyRect, scale);
    }
    const size_t pageCount = std::max<size_t>(1,
        (m_creditsLines.size() + m_creditsLinesPerPage - 1) / m_creditsLinesPerPage);
    m_creditsPageIndex = std::min(m_creditsPageIndex, pageCount - 1);
    const bool escapePressed = creditsKeyPressed(SDL_SCANCODE_ESCAPE);
    const bool returnPressed = creditsKeyPressed(SDL_SCANCODE_RETURN);
    const bool keypadEnterPressed = creditsKeyPressed(SDL_SCANCODE_KP_ENTER);
    const bool pageUpPressed = creditsKeyPressed(SDL_SCANCODE_PAGEUP);
    const bool leftPressed = creditsKeyPressed(SDL_SCANCODE_LEFT);
    const bool pageDownPressed = creditsKeyPressed(SDL_SCANCODE_PAGEDOWN);
    const bool rightPressed = creditsKeyPressed(SDL_SCANCODE_RIGHT);

    drawPixelsBgra("__main_menu_credits_backdrop__", 1, 1, {0, 0, 0, 160},
        {0.0f, 0.0f, static_cast<float>(frameWidth()), static_cast<float>(frameHeight())});
    drawTexture("parchment", rootRect);
    const float titleWidth = measureTextWidth("create", "制作人员", scale);
    drawText("create", "制作人员", rootRect.x + (rootRect.width - titleWidth) * 0.5f,
        rootRect.y + 12.0f * scale, 0xff80e0ffu, scale);

    const auto drawPageButton = [this, scale](const Rect &rect, const std::string &label, bool enabled)
    {
        const bool hovered = enabled && hitTest(rect);
        const uint32_t color = !enabled ? 0xff333333u : hovered ? 0xff395d7au : 0xff203346u;
        drawPixelsBgra("__main_menu_credits_button_" + std::to_string(color), 1, 1,
            {uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color), uint8_t(color >> 24)}, rect);
        const float width = measureTextWidth(CreditsFont, label, scale);
        const float height = static_cast<float>(fontHeight(CreditsFont)) * scale;
        drawText(CreditsFont, label, rect.x + (rect.width - width) * 0.5f,
            rect.y + (rect.height - height) * 0.5f, enabled ? 0xffffffffu : 0xff999999u, scale);
        return hovered && leftMouseJustReleased();
    };
    const float buttonY = rootRect.y + rootRect.height - 38.0f * scale;
    const bool previousClicked = drawPageButton(
        {rootRect.x + 20.0f * scale, buttonY, 80.0f * scale, 26.0f * scale},
        "上一页", m_creditsPageIndex > 0);
    const bool nextClicked = drawPageButton(
        {rootRect.x + 108.0f * scale, buttonY, 80.0f * scale, 26.0f * scale},
        "下一页", m_creditsPageIndex + 1 < pageCount);
    const bool closeClicked = drawPageButton(
        {rootRect.x + rootRect.width - 100.0f * scale, buttonY, 80.0f * scale, 26.0f * scale}, "返回", true);
    if ((previousClicked || pageUpPressed || leftPressed || mouseWheelDelta() > 0.0f)
        && m_creditsPageIndex > 0)
    {
        --m_creditsPageIndex;
    }
    else if ((nextClicked || pageDownPressed || rightPressed || mouseWheelDelta() < 0.0f)
        && m_creditsPageIndex + 1 < pageCount)
    {
        ++m_creditsPageIndex;
    }

    const size_t firstLine = m_creditsPageIndex * m_creditsLinesPerPage;
    const size_t endLine = std::min(m_creditsLines.size(), firstLine + m_creditsLinesPerPage);
    for (size_t line = firstLine; line < endLine; ++line)
    {
        drawText(CreditsFont, m_creditsLines[line], bodyRect.x,
            bodyRect.y + static_cast<float>(line - firstLine) * m_creditsLineHeight, 0xffffffffu, scale);
    }
    const std::string pageLabel = std::to_string(m_creditsPageIndex + 1) + " / " + std::to_string(pageCount);
    const float pageLabelWidth = measureTextWidth(CreditsFont, pageLabel, scale);
    drawText(CreditsFont, pageLabel, rootRect.x + (rootRect.width - pageLabelWidth) * 0.5f,
        buttonY + (26.0f - static_cast<float>(fontHeight(CreditsFont))) * scale * 0.5f, 0xffffffffu, scale);
    if (closeClicked || escapePressed || returnPressed || keypadEnterPressed)
    {
        playUiClickSound(SoundId::ClickIn);
        m_creditsVisible = false;
    }
}

void MainMenuScreen::playUiClickSound(SoundId soundId) const
{
    if (m_pGameAudioSystem == nullptr || soundId == SoundId::None)
    {
        return;
    }

    m_pGameAudioSystem->playCommonSound(soundId, GameAudioSystem::PlaybackGroup::Ui);
}

bool MainMenuScreen::ensureLayoutLoaded()
{
    if (m_layoutLoaded)
    {
        return true;
    }

    m_layoutManager.clear();
    m_layoutLoaded = m_layoutManager.loadLayoutFile(assetFileSystem(), MainMenuLayoutPath);
    return m_layoutLoaded;
}

std::optional<MenuScreenBase::Rect> MainMenuScreen::resolveLayoutRect(
    const std::string &layoutId,
    float fallbackWidth,
    float fallbackHeight) const
{
    if (!m_layoutLoaded)
    {
        return std::nullopt;
    }

    std::unordered_set<std::string> visited;
    const std::optional<ResolvedLayoutElement> resolved = resolveLayoutElementRecursive(
        m_layoutManager,
        layoutId,
        frameWidth(),
        frameHeight(),
        fallbackWidth,
        fallbackHeight,
        visited);

    if (!resolved.has_value())
    {
        return std::nullopt;
    }

    return Rect{
        std::round(resolved->x),
        std::round(resolved->y),
        std::round(resolved->width),
        std::round(resolved->height)
    };
}

MenuScreenBase::ButtonVisualSet MainMenuScreen::resolveButtonVisuals(
    const std::string &layoutId,
    const ButtonVisualSet &fallbackVisuals) const
{
    if (!m_layoutLoaded)
    {
        return fallbackVisuals;
    }

    const UiLayoutManager::LayoutElement *pLayout = m_layoutManager.findElement(layoutId);

    if (pLayout == nullptr)
    {
        return fallbackVisuals;
    }

    ButtonVisualSet visuals = fallbackVisuals;

    if (!pLayout->primaryAsset.empty())
    {
        visuals.defaultTextureName = pLayout->primaryAsset;
    }

    if (!pLayout->hoverAsset.empty())
    {
        visuals.highlightedTextureName = pLayout->hoverAsset;
    }

    if (!pLayout->pressedAsset.empty())
    {
        visuals.pressedTextureName = pLayout->pressedAsset;
    }

    return visuals;
}

std::string MainMenuScreen::resolveAssetName(const std::string &layoutId, const std::string &fallbackAssetName) const
{
    if (!m_layoutLoaded)
    {
        return fallbackAssetName;
    }

    const UiLayoutManager::LayoutElement *pLayout = m_layoutManager.findElement(layoutId);

    if (pLayout == nullptr || pLayout->primaryAsset.empty())
    {
        return fallbackAssetName;
    }

    return pLayout->primaryAsset;
}
}
