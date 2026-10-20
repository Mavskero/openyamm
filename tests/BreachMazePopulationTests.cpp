#include "doctest/doctest.h"
#include "game/content/BreachMazePopulation.h"
#include "tools/mmerge_import/BreachMazePopulationResources.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
std::filesystem::path originalSource()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

const BreachMazePopulationProfile &profile()
{
    static const BreachMazePopulationProfile value = loadBreachMazePopulationProfile(originalSource()).value();
    return value;
}

BreachMazeFloorDescriptor descriptor(uint32_t level, uint64_t seed)
{
    BreachMazeFloorDescriptor result;
    result.identity = {1, level, 1};
    result.seeds = deriveBreachMazeFloorSeeds(seed, result.identity);
    result.layout = generateBreachMazeLayout(profile().mazeTemplate, level, result.seeds.layout);
    return result;
}

BreachMazePopulationPlan plan(const BreachMazeFloorDescriptor &floor,
    std::span<const BreachMazeSkillView> perception = {})
{
    return planBreachMazePopulation(profile().catalog, profile().mazeTemplate, floor, perception);
}

// Independent small PRNG oracle: carries only seed/count, consumes trace bounds and never calls
// production population helpers. Constants are the published143/148 integer protocol, not Lua rand.
struct RollOracle
{
    uint64_t state;
    uint64_t count = 0;
    uint64_t next()
    {
        state += UINT64_C(0x9e3779b97f4a7c15);
        ++count;
        uint64_t value = state;
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }
    uint32_t uniform(uint32_t low, uint32_t high)
    {
        const uint64_t size = uint64_t(high) - low + 1;
        const uint64_t rejected = (uint64_t(0) - size) % size;
        uint64_t value = next();
        while (value < rejected)
        {
            value = next();
        }
        return low + uint32_t(value % size);
    }
};

