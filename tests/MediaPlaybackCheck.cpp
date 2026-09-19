#include "engine/AssetFileSystem.h"
#include "game/audio/HouseVideoPlayer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace OpenYAMM::Game
{
struct HouseVideoPlayerTestAccess
{
    static uint64_t frames(const HouseVideoPlayer &player)
    {
        return player.m_presentedFrameSerial;
    }
    static double seconds(const HouseVideoPlayer &player)
    {
        return player.m_presentedFrameSeconds;
    }
    static uint64_t audioFrames(const HouseVideoPlayer &player)
    {
        return player.m_totalQueuedAudioFrames;
    }
};
}

int main(int argc, char **argv)
{
    if (argc != 8)
    {
        std::cerr << "Usage: media_playback_check ASSETS WORLD STEM DIRECTORY SECONDS "
            "finish|loop|sample audio|silent\n";
        return 2;
    }
    try
    {
        using namespace OpenYAMM;
        using Access = Game::HouseVideoPlayerTestAccess;
        const auto require = [](bool condition, const char *pMessage)
        {
            if (!condition)
            {
                throw std::runtime_error(pMessage);
            }
        };
        SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
        Engine::AssetFileSystem files;
        const std::filesystem::path assets = std::filesystem::canonical(argv[1]);
        require(files.initialize(assets, assets, Engine::AssetScaleTier::X1, argv[2]), "Cannot mount media");
        const double seconds = std::stod(argv[5]);
        const std::string mode = argv[6];
        require(seconds > 0 && seconds <= 120, "Invalid check duration");
        require(mode == "finish" || mode == "loop" || mode == "sample", "Invalid check mode");
        const std::string audioMode = argv[7];
        require(audioMode == "audio" || audioMode == "silent", "Invalid audio expectation");
        const bool audio = audioMode == "audio";
        Game::HouseVideoPlayer player;
        require(player.initialize() && player.play(files, argv[3], argv[4], mode == "loop"), "Cannot start clip");
        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::duration<double>(seconds + 10);
        double previousTime = 0;
        uint64_t previousSerial = 0;
        double maximumGap = 0;
        while (std::chrono::steady_clock::now() < deadline && !player.hasFinishedPlayback())
        {
            player.update(0.005f);
            const double currentTime = Access::seconds(player);
            const uint64_t serial = Access::frames(player);
            if (serial != previousSerial)
            {
                require(currentTime >= previousTime, "Presentation clock moved backwards");
                maximumGap = std::max(maximumGap, currentTime - previousTime);
                previousTime = currentTime;
                previousSerial = serial;
            }
            if (mode != "finish" && currentTime >= seconds)
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        require(Access::frames(player) >= 2, "No advancing decoded frames");
        if (maximumGap >= 0.5)
        {
            std::cerr << "Maximum frame gap: " << maximumGap << '\n';
        }
        require(maximumGap < 0.5, "Unexpected video presentation gap");
        require(mode == "finish" ? player.hasFinishedPlayback() : Access::seconds(player) >= seconds,
            "Clip did not reach expected duration/end");
        require(audio ? Access::audioFrames(player) > 0 : Access::audioFrames(player) == 0,
            "Audio presence does not match source");
        std::ostringstream report;
        report << "{\"passed\":true,\"clip\":" << std::quoted(argv[3]) << ",\"mode\":" << std::quoted(mode)
            << ",\"frames_presented\":" << Access::frames(player) << ",\"last_frame_seconds\":"
            << Access::seconds(player) << ",\"audio_frames_queued\":" << Access::audioFrames(player)
            << ",\"maximum_frame_gap\":" << maximumGap << ",\"finished\":"
            << (player.hasFinishedPlayback() ? "true" : "false") << ",\"gpu_rendered\":false}\n";
        player.stop();
        require(Access::frames(player) == 0 && !player.hasActiveFrame(), "Stop retained stale playback state");
        require(player.play(files, argv[3], argv[4], false), "Cannot reopen stopped clip");
        std::cout << report.str();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Media playback check failed: " << error.what() << '\n';
        return 1;
    }
}
