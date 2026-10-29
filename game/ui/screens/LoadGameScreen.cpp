#include "game/ui/screens/LoadGameScreen.h"

#include "game/maps/SaveGame.h"
#include "game/ui/Utf8Text.h"
#include "game/ui/SaveSlotText.h"
#include "game/tables/UiTextTable.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr float ReferenceWidth = 640.0f;
constexpr float ReferenceHeight = 480.0f;
constexpr float MaxUiScale = 8.0f;
constexpr const char *LoadGameLayoutPath = "Data/ui/gameplay/load_game.yml";
constexpr size_t VisibleSlotCount = 10;
constexpr uint64_t SlotDoubleClickWindowMs = 500;
constexpr uint32_t WhiteColorAbgr = 0xffffffffu;
constexpr uint32_t SelectedRowColorAbgr = 0xff00ffffu;

struct ResolvedLayoutElement
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float scale = 1.0f;
};

std::string toLowerCopy(const std::string &value)
{
    std::string normalized = value;

    for (char &character : normalized)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    return normalized;
}

bool decodeBmpBytesToBgra(
    const std::vector<uint8_t> &bmpBytes,
    int &width,
    int &height,
    std::vector<uint8_t> &pixels)
{
    SDL_IOStream *pIoStream = SDL_IOFromConstMem(bmpBytes.data(), bmpBytes.size());

    if (pIoStream == nullptr)
    {
        return false;
    }

    SDL_Surface *pLoadedSurface = SDL_LoadBMP_IO(pIoStream, true);

    if (pLoadedSurface == nullptr)
    {
        return false;
    }

    SDL_Surface *pConvertedSurface = SDL_ConvertSurface(pLoadedSurface, SDL_PIXELFORMAT_BGRA32);
    SDL_DestroySurface(pLoadedSurface);

    if (pConvertedSurface == nullptr)
    {
        return false;
    }

    width = pConvertedSurface->w;
    height = pConvertedSurface->h;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    pixels.resize(pixelCount);
    std::memcpy(pixels.data(), pConvertedSurface->pixels, pixelCount);
    SDL_DestroySurface(pConvertedSurface);
    return true;
}

ResolvedLayoutElement resolveAttachedLayoutRect(
    UiLayoutManager::LayoutAttachMode attachTo,
    const ResolvedLayoutElement &parent,
    float width,
    float height,
    float gapX,
    float gapY,
    float scale)
{
    ResolvedLayoutElement resolved = {};
    resolved.width = width;
    resolved.height = height;
    resolved.scale = scale;

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
            resolved.x = parent.x + parent.width - resolved.width + gapX * scale;
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
            resolved.x = parent.x + parent.width - resolved.width + gapX * scale;
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
            resolved.x = parent.x + parent.width - resolved.width + gapX * scale;
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
            resolved.scale);
        visited.erase(layoutId);
        return resolved;
    }

    const float rootWidth = element.width > 0.0f ? element.width : fallbackWidth;
    const float rootHeight = element.height > 0.0f ? element.height : fallbackHeight;
    resolved.scale = std::clamp(baseScale, element.minScale, element.maxScale);
    if (rootWidth >= ReferenceWidth && rootHeight >= ReferenceHeight)
    {
        const float fitScale = std::min(
            static_cast<float>(screenWidth) / rootWidth,
            static_cast<float>(screenHeight) / rootHeight);
        resolved.scale = std::min(resolved.scale, std::max(0.0f, fitScale));
    }
    resolved.width = rootWidth * resolved.scale;
    resolved.height = rootHeight * resolved.scale;

    switch (element.anchor)
    {
        case UiLayoutManager::LayoutAnchor::TopLeft:
            resolved.x = viewportX + element.offsetX * resolved.scale;
            resolved.y = viewportY + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::TopCenter:
            resolved.x = viewportX + viewportWidth * 0.5f - resolved.width * 0.5f + element.offsetX * resolved.scale;
            resolved.y = viewportY + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::TopRight:
            resolved.x = viewportX + viewportWidth - resolved.width + element.offsetX * resolved.scale;
            resolved.y = viewportY + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::Left:
            resolved.x = viewportX + element.offsetX * resolved.scale;
            resolved.y = viewportY + viewportHeight * 0.5f - resolved.height * 0.5f + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::Center:
            resolved.x = viewportX + viewportWidth * 0.5f - resolved.width * 0.5f + element.offsetX * resolved.scale;
            resolved.y = viewportY + viewportHeight * 0.5f - resolved.height * 0.5f + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::Right:
            resolved.x = viewportX + viewportWidth - resolved.width + element.offsetX * resolved.scale;
            resolved.y = viewportY + viewportHeight * 0.5f - resolved.height * 0.5f + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::BottomLeft:
            resolved.x = viewportX + element.offsetX * resolved.scale;
            resolved.y = viewportY + viewportHeight - resolved.height + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::BottomCenter:
            resolved.x = viewportX + viewportWidth * 0.5f - resolved.width * 0.5f + element.offsetX * resolved.scale;
            resolved.y = viewportY + viewportHeight - resolved.height + element.offsetY * resolved.scale;
            break;

        case UiLayoutManager::LayoutAnchor::BottomRight:
            resolved.x = viewportX + viewportWidth - resolved.width + element.offsetX * resolved.scale;
            resolved.y = viewportY + viewportHeight - resolved.height + element.offsetY * resolved.scale;
            break;
    }

    visited.erase(layoutId);
    return resolved;
}

