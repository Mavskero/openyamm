#include "doctest/doctest.h"

#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/data/GameDataRepository.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayHudInputController.h"
#include "game/gameplay/GameplayScreenRuntime.h"
#include "game/outdoor/OutdoorWorldRuntime.h"

#include <optional>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
// The test double supplies world query results and records dispatch. All input policy,
// click latching, frame gates and activation routing run through the real GameSession.
// No AssetFileSystem, renderer or duplicated pointer state machine is involved.
class RecordingWorld : public OutdoorWorldRuntime
{
public:
    Party testParty;
    GameplayWorldHit hit;
    std::vector<size_t> activations;
    unsigned mouseQueries = 0;
    unsigned attackQueries = 0;
    unsigned steals = 0;
    unsigned heldUses = 0;
    bool stealHandled = true;
    bool allowActivation = true;
    mutable std::optional<GameplayInteractionMethod> lastActivationMethod;
    bool openLootOnActivation = false;
    bool lootActive = false;
    GameplayChestViewState loot;

    RecordingWorld()
    {
        PartySeed seed;
        seed.members.resize(1);
        seed.members[0].name = "Mouse tester";
        seed.members[0].health = seed.members[0].maxHealth = 100;
        testParty.seed(seed);
        hit.hasHit = true;
        hit.kind = GameplayWorldHitKind::Actor;
        GameplayActorTargetHit actor;
        actor.actorIndex = 7;
        actor.npcId = 772;
        actor.displayName = "World query fixture";
        actor.isFriendly = true;
        actor.distance = 345.0f;
        hit.actor = actor;
    }

    Party *party() override { return &testParty; }
    const Party *party() const override { return &testParty; }
    bool worldInteractionReady() const override { return true; }
    bool worldInspectModeActive() const override { return true; }
    GameplayWorldPickRequest buildWorldPickRequest(const GameplayWorldPickRequestInput &input) const override
    {
        GameplayWorldPickRequest request;
        request.screenX = input.screenX;
        request.screenY = input.screenY;
        request.viewWidth = input.screenWidth;
        request.viewHeight = input.screenHeight;
        request.hasRay = input.includeRay;
        return request;
    }
    GameplayWorldHit pickMouseInteractionTarget(const GameplayWorldPickRequest &) override
    {
        ++mouseQueries;
        return hit;
    }
    GameplayWorldHit pickKeyboardInteractionTarget(const GameplayWorldPickRequest &) override { return hit; }
    GameplayWorldHit pickPartyAttackTarget(const GameplayWorldPickRequest &) override
    {
        ++attackQueries;
        return {};
    }
    bool canActivateWorldHit(const GameplayWorldHit &value, GameplayInteractionMethod method) const override
    {
        lastActivationMethod = method;
        return allowActivation && value.hasHit && value.actor && value.actor->distance < 512.0f;
    }
    bool activateWorldHit(const GameplayWorldHit &value) override
    {
        activations.push_back(value.actor->actorIndex);
        if (openLootOnActivation) lootActive = true;
        return true;
    }
    bool tryStealFromActor(size_t, uint32_t, uint32_t) override
    {
        ++steals;
        return stealHandled;
    }
    GameplayWorldHit pickHeldItemWorldTarget(const GameplayWorldPickRequest &) override { return hit; }
    bool canUseHeldItemOnWorld(const GameplayWorldHit &) const override { return true; }
    bool useHeldItemOnWorld(const GameplayWorldHit &) override { ++heldUses; return true; }
    GameplayChestViewState *activeChestView() override { return lootActive ? &loot : nullptr; }
    const GameplayChestViewState *activeChestView() const override { return lootActive ? &loot : nullptr; }
    GameplayHoverStatusPayload refreshWorldHover(const GameplayWorldHoverRequest &) override
    {
        GameplayHoverStatusPayload payload;
        payload.worldHit = hit;
        return payload;
    }
    GameplayHoverStatusPayload readCachedWorldHover() override { return refreshWorldHover({}); }
    void clearWorldHover() override {}
};

struct Fixture
{
    GameDataLoader loader;
    GameDataRepository data;
    RecordingWorld world;
    GameSettings settings = GameSettings::createDefault();
    GameSession session;

