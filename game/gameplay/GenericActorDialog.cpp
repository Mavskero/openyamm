#include "game/gameplay/GenericActorDialog.h"

#include "game/events/EvtEnums.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataRepository.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/gameplay/ReputationRuntime.h"
#include "game/outdoor/OutdoorWorldRuntime.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/MapStats.h"
#include "game/tables/NpcDialogTable.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint32_t FirstGeneratedNpcId = 1184;
constexpr uint32_t LastGeneratedNpcId = 1223;

uint64_t fnv1a64(const std::string &text)
{
    uint64_t hash = 14695981039346656037ull;

    for (char character : text)
    {
        hash ^= static_cast<uint8_t>(character);
        hash *= 1099511628211ull;
    }

    return hash;
}

uint32_t findNpcIdByName(const NpcDialogTable &npcDialogTable, const std::string &name)
{
    const NpcEntry *pNpcEntry = npcDialogTable.findNpcByName(name);
    return pNpcEntry != nullptr ? pNpcEntry->id : 0;
}

uint32_t resolveActorPortraitId(
    const MergedMonsterPortraitTable *pMonsterPortraitTable,
    const std::string &actorName,
    const std::string &actorKey,
    uint32_t actorMonsterId)
{
    if (pMonsterPortraitTable == nullptr)
    {
        return 0;
    }

    const uint64_t seed = fnv1a64(actorKey);
    const std::optional<uint32_t> monsterPortrait =
        pMonsterPortraitTable->portraitForMonsterId(actorMonsterId, seed);

    if (monsterPortrait.has_value())
    {
        return *monsterPortrait;
    }

    return pMonsterPortraitTable->portraitForName(actorName, seed).value_or(0);
}

std::string generatedActorKey(
    const std::string &mapFileName,
    uint32_t actorGroup,
    const std::string &actorName,
    std::optional<size_t> actorIndex)
{
    return mapFileName + "#" + std::to_string(actorIndex.value_or(static_cast<size_t>(actorGroup)))
        + "#" + std::to_string(actorGroup) + "#" + actorName;
}

bool isGeneratedNpcSlot(uint32_t npcId)
{
    return npcId >= FirstGeneratedNpcId && npcId <= LastGeneratedNpcId;
}

uint32_t chooseGeneratedNpcId(const EventRuntimeState &runtimeState, const std::string &actorKey)
{
    const std::unordered_map<std::string, uint32_t>::const_iterator existingIt =
        runtimeState.generatedNpcIdsByActorKey.find(actorKey);

    if (existingIt != runtimeState.generatedNpcIdsByActorKey.end() && isGeneratedNpcSlot(existingIt->second))
    {
        if (runtimeState.unavailableNpcIds.contains(existingIt->second))
        {
            return 0;
        }

        return existingIt->second;
    }

    std::array<bool, LastGeneratedNpcId - FirstGeneratedNpcId + 1> usedSlots = {};

    for (const auto &[existingActorKey, npcId] : runtimeState.generatedNpcIdsByActorKey)
    {
        static_cast<void>(existingActorKey);

        if (isGeneratedNpcSlot(npcId))
        {
            usedSlots[npcId - FirstGeneratedNpcId] = true;
        }
    }

    const size_t slotCount = usedSlots.size();
    const size_t startSlot = static_cast<size_t>(fnv1a64(actorKey) % slotCount);

    for (size_t offset = 0; offset < slotCount; ++offset)
    {
        const size_t slot = (startSlot + offset) % slotCount;
        const uint32_t npcId = FirstGeneratedNpcId + static_cast<uint32_t>(slot);

        if (!usedSlots[slot] && !runtimeState.unavailableNpcIds.contains(npcId))
        {
            return npcId;
        }
    }

    return 0;
}

const MergedBolsterMapEntry *currentBolsterMapEntry(
    const MapStatsEntry *pCurrentMap,
    const MergedBolsterMapTable *pBolsterMapTable)
{
    if (pCurrentMap == nullptr || pBolsterMapTable == nullptr || pCurrentMap->id < 0)
    {
        return nullptr;
    }

    return pBolsterMapTable->findById(static_cast<uint32_t>(pCurrentMap->id));
}

std::optional<uint32_t> generatedProfessionId(
    const std::string &actorKey,
    const MergedNpcProfessionTable &professionTable,
    uint32_t maxRarity)
{
    std::vector<uint32_t> candidateIds;

    for (const MergedNpcProfessionEntry &entry : professionTable.entries())
    {
        if (entry.id != 0 && entry.rarity <= maxRarity)
        {
            candidateIds.push_back(entry.id);
        }
    }

    if (candidateIds.empty())
    {
        return std::nullopt;
    }

    return candidateIds[static_cast<size_t>(fnv1a64(actorKey + "#profession") % candidateIds.size())];
}

