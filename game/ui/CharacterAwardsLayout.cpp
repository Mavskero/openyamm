#include "game/ui/CharacterAwardsLayout.h"

#include "game/gameplay/AwardRuntime.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/ui/GameplayHudCommon.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace OpenYAMM::Game
{
size_t CharacterAwardsLayout::scrollOffsetAt(float pointerY, float thumbGrabOffset) const
{
    if (maximumScrollOffset == 0 || thumbTravel <= 0.0f)
    {
        return 0;
    }

    const float fraction = std::clamp((pointerY - thumbTrackTop - thumbGrabOffset) / thumbTravel, 0.0f, 1.0f);
    return static_cast<size_t>(std::round(fraction * static_cast<float>(maximumScrollOffset)));
}

std::optional<CharacterAwardsLayout> layoutCharacterAwards(
    GameplayScreenRuntime &context,
    const Character *pCharacter,
    const Party *pParty,
    int width,
    int height)
{
    const auto resolve = [&context, width, height](const char *pId)
        -> std::optional<GameplayResolvedHudLayoutElement>
    {
        const UiLayoutManager::LayoutElement *pLayout = context.findHudLayoutElement(pId);
        return pLayout != nullptr
            ? context.resolveHudLayoutElement(pId, width, height, pLayout->width, pLayout->height)
            : std::nullopt;
    };
    const UiLayoutManager::LayoutElement *pListLayout = context.findHudLayoutElement("CharacterAwardsList");
    const std::optional<GameplayResolvedHudLayoutElement> listRect = resolve("CharacterAwardsList");
    if (pListLayout == nullptr || !listRect)
    {
        return std::nullopt;
    }

    const std::optional<GameplayHudFontHandle> font = context.findHudFont(pListLayout->fontName);
    if (!font)
    {
        return std::nullopt;
    }

    CharacterAwardsLayout result;
    result.font = *font;
    result.listRect = *listRect;
    result.fontScale = listRect->scale * std::max(0.1f, pListLayout->textScale);
    result.fontScale = result.fontScale >= 1.0f
        ? GameplayHudCommon::snappedHudFontScale(result.fontScale)
        : std::max(0.5f, result.fontScale);
    result.awardGap = 8.0f * listRect->scale;
    result.textX = std::round(listRect->x + pListLayout->textPadX * listRect->scale);
    result.textY = std::round(listRect->y + pListLayout->textPadY * listRect->scale);
    const float lineHeight = static_cast<float>(font->fontHeight) * result.fontScale;
    const float wrapWidth = std::max(1.0f, listRect->width - pListLayout->textPadX * listRect->scale);
    const AwardTable *pAwardTable = context.awardTable();
    if (pAwardTable != nullptr && pCharacter != nullptr && pParty != nullptr)
    {
        const std::vector<VisibleAward> awards = buildVisibleAwards(
            *pAwardTable, *pCharacter, *pParty,
            [&context](uint32_t autonoteId) { return context.isAutonoteUnlocked(autonoteId); });
        result.rows.reserve(awards.size());
        for (const VisibleAward &award : awards)
        {
            CharacterAwardLayoutRow row;
            row.lines = context.wrapHudTextToWidth(*font, award.text, wrapWidth / result.fontScale);
            if (row.lines.empty())
            {
                row.lines.push_back("");
            }
            row.height = lineHeight * static_cast<float>(row.lines.size());
            row.priority = award.priority;
            result.rows.push_back(std::move(row));
        }
    }

    float suffixHeight = 0.0f;
    const float availableHeight = std::max(0.0f, listRect->y + listRect->height - result.textY);
    for (size_t index = result.rows.size(); index > 0; --index)
    {
        const float rowHeight = result.rows[index - 1].height;
        const float gap = index < result.rows.size() ? result.awardGap : 0.0f;
        if (suffixHeight + gap + rowHeight > availableHeight)
        {
            result.maximumScrollOffset = std::min(index, result.rows.size() - 1);
            break;
        }
        suffixHeight += gap + rowHeight;
    }

    GameplayUiController::CharacterScreenState &screen = context.characterScreen();
    screen.awardScrollOffset = std::min(screen.awardScrollOffset, result.maximumScrollOffset);
    result.trackRect = resolve("CharacterAwardsScrollTrack");
    result.thumbRect = resolve("CharacterAwardsScrollThumb");
    const std::optional<GameplayResolvedHudLayoutElement> downButton = resolve("CharacterAwardsScrollDownButton");
    if (!result.trackRect || !result.thumbRect || !downButton)
    {
        result.trackRect.reset();
        result.thumbRect.reset();
        return result;
    }

    result.thumbTrackTop = result.trackRect->y + 3.0f * result.trackRect->scale;
    result.thumbTravel = std::max(0.0f, downButton->y - result.thumbRect->height - result.thumbTrackTop);
    const float fraction = result.maximumScrollOffset > 0
        ? static_cast<float>(screen.awardScrollOffset) / static_cast<float>(result.maximumScrollOffset)
        : 0.0f;
    result.thumbRect->y = std::round(result.thumbTrackTop + result.thumbTravel * fraction);
    return result;
}
}