    explicit Fixture(ControlScheme scheme)
    {
        data.bind(loader);
        settings.controlScheme = scheme;
        settings.contextActionPopup = false;
        session.bindDataRepository(&data);
        session.gameplayScreenRuntime().bindSettings(&settings);
        session.bindActiveWorldRuntime(&world);
    }
    ~Fixture()
    {
        session.bindActiveWorldRuntime(nullptr);
        // bindDataRepository owns the shared class-multiplier table binding; clear it before loader dies.
        session.bindDataRepository(nullptr);
    }

    GameplayInputFrame input(bool left, bool right = false)
    {
        GameplayInputFrame frame;
        frame.screenWidth = 1210;
        frame.screenHeight = 834;
        frame.pointerX = 600.0f;
        frame.pointerY = 300.0f;
        frame.leftMouseButton.held = left;
        frame.rightMouseButton.held = right;
        return frame;
    }
    void send(const GameplayInputFrame &frame) { session.updateGameplay(frame, 0.0f); }
    void press(bool right = false)
    {
        auto frame = input(true, right);
        frame.leftMouseButton.pressed = true;
        send(frame);
    }
    void release(bool right = false)
    {
        auto frame = input(false, right);
        frame.leftMouseButton.released = true;
        send(frame);
    }
};
}

TEST_CASE("classic world pointer down and release dispatch exactly once through the real Session frame")
{
    Fixture f(ControlScheme::Classic);
    auto press = f.input(true);
    press.leftMouseButton.pressed = true;
    f.send(press);
    CHECK(f.world.activations.empty());
    f.send(f.input(true));
    CHECK(f.world.activations.empty());
    auto release = f.input(false);
    release.leftMouseButton.released = true;
    f.send(release);
    REQUIRE(f.world.activations.size() == 1);
    CHECK(f.world.activations.front() == 7);
    CHECK(f.world.lastActivationMethod == GameplayInteractionMethod::Mouse);
    CHECK(f.world.mouseQueries >= 2);
    f.send(f.input(false));
    CHECK(f.world.activations.size() == 1);
    CHECK(f.world.attackQueries == 0);
}

TEST_CASE("modern cursor mode preserves an actual world click while right mouse stays held")
{
    Fixture f(ControlScheme::Modern);
    f.send(f.input(false, true));
    REQUIRE(f.session.gameplayScreenState().gameplayMouseLookState().cursorModeActive);
    auto press = f.input(true, true);
    press.leftMouseButton.pressed = true;
    f.send(press);
    CHECK(f.world.activations.empty());
    auto release = f.input(false, true);
    release.leftMouseButton.released = true;
    f.send(release);
    REQUIRE(f.world.activations.size() == 1);
    CHECK(f.world.activations.front() == 7);
    CHECK(f.world.lastActivationMethod == GameplayInteractionMethod::Mouse);
    f.send(f.input(false, false));
    CHECK(f.world.activations.size() == 1);
    CHECK(f.world.attackQueries == 0);
}

TEST_CASE("world clicks require the same target on release and recheck activation eligibility")
{
    for (unsigned scenario = 0; scenario < 4; ++scenario)
    {
        CAPTURE(scenario);
        Fixture f(ControlScheme::Classic);
        f.press();
        if (scenario == 0) f.world.hit.actor->actorIndex = 8;
        if (scenario == 1) f.world.hit = {};
        if (scenario == 2) f.world.hit.actor->distance = 512.0f;
        if (scenario == 3) f.world.allowActivation = false;
        f.release();
        CHECK(f.world.activations.empty());
        CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
    }
}

TEST_CASE("modern mouse look left button keeps attack semantics instead of opening an actor")
{
    Fixture f(ControlScheme::Modern);
    f.press();
    REQUIRE(f.session.gameplayScreenState().gameplayMouseLookState().mouseLookActive);
    f.release();
    CHECK(f.world.activations.empty());
    CHECK(f.world.attackQueries > 0);
}

TEST_CASE("leaving and reentering cursor mode during one held click cannot submit or rearm it")
{
    Fixture f(ControlScheme::Modern);
    f.send(f.input(false, true));
    f.press(true);
    f.send(f.input(true, false)); // Right release is not a left release.
    CHECK(f.world.activations.empty());
    f.send(f.input(true, true)); // No new physical left press.
    f.release(true);
    CHECK(f.world.activations.empty());
    f.press(true);
    f.release(true);
    REQUIRE(f.world.activations.size() == 1);
}

