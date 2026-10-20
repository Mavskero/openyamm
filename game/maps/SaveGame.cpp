#include "game/maps/SaveGame.h"
#include "game/content/BreachMazeRuntimeProfile.h"
#include "game/events/GameTime.h"
#include "game/events/NamedGameTime.h"
#include "game/maps/MapIdentity.h"

#include "game/events/EvtEnums.h"
#include "game/gameplay/MercenaryRecruitmentRuntime.h"
#include "game/gameplay/MergeMonsterGenerationState.h"
#include "game/gameplay/PartyJumpState.h"
#include "game/tables/HouseTable.h"
#include "game/tables/ItemTable.h"

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint32_t SaveVersion = 100;
constexpr uint32_t SaveVersionOutdoorDecorationFlags = 100;
constexpr uint32_t SaveVersionOutdoorLightTime = 99;
constexpr uint32_t SaveVersionEventRandomState = 98;
constexpr uint32_t SaveVersionPartyJumpState = 97;
constexpr uint32_t SaveVersionBreachAlvarGeneration = 96;
constexpr uint32_t SaveVersionBreachMazeInteractions = 95;
constexpr uint32_t SaveVersionBreachMaze = 94;
constexpr uint32_t SaveVersionActorSpecialRuntime = 93;
constexpr uint32_t SaveVersionResolvedActorCombat = 92;
constexpr uint32_t SaveVersionBreachAlvarCompanions = 91;
constexpr uint32_t SaveVersionDetachedPartyMembers = 90;
constexpr uint32_t SaveVersionPendingItemDropPolicy = 89;
constexpr uint32_t SaveVersionPendingItemDelivery = 88;
constexpr uint32_t SaveVersionPreciseGameClock = 87;
constexpr uint32_t SaveVersionChestItemRecords = 86;
constexpr uint32_t SaveVersionTurnBasedCombat = 85;
constexpr uint32_t SaveVersionIndoorCamera = 84;
constexpr uint32_t SaveVersionAttackSpell = 19;
constexpr uint32_t SaveVersionIndoorCorpseViews = 21;
constexpr uint32_t SaveVersionIndoorChestViews = 22;
constexpr uint32_t SaveVersionIndoorActorSpellEffects = 23;
constexpr uint32_t SaveVersionIndoorActorAiStates = 24;
constexpr uint32_t SaveVersionWandCharges = 25;
constexpr uint32_t SaveVersionLloydBeaconPreview = 26;
constexpr uint32_t SaveVersionActorCarriedItem = 27;
constexpr uint32_t SaveVersionDungeonTransitionDialogue = 28;
constexpr uint32_t SaveVersionIndoorSaveLoadParity = 29;
constexpr uint32_t SaveVersionCombatEffectState = 30;
constexpr uint32_t SaveVersionGlobalNpcState = 31;
constexpr uint32_t SaveVersionInputPromptAnswers = 32;
constexpr uint32_t SaveVersionDialogueParticipantPicture = 33;
constexpr uint32_t SaveVersionOutdoorJournalRevealMask = 34;
constexpr uint32_t SaveVersionRuntimeMapNotes = 35;
constexpr uint32_t SaveVersionScopedHistory = 36;
constexpr uint32_t SaveVersionMapLuaRuntimeOverlays = 37;
constexpr uint32_t SaveVersionHiredNpcFollowers = 38;
constexpr uint32_t SaveVersionGeneratedNpcOverrides = 39;
constexpr uint32_t SaveVersionDialogueActorSource = 40;
constexpr uint32_t SaveVersionPartyHiredNpcFollowers = 41;
constexpr uint32_t SaveVersionRuntimeMapNoteMapFile = 42;
constexpr uint32_t SaveVersionPartyEverOwnedItems = 43;
constexpr uint32_t SaveVersionNamedLuaVars = 44;
constexpr uint32_t SaveVersionActorExtraItemOverrides = 45;
constexpr uint32_t SaveVersionInputPromptAnswerSteps = 46;
constexpr uint32_t SaveVersionPersistentLuaRuntimeState = 47;
constexpr uint32_t SaveVersionOutdoorModelMechanisms = 48;
constexpr uint32_t SaveVersionOutdoorActorBolsterCache = 49;
constexpr uint32_t SaveVersionGeneratedMercenaryRecruits = 50;
constexpr uint32_t SaveVersionSessionNamedGlobalVars = 51;
constexpr uint32_t SaveVersionGeneratedMercenaryNpcPicture = 52;
constexpr uint32_t SaveVersionPartyContinentReputations = 53;
constexpr uint32_t SaveVersionPartyFineGold = 54;
constexpr uint32_t SaveVersionOutdoorLocationInfo = 55;
constexpr uint32_t SaveVersionFullProjectileState = 56;
constexpr uint32_t SaveVersionCharacterConditionStartTimes = 57;
constexpr uint32_t SaveVersionHiredNpcFollowerAbilityUseDay = 58;
constexpr uint32_t SaveVersionMonsterBolsterRewards = 59;
constexpr uint32_t SaveVersionMonsterBolsterDamageDice = 60;
constexpr uint32_t SaveVersionPendingSoundNames = 61;
constexpr uint32_t SaveVersionOutdoorRuntimeSaveParity = 62;
constexpr uint32_t SaveVersionProjectileVisualMode = 63;
constexpr uint32_t SaveVersionIndoorFireSpikeTraps = 64;
constexpr uint32_t SaveVersionHeldCursorItem = 65;
constexpr uint32_t SaveVersionActorDiagnosticSource = 66;
constexpr uint32_t SaveVersionLegacyEvtTimers = 67;
constexpr uint32_t SaveVersionHiredNpcFollowerIdentity = 68;
constexpr uint32_t SaveVersionOutdoorFaceAttributes = 69;
constexpr uint32_t SaveVersionOutdoorMechanismRotationDirection = 70;
constexpr uint32_t SaveVersionMm9DialogueActors = 71;
constexpr uint32_t SaveVersionRequiredContentPackages = 72;
constexpr uint32_t SaveVersionItemWeeklyEffects = 73;
constexpr uint32_t SaveVersionMm9ActorReactions = 74;
constexpr uint32_t SaveVersionActorProceduralDeathLoot = 75;
constexpr uint32_t SaveVersionMm9ItemSourceState = 76;
constexpr uint32_t SaveVersionMm9SemanticWorldItems = 77;
constexpr uint32_t SaveVersionMm9IndoorSemanticWorldItems = 78;
constexpr uint32_t SaveVersionTimedPrimaryStatBonuses = 79;
constexpr uint32_t SaveVersionSuspendedMm9RudeVendor = 80;
constexpr uint32_t SaveVersionVendorStockGeneration = 80;
constexpr uint32_t SaveVersionOutdoorDestructibles = 81;
constexpr uint32_t SaveVersionMm9Barrels = 82;
constexpr uint32_t SaveVersionTemporaryEventBonuses = 83;
constexpr uint32_t SaveVersionConsumedCorpseMarkers = 83;
constexpr char SaveMagic[8] = {'O', 'Y', 'S', 'A', 'V', 'E', '1', '\0'};

std::string toLowerCopy(const std::string &value)
{
    std::string normalized = value;

    for (char &character : normalized)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    return normalized;
}

int savePathDisplayPriority(const std::filesystem::path &path)
{
    const std::string stem = toLowerCopy(path.stem().string());

    if (stem == "autosave")
    {
        return 0;
    }

    if (stem == "quicksave")
    {
        return 1;
    }

    return 2;
}

class BinaryWriter
{
public:
    explicit BinaryWriter(bool canonicalCollections = false) : m_canonicalCollections(canonicalCollections) {}
    bool canonicalCollections() const { return m_canonicalCollections; }

    void writeBytes(const void *pData, size_t size)
    {
        const uint8_t *pBytes = static_cast<const uint8_t *>(pData);
        m_bytes.insert(m_bytes.end(), pBytes, pBytes + size);
    }

    template <typename T>
    void write(const T &value)
    {
        writeValue(*this, value);
    }

    const std::vector<uint8_t> &bytes() const
    {
        return m_bytes;
    }

private:
    std::vector<uint8_t> m_bytes;
    bool m_canonicalCollections = false;
};

class BinaryReader
{
public:
    explicit BinaryReader(const std::vector<uint8_t> &bytes)
        : m_bytes(bytes)
    {
    }

    bool readBytes(void *pData, size_t size)
    {
        if (size > remainingBytes())
        {
            m_failed = true;
            return false;
        }

        std::memcpy(pData, m_bytes.data() + m_offset, size);
        m_offset += size;
        return true;
    }

    template <typename T>
    bool read(T &value)
    {
        return readValue(*this, value);
    }

    bool failed() const
    {
        return m_failed;
    }

    size_t remainingBytes() const
    {
        if (m_offset >= m_bytes.size())
        {
            return 0;
        }

        return m_bytes.size() - m_offset;
    }

    void setVersion(uint32_t version)
    {
        m_version = version;
    }

    uint32_t version() const
    {
        return m_version;
    }

private:
    const std::vector<uint8_t> &m_bytes;
    size_t m_offset = 0;
    bool m_failed = false;
    uint32_t m_version = SaveVersion;
};

void writeValue(BinaryWriter &writer, const HiredNpcFollower &value);
bool readValue(BinaryReader &reader, HiredNpcFollower &value);
void writeValue(BinaryWriter &writer, const BreachAlvarCompanionState &value);
bool readValue(BinaryReader &reader, BreachAlvarCompanionState &value);
void writeValue(BinaryWriter &writer, const BreachMazeRunData &value);
bool readValue(BinaryReader &reader, BreachMazeRunData &value);
void writeValue(BinaryWriter &writer, const FixedItemGenerationCursor &value);
bool readValue(BinaryReader &reader, FixedItemGenerationCursor &value);
void writeValue(BinaryWriter &writer, const BreachMazeTrapState &value);
bool readValue(BinaryReader &reader, BreachMazeTrapState &value);
void writeValue(BinaryWriter &writer, const BreachMazeDecorationState &value);
bool readValue(BinaryReader &reader, BreachMazeDecorationState &value);
void writeValue(BinaryWriter &writer, const BreachMazeInteractionState &value);
bool readValue(BinaryReader &reader, BreachMazeInteractionState &value);
void writeValue(BinaryWriter &writer, const GameplayProjectileService::Snapshot &value);
bool readValue(BinaryReader &reader, GameplayProjectileService::Snapshot &value);

bool canReadSerializedElementCount(const BinaryReader &reader, uint64_t count)
{
    if (count > std::numeric_limits<size_t>::max())
    {
        return false;
    }

    return count <= reader.remainingBytes();
}

template <typename T>
requires(std::is_arithmetic_v<T>)
void writeValue(BinaryWriter &writer, const T &value)
{
    writer.writeBytes(&value, sizeof(value));
}

template <typename T>
requires(std::is_arithmetic_v<T>)
bool readValue(BinaryReader &reader, T &value)
{
    return reader.readBytes(&value, sizeof(value));
}

bool readGameMinutes(BinaryReader &reader, double &minutes)
{
    if (reader.version() < SaveVersionPreciseGameClock)
    {
        float legacyMinutes = 0.0f;
        if (!readValue(reader, legacyMinutes))
        {
            return false;
        }
        minutes = legacyMinutes;
    }
    else if (!readValue(reader, minutes))
    {
        return false;
    }
    return validGameMinutes(minutes);
}

template <typename T>
requires(std::is_enum_v<T>)
void writeValue(BinaryWriter &writer, const T &value)
{
    const std::underlying_type_t<T> rawValue = static_cast<std::underlying_type_t<T>>(value);
    writeValue(writer, rawValue);
}

template <typename T>
requires(std::is_enum_v<T>)
bool readValue(BinaryReader &reader, T &value)
{
    std::underlying_type_t<T> rawValue = {};

    if (!readValue(reader, rawValue))
    {
        return false;
    }

    value = static_cast<T>(rawValue);
    return true;
}

void writeValue(BinaryWriter &writer, const std::string &value)
{
    const uint64_t size = static_cast<uint64_t>(value.size());
    writeValue(writer, size);

    if (!value.empty())
    {
        writer.writeBytes(value.data(), value.size());
    }
}

bool readValue(BinaryReader &reader, std::string &value)
{
    uint64_t size = 0;

    if (!readValue(reader, size))
    {
        return false;
    }

    if (!canReadSerializedElementCount(reader, size))
    {
        return false;
    }

    value.resize(static_cast<size_t>(size));
    return value.empty() || reader.readBytes(value.data(), value.size());
}

template <size_t N>
void writeValue(BinaryWriter &writer, const std::bitset<N> &value)
{
    writeValue(writer, value.to_string());
}

template <size_t N>
bool readValue(BinaryReader &reader, std::bitset<N> &value)
{
    std::string encoded;

    if (!readValue(reader, encoded) || encoded.size() != N)
    {
        return false;
    }

    value = std::bitset<N>(encoded);
    return true;
}

template <typename T>
void writeValue(BinaryWriter &writer, const std::optional<T> &value)
{
    writeValue(writer, value.has_value());

    if (value.has_value())
    {
        writeValue(writer, *value);
    }
}

template <typename T>
bool readValue(BinaryReader &reader, std::optional<T> &value)
{
    bool hasValue = false;

    if (!readValue(reader, hasValue))
    {
        return false;
    }

    if (!hasValue)
    {
        value.reset();
        return true;
    }

    T decoded = {};

    if (!readValue(reader, decoded))
    {
        return false;
    }

    value = std::move(decoded);
    return true;
}

template <typename T>
void writeValue(BinaryWriter &writer, const std::vector<T> &value)
{
    const uint64_t size = static_cast<uint64_t>(value.size());
    writeValue(writer, size);

    for (const T &entry : value)
    {
        writeValue(writer, entry);
    }
}

template <typename T>
bool readValue(BinaryReader &reader, std::vector<T> &value)
{
    uint64_t size = 0;

    if (!readValue(reader, size))
    {
        return false;
    }

    if (!canReadSerializedElementCount(reader, size))
    {
        return false;
    }

    value.clear();
    value.reserve(static_cast<size_t>(size));

    for (uint64_t index = 0; index < size; ++index)
    {
        T entry = {};

        if (!readValue(reader, entry))
        {
            return false;
        }

        value.push_back(std::move(entry));
    }

    return true;
}

template <typename T, size_t N>
void writeValue(BinaryWriter &writer, const std::array<T, N> &value)
{
    for (const T &entry : value)
    {
        writeValue(writer, entry);
    }
}

template <typename T, size_t N>
bool readValue(BinaryReader &reader, std::array<T, N> &value)
{
    for (T &entry : value)
    {
        if (!readValue(reader, entry))
        {
            return false;
        }
    }

    return true;
}

template <typename K, typename V>
void writeValue(BinaryWriter &writer, const std::unordered_map<K, V> &value)
{
    const uint64_t size = static_cast<uint64_t>(value.size());
    writeValue(writer, size);

    if (writer.canonicalCollections())
    {
        using Entry = typename std::unordered_map<K, V>::value_type;
        std::vector<const Entry *> entries;
        entries.reserve(value.size());
        for (const Entry &entry : value)
        {
            entries.push_back(&entry);
        }
        std::sort(entries.begin(), entries.end(),
            [](const Entry *pLeft, const Entry *pRight) { return pLeft->first < pRight->first; });
        for (const Entry *pEntry : entries)
        {
            writeValue(writer, pEntry->first);
            writeValue(writer, pEntry->second);
        }
        return;
    }

    for (const auto &[key, entry] : value)
    {
        writeValue(writer, key);
        writeValue(writer, entry);
    }
}

template <typename K, typename V>
bool readValue(BinaryReader &reader, std::unordered_map<K, V> &value)
{
    uint64_t size = 0;

    if (!readValue(reader, size))
    {
        return false;
    }

    if (!canReadSerializedElementCount(reader, size))
    {
        return false;
    }

    value.clear();
    value.reserve(static_cast<size_t>(size));

    for (uint64_t index = 0; index < size; ++index)
    {
        K key = {};
        V entry = {};

        if (!readValue(reader, key) || !readValue(reader, entry))
        {
            return false;
        }

        value.emplace(std::move(key), std::move(entry));
    }

    return true;
}

template <typename T>
void writeValue(BinaryWriter &writer, const std::unordered_set<T> &value)
{
    const uint64_t size = static_cast<uint64_t>(value.size());
    writeValue(writer, size);

    if (writer.canonicalCollections())
    {
        std::vector<T> entries(value.begin(), value.end());
        std::sort(entries.begin(), entries.end());
        for (const T &entry : entries)
        {
            writeValue(writer, entry);
        }
        return;
    }

    for (const T &entry : value)
    {
        writeValue(writer, entry);
    }
}

template <typename T>
bool readValue(BinaryReader &reader, std::unordered_set<T> &value)
{
    uint64_t size = 0;

    if (!readValue(reader, size))
    {
        return false;
    }

    if (!canReadSerializedElementCount(reader, size))
    {
        return false;
    }

    value.clear();
    value.reserve(static_cast<size_t>(size));

    for (uint64_t index = 0; index < size; ++index)
    {
        T entry = {};

        if (!readValue(reader, entry))
        {
            return false;
        }

        value.insert(std::move(entry));
    }

    return true;
}

void writeValue(BinaryWriter &writer, const CharacterSkill &value)
{
    writeValue(writer, value.name);
    writeValue(writer, value.level);
    writeValue(writer, value.mastery);
}

bool readValue(BinaryReader &reader, CharacterSkill &value)
{
    return readValue(reader, value.name)
        && readValue(reader, value.level)
        && readValue(reader, value.mastery);
}

void writeValue(BinaryWriter &writer, const InventoryItem &value)
{
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.quantity);
    writeValue(writer, value.width);
    writeValue(writer, value.height);
    writeValue(writer, value.gridX);
    writeValue(writer, value.gridY);
    writeValue(writer, value.identified);
    writeValue(writer, value.broken);
    writeValue(writer, value.stolen);
    writeValue(writer, value.standardEnchantId);
    writeValue(writer, value.standardEnchantPower);
    writeValue(writer, value.specialEnchantId);
    writeValue(writer, value.artifactId);
    writeValue(writer, value.rarity);
    writeValue(writer, value.currentCharges);
    writeValue(writer, value.maxCharges);
    writeValue(writer, value.temporaryBonusRemainingSeconds);
}

bool readValue(BinaryReader &reader, InventoryItem &value)
{
    return readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.quantity)
        && readValue(reader, value.width)
        && readValue(reader, value.height)
        && readValue(reader, value.gridX)
        && readValue(reader, value.gridY)
        && readValue(reader, value.identified)
        && readValue(reader, value.broken)
        && readValue(reader, value.stolen)
        && readValue(reader, value.standardEnchantId)
        && readValue(reader, value.standardEnchantPower)
        && readValue(reader, value.specialEnchantId)
        && readValue(reader, value.artifactId)
        && readValue(reader, value.rarity)
        && (reader.version() < SaveVersionWandCharges || readValue(reader, value.currentCharges))
        && (reader.version() < SaveVersionWandCharges || readValue(reader, value.maxCharges))
        && readValue(reader, value.temporaryBonusRemainingSeconds);
}

void writeValue(BinaryWriter &writer, const MergeBolsterAttack &value)
{
    writeValue(writer, value.type);
    writeValue(writer, value.diceCount);
    writeValue(writer, value.diceSides);
    writeValue(writer, value.damageAdd);
    writeValue(writer, value.missile);
}

bool readValue(BinaryReader &reader, MergeBolsterAttack &value)
{
    return readValue(reader, value.type)
        && readValue(reader, value.diceCount)
        && readValue(reader, value.diceSides)
        && readValue(reader, value.damageAdd)
        && readValue(reader, value.missile);
}

void writeValue(BinaryWriter &writer, const MergeBolsterSpell &value)
{
    writeValue(writer, value.id);
    writeValue(writer, value.chance);
    writeValue(writer, value.packedSkill);
}

bool readValue(BinaryReader &reader, MergeBolsterSpell &value)
{
    return readValue(reader, value.id)
        && readValue(reader, value.chance)
        && readValue(reader, value.packedSkill);
}

void writeValue(BinaryWriter &writer, const MergeBolsterSpecial &value)
{
    writeValue(writer, value.kind);
    writeValue(writer, value.a);
    writeValue(writer, value.b);
    writeValue(writer, value.c);
    writeValue(writer, value.d);
}

bool readValue(BinaryReader &reader, MergeBolsterSpecial &value)
{
    return readValue(reader, value.kind)
        && readValue(reader, value.a)
        && readValue(reader, value.b)
        && readValue(reader, value.c)
        && readValue(reader, value.d);
}

void writeValue(BinaryWriter &writer, const MergeBolsterRewards &value)
{
    writeValue(writer, value.experience);
    writeValue(writer, value.goldDiceCount);
    writeValue(writer, value.goldDiceSides);
    writeValue(writer, value.itemChance);
    writeValue(writer, value.itemLevel);
    writeValue(writer, value.itemType);
}

bool readValue(BinaryReader &reader, MergeBolsterRewards &value)
{
    return readValue(reader, value.experience)
        && readValue(reader, value.goldDiceCount)
        && readValue(reader, value.goldDiceSides)
        && readValue(reader, value.itemChance)
        && readValue(reader, value.itemLevel)
        && readValue(reader, value.itemType);
}

void writeValue(BinaryWriter &writer, const MergeBolsterCombatStats &value)
{
    writeValue(writer, value.id);
    writeValue(writer, value.level);
    writeValue(writer, value.fullHitPoints);
    writeValue(writer, value.armorClass);
    writeValue(writer, value.moveSpeed);
    writeValue(writer, value.attackRecovery);
    writeValue(writer, value.fly);
    writeValue(writer, value.moveType);
    writeValue(writer, value.aiType);
    writeValue(writer, value.hostileType);
    writeValue(writer, value.bonus);
    writeValue(writer, value.bonusMultiplier);
    writeValue(writer, value.preferredTargetCount);
    writeValue(writer, value.preferredTargets);
    writeValue(writer, value.bloodSplat);
    writeValue(writer, value.attack1);
    writeValue(writer, value.attack2);
    writeValue(writer, value.attack2Chance);
    writeValue(writer, value.spells);
    writeValue(writer, value.resistances);
    writeValue(writer, value.special);
    writeValue(writer, value.rewards);
}

bool readValue(BinaryReader &reader, MergeBolsterCombatStats &value)
{
    return readValue(reader, value.id)
        && readValue(reader, value.level)
        && readValue(reader, value.fullHitPoints)
        && readValue(reader, value.armorClass)
        && readValue(reader, value.moveSpeed)
        && readValue(reader, value.attackRecovery)
        && readValue(reader, value.fly)
        && readValue(reader, value.moveType)
        && readValue(reader, value.aiType)
        && readValue(reader, value.hostileType)
        && readValue(reader, value.bonus)
        && readValue(reader, value.bonusMultiplier)
        && readValue(reader, value.preferredTargetCount)
        && readValue(reader, value.preferredTargets)
        && readValue(reader, value.bloodSplat)
        && readValue(reader, value.attack1)
        && readValue(reader, value.attack2)
        && readValue(reader, value.attack2Chance)
        && readValue(reader, value.spells)
        && readValue(reader, value.resistances)
        && readValue(reader, value.special)
        && readValue(reader, value.rewards);
}

void writeValue(BinaryWriter &writer, const MergeBolsterMonsterSettings &value)
{
    writeValue(writer, value.style);
    writeValue(writer, value.creed);
    writeValue(writer, value.magic);
    writeValue(writer, value.ranged);
    writeValue(writer, value.spells);
    writeValue(writer, value.hpBySize);
    writeValue(writer, value.replicate);
    writeValue(writer, value.summons);
    writeValue(writer, value.summonId);
    writeValue(writer, value.levelShift);
    writeValue(writer, value.maximumHpBoostPercent);
}

bool readValue(BinaryReader &reader, MergeBolsterMonsterSettings &value)
{
    return readValue(reader, value.style)
        && readValue(reader, value.creed)
        && readValue(reader, value.magic)
        && readValue(reader, value.ranged)
        && readValue(reader, value.spells)
        && readValue(reader, value.hpBySize)
        && readValue(reader, value.replicate)
        && readValue(reader, value.summons)
        && readValue(reader, value.summonId)
        && readValue(reader, value.levelShift)
        && readValue(reader, value.maximumHpBoostPercent);
}

void writeValue(BinaryWriter &writer, const MergeBolsterSummonReference &value)
{
    writeValue(writer, value.id);
    writeValue(writer, value.fly);
}

bool readValue(BinaryReader &reader, MergeBolsterSummonReference &value)
{
    return readValue(reader, value.id)
        && readValue(reader, value.fly);
}

void writeValue(BinaryWriter &writer, const MergeBolsterFamilyMember &value)
{
    writeValue(writer, value.original);
    writeValue(writer, value.settings);
    writeValue(writer, value.modelHeight);
    writeValue(writer, value.summonReference);
}

bool readValue(BinaryReader &reader, MergeBolsterFamilyMember &value)
{
    return readValue(reader, value.original)
        && readValue(reader, value.settings)
        && readValue(reader, value.modelHeight)
        && readValue(reader, value.summonReference);
}

void writeValue(BinaryWriter &writer, const MergeBolsterMapRules &value)
{
    writeValue(writer, value.mapId);
    writeValue(writer, value.kind);
    writeValue(writer, value.spells);
    writeValue(writer, value.summons);
    writeValue(writer, value.levelShift);
}

bool readValue(BinaryReader &reader, MergeBolsterMapRules &value)
{
    return readValue(reader, value.mapId)
        && readValue(reader, value.kind)
        && readValue(reader, value.spells)
        && readValue(reader, value.summons)
        && readValue(reader, value.levelShift);
}

void writeValue(BinaryWriter &writer, const MergeBolsterMultipliers &value)
{
    writeValue(writer, value.hp);
    writeValue(writer, value.ac);
    writeValue(writer, value.maxDamage);
    writeValue(writer, value.moveSpeed);
    writeValue(writer, value.playerAc);
}

bool readValue(BinaryReader &reader, MergeBolsterMultipliers &value)
{
    return readValue(reader, value.hp)
        && readValue(reader, value.ac)
        && readValue(reader, value.maxDamage)
        && readValue(reader, value.moveSpeed)
        && readValue(reader, value.playerAc);
}

void writeValue(BinaryWriter &writer, const MergeBolsterFamilyInput &value)
{
    writeValue(writer, value.members);
    writeValue(writer, value.map);
    writeValue(writer, value.effectivePartyLevel);
    writeValue(writer, value.bolsterAmountPercent);
    writeValue(writer, value.multipliers);
    writeValue(writer, value.enabled);
    writeValue(writer, value.templatesLocalized);
    writeValue(writer, value.familySeed);
}

bool readValue(BinaryReader &reader, MergeBolsterFamilyInput &value)
{
    return readValue(reader, value.members)
        && readValue(reader, value.map)
        && readValue(reader, value.effectivePartyLevel)
        && readValue(reader, value.bolsterAmountPercent)
        && readValue(reader, value.multipliers)
        && readValue(reader, value.enabled)
        && readValue(reader, value.templatesLocalized)
        && readValue(reader, value.familySeed);
}

void writeValue(BinaryWriter &writer, const MergeMonsterFamilyContext &value)
{
    writeValue(writer, value.map);
    writeValue(writer, value.effectivePartyLevel);
    writeValue(writer, value.bolsterAmountPercent);
    writeValue(writer, value.multipliers);
    writeValue(writer, value.enabled);
    writeValue(writer, value.templatesLocalized);
    writeValue(writer, value.familySeed);
}

bool readValue(BinaryReader &reader, MergeMonsterFamilyContext &value)
{
    return readValue(reader, value.map)
        && readValue(reader, value.effectivePartyLevel)
        && readValue(reader, value.bolsterAmountPercent)
        && readValue(reader, value.multipliers)
        && readValue(reader, value.enabled)
        && readValue(reader, value.templatesLocalized)
        && readValue(reader, value.familySeed);
}

