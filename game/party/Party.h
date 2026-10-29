#pragma once

#include "game/content/OwnedMercenaryState.h"
#include "game/party/CharacterState.h"
#include "game/items/InventoryItem.h"
#include "game/gameplay/NpcFollowerTypes.h"
#include "game/tables/ClassSkillTable.h"
#include "game/outdoor/OutdoorMovementDriver.h"
#include "game/tables/PortraitEnums.h"
#include "game/audio/SoundIds.h"
#include "game/party/SpeechIds.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
class ItemTable;
class CharacterDollTable;
class ClassMultiplierTable;
class JournalQuestTable;
struct RosterEntry;
struct EventRuntimeState;
struct MercenaryReleaseProfile;
class StandardItemEnchantTable;
class SpecialItemEnchantTable;

enum class ItemPlacementFailure : uint8_t
{
    None,
    InvalidMember,
    InvalidItem,
    CannotEquip,
    TargetOccupied,
    InventoryFull,
    CannotPlace,
};

// Transient operation output; item state and legacy diagnostic text remain authoritative.
struct ItemOperationFeedback
{
    enum class Outcome
    {
        None,
        NoItem,
        Unavailable,
        AlreadyIdentified,
        IdentifyFailed,
        Identified,
        NothingToRepair,
        RepairFailed,
        Repaired,
    };

    Outcome outcome = Outcome::None;
    std::string itemName;
};

struct LloydBeacon
{
    std::string mapName;
    std::string locationName;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float directionDegrees = 0.0f;
    float remainingSeconds = 0.0f;
    int previewWidth = 0;
    int previewHeight = 0;
    std::vector<uint8_t> previewPixelsBgra;
};

struct TimedPrimaryStatBonus
{
    int power = 0;
    float remainingSeconds = 0.0f;
};

struct Character
{
    static constexpr int InventoryWidth = 14;
    static constexpr int InventoryHeight = 9;

    std::string name;
    std::string biography;
    std::string role;
    std::string className;
    std::string portraitTextureName;
    uint32_t portraitPictureId = 0;
    PortraitId portraitState = PortraitId::Normal;
    uint32_t portraitElapsedTicks = 0;
    uint32_t portraitDurationTicks = 0;
    uint32_t portraitSequenceCounter = 0;
    uint16_t portraitImageIndex = 0;
    uint32_t rosterId = 0;
    uint64_t memberId = 0; // Party-owned identity; roster templates remain unassigned until admission.
    SourceMercenaryRetention sourceMercenaryRetention;
    uint32_t characterDataId = 0;
    int32_t voiceId = -1;
    uint32_t birthYear = 0;
    uint32_t sexId = 0;
    uint32_t raceId = 0;
    uint32_t experience = 0;
    uint32_t level = 1;
    uint32_t skillPoints = 0;
    uint32_t might = 0;
    uint32_t intellect = 0;
    uint32_t personality = 0;
    uint32_t endurance = 0;
    uint32_t speed = 0;
    uint32_t accuracy = 0;
    uint32_t luck = 0;
    int maxHealth = 100;
    int health = 100;
    int maxSpellPoints = 0;
    int spellPoints = 0;
    uint8_t armageddonCastsToday = 0;
    std::string quickSpellName;
    std::string attackSpellName;
    std::unordered_set<uint32_t> knownSpellIds;
    CharacterResistanceSet baseResistances = {};
    CharacterStatBonuses permanentBonuses = {};
    CharacterStatBonuses temporaryEventBonuses = {};
    CharacterStatBonuses magicalBonuses = {};
    CharacterResistanceImmunitySet permanentImmunities = {};
    CharacterResistanceImmunitySet magicalImmunities = {};
    std::bitset<CharacterConditionCount> permanentConditionImmunities = {};
    std::bitset<CharacterConditionCount> magicalConditionImmunities = {};
    CharacterEquipment equipment = {};
    CharacterEquipmentRuntimeState equipmentRuntime = {};
    std::bitset<CharacterConditionCount> conditions = {};
    std::array<float, CharacterConditionCount> conditionStartGameMinutes = {};
    std::unordered_map<std::string, CharacterSkill> skills;
    std::unordered_set<uint32_t> awards;
    std::unordered_map<uint16_t, int32_t> eventVariables;
    float recoverySecondsRemaining = 0.0f;
    int armorClassModifier = 0;
    int levelModifier = 0;
    int ageModifier = 0;
    std::unordered_set<uint32_t> playerBits;
    uint32_t npcs2 = 0;
    int merchantBonus = 0;
    int weaponEnchantmentDamageBonus = 0;
    float vampiricHealFraction = 0.0f;
    bool physicalAttackDisabled = false;
    bool physicalDamageImmune = false;
    bool halfMissileDamage = false;
    int meleeArmorClassBonus = 0;
    int missileArmorClassBonus = 0;
    int magicArmorClassBonus = 0;
    float physicalDamageTakenMultiplier = 1.0f;
    float experienceGainMultiplier = 1.0f;
    float randomEncounterChanceMultiplier = 1.0f;
    bool waterWalking = false;
    bool featherFalling = false;
    float healthRegenPerSecond = 0.0f;
    float spellRegenPerSecond = 0.0f;
    float healthRegenAccumulator = 0.0f;
    float spellRegenAccumulator = 0.0f;
    int attackRecoveryReductionTicks = 0;
    float recoveryProgressMultiplier = 1.0f;
    std::unordered_map<std::string, int> itemSkillBonuses;
    std::unordered_set<std::string> equippedItemEffectFlags;
    std::array<std::optional<LloydBeacon>, 5> lloydsBeacons = {};