MenuScreenBase::Rect toRect(const ResolvedLayoutElement &resolved)
{
    return MenuScreenBase::Rect{
        std::round(resolved.x),
        std::round(resolved.y),
        std::round(resolved.width),
        std::round(resolved.height)
    };
}
}

LoadGameScreen::LoadGameScreen(
    const Engine::AssetFileSystem &assetFileSystem,
    const GameDataRepository &gameData,
    LoadAction loadAction,
    CancelAction cancelAction,
    const UiTextTable *pUiText)
    : MenuScreenBase(assetFileSystem)
    , m_pGameData(&gameData)
    , m_loadAction(std::move(loadAction))
    , m_cancelAction(std::move(cancelAction))
    , m_uiText(pUiText != nullptr ? std::make_optional(*pUiText) : std::nullopt)
    , m_emptySlotText(m_uiText ? m_uiText->text(UiTextTable::Message::EmptySlot) : "Empty")
{
}

std::vector<LoadGameScreen::SaveSlotSummary> LoadGameScreen::s_cachedSlots;
bool LoadGameScreen::s_cachedSlotsValid = false;

void LoadGameScreen::invalidateCachedSaveSlots()
{
    s_cachedSlots.clear();
    s_cachedSlotsValid = false;
}

AppMode LoadGameScreen::mode() const
{
    return AppMode::LoadMenu;
}

void LoadGameScreen::prepareForFirstFrame()
{
    ensureLayoutLoaded();
    preloadLayoutAssets(m_layoutManager);

    if (s_cachedSlotsValid)
    {
        loadCachedSaveSlots();
    }
    else
    {
        refreshSaveSlots();
    }
}

void LoadGameScreen::onEnter()
{
    if (!m_slotsLoaded)
    {
        if (s_cachedSlotsValid)
        {
            loadCachedSaveSlots();
        }
        else
        {
            refreshSaveSlots();
        }
    }
}