void writeValue(BinaryWriter &writer, const MergeMonsterPreparedFamily &value)
{
    writeValue(writer, value.familyId);
    writeValue(writer, value.input);
    writeValue(writer, value.templates);
    writeValue(writer, value.familyLevel);
    writeValue(writer, value.bolsterStep);
    writeValue(writer, value.rawDrawCount);
}

bool readValue(BinaryReader &reader, MergeMonsterPreparedFamily &value)
{
    return readValue(reader, value.familyId)
        && readValue(reader, value.input)
        && readValue(reader, value.templates)
        && readValue(reader, value.familyLevel)
        && readValue(reader, value.bolsterStep)
        && readValue(reader, value.rawDrawCount);
}

void writeValue(BinaryWriter &writer, const MergeMonsterGenerationState &value)
{
    writeValue(writer, value.version);
    writeValue(writer, value.sourceProfileVersion);
    writeValue(writer, value.sourceContentFixVersion);
    writeValue(writer, value.bolsterProfileVersion);
    writeValue(writer, value.bolsterRandomVersion);
    writeValue(writer, value.seedVersion);
    writeValue(writer, value.contentPackageId);
    writeValue(writer, value.contentPackageSchema);
    writeValue(writer, value.catalogSha256);
    writeValue(writer, value.profileSha256);
    writeValue(writer, value.mechanicalFingerprint);
    writeValue(writer, value.descriptorFingerprint);
    writeValue(writer, value.context);
    writeValue(writer, value.populationSeed);
    writeValue(writer, value.generationEpoch);
    writeValue(writer, value.generationLevel);
    writeValue(writer, value.generationSerial);
    writeValue(writer, value.nextActorOrdinal);
    writeValue(writer, value.preparedFamilies);
}

bool readValue(BinaryReader &reader, MergeMonsterGenerationState &value)
{
    const bool loaded = readValue(reader, value.version)
        && readValue(reader, value.sourceProfileVersion)
        && readValue(reader, value.sourceContentFixVersion)
        && readValue(reader, value.bolsterProfileVersion)
        && readValue(reader, value.bolsterRandomVersion)
        && readValue(reader, value.seedVersion)
        && readValue(reader, value.contentPackageId)
        && readValue(reader, value.contentPackageSchema)
        && readValue(reader, value.catalogSha256)
        && readValue(reader, value.profileSha256)
        && readValue(reader, value.mechanicalFingerprint)
        && readValue(reader, value.descriptorFingerprint)
        && readValue(reader, value.context)
        && readValue(reader, value.populationSeed)
        && readValue(reader, value.generationEpoch)
        && readValue(reader, value.generationLevel)
        && readValue(reader, value.generationSerial)
        && readValue(reader, value.nextActorOrdinal)
        && readValue(reader, value.preparedFamilies);
    std::string error;
    return loaded && validateMergeMonsterGenerationState(value, error);
}

void writeValue(BinaryWriter &writer, const GameplayMergeResolvedActorCombat &value)
{
    writeValue(writer, value.schemaVersion);
    writeValue(writer, value.profileVersion);
    writeValue(writer, value.randomVersion);
    writeValue(writer, value.contentPackageId);
    writeValue(writer, value.contentPackageSchema);
    writeValue(writer, value.catalogSha256);
    writeValue(writer, value.profileSha256);
    writeValue(writer, value.familySeed);
    writeValue(writer, value.actorSeed);
    writeValue(writer, value.generationEpoch);
    writeValue(writer, value.generationLevel);
    writeValue(writer, value.generationSerial);
    writeValue(writer, value.appliedModifiers);
    writeValue(writer, value.playerArmorClassPrepared);
    writeValue(writer, value.bolsterAmountPercent);
    writeValue(writer, value.playerArmorClassMultiplier);
    writeValue(writer, value.combat);
}

bool readValue(BinaryReader &reader, GameplayMergeResolvedActorCombat &value)
{
    return readValue(reader, value.schemaVersion)
        && readValue(reader, value.profileVersion)
        && readValue(reader, value.randomVersion)
        && readValue(reader, value.contentPackageId)
        && readValue(reader, value.contentPackageSchema)
        && readValue(reader, value.catalogSha256)
        && readValue(reader, value.profileSha256)
        && readValue(reader, value.familySeed)
        && readValue(reader, value.actorSeed)
        && readValue(reader, value.generationEpoch)
        && readValue(reader, value.generationLevel)
        && readValue(reader, value.generationSerial)
        && readValue(reader, value.appliedModifiers)
        && readValue(reader, value.playerArmorClassPrepared)
        && readValue(reader, value.bolsterAmountPercent)
        && readValue(reader, value.playerArmorClassMultiplier)
        && readValue(reader, value.combat);
}

void writeValue(BinaryWriter &writer, const GameplayActorCombatState &value)
{
    writeValue(writer, value.mode());
    if (const GameplayActorLegacySnapshot *pLegacy = value.legacySnapshot())
    {
        writeValue(writer, pLegacy->maximumHitPoints);
    }
    else if (const GameplayMergeResolvedActorCombat *pResolved = value.resolved())
    {
        writeValue(writer, *pResolved);
    }
}

bool readValue(BinaryReader &reader, GameplayActorCombatState &value)
{
    GameplayActorCombatMode mode = GameplayActorCombatMode::LegacyDynamic;
    if (!readValue(reader, mode))
    {
        return false;
    }
    switch (mode)
    {
    case GameplayActorCombatMode::LegacyDynamic:
        value = GameplayActorCombatState{};
        return true;
    case GameplayActorCombatMode::LegacySnapshot:
    {
        GameplayActorLegacySnapshot legacy;
        if (!readValue(reader, legacy.maximumHitPoints))
        {
            return false;
        }
        value = GameplayActorCombatState(legacy);
        return true;
    }
    case GameplayActorCombatMode::MergeResolvedV1:
    {
        GameplayMergeResolvedActorCombat resolved;
        if (!readValue(reader, resolved))
        {
            return false;
        }
        value = GameplayActorCombatState(std::move(resolved));
        std::string error;
        return validateGameplayActorCombatState(value, 0, error);
    }
    }
    return false;
}

// The original map records and OY saves through 91 both used signed i16 current HP.
bool readMapActorHitPoints(BinaryReader &reader, int32_t &value)
{
    if (reader.version() >= SaveVersionResolvedActorCombat)
    {
        return readValue(reader, value);
    }
    int16_t legacy = 0;
    if (!readValue(reader, legacy))
    {
        return false;
    }
    value = legacy;
    return true;
}

bool readLegacyOutdoorMaximumHitPoints(BinaryReader &reader, GameplayActorCombatState &state)
{
    if (reader.version() >= SaveVersionResolvedActorCombat)
    {
        return true; // Save92 writes the exclusive state once, at the actor record's tail.
    }
    int32_t legacy = 0;
    if (!readValue(reader, legacy))
    {
        return false;
    }
    state = GameplayActorCombatState(GameplayActorLegacySnapshot{legacy});
    return true;
}

bool validContainingItem(const MapDeltaSpriteObject &object)
{
    if (!object.containingItem)
    {
        return object.containingGoldAmount == 0;
    }
    const InventoryItem &item = *object.containingItem;
    return object.rawContainingItem.empty() && item.objectDescriptionId != 0 && item.quantity != 0
        && item.width != 0 && item.height != 0 && item.currentCharges <= item.maxCharges
        && std::isfinite(item.temporaryBonusRemainingSeconds) && item.temporaryBonusRemainingSeconds >= 0;
}

void writeValue(BinaryWriter &writer, const LloydBeacon &value)
{
    writeValue(writer, value.mapName);
    writeValue(writer, value.locationName);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.directionDegrees);
    writeValue(writer, value.remainingSeconds);
    writeValue(writer, value.previewWidth);
    writeValue(writer, value.previewHeight);
    writeValue(writer, value.previewPixelsBgra);
}

bool readValue(BinaryReader &reader, LloydBeacon &value)
{
    return readValue(reader, value.mapName)
        && readValue(reader, value.locationName)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.directionDegrees)
        && readValue(reader, value.remainingSeconds)
        && (reader.version() < SaveVersionLloydBeaconPreview || readValue(reader, value.previewWidth))
        && (reader.version() < SaveVersionLloydBeaconPreview || readValue(reader, value.previewHeight))
        && (reader.version() < SaveVersionLloydBeaconPreview || readValue(reader, value.previewPixelsBgra));
}

void writeValue(BinaryWriter &writer, const CharacterResistanceSet &value)
{
    writeValue(writer, value.fire);
    writeValue(writer, value.air);
    writeValue(writer, value.water);
    writeValue(writer, value.earth);
    writeValue(writer, value.mind);
    writeValue(writer, value.body);
    writeValue(writer, value.spirit);
}

bool readValue(BinaryReader &reader, CharacterResistanceSet &value)
{
    return readValue(reader, value.fire)
        && readValue(reader, value.air)
        && readValue(reader, value.water)
        && readValue(reader, value.earth)
        && readValue(reader, value.mind)
        && readValue(reader, value.body)
        && readValue(reader, value.spirit);
}

void writeValue(BinaryWriter &writer, const CharacterResistanceImmunitySet &value)
{
    writeValue(writer, value.fire);
    writeValue(writer, value.air);
    writeValue(writer, value.water);
    writeValue(writer, value.earth);
    writeValue(writer, value.mind);
    writeValue(writer, value.body);
    writeValue(writer, value.spirit);
}

bool readValue(BinaryReader &reader, CharacterResistanceImmunitySet &value)
{
    return readValue(reader, value.fire)
        && readValue(reader, value.air)
        && readValue(reader, value.water)
        && readValue(reader, value.earth)
        && readValue(reader, value.mind)
        && readValue(reader, value.body)
        && readValue(reader, value.spirit);
}

void writeValue(BinaryWriter &writer, const CharacterStatBonuses &value)
{
    writeValue(writer, value.might);
    writeValue(writer, value.intellect);
    writeValue(writer, value.personality);
    writeValue(writer, value.endurance);
    writeValue(writer, value.speed);
    writeValue(writer, value.accuracy);
    writeValue(writer, value.luck);
    writeValue(writer, value.maxHealth);
    writeValue(writer, value.maxSpellPoints);
    writeValue(writer, value.armorClass);
    writeValue(writer, value.meleeAttack);
    writeValue(writer, value.rangedAttack);
    writeValue(writer, value.meleeDamage);
    writeValue(writer, value.rangedDamage);
    writeValue(writer, value.resistances);
}

bool readValue(BinaryReader &reader, CharacterStatBonuses &value)
{
    return readValue(reader, value.might)
        && readValue(reader, value.intellect)
        && readValue(reader, value.personality)
        && readValue(reader, value.endurance)
        && readValue(reader, value.speed)
        && readValue(reader, value.accuracy)
        && readValue(reader, value.luck)
        && readValue(reader, value.maxHealth)
        && readValue(reader, value.maxSpellPoints)
        && readValue(reader, value.armorClass)
        && readValue(reader, value.meleeAttack)
        && readValue(reader, value.rangedAttack)
        && readValue(reader, value.meleeDamage)
        && readValue(reader, value.rangedDamage)
        && readValue(reader, value.resistances);
}

void writeValue(BinaryWriter &writer, const CharacterEquipment &value)
{
    writeValue(writer, value.offHand);
    writeValue(writer, value.mainHand);
    writeValue(writer, value.bow);
    writeValue(writer, value.armor);
    writeValue(writer, value.helm);
    writeValue(writer, value.belt);
    writeValue(writer, value.cloak);
    writeValue(writer, value.gauntlets);
    writeValue(writer, value.boots);
    writeValue(writer, value.amulet);
    writeValue(writer, value.ring1);
    writeValue(writer, value.ring2);
    writeValue(writer, value.ring3);
    writeValue(writer, value.ring4);
    writeValue(writer, value.ring5);
    writeValue(writer, value.ring6);
}

bool readValue(BinaryReader &reader, CharacterEquipment &value)
{
    return readValue(reader, value.offHand)
        && readValue(reader, value.mainHand)
        && readValue(reader, value.bow)
        && readValue(reader, value.armor)
        && readValue(reader, value.helm)
        && readValue(reader, value.belt)
        && readValue(reader, value.cloak)
        && readValue(reader, value.gauntlets)
        && readValue(reader, value.boots)
        && readValue(reader, value.amulet)
        && readValue(reader, value.ring1)
        && readValue(reader, value.ring2)
        && readValue(reader, value.ring3)
        && readValue(reader, value.ring4)
        && readValue(reader, value.ring5)
        && readValue(reader, value.ring6);
}

void writeValue(BinaryWriter &writer, const EquippedItemRuntimeState &value)
{
    writeValue(writer, value.identified);
    writeValue(writer, value.broken);
    writeValue(writer, value.stolen);
    writeValue(writer, value.standardEnchantId);
    writeValue(writer, value.standardEnchantPower);
    writeValue(writer, value.specialEnchantId);
    writeValue(writer, value.artifactId);
    writeValue(writer, value.rarity);
    writeValue(writer, value.currentCharges);
    writeValue(writer, value.maxCharges);
    writeValue(writer, value.temporaryBonusRemainingSeconds);
}

bool readValue(BinaryReader &reader, EquippedItemRuntimeState &value)
{
    return readValue(reader, value.identified)
        && readValue(reader, value.broken)
        && readValue(reader, value.stolen)
        && readValue(reader, value.standardEnchantId)
        && readValue(reader, value.standardEnchantPower)
        && readValue(reader, value.specialEnchantId)
        && readValue(reader, value.artifactId)
        && readValue(reader, value.rarity)
        && (reader.version() < SaveVersionWandCharges || readValue(reader, value.currentCharges))
        && (reader.version() < SaveVersionWandCharges || readValue(reader, value.maxCharges))
        && readValue(reader, value.temporaryBonusRemainingSeconds);
}

void writeValue(BinaryWriter &writer, const CharacterEquipmentRuntimeState &value)
{
    writeValue(writer, value.offHand);
    writeValue(writer, value.mainHand);
    writeValue(writer, value.bow);
    writeValue(writer, value.armor);
    writeValue(writer, value.helm);
    writeValue(writer, value.belt);
    writeValue(writer, value.cloak);
    writeValue(writer, value.gauntlets);
    writeValue(writer, value.boots);
    writeValue(writer, value.amulet);
    writeValue(writer, value.ring1);
    writeValue(writer, value.ring2);
    writeValue(writer, value.ring3);
    writeValue(writer, value.ring4);
    writeValue(writer, value.ring5);
    writeValue(writer, value.ring6);
}

bool readValue(BinaryReader &reader, CharacterEquipmentRuntimeState &value)
{
    return readValue(reader, value.offHand)
        && readValue(reader, value.mainHand)
        && readValue(reader, value.bow)
        && readValue(reader, value.armor)
        && readValue(reader, value.helm)
        && readValue(reader, value.belt)
        && readValue(reader, value.cloak)
        && readValue(reader, value.gauntlets)
        && readValue(reader, value.boots)
        && readValue(reader, value.amulet)
        && readValue(reader, value.ring1)
        && readValue(reader, value.ring2)
        && readValue(reader, value.ring3)
        && readValue(reader, value.ring4)
        && readValue(reader, value.ring5)
        && readValue(reader, value.ring6);
}

void writeValue(BinaryWriter &writer, const TimedPrimaryStatBonus &value)
{
    writeValue(writer, value.power);
    writeValue(writer, value.remainingSeconds);
}

bool readValue(BinaryReader &reader, TimedPrimaryStatBonus &value)
{
    return readValue(reader, value.power)
        && readValue(reader, value.remainingSeconds);
}

void writeValue(BinaryWriter &writer, const Character &value)
{
    writeValue(writer, value.name);
    writeValue(writer, value.role);
    writeValue(writer, value.className);
    writeValue(writer, value.portraitTextureName);
    writeValue(writer, value.portraitPictureId);
    writeValue(writer, value.portraitState);
    writeValue(writer, value.portraitElapsedTicks);
    writeValue(writer, value.portraitDurationTicks);
    writeValue(writer, value.portraitSequenceCounter);
    writeValue(writer, value.portraitImageIndex);
    writeValue(writer, value.rosterId);
    writeValue(writer, value.characterDataId);
    writeValue(writer, value.voiceId);
    writeValue(writer, value.birthYear);
    writeValue(writer, value.sexId);
    writeValue(writer, value.raceId);
    writeValue(writer, value.experience);
    writeValue(writer, value.level);
    writeValue(writer, value.skillPoints);
    writeValue(writer, value.might);
    writeValue(writer, value.intellect);
    writeValue(writer, value.personality);
    writeValue(writer, value.endurance);
    writeValue(writer, value.speed);
    writeValue(writer, value.accuracy);
    writeValue(writer, value.luck);
    writeValue(writer, value.maxHealth);
    writeValue(writer, value.health);
    writeValue(writer, value.maxSpellPoints);
    writeValue(writer, value.spellPoints);
    writeValue(writer, value.quickSpellName);
    writeValue(writer, value.attackSpellName);
    writeValue(writer, value.knownSpellIds);
    writeValue(writer, value.baseResistances);
    writeValue(writer, value.permanentBonuses);
    writeValue(writer, value.temporaryEventBonuses);
    writeValue(writer, value.magicalBonuses);
    writeValue(writer, value.permanentImmunities);
    writeValue(writer, value.magicalImmunities);
    writeValue(writer, value.permanentConditionImmunities);
    writeValue(writer, value.magicalConditionImmunities);
    writeValue(writer, value.equipment);
    writeValue(writer, value.equipmentRuntime);
    writeValue(writer, value.conditions);
    writeValue(writer, value.conditionStartGameMinutes);
    writeValue(writer, value.skills);
    writeValue(writer, value.awards);
    writeValue(writer, value.eventVariables);
    writeValue(writer, value.recoverySecondsRemaining);
    writeValue(writer, value.armorClassModifier);
    writeValue(writer, value.levelModifier);
    writeValue(writer, value.ageModifier);
    writeValue(writer, value.playerBits);
    writeValue(writer, value.npcs2);
    writeValue(writer, value.healthRegenAccumulator);
    writeValue(writer, value.spellRegenAccumulator);
    writeValue(writer, value.lloydsBeacons);
    writeValue(writer, value.inventory);
    writeValue(writer, value.timedPrimaryStatBonuses);
    writeValue(writer, value.memberId);
    writeValue(writer, value.armageddonCastsToday);
    writeValue(writer, value.biography);
}

bool readValue(BinaryReader &reader, Character &value)
{
    return readValue(reader, value.name)
        && readValue(reader, value.role)
        && readValue(reader, value.className)
        && readValue(reader, value.portraitTextureName)
        && readValue(reader, value.portraitPictureId)
        && readValue(reader, value.portraitState)
        && readValue(reader, value.portraitElapsedTicks)
        && readValue(reader, value.portraitDurationTicks)
        && readValue(reader, value.portraitSequenceCounter)
        && readValue(reader, value.portraitImageIndex)
        && readValue(reader, value.rosterId)
        && readValue(reader, value.characterDataId)
        && readValue(reader, value.voiceId)
        && readValue(reader, value.birthYear)
        && readValue(reader, value.sexId)
        && readValue(reader, value.raceId)
        && readValue(reader, value.experience)
        && readValue(reader, value.level)
        && readValue(reader, value.skillPoints)
        && readValue(reader, value.might)
        && readValue(reader, value.intellect)
        && readValue(reader, value.personality)
        && readValue(reader, value.endurance)
        && readValue(reader, value.speed)
        && readValue(reader, value.accuracy)
        && readValue(reader, value.luck)
        && readValue(reader, value.maxHealth)
        && readValue(reader, value.health)
        && readValue(reader, value.maxSpellPoints)
        && readValue(reader, value.spellPoints)
        && readValue(reader, value.quickSpellName)
        && (reader.version() < SaveVersionAttackSpell || readValue(reader, value.attackSpellName))
        && readValue(reader, value.knownSpellIds)
        && readValue(reader, value.baseResistances)
        && readValue(reader, value.permanentBonuses)
        && (reader.version() < SaveVersionTemporaryEventBonuses
            || readValue(reader, value.temporaryEventBonuses))
        && readValue(reader, value.magicalBonuses)
        && readValue(reader, value.permanentImmunities)
        && readValue(reader, value.magicalImmunities)
        && readValue(reader, value.permanentConditionImmunities)
        && readValue(reader, value.magicalConditionImmunities)
        && readValue(reader, value.equipment)
        && readValue(reader, value.equipmentRuntime)
        && readValue(reader, value.conditions)
        && (reader.version() < SaveVersionCharacterConditionStartTimes
            || readValue(reader, value.conditionStartGameMinutes))
        && readValue(reader, value.skills)
        && readValue(reader, value.awards)
        && readValue(reader, value.eventVariables)
        && readValue(reader, value.recoverySecondsRemaining)
        && readValue(reader, value.armorClassModifier)
        && readValue(reader, value.levelModifier)
        && readValue(reader, value.ageModifier)
        && readValue(reader, value.playerBits)
        && readValue(reader, value.npcs2)
        && readValue(reader, value.healthRegenAccumulator)
        && readValue(reader, value.spellRegenAccumulator)
        && readValue(reader, value.lloydsBeacons)
        && readValue(reader, value.inventory)
        && (reader.version() < SaveVersionTimedPrimaryStatBonuses
            || readValue(reader, value.timedPrimaryStatBonuses))
        && (reader.version() < SaveVersionDetachedPartyMembers || readValue(reader, value.memberId))
        && (reader.version() < SaveVersionDetachedPartyMembers || readValue(reader, value.armageddonCastsToday))
        && (reader.version() < SaveVersionBreachAlvarGeneration || readValue(reader, value.biography));
}

void writeValue(BinaryWriter &writer, const PartyBuffState &value)
{
    writeValue(writer, value.remainingSeconds);
    writeValue(writer, value.spellId);
    writeValue(writer, value.skillLevel);
    writeValue(writer, value.skillMastery);
    writeValue(writer, value.power);
    writeValue(writer, value.casterMemberIndex);
}

bool readValue(BinaryReader &reader, PartyBuffState &value)
{
    return readValue(reader, value.remainingSeconds)
        && readValue(reader, value.spellId)
        && readValue(reader, value.skillLevel)
        && readValue(reader, value.skillMastery)
        && readValue(reader, value.power)
        && readValue(reader, value.casterMemberIndex);
}

void writeValue(BinaryWriter &writer, const CharacterBuffState &value)
{
    writeValue(writer, value.remainingSeconds);
    writeValue(writer, value.periodicAccumulatorSeconds);
    writeValue(writer, value.spellId);
    writeValue(writer, value.skillLevel);
    writeValue(writer, value.skillMastery);
    writeValue(writer, value.power);
    writeValue(writer, value.casterMemberIndex);
    writeValue(writer, value.casterMemberId);
}

bool readValue(BinaryReader &reader, CharacterBuffState &value)
{
    return readValue(reader, value.remainingSeconds)
        && readValue(reader, value.periodicAccumulatorSeconds)
        && readValue(reader, value.spellId)
        && readValue(reader, value.skillLevel)
        && readValue(reader, value.skillMastery)
        && readValue(reader, value.power)
        && readValue(reader, value.casterMemberIndex)
        && (reader.version() < SaveVersionDetachedPartyMembers || readValue(reader, value.casterMemberId));
}

void writeValue(BinaryWriter &writer, const Party::HouseStockState &value)
{
    writeValue(writer, value.houseId);
    writeValue(writer, value.nextRefreshGameMinutes);
    writeValue(writer, value.refreshSequence);
    writeValue(writer, value.standardStock);
    writeValue(writer, value.specialStock);
    writeValue(writer, value.spellbookStock);
    writeValue(writer, value.generationVersion);
}

bool readValue(BinaryReader &reader, Party::HouseStockState &value)
{
    return readValue(reader, value.houseId)
        && readValue(reader, value.nextRefreshGameMinutes)
        && readValue(reader, value.refreshSequence)
        && readValue(reader, value.standardStock)
        && readValue(reader, value.specialStock)
        && readValue(reader, value.spellbookStock)
        && (reader.version() < SaveVersionVendorStockGeneration
            || readValue(reader, value.generationVersion));
}

void writeValue(BinaryWriter &writer, const AdventurersInnMember &value)
{
    writeValue(writer, value.character);
    writeValue(writer, value.portraitPictureId);
}

bool readValue(BinaryReader &reader, AdventurersInnMember &value)
{
    return readValue(reader, value.character)
        && readValue(reader, value.portraitPictureId);
}

void writeValue(BinaryWriter &writer, const Party::Snapshot &value)
{
    writeValue(writer, value.members);
    writeValue(writer, value.adventurersInnMembers);
    writeValue(writer, value.activeMemberIndex);
    writeValue(writer, value.partyBuffs);
    writeValue(writer, value.characterBuffs);
    writeValue(writer, value.gold);
    writeValue(writer, value.bankGold);
    writeValue(writer, value.food);
    writeValue(writer, value.fineGold);
    writeValue(writer, value.waterDamageTicks);
    writeValue(writer, value.burningDamageTicks);
    writeValue(writer, value.splashCount);
    writeValue(writer, value.landingSoundCount);
    writeValue(writer, value.hardLandingSoundCount);
    writeValue(writer, value.monsterTargetSelectionCounter);
    writeValue(writer, value.houseStockSeed);
    writeValue(writer, value.itemEffectElapsedGameSeconds);
    writeValue(writer, value.itemWeeklyEffectSequence);
    writeValue(writer, value.lastFallDamageDistance);
    writeValue(writer, value.foundArtifactItems);
    writeValue(writer, value.arcomageWonHouseIds);
    writeValue(writer, value.arcomageWinCount);
    writeValue(writer, value.arcomageLossCount);
    writeValue(writer, value.houseStockStates);
    writeValue(writer, value.everOwnedItemIds);
    writeValue(writer, value.continentReputations);
    writeValue(writer, value.questBits);
    writeValue(writer, value.eventVariables);
    writeValue(writer, value.npcTopicOverrides);
    writeValue(writer, value.npcGroupNews);
    writeValue(writer, value.npcGreetingOverrides);
    writeValue(writer, value.npcGreetingDisplayCounts);
    writeValue(writer, value.npcHouseOverrides);
    writeValue(writer, value.npcItemOverrides);
    writeValue(writer, value.unavailableNpcIds);
    writeValue(writer, value.hiredNpcFollowers);
    writeValue(writer, uint32_t(value.pendingItemDeliveries.size()));
    for (const Party::PendingItemDelivery &delivery : value.pendingItemDeliveries)
    {
        writeValue(writer, delivery.item);
        writeValue(writer, delivery.recipientMemberIndex);
        writeValue(writer, delivery.allowDropProtectedHeldItem);
    }
    writeValue(writer, uint32_t(value.detachedMembers.size()));
    for (const Party::DetachedPartyMember &member : value.detachedMembers)
    {
        writeValue(writer, member.ownerKey);
        writeValue(writer, member.slotId);
        writeValue(writer, member.character);
        writeValue(writer, member.characterBuffs);
        writeValue(writer, uint32_t(member.pendingItemDeliveries.size()));
        for (const Party::PendingItemDelivery &delivery : member.pendingItemDeliveries)
        {
            writeValue(writer, delivery.item);
            writeValue(writer, delivery.recipientMemberIndex);
            writeValue(writer, delivery.allowDropProtectedHeldItem);
        }
    }
    writeValue(writer, value.completedDetachmentOwners);
    writeValue(writer, value.nextMemberId);
}

