#include "game/maps/SaveGame.h"
#include "game/maps/MapDeltaData.h"
#include "game/events/EvtEnums.h"
#include "game/gameplay/CorpseLootRuntime.h"
#include "game/gameplay/MergeMonsterGenerationState.h"
#include "game/gameplay/PartyJumpState.h"

#include "game/indoor/IndoorMapData.h"
#include "game/outdoor/OutdoorMapData.h"

#include <fstream>
#include <iterator>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <limits>
#include <map>
#include <type_traits>

namespace
{
void writeJsonString(const std::string &text)
{
    constexpr char hex[] = "0123456789abcdef";
    std::cout << '"';
    for (const unsigned char character : text)
    {
        if (character == '"' || character == '\\')
        {
            std::cout << '\\' << character;
        }
        else if (character < 32)
        {
            std::cout << "\\u00" << hex[character >> 4] << hex[character & 15];
        }
        else
        {
            std::cout << character;
        }
    }
    std::cout << '"';
}

void writePartyJumpState(const OpenYAMM::Game::PartyJumpState &state)
{
    std::cout << "{\"pending_jump_requested\":" << state.pendingJumpRequested
        << ",\"pending_jump_velocity\":";
    if (state.pendingJumpVelocity) std::cout << *state.pendingJumpVelocity;
    else std::cout << "null";
    std::cout << ",\"pending_jump_lift\":" << state.pendingJumpLift
        << ",\"special_jump_active\":" << state.specialJumpActive
        << ",\"velocity_x\":" << state.velocityX << ",\"velocity_y\":" << state.velocityY
        << ",\"pending_special_jump\":" << state.pendingSpecialJump
        << ",\"pending_velocity_x\":" << state.pendingVelocityX
        << ",\"pending_velocity_y\":" << state.pendingVelocityY
        << ",\"pending_velocity_z\":" << state.pendingVelocityZ << '}';
}

template <typename PartySnapshot>
void writePartyMotion(const PartySnapshot &snapshot, const char *pWorldKind)
{
    const auto &position = snapshot.movementState;
    std::cout << "{\"world_kind\":";
    writeJsonString(pWorldKind);
    std::cout << ",\"x\":" << position.x << ",\"y\":" << position.y << ",\"foot_z\":" << position.footZ
        << ",\"vertical_velocity\":" << position.verticalVelocity
        << ",\"movement_accumulator_seconds\":" << snapshot.movementAccumulatorSeconds;
    if constexpr (std::is_same_v<PartySnapshot, OpenYAMM::Game::IndoorPartyRuntime::Snapshot>)
    {
        std::cout << ",\"grounded\":" << position.grounded
            << ",\"always_run_enabled\":" << snapshot.alwaysRunEnabled;
    }
    else
    {
        std::cout << ",\"airborne\":" << position.airborne;
    }
    std::cout << ",\"jump_state\":";
    writePartyJumpState(snapshot.jumpState);
    std::cout << '}';
}

void writeInventoryItem(const OpenYAMM::Game::InventoryItem &item)
{
    std::cout << "{\"item_id\":" << item.objectDescriptionId
        << ",\"quantity\":" << item.quantity
        << ",\"width\":" << int(item.width) << ",\"height\":" << int(item.height)
        << ",\"grid_x\":" << int(item.gridX) << ",\"grid_y\":" << int(item.gridY)
        << ",\"identified\":" << item.identified << ",\"broken\":" << item.broken
        << ",\"stolen\":" << item.stolen << ",\"standard_enchant_id\":" << item.standardEnchantId
        << ",\"standard_enchant_power\":" << item.standardEnchantPower
        << ",\"special_enchant_id\":" << item.specialEnchantId << ",\"artifact_id\":" << item.artifactId
        << ",\"rarity\":" << int(item.rarity) << ",\"current_charges\":" << item.currentCharges
        << ",\"max_charges\":" << item.maxCharges
        << ",\"temporary_bonus_seconds\":" << item.temporaryBonusRemainingSeconds << '}';
}

void writeItemRandomCursor(const OpenYAMM::Game::FixedItemGenerationCursor &cursor)
{
    std::cout << "{\"protocol\":" << uint32_t(cursor.protocol) << ",\"state\":" << cursor.state
        << ",\"raw_draw_count\":" << cursor.rawDrawCount << '}';
}

void writeBreachMazeInteraction(const std::optional<OpenYAMM::Game::BreachMazeInteractionState> &interaction)
{
    if (!interaction)
    {
        std::cout << "null";
        return;
    }
    const auto &state = *interaction;
    std::cout << "{\"version\":" << state.version << ",\"epoch\":" << state.identity.epoch
        << ",\"level\":" << state.identity.level << ",\"generation_serial\":" << state.identity.generationSerial
        << ",\"random\":";
    writeItemRandomCursor(state.random);
    std::cout << ",\"traps\":[";
    for (size_t index = 0; index < state.traps.size(); ++index)
    {
        const auto &trap = state.traps[index];
        std::cout << (index ? "," : "") << "{\"cell_id\":" << index + 1 << ",\"spell_id\":";
        if (trap.spellId) std::cout << *trap.spellId;
        else std::cout << "null";
        std::cout << ",\"next_shot_game_minute\":" << trap.nextShotGameMinute << '}';
    }
    std::cout << "],\"bookcase_used\":[";
    for (size_t index = 0; index < state.bookcaseUsed.size(); ++index)
    {
        std::cout << (index ? "," : "") << state.bookcaseUsed[index];
    }
    std::cout << "],\"decorations\":[";
    for (size_t index = 0; index < state.decorations.size(); ++index)
    {
        const auto &decoration = state.decorations[index];
        std::cout << (index ? "," : "") << "{\"slot_id\":" << index
            << ",\"used\":" << decoration.used << ",\"random\":";
        writeItemRandomCursor(decoration.random);
        std::cout << '}';
    }
    // Challenge receipts remain in the session's named_global_vars, shared across all floors.
    std::cout << "]}";
}

void writeBreachMaze(const std::optional<OpenYAMM::Game::BreachMazeRunData> &maze)
{
    if (!maze)
    {
        std::cout << "null";
        return;
    }
    const auto &run = *maze;
    std::cout << "{\"version\":" << run.version << ",\"seed_version\":" << run.seedVersion
        << ",\"template_fingerprint\":" << run.templateFingerprint << ",\"revision\":" << run.revision
        << ",\"root_seed\":" << run.rootSeed << ",\"epoch\":" << run.epoch
        << ",\"next_generation_serial\":" << run.nextGenerationSerial
        << ",\"current_level\":" << run.currentLevel << ",\"inside\":" << run.inside
        << ",\"direction\":" << int(run.direction) << ",\"floors\":[";
    bool first = true;
    for (const auto &[level, floor] : run.floors)
    {
        const auto &descriptor = floor.descriptor;
        const auto &runtime = floor.runtime;
        const auto &scene = runtime.scene;
        const auto &position = scene.partyRuntime.movementState;
        const auto &plan = *floor.population;
        std::cout << (first ? "" : ",") << "{\"level\":" << level
            << ",\"generation_serial\":" << descriptor.identity.generationSerial
            << ",\"layout_seed\":" << descriptor.seeds.layout << ",\"material_seed\":" << descriptor.seeds.material
            << ",\"population_seed\":" << descriptor.seeds.population
            << ",\"interaction_seed\":" << descriptor.seeds.interaction
            << ",\"layout_side\":" << int(descriptor.layout.side)
            << ",\"layout_random_state\":" << descriptor.layout.finalRandomState
            << ",\"layout_random_draw_count\":" << descriptor.layout.randomDrawCount
            << ",\"entrance_face\":" << descriptor.layout.endpoints.entrance.faceIndex
            << ",\"exit_face\":" << descriptor.layout.endpoints.exit.faceIndex
            << ",\"population_version\":" << plan.version << ",\"content_fix_version\":" << plan.contentFixVersion
            << ",\"catalog_fingerprint\":" << plan.catalogFingerprint
            << ",\"population_random_state\":" << plan.populationRandomState
            << ",\"population_random_draw_count\":" << plan.populationRandomDrawCount
            << ",\"planned_actors\":" << plan.actors.size() << ",\"planned_items\":" << plan.items.size()
            << ",\"placed_decorations\":" << plan.placedDecorationCount
            << ",\"actor_count\":" << scene.worldRuntime.mapActorAiStates.size()
            << ",\"game_minutes\":" << scene.worldRuntime.gameMinutes
            << ",\"x\":" << position.x << ",\"y\":" << position.y << ",\"foot_z\":" << position.footZ
            << ",\"yaw_radians\":" << runtime.yawRadians << ",\"pitch_radians\":" << runtime.pitchRadians
            << ",\"blood_splats\":" << scene.worldRuntime.bloodSplats.size()
            << ",\"timer_count\":" << scene.timers.size() << ",\"actor_rage\":[";
        for (size_t index = 0; index < runtime.actorRage.size(); ++index)
        {
            std::cout << (index ? "," : "") << unsigned(runtime.actorRage[index]);
        }
        std::cout << "],\"interaction\":";
        writeBreachMazeInteraction(runtime.interaction);
        std::cout << '}';
        first = false;
    }
    std::cout << "]}";
}

void writeActorCombatState(const OpenYAMM::Game::GameplayActorCombatState &state)
{
    using namespace OpenYAMM::Game;
    std::cout << "{\"mode\":" << int(state.mode()) << ",\"max_hp\":" << state.maximumHitPoints();
    if (const GameplayMergeResolvedActorCombat *pResolved = state.resolved())
    {
        const GameplayMergeResolvedActorCombat &resolved = *pResolved;
        const MergeBolsterCombatStats &combat = resolved.combat;
        std::cout << ",\"schema_version\":" << resolved.schemaVersion
            << ",\"profile_version\":" << resolved.profileVersion
            << ",\"random_version\":" << resolved.randomVersion << ",\"content_package\":";
        writeJsonString(resolved.contentPackageId);
        std::cout << ",\"content_schema\":" << resolved.contentPackageSchema << ",\"catalog_sha256\":";
        writeJsonString(resolved.catalogSha256);
        std::cout << ",\"profile_sha256\":";
        writeJsonString(resolved.profileSha256);
        std::cout << ",\"family_seed\":" << resolved.familySeed << ",\"actor_seed\":" << resolved.actorSeed
            << ",\"epoch\":" << resolved.generationEpoch << ",\"level\":" << resolved.generationLevel
            << ",\"generation_serial\":" << resolved.generationSerial
            << ",\"applied_modifiers\":" << resolved.appliedModifiers
            << ",\"player_ac_prepared\":" << resolved.playerArmorClassPrepared
            << ",\"bolster_amount_percent\":" << resolved.bolsterAmountPercent
            << ",\"player_ac_multiplier\":" << resolved.playerArmorClassMultiplier
            << ",\"monster_id\":" << combat.id << ",\"monster_level\":" << int(combat.level)
            << ",\"armor_class\":" << combat.armorClass << ",\"move_speed\":" << combat.moveSpeed
            << ",\"attack_recovery\":" << combat.attackRecovery << ",\"attacks\":[";
        for (const MergeBolsterAttack *pAttack : {&combat.attack1, &combat.attack2})
        {
            std::cout << (pAttack == &combat.attack1 ? "" : ",") << "{\"type\":" << int(pAttack->type)
                << ",\"dice_count\":" << int(pAttack->diceCount) << ",\"dice_sides\":" << int(pAttack->diceSides)
                << ",\"damage_add\":" << int(pAttack->damageAdd) << ",\"missile\":" << int(pAttack->missile) << '}';
        }
        std::cout << "],\"attack2_chance\":" << int(combat.attack2Chance) << ",\"spells\":[";
        for (size_t index = 0; index < combat.spells.size(); ++index)
        {
            const MergeBolsterSpell &spell = combat.spells[index];
            std::cout << (index ? "," : "") << "{\"id\":" << int(spell.id)
                << ",\"chance\":" << int(spell.chance) << ",\"packed_skill\":" << spell.packedSkill << '}';
        }
        std::cout << "],\"resistances\":[";
        for (size_t index = 0; index < combat.resistances.size(); ++index)
        {
            std::cout << (index ? "," : "") << combat.resistances[index];
        }
        std::cout << "],\"experience\":" << combat.rewards.experience
            << ",\"gold_dice_count\":" << int(combat.rewards.goldDiceCount)
            << ",\"gold_dice_sides\":" << int(combat.rewards.goldDiceSides)
            << ",\"item_chance\":" << int(combat.rewards.itemChance)
            << ",\"item_level\":" << int(combat.rewards.itemLevel)
            << ",\"item_type\":" << int(combat.rewards.itemType)
            << ",\"special\":{\"kind\":" << int(combat.special.kind)
            << ",\"a\":" << int(combat.special.a) << ",\"b\":" << int(combat.special.b)
            << ",\"c\":" << int(combat.special.c) << ",\"d\":" << combat.special.d << '}';
    }
    std::cout << '}';
}

void writeMonsterGeneration(const OpenYAMM::Game::MergeMonsterGenerationState &state)
{
    std::cout << "{\"version\":" << state.version
        << ",\"source_profile_version\":" << state.sourceProfileVersion
        << ",\"source_content_fix_version\":" << state.sourceContentFixVersion
        << ",\"bolster_profile_version\":" << state.bolsterProfileVersion
        << ",\"bolster_random_version\":" << state.bolsterRandomVersion
        << ",\"seed_version\":" << state.seedVersion << ",\"content_package\":";
    writeJsonString(state.contentPackageId);
    std::cout << ",\"content_schema\":" << state.contentPackageSchema << ",\"catalog_sha256\":";
    writeJsonString(state.catalogSha256);
    std::cout << ",\"profile_sha256\":";
    writeJsonString(state.profileSha256);
    std::cout << ",\"mechanical_fingerprint\":" << state.mechanicalFingerprint
        << ",\"descriptor_fingerprint\":" << state.descriptorFingerprint
        << ",\"population_seed\":" << state.populationSeed
        << ",\"epoch\":" << state.generationEpoch << ",\"level\":" << state.generationLevel
        << ",\"generation_serial\":" << state.generationSerial
        << ",\"next_actor_ordinal\":" << state.nextActorOrdinal
        << ",\"context\":{\"map_id\":" << state.context.map.mapId
        << ",\"map_kind\":" << int(state.context.map.kind)
        << ",\"map_spells\":" << state.context.map.spells
        << ",\"map_summons\":" << state.context.map.summons
        << ",\"map_level_shift\":" << state.context.map.levelShift
        << ",\"effective_party_level\":" << state.context.effectivePartyLevel
        << ",\"bolster_amount_percent\":" << state.context.bolsterAmountPercent
        << ",\"enabled\":" << state.context.enabled
        << ",\"templates_localized\":" << state.context.templatesLocalized
        << ",\"family_seed\":" << state.context.familySeed
        << ",\"multipliers\":{\"hp\":" << state.context.multipliers.hp
        << ",\"ac\":" << state.context.multipliers.ac
        << ",\"max_damage\":" << state.context.multipliers.maxDamage
        << ",\"move_speed\":" << state.context.multipliers.moveSpeed
        << ",\"player_ac\":" << state.context.multipliers.playerAc << "}},\"prepared_families\":[";
    for (size_t index = 0; index < state.preparedFamilies.size(); ++index)
    {
        const auto &family = state.preparedFamilies[index];
        std::cout << (index ? "," : "") << "{\"family_id\":" << family.familyId
            << ",\"family_seed\":" << family.input.familySeed
            << ",\"family_level\":" << family.familyLevel
            << ",\"raw_draw_count\":" << family.rawDrawCount << ",\"bolster_step\":";
        if (family.bolsterStep)
        {
            std::cout << int(*family.bolsterStep);
        }
        else
        {
            std::cout << "null";
        }
        std::cout << ",\"templates\":[";
        for (size_t member = 0; member < family.templates.size(); ++member)
        {
            const auto &combat = family.templates[member];
            std::cout << (member ? "," : "") << "{\"monster_id\":" << combat.id
                << ",\"original_hp\":" << family.input.members[member].original.fullHitPoints
                << ",\"full_hp\":" << combat.fullHitPoints
                << ",\"special_kind\":" << int(combat.special.kind)
                << ",\"special_a\":" << int(combat.special.a)
                << ",\"special_b\":" << int(combat.special.b)
                << ",\"special_c\":" << int(combat.special.c)
                << ",\"special_d\":" << combat.special.d << '}';
        }
        std::cout << "]}";
    }
    std::cout << "]}";
}

void writeChestItems(const std::vector<OpenYAMM::Game::GameplayChestItemState> &items)
{
    std::cout << '[';
    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto &item = items[i];
        std::cout << (i == 0 ? "" : ",") << "{\"item_id\":" << item.itemId << ",\"quantity\":" << item.quantity
            << ",\"gold\":" << item.goldAmount << ",\"gold_rolls\":" << item.goldRollCount
            << ",\"is_gold\":" << item.isGold << ",\"width\":" << int(item.width)
            << ",\"height\":" << int(item.height) << ",\"grid_x\":" << int(item.gridX)
            << ",\"grid_y\":" << int(item.gridY) << ",\"record_index\":" << item.itemRecordIndex
            << ",\"item\":";
        writeInventoryItem(item.item);
        std::cout << '}';
    }
    std::cout << ']';
}