void verifyTrace(const BreachMazeFloorDescriptor &floor, const BreachMazePopulationPlan &result)
{
    RollOracle material{floor.seeds.material};
    RollOracle population{floor.seeds.population};
    std::vector<BreachMazePopulationDraw> populationTrace;
    std::set<uint16_t> materialRooms;
    for (const BreachMazePopulationDraw &draw : result.draws)
    {
        if (draw.stream == BreachMazePopulationStream::Decoration)
        {
            continue;
        }
        RollOracle &oracle = draw.stream == BreachMazePopulationStream::Material ? material : population;
        CHECK(draw.value == oracle.uniform(draw.minimum, draw.maximum));
        CHECK(draw.rawDrawsAfter == oracle.count);
        if (draw.stream == BreachMazePopulationStream::Material)
        {
            CHECK(draw.purpose == BreachMazePopulationRoll::Material);
            CHECK(materialRooms.insert(draw.context).second);
            CHECK(draw.minimum == 1);
            CHECK(draw.maximum == 4);
            CHECK(result.paletteByRoom[draw.context - 1] == draw.value);
        }
        else
        {
            const auto bounds = [&](uint32_t minimum, uint32_t maximum)
            {
                CHECK(draw.minimum == minimum);
                CHECK(draw.maximum == maximum);
            };
            switch (draw.purpose)
            {
                case BreachMazePopulationRoll::DeadEndBranch: bounds(1, 3); break;
                case BreachMazePopulationRoll::UsefulDecoration: bounds(1, 14); break;
                case BreachMazePopulationRoll::TorchWall: bounds(1, floor.identity.level); break;
                case BreachMazePopulationRoll::FloorTrap: bounds(1, 6); break;
                case BreachMazePopulationRoll::ActorX:
                case BreachMazePopulationRoll::ActorY: bounds(10, 50); break;
                case BreachMazePopulationRoll::PlaceholderItem: bounds(177, 186); break;
                case BreachMazePopulationRoll::TreasureLevel:
                    bounds((floor.identity.level + 1) / 2, floor.identity.level);
                    break;
                case BreachMazePopulationRoll::MonsterId:
                    bounds(1, uint32_t(result.candidateMonsterIds.size()));
                    break;
                case BreachMazePopulationRoll::GroupSize:
                {
                    const BreachMazeTemplateCell &source = profile().mazeTemplate.cells[draw.context - 1];
                    const BreachMazeCell &cell = floor.layout.cellAt(source.x, source.y);
                    const size_t openings = std::count(cell.walls.begin(), cell.walls.end(), false);
                    REQUIRE((openings == 3 || openings == 4));
                    bounds(openings == 3 ? 1 : 3, openings == 3 ? 3 : 5);
                    break;
                }
                case BreachMazePopulationRoll::TreasureBudget:
                case BreachMazePopulationRoll::MonsterBudget: break; // Checked independently below.
                default: FAIL("Unexpected draw in the population stream");
            }
            populationTrace.push_back(draw);
        }
    }
    CHECK(material.state == result.materialRandomState);
    CHECK(material.count == result.materialRandomDrawCount);
    CHECK(population.state == result.populationRandomState);
    CHECK(population.count == result.populationRandomDrawCount);
    REQUIRE(populationTrace.size() >= 2);
    CHECK(populationTrace[0].purpose == BreachMazePopulationRoll::TreasureBudget);
    CHECK(populationTrace[1].purpose == BreachMazePopulationRoll::MonsterBudget);
    const uint64_t level = floor.identity.level;
    const uint64_t side = floor.layout.side;
    CHECK(populationTrace[0].minimum == 1);
    CHECK(populationTrace[0].maximum == level * side / 4);
    CHECK(populationTrace[1].minimum == (level - 1) * side / 4);
    CHECK(populationTrace[1].maximum == level * level * side * side / 16);
    CHECK(result.treasureLimit == populationTrace[0].value);
    CHECK(result.initialMonsterBudget == int32_t(populationTrace[1].value));

    // Source call order inside groups and items is stronger than a final-count assertion.
    for (size_t index = 2; index < populationTrace.size(); ++index)
    {
        const BreachMazePopulationDraw &draw = populationTrace[index];
        if (draw.purpose == BreachMazePopulationRoll::GroupSize)
        {
            REQUIRE(index > 0);
            CHECK(populationTrace[index - 1].purpose == BreachMazePopulationRoll::MonsterId);
            REQUIRE(index + 2 * draw.value < populationTrace.size());
            for (uint32_t actor = 0; actor < draw.value; ++actor)
            {
                CHECK(populationTrace[index + 1 + actor * 2].purpose == BreachMazePopulationRoll::ActorX);
                CHECK(populationTrace[index + 2 + actor * 2].purpose == BreachMazePopulationRoll::ActorY);
                CHECK(populationTrace[index + 1 + actor * 2].context == draw.context);
                CHECK(populationTrace[index + 2 + actor * 2].context == draw.context);
            }
        }
        if (draw.purpose == BreachMazePopulationRoll::PlaceholderItem)
        {
            REQUIRE(index + 1 < populationTrace.size());
            CHECK(populationTrace[index + 1].purpose == BreachMazePopulationRoll::TreasureLevel);
            CHECK(populationTrace[index + 1].context == draw.context);
        }
    }
}

const BreachMazePopulationFacePatch *facePatch(const BreachMazePopulationPlan &result, uint32_t face)
{
    const auto found = std::find_if(result.facePatches.begin(), result.facePatches.end(),
        [&](const BreachMazePopulationFacePatch &patch) { return patch.faceIndex == face; });
    return found == result.facePatches.end() ? nullptr : &*found;
}

