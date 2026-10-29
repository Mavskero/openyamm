#pragma once

#include "game/party/Party.h"
#include "game/tables/ReadableScrollTable.h"
#include "game/party/SkillData.h"
#include "game/party/SpeechIds.h"
#include "game/audio/SoundIds.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class ItemTable;
class MergedPotionSettingTable;
class MergedReagentSettingTable;

enum class InventoryItemUseAction : uint8_t
{
    None = 0,
    Equip,
    CastScroll,
    LearnSpell,
    LearnSkill,
    ConsumePotion,
    ReadMessageScroll,
    UseHorseshoe,
    UseGenieLamp,
    UseDeckOfFate,
    EatFoodItem,
    PlayInstrument,
    UseTempleInABottle,
    UseReagent,
    UseDimensionDoorScroll,
    UseConnectorStone,
};

struct InventoryItemUseContext
{
    bool underwater = false;
    float gameMinutes = 0.0f;
};

struct InventoryItemUseFeedback
{
    enum class Reason : uint8_t
    {
        None,
        CannotUse,
        InactiveCharacter,
        Underwater,
        SpellLearningSkillRequired,
        SkillBookNotLearnable,
        SkillLearningFailed,
    };

    enum class RewardKind : uint8_t
    {
        None,
        Stat,
        Gold,
        Food,
        SkillPoints,
        Experience,
    };

    Reason reason = Reason::None;
    std::string itemName;
    std::optional<CharacterCondition> condition;
    RewardKind rewardKind = RewardKind::None;
    uint32_t rewardAmount = 0;
    std::string rewardStatKey;
};

struct InventoryItemUseResult
{
    bool handled = false;
    bool consumed = false;
    InventoryItemUseAction action = InventoryItemUseAction::None;
    bool alreadyKnown = false;
    std::vector<std::string> learnedSkills;
    uint32_t spellId = 0;
    uint32_t spellSkillLevelOverride = 0;
    SkillMastery spellSkillMasteryOverride = SkillMastery::None;
    bool spellBypassRequiredMastery = false;
    std::string statusText;
    std::optional<SpeechId> speechId;
    std::optional<SoundId> soundId;
    std::string readableTitle;
    std::string readableBody;
    InventoryItemUseFeedback feedback;
};

class InventoryItemUseRuntime
{
public:
    static InventoryItemUseAction classifyItemUse(
        const InventoryItem &item,
        const ItemTable &itemTable,
        const MergedPotionSettingTable *pPotionSettingTable = nullptr,
        const MergedReagentSettingTable *pReagentSettingTable = nullptr);

    static InventoryItemUseResult useItemOnMember(
        Party &party,
        size_t targetMemberIndex,
        const InventoryItem &item,
        const ItemTable &itemTable,
        const ReadableScrollTable *pReadableScrollTable,
        const MergedPotionSettingTable *pPotionSettingTable = nullptr,
        const MergedReagentSettingTable *pReagentSettingTable = nullptr,
        const InventoryItemUseContext &context = {});
};
}
