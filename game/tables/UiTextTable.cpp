#include "game/tables/UiTextTable.h"
#include "game/party/SkillData.h"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
struct TextDefinition
{
    std::string_view key;
    const char *pEnglish;
};

constexpr std::array<TextDefinition, size_t(UiTextTable::Message::Count)> Definitions{{
    {"ui.empty_slot", "Empty"},
    {"ui.menu", "Menu"},
    {"ui.submit", "Submit"},
    {"ui.cancel", "Cancel"},
    {"ui.beacon_set", "Set Beacon"},
    {"ui.beacon_recall", "Recall Beacon"},
    {"ui.beacon_available", "Available"},
    {"ui.day", "Day"},
    {"ui.days", "Days"},
    {"ui.hour", "Hour"},
    {"ui.hours", "Hours"},
    {"ui.minute", "Minute"},
    {"ui.minutes", "Minutes"},
    {"ui.arcomage_resign", "Resign"},
    {"ui.arcomage_touch_hint", "Tap to play; swipe up to discard."},
    {"ui.arcomage_discard_hint", "Choose a card to discard."},
    {"ui.item_type", "Type: %s"},
    {"ui.item_stolen", "Stolen"},
    {"ui.sp_cost", "SP Cost"},
    {"ui.item_power", "Power"},
    {"ui.item_attack", "Attack"},
    {"ui.item_shoot", "Shoot"},
    {"ui.item_damage", "Damage"},
    {"ui.item_charges", "Charges"},
    {"ui.item_armor", "Armor"},
    {"ui.item_value", "Value"},
    {"ui.win_congratulations", "Congratulations!"},
    {"ui.win_total_time", "Total Time:"},
    {"ui.win_score", "Your score: %lu"},
    {"ui.win_character", "%s the Level %u %s"},
    {"ui.year", "Year"},
    {"ui.years", "Years"},
    {"ui.month", "Month"},
    {"ui.months", "Months"},
    {"ui.win_ending",
        "Excellent work! By thwarting the Destroyer of Worlds, you have pulled your world from the brink of "
        "unending oblivion. Not only may life continue, but a new peace reigns over Jadame. The mighty alliance "
        "you forged will see to the land's restoration and eventual prosperity."},
    {"ui.bank_balance", "Balance"},
    {"ui.bank_deposit", "Deposit"},
    {"ui.bank_withdraw", "Withdraw"},
    {"ui.bank_how_much", "How Much?"},
    {"ui.rest_heal_eight_hours", "Rest & Heal 8 Hours"},
    {"ui.rest_wait_without_healing", "Wait without healing"},
    {"ui.rest_wait_until_dawn", "Wait until Dawn"},
    {"ui.rest_wait_five_minutes", "Wait 5 Minutes"},
    {"ui.rest_wait_one_hour", "Wait 1 Hour"},
    {"ui.rest_exit", "Exit Rest"},
    {"ui.skill_normal", "Normal"},
    {"ui.skill_expert", "Expert"},
    {"ui.skill_master", "Master"},
    {"ui.skill_grandmaster", "Grandmaster"},
    {"ui.none", "None"},
    {"ui.weekday_monday", "Monday"},
    {"ui.weekday_tuesday", "Tuesday"},
    {"ui.weekday_wednesday", "Wednesday"},
    {"ui.weekday_thursday", "Thursday"},
    {"ui.weekday_friday", "Friday"},
    {"ui.weekday_saturday", "Saturday"},
    {"ui.weekday_sunday", "Sunday"},
    {"ui.month_january", "January"},
    {"ui.month_february", "February"},
    {"ui.month_march", "March"},
    {"ui.month_april", "April"},
    {"ui.month_may", "May"},
    {"ui.month_june", "June"},
    {"ui.month_july", "July"},
    {"ui.month_august", "August"},
    {"ui.month_september", "September"},
    {"ui.month_october", "October"},
    {"ui.month_november", "November"},
    {"ui.month_december", "December"},
    {"ui.save_autosave", "Autosave"},
    {"ui.save_quicksave", "Quicksave"},
    {"ui.save_slot", "Save"},
    {"ui.keyboard_forward", "Forward"},
    {"ui.keyboard_backward", "Backward"},
    {"ui.keyboard_left", "Left"},
    {"ui.keyboard_right", "Right"},
    {"ui.keyboard_yell", "Yell"},
    {"ui.keyboard_jump", "Jump"},
    {"ui.keyboard_combat", "Combat"},
    {"ui.keyboard_cast_ready", "Cast Ready"},
    {"ui.keyboard_attack", "Attack"},
    {"ui.keyboard_trigger", "Trigger"},
    {"ui.keyboard_cast", "Cast"},
    {"ui.keyboard_pass", "Pass"},
    {"ui.keyboard_char_cycle", "Char Cycle"},
    {"ui.keyboard_quest", "Quest"},
    {"ui.keyboard_quick_ref", "Quick Ref"},
    {"ui.keyboard_rest", "Rest"},
    {"ui.keyboard_history", "History"},
    {"ui.keyboard_use", "Use"},
    {"ui.keyboard_map_book", "Map Book"},
    {"ui.keyboard_always_run", "Always Run"},
    {"ui.keyboard_look_up", "Look Up"},
    {"ui.keyboard_look_down", "Look Down"},
    {"ui.keyboard_center_view", "Ctr. View"},
    {"ui.keyboard_zoom_in", "Zoom In"},
    {"ui.keyboard_zoom_out", "Zoom Out"},
    {"ui.keyboard_fly_up", "Fly Up"},
    {"ui.keyboard_fly_down", "Fly Down"},
    {"ui.keyboard_land", "Land"},
    {"ui.keyboard_double_speed", "Double Speed"},
    {"ui.keyboard_press_input", "Choose key"},
    {"ui.keyboard_binding_conflict", "Conflict"},
    {"ui.journal_potion_notes", "Potion Notes"},
    {"ui.journal_fountains", "Fountains"},
    {"ui.journal_obelisk_notes", "Obelisk Notes"},
    {"ui.journal_seer_notes", "Seer Notes"},
    {"ui.journal_miscellaneous", "Miscellaneous"},
    {"ui.journal_teacher_locations", "Teacher Locations"},
    {"ui.journal_current_quests", "Current Quests"},
    {"ui.journal_history", "History"},
    {"ui.journal_notes", "Notes"},
    {"ui.inventory_target_hint", "Select an item to cast; Exit cancels."},
    {"ui.skill_training_offer", "Become %s in %s for %lu gold"},
    {"ui.skill_training_learned", "%s is now a %s in %s."},
    {"ui.skill_training_requirement", "Your base %s must be at least %u to learn %s %s."},
    {"ui.skill_weapons", "Weapons"},
    {"ui.skill_magic", "Magic"},
    {"ui.skill_misc", "Misc"},
    {"ui.yes", "Yes"},
    {"ui.no", "No"},
    {"ui.join", "Join"},
    {"ui.more_information", "More Information"},
    {"ui.npc_dismiss", "Dismiss %s"},
    {"ui.confirm", "OK"},
    {"ui.transition_leave", "Do you wish to leave %s?"},
    {"ui.transition_enter", "Do you wish to enter %s?"},
    {"ui.transition_leave_area", "Do you wish to leave this area?"},
    {"ui.throne_serve_sentence", "Serve Sentence"},
    {"ui.disabled", "disabled"},
    {"ui.throne_no_fine", "You do not owe a fine."},
    {"ui.throne_sentence_served", "You have served one year in prison."},
    {"ui.finish_current_action", "Finish current action"},
    {"ui.spell_not_learned", "Spell not learned"},
    {"ui.spell_no_valid_target", "No valid target exists!"},
    {"ui.spell_select_actor", "Select actor for %s"},
    {"ui.spell_select_character", "Select character for %s"},
    {"ui.spell_select_ground", "Select ground point for %s"},
    {"ui.spell_select_target", "Select target for %s"},
    {"ui.spell_target_controls_mouse", "LMB cast  Esc cancel"},
    {"ui.spell_target_controls_touch_ground", "Drag target  Spell button cast  Pause cancel"},
    {"ui.spell_target_controls_touch_portrait", "Tap portrait  Menu cancel"},
    {"ui.spell_target_controls_touch_target", "Tap target  Menu cancel"},
    {"ui.town_portal_choose_destination", "Choose Town Portal destination"},
    {"ui.spell_cast", "Cast %s"},
    {"ui.high_magic_presence", "You feel high magic presence here."},
    {"ui.spell_unknown_quick", "Unknown quick spell"},
    {"ui.town_portal_data_missing", "Town Portal data missing"},
    {"ui.spell_not_enough_spell_points", "Not enough spell points"},
    {"ui.spell_failed", "Spell failed"},
    {"ui.spell_divine_intervention_blocked_by_gear", "Divine Intervention is blocked by equipped gear"},
    {"ui.spell_meteor_shower_indoors", "Can't cast Meteor Shower indoors!"},
    {"ui.spell_starburst_indoors", "Can't cast Starburst indoors!"},
    {"ui.spell_fly_indoors", "Can not cast Fly indoors!"},
    {"ui.spell_armageddon_indoors", "Can't cast Armageddon indoors!"},
    {"ui.spell_inferno_outdoors", "Can't cast Inferno outdoors!"},
    {"ui.spell_prismatic_light_outdoors", "Can't cast Prismatic Light outdoors!"},
    {"ui.spell_hostile_creatures_nearby", "There are hostile creatures nearby!"},
    {"ui.spell_wand_already_charged", "Wand already charged!"},
    {"ui.spell_item_quality_too_low", "Item is not of high enough quality"},
    {"ui.spell_summon_limit_reached", "This character can't summon any more monsters!"},
    {"ui.spell_not_skilled_enough", "Not skilled enough"},
    {"ui.spell_canceled", "Spell canceled"},
    {"ui.spell_unknown_attack", "Unknown attack spell"},
}};

