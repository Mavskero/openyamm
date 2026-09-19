#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "game/app/GameApplication.h"
#include "game/app/GameSettings.h"
#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/gameplay/HouseServiceRuntime.h"
#include "game/gameplay/ReputationRuntime.h"
#include "game/items/PriceCalculator.h"
#include "game/maps/SaveGame.h"
#include "game/tables/HouseTable.h"
#include "game/ui/GameplayHudCommon.h"
#include "game/ui/SpellbookUiLayout.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
enum class Page
{
    Temple,
    Npc,
    Shop,
    Tavern,
    Training,
    Transport,
    Character,
    Portal,
};

struct State
{
    unsigned phase = 0;
    uint32_t phaseFrame = 0;
    unsigned closeClicks = 0;
    bool complete = false;
    int goldBefore = 0;
    int foodBefore = 0;
    uint32_t levelBefore = 0;
    size_t inventoryBefore = 0;
};

struct PendingCapture
{
    uint32_t requestedFrame = 0;
};

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

bool isPhysicalInputEvent(const SDL_Event &event)
{
    return (event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_KEYMAP_CHANGED)
        || (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
        || (event.type >= SDL_EVENT_FINGER_DOWN && event.type <= SDL_EVENT_FINGER_CANCELED);
}

std::string pageName(Page page)
{
    switch (page)
    {
    case Page::Temple: return "temple";
    case Page::Npc: return "npc";
    case Page::Shop: return "shop";
    case Page::Tavern: return "tavern";
    case Page::Training: return "training";
    case Page::Transport: return "transport";
    case Page::Character: return "character";
    case Page::Portal: return "portal";
    }
    return "unknown";
}

std::set<std::string> expectedCaptures(Page page)
{
    switch (page)
    {
    case Page::Temple: return {"temple-menu", "temple-healed"};
    case Page::Npc: return {"npc-response"};
    case Page::Shop: return {"shop-menu", "shop-stock", "shop-after-buy", "shop-services"};
    case Page::Tavern: return {"tavern-menu", "tavern-after-food"};
    case Page::Training: return {"training-menu", "training-after-level"};
    case Page::Transport: return {"transport-menu", "transport-rejected"};
    case Page::Character: return {"character-skills"};
    case Page::Portal: return {"dimension-door-preview"};
    }
    return {};
}
}

struct ApplicationCaptureTestAccess
{
    static void advance(State &state, unsigned phase)
    {
        state.phase = phase;
        state.phaseFrame = 0;
    }

