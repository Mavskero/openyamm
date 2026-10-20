#include "game/content/BreachMazeRuntimeProfile.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace OpenYAMM::Game
{
namespace
{
constexpr std::string_view ProfileMagic = "OYBMPRF1";
constexpr std::string_view PlanMagic = "OYBMPOP1";
constexpr size_t MaximumStringBytes = 4096;
constexpr uint32_t SourceFaceCount = 5632;

void require(bool condition, const char *pMessage)
{
    if (!condition) throw std::invalid_argument(std::string("Breach maze codec: ") + pMessage);
}

// One explicit field order is shared by the reader and writer. No object layout, native padding,
// enum ABI, or pointer values appear on disk. Changing a field order requires a codec version.
template <typename Archive, typename T> void fields(Archive &a, T &v)
{
    using V = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<V, BreachMazeRuntimeProfile>) a(v.mazeTemplate, v.catalog);
    else if constexpr (std::is_same_v<V, BreachMazeTemplate>)
        a(v.version, v.vertexCount, v.decorationCount, v.lightCount, v.faces, v.cells);
    else if constexpr (std::is_same_v<V, BreachMazeTemplateFace>)
        a(v.faceIndex, v.cellId, v.role, v.wallSide, v.sourceBounds, v.sourceNormal);
    else if constexpr (std::is_same_v<V, BreachMazeTemplateCell>)
        a(v.cellId, v.x, v.y, v.faceIndices, v.wallFaceIndices, v.centralFloorFaceIndex);
    else if constexpr (std::is_same_v<V, BreachMazeHalfPoint>) a(v.x, v.y, v.z);
    else if constexpr (std::is_same_v<V, BreachMazeSourceFingerprint>) a(v.name, v.sha256);
    else if constexpr (std::is_same_v<V, BreachMazeMonsterSource>)
        a(v.id, v.level, v.baseHitPoints, v.height, v.radius, v.moveSpeed, v.toHitRadius, v.tintArgb,
            v.baseKinds, v.bolsterKinds, v.soundIds, v.spriteGroups, v.monsterRow, v.monListRow, v.bolsterRow);
    else if constexpr (std::is_same_v<V, BreachMazeDecorationSource>)
        a(v.id, v.name, v.spriteGroup, v.height, v.radius, v.lightRadius, v.soundId, v.noBlockMovement, v.sourceRow);
    else if constexpr (std::is_same_v<V, BreachMazeLightProperties>)
        a(v.radius, v.color, v.type, v.attributes, v.brightness, v.id);
    else if constexpr (std::is_same_v<V, BreachMazePopulationSourceSlot>)
        a(v.decorationId, v.position, v.facing, v.attributes, v.light);
    else if constexpr (std::is_same_v<V, BreachMazePopulationCatalog>)
        a(v.version, v.contentFixVersion, v.templateFingerprint, v.contentFingerprint, v.sources, v.monsters,
            v.decorations, v.sourceSlots, v.bolsterMapRow, v.bolsterFormulaRows);
    else if constexpr (std::is_same_v<V, BreachMazeFloorIdentity>) a(v.epoch, v.level, v.generationSerial);
    else if constexpr (std::is_same_v<V, BreachMazeFloorSeeds>) a(v.layout, v.material, v.population, v.interaction);
    else if constexpr (std::is_same_v<V, BreachMazePopulationDraw>)
        a(v.stream, v.purpose, v.context, v.minimum, v.maximum, v.value, v.rawDrawsAfter);
    else if constexpr (std::is_same_v<V, BreachMazeActorRequest>)
        a(v.ordinal, v.cellId, v.monsterId, v.position, v.hitPointMultiplier, v.familyResolveKey, v.actorResolveKey,
            v.hostilityType, v.hostile, v.showOnMap, v.onAlertMap, v.guardRadius, v.room,
            v.goldDiceCount, v.goldDiceSides, v.rage);
    else if constexpr (std::is_same_v<V, BreachMazeItemRequest>)
        a(v.ordinal, v.cellId, v.position, v.fixedItemId, v.placeholderItemId, v.treasureLevel,
            v.treasureType, v.positiveBonusStrengthAtLeastOne, v.resolveKey);
    else if constexpr (std::is_same_v<V, BreachMazePopulationFacePatch>)
        a(v.faceIndex, v.texture, v.eventId, v.click, v.step, v.secret);
    else if constexpr (std::is_same_v<V, BreachMazeDecorationInitialState>)
        a(v.handler, v.eventVariable, v.fixedRewardItemId, v.used, v.requiresGlobalChallengeLedger,
            v.challengeThreshold, v.challengeSkillPointReward, v.randomState, v.randomDrawCount);
    else if constexpr (std::is_same_v<V, BreachMazeDecorationPlan>)
        a(v.slot, v.cellId, v.decorationId, v.requestedName, v.resolvedName, v.sourceNameCorrected,
            v.position, v.facing, v.sourceAttributes, v.invisible, v.showOnMap, v.eventId, v.interaction);
    else if constexpr (std::is_same_v<V, BreachMazeLightPlan>) a(v.slot, v.position, v.sourceProperties, v.off);
    else if constexpr (std::is_same_v<V, BreachMazeTrapInitialState>)
        a(v.placed, v.visible, v.spellId, v.nextShotGameMinute);
    else if constexpr (std::is_same_v<V, BreachMazePopulationInteractionRules>)
        a(v.trapSpellChoices, v.trapSpellSkillLevel, v.trapSpellMastery, v.trapCooldownGameMinutes,
            v.bookcaseTreasureMinimum, v.bookcaseTreasureMaximum, v.bookcaseTreasureType,
            v.bookcaseRecipientPartyIndex, v.interactionsUsePartyPositionCell);
    else if constexpr (std::is_same_v<V, BreachMazePopulationResources>)
        a(v.textures, v.actorSpriteGroups, v.decorationSpriteGroups, v.soundIds, v.monsterIds, v.fixedItemIds,
            v.trapSpellIds, v.requiresResolvedActorResources, v.requiresResolvedItemResources);
    else if constexpr (std::is_same_v<V, BreachMazePopulationPlan>)
        a(v.version, v.contentFixVersion, v.identity, v.seeds, v.templateFingerprint, v.catalogFingerprint,
            v.layoutFinalRandomState, v.layoutRandomDrawCount, v.paletteByRoom, v.treasureLimit,
            v.initialMonsterBudget, v.remainingMonsterBudget, v.placedDecorationCount, v.torchCount,
            v.canSeeTrapsAtCreation, v.sectorOneDarkness, v.materialRandomState, v.materialRandomDrawCount,
            v.populationRandomState, v.populationRandomDrawCount, v.interactionRandomState,
            v.candidateMonsterIds, v.actors, v.items, v.facePatches, v.decorations, v.lights, v.traps,
            v.bookcaseCells, v.bookcaseUsed, v.interactionRules, v.resources, v.draws);
    else static_assert(!std::is_same_v<V, V>, "Unregistered Breach maze codec value");
}

template <typename T> constexpr size_t vectorLimit()
{
    if constexpr (std::is_same_v<T, BreachMazeTemplateFace>) return SourceFaceCount;
    if constexpr (std::is_same_v<T, BreachMazeMonsterSource>) return 652;
    if constexpr (std::is_same_v<T, BreachMazeDecorationSource>) return 673;
    if constexpr (std::is_same_v<T, BreachMazeSourceFingerprint>) return 32;
    if constexpr (std::is_same_v<T, BreachMazeActorRequest>) return BreachMazePopulationMaximumActors;
    if constexpr (std::is_same_v<T, BreachMazeItemRequest>) return 256;
    if constexpr (std::is_same_v<T, BreachMazePopulationFacePatch>) return SourceFaceCount;
    if constexpr (std::is_same_v<T, BreachMazePopulationDraw>) return 16384;
    if constexpr (std::is_same_v<T, std::vector<std::string>>) return 64;
    return 8192;
}

template <typename T> void validEnum(T value)
{
    unsigned maximum = 0;
    if constexpr (std::is_same_v<T, BreachMazeSide>) maximum = 3;
    else if constexpr (std::is_same_v<T, BreachMazeFaceRole>) maximum = 3;
    else if constexpr (std::is_same_v<T, BreachMazePopulationStream>) maximum = 2;
    else if constexpr (std::is_same_v<T, BreachMazePopulationRoll>) maximum = 13;
    else if constexpr (std::is_same_v<T, BreachMazeDecorationHandler>) maximum = 8;
    else static_assert(!std::is_same_v<T, T>, "Unregistered Breach maze codec enum");
    require(static_cast<unsigned>(value) <= maximum, "enum outside supported range");
}

class Writer
{
public:
    explicit Writer(size_t limit) : m_limit(limit) {}
    template <typename... T> void operator()(const T &...values) { (write(values), ...); }
    std::vector<uint8_t> take() { return std::move(m_bytes); }

private:
    void append(uint8_t value)
    {
        require(m_bytes.size() < m_limit, "encoded payload exceeds size limit");
        m_bytes.push_back(value);
    }
    template <typename T> void write(const T &value)
    {
        if constexpr (std::is_same_v<T, bool>) append(value ? 1 : 0);
        else if constexpr (std::is_integral_v<T>)
        {
            using U = std::make_unsigned_t<T>;
            const U bits = std::bit_cast<U>(value);
            for (size_t byte = 0; byte < sizeof(T); ++byte) append(uint8_t(bits >> (byte * 8)));
        }
        else if constexpr (std::is_enum_v<T>)
        {
            validEnum(value);
            write(static_cast<uint8_t>(value));
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            require(std::isfinite(value), "non-finite float");
            write(std::bit_cast<uint32_t>(value));
        }
        else fields(*this, value);
    }
    void write(const std::string &value)
    {
        require(value.size() <= MaximumStringBytes, "string exceeds size limit");
        write(static_cast<uint32_t>(value.size()));
        for (unsigned char byte : value) append(byte);
    }
    template <typename T, size_t N> void write(const std::array<T, N> &values)
    {
        for (const T &value : values) write(value);
    }
    template <typename T> void write(const std::vector<T> &values)
    {
        require(values.size() <= vectorLimit<T>(), "vector exceeds count limit");
        write(static_cast<uint32_t>(values.size()));
        for (const T &value : values) write(value);
    }
    template <typename T> void write(const std::optional<T> &value)
    {
        write(value.has_value());
        if (value) write(*value);
    }
    size_t m_limit;
    std::vector<uint8_t> m_bytes;
};

class Reader
{
public:
    explicit Reader(std::span<const uint8_t> bytes) : m_bytes(bytes) {}
    template <typename... T> void operator()(T &...values) { (read(values), ...); }
    bool finished() const { return m_offset == m_bytes.size(); }

private:
    uint8_t next()
    {
        require(m_offset < m_bytes.size(), "truncated payload");
        return m_bytes[m_offset++];
    }
    template <typename T> void read(T &value)
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            const uint8_t byte = next();
            require(byte <= 1, "noncanonical boolean");
            value = byte != 0;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            using U = std::make_unsigned_t<T>;
            U bits = 0;
            for (size_t byte = 0; byte < sizeof(T); ++byte) bits |= U(U(next()) << (byte * 8));
            value = std::bit_cast<T>(bits);
        }
        else if constexpr (std::is_enum_v<T>)
        {
            value = static_cast<T>(next());
            validEnum(value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            uint32_t bits = 0;
            read(bits);
            value = std::bit_cast<float>(bits);
            require(std::isfinite(value), "non-finite float");
        }
        else fields(*this, value);
    }
    void read(std::string &value)
    {
        uint32_t size = 0;
        read(size);
        require(size <= MaximumStringBytes && size <= m_bytes.size() - m_offset, "invalid string length");
        value.assign(reinterpret_cast<const char *>(m_bytes.data() + m_offset), size);
        m_offset += size;
    }
    template <typename T, size_t N> void read(std::array<T, N> &values)
    {
        for (T &value : values) read(value);
    }
    template <typename T> void read(std::vector<T> &values)
    {
        uint32_t size = 0;
        read(size);
        require(size <= vectorLimit<T>() && size <= m_bytes.size() - m_offset, "invalid vector count");
        values.reserve(size);
        for (uint32_t index = 0; index < size; ++index)
        {
            T value{};
            read(value);
            values.push_back(std::move(value));
        }
    }
    template <typename T> void read(std::optional<T> &value)
    {
        bool present = false;
        read(present);
        if (present)
        {
            T decoded{};
            read(decoded);
            value = std::move(decoded);
        }
        else value.reset();
    }
    std::span<const uint8_t> m_bytes;
    size_t m_offset = 0;
};

