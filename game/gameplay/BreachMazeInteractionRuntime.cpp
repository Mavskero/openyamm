#include "game/gameplay/BreachMazeInteractionRuntime.h"

#include "game/events/EventRuntime.h"
#include "game/events/GameTime.h"
#include "game/events/ISceneEventContext.h"
#include "game/items/ItemGenerator.h"
#include "game/party/Party.h"
#include "game/tables/ItemTable.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint64_t RandomIncrement = UINT64_C(0x9e3779b97f4a7c15);
constexpr uint32_t DecorationEventStart = 20000;

void require(bool condition, const char *pMessage)
{
    if (!condition)
    {
        throw std::invalid_argument(std::string("Breach maze interaction: ") + pMessage);
    }
}

void validateCursor(const FixedItemGenerationCursor &cursor, uint64_t initialState, uint64_t initialCount)
{
    require(cursor.protocol == FixedItemGenerationProtocol::V1 && cursor.rawDrawCount >= initialCount
        && cursor.state == initialState + (cursor.rawDrawCount - initialCount) * RandomIncrement,
        "random cursor is not a continuation of its source stream");
}

std::optional<size_t> partyCell(float x, float y)
{
    // Preserve the original script's offset, which is deliberately not exactly half a cell.
    const double column = std::floor((double(x) + 300.525) / BreachMazeCellWidth);
    const double row = std::floor((-double(y) + 300.525) / BreachMazeCellWidth);
    if (column < 0 || row < 0 || column >= BreachMazeTemplateSide || row >= BreachMazeTemplateSide)
    {
        return std::nullopt;
    }
    return size_t(row) * BreachMazeTemplateSide + size_t(column);
}

uint32_t skillLevel(const Character &member, const char *pName)
{
    const CharacterSkill *pSkill = member.findSkillByCanonicalName(pName);
    return pSkill != nullptr ? pSkill->level : 0;
}

bool canDisarm(const Character &member, uint32_t mazeLevel)
{
    const CharacterSkill *pSkill = member.findSkillByCanonicalName("DisarmTraps");
    if (pSkill == nullptr)
    {
        return false;
    }
    const uint32_t mastery = uint32_t(pSkill->mastery);
    return mastery == 4 || (mastery > 0 && mastery <= 3
        && pSkill->level > (uint64_t(mazeLevel) + mastery - 1) / mastery);
}

BreachMazeInteractionMessage text(BreachMazeInteractionTextSource source, uint32_t id)
{
    return {source, id, {}};
}

InventoryItem generateItem(Party &party, FixedItemGenerationCursor &cursor, int level, uint32_t type)
{
    require(party.itemTable() != nullptr && party.standardItemEnchantTable() != nullptr
        && party.specialItemEnchantTable() != nullptr, "missing shared item tables");
    ItemGenerationRequest request;
    request.treasureLevel = level;
    request.allowRareItems = false; // The persistent fixed-generation protocol owns this source reward path.
    std::string error;
    const std::optional<FixedItemGenerationResult> generated = ItemGenerator::generateFixedInventoryItem(
        *party.itemTable(), *party.standardItemEnchantTable(), *party.specialItemEnchantTable(),
        request, cursor, error, type);
    if (!generated)
    {
        throw std::invalid_argument("Breach maze interaction item: " + error);
    }
    cursor = generated->after;
    return generated->item;
}

void retainItem(Party &party, const InventoryItem &item, size_t recipient)
{
    require(recipient < party.memberCount() && recipient <= size_t(std::numeric_limits<int32_t>::max()),
        "invalid reward recipient");
    require(party.grantOwnedQuestItem(item, int32_t(recipient), false), "pending item delivery capacity exhausted");
}

void addVariable(EventRuntimeState &events, Party &party, ISceneEventContext &scene,
    uint32_t variable, int32_t value, size_t memberIndex)
{
    const EventRuntime::VariableRef reference = EventRuntime::decodeVariable(variable);
    const int32_t previous = EventRuntime::getVariableValue(events, reference, &party, memberIndex, &scene);
    require(int64_t(previous) + value <= std::numeric_limits<int32_t>::max(), "character variable overflow");
    EventRuntime::addVariableValue(events, reference, value, &party, {memberIndex}, &scene);
}