    static void pressAction(
        GameSession &session,
        int width,
        int height,
        KeyboardAction action,
        float delta = 0.0f)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.actions[keyboardActionIndex(action)] = {true, true, false};
        session.updateGameplay(input, delta);
        input.actions[keyboardActionIndex(action)] = {false, false, true};
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
    }

    static void clickPoint(GameSession &session, int width, int height, float x, float y)
    {
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = x;
        input.pointerY = y;
        input.leftMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.leftMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
        session.updateGameplay({}, 0.0f);
        std::cout << "Pointer click: " << x << ',' << y << '\n';
    }

    static GameplayScreenRuntime::ResolvedHudLayoutElement resolveLayout(
        GameplayScreenRuntime &runtime,
        const std::string &id,
        int width,
        int height)
    {
        const auto *pLayout = runtime.findHudLayoutElement(id);
        require(pLayout != nullptr && pLayout->visible, "Visible layout is absent: " + id);
        const auto resolved = runtime.resolveHudLayoutElement(id, width, height, pLayout->width, pLayout->height);
        require(resolved.has_value(), "Layout cannot be resolved: " + id);
        return *resolved;
    }

    static void clickLayout(
        GameSession &session,
        int width,
        int height,
        const std::string &id)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto resolved = resolveLayout(runtime, id, width, height);
        clickPoint(session, width, height,
            resolved.x + resolved.width * 0.5f,
            resolved.y + resolved.height * 0.5f);
        std::cout << "Layout click: " << id << '\n';
    }

    static size_t actionIndex(
        const GameplayScreenRuntime &runtime,
        EventDialogActionKind kind,
        uint32_t id)
    {
        const EventDialogContent &dialog = runtime.activeEventDialog();
        require(dialog.isActive, "Expected active dialogue");
        const auto action = std::find_if(dialog.actions.begin(), dialog.actions.end(),
            [&](const EventDialogAction &candidate) { return candidate.kind == kind && candidate.id == id; });
        if (action == dialog.actions.end())
        {
            for (const EventDialogAction &candidate : dialog.actions)
            {
                std::cout << "Available action: kind=" << int(candidate.kind)
                          << " id=" << candidate.id << " label=" << candidate.label << '\n';
            }
            throw std::runtime_error("Required dialogue action is absent: " + std::to_string(id));
        }
        require(action->enabled, "Required dialogue action is disabled: " + action->label);
        return size_t(action - dialog.actions.begin());
    }

    static std::array<float, 2> residentActionPoint(
        GameplayScreenRuntime &runtime,
        int width,
        int height,
        size_t index)
    {
        const auto eventDialog = resolveLayout(runtime, "DialogueEventDialog", width, height);
        const float scale = eventDialog.scale;
        const float paddingX = 10.0f * scale;
        const float paddingY = 10.0f * scale;
        const float portraitSize = 80.0f * scale;
        const float sectionGap = 8.0f * scale;
        const float nameHeight = 20.0f * scale;
        const size_t count = runtime.activeEventDialog().actions.size();
        const bool twoColumns = count > 3;
        const size_t column = twoColumns ? index % 2 : 0;
        const size_t row = twoColumns ? index / 2 : index;
        const float innerWidth = eventDialog.width - paddingX * 2.0f;
        const float columnWidth = twoColumns ? innerWidth / 2.0f : innerWidth;
        const float columnX = eventDialog.x + paddingX + float(column) * columnWidth;
        const float contentY = eventDialog.y + paddingY + 28.0f * scale;
        const float portraitX = std::round(columnX + (columnWidth - portraitSize) * 0.5f);
        const float portraitY = std::round(
            twoColumns ? contentY + float(row) * (portraitSize + nameHeight + sectionGap)
                       : contentY + float(index) * (portraitSize + nameHeight + sectionGap));
        return {portraitX + portraitSize * 0.5f, portraitY + portraitSize * 0.5f};
    }

    static std::array<float, 2> topicActionPoint(
        GameplayScreenRuntime &runtime,
        int width,
        int height,
        size_t index)
    {
        const auto eventDialog = resolveLayout(runtime, "DialogueEventDialog", width, height);
        const auto topicRow = resolveLayout(runtime, "DialogueTopicRow_1", width, height);
        const auto *pTopicLayout = runtime.findHudLayoutElement("DialogueTopicRow_1");
        require(pTopicLayout != nullptr, "Dialogue topic row layout is absent");
        const float scale = eventDialog.scale;
        const float panelPaddingY = 10.0f * scale;
        const float sectionGap = 8.0f * scale;
        const auto portrait = resolveLayout(runtime, "DialogueNpcPortrait", width, height);
        const auto name = resolveLayout(runtime, "DialogueNpcName", width, height);
        const auto title = resolveLayout(runtime, "DialogueHouseTitle", width, height);
        float contentY = eventDialog.y + panelPaddingY + title.height + 2.0f * scale;
        contentY = std::max(contentY, portrait.y);
        contentY = portrait.y + (name.y - portrait.y) + name.height + sectionGap;

        const auto font = runtime.findHudFont(pTopicLayout->fontName);
        const float fontScale = GameplayHudCommon::snappedHudFontScale(topicRow.scale);
        const float lineHeight = font ? float(font->fontHeight) * fontScale : 20.0f * scale;
        const float wrapWidth = 140.0f * topicRow.scale;
        const float textWidthScaled = std::max(0.0f,
            std::min(topicRow.width, wrapWidth)
            - std::abs(pTopicLayout->textPadX * fontScale) * 2.0f
            - 4.0f * fontScale);
        const float textWidth = std::max(0.0f, textWidthScaled / std::max(1.0f, fontScale));
        const float rowGap = 4.0f * scale;
        const size_t visibleCount = std::min<size_t>(runtime.activeEventDialog().actions.size(), 5);
        require(index < visibleCount, "Required dialogue action is outside the five visible topic rows");
        std::vector<float> rowHeights;
        rowHeights.reserve(visibleCount);
        for (size_t i = 0; i < visibleCount; ++i)
        {
            std::string label = runtime.activeEventDialog().actions[i].label;
            if (!runtime.activeEventDialog().actions[i].enabled
                && !runtime.activeEventDialog().actions[i].disabledReason.empty())
            {
                label += " [disabled]";
            }
            std::vector<std::string> lines = font
                ? runtime.wrapHudTextToWidth(*font, label, textWidth)
                : std::vector<std::string>{label};
            if (lines.empty()) lines.push_back(label);
            rowHeights.push_back(std::max(topicRow.height, float(lines.size()) * lineHeight));
        }
        float totalHeight = 0.0f;
        for (size_t i = 0; i < rowHeights.size(); ++i)
        {
            totalHeight += rowHeights[i];
            if (i + 1 < rowHeights.size()) totalHeight += rowGap;
        }
        const float availableHeight = eventDialog.y + eventDialog.height - panelPaddingY - contentY;
        float rowY = contentY + std::max(0.0f, (availableHeight - totalHeight) * 0.5f) - 8.0f * scale;
        for (size_t i = 0; i < index; ++i) rowY += rowHeights[i] + rowGap;
        return {topicRow.x + topicRow.width * 0.5f, rowY + rowHeights[index] * 0.5f};
    }

    static void clickAction(
        GameSession &session,
        int width,
        int height,
        EventDialogActionKind kind,
        uint32_t id)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const size_t index = actionIndex(runtime, kind, id);
        const bool residentSelection = std::all_of(
            runtime.activeEventDialog().actions.begin(), runtime.activeEventDialog().actions.end(),
            [](const EventDialogAction &action)
            {
                return action.kind == EventDialogActionKind::HouseResident
                    || action.kind == EventDialogActionKind::HouseProprietor;
            });
        const auto point = residentSelection
            ? residentActionPoint(runtime, width, height, index)
            : topicActionPoint(runtime, width, height, index);
        const std::string label = runtime.activeEventDialog().actions[index].label;
        clickPoint(session, width, height, point[0], point[1]);
        std::cout << "Pointer action: kind=" << int(kind) << " id=" << id
                  << " index=" << index << " label=" << label << '\n';
    }

    static void clickClose(GameSession &session, int width, int height)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto close = resolveLayout(runtime, "DialogueGoodbyeButton", width, height);
        clickPoint(session, width, height, close.x + close.width * 0.5f, close.y + close.height * 0.5f);
        std::cout << "Pointer close: dialogue=" << runtime.activeEventDialog().isActive
                  << " shop=" << runtime.houseShopOverlay().active << '\n';
    }

    static void clickPortraitToOpenCharacter(GameSession &session, int width, int height, size_t index)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto portrait = runtime.resolvePartyPortraitRect(width, height, index);
        require(portrait.has_value(), "Character comparison portrait is absent");
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = portrait->x + portrait->width * 0.5f;
        input.pointerY = portrait->y + portrait->height * 0.5f;
        input.rightMouseButton = {true, true, false};
        session.updateGameplay(input, 0.0f);
        input.rightMouseButton.pressed = false;
        for (unsigned click = 0; click < 2; ++click)
        {
            input.leftMouseButton = {true, true, false};
            session.updateGameplay(input, 0.0f);
            input.leftMouseButton = {false, false, true};
            session.updateGameplay(input, 0.0f);
        }
        input.leftMouseButton = {};
        input.rightMouseButton = {false, false, true};
        session.updateGameplay(input, 0.0f);
        require(runtime.characterScreen().open && runtime.selectedCharacterScreenSourceIndex() == index,
            "Normal portrait pointer input did not open the requested character");
    }

    static std::array<float, 2> destinationPoint(
        GameSession &session,
        int width,
        int height,
        const std::string &id)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        const auto destination = std::find_if(
            runtime.townPortalDestinations().begin(), runtime.townPortalDestinations().end(),
            [&](const GameplayTownPortalDestination &candidate) { return candidate.id == id; });
        require(destination != runtime.townPortalDestinations().end(), "Dimension Door destination is absent: " + id);
        const auto background = resolveLayout(runtime, "TownPortalBackground", width, height);
        const float scale = background.width / 640.0f;
        require(id == "TPGlobal:4", "Only the audited Enroth preview is supported");
        return {background.x + 310.0f * scale, background.y + 24.0f * scale};
    }

    static void hoverDestination(
        GameApplication &game,
        int width,
        int height,
        const std::string &id)
    {
        const auto point = destinationPoint(game.m_gameSession, width, height, id);
        int windowCount = 0;
        SDL_Window **pWindows = SDL_GetWindows(&windowCount);
        SDL_Window *pWindow = pWindows && windowCount == 1 ? pWindows[0] : nullptr;
        SDL_free(pWindows);
        require(pWindow != nullptr, "Destination hover needs the single capture window");
        int pointWidth = 0;
        int pointHeight = 0;
        require(SDL_GetWindowSize(pWindow, &pointWidth, &pointHeight)
                && pointWidth > 0 && pointHeight > 0 && width > 0 && height > 0,
            "Destination hover cannot resolve drawable-to-window scale");
        const float windowX = point[0] * float(pointWidth) / float(width);
        const float windowY = point[1] * float(pointHeight) / float(height);
        SDL_WarpMouseInWindow(pWindow, windowX, windowY);
        SDL_Event event = {};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = SDL_GetWindowID(pWindow);
        event.motion.x = windowX;
        event.motion.y = windowY;
        game.m_gameInputSystem.handleSdlEvent(event);
        GameplayInputFrame input;
        input.screenWidth = width;
        input.screenHeight = height;
        input.pointerX = point[0];
        input.pointerY = point[1];
        game.m_gameSession.updateGameplay(input, 0.0f);
        std::cout << "Destination hover: " << id << " drawable=" << point[0] << ',' << point[1]
                  << " window=" << windowX << ',' << windowY << '\n';
    }

    static bool clickAffordableShopItem(
        GameSession &session,
        int width,
        int height)
    {
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        Party *pParty = runtime.party();
        IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
        const HouseEntry *pHouse = runtime.houseTable() ? runtime.houseTable()->get(1) : nullptr;
        const Party::HouseStockState *pStock = pParty ? pParty->houseStockState(1) : nullptr;
        require(pParty && pWorld && pHouse && pStock && runtime.itemTable()
                && runtime.standardItemEnchantTable() && runtime.specialItemEnchantTable(),
            "Shop comparison lacks the original stock dependencies");
        const int reputation = effectivePartyReputation(
            pWorld->currentLocationReputation(), pWorld->eventRuntimeState());
        uint32_t selectedId = 0;
        int selectedPrice = std::numeric_limits<int>::max();
        for (const InventoryItem &item : pStock->standardStock)
        {
            if (!item.objectDescriptionId) continue;
            const int price = HouseServiceRuntime::buyPrice(
                *pParty,
                *runtime.itemTable(),
                *runtime.standardItemEnchantTable(),
                *runtime.specialItemEnchantTable(),
                *pHouse,
                item,
                reputation);
            if (price > 0 && price <= pParty->gold() && price < selectedPrice)
            {
                selectedId = item.objectDescriptionId;
                selectedPrice = price;
            }
        }
        require(selectedId != 0, "Original shop has no affordable rendered stock item");
        for (const GameplayRenderedInspectableHudItem &image : runtime.renderedInspectableHudItems())
        {
            if (image.objectDescriptionId != selectedId) continue;
            for (float y = image.y + 1.0f; y < image.y + image.height; y += 2.0f)
            {
                for (float x = image.x + 1.0f; x < image.x + image.width; x += 2.0f)
                {
                    if (!runtime.isOpaqueHudPixelAtPoint(image, x, y)) continue;
                    clickPoint(session, width, height, x, y);
                    std::cout << "Pointer shop item: id=" << selectedId << " price=" << selectedPrice
                              << " at=" << x << ',' << y << '\n';
                    return true;
                }
            }
        }
        throw std::runtime_error("Affordable shop item has no visible opaque pointer target");
    }

    static void writeDimensions(
        const std::filesystem::path &output,
        int requestedWidth,
        int requestedHeight,
        int callbackWidth,
        int callbackHeight)
    {
        int windowCount = 0;
        SDL_Window **pWindows = SDL_GetWindows(&windowCount);
        SDL_Window *pWindow = pWindows && windowCount == 1 ? pWindows[0] : nullptr;
        SDL_free(pWindows);
        require(pWindow != nullptr, "Dimension audit needs the single capture window");
        int windowWidth = 0;
        int windowHeight = 0;
        int drawableWidth = 0;
        int drawableHeight = 0;
        require(SDL_GetWindowSize(pWindow, &windowWidth, &windowHeight), "Cannot read Mac window point size");
        require(SDL_GetWindowSizeInPixels(pWindow, &drawableWidth, &drawableHeight),
            "Cannot read Mac drawable size");
        std::ofstream json(output / "dimensions.json");
        json.exceptions(std::ios::badbit | std::ios::failbit);
        json << "{\n"
             << "  \"requested_window_points\": {\"width\": " << requestedWidth
             << ", \"height\": " << requestedHeight << "},\n"
             << "  \"actual_window_points\": {\"width\": " << windowWidth
             << ", \"height\": " << windowHeight << "},\n"
             << "  \"callback_drawable_pixels\": {\"width\": " << callbackWidth
             << ", \"height\": " << callbackHeight << "},\n"
             << "  \"sdl_drawable_pixels\": {\"width\": " << drawableWidth
             << ", \"height\": " << drawableHeight << "}\n"
             << "}\n";
        std::cout << "Dimensions: requested_window=" << requestedWidth << 'x' << requestedHeight
                  << " actual_window=" << windowWidth << 'x' << windowHeight
                  << " callback_drawable=" << callbackWidth << 'x' << callbackHeight
                  << " sdl_drawable=" << drawableWidth << 'x' << drawableHeight << '\n';
    }

    static void updatePage(
        GameApplication &game,
        Page page,
        State &state,
        int width,
        int height,
        std::set<std::string> &captured,
        std::optional<std::string> &captureRequest,
        std::ofstream &checks)
    {
        GameSession &session = game.m_gameSession;
        GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
        if (state.complete) return;
        ++state.phaseFrame;
        if (state.phaseFrame < 6) return;

        const auto closeUntilDone = [&]()
        {
            if (!runtime.activeEventDialog().isActive
                && !runtime.houseShopOverlay().active
                && !runtime.inventoryNestedOverlay().active)
            {
                state.complete = true;
                checks << "pointer-close\tpass\tclicks=" << state.closeClicks << '\n';
                return;
            }
            require(++state.closeClicks <= 5, "Pointer close did not dismiss the bounded dialogue layers");
            clickClose(session, width, height);
            state.phaseFrame = 0;
        };

        switch (page)
        {
        case Page::Temple:
            if (state.phase == 0)
            {
                IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
                require(pWorld && session.currentMapFileName() == "out01.odm", "Temple fixture is not on out01");
                constexpr float x = 9516.0f;
                constexpr float y = 9450.0f;
                constexpr float z = 1920.0f;
                const auto &movement = game.m_activeWorldManifest.partyMovement;
                const float eyeHeight = movement.declared ? movement.eyeHeight : 160.0f;
                game.m_outdoorGameView.setCameraAngles(
                    std::atan2(y - pWorld->partyY(), x - pWorld->partyX()),
                    std::atan2(z - pWorld->partyFootZ() - eyeHeight,
                        std::hypot(x - pWorld->partyX(), y - pWorld->partyY())));
                pressAction(session, width, height, KeyboardAction::Use);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 303,
                    "Normal Use did not enter original temple 303");
                const bool hasProprietor = std::any_of(
                    runtime.activeEventDialog().actions.begin(), runtime.activeEventDialog().actions.end(),
                    [](const EventDialogAction &action)
                    {
                        return action.kind == EventDialogActionKind::HouseProprietor && action.id == 303;
                    });
                if (hasProprietor) clickAction(session, width, height,
                    EventDialogActionKind::HouseProprietor, 303);
                advance(state, 2);
            }
            else if (state.phase == 2)
            {
                const size_t heal = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TempleHeal));
                const std::string label = runtime.activeEventDialog().actions[heal].label;
                require(label.find("治疗") != std::string::npos && label.find("金币") != std::string::npos,
                    "Checkpoint-119 Chinese healing label is not rendered by the actual temple");
                state.goldBefore = runtime.partyReadOnly()->gold();
                captureRequest = "temple-menu";
                checks << "temple-menu\tpass\theal_label=" << label << '\n';
                advance(state, 3);
            }
            else if (state.phase == 3 && captured.contains("temple-menu"))
            {
                clickAction(session, width, height, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TempleHeal));
                require(runtime.partyReadOnly()->gold() == state.goldBefore - 10,
                    "Pointer healing did not charge the audited 10 gold");
                require(!runtime.activeEventDialog().lines.empty()
                        && runtime.activeEventDialog().lines.front().find("神殿已为") != std::string::npos,
                    "Pointer healing did not produce checkpoint-119 Chinese service feedback");
                captureRequest = "temple-healed";
                checks << "pointer-service\tpass\taction=TempleHeal gold=" << state.goldBefore
                       << "->" << runtime.partyReadOnly()->gold() << " feedback="
                       << runtime.activeEventDialog().lines.front() << '\n';
                advance(state, 4);
            }
            else if (state.phase == 4 && captured.contains("temple-healed")) closeUntilDone();
            break;

        case Page::Npc:
            if (state.phase == 0)
            {
                require(session.currentMapFileName() == "oute3.odm", "NPC fixture is not on oute3");
                game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
                pressAction(session, width, height, KeyboardAction::Trigger);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                    "Normal Trigger did not enter original tavern 260");
                clickAction(session, width, height, EventDialogActionKind::HouseResident, 786);
                advance(state, 2);
            }
            else if (state.phase == 2)
            {
                Party *pParty = runtime.party();
                require(pParty && pParty->inventoryItemCount(2069) == 1,
                    "NPC fixture lost the original candlestick item");
                if (!pParty->hasQuestBit(1150))
                {
                    clickAction(session, width, height, EventDialogActionKind::NpcTopic, 1609);
                }
                advance(state, 3);
            }
            else if (state.phase == 3)
            {
                clickAction(session, width, height, EventDialogActionKind::NpcTopic, 1610);
                require(!runtime.activeEventDialog().lines.empty()
                        && runtime.activeEventDialog().lines.front().size() >= 30,
                    "Original candlestick response is not a substantive multiline dialogue");
                captureRequest = "npc-response";
                checks << "pointer-dialogue-option\tpass\tnpc=786 topic=1610 body="
                       << runtime.activeEventDialog().lines.front() << '\n';
                advance(state, 4);
            }
            else if (state.phase == 4 && captured.contains("npc-response")) closeUntilDone();
            break;

        case Page::Shop:
            if (state.phase == 0)
            {
                require(session.currentMapFileName() == "out01.odm", "Shop fixture is not on out01");
                game.m_outdoorGameView.setCameraAngles(0.0f, -0.1f);
                pressAction(session, width, height, KeyboardAction::Trigger);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 1,
                    "Normal Trigger did not enter original weapon shop 1");
                const size_t buy = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::ShopBuyStandard));
                const std::string &buyLabel = runtime.activeEventDialog().actions[buy].label;
                require(buyLabel == "购买标准物品", "Actual shop did not render the Chinese buy label");
                captureRequest = "shop-menu";
                checks << "shop-menu\tpass\tbuy_label=" << buyLabel << '\n';
                advance(state, 2);
            }
            else if (state.phase == 2 && captured.contains("shop-menu"))
            {
                clickAction(session, width, height, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::ShopBuyStandard));
                require(runtime.houseShopOverlay().active && runtime.houseShopOverlay().houseId == 1,
                    "Pointer service click did not open original standard stock");
                captureRequest = "shop-stock";
                checks << "pointer-shop-service\tpass\taction=ShopBuyStandard\n";
                advance(state, 3);
            }
            else if (state.phase == 3 && captured.contains("shop-stock"))
            {
                Party *pParty = runtime.party();
                require(pParty != nullptr, "Shop pointer transaction has no party");
                state.goldBefore = pParty->gold();
                state.inventoryBefore = pParty->inventoryItemCount();
                clickAffordableShopItem(session, width, height);
                require(pParty->gold() < state.goldBefore
                        && pParty->inventoryItemCount() == state.inventoryBefore + 1,
                    "Visible shop item pointer click did not perform exactly one purchase");
                require(runtime.statusBarEventText().starts_with("购买了"),
                    "Pointer shop purchase did not display localized result feedback");
                captureRequest = "shop-after-buy";
                checks << "pointer-shop-item\tpass\tgold=" << state.goldBefore << "->" << pParty->gold()
                       << " inventory=" << state.inventoryBefore << "->" << pParty->inventoryItemCount()
                       << " feedback=" << runtime.statusBarEventText() << '\n';
                advance(state, 4);
            }
            else if (state.phase == 4 && captured.contains("shop-after-buy"))
            {
                clickClose(session, width, height);
                require(!runtime.houseShopOverlay().active && runtime.activeEventDialog().isActive,
                    "Pointer close did not return from shop stock to the service root");
                clickAction(session, width, height, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::OpenShopEquipmentMenu));
                const size_t sell = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::ShopSell));
                const size_t identify = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::ShopIdentify));
                const size_t repair = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::ShopRepair));
                require(runtime.activeEventDialog().actions[sell].label == "出售"
                        && runtime.activeEventDialog().actions[identify].label == "鉴定"
                        && runtime.activeEventDialog().actions[repair].label == "修理",
                    "Actual shop equipment submenu did not render the Chinese service labels");
                captureRequest = "shop-services";
                checks << "shop-services\tpass\tlabels=出售|鉴定|修理\n";
                advance(state, 5);
            }
            else if (state.phase == 5 && captured.contains("shop-services")) closeUntilDone();
            break;

        case Page::Tavern:
            if (state.phase == 0)
            {
                require(session.currentMapFileName() == "oute3.odm", "Tavern fixture is not on oute3");
                game.m_outdoorGameView.setCameraAngles(0.0f, -0.32f);
                pressAction(session, width, height, KeyboardAction::Trigger);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 260,
                    "Normal Trigger did not enter original tavern 260");
                const bool hasProprietor = std::any_of(
                    runtime.activeEventDialog().actions.begin(), runtime.activeEventDialog().actions.end(),
                    [](const EventDialogAction &action)
                    {
                        return action.kind == EventDialogActionKind::HouseProprietor && action.id == 260;
                    });
                if (hasProprietor)
                {
                    clickAction(session, width, height, EventDialogActionKind::HouseProprietor, 260);
                }
                advance(state, 2);
            }
            else if (state.phase == 2)
            {
                const size_t rent = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TavernRentRoom));
                const size_t food = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TavernBuyFood));
                const std::string &rentLabel = runtime.activeEventDialog().actions[rent].label;
                const std::string &foodLabel = runtime.activeEventDialog().actions[food].label;
                require(rentLabel.find("租房间") != std::string::npos
                        && foodLabel.find("购买") != std::string::npos
                        && foodLabel.find("金币") != std::string::npos,
                    "Actual tavern did not render localized room and provisions prices");
                const Party *pParty = runtime.partyReadOnly();
                require(pParty != nullptr && pParty->food() < 14, "Tavern fixture cannot buy provisions");
                state.goldBefore = pParty->gold();
                state.foodBefore = pParty->food();
                captureRequest = "tavern-menu";
                checks << "tavern-menu\tpass\trent_label=" << rentLabel
                       << " food_label=" << foodLabel << '\n';
                advance(state, 3);
            }
            else if (state.phase == 3 && captured.contains("tavern-menu"))
            {
                clickAction(session, width, height, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TavernBuyFood));
                const Party *pParty = runtime.partyReadOnly();
                require(pParty != nullptr && pParty->gold() < state.goldBefore
                        && pParty->food() == 14 && state.foodBefore < 14,
                    "Pointer tavern purchase did not apply the quoted original provisions service");
                require(!runtime.activeEventDialog().lines.empty()
                        && runtime.activeEventDialog().lines.front().find("食物补充到") != std::string::npos,
                    "Pointer tavern purchase did not display localized result feedback");
                captureRequest = "tavern-after-food";
                checks << "pointer-tavern-food\tpass\tgold=" << state.goldBefore << "->" << pParty->gold()
                       << " food=" << state.foodBefore << "->" << pParty->food()
                       << " feedback=" << runtime.activeEventDialog().lines.front() << '\n';
                advance(state, 4);
            }
            else if (state.phase == 4 && captured.contains("tavern-after-food")) closeUntilDone();
            break;

        case Page::Training:
            if (state.phase == 0)
            {
                require(session.currentMapFileName() == "outd3.odm", "Training fixture is not on outd3");
                game.m_outdoorGameView.setCameraAngles(-1.5707963f, -0.2175f);
                pressAction(session, width, height, KeyboardAction::Trigger);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 1589,
                    "Normal Trigger did not enter original training hall 1589");
                const size_t train = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TrainingTrainActiveMember));
                const std::string &label = runtime.activeEventDialog().actions[train].label;
                require(label.find("训练至等级") != std::string::npos
                        && label.find("金币") != std::string::npos,
                    "Actual training hall did not render localized level and fee");
                const Party *pParty = runtime.partyReadOnly();
                require(pParty != nullptr && pParty->activeMember() != nullptr,
                    "Training fixture has no active member");
                state.goldBefore = pParty->gold();
                state.levelBefore = pParty->activeMember()->level;
                captureRequest = "training-menu";
                checks << "training-menu\tpass\tlabel=" << label << '\n';
                advance(state, 2);
            }
            else if (state.phase == 2 && captured.contains("training-menu"))
            {
                clickAction(session, width, height, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TrainingTrainActiveMember));
                const Party *pParty = runtime.partyReadOnly();
                require(pParty != nullptr && pParty->activeMember() != nullptr
                        && pParty->gold() < state.goldBefore
                        && pParty->activeMember()->level == state.levelBefore + 1,
                    "Pointer training did not charge and advance exactly one level");
                require(runtime.statusBarEventText().find("已升至") != std::string::npos,
                    "Pointer training did not display localized level-up feedback");
                captureRequest = "training-after-level";
                checks << "pointer-training\tpass\tgold=" << state.goldBefore << "->" << pParty->gold()
                       << " level=" << state.levelBefore << "->" << pParty->activeMember()->level
                       << " feedback=" << runtime.statusBarEventText() << '\n';
                advance(state, 3);
            }
            else if (state.phase == 3 && captured.contains("training-after-level")) closeUntilDone();
            break;

        case Page::Transport:
            if (state.phase == 0)
            {
                require(session.currentMapFileName() == "oute3.odm",
                    "Transport fixture is not at the New Sorpigal stable");
                game.m_outdoorGameView.setCameraAngles(-2.6109f, -0.2175f);
                pressAction(session, width, height, KeyboardAction::Trigger);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                require(runtime.activeEventDialog().isActive && runtime.activeEventDialog().sourceId == 470,
                    "Normal Trigger did not enter original New Sorpigal stable 470");
                const size_t route = actionIndex(runtime, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TransportRoute));
                const std::string &label = runtime.activeEventDialog().actions[route].label;
                require(label.find("旅费") != std::string::npos
                        && label.find("天到") != std::string::npos,
                    "Actual stable did not render localized destination, days and fare");
                Party *pParty = runtime.party();
                require(pParty != nullptr && pParty->gold() > 0, "Transport fixture has no rejection gold to remove");
                state.goldBefore = pParty->gold();
                captureRequest = "transport-menu";
                checks << "transport-menu\tpass\tlabel=" << label
                       << " normal_trigger_entry=true\n";
                advance(state, 2);
            }
            else if (state.phase == 2 && captured.contains("transport-menu"))
            {
                Party *pParty = runtime.party();
                require(pParty != nullptr, "Transport rejection has no party");
                pParty->addGold(-pParty->gold());
                clickAction(session, width, height, EventDialogActionKind::HouseService,
                    uint32_t(HouseActionId::TransportRoute));
                require(pParty->gold() == 0 && !runtime.activeEventDialog().lines.empty()
                        && runtime.activeEventDialog().lines.front().starts_with("旅行需要"),
                    "Pointer transport rejection did not preserve gold and show localized fare feedback");
                captureRequest = "transport-rejected";
                checks << "pointer-transport-rejection\tpass\tcontrolled_gold=" << state.goldBefore
                       << "->0 feedback=" << runtime.activeEventDialog().lines.front() << '\n';
                advance(state, 3);
            }
            else if (state.phase == 3 && captured.contains("transport-rejected")) closeUntilDone();
            break;

        case Page::Character:
            if (state.phase == 0)
            {
                clickPortraitToOpenCharacter(session, width, height, 0);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                clickLayout(session, width, height, "CharacterSkillsButton");
                require(runtime.characterScreen().page == GameplayUiController::CharacterPage::Skills,
                    "Pointer skills-tab click did not open the accepted skills page");
                captureRequest = "character-skills";
                checks << "character-skills-comparison\tpass\tmember=0\n";
                advance(state, 2);
            }
            else if (state.phase == 2 && captured.contains("character-skills"))
            {
                clickLayout(session, width, height, "CharacterExitButton");
                require(!runtime.characterScreen().open, "Pointer character close did not exit the comparison page");
                state.complete = true;
            }
            break;

        case Page::Portal:
            if (state.phase == 0)
            {
                IGameplayWorldRuntime *pWorld = session.activeWorldRuntime();
                require(session.currentMapFileName() == "out01.odm" && pWorld
                        && pWorld->partyX() >= -512.0f && pWorld->partyX() < 0.0f
                        && pWorld->partyY() > 2048.0f && pWorld->partyY() <= 2560.0f
                        && static_cast<uint32_t>(std::floor(pWorld->gameMinutes() / 1440.0)) == 24,
                    "Portal comparison is not the accepted day-24 out01 tile 63,59 fixture");
                if (!runtime.shouldTownPortalCastOpenDimensionDoor())
                {
                    // The short-lived native activation is intentionally not part of save data. Re-arm the
                    // accepted exact-tile fixture through the production entry point, then use normal spellbook
                    // pointer input for the visual comparison. This does not claim a new natural-contact proof.
                    runtime.activateDimensionDoor(DimensionDoorActivationSource::NaturalLocation);
                    checks << "portal-controlled-rearm\tpass\tsource=NaturalLocation tile=63,59 day=24\n";
                }
                require(runtime.shouldTownPortalCastOpenDimensionDoor(),
                    "Controlled exact-tile Dimension Door comparison did not re-arm");
                pressAction(session, width, height, KeyboardAction::Cast);
                advance(state, 1);
            }
            else if (state.phase == 1)
            {
                clickLayout(session, width, height, "SpellbookSchoolButtonWater");
                advance(state, 2);
            }
            else if (state.phase == 2)
            {
                clickLayout(session, width, height,
                    spellbookSpellLayoutId(GameplayUiController::SpellbookSchool::Water, 9));
                advance(state, 3);
            }
            else if (state.phase == 3)
            {
                clickLayout(session, width, height, "SpellbookCastButton");
                advance(state, 4);
            }
            else if (state.phase == 4)
            {
                const auto &overlay = runtime.utilitySpellOverlayReadOnly();
                require(overlay.active
                        && overlay.mode == GameplayUiController::UtilitySpellOverlayMode::DimensionDoor
                        && runtime.ensureTownPortalDestinationsLoaded(),
                    "Normal spellbook pointer flow did not open Dimension Door");
                hoverDestination(game, width, height, "TPGlobal:4");
                captureRequest = "dimension-door-preview";
                checks << "dimension-door-comparison\tpass\tdestination=TPGlobal:4\n";
                advance(state, 5);
            }
            else if (state.phase == 5 && captured.contains("dimension-door-preview"))
            {
                state.complete = true;
            }
            break;
        }
    }

    static int run(
        const std::filesystem::path &runtimeRoot,
        const std::filesystem::path &inputSave,
        const std::filesystem::path &output,
        Page page,
        int requestedWidth,
        int requestedHeight)
    {
        require(std::filesystem::is_directory(runtimeRoot), "Runtime root does not exist");
        require(std::filesystem::is_regular_file(inputSave), "Input save does not exist");
        require(!std::filesystem::exists(output), "Capture output must be a new directory");
        require(requestedWidth >= 320 && requestedWidth <= 2560
                && requestedHeight >= 240 && requestedHeight <= 1600,
            "Requested capture window is outside the bounded size range");
        std::filesystem::create_directories(output);
        const std::filesystem::path absoluteOutput = std::filesystem::canonical(output);
        const std::filesystem::path absoluteSave = std::filesystem::canonical(inputSave);
        std::filesystem::current_path(absoluteOutput);

        GameSettings settings = GameSettings::createDefault();
        settings.startInMainMenu = true;
        settings.preseedParty = false;
        settings.immortal = false;
        settings.unlimitedMana = false;
        settings.debugConsole = false;
        settings.gameplayTrace = true;
        settings.gameplayTraceAppend = false;
        settings.gameplayTraceFile = (absoluteOutput / "gameplay.trace").string();
        settings.assetRoot = runtimeRoot.string();
        settings.windowMode = WindowMode::Windowed;
        std::string settingsError;
        require(saveGameSettings(absoluteOutput / "settings.ini", settings, settingsError),
            "Cannot write isolated capture settings: " + settingsError);

        std::ofstream checks(absoluteOutput / "checks.tsv");
        checks.exceptions(std::ios::badbit | std::ios::failbit);
        checks << "check\tresult\tdetail\n";

        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
        config.appName = "OpenYAMM house dialogue layout validation";
        config.assetRoot = runtimeRoot.string();
        config.activeWorldId = "mm6";
        config.windowWidth = requestedWidth;
        config.windowHeight = requestedHeight;
        config.windowMode = Engine::WindowMode::Windowed;
        config.verticalSync = true;

        GameApplication game(config);
        State state;
        uint32_t frames = 0;
        uint64_t startedTicks = 0;
        bool wroteDimensions = false;
        std::map<std::string, PendingCapture> pendingCaptures;
        std::set<std::string> captured;
        const std::set<std::string> expected = expectedCaptures(page);

        Engine::EngineApplication application(config,
            [&](Engine::AssetFileSystem &files)
            {
                return game.loadGameData(files);
            },
            [&]()
            {
                require(bgfx::getRendererType() == bgfx::RendererType::Metal,
                    "House dialogue capture requires the Metal backend");
                require(game.initializeRenderer() && game.loadSessionFromPath(absoluteSave),
                    "Cannot initialize the real application from the controlled save");
                startedTicks = SDL_GetTicks();
                return true;
            },
            [&](const SDL_Event &event)
            {
                if (!isPhysicalInputEvent(event)) game.handleSdlEvent(event);
            },
            [&](int width, int height, float wheel, float delta)
            {
                ++frames;
                if (!wroteDimensions)
                {
                    writeDimensions(absoluteOutput, requestedWidth, requestedHeight, width, height);
                    wroteDimensions = true;
                }
                std::optional<std::string> captureRequest;
                updatePage(game, page, state, width, height, captured, captureRequest, checks);
                game.renderFrame(width, height, wheel, delta);

                if (captureRequest)
                {
                    require(!pendingCaptures.contains(*captureRequest) && !captured.contains(*captureRequest),
                        "Screenshot token was requested twice: " + *captureRequest);
                    pendingCaptures.emplace(*captureRequest, PendingCapture{frames});
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, captureRequest->c_str());
                }
                for (auto iterator = pendingCaptures.begin(); iterator != pendingCaptures.end();)
                {
                    std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                        Engine::BgfxContext::consumeScreenshot(iterator->first);
                    if (!capture)
                    {
                        require(frames - iterator->second.requestedFrame <= 300,
                            "Metal screenshot readback timed out: " + iterator->first);
                        ++iterator;
                        continue;
                    }
                    SDL_Surface *surface = SDL_CreateSurfaceFrom(
                        int(capture->width), int(capture->height), SDL_PIXELFORMAT_BGRA32,
                        capture->bgraPixels.data(), int(capture->width * 4));
                    require(surface != nullptr, "Cannot wrap Metal screenshot pixels");
                    const bool written = SDL_SaveBMP(
                        surface, (absoluteOutput / (iterator->first + ".bmp")).c_str());
                    SDL_DestroySurface(surface);
                    require(written, "Cannot write Metal screenshot: " + iterator->first);
                    require(int(capture->width) > 0 && int(capture->height) > 0,
                        "Metal screenshot has invalid drawable dimensions");
                    std::cout << "Captured " << iterator->first << ": "
                              << capture->width << 'x' << capture->height << '\n';
                    checks << "capture\tpass\t" << iterator->first << '='
                           << capture->width << 'x' << capture->height << '\n';
                    captured.insert(iterator->first);
                    iterator = pendingCaptures.erase(iterator);
                }

                const bool timedOut = frames >= 3600 || SDL_GetTicks() - startedTicks >= 60000;
                if (state.complete || timedOut)
                {
                    require(state.complete, "Bounded page capture exceeded its frame or wall-time limit");
                    require(captured == expected, "Bounded page capture missed an expected screenshot");
                    SDL_Event quit = {};
                    quit.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit);
                }
            },
            [&]()
            {
                game.shutdownApplication();
            },
            [&]() { return game.applicationTextInputActive(); },
            [&](int width, int height) { return game.applicationTextInputArea(width, height); });

        const int result = application.run();
        require(result == 0 && state.complete && captured == expected && wroteDimensions,
            "House dialogue layout capture did not complete");
        std::cout << "House dialogue layout capture passed: page=" << pageName(page)
                  << " frames=" << frames << " captures=" << captured.size()
                  << "; Mac Metal only.\n";
        return 0;
    }
};
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 7)
        {
            throw std::runtime_error(
                "Usage: house_dialogue_layout_capture RUNTIME INPUT_SAVE NEW_OUTPUT "
                "--temple|--npc|--shop|--tavern|--training|--transport|--character|--portal "
                "WINDOW_WIDTH WINDOW_HEIGHT");
        }
        const std::string option = argv[4];
        OpenYAMM::Game::Page page;
        if (option == "--temple") page = OpenYAMM::Game::Page::Temple;
        else if (option == "--npc") page = OpenYAMM::Game::Page::Npc;
        else if (option == "--shop") page = OpenYAMM::Game::Page::Shop;
        else if (option == "--tavern") page = OpenYAMM::Game::Page::Tavern;
        else if (option == "--training") page = OpenYAMM::Game::Page::Training;
        else if (option == "--transport") page = OpenYAMM::Game::Page::Transport;
        else if (option == "--character") page = OpenYAMM::Game::Page::Character;
        else if (option == "--portal") page = OpenYAMM::Game::Page::Portal;
        else throw std::runtime_error("Unsupported house dialogue page: " + option);
        return OpenYAMM::Game::ApplicationCaptureTestAccess::run(
            std::filesystem::canonical(argv[1]),
            std::filesystem::canonical(argv[2]),
            std::filesystem::absolute(argv[3]),
            page,
            std::stoi(argv[5]),
            std::stoi(argv[6]));
    }
    catch (const std::exception &error)
    {
        std::cerr << "House dialogue layout capture failed: " << error.what() << '\n';
        return 1;
    }
}