    bool addInventoryItem(const InventoryItem &item);
    bool addInventoryItemAt(const InventoryItem &item, uint8_t gridX, uint8_t gridY);
    bool removeInventoryItem(uint32_t objectDescriptionId, uint32_t quantity = 1);
    const InventoryItem *inventoryItemAt(uint8_t gridX, uint8_t gridY) const;
    bool takeInventoryItemAt(uint8_t gridX, uint8_t gridY, InventoryItem &item);
    bool tryPlaceInventoryItemAt(
        const InventoryItem &item,
        uint8_t gridX,
        uint8_t gridY,
        std::optional<InventoryItem> &replacedItem);
    size_t inventoryItemCount() const;
    size_t usedInventoryCells() const;
    size_t inventoryCapacity() const;
    bool hasSkill(const std::string &skillName) const;
    bool hasCanonicalSkill(const std::string &canonicalSkillName) const;
    const CharacterSkill *findSkill(const std::string &skillName) const;
    const CharacterSkill *findSkillByCanonicalName(const std::string &canonicalSkillName) const;
    CharacterSkill *findSkill(const std::string &skillName);
    CharacterSkill *findSkillByCanonicalName(const std::string &canonicalSkillName);
    bool setSkillMastery(const std::string &skillName, SkillMastery mastery);
    void synchronizeInnateAbilitySpells();
    bool knowsSpell(uint32_t spellId) const;
    bool learnSpell(uint32_t spellId);
    bool forgetSpell(uint32_t spellId);

    std::vector<InventoryItem> inventory;
    std::array<TimedPrimaryStatBonus, 6> timedPrimaryStatBonuses = {};
};

// Structural validation only; source content and unique ownership are checked before world projection.
bool validSourceMercenaryRetention(const Character &member, std::string &error);
// A missing alias argument preserves an existing confirmed alias. A supplied alias must already be authenticated
// by the caller and match this Character's rosterId. Legacy receipt inference cannot overwrite a known fact.
bool setSourceMercenaryRetention(Character &member, uint64_t expectedMemberId,
    SourceMercenaryRetentionKind kind, SourceMercenaryProvenance provenance,
    std::optional<uint32_t> validatedSourceRosterId, std::string &error);

struct AdventurersInnMember
{
    Character character = {};
    uint32_t portraitPictureId = 0;
};

struct PartySeed
{
    std::vector<Character> members;
    std::vector<InventoryItem> inventory;
    int gold = 0;
    int food = 0;
};

enum class PartyBuffId : uint8_t
{
    TorchLight = 0,
    WizardEye,
    FeatherFall,
    DetectLife,
    WaterWalk,
    Fly,
    Invisibility,
    Stoneskin,
    DayOfGods,
    ProtectionFromMagic,
    FireResistance,
    WaterResistance,
    AirResistance,
    EarthResistance,
    MindResistance,
    BodyResistance,
    Shield,
    Heroism,
    Haste,
    Immolation,
    Glamour,
    Levitate,
    Count,
};

static constexpr size_t PartyBuffCount = static_cast<size_t>(PartyBuffId::Count);

enum class CharacterBuffId : uint8_t
{
    Bless = 0,
    Fate,
    Preservation,
    Regeneration,
    Hammerhands,
    PainReflection,
    FireAura,
    VampiricWeapon,
    Mistform,
    Glamour,
    Count,
};

static constexpr size_t CharacterBuffCount = static_cast<size_t>(CharacterBuffId::Count);

