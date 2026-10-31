#include "game/events/EventDialogContent.h"

#include "game/events/ISceneEventContext.h"
#include "game/app/GameSession.h"
#include "game/gameplay/OwnedMercenaryRuntime.h"
#include "game/gameplay/GenericActorDialog.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/gameplay/MasteryTeacherDialog.h"
#include "game/gameplay/MercenaryRecruitmentRuntime.h"
#include "game/gameplay/ReputationRuntime.h"
#include "game/StringUtils.h"
#include "game/tables/CharacterInspectTable.h"
#include "game/tables/HouseServiceTextTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/UiTextTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr const char *TransitionVideoDirectory = "Videos/Transitions";
constexpr size_t MaxNpcFollowerCount = 4;
constexpr uint32_t NpcBegTopicId = 1766;
constexpr uint32_t NpcThreatTopicId = 1767;
constexpr uint32_t NpcBribeTopicId = 1768;
constexpr uint32_t Mm6BountyHuntTopicId = 1712;
constexpr uint32_t NewSorpigalTownHallHouseId = 208;
constexpr uint32_t MistTownHallHouseId = 210;
constexpr uint32_t SilverCoveTownHallHouseId = 211;

std::optional<NpcEntry> runtimeNpcEntryIfExists(
    const NpcDialogTable *pNpcDialogTable,
    const EventRuntimeState &eventRuntimeState,
    uint32_t npcId);

bool houseExtraExitIsAvailable(const HouseEntry &houseEntry, const Party *pParty)
{
    return houseEntry.extraExit.has_value()
        && (houseEntry.extraExit->requiredQuestBit == 0
            || (pParty != nullptr && pParty->hasQuestBit(houseEntry.extraExit->requiredQuestBit)));
}

bool shouldUseResidentOnlyHouseLobby(
    const HouseEntry &houseEntry,
    HouseServiceType serviceType,
    const std::vector<uint32_t> &residentNpcIds,
    const NpcDialogTable *pNpcDialogTable,
    const EventRuntimeState &eventRuntimeState)
{
    if (serviceType != HouseServiceType::None
        && !residentNpcIds.empty()
        && houseEntry.extraExit.has_value()
        && houseEntry.proprietorName.empty()
        && houseEntry.proprietorTitle.empty())
    {
        return true;
    }

    if (serviceType != HouseServiceType::TownHall
        || residentNpcIds.empty()
        || houseEntry.proprietorPictureId != 0)
    {
        return false;
    }

    if (houseEntry.id == NewSorpigalTownHallHouseId
        || houseEntry.id == MistTownHallHouseId
        || houseEntry.id == SilverCoveTownHallHouseId)
    {
        return true;
    }

    for (uint32_t residentNpcId : residentNpcIds)
    {
        const std::optional<NpcEntry> resident =
            runtimeNpcEntryIfExists(pNpcDialogTable, eventRuntimeState, residentNpcId);

        if (resident
            && std::find(
                resident->topicIds.begin(),
                resident->topicIds.end(),
                Mm6BountyHuntTopicId) != resident->topicIds.end())
        {
            return true;
        }
    }

    return false;
}

const char *transitionImageName(uint32_t imageId)
{
    static constexpr std::array<const char *, 11> ImageNames = {{
        "",
        "Ticon01",
        "Ticon02",
        "Ticon03",
        "Ticon04",
        "Ticon05",
        "ISTAIRUP",
        "ITRAP",
        "Outside",
        "IDOOR",
        "ISECDOOR"
    }};

    return imageId < ImageNames.size() ? ImageNames[imageId] : "";
}

const char *defaultDungeonTransitionImageName()
{
    // TODO: Replace this with data-accurate Ticon01..Ticon05 selection per dungeon transition.
    return "Ticon01";
}

const char *defaultOutdoorTransitionImageName()
{
    return "Outside";
}

NpcEntry runtimeNpcEntry(
    const NpcDialogTable &npcDialogTable,
    const EventRuntimeState &eventRuntimeState,
    uint32_t npcId)
{
    const NpcEntry *pBaseNpc = npcDialogTable.getNpc(npcId);
    NpcEntry entry = pBaseNpc != nullptr ? *pBaseNpc : NpcEntry{};
    entry.id = npcId;

    const std::unordered_map<uint32_t, std::string>::const_iterator nameIt =
        eventRuntimeState.npcNameOverrides.find(npcId);
    if (nameIt != eventRuntimeState.npcNameOverrides.end())
    {
        entry.name = nameIt->second;
    }

    const std::unordered_map<uint32_t, uint32_t>::const_iterator pictureIt =
        eventRuntimeState.npcPictureOverrides.find(npcId);
    if (pictureIt != eventRuntimeState.npcPictureOverrides.end())
    {
        entry.pictureId = pictureIt->second;
    }

    const std::unordered_map<uint32_t, uint32_t>::const_iterator professionIt =
        eventRuntimeState.npcProfessionOverrides.find(npcId);
    if (professionIt != eventRuntimeState.npcProfessionOverrides.end())
    {
        entry.professionId = professionIt->second;
    }

    return entry;
}

std::optional<NpcEntry> runtimeNpcEntryIfExists(
    const NpcDialogTable *pNpcDialogTable,
    const EventRuntimeState &eventRuntimeState,
    uint32_t npcId)
{
    if (pNpcDialogTable == nullptr)
    {
        return std::nullopt;
    }

    const bool hasBaseNpc = pNpcDialogTable->getNpc(npcId) != nullptr;
    const bool hasRuntimeNpc =
        eventRuntimeState.npcNameOverrides.contains(npcId)
        || eventRuntimeState.npcPictureOverrides.contains(npcId)
        || eventRuntimeState.npcProfessionOverrides.contains(npcId);

    if (!hasBaseNpc && !hasRuntimeNpc)
    {
        return std::nullopt;
    }

    return runtimeNpcEntry(*pNpcDialogTable, eventRuntimeState, npcId);
}

std::string lowerMapFileName(const std::string &fileName)
{
    return toLowerCopy(fileName);
}

bool hasHiredNpcFollower(const EventRuntimeState &eventRuntimeState, uint32_t npcId)
{
    return std::find_if(
        eventRuntimeState.hiredNpcFollowers.begin(),
        eventRuntimeState.hiredNpcFollowers.end(),
        [npcId](const EventRuntimeState::HiredNpcFollower &follower)
        {
            return follower.npcId == npcId;
        }) != eventRuntimeState.hiredNpcFollowers.end();
}

bool npcCanOfferProfessionHire(
    const NpcEntry &npc,
    const MergedNpcProfessionEntry &profession,
    bool allowProfessionBasedHire)
{
    return npc.joins || (allowProfessionBasedHire && profession.joins);
}

void appendNpcBtbAction(
    EventDialogContent &dialog,
    const NpcDialogTable &npcDialogTable,
    uint32_t topicId,
    uint32_t successTextId,
    uint32_t failTextId,
    bool accepts,
    uint32_t cost = 0,
    const ScriptedEventProgram *pGlobalProgram = nullptr)
{
    const std::optional<NpcDialogTable::ResolvedTopic> topic = npcDialogTable.getTopicById(topicId);

    EventDialogAction action = {};
    action.kind = EventDialogActionKind::NpcBtb;
    action.id = topicId;
    action.secondaryId = accepts ? successTextId : failTextId;
    action.argument = accepts ? "accepted" : "rejected";
    action.label = topic && !topic->topic.empty() ? topic->topic : "Talk";
    if (topicId == NpcBribeTopicId)
    {
        const std::optional<std::string> gold =
            pGlobalProgram != nullptr ? pGlobalProgram->getImportedGlobalText(97) : std::nullopt;
        action.label += " " + std::to_string(cost) + " " + gold.value_or("Gold");
    }
    dialog.actions.push_back(std::move(action));
}

const MergedContinentSettingEntry *currentContinentSetting(
    const MapStatsEntry *pCurrentMap,
    const MergedContinentSettingTable *pContinentSettingTable)
{
    if (pCurrentMap == nullptr || pContinentSettingTable == nullptr)
    {
        return nullptr;
    }

    return pContinentSettingTable->findById(pCurrentMap->mergedContinentId);
}

bool continentAllowsProfessionNews(
    const MapStatsEntry *pCurrentMap,
    const MergedContinentSettingTable *pContinentSettingTable)
{
    const MergedContinentSettingEntry *pContinentSetting =
        currentContinentSetting(pCurrentMap, pContinentSettingTable);

    return pContinentSetting == nullptr || pContinentSetting->tellProfessionNews;
}

bool continentAllowsNpcFollowers(
    const MapStatsEntry *pCurrentMap,
    const MergedContinentSettingTable *pContinentSettingTable)
{
    const MergedContinentSettingEntry *pContinentSetting =
        currentContinentSetting(pCurrentMap, pContinentSettingTable);

    return pContinentSetting == nullptr || pContinentSetting->npcFollowers;
}

bool randomNpcNeedsBtbGate(
    const EventRuntimeState &eventRuntimeState,
    const IGameplayWorldRuntime *pWorld,
    uint32_t npcId,
    std::optional<uint32_t> actorIndex)
{
    const std::optional<GeneratedNpcIdentity> identity = pWorld != nullptr
        ? generatedNpcIdentity(eventRuntimeState, npcId, *pWorld, actorIndex) : std::nullopt;
    const std::optional<EventRuntimeState::DialogueRuntimeState::GeneratedNpcConversation> &conversation =
        eventRuntimeState.dialogueState.generatedNpcConversation;
    return identity && conversation && conversation->npcId == npcId && conversation->actorKey == identity->actorKey
        && conversation->admissionReady && !conversation->willSpeak;
}