void addAutonote(EventRuntimeState &events, Party &party, ISceneEventContext &scene,
    uint32_t note, size_t memberIndex)
{
    const uint32_t variable = (note << 16) | uint32_t(EvtVariable::AutoNotes);
    EventRuntime::setVariableValue(events, EventRuntime::decodeVariable(variable), 1, &party, {memberIndex}, &scene);
}

void setCondition(EventRuntimeState &events, Party &party, ISceneEventContext &scene,
    uint32_t variable, size_t memberIndex)
{
    EventRuntime::setVariableValue(events, EventRuntime::decodeVariable(variable), 0, &party, {memberIndex}, &scene);
}

struct TrapProjectile
{
    uint32_t spellId = 0;
    int32_t fromX = 0;
    int32_t fromY = 0;
    int32_t toX = 0;
    int32_t toY = 0;
    int32_t toZ = 0;
};
}

BreachMazeInteractionState makeBreachMazeInteractionState(const BreachMazePopulationPlan &population)
{
    BreachMazeInteractionState result;
    result.identity = population.identity;
    result.random.state = population.interactionRandomState;
    result.bookcaseUsed = population.bookcaseUsed;
    for (size_t index = 0; index < BreachMazeTemplateCellCount; ++index)
    {
        result.traps[index].spellId = population.traps[index].spellId;
        result.traps[index].nextShotGameMinute = double(population.traps[index].nextShotGameMinute);
        const BreachMazeDecorationInitialState &initial = population.decorations[index].interaction;
        result.decorations[index].used = initial.used;
        result.decorations[index].random.state = initial.randomState;
        result.decorations[index].random.rawDrawCount = initial.randomDrawCount;
    }
    std::string error;
    if (!validateBreachMazeInteractionState(result, population, error))
    {
        throw std::invalid_argument(error);
    }
    return result;
}