bool readDetachedPartyMembers(BinaryReader &reader, Party::Snapshot &value)
{
    uint32_t count = 0;
    if (!readValue(reader, count) || count > Party::MaxDetachedMembers)
    {
        return false;
    }
    value.detachedMembers.resize(count);
    size_t pendingCount = value.pendingItemDeliveries.size();
    for (Party::DetachedPartyMember &member : value.detachedMembers)
    {
        uint32_t deliveries = 0;
        if (!readValue(reader, member.ownerKey) || member.ownerKey.size() > 128
            || !readValue(reader, member.slotId) || !readValue(reader, member.character)
            || !readValue(reader, member.characterBuffs) || !readValue(reader, deliveries)
            || deliveries > Party::MaxPendingItemDeliveries - pendingCount)
        {
            return false;
        }
        pendingCount += deliveries;
        member.pendingItemDeliveries.resize(deliveries);
        for (Party::PendingItemDelivery &delivery : member.pendingItemDeliveries)
        {
            if (!readValue(reader, delivery.item) || !readValue(reader, delivery.recipientMemberIndex)
                || !readValue(reader, delivery.allowDropProtectedHeldItem))
            {
                return false;
            }
        }
    }
    // The generic vector writer uses a uint64_t length, unlike the explicit detached-member count above.
    uint64_t ownerCount = 0;
    if (!readValue(reader, ownerCount) || ownerCount > Party::MaxDetachmentBatches)
    {
        return false;
    }
    value.completedDetachmentOwners.resize(static_cast<size_t>(ownerCount));
    for (std::string &owner : value.completedDetachmentOwners)
    {
        if (!readValue(reader, owner) || owner.size() > 128)
        {
            return false;
        }
    }
    return readValue(reader, value.nextMemberId);
}

bool readPendingItemDeliveries(BinaryReader &reader, Party::Snapshot &value)
{
    uint32_t count = 0;
    if (!readValue(reader, count) || count > Party::MaxPendingItemDeliveries)
    {
        return false;
    }
    value.pendingItemDeliveries.resize(count);
    for (Party::PendingItemDelivery &delivery : value.pendingItemDeliveries)
    {
        if (!readValue(reader, delivery.item) || !readValue(reader, delivery.recipientMemberIndex)
            || (reader.version() >= SaveVersionPendingItemDropPolicy
                && !readValue(reader, delivery.allowDropProtectedHeldItem))
            || !Party::validPendingItemDelivery(delivery, value.members.size()))
        {
            return false;
        }
    }
    return true;
}

bool readValue(BinaryReader &reader, Party::Snapshot &value)
{
    return readValue(reader, value.members)
        && readValue(reader, value.adventurersInnMembers)
        && readValue(reader, value.activeMemberIndex)
        && readValue(reader, value.partyBuffs)
        && readValue(reader, value.characterBuffs)
        && readValue(reader, value.gold)
        && readValue(reader, value.bankGold)
        && readValue(reader, value.food)
        && (reader.version() < SaveVersionPartyFineGold || readValue(reader, value.fineGold))
        && readValue(reader, value.waterDamageTicks)
        && readValue(reader, value.burningDamageTicks)
        && readValue(reader, value.splashCount)
        && readValue(reader, value.landingSoundCount)
        && readValue(reader, value.hardLandingSoundCount)
        && readValue(reader, value.monsterTargetSelectionCounter)
        && readValue(reader, value.houseStockSeed)
        && (reader.version() < SaveVersionItemWeeklyEffects
            || readValue(reader, value.itemEffectElapsedGameSeconds))
        && (reader.version() < SaveVersionItemWeeklyEffects
            || readValue(reader, value.itemWeeklyEffectSequence))
        && readValue(reader, value.lastFallDamageDistance)
        && readValue(reader, value.foundArtifactItems)
        && readValue(reader, value.arcomageWonHouseIds)
        && readValue(reader, value.arcomageWinCount)
        && readValue(reader, value.arcomageLossCount)
        && readValue(reader, value.houseStockStates)
        && (reader.version() < SaveVersionPartyEverOwnedItems || readValue(reader, value.everOwnedItemIds))
        && (reader.version() < SaveVersionPartyContinentReputations
            || readValue(reader, value.continentReputations))
        && readValue(reader, value.questBits)
        && readValue(reader, value.eventVariables)
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.npcTopicOverrides))
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.npcGroupNews))
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.npcGreetingOverrides))
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.npcGreetingDisplayCounts))
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.npcHouseOverrides))
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.npcItemOverrides))
        && (reader.version() < SaveVersionGlobalNpcState || readValue(reader, value.unavailableNpcIds))
        && (reader.version() < SaveVersionPartyHiredNpcFollowers || readValue(reader, value.hiredNpcFollowers))
        && (reader.version() < SaveVersionPendingItemDelivery || readPendingItemDeliveries(reader, value))
        && (reader.version() < SaveVersionDetachedPartyMembers || readDetachedPartyMembers(reader, value));
}

void writeValue(BinaryWriter &writer, const OutdoorMoveState &value)
{
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.footZ);
    writeValue(writer, value.verticalVelocity);
    writeValue(writer, value.supportKind);
    writeValue(writer, value.supportBModelIndex);
    writeValue(writer, value.supportFaceIndex);
    writeValue(writer, value.supportIsFluid);
    writeValue(writer, value.supportOnWater);
    writeValue(writer, value.supportOnBurning);
    writeValue(writer, value.airborne);
    writeValue(writer, value.landedThisFrame);
    writeValue(writer, value.fallStartZ);
    writeValue(writer, value.fallDistance);
}

bool readValue(BinaryReader &reader, OutdoorMoveState &value)
{
    return readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.footZ)
        && readValue(reader, value.verticalVelocity)
        && readValue(reader, value.supportKind)
        && readValue(reader, value.supportBModelIndex)
        && readValue(reader, value.supportFaceIndex)
        && readValue(reader, value.supportIsFluid)
        && readValue(reader, value.supportOnWater)
        && readValue(reader, value.supportOnBurning)
        && readValue(reader, value.airborne)
        && readValue(reader, value.landedThisFrame)
        && readValue(reader, value.fallStartZ)
        && readValue(reader, value.fallDistance);
}

void writeValue(BinaryWriter &writer, const IndoorMoveState &value)
{
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.footZ);
    writeValue(writer, value.eyeHeight);
    writeValue(writer, value.verticalVelocity);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.eyeSectorId);
    writeValue(writer, value.supportFaceIndex);
    writeValue(writer, value.grounded);
}

bool readValue(BinaryReader &reader, IndoorMoveState &value)
{
    return readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.footZ)
        && readValue(reader, value.eyeHeight)
        && readValue(reader, value.verticalVelocity)
        && readValue(reader, value.sectorId)
        && readValue(reader, value.eyeSectorId)
        && readValue(reader, value.supportFaceIndex)
        && readValue(reader, value.grounded);
}

void writeValue(BinaryWriter &writer, const OutdoorPartyMovementState &value)
{
    writeValue(writer, value.running);
    writeValue(writer, value.flying);
    writeValue(writer, value.featherFall);
    writeValue(writer, value.waterWalk);
}

bool readValue(BinaryReader &reader, OutdoorPartyMovementState &value)
{
    value.activelyFlying = false;

    return readValue(reader, value.running)
        && readValue(reader, value.flying)
        && readValue(reader, value.featherFall)
        && readValue(reader, value.waterWalk);
}

template <typename PartySnapshot>
bool validSavedPartyMotion(const PartySnapshot &value)
{
    constexpr float MovementStepSeconds = 1.0f / 128.0f;
    constexpr float AccumulatorEpsilon = 0.000001f;
    return validPartyJumpState(value.jumpState)
        && std::isfinite(value.movementState.verticalVelocity)
        && std::isfinite(value.movementAccumulatorSeconds)
        && value.movementAccumulatorSeconds >= 0.0f
        && value.movementAccumulatorSeconds <= MovementStepSeconds + AccumulatorEpsilon;
}

bool readPartyJumpFlag(BinaryReader &reader, bool &value)
{
    uint8_t encoded = 0;
    if (!readValue(reader, encoded) || encoded > 1) return false;
    value = encoded != 0;
    return true;
}

void writeValue(BinaryWriter &writer, const PartyJumpState &value)
{
    // This leading flag is the entire legacy indoor pending-jump payload through Save96.
    writeValue(writer, value.pendingJumpRequested);
    writeValue(writer, value.pendingJumpVelocity);
    writeValue(writer, value.pendingJumpLift);
    writeValue(writer, value.specialJumpActive);
    writeValue(writer, value.velocityX);
    writeValue(writer, value.velocityY);
    writeValue(writer, value.pendingSpecialJump);
    writeValue(writer, value.pendingVelocityX);
    writeValue(writer, value.pendingVelocityY);
    writeValue(writer, value.pendingVelocityZ);
}

bool readValue(BinaryReader &reader, PartyJumpState &value)
{
    value = {};
    if (!readPartyJumpFlag(reader, value.pendingJumpRequested)) return false;
    if (reader.version() < SaveVersionPartyJumpState) return true;

    bool hasJumpVelocity = false;
    if (!readPartyJumpFlag(reader, hasJumpVelocity)) return false;
    if (hasJumpVelocity)
    {
        float velocity = 0.0f;
        if (!readValue(reader, velocity)) return false;
        value.pendingJumpVelocity = velocity;
    }
    return readValue(reader, value.pendingJumpLift)
        && readPartyJumpFlag(reader, value.specialJumpActive)
        && readValue(reader, value.velocityX)
        && readValue(reader, value.velocityY)
        && readPartyJumpFlag(reader, value.pendingSpecialJump)
        && readValue(reader, value.pendingVelocityX)
        && readValue(reader, value.pendingVelocityY)
        && readValue(reader, value.pendingVelocityZ)
        && validPartyJumpState(value);
}

void writeValue(BinaryWriter &writer, const IndoorPartyRuntime::Snapshot &value)
{
    writeValue(writer, value.movementState);
    writeValue(writer, value.movementAccumulatorSeconds);
    writeValue(writer, value.jumpState);
    writeValue(writer, value.alwaysRunEnabled);
}

bool readValue(BinaryReader &reader, IndoorPartyRuntime::Snapshot &value)
{
    value.alwaysRunEnabled = true;
    return readValue(reader, value.movementState)
        && readValue(reader, value.movementAccumulatorSeconds)
        && readValue(reader, value.jumpState)
        && (reader.version() < SaveVersionPartyJumpState || readPartyJumpFlag(reader, value.alwaysRunEnabled))
        && validSavedPartyMotion(value);
}

void writeValue(BinaryWriter &writer, const GameplayActorSpellEffectState &value)
{
    writeValue(writer, value.slowRemainingSeconds);
    writeValue(writer, value.slowMoveMultiplier);
    writeValue(writer, value.slowRecoveryMultiplier);
    writeValue(writer, value.stunRemainingSeconds);
    writeValue(writer, value.paralyzeRemainingSeconds);
    writeValue(writer, value.fearRemainingSeconds);
    writeValue(writer, value.blindRemainingSeconds);
    writeValue(writer, value.controlRemainingSeconds);
    writeValue(writer, value.controlMode);
    writeValue(writer, value.shrinkRemainingSeconds);
    writeValue(writer, value.shrinkDamageMultiplier);
    writeValue(writer, value.shrinkArmorClassMultiplier);
    writeValue(writer, value.armorClassHalvedRemainingSeconds);
    writeValue(writer, value.darkGraspRemainingSeconds);
    writeValue(writer, value.hostileToParty);
    writeValue(writer, value.hasDetectedParty);
    writeValue(writer, value.dayOfProtectionRemainingSeconds);
    writeValue(writer, value.dayOfProtectionPower);
    writeValue(writer, value.hourOfPowerRemainingSeconds);
    writeValue(writer, value.hourOfPowerPower);
    writeValue(writer, value.painReflectionRemainingSeconds);
    writeValue(writer, value.hammerhandsRemainingSeconds);
    writeValue(writer, value.hammerhandsPower);
    writeValue(writer, value.hasteRemainingSeconds);
    writeValue(writer, value.shieldRemainingSeconds);
    writeValue(writer, value.stoneskinRemainingSeconds);
    writeValue(writer, value.stoneskinPower);
    writeValue(writer, value.blessRemainingSeconds);
    writeValue(writer, value.blessPower);
    writeValue(writer, value.fateRemainingSeconds);
    writeValue(writer, value.fatePower);
    writeValue(writer, value.heroismRemainingSeconds);
    writeValue(writer, value.heroismPower);
}

bool readValue(BinaryReader &reader, GameplayActorSpellEffectState &value)
{
    return readValue(reader, value.slowRemainingSeconds)
        && readValue(reader, value.slowMoveMultiplier)
        && readValue(reader, value.slowRecoveryMultiplier)
        && readValue(reader, value.stunRemainingSeconds)
        && readValue(reader, value.paralyzeRemainingSeconds)
        && readValue(reader, value.fearRemainingSeconds)
        && readValue(reader, value.blindRemainingSeconds)
        && readValue(reader, value.controlRemainingSeconds)
        && readValue(reader, value.controlMode)
        && readValue(reader, value.shrinkRemainingSeconds)
        && readValue(reader, value.shrinkDamageMultiplier)
        && readValue(reader, value.shrinkArmorClassMultiplier)
        && (reader.version() < SaveVersionCombatEffectState
            || readValue(reader, value.armorClassHalvedRemainingSeconds))
        && readValue(reader, value.darkGraspRemainingSeconds)
        && readValue(reader, value.hostileToParty)
        && readValue(reader, value.hasDetectedParty)
        && (reader.version() < SaveVersionOutdoorRuntimeSaveParity
            || (readValue(reader, value.dayOfProtectionRemainingSeconds)
                && readValue(reader, value.dayOfProtectionPower)
                && readValue(reader, value.hourOfPowerRemainingSeconds)
                && readValue(reader, value.hourOfPowerPower)
                && readValue(reader, value.painReflectionRemainingSeconds)
                && readValue(reader, value.hammerhandsRemainingSeconds)
                && readValue(reader, value.hammerhandsPower)
                && readValue(reader, value.hasteRemainingSeconds)
                && readValue(reader, value.shieldRemainingSeconds)
                && readValue(reader, value.stoneskinRemainingSeconds)
                && readValue(reader, value.stoneskinPower)
                && readValue(reader, value.blessRemainingSeconds)
                && readValue(reader, value.blessPower)
                && readValue(reader, value.fateRemainingSeconds)
                && readValue(reader, value.fatePower)
                && readValue(reader, value.heroismRemainingSeconds)
                && readValue(reader, value.heroismPower)));
}

void writeValue(BinaryWriter &writer, const IndoorWorldRuntime::BloodSplatState::Vertex &value)
{
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.u);
    writeValue(writer, value.v);
}

bool readValue(BinaryReader &reader, IndoorWorldRuntime::BloodSplatState::Vertex &value)
{
    return readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.u)
        && readValue(reader, value.v);
}

void writeValue(BinaryWriter &writer, const IndoorWorldRuntime::BloodSplatState &value)
{
    writeValue(writer, value.sourceActorId);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.radius);
    writeValue(writer, value.vertices);
}

bool readValue(BinaryReader &reader, IndoorWorldRuntime::BloodSplatState &value)
{
    return readValue(reader, value.sourceActorId)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.radius)
        && readValue(reader, value.vertices);
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::BloodSplatState::Vertex &value)
{
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.u);
    writeValue(writer, value.v);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::BloodSplatState::Vertex &value)
{
    return readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.u)
        && readValue(reader, value.v);
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::BloodSplatState &value)
{
    writeValue(writer, value.sourceActorId);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.radius);
    writeValue(writer, value.vertices);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::BloodSplatState &value)
{
    return readValue(reader, value.sourceActorId)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.radius)
        && readValue(reader, value.vertices);
}

void writeValue(BinaryWriter &writer, const GameplayActorSpecialRuntimeState &value)
{
    writeValue(writer, value.summonedCount);
    writeValue(writer, value.replicatedCount);
    writeValue(writer, value.randomCursor);
    writeValue(writer, value.replicationPending);
    writeValue(writer, value.replicationDueGameMinutes);
    writeValue(writer, value.materializeRemainingSeconds);
}

bool readValue(BinaryReader &reader, GameplayActorSpecialRuntimeState &value)
{
    return readValue(reader, value.summonedCount)
        && readValue(reader, value.replicatedCount)
        && readValue(reader, value.randomCursor)
        && readValue(reader, value.replicationPending)
        && readValue(reader, value.replicationDueGameMinutes)
        && readValue(reader, value.materializeRemainingSeconds);
}

void writeValue(BinaryWriter &writer, const IndoorWorldRuntime::MapActorAiState &value)
{
    writeValue(writer, value.actorId);
    writeValue(writer, value.monsterId);
    writeValue(writer, value.displayName);
    writeValue(writer, value.hostileToParty);
    writeValue(writer, value.hasDetectedParty);
    writeValue(writer, value.bloodSplatSpawned);
    writeValue(writer, value.motionState);
    writeValue(writer, value.animationState);
    writeValue(writer, value.queuedAttackAbility);
    writeValue(writer, value.spellEffects);
    writeValue(writer, value.preciseX);
    writeValue(writer, value.preciseY);
    writeValue(writer, value.preciseZ);
    writeValue(writer, value.homePreciseX);
    writeValue(writer, value.homePreciseY);
    writeValue(writer, value.homePreciseZ);
    writeValue(writer, value.moveDirectionX);
    writeValue(writer, value.moveDirectionY);
    writeValue(writer, value.velocityX);
    writeValue(writer, value.velocityY);
    writeValue(writer, value.velocityZ);
    writeValue(writer, value.yawRadians);
    writeValue(writer, value.animationTimeTicks);
    writeValue(writer, value.recoverySeconds);
    writeValue(writer, value.attackAnimationSeconds);
    writeValue(writer, value.attackCooldownSeconds);
    writeValue(writer, value.idleDecisionSeconds);
    writeValue(writer, value.actionSeconds);
    writeValue(writer, value.idleDecisionCount);
    writeValue(writer, value.pursueDecisionCount);
    writeValue(writer, value.attackDecisionCount);
    writeValue(writer, value.attackImpactTriggered);
    writeValue(writer, value.spriteFrameIndex);
    writeValue(writer, value.actionSpriteFrameIndices);
    writeValue(writer, value.collisionRadius);
    writeValue(writer, value.collisionHeight);
    writeValue(writer, value.movementSpeed);
    writeValue(writer, value.bolsterRewardMultiplier);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.eyeSectorId);
    writeValue(writer, value.supportFaceIndex);
    writeValue(writer, value.grounded);
    writeValue(writer, value.meleeAttackAnimationSeconds);
    writeValue(writer, value.rangedAttackAnimationSeconds);
    writeValue(writer, value.dyingAnimationSeconds);
    writeValue(writer, value.crowdSideLockRemainingSeconds);
    writeValue(writer, value.crowdNoProgressSeconds);
    writeValue(writer, value.crowdLastEdgeDistance);
    writeValue(writer, value.crowdRetreatRemainingSeconds);
    writeValue(writer, value.crowdStandRemainingSeconds);
    writeValue(writer, value.crowdProbeEdgeDistance);
    writeValue(writer, value.crowdProbeElapsedSeconds);
    writeValue(writer, value.crowdEscapeAttempts);
    writeValue(writer, value.crowdSideSign);
    writeValue(writer, value.suppressLowHealthFlee);
    writeValue(writer, value.combatState);
    writeValue(writer, value.specialState);
    writeValue(writer, value.stolenItems);
}

bool readValue(BinaryReader &reader, IndoorWorldRuntime::MapActorAiState &value)
{
    return readValue(reader, value.actorId)
        && readValue(reader, value.monsterId)
        && readValue(reader, value.displayName)
        && readValue(reader, value.hostileToParty)
        && readValue(reader, value.hasDetectedParty)
        && readValue(reader, value.bloodSplatSpawned)
        && readValue(reader, value.motionState)
        && readValue(reader, value.animationState)
        && readValue(reader, value.queuedAttackAbility)
        && readValue(reader, value.spellEffects)
        && readValue(reader, value.preciseX)
        && readValue(reader, value.preciseY)
        && readValue(reader, value.preciseZ)
        && readValue(reader, value.homePreciseX)
        && readValue(reader, value.homePreciseY)
        && readValue(reader, value.homePreciseZ)
        && readValue(reader, value.moveDirectionX)
        && readValue(reader, value.moveDirectionY)
        && readValue(reader, value.velocityX)
        && readValue(reader, value.velocityY)
        && readValue(reader, value.velocityZ)
        && readValue(reader, value.yawRadians)
        && readValue(reader, value.animationTimeTicks)
        && readValue(reader, value.recoverySeconds)
        && readValue(reader, value.attackAnimationSeconds)
        && readValue(reader, value.attackCooldownSeconds)
        && readValue(reader, value.idleDecisionSeconds)
        && readValue(reader, value.actionSeconds)
        && readValue(reader, value.idleDecisionCount)
        && readValue(reader, value.pursueDecisionCount)
        && readValue(reader, value.attackDecisionCount)
        && readValue(reader, value.attackImpactTriggered)
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.spriteFrameIndex))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.actionSpriteFrameIndices))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.collisionRadius))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.collisionHeight))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.movementSpeed))
        && (reader.version() < SaveVersionMonsterBolsterRewards || readValue(reader, value.bolsterRewardMultiplier))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.sectorId))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.eyeSectorId))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.supportFaceIndex))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.grounded))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.meleeAttackAnimationSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.rangedAttackAnimationSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.dyingAnimationSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity
            || readValue(reader, value.crowdSideLockRemainingSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdNoProgressSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdLastEdgeDistance))
        && (reader.version() < SaveVersionIndoorSaveLoadParity
            || readValue(reader, value.crowdRetreatRemainingSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdStandRemainingSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdProbeEdgeDistance))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdProbeElapsedSeconds))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdEscapeAttempts))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.crowdSideSign))
        && (reader.version() < SaveVersionOutdoorRuntimeSaveParity || readValue(reader, value.suppressLowHealthFlee))
        && (reader.version() < SaveVersionResolvedActorCombat || readValue(reader, value.combatState))
        && (reader.version() < SaveVersionActorSpecialRuntime || readValue(reader, value.specialState))
        && (reader.version() < SaveVersionActorSpecialRuntime || readValue(reader, value.stolenItems));
}

void writeValue(BinaryWriter &writer, const IndoorWorldRuntime::Snapshot &value)
{
    writeValue(writer, value.gameMinutes);
    writeValue(writer, value.currentLocationReputation);
    writeValue(writer, value.sessionChestSeed);
    writeValue(writer, value.materializedChestViews);
    writeValue(writer, value.activeChestView);
    writeValue(writer, value.mapActorCorpseViews);
    writeValue(writer, value.activeCorpseView);
    writeValue(writer, value.mapActorAiStates);
    writeValue(writer, value.bloodSplats);
    writeValue(writer, value.fireSpikeTraps);
    writeValue(writer, value.actorUpdateAccumulatorSeconds);
    writeValue(writer, value.projectileState);
    writeValue(writer, value.searchedLootPropSourceIds);
    writeValue(writer, value.mergeMonsterGeneration);
}

bool readValue(BinaryReader &reader, IndoorWorldRuntime::Snapshot &value)
{
    return readGameMinutes(reader, value.gameMinutes)
        && readValue(reader, value.currentLocationReputation)
        && (reader.version() < SaveVersionIndoorChestViews || readValue(reader, value.sessionChestSeed))
        && (reader.version() < SaveVersionIndoorChestViews || readValue(reader, value.materializedChestViews))
        && (reader.version() < SaveVersionIndoorChestViews || readValue(reader, value.activeChestView))
        && (reader.version() < SaveVersionIndoorCorpseViews || readValue(reader, value.mapActorCorpseViews))
        && (reader.version() < SaveVersionIndoorCorpseViews || readValue(reader, value.activeCorpseView))
        && (reader.version() < SaveVersionIndoorActorSpellEffects
            || reader.version() >= SaveVersionIndoorActorAiStates
            || readValue(reader, value.mapActorSpellEffectStates))
        && (reader.version() < SaveVersionIndoorActorAiStates || readValue(reader, value.mapActorAiStates))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.bloodSplats))
        && (reader.version() < SaveVersionIndoorFireSpikeTraps || readValue(reader, value.fireSpikeTraps))
        && (reader.version() < SaveVersionIndoorSaveLoadParity
            || readValue(reader, value.actorUpdateAccumulatorSeconds))
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.projectileState))
        && (reader.version() < SaveVersionMm9ItemSourceState
            || readValue(reader, value.searchedLootPropSourceIds))
        && (reader.version() < SaveVersionActorSpecialRuntime || readValue(reader, value.mergeMonsterGeneration));
}

void writeValue(BinaryWriter &writer, const OutdoorPartyRuntime::Snapshot &value)
{
    writeValue(writer, value.movementState);
    writeValue(writer, value.partyMovementState);
    writeValue(writer, value.movementAccumulatorSeconds);
    writeValue(writer, value.jumpState);
}

bool readValue(BinaryReader &reader, OutdoorPartyRuntime::Snapshot &value)
{
    value.movementAccumulatorSeconds = 0.0f;
    value.jumpState = {};
    return readValue(reader, value.movementState)
        && readValue(reader, value.partyMovementState)
        && (reader.version() < SaveVersionPartyJumpState
            || (readValue(reader, value.movementAccumulatorSeconds) && readValue(reader, value.jumpState)))
        && validSavedPartyMotion(value);
}

void writeValue(BinaryWriter &writer, const RuntimeMechanismState &value)
{
    writeValue(writer, value.state);
    writeValue(writer, value.timeSinceTriggeredMs);
    writeValue(writer, value.currentDistance);
    writeValue(writer, value.isMoving);
    writeValue(writer, value.rotationDirection);
}

bool readValue(BinaryReader &reader, RuntimeMechanismState &value)
{
    return readValue(reader, value.state)
        && readValue(reader, value.timeSinceTriggeredMs)
        && readValue(reader, value.currentDistance)
        && readValue(reader, value.isMoving)
        && (reader.version() < SaveVersionOutdoorMechanismRotationDirection
            || readValue(reader, value.rotationDirection));
}

void writeValue(BinaryWriter &writer, const RuntimeDestructibleState &value)
{
    writeValue(writer, value.hp);
    writeValue(writer, value.damageEnabled);
    writeValue(writer, value.destroyed);
}

bool readValue(BinaryReader &reader, RuntimeDestructibleState &value)
{
    return readValue(reader, value.hp)
        && readValue(reader, value.damageEnabled)
        && readValue(reader, value.destroyed);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::OutdoorModelMechanismDefinition &value)
{
    writeValue(writer, value.mechanismId);
    writeValue(writer, value.modelName);
    writeValue(writer, value.bmodelIndex);
    writeValue(writer, value.dx);
    writeValue(writer, value.dy);
    writeValue(writer, value.dz);
    writeValue(writer, value.moveTimeMs);
    writeValue(writer, value.closed);
    writeValue(writer, value.moveParty);
}

