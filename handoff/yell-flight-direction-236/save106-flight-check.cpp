#include "game/maps/SaveGame.h"
#include "game/gameplay/ActorFleeReaction.h"
#include "game/gameplay/GameplayActorAiSystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
using namespace OpenYAMM::Game;
static void require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}
static std::vector<char> bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot read codec input");
    return {std::istreambuf_iterator<char>(input), {}};
}
static uint32_t version(const std::vector<char> &data)
{
    require(data.size() >= 12, "Missing save header");
    uint32_t value;
    std::memcpy(&value, data.data() + 8, sizeof(value));
    return value;
}
static void checkLegacy(const GameSaveData &data)
{
    const auto outdoor = [](const auto &world)
    {
        for (const auto &actor : world.mapActors)
            require(!actor.yellFleeMoveDirectionZ, "Legacy outdoor acquired a fixed direction");
    };
    const auto indoor = [](const auto &scene)
    {
        for (const auto &actor : scene.worldRuntime.mapActorAiStates)
            require(!actor.yellFleeMoveDirectionZ, "Legacy indoor acquired a fixed direction");
    };
    outdoor(data.outdoorWorld);
    indoor(data.indoorScene);
    for (const auto &[map, world] : data.outdoorWorldStates) outdoor(world);
    for (const auto &[map, scene] : data.indoorSceneStates) indoor(scene);
    if (data.breachMaze)
        for (const auto &[level, floor] : data.breachMaze->floors) indoor(floor.runtime.scene);
}
static std::optional<float> &slot(GameSaveData &data, size_t index)
{
    if (index == 0) return data.outdoorWorld.mapActors[0].yellFleeMoveDirectionZ;
    if (index == 1) return data.outdoorWorldStates.at("outd3.odm").mapActors[0].yellFleeMoveDirectionZ;
    if (index == 2) return data.indoorScene.worldRuntime.mapActorAiStates[0].yellFleeMoveDirectionZ;
    return data.indoorSceneStates.at("d01.blv").worldRuntime.mapActorAiStates[0].yellFleeMoveDirectionZ;
}
static void checkFlight()
{
    const auto flying = partyYellFleeReaction({0, 0, 0}, {128, 0, 0}, 128, 42, 80, true);
    const auto ground = partyYellFleeReaction({0, 0, 0}, {128, 0, 0}, 128, 42, 80, false);
    require(flying.yellFleeMoveDirectionZ && *flying.yellFleeMoveDirectionZ < 0,
        "Original flying direction should descend from the actor body center");
    require(!ground.yellFleeMoveDirectionZ && ground.moveDirectionX == flying.moveDirectionX
        && ground.moveDirectionY == flying.moveDirectionY && ground.actionSeconds == flying.actionSeconds,
        "Ground Yell or original horizontal movement changed");
    for (float direction : {-0.35f, 0.0f, 0.35f})
    {
        ActorAiFacts actor = {};
        actor.actorId = actor.identity.actorId = 25;
        actor.stats.currentHp = actor.stats.maxHp = 10;
        actor.stats.canFly = true;
        actor.world.active = true;
        actor.runtime.motionState = ActorAiMotionState::Fleeing;
        actor.runtime.animationState = ActorAiAnimationState::Walking;
        actor.runtime.actionSeconds = 1;
        actor.movement.position = {100, 200, 0};
        actor.movement.movementAllowed = true;
        actor.movement.moveDirectionX = 1;
        actor.movement.effectiveMoveSpeed = 180;
        actor.movement.yellFleeMoveDirectionZ = direction;
        actor.target.currentKind = ActorAiTargetKind::Party;
        actor.target.currentPosition = {0, 0, 1000};
        ActorAiFrameFacts frame = {};
        frame.deltaSeconds = frame.fixedStepSeconds = 1.0f / 128.0f;
        frame.party.position = {0, 0, 1000};
        frame.activeActors.push_back(actor);
        const auto result = GameplayActorAiSystem{}.updateActors(frame);
        require(result.actorUpdates.size() == 1, "Missing flying Flee update");
        const auto &movement = result.actorUpdates.front().movementIntent;
        require(movement.action == ActorAiMovementAction::Flee && movement.applyMovement
            && movement.yellFleeMoveDirectionZ == direction && movement.desiredMoveZ == direction
            && movement.desiredMoveX == 1 && movement.moveSpeed == 180,
            "Current high target overwrote fixed Flee direction or horizontal speed");
        frame.activeActors.front().runtime.actionSeconds = 0;
        require(!GameplayActorAiSystem{}.updateActors(frame).actorUpdates.front()
            .movementIntent.yellFleeMoveDirectionZ, "Expired Flee retained its fixed direction");
    }
}
int main(int argc, char **argv)
{
    try
    {
        require(argc == 4, "OLD104 OLD105 NEW_OUTPUT106 required");
        const auto original104 = bytes(argv[1]), original105 = bytes(argv[2]);
        require(version(original104) == 104 && version(original105) == 105, "Genuine old formats required");
        std::string error;
        auto old104 = loadGameDataFromPath(argv[1], error);
        require(bool(old104), "Read104: " + error);
        auto old105 = loadGameDataFromPath(argv[2], error);
        require(bool(old105), "Read105: " + error);
        checkLegacy(*old104);
        checkLegacy(*old105);
        auto save = *old105;
        require(save.outdoorWorld.mapActors.size() >= 2, "Expected existing outdoor actors");
        save.outdoorWorld.mapActors[1].yellFleeMoveDirectionZ.reset();
        save.outdoorWorldStates["outd3.odm"] = save.outdoorWorld;
        save.indoorScene.worldRuntime.mapActorAiStates.resize(2);
        save.indoorScene.worldRuntime.mapActorAiStates[1].actorId = 1;
        save.indoorSceneStates["d01.blv"] = save.indoorScene;
        const std::array<float, 4> directions = {0.31415927f, -0.27182818f, 0.16180339f, 0.0f};
        for (size_t index = 0; index < directions.size(); ++index) slot(save, index) = directions[index];
        require(saveGameDataToPath(argv[3], save, error), "Write106: " + error);
        const auto written = bytes(argv[3]);
        require(version(written) == 106, "Wrong new format header");
        auto loaded = loadGameDataFromPath(argv[3], error);
        require(bool(loaded), "Read106: " + error);
        for (size_t index = 0; index < directions.size(); ++index)
            require(slot(*loaded, index) == directions[index], "Root/cached field did not round trip");
        require(!loaded->outdoorWorld.mapActors[1].yellFleeMoveDirectionZ
            && !loaded->indoorScene.worldRuntime.mapActorAiStates[1].yellFleeMoveDirectionZ,
            "Ordinary actor acquired a fixed direction");
        const std::array<float, 4> bad = {std::numeric_limits<float>::quiet_NaN(), 1.25f,
            std::numeric_limits<float>::infinity(), -1.25f};
        for (size_t index = 0; index < bad.size(); ++index)
        {
            auto invalid = save;
            slot(invalid, index) = bad[index];
            require(!saveGameDataToPath(argv[3], invalid, error), "Writer accepted invalid direction");
            require(bytes(argv[3]) == written, "Rejected write changed its destination");
        }
        for (size_t index : {size_t(0), size_t(2)})
        {
            auto corrupt = written;
            std::array<char, sizeof(float)> marker;
            std::memcpy(marker.data(), &directions[index], sizeof(float));
            const auto found = std::search(corrupt.begin(), corrupt.end(), marker.begin(), marker.end());
            require(found != corrupt.end()
                && std::search(found + 1, corrupt.end(), marker.begin(), marker.end()) == corrupt.end(),
                "New fixture field is not uniquely located");
            const float invalid = index == 0 ? bad[0] : bad[1];
            std::memcpy(&*found, &invalid, sizeof(float));
            const auto path = std::string(argv[3]) + ".corrupt-" + std::to_string(index);
            { std::ofstream output(path, std::ios::binary); output.write(corrupt.data(), corrupt.size()); }
            require(!loadGameDataFromPath(path, error), "Reader accepted corrupt106 direction");
        }
        checkFlight();
        require(bytes(argv[1]) == original104 && bytes(argv[2]) == original105, "Old source bytes changed");
        std::cout << "{\"legacy104_105_read\":true,\"four_snapshot_paths_roundtrip\":true,"
            "\"nullopt_retained\":true,\"invalid_writes_atomic\":true,\"corrupt106_rejected\":true,"
            "\"fixed_flight_z_three_cases\":true,\"expired_marker_cleared\":true,"
            "\"ground_and_horizontal_unchanged\":true,\"old_input_bytes_unchanged\":true}\n";
        return 0;
    }
    catch (const std::exception &exception) { std::cerr << exception.what() << '\n'; return 1; }
}
