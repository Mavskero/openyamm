#include "game/ui/screens/WinGameScreen.h"
#include "game/ui/Utf8Text.h"
#if defined(__APPLE__)
#include "engine/AppleText.h"
#endif

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace OpenYAMM::Game
{
namespace
{
constexpr float ReferenceWidth = 640.0f;
constexpr float ReferenceHeight = 480.0f;
constexpr float MaxUiScale = 8.0f;
constexpr const char *EndgameFontName = "ENDGAME";
constexpr uint32_t TextColor = 0xffffffffu;
}

WinGameScreen::WinGameScreen(
    const Engine::AssetFileSystem &assetFileSystem,
    const WinGameCertificate &certificate,
    AppMode mode)
    : MenuScreenBase(assetFileSystem)
    , m_certificate(certificate)
    , m_mode(mode)
{
}

AppMode WinGameScreen::mode() const
{
    return m_mode;
}

bool WinGameScreen::shouldClose() const
{
    return m_shouldClose;
}

void WinGameScreen::handleSdlEvent(const SDL_Event &event)
{
    if (event.type == SDL_EVENT_KEY_DOWN
        && (event.key.key == SDLK_ESCAPE
            || event.key.key == SDLK_RETURN
            || event.key.key == SDLK_KP_ENTER
            || event.key.key == SDLK_SPACE))
    {
        m_shouldClose = true;
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_FINGER_UP)
    {
        m_shouldClose = true;
    }
}

void WinGameScreen::drawCenteredText(
    const std::string &fontName,
    const std::string &text,
    float logicalY,
    float scale,
    float viewportX,
    float viewportY,
    float viewportWidth,
    uint32_t colorAbgr)
{
    const float textWidth = measureTextWidth(fontName, text, scale);
    const float x = std::round(viewportX + (viewportWidth - textWidth) * 0.5f);
    const float y = std::round(viewportY + logicalY * scale);
    drawText(fontName, text, x, y, colorAbgr, scale, false);
}

std::vector<std::string> WinGameScreen::wrapText(
    const std::string &fontName,
    const std::string &text,
    float maxWidth,
    float scale)
{
#if defined(__APPLE__)
    if (hasUtf8MultibyteText(text))
    {
        const std::optional<std::vector<std::string>> wrapped = Engine::wrapAppleText(
            text, fontHeight(fontName) * scale, std::max(1.0f, maxWidth - 4.0f));
        if (wrapped) return *wrapped;
    }
#endif
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string word;
    std::string currentLine;

    while (input >> word)
    {
        const std::string candidate = currentLine.empty() ? word : currentLine + " " + word;

        if (!currentLine.empty() && measureTextWidth(fontName, candidate, scale) > maxWidth)
        {
            lines.push_back(currentLine);
            currentLine = word;
        }
        else
        {
            currentLine = candidate;
        }
    }

    if (!currentLine.empty())
    {
        lines.push_back(currentLine);
    }

    return lines;
}

void WinGameScreen::drawScreen(float)
{
    const Rect fullFrame = {0.0f, 0.0f, static_cast<float>(frameWidth()), static_cast<float>(frameHeight())};
    drawPixelsBgra("__wingame_black__", 1, 1, m_blackPixel, fullFrame);

    const float scale = std::min(
        std::min(
            static_cast<float>(frameWidth()) / ReferenceWidth,
            static_cast<float>(frameHeight()) / ReferenceHeight),
        MaxUiScale);
    const float viewportWidth = std::round(ReferenceWidth * scale);
    const float viewportHeight = std::round(ReferenceHeight * scale);
    const float viewportX = std::round((static_cast<float>(frameWidth()) - viewportWidth) * 0.5f);
    const float viewportY = std::round((static_cast<float>(frameHeight()) - viewportHeight) * 0.5f);
    const Rect viewport = {viewportX, viewportY, viewportWidth, viewportHeight};

    drawTexture(m_certificate.backgroundTexture, viewport);

    drawCenteredText(
        EndgameFontName, "Congratulations!", 155.0f, scale, viewportX, viewportY, viewportWidth, TextColor);
    drawCenteredText(
        EndgameFontName,
        m_certificate.characterLine,
        176.0f,
        scale,
        viewportX,
        viewportY,
        viewportWidth,
        TextColor);

    constexpr float EndingTop = 230.0f;
    constexpr float TotalTimeTop = 355.0f;
    const float availableEndingHeight = (TotalTimeTop - EndingTop - 12.0f) * scale;
    float endingScale = scale;
    float lineHeight = 0.0f;
    std::vector<std::string> endingLines;
    for (unsigned fit = 0; fit < 16; ++fit)
    {
        endingLines = wrapText(EndgameFontName, m_certificate.endingText, 520.0f * scale, endingScale);
        lineHeight = static_cast<float>(fontHeight(EndgameFontName) + 3) * endingScale;
#if defined(__APPLE__)
        for (const std::string &line : endingLines)
        {
            if (!hasUtf8MultibyteText(line)) continue;
            const std::optional<Engine::AppleTextImageSize> bounds =
                Engine::measureAppleTextImageSize(line, fontHeight(EndgameFontName) * endingScale);
            if (bounds)
            {
                lineHeight = std::max(lineHeight, float(bounds->height) + 2.0f);
            }
        }
#endif
        if (lineHeight * float(endingLines.size()) <= availableEndingHeight) break;
        if (fit + 1 < 16) endingScale *= 0.9f;
    }
    float endingY = EndingTop * scale;

    for (const std::string &line : endingLines)
    {
        drawCenteredText(
            EndgameFontName, line, endingY / endingScale, endingScale, viewportX, viewportY, viewportWidth, TextColor);
        endingY += lineHeight;
    }

    drawCenteredText(
        EndgameFontName,
        m_certificate.totalTimeLine,
        TotalTimeTop,
        scale,
        viewportX,
        viewportY,
        viewportWidth,
        TextColor);
    drawCenteredText(
        EndgameFontName,
        m_certificate.scoreLine,
        404.0f,
        scale,
        viewportX,
        viewportY,
        viewportWidth,
        TextColor);
}
}