std::string generatedNpcName(
    const std::string &actorKey,
    const MergedNpcNameTable &nameTable,
    const MergedBolsterMonsterEntry *pBolsterMonster)
{
    const bool useFemaleName = pBolsterMonster != nullptr
        ? pBolsterMonster->gender == "F"
        : (fnv1a64(actorKey + "#gender") % 2u) != 0;
    const std::vector<std::string> &names = useFemaleName ? nameTable.femaleNames() : nameTable.maleNames();

    if (names.empty())
    {
        return {};
    }

    return names[static_cast<size_t>(fnv1a64(actorKey + "#name") % names.size())];
}

bool canGenerateNpcFromBolsterMonster(const MergedBolsterMonsterEntry *pBolsterMonster)
{
    // MMerge's NPCNewsTopics.lua only randomizes profession NPCs from bolster monsters with peasant creed.
    return pBolsterMonster != nullptr && pBolsterMonster->creed == "Peasant";
}

std::optional<GenericActorDialogResolution> resolveGeneratedActorDialog(
    const std::string &mapFileName,
    const std::string &actorName,
    uint32_t actorGroup,
    const EventRuntimeState &runtimeState,
    const MergedMonsterPortraitTable *pMonsterPortraitTable,
    const MapStatsEntry *pCurrentMap,
    const MergedNpcNameTable *pNpcNameTable,
    const MergedNpcProfessionTable *pNpcProfessionTable,
    const MergedBolsterMapTable *pBolsterMapTable,
    const MergedBolsterMonsterTable *pBolsterMonsterTable,
    uint32_t actorMonsterId,
    std::optional<size_t> actorIndex)
{
    const MergedBolsterMapEntry *pBolsterEntry = currentBolsterMapEntry(pCurrentMap, pBolsterMapTable);

    if (pNpcNameTable == nullptr
        || pNpcProfessionTable == nullptr
        || pBolsterEntry == nullptr
        || !pBolsterEntry->professionMaxRarity.has_value()
        || *pBolsterEntry->professionMaxRarity == 0)
    {
        return std::nullopt;
    }

    const MergedBolsterMonsterEntry *pBolsterMonster =
        pBolsterMonsterTable != nullptr ? pBolsterMonsterTable->findById(actorMonsterId) : nullptr;

    if (!canGenerateNpcFromBolsterMonster(pBolsterMonster))
    {
        return std::nullopt;
    }

    const std::string actorKey = generatedActorKey(mapFileName, actorGroup, actorName, actorIndex);
    const uint32_t npcId = chooseGeneratedNpcId(runtimeState, actorKey);

    if (npcId == 0 || runtimeState.unavailableNpcIds.contains(npcId))
    {
        return std::nullopt;
    }

    const std::optional<uint32_t> professionId =
        generatedProfessionId(actorKey, *pNpcProfessionTable, *pBolsterEntry->professionMaxRarity);
    const std::string name = generatedNpcName(actorKey, *pNpcNameTable, pBolsterMonster);

    if (!professionId.has_value() || name.empty())
    {
        return std::nullopt;
    }

    GenericActorDialogResolution resolution = {};
    resolution.npcId = npcId;
    resolution.portraitPictureId =
        resolveActorPortraitId(pMonsterPortraitTable, actorName, actorKey, actorMonsterId);
    resolution.opensNpcTalk = true;
    resolution.generatedNpc = true;
    resolution.generatedActorKey = actorKey;
    const auto savedName = runtimeState.npcNameOverrides.find(npcId);
    const auto savedPicture = runtimeState.npcPictureOverrides.find(npcId);
    const auto savedProfession = runtimeState.npcProfessionOverrides.find(npcId);
    const bool existingIdentity = runtimeState.generatedNpcIdsByActorKey.contains(actorKey);
    resolution.generatedName = existingIdentity && savedName != runtimeState.npcNameOverrides.end()
        ? savedName->second : name;
    resolution.generatedProfessionId = existingIdentity && savedProfession != runtimeState.npcProfessionOverrides.end()
        ? savedProfession->second : *professionId;
    if (existingIdentity && savedPicture != runtimeState.npcPictureOverrides.end())
    {
        resolution.portraitPictureId = savedPicture->second;
    }
    return resolution;
}