bool readValue(BinaryReader &reader, EventRuntimeState::OutdoorModelMechanismDefinition &value)
{
    return readValue(reader, value.mechanismId)
        && readValue(reader, value.modelName)
        && readValue(reader, value.bmodelIndex)
        && readValue(reader, value.dx)
        && readValue(reader, value.dy)
        && readValue(reader, value.dz)
        && readValue(reader, value.moveTimeMs)
        && readValue(reader, value.closed)
        && readValue(reader, value.moveParty);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PendingMapMove &value)
{
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.mapName);
    writeValue(writer, value.directionDegrees);
    writeValue(writer, value.useMapStartPosition);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PendingMapMove &value)
{
    return readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.mapName)
        && readValue(reader, value.directionDegrees)
        && readValue(reader, value.useMapStartPosition);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PendingDialogueContext &value)
{
    writeValue(writer, value.kind);
    writeValue(writer, value.sourceId);
    writeValue(writer, value.sourceActorIndex);
    writeValue(writer, value.hostHouseId);
    writeValue(writer, value.newsId);
    writeValue(writer, value.participantPictureId);
    writeValue(writer, value.titleOverride);
    writeValue(writer, value.transitionMapMove);
    writeValue(writer, value.transitionTextId);
    writeValue(writer, value.transitionImageId);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PendingDialogueContext &value)
{
    return readValue(reader, value.kind)
        && readValue(reader, value.sourceId)
        && (reader.version() < SaveVersionDialogueActorSource || readValue(reader, value.sourceActorIndex))
        && readValue(reader, value.hostHouseId)
        && readValue(reader, value.newsId)
        && (reader.version() < SaveVersionDialogueParticipantPicture
            || readValue(reader, value.participantPictureId))
        && readValue(reader, value.titleOverride)
        && (reader.version() < SaveVersionDungeonTransitionDialogue || readValue(reader, value.transitionMapMove))
        && (reader.version() < SaveVersionDungeonTransitionDialogue || readValue(reader, value.transitionTextId))
        && (reader.version() < SaveVersionDungeonTransitionDialogue || readValue(reader, value.transitionImageId));
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PendingMovie &value)
{
    writeValue(writer, value.movieName);
    writeValue(writer, value.restoreAfterPlayback);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PendingMovie &value)
{
    return readValue(reader, value.movieName)
        && readValue(reader, value.restoreAfterPlayback);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PendingInputPrompt &value)
{
    writeValue(writer, value.kind);
    writeValue(writer, value.eventId);
    writeValue(writer, value.continueStep);
    writeValue(writer, value.correctStep);
    writeValue(writer, value.textId);
    writeValue(writer, value.answerTextIds);
    writeValue(writer, value.answers);
    writeValue(writer, value.answerContinueSteps);
    writeValue(writer, value.text);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PendingInputPrompt &value)
{
    return readValue(reader, value.kind)
        && readValue(reader, value.eventId)
        && readValue(reader, value.continueStep)
        && (reader.version() < SaveVersionInputPromptAnswers || readValue(reader, value.correctStep))
        && readValue(reader, value.textId)
        && (reader.version() < SaveVersionInputPromptAnswers || readValue(reader, value.answerTextIds))
        && (reader.version() < SaveVersionInputPromptAnswers || readValue(reader, value.answers))
        && (reader.version() < SaveVersionInputPromptAnswerSteps || readValue(reader, value.answerContinueSteps))
        && readValue(reader, value.text);
}

void writePendingInputPromptForSave(BinaryWriter &writer, const EventRuntimeState &value)
{
    (void)value;
    const std::optional<EventRuntimeState::PendingInputPrompt> emptyPrompt = std::nullopt;
    writeValue(writer, emptyPrompt);
}

bool readPendingInputPromptForSave(BinaryReader &reader, EventRuntimeState &value)
{
    std::optional<EventRuntimeState::PendingInputPrompt> discardedPrompt;

    if (!readValue(reader, discardedPrompt))
    {
        return false;
    }

    value.pendingInputPrompt.reset();
    return true;
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PendingSound &value)
{
    writeValue(writer, value.soundId);
    writeValue(writer, value.soundName);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.positional);
    writeValue(writer, value.hasExplicitZ);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PendingSound &value)
{
    return readValue(reader, value.soundId)
        && (reader.version() < SaveVersionPendingSoundNames || readValue(reader, value.soundName))
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.z))
        && readValue(reader, value.positional)
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readValue(reader, value.hasExplicitZ));
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::SpriteOverride &value)
{
    writeValue(writer, value.hidden);
    writeValue(writer, value.textureName);
}

bool readValue(BinaryReader &reader, EventRuntimeState::SpriteOverride &value)
{
    return readValue(reader, value.hidden)
        && readValue(reader, value.textureName);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PendingArcomageGame &value)
{
    writeValue(writer, value.houseId);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PendingArcomageGame &value)
{
    return readValue(reader, value.houseId);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::DialogueOfferState &value)
{
    writeValue(writer, value.kind);
    writeValue(writer, value.npcId);
    writeValue(writer, value.topicId);
    writeValue(writer, value.messageTextId);
    writeValue(writer, value.rosterId);
    writeValue(writer, value.partyFullTextId);
    writeValue(writer, value.sourceActorIndex);
}

bool readValue(BinaryReader &reader, EventRuntimeState::DialogueOfferState &value)
{
    return readValue(reader, value.kind)
        && readValue(reader, value.npcId)
        && readValue(reader, value.topicId)
        && readValue(reader, value.messageTextId)
        && readValue(reader, value.rosterId)
        && readValue(reader, value.partyFullTextId)
        && (reader.version() < SaveVersionDialogueActorSource || readValue(reader, value.sourceActorIndex));
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::SuspendedMm9RudeDialogue &value)
{
    writeValue(writer, value.rudeId);
    writeValue(writer, value.nodeId);
    writeValue(writer, value.sourceActorIndex);
    writeValue(writer, value.response);
    writeValue(writer, value.selectionIndex);
    writeValue(writer, value.exitCallbackExecuted);
}

bool readValue(BinaryReader &reader, EventRuntimeState::SuspendedMm9RudeDialogue &value)
{
    return readValue(reader, value.rudeId)
        && readValue(reader, value.nodeId)
        && readValue(reader, value.sourceActorIndex)
        && readValue(reader, value.response)
        && readValue(reader, value.selectionIndex)
        && readValue(reader, value.exitCallbackExecuted);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::DialogueRuntimeState &value)
{
    writeValue(writer, value.hostHouseId);
    writeValue(writer, value.menuStack);
    writeValue(writer, value.currentOffer);
    writeValue(writer, value.suspendedMm9RudeDialogue);
}

bool readValue(BinaryReader &reader, EventRuntimeState::DialogueRuntimeState &value)
{
    return readValue(reader, value.hostHouseId)
        && readValue(reader, value.menuStack)
        && readValue(reader, value.currentOffer)
        && (reader.version() < SaveVersionSuspendedMm9RudeVendor
            || readValue(reader, value.suspendedMm9RudeDialogue));
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::GeneratedMercenaryRecruit &value)
{
    writeValue(writer, value.npcId);
    writeValue(writer, value.rosterId);
    writeValue(writer, value.houseId);
    writeValue(writer, value.portraitPictureId);
    writeValue(writer, value.npcPictureId);
    writeValue(writer, value.character);
}

bool readValue(BinaryReader &reader, EventRuntimeState::GeneratedMercenaryRecruit &value)
{
    const bool loaded = readValue(reader, value.npcId)
        && readValue(reader, value.rosterId)
        && readValue(reader, value.houseId)
        && readValue(reader, value.portraitPictureId)
        && (reader.version() < SaveVersionGeneratedMercenaryNpcPicture
            || readValue(reader, value.npcPictureId))
        && readValue(reader, value.character);

    if (loaded && reader.version() < SaveVersionGeneratedMercenaryNpcPicture)
    {
        value.npcPictureId = value.portraitPictureId;
    }

    return loaded;
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::ActiveDecorationContext &value)
{
    writeValue(writer, value.decorVarIndex);
    writeValue(writer, value.baseEventId);
    writeValue(writer, value.currentEventId);
    writeValue(writer, value.eventCount);
    writeValue(writer, value.hideWhenCleared);
}

bool readValue(BinaryReader &reader, EventRuntimeState::ActiveDecorationContext &value)
{
    return readValue(reader, value.decorVarIndex)
        && readValue(reader, value.baseEventId)
        && readValue(reader, value.currentEventId)
        && readValue(reader, value.eventCount)
        && readValue(reader, value.hideWhenCleared);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::PortraitFxRequest &value)
{
    writeValue(writer, value.kind);
    writeValue(writer, value.memberIndices);
}

bool readValue(BinaryReader &reader, EventRuntimeState::PortraitFxRequest &value)
{
    return readValue(reader, value.kind)
        && readValue(reader, value.memberIndices);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::SpellFxRequest &value)
{
    writeValue(writer, value.spellId);
    writeValue(writer, value.memberIndices);
}

bool readValue(BinaryReader &reader, EventRuntimeState::SpellFxRequest &value)
{
    return readValue(reader, value.spellId)
        && readValue(reader, value.memberIndices);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::RuntimeMapNote &value)
{
    writeValue(writer, value.id);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.mapFileName);
    writeValue(writer, value.text);
    writeValue(writer, value.active);
}

bool readValue(BinaryReader &reader, EventRuntimeState::RuntimeMapNote &value)
{
    if (!readValue(reader, value.id)
        || !readValue(reader, value.x)
        || !readValue(reader, value.y))
    {
        return false;
    }

    if (reader.version() >= SaveVersionRuntimeMapNoteMapFile)
    {
        if (!readValue(reader, value.mapFileName))
        {
            return false;
        }
    }
    else
    {
        value.mapFileName.clear();
    }

    return readValue(reader, value.text)
        && readValue(reader, value.active);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::ChestItemRequest &value)
{
    writeValue(writer, value.itemId);
    writeValue(writer, value.gridX);
    writeValue(writer, value.gridY);
}

bool readValue(BinaryReader &reader, EventRuntimeState::ChestItemRequest &value)
{
    return readValue(reader, value.itemId)
        && readValue(reader, value.gridX)
        && readValue(reader, value.gridY);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::HiredNpcFollower &value)
{
    writeValue(writer, value.npcId);
    writeValue(writer, value.professionId);
    writeValue(writer, value.weeklyCost);
    writeValue(writer, value.abilityUsedDay);
    writeValue(writer, value.name);
    writeValue(writer, value.pictureId);
}

bool readValue(BinaryReader &reader, EventRuntimeState::HiredNpcFollower &value)
{
    return readValue(reader, value.npcId)
        && readValue(reader, value.professionId)
        && readValue(reader, value.weeklyCost)
        && (reader.version() < SaveVersionHiredNpcFollowerAbilityUseDay || readValue(reader, value.abilityUsedDay))
        && (reader.version() < SaveVersionHiredNpcFollowerIdentity || readValue(reader, value.name))
        && (reader.version() < SaveVersionHiredNpcFollowerIdentity || readValue(reader, value.pictureId));
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::SavedLocation &value)
{
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.continentId);
    writeValue(writer, value.mapName);
}

bool readValue(BinaryReader &reader, EventRuntimeState::SavedLocation &value)
{
    return readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.continentId)
        && readValue(reader, value.mapName);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState::TransportRouteOverride &value)
{
    writeValue(writer, value.houseId);
    writeValue(writer, value.routeIndex);
    writeValue(writer, value.destinationName);
    writeValue(writer, value.mapFileName);
    writeValue(writer, value.daysAvailable);
    writeValue(writer, value.travelDays);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.directionDegrees);
    writeValue(writer, value.requiredQBit);
    writeValue(writer, value.useMapStartPosition);
}

bool readValue(BinaryReader &reader, EventRuntimeState::TransportRouteOverride &value)
{
    return readValue(reader, value.houseId)
        && readValue(reader, value.routeIndex)
        && readValue(reader, value.destinationName)
        && readValue(reader, value.mapFileName)
        && readValue(reader, value.daysAvailable)
        && readValue(reader, value.travelDays)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.directionDegrees)
        && readValue(reader, value.requiredQBit)
        && readValue(reader, value.useMapStartPosition);
}

void writeValue(BinaryWriter &writer, const OutdoorLightTimeOverride &value)
{
    writeValue(writer, value.hour);
    writeValue(writer, value.minute);
}

bool readValue(BinaryReader &reader, OutdoorLightTimeOverride &value)
{
    return readValue(reader, value.hour) && readValue(reader, value.minute)
        && validOutdoorLightTimeOverride(value);
}

void writeValue(BinaryWriter &writer, const EventRuntimeState &value)
{
    writeValue(writer, value.variables);
    writeValue(writer, value.namedMapVars);
    writeValue(writer, value.namedGlobalVars);
    writeValue(writer, value.runtimeMapNotes);
    writeValue(writer, value.savedLocations);
    writeValue(writer, value.transportRouteOverrides);
    writeValue(writer, value.activeHistoryContinentId);
    writeValue(writer, value.historyEventTimes);
    writeValue(writer, value.historyEventTimesByContinent);
    writeValue(writer, value.mapVars);
    writeValue(writer, value.currentLocationReputation);
    writeValue(writer, value.facetSetMasks);
    writeValue(writer, value.facetClearMasks);
    writeValue(writer, value.mechanisms);
    writeValue(writer, value.destructibles);
    writeValue(writer, value.outdoorModelMechanisms);
    writeValue(writer, value.textureOverrides);
    writeValue(writer, value.outdoorModelFacetTextureOverrides);
    writeValue(writer, value.spriteOverrides);
    writeValue(writer, value.indoorLightsEnabled);
    writeValue(writer, value.snowEnabled);
    writeValue(writer, value.rainEnabled);
    writeValue(writer, value.actorSetMasks);
    writeValue(writer, value.actorClearMasks);
    writeValue(writer, value.actorGroupSetMasks);
    writeValue(writer, value.actorGroupClearMasks);
    writeValue(writer, value.actorIdGroupOverrides);
    writeValue(writer, value.actorGroupOverrides);
    writeValue(writer, value.actorGroupAllyOverrides);
    writeValue(writer, value.chestSetMasks);
    writeValue(writer, value.chestClearMasks);
    writeValue(writer, value.npcTopicOverrides);
    writeValue(writer, value.npcGroupNews);
    writeValue(writer, value.npcGreetingOverrides);
    writeValue(writer, value.npcGreetingDisplayCounts);
    writeValue(writer, value.npcHouseOverrides);
    writeValue(writer, value.npcNameOverrides);
    writeValue(writer, value.npcPictureOverrides);
    writeValue(writer, value.npcProfessionOverrides);
    writeValue(writer, value.generatedNpcIdsByActorKey);
    writeValue(writer, value.generatedMercenaryRecruitsByNpcId);
    writeValue(writer, value.npcItemOverrides);
    writeValue(writer, value.actorItemOverrides);
    writeValue(writer, value.actorExtraItemOverrides);
    writeValue(writer, value.monsterRelationOverrides);
    writeValue(writer, value.chestItemRequests);
    writeValue(writer, value.unavailableNpcIds);
    writeValue(writer, value.hiredNpcFollowers);
    writeValue(writer, value.dialogueState);
    writeValue(writer, value.decorVars);
    writeValue(writer, value.activeDecorationContext);
    writeValue(writer, value.messages);
    writeValue(writer, value.statusMessages);
    writeValue(writer, value.openedChestIds);
    writeValue(writer, value.grantedItems);
    writeValue(writer, value.grantedItemIds);
    writeValue(writer, value.removedItemIds);
    writeValue(writer, value.grantedAwardIds);
    writeValue(writer, value.removedAwardIds);
    writeValue(writer, value.portraitFxRequests);
    writeValue(writer, value.spellFxRequests);
    writeValue(writer, value.pendingDialogueContext);
    writeValue(writer, value.pendingMapMove);
    writeValue(writer, value.pendingMovie);
    writePendingInputPromptForSave(writer, value);
    writeValue(writer, value.pendingArcomageGame);
    writeValue(writer, value.pendingSounds);
    writeValue(writer, value.lastAffectedMechanismIds);
    writeValue(writer, value.lastActivationResult);
    writeValue(writer, value.localOnLoadEventsExecuted);
    writeValue(writer, value.globalOnLoadEventsExecuted);
    writeValue(writer, value.processedMapRespawnCount);
    writeValue(writer, value.eventRandomState);
    writeValue(writer, value.outdoorLightTimeOverride);
}

bool readValue(BinaryReader &reader, EventRuntimeState &value)
{
    value.eventRandomState = 0;
    value.outdoorLightTimeOverride.reset();
    const bool ok = readValue(reader, value.variables)
        && (reader.version() < SaveVersionNamedLuaVars || readValue(reader, value.namedMapVars))
        && (reader.version() < SaveVersionNamedLuaVars || readValue(reader, value.namedGlobalVars))
        && (reader.version() < SaveVersionRuntimeMapNotes || readValue(reader, value.runtimeMapNotes))
        && (reader.version() < SaveVersionPersistentLuaRuntimeState || readValue(reader, value.savedLocations))
        && (reader.version() < SaveVersionPersistentLuaRuntimeState || readValue(reader, value.transportRouteOverrides))
        && (reader.version() < SaveVersionScopedHistory || readValue(reader, value.activeHistoryContinentId))
        && readValue(reader, value.historyEventTimes)
        && (reader.version() < SaveVersionScopedHistory || readValue(reader, value.historyEventTimesByContinent))
        && readValue(reader, value.mapVars)
        && (reader.version() < SaveVersionPartyContinentReputations
            || readValue(reader, value.currentLocationReputation))
        && readValue(reader, value.facetSetMasks)
        && readValue(reader, value.facetClearMasks)
        && readValue(reader, value.mechanisms)
        && (reader.version() < SaveVersionOutdoorDestructibles
            || readValue(reader, value.destructibles))
        && (reader.version() < SaveVersionOutdoorModelMechanisms
            || readValue(reader, value.outdoorModelMechanisms))
        && readValue(reader, value.textureOverrides)
        && (reader.version() < SaveVersionMapLuaRuntimeOverlays
            || readValue(reader, value.outdoorModelFacetTextureOverrides))
        && readValue(reader, value.spriteOverrides)
        && readValue(reader, value.indoorLightsEnabled)
        && readValue(reader, value.snowEnabled)
        && readValue(reader, value.rainEnabled)
        && readValue(reader, value.actorSetMasks)
        && readValue(reader, value.actorClearMasks)
        && readValue(reader, value.actorGroupSetMasks)
        && readValue(reader, value.actorGroupClearMasks)
        && readValue(reader, value.actorIdGroupOverrides)
        && readValue(reader, value.actorGroupOverrides)
        && readValue(reader, value.actorGroupAllyOverrides)
        && readValue(reader, value.chestSetMasks)
        && readValue(reader, value.chestClearMasks)
        && readValue(reader, value.npcTopicOverrides)
        && readValue(reader, value.npcGroupNews)
        && readValue(reader, value.npcGreetingOverrides)
        && readValue(reader, value.npcGreetingDisplayCounts)
        && readValue(reader, value.npcHouseOverrides)
        && (reader.version() < SaveVersionGeneratedNpcOverrides || readValue(reader, value.npcNameOverrides))
        && (reader.version() < SaveVersionGeneratedNpcOverrides || readValue(reader, value.npcPictureOverrides))
        && (reader.version() < SaveVersionGeneratedNpcOverrides || readValue(reader, value.npcProfessionOverrides))
        && (reader.version() < SaveVersionGeneratedNpcOverrides || readValue(reader, value.generatedNpcIdsByActorKey))
        && (reader.version() < SaveVersionGeneratedMercenaryRecruits
            || readValue(reader, value.generatedMercenaryRecruitsByNpcId))
        && readValue(reader, value.npcItemOverrides)
        && readValue(reader, value.actorItemOverrides)
        && (reader.version() < SaveVersionActorExtraItemOverrides
            || readValue(reader, value.actorExtraItemOverrides))
        && (reader.version() < SaveVersionMapLuaRuntimeOverlays || readValue(reader, value.monsterRelationOverrides))
        && (reader.version() < SaveVersionMapLuaRuntimeOverlays || readValue(reader, value.chestItemRequests))
        && readValue(reader, value.unavailableNpcIds)
        && (reader.version() < SaveVersionHiredNpcFollowers || readValue(reader, value.hiredNpcFollowers))
        && readValue(reader, value.dialogueState)
        && readValue(reader, value.decorVars)
        && readValue(reader, value.activeDecorationContext)
        && readValue(reader, value.messages)
        && readValue(reader, value.statusMessages)
        && readValue(reader, value.openedChestIds)
        && readValue(reader, value.grantedItems)
        && readValue(reader, value.grantedItemIds)
        && readValue(reader, value.removedItemIds)
        && readValue(reader, value.grantedAwardIds)
        && readValue(reader, value.removedAwardIds)
        && readValue(reader, value.portraitFxRequests)
        && readValue(reader, value.spellFxRequests)
        && readValue(reader, value.pendingDialogueContext)
        && readValue(reader, value.pendingMapMove)
        && readValue(reader, value.pendingMovie)
        && readPendingInputPromptForSave(reader, value)
        && readValue(reader, value.pendingArcomageGame)
        && readValue(reader, value.pendingSounds)
        && readValue(reader, value.lastAffectedMechanismIds)
        && readValue(reader, value.lastActivationResult)
        && readValue(reader, value.localOnLoadEventsExecuted)
        && readValue(reader, value.globalOnLoadEventsExecuted)
        && (reader.version() < SaveVersionActorExtraItemOverrides
            || readValue(reader, value.processedMapRespawnCount))
        && (reader.version() < SaveVersionEventRandomState || readValue(reader, value.eventRandomState))
        && (reader.version() < SaveVersionOutdoorLightTime || readValue(reader, value.outdoorLightTimeOverride));

    if (!ok)
    {
        return false;
    }

    std::string timeError;
    if (!validateNamedGameTimes(value.namedGlobalVars, timeError))
    {
        return false;
    }
    value.activeHistoryContinentId = normalizedHistoryContinentId(value.activeHistoryContinentId);

    if (value.historyEventTimesByContinent.empty() && !value.historyEventTimes.empty())
    {
        value.historyEventTimesByContinent[1u] = value.historyEventTimes;
    }

    return true;
}

void writeValue(BinaryWriter &writer, const MapDeltaChest &value)
{
    writeValue(writer, value.chestTypeId);
    writeValue(writer, value.flags);
    writeValue(writer, value.rawItems);
    writeValue(writer, value.inventoryMatrix);
}

bool readValue(BinaryReader &reader, MapDeltaChest &value)
{
    return readValue(reader, value.chestTypeId)
        && readValue(reader, value.flags)
        && readValue(reader, value.rawItems)
        && readValue(reader, value.inventoryMatrix);
}

void writeValue(BinaryWriter &writer, const MapDeltaLocationInfo &value)
{
    writeValue(writer, value.respawnCount);
    writeValue(writer, value.lastRespawnDay);
    writeValue(writer, value.reputation);
    writeValue(writer, value.alertStatus);
    writeValue(writer, value.totalFacesCount);
    writeValue(writer, value.decorationCount);
    writeValue(writer, value.bmodelCount);
}

bool readValue(BinaryReader &reader, MapDeltaLocationInfo &value)
{
    return readValue(reader, value.respawnCount)
        && readValue(reader, value.lastRespawnDay)
        && readValue(reader, value.reputation)
        && readValue(reader, value.alertStatus)
        && readValue(reader, value.totalFacesCount)
        && readValue(reader, value.decorationCount)
        && readValue(reader, value.bmodelCount);
}

void writeValue(BinaryWriter &writer, const MapDeltaPersistentVariables &value)
{
    writeValue(writer, value.mapVars);
    writeValue(writer, value.decorVars);
}

bool readValue(BinaryReader &reader, MapDeltaPersistentVariables &value)
{
    return readValue(reader, value.mapVars)
        && readValue(reader, value.decorVars);
}

void writeValue(BinaryWriter &writer, const MapDeltaLocationTime &value)
{
    writeValue(writer, value.lastVisitTime);
    writeValue(writer, value.skyTextureName);
    writeValue(writer, value.weatherFlags);
    writeValue(writer, value.fogWeakDistance);
    writeValue(writer, value.fogStrongDistance);
    writeValue(writer, value.reserved);
}

bool readValue(BinaryReader &reader, MapDeltaLocationTime &value)
{
    return readValue(reader, value.lastVisitTime)
        && readValue(reader, value.skyTextureName)
        && readValue(reader, value.weatherFlags)
        && readValue(reader, value.fogWeakDistance)
        && readValue(reader, value.fogStrongDistance)
        && readValue(reader, value.reserved);
}

void writeValue(BinaryWriter &writer, const MapDeltaActor &value)
{
    writeValue(writer, value.name);
    writeValue(writer, value.npcId);
    writeValue(writer, value.attributes);
    writeValue(writer, value.hp);
    writeValue(writer, value.hostilityType);
    writeValue(writer, value.monsterInfoId);
    writeValue(writer, value.monsterId);
    writeValue(writer, value.radius);
    writeValue(writer, value.height);
    writeValue(writer, value.moveSpeed);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.spriteIds);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.currentActionAnimation);
    writeValue(writer, value.carriedItemId);
    writeValue(writer, value.group);
    writeValue(writer, value.ally);
    writeValue(writer, value.uniqueNameIndex);
    writeValue(writer, value.bolsterRewardMultiplier);
    const uint64_t diagnosticSourceActorIndex =
        value.diagnosticSourceActorIndex != static_cast<size_t>(-1)
            ? static_cast<uint64_t>(value.diagnosticSourceActorIndex)
            : std::numeric_limits<uint64_t>::max();
    writeValue(writer, diagnosticSourceActorIndex);
    writeValue(writer, value.mm9RudeId);
    writeValue(writer, value.mm9SourceObjectIndex);
    writeValue(writer, value.initialYawUnits);
    writeValue(writer, value.immobile);
    writeValue(writer, value.mm9CanReceiveDamage);
    writeValue(writer, value.mm9Civilian);
    writeValue(writer, value.mm9Guard);
    writeValue(writer, value.proceduralDeathLoot);
}

bool readValue(BinaryReader &reader, MapDeltaActor &value)
{
    if (!(readValue(reader, value.name)
        && readValue(reader, value.npcId)
        && readValue(reader, value.attributes)
        && readMapActorHitPoints(reader, value.hp)
        && readValue(reader, value.hostilityType)
        && readValue(reader, value.monsterInfoId)
        && readValue(reader, value.monsterId)
        && readValue(reader, value.radius)
        && readValue(reader, value.height)
        && readValue(reader, value.moveSpeed)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.spriteIds)
        && readValue(reader, value.sectorId)
        && readValue(reader, value.currentActionAnimation)
        && (reader.version() < SaveVersionActorCarriedItem || readValue(reader, value.carriedItemId))
        && readValue(reader, value.group)
        && readValue(reader, value.ally)
        && readValue(reader, value.uniqueNameIndex)
        && (reader.version() < SaveVersionMonsterBolsterRewards || readValue(reader, value.bolsterRewardMultiplier))))
    {
        return false;
    }

    if (reader.version() >= SaveVersionActorDiagnosticSource)
    {
        uint64_t diagnosticSourceActorIndex = std::numeric_limits<uint64_t>::max();

        if (!readValue(reader, diagnosticSourceActorIndex))
        {
            return false;
        }

        value.diagnosticSourceActorIndex = static_cast<size_t>(-1);

        if (diagnosticSourceActorIndex != std::numeric_limits<uint64_t>::max()
            && diagnosticSourceActorIndex <= static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        {
            value.diagnosticSourceActorIndex = static_cast<size_t>(diagnosticSourceActorIndex);
        }
    }

    if (reader.version() >= SaveVersionMm9DialogueActors
        && (!readValue(reader, value.mm9RudeId)
            || !readValue(reader, value.mm9SourceObjectIndex)
            || !readValue(reader, value.initialYawUnits)
            || !readValue(reader, value.immobile)))
    {
        return false;
    }

    if (reader.version() >= SaveVersionMm9ActorReactions
        && (!readValue(reader, value.mm9CanReceiveDamage)
            || !readValue(reader, value.mm9Civilian)
            || !readValue(reader, value.mm9Guard)))
    {
        return false;
    }

    if (reader.version() >= SaveVersionActorProceduralDeathLoot
        && !readValue(reader, value.proceduralDeathLoot))
    {
        return false;
    }

    return true;
}

