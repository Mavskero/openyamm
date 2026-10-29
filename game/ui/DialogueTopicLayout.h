#pragma once

#include "game/ui/GameplayOverlayTypes.h"
#include "game/ui/UiLayoutManager.h"

#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;

struct DialogueTopicTextLayout
{
    GameplayHudFontHandle font = {};
    std::vector<std::string> lines;
    float fontScale = 1.0f;
    float lineHeight = 0.0f;
    float lineAdvance = 0.0f;
    float height = 0.0f;
};

struct DialogueTopicRow
{
    size_t actionIndex = 0;
    GameplayResolvedHudLayoutElement rect = {};
    GameplayResolvedHudLayoutElement textRect = {};
    DialogueTopicTextLayout text;
};

struct DialogueTopicPage
{
    std::vector<DialogueTopicRow> rows;
};

struct DialogueTopicLayout
{
    GameplayResolvedHudLayoutElement viewport = {};
    GameplayResolvedHudLayoutElement previousButton = {};
    GameplayResolvedHudLayoutElement nextButton = {};
    GameplayResolvedHudLayoutElement pageLabel = {};
    std::vector<DialogueTopicPage> pages;
    size_t pageIndex = 0;
};

std::optional<DialogueTopicTextLayout> layoutDialogueTopicText(
    GameplayScreenRuntime &context,
    const UiLayoutManager::LayoutElement &style,
    const GameplayResolvedHudLayoutElement &rect,
    const std::string &text);

std::optional<DialogueTopicLayout> resolveDialogueTopicLayout(
    GameplayScreenRuntime &context,
    int width,
    int height);
}