std::string lowerTransitionTitle(const std::string &title)
{
    std::string normalized = toLowerCopy(title);
    normalized.erase(
        std::remove(normalized.begin(), normalized.end(), '"'),
        normalized.end());

    return normalized;
}

std::string comparableTransitionTitle(const std::string &title)
{
    std::string normalized = lowerTransitionTitle(title);

    constexpr const char *ThePrefix = "the ";
    if (normalized.starts_with(ThePrefix))
    {
        normalized.erase(0, std::char_traits<char>::length(ThePrefix));
    }

    return normalized;
}

std::string transitionVideoNameForTransitionTitle(const std::string &title)
{
    const std::string normalized = lowerTransitionTitle(title);

    static constexpr std::pair<const char *, const char *> TitleVideos[] = {
        {"abandoned temple", "ab_temp"},
        {"pirate outpost", "p_outpst"},
        {"smuggler's cove", "smg_cove"},
        {"dire wolf lair", "dire_lr"},
        {"dire wolf den", "dire_lr"},
        {"merchanthouse of alvar", "mrch_hs"},
        {"merchant house of alvar", "mrch_hs"},
        {"inside the crystal", "in_cryst"},
        {"escaton's crystal", "in_cryst"},
        {"wasp nest", "wasp_nst"},
        {"ogre raiding fort", "ogre_ft"},
        {"ogre fortress", "ogre_ft"},
        {"troll tomb", "trol_tmb"},
        {"ancient troll home", "trol_tmb"},
        {"cyclops larder", "cyc_lr"},
        {"chain of fire", "ch_fire"},
        {"dragon hunter camp", "dh_camp"},
        {"dragon hunter's camp", "dh_camp"},
        {"dragon cave", "drgn_cav"},
        {"ilsingore's cave", "drgn_cav"},
        {"yaardrake's cave", "drgn_cav"},
        {"old loeb's cave", "drgn_cav"},
        {"naga vault", "naga_vlt"},
        {"temple of the sun", "tpl_sun"},
        {"abandoned druid circle", "ab_druid"},
        {"druid circle", "ab_druid"},
        {"minotaur lair", "mino_lr"},
        {"balthazar lair", "mino_lr"},
        {"barbarian fortress", "barb_frt"},
        {"pirate stronghold", "p_strong"},
        {"abandoned pirate keep", "a_p_keep"},
        {"small sub pen", "s_subpen"},
        {"hand-cranked submarine", "s_subpen"},
        {"passage under regna", "rsub"},
        {"necromancers' guild", "nec_gild"},
        {"mad necromancer's lab", "mad_lab"},
        {"castle of air", "cstl_air"},
        {"castle of fire", "cstl_fir"},
        {"war camp", "war_camp"},
        {"eschaton's palace", "esch_pal"},
        {"prison of the lord of air", "pr_elords"},
        {"prison of the lord of earth", "pr_lorde"},
        {"prison of the lord of fire", "pr_lordf"},
        {"prison of the lord of water", "pr_lordw"},
        {"gateway to the plane of air", "gw_air"},
        {"gateway to the plane of earth", "gw_earth"},
        {"gateway to the plane of fire", "gw_fire"},
        {"gateway to the plane of water", "gw_water"}
    };

    for (const std::pair<const char *, const char *> &entry : TitleVideos)
    {
        if (normalized == entry.first)
        {
            return entry.second;
        }
    }

    return {};
}

std::string transitionVideoNameForSourceNote(const std::string &note)
{
    const std::string exact = transitionVideoNameForTransitionTitle(note);
    if (!exact.empty())
    {
        return exact;
    }

    // Source notes sometimes append an area number; only known video names may match after removal.
    std::string title = lowerTransitionTitle(note);
    const size_t area = title.rfind(" (area ");
    size_t digits = std::string::npos;
    size_t end = title.size();
    size_t cut = std::string::npos;
    if (area != std::string::npos && title.ends_with(')'))
    {
        digits = area + 7;
        --end;
        cut = area;
    }
    else
    {
        const size_t separator = title.find_last_of(" ,");
        if (separator != std::string::npos)
        {
            digits = separator + 1;
            cut = separator;
        }
    }
    if (digits == std::string::npos || digits >= end
        || !std::all_of(title.begin() + digits, title.begin() + end,
            [](char value) { return value >= '0' && value <= '9'; }))
    {
        return {};
    }
    while (cut > 0 && (title[cut - 1] == ' ' || title[cut - 1] == ','))
    {
        --cut;
    }
    title.resize(cut);
    return transitionVideoNameForTransitionTitle(title);
}

std::string transitionVideoNameForDungeonHouse(
    const HouseTable *pHouseTable,
    const MapStatsEntry *pCurrentMap,
    const std::string &transitionTitle)
{
    if (pHouseTable == nullptr || pCurrentMap == nullptr || transitionTitle.empty())
    {
        return {};
    }

    const std::string normalizedTitle = comparableTransitionTitle(transitionTitle);

    for (const auto &[houseId, houseEntry] : pHouseTable->entries())
    {
        static_cast<void>(houseId);

        if (houseEntry.type != "Dungeon Ent"
            || houseEntry.mapId != static_cast<uint32_t>(pCurrentMap->id)
            || houseEntry.videoName.empty())
        {
            continue;
        }

        if (comparableTransitionTitle(houseEntry.name) == normalizedTitle
            || comparableTransitionTitle(houseEntry.buildingName) == normalizedTitle)
        {
            return houseEntry.videoName;
        }
    }

    return {};
}

std::string transitionVideoNameForDungeonHouseId(const HouseTable *pHouseTable, uint32_t houseId)
{
    if (pHouseTable == nullptr || houseId == 0)
    {
        return {};
    }

    const HouseEntry *pHouseEntry = pHouseTable->get(houseId);

    if (pHouseEntry == nullptr || pHouseEntry->type != "Dungeon Ent" || pHouseEntry->videoName.empty())
    {
        return {};
    }

    return pHouseEntry->videoName;
}

std::string transitionVideoNameForMap(const std::string &mapFileName)
{
    const std::string normalized = lowerMapFileName(mapFileName);

    static constexpr std::pair<const char *, const char *> MapVideos[] = {
        {"d05.blv", "ab_temp"},
        {"d06.blv", "p_outpst"},
        {"d07.blv", "smg_cove"},
        {"d08.blv", "dire_lr"},
        {"d09.blv", "mrch_hs"},
        {"d10.blv", "in_cryst"},
        {"d11.blv", "wasp_nst"},
        {"d12.blv", "ogre_ft"},
        {"d13.blv", "trol_tmb"},
        {"d14.blv", "cyc_lr"},
        {"d15.blv", "ch_fire"},
        {"d16.blv", "dh_camp"},
        {"d17.blv", "drgn_cav"},
        {"d18.blv", "naga_vlt"},
        {"d19.blv", "nec_gild"},
        {"d20.blv", "mad_lab"},
        {"d22.blv", "tpl_sun"},
        {"d23.blv", "ab_druid"},
        {"d24.blv", "mino_lr"},
        {"d25.blv", "barb_frt"},
        {"d27.blv", "cstl_air"},
        {"d29.blv", "cstl_fir"},
        {"d30.blv", "war_camp"},
        {"d31.blv", "p_strong"},
        {"d32.blv", "a_p_keep"},
        {"d33.blv", "rsub"},
        {"d34.blv", "s_subpen"},
        {"d35.blv", "esch_pal"},
        {"d36.blv", "pr_elords"},
        {"d37.blv", "pr_lordf"},
        {"d38.blv", "pr_lordw"},
        {"d39.blv", "pr_lorde"},
        {"d43.blv", "trol_tmb"},
        {"d47.blv", "drgn_cav"},
        {"d48.blv", "drgn_cav"},
        {"d49.blv", "drgn_cav"},
        {"elema.odm", "gw_air"},
        {"eleme.odm", "gw_earth"},
        {"elemf.odm", "gw_fire"},
        {"elemw.odm", "gw_water"}
    };

    for (const std::pair<const char *, const char *> &entry : MapVideos)
    {
        if (normalized == entry.first)
        {
            return entry.second;
        }
    }

    return {};
}

const MapStatsEntry *findMapEntryByFileName(
    const std::vector<MapStatsEntry> *pMapEntries,
    const std::string &fileName)
{
    if (pMapEntries == nullptr)
    {
        return nullptr;
    }

    const std::string normalizedFileName = toLowerCopy(fileName);

    for (const MapStatsEntry &entry : *pMapEntries)
    {
        if (toLowerCopy(entry.fileName) == normalizedFileName)
        {
            return &entry;
        }
    }

    return nullptr;
}

