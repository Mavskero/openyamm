#include "game/ui/DialogueTopicLayout.h"

#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/tables/UiTextTable.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/Utf8Text.h"

#if defined(__APPLE__)
#include "engine/AppleText.h"
#endif

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
float textHeight(const DialogueTopicTextLayout &text, size_t lineCount)
{
    return lineCount == 0 ? 0.0f : text.lineHeight + static_cast<float>(lineCount - 1) * text.lineAdvance;
}

std::string contentKey(const EventDialogContent &dialog, uint32_t hostHouseId)
{
    std::string key;
    const auto appendNumber = [&key](uint64_t number)
    {
        key += std::to_string(number) + ";";
    };
    const auto appendText = [&key, &appendNumber](const std::string &text)
    {
        appendNumber(text.size());
        key += text;
    };
    appendNumber(hostHouseId);
    appendNumber(dialog.sourceId);
    appendNumber(dialog.sourceActorIndex.has_value());
    appendNumber(dialog.sourceActorIndex.value_or(0));
    appendNumber(dialog.dialogueNodeId);
    appendNumber(dialog.participantPictureId);
    appendNumber(dialog.isHouseDialog);
    appendText(dialog.houseTitle);
    appendText(dialog.title);
    appendNumber(dialog.lines.size());
    for (const std::string &line : dialog.lines)
    {
        appendText(line);
    }
    appendNumber(dialog.actions.size());
    for (const EventDialogAction &action : dialog.actions)
    {
        appendNumber(static_cast<uint32_t>(action.kind));
        appendNumber(action.id);
        appendNumber(action.secondaryId);
        appendNumber(action.enabled);
        appendText(action.label);
        appendText(action.argument);
        appendText(action.disabledReason);
    }
    return key;
}
}

std::optional<DialogueTopicTextLayout> layoutDialogueTopicText(
    GameplayScreenRuntime &context,
    const UiLayoutManager::LayoutElement &style,
    const GameplayResolvedHudLayoutElement &rect,
    const std::string &text)
{
    const std::optional<GameplayHudFontHandle> font = context.findHudFont(style.fontName);
    if (!font || rect.width <= 0.0f || rect.scale <= 0.0f)
    {
        return std::nullopt;
    }
    DialogueTopicTextLayout result = {};
    result.font = *font;
    result.fontScale = GameplayHudCommon::snappedHudFontScale(rect.scale * std::max(0.1f, style.textScale));
    const float textWidth = rect.width - 2.0f * std::abs(style.textPadX) * rect.scale;
    float wrapWidth = textWidth;
#if defined(__APPLE__)
    if (hasUtf8MultibyteText(text))
    {
        wrapWidth -= 4.0f + std::max(1.0f, result.fontScale);
        const std::optional<std::vector<std::string>> lines =
            Engine::wrapAppleText(text, font->fontHeight * result.fontScale, wrapWidth);
        if (!lines)
        {
            return std::nullopt;
        }
        result.lines = *lines;
    }
    else
#endif
    {
        result.lines = context.wrapHudTextToWidth(*font, text, wrapWidth / result.fontScale);
    }
    if (wrapWidth <= 0.0f)
    {
        return std::nullopt;
    }

    // The bitmap wrapper preserves whole words; split an oversized word without dropping bytes.
    std::vector<std::string> fittedLines;
    for (const std::string &line : result.lines)
    {
        if (context.measureHudTextWidth(*font, line) * result.fontScale <= wrapWidth)
        {
            fittedLines.push_back(line);
            continue;
        }
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
                && context.measureHudTextWidth(*font, fragment + character) * result.fontScale > wrapWidth)
            {
                fittedLines.push_back(std::move(fragment));
                fragment.clear();
            }
            fragment += character;
            offset += byteCount;
        }
        fittedLines.push_back(std::move(fragment));
    }
    result.lines = std::move(fittedLines);
    if (result.lines.empty())
    {
        result.lines.emplace_back();
    }
    result.lineHeight = font->fontHeight * result.fontScale;
#if defined(__APPLE__)
    for (const std::string &line : result.lines)
    {
        if (hasUtf8MultibyteText(line))
        {
            const std::optional<Engine::AppleTextImageSize> imageSize =
                Engine::measureAppleTextImageSize(line, font->fontHeight * result.fontScale);
            if (imageSize)
            {
                result.lineHeight = std::max(
                    result.lineHeight, imageSize->height + std::max(1.0f, result.fontScale));
            }
        }
    }
#endif
    result.lineAdvance = result.lineHeight + 2.0f * rect.scale;
    result.height = textHeight(result, result.lines.size());
    return result;
}

