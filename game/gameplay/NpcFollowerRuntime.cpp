#include "game/gameplay/NpcFollowerRuntime.h"

#include "game/party/Party.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/NpcDialogTable.h"

#include <algorithm>
#include <utility>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint32_t SailorProfessionId = 8;
constexpr uint32_t NavigatorProfessionId = 9;
constexpr uint32_t HorsemanProfessionId = 35;
constexpr uint32_t ExplorerProfessionId = 44;
constexpr uint32_t PirateProfessionId = 45;

NpcEntry resolvedNpcEntry(
    const EventRuntimeState &eventRuntimeState,
    const NpcDialogTable &npcDialogTable,
    uint32_t npcId)
{
    NpcEntry npc = {};
    const NpcEntry *pBaseNpc = npcDialogTable.getNpc(npcId);

    if (pBaseNpc != nullptr)
    {
        npc = *pBaseNpc;
    }
    else
    {
        npc.id = npcId;
    }

    const std::unordered_map<uint32_t, std::string>::const_iterator nameIt =
        eventRuntimeState.npcNameOverrides.find(npcId);

    if (nameIt != eventRuntimeState.npcNameOverrides.end())
    {
        npc.name = nameIt->second;
    }

    const std::unordered_map<uint32_t, uint32_t>::const_iterator pictureIt =
        eventRuntimeState.npcPictureOverrides.find(npcId);

    if (pictureIt != eventRuntimeState.npcPictureOverrides.end())
    {
        npc.pictureId = pictureIt->second;
    }

    const std::unordered_map<uint32_t, uint32_t>::const_iterator professionIt =
        eventRuntimeState.npcProfessionOverrides.find(npcId);

    if (professionIt != eventRuntimeState.npcProfessionOverrides.end())
    {
        npc.professionId = professionIt->second;
    }

    return npc;
}

int professionTransportDayReduction(uint32_t professionId, bool stable)
{
    if (stable)
    {
        switch (professionId)
        {
            case HorsemanProfessionId:
                return 2;

            case ExplorerProfessionId:
                return 1;

            default:
                return 0;
        }
    }

    switch (professionId)
    {
        case SailorProfessionId:
        case PirateProfessionId:
            return 2;

        case NavigatorProfessionId:
            return 3;

        case ExplorerProfessionId:
            return 1;

        default:
            return 0;
    }
}

bool followerProfessionMatches(const EventRuntimeState &eventRuntimeState, const std::vector<uint32_t> &professionIds)
{
    for (uint32_t professionId : professionIds)
    {
        if (hiredNpcHasProfession(eventRuntimeState, professionId))
        {
            return true;
        }
    }

    return false;
}
}

std::vector<HiredNpcFollowerView> buildHiredNpcFollowerViews(
    const EventRuntimeState &eventRuntimeState,
    const NpcDialogTable &npcDialogTable,
    const MergedNpcProfessionTable &npcProfessionTable
)
{
    std::vector<HiredNpcFollowerView> views;
    views.reserve(eventRuntimeState.hiredNpcFollowers.size());

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        const NpcEntry npc = resolvedNpcEntry(eventRuntimeState, npcDialogTable, follower.npcId);
        const uint32_t professionId = follower.professionId != 0 ? follower.professionId : npc.professionId;
        const MergedNpcProfessionEntry *pProfession = npcProfessionTable.get(professionId);

        HiredNpcFollowerView view = {};
        view.npcId = follower.npcId;
        view.professionId = professionId;
        view.weeklyCost = follower.weeklyCost;
        view.feePercent = follower.weeklyCost / 100u;
        view.portraitPictureId = npc.pictureId;
        view.name = npc.name;
        view.profession = pProfession != nullptr ? pProfession->displayName() : std::string();
        views.push_back(std::move(view));
    }

    return views;
}

std::vector<HiredNpcFollowerView> buildHiredNpcFollowerViews(
    const EventRuntimeState &eventRuntimeState,
    const Party *pParty,
    const NpcDialogTable &npcDialogTable,
    const MergedNpcProfessionTable &npcProfessionTable
)
{
    if (pParty == nullptr)
    {
        return buildHiredNpcFollowerViews(eventRuntimeState, npcDialogTable, npcProfessionTable);
    }

    EventRuntimeState syncedRuntimeState = eventRuntimeState;
    pParty->applyGlobalNpcStateTo(syncedRuntimeState);
    return buildHiredNpcFollowerViews(syncedRuntimeState, npcDialogTable, npcProfessionTable);
}

uint32_t totalHiredNpcFollowerFeePercent(const EventRuntimeState &eventRuntimeState)
{
    uint32_t total = 0;

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        total += follower.weeklyCost / 100u;
    }

    return total;
}

uint32_t hiredNpcFollowerGoldShare(uint32_t goldAmount, const EventRuntimeState &eventRuntimeState)
{
    return goldAmount * totalHiredNpcFollowerFeePercent(eventRuntimeState) / 100u;
}

bool hiredNpcHasProfession(const EventRuntimeState &eventRuntimeState, uint32_t professionId)
{
    return std::find_if(
        eventRuntimeState.hiredNpcFollowers.begin(),
        eventRuntimeState.hiredNpcFollowers.end(),
        [professionId](const EventRuntimeState::HiredNpcFollower &follower)
        {
            return follower.professionId == professionId;
        }) != eventRuntimeState.hiredNpcFollowers.end();
}

int hiredNpcTransportDayReduction(const EventRuntimeState &eventRuntimeState, bool stable)
{
    int reduction = 0;

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        reduction += professionTransportDayReduction(follower.professionId, stable);
    }

    return reduction;
}