template <typename T> std::vector<uint8_t> encode(const T &value, std::string_view magic, size_t limit)
{
    Writer writer(limit);
    for (char byte : magic) writer(static_cast<uint8_t>(byte));
    writer(BreachMazeRuntimeCodecVersion, value);
    return writer.take();
}

template <typename T> T decode(std::span<const uint8_t> bytes, std::string_view magic, size_t limit)
{
    require(bytes.size() >= magic.size() + sizeof(uint32_t) && bytes.size() <= limit, "invalid payload size");
    require(std::equal(magic.begin(), magic.end(), bytes.begin()), "wrong payload kind or magic");
    Reader reader(bytes.subspan(magic.size()));
    uint32_t version = 0;
    reader(version);
    require(version == BreachMazeRuntimeCodecVersion, "unsupported codec version");
    T value;
    reader(value);
    require(reader.finished(), "trailing payload bytes");
    return value;
}

uint32_t sourceInteger(const std::string &value)
{
    uint32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == value.data() + value.size(), "invalid source row integer");
    return result;
}

template <typename T> void sortedUnique(const std::vector<T> &values)
{
    require(std::is_sorted(values.begin(), values.end())
        && std::adjacent_find(values.begin(), values.end()) == values.end(), "noncanonical resource or candidate order");
}