const std::optional<MapEdgeTransition> *currentMapTransitionForContext(
    const EventRuntimeState::PendingDialogueContext &context,
    const MapStatsEntry *pCurrentMap,
    const std::string &currentMapFileName)
{
    if (context.kind != DialogueContextKind::MapTransition || pCurrentMap == nullptr
        || context.sourceId > static_cast<uint32_t>(MapBoundaryEdge::West))
    {
        return nullptr;
    }

    const std::optional<MapEdgeTransition> *pTransition =
        pCurrentMap->edgeTransition(static_cast<MapBoundaryEdge>(context.sourceId));
    if (context.transitionMapMove)
    {
        const EventRuntimeState::PendingMapMove &move = *context.transitionMapMove;
        if (move.traceSourceKind != "map_boundary" || move.traceSourceId != context.sourceId
            || !lowerMapFileName(pCurrentMap->fileName).ends_with(".odm")
            || lowerMapFileName(currentMapFileName) != lowerMapFileName(pCurrentMap->fileName)
            || pTransition == nullptr || !pTransition->has_value() || !move.mapName || move.mapName->empty()
            || lowerMapFileName(*move.mapName) != lowerMapFileName((*pTransition)->destinationMapFileName))
        {
            return nullptr;
        }
    }

    return pTransition;
}

std::vector<std::string> wrapDialogText(const std::string &text, size_t width)
{
    if (text.empty() || width == 0)
    {
        return {};
    }

    std::vector<std::string> lines;
    std::string currentLine;

    for (char character : text)
    {
        if (character == '\r')
        {
            continue;
        }

        if (character == '\n')
        {
            lines.push_back(currentLine);
            currentLine.clear();
            continue;
        }

        currentLine.push_back(character);
    }

    if (!currentLine.empty() || lines.empty())
    {
        lines.push_back(currentLine);
    }

    return lines;
}

bool shouldDisplayHouseType(const std::string &houseType)
{
    return houseType == "Weapon Shop"
        || houseType == "Armor Shop"
        || houseType == "Magic Shop"
        || houseType == "Alchemist"
        || houseType == "Stables"
        || houseType == "Boats"
        || houseType == "Temple"
        || houseType == "Training"
        || houseType == "Town Hall"
        || houseType == "Tavern"
        || houseType == "Bank"
        || houseType == "Spell Shop"
        || houseType.find("Guild") != std::string::npos;
}

std::string buildHouseParticipantTitle(const HouseEntry &houseEntry)
{
    std::string title;

    if (!houseEntry.proprietorName.empty() && houseEntry.proprietorName != "Placeholder")
    {
        title = houseEntry.proprietorName;
    }
    else
    {
        title = houseEntry.name;
    }

    if (!houseEntry.proprietorTitle.empty() && houseEntry.proprietorTitle != "Placeholder")
    {
        title += ", " + houseEntry.proprietorTitle;
    }

    return title;
}

uint32_t weekDayIndexFromGameMinutes(float currentGameMinutes)
{
    if (currentGameMinutes < 0.0f)
    {
        return 0;
    }

    constexpr float MinutesPerDay = 1440.0f;
    return static_cast<uint32_t>(currentGameMinutes / MinutesPerDay) % 7u;
}

DialogueMenuId currentDialogueMenuId(const EventRuntimeState &eventRuntimeState)
{
    if (eventRuntimeState.dialogueState.menuStack.empty())
    {
        return DialogueMenuId::None;
    }

    return eventRuntimeState.dialogueState.menuStack.back();
}

DialogueMenuId currentHouseServiceMenuId(const EventRuntimeState &eventRuntimeState)
{
    const DialogueMenuId menuId = currentDialogueMenuId(eventRuntimeState);
    return menuId == DialogueMenuId::HouseServiceRoot ? DialogueMenuId::None : menuId;
}

std::vector<uint32_t> collectSelectableResidentNpcIdsImpl(
    const HouseEntry &houseEntry,
    const NpcDialogTable &npcDialogTable,
    const EventRuntimeState &eventRuntimeState);

std::optional<uint32_t> singleSelectableResidentNpcId(
    const HouseEntry &houseEntry,
    const NpcDialogTable &npcDialogTable,
    const EventRuntimeState &eventRuntimeState
)
{
    const std::vector<uint32_t> residentNpcIdsForHouse =
        collectSelectableResidentNpcIdsImpl(houseEntry, npcDialogTable, eventRuntimeState);

    std::optional<uint32_t> residentNpcId;

    for (uint32_t candidateNpcId : residentNpcIdsForHouse)
    {
        if (residentNpcId)
        {
            return std::nullopt;
        }

        residentNpcId = candidateNpcId;
    }

    return residentNpcId;
}

std::vector<uint32_t> collectSelectableResidentNpcIdsImpl(
    const HouseEntry &houseEntry,
    const NpcDialogTable &npcDialogTable,
    const EventRuntimeState &eventRuntimeState)
{
    std::vector<uint32_t> residentNpcIds;

    auto appendNpcId = [&](uint32_t npcId)
    {
        if (std::find(residentNpcIds.begin(), residentNpcIds.end(), npcId) != residentNpcIds.end())
        {
            return;
        }

        const NpcEntry *pResident = npcDialogTable.getNpc(npcId);

        if (pResident == nullptr || pResident->name.empty())
        {
            return;
        }

        if (eventRuntimeState.unavailableNpcIds.contains(npcId))
        {
            return;
        }

        const auto overrideIt = eventRuntimeState.npcHouseOverrides.find(npcId);

        if (overrideIt != eventRuntimeState.npcHouseOverrides.end() && overrideIt->second != houseEntry.id)
        {
            return;
        }

        residentNpcIds.push_back(npcId);
    };

    for (uint32_t npcId : houseEntry.residentNpcIds)
    {
        appendNpcId(npcId);
    }

    for (uint32_t npcId : npcDialogTable.getNpcIdsForHouse(houseEntry.id, &eventRuntimeState.npcHouseOverrides))
    {
        appendNpcId(npcId);
    }

    return residentNpcIds;
}

void appendHouseResidentActions(
    EventDialogContent &dialog,
    const NpcDialogTable &npcDialogTable,
    const EventRuntimeState &eventRuntimeState,
    const std::vector<uint32_t> &residentNpcIds)
{
    for (uint32_t residentNpcId : residentNpcIds)
    {
        const std::optional<NpcEntry> resident =
            runtimeNpcEntryIfExists(&npcDialogTable, eventRuntimeState, residentNpcId);

        if (!resident || resident->name.empty())
        {
            continue;
        }

        EventDialogAction action = {};
        action.kind = EventDialogActionKind::HouseResident;
        action.id = residentNpcId;
        action.participantPictureId = resident->pictureId;
        action.participantVisual = EventDialogParticipantVisual::Portrait;
        action.label = resident->name;
        dialog.actions.push_back(std::move(action));
    }
}

void appendHouseExtraExitAction(EventDialogContent &dialog, const HouseEntry &houseEntry)
{
    if (!houseEntry.extraExit.has_value())
    {
        return;
    }

    EventDialogAction action = {};
    action.kind = EventDialogActionKind::HouseExtraExit;
    action.id = houseEntry.id;
    action.participantPictureId = houseEntry.extraExit->pictureId;
    action.participantVisual = EventDialogParticipantVisual::Portrait;
    action.label = !houseEntry.extraExit->destinationName.empty()
        ? houseEntry.extraExit->destinationName
        : houseEntry.extraExit->label;
    dialog.actions.push_back(std::move(action));
}
}

std::vector<uint32_t> collectSelectableResidentNpcIds(
    const HouseEntry &houseEntry,
    const NpcDialogTable &npcDialogTable,
    const EventRuntimeState &eventRuntimeState)
{
    return collectSelectableResidentNpcIdsImpl(houseEntry, npcDialogTable, eventRuntimeState);
}

uint32_t npcBtbDialogueAccessVariableKey(uint32_t npcId)
{
    constexpr uint32_t NpcBtbDialogueAccessVariableBase = 0x7B000000u;
    return NpcBtbDialogueAccessVariableBase | (npcId & 0x00FFFFFFu);
}

uint32_t npcBtbDialogueAccessDay(float currentGameMinutes)
{
    if (currentGameMinutes < 0.0f)
    {
        return 0;
    }

    constexpr uint32_t MinutesPerDay = 24u * 60u;
    constexpr uint32_t DaysPerMonth = 28u;
    const uint32_t elapsedDays = static_cast<uint32_t>(currentGameMinutes) / MinutesPerDay;
    return (elapsedDays % DaysPerMonth) + 1u;
}

bool npcProfessionActionTopicHasDailyCooldown(uint32_t topicId)
{
    return topicId >= static_cast<uint32_t>(NpcFollowerActionTopicId::HealParty)
        && topicId <= static_cast<uint32_t>(NpcFollowerActionTopicId::CastHeroism);
}

uint32_t npcProfessionActionCooldownVariableKey(uint32_t npcId)
{
    constexpr uint32_t NpcProfessionActionCooldownVariableBase = 0x7C000000u;
    return NpcProfessionActionCooldownVariableBase | (npcId & 0x00FFFFFFu);
}

uint32_t npcProfessionActionCooldownDay(float currentGameMinutes)
{
    if (currentGameMinutes < 0.0f)
    {
        return 0;
    }

    constexpr uint32_t MinutesPerDay = 24u * 60u;
    constexpr float OeDailyResetMinute = 3.0f * 60.0f;
    const int32_t elapsedResetDays =
        static_cast<int32_t>(std::floor((currentGameMinutes - OeDailyResetMinute) / MinutesPerDay));
    return static_cast<uint32_t>(std::max<int32_t>(0, elapsedResetDays + 1));
}

