#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "engine/AssetFileSystem.h"
#include "engine/AssetScaleTier.h"
#include "game/audio/HouseVideoPlayer.h"

#include <SDL3/SDL.h>
#include <doctest/doctest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>

namespace OpenYAMM::Game
{
struct HouseVideoPlayerTestAccess
{
    static bool isDecodeWindowBlocked(const HouseVideoPlayer &player)
    {
        return player.isDecodeWindowBlocked();
    }

    static bool hasStreamingSession(const HouseVideoPlayer &player)
    {
        return player.m_pStreamingSession != nullptr;
    }

    static uint64_t presentedFrameSerial(const HouseVideoPlayer &player)
    {
        return player.m_presentedFrameSerial;
    }

    static double presentedFrameSeconds(const HouseVideoPlayer &player)
    {
        return player.m_presentedFrameSeconds;
    }
};
}

namespace
{
const char *videoRuntimeRoot()
{
    const char *pRoot = std::getenv("OPENYAMM_VIDEO_TEST_RUNTIME_ROOT");
    return pRoot != nullptr && *pRoot != '\0' ? pRoot : nullptr;
}

std::filesystem::path videoAssetRoot()
{
    const char *pRoot = videoRuntimeRoot();
    return pRoot != nullptr ? std::filesystem::path(pRoot)
                            : std::filesystem::path(OPENYAMM_SOURCE_DIR) / "assets_dev";
}

bool waitForBlockedDecoder(const OpenYAMM::Game::HouseVideoPlayer &player)
{
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (OpenYAMM::Game::HouseVideoPlayerTestAccess::isDecodeWindowBlocked(player))
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
}

TEST_CASE("HouseVideoPlayer streams a complete OGV without predecoding the clip")
{
    SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        sourceRoot,
        videoAssetRoot(),
        OpenYAMM::Engine::AssetScaleTier::X1,
        "mm6"));

    OpenYAMM::Game::HouseVideoPlayer videoPlayer;
    REQUIRE(videoPlayer.initialize());
    REQUIRE(videoPlayer.play(assetFileSystem, "thfpoor", "Videos/Houses", false));

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!videoPlayer.hasFinishedPlayback() && std::chrono::steady_clock::now() < deadline)
    {
        videoPlayer.update(0.1f);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    CHECK(videoPlayer.hasFinishedPlayback());
    videoPlayer.stop();
    CHECK_FALSE(videoPlayer.hasActiveFrame());

    REQUIRE(videoPlayer.play(assetFileSystem, "elemair", "Videos/Houses", false));
    const std::chrono::steady_clock::time_point audioDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!videoPlayer.hasFinishedPlayback() && std::chrono::steady_clock::now() < audioDeadline)
    {
        videoPlayer.update(0.01f);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    CHECK(videoPlayer.hasFinishedPlayback());

    REQUIRE(videoPlayer.play(assetFileSystem, "lwpshp", "Videos/Houses", false));
    const std::chrono::steady_clock::time_point weaponShopDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(6);
    uint64_t presentedFrameSerial = 0;
    std::vector<double> presentedFrameTimestamps;
    std::vector<std::chrono::steady_clock::time_point> presentedFrameWallTimes;

    while (!videoPlayer.hasFinishedPlayback() && std::chrono::steady_clock::now() < weaponShopDeadline)
    {
        videoPlayer.update(0.01f);

        const uint64_t currentFrameSerial =
            OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSerial(videoPlayer);

        if (currentFrameSerial != presentedFrameSerial)
        {
            presentedFrameSerial = currentFrameSerial;
            presentedFrameTimestamps.push_back(
                OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSeconds(videoPlayer));
            presentedFrameWallTimes.push_back(std::chrono::steady_clock::now());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    CHECK(videoPlayer.hasFinishedPlayback());
    REQUIRE_GE(presentedFrameTimestamps.size(), 30);

    for (size_t index = 1; index < presentedFrameTimestamps.size(); ++index)
    {
        CHECK_LE(presentedFrameTimestamps[index] - presentedFrameTimestamps[index - 1], 0.14);
        CHECK_LE(
            std::chrono::duration<double>(presentedFrameWallTimes[index] - presentedFrameWallTimes[index - 1])
                .count(),
            0.2);
    }

    REQUIRE(videoPlayer.play(assetFileSystem, "elemair", "Videos/Houses", true));
    const std::chrono::steady_clock::time_point loopDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    presentedFrameSerial = 0;
    presentedFrameTimestamps.clear();
    presentedFrameWallTimes.clear();

    while (std::chrono::steady_clock::now() < loopDeadline)
    {
        videoPlayer.update(0.01f);

        const uint64_t currentFrameSerial =
            OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSerial(videoPlayer);

        if (currentFrameSerial != presentedFrameSerial)
        {
            presentedFrameSerial = currentFrameSerial;
            presentedFrameTimestamps.push_back(
                OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSeconds(videoPlayer));
            presentedFrameWallTimes.push_back(std::chrono::steady_clock::now());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    CHECK_FALSE(videoPlayer.hasFinishedPlayback());
    CHECK(videoPlayer.play(assetFileSystem, "elemair", "Videos/Houses", true));
    REQUIRE_GE(presentedFrameTimestamps.size(), 20);

    for (size_t index = 1; index < presentedFrameTimestamps.size(); ++index)
    {
        CHECK_LE(presentedFrameTimestamps[index] - presentedFrameTimestamps[index - 1], 0.2);
        CHECK_LE(
            std::chrono::duration<double>(presentedFrameWallTimes[index] - presentedFrameWallTimes[index - 1])
                .count(),
            0.2);
    }

    videoPlayer.stop();
}

TEST_CASE("HouseVideoPlayer streams MM7 event cutscenes")
{
    SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        sourceRoot,
        videoAssetRoot(),
        OpenYAMM::Engine::AssetScaleTier::X1,
        "mm7"));

    OpenYAMM::Game::HouseVideoPlayer videoPlayer;
    REQUIRE(videoPlayer.initialize());
    REQUIRE(videoPlayer.play(assetFileSystem, "arbiter good", "Videos/Cutscenes", false));

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!videoPlayer.hasFinishedPlayback()
        && OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSeconds(videoPlayer) < 1.5
        && std::chrono::steady_clock::now() < deadline)
    {
        videoPlayer.update(0.1f);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    CHECK_FALSE(videoPlayer.hasFinishedPlayback());
    CHECK_GT(OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSerial(videoPlayer), 0);
    CHECK_GE(OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSeconds(videoPlayer), 1.5);
}

TEST_CASE("HouseVideoPlayer stops immediately when a cutscene is skipped before its first update")
{
    SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        OPENYAMM_SOURCE_DIR, videoAssetRoot(), OpenYAMM::Engine::AssetScaleTier::X1,
        videoRuntimeRoot() != nullptr ? "mm6" : "mm7"));

    OpenYAMM::Game::HouseVideoPlayer player;
    REQUIRE(player.initialize());
    REQUIRE(player.play(assetFileSystem,
        videoRuntimeRoot() != nullptr ? "mm6end1" : "arbiter good", "Videos/Cutscenes", false));
    CHECK_EQ(OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSerial(player), 0u);

    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    player.stop();
    CHECK_LT(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(), 2.0);
    CHECK_FALSE(OpenYAMM::Game::HouseVideoPlayerTestAccess::hasStreamingSession(player));
    CHECK_FALSE(player.hasActiveFrame());
    CHECK_FALSE(player.hasFinishedPlayback());
    player.stop();
    player.shutdown();
}

