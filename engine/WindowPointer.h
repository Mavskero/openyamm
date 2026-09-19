#pragma once

#include <SDL3/SDL.h>

namespace OpenYAMM::Engine
{
inline constexpr const char *LockedMenuPointerProperty = "openyamm.pointer.locked_menus";
inline constexpr const char *MouseLookPointerProperty = "openyamm.pointer.mouse_look";

inline bool hasLockedMenuPointer(SDL_Window *pWindow)
{
    return pWindow != nullptr
        && SDL_GetBooleanProperty(SDL_GetWindowProperties(pWindow), LockedMenuPointerProperty, false);
}

inline bool windowMouseLookActive(SDL_Window *pWindow)
{
    if (pWindow == nullptr)
    {
        return false;
    }
    return hasLockedMenuPointer(pWindow)
        ? SDL_GetBooleanProperty(SDL_GetWindowProperties(pWindow), MouseLookPointerProperty, false)
        : SDL_GetWindowRelativeMouseMode(pWindow);
}

// Pointer confinement remains active in menus. Only camera input changes mode.
inline bool setLockedPointerMouseLook(SDL_Window *pWindow, bool enabled)
{
    if (!hasLockedMenuPointer(pWindow) || windowMouseLookActive(pWindow) == enabled)
    {
        return false;
    }
    SDL_SetBooleanProperty(SDL_GetWindowProperties(pWindow), MouseLookPointerProperty, enabled);
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(pWindow, &width, &height);
    if (width > 0 && height > 0)
    {
        SDL_WarpMouseInWindow(pWindow, width * 0.5f, height * 0.5f);
    }
    return true;
}
}