std::optional<DialogueTopicLayout> resolveDialogueTopicLayout(GameplayScreenRuntime &context, int width, int height)
{
    const EventDialogContent &dialog = context.activeEventDialog();
    if (!dialog.isActive || dialog.presentation != EventDialogPresentation::Standard || dialog.actions.empty()
        || width <= 0 || height <= 0)
    {
        return std::nullopt;
    }
    const UiLayoutManager::LayoutElement *pPanel = context.findHudLayoutElement("DialogueEventDialog");
    const UiLayoutManager::LayoutElement *pRow = context.findHudLayoutElement("DialogueTopicRow_1");
    if (pPanel == nullptr || pRow == nullptr)
    {
        return std::nullopt;
    }
    const auto resolve = [&context, width, height](const char *pId)
    {
        const UiLayoutManager::LayoutElement *pStyle = context.findHudLayoutElement(pId);
        return pStyle != nullptr
            ? context.resolveHudLayoutElement(pId, width, height, pStyle->width, pStyle->height)
            : std::optional<GameplayResolvedHudLayoutElement>{};
    };
    const std::optional<GameplayResolvedHudLayoutElement> panel = resolve("DialogueEventDialog");
    const std::optional<GameplayResolvedHudLayoutElement> rowTemplate = resolve("DialogueTopicRow_1");
    if (!panel || !rowTemplate)
    {
        return std::nullopt;
    }
    const float scale = panel->scale;
    const IGameplayWorldRuntime *pWorld = context.worldRuntime();
    const EventRuntimeState *pEvents = pWorld != nullptr ? pWorld->eventRuntimeState() : nullptr;
    const uint32_t hostHouseId = pEvents != nullptr ? pEvents->dialogueState.hostHouseId : 0;
    const HouseEntry *pHouse = hostHouseId != 0 && context.houseTable() != nullptr
        ? context.houseTable()->get(hostHouseId) : nullptr;
    float contentTop = panel->y + 10.0f * scale;
    const UiLayoutManager::LayoutElement *pTitle = context.findHudLayoutElement("DialogueHouseTitle");
    const UiLayoutManager::LayoutElement *pName = context.findHudLayoutElement("DialogueNpcName");
    if (pTitle == nullptr)
    {
        pTitle = pName;
    }
    const float titleWidth = panel->width - 8.0f * scale;
    if (pHouse != nullptr && pTitle != nullptr)
    {
        const std::optional<GameplayResolvedHudLayoutElement> authored = resolve(pTitle->id.c_str());
        GameplayResolvedHudLayoutElement titleRect = authored.value_or(GameplayResolvedHudLayoutElement{
            panel->x + 4.0f * scale, contentTop, titleWidth, 20.0f * scale, scale});
        titleRect.width = titleWidth;
        const std::optional<DialogueTopicTextLayout> title = layoutDialogueTopicText(
            context, *pTitle, titleRect, dialog.houseTitle.empty() ? pHouse->name : dialog.houseTitle);
        contentTop += std::max(titleRect.height, title ? title->height : titleRect.height) + 2.0f * scale;
    }
    const std::optional<GameplayResolvedHudLayoutElement> portrait = resolve("DialogueNpcPortrait");
    const std::optional<GameplayResolvedHudLayoutElement> name = resolve("DialogueNpcName");
    const float portraitY = std::round(std::max(contentTop, portrait ? portrait->y : panel->y + 10.0f * scale));
    contentTop = portraitY + (portrait ? portrait->height : 80.0f * scale);
    if (pName != nullptr && !dialog.title.empty())
    {
        const float nameOffset = name && portrait ? name->y - portrait->y
            : (portrait ? portrait->height : 80.0f * scale) + 2.0f * scale;
        GameplayResolvedHudLayoutElement nameRect = name.value_or(GameplayResolvedHudLayoutElement{
            panel->x + 4.0f * scale, 0.0f, titleWidth, 20.0f * scale, scale});
        nameRect.y = portraitY + nameOffset;
        nameRect.width = titleWidth;
        const std::optional<DialogueTopicTextLayout> text =
            layoutDialogueTopicText(context, *pName, nameRect, dialog.title);
        contentTop = nameRect.y + std::max(nameRect.height, text ? text->height : nameRect.height);
    }
    contentTop += 8.0f * scale;
    const float availableHeight = panel->y + panel->height - 10.0f * scale - contentTop;
    const float gap = 4.0f * scale;
    const float textInset = 2.0f * scale;
    std::vector<DialogueTopicRow> actions;
    float fullHeight = 0.0f;
    for (size_t index = 0; index < dialog.actions.size(); ++index)
    {
        std::string label = dialog.actions[index].label;
        if (!dialog.actions[index].enabled && !dialog.actions[index].disabledReason.empty())
        {
            label += " [" + context.gameplayUiRuntime().uiTextTable().text(UiTextTable::Message::Disabled) + "]";
        }
        GameplayResolvedHudLayoutElement textRect = *rowTemplate;
        textRect.width = std::min(textRect.width, 140.0f * scale) - 4.0f * scale;
        const std::optional<DialogueTopicTextLayout> text = layoutDialogueTopicText(context, *pRow, textRect, label);
        if (!text)
        {
            return std::nullopt;
        }
        DialogueTopicRow row;
        row.actionIndex = index;
        row.rect = *rowTemplate;
        row.rect.height = std::max(rowTemplate->height, text->height + 2.0f * textInset);
        row.text = *text;
        row.textRect = textRect;
        fullHeight += row.rect.height + (actions.empty() ? 0.0f : gap);
        actions.push_back(std::move(row));
    }

    DialogueTopicLayout layout;
    const bool paginated = fullHeight > availableHeight;
    const float footerHeight = paginated ? 28.0f * scale : 0.0f;
    const float pageHeight = availableHeight - footerHeight;
    layout.viewport = {rowTemplate->x, contentTop, rowTemplate->width, pageHeight, scale};
    DialogueTopicPage page;
    float used = 0.0f;
    const auto finishPage = [&layout, &page, &used]()
    {
        if (!page.rows.empty())
        {
            layout.pages.push_back(std::move(page));
            page = {};
            used = 0.0f;
        }
    };
    for (const DialogueTopicRow &action : actions)
    {
        if (pageHeight < action.text.lineHeight + 2.0f * textInset)
        {
            return std::nullopt;
        }
        if (!page.rows.empty() && used + gap + action.rect.height > pageHeight)
        {
            finishPage();
        }
        size_t firstLine = 0;
        while (firstLine < action.text.lines.size())
        {
            const float precedingGap = page.rows.empty() ? 0.0f : gap;
            const float available = pageHeight - used - precedingGap - 2.0f * textInset;
            const size_t capacity = 1 + size_t(std::floor(
                std::max(0.0f, available - action.text.lineHeight) / action.text.lineAdvance));
            const size_t count = std::min(capacity, action.text.lines.size() - firstLine);
            DialogueTopicRow fragment = action;
            fragment.text.lines.assign(action.text.lines.begin() + firstLine,
                action.text.lines.begin() + firstLine + count);
            fragment.text.height = textHeight(fragment.text, count);
            fragment.rect.height = std::max(
                std::min(rowTemplate->height, pageHeight), fragment.text.height + 2.0f * textInset);
            fragment.rect.y = contentTop + used + precedingGap;
            fragment.textRect.x = fragment.rect.x + (fragment.rect.width - fragment.textRect.width) * 0.5f;
            fragment.textRect.y = fragment.rect.y + (fragment.rect.height - fragment.text.height) * 0.5f;
            fragment.textRect.height = fragment.text.height;
            page.rows.push_back(std::move(fragment));
            used += precedingGap + page.rows.back().rect.height;
            firstLine += count;
            if (firstLine < action.text.lines.size())
            {
                finishPage();
            }
        }
    }
    finishPage();
    if (layout.pages.empty())
    {
        return std::nullopt;
    }
    for (DialogueTopicPage &topicPage : layout.pages)
    {
        const DialogueTopicRow &last = topicPage.rows.back();
        const float occupied = last.rect.y + last.rect.height - contentTop;
        const float centerOffset = std::max(0.0f, (pageHeight - occupied) * 0.5f - 8.0f * scale);
        for (DialogueTopicRow &row : topicPage.rows)
        {
            row.rect.y += centerOffset;
            row.textRect.y += centerOffset;
        }
    }
    if (paginated)
    {
        const float footerY = contentTop + pageHeight + 4.0f * scale;
        layout.previousButton = {rowTemplate->x, footerY, 32.0f * scale, 24.0f * scale, scale};
        layout.nextButton = {rowTemplate->x + rowTemplate->width - 32.0f * scale,
            footerY, 32.0f * scale, 24.0f * scale, scale};
        layout.pageLabel = {layout.previousButton.x + layout.previousButton.width, footerY,
            rowTemplate->width - 64.0f * scale, 24.0f * scale, scale};
    }
    GameplayOverlayInteractionState &state = context.interactionState();
    const std::string key = contentKey(dialog, hostHouseId);
    if (state.dialogueTopicContentKey != key)
    {
        state.dialogueTopicContentKey = key;
        state.dialogueTopicPageIndex = 0;
        state.dialogueTopicSelectedIndex = context.eventDialogSelectionIndex();
    }
    else if (state.dialogueTopicSelectedIndex != context.eventDialogSelectionIndex())
    {
        for (size_t pageIndex = 0; pageIndex < layout.pages.size(); ++pageIndex)
        {
            if (std::any_of(layout.pages[pageIndex].rows.begin(), layout.pages[pageIndex].rows.end(),
                    [&context](const DialogueTopicRow &row)
                    { return row.actionIndex == context.eventDialogSelectionIndex(); }))
            {
                state.dialogueTopicPageIndex = pageIndex;
                break;
            }
        }
        state.dialogueTopicSelectedIndex = context.eventDialogSelectionIndex();
    }
    state.dialogueTopicPageIndex = std::min(state.dialogueTopicPageIndex, layout.pages.size() - 1);
    layout.pageIndex = state.dialogueTopicPageIndex;
    return layout;
}
}
