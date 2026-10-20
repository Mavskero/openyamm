#include "AppleOutdoorPreview.h"
#include "engine/BgfxContext.h"
#include "engine/EngineApplication.h"
#include "engine/AppleText.h"
#include "engine/TextTable.h"
#include "game/app/GameInputSystem.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/outdoor/OutdoorGameView.h"
#include "game/outdoor/OutdoorGeometryUtils.h"
#include "game/outdoor/OutdoorGameplayInputController.h"
#include "game/scene/OutdoorSceneRuntime.h"
#include "game/ui/MenuScreenBase.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
class PreviewHud final : public OpenYAMM::Game::MenuScreenBase
{
public:
    explicit PreviewHud(const OpenYAMM::Engine::AssetFileSystem &files, size_t actors,
        const std::string &mapName, bool inspection)
        : MenuScreenBase(files), m_actorCount(actors), m_mapName(mapName), m_inspection(inspection)
    {
        setRenderViewId(254);
        setClearBackground(false);
    }
    OpenYAMM::Game::AppMode mode() const override
    {
        return OpenYAMM::Game::AppMode::GameplayOutdoor;
    }
    bool textRendered() const
    {
        return m_textRendered;
    }
private:
    void drawScreen(float) override
    {
        const float scale = std::clamp(float(frameWidth()) / 1280.0f, 1.0f, 2.0f);
        const std::vector<uint8_t> panel = {22, 18, 14, 205};
        drawPixelsBgra("preview-panel", 1, 1, panel, {12 * scale, 12 * scale, 108 * scale, 42 * scale});
        m_textRendered = drawUtf8Text("返回", 22 * scale, 35 * scale, 16 * scale);
        m_textRendered &= drawUtf8Text(m_mapName, 28 * scale, 142 * scale, 14 * scale, 0xffb7ddff);
        const float top = float(frameHeight()) - 120 * scale;
        drawPixelsBgra("preview-panel", 1, 1, panel, {12 * scale, top, 650 * scale, 108 * scale});
        m_textRendered &= drawUtf8Text("左侧拖动移动 · 右侧拖动转向 · WASD / 鼠标 / 方向键",
            18 * scale, 24 * scale, top + 9 * scale);
        if (m_lastScale != scale)
        {
            const std::string status = m_inspection
                ? "资源检查：未执行原始地图 Lua；未开放正常旅行、任务和存档。"
                : "已加载 " + std::to_string(m_actorCount)
                    + " 个角色外观；尚未接入人物 AI、对话、任务和存档。";
            const std::optional<std::vector<std::string>> lines = OpenYAMM::Engine::wrapAppleText(
                status, 18 * scale, 618 * scale);
            if (!lines)
            {
                m_textRendered = false;
                return;
            }
            m_statusLines = *lines;
            m_lastScale = scale;
        }
        float y = top + 39 * scale;
        for (const std::string &line : m_statusLines)
        {
            m_textRendered &= drawUtf8Text(line, 18 * scale, 24 * scale, y, 0xffb7ddff);
            y += 25 * scale;
        }
    }
    size_t m_actorCount;
    std::string m_mapName;
    bool m_inspection = false;
    float m_lastScale = 0;
    std::vector<std::string> m_statusLines;
    bool m_textRendered = false;
};

bool checkBuildingCollision(const OpenYAMM::Game::MapAssetInfo &map, float x, float y, float z)
{
    using namespace OpenYAMM::Game;
    OutdoorMovementController movement(*map.outdoorMapData, map.outdoorLandMask,
        std::nullopt, std::nullopt, std::nullopt);
    // Approach nearby solid vertical faces along their outward normal. Require
    // repeated contact with that exact face, not a stop caused by terrain slope.
    for (size_t modelIndex = 0; modelIndex < map.outdoorMapData->bmodels.size(); ++modelIndex)
    {
        const OutdoorBModel &model = map.outdoorMapData->bmodels[modelIndex];
        for (size_t faceIndex = 0; faceIndex < model.faces.size(); ++faceIndex)
        {
            OutdoorFaceGeometryData face = {};
            if (!buildOutdoorFaceGeometry(model, modelIndex, model.faces[faceIndex], faceIndex, face)
                || std::abs(face.normal.z) > 0.01f || face.maxZ - face.minZ < 192.0f)
            {
                continue;
            }
            const float centerX = (face.minX + face.maxX) * 0.5f;
            const float centerY = (face.minY + face.maxY) * 0.5f;
            if (std::hypot(centerX - x, centerY - y) > 6000.0f)
            {
                continue;
            }
            OutdoorMoveState state = movement.initializeState(centerX + face.normal.x * 200.0f,
                centerY + face.normal.y * 200.0f, z);
            unsigned stoppedFrames = 0;
            for (unsigned step = 0; step < 128; ++step)
            {
                OutdoorMoveDebugInfo debug = {};
                const OutdoorMoveState next = movement.resolveMove(state,
                    -face.normal.x * 768.0f, -face.normal.y * 768.0f, 0,
                    false, false, false, false, false, 480, 1536, 4000, 1.0f / 128.0f, nullptr, 1.0f, &debug);
                const bool stoppedAtWall = debug.lastHitKind == 1 && debug.lastHitBModelIndex == modelIndex
                    && debug.lastHitFaceIndex == faceIndex && std::hypot(next.x - state.x, next.y - state.y) < 0.1f;
                stoppedFrames = stoppedAtWall ? stoppedFrames + 1 : 0;
                state = next;
                if (stoppedFrames == 32)
                {
                    std::cout << "outdoor_collision blocked=1 building=" << modelIndex
                        << " face=" << faceIndex << " x=" << state.x << " y=" << state.y << std::endl;
                    return true;
                }
            }
        }
    }
    return false;
}
}