void writeChestView(const std::optional<OpenYAMM::Game::GameplayChestViewState> &view)
{
    if (!view)
    {
        std::cout << "null";
        return;
    }
    std::cout << "{\"id\":" << view->chestId << ",\"type\":" << view->chestTypeId << ",\"flags\":" << view->flags
        << ",\"grid_width\":" << int(view->gridWidth) << ",\"grid_height\":" << int(view->gridHeight)
        << ",\"record_capacity\":" << view->itemRecordCapacity << ",\"items\":";
    writeChestItems(view->items);
    std::cout << ",\"hidden_items\":";
    writeChestItems(view->hiddenItems);
    std::cout << '}';
}

void writeEquipmentRuntime(const OpenYAMM::Game::EquippedItemRuntimeState &item)
{
    std::cout << "{\"identified\":" << item.identified << ",\"broken\":" << item.broken
        << ",\"stolen\":" << item.stolen << ",\"standard_enchant_id\":" << item.standardEnchantId
        << ",\"standard_enchant_power\":" << item.standardEnchantPower
        << ",\"special_enchant_id\":" << item.specialEnchantId << ",\"artifact_id\":" << item.artifactId
        << ",\"rarity\":" << int(item.rarity) << ",\"current_charges\":" << item.currentCharges
        << ",\"max_charges\":" << item.maxCharges
        << ",\"temporary_bonus_seconds\":" << item.temporaryBonusRemainingSeconds << '}';
}

// SaveGame serializes these seven resistance channels for both numeric values and immunities.
template <typename ResistanceSet>
void writeSavedResistances(const ResistanceSet &value)
{
    std::cout << "{\"fire\":" << value.fire << ",\"air\":" << value.air
        << ",\"water\":" << value.water << ",\"earth\":" << value.earth
        << ",\"mind\":" << value.mind << ",\"body\":" << value.body
        << ",\"spirit\":" << value.spirit << '}';
}

void writeStatBonuses(const OpenYAMM::Game::CharacterStatBonuses &value)
{
    std::cout << "{\"might\":" << value.might << ",\"intellect\":" << value.intellect
        << ",\"personality\":" << value.personality << ",\"endurance\":" << value.endurance
        << ",\"speed\":" << value.speed << ",\"accuracy\":" << value.accuracy << ",\"luck\":" << value.luck
        << ",\"max_health\":" << value.maxHealth << ",\"max_spell_points\":" << value.maxSpellPoints
        << ",\"armor_class\":" << value.armorClass << ",\"melee_attack\":" << value.meleeAttack
        << ",\"ranged_attack\":" << value.rangedAttack << ",\"melee_damage\":" << value.meleeDamage
        << ",\"ranged_damage\":" << value.rangedDamage << ",\"resistances\":";
    writeSavedResistances(value.resistances);
    std::cout << '}';
}