struct PartyBuffState
{
    float remainingSeconds = 0.0f;
    uint32_t spellId = 0;
    uint32_t skillLevel = 0;
    SkillMastery skillMastery = SkillMastery::None;
    int power = 0;
    uint32_t casterMemberIndex = 0;

    bool active() const
    {
        return remainingSeconds > 0.0f;
    }
};

struct CharacterBuffState
{
    float remainingSeconds = 0.0f;
    float periodicAccumulatorSeconds = 0.0f;
    uint32_t spellId = 0;
    uint32_t skillLevel = 0;
    SkillMastery skillMastery = SkillMastery::None;
    int power = 0;
    uint32_t casterMemberIndex = 0;
    uint64_t casterMemberId = 0;

    bool active() const
    {
        return remainingSeconds > 0.0f;
    }
};

class Party
{
public:
    enum class ClassChangeMode
    {
        ApplyClassEffects,
        AssignmentOnly,
    };

    struct HouseStockState
    {
        uint32_t houseId = 0;
        float nextRefreshGameMinutes = 0.0f;
        uint32_t refreshSequence = 0;
        uint32_t generationVersion = 0;
        std::vector<InventoryItem> standardStock;
        std::vector<InventoryItem> specialStock;
        std::vector<InventoryItem> spellbookStock;
    };

    struct PendingItemDelivery
    {
        InventoryItem item;
        int32_t recipientMemberIndex = -1; // -1: mouse; otherwise a fixed active-party member.
        bool allowDropProtectedHeldItem = true; // Original source TakeOrDrop; generic event grants honor NoDrop.
    };
    static constexpr size_t MaxPendingItemDeliveries = 128;
    static constexpr size_t MaxDetachedMembers = 32;
    static constexpr size_t MaxDetachmentBatches = 64;
    static constexpr uint32_t NoActiveMember = UINT32_MAX;

    struct DetachmentAssignment
    {
        size_t memberIndex = 0;
        uint32_t slotId = 0;
    };

    struct ReservedDetachment
    {
        uint32_t slotId = 0;
        uint64_t memberId = 0; // Zero admits a new, uninstantiated template.
        // Existing custody is preserved unless source generation explicitly supplies a replacement.
        std::optional<Character> replacement;
        bool generated = false; // A transient preparation fact, never inferred from Character defaults.
    };

    struct DetachedPartyMember
    {
        std::string ownerKey;
        uint32_t slotId = 0;
        Character character;
        std::array<CharacterBuffState, CharacterBuffCount> characterBuffs = {};
        // A fixed recipient of zero denotes this container's sole character until rejoining.
        std::vector<PendingItemDelivery> pendingItemDeliveries;
    };

    struct Snapshot
    {
        std::vector<Character> members;
        std::vector<AdventurersInnMember> adventurersInnMembers;
        size_t activeMemberIndex = 0;
        std::array<PartyBuffState, PartyBuffCount> partyBuffs = {};
        std::array<std::array<CharacterBuffState, CharacterBuffCount>, 5> characterBuffs = {};
        int gold = 0;
        int bankGold = 0;
        int food = 0;
        int fineGold = 0;
        uint32_t waterDamageTicks = 0;
        uint32_t burningDamageTicks = 0;
        uint32_t splashCount = 0;
        uint32_t landingSoundCount = 0;
        uint32_t hardLandingSoundCount = 0;
        uint32_t monsterTargetSelectionCounter = 0;
        uint32_t houseStockSeed = 0;
        float itemEffectElapsedGameSeconds = 0.0f;
        uint32_t itemWeeklyEffectSequence = 0;
        float lastFallDamageDistance = 0.0f;
        std::unordered_set<uint32_t> foundArtifactItems;
        std::unordered_set<uint32_t> arcomageWonHouseIds;
        uint32_t arcomageWinCount = 0;
        uint32_t arcomageLossCount = 0;
        std::vector<HouseStockState> houseStockStates;
        std::unordered_set<uint32_t> everOwnedItemIds;
        std::unordered_map<uint32_t, int32_t> continentReputations;
        std::unordered_set<uint32_t> questBits;
        std::unordered_map<uint16_t, int32_t> eventVariables;
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> npcTopicOverrides;
        std::unordered_map<uint32_t, uint32_t> npcGroupNews;
        std::unordered_map<uint32_t, uint32_t> npcGreetingOverrides;
        std::unordered_map<uint32_t, uint32_t> npcGreetingDisplayCounts;
        std::unordered_map<uint32_t, uint32_t> npcHouseOverrides;
        std::unordered_map<uint32_t, uint32_t> npcItemOverrides;
        std::unordered_set<uint32_t> unavailableNpcIds;
        std::vector<HiredNpcFollower> hiredNpcFollowers;
        std::vector<PendingItemDelivery> pendingItemDeliveries;
        std::vector<DetachedPartyMember> detachedMembers;
        std::vector<std::string> completedDetachmentOwners;
        uint64_t nextMemberId = 1;
        // Source free-slot membership survives generation and hiring; it is not current availability.
        std::unordered_set<uint32_t> reusableMercenaryRosterIds;
    };

