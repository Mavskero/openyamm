#include "game/app/GprofControl.h"

#include <iostream>

#if defined(__GNUC__) && !defined(__ANDROID__) && !defined(_WIN32) && !defined(__APPLE__)
extern "C" void __gmon_start__(void) __attribute__((weak));
extern "C" void moncontrol(int mode) __attribute__((weak));
#endif

namespace OpenYAMM::Game
{
void setGprofProfilingEnabled(bool enabled)
{
#if defined(__GNUC__) && !defined(__ANDROID__) && !defined(_WIN32) && !defined(__APPLE__)
    static bool profilingEnabled = true;

    if (__gmon_start__ != nullptr && moncontrol != nullptr && profilingEnabled != enabled)
    {
        moncontrol(enabled ? 1 : 0);
        profilingEnabled = enabled;
        std::cerr << "[Gprof] collection " << (enabled ? "enabled" : "disabled") << '\n';
    }
#else
    (void)enabled;
#endif
}

GprofProfilingScope::GprofProfilingScope()
{
    setGprofProfilingEnabled(true);
}

GprofProfilingScope::~GprofProfilingScope()
{
    setGprofProfilingEnabled(false);
}
}
