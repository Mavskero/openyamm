#include "engine/AudioSystem.h"

#include <iostream>
#include <stdexcept>

namespace OpenYAMM::Engine
{
struct AudioSystemTestAccess
{
    static bool cached(const AudioSystem &audio, const std::string &name)
    {
        return audio.m_clipCache.contains("audio/" + name + ".wav");
    }
};
}

namespace
{
using namespace OpenYAMM::Engine;
using Access = AudioSystemTestAccess;
constexpr size_t ClipBytes = 1024 * 2 * sizeof(float);

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::runtime_error(pMessage);
    }
}

uint64_t start(AudioSystem &audio, const std::string &name, bool loop = false)
{
    AudioSystem::PlaybackOptions options;
    options.loop = loop;
    const uint64_t id = audio.playClip("audio/" + name + ".wav", options);
    require(id != 0, "Cannot play WAV fixture");
    return id;
}

void use(AudioSystem &audio, const std::string &name)
{
    audio.stopClip(start(audio, name));
}
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: audio_cache_check FIXTURES lru|active|pinned|natural-end\n";
        return 2;
    }
    try
    {
        SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
        AssetFileSystem files;
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        require(files.initialize(root, root, AssetScaleTier::X1, "mm6"), "Cannot mount fixtures");
        AudioSystem audio;
        require(audio.initialize(files), "Cannot initialize audio");
        const std::string mode = argv[2];
        if (mode == "lru")
        {
            audio.setTransientCacheLimit(2 * ClipBytes);
            use(audio, "a");
            use(audio, "b");
            require(audio.cacheStats().sampleBytes == 2 * ClipBytes, "WAV fixture sizes differ");
            use(audio, "a");
            use(audio, "c");
            require(Access::cached(audio, "a") && Access::cached(audio, "c") && !Access::cached(audio, "b"),
                "Cache did not retain the most recently used clips");
            require(audio.cacheStats().evictedClipCount == 1, "Unexpected eviction count");
            use(audio, "b");
            require(Access::cached(audio, "b") && !Access::cached(audio, "a"), "Evicted WAV did not reload");
            require(audio.cacheStats().transientSampleBytes == 2 * ClipBytes, "Transient cache exceeds quota");
        }
        else if (mode == "active")
        {
            audio.setTransientCacheLimit(0);
            const uint64_t a = start(audio, "a", true);
            const uint64_t secondA = start(audio, "a", true);
            audio.pauseClip(a);
            const uint64_t b = start(audio, "b", true);
            audio.update({});
            require(Access::cached(audio, "a") && Access::cached(audio, "b"), "Active or paused clip evicted");
            require(audio.cacheStats().transientSampleBytes == 2 * ClipBytes, "Shared instances duplicate PCM");
            audio.stopClip(b);
            audio.stopClip(secondA);
            require(Access::cached(audio, "a") && !Access::cached(audio, "b"), "Paused instance lost its data");
            audio.resumeClip(a);
            require(audio.isClipPlaying(a), "Protected instance could not resume");
            audio.stopClip(a);
            require(audio.cacheStats().sampleBytes == 0, "Stopped clips exceed zero idle quota");
        }
        else if (mode == "pinned")
        {
            audio.setTransientCacheLimit(0);
            require(audio.preloadClip("audio/a.wav"), "Preload failed");
            require(audio.registerClip("external-music", std::vector<float>(4096, 0.25f)), "Registration failed");
            use(audio, "b");
            audio.stopAll();
            require(Access::cached(audio, "a") && audio.cacheStats().clipCount == 2,
                "Pinned preload or externally registered clip evicted");
            require(audio.cacheStats().transientSampleBytes == 0, "Pinned clips consume transient quota");
            require(audio.releaseClip("audio/a.wav") && audio.releaseClip("external-music"),
                "Explicit release failed for protected clips");
            require(audio.cacheStats().sampleBytes == 0, "Explicit release retained pinned data");
            audio.setTransientCacheLimit(ClipBytes);
            use(audio, "c");
            require(audio.preloadClip("audio/c.wav"), "Cannot promote cached sound to preload");
            audio.setTransientCacheLimit(0);
            require(Access::cached(audio, "c"), "Cached preload promotion did not protect data");
        }
        else if (mode == "natural-end")
        {
            audio.setTransientCacheLimit(0);
            const uint64_t id = start(audio, "a");
            audio.update({});
            require(!audio.isClipPlaying(id), "One-shot did not finish in its mix chunk");
            require(!Access::cached(audio, "a"), "Naturally finished sound remained over quota");
        }
        else
        {
            throw std::runtime_error("Unsupported cache check mode");
        }
        std::cout << "Audio cache " << mode << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