void verifyOriginalCellRules(const BreachMazeFloorDescriptor &floor, const BreachMazePopulationPlan &result)
{
    const BreachMazeTemplate &mazeTemplate = profile().mazeTemplate;
    const BreachMazeLayout &layout = floor.layout;
    std::array<uint32_t, 16> roomDeadEnds = {};
    std::map<uint16_t, uint32_t> branchByCell;
    for (const BreachMazePopulationDraw &draw : result.draws)
    {
        if (draw.purpose == BreachMazePopulationRoll::DeadEndBranch)
        {
            CHECK(branchByCell.emplace(draw.context, draw.value).second);
        }
    }
    for (const BreachMazeCell &cell : layout.cells)
    {
        const BreachMazeTemplateCell &source = mazeTemplate.cells[cell.templateCellId - 1];
        const int openings = int(std::count(cell.walls.begin(), cell.walls.end(), false));
        const bool endpoint = cell.templateCellId == layout.endpoints.entrance.templateCellId
            || cell.templateCellId == layout.endpoints.exit.templateCellId;
        const auto branch = branchByCell.find(cell.templateCellId);
        if (openings == 1 && !endpoint && roomDeadEnds[cell.room - 1] <= result.treasureLimit)
        {
            ++roomDeadEnds[cell.room - 1];
            CHECK(branch != branchByCell.end());
        }
        else
        {
            CHECK(branch == branchByCell.end());
        }
        if (result.bookcaseCells[cell.templateCellId - 1])
        {
            REQUIRE(branch != branchByCell.end());
            CHECK(branch->second != 2);
            CHECK(openings == 1);
            CHECK_FALSE(endpoint);
            // An opening remains an opening. Its matching hidden main facet still carries Event4.
            for (uint32_t wall : source.wallFaceIndices)
            {
                const BreachMazePopulationFacePatch *pPatch = facePatch(result, wall);
                REQUIRE(pPatch != nullptr);
                REQUIRE(pPatch->texture.has_value());
                CHECK(*pPatch->texture == "bookcas1");
                CHECK(pPatch->eventId == 4);
                CHECK(pPatch->click == true);
                CHECK_FALSE(pPatch->step.has_value());
                CHECK_FALSE(pPatch->secret.has_value());
            }
            for (uint32_t face : source.faceIndices)
            {
                CHECK(facePatch(result, face) != nullptr);
            }
        }
        if (result.traps[cell.templateCellId - 1].placed)
        {
            CHECK(openings == 2);
            CHECK_FALSE(endpoint);
            const BreachMazePopulationFacePatch *pPatch = facePatch(result, source.centralFloorFaceIndex);
            REQUIRE(pPatch != nullptr);
            CHECK(pPatch->eventId == 3);
            CHECK(pPatch->step == true);
            CHECK(pPatch->click == false);
            CHECK_FALSE(result.traps[cell.templateCellId - 1].spellId.has_value());
            CHECK(result.traps[cell.templateCellId - 1].nextShotGameMinute == 0);
        }
        if (openings == 4)
        {
            CHECK(std::count_if(result.items.begin(), result.items.end(), [&](const BreachMazeItemRequest &item)
                { return item.cellId == cell.templateCellId && item.fixedItemId == 0; }) == 1);
        }
    }
    for (const BreachMazeEndpoint &endpoint : {layout.endpoints.entrance, layout.endpoints.exit})
    {
        const BreachMazeTemplateCell &source = mazeTemplate.cells[endpoint.templateCellId - 1];
        const BreachMazeCell &cell = layout.cellAt(source.x, source.y);
        // Valid endpoints are scanned dead ends or corner fallbacks; a corner has at most two openings.
        CHECK(std::count(cell.walls.begin(), cell.walls.end(), false) <= 2);
    }
    CHECK(facePatch(result, layout.endpoints.entrance.faceIndex) == nullptr);
    CHECK(facePatch(result, layout.endpoints.exit.faceIndex) == nullptr);
    CHECK(result.remainingMonsterBudget == result.initialMonsterBudget - int32_t(result.actors.size()));
    CHECK(result.remainingMonsterBudget >= -4);
    CHECK(result.actors.size() <= size_t(result.initialMonsterBudget + 4));
    for (const BreachMazeActorRequest &actor : result.actors)
    {
        const BreachMazeTemplateCell &source = mazeTemplate.cells[actor.cellId - 1];
        const BreachMazeCell &cell = layout.cellAt(source.x, source.y);
        const int openings = int(std::count(cell.walls.begin(), cell.walls.end(), false));
        CHECK(std::binary_search(result.candidateMonsterIds.begin(),
            result.candidateMonsterIds.end(), actor.monsterId));
        CHECK(actor.position.z == -400);
        if (openings == 1)
        {
            CHECK(actor.hitPointMultiplier == 2);
            CHECK(actor.position.x == 1203 * (source.x - 1));
            CHECK(actor.position.y == -1203 * (source.y - 1));
        }
        else
        {
            CHECK((openings == 3 || openings == 4));
            CHECK(actor.hitPointMultiplier == 1);
            CHECK(actor.position.x - 1203 * (source.x - 1) >= 20);
            CHECK(actor.position.x - 1203 * (source.x - 1) <= 100);
            CHECK(actor.position.y + 1203 * (source.y - 1) >= 20);
            CHECK(actor.position.y + 1203 * (source.y - 1) <= 100);
        }
        CHECK(actor.hostilityType == 3);
        CHECK(actor.hostile);
        CHECK_FALSE(actor.showOnMap);
        CHECK(actor.onAlertMap);
        CHECK(actor.guardRadius == 10);
        CHECK(actor.room == 0);
        CHECK(actor.goldDiceCount == 0);
        CHECK(actor.goldDiceSides == 0);
        CHECK_FALSE(actor.rage);
    }
}

