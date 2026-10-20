#pragma once

#include "engine/AssetFileSystem.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace OpenYAMM::Game
{
struct HouseVideoPlayerTestAccess;

class HouseVideoPlayer
{
public:
    HouseVideoPlayer();
    ~HouseVideoPlayer();

    HouseVideoPlayer(const HouseVideoPlayer &) = delete;
    HouseVideoPlayer &operator=(const HouseVideoPlayer &) = delete;

    bool initialize();
    void shutdown();
    void stop();
    bool play(const Engine::AssetFileSystem &assetFileSystem, const std::string &videoStem);
    bool play(
        const Engine::AssetFileSystem &assetFileSystem,
        const std::string &videoStem,
        const std::string &videoDirectory);
    bool play(
        const Engine::AssetFileSystem &assetFileSystem,
        const std::string &videoStem,
        const std::string &videoDirectory,
        bool loopPlayback);
    void setAudioVolume(float volume);
    void update(float deltaSeconds);
    bool hasActiveFrame() const;
    bool hasFinishedPlayback() const;
    bgfx::TextureHandle textureHandle() const;
    int videoTextureWidth() const;
    int videoTextureHeight() const;

private:
    struct StreamingSession;

    // Inspect under the wait mutex so tests can stop an actually blocked decoder.
    bool isDecodeWindowBlocked() const;
    bool ensureAudioStream();
    void ensureVideoTexture(int width, int height);
    bool uploadReadyVideoFrame(bool firstFrame);
    void updateAudioQueue();
    std::optional<float> playbackSecondsFromAudioQueue() const;
    void updateFinishedPlaybackState(float deltaSeconds);

    bool m_isInitialized;
    bool m_initializedAudioSubsystem;
    bgfx::TextureHandle m_videoTextureHandle;
    int m_videoTextureWidth;
    int m_videoTextureHeight;
    SDL_AudioStream *m_pAudioStream;
    std::unique_ptr<StreamingSession> m_pStreamingSession;
    std::string m_activeClipKey;
    float m_playbackSeconds;
    uint64_t m_totalQueuedAudioFrames;
    bool m_loopPlayback;
    bool m_playbackStarted;
    bool m_hasActiveFrame;
    bool m_finishedPlayback;
    double m_presentedFrameSeconds;
    uint64_t m_presentedFrameSerial;
    float m_audioVolume;

    friend struct HouseVideoPlayerTestAccess;
};
}
