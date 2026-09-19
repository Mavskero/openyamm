#include "engine/ApplePointer.h"
#include "engine/WindowPointer.h"

#include <TargetConditionals.h>

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <iostream>

@interface OYLockedPointerState : NSObject
@property(nonatomic, strong) CAShapeLayer *cursor;
@property(nonatomic) BOOL reported;
@property(nonatomic) BOOL requested;
@property(nonatomic) BOOL locked;
@property(nonatomic) BOOL active;
@end

@implementation OYLockedPointerState
@end
#endif

namespace OpenYAMM::Engine
{
ApplePointer::ApplePointer(SDL_Window *pWindow, bool trace)
    : m_pWindow(pWindow), m_trace(trace)
{
#if TARGET_OS_IOS
    UIWindow *window = (__bridge UIWindow *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(pWindow), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    if (window == nil)
    {
        return;
    }
    OYLockedPointerState *state = [OYLockedPointerState new];
    state.cursor = [CAShapeLayer layer];
    UIBezierPath *path = [UIBezierPath bezierPath];
    [path moveToPoint:CGPointMake(0, 0)];
    [path addLineToPoint:CGPointMake(0, 21)];
    [path addLineToPoint:CGPointMake(5, 16)];
    [path addLineToPoint:CGPointMake(9, 24)];
    [path addLineToPoint:CGPointMake(13, 22)];
    [path addLineToPoint:CGPointMake(9, 14)];
    [path addLineToPoint:CGPointMake(16, 14)];
    [path closePath];
    state.cursor.path = path.CGPath;
    state.cursor.fillColor = UIColor.whiteColor.CGColor;
    state.cursor.strokeColor = UIColor.blackColor.CGColor;
    state.cursor.lineWidth = 1.5;
    state.cursor.zPosition = 10000;
    state.cursor.hidden = YES;
    [window.rootViewController.view.layer addSublayer:state.cursor];
    m_pState = (__bridge_retained void *)state;
    SDL_SetBooleanProperty(SDL_GetWindowProperties(pWindow), LockedMenuPointerProperty, true);
#endif
}

ApplePointer::~ApplePointer()
{
#if TARGET_OS_IOS
    if (m_pState != nullptr)
    {
        OYLockedPointerState *state = (__bridge_transfer OYLockedPointerState *)m_pState;
        [state.cursor removeFromSuperlayer];
        SDL_ClearProperty(SDL_GetWindowProperties(m_pWindow), LockedMenuPointerProperty);
        SDL_ClearProperty(SDL_GetWindowProperties(m_pWindow), MouseLookPointerProperty);
        SDL_SetWindowRelativeMouseMode(m_pWindow, false);
    }
#endif
}

void ApplePointer::update()
{
#if TARGET_OS_IOS
    if (m_pState == nullptr)
    {
        return;
    }
    OYLockedPointerState *state = (__bridge OYLockedPointerState *)m_pState;
    UIWindow *window = (__bridge UIWindow *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(m_pWindow), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
    const bool active = UIApplication.sharedApplication.applicationState == UIApplicationStateActive;
    const bool requested = active && SDL_HasMouse()
        && (SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (SDL_GetWindowRelativeMouseMode(m_pWindow) != requested)
    {
        if (!SDL_SetWindowRelativeMouseMode(m_pWindow, requested) && m_trace)
        {
            std::cerr << "[PointerLock] request failed: " << SDL_GetError() << '\n';
        }
    }
    const bool locked = window.windowScene.pointerLockState.isLocked;
    if (m_trace
        && (!state.reported || state.requested != requested || state.locked != locked || state.active != active))
    {
        std::cout << "[PointerLock] requested=" << requested << " locked=" << locked
            << " active=" << active << " scene=" << (window.windowScene != nil) << std::endl;
        state.reported = YES;
        state.requested = requested;
        state.locked = locked;
        state.active = active;
    }
    float x = 0;
    float y = 0;
    SDL_GetMouseState(&x, &y);
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_pWindow, &width, &height);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    state.cursor.hidden = !requested || !locked || windowMouseLookActive(m_pWindow);
    state.cursor.position = CGPointMake(
        std::clamp(x, 0.0f, std::max(0.0f, width - 1.0f)),
        std::clamp(y, 0.0f, std::max(0.0f, height - 1.0f)));
    [CATransaction commit];
#endif
}
}