bool hasExpectedParameters(const std::string &format, std::initializer_list<std::string_view> parameters)
{
    size_t nextOffset = 0;
    for (const std::string_view parameter : parameters)
    {
        const size_t offset = format.find('%', nextOffset);
        if (offset == std::string::npos || format.compare(offset, parameter.size(), parameter) != 0)
        {
            return false;
        }
        nextOffset = offset + parameter.size();
    }
    return format.find('%', nextOffset) == std::string::npos;
}

std::string formatStringParameter(std::string format, const std::string &value)
{
    if (!hasExpectedParameters(format, {"%s"}))
    {
        return {};
    }
    format.replace(format.find("%s"), 2, value);
    return format;
}

size_t itemTypeParameterOffset(const std::string &format)
{
    const size_t offset = format.find('%');
    return offset != std::string::npos
            && format.compare(offset, 2, "%s") == 0
            && format.find('%', offset + 2) == std::string::npos
        ? offset : std::string::npos;
}
}

UiTextTable::UiTextTable()
{
    for (size_t index = 0; index < Definitions.size(); ++index)
    {
        m_texts[index] = Definitions[index].pEnglish;
    }
}

bool UiTextTable::loadFromRows(const std::vector<std::vector<std::string>> &rows)
{
    // Migrate only complete older prefixes: the original 13 captions, the 16 with
    // Arcomage, the 19 with initial inspect labels, the 26 with item details,
    // the 35 with win text, the 39 with bank captions, the 45 with rest captions,
    // the 72 with mastery/calendar/save, the 103 with keyboard, the 112 with journal
    // the 115/116 with initial skill training captions and base-attribute requirements,
    // the 119 with skill category headings, the 122 with NPC confirmations,
    // the 132 with ordinary travel and fine captions,
    // and the 149 with spell feedback and target-selection captions.
    // Other missing-key shapes remain errors.
    constexpr size_t OriginalMessageCount = size_t(Message::ArcomageResign);
    constexpr size_t ArcomageMessageCount = size_t(Message::ItemTypeFormat);
    constexpr size_t InspectMessageCount = size_t(Message::ItemPower);
    constexpr size_t ItemDetailMessageCount = size_t(Message::Congratulations);
    constexpr size_t WinMessageCount = size_t(Message::BankBalance);
    constexpr size_t BankMessageCount = size_t(Message::RestHealEightHours);
    constexpr size_t RestMessageCount = size_t(Message::SkillNormal);
    constexpr size_t SaveMessageCount = size_t(Message::KeyboardForward);
    constexpr size_t KeyboardMessageCount = size_t(Message::JournalPotionNotes);
    constexpr size_t JournalMessageCount = size_t(Message::InventoryTargetHint);
    constexpr size_t TrainingMessageCount = size_t(Message::SkillTrainingRequirement);
    constexpr size_t TrainingRequirementMessageCount = size_t(Message::SkillWeapons);
    constexpr size_t SkillHeadingMessageCount = size_t(Message::Yes);
    constexpr size_t NpcConfirmationMessageCount = size_t(Message::MoreInformation);
    constexpr size_t TravelMessageCount = size_t(Message::FinishCurrentAction);
    constexpr size_t SpellMessageCount = size_t(Message::SpellFailed);
    const size_t schemaMessageCount = rows.size();
    if (schemaMessageCount != Definitions.size()
        && schemaMessageCount != OriginalMessageCount
        && schemaMessageCount != ArcomageMessageCount
        && schemaMessageCount != InspectMessageCount
        && schemaMessageCount != ItemDetailMessageCount
        && schemaMessageCount != WinMessageCount
        && schemaMessageCount != BankMessageCount
        && schemaMessageCount != RestMessageCount
        && schemaMessageCount != SaveMessageCount
        && schemaMessageCount != KeyboardMessageCount
        && schemaMessageCount != JournalMessageCount
        && schemaMessageCount != TrainingMessageCount
        && schemaMessageCount != TrainingRequirementMessageCount
        && schemaMessageCount != SkillHeadingMessageCount
        && schemaMessageCount != NpcConfirmationMessageCount
        && schemaMessageCount != TravelMessageCount
        && schemaMessageCount != SpellMessageCount)
    {
        return false;
    }
    decltype(m_texts) texts;
    for (const std::vector<std::string> &row : rows)
    {
        if (row.size() != 2 || row[1].empty() || row[1].size() > 4096
            || row[1].find_first_not_of(' ') == std::string::npos
            || std::any_of(row[1].begin(), row[1].end(), [](unsigned char character)
                { return character < 32 || character == 127; }))
        {
            return false;
        }
        const auto found = std::find_if(Definitions.begin(), Definitions.end(),
            [&row](const TextDefinition &definition) { return definition.key == row[0]; });
        if (found == Definitions.end())
        {
            return false;
        }
        const size_t index = size_t(found - Definitions.begin());
        if (index >= schemaMessageCount || !texts[index].empty()
            || (index == size_t(Message::ItemTypeFormat)
                && itemTypeParameterOffset(row[1]) == std::string::npos)
            || (index == size_t(Message::ScoreFormat) && !hasExpectedParameters(row[1], {"%lu"}))
            || (index == size_t(Message::CharacterFormat) && !hasExpectedParameters(row[1], {"%s", "%u", "%s"}))
            || (index == size_t(Message::SkillTrainingOffer) && !hasExpectedParameters(row[1], {"%s", "%s", "%lu"}))
            || (index == size_t(Message::SkillTrainingLearned) && !hasExpectedParameters(row[1], {"%s", "%s", "%s"}))
            || (index == size_t(Message::SkillTrainingRequirement)
                && !hasExpectedParameters(row[1], {"%s", "%u", "%s", "%s"}))
            || ((index == size_t(Message::NpcDismissFormat)
                    || index == size_t(Message::TransitionLeaveFormat)
                    || index == size_t(Message::TransitionEnterFormat)
                    || index == size_t(Message::SpellSelectActorFormat)
                    || index == size_t(Message::SpellSelectCharacterFormat)
                    || index == size_t(Message::SpellSelectGroundFormat)
                    || index == size_t(Message::SpellSelectTargetFormat)
                    || index == size_t(Message::SpellCastFormat))
                && !hasExpectedParameters(row[1], {"%s"})))
        {
            return false;
        }
        texts[index] = row[1];
    }
    for (size_t index = schemaMessageCount; index < Definitions.size(); ++index)
    {
        texts[index] = Definitions[index].pEnglish;
    }
    m_texts = std::move(texts);
    return true;
}