void writeValue(BinaryWriter &writer, const MapDeltaSpriteObject &value)
{
    writeValue(writer, value.spriteId);
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.velocityX);
    writeValue(writer, value.velocityY);
    writeValue(writer, value.velocityZ);
    writeValue(writer, value.yawAngle);
    writeValue(writer, value.soundId);
    writeValue(writer, value.attributes);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.timeSinceCreated);
    writeValue(writer, value.temporaryLifetime);
    writeValue(writer, value.glowRadiusMultiplier);
    writeValue(writer, value.spellId);
    writeValue(writer, value.spellLevel);
    writeValue(writer, value.spellSkill);
    writeValue(writer, value.field54);
    writeValue(writer, value.spellCasterPid);
    writeValue(writer, value.spellTargetPid);
    writeValue(writer, value.lodDistance);
    writeValue(writer, value.spellCasterAbility);
    writeValue(writer, value.initialX);
    writeValue(writer, value.initialY);
    writeValue(writer, value.initialZ);
    writeValue(writer, value.rawContainingItem);
    writeValue(writer, value.semanticSourceId);
    writeValue(writer, value.semanticLootContainerId);
    writeValue(writer, value.semanticPlacedPickup);
    writeValue(writer, value.semanticLootContainer);
    writeValue(writer, value.semanticPolicyHidden);
    writeValue(writer, value.containingItem);
    writeValue(writer, value.containingGoldAmount);
}

bool readValue(BinaryReader &reader, MapDeltaSpriteObject &value)
{
    return readValue(reader, value.spriteId)
        && readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.velocityX)
        && readValue(reader, value.velocityY)
        && readValue(reader, value.velocityZ)
        && readValue(reader, value.yawAngle)
        && readValue(reader, value.soundId)
        && readValue(reader, value.attributes)
        && readValue(reader, value.sectorId)
        && readValue(reader, value.timeSinceCreated)
        && readValue(reader, value.temporaryLifetime)
        && readValue(reader, value.glowRadiusMultiplier)
        && readValue(reader, value.spellId)
        && readValue(reader, value.spellLevel)
        && readValue(reader, value.spellSkill)
        && readValue(reader, value.field54)
        && readValue(reader, value.spellCasterPid)
        && readValue(reader, value.spellTargetPid)
        && readValue(reader, value.lodDistance)
        && readValue(reader, value.spellCasterAbility)
        && readValue(reader, value.initialX)
        && readValue(reader, value.initialY)
        && readValue(reader, value.initialZ)
        && readValue(reader, value.rawContainingItem)
        && (reader.version() < SaveVersionMm9IndoorSemanticWorldItems
            || (readValue(reader, value.semanticSourceId)
                && readValue(reader, value.semanticLootContainerId)
                && readValue(reader, value.semanticPlacedPickup)
                && readValue(reader, value.semanticLootContainer)
                && readValue(reader, value.semanticPolicyHidden)))
        && (reader.version() < SaveVersionResolvedActorCombat
            || (readValue(reader, value.containingItem) && readValue(reader, value.containingGoldAmount)))
        && validContainingItem(value);
}

void writeValue(BinaryWriter &writer, const MapDeltaDoor &value)
{
    writeValue(writer, value.slotIndex);
    writeValue(writer, value.attributes);
    writeValue(writer, value.doorId);
    writeValue(writer, value.timeSinceTriggered);
    writeValue(writer, value.directionX);
    writeValue(writer, value.directionY);
    writeValue(writer, value.directionZ);
    writeValue(writer, value.moveLength);
    writeValue(writer, value.openSpeed);
    writeValue(writer, value.closeSpeed);
    writeValue(writer, value.numVertices);
    writeValue(writer, value.numFaces);
    writeValue(writer, value.numSectors);
    writeValue(writer, value.numOffsets);
    writeValue(writer, value.state);
    writeValue(writer, value.vertexIds);
    writeValue(writer, value.faceIds);
    writeValue(writer, value.sectorIds);
    writeValue(writer, value.deltaUs);
    writeValue(writer, value.deltaVs);
    writeValue(writer, value.xOffsets);
    writeValue(writer, value.yOffsets);
    writeValue(writer, value.zOffsets);
}

bool readValue(BinaryReader &reader, MapDeltaDoor &value)
{
    return readValue(reader, value.slotIndex)
        && readValue(reader, value.attributes)
        && readValue(reader, value.doorId)
        && readValue(reader, value.timeSinceTriggered)
        && readValue(reader, value.directionX)
        && readValue(reader, value.directionY)
        && readValue(reader, value.directionZ)
        && readValue(reader, value.moveLength)
        && readValue(reader, value.openSpeed)
        && readValue(reader, value.closeSpeed)
        && readValue(reader, value.numVertices)
        && readValue(reader, value.numFaces)
        && readValue(reader, value.numSectors)
        && readValue(reader, value.numOffsets)
        && readValue(reader, value.state)
        && readValue(reader, value.vertexIds)
        && readValue(reader, value.faceIds)
        && readValue(reader, value.sectorIds)
        && readValue(reader, value.deltaUs)
        && readValue(reader, value.deltaVs)
        && readValue(reader, value.xOffsets)
        && readValue(reader, value.yOffsets)
        && readValue(reader, value.zOffsets);
}

void writeValue(BinaryWriter &writer, const MapDeltaMm9BarrelState &value)
{
    writeValue(writer, value.sourceObjectIndex);
    writeValue(writer, value.type);
    writeValue(writer, value.used);
}

bool readValue(BinaryReader &reader, MapDeltaMm9BarrelState &value)
{
    uint8_t rawType = 0;
    if (!readValue(reader, value.sourceObjectIndex)
        || !readValue(reader, rawType)
        || !readValue(reader, value.used)
        || rawType < static_cast<uint8_t>(Mm9BarrelType::RedMight)
        || rawType > static_cast<uint8_t>(Mm9BarrelType::Water))
    {
        return false;
    }
    value.type = static_cast<Mm9BarrelType>(rawType);
    return true;
}

void writeValue(BinaryWriter &writer, const MapDeltaData &value)
{
    writeValue(writer, value.locationInfo);
    writeValue(writer, value.fullyRevealedCells);
    writeValue(writer, value.partiallyRevealedCells);
    writeValue(writer, value.visibleOutlines);
    writeValue(writer, value.faceAttributes);
    writeValue(writer, value.decorationFlags);
    writeValue(writer, value.actors);
    writeValue(writer, value.spriteObjects);
    writeValue(writer, value.chests);
    writeValue(writer, value.doorSlotCount);
    writeValue(writer, value.doors);
    writeValue(writer, value.doorsData);
    writeValue(writer, value.eventVariables);
    writeValue(writer, value.locationTime);
    writeValue(writer, value.mm9Barrels);
}

bool readValue(BinaryReader &reader, MapDeltaData &value)
{
    return readValue(reader, value.locationInfo)
        && readValue(reader, value.fullyRevealedCells)
        && readValue(reader, value.partiallyRevealedCells)
        && readValue(reader, value.visibleOutlines)
        && readValue(reader, value.faceAttributes)
        && readValue(reader, value.decorationFlags)
        && readValue(reader, value.actors)
        && readValue(reader, value.spriteObjects)
        && readValue(reader, value.chests)
        && readValue(reader, value.doorSlotCount)
        && readValue(reader, value.doors)
        && readValue(reader, value.doorsData)
        && readValue(reader, value.eventVariables)
        && readValue(reader, value.locationTime)
        && (reader.version() < SaveVersionMm9Barrels || readValue(reader, value.mm9Barrels));
}

void writeValue(BinaryWriter &writer, const ScriptedEventTimerDefinition &value)
{
    writeValue(writer, value.scope);
    writeValue(writer, value.origin);
    writeValue(writer, value.triggerKind);
    writeValue(writer, value.scheduleKind);
    writeValue(writer, value.eventId);
    writeValue(writer, value.sourceEventId);
    writeValue(writer, value.triggerStep);
    writeValue(writer, value.registrationIndex);
    writeValue(writer, value.repeating);
    writeValue(writer, value.intervalHalfMinutes);
    writeValue(writer, value.startHour);
    writeValue(writer, value.startMinute);
    writeValue(writer, value.startSecond);
    writeValue(writer, value.intervalGameMinutes);
    writeValue(writer, value.initialDelayGameMinutes);
}

bool readValue(BinaryReader &reader, ScriptedEventTimerDefinition &value)
{
    return readValue(reader, value.scope)
        && readValue(reader, value.origin)
        && readValue(reader, value.triggerKind)
        && readValue(reader, value.scheduleKind)
        && readValue(reader, value.eventId)
        && readValue(reader, value.sourceEventId)
        && readValue(reader, value.triggerStep)
        && readValue(reader, value.registrationIndex)
        && readValue(reader, value.repeating)
        && readValue(reader, value.intervalHalfMinutes)
        && readValue(reader, value.startHour)
        && readValue(reader, value.startMinute)
        && readValue(reader, value.startSecond)
        && readValue(reader, value.intervalGameMinutes)
        && readValue(reader, value.initialDelayGameMinutes);
}

void writeValue(BinaryWriter &writer, const ScriptedEventTimerState &value)
{
    writeValue(writer, value.definition);
    writeValue(writer, value.nextAlarmGameMinutes);
    writeValue(writer, value.eligibleGameMinutes);
    writeValue(writer, value.hasFired);
    writeValue(writer, value.initialCalendarFirePending);
    writeValue(writer, value.migratedFromLegacySave);
    writeValue(writer, value.migratedRemainingGameMinutes);
}

bool readValue(BinaryReader &reader, ScriptedEventTimerState &value)
{
    return readValue(reader, value.definition)
        && readValue(reader, value.nextAlarmGameMinutes)
        && readValue(reader, value.eligibleGameMinutes)
        && readValue(reader, value.hasFired)
        && readValue(reader, value.initialCalendarFirePending)
        && readValue(reader, value.migratedFromLegacySave)
        && readValue(reader, value.migratedRemainingGameMinutes);
}

struct LegacyOutdoorTimerState
{
    uint16_t eventId = 0;
    bool repeating = false;
    float intervalGameMinutes = 0.0f;
    float remainingGameMinutes = 0.0f;
    std::optional<int> targetHour;
    bool hasFired = false;
};

bool readValue(BinaryReader &reader, LegacyOutdoorTimerState &value)
{
    return readValue(reader, value.eventId)
        && readValue(reader, value.repeating)
        && readValue(reader, value.intervalGameMinutes)
        && readValue(reader, value.remainingGameMinutes)
        && readValue(reader, value.targetHour)
        && readValue(reader, value.hasFired);
}

struct LegacyIndoorTimerState
{
    uint16_t eventId = 0;
    bool repeating = false;
    int targetHour = 0;
    float intervalGameMinutes = 0.0f;
    float remainingGameMinutes = 0.0f;
    bool hasFired = false;
};

bool readValue(BinaryReader &reader, LegacyIndoorTimerState &value)
{
    return readValue(reader, value.eventId)
        && readValue(reader, value.repeating)
        && readValue(reader, value.targetHour)
        && readValue(reader, value.intervalGameMinutes)
        && readValue(reader, value.remainingGameMinutes)
        && readValue(reader, value.hasFired);
}

ScriptedEventTimerState migrateLegacyTimerState(
    uint16_t eventId,
    bool repeating,
    float intervalGameMinutes,
    float remainingGameMinutes,
    bool hasFired,
    uint32_t registrationIndex)
{
    ScriptedEventTimerState state = {};
    state.definition.scope = ScriptedEventScope::Map;
    state.definition.origin = ScriptedEventTimerOrigin::Native;
    state.definition.scheduleKind = ScriptedEventTimerScheduleKind::Relative;
    state.definition.eventId = eventId;
    state.definition.sourceEventId = eventId;
    state.definition.registrationIndex = registrationIndex;
    state.definition.repeating = repeating;
    state.definition.intervalGameMinutes = intervalGameMinutes;
    state.hasFired = hasFired;
    state.migratedFromLegacySave = true;
    state.migratedRemainingGameMinutes = remainingGameMinutes;
    return state;
}

bool readOutdoorTimerStates(BinaryReader &reader, std::vector<ScriptedEventTimerState> &values)
{
    if (reader.version() >= SaveVersionLegacyEvtTimers)
    {
        return readValue(reader, values);
    }

    std::vector<LegacyOutdoorTimerState> legacyValues;
    if (!readValue(reader, legacyValues))
    {
        return false;
    }

    values.clear();
    values.reserve(legacyValues.size());

    for (size_t index = 0; index < legacyValues.size(); ++index)
    {
        const LegacyOutdoorTimerState &legacyValue = legacyValues[index];
        values.push_back(migrateLegacyTimerState(
            legacyValue.eventId,
            legacyValue.repeating,
            legacyValue.intervalGameMinutes,
            legacyValue.remainingGameMinutes,
            legacyValue.hasFired,
            static_cast<uint32_t>(index + 1)));
    }

    return true;
}

bool readIndoorTimerStates(BinaryReader &reader, std::vector<ScriptedEventTimerState> &values)
{
    if (reader.version() >= SaveVersionLegacyEvtTimers)
    {
        return readValue(reader, values);
    }

    std::vector<LegacyIndoorTimerState> legacyValues;
    if (!readValue(reader, legacyValues))
    {
        return false;
    }

    values.clear();
    values.reserve(legacyValues.size());

    for (size_t index = 0; index < legacyValues.size(); ++index)
    {
        const LegacyIndoorTimerState &legacyValue = legacyValues[index];
        values.push_back(migrateLegacyTimerState(
            legacyValue.eventId,
            legacyValue.repeating,
            legacyValue.intervalGameMinutes,
            legacyValue.remainingGameMinutes,
            legacyValue.hasFired,
            static_cast<uint32_t>(index + 1)));
    }

    return true;
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::MapActorState &value)
{
    writeValue(writer, value.actorId);
    writeValue(writer, value.monsterId);
    writeValue(writer, value.displayName);
    writeValue(writer, value.uniqueNameId);
    writeValue(writer, value.spawnedAtRuntime);
    writeValue(writer, value.fromSpawnPoint);
    writeValue(writer, value.spawnPointIndex);
    writeValue(writer, value.group);
    writeValue(writer, value.ally);
    writeValue(writer, value.hostilityType);
    writeValue(writer, value.specialItemId);
    writeValue(writer, value.currentHp);
    writeValue(writer, value.bolsterRewardMultiplier);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.preciseX);
    writeValue(writer, value.preciseY);
    writeValue(writer, value.preciseZ);
    writeValue(writer, value.homeX);
    writeValue(writer, value.homeY);
    writeValue(writer, value.homeZ);
    writeValue(writer, value.homePreciseX);
    writeValue(writer, value.homePreciseY);
    writeValue(writer, value.homePreciseZ);
    writeValue(writer, value.radius);
    writeValue(writer, value.height);
    writeValue(writer, value.moveSpeed);
    writeValue(writer, value.armorClass);
    writeValue(writer, value.immobile);
    writeValue(writer, value.attack1DamageDiceRolls);
    writeValue(writer, value.attack1DamageDiceSides);
    writeValue(writer, value.attack1DamageBonus);
    writeValue(writer, value.attack2DamageDiceRolls);
    writeValue(writer, value.attack2DamageDiceSides);
    writeValue(writer, value.attack2DamageBonus);
    writeValue(writer, value.spell1SkillLevel);
    writeValue(writer, value.spell1SkillMastery);
    writeValue(writer, value.spell2SkillLevel);
    writeValue(writer, value.spell2SkillMastery);
    writeValue(writer, value.spriteFrameIndex);
    writeValue(writer, value.actionSpriteFrameIndices);
    writeValue(writer, value.useStaticSpriteFrame);
    writeValue(writer, value.hostileToParty);
    writeValue(writer, value.isDead);
    writeValue(writer, value.isInvisible);
    writeValue(writer, value.hasDetectedParty);
    writeValue(writer, value.aiState);
    writeValue(writer, value.animation);
    writeValue(writer, value.animationTimeTicks);
    writeValue(writer, value.recoverySeconds);
    writeValue(writer, value.attackAnimationSeconds);
    writeValue(writer, value.attackCooldownSeconds);
    writeValue(writer, value.idleDecisionSeconds);
    writeValue(writer, value.actionSeconds);
    writeValue(writer, value.moveDirectionX);
    writeValue(writer, value.moveDirectionY);
    writeValue(writer, value.velocityX);
    writeValue(writer, value.velocityY);
    writeValue(writer, value.velocityZ);
    writeValue(writer, value.yawRadians);
    writeValue(writer, value.slowRemainingSeconds);
    writeValue(writer, value.slowMoveMultiplier);
    writeValue(writer, value.slowRecoveryMultiplier);
    writeValue(writer, value.stunRemainingSeconds);
    writeValue(writer, value.paralyzeRemainingSeconds);
    writeValue(writer, value.fearRemainingSeconds);
    writeValue(writer, value.blindRemainingSeconds);
    writeValue(writer, value.controlRemainingSeconds);
    writeValue(writer, value.controlMode);
    writeValue(writer, value.shrinkRemainingSeconds);
    writeValue(writer, value.shrinkDamageMultiplier);
    writeValue(writer, value.shrinkArmorClassMultiplier);
    writeValue(writer, value.armorClassHalvedRemainingSeconds);
    writeValue(writer, value.darkGraspRemainingSeconds);
    writeValue(writer, value.idleDecisionCount);
    writeValue(writer, value.pursueDecisionCount);
    writeValue(writer, value.attackDecisionCount);
    writeValue(writer, value.attackImpactTriggered);
    writeValue(writer, value.queuedAttackAbility);
    writeValue(writer, value.movementState);
    writeValue(writer, value.movementStateInitialized);
    writeValue(writer, value.npcId);
    writeValue(writer, value.aiType);
    writeValue(writer, value.canFly);
    writeValue(writer, value.attack1DamageType);
    writeValue(writer, value.attack2DamageType);
    writeValue(writer, value.spell1Id);
    writeValue(writer, value.spell1DamageType);
    writeValue(writer, value.spell1CastSupported);
    writeValue(writer, value.spell2Id);
    writeValue(writer, value.spell2DamageType);
    writeValue(writer, value.spell2CastSupported);
    writeValue(writer, value.wanderRadius);
    writeValue(writer, value.generatedAttack2);
    writeValue(writer, value.generatedAttack2IsRanged);
    writeValue(writer, value.copyAttack1DamageToAttack2);
    writeValue(writer, value.generatedAttack2MissileType);
    writeValue(writer, value.generatedAttack2Chance);
    writeValue(writer, value.generatedSpell1UseChance);
    writeValue(writer, value.generatedSpell2UseChance);
    writeValue(writer, value.alertStatusBit);
    writeValue(writer, value.bloodSplatSpawned);
    writeValue(writer, value.dayOfProtectionRemainingSeconds);
    writeValue(writer, value.dayOfProtectionPower);
    writeValue(writer, value.hourOfPowerRemainingSeconds);
    writeValue(writer, value.hourOfPowerPower);
    writeValue(writer, value.painReflectionRemainingSeconds);
    writeValue(writer, value.hammerhandsRemainingSeconds);
    writeValue(writer, value.hammerhandsPower);
    writeValue(writer, value.hasteRemainingSeconds);
    writeValue(writer, value.shieldRemainingSeconds);
    writeValue(writer, value.stoneskinRemainingSeconds);
    writeValue(writer, value.stoneskinPower);
    writeValue(writer, value.blessRemainingSeconds);
    writeValue(writer, value.blessPower);
    writeValue(writer, value.fateRemainingSeconds);
    writeValue(writer, value.fatePower);
    writeValue(writer, value.heroismRemainingSeconds);
    writeValue(writer, value.heroismPower);
    writeValue(writer, value.crowdSideLockRemainingSeconds);
    writeValue(writer, value.crowdNoProgressSeconds);
    writeValue(writer, value.crowdLastEdgeDistance);
    writeValue(writer, value.crowdRetreatRemainingSeconds);
    writeValue(writer, value.crowdStandRemainingSeconds);
    writeValue(writer, value.crowdProbeX);
    writeValue(writer, value.crowdProbeY);
    writeValue(writer, value.crowdProbeEdgeDistance);
    writeValue(writer, value.crowdProbeElapsedSeconds);
    writeValue(writer, value.crowdEscapeAttempts);
    writeValue(writer, value.crowdSideSign);
    writeValue(writer, value.suppressLowHealthFlee);
    writeValue(writer, value.mm9RudeId);
    writeValue(writer, value.usesMm9ActorRules);
    writeValue(writer, value.canReceiveDamage);
    writeValue(writer, value.mm9Civilian);
    writeValue(writer, value.mm9Guard);
    writeValue(writer, value.mm9FleeingFromParty);
    writeValue(writer, value.mm9FleeRemainingSeconds);
    writeValue(writer, value.mm9HelpCooldownSeconds);
    writeValue(writer, value.mm9PlayerHitCount);
    writeValue(writer, value.proceduralDeathLoot);
    writeValue(writer, value.breachAlvarCompanion);
    writeValue(writer, value.breachAlvarCompanionRemoved);
    writeValue(writer, value.breachAlvarNextMaintenanceGameMinute);
    writeValue(writer, value.combatState);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::MapActorState &value)
{
    return readValue(reader, value.actorId)
        && readValue(reader, value.monsterId)
        && readValue(reader, value.displayName)
        && readValue(reader, value.uniqueNameId)
        && readValue(reader, value.spawnedAtRuntime)
        && readValue(reader, value.fromSpawnPoint)
        && readValue(reader, value.spawnPointIndex)
        && readValue(reader, value.group)
        && readValue(reader, value.ally)
        && readValue(reader, value.hostilityType)
        && readValue(reader, value.specialItemId)
        && readValue(reader, value.currentHp)
        && readLegacyOutdoorMaximumHitPoints(reader, value.combatState)
        && (reader.version() < SaveVersionMonsterBolsterRewards || readValue(reader, value.bolsterRewardMultiplier))
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.preciseX)
        && readValue(reader, value.preciseY)
        && readValue(reader, value.preciseZ)
        && readValue(reader, value.homeX)
        && readValue(reader, value.homeY)
        && readValue(reader, value.homeZ)
        && readValue(reader, value.homePreciseX)
        && readValue(reader, value.homePreciseY)
        && readValue(reader, value.homePreciseZ)
        && readValue(reader, value.radius)
        && readValue(reader, value.height)
        && readValue(reader, value.moveSpeed)
        && (reader.version() < SaveVersionOutdoorActorBolsterCache
            || (readValue(reader, value.armorClass)
                && readValue(reader, value.immobile)
                && (reader.version() < SaveVersionMonsterBolsterDamageDice
                    || (readValue(reader, value.attack1DamageDiceRolls)
                        && readValue(reader, value.attack1DamageDiceSides)))
                && readValue(reader, value.attack1DamageBonus)
                && (reader.version() < SaveVersionMonsterBolsterDamageDice
                    || (readValue(reader, value.attack2DamageDiceRolls)
                        && readValue(reader, value.attack2DamageDiceSides)))
                && readValue(reader, value.attack2DamageBonus)
                && readValue(reader, value.spell1SkillLevel)
                && readValue(reader, value.spell1SkillMastery)
                && readValue(reader, value.spell2SkillLevel)
                && readValue(reader, value.spell2SkillMastery)))
        && readValue(reader, value.spriteFrameIndex)
        && readValue(reader, value.actionSpriteFrameIndices)
        && readValue(reader, value.useStaticSpriteFrame)
        && readValue(reader, value.hostileToParty)
        && readValue(reader, value.isDead)
        && readValue(reader, value.isInvisible)
        && readValue(reader, value.hasDetectedParty)
        && readValue(reader, value.aiState)
        && readValue(reader, value.animation)
        && readValue(reader, value.animationTimeTicks)
        && readValue(reader, value.recoverySeconds)
        && readValue(reader, value.attackAnimationSeconds)
        && readValue(reader, value.attackCooldownSeconds)
        && readValue(reader, value.idleDecisionSeconds)
        && readValue(reader, value.actionSeconds)
        && readValue(reader, value.moveDirectionX)
        && readValue(reader, value.moveDirectionY)
        && readValue(reader, value.velocityX)
        && readValue(reader, value.velocityY)
        && readValue(reader, value.velocityZ)
        && readValue(reader, value.yawRadians)
        && readValue(reader, value.slowRemainingSeconds)
        && readValue(reader, value.slowMoveMultiplier)
        && readValue(reader, value.slowRecoveryMultiplier)
        && readValue(reader, value.stunRemainingSeconds)
        && readValue(reader, value.paralyzeRemainingSeconds)
        && readValue(reader, value.fearRemainingSeconds)
        && readValue(reader, value.blindRemainingSeconds)
        && readValue(reader, value.controlRemainingSeconds)
        && readValue(reader, value.controlMode)
        && readValue(reader, value.shrinkRemainingSeconds)
        && readValue(reader, value.shrinkDamageMultiplier)
        && readValue(reader, value.shrinkArmorClassMultiplier)
        && (reader.version() < SaveVersionCombatEffectState
            || readValue(reader, value.armorClassHalvedRemainingSeconds))
        && readValue(reader, value.darkGraspRemainingSeconds)
        && readValue(reader, value.idleDecisionCount)
        && readValue(reader, value.pursueDecisionCount)
        && readValue(reader, value.attackDecisionCount)
        && readValue(reader, value.attackImpactTriggered)
        && readValue(reader, value.queuedAttackAbility)
        && readValue(reader, value.movementState)
        && readValue(reader, value.movementStateInitialized)
        && (reader.version() < SaveVersionOutdoorRuntimeSaveParity
            || (readValue(reader, value.npcId)
                && readValue(reader, value.aiType)
                && readValue(reader, value.canFly)
                && readValue(reader, value.attack1DamageType)
                && readValue(reader, value.attack2DamageType)
                && readValue(reader, value.spell1Id)
                && readValue(reader, value.spell1DamageType)
                && readValue(reader, value.spell1CastSupported)
                && readValue(reader, value.spell2Id)
                && readValue(reader, value.spell2DamageType)
                && readValue(reader, value.spell2CastSupported)
                && readValue(reader, value.wanderRadius)
                && readValue(reader, value.generatedAttack2)
                && readValue(reader, value.generatedAttack2IsRanged)
                && readValue(reader, value.copyAttack1DamageToAttack2)
                && readValue(reader, value.generatedAttack2MissileType)
                && readValue(reader, value.generatedAttack2Chance)
                && readValue(reader, value.generatedSpell1UseChance)
                && readValue(reader, value.generatedSpell2UseChance)
                && readValue(reader, value.alertStatusBit)
                && readValue(reader, value.bloodSplatSpawned)
                && readValue(reader, value.dayOfProtectionRemainingSeconds)
                && readValue(reader, value.dayOfProtectionPower)
                && readValue(reader, value.hourOfPowerRemainingSeconds)
                && readValue(reader, value.hourOfPowerPower)
                && readValue(reader, value.painReflectionRemainingSeconds)
                && readValue(reader, value.hammerhandsRemainingSeconds)
                && readValue(reader, value.hammerhandsPower)
                && readValue(reader, value.hasteRemainingSeconds)
                && readValue(reader, value.shieldRemainingSeconds)
                && readValue(reader, value.stoneskinRemainingSeconds)
                && readValue(reader, value.stoneskinPower)
                && readValue(reader, value.blessRemainingSeconds)
                && readValue(reader, value.blessPower)
                && readValue(reader, value.fateRemainingSeconds)
                && readValue(reader, value.fatePower)
                && readValue(reader, value.heroismRemainingSeconds)
                && readValue(reader, value.heroismPower)
                && readValue(reader, value.crowdSideLockRemainingSeconds)
                && readValue(reader, value.crowdNoProgressSeconds)
                && readValue(reader, value.crowdLastEdgeDistance)
                && readValue(reader, value.crowdRetreatRemainingSeconds)
                && readValue(reader, value.crowdStandRemainingSeconds)
                && readValue(reader, value.crowdProbeX)
                && readValue(reader, value.crowdProbeY)
                && readValue(reader, value.crowdProbeEdgeDistance)
                && readValue(reader, value.crowdProbeElapsedSeconds)
                && readValue(reader, value.crowdEscapeAttempts)
                && readValue(reader, value.crowdSideSign)
                && readValue(reader, value.suppressLowHealthFlee)))
        && (reader.version() < SaveVersionMm9DialogueActors || readValue(reader, value.mm9RudeId))
        && (reader.version() < SaveVersionMm9ActorReactions
            || (readValue(reader, value.usesMm9ActorRules)
                && readValue(reader, value.canReceiveDamage)
                && readValue(reader, value.mm9Civilian)
                && readValue(reader, value.mm9Guard)
                && readValue(reader, value.mm9FleeingFromParty)
                && readValue(reader, value.mm9FleeRemainingSeconds)
                && readValue(reader, value.mm9HelpCooldownSeconds)
                && readValue(reader, value.mm9PlayerHitCount)))
        && (reader.version() < SaveVersionActorProceduralDeathLoot
            || readValue(reader, value.proceduralDeathLoot))
        && (reader.version() < SaveVersionBreachAlvarCompanions
            || (readValue(reader, value.breachAlvarCompanion)
                && readValue(reader, value.breachAlvarCompanionRemoved)
                && readValue(reader, value.breachAlvarNextMaintenanceGameMinute)))
        && (reader.version() < SaveVersionResolvedActorCombat || readValue(reader, value.combatState));
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::ChestItemState &value)
{
    writeValue(writer, value.item);
    writeValue(writer, value.itemId);
    writeValue(writer, value.quantity);
    writeValue(writer, value.goldAmount);
    writeValue(writer, value.goldRollCount);
    writeValue(writer, value.isGold);
    writeValue(writer, value.width);
    writeValue(writer, value.height);
    writeValue(writer, value.gridX);
    writeValue(writer, value.gridY);
    writeValue(writer, value.itemRecordIndex);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::ChestItemState &value)
{
    return readValue(reader, value.item)
        && readValue(reader, value.itemId)
        && readValue(reader, value.quantity)
        && readValue(reader, value.goldAmount)
        && readValue(reader, value.goldRollCount)
        && readValue(reader, value.isGold)
        && readValue(reader, value.width)
        && readValue(reader, value.height)
        && readValue(reader, value.gridX)
        && readValue(reader, value.gridY)
        && (reader.version() < SaveVersionChestItemRecords || readValue(reader, value.itemRecordIndex));
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::ChestViewState &value)
{
    writeValue(writer, value.chestId);
    writeValue(writer, value.chestTypeId);
    writeValue(writer, value.flags);
    writeValue(writer, value.gridWidth);
    writeValue(writer, value.gridHeight);
    writeValue(writer, value.items);
    writeValue(writer, value.hiddenItems);
    writeValue(writer, value.itemRecordCapacity);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::ChestViewState &value)
{
    return readValue(reader, value.chestId)
        && readValue(reader, value.chestTypeId)
        && readValue(reader, value.flags)
        && readValue(reader, value.gridWidth)
        && readValue(reader, value.gridHeight)
        && readValue(reader, value.items)
        && readValue(reader, value.hiddenItems)
        && (reader.version() < SaveVersionChestItemRecords || readValue(reader, value.itemRecordCapacity));
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::CorpseViewState &value)
{
    writeValue(writer, value.fromSummonedMonster);
    writeValue(writer, value.sourceIndex);
    writeValue(writer, value.title);
    writeValue(writer, value.items);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::CorpseViewState &value)
{
    return readValue(reader, value.fromSummonedMonster)
        && readValue(reader, value.sourceIndex)
        && readValue(reader, value.title)
        && readValue(reader, value.items);
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::WorldItemState &value)
{
    writeValue(writer, value.worldItemId);
    writeValue(writer, value.item);
    writeValue(writer, value.goldAmount);
    writeValue(writer, value.isGold);
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.objectSpriteId);
    writeValue(writer, value.objectSpriteFrameIndex);
    writeValue(writer, value.objectFlags);
    writeValue(writer, value.radius);
    writeValue(writer, value.height);
    writeValue(writer, value.soundId);
    writeValue(writer, value.attributes);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.objectName);
    writeValue(writer, value.objectSpriteName);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.velocityX);
    writeValue(writer, value.velocityY);
    writeValue(writer, value.velocityZ);
    writeValue(writer, value.initialX);
    writeValue(writer, value.initialY);
    writeValue(writer, value.initialZ);
    writeValue(writer, value.timeSinceCreatedTicks);
    writeValue(writer, value.lifetimeTicks);
    writeValue(writer, value.spawnedByPlayer);
    writeValue(writer, value.isExpired);
    writeValue(writer, value.semanticSourceId);
    writeValue(writer, value.semanticPlacedPickup);
    writeValue(writer, value.semanticLootContainerId);
    writeValue(writer, value.semanticLootContainer);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::WorldItemState &value)
{
    return readValue(reader, value.worldItemId)
        && readValue(reader, value.item)
        && readValue(reader, value.goldAmount)
        && readValue(reader, value.isGold)
        && readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.objectSpriteId)
        && readValue(reader, value.objectSpriteFrameIndex)
        && readValue(reader, value.objectFlags)
        && readValue(reader, value.radius)
        && readValue(reader, value.height)
        && readValue(reader, value.soundId)
        && readValue(reader, value.attributes)
        && readValue(reader, value.sectorId)
        && readValue(reader, value.objectName)
        && readValue(reader, value.objectSpriteName)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.velocityX)
        && readValue(reader, value.velocityY)
        && readValue(reader, value.velocityZ)
        && readValue(reader, value.initialX)
        && readValue(reader, value.initialY)
        && readValue(reader, value.initialZ)
        && readValue(reader, value.timeSinceCreatedTicks)
        && readValue(reader, value.lifetimeTicks)
        && readValue(reader, value.spawnedByPlayer)
        && readValue(reader, value.isExpired)
        && (reader.version() < SaveVersionMm9SemanticWorldItems
            || (readValue(reader, value.semanticSourceId)
                && readValue(reader, value.semanticPlacedPickup)
                && readValue(reader, value.semanticLootContainerId)
                && readValue(reader, value.semanticLootContainer)));
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::ArmageddonState &value)
{
    writeValue(writer, value.remainingSeconds);
    writeValue(writer, value.skillLevel);
    writeValue(writer, value.skillMastery);
    writeValue(writer, value.casterMemberIndex);
    writeValue(writer, value.shakeStepsRemaining);
    writeValue(writer, value.shakeSequence);
    writeValue(writer, value.cameraShakeYawRadians);
    writeValue(writer, value.cameraShakePitchRadians);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::ArmageddonState &value)
{
    return readValue(reader, value.remainingSeconds)
        && readValue(reader, value.skillLevel)
        && readValue(reader, value.skillMastery)
        && readValue(reader, value.casterMemberIndex)
        && readValue(reader, value.shakeStepsRemaining)
        && readValue(reader, value.shakeSequence)
        && readValue(reader, value.cameraShakeYawRadians)
        && readValue(reader, value.cameraShakePitchRadians);
}