void LoadGameScreen::drawScreen(float deltaSeconds)
{
    static_cast<void>(deltaSeconds);

    ensureLayoutLoaded();
    const bool escapeHeld = isScancodeHeld(SDL_SCANCODE_ESCAPE);
    const bool returnHeld = isScancodeHeld(SDL_SCANCODE_RETURN);
    const bool escapePressed = inputFrame().scancodePressCount(SDL_SCANCODE_ESCAPE) != 0
        || (escapeHeld && !m_escapePressed);
    const bool returnPressed = inputFrame().scancodePressCount(SDL_SCANCODE_RETURN) != 0
        || (returnHeld && !m_returnPressed);
    m_escapePressed = escapeHeld;
    m_returnPressed = returnHeld;

    if (escapePressed)
    {
        cancel();
        return;
    }

    if (!m_slots.empty())
    {
        const size_t previousIndex = m_selectedIndex;
        const auto pressed = [this](SDL_Scancode scancode)
        {
            return inputFrame().scancodePressCount(scancode) != 0;
        };
        if (pressed(SDL_SCANCODE_UP) && m_selectedIndex > 0)
        {
            --m_selectedIndex;
        }
        else if (pressed(SDL_SCANCODE_DOWN))
        {
            m_selectedIndex = std::min(m_selectedIndex + 1, m_slots.size() - 1);
        }
        else if (pressed(SDL_SCANCODE_PAGEUP))
        {
            m_selectedIndex -= std::min(m_selectedIndex, VisibleSlotCount);
        }
        else if (pressed(SDL_SCANCODE_PAGEDOWN))
        {
            m_selectedIndex = std::min(m_selectedIndex + VisibleSlotCount, m_slots.size() - 1);
        }
        else if (pressed(SDL_SCANCODE_HOME))
        {
            m_selectedIndex = 0;
        }
        else if (pressed(SDL_SCANCODE_END))
        {
            m_selectedIndex = m_slots.size() - 1;
        }
        if (m_selectedIndex != previousIndex)
        {
            if (m_selectedIndex < m_scrollOffset)
            {
                m_scrollOffset = m_selectedIndex;
            }
            else if (m_selectedIndex >= m_scrollOffset + VisibleSlotCount)
            {
                m_scrollOffset = m_selectedIndex - VisibleSlotCount + 1;
            }
            m_lastClickedSlotIndex = static_cast<size_t>(-1);
        }
    }

    if (returnPressed)
    {
        tryLoadSelectedSlot();
        return;
    }

    if (mouseWheelDelta() > 0.0f && m_scrollOffset > 0)
    {
        --m_scrollOffset;
    }
    else if (mouseWheelDelta() < 0.0f && m_scrollOffset + VisibleSlotCount < m_slots.size())
    {
        ++m_scrollOffset;
    }

    const Rect rootRect = resolveLayoutRect("LoadGameRoot", ReferenceWidth, ReferenceHeight).value_or(
        Rect{0.0f, 0.0f, ReferenceWidth, ReferenceHeight});

    drawViewportSidePanels("UI-Parch", ReferenceWidth, ReferenceHeight);
    drawTexture(resolveAssetName("LoadGameBackground", "Lsave640"), rootRect);

    if (const std::optional<Rect> titleRect = resolveLayoutRect("LoadGameTitle"))
    {
        drawTexture(resolveAssetName("LoadGameTitle", "bt_loagU"), *titleRect);
    }

    const ButtonState confirmButton = drawButton(
        resolveButtonVisuals("LoadGameConfirmButton", ButtonVisualSet{"bt_loadU", "bt_loadH", "bt_loadD"}),
        resolveLayoutRect("LoadGameConfirmButton", 106.0f, 37.0f).value_or(Rect{}));
    const ButtonState cancelButton = drawButton(
        resolveButtonVisuals("LoadGameCancelButton", ButtonVisualSet{"bt_cnclU", "bt_cnclH", "bt_cnclD"}),
        resolveLayoutRect("LoadGameCancelButton", 106.0f, 37.0f).value_or(Rect{}));

    const std::optional<Rect> scrollUpRect = resolveLayoutRect("LoadGameScrollUpButton", 18.0f, 16.0f);
    const std::optional<Rect> scrollDownRect = resolveLayoutRect("LoadGameScrollDownButton", 18.0f, 16.0f);
    const std::optional<Rect> previewRect = resolveLayoutRect("LoadGamePreviewRect", 272.0f, 169.0f);
    const std::optional<Rect> thumbRect = resolveLayoutRect("LoadGameScrollThumb", 17.0f, 17.0f);

    if (scrollUpRect && hitTest(*scrollUpRect) && leftMouseJustReleased() && m_scrollOffset > 0)
    {
        --m_scrollOffset;
    }

    if (scrollDownRect
        && hitTest(*scrollDownRect)
        && leftMouseJustReleased()
        && m_scrollOffset + VisibleSlotCount < m_slots.size())
    {
        ++m_scrollOffset;
    }

    if (previewRect)
    {
        static const std::vector<uint8_t> blackPixel = {0, 0, 0, 255};
        drawPixelsBgra("__load_game_preview_backdrop__", 1, 1, blackPixel, *previewRect);

        if (!m_slots.empty())
        {
            const SaveSlotSummary &slot = m_slots[std::min(m_selectedIndex, m_slots.size() - 1)];

            if (slot.previewWidth > 0 && slot.previewHeight > 0 && !slot.previewPixelsBgra.empty())
            {
                const float previewScale = std::min(
                    previewRect->width / static_cast<float>(slot.previewWidth),
                    previewRect->height / static_cast<float>(slot.previewHeight));
                const float drawWidth = static_cast<float>(slot.previewWidth) * previewScale;
                const float drawHeight = static_cast<float>(slot.previewHeight) * previewScale;
                const Rect drawRect = {
                    std::round(previewRect->x + (previewRect->width - drawWidth) * 0.5f),
                    std::round(previewRect->y + (previewRect->height - drawHeight) * 0.5f),
                    std::round(drawWidth),
                    std::round(drawHeight)
                };
                drawPixelsBgra(
                    "__load_game_preview__",
                    slot.previewWidth,
                    slot.previewHeight,
                    slot.previewPixelsBgra,
                    drawRect);
            }
        }
    }

    if (thumbRect && scrollUpRect && scrollDownRect)
    {
        float thumbY = thumbRect->y;

        if (m_slots.size() > VisibleSlotCount)
        {
            const float trackTop = scrollUpRect->y + scrollUpRect->height;
            const float trackBottom = scrollDownRect->y;
            const float travel = std::max(0.0f, trackBottom - trackTop - thumbRect->height);
            const float maxOffset = static_cast<float>(m_slots.size() - VisibleSlotCount);
            const float t = maxOffset > 0.0f ? static_cast<float>(m_scrollOffset) / maxOffset : 0.0f;
            thumbY = std::round(trackTop + travel * t);
        }

        Rect drawRect = *thumbRect;
        drawRect.y = thumbY;
        drawTexture(resolveAssetName("LoadGameScrollThumb", "but_thum"), drawRect);
    }

    bool shouldLoadSelectedSlot = false;

    for (size_t row = 0; row < VisibleSlotCount; ++row)
    {
        const size_t slotIndex = m_scrollOffset + row;
        const std::string layoutId = "LoadGameSlotRow" + std::to_string(row);
        const std::optional<Rect> rowRect = resolveLayoutRect(layoutId, 205.0f, 22.0f);

        if (!rowRect)
        {
            continue;
        }

        if (hitTest(*rowRect) && leftMouseJustReleased() && slotIndex < m_slots.size())
        {
            m_selectedIndex = slotIndex;
            const uint64_t nowTicks = SDL_GetTicks();

            if (m_lastClickedSlotIndex == slotIndex
                && nowTicks - m_lastClickedSlotTicks <= SlotDoubleClickWindowMs)
            {
                shouldLoadSelectedSlot = true;
            }

            m_lastClickedSlotIndex = slotIndex;
            m_lastClickedSlotTicks = nowTicks;
        }

        if (slotIndex >= m_slots.size() || m_slots[slotIndex].fileLabel.empty())
        {
            continue;
        }

        const uint32_t rowColor = slotIndex == std::min(m_selectedIndex, m_slots.size() - 1)
            ? SelectedRowColorAbgr
            : WhiteColorAbgr;
        drawLayoutText(layoutId, m_slots[slotIndex].fileLabel, rowColor);
    }

    if (!m_errorText.empty())
    {
        drawLayoutText("LoadGameSelectedName", m_errorText, 0xff4040ffu);
    }
    else if (!m_slots.empty())
    {
        const SaveSlotSummary &slot = m_slots[std::min(m_selectedIndex, m_slots.size() - 1)];
        drawLayoutText("LoadGameSelectedName", slot.locationName);
        drawLayoutText("LoadGamePreviewLine1", slot.weekdayClockText);
        drawLayoutText("LoadGamePreviewLine2", slot.dateText);
    }
    else
    {
        drawLayoutText("LoadGameSlotRow0", m_emptySlotText);
    }

    if (confirmButton.clicked)
    {
        shouldLoadSelectedSlot = true;
    }

    if (cancelButton.clicked)
    {
        cancel();
        return;
    }

    if (shouldLoadSelectedSlot)
    {
        tryLoadSelectedSlot();
        return;
    }
}