std::filesystem::path fixtureRoot()
{
    const std::filesystem::path path = std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/breach-maze-lifecycle-148/population-implementation-001"
        / ("source-fixture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directories(path / "Scripts/Maps"));
    return path;
}
}

TEST_CASE("Breach population source profile reads complete original data and unions kind tables")
{
    const BreachMazePopulationCatalog &catalog = profile().catalog;
    CHECK(catalog.sources.size() == 14);
    REQUIRE(catalog.monsters.size() == 652);
    CHECK(catalog.decorations.size() == 673);
    CHECK(catalog.contentFingerprint == breachMazePopulationCatalogFingerprint(catalog));
    CHECK(catalog.templateFingerprint == breachMazePopulationTemplateFingerprint(profile().mazeTemplate));
    CHECK((catalog.monsters[141].baseKinds & (1u << 5)) != 0);
    CHECK((catalog.monsters[141].bolsterKinds & (1u << 5)) == 0);
    CHECK((catalog.monsters[141].bolsterKinds & (1u << 3)) != 0);
    CHECK_FALSE(catalog.monsters[651].bolsterRow.has_value()); // Reactor is outside the finite pool.
    CHECK(catalog.monsters[651].level == 100);
    CHECK(catalog.bolsterMapRow[3] == "LowerToEqual");
    CHECK(catalog.bolsterMapRow[4] == "x");
    CHECK(catalog.bolsterMapRow[5] == "x");
    CHECK(catalog.bolsterFormulaRows.front()[0] == "Target");
    constexpr std::array<size_t, 3> counts = {106, 210, 285};
    for (uint32_t level = 1; level <= 3; ++level)
    {
        const BreachMazePopulationPlan result = plan(descriptor(level, 173));
        CHECK(result.candidateMonsterIds.size() == counts[level - 1]);
        CHECK_FALSE(std::binary_search(result.candidateMonsterIds.begin(), result.candidateMonsterIds.end(), 142));
        CHECK_FALSE(std::binary_search(result.candidateMonsterIds.begin(), result.candidateMonsterIds.end(), 143));
        for (uint32_t id : result.candidateMonsterIds)
        {
            const BreachMazeMonsterSource &source = catalog.monsters[id - 1];
            CHECK(std::stoul(source.monsterRow[3]) >= level);
            CHECK(std::stoul(source.monsterRow[3]) <= level * 10);
            CHECK(std::stoul(source.monListRow[1]) <= 400);
            CHECK(source.monsterRow[2] == source.monListRow[13]);
            CHECK(source.monsterRow[18].size() > 0); // Raw attack/skills are kept for the shared resolver.
            CHECK(source.bolsterRow.has_value());
        }
    }
}

