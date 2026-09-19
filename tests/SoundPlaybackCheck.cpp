#include "engine/TextTable.h"
#include "game/audio/GameAudioSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace OpenYAMM::Engine
{
struct AudioSystemTestAccess
{
    static bool cached(const AudioSystem &audio, const std::string &path)
    {
        return audio.m_clipCache.contains(path);
    }
    static SDL_AudioDeviceID device(const AudioSystem &audio)
    {
        return SDL_GetAudioStreamDevice(audio.m_pAudioStream);
    }
    static const std::vector<float> &samples(const AudioSystem &audio, const std::string &path)
    {
        return audio.m_clipCache.at(path)->samples;
    }
};
}
namespace OpenYAMM::Game
{
struct GameAudioSystemTestAccess
{
    static void disableIdleCache(GameAudioSystem &audio)
    {
        audio.m_audioSystem.setTransientCacheLimit(0);
    }
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

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::vector<std::vector<std::string>> readTable(const Engine::AssetFileSystem &files, const std::string &name)
{
    const std::optional<std::string> text = files.readTextFile("engine/data_tables/" + name);
    require(text.has_value(), "Missing table " + name);
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(*text);
    require(table.has_value(), "Cannot parse table " + name);
    std::vector<std::vector<std::string>> rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        if (!table->getRow(index).empty() && table->getRow(index) != std::vector<std::string>{""})
        {
            rows.push_back(table->getRow(index));
        }
    }
    return rows;
}

void decodeAll(const Engine::AssetFileSystem &files)
{
    Engine::AudioSystem audio;
    require(audio.initialize(files), "Cannot initialize audio");
    size_t count = 0;
    for (const std::string &name : files.enumerate("audio"))
    {
        if (!name.ends_with(".wav"))
        {
            continue;
        }
        const std::string path = "audio/" + name;
        require(audio.preloadClip(path), "Cannot decode original WAV " + name);
        const std::vector<float> &samples = EngineAccess::samples(audio, path);
        require(!samples.empty() && samples.size() % 2 == 0, "Invalid decoded PCM length: " + name);
        double energy = 0;
        float peak = 0;
        for (float sample : samples)
        {
            require(std::isfinite(sample), "Nonfinite decoded PCM: " + name);
            peak = std::max(peak, std::abs(sample));
            energy += double(sample) * sample;
        }
        require(audio.cacheStats().clipCount == 1, "Unexpected retained sound cache entries");
        std::cout << "{\"name\":" << std::quoted(name) << ",\"decoded_stereo_frames\":" << samples.size() / 2
            << ",\"pcm_peak\":" << peak << ",\"pcm_rms\":" << std::sqrt(energy / samples.size()) << "}\n";
        require(audio.releaseClip(path) && audio.cacheStats().clipCount == 0, "Sound cache release failed");
        ++count;
    }
    require(count > 0, "No original WAV files found");
}

void retention(const Engine::AssetFileSystem &files, size_t requestedCount)
{
    require(requestedCount > 0 && requestedCount <= 1000, "Invalid retention check count");
    Engine::AudioSystem audio;
    require(audio.initialize(files), "Cannot initialize audio");
    size_t played = 0;
    size_t peakBytes = 0;
    for (const std::string &name : files.enumerate("audio"))
    {
        if (!name.ends_with(".wav"))
        {
            continue;
        }
        Engine::AudioSystem::PlaybackOptions options;
        const uint64_t instance = audio.playClip("audio/" + name, options);
        require(instance != 0, "Cannot play retention-check source " + name);
        audio.stopClip(instance);
        audio.update({});
        peakBytes = std::max(peakBytes, audio.cacheStats().sampleBytes);
        if (++played == requestedCount)
        {
            break;
        }
    }
    require(played == requestedCount, "Not enough sound payloads for retention check");
    std::cout << "{\"played_distinct_originals\":" << played << ",\"peak_retained_sample_bytes\":" << peakBytes
        << ",\"settled_clip_count\":" << audio.cacheStats().clipCount
        << ",\"settled_sample_bytes\":" << audio.cacheStats().sampleBytes << "}\n";
}

struct OutputMeter
{
    std::mutex mutex;
    uint64_t count = 0;
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
            ++meter.count;
        }
    }
};

void play(const Engine::AssetFileSystem &files, bool speech, uint32_t id)
{
    Game::CharacterDollTable dolls;
    Game::MergedCharacterVoiceTable voices;
    if (speech)
    {
        require(dolls.loadCharacterRows(readTable(files, "character_data.txt")), "Cannot load characters");
        require(voices.loadFromRows(readTable(files, "character_voices.txt")), "Cannot load voices");
    }
    OutputMeter meter;
    Game::GameAudioSystem audio;
    require(speech ? audio.initialize(files, dolls, voices) : audio.initializeMenuAudio(files),
        "Cannot initialize game audio");
    require(SDL_SetAudioPostmixCallback(EngineAccess::device(GameAccess::engine(audio)),
        &OutputMeter::receive, &meter), "Cannot observe postmix output");
    if (speech)
    {
        require(dolls.getCharacter(id) != nullptr, "Unknown source character ID");
        Game::Character character;
        character.characterDataId = id;
        require(audio.playSpeech(character, Game::SpeechId::Yes, 1, 1), "Cannot play original character speech");
    }
    else
    {
        require(audio.playSound(id, Game::GameAudioSystem::PlaybackGroup::Ui), "Cannot play original sound ID");
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline)
    {
        audio.update(0, 0, 0, 0.005f);
        SDL_Delay(5);
    }
    audio.shutdown();
    require(meter.count > 0 && meter.finite && meter.energy > 0, "No finite nonzero audio reached SDL postmix");
    std::cout << "{\"mode\":\"" << (speech ? "speech" : "sound-id") << "\",\"id\":" << id
        << ",\"postmix_rms\":" << std::sqrt(meter.energy / meter.count)
        << ",\"dummy_output\":true,\"physical_audio_validated\":false}\n";
}