void LoadGameScreen::refreshSaveSlots()
{
    m_slots.clear();
    m_errorText.clear();
    m_slotsLoaded = true;
    invalidateCachedSaveSlots();
    const UiTextTable *pUiText = m_uiText ? &*m_uiText : nullptr;
    std::error_code directoryError;
    std::filesystem::create_directories("saves", directoryError);
    if (directoryError)
    {
        m_errorText = "无法访问存档目录";
        return;
    }

    const std::filesystem::directory_iterator end;
    std::filesystem::directory_iterator entries("saves", directoryError);
    while (!directoryError && entries != end)
    {
        const std::filesystem::directory_entry entry = *entries;
        std::error_code entryError;
        const bool regularFile = entry.is_regular_file(entryError);
        entries.increment(directoryError);
        if (entryError)
        {
            directoryError = entryError;
            break;
        }
        if (!regularFile || toLowerCopy(entry.path().extension().string()) != ".oysav")
        {
            continue;
        }

        std::string error;
        const std::optional<GameSaveData> saveData = loadGameDataFromPath(entry.path(), error);

        if (!saveData)
        {
            m_errorText = "部分存档无法读取，原文件已保留";
            continue;
        }

        SaveSlotSummary slot = {};
        slot.path = entry.path();
        slot.hasCustomName = !saveData->saveName.empty();
        slot.fileLabel = slot.hasCustomName ? saveData->saveName : friendlySaveFileLabel(entry.path(), pUiText);
        slot.locationName = saveData->mapFileName;
        slot.savedGameMinutes = saveData->savedGameMinutes;
        slot.populated = true;
        const SaveSlotTimeText timeText = formatSaveSlotTime(slot.savedGameMinutes, pUiText);
        slot.weekdayClockText = timeText.clock;
        slot.dateText = timeText.date;

        if (m_pGameData != nullptr)
        {
            for (const MapStatsEntry &entryMap : m_pGameData->mapEntries())
            {
                if (toLowerCopy(entryMap.fileName) == toLowerCopy(saveData->mapFileName))
                {
                    slot.locationName = entryMap.name;
                    break;
                }
            }
        }

        if (!saveData->previewBmp.empty())
        {
            decodeBmpBytesToBgra(
                saveData->previewBmp,
                slot.previewWidth,
                slot.previewHeight,
                slot.previewPixelsBgra);
        }

        m_slots.push_back(std::move(slot));
    }
    if (directoryError)
    {
        m_slots.clear();
        m_errorText = "无法读取存档目录";
        return;
    }

    std::sort(
        m_slots.begin(),
        m_slots.end(),
        [](const SaveSlotSummary &left, const SaveSlotSummary &right)
        {
            return compareSavePathsForDisplay(left.path, right.path);
        });

    if (m_selectedIndex >= m_slots.size())
    {
        m_selectedIndex = 0;
    }

    m_scrollOffset = 0;
    m_lastClickedSlotIndex = static_cast<size_t>(-1);
    m_lastClickedSlotTicks = 0;
    if (m_errorText.empty())
    {
        s_cachedSlots = m_slots;
        s_cachedSlotsValid = true;
    }
}