TEST_CASE("Breach population all finite level and size budgets use floor after square and fixed draw protocol")
{
    RollOracle golden{0};
    CHECK(golden.next() == UINT64_C(0xe220a8397b1dcdaf));
    CHECK(golden.next() == UINT64_C(0x6e789e6aa1b965f4));
    constexpr std::array<std::array<uint32_t, 5>, 3> treasureGoldens = {{{1, 1, 1, 1, 2},
        {2, 2, 2, 2, 4}, {2, 2, 4, 1, 2}}};
    constexpr std::array<std::array<int32_t, 5>, 3> monsterGoldens = {{{0, 0, 0, 0, 0},
        {1, 1, 1, 1, 2}, {6, 2, 3, 3, 25}}};
    for (uint32_t level = 1; level <= 3; ++level)
    {
        std::set<uint8_t> sides;
        for (uint64_t seed = 0; seed < 512 && sides.size() < 5; ++seed)
        {
            BreachMazeFloorDescriptor floor = descriptor(level, seed);
            if (!sides.insert(floor.layout.side).second)
            {
                continue;
            }
            // Explicit controlled streams isolate the population draw protocol from layout generation.
            floor.seeds.material = 0;
            floor.seeds.population = 0;
            floor.seeds.interaction = 0;
            const BreachMazePopulationPlan result = plan(floor);
            CHECK(result.treasureLimit == treasureGoldens[level - 1][floor.layout.side - 4]);
            CHECK(result.initialMonsterBudget == monsterGoldens[level - 1][floor.layout.side - 4]);
            verifyTrace(floor, result);
            verifyOriginalCellRules(floor, result);
            CHECK(result.draws.front().value == 4); // Published first SplitMix64(0) value modulo4, then+1.
            if (level == 2 && floor.layout.side == 5)
            {
                const auto budget = std::find_if(result.draws.begin(), result.draws.end(),
                    [](const BreachMazePopulationDraw &draw)
                    { return draw.purpose == BreachMazePopulationRoll::MonsterBudget; });
                REQUIRE(budget != result.draws.end());
                CHECK(budget->maximum == 6); // floor(2.5^2), not floor(2.5)^2.
            }
            CHECK(result == plan(floor));
        }
        CHECK((sides == std::set<uint8_t>{4, 5, 6, 7, 8}));
    }
}