TEST_CASE("HouseVideoPlayer stops a decoder waiting on a full decode window and can replay")
{
    SDL_setenv_unsafe("SDL_AUDIODRIVER", "dummy", 1);
    OpenYAMM::Engine::AssetFileSystem assetFileSystem;
    REQUIRE(assetFileSystem.initialize(
        OPENYAMM_SOURCE_DIR, videoAssetRoot(), OpenYAMM::Engine::AssetScaleTier::X1,
        videoRuntimeRoot() != nullptr ? "mm6" : "mm7"));

    OpenYAMM::Game::HouseVideoPlayer player;
    REQUIRE(player.initialize());
    const char *pClip = videoRuntimeRoot() != nullptr ? "mm6end1" : "arbiter good";
    for (bool loopPlayback : {false, true})
    {
        CAPTURE(loopPlayback);
        REQUIRE(player.play(assetFileSystem, pClip, "Videos/Cutscenes", loopPlayback));
        // Do not update playback: the worker must fill the decode-ahead window.
        // Acquiring the same mutex as the predicate proves it has entered wait.
        REQUIRE(waitForBlockedDecoder(player));
        CHECK_FALSE(player.hasFinishedPlayback());

        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        if (loopPlayback)
        {
            // Screen exit uses shutdown; the previous pass exercises direct stop.
            player.shutdown();
        }
        else
        {
            player.stop();
        }
        CHECK_LT(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(), 2.0);
        CHECK_FALSE(OpenYAMM::Game::HouseVideoPlayerTestAccess::hasStreamingSession(player));
        CHECK_FALSE(OpenYAMM::Game::HouseVideoPlayerTestAccess::isDecodeWindowBlocked(player));
        CHECK_FALSE(player.hasActiveFrame());
        CHECK_EQ(OpenYAMM::Game::HouseVideoPlayerTestAccess::presentedFrameSerial(player), 0u);
    }
}
