#include "doctest/doctest.h"
#include "game/app/GameSession.h"
#include "game/gameplay/GameplayScreenController.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/indoor/IndoorWorldRuntime.h"

using namespace OpenYAMM::Game;

namespace
{
class RestClockWorld : public IndoorWorldRuntime
{
public:
    Party *party() override { return &m_party; }
    const Party *party() const override { return &m_party; }
private:
    Party m_party;
};

struct RestFixture
{
    RestClockWorld world;
    GameSession session;

    explicit RestFixture(float start)
    {
        IndoorWorldRuntime::Snapshot snapshot;
        snapshot.gameMinutes = start;
        world.restoreSnapshot(snapshot);
        session.bindActiveWorldRuntime(&world);
        session.gameplayScreenRuntime().openRestOverlay(false);
    }

    void animate(float step)
    {
        auto &runtime = session.gameplayScreenRuntime();
        for (unsigned frame = 0; frame < 2000 && runtime.restScreenState().mode != GameplayUiController::RestMode::None;
            ++frame)
            GameplayScreenController::updateRestOverlayProgress(runtime, step);
        REQUIRE(runtime.restScreenState().mode == GameplayUiController::RestMode::None);
    }
};
}

TEST_CASE("waiting until dawn reaches six exactly and opens the original daytime house")
{
    for (const float step : {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f, 0.0173f})
    {
        INFO("step = " << step);
        RestFixture fixture(1262.970459f);
        auto &runtime = fixture.session.gameplayScreenRuntime();
        runtime.startRestAction(GameplayUiController::RestMode::Wait, 1800.0f - fixture.world.gameMinutes());
        fixture.animate(step);
        CHECK(fixture.world.gameMinutes() == 1800.0f);
        HouseEntry house;
        house.openHour = 6;
        house.closeHour = 18;
        CHECK(isHouseOpenAtGameMinute(house, fixture.world.gameMinutes()));
    }
}

TEST_CASE("animated short wait and eight hour rest retain their requested end times")
{
    for (const float start : {540.1234f, 1262.970459f, 43200.25f})
    {
        for (const float duration : {5.0f, 60.0f, 480.0f})
        {
            INFO("start = " << start << ", duration = " << duration);
            RestFixture fixture(start);
            fixture.session.gameplayScreenRuntime().beginRestAction(
                duration == 480.0f ? GameplayUiController::RestMode::Heal : GameplayUiController::RestMode::Wait,
                duration, false, false);
            fixture.animate(1.0f / 60.0f);
            CHECK(fixture.world.gameMinutes() == start + duration);
        }
    }
}

TEST_CASE("completing an animated rest early advances only to its original target")
{
    RestFixture fixture(1262.970459f);
    auto &runtime = fixture.session.gameplayScreenRuntime();
    runtime.startRestAction(GameplayUiController::RestMode::Wait, 1800.0f - fixture.world.gameMinutes());
    for (unsigned i = 0; i < 7; ++i)
        GameplayScreenController::updateRestOverlayProgress(runtime, 0.0173f);
    runtime.completeRestAction(false);
    CHECK(fixture.world.gameMinutes() == 1800.0f);
    CHECK(runtime.restScreenState().mode == GameplayUiController::RestMode::None);
    runtime.completeRestAction(false);
    CHECK(fixture.world.gameMinutes() == 1800.0f);
}
