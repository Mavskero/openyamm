#pragma once

#include <SDL3/SDL.h>

namespace OpenYAMM::Engine
{
// Owns the iPad pointer lock and its menu cursor for one SDL game window.
class ApplePointer
{
public:
    ApplePointer(SDL_Window *pWindow, bool trace);
    ~ApplePointer();
    ApplePointer(const ApplePointer &) = delete;
    ApplePointer &operator=(const ApplePointer &) = delete;
    void update();

private:
    SDL_Window *m_pWindow;
    void *m_pState = nullptr;
    bool m_trace;
};
}
