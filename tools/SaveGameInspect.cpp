#include "game/maps/SaveGame.h"
#include "game/maps/MapDeltaData.h"
#include "game/events/EvtEnums.h"
#include "game/gameplay/CorpseLootRuntime.h"

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
            << ",\"grid_y\":" << int(item.gridY) << ",\"item\":";
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
        << ",\"items\":";
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

void writeCharacter(const OpenYAMM::Game::Character &member)
{
    std::cout << "{\"name\":";
    writeJsonString(member.name);
    std::cout << ",\"class\":";
    writeJsonString(member.className);
    std::cout << ",\"character_data_id\":" << member.characterDataId
        << ",\"roster_id\":" << member.rosterId
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
    if ((argc == 3 && std::string(argv[1]) == "--outdoor-events")
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
    const OpenYAMM::Game::IndoorMoveState &indoorPosition = save->indoorScene.partyRuntime.movementState;
    const bool outdoor = save->currentSceneKind == OpenYAMM::Game::SceneKind::Outdoor;
    std::vector<uint32_t> questBits(save->party.questBits.begin(), save->party.questBits.end());
    std::sort(questBits.begin(), questBits.end());
    std::cout << std::boolalpha << std::setprecision(std::numeric_limits<float>::max_digits10);
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
        << ",\n  \"gold\": " << save->party.gold
        << ",\n  \"bank_gold\": " << save->party.bankGold
        << ",\n  \"deaths\": " << deaths
        << ",\n  \"food\": " << save->party.food
        << ",\n  \"x\": " << (outdoor ? position.x : indoorPosition.x)
        << ",\n  \"y\": " << (outdoor ? position.y : indoorPosition.y)
        << ",\n  \"foot_z\": " << (outdoor ? position.footZ : indoorPosition.footZ)
        << ",\n  \"yaw_radians\": " << (outdoor ? save->outdoorCameraYawRadians : save->indoorCameraYawRadians)
        << ",\n  \"pitch_radians\": " << (outdoor ? save->outdoorCameraPitchRadians : save->indoorCameraPitchRadians)
        << ",\n  \"preview_bytes\": " << save->previewBmp.size()
        << ",\n  \"held_item_active\": " << save->heldInventoryItemActive
        << ",\n  \"held_item\": ";
    writeInventoryItem(save->heldInventoryItem);
    std::cout
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
    std::cout << "],\n  \"autonotes\": [";
    bool firstAutonote = true;
    for (const auto &[id, value] : autonotes)
    {
        std::cout << (firstAutonote ? "" : ",") << "{\"id\":" << id << ",\"value\":" << value << '}';
        firstAutonote = false;
    }
    std::cout << "],\n  \"current_map_history\": [";
    const auto &historyState = outdoor ? save->outdoorWorld.eventRuntimeState : save->indoorScene.eventRuntimeState;
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
    std::cout << "},\n  \"npc_greeting_overrides\": [";
    const std::map<uint32_t, uint32_t> npcGreetings(save->party.npcGreetingOverrides.begin(),
        save->party.npcGreetingOverrides.end());
    bool firstGreeting = true;
    for (const auto &[npc, greeting] : npcGreetings)
    {
        std::cout << (firstGreeting ? "" : ",") << "{\"npc\":" << npc << ",\"greeting\":" << greeting << '}';
        firstGreeting = false;
    }
    std::cout << "],\n  \"npc_house_overrides\": [";
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
    const auto &activeEventState = outdoor ? save->outdoorWorld.eventRuntimeState : save->indoorScene.eventRuntimeState;
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
    std::cout << "],\n  \"indoor_actors\": {";
    std::map<std::string, const OpenYAMM::Game::IndoorSceneRuntime::Snapshot *> indoorStates;
    for (const auto &[map, state] : save->indoorSceneStates) indoorStates[map] = &state;
    if (!outdoor && save->hasIndoorSceneState) indoorStates[save->mapFileName] = &save->indoorScene;
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
                std::cout << "]}";
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
    std::cout << "},\n  \"outdoor_actors\": {";
    std::map<std::string, const OpenYAMM::Game::OutdoorWorldRuntime::Snapshot *> outdoorStates;
    for (const auto &[map, state] : save->outdoorWorldStates) outdoorStates[map] = &state;
    if (outdoor && save->hasOutdoorRuntimeState) outdoorStates[save->mapFileName] = &save->outdoorWorld;
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
                << ",\"dead\":" << actor.isDead << ",\"invisible\":" << actor.isInvisible
                << ",\"x\":" << actor.preciseX << ",\"y\":" << actor.preciseY << ",\"z\":" << actor.preciseZ;
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
        : save->indoorScene.worldRuntime.projectileState.projectiles;
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
