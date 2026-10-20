#include "game/content/BreachMazePopulation.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint64_t ActorTag = 0x4143544f52530001ULL;
constexpr uint64_t ItemTag = 0x4954454d535f0001ULL;
constexpr uint64_t DecorationTag = 0x4445434f525f0001ULL;
constexpr uint8_t ExcludedKinds = (1u << 2) | (1u << 4) | (1u << 5);
constexpr std::array<std::array<std::string_view, 4>, 4> Materials = {{
    {"7cdb", "7c1fl", "7cdb", "7c1fl"}, {"cwb", "7c1fl", "cwb", "cwb1"},
    {"7t1s2bs4", "7tfb1", "7trim9_32", "7trimd"}, {"t65c02", "t65b03", "t65b10", "t65a03"},
}};
constexpr std::array<std::string_view, 14> UsefulDecorations = {
    "crystl0", "crclstr", "crys5", "crys6", "trashheap", "bag01", "Bucket", "floursac",
    "bigbarel", "7dec03", "7dec10", "dec62", "dec61", "dec63"};

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::invalid_argument(std::string("Breach maze population: ") + pMessage);
    }
}

std::string lower(std::string value)
{
    for (char &letter : value)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return value;
}

uint32_t integer(std::string_view text)
{
    uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size(), "invalid source integer");
    return value;
}