void writePendingItemDeliveries(const std::vector<OpenYAMM::Game::Party::PendingItemDelivery> &deliveries)
{
    std::cout << '[';
    for (size_t index = 0; index < deliveries.size(); ++index)
    {
        const auto &delivery = deliveries[index];
        std::cout << (index ? "," : "") << "{\"recipient\":" << delivery.recipientMemberIndex
            << ",\"allow_drop_protected_held_item\":" << delivery.allowDropProtectedHeldItem << ",\"item\":";
        writeInventoryItem(delivery.item);
        std::cout << '}';
    }
    std::cout << ']';
}

void writeCharacterBuffs(
    const std::array<OpenYAMM::Game::CharacterBuffState, OpenYAMM::Game::CharacterBuffCount> &buffs)
{
    std::cout << '[';
    for (size_t index = 0; index < buffs.size(); ++index)
    {
        const auto &buff = buffs[index];
        std::cout << (index ? "," : "") << "{\"slot\":" << index
            << ",\"remaining_seconds\":" << buff.remainingSeconds
            << ",\"periodic_accumulator_seconds\":" << buff.periodicAccumulatorSeconds
            << ",\"spell_id\":" << buff.spellId << ",\"skill_level\":" << buff.skillLevel
            << ",\"skill_mastery\":" << int(buff.skillMastery) << ",\"power\":" << buff.power
            << ",\"caster_member_index\":" << buff.casterMemberIndex
            << ",\"caster_member_id\":" << buff.casterMemberId << '}';
    }
    std::cout << ']';
}

void writeCharacter(const OpenYAMM::Game::Character &member)
{
    std::cout << "{\"name\":";
    writeJsonString(member.name);
    std::cout << ",\"biography\":";
    writeJsonString(member.biography);
    std::cout << ",\"role\":";
    writeJsonString(member.role);
    std::cout << ",\"class\":";
    writeJsonString(member.className);
    std::cout << ",\"portrait_texture_name\":";
    writeJsonString(member.portraitTextureName);
    std::cout << ",\"character_data_id\":" << member.characterDataId
        << ",\"member_id\":" << member.memberId
        << ",\"roster_id\":" << member.rosterId
        << ",\"voice_id\":" << member.voiceId << ",\"birth_year\":" << member.birthYear
        << ",\"portrait_picture_id\":" << member.portraitPictureId
        << ",\"portrait_state\":" << int(member.portraitState)
        << ",\"portrait_elapsed_ticks\":" << member.portraitElapsedTicks
        << ",\"portrait_duration_ticks\":" << member.portraitDurationTicks
        << ",\"portrait_sequence_counter\":" << member.portraitSequenceCounter
        << ",\"portrait_image_index\":" << member.portraitImageIndex
        << ",\"race_id\":" << member.raceId << ",\"sex_id\":" << member.sexId
        << ",\"level\":" << member.level << ",\"experience\":" << member.experience
        << ",\"skill_points\":" << member.skillPoints
        << ",\"might\":" << member.might << ",\"intellect\":" << member.intellect
        << ",\"personality\":" << member.personality << ",\"endurance\":" << member.endurance
        << ",\"accuracy\":" << member.accuracy << ",\"speed\":" << member.speed << ",\"luck\":" << member.luck
        << ",\"health\":" << member.health << ",\"max_health\":" << member.maxHealth
        << ",\"spell_points\":" << member.spellPoints << ",\"max_spell_points\":" << member.maxSpellPoints
        << ",\"conditions\":" << member.conditions.to_ullong()
        << ",\"recovery_seconds\":" << member.recoverySecondsRemaining
        << ",\"health_regen_accumulator\":" << member.healthRegenAccumulator
        << ",\"spell_regen_accumulator\":" << member.spellRegenAccumulator
        << ",\"armageddon_casts_today\":" << unsigned(member.armageddonCastsToday)
        << ",\"armor_class_modifier\":" << member.armorClassModifier
        << ",\"level_modifier\":" << member.levelModifier << ",\"age_modifier\":" << member.ageModifier
        << ",\"npcs2\":" << member.npcs2
        << ",\"equipment\":{\"off_hand\":" << member.equipment.offHand
        << ",\"main_hand\":" << member.equipment.mainHand << ",\"bow\":" << member.equipment.bow
        << ",\"armor\":" << member.equipment.armor << ",\"helm\":" << member.equipment.helm
        << ",\"belt\":" << member.equipment.belt << ",\"cloak\":" << member.equipment.cloak
        << ",\"gauntlets\":" << member.equipment.gauntlets << ",\"boots\":" << member.equipment.boots
        << ",\"amulet\":" << member.equipment.amulet << ",\"ring1\":" << member.equipment.ring1
        << ",\"ring2\":" << member.equipment.ring2 << ",\"ring3\":" << member.equipment.ring3
        << ",\"ring4\":" << member.equipment.ring4 << ",\"ring5\":" << member.equipment.ring5
        << ",\"ring6\":" << member.equipment.ring6 << '}'
        << ",\"equipment_runtime\":{";
    const auto &equipment = member.equipmentRuntime;
    const std::array<std::pair<const char *, const OpenYAMM::Game::EquippedItemRuntimeState *>, 16> slots = {{{
        "off_hand", &equipment.offHand}, {"main_hand", &equipment.mainHand}, {"bow", &equipment.bow},
        {"armor", &equipment.armor}, {"helm", &equipment.helm}, {"belt", &equipment.belt},
        {"cloak", &equipment.cloak}, {"gauntlets", &equipment.gauntlets}, {"boots", &equipment.boots},
        {"amulet", &equipment.amulet}, {"ring1", &equipment.ring1}, {"ring2", &equipment.ring2},
        {"ring3", &equipment.ring3}, {"ring4", &equipment.ring4}, {"ring5", &equipment.ring5},
        {"ring6", &equipment.ring6}}};
    for (size_t index = 0; index < slots.size(); ++index)
    {
        std::cout << (index == 0 ? "" : ",");
        writeJsonString(slots[index].first);
        std::cout << ':';
        writeEquipmentRuntime(*slots[index].second);
    }
    std::cout << "},\"skills\":[";
    const std::map<std::string, OpenYAMM::Game::CharacterSkill> skills(member.skills.begin(), member.skills.end());
    bool first = true;
    for (const auto &[name, skill] : skills)
    {
        std::cout << (first ? "" : ",") << "{\"id\":";
        writeJsonString(name);
        std::cout << ",\"name\":";
        writeJsonString(skill.name);
        std::cout << ",\"level\":" << skill.level << ",\"mastery\":" << int(skill.mastery) << '}';
        first = false;
    }
    std::vector<uint32_t> spells(member.knownSpellIds.begin(), member.knownSpellIds.end());
    std::sort(spells.begin(), spells.end());
    std::cout << "],\"known_spells\":[";
    for (size_t index = 0; index < spells.size(); ++index)
    {
        std::cout << (index == 0 ? "" : ",") << spells[index];
    }
    std::vector<uint32_t> awards(member.awards.begin(), member.awards.end());
    std::sort(awards.begin(), awards.end());
    std::cout << "],\"awards\":[";
    for (size_t index = 0; index < awards.size(); ++index)
    {
        std::cout << (index == 0 ? "" : ",") << awards[index];
    }
    std::cout << "],\"inventory\":[";
    for (size_t index = 0; index < member.inventory.size(); ++index)
    {
        std::cout << (index == 0 ? "" : ",");
        writeInventoryItem(member.inventory[index]);
    }
    std::cout << "],\"quick_spell_name\":";
    writeJsonString(member.quickSpellName);
    std::cout << ",\"attack_spell_name\":";
    writeJsonString(member.attackSpellName);
    std::cout << ",\"base_resistances\":";
    writeSavedResistances(member.baseResistances);
    std::cout << ",\"permanent_bonuses\":";
    writeStatBonuses(member.permanentBonuses);
    std::cout << ",\"temporary_event_bonuses\":";
    writeStatBonuses(member.temporaryEventBonuses);
    std::cout << ",\"magical_bonuses\":";
    writeStatBonuses(member.magicalBonuses);
    std::cout << ",\"permanent_immunities\":";
    writeSavedResistances(member.permanentImmunities);
    std::cout << ",\"magical_immunities\":";
    writeSavedResistances(member.magicalImmunities);
    std::cout << ",\"permanent_condition_immunities\":" << member.permanentConditionImmunities.to_ullong()
        << ",\"magical_condition_immunities\":" << member.magicalConditionImmunities.to_ullong()
        << ",\"condition_start_game_minutes\":[";
    for (size_t index = 0; index < member.conditionStartGameMinutes.size(); ++index)
    {
        std::cout << (index ? "," : "") << member.conditionStartGameMinutes[index];
    }
    std::cout << "],\"event_variables\":[";
    const std::map<uint16_t, int32_t> variables(member.eventVariables.begin(), member.eventVariables.end());
    first = true;
    for (const auto &[id, value] : variables)
    {
        std::cout << (first ? "" : ",") << "{\"id\":" << id << ",\"value\":" << value << '}';
        first = false;
    }
    std::vector<uint32_t> playerBits(member.playerBits.begin(), member.playerBits.end());
    std::sort(playerBits.begin(), playerBits.end());
    std::cout << "],\"player_bits\":[";
    for (size_t index = 0; index < playerBits.size(); ++index)
    {
        std::cout << (index ? "," : "") << playerBits[index];
    }
    std::cout << "],\"timed_primary_stat_bonuses\":[";
    for (size_t index = 0; index < member.timedPrimaryStatBonuses.size(); ++index)
    {
        const auto &bonus = member.timedPrimaryStatBonuses[index];
        std::cout << (index ? "," : "") << "{\"power\":" << bonus.power
            << ",\"remaining_seconds\":" << bonus.remainingSeconds << '}';
    }
    std::cout << "],\"lloyds_beacons\":[";
    for (size_t index = 0; index < member.lloydsBeacons.size(); ++index)
    {
        std::cout << (index ? "," : "");
        const auto &beacon = member.lloydsBeacons[index];
        if (!beacon)
        {
            std::cout << "null";
            continue;
        }
        std::cout << "{\"map_name\":";
        writeJsonString(beacon->mapName);
        std::cout << ",\"location_name\":";
        writeJsonString(beacon->locationName);
        std::cout << ",\"x\":" << beacon->x << ",\"y\":" << beacon->y << ",\"z\":" << beacon->z
            << ",\"direction_degrees\":" << beacon->directionDegrees
            << ",\"remaining_seconds\":" << beacon->remainingSeconds
            << ",\"preview_width\":" << beacon->previewWidth << ",\"preview_height\":" << beacon->previewHeight
            << ",\"preview_pixels_bgra_hex\":\"";
        constexpr char hex[] = "0123456789abcdef";
        for (uint8_t byte : beacon->previewPixelsBgra)
        {
            std::cout << hex[byte >> 4] << hex[byte & 15];
        }
        std::cout << "\"}";
    }
    std::cout << "]}";
}
}