const std::string &UiTextTable::text(Message message) const
{
    static const std::string Empty;
    const size_t index = size_t(message);
    return index < m_texts.size() ? m_texts[index] : Empty;
}

const std::string &UiTextTable::skillMasteryName(SkillMastery mastery) const
{
    switch (mastery)
    {
        case SkillMastery::Normal:
            return text(Message::SkillNormal);
        case SkillMastery::Expert:
            return text(Message::SkillExpert);
        case SkillMastery::Master:
            return text(Message::SkillMaster);
        case SkillMastery::Grandmaster:
            return text(Message::SkillGrandmaster);
        default:
            return text(Message::None);
    }
}

std::string UiTextTable::formatItemType(const std::string &type) const
{
    std::string result = text(Message::ItemTypeFormat);
    const size_t offset = itemTypeParameterOffset(result);
    if (offset == std::string::npos)
    {
        return {};
    }
    result.replace(offset, 2, type);
    return result;
}

std::string UiTextTable::formatWinScore(uint64_t score) const
{
    std::string result = text(Message::ScoreFormat);
    if (!hasExpectedParameters(result, {"%lu"}))
    {
        return {};
    }
    result.replace(result.find("%lu"), 3, std::to_string(score));
    return result;
}

std::string UiTextTable::formatWinCharacter(
    const std::string &name, uint32_t level, const std::string &className) const
{
    std::string result = text(Message::CharacterFormat);
    if (!hasExpectedParameters(result, {"%s", "%u", "%s"}))
    {
        return {};
    }
    const size_t nameOffset = result.find("%s");
    const size_t levelOffset = result.find("%u", nameOffset + 2);
    const size_t classOffset = result.find("%s", levelOffset + 2);
    // Replace from right to left so percent signs in player names remain literal.
    result.replace(classOffset, 2, className);
    result.replace(levelOffset, 2, std::to_string(level));
    result.replace(nameOffset, 2, name);
    return result;
}

