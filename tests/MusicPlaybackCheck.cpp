#include "game/audio/GameAudioSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace OpenYAMM::Engine
{
struct AudioSystemTestAccess
{
    static SDL_AudioDeviceID device(const AudioSystem &audio)
    {
        return SDL_GetAudioStreamDevice(audio.m_pAudioStream);
    }
    static uint32_t offset(const AudioSystem &audio)
    {
        return audio.m_playingInstances.at(0).frameOffset;
    }
    static const std::vector<float> &samples(const AudioSystem &audio)
    {
        return audio.m_playingInstances.at(0).pClip->samples;
    }
    static bool looping(const AudioSystem &audio)
    {
        return audio.m_playingInstances.at(0).loop;
    }
};
}

namespace OpenYAMM::Game
{
struct GameAudioSystemTestAccess
{
    static const Engine::AudioSystem &engine(const GameAudioSystem &audio)
    {
        return audio.m_audioSystem;
    }
};
}

namespace
{
using namespace OpenYAMM;
using EngineAccess = Engine::AudioSystemTestAccess;
using GameAccess = Game::GameAudioSystemTestAccess;

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::runtime_error(pMessage);
    }
}

struct OutputMeter
{
    std::mutex mutex;
    uint64_t samples = 0;
    double energy = 0;
    bool finite = true;

    static void SDLCALL receive(void *pOpaque, const SDL_AudioSpec *, float *pBuffer, int bytes)
    {
        OutputMeter &meter = *static_cast<OutputMeter *>(pOpaque);
        std::lock_guard lock(meter.mutex);
        for (int index = 0; index < bytes / int(sizeof(float)); ++index)
        {
            meter.finite = meter.finite && std::isfinite(pBuffer[index]);
            meter.energy += double(pBuffer[index]) * pBuffer[index];
            ++meter.samples;
        }
    }

    void reset()
    {
        std::lock_guard lock(mutex);
        samples = 0;
        energy = 0;
        finite = true;
    }

    double rms()
    {
        std::lock_guard lock(mutex);
        require(samples > 0 && finite, "No finite postmix output received");
        return std::sqrt(energy / samples);
    }
};

void pump(Game::GameAudioSystem &audio, double seconds)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (std::chrono::steady_clock::now() < end)
    {
        audio.update(0, 0, 0, 0.005f);
        SDL_Delay(5);
    }
}

void waitTrack(Game::GameAudioSystem &audio, int track)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (audio.currentBackgroundMusicTrack() != track && std::chrono::steady_clock::now() < deadline)
    {
        pump(audio, 0.01);
    }
    if (audio.currentBackgroundMusicTrack() != track)
    {
        std::cerr << "Expected track " << track << ", current " << audio.currentBackgroundMusicTrack() << '\n';
    }
    require(audio.currentBackgroundMusicTrack() == track, "Music did not reach requested track");
}

