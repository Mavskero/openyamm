#pragma once

#include "engine/ApplicationConfig.h"
#include "engine/AssetFileSystem.h"

#include <SDL3/SDL.h>

#include <functional>

namespace OpenYAMM::Engine
{
class EngineApplication
{
public:
    using StartupCallback = std::function<bool(AssetFileSystem &)>;
    using RenderSetupCallback = std::function<bool()>;
    using RenderFrameCallback = std::function<void(int, int, float, float)>;
    using EventCallback = std::function<void(const SDL_Event &)>;
    using ShutdownCallback = std::function<void()>;
    using TextInputActiveCallback = std::function<bool()>;
    using TextInputAreaCallback = std::function<SDL_Rect(int, int)>;

    EngineApplication(
        const ApplicationConfig &config,
        StartupCallback startupCallback,
        RenderSetupCallback renderSetupCallback,
        EventCallback eventCallback,
        RenderFrameCallback renderFrameCallback,
        ShutdownCallback shutdownCallback = {},
        TextInputActiveCallback textInputActiveCallback = {},
        TextInputAreaCallback textInputAreaCallback = {}
    );

    int run() const;
    void setConfiguration(const ApplicationConfig &config);

private:
    bool initializeAssetFileSystem(AssetFileSystem &assetFileSystem) const;
    bool validateConfiguration() const;

    ApplicationConfig m_config;
    StartupCallback m_startupCallback;
    RenderSetupCallback m_renderSetupCallback;
    EventCallback m_eventCallback;
    RenderFrameCallback m_renderFrameCallback;
    ShutdownCallback m_shutdownCallback;
    TextInputActiveCallback m_textInputActiveCallback;
    TextInputAreaCallback m_textInputAreaCallback;
};
}