int main(int argc, char **argv)
{
    if (argc == 4 && std::string(argv[1]) == "--indoor-delta")
    {
        std::ifstream mapInput(argv[2], std::ios::binary);
        std::ifstream deltaInput(argv[3], std::ios::binary);
        if (!mapInput || !deltaInput)
        {
            std::cerr << "Cannot open indoor map or delta\n";
            return 1;
        }
        const std::vector<uint8_t> mapBytes{
            std::istreambuf_iterator<char>(mapInput), std::istreambuf_iterator<char>()};
        const std::vector<uint8_t> deltaBytes{
            std::istreambuf_iterator<char>(deltaInput), std::istreambuf_iterator<char>()};
        const auto map = OpenYAMM::Game::IndoorMapDataLoader().loadFromBytes(mapBytes);
        if (!map)
        {
            std::cerr << "Cannot decode indoor map\n";
            return 1;
        }
        const auto delta = OpenYAMM::Game::MapDeltaDataLoader().loadIndoorFromBytes(deltaBytes, *map);
        if (!delta)
        {
            std::cerr << "Cannot decode indoor delta\n";
            return 1;
        }

        constexpr size_t RawItemRecordSize = 36;
        std::cout << "{\"chests\":[";
        for (size_t chestIndex = 0; chestIndex < delta->chests.size(); ++chestIndex)
        {
            const auto &chest = delta->chests[chestIndex];
            std::cout << (chestIndex == 0 ? "" : ",") << "{\"id\":" << chestIndex
                << ",\"type\":" << chest.chestTypeId << ",\"flags\":" << chest.flags
                << ",\"raw_item_ids\":[";
            const size_t recordCount = chest.rawItems.size() / RawItemRecordSize;
            for (size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex)
            {
                int32_t rawItemId = 0;
                std::memcpy(
                    &rawItemId,
                    chest.rawItems.data() + recordIndex * RawItemRecordSize,
                    sizeof(rawItemId));
                std::cout << (recordIndex == 0 ? "" : ",") << rawItemId;
            }
            std::cout << "]}";
        }
        std::cout << "],\"actors\":[";
        for (size_t actorIndex = 0; actorIndex < delta->actors.size(); ++actorIndex)
        {
            const auto &actor = delta->actors[actorIndex];
            std::cout << (actorIndex == 0 ? "" : ",") << "{\"id\":" << actorIndex << ",\"name\":";
            writeJsonString(actor.name);
            std::cout << ",\"monster_info_id\":" << actor.monsterInfoId
                << ",\"monster_id\":" << actor.monsterId << ",\"hp\":" << actor.hp
                << ",\"attributes\":" << actor.attributes << ",\"hostility_type\":"
                << unsigned(actor.hostilityType) << ",\"group\":" << actor.group << ",\"ally\":" << actor.ally
                << ",\"carried_item_id\":" << actor.carriedItemId
                << ",\"position\":[" << actor.x << ',' << actor.y << ',' << actor.z << "]}";
        }
        std::cout << "],\"map_vars\":[";
        for (size_t index = 0; index < delta->eventVariables.mapVars.size(); ++index)
        {
            std::cout << (index == 0 ? "" : ",") << int(delta->eventVariables.mapVars[index]);
        }
        std::cout << "],\"doors\":[";
        for (size_t doorIndex = 0; doorIndex < delta->doors.size(); ++doorIndex)
        {
            const auto &door = delta->doors[doorIndex];
            std::cout << (doorIndex == 0 ? "" : ",") << "{\"slot\":" << door.slotIndex
                << ",\"id\":" << door.doorId << ",\"state\":" << door.state
                << ",\"face_ids\":[";
            for (size_t faceIndex = 0; faceIndex < door.faceIds.size(); ++faceIndex)
                std::cout << (faceIndex == 0 ? "" : ",") << door.faceIds[faceIndex];
            std::cout << "]}";
        }
        std::cout << "]}\n";
        return 0;
    }
    if ((argc == 3 && (std::string(argv[1]) == "--outdoor-events"
            || std::string(argv[1]) == "--outdoor-decorations"))
        || (argc == 4 && std::string(argv[1]) == "--outdoor-model"))
    {
        std::ifstream input(argv[2], std::ios::binary);
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto map = OpenYAMM::Game::OutdoorMapDataLoader().loadFromBytes(bytes);
        if (!map)
        {
            std::cerr << "Cannot decode outdoor map\n";
            return 1;
        }
        if (std::string(argv[1]) == "--outdoor-decorations")
        {
            std::cout << '[';
            for (size_t index = 0; index < map->entities.size(); ++index)
            {
                const auto &entity = map->entities[index];
                std::cout << (index ? "," : "") << "{\"index\":" << index
                    << ",\"decoration_id\":" << entity.decorationListId
                    << ",\"event\":" << entity.scriptEventId()
                    << ",\"x\":" << entity.x << ",\"y\":" << entity.y << ",\"z\":" << entity.z << '}';
            }
            std::cout << "]\n";
            return 0;
        }
        std::cout << "[";
        bool first = true;
        for (size_t modelId = 0; modelId < map->bmodels.size(); ++modelId)
        {
            if (argc == 4 && std::to_string(modelId) != argv[3]) continue;
            const auto &model = map->bmodels[modelId];
            for (size_t faceId = 0; faceId < model.faces.size(); ++faceId)
            {
                const auto &face = model.faces[faceId];
                if (argc == 3 && face.cogTriggeredNumber == 0 && face.cogTrigger == 0) continue;
                std::cout << (first ? "" : ",") << "{\"model\":" << modelId << ",\"face\":" << faceId
                    << ",\"event\":" << face.cogTriggeredNumber << ",\"trigger\":" << face.cogTrigger
                    << ",\"vertices\":[";
                first = false;
                for (size_t i = 0; i < face.vertexIndices.size(); ++i)
                {
                    const auto &vertex = model.vertices.at(face.vertexIndices[i]);
                    std::cout << (i == 0 ? "" : ",") << '[' << vertex.x << ',' << vertex.y << ',' << vertex.z << ']';
                }
                std::cout << "]}";
            }
        }
        std::cout << "]\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--indoor-map")
    {
        std::ifstream input(argv[2], std::ios::binary);
        if (!input)
        {
            std::cerr << "Cannot open indoor map\n";
            return 1;
        }
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto map = OpenYAMM::Game::IndoorMapDataLoader().loadFromBytes(bytes);
        if (!map)
        {
            std::cerr << "Cannot decode indoor map\n";
            return 1;
        }
        std::cout << "{\"vertices\":[";
        for (size_t i = 0; i < map->vertices.size(); ++i)
        {
            const auto &v = map->vertices[i];
            std::cout << (i == 0 ? "" : ",") << '[' << v.x << ',' << v.y << ',' << v.z << ']';
        }
        std::cout << "],\"faces\":[";
        for (size_t i = 0; i < map->faces.size(); ++i)
        {
            const auto &f = map->faces[i];
            std::cout << (i == 0 ? "" : ",") << "{\"id\":" << i << ",\"event\":" << f.cogTriggered
                << ",\"cog\":" << f.cogNumber
                << ",\"attributes\":" << f.attributes << ",\"type\":" << int(f.facetType)
                << ",\"sector\":" << f.roomNumber << ",\"back_sector\":" << f.roomBehindNumber
                << ",\"portal\":" << (f.isPortal ? "true" : "false") << ",\"texture\":";
            writeJsonString(f.textureName);
            std::cout << ",\"vertices\":[";
            for (size_t j = 0; j < f.vertexIndices.size(); ++j)
            {
                std::cout << (j == 0 ? "" : ",") << f.vertexIndices[j];
            }
            std::cout << "]}";
        }
        std::cout << "],\"entities\":[";
        for (size_t i = 0; i < map->entities.size(); ++i)
        {
            const auto &e = map->entities[i];
            std::cout << (i == 0 ? "" : ",") << "{\"id\":" << i << ",\"event\":" << e.scriptEventId()
                << ",\"position\":[" << e.x << ',' << e.y << ',' << e.z << "],\"name\":";
            writeJsonString(e.name);
            std::cout << '}';
        }
        std::cout << "]}\n";
        return 0;
    }
    if (argc != 2)
    {
        std::cerr << "Usage: openyamm_save_inspect SAVE_PATH | --indoor-map MAP.blv | --indoor-delta MAP.blv MAP.dlv | --outdoor-events MAP.odm | --outdoor-model MAP.odm MODEL_ID\n";
        return 2;
    }
    std::string error;
    const std::optional<OpenYAMM::Game::GameSaveData> save =
        OpenYAMM::Game::loadGameDataFromPath(argv[1], error);
    if (!save)
    {
        std::cerr << error << '\n';
        return 1;
    }
    const OpenYAMM::Game::OutdoorMoveState &position = save->outdoorParty.movementState;
    const OpenYAMM::Game::BreachMazeFloorState *pActiveMazeFloor = save->breachMaze && save->breachMaze->inside
        ? &save->breachMaze->floors.at(save->breachMaze->currentLevel) : nullptr;
    const auto &activeIndoor = pActiveMazeFloor ? pActiveMazeFloor->runtime.scene : save->indoorScene;
    const OpenYAMM::Game::IndoorMoveState &indoorPosition = activeIndoor.partyRuntime.movementState;
    const float indoorYaw = pActiveMazeFloor ? pActiveMazeFloor->runtime.yawRadians : save->indoorCameraYawRadians;
    const float indoorPitch = pActiveMazeFloor ? pActiveMazeFloor->runtime.pitchRadians : save->indoorCameraPitchRadians;
    const bool outdoor = save->currentSceneKind == OpenYAMM::Game::SceneKind::Outdoor;
    std::vector<uint32_t> questBits(save->party.questBits.begin(), save->party.questBits.end());
    std::sort(questBits.begin(), questBits.end());
    std::cout << std::boolalpha << std::setprecision(std::numeric_limits<double>::max_digits10);
    const uint16_t deathVariable = uint16_t(OpenYAMM::Game::EvtVariable::NumDeaths);
    const int deaths = save->party.eventVariables.contains(deathVariable)
        ? save->party.eventVariables.at(deathVariable) : 0;
    std::cout << "{\n  \"name\": ";
    writeJsonString(save->saveName);
    std::cout << ",\n  \"map\": ";
    writeJsonString(save->mapFileName);
    std::cout << ",\n  \"outdoor\": " << outdoor
        << ",\n  \"game_minutes\": " << save->savedGameMinutes
        << ",\n  \"party_members\": " << save->party.members.size()
        << ",\n  \"active_member\": " << save->party.activeMemberIndex
        << ",\n  \"next_member_id\": " << save->party.nextMemberId
        << ",\n  \"gold\": " << save->party.gold
        << ",\n  \"bank_gold\": " << save->party.bankGold
        << ",\n  \"deaths\": " << deaths
        << ",\n  \"food\": " << save->party.food
        << ",\n  \"x\": " << (outdoor ? position.x : indoorPosition.x)
        << ",\n  \"y\": " << (outdoor ? position.y : indoorPosition.y)
        << ",\n  \"foot_z\": " << (outdoor ? position.footZ : indoorPosition.footZ)
        << ",\n  \"yaw_radians\": " << (outdoor ? save->outdoorCameraYawRadians : indoorYaw)
        << ",\n  \"pitch_radians\": " << (outdoor ? save->outdoorCameraPitchRadians : indoorPitch)
        << ",\n  \"preview_bytes\": " << save->previewBmp.size()
        << ",\n  \"active_party_motion\": ";
    if (outdoor && save->hasOutdoorRuntimeState)
    {
        writePartyMotion(save->outdoorParty, "outdoor");
    }
    else if (save->currentSceneKind == OpenYAMM::Game::SceneKind::Indoor
        && (pActiveMazeFloor != nullptr || save->hasIndoorSceneState))
    {
        writePartyMotion(activeIndoor.partyRuntime, "indoor");
    }
    else
    {
        std::cout << "null";
    }
    const auto &activeEvents = outdoor ? save->outdoorWorld.eventRuntimeState : activeIndoor.eventRuntimeState;
    std::cout << ",\n  \"active_event_random_state\": ";
    if (activeEvents) std::cout << activeEvents->eventRandomState;
    else std::cout << "null";
    std::cout << ",\n  \"active_outdoor_light_time\": ";
    if (outdoor && save->hasOutdoorRuntimeState)
    {
        const std::optional<OpenYAMM::Game::OutdoorLightTimeOverride> lightTime = activeEvents
            ? activeEvents->outdoorLightTimeOverride : std::nullopt;
        std::cout << "{\"actual_game_minutes\":" << save->outdoorWorld.gameMinutes
            << ",\"light_game_minutes\":" << OpenYAMM::Game::projectedOutdoorLightGameMinutes(
                save->outdoorWorld.gameMinutes, lightTime)
            << ",\"override\":";
        if (lightTime)
        {
            std::cout << "{\"hour\":" << lightTime->hour << ",\"minute\":";
            if (lightTime->minute) std::cout << *lightTime->minute;
            else std::cout << "null";
            std::cout << '}';
        }
        else std::cout << "null";
        std::cout << '}';
    }
    else std::cout << "null";
    std::cout << ",\n  \"active_outdoor_decoration_flag_count\": "
        << (outdoor ? save->outdoorWorld.decorationFlags.size() : 0)
        << ",\n  \"active_outdoor_hidden_decoration_indices\": [";
    bool decorationSeparator = false;
    if (outdoor)
    {
        for (size_t index = 0; index < save->outdoorWorld.decorationFlags.size(); ++index)
        {
            if ((save->outdoorWorld.decorationFlags[index] & 0x0020u) != 0)
            {
                std::cout << (decorationSeparator ? "," : "") << index;
                decorationSeparator = true;
            }
        }
    }
    std::cout << "]";
    std::cout << ",\n  \"held_item_active\": " << save->heldInventoryItemActive
        << ",\n  \"held_item\": ";
    writeInventoryItem(save->heldInventoryItem);
    std::cout
        << ",\n  \"held_item_grab_cell_offset_x\": " << unsigned(save->heldInventoryItemGrabCellOffsetX)
        << ",\n  \"held_item_grab_cell_offset_y\": " << unsigned(save->heldInventoryItemGrabCellOffsetY)
        << ",\n  \"held_item_grab_offset_x\": " << save->heldInventoryItemGrabOffsetX
        << ",\n  \"held_item_grab_offset_y\": " << save->heldInventoryItemGrabOffsetY
        << ",\n  \"quest_bits\": [";
    for (size_t index = 0; index < questBits.size(); ++index)
    {
        std::cout << (index == 0 ? "" : ", ") << questBits[index];
    }
    std::map<uint32_t, int32_t> autonotes;
    const auto collectAutonotes = [&](const auto &snapshot)
    {
        if (!snapshot.eventRuntimeState) return;
        for (const auto &[id, value] : snapshot.eventRuntimeState->variables)
        {
            const auto variable = OpenYAMM::Game::EventRuntime::decodeVariable(id);
            if (variable.kind == OpenYAMM::Game::EventRuntime::VariableKind::AutoNote
                && variable.index != 0 && value != 0)
                autonotes[variable.index] = value;
        }
    };
    collectAutonotes(save->outdoorWorld);
    collectAutonotes(save->indoorScene);
    for (const auto &[map, snapshot] : save->outdoorWorldStates) collectAutonotes(snapshot);
    for (const auto &[map, snapshot] : save->indoorSceneStates) collectAutonotes(snapshot);
    if (save->breachMaze)
    {
        for (const auto &[level, floor] : save->breachMaze->floors) collectAutonotes(floor.runtime.scene);
    }
    std::cout << "],\n  \"pending_quest_item_deliveries\": ";
    writePendingItemDeliveries(save->party.pendingItemDeliveries);
    std::cout << ",\n  \"autonotes\": [";
    bool firstAutonote = true;
    for (const auto &[id, value] : autonotes)
    {
        std::cout << (firstAutonote ? "" : ",") << "{\"id\":" << id << ",\"value\":" << value << '}';
        firstAutonote = false;
    }
    std::cout << "],\n  \"current_map_history\": [";
    const auto &historyState = outdoor ? save->outdoorWorld.eventRuntimeState : activeIndoor.eventRuntimeState;
    if (historyState)
    {
        const std::map<uint32_t, std::unordered_map<uint32_t, int32_t>> continents(
            historyState->historyEventTimesByContinent.begin(), historyState->historyEventTimesByContinent.end());
        bool first = true;
        for (const auto &[continent, events] : continents)
        {
            const std::map<uint32_t, int32_t> ordered(events.begin(), events.end());
            for (const auto &[id, time] : ordered)
            {
                std::cout << (first ? "" : ",") << "{\"continent\":" << continent
                    << ",\"event\":" << id << ",\"time\":" << time << '}';
                first = false;
            }
        }
    }
    std::cout << "],\n  \"map_histories\": {";
    bool firstHistoryMap = true;
    const auto writeMapHistory = [&](const std::string &name, const auto &snapshot)
    {
        if (!snapshot.eventRuntimeState) return;
        std::cout << (firstHistoryMap ? "" : ",");
        writeJsonString(name);
        std::cout << ":[";
        firstHistoryMap = false;
        const auto &state = *snapshot.eventRuntimeState;
        const std::map<uint32_t, std::unordered_map<uint32_t, int32_t>> continents(
            state.historyEventTimesByContinent.begin(), state.historyEventTimesByContinent.end());
        bool first = true;
        for (const auto &[continent, events] : continents)
        {
            const std::map<uint32_t, int32_t> ordered(events.begin(), events.end());
            for (const auto &[id, time] : ordered)
            {
                std::cout << (first ? "" : ",") << "{\"continent\":" << continent
                    << ",\"event\":" << id << ",\"time\":" << time << '}';
                first = false;
            }
        }
        std::cout << ']';
    };
    const std::map<std::string, OpenYAMM::Game::OutdoorWorldRuntime::Snapshot> historyOutdoors(
        save->outdoorWorldStates.begin(), save->outdoorWorldStates.end());
    const std::map<std::string, OpenYAMM::Game::IndoorSceneRuntime::Snapshot> historyIndoors(
        save->indoorSceneStates.begin(), save->indoorSceneStates.end());
    for (const auto &[map, snapshot] : historyOutdoors) writeMapHistory("outdoor:" + map, snapshot);
    for (const auto &[map, snapshot] : historyIndoors) writeMapHistory("indoor:" + map, snapshot);
    if (save->breachMaze)
    {
        for (const auto &[level, floor] : save->breachMaze->floors)
        {
            writeMapHistory("maze:level:" + std::to_string(level), floor.runtime.scene);
        }
    }
    std::cout << "},\n  \"npc_greeting_overrides\": [";
    const std::map<uint32_t, uint32_t> npcGreetings(save->party.npcGreetingOverrides.begin(),
        save->party.npcGreetingOverrides.end());
    bool firstGreeting = true;
    for (const auto &[npc, greeting] : npcGreetings)
    {
        std::cout << (firstGreeting ? "" : ",") << "{\"npc\":" << npc << ",\"greeting\":" << greeting << '}';
        firstGreeting = false;
    }
    std::cout << "],\n  \"npc_greeting_display_counts\": {";
    const std::map<uint32_t, uint32_t> greetingCounts(save->party.npcGreetingDisplayCounts.begin(),
        save->party.npcGreetingDisplayCounts.end());
    bool firstGreetingCount = true;
    for (const auto &[npc, count] : greetingCounts)
    {
        std::cout << (firstGreetingCount ? "" : ","); writeJsonString(std::to_string(npc));
        std::cout << ':' << count; firstGreetingCount = false;
    }
    std::cout << "},\n  \"npc_house_overrides\": [";
    const std::map<uint32_t, uint32_t> npcHouses(save->party.npcHouseOverrides.begin(),
        save->party.npcHouseOverrides.end());
    bool firstHouse = true;
    for (const auto &[id, house] : npcHouses)
    {
        std::cout << (firstHouse ? "" : ",") << "{\"npc\":" << id << ",\"house\":" << house << '}';
        firstHouse = false;
    }
    std::cout << "],\n  \"unavailable_npc_ids\": [";
    std::vector<uint32_t> unavailable(save->party.unavailableNpcIds.begin(), save->party.unavailableNpcIds.end());
    std::sort(unavailable.begin(), unavailable.end());
    for (size_t index = 0; index < unavailable.size(); ++index)
        std::cout << (index ? "," : "") << unavailable[index];
    std::cout << "],\n  \"npc_topic_overrides\": [";
    const std::map<uint32_t, std::unordered_map<uint32_t, uint32_t>> npcTopics(
        save->party.npcTopicOverrides.begin(), save->party.npcTopicOverrides.end());
    bool firstTopic = true;
    for (const auto &[npcId, slots] : npcTopics)
    {
        const std::map<uint32_t, uint32_t> orderedSlots(slots.begin(), slots.end());
        for (const auto &[slot, topic] : orderedSlots)
        {
            std::cout << (firstTopic ? "" : ",") << "{\"npc_id\":" << npcId
                << ",\"slot\":" << slot << ",\"topic_id\":" << topic << '}';
            firstTopic = false;
        }
    }
    std::cout << "],\n  \"current_map_mercenaries\": [";
    const auto &activeEventState = outdoor ? save->outdoorWorld.eventRuntimeState : activeIndoor.eventRuntimeState;
    if (activeEventState)
    {
        using Recruit = OpenYAMM::Game::EventRuntimeState::GeneratedMercenaryRecruit;
        std::map<uint32_t, const Recruit *> recruits;
        for (const auto &[id, recruit] : activeEventState->generatedMercenaryRecruitsByNpcId)
            recruits.emplace(id, &recruit);
        bool firstRecruit = true;
        for (const auto &[id, pRecruit] : recruits)
        {
            std::cout << (firstRecruit ? "" : ",") << "{\"npc_id\":" << id
                << ",\"roster_id\":" << pRecruit->rosterId << ",\"house_id\":" << pRecruit->houseId
                << ",\"portrait_picture_id\":" << pRecruit->portraitPictureId
                << ",\"npc_picture_id\":" << pRecruit->npcPictureId << ",\"character\":";
            writeCharacter(pRecruit->character);
            std::cout << '}';
            firstRecruit = false;
        }
    }
    std::cout << "],\n  \"current_map_named_vars\": {";
    if (activeEventState)
    {
        const std::map<std::string, int32_t> ordered(
            activeEventState->namedMapVars.begin(), activeEventState->namedMapVars.end());
        bool first = true;
        for (const auto &[name, value] : ordered)
        {
            std::cout << (first ? "" : ",");
            writeJsonString(name);
            std::cout << ':' << value;
            first = false;
        }
    }
    std::cout << "},\n  \"named_global_vars\": {";
    const std::map<std::string, int32_t> orderedGlobals(save->namedGlobalVars.begin(), save->namedGlobalVars.end());
    bool firstGlobal = true;
    for (const auto &[name, value] : orderedGlobals)
    {
        std::cout << (firstGlobal ? "" : ",");
        writeJsonString(name);
        std::cout << ':' << value;
        firstGlobal = false;
    }
    std::cout << "},\n  \"house_stock_seed\": " << save->party.houseStockSeed
        << ",\n  \"house_stock\": [";
    std::vector<OpenYAMM::Game::Party::HouseStockState> stocks = save->party.houseStockStates;
    std::sort(stocks.begin(), stocks.end(), [](const auto &left, const auto &right)
        { return left.houseId < right.houseId; });
    for (size_t index = 0; index < stocks.size(); ++index)
    {
        const auto &stock = stocks[index];
        std::cout << (index == 0 ? "" : ",") << "{\"house_id\":" << stock.houseId
            << ",\"next_refresh_minutes\":" << stock.nextRefreshGameMinutes
            << ",\"refresh_sequence\":" << stock.refreshSequence
            << ",\"generation_version\":" << stock.generationVersion;
        const auto items = [](const char *pName, const auto &entries)
        {
            std::cout << ",\"" << pName << "\":[";
            for (size_t i = 0; i < entries.size(); ++i)
            {
                if (i) std::cout << ',';
                writeInventoryItem(entries[i]);
            }
            std::cout << ']';
        };
        items("standard", stock.standardStock);
        items("special", stock.specialStock);
        items("spellbooks", stock.spellbookStock);
        std::cout << '}';
    }
    std::cout << "],\n  \"member_inventory\": [";
    for (size_t memberIndex = 0; memberIndex < save->party.members.size(); ++memberIndex)
    {
        std::cout << (memberIndex == 0 ? "" : ", ") << '[';
        const auto &inventory = save->party.members[memberIndex].inventory;
        for (size_t itemIndex = 0; itemIndex < inventory.size(); ++itemIndex)
        {
            const OpenYAMM::Game::InventoryItem &item = inventory[itemIndex];
            std::cout << (itemIndex == 0 ? "" : ", ") << "{\"item_id\":" << item.objectDescriptionId
                << ",\"quantity\":" << item.quantity << '}';
        }
        std::cout << ']';
    }
    std::cout << "],\n  \"characters\": [";
    for (size_t index = 0; index < save->party.members.size(); ++index)
    {
        if (index != 0)
        {
            std::cout << ',';
        }
        writeCharacter(save->party.members[index]);
    }
    std::cout << "],\n  \"adventurers_inn\": [";
    for (size_t index = 0; index < save->party.adventurersInnMembers.size(); ++index)
    {
        const auto &member = save->party.adventurersInnMembers[index];
        std::cout << (index ? "," : "") << "{\"portrait_picture_id\":" << member.portraitPictureId
            << ",\"character\":";
        writeCharacter(member.character);
        std::cout << '}';
    }
    std::cout << "],\n  \"detached_members\": [";
    for (size_t index = 0; index < save->party.detachedMembers.size(); ++index)
    {
        const auto &member = save->party.detachedMembers[index];
        std::cout << (index ? "," : "") << "{\"owner_key\":";
        writeJsonString(member.ownerKey);
        std::cout << ",\"slot_id\":" << member.slotId << ",\"character\":";
        writeCharacter(member.character);
        std::cout << ",\"character_buffs\":";
        writeCharacterBuffs(member.characterBuffs);
        std::cout << ",\"pending_item_deliveries\":";
        writePendingItemDeliveries(member.pendingItemDeliveries);
        std::cout << '}';
    }
    std::cout << "],\n  \"completed_detachment_owners\": [";
    for (size_t index = 0; index < save->party.completedDetachmentOwners.size(); ++index)
    {
        std::cout << (index ? "," : "");
        writeJsonString(save->party.completedDetachmentOwners[index]);
    }
    std::cout << "],\n  \"character_buffs\": [";
    for (size_t index = 0; index < save->party.characterBuffs.size(); ++index)
    {
        std::cout << (index ? "," : "");
        writeCharacterBuffs(save->party.characterBuffs[index]);
    }
    std::cout << "],\n  \"party_buffs\": [";
    for (size_t index = 0; index < save->party.partyBuffs.size(); ++index)
    {
        const auto &buff = save->party.partyBuffs[index];
        std::cout << (index ? "," : "") << "{\"slot\":" << index
            << ",\"remaining_seconds\":" << buff.remainingSeconds << ",\"spell_id\":" << buff.spellId
            << ",\"skill_level\":" << buff.skillLevel << ",\"skill_mastery\":" << int(buff.skillMastery)
            << ",\"power\":" << buff.power << ",\"caster_member_index\":" << buff.casterMemberIndex << '}';
    }
    std::cout << "],\n  \"breach_alvar_companions\": ";
    if (save->breachAlvarCompanions)
    {
        const auto &state = *save->breachAlvarCompanions;
        const auto &receipt = state.receipt;
        std::cout << "{\"receipt\":{\"profile_version\":" << receipt.profileVersion
            << ",\"plan_version\":" << receipt.planVersion << ",\"seed\":" << receipt.seed
            << ",\"final_random_state\":" << receipt.finalRandomState
            << ",\"random_draw_count\":" << receipt.randomDrawCount
            << ",\"initial_active_count\":" << receipt.initialActiveCount
            << ",\"leader_level_at_split\":" << receipt.leaderLevelAtSplit
            << ",\"leader_member_id\":" << receipt.leaderMemberId << ",\"companions\":[";
        for (size_t index = 0; index < receipt.companions.size(); ++index)
        {
            const auto &entry = receipt.companions[index];
            // The typed receipt fixes array order to NPC772..775; no inferred custody or success flag.
            std::cout << (index ? "," : "") << "{\"npc_id\":" << 772 + index
                << ",\"member_id\":" << entry.memberId << ",\"returned\":" << entry.returned
                << ",\"generation\":";
            if (entry.generation)
            {
                const auto &generation = *entry.generation;
                std::cout << "{\"roster_id\":" << generation.request.rosterId
                    << ",\"level\":" << generation.request.level << ",\"seed\":" << generation.request.seed
                    << ",\"final_random_state\":" << generation.finalRandomState
                    << ",\"random_draw_count\":" << generation.randomDrawCount << '}';
            }
            else std::cout << "null";
            std::cout << '}';
        }
        std::cout << "]},\"friend_actor\":{\"canonical_map_key\":";
        writeJsonString(state.friendActor.canonicalMapKey);
        std::cout << ",\"actor_id\":" << state.friendActor.actorId
            << ",\"spawn_monster_id\":" << state.friendActor.spawnMonsterId << "}}";
    }
    else
    {
        std::cout << "null";
    }
    std::cout << ",\n  \"breach_maze\": ";
    writeBreachMaze(save->breachMaze);
    std::cout << ",\n  \"has_generic_indoor_scene\": " << save->hasIndoorSceneState
        << ",\n  \"generic_indoor_cache_keys\": [";
    const std::map<std::string, OpenYAMM::Game::IndoorSceneRuntime::Snapshot> genericIndoorCache(
        save->indoorSceneStates.begin(), save->indoorSceneStates.end());
    bool firstCacheKey = true;
    for (const auto &[map, state] : genericIndoorCache)
    {
        std::cout << (firstCacheKey ? "" : ",");
        writeJsonString(map);
        firstCacheKey = false;
    }
    std::cout << ']';
    std::map<std::string, const OpenYAMM::Game::IndoorSceneRuntime::Snapshot *> indoorStates;
    for (const auto &[map, state] : save->indoorSceneStates) indoorStates[map] = &state;
    if (!outdoor && save->hasIndoorSceneState) indoorStates[save->mapFileName] = &save->indoorScene;
    if (save->breachMaze)
    {
        for (const auto &[level, floor] : save->breachMaze->floors)
        {
            indoorStates["maze:level:" + std::to_string(level)] = &floor.runtime.scene;
        }
    }
    std::cout << ",\n  \"indoor_party_states\": {";
    bool firstPartyState = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstPartyState ? "" : ",");
        firstPartyState = false;
        writeJsonString(map);
        std::cout << ':';
        writePartyMotion(state->partyRuntime, "indoor");
    }
    std::cout << "},\n  \"indoor_actors\": {";
    bool firstMap = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":[";
        if (state->mapDeltaData)
        {
            for (size_t index = 0; index < state->mapDeltaData->actors.size(); ++index)
            {
                const auto &actor = state->mapDeltaData->actors[index];
                std::cout << (index == 0 ? "" : ",") << "{\"monster_id\":" << actor.monsterInfoId
                    << ",\"hp\":" << actor.hp << ",\"x\":" << actor.x << ",\"y\":" << actor.y
                    << ",\"z\":" << actor.z << ",\"attributes\":" << actor.attributes
                    << ",\"group\":" << actor.group;
                if (index < state->worldRuntime.mapActorAiStates.size())
                {
                    const auto &ai = state->worldRuntime.mapActorAiStates[index];
                    std::cout << ",\"motion_state\":" << int(ai.motionState)
                        << ",\"action_seconds\":" << ai.actionSeconds
                        << ",\"actor_id\":" << ai.actorId
                        << ",\"precise_x\":" << ai.preciseX << ",\"precise_y\":" << ai.preciseY
                        << ",\"precise_z\":" << ai.preciseZ << ",\"yaw\":" << ai.yawRadians
                        << ",\"velocity_x\":" << ai.velocityX << ",\"velocity_y\":" << ai.velocityY
                        << ",\"velocity_z\":" << ai.velocityZ << ",\"sector\":" << ai.sectorId
                        << ",\"grounded\":" << ai.grounded << ",\"hostile\":" << ai.hostileToParty
                        << ",\"animation\":" << int(ai.animationState)
                        << ",\"animation_ticks\":" << ai.animationTimeTicks
                        << ",\"recovery_seconds\":" << ai.recoverySeconds
                        << ",\"attack_cooldown\":" << ai.attackCooldownSeconds
                        << ",\"attack_impact_triggered\":" << ai.attackImpactTriggered;
                    std::cout << ",\"combat_state\":";
                    writeActorCombatState(ai.combatState);
                    std::cout << ",\"special_runtime\":{\"summoned_count\":" << ai.specialState.summonedCount
                        << ",\"replicated_count\":" << ai.specialState.replicatedCount
                        << ",\"random_cursor\":" << ai.specialState.randomCursor
                        << ",\"replication_pending\":" << ai.specialState.replicationPending
                        << ",\"replication_due_game_minutes\":" << ai.specialState.replicationDueGameMinutes
                        << ",\"materialize_remaining_seconds\":" << ai.specialState.materializeRemainingSeconds << '}';
                    std::cout << ",\"stolen_items\":[";
                    for (size_t stolenIndex = 0; stolenIndex < ai.stolenItems.size(); ++stolenIndex)
                    {
                        std::cout << (stolenIndex ? "," : "");
                        writeInventoryItem(ai.stolenItems[stolenIndex]);
                    }
                    std::cout << ']';
                }
                if (index < state->worldRuntime.mapActorCorpseViews.size())
                {
                    const auto &corpse = state->worldRuntime.mapActorCorpseViews[index];
                    std::cout << ",\"corpse_materialized\":" << corpse.has_value()
                        << ",\"corpse_consumed\":" << OpenYAMM::Game::isConsumedCorpseView(corpse);
                    if (corpse) std::cout << ",\"corpse_items\":" << corpse->items.size();
                }
                std::cout << '}';
            }
        }
        std::cout << ']';
    }
    std::cout << "},\n  \"indoor_monster_generation\": {";
    firstMap = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ':';
        if (state->worldRuntime.mergeMonsterGeneration)
        {
            writeMonsterGeneration(*state->worldRuntime.mergeMonsterGeneration);
        }
        else
        {
            std::cout << "null";
        }
    }
    std::cout << "},\n  \"indoor_ground_objects\": {";
    firstMap = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":[";
        if (state->mapDeltaData)
        {
            for (size_t index = 0; index < state->mapDeltaData->spriteObjects.size(); ++index)
            {
                const auto &object = state->mapDeltaData->spriteObjects[index];
                std::cout << (index ? "," : "") << "{\"index\":" << index
                    << ",\"sprite_id\":" << object.spriteId << ",\"object_id\":" << object.objectDescriptionId
                    << ",\"x\":" << object.x << ",\"y\":" << object.y << ",\"z\":" << object.z
                    << ",\"attributes\":" << object.attributes << ",\"raw_item\":[";
                for (size_t byte = 0; byte < object.rawContainingItem.size(); ++byte)
                    std::cout << (byte ? "," : "") << unsigned(object.rawContainingItem[byte]);
                std::cout << "],\"typed_item\":";
                if (object.containingItem) writeInventoryItem(*object.containingItem);
                else std::cout << "null";
                std::cout << ",\"typed_gold\":" << object.containingGoldAmount << '}';
            }
        }
        std::cout << ']';
    }
    std::cout << "},\n  \"indoor_chests\": {";
    firstMap = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        const auto &world = state->worldRuntime;
        std::cout << ":{\"seed\":" << world.sessionChestSeed << ",\"materialized\":[";
        for (size_t i = 0; i < world.materializedChestViews.size(); ++i)
        {
            std::cout << (i == 0 ? "" : ",");
            writeChestView(world.materializedChestViews[i]);
        }
        std::cout << "],\"active\":";
        writeChestView(world.activeChestView);
        std::cout << ",\"source_flags\":[";
        if (state->mapDeltaData)
        {
            for (size_t i = 0; i < state->mapDeltaData->chests.size(); ++i)
            {
                std::cout << (i == 0 ? "" : ",") << state->mapDeltaData->chests[i].flags;
            }
        }
        std::cout << "],\"map_vars\":[";
        if (state->eventRuntimeState)
        {
            const auto &vars = state->eventRuntimeState->mapVars;
            for (size_t i = 0; i < vars.size(); ++i)
            {
                std::cout << (i == 0 ? "" : ",") << unsigned(vars[i]);
            }
        }
        std::cout << "]}";
    }
    std::cout << "},\n  \"indoor_mechanisms\": {";
    firstMap = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":[";
        if (state->eventRuntimeState)
        {
            const std::map<uint32_t, OpenYAMM::Game::RuntimeMechanismState> mechanisms(
                state->eventRuntimeState->mechanisms.begin(), state->eventRuntimeState->mechanisms.end());
            bool first = true;
            for (const auto &[id, mechanism] : mechanisms)
            {
                std::cout << (first ? "" : ",") << "{\"id\":" << id << ",\"state\":" << mechanism.state
                    << ",\"elapsed_ms\":" << mechanism.timeSinceTriggeredMs
                    << ",\"distance\":" << mechanism.currentDistance << ",\"moving\":" << mechanism.isMoving << '}';
                first = false;
            }
        }
        std::cout << ']';
    }
    std::cout << "},\n  \"indoor_presentation\": {";
    firstMap = true;
    for (const auto &[map, state] : indoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":{\"textures\":[";
        if (state->eventRuntimeState)
        {
            const std::map<uint32_t, std::string> textures(state->eventRuntimeState->textureOverrides.begin(),
                state->eventRuntimeState->textureOverrides.end());
            bool first = true;
            for (const auto &[id, name] : textures)
            {
                std::cout << (first ? "" : ",") << "{\"id\":" << id << ",\"name\":";
                writeJsonString(name);
                std::cout << '}';
                first = false;
            }
        }
        std::cout << "],\"lights\":[";
        if (state->eventRuntimeState)
        {
            const std::map<uint32_t, bool> lights(state->eventRuntimeState->indoorLightsEnabled.begin(),
                state->eventRuntimeState->indoorLightsEnabled.end());
            bool first = true;
            for (const auto &[id, enabled] : lights)
            {
                std::cout << (first ? "" : ",") << "{\"id\":" << id << ",\"enabled\":" << enabled << '}';
                first = false;
            }
        }
        std::cout << "]}";
    }
    std::map<std::string, const OpenYAMM::Game::OutdoorWorldRuntime::Snapshot *> outdoorStates;
    for (const auto &[map, state] : save->outdoorWorldStates) outdoorStates[map] = &state;
    if (outdoor && save->hasOutdoorRuntimeState) outdoorStates[save->mapFileName] = &save->outdoorWorld;
    std::cout << "},\n  \"outdoor_world_items\": {";
    firstMap = true;
    for (const auto &[map, state] : outdoorStates)
    {
        std::cout << (firstMap ? "" : ","); firstMap = false; writeJsonString(map); std::cout << ":[";
        for (size_t index = 0; index < state->worldItems.size(); ++index)
        {
            const auto &worldItem = state->worldItems[index];
            std::cout << (index ? "," : "") << "{\"world_item_id\":" << worldItem.worldItemId
                << ",\"expired\":" << worldItem.isExpired << ",\"player_spawned\":" << worldItem.spawnedByPlayer
                << ",\"item\":";
            writeInventoryItem(worldItem.item); std::cout << '}';
        }
        std::cout << ']';
    }
    std::cout << "},\n  \"outdoor_actors\": {";
    firstMap = true;
    for (const auto &[map, state] : outdoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":[";
        for (size_t index = 0; index < state->mapActors.size(); ++index)
        {
            const auto &actor = state->mapActors[index];
            std::cout << (index == 0 ? "" : ",") << "{\"actor_id\":" << actor.actorId
                << ",\"monster_id\":" << actor.monsterId << ",\"hp\":" << actor.currentHp
                << ",\"max_hp\":" << actor.combatState.maximumHitPoints() << ",\"npc_id\":" << actor.npcId
                << ",\"group\":" << actor.group << ",\"ally\":" << actor.ally
                << ",\"hostility_type\":" << unsigned(actor.hostilityType)
                << ",\"hostile_to_party\":" << actor.hostileToParty << ",\"ai_state\":" << int(actor.aiState)
                << ",\"spawned_at_runtime\":" << actor.spawnedAtRuntime
                << ",\"dead\":" << actor.isDead << ",\"invisible\":" << actor.isInvisible
                << ",\"x\":" << actor.preciseX << ",\"y\":" << actor.preciseY << ",\"z\":" << actor.preciseZ
                << ",\"map_x\":" << actor.x << ",\"map_y\":" << actor.y << ",\"map_z\":" << actor.z
                << ",\"breach_alvar_companion\":" << actor.breachAlvarCompanion
                << ",\"breach_alvar_companion_removed\":" << actor.breachAlvarCompanionRemoved
                << ",\"breach_alvar_next_maintenance_game_minute\":" << actor.breachAlvarNextMaintenanceGameMinute;
            std::cout << ",\"combat_state\":";
            writeActorCombatState(actor.combatState);
            if (index < state->mapActorCorpseViews.size())
            {
                const auto &corpse = state->mapActorCorpseViews[index];
                std::cout << ",\"corpse_materialized\":" << corpse.has_value()
                    << ",\"corpse_consumed\":" << OpenYAMM::Game::isConsumedCorpseView(corpse);
                if (corpse) std::cout << ",\"corpse_items\":" << corpse->items.size();
            }
            std::cout << '}';
        }
        std::cout << ']';
    }
    std::cout << "},\n  \"outdoor_chests\": {";
    firstMap = true;
    for (const auto &[map, state] : outdoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":{\"materialized\":[";
        for (size_t index = 0; index < state->materializedChestViews.size(); ++index)
        {
            std::cout << (index ? "," : "");
            writeChestView(state->materializedChestViews[index]);
        }
        std::cout << "],\"active\":";
        writeChestView(state->activeChestView);
        std::cout << ",\"raw_item_ids\":[";
        for (size_t index = 0; index < state->chests.size(); ++index)
        {
            std::cout << (index ? ",[" : "[");
            const auto &bytes = state->chests[index].rawItems;
            for (size_t offset = 0; offset + 36 <= bytes.size(); offset += 36)
            {
                int32_t id = 0;
                std::memcpy(&id, bytes.data() + offset, sizeof(id));
                std::cout << (offset ? "," : "") << id;
            }
            std::cout << ']';
        }
        std::cout << "]}";
    }
    std::cout << "},\n  \"outdoor_event_vars\": {";
    firstMap = true;
    for (const auto &[map, state] : outdoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":[";
        if (state->eventRuntimeState)
        {
            const auto &vars = state->eventRuntimeState->mapVars;
            for (size_t index = 0; index < vars.size(); ++index)
                std::cout << (index ? "," : "") << unsigned(vars[index]);
        }
        std::cout << ']';
    }
    std::cout << "},\n  \"outdoor_mechanisms\": {";
    firstMap = true;
    for (const auto &[map, state] : outdoorStates)
    {
        std::cout << (firstMap ? "" : ",");
        firstMap = false;
        writeJsonString(map);
        std::cout << ":[";
        if (state->eventRuntimeState)
        {
            const std::map<uint32_t, OpenYAMM::Game::RuntimeMechanismState> mechanisms(
                state->eventRuntimeState->mechanisms.begin(), state->eventRuntimeState->mechanisms.end());
            bool first = true;
            for (const auto &[id, mechanism] : mechanisms)
            {
                std::cout << (first ? "" : ",") << "{\"id\":" << id << ",\"state\":" << mechanism.state
                    << ",\"elapsed_ms\":" << mechanism.timeSinceTriggeredMs
                    << ",\"distance\":" << mechanism.currentDistance << ",\"moving\":" << mechanism.isMoving << '}';
                first = false;
            }
        }
        std::cout << ']';
    }
    const auto &turn = save->turnBasedCombat;
    std::cout << "},\n  \"turn_based\": {";
    std::cout << "\"active\":" << turn.active;
    std::cout << ",\"stage\":" << int(turn.stage);
    std::cout << ",\"turnInitiative\":" << turn.turnInitiative;
    std::cout << ",\"aiTurnTimerTicks\":" << turn.aiTurnTimerTicks;
    std::cout << ",\"movementActionPoints\":" << turn.movementActionPoints;
    std::cout << ",\"pendingActions\":" << turn.pendingActions;
    std::cout << ",\"turnCount\":" << turn.turnCount;
    std::cout << ",\"actorActionTimerTicks\":" << turn.actorActionTimerTicks;
    std::cout << ",\"movementFinished\":" << turn.movementFinished;
    std::cout << ",\"waitingForActorActionResolution\":" << turn.waitingForActorActionResolution;
    std::cout << ",\"pendingGameTimeAdvanceMinutes\":" << turn.pendingGameTimeAdvanceMinutes;
    std::cout << ",\"member_recovery\":[";
    for (size_t index = 0; index < turn.memberTurnRecoverySeconds.size(); ++index)
    {
        std::cout << (index ? "," : "") << turn.memberTurnRecoverySeconds[index];
    }
    std::cout << "],\"queue\":[";
    for (size_t index = 0; index < turn.queue.size(); ++index)
    {
        const auto &entry = turn.queue[index];
        std::cout << (index ? "," : "") << "{\"kind\":" << int(entry.kind) << ",\"id\":" << entry.id
            << ",\"initiative\":" << entry.initiative << ",\"recovery\":" << entry.actionRecoverySeconds << '}';
    }
    std::cout << "]},\n  \"projectiles\": [";
    const auto &projectiles = outdoor ? save->outdoorWorld.projectiles
        : activeIndoor.worldRuntime.projectileState.projectiles;
    for (size_t index = 0; index < projectiles.size(); ++index)
    {
        const auto &p = projectiles[index];
        std::cout << (index ? "," : "") << '{';
        std::cout << "\"projectileId\":" << p.projectileId;
        std::cout << ",\"sourceKind\":" << int(p.sourceKind);
        std::cout << ",\"sourceId\":" << p.sourceId;
        std::cout << ",\"sourcePartyMemberIndex\":" << p.sourcePartyMemberIndex;
        std::cout << ",\"spellId\":" << p.spellId;
        std::cout << ",\"x\":" << p.x;
        std::cout << ",\"y\":" << p.y;
        std::cout << ",\"z\":" << p.z;
        std::cout << ",\"velocityX\":" << p.velocityX;
        std::cout << ",\"velocityY\":" << p.velocityY;
        std::cout << ",\"velocityZ\":" << p.velocityZ;
        std::cout << ",\"damage\":" << p.damage;
        std::cout << ",\"attackBonus\":" << p.attackBonus;
        std::cout << ",\"turnBasedPendingAction\":" << p.turnBasedPendingAction;
        std::cout << ",\"timeSinceCreatedTicks\":" << p.timeSinceCreatedTicks;
        std::cout << ",\"lifetimeTickAccumulator\":" << p.lifetimeTickAccumulator;
        std::cout << ",\"lifetimeTicks\":" << p.lifetimeTicks;
        std::cout << ",\"isSettled\":" << p.isSettled;
        std::cout << ",\"isExpired\":" << p.isExpired;
        std::cout << '}';
    }
    std::cout << "]\n}\n";
    return 0;
}
