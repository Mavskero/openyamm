#pragma once

#include "game/events/EventRuntime.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace OpenYAMM::Game
{
class NpcDialogTable;
class MergedBolsterMapTable;
class MergedBolsterMonsterTable;
class MergedMonsterPortraitTable;
class MergedNpcNameTable;
class MergedNpcProfessionTable;
class MergedNewsTopicTable;
class IGameplayWorldRuntime;
struct MapStatsEntry;

struct GeneratedNpcIdentity
{
    std::string actorKey;
    uint32_t actorIndex = 0;
};

std::optional<GeneratedNpcIdentity> generatedNpcIdentity(
    const EventRuntimeState &state, uint32_t npcId, const IGameplayWorldRuntime &world,
    std::optional<uint32_t> actorIndex = std::nullopt);
std::string generatedNpcVariable(const GeneratedNpcIdentity &identity, const std::string &field);
std::optional<int32_t> generatedNpcDay(float gameMinutes);
bool prepareGeneratedNpcConversation(EventRuntimeState &state, IGameplayWorldRuntime &world, Party &party,
    const std::optional<ScriptedEventProgram> &globalProgram,
    const std::function<std::optional<int32_t>()> &drawAlignment);
std::optional<std::string> formatGeneratedNpcText(const std::string &text, const EventRuntimeState &state,
    uint32_t npcId, const Party &party, const IGameplayWorldRuntime &world,
    const std::optional<ScriptedEventProgram> &globalProgram);

struct GenericActorDialogResolution
{
    uint32_t npcId = 0;
    uint32_t newsId = 0;
    bool newsFromNpcText = false;
    uint32_t portraitPictureId = 0;
    bool opensNpcTalk = false;
    bool generatedNpc = false;
    std::string generatedActorKey;
    std::string generatedName;
    uint32_t generatedProfessionId = 0;
};

std::optional<GenericActorDialogResolution> resolveGenericActorDialog(
    const std::string &mapFileName,
    const std::string &actorName,
    uint32_t actorGroup,
    const EventRuntimeState &runtimeState,
    const NpcDialogTable &npcDialogTable,
    const MergedMonsterPortraitTable *pMonsterPortraitTable = nullptr,
    const MapStatsEntry *pCurrentMap = nullptr,
    const MergedNewsTopicTable *pNewsAreaTopicTable = nullptr,
    const MergedNewsTopicTable *pNewsContinentTopicTable = nullptr,
    const MergedNpcNameTable *pNpcNameTable = nullptr,
    const MergedNpcProfessionTable *pNpcProfessionTable = nullptr,
    const MergedBolsterMapTable *pBolsterMapTable = nullptr,
    const MergedBolsterMonsterTable *pBolsterMonsterTable = nullptr,
    uint32_t actorMonsterId = 0,
    std::optional<size_t> actorIndex = std::nullopt
);

void applyGenericActorDialogResolution(
    EventRuntimeState &runtimeState,
    const GenericActorDialogResolution &resolution
);

bool hideGeneratedNpcActor(
    EventRuntimeState &runtimeState,
    uint32_t npcId,
    const MapStatsEntry *pCurrentMap
);

bool hideMapActorByIndex(EventRuntimeState &runtimeState, uint32_t actorIndex);
}