uint64_t mix(uint64_t value)
{
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

uint64_t resolveKey(uint64_t seed, uint64_t tag, uint64_t index)
{
    return mix(mix(seed ^ tag) ^ index);
}

class Fingerprint
{
public:
    void number(uint64_t value)
    {
        for (size_t shift = 0; shift < 64; shift += 8)
        {
            m_value = (m_value ^ uint8_t(value >> shift)) * 1099511628211ULL;
        }
    }
    void string(std::string_view value)
    {
        number(value.size());
        for (unsigned char byte : value)
        {
            m_value = (m_value ^ byte) * 1099511628211ULL;
        }
    }
    template <typename Values> void strings(const Values &values)
    {
        number(values.size());
        for (const std::string &value : values)
        {
            string(value);
        }
    }
    void point(const BreachMazeHalfPoint &value)
    {
        number(uint32_t(value.x));
        number(uint32_t(value.y));
        number(uint32_t(value.z));
    }
    uint64_t value() const
    {
        return m_value;
    }
private:
    uint64_t m_value = 14695981039346656037ULL;
};

class PopulationRandom
{
public:
    PopulationRandom(uint64_t seed, BreachMazePopulationStream stream, std::vector<BreachMazePopulationDraw> &draws)
        : m_state(seed), m_stream(stream), m_draws(draws)
    {
    }

    uint32_t draw(uint32_t minimum, uint32_t maximum, BreachMazePopulationRoll purpose, uint16_t context = 0)
    {
        require(minimum <= maximum && maximum < std::numeric_limits<uint32_t>::max(), "invalid random bounds");
        const uint64_t bound = uint64_t(maximum) - minimum + 1;
        const uint64_t threshold = (uint64_t{0} - bound) % bound;
        uint64_t value;
        do
        {
            m_state += 0x9e3779b97f4a7c15ULL;
            ++m_count;
            value = mix(m_state);
        } while (value < threshold);
        const uint32_t result = minimum + uint32_t(value % bound);
        m_draws.push_back({m_stream, purpose, context, minimum, maximum, result, m_count});
        return result;
    }
    uint64_t state() const
    {
        return m_state;
    }
    uint64_t count() const
    {
        return m_count;
    }
private:
    uint64_t m_state;
    uint64_t m_count = 0;
    BreachMazePopulationStream m_stream;
    std::vector<BreachMazePopulationDraw> &m_draws;
};

const BreachMazeDecorationSource &decoration(const BreachMazePopulationCatalog &catalog, std::string_view name)
{
    const std::string normalized = lower(std::string(name));
    const auto found = std::find_if(catalog.decorations.begin(), catalog.decorations.end(),
        [&](const BreachMazeDecorationSource &entry) { return lower(entry.name) == normalized; });
    require(found != catalog.decorations.end(), "required decoration is missing; cannot reroll");
    return *found;
}

void validateCatalog(const BreachMazePopulationCatalog &catalog, const BreachMazeTemplate &mazeTemplate)
{
    require(catalog.version == BreachMazePopulationVersion
        && catalog.contentFixVersion == BreachMazePopulationContentFixVersion, "unsupported source profile version");
    require(catalog.contentFingerprint == breachMazePopulationCatalogFingerprint(catalog),
        "catalog fingerprint mismatch");
    require(mazeTemplate.version == BreachMazeTemplateVersion && mazeTemplate.vertexCount == 4224
        && mazeTemplate.faces.size() == 5632 && mazeTemplate.decorationCount == 256 && mazeTemplate.lightCount == 256,
        "unexpected original template counts");
    require(catalog.templateFingerprint == breachMazePopulationTemplateFingerprint(mazeTemplate),
        "metadata does not match catalog template");
    require(catalog.sources.size() >= 10, "missing authenticated source identities");
    std::string previous;
    for (const BreachMazeSourceFingerprint &source : catalog.sources)
    {
        require(source.name > previous && source.sha256.size() == 64
            && source.sha256.find_first_not_of("0123456789abcdef") == std::string::npos, "invalid source identities");
        previous = source.name;
    }
    require(catalog.monsters.size() == 652, "incomplete original monster catalog");
    for (size_t index = 0; index < catalog.monsters.size(); ++index)
    {
        const BreachMazeMonsterSource &monster = catalog.monsters[index];
        require(monster.id == index + 1 && integer(monster.monsterRow[0]) == monster.id
            && integer(monster.monListRow[0]) == index && integer(monster.monsterRow[3]) == monster.level
            && integer(monster.monsterRow[4]) == monster.baseHitPoints
            && integer(monster.monListRow[1]) == monster.height
            && lower(monster.monsterRow[2]) == lower(monster.monListRow[13]), "inconsistent monster catalog row");
        require(monster.id <= 198 || monster.baseKinds == 0, "base kind policy is restricted to original table range");
        require(monster.bolsterRow.has_value() == (monster.id <= 651), "incomplete bolster input");
    }
    require(!catalog.decorations.empty(), "missing original decoration catalog");
    std::vector<std::string> names;
    for (size_t index = 0; index < catalog.decorations.size(); ++index)
    {
        const BreachMazeDecorationSource &entry = catalog.decorations[index];
        require(entry.id == index && integer(entry.sourceRow[0]) == index && entry.name == entry.sourceRow[1]
            && entry.spriteGroup == lower(entry.sourceRow[7]), "inconsistent decoration source row");
        names.push_back(lower(entry.name));
    }
    std::sort(names.begin(), names.end());
    require(std::adjacent_find(names.begin(), names.end()) == names.end(), "ambiguous decoration names");
    for (const BreachMazePopulationSourceSlot &slot : catalog.sourceSlots)
    {
        require(slot.decorationId < catalog.decorations.size(), "source slot decoration out of range");
    }
    require(catalog.bolsterMapRow.size() >= 7 && catalog.bolsterMapRow[0] == "206"
        && catalog.bolsterMapRow[3] == "LowerToEqual" && !catalog.bolsterFormulaRows.empty(),
        "missing BrBase shared combat resolver inputs");
    // Validate every possible draw before consuming random state. Alias is a versioned source correction.
    for (std::string_view name : UsefulDecorations)
    {
        decoration(catalog, name == "trashheap" ? "trasheap" : name);
    }
    decoration(catalog, "torch01");
}

BreachMazeHalfPoint center(const BreachMazeTemplateCell &cell)
{
    return {1203 * (int32_t(cell.x) - 1), -1203 * (int32_t(cell.y) - 1), -400};
}

template <typename T> void sortUnique(std::vector<T> &values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
}

uint64_t breachMazePopulationTemplateFingerprint(const BreachMazeTemplate &mazeTemplate)
{
    Fingerprint hash;
    hash.number(1);
    hash.number(mazeTemplate.version);
    hash.number(mazeTemplate.vertexCount);
    hash.number(mazeTemplate.decorationCount);
    hash.number(mazeTemplate.lightCount);
    hash.number(mazeTemplate.faces.size());
    for (const BreachMazeTemplateFace &face : mazeTemplate.faces)
    {
        hash.number(face.faceIndex);
        hash.number(face.cellId);
        hash.number(static_cast<uint8_t>(face.role));
        hash.number(face.wallSide ? 1 + static_cast<uint8_t>(*face.wallSide) : 0);
        for (int16_t value : face.sourceBounds)
        {
            hash.number(uint16_t(value));
        }
        for (float value : face.sourceNormal)
        {
            hash.number(std::bit_cast<uint32_t>(value));
        }
    }
    for (const BreachMazeTemplateCell &cell : mazeTemplate.cells)
    {
        hash.number(cell.cellId);
        hash.number(cell.x);
        hash.number(cell.y);
        hash.number(cell.faceIndices.size());
        for (uint32_t value : cell.faceIndices)
        {
            hash.number(value);
        }
        for (uint32_t value : cell.wallFaceIndices)
        {
            hash.number(value);
        }
        hash.number(cell.centralFloorFaceIndex);
    }
    return hash.value();
}

uint64_t breachMazePopulationCatalogFingerprint(const BreachMazePopulationCatalog &catalog)
{
    Fingerprint hash;
    hash.number(1);
    hash.number(catalog.version);
    hash.number(catalog.contentFixVersion);
    hash.number(catalog.templateFingerprint);
    hash.number(catalog.sources.size());
    for (const BreachMazeSourceFingerprint &source : catalog.sources)
    {
        hash.string(source.name);
        hash.string(source.sha256);
    }
    hash.number(catalog.monsters.size());
    for (const BreachMazeMonsterSource &monster : catalog.monsters)
    {
        hash.number(monster.id);
        hash.number(monster.level);
        hash.number(monster.baseHitPoints);
        hash.number(monster.height);
        hash.number(monster.radius);
        hash.number(monster.moveSpeed);
        hash.number(uint16_t(monster.toHitRadius));
        hash.number(monster.tintArgb);
        hash.number(monster.baseKinds);
        hash.number(monster.bolsterKinds);
        for (uint16_t value : monster.soundIds)
        {
            hash.number(value);
        }
        hash.strings(monster.spriteGroups);
        hash.strings(monster.monsterRow);
        hash.strings(monster.monListRow);
        hash.number(monster.bolsterRow.has_value());
        if (monster.bolsterRow)
        {
            hash.strings(*monster.bolsterRow);
        }
    }
    hash.number(catalog.decorations.size());
    for (const BreachMazeDecorationSource &entry : catalog.decorations)
    {
        hash.number(entry.id);
        hash.string(entry.name);
        hash.string(entry.spriteGroup);
        hash.number(entry.height);
        hash.number(entry.radius);
        hash.number(entry.lightRadius);
        hash.number(entry.soundId);
        hash.number(entry.noBlockMovement);
        hash.strings(entry.sourceRow);
    }
    for (const BreachMazePopulationSourceSlot &slot : catalog.sourceSlots)
    {
        hash.number(slot.decorationId);
        hash.point(slot.position);
        hash.number(uint32_t(slot.facing));
        hash.number(slot.attributes);
        hash.number(uint16_t(slot.light.radius));
        for (uint8_t value : slot.light.color)
        {
            hash.number(value);
        }
        hash.number(slot.light.type);
        hash.number(uint16_t(slot.light.attributes));
        hash.number(uint16_t(slot.light.brightness));
        hash.number(uint32_t(slot.light.id));
    }
    hash.strings(catalog.bolsterMapRow);
    hash.number(catalog.bolsterFormulaRows.size());
    for (const std::vector<std::string> &row : catalog.bolsterFormulaRows)
    {
        hash.strings(row);
    }
    return hash.value();
}

BreachMazePopulationPlan planBreachMazePopulation(const BreachMazePopulationCatalog &catalog,
    const BreachMazeTemplate &mazeTemplate, const BreachMazeFloorDescriptor &floor,
    std::span<const BreachMazeSkillView> perceptionAtCreation)
{
    const uint32_t level = floor.identity.level;
    require(level >= 1 && level <= BreachMazeMaximumPlannedLevel, "population level exceeds the planning protocol");
    require(floor.identity.epoch > 0 && floor.identity.generationSerial > 0, "invalid floor identity");
    validateCatalog(catalog, mazeTemplate);
    const BreachMazeLayout &layout = floor.layout;
    require(layout.level == level && layout.seed == floor.seeds.layout
        && layout == generateBreachMazeLayout(mazeTemplate, level, floor.seeds.layout),
        "layout differs from its versioned seed, identity or metadata");
    require(layout.side >= 4 && layout.side <= BreachMazePopulationMaximumSide, "unsupported population side");
    require(perceptionAtCreation.size() <= 5, "perception party exceeds original five slots");
    bool canSeeTraps = false;
    for (const BreachMazeSkillView &view : perceptionAtCreation)
    {
        require(view.mastery <= 4 && view.skill <= 65535, "invalid perception skill view");
        canSeeTraps |= view.mastery == 4
            || (view.mastery > 0 && view.skill > (uint64_t(level) + view.mastery - 1) / view.mastery);
    }

    BreachMazePopulationPlan result;
    result.canSeeTrapsAtCreation = canSeeTraps;
    result.identity = floor.identity;
    result.seeds = floor.seeds;
    result.templateFingerprint = catalog.templateFingerprint;
    result.catalogFingerprint = catalog.contentFingerprint;
    result.layoutFinalRandomState = layout.finalRandomState;
    result.layoutRandomDrawCount = layout.randomDrawCount;
    result.interactionRandomState = floor.seeds.interaction;
    result.interactionRules.bookcaseTreasureMinimum =
        uint8_t(std::min((level + 1) / 2, uint32_t(BreachMazeMaximumTreasureLevel)));
    result.interactionRules.bookcaseTreasureMaximum =
        uint8_t(std::min(level, uint32_t(BreachMazeMaximumTreasureLevel)));
    const size_t trapSpellCount = std::min<size_t>(level, BreachMazeTrapSpells.size());
    result.interactionRules.trapSpellChoices.assign(
        BreachMazeTrapSpells.begin(), BreachMazeTrapSpells.begin() + trapSpellCount);
    // Original array count is the 656 table rows minus four headers. BrBase iterates
    // 1..count-1, so the authenticated trailing Reactor652 row is not a spawn candidate.
    for (const BreachMazeMonsterSource &monster : catalog.monsters)
    {
        if (monster.id < catalog.monsters.size()
            && ((monster.baseKinds | monster.bolsterKinds) & ExcludedKinds) == 0 && monster.height <= 400
            && monster.level >= std::min(level, 50u) && monster.level <= uint64_t(level) * 10)
        {
            result.candidateMonsterIds.push_back(monster.id);
        }
    }
    require(!result.candidateMonsterIds.empty(), "empty monster selection; cannot substitute or reroll");
    for (size_t index = 0; index < result.decorations.size(); ++index)
    {
        const BreachMazePopulationSourceSlot &source = catalog.sourceSlots[index];
        BreachMazeDecorationPlan &entry = result.decorations[index];
        entry.slot = uint16_t(index);
        entry.decorationId = source.decorationId;
        entry.requestedName = catalog.decorations[source.decorationId].name;
        entry.resolvedName = entry.requestedName;
        entry.position = source.position;
        entry.facing = source.facing;
        entry.sourceAttributes = source.attributes;
    }
    PopulationRandom material(floor.seeds.material, BreachMazePopulationStream::Material, result.draws);
    for (const BreachMazeCell &cell : layout.cells)
    {
        require(cell.room >= 1 && cell.room <= 16, "invalid source room label");
        uint8_t &palette = result.paletteByRoom[cell.room - 1];
        if (palette == 0)
        {
            palette = uint8_t(material.draw(1, 4, BreachMazePopulationRoll::Material, cell.room));
            for (std::string_view name : Materials[palette - 1])
            {
                result.resources.textures.emplace_back(name);
            }
        }
    }
    result.resources.textures.emplace_back("rmzdra");
    result.resources.textures.emplace_back("rmzdrb");
    PopulationRandom population(floor.seeds.population, BreachMazePopulationStream::Population, result.draws);
    // Source random bounds are truncated positive Lua numbers. Preserve the actual bounds and
    // ledger rather than clipping actor counts to a budget that fits this unchanged wire format.
    const uint64_t scaled = uint64_t(level) * layout.side;
    require(scaled <= std::numeric_limits<uint64_t>::max() / scaled,
        "source monster budget square exceeds uint64 representation");
    const uint64_t maximumMonsterBudget = scaled * scaled / 16;
    const uint64_t minimumMonsterBudget = uint64_t(level - 1) * layout.side / 4;
    const uint64_t maximumTreasureBudget = scaled / 4;
    require(maximumMonsterBudget <= uint64_t(std::numeric_limits<int32_t>::max()),
        "source monster budget exceeds persistent int32 representation");
    require(maximumTreasureBudget < std::numeric_limits<uint32_t>::max(),
        "source treasure budget exceeds the uint32 random ledger");
    result.treasureLimit = population.draw(1, uint32_t(maximumTreasureBudget),
        BreachMazePopulationRoll::TreasureBudget);
    result.initialMonsterBudget = int32_t(population.draw(uint32_t(minimumMonsterBudget),
        uint32_t(maximumMonsterBudget), BreachMazePopulationRoll::MonsterBudget));
    result.remainingMonsterBudget = result.initialMonsterBudget;
    std::array<uint32_t, 16> treasuresByRoom = {};
    std::map<uint32_t, BreachMazePopulationFacePatch> patches;
    const auto patch = [&](uint32_t index) -> BreachMazePopulationFacePatch &
    {
        require(index < mazeTemplate.faces.size(), "patch face out of range");
        BreachMazePopulationFacePatch &value = patches[index];
        value.faceIndex = index;
        return value;
    };
    const auto monsterId = [&](uint16_t cell)
    {
        const uint32_t choice = population.draw(1, uint32_t(result.candidateMonsterIds.size()),
            BreachMazePopulationRoll::MonsterId, cell);
        return result.candidateMonsterIds[choice - 1];
    };
    const auto addActor = [&](uint32_t id, const BreachMazeTemplateCell &cell,
        BreachMazeHalfPoint position, uint8_t multiplier)
    {
        BreachMazeActorRequest request;
        request.ordinal = uint32_t(result.actors.size());
        request.cellId = cell.cellId;
        request.monsterId = id;
        request.position = position;
        request.hitPointMultiplier = multiplier;
        request.familyResolveKey = resolveKey(floor.seeds.population, ActorTag, (id + 2) / 3);
        request.actorResolveKey = resolveKey(floor.seeds.population, ActorTag, request.ordinal);
        result.actors.push_back(request);
        --result.remainingMonsterBudget; // Source checks once before a group; negative down to-4 is intentional.
        result.resources.monsterIds.push_back(id);
        const BreachMazeMonsterSource &source = catalog.monsters[id - 1];
        for (const std::string &sprite : source.spriteGroups)
        {
            result.resources.actorSpriteGroups.push_back(sprite);
        }
        for (uint16_t sound : source.soundIds)
        {
            result.resources.soundIds.push_back(sound);
        }
    };
    const auto addItem = [&](const BreachMazeTemplateCell &cell, bool emptyBottle)
    {
        BreachMazeItemRequest request;
        request.ordinal = uint32_t(result.items.size());
        request.cellId = cell.cellId;
        request.position = center(cell);
        request.resolveKey = resolveKey(floor.seeds.population, ItemTag, request.ordinal);
        if (emptyBottle)
        {
            request.fixedItemId = 220;
            result.resources.fixedItemIds.push_back(220);
        }
        else
        {
            request.placeholderItemId = population.draw(177, 186,
                BreachMazePopulationRoll::PlaceholderItem, cell.cellId);
            const uint32_t rolledLevel = population.draw((level + 1) / 2, level,
                BreachMazePopulationRoll::TreasureLevel, cell.cellId);
            request.treasureLevel = uint8_t(std::min(rolledLevel, uint32_t(BreachMazeMaximumTreasureLevel)));
            request.positiveBonusStrengthAtLeastOne = true;
            result.resources.requiresResolvedItemResources = true;
        }
        result.items.push_back(request);
    };
    const auto placeDecoration = [&](const BreachMazeTemplateCell &cell, std::string_view requested,
        BreachMazeHalfPoint position)
    {
        require(result.placedDecorationCount < 256, "decoration capacity exceeded");
        const uint16_t slot = uint16_t(result.placedDecorationCount++);
        BreachMazeDecorationPlan &entry = result.decorations[slot];
        const BreachMazeDecorationSource &source = decoration(catalog,
            requested == "trashheap" ? "trasheap" : requested);
        entry.cellId = cell.cellId;
        entry.decorationId = source.id;
        entry.requestedName = requested;
        entry.resolvedName = source.name;
        entry.sourceNameCorrected = requested == "trashheap";
        entry.position = position;
        entry.invisible = false;
        result.resources.decorationSpriteGroups.push_back(source.spriteGroup);
        result.resources.soundIds.push_back(source.soundId);
        BreachMazeDecorationInitialState &state = entry.interaction;
        PopulationRandom interaction(resolveKey(floor.seeds.interaction, DecorationTag, slot),
            BreachMazePopulationStream::Decoration, result.draws);
        const std::string name = lower(source.name);
        const auto crystal = std::find(UsefulDecorations.begin(), UsefulDecorations.begin() + 4, name);
        if (crystal != UsefulDecorations.begin() + 4)
        {
            constexpr std::array<uint32_t, 4> rewards = {2060, 2056, 2059, 2065};
            state.handler = BreachMazeDecorationHandler::Crystal;
            state.eventVariable = uint16_t(crystal - UsefulDecorations.begin() + 1);
            state.fixedRewardItemId = rewards[state.eventVariable - 1];
            result.resources.fixedItemIds.push_back(state.fixedRewardItemId);
        }
        else if (name == "bucket")
        {
            state.handler = BreachMazeDecorationHandler::ReagentBucket;
            state.eventVariable = 45;
        }
        else if (name == "bag01")
        {
            state.handler = BreachMazeDecorationHandler::MiscBag;
            state.eventVariable = 22;
        }
        else if (name == "floursac")
        {
            state.handler = BreachMazeDecorationHandler::FoodSack;
        }
        else if (name == "trasheap" || name == "7dec10")
        {
            state.handler = BreachMazeDecorationHandler::TrashHeap;
        }
        else if (name == "bigbarel" || name == "7dec03")
        {
            state.handler = name == "bigbarel" ? BreachMazeDecorationHandler::AttributeBarrel
                : BreachMazeDecorationHandler::ResistanceCauldron;
            state.eventVariable = uint16_t(interaction.draw(1, name == "bigbarel" ? 7 : 4,
                BreachMazePopulationRoll::DecorationAttribute, slot));
        }
        else if (name == "dec61" || name == "dec62" || name == "dec63")
        {
            const uint16_t tier = uint16_t(name.back() - '0' + 1);
            state.handler = BreachMazeDecorationHandler::Challenge;
            state.eventVariable = uint16_t(100 * tier + interaction.draw(1, 7,
                BreachMazePopulationRoll::DecorationAttribute, slot));
            state.requiresGlobalChallengeLedger = true;
            constexpr std::array<uint16_t, 3> rewards = {5, 7, 10};
            state.challengeThreshold = uint16_t(25 * (1u << (tier - 1)));
            state.challengeSkillPointReward = rewards[tier - 2];
        }
        else
        {
            require(name == "torch01", "unhandled source decoration");
        }
        state.randomState = interaction.state();
        state.randomDrawCount = interaction.count();
        if (state.handler != BreachMazeDecorationHandler::None)
        {
            entry.eventId = 20000 + slot;
        }
    };

    for (const BreachMazeCell &mazeCell : layout.cells)
    {
        const BreachMazeTemplateCell &cell = mazeTemplate.cells[mazeCell.templateCellId - 1];
        const uint32_t openings = uint32_t(std::count(mazeCell.walls.begin(), mazeCell.walls.end(), false));
        const bool endpoint = cell.cellId == layout.endpoints.entrance.templateCellId
            || cell.cellId == layout.endpoints.exit.templateCellId;
        require(openings > 0, "closed active cell");
        if (openings == 1)
        {
            uint32_t &count = treasuresByRoom[mazeCell.room - 1];
            if (endpoint || count > result.treasureLimit)
            {
                continue;
            }
            ++count;
            const uint32_t branch = population.draw(1, 3, BreachMazePopulationRoll::DeadEndBranch, cell.cellId);
            const BreachMazeCell *pNeighbor = nullptr;
            if (!mazeCell.walls[2])
            {
                pNeighbor = &layout.cellAt(cell.x, uint8_t(cell.y - 1));
            }
            else if (!mazeCell.walls[3])
            {
                pNeighbor = &layout.cellAt(cell.x, uint8_t(cell.y + 1));
            }
            else if (!mazeCell.walls[0])
            {
                pNeighbor = &layout.cellAt(uint8_t(cell.x - 1), cell.y);
            }
            else
            {
                pNeighbor = &layout.cellAt(uint8_t(cell.x + 1), cell.y);
            }
            if (pNeighbor->room != mazeCell.room)
            {
                const uint8_t palette = result.paletteByRoom[pNeighbor->room - 1];
                for (uint32_t faceId : cell.faceIndices)
                {
                    const size_t role = static_cast<size_t>(mazeTemplate.faces[faceId].role);
                    require(role < 4, "invalid source facet role");
                    patch(faceId).texture = Materials[palette - 1][role];
                }
                if (branch != 2)
                {
                    result.bookcaseCells[cell.cellId - 1] = true;
                    result.resources.textures.emplace_back("bookcas1");
                    for (uint32_t faceId : cell.wallFaceIndices)
                    {
                        BreachMazePopulationFacePatch &value = patch(faceId);
                        value.texture = "bookcas1";
                        value.eventId = 4;
                        value.click = true;
                    }
                }
            }
            if (branch == 1)
            {
                addItem(cell, false);
            }
            else if (branch == 2)
            {
                if (result.placedDecorationCount < 256)
                {
                    const uint32_t choice = population.draw(1, 14,
                        BreachMazePopulationRoll::UsefulDecoration, cell.cellId);
                    BreachMazeHalfPoint position = center(cell);
                    position.z = -422;
                    placeDecoration(cell, UsefulDecorations[choice - 1], position);
                }
            }
            else if (result.remainingMonsterBudget > 0)
            {
                addActor(monsterId(cell.cellId), cell, center(cell), 2);
            }
            else
            {
                addItem(cell, true);
            }
        }
        else if (openings == 2 && !endpoint)
        {
            if (result.placedDecorationCount < 256 && result.torchCount < 256)
            {
                for (uint32_t faceId : cell.wallFaceIndices)
                {
                    const BreachMazeTemplateFace &face = mazeTemplate.faces[faceId];
                    if (mazeCell.walls[static_cast<size_t>(*face.wallSide)]
                        && population.draw(1, std::min(level, 3u),
                            BreachMazePopulationRoll::TorchWall, cell.cellId) == 1)
                    {
                        BreachMazeHalfPoint position = {
                            int32_t(face.sourceBounds[0]) + face.sourceBounds[1] + int32_t(20 * face.sourceNormal[0]),
                            int32_t(face.sourceBounds[2]) + face.sourceBounds[3]
                                + int32_t(20 * face.sourceNormal[1]), -100};
                        placeDecoration(cell, "torch01", position);
                        ++result.torchCount;
                        break;
                    }
                }
            }
            if (population.draw(1, 6, BreachMazePopulationRoll::FloorTrap, cell.cellId) == 1)
            {
                BreachMazePopulationFacePatch &value = patch(cell.centralFloorFaceIndex);
                value.eventId = 3;
                value.click = false;
                value.step = true;
                value.secret = canSeeTraps;
                result.traps[cell.cellId - 1].placed = true;
                result.traps[cell.cellId - 1].visible = canSeeTraps;
                for (size_t index = 0; index < trapSpellCount; ++index)
                {
                    result.resources.trapSpellIds.push_back(BreachMazeTrapSpells[index]);
                }
            }
        }
        else if (openings >= 3)
        {
            if (result.remainingMonsterBudget > 0)
            {
                const uint32_t id = monsterId(cell.cellId);
                const uint32_t count = population.draw(openings == 3 ? 1 : 3, openings == 3 ? 3 : 5,
                    BreachMazePopulationRoll::GroupSize, cell.cellId);
                for (uint32_t index = 0; index < count; ++index)
                {
                    BreachMazeHalfPoint position = center(cell);
                    position.x += 2 * int32_t(population.draw(10, 50, BreachMazePopulationRoll::ActorX, cell.cellId));
                    position.y += 2 * int32_t(population.draw(10, 50, BreachMazePopulationRoll::ActorY, cell.cellId));
                    addActor(id, cell, position, 1);
                }
            }
            if (openings == 4)
            {
                addItem(cell, false);
            }
        }
    }
    require(result.remainingMonsterBudget >= -4
        && result.actors.size() <= uint64_t(result.initialMonsterBudget) + 4
        && result.actors.size() <= BreachMazePopulationMaximumActors, "source group budget invariant failed");
    for (const auto &[faceIndex, value] : patches)
    {
        require(faceIndex != layout.endpoints.entrance.faceIndex && faceIndex != layout.endpoints.exit.faceIndex,
            "population attempted to replace an entrance/exit door");
        result.facePatches.push_back(value);
        if (value.texture)
        {
            result.resources.textures.push_back(*value.texture);
        }
    }
    for (size_t index = 0; index < result.lights.size(); ++index)
    {
        result.lights[index] = {uint16_t(index), result.decorations[index].position,
            catalog.sourceSlots[index].light, result.decorations[index].invisible};
    }
    result.materialRandomState = material.state();
    result.materialRandomDrawCount = material.count();
    result.populationRandomState = population.state();
    result.populationRandomDrawCount = population.count();
    result.resources.requiresResolvedActorResources = !result.actors.empty();
    sortUnique(result.resources.textures);
    sortUnique(result.resources.actorSpriteGroups);
    sortUnique(result.resources.decorationSpriteGroups);
    sortUnique(result.resources.soundIds);
    sortUnique(result.resources.monsterIds);
    sortUnique(result.resources.fixedItemIds);
    sortUnique(result.resources.trapSpellIds);
    return result;
}
}