void writeValue(BinaryWriter &writer, const GameplayProjectileService::ProjectileState &value)
{
    writeValue(writer, value.projectileId);
    writeValue(writer, value.sourceKind);
    writeValue(writer, value.sourceId);
    writeValue(writer, value.sourcePartyMemberIndex);
    writeValue(writer, value.sourceMonsterId);
    writeValue(writer, value.fromSummonedMonster);
    writeValue(writer, value.ability);
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.objectSpriteId);
    writeValue(writer, value.objectSpriteFrameIndex);
    writeValue(writer, value.impactObjectDescriptionId);
    writeValue(writer, value.objectFlags);
    writeValue(writer, value.visualMode);
    writeValue(writer, value.radius);
    writeValue(writer, value.height);
    writeValue(writer, value.spellId);
    writeValue(writer, value.effectSoundId);
    writeValue(writer, value.impactSoundIdOverride);
    writeValue(writer, value.skillLevel);
    writeValue(writer, value.skillMastery);
    writeValue(writer, value.objectName);
    writeValue(writer, value.objectSpriteName);
    writeValue(writer, value.sourceX);
    writeValue(writer, value.sourceY);
    writeValue(writer, value.sourceZ);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.velocityX);
    writeValue(writer, value.velocityY);
    writeValue(writer, value.velocityZ);
    writeValue(writer, value.damage);
    writeValue(writer, value.attackBonus);
    writeValue(writer, value.damageType);
    writeValue(writer, value.useActorHitChance);
    writeValue(writer, value.timeSinceCreatedTicks);
    writeValue(writer, value.lifetimeTickAccumulator);
    writeValue(writer, value.lifetimeTicks);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.isSettled);
    writeValue(writer, value.isExpired);
    writeValue(writer, value.turnBasedPendingAction);
}

bool readValue(BinaryReader &reader, GameplayProjectileService::ProjectileState &value)
{
    return readValue(reader, value.projectileId)
        && readValue(reader, value.sourceKind)
        && readValue(reader, value.sourceId)
        && readValue(reader, value.sourcePartyMemberIndex)
        && readValue(reader, value.sourceMonsterId)
        && readValue(reader, value.fromSummonedMonster)
        && readValue(reader, value.ability)
        && readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.objectSpriteId)
        && readValue(reader, value.objectSpriteFrameIndex)
        && readValue(reader, value.impactObjectDescriptionId)
        && readValue(reader, value.objectFlags)
        && (reader.version() < SaveVersionProjectileVisualMode || readValue(reader, value.visualMode))
        && readValue(reader, value.radius)
        && readValue(reader, value.height)
        && readValue(reader, value.spellId)
        && readValue(reader, value.effectSoundId)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.impactSoundIdOverride))
        && readValue(reader, value.skillLevel)
        && readValue(reader, value.skillMastery)
        && readValue(reader, value.objectName)
        && readValue(reader, value.objectSpriteName)
        && readValue(reader, value.sourceX)
        && readValue(reader, value.sourceY)
        && readValue(reader, value.sourceZ)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.velocityX)
        && readValue(reader, value.velocityY)
        && readValue(reader, value.velocityZ)
        && readValue(reader, value.damage)
        && readValue(reader, value.attackBonus)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.damageType))
        && readValue(reader, value.useActorHitChance)
        && readValue(reader, value.timeSinceCreatedTicks)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.lifetimeTickAccumulator))
        && readValue(reader, value.lifetimeTicks)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.sectorId))
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.isSettled))
        && readValue(reader, value.isExpired)
        && (reader.version() < SaveVersionTurnBasedCombat || readValue(reader, value.turnBasedPendingAction));
}

void writeValue(BinaryWriter &writer, const GameplayProjectileService::ProjectileImpactState &value)
{
    writeValue(writer, value.effectId);
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.objectSpriteId);
    writeValue(writer, value.objectSpriteFrameIndex);
    writeValue(writer, value.sourceObjectFlags);
    writeValue(writer, value.sourceSpellId);
    writeValue(writer, value.objectName);
    writeValue(writer, value.objectSpriteName);
    writeValue(writer, value.sourceObjectName);
    writeValue(writer, value.sourceObjectSpriteName);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.timeSinceCreatedTicks);
    writeValue(writer, value.lifetimeTickAccumulator);
    writeValue(writer, value.lifetimeTicks);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.freezeAnimation);
    writeValue(writer, value.isExpired);
}

bool readValue(BinaryReader &reader, GameplayProjectileService::ProjectileImpactState &value)
{
    return readValue(reader, value.effectId)
        && readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.objectSpriteId)
        && readValue(reader, value.objectSpriteFrameIndex)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.sourceObjectFlags))
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.sourceSpellId))
        && readValue(reader, value.objectName)
        && readValue(reader, value.objectSpriteName)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.sourceObjectName))
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.sourceObjectSpriteName))
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.timeSinceCreatedTicks)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.lifetimeTickAccumulator))
        && readValue(reader, value.lifetimeTicks)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.sectorId))
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.freezeAnimation))
        && readValue(reader, value.isExpired);
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::FireSpikeTrapState &value)
{
    writeValue(writer, value.trapId);
    writeValue(writer, value.sourceKind);
    writeValue(writer, value.sourceId);
    writeValue(writer, value.sourcePartyMemberIndex);
    writeValue(writer, value.sourceMonsterId);
    writeValue(writer, value.fromSummonedMonster);
    writeValue(writer, value.ability);
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.objectSpriteId);
    writeValue(writer, value.objectSpriteFrameIndex);
    writeValue(writer, value.impactObjectDescriptionId);
    writeValue(writer, value.objectFlags);
    writeValue(writer, value.radius);
    writeValue(writer, value.height);
    writeValue(writer, value.spellId);
    writeValue(writer, value.effectSoundId);
    writeValue(writer, value.skillLevel);
    writeValue(writer, value.skillMastery);
    writeValue(writer, value.objectName);
    writeValue(writer, value.objectSpriteName);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.timeSinceCreatedTicks);
    writeValue(writer, value.isExpired);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::FireSpikeTrapState &value)
{
    return readValue(reader, value.trapId)
        && readValue(reader, value.sourceKind)
        && readValue(reader, value.sourceId)
        && readValue(reader, value.sourcePartyMemberIndex)
        && readValue(reader, value.sourceMonsterId)
        && readValue(reader, value.fromSummonedMonster)
        && readValue(reader, value.ability)
        && readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.objectSpriteId)
        && readValue(reader, value.objectSpriteFrameIndex)
        && readValue(reader, value.impactObjectDescriptionId)
        && readValue(reader, value.objectFlags)
        && readValue(reader, value.radius)
        && readValue(reader, value.height)
        && readValue(reader, value.spellId)
        && readValue(reader, value.effectSoundId)
        && readValue(reader, value.skillLevel)
        && readValue(reader, value.skillMastery)
        && readValue(reader, value.objectName)
        && readValue(reader, value.objectSpriteName)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.timeSinceCreatedTicks)
        && readValue(reader, value.isExpired);
}

void writeValue(BinaryWriter &writer, const IndoorWorldRuntime::FireSpikeTrapState &value)
{
    writeValue(writer, value.trapId);
    writeValue(writer, value.sourceKind);
    writeValue(writer, value.sourceId);
    writeValue(writer, value.sourcePartyMemberIndex);
    writeValue(writer, value.sourceMonsterId);
    writeValue(writer, value.fromSummonedMonster);
    writeValue(writer, value.ability);
    writeValue(writer, value.objectDescriptionId);
    writeValue(writer, value.objectSpriteId);
    writeValue(writer, value.objectSpriteFrameIndex);
    writeValue(writer, value.impactObjectDescriptionId);
    writeValue(writer, value.objectFlags);
    writeValue(writer, value.radius);
    writeValue(writer, value.height);
    writeValue(writer, value.spellId);
    writeValue(writer, value.effectSoundId);
    writeValue(writer, value.skillLevel);
    writeValue(writer, value.skillMastery);
    writeValue(writer, value.objectName);
    writeValue(writer, value.objectSpriteName);
    writeValue(writer, value.x);
    writeValue(writer, value.y);
    writeValue(writer, value.z);
    writeValue(writer, value.sectorId);
    writeValue(writer, value.timeSinceCreatedTicks);
    writeValue(writer, value.isExpired);
}

bool readValue(BinaryReader &reader, IndoorWorldRuntime::FireSpikeTrapState &value)
{
    return readValue(reader, value.trapId)
        && readValue(reader, value.sourceKind)
        && readValue(reader, value.sourceId)
        && readValue(reader, value.sourcePartyMemberIndex)
        && readValue(reader, value.sourceMonsterId)
        && readValue(reader, value.fromSummonedMonster)
        && readValue(reader, value.ability)
        && readValue(reader, value.objectDescriptionId)
        && readValue(reader, value.objectSpriteId)
        && readValue(reader, value.objectSpriteFrameIndex)
        && readValue(reader, value.impactObjectDescriptionId)
        && readValue(reader, value.objectFlags)
        && readValue(reader, value.radius)
        && readValue(reader, value.height)
        && readValue(reader, value.spellId)
        && readValue(reader, value.effectSoundId)
        && readValue(reader, value.skillLevel)
        && readValue(reader, value.skillMastery)
        && readValue(reader, value.objectName)
        && readValue(reader, value.objectSpriteName)
        && readValue(reader, value.x)
        && readValue(reader, value.y)
        && readValue(reader, value.z)
        && readValue(reader, value.sectorId)
        && readValue(reader, value.timeSinceCreatedTicks)
        && readValue(reader, value.isExpired);
}

void writeValue(BinaryWriter &writer, const GameplayProjectileService::Snapshot &value)
{
    writeValue(writer, value.nextProjectileId);
    writeValue(writer, value.nextProjectileImpactId);
    writeValue(writer, value.nextFireSpikeTrapId);
    writeValue(writer, value.projectiles);
    writeValue(writer, value.projectileImpacts);
}

bool readValue(BinaryReader &reader, GameplayProjectileService::Snapshot &value)
{
    return readValue(reader, value.nextProjectileId)
        && readValue(reader, value.nextProjectileImpactId)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.nextFireSpikeTrapId))
        && readValue(reader, value.projectiles)
        && readValue(reader, value.projectileImpacts);
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::AtmosphereState &value)
{
    writeValue(writer, value.sourceSkyTextureName);
    writeValue(writer, value.skyTextureName);
    writeValue(writer, value.weatherFlags);
    writeValue(writer, value.fogWeakDistance);
    writeValue(writer, value.fogStrongDistance);
    writeValue(writer, value.redFog);
    writeValue(writer, value.isNight);
    writeValue(writer, value.fogDensity);
    writeValue(writer, value.ambientBrightness);
    writeValue(writer, value.visibilityDistance);
    writeValue(writer, value.darknessOverlayAlpha);
    writeValue(writer, value.darknessOverlayColorAbgr);
    writeValue(writer, value.sunDirectionX);
    writeValue(writer, value.sunDirectionY);
    writeValue(writer, value.sunDirectionZ);
    writeValue(writer, value.clearColorAbgr);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::AtmosphereState &value)
{
    return readValue(reader, value.sourceSkyTextureName)
        && readValue(reader, value.skyTextureName)
        && readValue(reader, value.weatherFlags)
        && readValue(reader, value.fogWeakDistance)
        && readValue(reader, value.fogStrongDistance)
        && readValue(reader, value.redFog)
        && readValue(reader, value.isNight)
        && readValue(reader, value.fogDensity)
        && readValue(reader, value.ambientBrightness)
        && readValue(reader, value.visibilityDistance)
        && readValue(reader, value.darknessOverlayAlpha)
        && readValue(reader, value.darknessOverlayColorAbgr)
        && readValue(reader, value.sunDirectionX)
        && readValue(reader, value.sunDirectionY)
        && readValue(reader, value.sunDirectionZ)
        && readValue(reader, value.clearColorAbgr);
}

void writeValue(BinaryWriter &writer, const OutdoorWorldRuntime::Snapshot &value)
{
    writeValue(writer, value.gameMinutes);
    writeValue(writer, value.locationInfo);
    writeValue(writer, value.locationTime);
    writeValue(writer, value.hasLocationTime);
    writeValue(writer, value.atmosphere);
    writeValue(writer, value.timers);
    writeValue(writer, value.mapActors);
    writeValue(writer, value.chests);
    writeValue(writer, value.openedChestFlags);
    writeValue(writer, value.materializedChestViews);
    writeValue(writer, value.activeChestView);
    writeValue(writer, value.eventRuntimeState);
    writeValue(writer, value.actorUpdateAccumulatorSeconds);
    writeValue(writer, value.sessionChestSeed);
    writeValue(writer, value.nextActorId);
    writeValue(writer, value.mapActorCorpseViews);
    writeValue(writer, value.activeCorpseView);
    writeValue(writer, value.worldItems);
    writeValue(writer, value.nextWorldItemId);
    writeValue(writer, value.nextProjectileId);
    writeValue(writer, value.nextProjectileImpactId);
    writeValue(writer, value.nextFireSpikeTrapId);
    writeValue(writer, value.projectiles);
    writeValue(writer, value.projectileImpacts);
    writeValue(writer, value.fireSpikeTraps);
    writeValue(writer, value.fullyRevealedCells);
    writeValue(writer, value.partiallyRevealedCells);
    writeValue(writer, value.gameplayOverlayRemainingSeconds);
    writeValue(writer, value.gameplayOverlayDurationSeconds);
    writeValue(writer, value.gameplayOverlayPeakAlpha);
    writeValue(writer, value.gameplayOverlayColorAbgr);
    writeValue(writer, value.armageddon);
    writeValue(writer, value.hasRainIntensityOverride);
    writeValue(writer, value.rainIntensityPreset);
    writeValue(writer, value.bloodSplats);
    writeValue(writer, value.faceAttributes);
    writeValue(writer, value.searchedLootPropSourceIds);
    writeValue(writer, value.decorationFlags);
}

bool readValue(BinaryReader &reader, OutdoorWorldRuntime::Snapshot &value)
{
    const bool loaded = readGameMinutes(reader, value.gameMinutes)
        && (reader.version() < SaveVersionOutdoorLocationInfo || readValue(reader, value.locationInfo))
        && (reader.version() < SaveVersionLegacyEvtTimers || readValue(reader, value.locationTime))
        && (reader.version() < SaveVersionLegacyEvtTimers || readValue(reader, value.hasLocationTime))
        && readValue(reader, value.atmosphere)
        && readOutdoorTimerStates(reader, value.timers)
        && readValue(reader, value.mapActors)
        && readValue(reader, value.chests)
        && readValue(reader, value.openedChestFlags)
        && readValue(reader, value.materializedChestViews)
        && readValue(reader, value.activeChestView)
        && readValue(reader, value.eventRuntimeState)
        && readValue(reader, value.actorUpdateAccumulatorSeconds)
        && readValue(reader, value.sessionChestSeed)
        && readValue(reader, value.nextActorId)
        && readValue(reader, value.mapActorCorpseViews)
        && readValue(reader, value.activeCorpseView)
        && readValue(reader, value.worldItems)
        && readValue(reader, value.nextWorldItemId)
        && readValue(reader, value.nextProjectileId)
        && readValue(reader, value.nextProjectileImpactId)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.nextFireSpikeTrapId))
        && readValue(reader, value.projectiles)
        && readValue(reader, value.projectileImpacts)
        && (reader.version() < SaveVersionFullProjectileState || readValue(reader, value.fireSpikeTraps))
        && (reader.version() < SaveVersionOutdoorJournalRevealMask
            || readValue(reader, value.fullyRevealedCells))
        && (reader.version() < SaveVersionOutdoorJournalRevealMask
            || readValue(reader, value.partiallyRevealedCells))
        && (reader.version() < SaveVersionOutdoorRuntimeSaveParity
            || (readValue(reader, value.gameplayOverlayRemainingSeconds)
                && readValue(reader, value.gameplayOverlayDurationSeconds)
                && readValue(reader, value.gameplayOverlayPeakAlpha)
                && readValue(reader, value.gameplayOverlayColorAbgr)
                && readValue(reader, value.armageddon)
                && readValue(reader, value.hasRainIntensityOverride)
                && readValue(reader, value.rainIntensityPreset)
                && readValue(reader, value.bloodSplats)))
        && (reader.version() < SaveVersionOutdoorFaceAttributes
            || readValue(reader, value.faceAttributes))
        && (reader.version() < SaveVersionMm9ItemSourceState
            || readValue(reader, value.searchedLootPropSourceIds))
        && (reader.version() < SaveVersionOutdoorDecorationFlags
            || readValue(reader, value.decorationFlags));

    if (loaded && reader.version() >= SaveVersionOutdoorRuntimeSaveParity)
    {
        value.hasOutdoorRuntimeSaveParityFields = true;
    }
    if (reader.version() < SaveVersionOutdoorDecorationFlags)
    {
        value.decorationFlags.clear();
    }

    if (loaded && value.eventRuntimeState)
    {
        migrateLegacyMercenaryRefillState(*value.eventRuntimeState, value.gameMinutes);
    }

    return loaded;
}

void writeValue(BinaryWriter &writer, const IndoorSceneRuntime::Snapshot &value)
{
    writeValue(writer, value.mapDeltaData);
    writeValue(writer, value.eventRuntimeState);
    writeValue(writer, value.worldRuntime);
    writeValue(writer, value.partyRuntime);
    writeValue(writer, value.timers);
    writeValue(writer, value.lastProcessedPartyMoveStateForFaceTriggers);
    writeValue(writer, value.mechanismAccumulatorMilliseconds);
}

bool readValue(BinaryReader &reader, IndoorSceneRuntime::Snapshot &value)
{
    const bool loaded = readValue(reader, value.mapDeltaData)
        && readValue(reader, value.eventRuntimeState)
        && (reader.version() < 20 || readValue(reader, value.worldRuntime))
        && (reader.version() < 20 || readValue(reader, value.partyRuntime))
        && (reader.version() < SaveVersionIndoorSaveLoadParity || readIndoorTimerStates(reader, value.timers))
        && (reader.version() < SaveVersionIndoorSaveLoadParity
            || readValue(reader, value.lastProcessedPartyMoveStateForFaceTriggers))
        && readValue(reader, value.mechanismAccumulatorMilliseconds);
    if (loaded && value.eventRuntimeState)
    {
        migrateLegacyMercenaryRefillState(*value.eventRuntimeState, value.worldRuntime.gameMinutes);
    }
    return loaded;
}

void writeValue(BinaryWriter &writer, const BreachMazeFloorIdentity &value)
{
    writeValue(writer, value.epoch);
    writeValue(writer, value.level);
    writeValue(writer, value.generationSerial);
}

bool readValue(BinaryReader &reader, BreachMazeFloorIdentity &value)
{
    return readValue(reader, value.epoch)
        && readValue(reader, value.level)
        && readValue(reader, value.generationSerial);
}

void writeValue(BinaryWriter &writer, const BreachMazeFloorSeeds &value)
{
    writeValue(writer, value.layout);
    writeValue(writer, value.material);
    writeValue(writer, value.population);
    writeValue(writer, value.interaction);
}

bool readValue(BinaryReader &reader, BreachMazeFloorSeeds &value)
{
    return readValue(reader, value.layout)
        && readValue(reader, value.material)
        && readValue(reader, value.population)
        && readValue(reader, value.interaction);
}

void writeValue(BinaryWriter &writer, const BreachMazeCell &value)
{
    writeValue(writer, value.templateCellId);
    writeValue(writer, value.room);
    writeValue(writer, value.walls);
}

bool readValue(BinaryReader &reader, BreachMazeCell &value)
{
    return readValue(reader, value.templateCellId)
        && readValue(reader, value.room)
        && readValue(reader, value.walls);
}

void writeValue(BinaryWriter &writer, const BreachMazeEndpoint &value)
{
    writeValue(writer, value.templateCellId);
    writeValue(writer, value.faceIndex);
    writeValue(writer, value.wallSide);
}

bool readValue(BinaryReader &reader, BreachMazeEndpoint &value)
{
    return readValue(reader, value.templateCellId)
        && readValue(reader, value.faceIndex)
        && readValue(reader, value.wallSide);
}

void writeValue(BinaryWriter &writer, const BreachMazeEndpoints &value)
{
    writeValue(writer, value.entranceSearchSide);
    writeValue(writer, value.exitSearchSide);
    writeValue(writer, value.entrance);
    writeValue(writer, value.exit);
    writeValue(writer, value.entranceUsedFallback);
    writeValue(writer, value.exitUsedFallback);
    writeValue(writer, value.duplicateCellFallback);
}

bool readValue(BinaryReader &reader, BreachMazeEndpoints &value)
{
    return readValue(reader, value.entranceSearchSide)
        && readValue(reader, value.exitSearchSide)
        && readValue(reader, value.entrance)
        && readValue(reader, value.exit)
        && readValue(reader, value.entranceUsedFallback)
        && readValue(reader, value.exitUsedFallback)
        && readValue(reader, value.duplicateCellFallback);
}

void writeValue(BinaryWriter &writer, const BreachMazeLayout &value)
{
    writeValue(writer, value.algorithmVersion);
    writeValue(writer, value.seed);
    writeValue(writer, value.finalRandomState);
    writeValue(writer, value.randomDrawCount);
    writeValue(writer, value.level);
    writeValue(writer, value.side);
    writeValue(writer, value.cells);
    writeValue(writer, value.endpoints);
}

