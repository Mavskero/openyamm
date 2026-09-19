#include "game/ui/CharacterSkillsLayout.h"

#include "game/gameplay/GameplayScreenRuntime.h"

#include <algorithm>
#include <optional>

namespace OpenYAMM::Game
{
bool CharacterSkillLayout::placeVisible(GameplayResolvedHudLayoutElement &rect) const
{
    rect.y -= scrollOffset * viewport.scale;
    return viewport.height > 0 && rect.y >= viewport.y - 0.01f
        && rect.y + rect.height <= viewport.y + viewport.height + 0.01f;
}

bool CharacterSkillLayout::contains(float x, float y) const
{
    return GameplayHudCommon::isPointerInsideResolvedElement(viewport, x, y);
}

bool isCharacterSkillContentHeader(std::string_view normalizedId)
{
    return normalizedId.starts_with("characterskills") && normalizedId.ends_with("header");
}

CharacterSkillLayout layoutCharacterSkillRows(
    GameplayScreenRuntime &context,
    int width,
    int height,
    float naturalRowHeight,
    const std::array<size_t, 4> &rowCounts)
{
    constexpr std::array<const char *, 4> RegionIds = {
        "CharacterSkillsWeaponsListRegion", "CharacterSkillsMagicListRegion",
        "CharacterSkillsArmorListRegion", "CharacterSkillsMiscListRegion"
    };
    const auto applyHeights = [&](float rowHeight)
    {
        context.clearHudLayoutRuntimeHeightOverrides();
        for (size_t index = 0; index < RegionIds.size(); ++index)
        {
            context.setHudLayoutRuntimeHeightOverride(
                RegionIds[index], rowHeight * static_cast<float>(std::max<size_t>(1, rowCounts[index])));
        }
    };
    const auto resolve = [&](const char *pId) -> std::optional<GameplayResolvedHudLayoutElement>
    {
        const UiLayoutManager::LayoutElement *pLayout = context.findHudLayoutElement(pId);
        if (pLayout == nullptr)
        {
            return std::nullopt;
        }
        return context.resolveHudLayoutElement(pId, width, height, pLayout->width, pLayout->height);
    };

    CharacterSkillLayout result;
    result.rowHeight = std::max(18.0f, naturalRowHeight);
    applyHeights(result.rowHeight);
    const std::optional<GameplayResolvedHudLayoutElement> page = resolve("CharacterSkillsPage");
    const std::optional<GameplayResolvedHudLayoutElement> button = resolve("CharacterSkillsButton");
    const std::optional<GameplayResolvedHudLayoutElement> header = resolve("CharacterSkillsWeaponsHeader");
    if (!page || !button || !header || page->scale <= 0)
    {
        return result;
    }

    const float bottom = std::min(page->y + page->height, button->y) - 2.0f * page->scale;
    result.viewport = {page->x, header->y, page->width, std::max(0.0f, bottom - header->y), page->scale};
    float contentBottom = bottom;
    for (const char *pId : RegionIds)
    {
        const std::optional<GameplayResolvedHudLayoutElement> region = resolve(pId);
        if (region)
        {
            contentBottom = std::max(contentBottom, region->y + region->height);
        }
    }
    result.maximumScroll = (contentBottom - bottom) / page->scale;
    GameplayUiController::CharacterScreenState &screen = context.characterScreen();
    screen.skillScrollOffset = std::clamp(screen.skillScrollOffset, 0.0f, result.maximumScroll);
    result.scrollOffset = screen.skillScrollOffset;
    return result;
}
}