int runAppleOutdoorPreview(const std::filesystem::path &assets,
    const std::filesystem::path &captureDirectory, bool automated,
    const std::filesystem::path &inspectionBase, const std::string &inspectionMap)
{
    using namespace OpenYAMM;
    Engine::ApplicationConfig config = Engine::ApplicationConfig::createDefault();
    const bool inspection = !inspectionBase.empty();
    if (inspection && inspectionMap != "breach.odm" && inspectionMap != "bralvar.odm")
    {
        throw std::runtime_error("Outdoor inspection capture requires an audited Merge outdoor map");
    }
    config.appName = inspection ? "OpenYAMM Merge resource inspection" : "OpenYAMM New Sorpigal scene preview";
    config.assetRoot = assets.string();
    config.activeWorldId = inspection ? "mmmerge" : "mm6";
    config.verticalSync = true;
    // This scene diagnostic deliberately has no gameplay tables, events or save
    // callbacks. It exercises the production map loader, renderer and movement.
    Game::GameDataLoader emptyTables;
    Game::GameDataRepository data;
    data.bind(emptyTables);
    Game::GameSession session;
    session.bindDataRepository(&data);
    Game::GameInputSystem input;
    Game::GameSettings settings = Game::GameSettings::createDefault();
    settings.controlScheme = Game::ControlScheme::Modern;
    std::optional<Game::MapAssetInfo> map;
    Game::MonsterTable previewMonsters;
    std::unique_ptr<Game::OutdoorPartyRuntime> pParty;
    std::unique_ptr<Game::OutdoorWorldRuntime> pWorld;
    std::unique_ptr<Game::OutdoorSceneRuntime> pScene;
    std::unique_ptr<Game::OutdoorGameView> pView;
    std::unique_ptr<PreviewHud> pHud;
    Engine::AssetFileSystem *pFiles = nullptr;
    unsigned frames = 0;
    unsigned captures = 0;
    uint64_t started = 0;
    float startX = 0.0f;
    float startY = 0.0f;
    float distance = 0.0f;
    const auto quit = []()
    {
        SDL_Event event = {};
        event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&event);
    };
    Engine::EngineApplication application(config,
        [&](Engine::AssetFileSystem &files)
        {
            pFiles = &files;
            Game::MapStatsEntry entry = {};
            entry.worldId = "mm6";
            entry.id = 151;
            entry.canonicalId = "mm6:oute3.odm";
            entry.fileName = "oute3.odm";
            entry.name = "New Sorpigal";
            if (inspection)
            {
                // Explicitly mounted only by this resource diagnostic, with no events or save callbacks.
                if (!files.mountDevelopmentRoot(inspectionBase))
                {
                    throw std::runtime_error("Cannot mount shared inspection tables");
                }
                const auto text = files.readTextFile("engine/data_tables/map_stats.txt");
                const auto table = text ? Engine::TextTable::parseTabSeparated(*text) : std::nullopt;
                std::vector<std::vector<std::string>> rows;
                if (table)
                {
                    for (size_t index = 0; index < table->getRowCount(); ++index)
                    {
                        rows.push_back(table->getRow(index));
                    }
                }
                Game::MapStats registry;
                if (!registry.loadFromRows(rows))
                {
                    throw std::runtime_error("Cannot load inspection registry");
                }
                const Game::MapStatsEntry *pEntry = registry.findByFileName(inspectionMap);
                if (!pEntry || pEntry->worldId != "mmmerge")
                {
                    throw std::runtime_error("Inspection world ownership changed");
                }
                entry = *pEntry;
            }
            map = Game::MapAssetLoader().load(files, entry, data.monsterTable(), data.objectTable(),
                Game::MapLoadPurpose::RenderSurfaces, {.allowSceneYml = false, .allowLegacyCompanion = false});
            if (!map || !map->outdoorMapData || !map->outdoorTerrainTextureAtlas || !map->outdoorBModelTextureSet)
            {
                throw std::runtime_error("Scene preview is missing geometry or textures");
            }
            map->outdoorMapData->skyTexture = inspection ? "sky05" : "6sky05";
            const Game::OutdoorTerrainTextureAtlas &atlas = *map->outdoorTerrainTextureAtlas;
            for (uint8_t tile : map->outdoorMapData->tileMap)
            {
                if (!atlas.tileRegions[tile].isValid)
                {
                    throw std::runtime_error("An active terrain tile has no texture");
                }
            }
            std::cout << "outdoor_resources buildings=" << map->outdoorMapData->bmodels.size()
                << " textures=" << map->outdoorBModelTextureSet->textures.size()
                << " atlas=" << atlas.width << 'x' << atlas.height << std::endl;
            if (files.exists("engine/data_tables/preview_monster_descriptors.txt"))
            {
                const auto readRows = [&](const std::string &path)
                {
                    const std::optional<std::string> text = files.readTextFile(path);
                    const std::optional<Engine::TextTable> table = text
                        ? Engine::TextTable::parseTabSeparated(*text) : std::nullopt;
                    if (!table)
                    {
                        throw std::runtime_error("Missing converted actor table: " + path);
                    }
                    std::vector<std::vector<std::string>> rows;
                    for (size_t i = 0; i < table->getRowCount(); ++i)
                    {
                        rows.push_back(table->getRow(i));
                    }
                    return rows;
                };
                previewMonsters.loadEntriesFromRows(readRows("engine/data_tables/preview_monster_descriptors.txt"));
                previewMonsters.loadDisplayNamesFromRows(readRows("engine/data_tables/preview_monster_names.txt"));
                std::optional<Game::MapAssetInfo> actors = Game::MapAssetLoader().load(files, entry,
                    previewMonsters, data.objectTable(), Game::MapLoadPurpose::ActorPreviews,
                    {.allowSceneYml = false, .allowLegacyCompanion = true});
                if (!actors || !actors->outdoorActorPreviewBillboardSet
                    || actors->outdoorActorPreviewBillboardSet->missingTextureActorCount > 0)
                {
                    throw std::runtime_error("Actor preview has unresolved sprite resources");
                }
                map->outdoorActorPreviewBillboardSet = std::move(actors->outdoorActorPreviewBillboardSet);
            }
            return true;
        },
        [&]()
        {
            if (bgfx::getRendererType() != bgfx::RendererType::Metal)
            {
                return false;
            }
            const Game::OutdoorMovementDriver movement(*map->outdoorMapData,
                map->outdoorLandMask, std::nullopt, std::nullopt, std::nullopt);
            pParty = std::make_unique<Game::OutdoorPartyRuntime>(movement, data.itemTable());
            pWorld = std::make_unique<Game::OutdoorWorldRuntime>();
            pWorld->initialize(map->map, data.monsterTable(), data.monsterProjectileTable(), data.objectTable(),
                data.spellTable(), data.itemTable(), &pParty->party(), pParty.get(), data.standardItemEnchantTable(),
                data.specialItemEnchantTable(), &data.chestTable(), map->outdoorMapData,
                std::nullopt, std::nullopt, std::nullopt);
            pScene = std::make_unique<Game::OutdoorSceneRuntime>(map->map.fileName, map->map, *pParty, *pWorld,
                std::nullopt, std::nullopt);
            pView = std::make_unique<Game::OutdoorGameView>(session);
            if (!pView->initialize(*pFiles, map->map, *map->outdoorMapData, map->outdoorLandMask,
                map->outdoorTileColors, map->outdoorTerrainTextureAtlas, map->outdoorBModelTextureSet,
                std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                map->outdoorActorPreviewBillboardSet, std::nullopt,
                std::nullopt, nullptr, *pScene, settings))
            {
                return false;
            }
            const Game::OutdoorEntity *pStart = nullptr;
            for (const Game::OutdoorEntity &entity : map->outdoorMapData->entities)
            {
                if (entity.name == "Party Start")
                {
                    pStart = &entity;
                    break;
                }
            }
            if (pStart == nullptr)
            {
                throw std::runtime_error("Map has no authored party start");
            }
            pParty->teleportTo(float(pStart->x), float(pStart->y), float(pStart->z));
            pView->setCameraAngles(float(pStart->facing) * 6.28318530718f / 2048.0f, -0.1f);
            startX = pParty->partyX();
            startY = pParty->partyY();
            if (automated && !inspection && !checkBuildingCollision(*map, startX, startY, pParty->partyFootZ()))
            {
                throw std::runtime_error("Building collision check did not stop at a solid face");
            }
            started = SDL_GetTicks();
            pHud = std::make_unique<PreviewHud>(*pFiles, map->outdoorActorPreviewBillboardSet
                ? map->outdoorActorPreviewBillboardSet->billboards.size() : 0,
                inspection ? map->map.name : "新索匹格镇", inspection);
            bgfx::setDebug(BGFX_DEBUG_NONE);
            return true;
        },
        [&](const SDL_Event &event)
        {
            input.handleSdlEvent(event);
            if ((event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
                || (event.type == SDL_EVENT_FINGER_DOWN && event.tfinger.x < 0.10f && event.tfinger.y < 0.12f))
            {
                quit();
            }
        },
        [&](int width, int height, float wheel, float delta)
        {
            ++frames;
            input.updateFromEngineInput(width, height, wheel, settings, false, true, true, false, false);
            Game::GameplayInputFrame frame = input.frame();
            const float step = inspection ? 0.0f : automated ? 1.0f / 60.0f : std::min(delta, 0.05f);
            if (automated)
            {
                frame = {};
                frame.screenWidth = width;
                frame.screenHeight = height;
                frame.actions[Game::keyboardActionIndex(Game::KeyboardAction::Forward)].held =
                    !inspection && frames >= 90 && frames < 210;
            }
            float yaw = pView->cameraYawRadians() - frame.relativeMouseX * 0.002f;
            float pitch = pView->cameraPitchRadians() - frame.relativeMouseY * 0.002f;
            yaw += (float(frame.isScancodeHeld(SDL_SCANCODE_LEFT))
                - float(frame.isScancodeHeld(SDL_SCANCODE_RIGHT))) * step * 1.5f;
            pView->setCameraAngles(yaw, std::clamp(pitch, -1.3f, 1.3f));
            Game::OutdoorGameplayInputController::updateCameraFromInput(*pView, frame, step);
            pView->render(width, height, frame, step);
            distance = std::hypot(pParty->partyX() - startX, pParty->partyY() - startY);
            pHud->renderFrame(width, height, frame, step);
            if (!pHud->textRendered())
            {
                throw std::runtime_error("Unicode preview text rendering failed");
            }
            if (frames == 30 || (automated && !inspection && frames == 240))
            {
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, frames == 30 ? "outdoor-start" : "outdoor-end");
            }
            for (const char *pName : {"outdoor-start", "outdoor-end"})
            {
                const std::optional<Engine::BgfxContext::ScreenshotCapture> capture =
                    Engine::BgfxContext::consumeScreenshot(pName);
                if (capture)
                {
                    std::ofstream stream(captureDirectory / (std::string(pName) + ".ppm"), std::ios::binary);
                    stream << "P6\n" << capture->width << ' ' << capture->height << "\n255\n";
                    for (size_t i = 0; i < capture->bgraPixels.size(); i += 4)
                    {
                        const char rgb[] = {char(capture->bgraPixels[i + 2]), char(capture->bgraPixels[i + 1]),
                            char(capture->bgraPixels[i])};
                        stream.write(rgb, 3);
                    }
                    stream.close();
                    if (!stream)
                    {
                        throw std::runtime_error("Could not write outdoor frame capture");
                    }
                    ++captures;
                    std::cout << pName << " renderer=Metal width=" << width << " height=" << height
                        << " distance=" << distance << " foot_z=" << pParty->partyFootZ()
                        << " airborne=" << pParty->movementState().airborne
                        << " unicode_text=" << pHud->textRendered() << std::endl;
                }
            }
            if ((inspection && captures >= 1) || (automated && frames >= 300) || SDL_GetTicks() - started >= 60000)
            {
                quit();
            }
        },
        [&]()
        {
            pHud.reset();
            pView.reset();
            session.clear();
            Game::MenuScreenBase::shutdownSharedResources();
            pScene.reset();
            pWorld.reset();
            pParty.reset();
        });
    const int result = application.run();
    return result != 0 ? result
        : (captures >= (automated && !inspection ? 2u : 1u)
            && (inspection || !automated || distance > 100.0f) ? 0 : 5);
}