void mapCache(const Engine::AssetFileSystem &files)
{
    Game::GameAudioSystem audio;
    require(audio.initializeMenuAudio(files), "Cannot initialize game audio");
    GameAccess::disableIdleCache(audio);
    const Engine::AudioSystem &engine = GameAccess::engine(audio);
    require(audio.preloadSound(Game::engineSound(31201)), "Cannot preload persistent dialogue");
    audio.beginMapSoundPreload();
    require(audio.preloadSound(Game::engineSound(42186)), "Cannot preload first map sound");
    audio.endMapSoundPreload();
    require(audio.cacheStats().clipCount == 2 && audio.cacheStats().transientSampleBytes == 0,
        "Game preloads were not protected");
    audio.setBackgroundMusicTrack(37);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (audio.currentBackgroundMusicTrack() != 37 && std::chrono::steady_clock::now() < deadline)
    {
        audio.update(0, 0, 0, 0.005f);
        SDL_Delay(5);
    }
    require(audio.currentBackgroundMusicTrack() == 37, "Protected music did not start");
    audio.pauseBackgroundMusic();
    audio.beginMapSoundPreload();
    require(audio.preloadSound(Game::engineSound(42602)), "Cannot preload second map sound");
    audio.endMapSoundPreload();
    require(!EngineAccess::cached(engine, "audio/7rungrass.wav")
        && EngineAccess::cached(engine, "audio/6clickmovingselector.wav")
        && EngineAccess::cached(engine, "audio/01_01.wav") && EngineAccess::cached(engine, "music_track_37"),
        "Map preload replacement lost shared data or retained old map data");
    require(audio.playSound(42186, Game::GameAudioSystem::PlaybackGroup::Walking), "Cannot reload transient sound");
    require(EngineAccess::cached(engine, "audio/7rungrass.wav"), "Active transient sound was evicted");
    audio.stopGroup(Game::GameAudioSystem::PlaybackGroup::Walking);
    require(!EngineAccess::cached(engine, "audio/7rungrass.wav"), "Stopped transient sound exceeded zero quota");
    require(audio.isBackgroundMusicPaused() && audio.currentBackgroundMusicTrack() == 37,
        "Sound cleanup disturbed paused music");
    audio.resumeBackgroundMusic();
    audio.update(0, 0, 0, 0.02f);
    require(audio.currentBackgroundMusicTrack() == 37 && !audio.isBackgroundMusicPaused(),
        "Music did not resume after map cache transition");
    audio.stopAllPlayback();
    require(!EngineAccess::cached(engine, "music_track_37") && audio.cacheStats().clipCount == 2,
        "Stopping music lost preloads or retained music data");
    audio.beginMapSoundPreload();
    audio.endMapSoundPreload();
    require(audio.cacheStats().clipCount == 1 && EngineAccess::cached(engine, "audio/01_01.wav"),
        "Map release did not preserve only persistent preload");
    audio.shutdown();
    require(audio.cacheStats().sampleBytes == 0, "Shutdown retained audio PCM");
    std::cout << "{\"map_cache_music_lifecycle_passed\":true,\"dummy_output\":true,"
        "\"physical_audio_validated\":false}\n";
}
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: sound_playback_check RUNTIME WORLD decode|play|speech|retention|map-cache [ID...|COUNT]\n";
        return 2;
    }
    try
    {
        SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
        Engine::AssetFileSystem files;
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        require(files.initialize(root, root, Engine::AssetScaleTier::X1, argv[2]), "Cannot mount sound assets");
        const std::string mode = argv[3];
        require(mode == "decode" || mode == "play" || mode == "speech" || mode == "retention" || mode == "map-cache",
            "Unsupported check mode");
        if (mode == "decode")
        {
            decodeAll(files);
        }
        else if (mode == "map-cache")
        {
            mapCache(files);
        }
        else if (mode == "retention")
        {
            require(argc == 5, "Retention check needs a sound count");
            retention(files, std::stoul(argv[4]));
        }
        else
        {
            require(argc >= 5, "No sound/character IDs supplied");
            for (int index = 4; index < argc; ++index)
            {
                play(files, mode == "speech", std::stoul(argv[index]));
            }
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Sound playback check failed: " << error.what() << '\n';
        return 1;
    }
}