int hiredNpcCrossMapDayReduction(const EventRuntimeState &eventRuntimeState)
{
    int reduction = 0;

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        switch (follower.professionId)
        {
            case 5:
                reduction += 1;
                break;
            case 6:
                reduction += 2;
                break;
            case 7:
                reduction += 3;
                break;
            case ExplorerProfessionId:
                reduction += 1;
                break;
            default:
                break;
        }
    }

    return reduction;
}

int hiredNpcRestFoodReduction(const EventRuntimeState &eventRuntimeState)
{
    int reduction = 0;

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        if (follower.professionId == 29)
        {
            reduction += 1;
        }
        else if (follower.professionId == 30)
        {
            reduction += 2;
        }
    }

    return reduction;
}

int hiredNpcSkillBonus(const EventRuntimeState &eventRuntimeState, const std::string &skillName)
{
    return hiredNpcSkillBonus(eventRuntimeState.hiredNpcFollowers, skillName);
}

int hiredNpcSkillBonus(const std::vector<HiredNpcFollower> &followers, const std::string &skillName)
{
    int bonus = 0;

    for (const HiredNpcFollower &follower : followers)
    {
        const uint32_t professionId = follower.professionId;

        if (skillName == "Learning")
        {
            if (professionId == 4) bonus += 5;
            else if (professionId == 13) bonus += 10;
            else if (professionId == 14) bonus += 15;
        }
        else if (skillName == "Merchant")
        {
            if (professionId == 20) bonus += 4;
            else if (professionId == 21) bonus += 6;
        }
        else if (skillName == "DisarmTraps")
        {
            if (professionId == 25) bonus += 4;
            else if (professionId == 26) bonus += 6;
        }
        else if (skillName == "Perception")
        {
            if (professionId == 22) bonus += 6;
            else if (professionId == 47) bonus += 5;
        }
        else if (skillName == "LeatherArmor" || skillName == "ChainArmor" || skillName == "PlateArmor")
        {
            if (professionId == 46) bonus += 2;
        }
        else if (skillName == "Staff" || skillName == "Sword" || skillName == "Dagger" || skillName == "Axe"
            || skillName == "Spear" || skillName == "Bow" || skillName == "Mace")
        {
            if (professionId == 15) bonus += 2;
            else if (professionId == 16) bonus += 3;
            else if (professionId == 46) bonus += 2;
        }
        else if (skillName == "FireMagic" || skillName == "AirMagic" || skillName == "WaterMagic"
            || skillName == "EarthMagic" || skillName == "SpiritMagic" || skillName == "MindMagic"
            || skillName == "BodyMagic" || skillName == "LightMagic" || skillName == "DarkMagic")
        {
            if (professionId == 17) bonus += 2;
            else if (professionId == 18) bonus += 3;
            else if (professionId == 19) bonus += 4;
        }
    }

    return bonus;
}

int hiredNpcPrimaryStatBonus(const EventRuntimeState &eventRuntimeState, const std::string &statName)
{
    if (statName != "Luck")
    {
        return 0;
    }

    int bonus = 0;

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        if (follower.professionId == 27)
        {
            bonus += 10;
        }
        else if (follower.professionId == 28)
        {
            bonus += 20;
        }
    }

    return bonus;
}

int hiredNpcResistanceBonus(const EventRuntimeState &eventRuntimeState, const std::string &resistanceName)
{
    if (resistanceName != "Fire" && resistanceName != "Air" && resistanceName != "Water" && resistanceName != "Earth")
    {
        return 0;
    }

    return hiredNpcHasProfession(eventRuntimeState, 37) ? 20 : 0;
}

uint32_t hiredNpcGoldFindBonusPercent(const EventRuntimeState &eventRuntimeState)
{
    uint32_t percent = 0;

    for (const EventRuntimeState::HiredNpcFollower &follower : eventRuntimeState.hiredNpcFollowers)
    {
        if (follower.professionId == 31 || follower.professionId == 45)
        {
            percent += 10;
        }
        else if (follower.professionId == 32)
        {
            percent += 20;
        }
    }

    return percent;
}

uint32_t hiredNpcGoldAfterBonusAndFees(uint32_t goldAmount, const EventRuntimeState &eventRuntimeState)
{
    const uint32_t withBonus = goldAmount + goldAmount * hiredNpcGoldFindBonusPercent(eventRuntimeState) / 100u;
    const uint32_t fee = hiredNpcFollowerGoldShare(withBonus, eventRuntimeState);
    return withBonus > fee ? withBonus - fee : 0;
}

bool hiredNpcCanRepairItemKind(const EventRuntimeState &eventRuntimeState, const std::string &equipStat)
{
    if (equipStat == "Armor" || equipStat == "Shield" || equipStat == "Helm" || equipStat == "Belt"
        || equipStat == "Cloak" || equipStat == "Gauntlets" || equipStat == "Boots")
    {
        return hiredNpcHasProfession(eventRuntimeState, 2);
    }

    if (equipStat == "Ring" || equipStat == "Amulet" || equipStat == "WeaponW")
    {
        return hiredNpcHasProfession(eventRuntimeState, 3);
    }

    return followerProfessionMatches(eventRuntimeState, {1});
}

bool hiredNpcCanIdentifyItemKind(const EventRuntimeState &eventRuntimeState, const std::string &equipStat)
{
    if (equipStat == "Helm" || equipStat == "Belt" || equipStat == "Cloak" || equipStat == "Gauntlets"
        || equipStat == "Boots" || equipStat == "Ring" || equipStat == "Amulet" || equipStat == "WeaponW")
    {
        return hiredNpcHasProfession(eventRuntimeState, 4);
    }

    return false;
}
}