    struct PendingAudioRequest
    {
        enum class Kind : uint8_t
        {
            Sound,
            Speech,
        };

        Kind kind = Kind::Sound;
        SoundId soundId = SoundId::None;
        SpeechId speechId = SpeechId::None;
        size_t memberIndex = 0;
    };

    static PartySeed createDefaultSeed();

    void setItemTable(const ItemTable *pItemTable);
    void setJournalQuestTable(const JournalQuestTable *pJournalQuestTable);
    void setCharacterDollTable(const CharacterDollTable *pCharacterDollTable);
    void setItemEnchantTables(
        const StandardItemEnchantTable *pStandardItemEnchantTable,
        const SpecialItemEnchantTable *pSpecialItemEnchantTable);
    const ItemTable *itemTable() const;
    const JournalQuestTable *journalQuestTable() const;
    const StandardItemEnchantTable *standardItemEnchantTable() const;
    const SpecialItemEnchantTable *specialItemEnchantTable() const;
    size_t equippedItemSetPieceCount(size_t memberIndex, const std::string &setId) const;
    int32_t eventVariableValue(uint16_t variableId) const;
    void setEventVariableValue(uint16_t variableId, int32_t value);
    void addEventVariableValue(uint16_t variableId, int32_t value);
    void subtractEventVariableValue(uint16_t variableId, int32_t value);
    int fineGold() const;
    void addFineGold(int amount);
    void clearFineGold();
    int32_t continentReputation(uint32_t continentId) const;
    void setContinentReputation(uint32_t continentId, int32_t value);
    void setClassMultiplierTable(const ClassMultiplierTable *pClassMultiplierTable);
    const ClassMultiplierTable *classMultiplierTable() const;
    void setClassSkillTable(const ClassSkillTable *pClassSkillTable);
    const ClassSkillTable *classSkillTable() const;
    Snapshot snapshot() const;
    void restoreSnapshot(const Snapshot &snapshot);
    void reset();
    void seed(const PartySeed &seed);
    void applyMovementEffects(const OutdoorMovementEffects &effects);
    void applyEventRuntimeState(const EventRuntimeState &runtimeState, bool grantItemsToInventory = true);
    void setDebugDamageImmune(bool enabled);
    void setDebugUnlimitedMana(bool enabled);
    bool applyDamageToMember(
        size_t memberIndex,
        int damage,
        const std::string &status,
        bool allowUnconscious = false);
    bool applyDamageToActiveMember(int damage, const std::string &status);
    bool applyPotionExplosionToMember(size_t memberIndex, uint8_t damageLevel);
    bool applyDamageToAllLivingMembers(int damage, const std::string &status);
    uint32_t addExperienceToMember(size_t memberIndex, int64_t amount);
    uint32_t setMemberExperience(size_t memberIndex, uint32_t experience);
    uint32_t grantSharedExperience(uint32_t totalExperience);
    std::optional<size_t> chooseMonsterAttackTarget(uint32_t attackPreferences, uint32_t seedHint);
    std::vector<size_t> chooseMergeMonsterAttackTargets(
        uint32_t sourcePreferences, uint8_t targetCount, uint32_t seedHint);
    void addGold(int amount);
    void addFood(int amount);
    void requestSound(SoundId soundId);
    void requestDamageImpactSoundForMember(size_t memberIndex);
    void requestSpeech(size_t memberIndex, SpeechId speechId);
    bool hasQuestBit(uint32_t questBitId) const;
    void setQuestBit(uint32_t questBitId, bool value);
    uint64_t dialogueEligibilityRevision() const;
    void applyGlobalNpcStateTo(EventRuntimeState &runtimeState) const;
    void setNpcTopicOverride(uint32_t npcId, uint32_t topicSlotIndex, uint32_t topicId);
    void setNpcGroupNews(uint32_t groupId, uint32_t newsId);
    void setNpcGreetingOverride(uint32_t npcId, uint32_t greetingId);
    void recordNpcGreetingDisplay(uint32_t npcId, uint32_t displayCount);
    void setNpcHouseOverride(uint32_t npcId, uint32_t houseId);
    void clearNpcHouseOverride(uint32_t npcId);
    void setNpcItemOverride(uint32_t npcId, uint32_t itemId);
    void setNpcUnavailable(uint32_t npcId, bool unavailable);
    void addHiredNpcFollower(const HiredNpcFollower &follower);
    void removeHiredNpcFollower(uint32_t npcId);
    void setHiredNpcFollowerAbilityUsedDay(uint32_t npcId, uint32_t day);
    bool tryGrantItem(uint32_t objectDescriptionId, uint32_t quantity = 1);
    bool tryGrantInventoryItem(const InventoryItem &item, size_t *pRecipientMemberIndex = nullptr);
    bool tryGrantInventoryItemToMember(size_t memberIndex, const InventoryItem &item);
    bool grantOwnedQuestItem(
        const InventoryItem &item, int32_t recipientMemberIndex, bool allowDropProtectedHeldItem = true);
    bool canQueueItemDeliveries(size_t count) const;
    const std::vector<PendingItemDelivery> &pendingItemDeliveries() const;
    void completeFirstPendingItemDelivery();
    bool ownsPendingItem(uint32_t itemId) const;
    int pendingItemCount(uint32_t itemId, std::optional<size_t> memberIndex = std::nullopt) const;
    bool removePendingItem(uint32_t itemId, std::optional<size_t> memberIndex = std::nullopt);
    static bool validPendingItemDelivery(const PendingItemDelivery &delivery, size_t memberCount);
    bool tryGrantInventoryItemStartingAt(
        size_t firstMemberIndex,
        const InventoryItem &item,
        size_t *pRecipientMemberIndex = nullptr);
    void grantItem(uint32_t objectDescriptionId, uint32_t quantity = 1);
    bool removeItem(uint32_t objectDescriptionId, uint32_t quantity = 1);
    static int effectiveMaximumHealth(const Character &member);
    static int effectiveMaximumSpellPoints(const Character &member);
    bool needsHealing() const;
    bool activeMemberNeedsHealing() const;
    void restoreAll();
    void restAndHealAll();
    void reviveAndRestoreAll();
    void restoreActiveMember();
    bool trainLeader(uint32_t maxLevel, uint32_t &newLevel, uint32_t &skillPointsEarned);
    bool trainActiveMember(uint32_t maxLevel, uint32_t &newLevel, uint32_t &skillPointsEarned);
    bool canMemberLearnSkill(size_t memberIndex, const std::string &skillName) const;
    bool learnMemberSkill(size_t memberIndex, const std::string &skillName);
    bool canActiveMemberLearnSkill(const std::string &skillName) const;
    bool learnActiveMemberSkill(const std::string &skillName);
    bool canIncreaseActiveMemberSkillLevel(const std::string &skillName) const;
    bool increaseActiveMemberSkillLevel(const std::string &skillName);
    int depositGoldToBank(int amount);
    int withdrawBankGold(int amount);
    int depositAllGoldToBank();
    int withdrawAllBankGold();
    bool isFull() const;
    bool recruitRosterMember(const RosterEntry &rosterEntry);
    Character makeRosterCharacter(const RosterEntry &rosterEntry) const;
    bool recruitCharacter(const Character &character);
    bool addAdventurersInnMember(const RosterEntry &rosterEntry, uint32_t portraitPictureId);
    bool addAdventurersInnMember(const Character &character, uint32_t portraitPictureId);
    bool hireAdventurersInnMember(size_t innIndex);
    bool dismissMemberToAdventurersInn(size_t memberIndex);
    bool releaseAdventurersInnMember(uint64_t memberId, uint32_t sourceRosterId,
        const MercenaryReleaseProfile &profile, std::string &error);
    const std::unordered_set<uint32_t> &reusableMercenaryRosterIds() const;
    bool replaceMemberWithRosterEntry(size_t memberIndex, const RosterEntry &rosterEntry);
    bool hasRosterMember(uint32_t rosterId) const;
    bool detachMembers(const std::string &ownerKey, const std::vector<DetachmentAssignment> &assignments);
    bool detachMembersWithReserves(const std::string &ownerKey,
        const std::vector<DetachmentAssignment> &assignments, const std::vector<ReservedDetachment> &reserves);
    bool joinDetachedMember(const std::string &ownerKey, uint32_t slotId);
    bool hasDetachedBatch(const std::string &ownerKey) const;
    const DetachedPartyMember *detachedMember(const std::string &ownerKey, uint32_t slotId) const;
    const std::vector<DetachedPartyMember> &detachedMembers() const;
    static void initializeLegacyMemberIdentities(Snapshot &snapshot);
    static bool validMemberOwnership(const Snapshot &snapshot, std::string &error);
    bool hasAward(uint32_t awardId) const;
    bool hasAward(size_t memberIndex, uint32_t awardId) const;
    void addAward(uint32_t awardId);
    void addAward(size_t memberIndex, uint32_t awardId);
    void removeAward(uint32_t awardId);
    void removeAward(size_t memberIndex, uint32_t awardId);
    int inventoryItemCount(uint32_t objectDescriptionId, std::optional<size_t> memberIndex = std::nullopt) const;
    void recordEverOwnedItem(uint32_t objectDescriptionId);
    bool hasEverOwnedItem(uint32_t objectDescriptionId) const;
    void setHeldItemForQueries(const InventoryItem &item);
    uint32_t heldItemIdForQueries() const;
    void clearHeldItemForQueries();
    bool hasItemAnywhere(uint32_t objectDescriptionId) const;
    bool grantItemToMember(size_t memberIndex, uint32_t objectDescriptionId, uint32_t quantity = 1);
    bool removeItemFromMember(size_t memberIndex, uint32_t objectDescriptionId, uint32_t quantity = 1);
    bool takeItemFromMemberInventoryCell(size_t memberIndex, uint8_t gridX, uint8_t gridY, InventoryItem &item);
    bool tryAutoPlaceItemInMemberInventory(size_t memberIndex, const InventoryItem &item);
    bool tryPlaceItemInMemberInventoryCell(
        size_t memberIndex,
        const InventoryItem &item,
        uint8_t gridX,
        uint8_t gridY,
        std::optional<InventoryItem> &replacedItem,
        ItemPlacementFailure *pFailure = nullptr);
    bool takeEquippedItemFromMember(size_t memberIndex, EquipmentSlot slot, InventoryItem &item);
    bool tryEquipItemOnMember(
        size_t memberIndex,
        EquipmentSlot targetSlot,
        const InventoryItem &item,
        std::optional<EquipmentSlot> displacedSlot,
        bool autoStoreDisplacedItem,
        std::optional<InventoryItem> &heldReplacement,
        ItemPlacementFailure *pFailure = nullptr);
    std::optional<InventoryItem> equippedItem(size_t memberIndex, EquipmentSlot slot) const;
    const InventoryItem *memberInventoryItem(size_t memberIndex, uint8_t gridX, uint8_t gridY) const;
    InventoryItem *memberInventoryItemMutable(size_t memberIndex, uint8_t gridX, uint8_t gridY);
    uint32_t equippedItemId(size_t memberIndex, EquipmentSlot slot) const;
    const EquippedItemRuntimeState *equippedItemRuntime(size_t memberIndex, EquipmentSlot slot) const;
    EquippedItemRuntimeState *equippedItemRuntimeMutable(size_t memberIndex, EquipmentSlot slot);
    bool consumeEquippedWandCharge(size_t memberIndex);
    void refreshDerivedState();
    static bool isPartyWideUtilitySkill(std::string_view skillName);
    std::optional<size_t> bestPartyWideUtilitySkillMemberIndex(
        std::string_view skillName,
        bool requireCanAct = true) const;
    const Character *bestPartyWideUtilitySkillMember(std::string_view skillName, bool requireCanAct = true) const;
    int bestPartyWideUtilitySkillValue(std::string_view skillName, bool requireCanAct = true) const;
    bool tryIdentifyMemberInventoryItem(
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        size_t inspectorMemberIndex,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool tryRepairMemberInventoryItem(
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        size_t inspectorMemberIndex,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool identifyMemberInventoryItem(
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool repairMemberInventoryItem(
        size_t memberIndex,
        uint8_t gridX,
        uint8_t gridY,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool tryIdentifyEquippedItem(
        size_t memberIndex,
        EquipmentSlot slot,
        size_t inspectorMemberIndex,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool tryRepairEquippedItem(
        size_t memberIndex,
        EquipmentSlot slot,
        size_t inspectorMemberIndex,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool identifyEquippedItem(
        size_t memberIndex,
        EquipmentSlot slot,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool repairEquippedItem(
        size_t memberIndex,
        EquipmentSlot slot,
        std::string &statusText,
        ItemOperationFeedback *pFeedback = nullptr);
    bool setMemberClassName(
        size_t memberIndex,
        const std::string &className,
        ClassChangeMode mode = ClassChangeMode::ApplyClassEffects);
    bool applyLichTransformation(size_t memberIndex);
    void applyMergedLichAppearance();
    const Character *member(size_t memberIndex) const;
    Character *member(size_t memberIndex);
    bool canSelectMemberInGameplay(size_t memberIndex) const;
    bool hasSelectableMemberInGameplay() const;
    bool hasActableMember() const;
    bool setActiveMemberIndex(size_t memberIndex);
    bool canSpendSpellPoints(size_t memberIndex, int amount) const;
    bool spendSpellPoints(size_t memberIndex, int amount);
    bool spendSpellPointsOnActiveMember(int amount);
    void advanceTimedStates(float deltaSeconds);
    void updateRecovery(float deltaSeconds, float progressScale = 1.0f);
    bool applyRecoveryToMember(size_t memberIndex, float recoverySeconds);
    bool applyRecoveryToActiveMember(float recoverySeconds);
    bool switchToNextReadyMember();
    void applyPartyBuff(
        PartyBuffId buffId,
        float durationSeconds,
        int power,
        uint32_t spellId,
        uint32_t skillLevel,
        SkillMastery skillMastery,
        uint32_t casterMemberIndex);
    bool extendFeatherFall(float minimumRemainingSeconds);
    void applyCharacterBuff(
        size_t memberIndex,
        CharacterBuffId buffId,
        float durationSeconds,
        int power,
        uint32_t spellId,
        uint32_t skillLevel,
        SkillMastery skillMastery,
        uint32_t casterMemberIndex);
    void applyTemporaryPrimaryStatBonus(uint32_t statIndex, int power, float durationSeconds);
    void clearPartyBuff(PartyBuffId buffId);
    void clearCharacterBuff(size_t memberIndex, CharacterBuffId buffId);
    bool clearDispellableBuffs();
    bool hasDispellableBuffs() const;
    bool hasPartyBuff(PartyBuffId buffId) const;
    bool hasCharacterBuff(size_t memberIndex, CharacterBuffId buffId) const;
    const PartyBuffState *partyBuff(PartyBuffId buffId) const;
    const CharacterBuffState *characterBuff(size_t memberIndex, CharacterBuffId buffId) const;
    bool blockConditionWithProtectionFromMagic(CharacterCondition condition);
    bool applyMemberCondition(size_t memberIndex, CharacterCondition condition, float gameMinutes = 0.0f);
    bool clearMemberCondition(size_t memberIndex, CharacterCondition condition);
    bool hasMemberConditionImmunity(size_t memberIndex, CharacterCondition condition) const;
    bool healMember(size_t memberIndex, int amount);
    bool reviveMember(size_t memberIndex, int health, bool applyWeak);

    const std::vector<Character> &members() const;
    size_t memberCount() const;
    const std::vector<AdventurersInnMember> &adventurersInnMembers() const;
    const AdventurersInnMember *adventurersInnMember(size_t innIndex) const;
    AdventurersInnMember *adventurersInnMember(size_t innIndex);
    const Character *adventurersInnCharacter(size_t innIndex) const;
    Character *adventurersInnCharacter(size_t innIndex);
    void clearAdventurersInnMembers();
    const Character *activeMember() const;
    Character *activeMember();
    size_t activeMemberIndex() const;
    int totalHealth() const;
    int totalMaxHealth() const;
    int gold() const;
    int bankGold() const;
    int food() const;
    uint32_t houseStockSeed() const;
    bool hasFoundArtifactItem(uint32_t itemId) const;
    void markArtifactItemFound(uint32_t itemId);
    void clearFoundArtifactItems();
    bool hasArcomageWinAt(uint32_t houseId) const;
    void recordArcomageWin(uint32_t houseId, int goldReward, uint32_t firstWinAwardId);
    void recordArcomageLoss();
    uint32_t arcomageWinCount() const;
    uint32_t arcomageLossCount() const;
    HouseStockState *houseStockState(uint32_t houseId);
    const HouseStockState *houseStockState(uint32_t houseId) const;
    HouseStockState &ensureHouseStockState(uint32_t houseId);
    void clearHouseStockStates();
    size_t inventoryItemCount() const;
    size_t usedInventoryCells() const;
    size_t inventoryCapacity() const;
    uint32_t waterDamageTicks() const;
    uint32_t burningDamageTicks() const;
    uint32_t splashCount() const;
    uint32_t landingSoundCount() const;
    uint32_t hardLandingSoundCount() const;
    float lastFallDamageDistance() const;
    const std::string &lastStatus() const;
    const std::vector<PendingAudioRequest> &pendingAudioRequests() const;
    void clearPendingAudioRequests();
    void rebuildMagicalBonusesFromBuffs();

private:
    static constexpr size_t MaxMembers = 5;

    uint8_t resolveInventoryWidth(uint32_t objectDescriptionId) const;
    uint8_t resolveInventoryHeight(uint32_t objectDescriptionId) const;
    void applyDefaultStartingSkills(Character &member) const;
    void markArtifactItemFoundIfRelevant(const InventoryItem &item);
    void recordEverOwnedItemIfRelevant(const InventoryItem &item);
    void recordEverOwnedItemsFromCharacter(const Character &member);
    void recordEverOwnedItemsFromCurrentState();
    void applyWeeklyItemEffects();
    bool assignMemberIdentity(Character &character);
    bool hasDetachedRosterMember(uint32_t rosterId) const;
    size_t totalPendingItemDeliveries() const;
    void synchronizeCharacterBuffCasters();
    void rebuildMagicalBonusesPreservingProgress();
    void queueSound(SoundId soundId);
    void queueSpeech(size_t memberIndex, SpeechId speechId);
    SoundId resolveDamageImpactSoundForMember(size_t memberIndex) const;

    const ItemTable *m_pItemTable = nullptr;
    const JournalQuestTable *m_pJournalQuestTable = nullptr;
    const CharacterDollTable *m_pCharacterDollTable = nullptr;
    const StandardItemEnchantTable *m_pStandardItemEnchantTable = nullptr;
    const SpecialItemEnchantTable *m_pSpecialItemEnchantTable = nullptr;
    const ClassMultiplierTable *m_pClassMultiplierTable = nullptr;
    const ClassSkillTable *m_pClassSkillTable = nullptr;
    std::vector<Character> m_members;
    std::vector<AdventurersInnMember> m_adventurersInnMembers;
    std::unordered_set<uint32_t> m_reusableMercenaryRosterIds;
    std::vector<DetachedPartyMember> m_detachedMembers;
    std::vector<std::string> m_completedDetachmentOwners;
    uint64_t m_nextMemberId = 1;
    size_t m_activeMemberIndex = 0;
    std::array<PartyBuffState, PartyBuffCount> m_partyBuffs = {};
    std::array<std::array<CharacterBuffState, CharacterBuffCount>, MaxMembers> m_characterBuffs = {};
    int m_gold = 0;
    int m_bankGold = 0;
    int m_food = 0;
    int m_fineGold = 0;
    uint32_t m_waterDamageTicks = 0;
    uint32_t m_burningDamageTicks = 0;
    uint32_t m_splashCount = 0;
    uint32_t m_landingSoundCount = 0;
    uint32_t m_hardLandingSoundCount = 0;
    uint32_t m_monsterTargetSelectionCounter = 0;
    uint32_t m_houseStockSeed = 0;
    float m_itemEffectElapsedGameSeconds = 0.0f;
    uint32_t m_itemWeeklyEffectSequence = 0;
    float m_lastFallDamageDistance = 0.0f;
    std::string m_lastStatus;
    std::unordered_set<uint32_t> m_foundArtifactItems;
    std::unordered_set<uint32_t> m_arcomageWonHouseIds;
    std::unordered_set<uint32_t> m_everOwnedItemIds;
    std::unordered_map<uint32_t, int32_t> m_continentReputations;
    std::unordered_set<uint32_t> m_questBits;
    uint64_t m_dialogueEligibilityRevision = 1;
    uint32_t m_arcomageWinCount = 0;
    uint32_t m_arcomageLossCount = 0;
    bool m_debugDamageImmune = false;
    bool m_debugUnlimitedMana = false;
    std::unordered_map<uint32_t, HouseStockState> m_houseStockStates;
    std::unordered_map<uint16_t, int32_t> m_eventVariables;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> m_npcTopicOverrides;
    std::unordered_map<uint32_t, uint32_t> m_npcGroupNews;
    std::unordered_map<uint32_t, uint32_t> m_npcGreetingOverrides;
    std::unordered_map<uint32_t, uint32_t> m_npcGreetingDisplayCounts;
    std::unordered_map<uint32_t, uint32_t> m_npcHouseOverrides;
    std::unordered_map<uint32_t, uint32_t> m_npcItemOverrides;
    std::unordered_set<uint32_t> m_unavailableNpcIds;
    std::vector<HiredNpcFollower> m_hiredNpcFollowers;
    std::vector<PendingItemDelivery> m_pendingItemDeliveries;
    uint32_t m_heldItemIdForQueries = 0;
    std::vector<PendingAudioRequest> m_pendingAudioRequests;
};
}