TEST_CASE("Breach population branches preserve libraries group overshoot doors and source alias")
{
    std::set<std::string> useful;
    bool overshoot = false;
    bool bottle = false;
    bool libraryOpening = false;
    bool alias = false;
    bool materialThree = false;
    bool fourOpenTreasure = false;
    const std::array<BreachMazeSkillView, 1> perception = {{{0, 4}}};
    for (uint64_t seed = 0; seed < 256; ++seed)
    {
        const BreachMazeFloorDescriptor floor = descriptor(uint32_t(seed % 3 + 1), seed);
        const BreachMazePopulationPlan result = plan(floor, perception);
        verifyTrace(floor, result);
        verifyOriginalCellRules(floor, result);
        overshoot |= result.remainingMonsterBudget < 0;
        for (const BreachMazeItemRequest &item : result.items)
        {
            bottle |= item.fixedItemId == 220;
            if (item.fixedItemId == 0)
            {
                CHECK(item.placeholderItemId >= 177);
                CHECK(item.placeholderItemId <= 186);
                CHECK(item.treasureLevel >= (floor.identity.level + 1) / 2);
                CHECK(item.treasureLevel <= floor.identity.level);
                CHECK(item.positiveBonusStrengthAtLeastOne);
                const BreachMazeTemplateCell &source = profile().mazeTemplate.cells[item.cellId - 1];
                const BreachMazeCell &cell = floor.layout.cellAt(source.x, source.y);
                fourOpenTreasure |= std::count(cell.walls.begin(), cell.walls.end(), false) == 4;
            }
            else
            {
                CHECK(item.placeholderItemId == 0); CHECK(item.treasureLevel == 0);
            }
        }
        for (const BreachMazeCell &cell : floor.layout.cells)
        {
            if (result.bookcaseCells[cell.templateCellId - 1])
            {
                const BreachMazeTemplateCell &source = profile().mazeTemplate.cells[cell.templateCellId - 1];
                for (uint32_t wall : source.wallFaceIndices)
                {
                    const BreachMazeTemplateFace &face = profile().mazeTemplate.faces[wall];
                    libraryOpening |= !cell.walls[static_cast<size_t>(*face.wallSide)];
                }
            }
        }
        materialThree |= std::find(result.paletteByRoom.begin(), result.paletteByRoom.end(), 3)
            != result.paletteByRoom.end();
        if (std::find(result.paletteByRoom.begin(), result.paletteByRoom.end(), 3) != result.paletteByRoom.end())
        {
            CHECK(std::binary_search(result.resources.textures.begin(), result.resources.textures.end(), "7trimd"));
        }
        for (size_t index = 0; index < 256; ++index)
        {
            const BreachMazeDecorationPlan &deco = result.decorations[index];
            CHECK(deco.slot == index);
            CHECK(result.lights[index].slot == index);
            CHECK(result.lights[index].position == deco.position);
            CHECK(result.lights[index].off == deco.invisible);
            CHECK(result.lights[index].sourceProperties == profile().catalog.sourceSlots[index].light);
            CHECK_FALSE(deco.showOnMap);
            CHECK_FALSE(result.bookcaseUsed[index]);
            if (deco.invisible)
            {
                CHECK(deco.position == profile().catalog.sourceSlots[index].position);
                CHECK(deco.eventId == 0);
            }
            else if (deco.requestedName != "torch01")
            {
                useful.insert(deco.requestedName);
                CHECK(deco.position.z == -422);
                CHECK(deco.eventId == 20000 + index);
                if (deco.sourceNameCorrected)
                {
                    alias = true;
                    CHECK(deco.requestedName == "trashheap");
                    CHECK(deco.resolvedName == "trasheap");
                    CHECK(deco.decorationId == 138);
                    CHECK(deco.interaction.handler == BreachMazeDecorationHandler::TrashHeap);
                }
                if (deco.interaction.handler == BreachMazeDecorationHandler::Challenge)
                {
                    const uint16_t tier = deco.interaction.eventVariable / 100;
                    REQUIRE(tier >= 2);
                    REQUIRE(tier <= 4);
                    CHECK(deco.interaction.requiresGlobalChallengeLedger);
                    CHECK(deco.interaction.challengeThreshold == 25 * (1u << (tier - 1)));
                    CHECK(deco.interaction.challengeSkillPointReward == (tier == 2 ? 5 : tier == 3 ? 7 : 10));
                }
            }
            else
            {
                CHECK(deco.position.z == -100);
                CHECK(deco.eventId == 0);
                const BreachMazeTemplateCell &source = profile().mazeTemplate.cells[deco.cellId - 1];
                const BreachMazeCell &cell = floor.layout.cellAt(source.x, source.y);
                std::vector<uint32_t> closedWalls;
                std::vector<uint32_t> rolls;
                for (uint32_t wall : source.wallFaceIndices)
                {
                    if (cell.walls[static_cast<size_t>(*profile().mazeTemplate.faces[wall].wallSide)])
                    {
                        closedWalls.push_back(wall);
                    }
                }
                for (const BreachMazePopulationDraw &draw : result.draws)
                {
                    if (draw.context == deco.cellId && draw.purpose == BreachMazePopulationRoll::TorchWall)
                    {
                        rolls.push_back(draw.value);
                    }
                }
                REQUIRE_FALSE(rolls.empty());
                REQUIRE(rolls.size() <= closedWalls.size());
                CHECK(rolls.back() == 1);
                CHECK(std::count(rolls.begin(), rolls.end(), 1) == 1);
                const BreachMazeTemplateFace &selected = profile().mazeTemplate.faces[closedWalls[rolls.size() - 1]];
                CHECK(deco.position.x == selected.sourceBounds[0] + selected.sourceBounds[1]
                    + int32_t(selected.sourceNormal[0] * 20));
                CHECK(deco.position.y == selected.sourceBounds[2] + selected.sourceBounds[3]
                    + int32_t(selected.sourceNormal[1] * 20));
            }
        }
        CHECK(result.decorations[255].invisible); // Finite grid cannot consume all original256 slots.
        CHECK(result.placedDecorationCount <= uint32_t(floor.layout.side) * floor.layout.side);
        CHECK(result.sectorOneDarkness == 26);
    }
    CHECK(useful.size() == 14);
    CHECK(overshoot);
    CHECK(bottle);
    CHECK(libraryOpening);
    CHECK(alias);
    CHECK(materialThree);
    CHECK(fourOpenTreasure);
}