bool validateBreachMazeInteractionState(const BreachMazeInteractionState &state,
    const BreachMazePopulationPlan &population, std::string &error)
{
    error.clear();
    try
    {
        require(state.version == BreachMazeInteractionStateVersion && state.identity == population.identity
            && population.version == BreachMazePopulationVersion
            && population.contentFixVersion == BreachMazePopulationContentFixVersion
            && state.identity.epoch != 0 && state.identity.generationSerial != 0
            && state.identity.level >= 1 && state.identity.level <= BreachMazeMaximumPlannedLevel,
            "unsupported state or mismatched floor identity");
        const BreachMazePopulationInteractionRules &rules = population.interactionRules;
        const size_t trapSpellCount = std::min<size_t>(state.identity.level, BreachMazeTrapSpells.size());
        require(rules.trapSpellChoices.size() == trapSpellCount
            && std::equal(rules.trapSpellChoices.begin(), rules.trapSpellChoices.end(), BreachMazeTrapSpells.begin())
            && rules.trapSpellSkillLevel == 7 && rules.trapSpellMastery == 3 && rules.trapCooldownGameMinutes == 3
            && rules.bookcaseTreasureMinimum == std::min((state.identity.level + 1) / 2,
                uint32_t(BreachMazeMaximumTreasureLevel))
            && rules.bookcaseTreasureMaximum == std::min(state.identity.level, uint32_t(BreachMazeMaximumTreasureLevel))
            && rules.bookcaseTreasureType == 16
            && rules.bookcaseRecipientPartyIndex == 0 && rules.interactionsUsePartyPositionCell,
            "unsupported source interaction rules");
        validateCursor(state.random, population.interactionRandomState, 0);
        for (size_t index = 0; index < BreachMazeTemplateCellCount; ++index)
        {
            const BreachMazeTrapState &trap = state.traps[index];
            require(validGameMinutes(trap.nextShotGameMinute)
                && (trap.spellId.has_value() || trap.nextShotGameMinute == 0)
                && (!trap.spellId || *trap.spellId == 0
                    || std::find(rules.trapSpellChoices.begin(), rules.trapSpellChoices.end(), *trap.spellId)
                        != rules.trapSpellChoices.end()), "invalid trap state");
            const BreachMazeDecorationInitialState &initial = population.decorations[index].interaction;
            const BreachMazeDecorationState &decoration = state.decorations[index];
            require(uint32_t(initial.handler) <= uint32_t(BreachMazeDecorationHandler::Challenge),
                "unknown decoration handler");
            validateCursor(decoration.random, initial.randomState, initial.randomDrawCount);
            if (initial.handler == BreachMazeDecorationHandler::None
                || initial.handler == BreachMazeDecorationHandler::Challenge)
            {
                require(!decoration.used && decoration.random.state == initial.randomState
                    && decoration.random.rawDrawCount == initial.randomDrawCount,
                    "nonconsumable decoration has local mutable state");
            }
            if (initial.handler == BreachMazeDecorationHandler::Crystal)
            {
                constexpr std::array<uint32_t, 4> crystals = {2060, 2056, 2059, 2065};
                require(initial.eventVariable >= 1 && initial.eventVariable <= crystals.size()
                    && initial.fixedRewardItemId == crystals[initial.eventVariable - 1], "invalid crystal reward");
            }
            if (initial.handler == BreachMazeDecorationHandler::AttributeBarrel)
            {
                require(initial.eventVariable >= 1 && initial.eventVariable <= 7, "invalid barrel attribute");
            }
            if (initial.handler == BreachMazeDecorationHandler::ResistanceCauldron)
            {
                require(initial.eventVariable >= 1 && initial.eventVariable <= 4, "invalid cauldron resistance");
            }
            if (initial.handler == BreachMazeDecorationHandler::Challenge)
            {
                const uint32_t tier = initial.eventVariable / 100;
                const uint32_t attribute = initial.eventVariable % 100;
                constexpr std::array<uint32_t, 3> rewards = {5, 7, 10};
                require(tier >= 2 && tier <= 4 && attribute >= 1 && attribute <= 7
                    && initial.requiresGlobalChallengeLedger
                    && initial.challengeThreshold == 25 * (1u << (tier - 1))
                    && initial.challengeSkillPointReward == rewards[tier - 2], "invalid challenge rule");
            }
        }
        return true;
    }
    catch (const std::exception &failure)
    {
        error = failure.what();
        return false;
    }
}