bool readValue(BinaryReader &reader, BreachMazeLayout &value)
{
    return readValue(reader, value.algorithmVersion)
        && readValue(reader, value.seed)
        && readValue(reader, value.finalRandomState)
        && readValue(reader, value.randomDrawCount)
        && readValue(reader, value.level)
        && readValue(reader, value.side)
        && readValue(reader, value.cells)
        && readValue(reader, value.endpoints);
}

void writeValue(BinaryWriter &writer, const BreachMazeFloorDescriptor &value)
{
    writeValue(writer, value.identity);
    writeValue(writer, value.seeds);
    writeValue(writer, value.layout);
}

bool readValue(BinaryReader &reader, BreachMazeFloorDescriptor &value)
{
    return readValue(reader, value.identity)
        && readValue(reader, value.seeds)
        && readValue(reader, value.layout);
}

void writeValue(BinaryWriter &writer, const FixedItemGenerationCursor &value)
{
    writeValue(writer, value.protocol);
    writeValue(writer, value.state);
    writeValue(writer, value.rawDrawCount);
}

bool readValue(BinaryReader &reader, FixedItemGenerationCursor &value)
{
    return readValue(reader, value.protocol)
        && readValue(reader, value.state)
        && readValue(reader, value.rawDrawCount);
}

bool readBreachMazeFlag(BinaryReader &reader, bool &value)
{
    uint8_t encoded = 0;
    if (!readValue(reader, encoded) || encoded > 1) return false;
    value = encoded != 0;
    return true;
}

void writeValue(BinaryWriter &writer, const BreachMazeTrapState &value)
{
    writeValue(writer, uint8_t(value.spellId.has_value()));
    if (value.spellId) writeValue(writer, *value.spellId);
    writeValue(writer, value.nextShotGameMinute);
}

bool readValue(BinaryReader &reader, BreachMazeTrapState &value)
{
    bool selected = false;
    if (!readBreachMazeFlag(reader, selected)) return false;
    value.spellId.reset();
    if (selected)
    {
        uint32_t spellId = 0;
        if (!readValue(reader, spellId)) return false;
        value.spellId = spellId;
    }
    return readValue(reader, value.nextShotGameMinute);
}

void writeValue(BinaryWriter &writer, const BreachMazeDecorationState &value)
{
    writeValue(writer, uint8_t(value.used));
    writeValue(writer, value.random);
}

bool readValue(BinaryReader &reader, BreachMazeDecorationState &value)
{
    return readBreachMazeFlag(reader, value.used) && readValue(reader, value.random);
}

void writeValue(BinaryWriter &writer, const BreachMazeInteractionState &value)
{
    writeValue(writer, value.version);
    writeValue(writer, value.identity);
    writeValue(writer, value.random);
    writeValue(writer, value.traps);
    for (bool used : value.bookcaseUsed) writeValue(writer, uint8_t(used));
    writeValue(writer, value.decorations);
}

bool readValue(BinaryReader &reader, BreachMazeInteractionState &value)
{
    if (!readValue(reader, value.version) || !readValue(reader, value.identity)
        || !readValue(reader, value.random) || !readValue(reader, value.traps)) return false;
    for (bool &used : value.bookcaseUsed)
    {
        if (!readBreachMazeFlag(reader, used)) return false;
    }
    return readValue(reader, value.decorations);
}

void writeValue(BinaryWriter &writer, const BreachMazeFloorState &value)
{
    writeValue(writer, value.descriptor);
    writeValue(writer, encodeBreachMazePopulationPlan(*value.population));
    writeValue(writer, value.runtime.scene);
    writeValue(writer, value.runtime.yawRadians);
    writeValue(writer, value.runtime.pitchRadians);
    writeValue(writer, value.runtime.actorRage);
    // This scene bookkeeping field was not part of the pre-94 generic Scene Snapshot codec.
    writeValue(writer, value.runtime.scene.lastPartyFloorFaceForPressurePlateTriggers);
    // Save95 appends the authoritative interaction record after the unchanged Save94 floor fields.
    writeValue(writer, uint8_t(value.runtime.interaction.has_value()));
    if (value.runtime.interaction) writeValue(writer, *value.runtime.interaction);
}

bool readValue(BinaryReader &reader, BreachMazeFloorState &value)
{
    uint64_t planBytes = 0;
    if (!readValue(reader, value.descriptor) || !readValue(reader, planBytes)
        || planBytes == 0 || planBytes > BreachMazePopulationPlanMaximumBytes
        || !canReadSerializedElementCount(reader, planBytes))
    {
        return false;
    }
    std::vector<uint8_t> encoded(static_cast<size_t>(planBytes));
    if (!reader.readBytes(encoded.data(), encoded.size())) return false;
    value.population = decodeBreachMazePopulationPlan(encoded);
    bool loaded = readValue(reader, value.runtime.scene)
        && readValue(reader, value.runtime.yawRadians)
        && readValue(reader, value.runtime.pitchRadians)
        && readValue(reader, value.runtime.actorRage)
        && readValue(reader, value.runtime.scene.lastPartyFloorFaceForPressurePlateTriggers);
    value.runtime.interaction.reset();
    if (loaded && reader.version() >= SaveVersionBreachMazeInteractions)
    {
        bool present = false;
        BreachMazeInteractionState interaction;
        loaded = readBreachMazeFlag(reader, present) && present && readValue(reader, interaction);
        if (loaded) value.runtime.interaction = std::move(interaction);
    }
    if (loaded && value.runtime.scene.eventRuntimeState)
    {
        // Event map names are derived runtime context, omitted by the shared scene codec.
        // A maze floor always belongs to BrBase, so restore that context before validation.
        value.runtime.scene.eventRuntimeState->mapFileName = "brbase.blv";
    }
    return loaded;
}

void writeValue(BinaryWriter &writer, const BreachMazeRunData &value)
{
    writeValue(writer, value.version);
    writeValue(writer, value.seedVersion);
    writeValue(writer, value.templateFingerprint);
    writeValue(writer, value.revision);
    writeValue(writer, value.rootSeed);
    writeValue(writer, value.epoch);
    writeValue(writer, value.nextGenerationSerial);
    writeValue(writer, value.currentLevel);
    writeValue(writer, value.inside);
    writeValue(writer, value.direction);
    writeValue(writer, static_cast<uint64_t>(value.floors.size()));
    for (const auto &[level, floor] : value.floors)
    {
        writeValue(writer, level);
        writeValue(writer, floor);
    }
}

bool readValue(BinaryReader &reader, BreachMazeRunData &value)
{
    uint64_t floorCount = 0;
    const uint32_t expectedVersion = reader.version() == SaveVersionBreachMaze
        ? BreachMazeLegacyPlayableStateVersion : BreachMazePlayableStateVersion;
    if (!readValue(reader, value.version) || value.version != expectedVersion
        || !readValue(reader, value.seedVersion) || !readValue(reader, value.templateFingerprint)
        || !readValue(reader, value.revision) || !readValue(reader, value.rootSeed)
        || !readValue(reader, value.epoch) || !readValue(reader, value.nextGenerationSerial)
        || !readValue(reader, value.currentLevel) || !readValue(reader, value.inside)
        || !readValue(reader, value.direction) || !readValue(reader, floorCount) || floorCount > 3)
    {
        return false;
    }
    value.floors.clear();
    uint32_t previousLevel = 0;
    for (uint64_t index = 0; index < floorCount; ++index)
    {
        uint32_t level = 0;
        BreachMazeFloorState floor;
        if (!readValue(reader, level) || level <= previousLevel || level > BreachMazeMaximumPlannedLevel
            || !readValue(reader, floor) || floor.descriptor.identity.level != level)
        {
            return false;
        }
        previousLevel = level;
        value.floors.emplace(level, std::move(floor));
    }
    std::string error;
    if (reader.version() == SaveVersionBreachMaze)
    {
        // Save94 never ran live maze interactions. This is the sole decode path allowed to
        // initialize them from frozen plans; Save95 must supply every actual mutable record.
        return migrateBreachMazeV2Interactions(value, error);
    }
    return validateBreachMazePlayableData(value, error);
}

void writeValue(BinaryWriter &writer, const TurnBasedCombatRuntime::QueueEntry &value)
{
    writeValue(writer, value.kind);
    writeValue(writer, static_cast<uint64_t>(value.id));
    writeValue(writer, value.initiative);
    writeValue(writer, value.actionRecoverySeconds);
}

bool readValue(BinaryReader &reader, TurnBasedCombatRuntime::QueueEntry &value)
{
    uint64_t id = 0;
    if (!readValue(reader, value.kind) || !readValue(reader, id) || id > std::numeric_limits<size_t>::max()
        || !readValue(reader, value.initiative) || !readValue(reader, value.actionRecoverySeconds))
    {
        return false;
    }
    value.id = static_cast<size_t>(id);
    return true;
}

void writeValue(BinaryWriter &writer, const TurnBasedCombatRuntime::Snapshot &value)
{
    writeValue(writer, value.active);
    writeValue(writer, value.stage);
    writeValue(writer, value.queue);
    writeValue(writer, value.turnInitiative);
    writeValue(writer, value.aiTurnTimerTicks);
    writeValue(writer, value.movementActionPoints);
    writeValue(writer, value.pendingActions);
    writeValue(writer, value.turnCount);
    writeValue(writer, value.actorActionTimerTicks);
    writeValue(writer, value.movementFinished);
    writeValue(writer, value.waitingForActorActionResolution);
    writeValue(writer, value.pendingGameTimeAdvanceMinutes);
    for (float recovery : value.memberTurnRecoverySeconds)
    {
        writeValue(writer, recovery);
    }
}

bool readValue(BinaryReader &reader, TurnBasedCombatRuntime::Snapshot &value)
{
    if (!readValue(reader, value.active)
        || !readValue(reader, value.stage)
        || !readValue(reader, value.queue)
        || !readValue(reader, value.turnInitiative)
        || !readValue(reader, value.aiTurnTimerTicks)
        || !readValue(reader, value.movementActionPoints)
        || !readValue(reader, value.pendingActions)
        || !readValue(reader, value.turnCount)
        || !readValue(reader, value.actorActionTimerTicks)
        || !readValue(reader, value.movementFinished)
        || !readValue(reader, value.waitingForActorActionResolution)
        || !readValue(reader, value.pendingGameTimeAdvanceMinutes))
    {
        return false;
    }
    for (float &recovery : value.memberTurnRecoverySeconds)
    {
        if (!readValue(reader, recovery))
        {
            return false;
        }
    }
    return TurnBasedCombatRuntime::isValidSnapshot(value);
}

bool validateSavedOutdoorLightTimes(const GameSaveData &data, std::string &error)
{
    const auto validate = [&error](const std::optional<EventRuntimeState> &state)
    {
        if (!state || !state->outdoorLightTimeOverride
            || validOutdoorLightTimeOverride(*state->outdoorLightTimeOverride)) return true;
        error = "Invalid saved outdoor light hour or minute";
        return false;
    };
    if (!validate(data.outdoorWorld.eventRuntimeState) || !validate(data.indoorScene.eventRuntimeState)) return false;
    for (const auto &[map, world] : data.outdoorWorldStates)
    {
        if (!validate(world.eventRuntimeState)) return false;
    }
    for (const auto &[map, scene] : data.indoorSceneStates)
    {
        if (!validate(scene.eventRuntimeState)) return false;
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors)
        {
            if (!validate(floor.runtime.scene.eventRuntimeState)) return false;
        }
    }
    return true;
}

bool validateSavedPartyMotion(const GameSaveData &data, std::string &error)
{
    const auto validate = [&error](const auto &snapshot, const std::string &context)
    {
        if (validSavedPartyMotion(snapshot)) return true;
        error = "Invalid Party jump state, vertical speed or movement accumulator in " + context;
        return false;
    };
    if (!validate(data.outdoorParty, "outdoor runtime")
        || !validate(data.indoorScene.partyRuntime, "indoor runtime")) return false;
    for (const auto &[map, scene] : data.indoorSceneStates)
    {
        if (!validate(scene.partyRuntime, "cached indoor map " + map)) return false;
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors)
        {
            if (!validate(floor.runtime.scene.partyRuntime, "maze floor " + std::to_string(level))) return false;
        }
    }
    return true;
}

bool validateSavedTurnBasedCombat(const GameSaveData &data, std::string &error)
{
    const TurnBasedCombatRuntime::Snapshot &turn = data.turnBasedCombat;
    if (!TurnBasedCombatRuntime::isValidSnapshot(turn))
    {
        error = "invalid turn-based combat state";
        return false;
    }
    if (!turn.active)
    {
        return true;
    }
    const bool outdoor = data.currentSceneKind == SceneKind::Outdoor && data.hasOutdoorRuntimeState;
    const IndoorSceneRuntime::Snapshot *pIndoor = nullptr;
    if (data.currentSceneKind == SceneKind::Indoor)
    {
        if (data.breachMaze && data.breachMaze->inside)
        {
            const auto floor = data.breachMaze->floors.find(data.breachMaze->currentLevel);
            if (floor != data.breachMaze->floors.end()) pIndoor = &floor->second.runtime.scene;
        }
        else if (data.hasIndoorSceneState)
        {
            pIndoor = &data.indoorScene;
        }
    }
    if (!outdoor && pIndoor == nullptr)
    {
        error = "turn-based combat has no saved world";
        return false;
    }
    const size_t actorCount = outdoor ? data.outdoorWorld.mapActors.size()
        : pIndoor->worldRuntime.mapActorAiStates.size();
    for (const TurnBasedCombatRuntime::QueueEntry &entry : turn.queue)
    {
        const size_t count = entry.kind == TurnBasedCombatRuntime::QueueEntry::Kind::Character
            ? data.party.members.size() : actorCount;
        if (entry.id >= count)
        {
            error = "turn-based queue references an unavailable combatant";
            return false;
        }
    }
    const auto &projectiles = outdoor ? data.outdoorWorld.projectiles
        : pIndoor->worldRuntime.projectileState.projectiles;
    const size_t pending = std::count_if(projectiles.begin(), projectiles.end(), [](const auto &projectile)
    {
        return projectile.turnBasedPendingAction && !projectile.isExpired && !projectile.isSettled;
    });
    if (pending != static_cast<size_t>(turn.pendingActions))
    {
        error = "turn-based pending actions do not match saved projectiles";
        return false;
    }
    return true;
}

void writeValue(BinaryWriter &writer, const BreachAlvarCompanionState &value)
{
    const BreachAlvarCompanionReceipt &receipt = value.receipt;
    writeValue(writer, receipt.profileVersion);
    writeValue(writer, receipt.planVersion);
    writeValue(writer, receipt.seed);
    writeValue(writer, receipt.finalRandomState);
    writeValue(writer, receipt.randomDrawCount);
    writeValue(writer, receipt.leaderMemberId);
    for (const BreachAlvarCompanionReceiptEntry &entry : receipt.companions)
    {
        writeValue(writer, entry.memberId);
        writeValue(writer, entry.returned);
    }
    writeValue(writer, value.friendActor.canonicalMapKey);
    writeValue(writer, value.friendActor.actorId);
    writeValue(writer, value.friendActor.spawnMonsterId);
    writeValue(writer, receipt.initialActiveCount);
    writeValue(writer, receipt.leaderLevelAtSplit);
    for (const BreachAlvarCompanionReceiptEntry &entry : receipt.companions)
    {
        writeValue(writer, entry.generation.has_value());
        if (!entry.generation) continue;
        writeValue(writer, entry.generation->request.rosterId);
        writeValue(writer, entry.generation->request.level);
        writeValue(writer, entry.generation->request.seed);
        writeValue(writer, entry.generation->finalRandomState);
        writeValue(writer, entry.generation->randomDrawCount);
    }
}

bool readValue(BinaryReader &reader, BreachAlvarCompanionState &value)
{
    BreachAlvarCompanionReceipt &receipt = value.receipt;
    if (!readValue(reader, receipt.profileVersion)
        || !readValue(reader, receipt.planVersion)
        || !readValue(reader, receipt.seed)
        || !readValue(reader, receipt.finalRandomState)
        || !readValue(reader, receipt.randomDrawCount)
        || !readValue(reader, receipt.leaderMemberId))
    {
        return false;
    }
    for (BreachAlvarCompanionReceiptEntry &entry : receipt.companions)
    {
        if (!readValue(reader, entry.memberId) || !readValue(reader, entry.returned))
        {
            return false;
        }
    }
    if (!readValue(reader, value.friendActor.canonicalMapKey)
        || !readValue(reader, value.friendActor.actorId)
        || !readValue(reader, value.friendActor.spawnMonsterId)) return false;
    if (reader.version() < SaveVersionBreachAlvarGeneration)
    {
        // Save91–95 admitted only the existing-five-member plan, with exactly one bound-four draw.
        if (receipt.profileVersion != 1 || receipt.planVersion != BreachAlvarSplitPlanVersion
            || receipt.randomDrawCount != 1
            || receipt.finalRandomState != receipt.seed + 0x9e3779b97f4a7c15ULL) return false;
        receipt.profileVersion = BreachAlvarCompanionProfileVersion;
        receipt.initialActiveCount = 5;
        receipt.leaderLevelAtSplit = 0;
        return true;
    }
    if (receipt.profileVersion != BreachAlvarCompanionProfileVersion
        || !readValue(reader, receipt.initialActiveCount)
        || !readValue(reader, receipt.leaderLevelAtSplit)) return false;
    for (BreachAlvarCompanionReceiptEntry &entry : receipt.companions)
    {
        bool generated = false;
        if (!readValue(reader, generated)) return false;
        if (!generated) continue;
        BreachAlvarCompanionGenerationReceipt generation;
        if (!readValue(reader, generation.request.rosterId) || !readValue(reader, generation.request.level)
            || !readValue(reader, generation.request.seed) || !readValue(reader, generation.finalRandomState)
            || !readValue(reader, generation.randomDrawCount)) return false;
        entry.generation = generation;
    }
    return true;
}

void writeValue(BinaryWriter &writer, const GameSaveData &value)
{
    writeValue(writer, value.currentSceneKind);
    writeValue(writer, value.mapFileName);
    writeValue(writer, value.party);
    writeValue(writer, value.namedGlobalVars);
    writeValue(writer, value.hasOutdoorRuntimeState);
    writeValue(writer, value.outdoorParty);
    writeValue(writer, value.outdoorWorld);
    writeValue(writer, value.outdoorWorldStates);
    writeValue(writer, value.hasIndoorSceneState);
    writeValue(writer, value.indoorScene);
    writeValue(writer, value.indoorSceneStates);
    writeValue(writer, value.savedGameMinutes);
    writeValue(writer, value.outdoorCameraYawRadians);
    writeValue(writer, value.outdoorCameraPitchRadians);
    writeValue(writer, value.indoorCameraYawRadians);
    writeValue(writer, value.indoorCameraPitchRadians);
    writeValue(writer, value.heldInventoryItemActive);
    writeValue(writer, value.heldInventoryItem);
    writeValue(writer, value.heldInventoryItemGrabCellOffsetX);
    writeValue(writer, value.heldInventoryItemGrabCellOffsetY);
    writeValue(writer, value.heldInventoryItemGrabOffsetX);
    writeValue(writer, value.heldInventoryItemGrabOffsetY);
    writeValue(writer, value.requiredContentPackages);
    writeValue(writer, value.saveName);
    writeValue(writer, value.previewBmp);
    writeValue(writer, value.turnBasedCombat);
    writeValue(writer, value.breachAlvarCompanions);
    writeValue(writer, value.breachMaze);
}

bool readValue(BinaryReader &reader, GameSaveData &value)
{
    return readValue(reader, value.currentSceneKind)
        && readValue(reader, value.mapFileName)
        && readValue(reader, value.party)
        && (reader.version() < SaveVersionSessionNamedGlobalVars || readValue(reader, value.namedGlobalVars))
        && readValue(reader, value.hasOutdoorRuntimeState)
        && readValue(reader, value.outdoorParty)
        && readValue(reader, value.outdoorWorld)
        && readValue(reader, value.outdoorWorldStates)
        && readValue(reader, value.hasIndoorSceneState)
        && readValue(reader, value.indoorScene)
        && readValue(reader, value.indoorSceneStates)
        && readGameMinutes(reader, value.savedGameMinutes)
        && readValue(reader, value.outdoorCameraYawRadians)
        && readValue(reader, value.outdoorCameraPitchRadians)
        && (reader.version() < SaveVersionIndoorCamera || readValue(reader, value.indoorCameraYawRadians))
        && (reader.version() < SaveVersionIndoorCamera || readValue(reader, value.indoorCameraPitchRadians))
        && (reader.version() < SaveVersionHeldCursorItem || readValue(reader, value.heldInventoryItemActive))
        && (reader.version() < SaveVersionHeldCursorItem || readValue(reader, value.heldInventoryItem))
        && (reader.version() < SaveVersionHeldCursorItem
            || readValue(reader, value.heldInventoryItemGrabCellOffsetX))
        && (reader.version() < SaveVersionHeldCursorItem
            || readValue(reader, value.heldInventoryItemGrabCellOffsetY))
        && (reader.version() < SaveVersionHeldCursorItem || readValue(reader, value.heldInventoryItemGrabOffsetX))
        && (reader.version() < SaveVersionHeldCursorItem || readValue(reader, value.heldInventoryItemGrabOffsetY))
        && (reader.version() < SaveVersionRequiredContentPackages
            || readValue(reader, value.requiredContentPackages))
        && readValue(reader, value.saveName)
        && readValue(reader, value.previewBmp)
        && (reader.version() < SaveVersionTurnBasedCombat || readValue(reader, value.turnBasedCombat))
        && (reader.version() < SaveVersionBreachAlvarCompanions || readValue(reader, value.breachAlvarCompanions))
        && (reader.version() < SaveVersionBreachMaze || readValue(reader, value.breachMaze));
}
}

namespace
{
void migrateLegacyIndoorCorpseMarkers(IndoorSceneRuntime::Snapshot &snapshot)
{
    if (!snapshot.mapDeltaData)
    {
        return;
    }

    const std::vector<MapDeltaActor> &actors = snapshot.mapDeltaData->actors;
    std::vector<std::optional<GameplayCorpseViewState>> &corpseViews =
        snapshot.worldRuntime.mapActorCorpseViews;
    corpseViews.resize(std::max(corpseViews.size(), actors.size()));
    const uint32_t invisibleMask = static_cast<uint32_t>(EvtActorAttribute::Invisible);

    for (size_t actorIndex = 0; actorIndex < actors.size(); ++actorIndex)
    {
        const MapDeltaActor &actor = actors[actorIndex];

        if (actor.hp <= 0 && (actor.attributes & invisibleMask) != 0 && !corpseViews[actorIndex].has_value())
        {
            GameplayCorpseViewState consumedCorpse = {};
            consumedCorpse.sourceIndex = static_cast<uint32_t>(actorIndex);
            corpseViews[actorIndex] = std::move(consumedCorpse);
        }
    }
}

void migrateLegacyOutdoorCorpseMarkers(OutdoorWorldRuntime::Snapshot &snapshot)
{
    std::vector<std::optional<GameplayCorpseViewState>> &corpseViews = snapshot.mapActorCorpseViews;
    corpseViews.resize(std::max(corpseViews.size(), snapshot.mapActors.size()));

    for (size_t actorIndex = 0; actorIndex < snapshot.mapActors.size(); ++actorIndex)
    {
        const OutdoorWorldRuntime::MapActorState &actor = snapshot.mapActors[actorIndex];
        const bool terminalCorpse = actor.isDead
            || actor.currentHp <= 0
            || actor.aiState == OutdoorWorldRuntime::ActorAiState::Dying
            || actor.aiState == OutdoorWorldRuntime::ActorAiState::Dead;

        if (terminalCorpse && actor.isInvisible && !corpseViews[actorIndex].has_value())
        {
            GameplayCorpseViewState consumedCorpse = {};
            consumedCorpse.sourceIndex = static_cast<uint32_t>(actorIndex);
            corpseViews[actorIndex] = std::move(consumedCorpse);
        }
    }
}

std::vector<const GameplayMergeResolvedActorCombat *> savedResolvedActorCombat(const GameSaveData &data)
{
    std::vector<const GameplayMergeResolvedActorCombat *> result;
    const auto appendActors = [&result](const auto &actors)
    {
        for (const auto &actor : actors)
        {
            if (const GameplayMergeResolvedActorCombat *pResolved = actor.combatState.resolved())
            {
                result.push_back(pResolved);
            }
        }
    };
    if (data.hasOutdoorRuntimeState)
    {
        appendActors(data.outdoorWorld.mapActors);
    }
    for (const auto &[map, world] : data.outdoorWorldStates)
    {
        appendActors(world.mapActors);
    }
    if (data.hasIndoorSceneState)
    {
        appendActors(data.indoorScene.worldRuntime.mapActorAiStates);
    }
    for (const auto &[map, scene] : data.indoorSceneStates)
    {
        appendActors(scene.worldRuntime.mapActorAiStates);
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors)
        {
            appendActors(floor.runtime.scene.worldRuntime.mapActorAiStates);
        }
    }
    return result;
}

std::vector<const MergeMonsterGenerationState *> savedMonsterGenerations(const GameSaveData &data)
{
    std::vector<const MergeMonsterGenerationState *> result;
    const auto appendScene = [&result](const IndoorSceneRuntime::Snapshot &scene)
    {
        if (scene.worldRuntime.mergeMonsterGeneration)
        {
            result.push_back(&*scene.worldRuntime.mergeMonsterGeneration);
        }
    };
    if (data.hasIndoorSceneState)
    {
        appendScene(data.indoorScene);
    }
    for (const auto &[map, scene] : data.indoorSceneStates)
    {
        appendScene(scene);
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors) appendScene(floor.runtime.scene);
    }
    return result;
}