void validateProfile(const BreachMazeRuntimeProfile &profile)
{
    const BreachMazeTemplate &maze = profile.mazeTemplate;
    const BreachMazePopulationCatalog &catalog = profile.catalog;
    require(maze.version == BreachMazeTemplateVersion && maze.vertexCount == 4224
        && maze.faces.size() == SourceFaceCount && maze.decorationCount == 256 && maze.lightCount == 256,
        "unsupported source template counts or version");
    require(catalog.version == BreachMazePopulationVersion
        && catalog.contentFixVersion == BreachMazePopulationContentFixVersion,
        "unsupported catalog version");
    require(catalog.templateFingerprint == breachMazePopulationTemplateFingerprint(maze)
        && catalog.contentFingerprint == breachMazePopulationCatalogFingerprint(catalog),
        "catalog or template fingerprint mismatch");
    // This invokes the existing structural validator without mutating any saved/generated identity.
    (void)generateBreachMazeLayout(maze, 1, 0);
    require(catalog.sources.size() == 14 && catalog.monsters.size() == 652 && catalog.decorations.size() == 673,
        "incomplete source catalog");
    std::string previous;
    for (const BreachMazeSourceFingerprint &source : catalog.sources)
    {
        require(source.name > previous && source.sha256.size() == 64
            && source.sha256.find_first_not_of("0123456789abcdef") == std::string::npos,
            "invalid source identity list");
        previous = source.name;
    }
    for (size_t index = 0; index < catalog.monsters.size(); ++index)
    {
        const BreachMazeMonsterSource &monster = catalog.monsters[index];
        require(monster.id == index + 1 && sourceInteger(monster.monsterRow[0]) == monster.id
            && sourceInteger(monster.monListRow[0]) == index
            && sourceInteger(monster.monsterRow[3]) == monster.level
            && sourceInteger(monster.monsterRow[4]) == monster.baseHitPoints
            && sourceInteger(monster.monListRow[1]) == monster.height
            && monster.bolsterRow.has_value() == (monster.id <= 651), "inconsistent source monster row");
    }
    for (size_t index = 0; index < catalog.decorations.size(); ++index)
    {
        const BreachMazeDecorationSource &decoration = catalog.decorations[index];
        require(decoration.id == index && sourceInteger(decoration.sourceRow[0]) == index
            && decoration.name == decoration.sourceRow[1], "inconsistent source decoration row");
    }
    for (const BreachMazePopulationSourceSlot &slot : catalog.sourceSlots)
        require(slot.decorationId < catalog.decorations.size(), "source decoration slot out of range");
    require(catalog.bolsterMapRow.size() >= 9 && catalog.bolsterMapRow.size() <= 32
        && catalog.bolsterMapRow[0] == "206" && catalog.bolsterMapRow[3] == "LowerToEqual"
        && !catalog.bolsterFormulaRows.empty(), "missing original resolver tables");
    for (const std::vector<std::string> &row : catalog.bolsterFormulaRows)
        require(row.size() <= 32, "formula row has too many columns");
}