std::string resolveGenericNpcName(const std::string &actorName, uint32_t actorGroup)
{
    if (actorName == "Lizardman Peasant")
    {
        return "Lizardman Peasant";
    }

    if (actorName == "Lizardman Villager")
    {
        return "Lizardman Peasant";
    }

    if ((actorName == "Swordsman" || actorName == "Guard" || actorName == "Adventurer")
        && actorGroup >= 54
        && actorGroup <= 60)
    {
        return "Guard";
    }

    if (actorName == "Lizardman Guard"
        || actorName == "Lizardman Soldier"
        || actorName == "Lizardman Warrior"
        || actorName == "Lizardman Lookout")
    {
        return "Lizardman Guard";
    }

    if (actorName == "Dark Elf Peasant")
    {
        return "Dark Elf Peasant";
    }

    if (actorName == "Dark Elf Guard")
    {
        return "Dark Elf Guard";
    }

    if (actorName == "Ogre Peasant")
    {
        return "Ogre Peasant";
    }

    if (actorName == "Wererat Peasant" || actorName == "Wererat Peasants")
    {
        return "Wererat Peasants";
    }

    if (actorName == "Troll Peasant")
    {
        return "Troll Peasant";
    }

    if (actorName == "Dragon Hunter")
    {
        return "Dragon Hunter";
    }

    if (actorName == "Necromancer Peasant")
    {
        return "Necromancer Peasant";
    }

    if (actorName == "Cleric Peasant")
    {
        return "Cleric Peasant";
    }

    if (actorName == "Regnan Peasant" || actorName == "Pirate")
    {
        return "Regnan Peasant";
    }

    if (actorName == "Minotaur Peasant")
    {
        return "Minotaur Peasant";
    }

    switch (actorGroup)
    {
        case 1:
        case 3:
            return "Lizardman Peasant";

        case 2:
        case 9:
            return "Lizardman Guard";

        case 5:
        case 6:
        case 7:
        case 16:
        case 17:
        case 18:
            return "Dark Elf Peasant";

        case 8:
            return "Wererat Peasants";

        case 19:
        case 20:
        case 21:
            return "Troll Peasant";

        case 22:
        case 23:
        case 24:
            return "Dragon Hunter";

        case 25:
        case 26:
        case 27:
            return "Necromancer Peasant";

        case 28:
        case 29:
            return "Cleric Peasant";

        case 32:
        case 33:
        case 34:
            return "Regnan Peasant";

        case 35:
        case 36:
        case 37:
            return "Minotaur Peasant";

        default:
            return {};
    }
}

const MergedNewsTopicEntry *firstMapAreaNews(
    const MapStatsEntry *pCurrentMap,
    const MergedNewsTopicTable *pNewsAreaTopicTable)
{
    if (pCurrentMap == nullptr || pNewsAreaTopicTable == nullptr)
    {
        return nullptr;
    }

    const uint32_t mapId = static_cast<uint32_t>(std::max(pCurrentMap->id, 0));

    for (const MergedNewsTopicEntry &entry : pNewsAreaTopicTable->entries())
    {
        if (entry.ownerId == mapId && entry.newsTextId != 0)
        {
            return &entry;
        }
    }

    return nullptr;
}

const MergedNewsTopicEntry *firstContinentNews(
    const MapStatsEntry *pCurrentMap,
    const MergedNewsTopicTable *pNewsContinentTopicTable)
{
    if (pCurrentMap == nullptr || pNewsContinentTopicTable == nullptr || pCurrentMap->mergedContinentId == 0)
    {
        return nullptr;
    }

    for (const MergedNewsTopicEntry &entry : pNewsContinentTopicTable->entries())
    {
        if (entry.ownerId == pCurrentMap->mergedContinentId && entry.newsTextId != 0)
        {
            return &entry;
        }
    }

    return nullptr;
}

bool isPlaceholderGroupNews(uint32_t newsId)
{
    // MMerge uses this as a default "change me" entry for many imported MM6/MM7 peasant groups.
    return newsId == 51;
}
}