TEST_CASE("Breach population perception is strict and does not consume randomness or choose trap spells early")
{
    const std::array<BreachMazeSkillView, 1> untrained = {{{0, 0}}};
    const std::array<BreachMazeSkillView, 1> equalThreshold = {{{1, 3}}};
    const std::array<BreachMazeSkillView, 1> exceeds = {{{2, 3}}};
    const std::array<BreachMazeSkillView, 2> laterGrandmaster = {{{0, 0}, {0, 4}}};
    bool found = false;
    for (uint64_t seed = 0; seed < 64 && !found; ++seed)
    {
        const BreachMazeFloorDescriptor floor = descriptor(3, seed);
        const BreachMazePopulationPlan hidden = plan(floor, untrained);
        if (std::none_of(hidden.traps.begin(), hidden.traps.end(),
            [](const BreachMazeTrapInitialState &trap) { return trap.placed; })) { continue; }
        found = true;
        const BreachMazePopulationPlan equal = plan(floor, equalThreshold);
        const BreachMazePopulationPlan visible = plan(floor, exceeds);
        const BreachMazePopulationPlan gm = plan(floor, laterGrandmaster);
        CHECK(hidden == equal);
        CHECK(visible == gm);
        CHECK(hidden.draws == visible.draws);
        CHECK(hidden.actors == visible.actors);
        CHECK(hidden.decorations == visible.decorations);
        CHECK(hidden.items == visible.items);
        for (size_t index = 0; index < 256; ++index)
        {
            if (hidden.traps[index].placed)
            {
                CHECK_FALSE(hidden.traps[index].visible);
                CHECK(visible.traps[index].visible);
                CHECK_FALSE(visible.traps[index].spellId.has_value());
            }
        }
        CHECK((visible.interactionRules.trapSpellChoices == std::vector<uint32_t>{2, 15, 24}));
        CHECK(visible.interactionRules.trapSpellSkillLevel == 7);
        CHECK(visible.interactionRules.trapSpellMastery == 3);
        CHECK(visible.interactionRules.trapCooldownGameMinutes == 3);
        CHECK(visible.interactionRules.bookcaseTreasureType == 16);
        CHECK(visible.interactionRules.bookcaseRecipientPartyIndex == 0);
        CHECK(visible.interactionRules.bookcaseTreasureMinimum == 2);
        CHECK(visible.interactionRules.bookcaseTreasureMaximum == 3);
        CHECK(visible.interactionRandomState == floor.seeds.interaction);
    }
    CHECK(found);
}

TEST_CASE("Breach population streams are independent and generation has no caller mutation")
{
    const BreachMazeFloorDescriptor floor = descriptor(2, 491);
    const BreachMazeFloorDescriptor before = floor;
    const BreachMazePopulationCatalog catalogBefore = profile().catalog;
    const BreachMazeTemplate templateBefore = profile().mazeTemplate;
    const BreachMazePopulationPlan first = plan(floor);
    BreachMazeFloorDescriptor changed = floor;
    changed.seeds.material ^= 123;
    const BreachMazePopulationPlan material = plan(changed);
    CHECK(first.actors == material.actors);
    CHECK(first.items == material.items);
    CHECK(first.decorations == material.decorations);
    CHECK(first.traps == material.traps);
    CHECK(first.populationRandomState == material.populationRandomState);
    changed = floor;
    changed.seeds.interaction ^= 567;
    const BreachMazePopulationPlan interaction = plan(changed);
    CHECK(first.paletteByRoom == interaction.paletteByRoom);
    CHECK(first.actors == interaction.actors);
    CHECK(first.items == interaction.items);
    CHECK(first.facePatches == interaction.facePatches);
    CHECK(first.populationRandomState == interaction.populationRandomState);
    CHECK(floor.identity == before.identity);
    CHECK(floor.seeds == before.seeds);
    CHECK(floor.layout == before.layout);
    CHECK(profile().catalog == catalogBefore);
    CHECK(profile().mazeTemplate == templateBefore);
}