std::vector<uint32_t> persistentSaveItemIds(const GameSaveData &data)
{
    std::vector<uint32_t> itemIds;
    const auto appendInventoryItem = [&itemIds](const InventoryItem &item)
    {
        if (item.objectDescriptionId != 0)
        {
            itemIds.push_back(item.objectDescriptionId);
        }
    };
    const auto appendChestView = [&appendInventoryItem](const GameplayChestViewState &view)
    {
        for (const GameplayChestItemState &item : view.items)
        {
            appendInventoryItem(item.item);
        }
        for (const GameplayChestItemState &item : view.hiddenItems)
        {
            appendInventoryItem(item.item);
        }
    };
    const auto appendCorpseView = [&appendInventoryItem](const GameplayCorpseViewState &view)
    {
        for (const GameplayChestItemState &item : view.items)
        {
            appendInventoryItem(item.item);
        }
    };
    const auto appendCharacterItems = [&itemIds](const Character &member)
    {
        for (const InventoryItem &item : member.inventory)
        {
            if (item.objectDescriptionId != 0)
            {
                itemIds.push_back(item.objectDescriptionId);
            }
        }

        const CharacterEquipment &equipment = member.equipment;
        for (uint32_t itemId : {
                 equipment.offHand,
                 equipment.mainHand,
                 equipment.bow,
                 equipment.armor,
                 equipment.helm,
                 equipment.belt,
                 equipment.cloak,
                 equipment.gauntlets,
                 equipment.boots,
                 equipment.amulet,
                 equipment.ring1,
                 equipment.ring2,
                 equipment.ring3,
                 equipment.ring4,
                 equipment.ring5,
                 equipment.ring6})
        {
            if (itemId != 0)
            {
                itemIds.push_back(itemId);
            }
        }
    };

    for (const Character &member : data.party.members)
    {
        appendCharacterItems(member);
    }
    for (const Party::PendingItemDelivery &delivery : data.party.pendingItemDeliveries)
    {
        appendInventoryItem(delivery.item);
    }

    for (const AdventurersInnMember &member : data.party.adventurersInnMembers)
    {
        appendCharacterItems(member.character);
    }
    for (const Party::DetachedPartyMember &member : data.party.detachedMembers)
    {
        appendCharacterItems(member.character);
        for (const Party::PendingItemDelivery &delivery : member.pendingItemDeliveries)
        {
            appendInventoryItem(delivery.item);
        }
    }

    for (const Party::HouseStockState &state : data.party.houseStockStates)
    {
        for (const InventoryItem &item : state.standardStock)
        {
            appendInventoryItem(item);
        }
        for (const InventoryItem &item : state.specialStock)
        {
            appendInventoryItem(item);
        }
        for (const InventoryItem &item : state.spellbookStock)
        {
            appendInventoryItem(item);
        }
    }

    if (data.heldInventoryItemActive && data.heldInventoryItem.objectDescriptionId != 0)
    {
        itemIds.push_back(data.heldInventoryItem.objectDescriptionId);
    }

    const auto appendOutdoorWorld = [&](const OutdoorWorldRuntime::Snapshot &snapshot)
    {
        for (const std::optional<GameplayChestViewState> &view : snapshot.materializedChestViews)
        {
            if (view)
            {
                appendChestView(*view);
            }
        }
        if (snapshot.activeChestView)
        {
            appendChestView(*snapshot.activeChestView);
        }
        for (const std::optional<GameplayCorpseViewState> &view : snapshot.mapActorCorpseViews)
        {
            if (view)
            {
                appendCorpseView(*view);
            }
        }
        if (snapshot.activeCorpseView)
        {
            appendCorpseView(*snapshot.activeCorpseView);
        }
        for (const OutdoorWorldRuntime::WorldItemState &item : snapshot.worldItems)
        {
            appendInventoryItem(item.item);
        }
    };
    const auto appendIndoorWorld = [&](const IndoorWorldRuntime::Snapshot &snapshot)
    {
        for (const IndoorWorldRuntime::MapActorAiState &actor : snapshot.mapActorAiStates)
        {
            for (const InventoryItem &item : actor.stolenItems)
            {
                appendInventoryItem(item);
            }
        }
        for (const std::optional<GameplayChestViewState> &view : snapshot.materializedChestViews)
        {
            if (view)
            {
                appendChestView(*view);
            }
        }
        if (snapshot.activeChestView)
        {
            appendChestView(*snapshot.activeChestView);
        }
        for (const std::optional<GameplayCorpseViewState> &view : snapshot.mapActorCorpseViews)
        {
            if (view)
            {
                appendCorpseView(*view);
            }
        }
        if (snapshot.activeCorpseView)
        {
            appendCorpseView(*snapshot.activeCorpseView);
        }
    };

    const auto appendIndoorScene = [&](const IndoorSceneRuntime::Snapshot &snapshot)
    {
        appendIndoorWorld(snapshot.worldRuntime);
        if (snapshot.mapDeltaData)
        {
            for (const MapDeltaSpriteObject &object : snapshot.mapDeltaData->spriteObjects)
            {
                if (object.containingItem)
                {
                    appendInventoryItem(*object.containingItem);
                }
            }
        }
    };

    if (data.hasOutdoorRuntimeState)
    {
        appendOutdoorWorld(data.outdoorWorld);
    }
    for (const auto &[mapName, snapshot] : data.outdoorWorldStates)
    {
        (void)mapName;
        appendOutdoorWorld(snapshot);
    }
    if (data.hasIndoorSceneState)
    {
        appendIndoorScene(data.indoorScene);
    }
    for (const auto &[mapName, snapshot] : data.indoorSceneStates)
    {
        (void)mapName;
        appendIndoorScene(snapshot);
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors) appendIndoorScene(floor.runtime.scene);
    }

    std::sort(itemIds.begin(), itemIds.end());
    itemIds.erase(std::unique(itemIds.begin(), itemIds.end()), itemIds.end());
    return itemIds;
}
}

std::vector<uint8_t> encodePartySnapshotForComparison(const Party::Snapshot &snapshot)
{
    BinaryWriter writer(true);
    writeValue(writer, snapshot);
    return writer.bytes();
}

std::vector<uint8_t> encodeInventoryItemForComparison(const InventoryItem &item)
{
    BinaryWriter writer;
    writeValue(writer, item);
    return writer.bytes();
}

void migrateLegacyConsumedCorpseMarkers(GameSaveData &data, uint32_t sourceSaveVersion)
{
    if (sourceSaveVersion >= SaveVersionConsumedCorpseMarkers)
    {
        return;
    }

    if (data.hasOutdoorRuntimeState)
    {
        migrateLegacyOutdoorCorpseMarkers(data.outdoorWorld);
    }

    for (auto &[mapName, snapshot] : data.outdoorWorldStates)
    {
        (void)mapName;
        migrateLegacyOutdoorCorpseMarkers(snapshot);
    }

    if (data.hasIndoorSceneState)
    {
        migrateLegacyIndoorCorpseMarkers(data.indoorScene);
    }

    for (auto &[mapName, snapshot] : data.indoorSceneStates)
    {
        (void)mapName;
        migrateLegacyIndoorCorpseMarkers(snapshot);
    }
}

std::unordered_map<std::string, uint32_t> collectRequiredContentPackages(
    const GameSaveData &data,
    const ItemTable &itemTable,
    const HouseTable &houseTable,
    const std::unordered_map<std::string, uint32_t> &loadedPackageSchemas)
{
    std::unordered_map<std::string, uint32_t> result;

    for (uint32_t itemId : persistentSaveItemIds(data))
    {
        const ItemDefinition *pDefinition = itemTable.get(itemId);

        if (pDefinition == nullptr || pDefinition->packageId.empty() || pDefinition->packageId == "engine")
        {
            continue;
        }

        const auto foundSchema = loadedPackageSchemas.find(pDefinition->packageId);
        result[pDefinition->packageId] = foundSchema != loadedPackageSchemas.end() ? foundSchema->second : 0;
    }

    for (const Party::HouseStockState &state : data.party.houseStockStates)
    {
        const HouseEntry *pHouseEntry = houseTable.get(state.houseId);
        if (state.generationVersion == 0
            || pHouseEntry == nullptr
            || pHouseEntry->packageId.empty()
            || pHouseEntry->packageId == "engine")
        {
            continue;
        }
        const auto foundSchema = loadedPackageSchemas.find(pHouseEntry->packageId);
        result[pHouseEntry->packageId] = foundSchema != loadedPackageSchemas.end() ? foundSchema->second : 0;
    }

    for (const GameplayMergeResolvedActorCombat *pResolved : savedResolvedActorCombat(data))
    {
        const auto [position, inserted] = result.emplace(pResolved->contentPackageId, pResolved->contentPackageSchema);
        if (!inserted && position->second != pResolved->contentPackageSchema)
        {
            position->second = 0; // Conflicting saved dependencies must fail validation, not pick one.
        }
    }
    for (const MergeMonsterGenerationState *pGeneration : savedMonsterGenerations(data))
    {
        const auto [position, inserted] = result.emplace(
            pGeneration->contentPackageId, pGeneration->contentPackageSchema);
        if (!inserted && position->second != pGeneration->contentPackageSchema)
        {
            position->second = 0;
        }
    }
    return result;
}

void migrateLegacyPartyMemberIdentities(GameSaveData &data, uint32_t sourceSaveVersion)
{
    if (sourceSaveVersion < SaveVersionDetachedPartyMembers)
    {
        Party::initializeLegacyMemberIdentities(data.party);
    }
}

bool validateRequiredContentPackages(
    const GameSaveData &data,
    const ItemTable &itemTable,
    const HouseTable &houseTable,
    const std::unordered_map<std::string, uint32_t> &loadedPackageSchemas,
    std::string &error)
{
    error.clear();

    for (const auto &[packageId, requiredSchema] : data.requiredContentPackages)
    {
        const auto foundPackage = loadedPackageSchemas.find(packageId);

        if (foundPackage == loadedPackageSchemas.end())
        {
            error = "save requires missing content package '" + packageId + "'";
            return false;
        }

        if (requiredSchema == 0 || foundPackage->second != requiredSchema)
        {
            error = "save requires content package '" + packageId + "' schema "
                + std::to_string(requiredSchema) + ", loaded schema is " + std::to_string(foundPackage->second);
            return false;
        }
    }

    for (const GameplayMergeResolvedActorCombat *pResolved : savedResolvedActorCombat(data))
    {
        const auto dependency = data.requiredContentPackages.find(pResolved->contentPackageId);
        if (dependency == data.requiredContentPackages.end() || dependency->second != pResolved->contentPackageSchema)
        {
            error = "save is missing the resolved actor's exact content package dependency";
            return false;
        }
    }
    for (const MergeMonsterGenerationState *pGeneration : savedMonsterGenerations(data))
    {
        const auto dependency = data.requiredContentPackages.find(pGeneration->contentPackageId);
        if (dependency == data.requiredContentPackages.end() || dependency->second != pGeneration->contentPackageSchema)
        {
            error = "save is missing the monster generation's exact content package dependency";
            return false;
        }
    }

    for (uint32_t itemId : persistentSaveItemIds(data))
    {
        const ItemDefinition *pDefinition = itemTable.get(itemId);

        if (pDefinition == nullptr)
        {
            error = "save references missing item definition " + std::to_string(itemId);
            return false;
        }

        if (pDefinition->packageId != "engine"
            && !data.requiredContentPackages.contains(pDefinition->packageId))
        {
            error = "save item " + std::to_string(itemId) + " is missing required package metadata for '"
                + pDefinition->packageId + "'";
            return false;
        }
    }

    for (const Party::HouseStockState &state : data.party.houseStockStates)
    {
        if (state.generationVersion == 0)
        {
            continue;
        }
        const HouseEntry *pHouseEntry = houseTable.get(state.houseId);
        if (pHouseEntry == nullptr || pHouseEntry->vendorStockProfile == VendorStockProfile::None)
        {
            error = "save references missing vendor definition " + std::to_string(state.houseId);
            return false;
        }
        if (pHouseEntry->packageId != "engine"
            && !data.requiredContentPackages.contains(pHouseEntry->packageId))
        {
            error = "save vendor " + std::to_string(state.houseId)
                + " is missing required package metadata for '" + pHouseEntry->packageId + "'";
            return false;
        }
    }

    return true;
}

bool validateSavedBreachMaze(const GameSaveData &data, std::string &error)
{
    // Legacy inspection snapshots remain readable; map admission is a separate loaded-content check.
    if (!data.breachMaze)
    {
        return true;
    }
    const auto isMazeMap = [](const std::string &map)
    {
        return normalizeMapFileStem(map) == "brbase"
            || toLowerCopy(map) == "world.mmmerge.map.brbase";
    };
    const bool activeMaze = data.breachMaze->inside;
    if (isMazeMap(data.mapFileName) != activeMaze
        || (activeMaze && (data.currentSceneKind != SceneKind::Indoor || data.hasIndoorSceneState)))
    {
        error = "Active brbase requires its sole scene in an active playable maze floor";
        return false;
    }
    if (data.indoorScene.eventRuntimeState && isMazeMap(data.indoorScene.eventRuntimeState->mapFileName))
    {
        error = "Maze scene cannot also be stored in the generic indoor scene";
        return false;
    }
    for (const auto &[map, scene] : data.indoorSceneStates)
    {
        if (isMazeMap(map) || (scene.eventRuntimeState && isMazeMap(scene.eventRuntimeState->mapFileName)))
        {
            error = "Maze floors cannot also be stored in the generic indoor map cache";
            return false;
        }
    }
    if (!validateBreachMazePlayableData(*data.breachMaze, error))
    {
        return false;
    }
    if (activeMaze)
    {
        BinaryWriter actual(true);
        BinaryWriter empty(true);
        writeValue(actual, data.indoorScene);
        writeValue(empty, IndoorSceneRuntime::Snapshot{});
        if (actual.bytes() != empty.bytes() || data.indoorScene.lastPartyFloorFaceForPressurePlateTriggers)
        {
            error = "Active maze save contains duplicate generic indoor scene data";
            return false;
        }
    }
    for (const auto &[level, floor] : data.breachMaze->floors)
    {
        const IndoorSceneRuntime::Snapshot &scene = floor.runtime.scene;
        if (!validGameMinutes(scene.worldRuntime.gameMinutes))
        {
            error = "Maze floor has an invalid authoritative game clock";
            return false;
        }
        if (scene.eventRuntimeState)
        {
            if (!isMazeMap(scene.eventRuntimeState->mapFileName))
            {
                error = "Maze floor contains event state from another map";
                return false;
            }
            if (!validateNamedGameTimes(scene.eventRuntimeState->namedGlobalVars, error)) return false;
        }
    }
    return true;
}

bool validateSavedBreachAlvarCompanions(
    const GameSaveData &data, std::string &error, const BreachAlvarFriendActorRules *pRules)
{
    if (!validBreachAlvarCompanionState(data.breachAlvarCompanions, data.party, error))
    {
        return false;
    }
    constexpr const char *splitVariable = "MMerge.CrossContinents.BreachSplit";
    const auto validSplitFlag = [&](const auto &variables, bool required)
    {
        const auto found = variables.find(splitVariable);
        const int32_t value = found != variables.end() ? found->second : 0;
        if ((!data.breachAlvarCompanions && value != 0)
            || (data.breachAlvarCompanions && ((required && value != 1)
                || (found != variables.end() && value != 1))))
        {
            error = "Breach split flag has no matching authoritative companion state";
            return false;
        }
        if (data.breachAlvarCompanions)
        {
            for (size_t index = 0; index < 4; ++index)
            {
                const std::string key = "MMerge.CrossContinents.FQMercRnd." + std::to_string(772 + index);
                const auto generated = variables.find(key);
                const int32_t expected = data.breachAlvarCompanions->receipt.companions[index].generation ? 1 : 0;
                if ((required && expected == 1 && generated == variables.end())
                    || (generated != variables.end() && generated->second != expected))
                {
                    error = "Breach generated companion flag disagrees with its generation receipt";
                    return false;
                }
            }
        }
        return true;
    };
    if (!validSplitFlag(data.namedGlobalVars, true))
    {
        return false;
    }
    const auto finished = data.namedGlobalVars.find("MMerge.CrossContinents.QuestFinished");
    if (data.breachAlvarCompanions && data.breachAlvarCompanions->receipt.initialActiveCount > 1
        && !data.party.questBits.contains(1714)
        && (finished == data.namedGlobalVars.end() || finished->second != 1))
    {
        error = "Breach companion state is missing its split quest bit";
        return false;
    }

    bool foundFriendWorld = false;
    std::optional<std::vector<uint8_t>> canonicalFriendWorld;
    const std::string friendCanonicalId = buildCanonicalMapId("mmmerge", "bralvar.odm");
    const auto validateWorld = [&](const OutdoorWorldRuntime::Snapshot &snapshot, const std::string &mapKey)
    {
        const std::string normalized = toLowerCopy(mapKey);
        const bool friendMap = normalized == "bralvar.odm" || normalized == BreachAlvarCompanionMap
            || normalized == friendCanonicalId;
        if (snapshot.eventRuntimeState
            && !validSplitFlag(snapshot.eventRuntimeState->namedGlobalVars, false))
        {
            return false;
        }
        const bool markedActor = std::any_of(snapshot.mapActors.begin(), snapshot.mapActors.end(),
            [](const OutdoorWorldRuntime::MapActorState &actor)
            {
                return actor.breachAlvarCompanion || actor.breachAlvarCompanionRemoved
                    || actor.breachAlvarNextMaintenanceGameMinute != 0.0;
            });
        if (markedActor && (!friendMap || !data.breachAlvarCompanions))
        {
            error = "Breach friend actor has no matching map or companion state";
            return false;
        }
        if (!friendMap || !data.breachAlvarCompanions)
        {
            return true;
        }
        const auto &state = *data.breachAlvarCompanions;
        if (!OutdoorWorldRuntime::validateBreachAlvarFriendActorBinding(
                snapshot, state.friendActor, state.receipt.companions[0].returned, error, pRules))
        {
            return false;
        }
        const auto actor = std::find_if(snapshot.mapActors.begin(), snapshot.mapActors.end(),
            [&](const auto &entry) { return entry.actorId == state.friendActor.actorId; });
        if (actor == snapshot.mapActors.end() || actor->npcId != 772)
        {
            error = "Breach friend actor does not represent the first companion receipt slot";
            return false;
        }
        BinaryWriter canonical(true);
        writeValue(canonical, snapshot);
        if (canonicalFriendWorld && *canonicalFriendWorld != canonical.bytes())
        {
            error = "Breach friend world aliases contain conflicting persistent state";
            return false;
        }
        if (!canonicalFriendWorld)
        {
            canonicalFriendWorld = canonical.bytes();
        }
        foundFriendWorld = true;
        return true;
    };
    if (data.hasOutdoorRuntimeState)
    {
        const std::string map = data.currentSceneKind == SceneKind::Outdoor ? data.mapFileName
            : (data.outdoorWorld.eventRuntimeState ? data.outdoorWorld.eventRuntimeState->mapFileName : "");
        if (!validateWorld(data.outdoorWorld, map))
        {
            return false;
        }
    }
    for (const auto &[map, snapshot] : data.outdoorWorldStates)
    {
        if (!validateWorld(snapshot, map))
        {
            return false;
        }
    }
    const auto validateIndoor = [&](const IndoorSceneRuntime::Snapshot &snapshot)
    {
        return !snapshot.eventRuntimeState || validSplitFlag(snapshot.eventRuntimeState->namedGlobalVars, false);
    };
    if ((data.hasIndoorSceneState && !validateIndoor(data.indoorScene))
        || !std::all_of(data.indoorSceneStates.begin(), data.indoorSceneStates.end(),
            [&](const auto &entry) { return validateIndoor(entry.second); }))
    {
        return false;
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors)
        {
            if (!validateIndoor(floor.runtime.scene)) return false;
        }
    }
    if (data.breachAlvarCompanions && !foundFriendWorld)
    {
        error = "Breach companion state requires its persistent friend world";
        return false;
    }
    return true;
}

bool validateSavedActorCombat(const GameSaveData &data, std::string &error)
{
    const auto validateOutdoor = [&error](const OutdoorWorldRuntime::Snapshot &world)
    {
        std::unordered_set<uint32_t> actorIds;
        const bool hasResolved = std::any_of(world.mapActors.begin(), world.mapActors.end(),
            [](const auto &actor) { return actor.combatState.resolved() != nullptr; });
        for (const OutdoorWorldRuntime::MapActorState &actor : world.mapActors)
        {
            if (actor.combatState.mode() == GameplayActorCombatMode::LegacyDynamic
                || (hasResolved && !actorIds.insert(actor.actorId).second))
            {
                error = "Invalid outdoor actor combat mode or duplicate actor identity";
                return false;
            }
            if (!validateGameplayActorCombatState(actor.combatState, actor.currentHp, error))
            {
                return false;
            }
        }
        return true;
    };
    const auto validateIndoor = [&error](const IndoorSceneRuntime::Snapshot &scene)
    {
        const auto &generation = scene.worldRuntime.mergeMonsterGeneration;
        if (generation && !validateMergeMonsterGenerationState(*generation, error))
        {
            return false;
        }
        if (scene.mapDeltaData)
        {
            for (const MapDeltaSpriteObject &object : scene.mapDeltaData->spriteObjects)
            {
                if (!validContainingItem(object))
                {
                    error = "Invalid indoor ground item or simultaneous raw and typed item state";
                    return false;
                }
            }
        }
        const auto &actors = scene.worldRuntime.mapActorAiStates;
        for (size_t index = 0; index < actors.size(); ++index)
        {
            const IndoorWorldRuntime::MapActorAiState &actor = actors[index];
            if (actor.stolenItems.size() > 2 || std::any_of(actor.stolenItems.begin(), actor.stolenItems.end(),
                [](const InventoryItem &item)
                {
                    return item.objectDescriptionId == 0 || item.quantity == 0 || item.width == 0 || item.height == 0
                        || item.currentCharges > item.maxCharges || !std::isfinite(item.temporaryBonusRemainingSeconds)
                        || item.temporaryBonusRemainingSeconds < 0;
                }))
            {
                error = "Invalid monster stolen item custody or more than two native stolen slots";
                return false;
            }
            if (!validateGameplayActorSpecialRuntimeState(actor.specialState, actor.combatState, error))
            {
                return false;
            }
            if (actor.specialState.replicationPending && actor.motionState != ActorAiMotionState::Dying
                && actor.motionState != ActorAiMotionState::Dead)
            {
                error = "Pending actor replication requires a dying or dead source actor";
                return false;
            }
            const GameplayMergeResolvedActorCombat *pResolved = actor.combatState.resolved();
            if (pResolved == nullptr)
            {
                continue;
            }
            if (generation && (pResolved->contentPackageId != generation->contentPackageId
                || pResolved->contentPackageSchema != generation->contentPackageSchema
                || pResolved->catalogSha256 != generation->catalogSha256
                || pResolved->profileSha256 != generation->profileSha256
                || pResolved->generationEpoch != generation->generationEpoch
                || pResolved->generationLevel != generation->generationLevel
                || pResolved->generationSerial != generation->generationSerial))
            {
                error = "Resolved indoor actor and saved generation have different source identities";
                return false;
            }
            if (!scene.mapDeltaData || scene.mapDeltaData->actors.size() != actors.size() || actor.actorId != index)
            {
                error = "Resolved indoor actor has no matching delta/AI identity";
                return false;
            }
            if (!validateGameplayActorCombatState(actor.combatState, scene.mapDeltaData->actors[index].hp, error))
            {
                return false;
            }
        }
        return true;
    };
    if (!validateOutdoor(data.outdoorWorld) || !validateIndoor(data.indoorScene))
    {
        return false;
    }
    for (const auto &[map, world] : data.outdoorWorldStates)
    {
        if (!validateOutdoor(world))
        {
            return false;
        }
    }
    for (const auto &[map, scene] : data.indoorSceneStates)
    {
        if (!validateIndoor(scene))
        {
            return false;
        }
    }
    if (data.breachMaze)
    {
        for (const auto &[level, floor] : data.breachMaze->floors)
        {
            if (!validateIndoor(floor.runtime.scene)) return false;
        }
    }
    return true;
}

bool saveGameDataToPath(const std::filesystem::path &path, const GameSaveData &data, std::string &error,
    const BreachAlvarFriendActorRules *pCompanionRules)
{
    if (!validateSavedOutdoorLightTimes(data, error) || !validateSavedPartyMotion(data, error)
        || !validateSavedBreachMaze(data, error) || !validateSavedActorCombat(data, error))
    {
        return false;
    }
    if (data.breachAlvarCompanions && pCompanionRules == nullptr)
    {
        error = "Saving Breach companions requires their loaded source content rules";
        return false;
    }
    if (!validateSavedBreachAlvarCompanions(data, error, pCompanionRules))
    {
        return false;
    }
    if (data.party.pendingItemDeliveries.size() > Party::MaxPendingItemDeliveries
        || std::any_of(data.party.pendingItemDeliveries.begin(), data.party.pendingItemDeliveries.end(),
            [&data](const Party::PendingItemDelivery &delivery)
            { return !Party::validPendingItemDelivery(delivery, data.party.members.size()); }))
    {
        error = "Invalid pending quest item delivery";
        return false;
    }
    const auto validOutdoor = [](const auto &entry) { return validGameMinutes(entry.second.gameMinutes); };
    const auto validIndoor = [](const auto &entry) { return validGameMinutes(entry.second.worldRuntime.gameMinutes); };
    if (!validGameMinutes(data.savedGameMinutes) || !validGameMinutes(data.outdoorWorld.gameMinutes)
        || !validGameMinutes(data.indoorScene.worldRuntime.gameMinutes)
        || !std::all_of(data.outdoorWorldStates.begin(), data.outdoorWorldStates.end(), validOutdoor)
        || !std::all_of(data.indoorSceneStates.begin(), data.indoorSceneStates.end(), validIndoor))
    {
        error = "invalid authoritative game clock";
        return false;
    }
    if (!validateNamedGameTimes(data.namedGlobalVars, error))
    {
        return false;
    }
    const auto validRuntimeTimes = [&error](const auto &state)
    {
        return !state || validateNamedGameTimes(state->namedGlobalVars, error);
    };
    if (!validRuntimeTimes(data.outdoorWorld.eventRuntimeState)
        || !validRuntimeTimes(data.indoorScene.eventRuntimeState))
    {
        return false;
    }
    for (const auto &[map, state] : data.outdoorWorldStates)
    {
        if (!validRuntimeTimes(state.eventRuntimeState)) return false;
    }
    for (const auto &[map, state] : data.indoorSceneStates)
    {
        if (!validRuntimeTimes(state.eventRuntimeState)) return false;
    }
    if (!validateSavedTurnBasedCombat(data, error))
    {
        return false;
    }
    BinaryWriter writer;
    try
    {
        writer.writeBytes(SaveMagic, sizeof(SaveMagic));
        writer.write(SaveVersion);
        writer.write(data);
    }
    catch (const std::exception &exception)
    {
        error = std::string("Could not encode save data: ") + exception.what();
        return false;
    }

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);

    if (!output)
    {
        error = "could not open save file for writing";
        return false;
    }

    const std::vector<uint8_t> &bytes = writer.bytes();
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    if (!output.good())
    {
        error = "could not write save file";
        return false;
    }

    return true;
}

std::optional<GameSaveData> loadGameDataFromPath(const std::filesystem::path &path, std::string &error)
{
    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        error = "save file not found";
        return std::nullopt;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);

    if (size <= 0)
    {
        error = "save file is empty";
        return std::nullopt;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.read(reinterpret_cast<char *>(bytes.data()), size);

    if (!input.good() && !input.eof())
    {
        error = "could not read save file";
        return std::nullopt;
    }

    BinaryReader reader(bytes);
    char magic[sizeof(SaveMagic)] = {};
    uint32_t version = 0;

    if (!reader.readBytes(magic, sizeof(magic))
        || std::memcmp(magic, SaveMagic, sizeof(SaveMagic)) != 0
        || !reader.read(version))
    {
        error = "invalid save header";
        return std::nullopt;
    }

    if (version > SaveVersion)
    {
        error = "unsupported save version";
        return std::nullopt;
    }

    reader.setVersion(version);

    GameSaveData data = {};
    bool decoded = false;

    try
    {
        decoded = reader.read(data);
    }
    catch (const std::exception &exception)
    {
        error = std::string("save data is corrupted: ") + exception.what();
        return std::nullopt;
    }

    if (!decoded || reader.failed() || (version >= SaveVersionBreachMaze && reader.remainingBytes() != 0))
    {
        error = "save data is corrupted";
        return std::nullopt;
    }

    if (!validateSavedOutdoorLightTimes(data, error) || !validateSavedPartyMotion(data, error)
        || !validateSavedBreachMaze(data, error) || !validateSavedActorCombat(data, error))
    {
        return std::nullopt;
    }
    migrateLegacyPartyMemberIdentities(data, version);
    if (!validateSavedBreachAlvarCompanions(data, error))
    {
        return std::nullopt;
    }

    if (!validateNamedGameTimes(data.namedGlobalVars, error))
    {
        return std::nullopt;
    }

    if (!validateSavedTurnBasedCombat(data, error))
    {
        return std::nullopt;
    }
    // Versions through 84 never stored a turn; they explicitly retain their former realtime load behavior.
    migrateLegacyConsumedCorpseMarkers(data, version);

    return data;
}

bool compareSavePathsForDisplay(const std::filesystem::path &left, const std::filesystem::path &right)
{
    const int leftPriority = savePathDisplayPriority(left);
    const int rightPriority = savePathDisplayPriority(right);

    if (leftPriority != rightPriority)
    {
        return leftPriority < rightPriority;
    }

    return toLowerCopy(left.filename().string()) < toLowerCopy(right.filename().string());
}
}