std::optional<GenericActorDialogResolution> resolveGenericActorDialog(
    const std::string &mapFileName,
    const std::string &actorName,
    uint32_t actorGroup,
    const EventRuntimeState &runtimeState,
    const NpcDialogTable &npcDialogTable,
    const MergedMonsterPortraitTable *pMonsterPortraitTable,
    const MapStatsEntry *pCurrentMap,
    const MergedNewsTopicTable *pNewsAreaTopicTable,
    const MergedNewsTopicTable *pNewsContinentTopicTable,
    const MergedNpcNameTable *pNpcNameTable,
    const MergedNpcProfessionTable *pNpcProfessionTable,
    const MergedBolsterMapTable *pBolsterMapTable,
    const MergedBolsterMonsterTable *pBolsterMonsterTable,
    uint32_t actorMonsterId,
    std::optional<size_t> actorIndex
)
{
    const std::string genericNpcName = resolveGenericNpcName(actorName, actorGroup);
    const uint32_t genericNpcId = findNpcIdByName(npcDialogTable, genericNpcName);

    if (genericNpcId == 0)
    {
        const std::optional<GenericActorDialogResolution> generatedResolution =
            resolveGeneratedActorDialog(
                mapFileName,
                actorName,
                actorGroup,
                runtimeState,
                pMonsterPortraitTable,
                pCurrentMap,
                pNpcNameTable,
                pNpcProfessionTable,
                pBolsterMapTable,
                pBolsterMonsterTable,
                actorMonsterId,
                actorIndex);

        if (generatedResolution.has_value())
        {
            return generatedResolution;
        }
    }

    uint32_t newsId = 0;
    const std::unordered_map<uint32_t, uint32_t>::const_iterator overrideIt =
        runtimeState.npcGroupNews.find(actorGroup);

    if (overrideIt != runtimeState.npcGroupNews.end())
    {
        newsId = overrideIt->second;
    }
    else
    {
        const std::optional<uint32_t> defaultNewsId = npcDialogTable.getNewsIdForGroup(actorGroup);

        if (defaultNewsId)
        {
            newsId = *defaultNewsId;
        }
    }

    if (isPlaceholderGroupNews(newsId))
    {
        newsId = 0;
    }

    const MergedNewsTopicEntry *pNews = nullptr;
    if (newsId == 0)
    {
        pNews = firstMapAreaNews(pCurrentMap, pNewsAreaTopicTable);
        if (pNews == nullptr)
        {
            pNews = firstContinentNews(pCurrentMap, pNewsContinentTopicTable);
        }
        newsId = pNews != nullptr ? pNews->newsTextId : 0;
    }
    const bool newsFromNpcText = pNews != nullptr && pNews->textFromNpcTable;

    if (newsId == 0 || !npcDialogTable.getNewsDialogText(newsId, newsFromNpcText))
    {
        return std::nullopt;
    }

    GenericActorDialogResolution resolution = {};
    resolution.npcId = genericNpcId;
    resolution.newsId = newsId;
    resolution.newsFromNpcText = newsFromNpcText;
    resolution.portraitPictureId = resolveActorPortraitId(
        pMonsterPortraitTable,
        actorName,
        generatedActorKey(mapFileName, actorGroup, actorName, actorIndex),
        actorMonsterId);
    return resolution;
}

void applyGenericActorDialogResolution(
    EventRuntimeState &runtimeState,
    const GenericActorDialogResolution &resolution
)
{
    if (!resolution.generatedNpc || resolution.npcId == 0)
    {
        return;
    }

    if (!runtimeState.generatedNpcIdsByActorKey.contains(resolution.generatedActorKey))
    {
        // RandomizeNPC assigns Alignment at generation. The draw itself belongs to the
        // ensuing real NpcEnter Lua owner, not this also-used-for-inspection resolver.
        runtimeState.namedMapVars["MMerge.RandomNPC." + resolution.generatedActorKey + ".NewIdentity"] = 1;
    }
    runtimeState.generatedNpcIdsByActorKey[resolution.generatedActorKey] = resolution.npcId;
    runtimeState.npcNameOverrides[resolution.npcId] = resolution.generatedName;
    runtimeState.npcPictureOverrides[resolution.npcId] = resolution.portraitPictureId;
    runtimeState.npcProfessionOverrides[resolution.npcId] = resolution.generatedProfessionId;
}

std::optional<GeneratedNpcIdentity> generatedNpcIdentity(
    const EventRuntimeState &state, uint32_t npcId, const IGameplayWorldRuntime &world,
    std::optional<uint32_t> actorIndex)
{
    const GameSession *pSession = world.boundGameSession();
    if (pSession == nullptr)
    {
        return std::nullopt;
    }
    const std::string &mapFileName = pSession->currentMapFileName();
    if (!isGeneratedNpcSlot(npcId) || mapFileName.empty()
        || !state.npcNameOverrides.contains(npcId) || !state.npcProfessionOverrides.contains(npcId))
    {
        return std::nullopt;
    }
    const std::string prefix = mapFileName + "#";
    std::optional<GeneratedNpcIdentity> result;
    for (const auto &[key, id] : state.generatedNpcIdsByActorKey)
    {
        if (id != npcId || !key.starts_with(prefix))
        {
            continue;
        }
        const size_t end = key.find('#', prefix.size());
        if (end == std::string::npos)
        {
            return std::nullopt;
        }
        uint32_t index = 0;
        const std::from_chars_result parsed =
            std::from_chars(key.data() + prefix.size(), key.data() + end, index);
        if (parsed.ec != std::errc() || parsed.ptr != key.data() + end
            || (actorIndex && index != *actorIndex) || result)
        {
            return std::nullopt;
        }
        uint32_t group = 0;
        std::string name;
        if (const OutdoorWorldRuntime *pOutdoor = dynamic_cast<const OutdoorWorldRuntime *>(&world))
        {
            const OutdoorWorldRuntime::MapActorState *pActor = pOutdoor->mapActorState(index);
            if (pActor == nullptr)
            {
                return std::nullopt;
            }
            group = pActor->group;
            name = pActor->displayName;
        }
        else
        {
            const MapDeltaData *pDelta = world.mapDeltaData();
            GameplayActorInspectState inspect;
            if (pDelta == nullptr || index >= pDelta->actors.size() || !world.actorInspectState(index, 0, inspect))
            {
                return std::nullopt;
            }
            group = pDelta->actors[index].group;
            name = inspect.displayName;
        }
        // Slots are reused across maps. Only the resolver's full map/index/group/name binding is identity.
        if (key != generatedActorKey(mapFileName, group, name, index))
        {
            return std::nullopt;
        }
        result = GeneratedNpcIdentity{key, index};
    }
    return result;
}