TEST_CASE("Breach population corrupt profile layout metadata and invalid finite inputs fail without fallback")
{
    const BreachMazeFloorDescriptor floor = descriptor(1, 41);
    BreachMazeFloorDescriptor broken = floor;
    broken.identity.level = 4;
    CHECK_THROWS_AS(plan(broken), std::invalid_argument);
    broken.identity.level = std::numeric_limits<uint32_t>::max();
    CHECK_THROWS_AS(plan(broken), std::invalid_argument);
    broken = floor;
    broken.identity.generationSerial = 0;
    CHECK_THROWS_AS(plan(broken), std::invalid_argument);
    broken = floor;
    broken.layout.cells[0].room = 16;
    broken.layout.cells[0].walls[0] = false;
    CHECK_THROWS_AS(plan(broken), std::invalid_argument);
    broken = floor;
    broken.layout.finalRandomState ^= 1;
    CHECK_THROWS_AS(plan(broken), std::invalid_argument);
    broken = floor;
    broken.layout.endpoints.entrance.faceIndex = broken.layout.endpoints.exit.faceIndex;
    CHECK_THROWS_AS(plan(broken), std::invalid_argument);
    BreachMazeTemplate badTemplate = profile().mazeTemplate;
    badTemplate.faces[0].sourceBounds[0] += 1;
    CHECK_THROWS_AS(planBreachMazePopulation(profile().catalog, badTemplate, floor), std::invalid_argument);
    BreachMazePopulationCatalog catalog = profile().catalog;
    catalog.monsters[0].baseHitPoints += 1;
    CHECK_THROWS_AS(planBreachMazePopulation(catalog, profile().mazeTemplate, floor), std::invalid_argument);
    catalog = profile().catalog;
    catalog.sourceSlots[255].light.radius += 1;
    CHECK_THROWS_AS(planBreachMazePopulation(catalog, profile().mazeTemplate, floor), std::invalid_argument);
    catalog = profile().catalog;
    catalog.contentFixVersion = 2;
    CHECK_THROWS_AS(planBreachMazePopulation(catalog, profile().mazeTemplate, floor), std::invalid_argument);
    catalog = profile().catalog;
    catalog.monsters.clear();
    catalog.contentFingerprint = breachMazePopulationCatalogFingerprint(catalog);
    CHECK_THROWS_AS(planBreachMazePopulation(catalog, profile().mazeTemplate, floor), std::invalid_argument);
    // Structurally complete but no eligible monster: explicitly rejected, never a substituted actor.
    catalog = profile().catalog;
    for (BreachMazeMonsterSource &monster : catalog.monsters)
    {
        monster.bolsterKinds |= 1u << 5;
    }
    catalog.contentFingerprint = breachMazePopulationCatalogFingerprint(catalog);
    CHECK_THROWS_AS(planBreachMazePopulation(catalog, profile().mazeTemplate, floor), std::invalid_argument);
    catalog = profile().catalog;
    catalog.decorations[138].name = "missing-controlled-decoration";
    catalog.decorations[138].sourceRow[1] = catalog.decorations[138].name;
    catalog.contentFingerprint = breachMazePopulationCatalogFingerprint(catalog);
    CHECK_THROWS_AS(planBreachMazePopulation(catalog, profile().mazeTemplate, floor), std::invalid_argument);
    const std::array<BreachMazeSkillView, 1> badSkill = {{{1, 5}}};
    CHECK_THROWS_AS(plan(floor, badSkill), std::invalid_argument);
    CHECK(plan(floor) == plan(floor));
}

TEST_CASE("Breach population source profile absence and untrusted paths or modified source fail closed")
{
    const std::filesystem::path absent = fixtureRoot();
    CHECK_FALSE(loadBreachMazePopulationProfile(absent).has_value());
    const std::filesystem::path escaped = fixtureRoot();
    std::filesystem::create_symlink(originalSource() / "Scripts/Maps/BrBase.lua", escaped / "Scripts/Maps/BrBase.lua");
    CHECK_THROWS(loadBreachMazePopulationProfile(escaped));
    const std::filesystem::path modified = fixtureRoot();
    REQUIRE(std::filesystem::copy_file(originalSource() / "Scripts/Maps/BrBase.lua",
        modified / "Scripts/Maps/BrBase.lua"));
    std::ofstream append(modified / "Scripts/Maps/BrBase.lua", std::ios::binary | std::ios::app);
    append << "\n-- controlled source mismatch\n";
    append.close();
    CHECK_THROWS(loadBreachMazePopulationProfile(modified));
}

TEST_CASE("Breach population original kind table modification is rejected at the source trust boundary")
{
    const std::filesystem::path candidate = fixtureRoot();
    std::set<std::string> files;
    for (const BreachMazeSourceFingerprint &source : profile().catalog.sources)
    {
        files.insert(source.name.substr(0, source.name.find("::")));
    }
    for (const std::string &file : files)
    {
        std::filesystem::create_directories((candidate / file).parent_path());
        REQUIRE(std::filesystem::copy_file(originalSource() / file, candidate / file));
    }
    std::ofstream changed(candidate / "Data/Tables/Monster Kinds.txt", std::ios::binary | std::ios::app);
    changed << "\ncontrolled source edit\n";
    changed.close();
    CHECK_THROWS(loadBreachMazePopulationProfile(candidate));
}