void validatePlan(const BreachMazePopulationPlan &plan)
{
    require(plan.version == BreachMazePopulationVersion
        && plan.contentFixVersion == BreachMazePopulationContentFixVersion
        && plan.identity.epoch != 0 && plan.identity.generationSerial != 0
        && plan.identity.level > 0 && plan.identity.level <= BreachMazeMaximumPlannedLevel
        && plan.templateFingerprint != 0 && plan.catalogFingerprint != 0, "invalid plan identity or version");
    const uint64_t maximumScaled = uint64_t(plan.identity.level) * std::clamp(plan.identity.level, 8u, 16u);
    // Only bound validation by the storage range when the theoretical largest layout exceeds it;
    // generation independently checks the actual side and never substitutes a smaller source budget.
    const uint64_t maximumStoredBudget = maximumScaled > uint64_t(std::numeric_limits<int32_t>::max())
        ? uint64_t(std::numeric_limits<int32_t>::max())
        : std::min(maximumScaled * maximumScaled / 16, uint64_t(std::numeric_limits<int32_t>::max()));
    require(plan.treasureLimit > 0 && plan.treasureLimit <= maximumScaled / 4
        && plan.initialMonsterBudget >= 0 && uint64_t(plan.initialMonsterBudget) >= plan.identity.level - 1
        && uint64_t(plan.initialMonsterBudget) <= maximumStoredBudget
        && plan.remainingMonsterBudget >= -4 && plan.remainingMonsterBudget <= plan.initialMonsterBudget
        && plan.placedDecorationCount <= 256 && plan.torchCount <= plan.placedDecorationCount,
        "plan budget or decoration count out of range");
    for (uint8_t palette : plan.paletteByRoom) require(palette <= 4, "invalid palette");
    require(!plan.candidateMonsterIds.empty() && plan.candidateMonsterIds.size() <= 651, "invalid candidate count");
    sortedUnique(plan.candidateMonsterIds);
    for (uint32_t id : plan.candidateMonsterIds) require(id > 0 && id <= 651, "invalid monster candidate");
    for (size_t index = 0; index < plan.actors.size(); ++index)
    {
        const BreachMazeActorRequest &actor = plan.actors[index];
        require(actor.ordinal == index && actor.cellId > 0 && actor.cellId <= 256
            && std::binary_search(plan.candidateMonsterIds.begin(), plan.candidateMonsterIds.end(), actor.monsterId)
            && (actor.hitPointMultiplier == 1 || actor.hitPointMultiplier == 2)
            && actor.hostilityType <= 4 && actor.room < 2, "invalid frozen actor request");
    }
    for (size_t index = 0; index < plan.items.size(); ++index)
    {
        const BreachMazeItemRequest &item = plan.items[index];
        require(item.ordinal == index && item.cellId > 0 && item.cellId <= 256
            && ((item.fixedItemId == 220 && item.placeholderItemId == 0)
                || (item.fixedItemId == 0 && item.placeholderItemId >= 177 && item.placeholderItemId <= 186))
            && item.treasureLevel <= 6 && item.treasureType <= 46, "invalid frozen item request");
    }
    uint32_t previousFace = 0;
    bool firstFace = true;
    for (const BreachMazePopulationFacePatch &patch : plan.facePatches)
    {
        require(patch.faceIndex < SourceFaceCount && (firstFace || patch.faceIndex > previousFace)
            && (!patch.texture || !patch.texture->empty()) && (!patch.eventId || *patch.eventId <= 4),
            "invalid frozen face patch");
        firstFace = false;
        previousFace = patch.faceIndex;
    }
    for (size_t index = 0; index < 256; ++index)
    {
        const BreachMazeDecorationPlan &decoration = plan.decorations[index];
        require(decoration.slot == index && decoration.cellId <= 256 && decoration.decorationId < 673
            && decoration.eventId <= 65535 && !decoration.resolvedName.empty()
            && plan.lights[index].slot == index, "invalid frozen decoration/light slot");
        validEnum(decoration.interaction.handler);
        const BreachMazeTrapInitialState &trap = plan.traps[index];
        require(!trap.spellId || *trap.spellId == 0
            || std::find(plan.interactionRules.trapSpellChoices.begin(), plan.interactionRules.trapSpellChoices.end(),
                *trap.spellId) != plan.interactionRules.trapSpellChoices.end(), "invalid trap spell");
        require(!plan.bookcaseUsed[index] || plan.bookcaseCells[index], "used bookcase has no source cell");
    }
    const BreachMazePopulationInteractionRules &rules = plan.interactionRules;
    const size_t trapSpellCount = std::min<size_t>(plan.identity.level, BreachMazeTrapSpells.size());
    require(rules.trapSpellChoices.size() == trapSpellCount
        && std::equal(rules.trapSpellChoices.begin(), rules.trapSpellChoices.end(), BreachMazeTrapSpells.begin())
        && rules.trapSpellSkillLevel == 7 && rules.trapSpellMastery == 3 && rules.trapCooldownGameMinutes == 3
        && rules.bookcaseTreasureMinimum == std::min((plan.identity.level + 1) / 2,
            uint32_t(BreachMazeMaximumTreasureLevel))
        && rules.bookcaseTreasureMaximum == std::min(plan.identity.level, uint32_t(BreachMazeMaximumTreasureLevel))
        && rules.bookcaseTreasureType == 16
        && rules.bookcaseRecipientPartyIndex == 0 && rules.interactionsUsePartyPositionCell,
        "unsupported interaction rules");
    const BreachMazePopulationResources &resources = plan.resources;
    sortedUnique(resources.textures);
    sortedUnique(resources.actorSpriteGroups);
    sortedUnique(resources.decorationSpriteGroups);
    sortedUnique(resources.soundIds);
    sortedUnique(resources.monsterIds);
    sortedUnique(resources.fixedItemIds);
    sortedUnique(resources.trapSpellIds);
    for (uint32_t id : resources.monsterIds)
        require(std::binary_search(plan.candidateMonsterIds.begin(), plan.candidateMonsterIds.end(), id),
            "resource monster outside frozen candidates");
    for (const BreachMazePopulationDraw &draw : plan.draws)
    {
        validEnum(draw.stream);
        validEnum(draw.purpose);
        require(draw.context <= 256 && draw.minimum <= draw.value && draw.value <= draw.maximum
            && draw.rawDrawsAfter != 0, "invalid frozen random draw");
    }
}
}

std::vector<uint8_t> encodeBreachMazeRuntimeProfile(const BreachMazeRuntimeProfile &profile)
{
    validateProfile(profile);
    return encode(profile, ProfileMagic, BreachMazeRuntimeProfileMaximumBytes);
}

BreachMazeRuntimeProfile decodeBreachMazeRuntimeProfile(std::span<const uint8_t> bytes)
{
    BreachMazeRuntimeProfile result = decode<BreachMazeRuntimeProfile>(bytes, ProfileMagic,
        BreachMazeRuntimeProfileMaximumBytes);
    validateProfile(result);
    return result;
}

std::vector<uint8_t> encodeBreachMazePopulationPlan(const BreachMazePopulationPlan &plan)
{
    validatePlan(plan);
    return encode(plan, PlanMagic, BreachMazePopulationPlanMaximumBytes);
}

BreachMazePopulationPlan decodeBreachMazePopulationPlan(std::span<const uint8_t> bytes)
{
    BreachMazePopulationPlan result = decode<BreachMazePopulationPlan>(bytes, PlanMagic,
        BreachMazePopulationPlanMaximumBytes);
    validatePlan(result);
    return result;
}
}