BreachMazeInteractionResult activateBreachMazeInteraction(BreachMazeInteractionState &state,
    const BreachMazePopulationPlan &population, const BreachMazeTemplate &mazeTemplate,
    const BreachMazeInteractionRequest &request, BreachMazeInteractionContext context)
{
    BreachMazeInteractionResult result;
    std::optional<size_t> decorationSlot;
    if (request.eventId >= DecorationEventStart
        && request.eventId < DecorationEventStart + BreachMazeTemplateCellCount)
    {
        const size_t slot = request.eventId - DecorationEventStart;
        if (population.decorations[slot].eventId == request.eventId
            && population.decorations[slot].interaction.handler != BreachMazeDecorationHandler::None)
        {
            decorationSlot = slot;
        }
    }
    result.handled = request.eventId == 3 || request.eventId == 4 || decorationSlot.has_value();
    if (!result.handled)
    {
        return result;
    }
    if (!validateBreachMazeInteractionState(state, population, result.error))
    {
        return result;
    }

    try
    {
        require(validGameMinutes(context.gameMinutes) && std::isfinite(context.partyX)
            && std::isfinite(context.partyY) && std::isfinite(context.partyFootZ), "invalid party position or clock");
        require(context.party.memberCount() > 0 && (!request.currentMemberIndex
            || *request.currentMemberIndex < context.party.memberCount()), "invalid current member");
        require(mazeTemplate.version == BreachMazeTemplateVersion
            && mazeTemplate.faces.size() > 0, "missing maze template");
        BreachMazeInteractionState candidate = state;
        Party party = context.party;
        EventRuntimeState events = context.events;
        const size_t current = request.currentMemberIndex.value_or(0);
        std::optional<TrapProjectile> projectile;

        if (request.eventId == 3 || request.eventId == 4)
        {
            const std::optional<size_t> cell = partyCell(context.partyX, context.partyY);
            if (!cell)
            {
                return result;
            }
            const BreachMazePopulationInteractionRules &rules = population.interactionRules;
            FixedItemGenerationRandom random(candidate.random);
            if (request.eventId == 4)
            {
                if (candidate.bookcaseUsed[*cell])
                {
                    result.message = text(BreachMazeInteractionTextSource::GlobalText, 521);
                    return result;
                }
                // The source rolls the full depth range before clamping at6. bounded() has the
                // same schedule as the old intRange for L1..3 and safely accepts uint32 depths.
                const uint32_t minimum = (state.identity.level + 1) / 2;
                const uint64_t rolledLevel = uint64_t(minimum)
                    + random.bounded(uint64_t(state.identity.level) - minimum + 1);
                const int level = int(std::min(rolledLevel, uint64_t(BreachMazeMaximumTreasureLevel)));
                candidate.random = random.cursor();
                const InventoryItem reward = generateItem(party, candidate.random, level, rules.bookcaseTreasureType);
                retainItem(party, reward, rules.bookcaseRecipientPartyIndex);
                // Source keys EmptyBookcases by Party position, even if a different face was clicked.
                candidate.bookcaseUsed[*cell] = true;
            }
            else
            {
                BreachMazeTrapState &trap = candidate.traps[*cell];
                if (!trap.spellId)
                {
                    const int choice = random.intRange(0, int(rules.trapSpellChoices.size()) - 1);
                    trap.spellId = rules.trapSpellChoices[size_t(choice)];
                }
                if (*trap.spellId == 0)
                {
                    return result;
                }
                const BreachMazeTemplateCell &sourceCell = mazeTemplate.cells[*cell];
                require(sourceCell.cellId == *cell + 1
                    && sourceCell.centralFloorFaceIndex < mazeTemplate.faces.size(), "invalid trap source floor");
                if (request.currentMemberIndex && canDisarm(*party.member(current), state.identity.level))
                {
                    trap.spellId = 0;
                    result.clearTrapFaceIndex = sourceCell.centralFloorFaceIndex;
                    result.message = text(BreachMazeInteractionTextSource::LocalString, 4);
                    result.faceAnimation = BreachMazeInteractionFaceAnimation{current, 4};
                    party.requestSpeech(current, SpeechId::DisarmTrap);
                }
                else
                {
                    if (trap.nextShotGameMinute > context.gameMinutes)
                    {
                        return result;
                    }
                    require(context.gameMinutes <= MaximumGameMinutes - rules.trapCooldownGameMinutes,
                        "trap cooldown overflows the game clock");
                    trap.nextShotGameMinute = context.gameMinutes + rules.trapCooldownGameMinutes;
                    const uint32_t animation = uint32_t(random.intRange(5, 6));
                    if (request.currentMemberIndex)
                    {
                        result.faceAnimation = BreachMazeInteractionFaceAnimation{current, animation};
                        if (animation == 5)
                        {
                            party.requestSpeech(current, SpeechId::TrapExploded);
                        }
                    }
                    const double targetZ = double(context.partyFootZ) + random.intRange(1, 100);
                    require(context.partyX >= std::numeric_limits<int32_t>::min()
                        && double(context.partyX) <= std::numeric_limits<int32_t>::max()
                        && context.partyY >= std::numeric_limits<int32_t>::min()
                        && double(context.partyY) <= std::numeric_limits<int32_t>::max()
                        && targetZ >= std::numeric_limits<int32_t>::min()
                        && targetZ <= std::numeric_limits<int32_t>::max(),
                        "trap target is outside integer coordinates");
                    projectile = TrapProjectile{*trap.spellId,
                        int32_t(double(*cell % BreachMazeTemplateSide) * BreachMazeCellWidth),
                        int32_t(-double(*cell / BreachMazeTemplateSide) * BreachMazeCellWidth),
                        int32_t(context.partyX), int32_t(context.partyY), int32_t(targetZ)};
                    result.message = text(BreachMazeInteractionTextSource::LocalString, 5);
                    if (random.intRange(1, 3) == 1)
                    {
                        trap.spellId = 0;
                        result.clearTrapFaceIndex = sourceCell.centralFloorFaceIndex;
                    }
                }
                candidate.random = random.cursor();
            }
        }
        else
        {
            const size_t slot = *decorationSlot;
            const BreachMazeDecorationInitialState &source = population.decorations[slot].interaction;
            BreachMazeDecorationState &decoration = candidate.decorations[slot];
            FixedItemGenerationRandom random(decoration.random);
            const uint32_t variable = source.eventVariable;
            switch (source.handler)
            {
                case BreachMazeDecorationHandler::Crystal:
                    if (decoration.used)
                    {
                        return result;
                    }
                    if (skillLevel(*party.member(current), "Perception") >= 7)
                    {
                        require(party.itemTable() != nullptr
                            && party.itemTable()->get(source.fixedRewardItemId) != nullptr, "missing crystal item");
                        retainItem(party, ItemGenerator::makeInventoryItem(source.fixedRewardItemId,
                            *party.itemTable(), ItemGenerationMode::Generic), 0);
                        decoration.used = true;
                        result.hideDecorationSlot = uint16_t(slot);
                        result.faceAnimation = BreachMazeInteractionFaceAnimation{current, 36};
                    }
                    else
                    {
                        const uint32_t animation = random.intRange(1, 2) == 1 ? 45 : 53;
                        result.faceAnimation = BreachMazeInteractionFaceAnimation{current, animation};
                        if (animation == 53)
                        {
                            party.requestSpeech(current, SpeechId::Beg);
                        }
                    }
                    decoration.random = random.cursor();
                    break;
                case BreachMazeDecorationHandler::ReagentBucket:
                    if (decoration.used)
                    {
                        result.message = text(BreachMazeInteractionTextSource::NpcText, 2135);
                        return result;
                    }
                    retainItem(party, generateItem(party, decoration.random, 1, 45), current);
                    decoration.used = true;
                    break;
                case BreachMazeDecorationHandler::MiscBag:
                    if (decoration.used)
                    {
                        return result;
                    }
                    if (random.intRange(1, 2) == 2)
                    {
                        decoration.random = random.cursor();
                        retainItem(party, generateItem(party, decoration.random, 1, 22), current);
                    }
                    else
                    {
                        decoration.random = random.cursor();
                        result.message = text(BreachMazeInteractionTextSource::NpcText, 2123);
                    }
                    decoration.used = true;
                    result.hideDecorationSlot = uint16_t(slot);
                    break;
                case BreachMazeDecorationHandler::FoodSack:
                {
                    if (decoration.used)
                    {
                        return result;
                    }
                    const int food = random.intRange(0, 2);
                    if (food > 0)
                    {
                        party.addFood(food);
                    }
                    else
                    {
                        result.message = text(BreachMazeInteractionTextSource::NpcText, 2121);
                    }
                    decoration.used = true;
                    decoration.random = random.cursor();
                    result.hideDecorationSlot = uint16_t(slot);
                    break;
                }
                case BreachMazeDecorationHandler::AttributeBarrel:
                {
                    constexpr std::array<uint32_t, 8> messages = {713, 714, 717, 716, 718, 719, 715, 720};
                    constexpr std::array<uint32_t, 7> notes = {17, 18, 19, 20, 22, 21, 23};
                    result.message = text(BreachMazeInteractionTextSource::NpcText,
                        messages[decoration.used ? 0 : variable]);
                    if (decoration.used)
                    {
                        return result;
                    }
                    addVariable(events, party, context.scene, 31 + variable, 2, current);
                    addAutonote(events, party, context.scene, notes[variable - 1], current);
                    decoration.used = true;
                    break;
                }
                case BreachMazeDecorationHandler::ResistanceCauldron:
                {
                    constexpr std::array<uint32_t, 5> messages = {721, 724, 723, 725, 722};
                    constexpr std::array<uint32_t, 4> resistances = {1, 2, 3, 0};
                    constexpr std::array<uint32_t, 4> notes = {24, 25, 26, 27};
                    result.message = text(BreachMazeInteractionTextSource::NpcText,
                        messages[decoration.used ? 0 : variable]);
                    if (decoration.used)
                    {
                        return result;
                    }
                    addVariable(events, party, context.scene,
                        uint32_t(EvtVariable::FireResistance) + resistances[variable - 1], 2, current);
                    addAutonote(events, party, context.scene, notes[variable - 1], current);
                    decoration.used = true;
                    break;
                }
                case BreachMazeDecorationHandler::TrashHeap:
                {
                    const int roll = random.intRange(1, 3);
                    const bool perception = skillLevel(*party.member(current), "Perception") >= 1;
                    if (decoration.used)
                    {
                        if (roll > 2 && !perception)
                        {
                            setCondition(events, party, context.scene, uint32_t(EvtVariable::DiseasedGreen), current);
                            result.message = text(BreachMazeInteractionTextSource::NpcText, 726);
                        }
                        else
                        {
                            result.message = text(BreachMazeInteractionTextSource::NpcText, 727);
                        }
                        decoration.random = random.cursor();
                    }
                    else
                    {
                        decoration.random = random.cursor();
                        retainItem(party, generateItem(party, decoration.random, roll, uint32_t(19 + roll)), current);
                        if (!perception)
                        {
                            // Preserve the source's raw condition IDs and text ordering, including its mismatches.
                            setCondition(events, party, context.scene, uint32_t(113 + roll), current);
                            result.message = text(BreachMazeInteractionTextSource::NpcText, uint32_t(725 + roll));
                        }
                        decoration.used = true;
                    }
                    break;
                }
                case BreachMazeDecorationHandler::Challenge:
                {
                    const Character &member = *party.member(current);
                    require(member.memberId != 0, "challenge requires a stable Party member identity");
                    const std::string key = "Merge.WonChallenges." + std::to_string(member.memberId)
                        + "." + std::to_string(variable);
                    const auto won = events.namedGlobalVars.find(key);
                    if (won != events.namedGlobalVars.end() && won->second != 0)
                    {
                        result.message = text(BreachMazeInteractionTextSource::NpcText, 733);
                        return result;
                    }
                    const int32_t actualStat = EventRuntime::getVariableValue(events,
                        EventRuntime::decodeVariable(38 + variable % 100), &party, current, &context.scene);
                    if (actualStat < source.challengeThreshold)
                    {
                        result.message = text(BreachMazeInteractionTextSource::NpcText, 729);
                        return result;
                    }
                    require(member.skillPoints <= uint32_t(std::numeric_limits<int32_t>::max())
                        - source.challengeSkillPointReward, "challenge skill-point reward overflows");
                    addVariable(events, party, context.scene, uint32_t(EvtVariable::NumSkillPoints),
                        source.challengeSkillPointReward, current);
                    events.namedGlobalVars[key] = 1;
                    result.message = text(BreachMazeInteractionTextSource::NpcText, 734);
                    result.message->formatArgument = "+" + std::to_string(source.challengeSkillPointReward);
                    break;
                }
                case BreachMazeDecorationHandler::None:
                    return result;
            }
        }

        std::string validationError;
        if (!validateBreachMazeInteractionState(candidate, population, validationError))
        {
            throw std::invalid_argument(validationError);
        }
        if (projectile)
        {
            require(context.scene.castEventSpell(projectile->spellId, population.interactionRules.trapSpellSkillLevel,
                population.interactionRules.trapSpellMastery, projectile->fromX, projectile->fromY, 0,
                projectile->toX, projectile->toY, projectile->toZ), "trap projectile could not be created");
        }
        state = std::move(candidate);
        context.party = std::move(party);
        context.events = std::move(events);
        result.changed = true;
    }
    catch (const std::exception &failure)
    {
        result = {};
        result.handled = true;
        result.error = failure.what();
    }
    return result;
}
}