bool npcProfessionActionUsedToday(
    const EventRuntimeState &eventRuntimeState,
    uint32_t npcId,
    float currentGameMinutes)
{
    const uint32_t currentDay = npcProfessionActionCooldownDay(currentGameMinutes);

    const auto followerIt = std::find_if(
        eventRuntimeState.hiredNpcFollowers.begin(),
        eventRuntimeState.hiredNpcFollowers.end(),
        [npcId](const EventRuntimeState::HiredNpcFollower &follower)
        {
            return follower.npcId == npcId;
        });

    if (followerIt != eventRuntimeState.hiredNpcFollowers.end()
        && followerIt->abilityUsedDay != 0
        && followerIt->abilityUsedDay == currentDay)
    {
        return true;
    }

    const std::unordered_map<uint32_t, int32_t>::const_iterator variableIt =
        eventRuntimeState.variables.find(npcProfessionActionCooldownVariableKey(npcId));
    return variableIt != eventRuntimeState.variables.end()
        && variableIt->second == static_cast<int32_t>(currentDay);
}

EventDialogContent buildEventDialogContent(
    EventRuntimeState &eventRuntimeState,
    size_t previousMessageCount,
    bool allowNpcFallbackContent,
    const std::optional<ScriptedEventProgram> *pGlobalEventProgram,
    const HouseTable *pHouseTable,
    const ClassSkillTable *pClassSkillTable,
    const NpcDialogTable *pNpcDialogTable,
    const TransitionTable *pTransitionTable,
    const MapStatsEntry *pCurrentMap,
    const std::vector<MapStatsEntry> *pMapEntries,
    const Party *pParty,
    const IGameplayWorldRuntime *pWorldRuntime,
    float currentGameMinutes,
    const MergedNpcProfessionTable *pNpcProfessionTable,
    const MergedNewsProfessionTopicTable *pNewsProfessionTopicTable,
    const MergedNpcBtbTable *pNpcBtbTable,
    const MergedTeacherTopicTable *pTeacherTopicTable,
    const MergedContinentSettingTable *pContinentSettingTable,
    const CharacterInspectTable *pCharacterInspectTable,
    const HouseServiceTextTable *pHouseServiceTextTable,
    const UiTextTable *pUiTextTable
)
{
    EventDialogContent dialog = {};

    if (!eventRuntimeState.pendingDialogueContext
        || eventRuntimeState.pendingDialogueContext->kind == DialogueContextKind::None)
    {
        return dialog;
    }

    EventRuntimeState npcRuntimeState = eventRuntimeState;
    if (pParty != nullptr)
    {
        pParty->applyGlobalNpcStateTo(npcRuntimeState);
    }

    if (eventRuntimeState.pendingDialogueContext->kind == DialogueContextKind::HouseService
        && pHouseTable != nullptr
        && pNpcDialogTable != nullptr)
    {
        const HouseEntry *pHouseEntry = pHouseTable->get(eventRuntimeState.pendingDialogueContext->sourceId);

        if (pHouseEntry != nullptr)
        {
            const std::vector<HouseActionOption> houseActions = buildHouseActionOptions(
                *pHouseEntry,
                pParty,
                pClassSkillTable,
                pWorldRuntime,
                currentGameMinutes,
                currentHouseServiceMenuId(eventRuntimeState),
                pCharacterInspectTable,
                pHouseServiceTextTable,
                pUiTextTable
            );
            const std::optional<uint32_t> residentNpcId = singleSelectableResidentNpcId(
                *pHouseEntry,
                *pNpcDialogTable,
                npcRuntimeState
            );

            const bool hasPrisonSentenceAction = pParty != nullptr && pParty->fineGold() > 0
                && pHouseEntry->type == "Throne";
            if (residentNpcId && houseActions.empty() && !hasPrisonSentenceAction)
            {
                EventRuntimeState::PendingDialogueContext context = {};
                context.kind = DialogueContextKind::NpcTalk;
                context.sourceId = *residentNpcId;
                context.hostHouseId = pHouseEntry->id;
                eventRuntimeState.pendingDialogueContext = std::move(context);
            }
        }
    }

    const EventRuntimeState::PendingDialogueContext &context = *eventRuntimeState.pendingDialogueContext;
    dialog.isActive = true;
    dialog.isHouseDialog = context.kind == DialogueContextKind::HouseService;
    dialog.sourceId = context.sourceId;
    dialog.sourceActorIndex = context.sourceActorIndex;
    bool allowEmptyNpcTalkDialog = false;

    constexpr size_t MaxLineWidth = 58;
    std::vector<std::string> eventMessageLines;
    const EventRuntimeState::DialogueOfferState *pCurrentOffer =
        eventRuntimeState.dialogueState.currentOffer ? &*eventRuntimeState.dialogueState.currentOffer : nullptr;

    if (previousMessageCount < eventRuntimeState.messages.size())
    {
        for (
            size_t messageIndex = previousMessageCount;
            messageIndex < eventRuntimeState.messages.size();
            ++messageIndex)
        {
            const std::vector<std::string> wrappedLines =
                wrapDialogText(eventRuntimeState.messages[messageIndex], MaxLineWidth);
            eventMessageLines.insert(eventMessageLines.end(), wrappedLines.begin(), wrappedLines.end());
        }
    }

    if (context.kind == DialogueContextKind::NpcTalk && pWorldRuntime != nullptr && context.sourceActorIndex)
    {
        const auto target = pWorldRuntime->ownedMercenaryActorBinding(context.sourceId, *context.sourceActorIndex);
        if (target)
        {
            // Recognition is separate from current hire eligibility. A stale/active/released binding
            // must never fall through to generic roster recruitment and generate a substitute Character.
            dialog.lines = eventMessageLines;
            const GameSession *pSession = pWorldRuntime->boundGameSession();
            if (pParty == nullptr || pSession == nullptr || !pSession->hasDataRepository()
                || !pSession->data().breachMercenaryProfile())
                return dialog;
            const auto &profile = *pSession->data().breachMercenaryProfile();
            const auto resolved = resolveOwnedMercenary(*pParty, target->binding.memberId, profile);
            if (resolved.pCharacter)
            {
                dialog.title = resolved.pCharacter->name;
                const auto *pDoll = pSession->data().characterDollTable().getCharacter(
                    resolved.pCharacter->characterDataId);
                if (pDoll) dialog.participantPictureId = pDoll->npcPictureId;
            }
            if (resolved.code != OwnedMercenaryResolutionCode::Eligible || !target->actorInteractable
                || resolved.sourceRosterId != target->binding.sourceRosterId || resolved.npcId != context.sourceId
                || target->profileVersion != profile.version || target->canonicalMapKey != profile.canonicalMapKey
                || target->sourceSha256 != profile.sourceSha256)
                return dialog;
            const EventRuntimeState::OwnedMercenaryJoinOffer expected{
                target->binding, resolved.owner, target->canonicalMapKey, target->profileVersion,
                target->sourceSha256, target->sceneGeneration, target->sourceActorIndex};
            const auto addAction = [&](EventDialogActionKind kind, const std::string &label)
            {
                EventDialogAction action;
                action.kind = kind;
                action.id = context.sourceId;
                action.label = label;
                dialog.actions.push_back(std::move(action));
            };
            if (eventRuntimeState.dialogueState.ownedMercenaryOffer == expected)
            {
                addAction(EventDialogActionKind::OwnedMercenaryJoinAccept, profile.yesText);
                addAction(EventDialogActionKind::OwnedMercenaryJoinDecline, profile.noText);
            }
            else
            {
                eventRuntimeState.dialogueState.ownedMercenaryOffer.reset();
                const auto topic = pSession->data().npcDialogTable().getTopicById(profile.joinTopicId);
                if (topic) addAction(EventDialogActionKind::OwnedMercenaryJoinOffer, topic->topic);
            }
            return dialog;
        }
    }

    if (context.kind == DialogueContextKind::MapEvent)
    {
        dialog.participantPictureId = context.participantPictureId;

        if (context.titleOverride.has_value())
        {
            dialog.title = *context.titleOverride;
        }
    }
    else if (context.kind == DialogueContextKind::MapTransition)
    {
        dialog.participantVisual = EventDialogParticipantVisual::MapIcon;
        dialog.presentation = EventDialogPresentation::Transition;
        dialog.participantPictureId = 0;
        const std::optional<MapEdgeTransition> *pTransition =
            currentMapTransitionForContext(context, pCurrentMap, eventRuntimeState.mapFileName);
        const std::string transitionMapName =
            context.transitionMapMove.has_value() && context.transitionMapMove->mapName.has_value()
                ? *context.transitionMapMove->mapName
                : ((pTransition != nullptr && pTransition->has_value())
                    ? (*pTransition)->destinationMapFileName
                    : std::string());
        const std::string currentMapName = pCurrentMap != nullptr ? pCurrentMap->fileName : std::string();
        const bool destinationIsDungeon =
            !transitionMapName.empty() && lowerMapFileName(transitionMapName).find(".blv") != std::string::npos;
        const bool currentMapIsDungeon =
            !currentMapName.empty() && lowerMapFileName(currentMapName).find(".blv") != std::string::npos;
        const bool isDungeonTransition = destinationIsDungeon || currentMapIsDungeon;
        dialog.participantTextureName =
            context.transitionImageId != 0
                ? transitionImageName(context.transitionImageId)
                : (isDungeonTransition
                    ? defaultDungeonTransitionImageName()
                    : defaultOutdoorTransitionImageName());
        const MapStatsEntry *pDestinationMap =
            !transitionMapName.empty()
                ? findMapEntryByFileName(pMapEntries, transitionMapName)
                : nullptr;
        const bool leavingCurrentDungeon = currentMapIsDungeon && !destinationIsDungeon;
        const TransitionEntry *pTransitionText =
            pTransitionTable != nullptr && context.transitionTextId != 0
                ? pTransitionTable->get(context.transitionTextId)
                : nullptr;

        if (leavingCurrentDungeon && pCurrentMap != nullptr && !pCurrentMap->name.empty())
        {
            dialog.title = pCurrentMap->name;
        }
        else if (pDestinationMap != nullptr)
        {
            dialog.title = pDestinationMap->name;
        }
        else if (context.titleOverride.has_value())
        {
            dialog.title = *context.titleOverride;
        }
        else if (pTransitionText != nullptr && !pTransitionText->title.empty())
        {
            dialog.title = pTransitionText->title;
        }
        else
        {
            dialog.title = !transitionMapName.empty() ? transitionMapName : "Travel";
        }

        const MapStatsEntry *pDungeonHouseMap = destinationIsDungeon ? pCurrentMap : nullptr;
        if (!leavingCurrentDungeon)
        {
            dialog.videoName = transitionVideoNameForDungeonHouseId(pHouseTable, context.transitionTextId);
            if (dialog.videoName.empty())
            {
                dialog.videoName = pDungeonHouseMap != nullptr
                    ? transitionVideoNameForDungeonHouse(pHouseTable, pDungeonHouseMap, dialog.title)
                    : std::string();
            }
            if (dialog.videoName.empty() && pTransitionText != nullptr)
            {
                dialog.videoName = pTransitionText->sourceNote.empty()
                    ? transitionVideoNameForTransitionTitle(pTransitionText->title)
                    : transitionVideoNameForSourceNote(pTransitionText->sourceNote);
            }
            if (dialog.videoName.empty() && !dialog.title.empty())
            {
                dialog.videoName = transitionVideoNameForTransitionTitle(dialog.title);
            }
            if (dialog.videoName.empty())
            {
                dialog.videoName = transitionVideoNameForMap(transitionMapName);
            }
        }
        dialog.videoDirectory = !dialog.videoName.empty() ? TransitionVideoDirectory : std::string();

        // A positioned script move has no implicit journey duration. Only an explicit edge route
        // supplies days here; two outdoor filenames alone do not describe a walking journey.
        const int travelDays = pTransition != nullptr && pTransition->has_value() ? (*pTransition)->travelDays : 0;
        if (travelDays > 0)
        {
            std::string duration =
                "It will take "
                + std::to_string(travelDays)
                + " day"
                + (travelDays == 1 ? "" : "s")
                + " to travel to "
                + dialog.title
                + ".";
            if (pHouseServiceTextTable != nullptr)
            {
                HouseServiceTextTable::Arguments arguments;
                arguments.days = std::to_string(travelDays);
                arguments.destination = dialog.title;
                const std::string translated = pHouseServiceTextTable->format(
                    travelDays == 1 ? HouseServiceTextTable::Message::TransportSuccessSingle
                                    : HouseServiceTextTable::Message::TransportSuccessPlural,
                    arguments);
                if (!translated.empty())
                {
                    duration = translated;
                }
            }
            dialog.lines.push_back(std::move(duration));
        }

        if (pTransitionText != nullptr && !pTransitionText->description.empty())
        {
            dialog.lines.push_back(pTransitionText->description);
        }
        else if (leavingCurrentDungeon)
        {
            dialog.lines.push_back(pUiTextTable != nullptr
                ? pUiTextTable->formatTransitionLeave(dialog.title)
                : "Do you wish to leave " + dialog.title + "?");
        }
        else if (context.transitionMapMove.has_value())
        {
            dialog.lines.push_back(pUiTextTable != nullptr
                ? pUiTextTable->formatTransitionEnter(dialog.title)
                : "Do you wish to enter " + dialog.title + "?");
        }
        else if (pCurrentMap != nullptr && !pCurrentMap->name.empty())
        {
            dialog.lines.push_back(pUiTextTable != nullptr
                ? pUiTextTable->formatTransitionLeave(pCurrentMap->name)
                : "Do you wish to leave " + pCurrentMap->name + "?");
        }
        else
        {
            dialog.lines.push_back(pUiTextTable != nullptr
                ? pUiTextTable->text(UiTextTable::Message::TransitionLeaveArea)
                : "Do you wish to leave this area?");
        }

        EventDialogAction confirmAction = {};
        confirmAction.kind = EventDialogActionKind::MapTransitionConfirm;
        confirmAction.label = pUiTextTable != nullptr ? pUiTextTable->text(UiTextTable::Message::Confirm) : "OK";
        dialog.actions.push_back(std::move(confirmAction));

        EventDialogAction cancelAction = {};
        cancelAction.kind = EventDialogActionKind::MapTransitionCancel;
        cancelAction.label = pUiTextTable != nullptr ? pUiTextTable->text(UiTextTable::Message::Cancel) : "Close";
        dialog.actions.push_back(std::move(cancelAction));
    }
    else if (context.kind == DialogueContextKind::HouseService)
    {
        const HouseEntry *pHouseEntry = pHouseTable != nullptr ? pHouseTable->get(dialog.sourceId) : nullptr;
        dialog.houseTitle = pHouseEntry != nullptr ? pHouseEntry->name : ("House #" + std::to_string(dialog.sourceId));
        dialog.title = dialog.houseTitle;
        dialog.participantPictureId = pHouseEntry != nullptr ? pHouseEntry->proprietorPictureId : 0;
        if (dialog.participantPictureId == 0 && pHouseEntry != nullptr && pHouseEntry->extraExit.has_value())
        {
            dialog.participantPictureId = pHouseEntry->extraExit->pictureId;
        }
        bool hasResidentActions = false;

        if (pHouseEntry != nullptr)
        {
            dialog.scenePolicy = pHouseEntry->dialogueScenePolicy;
            const HouseServiceType serviceType = resolveHouseServiceType(*pHouseEntry);
            const DialogueMenuId menuId = currentDialogueMenuId(eventRuntimeState);
            const DialogueMenuId houseServiceMenuId = currentHouseServiceMenuId(eventRuntimeState);
            const std::vector<uint32_t> residentNpcIds = pNpcDialogTable != nullptr
                ? collectSelectableResidentNpcIds(*pHouseEntry, *pNpcDialogTable, npcRuntimeState)
                : std::vector<uint32_t>{};
            const bool useResidentOnlyLobby =
                shouldUseResidentOnlyHouseLobby(
                    *pHouseEntry,
                    serviceType,
                    residentNpcIds,
                    pNpcDialogTable,
                    npcRuntimeState);
            const bool useResidentExtraExitLobby =
                serviceType == HouseServiceType::None
                && pHouseEntry->type == "House"
                && !residentNpcIds.empty()
                && menuId == DialogueMenuId::None
                && houseExtraExitIsAvailable(*pHouseEntry, pParty);
            const bool showOccupantSelection =
                serviceType != HouseServiceType::None
                && !residentNpcIds.empty()
                && menuId == DialogueMenuId::None
                && !useResidentOnlyLobby;

            if (useResidentExtraExitLobby)
            {
                if (pNpcDialogTable != nullptr)
                {
                    appendHouseResidentActions(dialog, *pNpcDialogTable, eventRuntimeState, residentNpcIds);
                }

                appendHouseExtraExitAction(dialog, *pHouseEntry);
                return dialog;
            }

            if (useResidentOnlyLobby && menuId == DialogueMenuId::None)
            {
                if (pNpcDialogTable != nullptr)
                {
                    appendHouseResidentActions(dialog, *pNpcDialogTable, eventRuntimeState, residentNpcIds);
                }

                if (houseExtraExitIsAvailable(*pHouseEntry, pParty)
                    && residentNpcIds.size() < pHouseEntry->residentNpcIds.size())
                {
                    appendHouseExtraExitAction(dialog, *pHouseEntry);
                }

                return dialog;
            }

            if (showOccupantSelection)
            {
                EventDialogAction proprietorAction = {};
                proprietorAction.kind = EventDialogActionKind::HouseProprietor;
                proprietorAction.id = pHouseEntry->id;
                proprietorAction.participantPictureId = dialog.participantPictureId;
                proprietorAction.participantVisual = EventDialogParticipantVisual::Portrait;
                proprietorAction.label = buildHouseParticipantTitle(*pHouseEntry);
                dialog.actions.push_back(std::move(proprietorAction));

                if (pNpcDialogTable != nullptr)
                {
                    appendHouseResidentActions(dialog, *pNpcDialogTable, eventRuntimeState, residentNpcIds);
                }

                return dialog;
            }

            const std::vector<HouseActionOption> houseActions = buildHouseActionOptions(
                *pHouseEntry,
                pParty,
                pClassSkillTable,
                pWorldRuntime,
                currentGameMinutes,
                houseServiceMenuId,
                pCharacterInspectTable,
                pHouseServiceTextTable,
                pUiTextTable
            );

            for (const HouseActionOption &houseAction : houseActions)
            {
                EventDialogAction action = {};
                action.kind = EventDialogActionKind::HouseService;
                action.id = static_cast<uint32_t>(houseAction.id);
                action.label = houseAction.label;
                action.argument = houseAction.argument;
                action.enabled = houseAction.enabled;
                action.disabledReason = houseAction.disabledReason;
                dialog.actions.push_back(std::move(action));
            }

            if (pParty != nullptr && pParty->fineGold() > 0 && pHouseEntry->type == "Throne")
            {
                EventDialogAction action = {};
                action.kind = EventDialogActionKind::HouseService;
                action.id = static_cast<uint32_t>(HouseActionId::ThroneServeSentence);
                action.label = pUiTextTable != nullptr
                    ? pUiTextTable->text(UiTextTable::Message::ThroneServeSentence) : "Serve Sentence";
                dialog.actions.push_back(std::move(action));
            }

            if (serviceType == HouseServiceType::None
                && menuId == DialogueMenuId::None
                && pNpcDialogTable != nullptr)
            {
                appendHouseResidentActions(dialog, *pNpcDialogTable, eventRuntimeState, residentNpcIds);
                hasResidentActions = !residentNpcIds.empty();
            }

            if (!hasResidentActions && serviceType != HouseServiceType::None)
            {
                dialog.title = buildHouseParticipantTitle(*pHouseEntry);
            }
        }
    }
    else
    {
        dialog.title = "NPC #" + std::to_string(dialog.sourceId);

        const uint32_t overriddenGreetingId =
            npcRuntimeState.npcGreetingOverrides.contains(dialog.sourceId)
                ? npcRuntimeState.npcGreetingOverrides.at(dialog.sourceId)
                : 0;
        const NpcGreetingEntry *pGreeting = pNpcDialogTable != nullptr
            ? (overriddenGreetingId != 0
                ? pNpcDialogTable->getGreeting(overriddenGreetingId)
                : pNpcDialogTable->getGreetingForNpc(dialog.sourceId))
            : nullptr;
        const std::optional<NpcEntry> npcEntry =
            runtimeNpcEntryIfExists(pNpcDialogTable, npcRuntimeState, dialog.sourceId);
        const NpcEntry *pNpc = npcEntry ? &*npcEntry : nullptr;
        const std::optional<GeneratedNpcIdentity> generatedIdentity = pWorldRuntime != nullptr
            ? generatedNpcIdentity(npcRuntimeState, dialog.sourceId, *pWorldRuntime, context.sourceActorIndex)
            : std::nullopt;
        const std::optional<EventRuntimeState::DialogueRuntimeState::GeneratedNpcConversation> &generatedConversation =
            eventRuntimeState.dialogueState.generatedNpcConversation;
        const bool hasGeneratedConversation = generatedIdentity && generatedConversation
            && generatedConversation->npcId == dialog.sourceId
            && generatedConversation->actorKey == generatedIdentity->actorKey;
        const bool reusableMercenaryNpc = pParty != nullptr && isReusableMercenaryNpcId(*pParty, dialog.sourceId);
        const bool hasGeneratedMercenaryCandidate =
            npcRuntimeState.generatedMercenaryRecruitsByNpcId.contains(dialog.sourceId);
        const bool availableGeneratedMercenary = pParty != nullptr
            && isAvailableGeneratedMercenaryRecruit(*pParty, npcRuntimeState, dialog.sourceId);
        const bool hasPendingRosterJoinInvite =
            pCurrentOffer != nullptr
            && pCurrentOffer->kind == DialogueOfferKind::RosterJoin
            && pCurrentOffer->npcId == dialog.sourceId
            && ((!reusableMercenaryNpc && !hasGeneratedMercenaryCandidate
                    && !isGeneratedMercenaryNpcId(dialog.sourceId)
                    && (pParty == nullptr || !isReusableMercenaryRosterId(*pParty, pCurrentOffer->rosterId)))
                || (availableGeneratedMercenary && pCurrentOffer->rosterId
                    == npcRuntimeState.generatedMercenaryRecruitsByNpcId.at(dialog.sourceId).rosterId));
        const bool hasPendingMasteryTeacherOffer =
            pCurrentOffer != nullptr
            && pCurrentOffer->kind == DialogueOfferKind::MasteryTeacher
            && pCurrentOffer->npcId == dialog.sourceId;
        const bool hasPendingGuildMembershipOffer =
            pCurrentOffer != nullptr
            && pCurrentOffer->kind == DialogueOfferKind::GuildMembership
            && pCurrentOffer->npcId == dialog.sourceId;
        const bool hasPendingNpcHireOffer =
            pCurrentOffer != nullptr
            && pCurrentOffer->kind == DialogueOfferKind::NpcHire
            && pCurrentOffer->npcId == dialog.sourceId;
        const bool hasEventMessageLines = !eventMessageLines.empty();
        allowEmptyNpcTalkDialog =
            context.kind == DialogueContextKind::NpcTalk
            && allowNpcFallbackContent
            && (pNpc != nullptr || pGreeting != nullptr);

        if (context.kind == DialogueContextKind::NpcNews && dialog.sourceId == 0)
        {
            dialog.title = "News";
        }
        else if (pNpc != nullptr && !pNpc->name.empty())
        {
            dialog.title = pNpc->name;
        }
        else if (pGreeting != nullptr && !pGreeting->owner.empty())
        {
            dialog.title = pGreeting->owner;
        }

        if (context.kind == DialogueContextKind::NpcNews && context.participantPictureId != 0)
        {
            dialog.participantPictureId = context.participantPictureId;
        }
        else
        {
            dialog.participantPictureId = pNpc != nullptr ? pNpc->pictureId : 0;

            if (dialog.participantPictureId == 0 && context.participantPictureId != 0)
            {
                dialog.participantPictureId = context.participantPictureId;
            }
        }

        if (context.titleOverride && !context.titleOverride->empty())
        {
            dialog.title = *context.titleOverride;
        }

        if (context.kind == DialogueContextKind::NpcTalk
            && allowNpcFallbackContent
            && !hasPendingRosterJoinInvite
            && !hasPendingMasteryTeacherOffer
            && !hasPendingGuildMembershipOffer
            && !hasPendingNpcHireOffer
            && !hasEventMessageLines
            && ((hasGeneratedConversation && generatedConversation->greeting) || pGreeting != nullptr))
        {
            const bool generatedGreeting = hasGeneratedConversation && generatedConversation->greeting;
            const uint32_t greetingDisplayCount = npcRuntimeState.npcGreetingDisplayCounts[dialog.sourceId];
            const bool primaryGreeting = pGreeting != nullptr
                && (greetingDisplayCount == 0 || pGreeting->greetingSecondary.empty());
            const auto primaryOverride = npcRuntimeState.npcPrimaryGreetingTextOverrides.find(dialog.sourceId);
            const std::string &greetingText = generatedGreeting ? *generatedConversation->greeting
                : primaryGreeting
                    ? (primaryOverride != npcRuntimeState.npcPrimaryGreetingTextOverrides.end()
                        ? primaryOverride->second : pGreeting->greetingPrimary)
                    : pGreeting->greetingSecondary;

            if (!greetingText.empty())
            {
                const std::vector<std::string> wrappedGreeting = wrapDialogText(greetingText, MaxLineWidth);
                dialog.lines.insert(dialog.lines.end(), wrappedGreeting.begin(), wrappedGreeting.end());
                if (!generatedGreeting)
                {
                    eventRuntimeState.npcGreetingDisplayCounts[dialog.sourceId] = greetingDisplayCount + 1;
                    dialog.displayedNpcGreetingCount = greetingDisplayCount + 1;
                }
            }
        }

        if (context.kind == DialogueContextKind::NpcTalk
            && allowNpcFallbackContent
            && pNpcDialogTable != nullptr)
        {
            const std::optional<ScriptedEventProgram> emptyGlobalProgram = std::nullopt;
            const std::optional<ScriptedEventProgram> &globalProgram =
                pGlobalEventProgram != nullptr ? *pGlobalEventProgram : emptyGlobalProgram;
            const std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>>::const_iterator overrideIt =
                npcRuntimeState.npcTopicOverrides.find(dialog.sourceId);
            const std::unordered_map<uint32_t, uint32_t> *pTopicOverrides =
                overrideIt != npcRuntimeState.npcTopicOverrides.end() ? &overrideIt->second : nullptr;
            const std::vector<NpcDialogTable::ResolvedTopic> topics =
                pNpcDialogTable->getTopicsForNpc(dialog.sourceId, pTopicOverrides);
            const EventRuntime eventRuntime = {};

            if (hasPendingRosterJoinInvite)
            {
                EventDialogAction acceptAction = {};
                acceptAction.kind = EventDialogActionKind::RosterJoinAccept;
                acceptAction.label = pUiTextTable != nullptr
                    ? pUiTextTable->text(UiTextTable::Message::Yes) : "Yes";
                dialog.actions.push_back(std::move(acceptAction));

                EventDialogAction declineAction = {};
                declineAction.kind = EventDialogActionKind::RosterJoinDecline;
                declineAction.label = pUiTextTable != nullptr
                    ? pUiTextTable->text(UiTextTable::Message::No) : "No";
                dialog.actions.push_back(std::move(declineAction));
            }
            else if (hasPendingMasteryTeacherOffer && pClassSkillTable != nullptr && pParty != nullptr)
            {
                const std::optional<MasteryTeacherEvaluation> evaluation = evaluateMasteryTeacherTopic(
                    pCurrentOffer->topicId,
                    *pParty,
                    *pClassSkillTable,
                    *pNpcDialogTable,
                    pTeacherTopicTable
                );

                if (evaluation && !evaluation->displayText.empty())
                {
                    EventDialogAction learnAction = {};
                    learnAction.kind = EventDialogActionKind::MasteryTeacherLearn;
                    learnAction.id = pCurrentOffer->topicId;
                    learnAction.label = evaluation->displayText;
                    if (evaluation->approved && pUiTextTable != nullptr && pCharacterInspectTable != nullptr)
                    {
                        const SkillInspectEntry *pSkill = pCharacterInspectTable->getSkill(evaluation->skillName);
                        const std::string skillName = pSkill != nullptr && !pSkill->name.empty()
                            ? pSkill->name : displaySkillName(evaluation->skillName);
                        const std::string offerText = pUiTextTable->formatSkillTrainingOffer(
                            pUiTextTable->skillMasteryName(evaluation->targetMastery),
                            skillName,
                            evaluation->cost);
                        if (!offerText.empty())
                        {
                            learnAction.label = offerText;
                        }
                    }
                    else if (!evaluation->approved && evaluation->requiredBaseAttributeId && pUiTextTable != nullptr)
                    {
                        const char *pAttributeKey = nullptr;
                        switch (*evaluation->requiredBaseAttributeId)
                        {
                            case 0x21: pAttributeKey = "Intellect"; break;
                            case 0x22: pAttributeKey = "Personality"; break;
                            case 0x23: pAttributeKey = "Endurance"; break;
                            default: break;
                        }
                        if (pAttributeKey != nullptr)
                        {
                            const StatInspectEntry *pAttribute = pCharacterInspectTable != nullptr
                                ? pCharacterInspectTable->getStat(pAttributeKey) : nullptr;
                            const SkillInspectEntry *pSkill = pCharacterInspectTable != nullptr
                                ? pCharacterInspectTable->getSkill(evaluation->skillName) : nullptr;
                            const std::string requirementText = pUiTextTable->formatSkillTrainingRequirement(
                                pAttribute != nullptr && !pAttribute->name.empty() ? pAttribute->name : pAttributeKey,
                                evaluation->requiredBaseAttributeValue,
                                pUiTextTable->skillMasteryName(evaluation->targetMastery),
                                pSkill != nullptr && !pSkill->name.empty()
                                    ? pSkill->name : displaySkillName(evaluation->skillName));
                            if (!requirementText.empty())
                            {
                                learnAction.label = requirementText;
                            }
                        }
                    }
                    dialog.actions.push_back(std::move(learnAction));
                }
            }
            else if (hasPendingGuildMembershipOffer)
            {
                const std::optional<NpcDialogTable::GuildMembershipOffer> offer =
                    pNpcDialogTable->getGuildMembershipOfferForTopic(pCurrentOffer->topicId);

                if (offer)
                {
                    EventDialogAction joinAction = {};
                    joinAction.kind = EventDialogActionKind::GuildMembershipJoin;
                    joinAction.id = offer->topicId;

                    const std::optional<std::string> joinText = pNpcDialogTable->getText(offer->joinTextId);
                    joinAction.label = joinText && !joinText->empty()
                        ? *joinText
                        : "Join guild for " + std::to_string(offer->cost) + " gold";
                    dialog.actions.push_back(std::move(joinAction));
                }
            }
            else if (hasPendingNpcHireOffer)
            {
                EventDialogAction acceptAction = {};
                acceptAction.kind = EventDialogActionKind::NpcHireAccept;
                acceptAction.label = pUiTextTable != nullptr
                    ? pUiTextTable->text(UiTextTable::Message::Yes) : "Yes";
                dialog.actions.push_back(std::move(acceptAction));

                EventDialogAction declineAction = {};
                declineAction.kind = EventDialogActionKind::NpcHireDecline;
                declineAction.label = pUiTextTable != nullptr
                    ? pUiTextTable->text(UiTextTable::Message::No) : "No";
                dialog.actions.push_back(std::move(declineAction));
            }
            else
            {
                if (availableGeneratedMercenary)
                {
                    EventDialogAction joinAction = {};
                    joinAction.kind = EventDialogActionKind::GeneratedMercenaryJoinOffer;
                    joinAction.id = dialog.sourceId;
                    joinAction.label = pUiTextTable != nullptr
                        ? pUiTextTable->text(UiTextTable::Message::Join) : "Join";
                    dialog.actions.push_back(std::move(joinAction));
                }

                const MergedNpcProfessionEntry *pProfession =
                    pNpc != nullptr && pNpc->professionId != 0 && pNpcProfessionTable != nullptr
                        ? pNpcProfessionTable->get(pNpc->professionId)
                        : nullptr;
                const bool npcIsHired =
                    pNpc != nullptr && hasHiredNpcFollower(npcRuntimeState, pNpc->id);
                const bool allowProfessionBasedHire =
                    pNpc != nullptr && (context.hostHouseId == 0 || pNpc->topicIds.empty());
                const bool suppressProfessionTopicForHireableNpc =
                    pNpc != nullptr
                    && pProfession != nullptr
                    && npcCanOfferProfessionHire(*pNpc, *pProfession, allowProfessionBasedHire)
                    && !npcIsHired;

                for (const NpcDialogTable::ResolvedTopic &topic : topics)
                {
                    if (topic.topic.empty()
                        || (hasGeneratedConversation && generatedConversation->admissionReady
                            && !generatedConversation->willSpeak))
                    {
                        continue;
                    }

                    if (topic.specialKind == NpcTopicEntry::SpecialKind::RosterJoinOffer)
                    {
                        const auto offer = pNpcDialogTable->getRosterJoinOfferForTopic(topic.id);
                        if (reusableMercenaryNpc || hasGeneratedMercenaryCandidate
                            || (pParty != nullptr && offer
                                && isReusableMercenaryRosterId(*pParty, offer->rosterId)))
                        {
                            continue;
                        }
                    }

                    if (suppressProfessionTopicForHireableNpc
                        && (topic.id == pProfession->actionTopicId
                            || topic.id == pProfession->globalTextId))
                    {
                        continue;
                    }

                    const ISceneEventContext *pSceneEventContext =
                        dynamic_cast<const ISceneEventContext *>(pWorldRuntime);

                    if (!eventRuntime.canShowTopic(
                            globalProgram,
                            static_cast<uint16_t>(topic.id),
                            npcRuntimeState,
                            pParty,
                            pSceneEventContext))
                    {
                        continue;
                    }

                    EventDialogAction action = {};
                    const bool isTeacherTopic = isMasteryTeacherTopic(topic.id, pTeacherTopicTable);

                    action.kind = EventDialogActionKind::NpcTopic;
                    action.secondaryId = topic.id;
                    action.textOnly = !isTeacherTopic && topic.specialKind == NpcTopicEntry::SpecialKind::TextOnly;

                    if (topic.specialKind == NpcTopicEntry::SpecialKind::RosterJoinOffer)
                    {
                        action.kind = EventDialogActionKind::RosterJoinOffer;
                    }
                    else if (isTeacherTopic)
                    {
                        action.kind = EventDialogActionKind::MasteryTeacherOffer;
                    }
                    else if (topic.specialKind == NpcTopicEntry::SpecialKind::GuildMembershipOffer)
                    {
                        action.kind = EventDialogActionKind::GuildMembershipOffer;
                    }

                    action.id = topic.id;
                    action.label = topic.topic;
                    dialog.actions.push_back(std::move(action));
                }

                const bool canUseProfessionFallback =
                    dialog.actions.empty()
                    || npcIsHired
                    || (pNpc != nullptr
                        && pProfession != nullptr
                        && !npcIsHired
                        && npcCanOfferProfessionHire(*pNpc, *pProfession, allowProfessionBasedHire));
                bool suppressProfessionNews = false;

                if (canUseProfessionFallback && pNpc != nullptr && pProfession != nullptr)
                {
                    const bool npcCanJoin =
                        npcCanOfferProfessionHire(*pNpc, *pProfession, allowProfessionBasedHire);
                    const bool canUseNpcFollowers =
                        continentAllowsNpcFollowers(pCurrentMap, pContinentSettingTable);
                    const MergedNpcBtbEntry *pBtbEntry =
                        pNpcBtbTable != nullptr ? pNpcBtbTable->get(pProfession->personality) : nullptr;
                    const bool showBtbGate = pBtbEntry != nullptr
                        && randomNpcNeedsBtbGate(npcRuntimeState, pWorldRuntime, pNpc->id, context.sourceActorIndex);

                    if (showBtbGate)
                    {
                        appendNpcBtbAction(
                            dialog,
                            *pNpcDialogTable,
                            NpcBegTopicId,
                            pBtbEntry->begSuccessTextId,
                            pBtbEntry->begFailTextId,
                            pBtbEntry->acceptBeg);
                        appendNpcBtbAction(
                            dialog,
                            *pNpcDialogTable,
                            NpcThreatTopicId,
                            pBtbEntry->threatSuccessTextId,
                            pBtbEntry->threatFailTextId,
                            pBtbEntry->acceptThreat);
                        appendNpcBtbAction(
                            dialog,
                            *pNpcDialogTable,
                            NpcBribeTopicId,
                            pBtbEntry->bribeSuccessTextId,
                            pBtbEntry->bribeFailTextId,
                            pBtbEntry->acceptBribe,
                            pProfession->weeklyCost,
                            globalProgram ? &*globalProgram : nullptr);
                        suppressProfessionNews = true;
                    }
                    else if (!npcIsHired && npcCanJoin && canUseNpcFollowers)
                    {
                        EventDialogAction action = {};
                        action.kind = EventDialogActionKind::NpcHireOffer;
                        action.id = pNpc->professionId;
                        action.label = pUiTextTable != nullptr
                            ? pUiTextTable->text(UiTextTable::Message::Join) : "Join";
                        action.enabled = npcRuntimeState.hiredNpcFollowers.size() < MaxNpcFollowerCount;
                        action.disabledReason = action.enabled ? std::string() : "You already have enough followers.";
                        if (!action.enabled)
                        {
                            const std::optional<std::string> fullText = pNpcDialogTable->getText(1688);
                            if (fullText && !fullText->empty())
                            {
                                action.disabledReason = *fullText;
                            }
                        }
                        dialog.actions.push_back(std::move(action));

                        if (pProfession->descriptionTextId != 0)
                        {
                            EventDialogAction infoAction = {};
                            infoAction.kind = EventDialogActionKind::NpcProfessionDescription;
                            infoAction.id = pNpc->professionId;
                            infoAction.secondaryId = pProfession->descriptionTextId;
                            infoAction.label = pUiTextTable != nullptr
                                ? pUiTextTable->text(UiTextTable::Message::MoreInformation) : "More Info";
                            dialog.actions.push_back(std::move(infoAction));
                        }
                    }
                    else
                    {
                        if (pProfession->actionTopicId != 0)
                        {
                            const bool actionOnCooldown = npcProfessionActionTopicHasDailyCooldown(
                                pProfession->actionTopicId)
                                && npcProfessionActionUsedToday(npcRuntimeState, pNpc->id, currentGameMinutes);

                            const std::optional<NpcDialogTable::ResolvedTopic> professionTopic =
                                pNpcDialogTable->getTopicById(pProfession->actionTopicId);

                            if (!actionOnCooldown)
                            {
                                EventDialogAction action = {};
                                action.kind = EventDialogActionKind::NpcProfessionAction;
                                action.id = pProfession->actionTopicId;
                                action.label = professionTopic && !professionTopic->topic.empty()
                                    ? professionTopic->topic
                                    : (!pProfession->displayName().empty() ? pProfession->displayName() : "Profession");
                                dialog.actions.push_back(std::move(action));
                            }
                        }

                        if (pProfession->descriptionTextId != 0)
                        {
                            EventDialogAction action = {};
                            action.kind = EventDialogActionKind::NpcProfessionDescription;
                            action.id = pNpc->professionId;
                            action.secondaryId = pProfession->descriptionTextId;
                            action.label = pUiTextTable != nullptr
                                ? pUiTextTable->text(UiTextTable::Message::MoreInformation) : "More Info";
                            dialog.actions.push_back(std::move(action));
                        }

                        if (npcIsHired)
                        {
                            EventDialogAction action = {};
                            action.kind = EventDialogActionKind::NpcDismiss;
                            action.id = pNpc->professionId;
                            action.label = pUiTextTable != nullptr
                                ? pUiTextTable->formatNpcDismiss(pNpc->name) : "Dismiss";
                            dialog.actions.push_back(std::move(action));
                        }
                    }
                }

                if (canUseProfessionFallback
                    && !suppressProfessionNews
                    && !npcIsHired
                    && pNpc != nullptr
                    && pProfession != nullptr
                    && pNewsProfessionTopicTable != nullptr
                    && continentAllowsProfessionNews(pCurrentMap, pContinentSettingTable))
                {
                    const MergedNewsProfessionDayTopic *pProfessionTopic =
                        pNewsProfessionTopicTable->get(
                            pNpc->professionId,
                            weekDayIndexFromGameMinutes(currentGameMinutes));

                    if (pProfessionTopic != nullptr && pProfessionTopic->newsTextId != 0)
                    {
                        const std::optional<std::string> newsTopic =
                            pNpcDialogTable->getNewsTopic(pProfessionTopic->topicTextId);

                        EventDialogAction action = {};
                        action.kind = EventDialogActionKind::NpcProfessionNews;
                        action.id = pNpc->professionId;
                        action.secondaryId = pProfessionTopic->newsTextId;
                        action.label = newsTopic && !newsTopic->empty()
                            ? *newsTopic
                            : (!pProfession->displayName().empty() ? pProfession->displayName() : "Profession");
                        dialog.actions.push_back(std::move(action));
                    }
                }
            }
        }

        if (context.kind == DialogueContextKind::NpcNews
            && eventMessageLines.empty()
            && pNpcDialogTable != nullptr
            && context.newsId != 0)
        {
            const std::optional<std::string> newsText =
                pNpcDialogTable->getNewsDialogText(context.newsId, context.newsFromNpcText);

            if (newsText && !newsText->empty())
            {
                const std::vector<std::string> wrappedNews = wrapDialogText(*newsText, MaxLineWidth);
                dialog.lines.insert(dialog.lines.end(), wrappedNews.begin(), wrappedNews.end());
            }
        }
    }

    if (context.kind == DialogueContextKind::NpcTalk
        && pCurrentOffer != nullptr
        && pCurrentOffer->kind == DialogueOfferKind::MasteryTeacher
        && pCurrentOffer->npcId == dialog.sourceId
        && eventMessageLines.empty()
        && pNpcDialogTable != nullptr)
    {
        const std::optional<MasteryTeacherTopicDefinition> teacherTopic =
            resolveMasteryTeacherTopic(pCurrentOffer->topicId, pTeacherTopicTable);

        if (teacherTopic && teacherTopic->textId != 0)
        {
            const std::optional<std::string> text = pNpcDialogTable->getText(teacherTopic->textId);

            if (text && !text->empty())
            {
                eventMessageLines = wrapDialogText(*text, MaxLineWidth);
            }
        }
    }

    if (context.kind == DialogueContextKind::NpcTalk
        && pCurrentOffer != nullptr
        && pCurrentOffer->kind == DialogueOfferKind::RosterJoin
        && pCurrentOffer->npcId == dialog.sourceId
        && eventMessageLines.empty()
        && pNpcDialogTable != nullptr)
    {
        const std::optional<std::string> inviteText =
            pNpcDialogTable->getText(pCurrentOffer->messageTextId);

        if (inviteText && !inviteText->empty())
        {
            eventMessageLines = wrapDialogText(*inviteText, MaxLineWidth);
        }
    }

    if (context.kind == DialogueContextKind::NpcTalk
        && pCurrentOffer != nullptr
        && pCurrentOffer->kind == DialogueOfferKind::NpcHire
        && pCurrentOffer->npcId == dialog.sourceId
        && eventMessageLines.empty()
        && pNpcDialogTable != nullptr)
    {
        const std::optional<std::string> hireText =
            pNpcDialogTable->getText(pCurrentOffer->messageTextId);

        if (hireText && !hireText->empty())
        {
            eventMessageLines = wrapDialogText(*hireText, MaxLineWidth);
        }
    }

    if (context.kind == DialogueContextKind::MapTransition)
    {
    }
    else if (context.kind == DialogueContextKind::HouseService)
    {
        dialog.lines.clear();

        if (!eventMessageLines.empty())
        {
            dialog.lines.insert(dialog.lines.end(), eventMessageLines.begin(), eventMessageLines.end());
        }
        else if (!dialog.actions.empty())
        {
            const HouseEntry *pHouseEntry = pHouseTable != nullptr ? pHouseTable->get(dialog.sourceId) : nullptr;

            if (pHouseEntry != nullptr)
            {
                const DialogueMenuId menuId = currentHouseServiceMenuId(eventRuntimeState);
                dialog.lines = buildHouseServiceInfoLines(
                    *pHouseEntry,
                    pParty,
                    pClassSkillTable,
                    menuId,
                    pHouseServiceTextTable,
                    pUiTextTable);
            }
        }
    }
    else
    {
        if (dialog.isHouseDialog && eventMessageLines.empty() && !dialog.actions.empty())
        {
            dialog.lines.clear();
        }

        dialog.lines.insert(dialog.lines.end(), eventMessageLines.begin(), eventMessageLines.end());
    }

    if (!dialog.isHouseDialog
        && context.kind != DialogueContextKind::MapEvent
        && context.kind != DialogueContextKind::MapTransition
        && context.kind != DialogueContextKind::NpcNews
        && !allowNpcFallbackContent
        && eventMessageLines.empty())
    {
        return {};
    }

    if (dialog.lines.empty() && dialog.actions.empty())
    {
        if (context.kind == DialogueContextKind::MapEvent)
        {
            return {};
        }

        if (!dialog.isHouseDialog && !allowEmptyNpcTalkDialog)
        {
            dialog.lines.push_back("NPC interaction UI is not implemented yet.");
        }
    }

    return dialog;
}
}