std::string UiTextTable::formatMasteryDescription(Message mastery, const std::string &description) const
{
    return description.empty() ? std::string{} : text(mastery) + ": " + description;
}

std::string UiTextTable::formatSkillTrainingOffer(
    const std::string &mastery, const std::string &skill, uint32_t cost) const
{
    std::string result = text(Message::SkillTrainingOffer);
    if (!hasExpectedParameters(result, {"%s", "%s", "%lu"}))
    {
        return {};
    }
    const size_t masteryOffset = result.find("%s");
    const size_t skillOffset = result.find("%s", masteryOffset + 2);
    const size_t costOffset = result.find("%lu", skillOffset + 2);
    result.replace(costOffset, 3, std::to_string(cost));
    result.replace(skillOffset, 2, skill);
    result.replace(masteryOffset, 2, mastery);
    return result;
}

std::string UiTextTable::formatSkillTrainingLearned(
    const std::string &name, const std::string &mastery, const std::string &skill) const
{
    std::string result = text(Message::SkillTrainingLearned);
    if (!hasExpectedParameters(result, {"%s", "%s", "%s"}))
    {
        return {};
    }
    const size_t nameOffset = result.find("%s");
    const size_t masteryOffset = result.find("%s", nameOffset + 2);
    const size_t skillOffset = result.find("%s", masteryOffset + 2);
    result.replace(skillOffset, 2, skill);
    result.replace(masteryOffset, 2, mastery);
    result.replace(nameOffset, 2, name);
    return result;
}