void LoadGameScreen::loadCachedSaveSlots()
{
    m_slots = s_cachedSlots;
    const UiTextTable *pUiText = m_uiText ? &*m_uiText : nullptr;
    for (SaveSlotSummary &slot : m_slots)
    {
        if (!slot.hasCustomName)
        {
            slot.fileLabel = friendlySaveFileLabel(slot.path, pUiText);
        }
        const SaveSlotTimeText timeText = formatSaveSlotTime(slot.savedGameMinutes, pUiText);
        slot.weekdayClockText = timeText.clock;
        slot.dateText = timeText.date;
    }
    m_errorText.clear();

    if (m_selectedIndex >= m_slots.size())
    {
        m_selectedIndex = 0;
    }

    m_scrollOffset = 0;
    m_lastClickedSlotIndex = static_cast<size_t>(-1);
    m_lastClickedSlotTicks = 0;
    m_slotsLoaded = true;
}

bool LoadGameScreen::tryLoadSelectedSlot()
{
    if (m_slots.empty() || m_selectedIndex >= m_slots.size() || !m_loadAction)
    {
        return false;
    }

    if (!m_loadAction(m_slots[m_selectedIndex].path))
    {
        m_errorText = "所选存档无法读取，原文件已保留";
        invalidateCachedSaveSlots();
        return false;
    }
    return true;
}

