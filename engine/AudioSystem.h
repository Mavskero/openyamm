#pragma once

#include "engine/AssetFileSystem.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Engine
{
class AudioSystem
{
public:
    struct CacheStats
    {
        size_t clipCount = 0;
        size_t sampleBytes = 0;
        size_t transientSampleBytes = 0;
        size_t evictedClipCount = 0;
    };

    struct ListenerState
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct PlaybackOptions
    {
        float volume = 1.0f;
        bool positional = false;
        bool loop = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;

    bool initialize(const AssetFileSystem &assetFileSystem);
    void shutdown();

    bool preloadClip(const std::string &virtualPath);
    bool registerClip(const std::string &virtualPath, std::vector<float> samples, uint32_t frameCount = 0);
    bool releaseClip(const std::string &virtualPath);
    CacheStats cacheStats() const;
    // Applies to on-demand WAVs. Explicit preloads, registered clips and active instances are protected.
    void setTransientCacheLimit(size_t sampleBytes);
    uint64_t playClip(const std::string &virtualPath, const PlaybackOptions &options);
    void stopClip(uint64_t instanceId);
    void pauseClip(uint64_t instanceId);
    void resumeClip(uint64_t instanceId);
    void setClipVolume(uint64_t instanceId, float volume);
    bool isClipPlaying(uint64_t instanceId) const;
    void clearQueuedAudio();
    void stopAll();
    void update(const ListenerState &listenerState);

private:
    friend struct AudioSystemTestAccess;

    struct AudioClip
    {
        std::string virtualPath;
        std::vector<float> samples;
        uint32_t frameCount = 0;
        bool pinned = false;
        uint64_t lastUseSerial = 0;
    };

    struct PlayingInstance
    {
        uint64_t instanceId = 0;
        std::shared_ptr<AudioClip> pClip;
        uint32_t frameOffset = 0;
        float volume = 1.0f;
        bool positional = false;
        bool loop = false;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool paused = false;
    };

    static constexpr int OutputSampleRate = 48000;
    static constexpr int OutputChannels = 2;
    static constexpr SDL_AudioFormat OutputFormat = SDL_AUDIO_F32LE;
    static constexpr int MixChunkFrames = 1024;
    static constexpr int TargetQueuedBytes = MixChunkFrames * OutputChannels * sizeof(float) * 3;

    std::shared_ptr<AudioClip> loadClip(const std::string &virtualPath, bool pinned = false);
    void trimClipCache();
    bool hasMixableInstances() const;
    void mixNextChunk(const ListenerState &listenerState);
    static void calculateStereoGains(
        const ListenerState &listenerState,
        const PlayingInstance &instance,
        float &leftGain,
        float &rightGain);

    const AssetFileSystem *m_pAssetFileSystem = nullptr;
    SDL_AudioStream *m_pAudioStream = nullptr;
    SDL_AudioSpec m_outputSpec = {};
    uint64_t m_nextInstanceId = 1;
    uint64_t m_cacheUseSerial = 0;
    size_t m_transientCacheLimit = 64 * 1024 * 1024;
    size_t m_cacheEvictions = 0;
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> m_clipCache;
    std::unordered_set<std::string> m_playbackCacheMissWarningPaths;
    std::vector<PlayingInstance> m_playingInstances;
};
}