TEST_CASE("held item ownership cancels an armed ordinary click and retains its own input priority")
{
    Fixture f(ControlScheme::Classic);
    f.press();
    auto &held = f.session.gameplayScreenRuntime().heldInventoryItem();
    held.active = true;
    held.item.objectDescriptionId = 1;
    f.send(f.input(true));
    CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
    held.active = false;
    f.send(f.input(true));
    f.release();
    CHECK(f.world.activations.empty());

    held.active = true;
    f.press();
    f.release();
    CHECK(f.world.heldUses == 1);
    CHECK(f.world.activations.empty());
    held.active = false;
    f.press();
    f.release();
    REQUIRE(f.world.activations.size() == 1);
}

TEST_CASE("pending spell targeting cancels an armed ordinary world click")
{
    Fixture f(ControlScheme::Classic);
    f.press();
    auto &pending = f.session.gameplayScreenState().pendingSpellTarget();
    pending.active = true;
    // No spell is cast: the target mode is cancelled by the real Escape input path.
    auto cancel = f.input(false);
    cancel.keyboardHeld[SDL_SCANCODE_ESCAPE] = true;
    f.send(cancel);
    CHECK(f.world.activations.empty());
    CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
    f.send(f.input(false));
    CHECK(f.world.activations.empty());
}

TEST_CASE("real Session modal and loot gates consume a click without deferred world activation")
{
    for (unsigned modal = 0; modal < 4; ++modal)
    {
        CAPTURE(modal);
        Fixture f(ControlScheme::Classic);
        auto &screen = f.session.gameplayScreenRuntime();
        f.press();
        if (modal == 0) screen.menuScreenState().active = true;
        if (modal == 1) screen.journalScreenState().active = true;
        if (modal == 2) screen.activeEventDialog().isActive = true;
        if (modal == 3) f.world.lootActive = true;
        f.send(f.input(true));
        CHECK(f.world.activations.empty());
        CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
        screen.menuScreenState().active = false;
        screen.journalScreenState().active = false;
        screen.activeEventDialog().isActive = false;
        f.world.lootActive = false;
        f.send(f.input(true));
        f.release();
        CHECK(f.world.activations.empty());
        f.press();
        f.release();
        REQUIRE(f.world.activations.size() == 1);
    }
}

TEST_CASE("a world click that opens loot does not run a second activation in the release frame")
{
    Fixture f(ControlScheme::Classic);
    f.world.openLootOnActivation = true;
    f.press();
    f.release();
    REQUIRE(f.world.activations.size() == 1);
    REQUIRE(f.world.lootActive);
    CHECK(f.session.overlayInteractionState().lootChestItemLatch);
    f.send(f.input(false));
    CHECK(f.world.activations.size() == 1);
}