void lifecycle(Game::GameAudioSystem &audio, OutputMeter &meter, int first, int second)
{
    audio.setBackgroundMusicTrack(first);
    waitTrack(audio, first);
    pump(audio, 2);
    meter.reset();
    pump(audio, 1);
    const double playingRms = meter.rms();
    require(playingRms > 0.00001, "Decoded music did not reach SDL postmix output");
    audio.pauseBackgroundMusic();
    const uint32_t pausedOffset = EngineAccess::offset(GameAccess::engine(audio));
    pump(audio, 0.3); // Drain already queued output before measuring silence.
    meter.reset();
    pump(audio, 0.3);
    require(meter.rms() == 0 && EngineAccess::offset(GameAccess::engine(audio)) == pausedOffset,
        "Paused music kept advancing or mixing");
    audio.resumeBackgroundMusic();
    meter.reset();
    pump(audio, 0.4);
    require(meter.rms() > 0.00001, "Resumed music stayed silent");
    audio.setMusicVolume(0);
    pump(audio, 0.3);
    meter.reset();
    pump(audio, 0.3);
    require(meter.rms() == 0, "Muted music still reached output");
    audio.setMusicVolume(1);
    meter.reset();
    pump(audio, 0.4);
    require(meter.rms() > 0.00001, "Music volume did not restore output");
    audio.pauseBackgroundMusic();
    audio.setBackgroundMusicTrack(second);
    pump(audio, 0.8);
    require(audio.isBackgroundMusicPaused() && audio.currentBackgroundMusicTrack() == first,
        "Paused track change resumed music prematurely");
    audio.resumeBackgroundMusic();
    waitTrack(audio, second);
    pump(audio, 1.5);
    require(audio.cacheStats().clipCount == 1, "Old decoded music was not evicted after transition");
    // A stopped asynchronous decode must not supersede the new map's request.
    audio.stopBackgroundMusicImmediate();
    audio.setBackgroundMusicTrack(first);
    audio.stopBackgroundMusicImmediate();
    audio.setBackgroundMusicTrack(second);
    waitTrack(audio, second);
    audio.setBackgroundMusicTrack(0);
    waitTrack(audio, 0);
    require(audio.cacheStats().clipCount == 0, "Stopped music remained in the decode cache");
    std::cout << "{\"lifecycle_passed\":true,\"playing_postmix_rms\":" << playingRms << "}\n";
}
}

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        std::cerr << "Usage: music_playback_check RUNTIME WORLD sample|lifecycle|loop TRACK...\n";
        return 2;
    }
    try
    {
        SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
        Engine::AssetFileSystem files;
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, argv[2]), "Cannot mount music assets");
        // Declare meter before audio so shutdown joins callbacks before meter destruction, including on failure.
        OutputMeter meter;
        Game::GameAudioSystem audio;
        require(audio.initializeMenuAudio(files), "Cannot initialize shared game audio");
        require(SDL_SetAudioPostmixCallback(EngineAccess::device(GameAccess::engine(audio)),
            &OutputMeter::receive, &meter), "Cannot observe SDL postmix");
        const std::string mode = argv[3];
        require(mode == "sample" || mode == "lifecycle" || mode == "loop", "Unknown check mode");
        for (int argument = 4; argument < argc; ++argument)
        {
            const int track = std::stoi(argv[argument]);
            audio.setBackgroundMusicTrack(track);
            waitTrack(audio, track);
            const Engine::AudioSystem &engine = GameAccess::engine(audio);
            const std::vector<float> &samples = EngineAccess::samples(engine);
            double energy = 0;
            float peak = 0;
            for (float sample : samples)
            {
                require(std::isfinite(sample), "Nonfinite decoded PCM");
                energy += double(sample) * sample;
                peak = std::max(peak, std::abs(sample));
            }
            require(!samples.empty() && samples.size() % 2 == 0 && energy > 0, "Empty or silent decoded MP3");
            require(EngineAccess::looping(engine), "Background music does not loop");
            require(audio.cacheStats().clipCount == 1, "More than one settled track retained");
            if (mode == "loop")
            {
                const double seconds = double(samples.size()) / 96000;
                require(seconds < 3, "Loop test requires a short explicit fixture");
                uint32_t previousOffset = EngineAccess::offset(engine);
                int wraps = 0;
                const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds * 3.5);
                while (std::chrono::steady_clock::now() < end)
                {
                    pump(audio, 0.005);
                    const uint32_t offset = EngineAccess::offset(engine);
                    wraps += offset < previousOffset ? 1 : 0;
                    previousOffset = offset;
                }
                require(wraps >= 2 && audio.currentBackgroundMusicTrack() == track, "Music failed to loop twice");
            }
            std::cout << "{\"track\":" << track << ",\"decoded_frames\":" << samples.size() / 2
                << ",\"pcm_peak\":" << peak << ",\"pcm_rms\":" << std::sqrt(energy / samples.size())
                << ",\"dummy_output\":true,\"physical_audio_validated\":false}\n";
            audio.stopBackgroundMusicImmediate();
            require(audio.cacheStats().clipCount == 0, "Immediate stop retained decoded music");
        }
        if (mode == "lifecycle")
        {
            require(argc == 6, "Lifecycle check needs exactly two tracks");
            lifecycle(audio, meter, std::stoi(argv[4]), std::stoi(argv[5]));
        }
        audio.shutdown();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Music playback check failed: " << error.what() << '\n';
        return 1;
    }
}