std::string UiTextTable::formatSkillTrainingRequirement(
    const std::string &attribute, uint32_t value, const std::string &mastery, const std::string &skill) const
{
    std::string result = text(Message::SkillTrainingRequirement);
    if (!hasExpectedParameters(result, {"%s", "%u", "%s", "%s"}))
    {
        return {};
    }
    const size_t attributeOffset = result.find("%s");
    const size_t valueOffset = result.find("%u", attributeOffset + 2);
    const size_t masteryOffset = result.find("%s", valueOffset + 2);
    const size_t skillOffset = result.find("%s", masteryOffset + 2);
    result.replace(skillOffset, 2, skill);
    result.replace(masteryOffset, 2, mastery);
    result.replace(valueOffset, 2, std::to_string(value));
    result.replace(attributeOffset, 2, attribute);
    return result;
}

std::string UiTextTable::formatNpcDismiss(const std::string &name) const
{
    return formatStringParameter(text(Message::NpcDismissFormat), name);
}

std::string UiTextTable::formatTransitionLeave(const std::string &name) const
{
    return formatStringParameter(text(Message::TransitionLeaveFormat), name);
}

std::string UiTextTable::formatTransitionEnter(const std::string &name) const
{
    return formatStringParameter(text(Message::TransitionEnterFormat), name);
}

std::string UiTextTable::formatSpellPrompt(Message message, const std::string &spellName) const
{
    switch (message)
    {
        case Message::SpellSelectActorFormat:
        case Message::SpellSelectCharacterFormat:
        case Message::SpellSelectGroundFormat:
        case Message::SpellSelectTargetFormat:
        case Message::SpellCastFormat:
            return formatStringParameter(text(message), spellName);

        default:
            return {};
    }
}

const char *UiTextTable::key(Message message)
{
    const size_t index = size_t(message);
    return index < Definitions.size() ? Definitions[index].key.data() : "";
}
}
