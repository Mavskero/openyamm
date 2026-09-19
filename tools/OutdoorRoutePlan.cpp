#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/outdoor/OutdoorPathfindingBuilder.h"
#include "game/outdoor/OutdoorMovementController.h"
#include "game/indoor/IndoorGeometryUtils.h"
#include "game/indoor/IndoorPathfindingBuilder.h"
#include "game/pathfinding/PathPlanner.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace
{
using namespace OpenYAMM::Game;

bool probePartyWalk(const MapAssetInfo &map, bx::Vec3 source, bx::Vec3 target,
    const std::filesystem::path &output)
{
    const OutdoorMovementController controller(*map.outdoorMapData, map.outdoorLandMask,
        map.outdoorDecorationCollisionSet, std::nullopt, map.outdoorSpriteObjectCollisionSet);
    OutdoorMoveState state = controller.initializeState(source.x, source.y, source.z);
    std::ofstream stream(output);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << "# Read-only collision probe, not a gameplay save.\n";
    for (unsigned frame = 0; frame < 120; ++frame)
    {
        stream << state.x << '\t' << state.y << '\t' << state.footZ << '\n';
        const float dx = target.x - state.x, dy = target.y - state.y;
        const float distance = std::hypot(dx, dy);
        if (distance < 5.0f && !state.airborne) return true;
        const float speed = distance < 5.0f ? 0.0f : std::min(720.0f, distance * 128.0f);
        OutdoorMoveDebugInfo debug;
        const OutdoorMoveState next = controller.resolveMoveForBody(state, OutdoorBodyDimensions{},
            distance > 0 ? dx / distance * speed : 0, distance > 0 ? dy / distance * speed : 0, 0,
            false, false, false, false, false, 480, 1536, 4000, 1.0f / 128.0f,
            nullptr, std::nullopt, 1.0f, &debug);
        const float moved = std::hypot(next.x - state.x, next.y - state.y);
        if (frame % 12 == 0 || moved < 0.01f)
            std::cout << "Probe: frame=" << frame << " at=" << state.x << ',' << state.y << ',' << state.footZ
                << " next=" << next.x << ',' << next.y << ',' << next.footZ
                << " hit_kind=" << debug.lastHitKind << " model=" << debug.lastHitBModelIndex
                << " face=" << debug.lastHitFaceIndex << " collider=" << debug.lastHitColliderIndex
                << " normal=" << debug.lastHitNormal.x << ',' << debug.lastHitNormal.y << ',' << debug.lastHitNormal.z
                << " step_floor=" << debug.steppedBModelFloor.height
                << " final_floor=" << debug.finalFloor.height << " fall=" << next.fallDistance << '\n';
        if (moved < 0.01f && std::abs(next.footZ - state.footZ) < 0.01f
            && distance >= 5.0f && frame > 12) return false;
        state = next;
    }
    return false;
}

// Diagnostic only: use the party resolver for every proposed edge. Moving NPCs and gameplay
// events still require an actual application run; no generated state is loaded into a game.
std::vector<OutdoorMoveState> planPartyWalk(
    const MapAssetInfo &map, bx::Vec3 source, bx::Vec3 target, bool stableStops)
{
    const OutdoorMovementController controller(*map.outdoorMapData, map.outdoorLandMask,
        map.outdoorDecorationCollisionSet, std::nullopt, map.outdoorSpriteObjectCollisionSet);
    const auto walk = [&](OutdoorMoveState state, float x, float y) -> std::optional<OutdoorMoveState>
    {
        for (unsigned frame = 0; frame < 100; ++frame)
        {
            const float dx = x - state.x;
            const float dy = y - state.y;
            const float distance = std::hypot(dx, dy);
            if (distance < 5.0f && !state.airborne)
            {
                if (stableStops)
                {
                    // The real application continues gravity between injected input frames.
                    // A temporary foothold that slides while idle is not a usable waypoint.
                    OutdoorMoveState settled = state;
                    for (unsigned idle = 0; idle < 16; ++idle)
                    {
                        settled = controller.resolveMoveForBody(settled, OutdoorBodyDimensions{}, 0, 0, 0,
                            false, false, false, false, false, 480, 1536, 4000, 1.0f / 128.0f);
                        if (settled.supportOnWater || settled.supportOnBurning
                            || std::hypot(settled.x - state.x, settled.y - state.y) > 2.0f
                            || std::abs(settled.footZ - state.footZ) > 10.0f) return std::nullopt;
                    }
                    return settled;
                }
                return state;
            }
            const float speed = distance < 5.0f ? 0.0f : std::min(720.0f, distance * 128.0f);
            OutdoorMoveDebugInfo debug;
            const OutdoorMoveState next = controller.resolveMoveForBody(state, OutdoorBodyDimensions{},
                distance > 0 ? dx / distance * speed : 0,
                distance > 0 ? dy / distance * speed : 0, 0,
                false, false, false, false, false, 480, 1536, 4000, 1.0f / 128.0f,
                nullptr, std::nullopt, 1.0f, stableStops ? &debug : nullptr);
            if (next.supportOnWater || next.supportOnBurning || next.fallDistance > 200
                || (stableStops && debug.slopeSlideActive)) return std::nullopt;
            // Match the actual route driver tolerance and allow a short drop to finish landing.
            if (std::hypot(next.x - state.x, next.y - state.y) < 0.01f
                && std::abs(next.footZ - state.footZ) < 0.01f && distance >= 5.0f)
                return std::nullopt;
            state = next;
        }
        return std::nullopt;
    };
    // Keep stacked walkable surfaces distinct; a roof must not replace the entrance below it.
    using Cell = std::tuple<int, int, int>;
    struct Node
    {
        OutdoorMoveState state;
        float cost;
        Cell parent;
    };
    constexpr float grid = 128.0f;
    const auto cellFor = [](float x, float y, float z)
    {
        return Cell{std::lround((x - 64.0f) / grid), std::lround(y / grid), std::lround(z / 64.0f)};
    };
    const auto heuristic = [&](const OutdoorMoveState &state)
    {
        return std::hypot(target.x - state.x, target.y - state.y);
    };
    const OutdoorMoveState initial = controller.initializeState(source.x, source.y, source.z);
    const Cell first = cellFor(initial.x, initial.y, initial.footZ);
    std::map<Cell, Node> nodes;
    nodes.emplace(first, Node{initial, 0, first});
    using Entry = std::pair<float, Cell>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    queue.emplace(heuristic(initial), first);
    const std::array<std::pair<int, int>, 8> directions = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
    unsigned expanded = 0;
    Cell closest = first;
    float closestDistance = heuristic(initial);
    while (!queue.empty() && expanded < 60000)
    {
        const auto [estimate, cell] = queue.top();
        queue.pop();
        const Node current = nodes.at(cell);
        if (estimate > current.cost + heuristic(current.state) + 0.01f) continue;
        ++expanded;
        if (heuristic(current.state) < closestDistance)
        {
            closest = cell;
            closestDistance = heuristic(current.state);
        }
        if (heuristic(current.state) < 180)
        {
            const auto end = walk(current.state, target.x, target.y);
            if (end && std::abs(end->footZ - target.z) < 128)
            {
                std::vector<OutdoorMoveState> path{*end};
                Cell back = cell;
                while (back != first)
                {
                    path.push_back(nodes.at(back).state);
                    back = nodes.at(back).parent;
                }
                path.push_back(initial);
                std::reverse(path.begin(), path.end());
                std::cout << "Party resolver proposal: expanded=" << expanded << " points=" << path.size() << '\n';
                return path;
            }
        }
        for (const auto &direction : directions)
        {
            const float x = (std::get<0>(cell) + direction.first) * grid + 64.0f;
            const float y = (std::get<1>(cell) + direction.second) * grid;
            if (std::abs(x) > 30000 || std::abs(y) > 30000) continue;
            const auto next = walk(current.state, x, y);
            if (!next) continue;
            const Cell nextCell = cellFor(next->x, next->y, next->footZ);
            const float cost = current.cost + std::hypot(next->x - current.state.x, next->y - current.state.y)
                + std::abs(next->footZ - current.state.footZ);
            const auto found = nodes.find(nextCell);
            if (found != nodes.end() && found->second.cost <= cost) continue;
            nodes.insert_or_assign(nextCell, Node{*next, cost, cell});
            queue.emplace(cost + heuristic(*next), nextCell);
        }
        if (expanded % 2000 == 0)
            std::cout << "Party resolver search: expanded=" << expanded << " queued=" << queue.size() << std::endl;
    }
    const auto &best = nodes.at(closest).state;
    std::cout << "Closest reachable party point: " << best.x << "," << best.y << "," << best.footZ
        << " distance=" << closestDistance << " target_z=" << target.z << '\n';
    throw std::runtime_error("No party walking proposal within the search limit");
}
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    try
    {
        const bool doorsOpen = argc == 12 && std::string_view(argv[11]) == "--doors-open";
        const bool stableStops = argc == 12 && std::string_view(argv[11]) == "--party-stable";
        const bool party = stableStops || (argc == 12 && std::string_view(argv[11]) == "--party");
        const bool probe = argc == 12 && std::string_view(argv[11]) == "--party-probe";
        if (argc != 11 && !party && !probe && !doorsOpen)
            throw std::runtime_error("Usage: openyamm_route_plan ROOT WORLD MAP OUTPUT X Y Z TX TY TZ [--party|--party-stable|--party-probe|--doors-open]");
        const std::filesystem::path root = std::filesystem::canonical(argv[1]);
        const std::filesystem::path output = std::filesystem::absolute(argv[4]);
        if (std::filesystem::exists(output)) throw std::runtime_error("Route output must not already exist");
        if (!SDL_Init(SDL_INIT_EVENTS)) throw std::runtime_error(SDL_GetError());
        Engine::AssetFileSystem files;
        if (!files.initialize(root, root, Engine::AssetScaleTier::X1, argv[2]))
            throw std::runtime_error("Cannot mount the existing runtime");
        Game::GameDataLoader loader;
        loader.setActiveWorldId(argv[2]);
        loader.setInitialMapFileName(argv[3]);
        // Full gameplay loading provides the authoritative land mask and decoration colliders.
        // The lightweight headless map omits these and would propose routes across water.
        if (!loader.loadForGameplay(files)) throw std::runtime_error("Cannot load the original map");
        const auto &map = loader.getSelectedMap();
        if (!map) throw std::runtime_error("Route planning could not load the requested map");
        if (map->indoorMapData)
        {
            if (party || probe)
                throw std::runtime_error("Party-resolver modes currently require an outdoor map");
            for (size_t faceIndex = 0; faceIndex < map->indoorMapData->faces.size(); ++faceIndex)
            {
                const Game::IndoorFace &face = map->indoorMapData->faces[faceIndex];
                if (face.cogTriggered != 201 && face.cogTriggered != 501) continue;
                double x = 0.0, y = 0.0, z = 0.0;
                for (uint16_t vertexIndex : face.vertexIndices)
                {
                    const Game::IndoorVertex &vertex = map->indoorMapData->vertices.at(vertexIndex);
                    x += vertex.x;
                    y += vertex.y;
                    z += vertex.z;
                }
                if (!face.vertexIndices.empty())
                {
                    x /= face.vertexIndices.size();
                    y /= face.vertexIndices.size();
                    z /= face.vertexIndices.size();
                }
                std::cout << "Indoor event face: event=" << face.cogTriggered
                    << " face=" << faceIndex << " center=" << x << ',' << y << ',' << z << '\n';
            }
            std::optional<Game::EventRuntimeState> routeEventState = map->eventRuntimeState;
            if (doorsOpen)
            {
                if (!map->indoorMapDeltaData)
                    throw std::runtime_error("Open-door planning requires an indoor map delta");
                if (!routeEventState) routeEventState.emplace();
                for (const Game::MapDeltaDoor &door : map->indoorMapDeltaData->doors)
                {
                    Game::RuntimeMechanismState &mechanism = routeEventState->mechanisms[door.doorId];
                    mechanism.state = static_cast<uint16_t>(Game::EvtMechanismState::Open);
                    mechanism.timeSinceTriggeredMs = 0.0f;
                    mechanism.currentDistance = 0.0f;
                    mechanism.isMoving = false;
                }
            }
            const std::vector<Game::IndoorVertex> vertices = Game::buildIndoorMechanismAdjustedVertices(
                *map->indoorMapData,
                map->indoorMapDeltaData ? &*map->indoorMapDeltaData : nullptr,
                routeEventState ? &*routeEventState : nullptr);
            const Game::IndoorPathMapBuildResult built = Game::IndoorPathfindingBuilder::buildPathMap(
                *map->indoorMapData,
                vertices,
                map->indoorMapDeltaData ? &*map->indoorMapDeltaData : nullptr);
            Game::PathPlanRequest request;
            request.source = {std::stof(argv[5]), std::stof(argv[6]), std::stof(argv[7])};
            request.target = {std::stof(argv[8]), std::stof(argv[9]), std::stof(argv[10])};
            request.object.radius = 37.0f;
            request.object.stepHeight = 128.0f;
            request.object.stepLength = 128.0f;
            request.nodeLimit = 200000;
            request.allowPartialPath = true;
            Game::PathPlanner planner;
            const Game::PathPlanResult result = planner.plan(built.pathMap, request);
            std::ofstream stream(output);
            stream.exceptions(std::ios::failbit | std::ios::badbit);
            stream << "# Indoor planning proposal only; dynamic actors and mechanisms require gameplay verification.\n"
                << "# doors_open=" << doorsOpen << '\n'
                << "# status=" << int(result.status) << " nodes=" << result.analyzedNodeCount
                << " source_valid=" << result.debug.sourceValid << " target_valid=" << result.debug.targetValid
                << " best_distance=" << result.debug.bestDistance2d << '\n';
            for (const Game::PathPoint &point : result.waypoints)
                stream << point.x << '\t' << point.y << '\t' << point.z << '\n';
            std::cout << "Original indoor route proposal: status=" << int(result.status)
                << " nodes=" << result.analyzedNodeCount << " waypoints=" << result.waypoints.size()
                << " best_distance=" << result.debug.bestDistance2d << '\n';
            SDL_Quit();
            return result.status == Game::PathPlanStatus::Success ? 0 : 1;
        }
        if (map->outdoorMapData)
        {
            for (size_t bModelIndex = 0; bModelIndex < map->outdoorMapData->bmodels.size(); ++bModelIndex)
            {
                const Game::OutdoorBModel &bModel = map->outdoorMapData->bmodels[bModelIndex];
                for (size_t faceIndex = 0; faceIndex < bModel.faces.size(); ++faceIndex)
                {
                    const Game::OutdoorBModelFace &face = bModel.faces[faceIndex];
                    if (face.cogTriggeredNumber != 187) continue;
                    Game::OutdoorFaceGeometryData geometry;
                    if (!Game::buildOutdoorFaceGeometry(bModel, bModelIndex, face, faceIndex, geometry, true))
                        continue;
                    std::cout << "Outdoor event face: event=" << face.cogTriggeredNumber
                        << " bmodel=" << bModelIndex << " face=" << faceIndex
                        << " center=" << (geometry.minX + geometry.maxX) * 0.5f << ','
                        << (geometry.minY + geometry.maxY) * 0.5f << ','
                        << (geometry.minZ + geometry.maxZ) * 0.5f << '\n';
                }
            }
        }
        if (!map->outdoorMapData) throw std::runtime_error("Route planning requires an outdoor or indoor map");
        if (!map->outdoorLandMask || !map->outdoorDecorationCollisionSet)
            throw std::runtime_error("Route planning requires the full gameplay water and decoration collision data");
        if (probe)
        {
            const bool reached = probePartyWalk(*map,
                {std::stof(argv[5]), std::stof(argv[6]), std::stof(argv[7])},
                {std::stof(argv[8]), std::stof(argv[9]), std::stof(argv[10])}, output);
            SDL_Quit();
            return reached ? 0 : 1;
        }
        if (party)
        {
            const auto path = planPartyWalk(*map, {std::stof(argv[5]), std::stof(argv[6]), std::stof(argv[7])},
                {std::stof(argv[8]), std::stof(argv[9]), std::stof(argv[10])}, stableStops);
            std::ofstream stream(output);
            stream.exceptions(std::ios::failbit | std::ios::badbit);
            stream << "# Party resolver proposal; moving NPCs/events require actual application verification.\n"
                << "# stable_stops=" << stableStops << "\n";
            for (const auto &point : path) stream << point.x << '\t' << point.y << '\t' << point.footZ << '\n';
            SDL_Quit();
            return 0;
        }
        Game::OutdoorPathMapBuildOptions options;
        options.terrainMode = Game::OutdoorPathTerrainMode::LandOnly;
        const Game::OutdoorPathMapBuildResult built = Game::OutdoorPathfindingBuilder::buildPathMap(
            *map->outdoorMapData, map->outdoorMapDeltaData ? &*map->outdoorMapDeltaData : nullptr,
            nullptr, options, map->outdoorLandMask ? &*map->outdoorLandMask : nullptr);
        Game::PathPlanRequest request;
        request.source = {std::stof(argv[5]), std::stof(argv[6]), std::stof(argv[7])};
        request.target = {std::stof(argv[8]), std::stof(argv[9]), std::stof(argv[10])};
        request.object.radius = 37.0f;
        request.object.stepHeight = 128.0f;
        request.object.stepLength = 128.0f;
        request.nodeLimit = 100000;
        request.allowPartialPath = true;
        Game::PathPlanner planner;
        const Game::PathPlanResult result = planner.plan(built.pathMap, request);
        std::ofstream stream(output);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        stream << "# Planning proposal only; requires normal gameplay movement verification.\n"
            << "# status=" << int(result.status) << " nodes=" << result.analyzedNodeCount
            << " source_valid=" << result.debug.sourceValid << " target_valid=" << result.debug.targetValid
            << " best_distance=" << result.debug.bestDistance2d << '\n';
        for (const Game::PathPoint &point : result.waypoints)
            stream << point.x << '\t' << point.y << '\t' << point.z << '\n';
        std::cout << "Original outdoor route proposal: status=" << int(result.status)
            << " nodes=" << result.analyzedNodeCount << " waypoints=" << result.waypoints.size()
            << " best_distance=" << result.debug.bestDistance2d << '\n';
        SDL_Quit();
        return result.status == Game::PathPlanStatus::Success ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