std::string generatedNpcVariable(const GeneratedNpcIdentity &identity, const std::string &field)
{
    return "MMerge.RandomNPC." + identity.actorKey + "." + field;
}

std::optional<int32_t> generatedNpcDay(float gameMinutes)
{
    if (!std::isfinite(gameMinutes) || gameMinutes < 0.0f)
    {
        return std::nullopt;
    }
    // Game.DayOfMonth is 0..27; storing zero must remain distinct from an absent Lua field.
    return static_cast<int32_t>(std::fmod(std::floor(double(gameMinutes) / 1440.0), 28.0));
}

namespace
{
struct GeneratedNpcSource
{
    GeneratedNpcIdentity identity;
    const GameDataRepository *pData = nullptr;
    const MapStatsEntry *pMap = nullptr;
    const MergedNpcProfessionEntry *pProfession = nullptr;
    const MergedNpcBtbEntry *pPersonality = nullptr;
};

std::optional<GeneratedNpcSource> generatedNpcSource(const EventRuntimeState &state, uint32_t npcId,
    const IGameplayWorldRuntime &world, std::optional<uint32_t> actorIndex = std::nullopt)
{
    const GameSession *pSession = world.boundGameSession();
    if (pSession == nullptr || !pSession->hasDataRepository())
    {
        return std::nullopt;
    }
    const GameDataRepository &data = pSession->data();
    const MapStatsEntry *pMap = nullptr;
    for (const MapStatsEntry &entry : data.mapEntries())
    {
        if (entry.fileName == pSession->currentMapFileName())
        {
            pMap = &entry;
            break;
        }
    }
    if (pMap == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<GeneratedNpcIdentity> identity = generatedNpcIdentity(state, npcId, world, actorIndex);
    if (!identity)
    {
        return std::nullopt;
    }
    const MergedNpcProfessionEntry *pProfession =
        data.mergedNpcProfessionTable().get(state.npcProfessionOverrides.at(npcId));
    const MergedNpcBtbEntry *pPersonality = pProfession != nullptr
        ? data.mergedNpcBtbTable().get(pProfession->personality) : nullptr;
    if (pPersonality == nullptr)
    {
        return std::nullopt;
    }
    return GeneratedNpcSource{*identity, &data, pMap, pProfession, pPersonality};
}

std::optional<int32_t> npcField(const EventRuntimeState &state, const GeneratedNpcIdentity &identity,
    const std::string &field)
{
    const auto iterator = state.namedMapVars.find(generatedNpcVariable(identity, field));
    return iterator == state.namedMapVars.end() ? std::nullopt : std::optional<int32_t>(iterator->second);
}

std::optional<bool> generatedNpcFemale(const GeneratedNpcSource &source, const IGameplayWorldRuntime &world)
{
    GameplayRuntimeActorState actor;
    if (!world.actorRuntimeState(source.identity.actorIndex, actor))
    {
        return std::nullopt;
    }
    const MergedBolsterMonsterEntry *pMonster = source.pData->mergedBolsterMonsterTable().findById(actor.monsterId);
    if (pMonster == nullptr || (pMonster->gender != "M" && pMonster->gender != "F"))
    {
        return std::nullopt;
    }
    return pMonster->gender == "F";
}

std::optional<uint32_t> generatedNpcGroup(const GeneratedNpcIdentity &identity, const IGameplayWorldRuntime &world)
{
    if (const OutdoorWorldRuntime *pOutdoor = dynamic_cast<const OutdoorWorldRuntime *>(&world))
    {
        const OutdoorWorldRuntime::MapActorState *pActor = pOutdoor->mapActorState(identity.actorIndex);
        return pActor != nullptr ? std::optional<uint32_t>(pActor->group) : std::nullopt;
    }
    const MapDeltaData *pDelta = world.mapDeltaData();
    return pDelta != nullptr && identity.actorIndex < pDelta->actors.size()
        ? std::optional<uint32_t>(pDelta->actors[identity.actorIndex].group) : std::nullopt;
}

// Reputation.lua starts ContinentFame as an empty table when absent. Store the
// original signed experience bases in two existing int32 named fields; no Save layout change.
std::optional<int64_t> generatedNpcFame(EventRuntimeState &state, const Party &party, uint32_t continent)
{
    const Character *pFirst = party.member(0);
    if (pFirst == nullptr)
    {
        return std::nullopt;
    }
    if (continent == 4)
    {
        return int64_t(pFirst->experience) / 250;
    }
    constexpr std::string_view prefix = "MMerge.ContinentFame.";
    const std::string current = std::string(prefix) + std::to_string(continent);
    int64_t total = 0, currentBase = 0;
    for (const auto &[key, low] : state.namedGlobalVars)
    {
        if (!key.starts_with(prefix) || !key.ends_with(".Lo"))
        {
            continue;
        }
        const std::string base = key.substr(0, key.size() - 3);
        const auto hi = state.namedGlobalVars.find(base + ".Hi");
        if (hi == state.namedGlobalVars.end())
        {
            return std::nullopt;
        }
        const int64_t value = std::bit_cast<int64_t>((uint64_t(std::bit_cast<uint32_t>(hi->second)) << 32)
            | std::bit_cast<uint32_t>(low));
        if ((value > 0 && total > std::numeric_limits<int64_t>::max() - value)
            || (value < 0 && total < std::numeric_limits<int64_t>::min() - value))
        {
            return std::nullopt;
        }
        total += value;
        if (base == current)
        {
            currentBase = value;
        }
    }
    // Remove the current continent before subtracting the others from Party[0].Experience.
    if ((currentBase < 0 && total > std::numeric_limits<int64_t>::max() + currentBase)
        || (currentBase > 0 && total < std::numeric_limits<int64_t>::min() + currentBase))
    {
        return std::nullopt;
    }
    const int64_t others = total - currentBase;
    if (others < 0 && int64_t(pFirst->experience) > std::numeric_limits<int64_t>::max() + others)
    {
        return std::nullopt;
    }
    const int64_t value = int64_t(pFirst->experience) - others;
    const uint64_t bits = std::bit_cast<uint64_t>(value);
    state.namedGlobalVars[current + ".Hi"] = std::bit_cast<int32_t>(uint32_t(bits >> 32));
    state.namedGlobalVars[current + ".Lo"] = std::bit_cast<int32_t>(uint32_t(bits));
    return value / 250;
}

uint32_t sourceReputationTextId(int reputation)
{
    return reputation > 24 ? 379 : reputation > 5 ? 392 : reputation > -6 ? 399
        : reputation > -25 ? 402 : 434;
}

enum class NpcRefusal
{
    None,
    Reputation,
    LowReputation,
    NoFame
};

struct NpcAdmission
{
    bool speaks = true;
    NpcRefusal refusal = NpcRefusal::None;
};
}

std::optional<std::string> formatGeneratedNpcText(const std::string &text, const EventRuntimeState &state,
    uint32_t npcId, const Party &party, const IGameplayWorldRuntime &world,
    const std::optional<ScriptedEventProgram> &globalProgram)
{
    const std::optional<GeneratedNpcSource> source = generatedNpcSource(state, npcId, world);
    const Character *pPlayer = party.activeMember();
    if (pPlayer == nullptr)
    {
        pPlayer = party.member(0);
    }
    if (!source || pPlayer == nullptr || !globalProgram)
    {
        return std::nullopt;
    }
    EventRuntimeState merged = state;
    party.applyGlobalNpcStateTo(merged);
    const int reputation = effectivePartyReputation(world.currentLocationReputation(), &merged);
    const CharacterDollEntry *pPlayerPortrait =
        source->pData->characterDollTable().getCharacter(pPlayer->characterDataId);
    const ClassInspectEntry *pPlayerClass = source->pData->characterInspectTable().getClass(pPlayer->className);
    const std::optional<bool> female = generatedNpcFemale(*source, world);
    const auto global = [&](uint32_t id) -> std::optional<std::string>
    {
        return globalProgram->getImportedGlobalText(id);
    };
    const int requiredReputation = npcField(state, source->identity, "Alignment") == 2
        ? source->pPersonality->requiredReputation : -source->pPersonality->requiredReputation;
    if (!std::isfinite(world.gameMinutes()) || world.gameMinutes() < 0.0f)
    {
        return std::nullopt;
    }
    const int hour = static_cast<int>(std::fmod(std::floor(double(world.gameMinutes()) / 60.0), 24.0));
    std::string result = text;
    for (int token = 1; token <= 16; ++token)
    {
        const std::string marker = "%" + std::string(token < 10 ? "0" : "") + std::to_string(token);
        if (result.find(marker) == std::string::npos)
        {
            continue;
        }
        std::optional<std::string> replacement;
        switch (token)
        {
            case 1:
                replacement = state.npcNameOverrides.at(npcId);
                break;
            case 2:
            case 13:
            case 15:
            case 16:
                replacement = pPlayer->name;
                break;
            case 3:
                if (female)
                {
                    replacement = global(*female ? 384 : 383);
                }
                break;
            case 4:
                replacement = std::to_string(source->pProfession->weeklyCost);
                break;
            case 5:
                replacement = global(hour <= 8 ? 395 : hour <= 16 ? 396 : 397);
                break;
            case 6:
            case 7:
                if (pPlayerClass != nullptr)
                {
                    replacement = pPlayerClass->name;
                }
                break;
            case 8:
            case 11:
                replacement = global(sourceReputationTextId(reputation));
                break;
            case 9:
                if (pPlayerPortrait != nullptr)
                {
                    replacement = global(pPlayerPortrait->defaultSex == 1 ? 384 : 383);
                }
                break;
            case 10:
                if (pPlayerPortrait != nullptr)
                {
                    replacement = global(pPlayerPortrait->defaultSex == 1 ? 389 : 388);
                }
                break;
            case 12:
                replacement = global(sourceReputationTextId(requiredReputation));
                break;
            case 14:
                replacement = source->pProfession->displayName();
                break;
        }
        if (!replacement)
        {
            return std::nullopt;
        }
        size_t position = 0;
        while ((position = result.find(marker, position)) != std::string::npos)
        {
            result.replace(position, marker.size(), *replacement);
            position += replacement->size();
        }
    }
    return result;
}

bool prepareGeneratedNpcConversation(EventRuntimeState &state, IGameplayWorldRuntime &world, Party &party,
    const std::optional<ScriptedEventProgram> &globalProgram,
    const std::function<std::optional<int32_t>()> &drawAlignment)
{
    state.dialogueState.generatedNpcConversation.reset();
    if (!state.activeHookContext || state.activeHookContext->kind != EventRuntimeHookKind::NpcEnter)
    {
        return false;
    }
    const EventRuntimeState::ActiveHookContext &hook = *state.activeHookContext;
    const std::optional<GeneratedNpcSource> source = generatedNpcSource(state, hook.npcId, world, hook.actorIndex);
    if (!source)
    {
        return false;
    }
    EventRuntimeState::DialogueRuntimeState::GeneratedNpcConversation &conversation =
        state.dialogueState.generatedNpcConversation.emplace();
    conversation.npcId = hook.npcId;
    conversation.actorKey = source->identity.actorKey;
    const std::optional<int32_t> day = generatedNpcDay(world.gameMinutes());
    const std::optional<uint32_t> group = generatedNpcGroup(source->identity, world);
    if (!day || !group || !globalProgram)
    {
        return true;
    }
    const MergedNpcBtbEntry &personality = *source->pPersonality;
    const MergedContinentSettingEntry *pContinent =
        source->pData->mergedContinentSettingTable().findById(source->pMap->mergedContinentId);
    if (pContinent == nullptr)
    {
        return true;
    }
    const auto ensureAlignment = [&]() -> bool
    {
        const std::optional<int32_t> old = npcField(state, source->identity, "Alignment");
        if (old)
        {
            return *old == 1 || *old == 2;
        }
        const std::optional<int32_t> value = drawAlignment();
        if (!value || (*value != 1 && *value != 2))
        {
            return false;
        }
        state.namedMapVars[generatedNpcVariable(source->identity, "Alignment")] = *value;
        return true;
    };
    if (npcField(state, source->identity, "NewIdentity") == 1)
    {
        if (!ensureAlignment())
        {
            return true;
        }
        state.namedMapVars.erase(generatedNpcVariable(source->identity, "NewIdentity"));
    }
    EventRuntimeState merged = state;
    party.applyGlobalNpcStateTo(merged);
    const bool hired = std::any_of(merged.hiredNpcFollowers.begin(), merged.hiredNpcFollowers.end(),
        [&](const EventRuntimeState::HiredNpcFollower &follower)
        {
            return follower.npcId == hook.npcId;
        });
    const int reputation = effectivePartyReputation(world.currentLocationReputation(), &merged);
    const auto willSpeak = [&]() -> std::optional<NpcAdmission>
    {
        if (hook.houseId != 0 || hired || npcField(state, source->identity, "ThreatSuccess")
            || !pContinent->reputationAffectsNpc)
        {
            return NpcAdmission{};
        }
        const std::optional<int64_t> fame = generatedNpcFame(state, party, source->pMap->mergedContinentId);
        if (!fame)
        {
            return std::nullopt;
        }
        // NPCFollowers.lua544-545 intentionally captures IsGood before filling a missing Alignment.
        const bool good = npcField(state, source->identity, "Alignment") == 1;
        if (!ensureAlignment())
        {
            return std::nullopt;
        }
        if ((good && reputation > 0) || (!good && reputation < -10))
        {
            return NpcAdmission{false, NpcRefusal::Reputation};
        }
        if (std::abs(int64_t(reputation)) < personality.requiredReputation)
        {
            return NpcAdmission{false, NpcRefusal::LowReputation};
        }
        if (int64_t(personality.requiredFame) > *fame)
        {
            return NpcAdmission{false, NpcRefusal::NoFame};
        }
        return NpcAdmission{};
    };
    // EnterNPC sets topics before DrawNPCGreeting makes its own query.
    const std::optional<NpcAdmission> admission = willSpeak();
    if (!admission)
    {
        return true;
    }
    conversation.admissionReady = true;
    conversation.willSpeak = admission->speaks;
    if (hook.houseId != 0)
    {
        return true; // DrawNPCGreeting excludes CurrentScreen 13.
    }
    const bool seen = npcField(state, source->identity, "SeenGreet").value_or(0) != 0;
    state.namedMapVars[generatedNpcVariable(source->identity, "SeenGreet")] = 1;
    const auto groupOverride = merged.npcGroupNews.find(*group);
    const uint32_t newsId = groupOverride != merged.npcGroupNews.end() ? groupOverride->second
        : source->pData->npcDialogTable().getNewsIdForGroup(*group).value_or(0);
    if (*group > 0 && newsId != 0)
    {
        const std::optional<std::string> news = source->pData->npcDialogTable().getNewsText(newsId);
        if (news && !news->empty())
        {
            conversation.greeting = *news;
            return true;
        }
    }
    const std::optional<NpcAdmission> greetingAdmission = willSpeak();
    if (!greetingAdmission)
    {
        return true;
    }
    const bool good = npcField(state, source->identity, "Alignment") == 1;
    uint32_t textId = 0;
    if (npcField(state, source->identity, "ThreatSuccess") == day)
    {
        textId = personality.threatReturnTextId;
    }
    else if (npcField(state, source->identity, "BribeSuccess") == day)
    {
        textId = personality.bribeReturnTextId;
    }
    else if (hired || greetingAdmission->speaks)
    {
        if (reputation <= -25)
        {
            textId = good ? personality.reputationSaintlyGoodTextId : personality.reputationSaintlyEvilTextId;
        }
        else if (reputation >= 25)
        {
            textId = good ? personality.reputationNotoriousGoodTextId : personality.reputationNotoriousEvilTextId;
        }
        else
        {
            textId = seen ? personality.reputationOkSecondTextId : personality.reputationOkFirstTextId;
        }
    }
    else if (greetingAdmission->refusal == NpcRefusal::Reputation)
    {
        textId = good
            ? (seen ? personality.reputationBelowZeroSecondGoodTextId : personality.reputationBelowZeroFirstGoodTextId)
            : (seen ? personality.reputationAboveTenSecondEvilTextId : personality.reputationAboveTenFirstEvilTextId);
    }
    else if (greetingAdmission->refusal == NpcRefusal::LowReputation)
    {
        textId = good ? (seen ? personality.lowReputationSecondGoodTextId : personality.lowReputationFirstGoodTextId)
            : (seen ? personality.lowReputationSecondEvilTextId : personality.lowReputationFirstEvilTextId);
    }
    else
    {
        textId = personality.fameTooLowTextId;
    }
    const std::optional<std::string> text = source->pData->npcDialogTable().getText(textId);
    if (text)
    {
        conversation.greeting = formatGeneratedNpcText(*text, state, hook.npcId, party, world, globalProgram);
    }
    return true;
}

bool hideGeneratedNpcActor(
    EventRuntimeState &runtimeState,
    uint32_t npcId,
    const MapStatsEntry *pCurrentMap
)
{
    if (npcId == 0 || pCurrentMap == nullptr || pCurrentMap->fileName.empty())
    {
        return false;
    }

    const std::string keyPrefix = pCurrentMap->fileName + "#";

    for (const auto &[actorKey, mappedNpcId] : runtimeState.generatedNpcIdsByActorKey)
    {
        if (mappedNpcId != npcId || actorKey.rfind(keyPrefix, 0) != 0)
        {
            continue;
        }

        const size_t actorIndexBegin = keyPrefix.size();
        const size_t actorIndexEnd = actorKey.find('#', actorIndexBegin);

        if (actorIndexEnd == std::string::npos || actorIndexEnd == actorIndexBegin)
        {
            continue;
        }

        const uint32_t actorIndex = static_cast<uint32_t>(
            std::strtoul(actorKey.substr(actorIndexBegin, actorIndexEnd - actorIndexBegin).c_str(), nullptr, 10));
        return hideMapActorByIndex(runtimeState, actorIndex);
    }

    return false;
}

bool hideMapActorByIndex(EventRuntimeState &runtimeState, uint32_t actorIndex)
{
    const uint32_t invisibleBit = static_cast<uint32_t>(EvtActorAttribute::Invisible);
    runtimeState.actorSetMasks[actorIndex] |= invisibleBit;
    runtimeState.actorClearMasks[actorIndex] &= ~invisibleBit;

    if (runtimeState.actorClearMasks[actorIndex] == 0)
    {
        runtimeState.actorClearMasks.erase(actorIndex);
    }

    return true;
}
}