TEST_CASE("a real resolved HUD button consumes pointer input over a pickable world actor")
{
    Fixture f(ControlScheme::Classic);
    f.settings.gameplayUiLayout = GameplayUiLayout::Widescreen;
    // Explicit hit geometry is sufficient for the real HUD controller; no fake textures are needed.
    REQUIRE(f.session.gameplayUiRuntime().layoutManager().loadLayoutText("mouse-ui-fixture", R"(
screen: OutdoorHud
elements:
  - id: OutdoorButtonOptions
    anchor: top_left
    offset_x: 500
    offset_y: 200
    width: 100
    height: 100
    scale: {min: 1.0, max: 1.0}
)"));
    auto &screen = f.session.gameplayScreenRuntime();
    const auto button = screen.resolveHudLayoutElement("OutdoorButtonOptions", 1210, 834, 100, 100);
    REQUIRE(button.has_value());
    auto input = f.input(true);
    input.pointerX = button->x + button->width * 0.5f;
    input.pointerY = button->y + button->height * 0.5f;
    input.leftMouseButton.pressed = true;
    f.send(input);
    REQUIRE(f.session.overlayInteractionState().gameplayHudPressedTarget.type
        == GameplayHudPointerTargetType::MenuButton);
    CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
    input.leftMouseButton = {false, false, true};
    f.send(input);
    CHECK(screen.menuScreenState().active);
    CHECK(f.world.activations.empty());
}

TEST_CASE("keyboard activation wins a simultaneous mouse release without a delayed duplicate")
{
    for (KeyboardAction action : {KeyboardAction::Trigger, KeyboardAction::Use})
    {
        CAPTURE(int(action));
        Fixture f(ControlScheme::Classic);
        f.press();
        auto release = f.input(false);
        release.leftMouseButton.released = true;
        release.actions[keyboardActionIndex(action)] = {true, true, false};
        f.send(release);
        REQUIRE(f.world.activations.size() == 1);
        CHECK(f.world.lastActivationMethod == GameplayInteractionMethod::Keyboard);
        f.send(f.input(false));
        CHECK(f.world.activations.size() == 1);
        CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
    }
}

TEST_CASE("Ctrl stealing consumes the whole held gesture even after Ctrl is released")
{
    for (bool handled : {false, true})
    {
        CAPTURE(handled);
        Fixture f(ControlScheme::Classic);
        f.world.stealHandled = handled;
        auto press = f.input(true);
        press.leftMouseButton.pressed = true;
        press.keyboardHeld[SDL_SCANCODE_LCTRL] = true;
        f.send(press);
        REQUIRE(f.world.steals == 1);
        f.send(f.input(true));
        f.release();
        CHECK(f.world.activations.empty());
        f.press();
        f.release();
        REQUIRE(f.world.activations.size() == 1);
    }
}

TEST_CASE("a world press dragged onto a real HUD button cannot release through it")
{
    Fixture f(ControlScheme::Classic);
    f.settings.gameplayUiLayout = GameplayUiLayout::Widescreen;
    REQUIRE(f.session.gameplayUiRuntime().layoutManager().loadLayoutText("mouse-ui-release-fixture", R"(
screen: OutdoorHud
elements:
  - id: OutdoorButtonOptions
    anchor: top_left
    offset_x: 500
    offset_y: 200
    width: 100
    height: 100
    scale: {min: 1.0, max: 1.0}
)"));
    auto &screen = f.session.gameplayScreenRuntime();
    const auto button = screen.resolveHudLayoutElement("OutdoorButtonOptions", 1210, 834, 100, 100);
    REQUIRE(button.has_value());
    auto input = f.input(true);
    input.pointerX = button->x - 100.0f;
    input.pointerY = button->y - 100.0f;
    input.leftMouseButton.pressed = true;
    f.send(input);
    REQUIRE(f.session.overlayInteractionState().gameplayHudPressedTarget.type
        == GameplayHudPointerTargetType::None);
    REQUIRE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
    input.pointerX = button->x + button->width * 0.5f;
    input.pointerY = button->y + button->height * 0.5f;
    input.leftMouseButton = {false, false, true};
    f.send(input);
    // Neither target owns a complete gesture: HUD did not receive down, world did not receive up.
    CHECK_FALSE(screen.menuScreenState().active);
    CHECK(f.world.activations.empty());
    CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
}

TEST_CASE("crossing a HUD button while held cancels the world gesture until a fresh press")
{
    for (ControlScheme scheme : {ControlScheme::Classic, ControlScheme::Modern})
    {
        for (GameplayUiLayout layout : {GameplayUiLayout::Widescreen, GameplayUiLayout::Standard})
        {
            CAPTURE(int(scheme));
            CAPTURE(int(layout));
            Fixture f(scheme);
            f.settings.gameplayUiLayout = layout;
            REQUIRE(f.session.gameplayUiRuntime().layoutManager().loadLayoutText("mouse-ui-crossing-fixture", R"(
screen: OutdoorHud
elements:
  - id: OutdoorButtonOptions
    anchor: top_left
    offset_x: 500
    offset_y: 200
    width: 100
    height: 100
    scale: {min: 1.0, max: 1.0}
  - id: OutdoorStandardButtonOptions
    anchor: top_left
    offset_x: 500
    offset_y: 200
    width: 100
    height: 100
    scale: {min: 1.0, max: 1.0}
)"));
            auto &screen = f.session.gameplayScreenRuntime();
            const char *layoutId = layout == GameplayUiLayout::Widescreen
                ? "OutdoorButtonOptions" : "OutdoorStandardButtonOptions";
            const auto button = screen.resolveHudLayoutElement(layoutId, 1210, 834, 100, 100);
            REQUIRE(button.has_value());
            auto input = f.input(true, scheme == ControlScheme::Modern);
            input.pointerX = button->x - 100.0f;
            input.pointerY = button->y - 100.0f;
            input.leftMouseButton.pressed = true;
            f.send(input);
            REQUIRE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
            input.leftMouseButton.pressed = false;
            input.pointerX = button->x + button->width * 0.5f;
            input.pointerY = button->y + button->height * 0.5f;
            f.send(input);
            CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
            input.pointerX = button->x - 100.0f;
            input.pointerY = button->y - 100.0f;
            f.send(input);
            CHECK_FALSE(f.session.gameplayScreenState().worldInteractionInputState().inspectMouseActivateLatch);
            input.leftMouseButton = {false, false, true};
            f.send(input);
            CHECK(f.world.activations.empty());
            CHECK_FALSE(screen.menuScreenState().active);
            input.leftMouseButton = {true, true, false};
            f.send(input);
            input.leftMouseButton = {false, false, true};
            f.send(input);
            REQUIRE(f.world.activations.size() == 1);
            CHECK(f.world.lastActivationMethod == GameplayInteractionMethod::Mouse);
        }
    }
}

TEST_CASE("HUD target queries are read only and context buttons keep their press-time world target")
{
    for (bool enabled : {false, true})
    {
        CAPTURE(enabled);
        Fixture f(ControlScheme::Classic);
        f.settings.contextActionPopup = enabled;
        REQUIRE(f.session.gameplayUiRuntime().layoutManager().loadLayoutText("mouse-context-fixture", R"(
screen: OutdoorHud
elements:
  - id: OutdoorMobileContextActionButton
    anchor: top_left
    offset_x: 500
    offset_y: 200
    width: 100
    height: 100
    scale: {min: 1.0, max: 1.0}
)"));
        auto &screen = f.session.gameplayScreenRuntime();
        const auto button = screen.resolveHudLayoutElement("OutdoorMobileContextActionButton", 1210, 834, 100, 100);
        REQUIRE(button.has_value());
        GameplayContextAction action;
        action.kind = GameplayContextActionKind::Talk;
        action.worldHit = f.world.hit;
        screen.contextActionState().visible = true;
        screen.contextActionState().actions = {action};
        auto input = f.input(true);
        input.pointerX = button->x + button->width * 0.5f;
        input.pointerY = button->y + button->height * 0.5f;
        input.leftMouseButton.pressed = true;
        const auto target = GameplayHudInputController::resolveGameplayHudButtonTarget(
            screen,
            GameplayHudButtonInputConfig{
                .screenWidth = input.screenWidth,
                .screenHeight = input.screenHeight,
                .pointerX = input.pointerX,
                .pointerY = input.pointerY,
                .leftButtonPressed = true,
                .allowInput = true,
            });
        CHECK(target.type == (enabled
            ? GameplayHudPointerTargetType::ContextActionButton : GameplayHudPointerTargetType::None));
        CHECK_FALSE(screen.interactionState().gameplayHudClickLatch);
        CHECK(screen.interactionState().gameplayHudPressedTarget.type == GameplayHudPointerTargetType::None);
        CHECK_FALSE(screen.interactionState().gameplayHudPressedContextActionActive);
        CHECK_FALSE(screen.interactionState().gameplayHudPressedContextActionHit.hasHit);
        REQUIRE(screen.contextActionStateReadOnly().actions.size() == 1);
        CHECK(screen.contextActionStateReadOnly().actions.front().worldHit.actor->actorIndex == 7);
        f.send(input);
        CHECK(screen.interactionState().gameplayHudPressedContextActionActive == enabled);
        if (enabled)
        {
            REQUIRE(screen.interactionState().gameplayHudPressedContextActionHit.actor.has_value());
            CHECK(screen.interactionState().gameplayHudPressedContextActionHit.actor->actorIndex == 7);
            // A refreshed hover may point elsewhere. The real HUD release still owns its pressed target.
            f.world.hit.actor->actorIndex = 8;
            action.worldHit = f.world.hit;
            screen.contextActionState().visible = true;
            screen.contextActionState().actions = {action};
        }
        input.leftMouseButton = {false, false, true};
        f.send(input);
        REQUIRE(f.world.activations.size() == 1);
        CHECK(f.world.activations.front() == 7);
        CHECK(f.world.lastActivationMethod == (enabled
            ? GameplayInteractionMethod::Keyboard : GameplayInteractionMethod::Mouse));
        CHECK_FALSE(screen.interactionState().gameplayHudPressedContextActionActive);
    }
}