void LoadGameScreen::cancel()
{
    if (m_cancelAction)
    {
        m_cancelAction();
    }
}

bool LoadGameScreen::ensureLayoutLoaded()
{
    if (m_layoutLoaded)
    {
        return true;
    }

    m_layoutManager.clear();
    m_layoutLoaded = m_layoutManager.loadLayoutFile(assetFileSystem(), LoadGameLayoutPath);
    return m_layoutLoaded;
}

std::optional<MenuScreenBase::Rect> LoadGameScreen::resolveLayoutRect(
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

    return toRect(*resolved);
}

MenuScreenBase::ButtonVisualSet LoadGameScreen::resolveButtonVisuals(
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

std::string LoadGameScreen::resolveAssetName(const std::string &layoutId, const std::string &fallbackAssetName) const
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

void LoadGameScreen::drawLayoutText(
    const std::string &layoutId,
    const std::string &text,
    uint32_t colorAbgrOverride) const
{
    if (!m_layoutLoaded || text.empty())
    {
        return;
    }

    const UiLayoutManager::LayoutElement *pLayout = m_layoutManager.findElement(layoutId);

    if (pLayout == nullptr)
    {
        return;
    }

    std::unordered_set<std::string> visited;
    const std::optional<ResolvedLayoutElement> resolved = resolveLayoutElementRecursive(
        m_layoutManager,
        layoutId,
        frameWidth(),
        frameHeight(),
        0.0f,
        0.0f,
        visited);

    if (!resolved.has_value())
    {
        return;
    }

    const std::string fontName = pLayout->fontName.empty() ? "Create" : pLayout->fontName;
    const uint32_t color = colorAbgrOverride != 0 ? colorAbgrOverride : pLayout->textColorAbgr;
    const float fontScale = resolved->scale * std::max(0.1f, pLayout->textScale);
    const float padX = pLayout->textPadX * resolved->scale;
    const float padY = pLayout->textPadY * resolved->scale;
    float drawX = resolved->x + padX;
    float drawY = resolved->y + padY;
    LoadGameScreen *pMutableScreen = const_cast<LoadGameScreen *>(this);
    std::string displayText = text;
    if (layoutId.starts_with("LoadGameSlotRow"))
    {
        const float availableWidth = std::max(
            0.0f, resolved->width - 2.0f * std::abs(padX) - 4.0f - std::max(1.0f, fontScale));
        if (pMutableScreen->measureTextWidth(fontName, displayText, fontScale) > availableWidth)
        {
            const std::string ellipsis = "...";
            while (!displayText.empty()
                && pMutableScreen->measureTextWidth(fontName, displayText + ellipsis, fontScale) > availableWidth)
            {
                if (!eraseLastUtf8Character(displayText))
                {
                    displayText.clear();
                }
            }
            displayText = pMutableScreen->measureTextWidth(fontName, ellipsis, fontScale) <= availableWidth
                ? displayText + ellipsis : std::string();
        }
    }
    const float textWidth = pMutableScreen->measureTextWidth(fontName, displayText, fontScale);
    const float textHeight = static_cast<float>(pMutableScreen->fontHeight(fontName)) * fontScale;

    if (pLayout->textAlignX == UiLayoutManager::TextAlignX::Center)
    {
        drawX = resolved->x + (resolved->width - textWidth) * 0.5f + padX;
    }
    else if (pLayout->textAlignX == UiLayoutManager::TextAlignX::Right)
    {
        drawX = resolved->x + resolved->width - textWidth - padX;
    }

    if (pLayout->textAlignY == UiLayoutManager::TextAlignY::Middle)
    {
        drawY = resolved->y + (resolved->height - textHeight) * 0.5f + padY;
    }
    else if (pLayout->textAlignY == UiLayoutManager::TextAlignY::Bottom)
    {
        drawY = resolved->y + resolved->height - textHeight - padY;
    }

    pMutableScreen->drawText(fontName, displayText, drawX, drawY, color, fontScale);
}
}
