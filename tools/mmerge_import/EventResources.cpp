#include "EventResources.h"
#include "BlasterLearningResources.h"
#include "BreachAlvarCompanionResources.h"
#include "BreachMercenaryResources.h"
#include "MercenaryReleaseResources.h"
#include "ControlCubeRefillResources.h"
#include "CrossContinentsResources.h"
#include "DimensionDoorResources.h"
#include "GloamingCastleResources.h"
#include "HarmondaleCastleResources.h"
#include "LambentCastleResources.h"
#include "StoneCityResources.h"
#include "ArtifactMessengerResources.h"
#include "EmeraldIslandTravelResources.h"
#include "FreeHavenTempleResources.h"
#include "Mm6QuestContinuityResources.h"
#include "RegnaCannonResources.h"
#include "Mm6LostShieldResources.h"
#include "PostQuestVisitResources.h"
#include "Mm8RemainingMapResources.h"
#include "Mm6RemainingMapResources.h"
#include "Mm7RemainingMapResources.h"
#include "TownPortalVisitResources.h"
#include "SubmarineTravelResources.h"
#include "StoneStatueResources.h"
#include "Mm8ReentryResources.h"
#include "TempleBottleResources.h"
#include "LichPromotionResources.h"
#include "Mm7EndingResources.h"
#include "ArchibaldResources.h"
#include "SkeletonTransformerResources.h"
#include "SciFiResources.h"
#include "SmugglersCoveResources.h"
#include "Mm6HiveResources.h"
#include "Mm7ShoalsResources.h"
#include "Mm8EndgameResources.h"
#include "QuestItemCapacityResources.h"
#include "RunawayChaosResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "EventSupportSource.h"
#include "engine/TextTable.h"
#include "game/events/ScriptedEventProgram.h"
#include "tools/LegacyLuaExport.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OpenYAMM::Import
{
namespace
{
using Rows = std::vector<std::vector<std::string>>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool sameTimerRegistrations(const std::vector<Game::ScriptedEventProgram::TimerTrigger> &left,
    const std::vector<Game::ScriptedEventProgram::TimerTrigger> &right)
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
        [](const Game::ScriptedEventProgram::TimerTrigger &first,
            const Game::ScriptedEventProgram::TimerTrigger &second)
        {
            return Game::sameScriptedEventTimerIdentity(first, second)
                && first.eventId == second.eventId && first.triggerStep == second.triggerStep
                && first.triggerKind == second.triggerKind && first.scheduleKind == second.scheduleKind
                && first.repeating == second.repeating
                && first.temporaryMapLifecycle == second.temporaryMapLifecycle
                && first.intervalHalfMinutes == second.intervalHalfMinutes
                && first.startHour == second.startHour && first.startMinute == second.startMinute
                && first.startSecond == second.startSecond
                && first.intervalGameMinutes == second.intervalGameMinutes
                && first.initialDelayGameMinutes == second.initialDelayGameMinutes;
        });
}

std::string lower(std::string text)
{
    for (char &character : text)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character += 'a' - 'A';
        }
    }
    return text;
}

bool safeName(const std::string &name)
{
    return !name.empty() && name != "." && name != ".."
        && name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-") == std::string::npos;
}

bool isOutside(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const std::filesystem::path relative = path.lexically_relative(root);
    return !relative.empty() && (relative.is_absolute() || *relative.begin() == "..");
}

std::optional<uint32_t> rowId(const std::vector<std::string> &row)
{
    if (row.empty())
    {
        return std::nullopt;
    }
    uint32_t id = 0;
    const auto parsed = std::from_chars(row[0].data(), row[0].data() + row[0].size(), id);
    return parsed.ec == std::errc() && parsed.ptr == row[0].data() + row[0].size()
        ? std::optional<uint32_t>(id) : std::nullopt;
}

Rows tableRows(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Missing converted lookup table: " + path.string());
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid converted lookup table: " + path.string());
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        if (rowId(row))
        {
            rows.push_back(row);
        }
    }
    require(!rows.empty(), "Empty converted lookup table: " + path.string());
    return rows;
}

std::unordered_map<uint32_t, std::string> textLookup(const std::filesystem::path &path, size_t column)
{
    std::unordered_map<uint32_t, std::string> result;
    for (const std::vector<std::string> &row : tableRows(path))
    {
        require(row.size() > column, "Incomplete lookup row in " + path.string());
        require(result.emplace(*rowId(row), row[column]).second, "Duplicate lookup ID in " + path.string());
    }
    return result;
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 3> GeneratedNpcSources = {{
    {"Scripts/General/NPCNewsTopics.lua", "9531bafcbc2482c153bef05dfd8ff49ff3d46d5e9cadaa22634bc1089582b258"},
    {"Scripts/General/NPCFollowers.lua", "d1798480623d05d954b6b82de04761b8e54d2dfbce237d2a150549368810c107"},
    {"Scripts/Global/Reputation.lua", "21630dbcb976fc9619979336748ec0477cf3b9ab0a1dda250e83d1010ac3416e"}
}};

std::string npcSourceSha256(const std::vector<uint8_t> &bytes)
{
    require(!bytes.empty() && bytes.size() <= 16 * 1024 * 1024, "Invalid generated NPC source size");
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), hash.data());
    std::ostringstream result;
    for (unsigned char byte : hash)
    {
        result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    }
    return result.str();
}

std::filesystem::path checkedNpcSourcePath(const std::filesystem::path &root, std::string_view relative)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relative);
    const std::filesystem::path local = path.lexically_relative(root);
    require(!local.empty() && !local.is_absolute() && *local.begin() != ".."
        && std::filesystem::is_regular_file(path), "Generated NPC dependency escapes source root");
    return path;
}

struct GeneratedNpcTexts
{
    std::string luaSource;
    std::string archive;
    std::string globalTextSha256;
};

std::optional<GeneratedNpcTexts> loadGeneratedNpcTexts(const std::filesystem::path &root)
{
    if (!std::filesystem::exists(root / GeneratedNpcSources[0].first))
    {
        return std::nullopt;
    }
    for (const auto &[path, expected] : GeneratedNpcSources)
    {
        const std::filesystem::path source = checkedNpcSourcePath(root, path);
        const uintmax_t size = std::filesystem::file_size(source);
        require(size > 0 && size <= 16 * 1024 * 1024, "Invalid generated NPC script size");
        std::ifstream input(source, std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        require(npcSourceSha256(bytes) == expected, "Unsupported generated NPC script: " + std::string(path));
    }
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    GeneratedNpcTexts result;
    result.archive = chinese ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod";
    const std::vector<uint8_t> bytes = readLodText(checkedNpcSourcePath(root, result.archive), "global.txt");
    result.globalTextSha256 = npcSourceSha256(bytes);
    size_t glyphs = 0;
    const std::optional<Engine::TextTable> table =
        Engine::TextTable::parseTabSeparated(decodeLegacyText(bytes, chinese, glyphs));
    require(table && table->getRowCount() > 2
        && table->getRow(0) == std::vector<std::string>{"Global Text", ""}, "Invalid generated NPC GlobalTxt");
    const std::set<uint32_t> required = {97, 155, 379, 383, 384, 388, 389, 392, 395, 396, 397, 399, 402, 434};
    std::map<uint32_t, std::string> texts;
    for (size_t index = 1; index < table->getRowCount(); ++index)
    {
        const std::vector<std::string> &row = table->getRow(index);
        const std::optional<uint32_t> id = rowId(row);
        if (!id || !required.contains(*id))
        {
            continue;
        }
        require(row.size() >= 2 && !row[1].empty() && texts.emplace(*id, row[1]).second,
            "Missing or duplicate generated NPC GlobalTxt");
    }
    require(texts.size() == required.size(), "Incomplete generated NPC GlobalTxt dependencies");
    std::ostringstream lua;
    lua << "\n-- NPCNewsTopics/NPCFollowers/Reputation native greeting text dependencies, active source language.\n"
        << "evt.importedGlobalText = evt.importedGlobalText or {}\n";
    for (const auto &[id, text] : texts)
    {
        lua << "evt.importedGlobalText[" << id << "] = \"";
        for (unsigned char byte : text)
        {
            if (byte == '"' || byte == '\\')
            {
                lua << '\\' << char(byte);
            }
            else if (byte < 32 || byte == 127)
            {
                lua << '\\' << std::setw(3) << std::setfill('0') << uint32_t(byte);
            }
            else
            {
                lua << char(byte);
            }
        }
        lua << "\"\n";
    }
    result.luaSource = lua.str();
    return result;
}

struct ArchiveMember
{
    std::filesystem::path archive;
    bool chinese = false;
};

using Members = std::map<std::string, ArchiveMember>;

Game::StrTable decodeStrings(const Members &members, const std::string &name, size_t &glyphs)
{
    Game::StrTable strings;
    const auto found = members.find(name);
    if (found == members.end())
    {
        return strings;
    }
    const std::vector<uint8_t> bytes = readLodText(found->second.archive, name);
    Game::StrTable encoded;
    require(encoded.loadFromNullSeparatedBytes(bytes), "Unterminated source STR: " + name);
    std::vector<uint8_t> decoded;
    for (const std::string &entry : encoded.getEntries())
    {
        const std::string text = decodeLegacyText({entry.begin(), entry.end()}, found->second.chinese, glyphs);
        decoded.insert(decoded.end(), text.begin(), text.end());
        decoded.push_back(0);
    }
    require(strings.loadFromNullSeparatedBytes(decoded), "Invalid decoded STR: " + name);
    return strings;
}

struct MapSource
{
    std::string world;
    std::string file;
    std::string name;
};

std::optional<uint16_t> dimensionDoorReplacementEventId(const std::string &mapStem)
{
    if (mapStem == "out07")
    {
        return 500;
    }
    if (mapStem == "outb3")
    {
        return 105;
    }
    if (mapStem == "7out04")
    {
        return 504;
    }
    if (mapStem == "out09")
    {
        return 6;
    }
    if (mapStem == "oute3")
    {
        return 140;
    }
    return std::nullopt;
}

void validateInstruction(const Game::EvtInstruction &instruction, const std::string &context)
{
    const uint8_t opcode = static_cast<uint8_t>(instruction.opcode);
    require(opcode >= 1 && opcode <= 68,
        "Unimplemented opcode " + std::to_string(opcode) + " in " + context);
    // MM8 field widths, including terminators for the four inline-string commands.
    // MMerge's MM6/MM7 EVT archives already use this format; their IDs must not be remapped again.
    constexpr std::array<size_t, 69> minimumPayload = {
        0, 1, 4, 12, 1, 1, 28, 1, 2, 6, 2, 5, 3, 6, 7, 2,
        6, 6, 6, 23, 5, 27, 4, 9, 9, 6, 13, 2, 1, 4, 4, 10,
        5, 1, 22, 1, 1, 1, 10, 9, 8, 6, 4, 7, 7, 1, 1, 8,
        8, 9, 8, 8, 8, 1, 8, 8, 2, 9, 9, 2, 9, 8, 3, 4,
        7, 6, 8, 9, 5,
    };
    const std::vector<uint8_t> &payload = instruction.rawPayload;
    require(payload.size() >= minimumPayload[opcode],
        "Truncated opcode payload " + std::to_string(opcode) + " in " + context);
    if (opcode == 6 || opcode == 11 || opcode == 12 || opcode == 13)
    {
        const size_t stringOffset = minimumPayload[opcode] - 1;
        require(std::find(payload.begin() + stringOffset, payload.end(), 0) != payload.end(),
            "Unterminated opcode string payload in " + context);
    }
}
}

void convertEventResources(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::string &mapStem)
{
    convertEventResourcesForMaps(source, convertedBase, destination,
        mapStem.empty() ? std::vector<std::string>{} : std::vector<std::string>{mapStem});
}

void convertEventResourcesForMaps(const std::filesystem::path &source, const std::filesystem::path &convertedBase,
    const std::filesystem::path &destination, const std::vector<std::string> &mapStems,
    const ImportCancelCheck &cancelled)
{
    checkImportCancellation(cancelled);
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path base = std::filesystem::canonical(convertedBase);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    require(!std::filesystem::exists(output), "Event destination already exists");
    require(isOutside(output, root) && isOutside(output, base), "Event output must be outside both input roots");
    std::set<std::string> requested;
    for (const std::string &stem : mapStems)
    {
        require(!stem.empty() && safeName(lower(stem)) && requested.insert(lower(stem)).second,
            "Invalid or duplicate requested map stem");
    }
    std::set<std::string> remaining = requested;

    Members members;
    Members baseMembers;
    for (const char *pName : {"mm6.T.lod", "mm7.T.lod", "mmmerge.T.lod", "10 LocZHCN.EnglishT.lod"})
    {
        const std::filesystem::path archive = root / "Data" / pName;
        if (!std::filesystem::exists(archive))
        {
            continue;
        }
        const bool chinese = std::string_view(pName) == "10 LocZHCN.EnglishT.lod";
        for (const std::string &member : listLodMembers(archive))
        {
            const std::string name = lower(member);
            if (name.ends_with(".evt") || name.ends_with(".str"))
            {
                require(safeName(name), "Unsafe event member name");
                require(!chinese || !name.ends_with(".evt"), "Language archive must not replace event mechanics");
                members[name] = {archive, chinese};
                if (!chinese)
                {
                    baseMembers[name] = {archive, false};
                }
            }
        }
    }
    require(members.contains("global.evt"), "Missing merged Global.evt");

    const std::filesystem::path tables = base / "engine/data_tables";
    Game::LegacyLuaExportLookups lookups;
    lookups.houseNames = textLookup(tables / "house_data.txt", 5);
    lookups.npcTexts = textLookup(tables / "npc_topic_text.txt", 1);
    lookups.npcNames = textLookup(tables / "npc.txt", 1);
    lookups.npcTopicNames = textLookup(tables / "npc_topic.txt", 1);
    lookups.questNotes = textLookup(tables / "english/quests.txt", 1);
    const std::optional<GeneratedNpcTexts> generatedNpcTexts = loadGeneratedNpcTexts(root);

    const std::optional<BlasterLearningProfile> blasterLearning = loadBlasterLearningProfile(
        root, std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod"));
    if (blasterLearning)
    {
        require(lookups.npcTopicNames.contains(950), "Missing original Blaster learning topic 950");
        lookups.npcTopicNames[950] = blasterLearning->topic;
    }

    const std::optional<LichPromotionProfile> lichPromotion = loadLichPromotionProfile(root);
    if (lichPromotion)
    {
        for (uint32_t textId : {114u, 115u, 116u, 925u, 1146u, 1147u, 1148u, 1149u, 1150u, 1151u, 2698u})
        {
            require(lookups.npcTexts.contains(textId) && !lookups.npcTexts.at(textId).empty(),
                "Missing original Lich promotion NPC text " + std::to_string(textId));
        }
    }

    const std::optional<Mm7EndingProfile> mm7Ending = loadMm7EndingProfile(root);
    const std::optional<ArchibaldProfile> archibald = loadArchibaldProfile(root);
    const auto breachAlvar = loadBreachAlvarCompanionResources(root);
    const std::optional<BreachMercenaryResources> breachMercenaries = loadBreachMercenaryResources(
        root, std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod"));
    const std::optional<MercenaryReleaseResources> mercenaryRelease = loadMercenaryReleaseResources(
        root, std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod"));
    if (breachMercenaries)
    {
        require(lookups.npcTopicNames.contains(breachMercenaries->profile.joinTopicId)
            && !lookups.npcTopicNames.at(breachMercenaries->profile.joinTopicId).empty(),
            "Missing Breach mercenary source Join topic");
        require(lookups.npcTexts.contains(breachMercenaries->profile.partyFullNpcTextId)
            && !lookups.npcTexts.at(breachMercenaries->profile.partyFullNpcTextId).empty(),
            "Missing Breach mercenary source full-party text");
    }
    const std::optional<CrossContinentsIntroProfile> crossContinents = loadCrossContinentsIntroProfile(root);
    const std::optional<RunawayChaosProfile> runawayChaos = loadRunawayChaosProfile(
        root, lookups.npcNames, lookups.npcTopicNames, lookups.npcTexts);
    if (crossContinents)
    {
        require(lookups.npcNames.contains(803) && !lookups.npcNames.at(803).empty(),
            "Cross-continent introduction requires NPC 803");
        const std::unordered_map<uint32_t, std::string> greetings = textLookup(tables / "npc_greet.txt", 1);
        require(greetings.contains(328) && !greetings.at(328).empty(),
            "Missing cross-continent introduction completion greeting 328");
        require(greetings.contains(329) && !greetings.at(329).empty(),
            "Missing cross-continent invitation greeting 329");
        for (uint32_t id : {1778u, 1779u, 1780u})
        {
            require(lookups.npcTopicNames.contains(id) && !lookups.npcTopicNames.at(id).empty(),
                "Missing cross-continent introduction topic " + std::to_string(id));
        }
        for (uint32_t id : {2149u, 2150u, 2151u, 2152u})
        {
            require(lookups.npcTexts.contains(id) && !lookups.npcTexts.at(id).empty(),
                "Missing cross-continent introduction text " + std::to_string(id));
        }
    }
    const std::optional<DimensionDoorProfile> dimensionDoor = loadDimensionDoorProfile(root);
    const std::optional<HarmondaleCastleProfile> harmondaleCastle = loadHarmondaleCastleProfile(root);
    const std::optional<LambentCastleProfile> lambentCastle = loadLambentCastleProfile(root);
    const std::optional<StoneCityProfile> stoneCity = loadStoneCityProfile(root);
    const std::optional<EmeraldIslandTravelProfile> emeraldTravel = loadEmeraldIslandTravelProfile(root);
    const std::optional<FreeHavenTempleProfile> freeHavenTemple = loadFreeHavenTempleProfile(root);
    const std::optional<FreeHavenCouncilProfile> freeHavenCouncil = loadFreeHavenCouncilProfile(root);
    const std::optional<Mm6QuestContinuityProfile> mm6QuestContinuity = loadMm6QuestContinuityProfile(root);
    const std::optional<RegnaCannonProfile> regnaCannon = loadRegnaCannonProfile(root);
    const auto lostShield = loadMm6LostShieldProfile(root);
    const auto postQuestVisits = loadPostQuestVisitProfiles(root);
    const auto mm8Remaining = loadMm8RemainingMapProfiles(root);
    const auto mm6Remaining = loadMm6RemainingMapProfiles(root);
    const auto mm6RemainingGlobal = loadMm6RemainingGlobalProfile(root);
    const auto mm7Remaining = loadMm7RemainingMapProfiles(root,
        std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod"));
    const auto mm7RemainingGlobal = loadMm7RemainingGlobalProfiles(root);
    if (lostShield)
    {
        const Rows items = tableRows(tables / "items.txt");
        const auto shield = std::find_if(items.begin(), items.end(), [](const auto &row)
            { return rowId(row) == 2119; });
        require(shield != items.end() && shield->size() > 3 && (*shield)[3] == "0",
            "Lost shield projection requires source zero-value item2119");
    }
    const std::optional<ArtifactMessengerProfile> artifactMessenger = loadArtifactMessengerProfile(root);
    const std::vector<TownPortalVisitProfile> townPortalVisits = loadTownPortalVisitProfiles(root);
    const std::vector<SubmarineTravelProfile> submarineTravel = loadSubmarineTravelProfiles(root);
    const std::optional<StoneStatueProfile> stoneStatue = loadStoneStatueProfile(root);
    const std::vector<Mm8ReentryProfile> reentries = loadMm8ReentryProfiles(root);
    const std::optional<TempleBottleProfile> templeBottle = loadTempleBottleProfile(root);
    const std::optional<GloamingCastleProfile> gloamingCastle = loadGloamingCastleProfile(root);
    const std::optional<SkeletonTransformerProfile> skeletonTransformer = loadSkeletonTransformerProfile(root);
    const std::optional<SciFiProfile> sciFi = loadSciFiProfile(root);
    const std::optional<ControlCubeRefillProfile> controlCubeRefill = loadControlCubeRefillProfile(root);
    const std::optional<SmugglersCoveProfile> smugglersCove = loadSmugglersCoveProfile(root);
    const std::optional<Mm6HiveProfile> mm6Hive = loadMm6HiveProfile(root);
    const std::optional<Mm7ShoalsProfile> mm7Shoals = loadMm7ShoalsProfile(root);
    const std::optional<Mm8EndgameProfile> mm8Endgame = loadMm8EndgameProfile(root);
    std::optional<std::string> questItemCapacity;
    if (mm8Endgame)
    {
        require(members.contains("global.evt")
            && std::filesystem::equivalent(members.at("global.evt").archive, root / mm8Endgame->globalEventArchive),
            "MM8 answer binding and event importer selected different Global.evt sources");
        require(members.contains("d35.str")
            && std::filesystem::equivalent(members.at("d35.str").archive, root / mm8Endgame->answerArchive),
            "MM8 answer binding and event importer selected different Escaton strings");
        lookups.inputStringAnswerBindings = mm8Endgame->inputStringAnswerBindings;
        questItemCapacity = buildQuestItemCapacityOverlay(
            readLodText(members.at("global.evt").archive, "global.evt"), mm8Endgame->chineseAnswers);
    }

    std::vector<MapSource> maps = {{"", "global", ""}};
    for (const std::vector<std::string> &row : tableRows(tables / "map_stats.txt"))
    {
        require(row.size() >= 35, "Converted map registry must include explicit world IDs");
        const std::string file = lower(row[2]);
        const std::string world = lower(row[34]);
        require(safeName(file) && safeName(world), "Unsafe map registry identity");
        lookups.mapNamesByFile[file] = row[1];
        const std::string stem = std::filesystem::path(file).stem().string();
        lookups.mapNamesByFile[stem] = row[1];
        if (requested.empty() || requested.contains(stem))
        {
            require(members.contains(stem + ".evt"), "Missing map EVT: " + file);
            if (controlCubeRefill && world == "mm7" && stem == controlCubeRefill->mapStem)
            {
                require(row[6] == std::to_string(controlCubeRefill->refillIntervalDays),
                    "Control cube refill requires the original 672-day map interval");
            }
            maps.push_back({world, file, row[1]});
            remaining.erase(stem);
        }
    }
    require(maps.size() > 1, "Requested map is absent from the converted registry");
    require(remaining.empty(), "One or more requested maps are absent from the converted registry");

    std::map<std::string, const DimensionDoorMapOverlay *> dimensionDoorOverlays;
    if (dimensionDoor)
    {
        for (const DimensionDoorMapOverlay &overlay : dimensionDoor->maps)
        {
            dimensionDoorOverlays.emplace(overlay.mapStem, &overlay);
        }
    }

    std::vector<std::pair<const MapSource *, const DimensionDoorMapOverlay *>> selectedDimensionDoorOverlays;
    std::map<std::string, const Mm7ShoalsMapOverlay *> selectedShoalsOverlays;
    if (mm7Shoals)
    {
        for (const Mm7ShoalsMapOverlay &overlay : mm7Shoals->maps)
        {
            if (std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
                { return map.world == "mm7" && std::filesystem::path(map.file).stem() == overlay.mapStem; }))
            {
                selectedShoalsOverlays.emplace(overlay.mapStem, &overlay);
            }
        }
    }
    for (const MapSource &map : maps)
    {
        const std::string stem = std::filesystem::path(map.file).stem().string();
        const auto overlay = dimensionDoorOverlays.find(stem);
        if (!map.world.empty() && overlay != dimensionDoorOverlays.end())
        {
            selectedDimensionDoorOverlays.emplace_back(&map, overlay->second);
        }
    }
    const auto selectedHarmondaleCastle = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return harmondaleCastle && map.world == "mm7"
            && std::filesystem::path(map.file).stem().string() == harmondaleCastle->mapStem;
    });

    const auto selectedStoneCity = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return stoneCity && map.world == "mm7" && std::filesystem::path(map.file).stem() == stoneCity->mapStem;
    });

    const auto selectedEmeraldTravel = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return emeraldTravel && map.world == "mm7"
            && std::filesystem::path(map.file).stem() == emeraldTravel->mapStem;
    });
    const auto selectedFreeHavenTemple = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return freeHavenTemple && map.world == "mm6"
            && std::filesystem::path(map.file).stem() == freeHavenTemple->mapStem;
    });
    const bool selectedSewerContinuity = mm6QuestContinuity
        && std::any_of(maps.begin(), maps.end(), [](const MapSource &map)
            { return map.world == "mm6" && std::filesystem::path(map.file).stem() == "sewer"; });
    const bool selectedKeyChestContinuity = mm6QuestContinuity
        && std::any_of(maps.begin(), maps.end(), [](const MapSource &map)
            { return map.world == "mm6" && std::filesystem::path(map.file).stem() == "6d07"; });
    const bool selectedRegnaCannon = regnaCannon
        && std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
            { return map.world == "mm8" && std::filesystem::path(map.file).stem() == regnaCannon->mapStem; });

    std::map<std::string, const TownPortalVisitProfile *> selectedTownPortalVisits;
    for (const TownPortalVisitProfile &profile : townPortalVisits)
    {
        if (std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
            { return map.world == "mm6" && std::filesystem::path(map.file).stem() == profile.mapStem; }))
        {
            selectedTownPortalVisits.emplace(profile.mapStem, &profile);
        }
    }
    std::map<std::string, const SubmarineTravelProfile *> selectedSubmarineTravel;
    for (const SubmarineTravelProfile &profile : submarineTravel)
    {
        if (std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
            { return map.world == "mm8" && std::filesystem::path(map.file).stem() == profile.mapStem; }))
        {
            selectedSubmarineTravel.emplace(profile.mapStem, &profile);
        }
    }
    const auto selectedStoneStatue = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return stoneStatue && map.world == "mm8" && std::filesystem::path(map.file).stem() == stoneStatue->mapStem;
    });

    std::map<std::string, const Mm8ReentryProfile *> selectedReentries;
    for (const Mm8ReentryProfile &profile : reentries)
    {
        if (std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
            { return map.world == "mm8" && std::filesystem::path(map.file).stem() == profile.mapStem; }))
        {
            selectedReentries.emplace(profile.mapStem, &profile);
        }
    }
    const bool selectedTempleBottle = templeBottle && std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
        { return map.world == "mm7" && std::filesystem::path(map.file).stem() == templeBottle->mapStem; });

    const auto selectedLambentCastle = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return lambentCastle && map.world == "mm7"
            && std::filesystem::path(map.file).stem().string() == lambentCastle->mapStem;
    });

    const auto selectedGloamingCastle = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return gloamingCastle && map.world == "mm7" && std::filesystem::path(map.file).stem() == "d03";
    });

    const auto selectedSkeletonTransformer = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return skeletonTransformer && map.world == "mm8" && std::filesystem::path(map.file).stem() == "d19";
    });

    const auto selectedSmugglersCove = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return smugglersCove && map.world == "mm8"
            && std::filesystem::path(map.file).stem().string() == smugglersCove->mapStem;
    });

    const auto selectedSciFi = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return sciFi && map.world == "mm6" && std::filesystem::path(map.file).stem() == sciFi->mapStem;
    });

    const auto selectedControlCubeRefill = std::find_if(maps.begin(), maps.end(), [&](const MapSource &map)
    {
        return controlCubeRefill && map.world == "mm7"
            && std::filesystem::path(map.file).stem() == controlCubeRefill->mapStem;
    });

    std::filesystem::create_directories(output);
    try
    {
        writeConvertedResource(output / "engine/scripts/common/event_support.lua",
            {EventSupportSource.begin(), EventSupportSource.end()});
        if (breachAlvar)
        {
            require(runawayChaos.has_value(), "Breach Alvar companions require the source Runaway Chaos overlay");
            const std::string profile = Game::serializeBreachAlvarCompanionProfile(breachAlvar->profile);
            writeConvertedResource(output / "engine/breach_alvar_companions.txt", {profile.begin(), profile.end()});
        }
        if (breachMercenaries)
        {
            const std::string profile = Game::serializeBreachMercenaryProfile(breachMercenaries->profile);
            writeConvertedResource(output / "engine/breach_mercenaries.txt", {profile.begin(), profile.end()});
        }
        if (mercenaryRelease)
        {
            const std::string profile = Game::serializeMercenaryReleaseProfile(mercenaryRelease->profile);
            writeConvertedResource(output / "engine/mercenary_release.txt", {profile.begin(), profile.end()});
        }
        std::ostringstream manifest;
        manifest << "{\"schema\":1,\"stage\":\"merged-events\",\"legacy_event_version\":8,"
            "\"runtime_ready\":false,\"mmextension_lua_overlays_applied\":false,\"map_filter\":"
            << std::quoted(requested.size() == 1 ? *requested.begin() : "") << ",\"map_filters\":[";
        size_t filterIndex = 0;
        for (const std::string &stem : requested)
        {
            manifest << (filterIndex++ ? "," : "") << std::quoted(stem);
        }
        manifest << "],\"selective_overlays\":[";
        bool firstSelectiveOverlay = true;
        if (generatedNpcTexts)
        {
            manifest << "{\"profile\":\"mmmerge-generated-npc-greeting-v1\",\"sources\":[";
            for (size_t index = 0; index < GeneratedNpcSources.size(); ++index)
            {
                const auto &[path, hash] = GeneratedNpcSources[index];
                manifest << (index ? "," : "") << "{\"path\":" << std::quoted(std::string(path))
                    << ",\"sha256\":" << std::quoted(std::string(hash)) << '}';
            }
            manifest << "],\"path\":\"engine/events/Global.lua\",\"global_text_archive\":"
                << std::quoted(generatedNpcTexts->archive) << ",\"global_text_member\":\"global.txt\","
                "\"global_text_payload_sha256\":" << std::quoted(generatedNpcTexts->globalTextSha256)
                << ",\"scope\":\"Native generated identity greeting, admission and typed BTB history; "
                "existing names, portraits and professions retained\","
                "\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (crossContinents)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-cross-continents-final-guide-v6\","
                "\"source\":\"Scripts/Global/Quest_CrossContinents.lua\",\"source_sha256\":"
                << std::quoted(crossContinents->sourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_cross_continents_intro.lua\","
                "\"scope\":\"Invitation/introduction, explicit StartedAt policy, source encounters/letter/NPC location and scoped dialogue/hints; connector grant/use/replacement/leave recharge; continent rewards and final guide; dynamic Breach and quest completion unsupported\",\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (runawayChaos)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-runaway-chaos-dialogue-v1\","
                "\"source\":\"Scripts/Global/Quest_CrossContinents.lua\",\"source_sha256\":"
                << std::quoted(runawayChaos->sourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_runaway_chaos.lua\","
                "\"scope\":\"source hints, riddle answers and follower capture; dynamic maps remain unsupported\","
                "\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (breachAlvar)
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-breach-alvar-companions-v2\","
                "\"path\":\"engine/events/Global_mmmerge_runaway_companions.lua\","
                "\"content_profile\":\"engine/breach_alvar_companions.txt\",\"source_sha256\":[";
            for (size_t index = 0; index < breachAlvar->profile.sourceSha256.size(); ++index)
            {
                manifest << (index ? "," : "") << std::quoted(breachAlvar->profile.sourceSha256[index]);
            }
            manifest << "],\"generation_source_sha256\":[";
            for (size_t index = 0; index < breachAlvar->profile.generationSourceSha256.size(); ++index)
            {
                manifest << (index ? "," : "")
                    << std::quoted(breachAlvar->profile.generationSourceSha256[index]);
            }
            manifest << "],\"scope\":\"Source one-to-five-member split, reserve generation and return requests; "
                "normal Merge admission remains closed\",\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (breachMercenaries)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const Game::BreachMercenaryProfile &profile = breachMercenaries->profile;
            manifest << "{\"profile\":\"mmmerge-breach-mercenaries-v1\","
                "\"content_profile\":\"engine/breach_mercenaries.txt\",\"sources\":[";
            for (size_t index = 0; index < BreachMercenarySourcePaths.size(); ++index)
            {
                manifest << (index ? "," : "") << "{\"path\":"
                    << std::quoted(std::string(BreachMercenarySourcePaths[index])) << ",\"sha256\":"
                    << std::quoted(profile.sourceSha256[index]) << '}';
            }
            manifest << "],\"global_text_archive\":" << std::quoted(profile.chineseText
                ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod")
                << ",\"global_text_member\":\"global.txt\",\"global_text_payload_sha256\":"
                << std::quoted(profile.globalTextSha256)
                << ",\"scope\":\"Source spots, face mapping, roster aliases, fresh roster and Join text; "
                "no Character ownership or scene admission\",\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (mercenaryRelease)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const Game::MercenaryReleaseProfile &profile = mercenaryRelease->profile;
            manifest << "{\"profile\":\"mmmerge-mercenary-release-v1\","
                "\"content_profile\":\"engine/mercenary_release.txt\",\"source\":"
                << std::quoted(std::string(MercenaryReleaseSourcePath)) << ",\"source_sha256\":"
                << std::quoted(profile.sourceSha256)
                << ",\"global_text_archive\":" << std::quoted(profile.chineseText
                    ? "Data/10 LocZHCN.EnglishT.lod" : "Data/mmmerge.T.lod")
                << ",\"global_text_member\":\"global.txt\",\"global_text_payload_sha256\":"
                << std::quoted(profile.globalTextSha256)
                << ",\"scope\":\"Ordinary Inn source roster protection, release aliases and confirmation text; "
                "no Character ownership or refill execution\",\"complete_quest\":false}";
            firstSelectiveOverlay = false;
        }
        if (dimensionDoor && !selectedDimensionDoorOverlays.empty())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-dimension-door-activation-v1\","
                "\"source\":\"Scripts/General/2_OutdoorTravels.lua\",\"source_sha256\":"
                << std::quoted(dimensionDoor->sourceSha256)
                << ",\"timer_source\":\"Scripts/Core/timers.lua\",\"timer_source_sha256\":"
                << std::quoted(dimensionDoor->timerSourceSha256)
                << ",\"scope\":\"seven audited map activation and final-range hooks only\","
                "\"complete_cross_continents\":false,\"map_overlays\":[";
            size_t overlayIndex = 0;
            for (const auto &[pMap, pOverlay] : selectedDimensionDoorOverlays)
            {
                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / pMap->world
                    / "events/maps" / (pOverlay->mapStem + "_mmmerge_dimension_door.lua");
                manifest << (overlayIndex++ ? "," : "") << "{\"map_stem\":" << std::quoted(pOverlay->mapStem)
                    << ",\"source\":" << std::quoted("Scripts/Maps/" + pOverlay->mapStem + ".lua")
                    << ",\"source_sha256\":" << std::quoted(pOverlay->sourceSha256)
                    << ",\"path\":" << std::quoted(overlayPath.generic_string()) << '}';
            }
            manifest << "]}";
            firstSelectiveOverlay = false;
        }
        if (harmondaleCastle && selectedHarmondaleCastle != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                / (harmondaleCastle->mapStem + "_mmmerge_castle_history.lua");
            manifest << "{\"profile\":\"mmmerge-harmondale-castle-and-arbiter-v2\","
                "\"source\":\"Scripts/Maps/7out02.lua\",\"source_sha256\":"
                << std::quoted(harmondaleCastle->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"scope\":\"event301 entrance/history and event211 source arbiter succession after "
                   "artifact delivery or strictly more than six months since the first rebuilt-castle load\"}";
            firstSelectiveOverlay = false;
        }
        if (!selectedTownPortalVisits.empty())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-town-portal-visits-v1\",\"maps\":[";
            size_t index = 0;
            for (const auto &[stem, pProfile] : selectedTownPortalVisits)
            {
                manifest << (index++ ? "," : "") << "{\"map_stem\":" << std::quoted(stem)
                    << ",\"source\":" << std::quoted("Scripts/Maps/" + stem + ".lua")
                    << ",\"source_sha256\":" << std::quoted(pProfile->sourceSha256)
                    << ",\"event_id\":" << pProfile->eventId
                    << ",\"legacy_qbit\":" << pProfile->legacyQBit
                    << ",\"persistent_qbit\":" << pProfile->unlockQBit
                    << ",\"hook\":" << std::quoted(pProfile->afterLoad ? "AfterLoadMap" : "LoadMap")
                    << ",\"path\":" << std::quoted("worlds/mm6/events/maps/" + stem + "_mmmerge_town_visit.lua")
                    << '}';
            }
            manifest << "]}";
            firstSelectiveOverlay = false;
        }
        if (!selectedSubmarineTravel.empty())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-submarine-travel-v1\",\"maps\":[";
            size_t index = 0;
            for (const auto &[stem, pProfile] : selectedSubmarineTravel)
            {
                manifest << (index++ ? "," : "") << "{\"map_stem\":" << std::quoted(stem)
                    << ",\"source\":" << std::quoted("Scripts/Maps/" + stem + ".lua")
                    << ",\"source_sha256\":" << std::quoted(pProfile->sourceSha256)
                    << ",\"path\":" << std::quoted("worlds/mm8/events/maps/" + stem + "_mmmerge_submarine.lua")
                    << '}';
            }
            manifest << "]}";
            firstSelectiveOverlay = false;
        }
        if (stoneStatue && selectedStoneStatue != maps.end())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-stone-statue-rescue-v1\","
                "\"source\":\"Scripts/Maps/out07.lua\",\"source_sha256\":"
                << std::quoted(stoneStatue->sourceSha256)
                << ",\"path\":\"worlds/mm8/events/maps/out07_mmmerge_stone_statues.lua\","
                "\"scope\":\"events132-136 current member learned spell40 or party scroll339; source NPC/flags\"}";
            firstSelectiveOverlay = false;
        }
        if (!selectedReentries.empty())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-mm8-reentry-v1\",\"maps\":[";
            size_t index = 0;
            for (const auto &[stem, pProfile] : selectedReentries)
            {
                manifest << (index++ ? "," : "") << "{\"map_stem\":" << std::quoted(stem)
                    << ",\"source_sha256\":" << std::quoted(pProfile->sourceSha256)
                    << ",\"path\":" << std::quoted("worlds/mm8/events/maps/" + stem + "_mmmerge_reentry.lua")
                    << '}';
            }
            manifest << "]}";
            firstSelectiveOverlay = false;
        }
        if (selectedTempleBottle)
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-temple-bottle-return-v1\",\"source_sha256\":"
                << std::quoted(templeBottle->sourceSha256)
                << ",\"item_source_sha256\":" << std::quoted(templeBottle->itemSourceSha256)
                << ",\"path\":\"worlds/mm7/events/maps/7nwc_mmmerge_bottle_return.lua\","
                   "\"scope\":\"source501 session-owned return and original out02 fallback\"}";
            firstSelectiveOverlay = false;
        }
        if (emeraldTravel && selectedEmeraldTravel != maps.end())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-emerald-island-travel-v1\","
                "\"source\":\"Scripts/Maps/7out03.lua\",\"source_sha256\":"
                << std::quoted(emeraldTravel->sourceSha256)
                << ",\"path\":\"worlds/mm7/events/maps/7out03_mmmerge_emerald_travel.lua\","
                "\"scope\":\"House486 route4 follows source Q519 selection of location101 or44\"}";
            firstSelectiveOverlay = false;
        }
        if (freeHavenTemple && selectedFreeHavenTemple != maps.end())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-free-haven-temple-v1\","
                "\"source\":\"Scripts/Maps/outc2.lua\",\"source_sha256\":"
                << std::quoted(freeHavenTemple->sourceSha256)
                << ",\"path\":\"worlds/mm6/events/maps/outc2_mmmerge_temple.lua\","
                "\"scope\":\"event19 source follower professions and chalice transaction; event140 inn1607\"}";
            firstSelectiveOverlay = false;
        }
        if (stoneCity && selectedStoneCity != maps.end())
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-stone-city-entrance-v1\","
                "\"source\":\"Scripts/Maps/7d24.lua\",\"source_sha256\":"
                << std::quoted(stoneCity->sourceSha256)
                << ",\"path\":\"worlds/mm7/events/maps/7d24_mmmerge_stone_city.lua\","
                "\"scope\":\"event416 source Q647 gate instead of per-character award3; "
                "ExitNPC398 clears obsolete quest658\"}";
            firstSelectiveOverlay = false;
        }
        if (lambentCastle && selectedLambentCastle != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                / (lambentCastle->mapStem + "_mmmerge_lambent_entry.lua");
            manifest << "{\"profile\":\"mmmerge-lambent-castle-entrance-v1\","
                "\"source\":\"Scripts/Maps/7d30.lua\",\"source_sha256\":"
                << std::quoted(lambentCastle->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"scope\":\"event416 light-route and current-enemy throne-room gate only\"}";
            firstSelectiveOverlay = false;
        }
        if (gloamingCastle && selectedGloamingCastle != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                / (gloamingCastle->mapStem + "_mmmerge_gloaming_entry.lua");
            manifest << "{\"profile\":\"mmmerge-gloaming-castle-entrance-v1\","
                "\"source\":\"Scripts/Maps/d03.lua\",\"source_sha256\":"
                << std::quoted(gloamingCastle->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"scope\":\"event5 dark-route and current-enemy throne-room gate with Q710 house choice only\"}";
            firstSelectiveOverlay = false;
        }
        if (skeletonTransformer && selectedSkeletonTransformer != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm8/events/maps")
                / (skeletonTransformer->mapStem + "_mmmerge_skeleton_transformer.lua");
            manifest << "{\"profile\":\"mmmerge-skeleton-transformer-gates-v1\","
                "\"source\":\"Scripts/Maps/d19.lua\",\"source_sha256\":"
                << std::quoted(skeletonTransformer->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"source_map_var_index\":19,\"generated_map_var_index\":21"
                << ",\"scope\":\"event15 source chamber access and event131 exact puzzle completion without Dyson\"}";
            firstSelectiveOverlay = false;
        }
        if (sciFi && selectedSciFi != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm6/events/maps")
                / (sciFi->mapStem + "_mmmerge_blaster_learning.lua");
            manifest << "{\"profile\":\"mmmerge-sci-fi-blaster-learning-v1\","
                "\"source\":\"Scripts/Maps/sci-fi.lua\",\"source_sha256\":"
                << std::quoted(sciFi->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"scope\":\"append event61 zero-skill Blaster learning for every current party member; "
                   "preserve the source terminal message and continuation\"}";
            firstSelectiveOverlay = false;
        }
        if (controlCubeRefill && selectedControlCubeRefill != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-control-cube-refill-v1\","
                "\"source\":\"Scripts/Maps/mdt15.lua\",\"source_sha256\":"
                << std::quoted(controlCubeRefill->sourceSha256)
                << ",\"path\":\"worlds/mm7/events/maps/mdt15_mmmerge_control_cube_refill.lua\","
                "\"hook\":\"MapRefill\",\"hook_event_id\":" << controlCubeRefill->hookEventId
                << ",\"refill_interval_days\":" << controlCubeRefill->refillIntervalDays
                << ",\"scope\":\"Source count guards for actors 0/1; clear items then assign 1477 and 866 "
                   "on refill; preserve original Q759 event 1\"}";
            firstSelectiveOverlay = false;
        }
        if (smugglersCove && selectedSmugglersCove != maps.end())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm8/events/maps")
                / (smugglersCove->mapStem + "_mmmerge_hostility.lua");
            manifest << "{\"profile\":\"mmmerge-smugglers-cove-hostility-v1\","
                "\"source\":\"Scripts/Maps/d07.lua\",\"source_sha256\":"
                << std::quoted(smugglersCove->sourceSha256)
                << ",\"path\":" << std::quoted(overlayPath.generic_string())
                << ",\"scope\":\"map entry monster groups and kill anger with last-visit expiry\"}";
            firstSelectiveOverlay = false;
        }
        if (archibald && std::any_of(maps.begin(), maps.end(), [](const MapSource &map)
            { return map.world == "mm6" && std::filesystem::path(map.file).stem() == "outd3"; }))
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-archibald-entrance-v1\","
                "\"source\":\"Scripts/Maps/outd3.lua\",\"source_sha256\":"
                << std::quoted(archibald->sourceSha256)
                << ",\"path\":\"worlds/mm6/events/maps/outd3_mmmerge_archibald.lua\","
                "\"scope\":\"event42 bell/completion entrance and release movie; original global1343 reward retained; "
                "post-departure house animation487 selected from current NPC ownership\"}";
            firstSelectiveOverlay = false;
        }
        if (mm6Hive && std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
            { return map.world == "mm6" && std::filesystem::path(map.file).stem() == mm6Hive->mapStem; }))
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-mm6-hive-ending-v1\","
                "\"source\":\"Scripts/Maps/hive.lua\",\"source_sha256\":"
                << std::quoted(mm6Hive->sourceSha256)
                << ",\"path\":\"worlds/mm6/events/maps/hive_mmmerge_ending.lua\","
                "\"scope\":\"reactor damage, scroll gate, reinforcements, queen and map-leave endings\"}";
            firstSelectiveOverlay = false;
        }
        if (!selectedShoalsOverlays.empty())
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-mm7-shoals-route-v1\","
                "\"scope\":\"west boundary suit gate, surface exit and Lincoln return; "
                "underwater action restrictions and temporary portraits unsupported\",\"map_overlays\":[";
            size_t overlayIndex = 0;
            for (const auto &[stem, pOverlay] : selectedShoalsOverlays)
            {
                manifest << (overlayIndex++ ? "," : "") << "{\"map_stem\":" << std::quoted(stem)
                    << ",\"source\":" << std::quoted("Scripts/Maps/" + stem + ".lua")
                    << ",\"source_sha256\":" << std::quoted(pOverlay->sourceSha256)
                    << ",\"path\":" << std::quoted("worlds/mm7/events/maps/" + stem + "_mmmerge_shoals.lua")
                    << '}';
            }
            manifest << "]}";
            firstSelectiveOverlay = false;
        }
        if (blasterLearning)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-blaster-learning-topic-v1\",\"scope\":\"global\","
                "\"event_ids\":[950],\"source\":\"Scripts/Global/PromotionTopics.lua\",\"source_sha256\":"
                << std::quoted(blasterLearning->sourceSha256)
                << ",\"global_text_payload_sha256\":" << std::quoted(blasterLearning->globalTextSha256)
                << ",\"label_source\":\"Game.GlobalTxt[278]\","
                "\"path\":\"engine/events/Global_mmmerge_blaster_learning.lua\","
                "\"behavior\":\"teach zero-skill party members whose current class admits Blaster; "
                "preserve existing skills and source NPC topic unlocks\"}";
            firstSelectiveOverlay = false;
        }
        if (lichPromotion)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-lich-promotion-v2\",\"scope\":\"global\","
                "\"event_ids\":[89,738,846,847],\"source\":\"Scripts/Global/PromotionTopics.lua\",\"source_sha256\":"
                << std::quoted(lichPromotion->sourceSha256)
                << ",\"policy_source\":\"Scripts/General/MiscTweaks.lua\",\"policy_source_sha256\":"
                << std::quoted(lichPromotion->miscTweaksSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_lich_promotion.lua\","
                "\"behavior\":\"source-managed ClassIs effects; MM8 original first quest with appended final helper, "
                "MM7 acceptance/per-holder ritual and MM8 per-member repeat promotion; "
                "Dark Archmage fallback and final source Lich appearance/state\"}";
            firstSelectiveOverlay = false;
        }
        if (mm7Ending)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-mm7-ending-v1\",\"scope\":\"global\","
                "\"event_ids\":[920,922],\"source\":\"Scripts/Global/StdQuestsFunctions.lua\",\"source_sha256\":"
                << std::quoted(mm7Ending->sourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_endgame_tail.lua\","
                "\"behavior\":\"append source light journal and dark completion tails after original events\"}";
            firstSelectiveOverlay = false;
        }
        if (freeHavenCouncil)
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-free-haven-council-v1\",\"scope\":\"global\","
                "\"source\":\"Scripts/Global/StdQuestsFunctions.lua\",\"source_sha256\":"
                << std::quoted(freeHavenCouncil->globalSourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_council.lua\","
                "\"behavior\":\"append1693 source movie and rebuild quest-gated Council lobby\"}";
            firstSelectiveOverlay = false;
            if (selectedFreeHavenTemple != maps.end())
            {
                manifest << ",{\"profile\":\"mmmerge-free-haven-council-v1\",\"scope\":\"map\","
                    "\"source\":\"Scripts/Maps/outc2.lua\",\"source_sha256\":"
                    << std::quoted(freeHavenCouncil->mapSourceSha256)
                    << ",\"path\":\"worlds/mm6/events/maps/outc2_mmmerge_council.lua\","
                    "\"behavior\":\"doors49/50 original evidence transaction and owned-roster endorsements\"}";
            }
        }
        if (mm6QuestContinuity)
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-mm6-quest-followers-v1\",\"scope\":\"global\","
                "\"event_ids\":[1346,1347],\"source\":\"Scripts/Global/StdQuestsFollowers.lua\","
                "\"source_sha256\":" << std::quoted(mm6QuestContinuity->followersSourceSha256)
                << ",\"follower_api_source\":\"Scripts/General/NPCFollowers.lua\","
                "\"follower_api_source_sha256\":" << std::quoted(mm6QuestContinuity->followerApiSourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_quest_followers.lua\","
                "\"behavior\":\"append NPC802 follower cleanup after original reward and topic events\"}";
            firstSelectiveOverlay = false;
            if (selectedSewerContinuity)
            {
                manifest << ",{\"profile\":\"mmmerge-mm6-quest-followers-v1\",\"scope\":\"map\","
                    "\"event_ids\":[8],\"source\":\"Scripts/Maps/sewer.lua\",\"source_sha256\":"
                    << std::quoted(mm6QuestContinuity->sewerSourceSha256)
                    << ",\"follower_api_source_sha256\":"
                    << std::quoted(mm6QuestContinuity->followerApiSourceSha256)
                    << ",\"path\":\"worlds/mm6/events/maps/sewer_mmmerge_quest_followers.lua\","
                    "\"behavior\":\"append source Q1701-guarded NPC802 follower admission after event8\"}";
            }
            if (selectedKeyChestContinuity)
            {
                manifest << ",{\"profile\":\"mmmerge-mm6-key-chest-v1\",\"scope\":\"map\","
                    "\"event_ids\":[16],\"source\":\"Scripts/Maps/6d07.lua\",\"source_sha256\":"
                    << std::quoted(mm6QuestContinuity->keyChestSourceSha256)
                    << ",\"path\":\"worlds/mm6/events/maps/6d07_mmmerge_key_chest.lua\","
                    "\"source_map_var\":4,\"exported_map_var\":6,"
                    "\"behavior\":\"choose original chest1 or6 from current2107 inventory and retained counter\"}";
            }
        }
        if (selectedRegnaCannon)
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-regna-cannon-v1\",\"scope\":\"map\","
                "\"event_ids\":[451,452," << RegnaCannonMigrationHookEventId
                << "],\"source\":\"Scripts/Maps/out13.lua\",\"source_sha256\":"
                << std::quoted(regnaCannon->sourceSha256)
                << ",\"path\":\"worlds/mm8/events/maps/out13_mmmerge_cannon.lua\","
                "\"removed_legacy_timer_source_event_id\":452,\"dynamic_timer_interval_minutes\":1,"
                "\"migration_hook_event_id\":" << RegnaCannonMigrationHookEventId
                << ",\"migration\":\"once-only legacy MapVar41 in-flight stage transfer; restart after one minute\","
                "\"behavior\":\"source repeatable cannon and independent map-visit timer instances; "
                "completion NPC and history transaction remains once-only\"}";
            firstSelectiveOverlay = false;
        }
        if (artifactMessenger && std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
            { return map.world == "mm7" && std::filesystem::path(map.file).stem() == artifactMessenger->mapStem; }))
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-artifact-messenger-v1\",\"scope\":\"map\","
                "\"source\":\"Scripts/Maps/7out04.lua\",\"source_sha256\":"
                << std::quoted(artifactMessenger->sourceSha256)
                << ",\"path\":\"worlds/mm7/events/maps/7out04_mmmerge_messenger.lua\","
                "\"behavior\":\"Judge death gate and original six war groups, once per source map state\"}";
            firstSelectiveOverlay = false;
        }
        const auto recordMapOverlay = [&](const std::string &world, const std::string &stem,
            const std::string &hash, const std::string &suffix)
        {
            if (!std::any_of(maps.begin(), maps.end(), [&](const MapSource &map)
                { return map.world == world && std::filesystem::path(map.file).stem() == stem; }))
            {
                return;
            }
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":" << std::quoted("mmmerge-" + suffix + "-v1")
                << ",\"scope\":\"map\",\"source\":" << std::quoted("Scripts/Maps/" + stem + ".lua")
                << ",\"source_sha256\":" << std::quoted(hash)
                << ",\"path\":" << std::quoted("worlds/" + world + "/events/maps/" + stem
                    + "_mmmerge_" + suffix + ".lua") << '}';
            firstSelectiveOverlay = false;
        };
        if (lostShield) recordMapOverlay("mm6", lostShield->mapStem, lostShield->sourceSha256, "lost_shield");
        for (const auto &profile : postQuestVisits)
        {
            recordMapOverlay(profile.world, profile.mapStem, profile.sourceSha256, "post_quest_visit");
        }
        for (const auto &profile : mm8Remaining)
        {
            if (!profile.requiredEventIds.empty() || !profile.addedOnLoadEventIds.empty()
                || !profile.addedAfterLoadEventIds.empty())
            {
                recordMapOverlay("mm8", profile.mapStem, profile.sourceSha256, "remaining");
            }
        }
        for (const auto &profile : mm6Remaining)
        {
            if (!profile.requiredEventIds.empty() || !profile.addedEventIds.empty())
                recordMapOverlay("mm6", profile.mapStem, profile.sourceSha256, "remaining");
        }
        for (const auto &profile : mm7Remaining)
            recordMapOverlay("mm7", profile.mapStem, profile.sourceSha256, "remaining");
        if (mm6RemainingGlobal)
        {
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-mm6-remaining-v1\",\"scope\":\"global\","
                "\"source_sha256\":" << std::quoted(mm6RemainingGlobal->followersSourceSha256)
                << ",\"functions_sha256\":" << std::quoted(mm6RemainingGlobal->functionsSourceSha256)
                << ",\"follower_api_sha256\":" << std::quoted(mm6RemainingGlobal->followerApiSourceSha256)
                << ",\"path\":\"engine/events/Global_mmmerge_mm6_remaining.lua\"}";
            firstSelectiveOverlay = false;
        }
        for (size_t index = 0; index < mm7RemainingGlobal.size(); ++index)
        {
            const auto &profile = mm7RemainingGlobal[index];
            if (!firstSelectiveOverlay) manifest << ',';
            manifest << "{\"profile\":\"mmmerge-mm7-remaining-v1\",\"scope\":\"global\","
                "\"source\":" << std::quoted(profile.sourcePath)
                << ",\"source_sha256\":" << std::quoted(profile.sourceSha256)
                << ",\"follower_api_sha256\":" << std::quoted(profile.followerApiSourceSha256)
                << ",\"path\":" << std::quoted("engine/events/Global_mmmerge_mm7_remaining_"
                    + std::to_string(index) + ".lua") << '}';
            firstSelectiveOverlay = false;
        }
        if (questItemCapacity)
        {
            if (!firstSelectiveOverlay)
            {
                manifest << ',';
            }
            manifest << "{\"profile\":\"mmmerge-quest-item-capacity-v1\",\"scope\":\"global\","
                "\"event_ids\":[29,164,165,166,175,188],"
                "\"path\":\"engine/events/Global_mmmerge_quest_item_capacity.lua\",\"source_sha256\":"
                << std::quoted(mm8Endgame->globalEventSha256) << '}';
            firstSelectiveOverlay = false;
        }
        manifest << "],\"input_string_answer_bindings\":[";
        if (mm8Endgame)
        {
            manifest << "{\"profile\":\"mmmerge-mm8-escaton-answers-v1\",\"scope\":\"global\","
                "\"event_ids\":[164,165,166],\"step\":1,\"answer_resource\":\"d35.str\",\"evt_archive\":"
                << std::quoted(mm8Endgame->globalEventArchive) << ",\"evt_sha256\":"
                << std::quoted(mm8Endgame->globalEventSha256) << ",\"answer_archive\":"
                << std::quoted(mm8Endgame->answerArchive) << ",\"answer_sha256\":"
                << std::quoted(mm8Endgame->answerStrSha256) << ",\"chinese_answers\":"
                << (mm8Endgame->chineseAnswers ? "true" : "false") << '}';
        }
        manifest << "],\"programs\":[";
        bool first = true;
        for (const MapSource &map : maps)
        {
            checkImportCancellation(cancelled);
            const bool global = map.world.empty();
            const std::string stem = std::filesystem::path(map.file).stem().string();
            const std::string evtName = stem + ".evt";
            const std::string strName = stem + ".str";
            Game::EvtProgram evt;
            const std::vector<uint8_t> bytes = readLodText(members.at(evtName).archive, evtName);
            require(!bytes.empty() && evt.loadFromBytes(bytes), "Malformed EVT: " + evtName);
            for (const Game::EvtEvent &event : evt.getEvents())
            {
                for (const Game::EvtInstruction &instruction : event.instructions)
                {
                    validateInstruction(instruction, evtName + " event " + std::to_string(event.eventId)
                        + " step " + std::to_string(instruction.step));
                }
            }
            size_t glyphs = 0;
            const Game::StrTable strings = decodeStrings(members, strName, glyphs);
            if (members.contains(strName) && members.at(strName).chinese)
            {
                size_t unusedGlyphs = 0;
                const Game::StrTable originalStrings = decodeStrings(baseMembers, strName, unusedGlyphs);
                require(originalStrings.getEntries().size() == strings.getEntries().size(),
                    "Localized STR slot count differs from base: " + strName);
            }
            Game::LegacyLuaExportLookups mapLookups = lookups;
            if (!global)
            {
                mapLookups.mapName = map.name;
                mapLookups.sourceMapFile = map.file;
            }
            std::string lua = Game::generateLegacyEventLuaChunk(evt, strings, mapLookups,
                global ? Game::LegacyLuaExportScope::Global : Game::LegacyLuaExportScope::Map,
                Game::LegacyEventVersion::Mm8);
            if (global && generatedNpcTexts)
            {
                lua += generatedNpcTexts->luaSource;
            }
            std::string error;
            const std::optional<Game::ScriptedEventProgram> program = Game::ScriptedEventProgram::loadFromLuaText(
                std::string(EventSupportSource) + "\n" + lua, "@" + evtName,
                global ? Game::ScriptedEventScope::Global : Game::ScriptedEventScope::Map, error);
            require(program.has_value(), "Generated Lua rejected for " + evtName + ": " + error);
            if (global && blasterLearning)
            {
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + blasterLearning->luaSource,
                    "@Global.lua+blaster-learning", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Blaster learning topic Lua rejected: " + error);
                std::vector<uint16_t> expectedEvents = program->eventIds();
                if (!program->hasEvent(950))
                {
                    expectedEvents.push_back(950);
                    std::sort(expectedEvents.begin(), expectedEvents.end());
                }
                require(overlaid->hasEvent(950) && overlaid->eventIds() == expectedEvents
                    && overlaid->canShowTopicEventIds() == program->canShowTopicEventIds(),
                    "Blaster learning overlay changed unrelated source event or topic registration");
                writeConvertedResource(output / "engine/events/Global_mmmerge_blaster_learning.lua",
                    {blasterLearning->luaSource.begin(), blasterLearning->luaSource.end()});
            }
            if (global && lichPromotion)
            {
                require(program->hasEvent(89) && program->hasEvent(738)
                    && program->hasEvent(846) && program->hasEvent(847),
                    "Missing original Lich promotion events");
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + lichPromotion->luaSource,
                    "@Global.lua+lich-promotion", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Lich promotion Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->canShowTopicEventIds() == program->canShowTopicEventIds(),
                    "Lich promotion overlay changed unrelated source event or topic registration");
                require(overlaid->sourceManagedLichEffects(),
                    "Lich promotion overlay did not declare source-managed class effects");
                writeConvertedResource(output / "engine/events/Global_mmmerge_lich_promotion.lua",
                    {lichPromotion->luaSource.begin(), lichPromotion->luaSource.end()});
            }
            if (global && mm7Ending)
            {
                require(program->hasEvent(920) && program->hasEvent(922), "Missing original MM7 ending events");
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + mm7Ending->luaSource,
                    "@Global.lua+mm7-ending", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "MM7 ending Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->canShowTopicEventIds() == program->canShowTopicEventIds(),
                    "MM7 ending overlay changed unrelated source event or topic registration");
                writeConvertedResource(output / "engine/events/Global_mmmerge_endgame_tail.lua",
                    {mm7Ending->luaSource.begin(), mm7Ending->luaSource.end()});
            }
            if (global && freeHavenCouncil)
            {
                require(program->hasEvent(1693), "Missing original Council topic1693");
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + freeHavenCouncil->globalLuaSource,
                    "@Global.lua+council", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Council topic Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->canShowTopicEventIds() == program->canShowTopicEventIds(),
                    "Council topic overlay changed unrelated event or topic registration");
                writeConvertedResource(output / "engine/events/Global_mmmerge_council.lua",
                    {freeHavenCouncil->globalLuaSource.begin(), freeHavenCouncil->globalLuaSource.end()});
            }
            if (global && mm6QuestContinuity)
            {
                require(program->hasEvent(1346) && program->hasEvent(1347),
                    "Missing original MM6 prisoner reward or follow-up topic");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + mm6QuestContinuity->globalLuaSource,
                    "@Global.lua+mm6-quest-followers", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "MM6 quest follower global Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->canShowTopicEventIds() == program->canShowTopicEventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && sameTimerRegistrations(overlaid->timerTriggers(), program->timerTriggers())
                    && overlaid->getHint(1346) == program->getHint(1346)
                    && overlaid->getHint(1347) == program->getHint(1347),
                    "MM6 quest follower global overlay changed unrelated registration or hints");
                writeConvertedResource(output / "engine/events/Global_mmmerge_quest_followers.lua",
                    {mm6QuestContinuity->globalLuaSource.begin(), mm6QuestContinuity->globalLuaSource.end()});
            }
            if (global && questItemCapacity)
            {
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + *questItemCapacity,
                    "@Global.lua+quest-item-capacity", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Quest item capacity Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->canShowTopicEventIds() == program->canShowTopicEventIds(),
                    "Quest item capacity overlay changed source event or topic registration");
                writeConvertedResource(output / "engine/events/Global_mmmerge_quest_item_capacity.lua",
                    {questItemCapacity->begin(), questItemCapacity->end()});
            }
            if (global && crossContinents)
            {
                for (uint16_t id : {65500, 65501, 65502, 65503})
                {
                    require(!program->hasEvent(id), "Cross-continent hook ID conflicts with Global.evt");
                }
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + crossContinents->luaSource,
                    "@Global.lua+cross-continent-intro", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Cross-continent introduction Lua rejected: " + error);
                for (uint16_t id : {1778, 1779, 1780, 65500, 65501, 65502, 65503})
                {
                    require(overlaid->hasEvent(id), "Cross-continent introduction handler was not registered");
                }
                writeConvertedResource(output / "engine/events/Global_mmmerge_cross_continents_intro.lua",
                    {crossContinents->luaSource.begin(), crossContinents->luaSource.end()});
            }
            if (global && runawayChaos)
            {
                for (uint16_t id : {65520, 65521, 65522})
                {
                    require(!program->hasEvent(id), "Runaway Chaos hook ID conflicts with Global.evt");
                }
                const std::string combined = std::string(EventSupportSource) + "\n" + lua + "\n"
                    + (crossContinents ? crossContinents->luaSource : std::string()) + "\n" + runawayChaos->luaSource;
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    combined, "@Global.lua+runaway-chaos", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Runaway Chaos Lua rejected: " + error);
                for (uint16_t id : {65520, 65521, 65522})
                {
                    require(overlaid->hasEvent(id), "Runaway Chaos hook was not registered");
                }
                writeConvertedResource(output / "engine/events/Global_mmmerge_runaway_chaos.lua",
                    {runawayChaos->luaSource.begin(), runawayChaos->luaSource.end()});
            }
            if (global && breachAlvar)
            {
                const std::string combined = std::string(EventSupportSource) + "\n" + lua + "\n"
                    + (crossContinents ? crossContinents->luaSource : std::string()) + "\n"
                    + runawayChaos->luaSource + "\n" + breachAlvar->globalLuaSource;
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    combined, "@Global.lua+breach-alvar-companions", Game::ScriptedEventScope::Global, error);
                require(overlaid.has_value(), "Breach Alvar global companion Lua rejected: " + error);
                // Sorted overlay discovery loads this after Global_mmmerge_runaway_chaos.lua.
                writeConvertedResource(output / "engine/events/Global_mmmerge_runaway_companions.lua",
                    {breachAlvar->globalLuaSource.begin(), breachAlvar->globalLuaSource.end()});
            }
            const auto dimensionDoorOverlay = dimensionDoorOverlays.find(stem);
            if (!global && dimensionDoorOverlay != dimensionDoorOverlays.end())
            {
                const DimensionDoorMapOverlay &overlay = *dimensionDoorOverlay->second;
                const bool usesFirstReservedId = overlay.mapStem != "oute3";
                const bool usesSecondReservedId = overlay.mapStem == "out02";
                require(!usesFirstReservedId || !program->hasEvent(65490),
                    "Dimension Door hook ID 65490 conflicts with " + evtName);
                require(!usesSecondReservedId || !program->hasEvent(65491),
                    "Dimension Door hook ID 65491 conflicts with " + evtName);

                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay.luaSource,
                    "@" + evtName + "+dimension-door", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Dimension Door activation Lua rejected for " + evtName + ": " + error);

                if (usesFirstReservedId)
                {
                    require(overlaid->hasEvent(65490), "Dimension Door activation hook 65490 was not registered");
                }
                if (usesSecondReservedId)
                {
                    require(overlaid->hasEvent(65491), "Dimension Door Town Portal hook 65491 was not registered");
                }
                if (overlay.mapStem == "out01" || overlay.mapStem == "out07" || overlay.mapStem == "outb3"
                    || overlay.mapStem == "7out04" || overlay.mapStem == "out02")
                {
                    require(std::find(overlaid->outdoorStepHookEventIds().begin(),
                        overlaid->outdoorStepHookEventIds().end(), 65490)
                            != overlaid->outdoorStepHookEventIds().end(),
                        "Dimension Door outdoor-step hook was not registered for " + evtName);
                }
                if (overlay.mapStem == "out02")
                {
                    require(std::find(overlaid->townPortalHookEventIds().begin(),
                        overlaid->townPortalHookEventIds().end(), 65491)
                            != overlaid->townPortalHookEventIds().end(),
                        "Dimension Door Town Portal hook was not registered for " + evtName);
                }
                if (overlay.mapStem == "7out04")
                {
                    const auto modelEvent = overlaid->outdoorModelEvents().find("ClL1_W");
                    require(modelEvent != overlaid->outdoorModelEvents().end() && modelEvent->second == 504,
                        "Dimension Door model-face event was not registered for " + evtName);
                }
                if (overlay.mapStem == "out09")
                {
                    const auto timer = std::find_if(
                        overlaid->timerTriggers().begin(),
                        overlaid->timerTriggers().end(),
                        [](const Game::ScriptedEventProgram::TimerTrigger &trigger)
                        {
                            return trigger.eventId == 65490;
                        });
                    require(timer != overlaid->timerTriggers().end()
                            && timer->scheduleKind == Game::ScriptedEventTimerScheduleKind::Interval
                            && timer->repeating && timer->intervalGameMinutes == 1.0 && timer->startMinute == 3,
                        "Dimension Door exact interval timer was not registered for " + evtName);
                }

                if (const std::optional<uint16_t> eventId = dimensionDoorReplacementEventId(overlay.mapStem))
                {
                    require(overlaid->hasEvent(*eventId),
                        "Dimension Door replacement event was not registered for " + evtName);
                }

                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / map.world
                    / "events/maps" / (stem + "_mmmerge_dimension_door.lua");
                writeConvertedResource(output / overlayPath, {overlay.luaSource.begin(), overlay.luaSource.end()});
            }
            if (!global && harmondaleCastle && map.world == "mm7" && stem == harmondaleCastle->mapStem)
            {
                require(program->hasEvent(211) && program->hasEvent(301),
                    "Castle Harmondale source events 211/301 are absent");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + harmondaleCastle->luaSource,
                    "@" + evtName + "+harmondale-castle", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Castle Harmondale entrance/arbiter Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds(),
                    "Castle Harmondale entrance/arbiter overlay changed source event registration");
                std::vector<uint16_t> originalOnLoad = program->onLoadEventIds();
                std::vector<uint16_t> overlaidOnLoad = overlaid->onLoadEventIds();
                std::sort(originalOnLoad.begin(), originalOnLoad.end());
                std::sort(overlaidOnLoad.begin(), overlaidOnLoad.end());
                require(originalOnLoad == overlaidOnLoad
                    && std::count(overlaidOnLoad.begin(), overlaidOnLoad.end(), 211) == 1,
                    "Castle Harmondale arbiter source load hook was not retained exactly once");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / map.world
                    / "events/maps" / (stem + "_mmmerge_castle_history.lua");
                writeConvertedResource(output / overlayPath,
                    {harmondaleCastle->luaSource.begin(), harmondaleCastle->luaSource.end()});
            }
            if (!global && mm6QuestContinuity && map.world == "mm6" && (stem == "sewer" || stem == "6d07"))
            {
                const bool sewer = stem == "sewer";
                const uint16_t eventId = sewer ? 8 : 16;
                require(program->hasEvent(eventId), "Missing original MM6 quest continuity source event");
                if (!sewer)
                {
                    require(lua.find("SetValue(MapVar(6), 1)") != std::string::npos,
                        "MM6 key chest exported counter layout changed; review source MapVar4");
                    require(program->getHint(16).has_value(), "Missing MM6 key chest localized hint");
                }
                const std::string &overlay = sewer
                    ? mm6QuestContinuity->sewerLuaSource : mm6QuestContinuity->keyChestLuaSource;
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+mm6-quest-continuity", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "MM6 quest continuity Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && sameTimerRegistrations(overlaid->timerTriggers(), program->timerTriggers())
                    && overlaid->getHint(eventId) == program->getHint(eventId),
                    "MM6 quest continuity overlay changed unrelated registration or hint");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm6/events/maps")
                    / (stem + (sewer ? "_mmmerge_quest_followers.lua" : "_mmmerge_key_chest.lua"));
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            const auto writeSupplement = [&](const std::string &overlay, const std::string &suffix,
                std::vector<uint16_t> addedIds, const std::vector<uint16_t> &addedLoad,
                const std::vector<uint16_t> &addedAfter,
                const std::vector<uint16_t> &removedTimers = {},
                const std::vector<uint16_t> &addedLeave = {},
                const std::vector<uint16_t> &removedLoad = {})
            {
                if (overlay.empty()) return;
                for (uint16_t id : addedIds)
                {
                    require(!program->hasEvent(id), "Supplement event collision in " + stem);
                }
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+" + suffix,
                    global ? Game::ScriptedEventScope::Global : Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Supplement Lua rejected for " + stem + ": " + error);
                auto expectedIds = program->eventIds();
                expectedIds.insert(expectedIds.end(), addedIds.begin(), addedIds.end());
                std::sort(expectedIds.begin(), expectedIds.end());
                auto expectedLoad = program->onLoadEventIds();
                for (uint16_t id : removedLoad)
                {
                    require(std::erase(expectedLoad, id) > 0, "Missing removed load hook for " + stem);
                }
                expectedLoad.insert(expectedLoad.end(), addedLoad.begin(), addedLoad.end());
                auto expectedAfter = program->afterLoadEventIds();
                expectedAfter.insert(expectedAfter.end(), addedAfter.begin(), addedAfter.end());
                auto expectedLeave = program->onLeaveEventIds();
                expectedLeave.insert(expectedLeave.end(), addedLeave.begin(), addedLeave.end());
                auto expectedTimers = program->timerTriggers();
                for (uint16_t id : removedTimers)
                {
                    require(std::erase_if(expectedTimers, [id](const auto &timer)
                        { return timer.origin == Game::ScriptedEventTimerOrigin::Legacy
                            && timer.sourceEventId == id; }) == 1,
                        "Expected exactly one removed source timer for " + stem);
                }
                require(overlaid->eventIds() == expectedIds && overlaid->onLoadEventIds() == expectedLoad
                    && overlaid->afterLoadEventIds() == expectedAfter
                    && overlaid->onLeaveEventIds() == expectedLeave
                    && sameTimerRegistrations(overlaid->timerTriggers(), expectedTimers),
                    "Supplement changed unrelated registration for " + stem);
                const auto path = global
                    ? std::filesystem::path("engine/events") / ("Global_mmmerge_" + suffix + ".lua")
                    : std::filesystem::path("worlds") / map.world / "events/maps"
                        / (stem + "_mmmerge_" + suffix + ".lua");
                writeConvertedResource(output / path, {overlay.begin(), overlay.end()});
            };
            const auto localizedSupplementStrings = [&](const auto &profile)
            {
                for (uint16_t id : profile.requiredEventIds)
                    require(program->hasEvent(id), "Missing original remaining source event for " + stem);
                std::unordered_map<uint16_t, std::string> localized;
                for (uint16_t id : profile.requiredStringIds)
                {
                    const auto text = strings.get(id);
                    require(text.has_value(), "Missing original remaining source string for " + stem);
                    localized.emplace(id, *text);
                }
                return localized;
            };
            if (global && mm6RemainingGlobal)
            {
                for (uint16_t id : {1331, 1333, 1334, 1631, 1634, 1638, 1640, 1642, 1645, 1646})
                    require(program->hasEvent(id), "Missing original MM6 follower global event");
                writeSupplement(mm6RemainingGlobal->luaSource, "mm6_remaining",
                    mm6RemainingGlobal->addedEventIds, {}, mm6RemainingGlobal->addedAfterLoadEventIds);
            }
            if (global)
            {
                for (size_t index = 0; index < mm7RemainingGlobal.size(); ++index)
                {
                    const auto &profile = mm7RemainingGlobal[index];
                    for (uint16_t id : profile.requiredEventIds)
                        require(program->hasEvent(id), "Missing original MM7 follower global event");
                    writeSupplement(profile.luaSource, "mm7_remaining_" + std::to_string(index),
                        profile.addedNpcEnterHookEventIds, {}, {});
                }
            }
            if (!global && map.world == "mm6")
            {
                for (const auto &profile : mm6Remaining)
                {
                    if (profile.mapStem != stem) continue;
                    const auto localized = localizedSupplementStrings(profile);
                    writeSupplement(makeMm6RemainingMapOverlay(profile, localized), "remaining", profile.addedEventIds,
                        {}, profile.addedAfterLoadEventIds, profile.removedLegacyTimerEventIds);
                }
            }
            if (!global && map.world == "mm7")
            {
                for (const auto &profile : mm7Remaining)
                {
                    if (profile.mapStem != stem) continue;
                    const auto localized = localizedSupplementStrings(profile);
                    writeSupplement(makeMm7RemainingMapOverlay(profile, localized), "remaining", profile.addedEventIds,
                        profile.addedOnLoadEventIds, profile.addedAfterLoadEventIds,
                        profile.removedLegacyTimerEventIds, profile.addedOnLeaveEventIds,
                        profile.removedOnLoadEventIds);
                }
            }
            if (!global && lostShield && map.world == "mm6" && stem == lostShield->mapStem)
            {
                writeSupplement(lostShield->luaSource, "lost_shield", {Mm6LostShieldHookEventId}, {}, {});
            }
            if (!global)
            {
                for (const auto &profile : postQuestVisits)
                {
                    if (profile.world != map.world || profile.mapStem != stem) continue;
                    std::vector<uint16_t> added;
                    if (profile.afterLoadEventId != 0) added.push_back(profile.afterLoadEventId);
                    writeSupplement(profile.luaSource, "post_quest_visit", added, {}, added);
                }
                if (map.world == "mm8")
                {
                    for (const auto &profile : mm8Remaining)
                    {
                        if (profile.mapStem != stem) continue;
                        for (uint16_t id : profile.requiredEventIds)
                            require(program->hasEvent(id), "Missing source event for " + stem);
                        std::unordered_map<uint16_t, std::string> localized;
                        for (uint16_t id : profile.requiredStringIds)
                        {
                            const auto text = strings.get(id);
                            require(text.has_value(), "Missing source string for " + stem);
                            localized.emplace(id, *text);
                        }
                        auto added = profile.addedOnLoadEventIds;
                        added.insert(added.end(), profile.addedAfterLoadEventIds.begin(),
                            profile.addedAfterLoadEventIds.end());
                        writeSupplement(makeMm8RemainingMapOverlay(profile, localized), "remaining", added,
                            profile.addedOnLoadEventIds, profile.addedAfterLoadEventIds);
                    }
                }
            }
            if (!global && selectedRegnaCannon && map.world == "mm8" && stem == regnaCannon->mapStem)
            {
                const std::optional<std::string> cannonHint = strings.get(13);
                const std::optional<std::string> firingText = strings.get(11);
                const std::optional<std::string> noAmmoText = strings.get(18);
                require(program->hasEvent(451) && program->hasEvent(452) && cannonHint && firingText && noAmmoText,
                    "Missing original Regna cannon events or localized STR slots");
                require(!program->hasEvent(RegnaCannonMigrationHookEventId),
                    "Regna cannon migration hook conflicts with a source event");
                const std::string overlay = makeRegnaCannonOverlay(*cannonHint, *firingText, *noAmmoText);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+regna-cannon", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Regna cannon Lua rejected: " + error);
                std::vector<Game::ScriptedEventProgram::TimerTrigger> expectedTimers = program->timerTriggers();
                const size_t removed = std::erase_if(expectedTimers,
                    [](const Game::ScriptedEventProgram::TimerTrigger &timer)
                    {
                        return timer.origin == Game::ScriptedEventTimerOrigin::Legacy && timer.sourceEventId == 452;
                    });
                require(removed == 1, "Regna cannon source must contain exactly one legacy event452 timer");
                std::vector<uint16_t> expectedIds = program->eventIds();
                expectedIds.push_back(RegnaCannonMigrationHookEventId);
                std::sort(expectedIds.begin(), expectedIds.end());
                std::vector<uint16_t> expectedAfterLoad = program->afterLoadEventIds();
                expectedAfterLoad.push_back(RegnaCannonMigrationHookEventId);
                require(overlaid->eventIds() == expectedIds
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == expectedAfterLoad
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && overlaid->getHint(451) == program->getHint(451)
                    && overlaid->getHint(452) == program->getHint(452)
                    && sameTimerRegistrations(overlaid->timerTriggers(), expectedTimers),
                    "Regna cannon overlay changed unrelated registration, hint or timer schedule");
                const std::filesystem::path overlayPath = "worlds/mm8/events/maps/out13_mmmerge_cannon.lua";
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && sciFi && map.world == "mm6" && stem == sciFi->mapStem)
            {
                require(program->hasEvent(61), "Sci-fi terminal source event 61 is absent");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + sciFi->luaSource,
                    "@" + evtName + "+blaster-learning", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Sci-fi Blaster learning Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Sci-fi Blaster learning overlay changed source event/lifecycle registration");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm6/events/maps")
                    / (stem + "_mmmerge_blaster_learning.lua");
                writeConvertedResource(output / overlayPath, {sciFi->luaSource.begin(), sciFi->luaSource.end()});
            }
            if (!global && controlCubeRefill && map.world == "mm7" && stem == controlCubeRefill->mapStem)
            {
                require(program->hasEvent(1), "Control cube original entry event 1 is absent");
                require(!program->hasEvent(controlCubeRefill->hookEventId),
                    "Control cube refill hook conflicts with a source event");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + controlCubeRefill->luaSource,
                    "@" + evtName + "+control-cube-refill", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Control cube refill Lua rejected: " + error);
                std::vector<uint16_t> expectedIds = program->eventIds();
                expectedIds.push_back(controlCubeRefill->hookEventId);
                std::sort(expectedIds.begin(), expectedIds.end());
                require(overlaid->eventIds() == expectedIds
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && overlaid->timerTriggers().size() == program->timerTriggers().size(),
                    "Control cube refill overlay changed unrelated source event/lifecycle registration");
                const std::filesystem::path overlayPath =
                    "worlds/mm7/events/maps/mdt15_mmmerge_control_cube_refill.lua";
                writeConvertedResource(output / overlayPath,
                    {controlCubeRefill->luaSource.begin(), controlCubeRefill->luaSource.end()});
            }
            if (!global && map.world == "mm6" && selectedTownPortalVisits.contains(stem))
            {
                const TownPortalVisitProfile &profile = *selectedTownPortalVisits.at(stem);
                require(!program->hasEvent(profile.eventId), "Town Portal visit hook conflicts with a source event");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + profile.luaSource,
                    "@" + evtName + "+town-portal-visit", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Town Portal visit Lua rejected: " + error);
                std::vector<uint16_t> expectedIds = program->eventIds();
                expectedIds.push_back(profile.eventId);
                std::sort(expectedIds.begin(), expectedIds.end());
                std::vector<uint16_t> expectedOnLoad = program->onLoadEventIds();
                std::vector<uint16_t> expectedAfterLoad = program->afterLoadEventIds();
                (profile.afterLoad ? expectedAfterLoad : expectedOnLoad).push_back(profile.eventId);
                require(overlaid->eventIds() == expectedIds && overlaid->onLoadEventIds() == expectedOnLoad
                    && overlaid->afterLoadEventIds() == expectedAfterLoad
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Town Portal visit overlay changed unrelated source registration");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm6/events/maps")
                    / (stem + "_mmmerge_town_visit.lua");
                writeConvertedResource(output / overlayPath, {profile.luaSource.begin(), profile.luaSource.end()});
            }
            if (!global && map.world == "mm8" && selectedSubmarineTravel.contains(stem))
            {
                const SubmarineTravelProfile &profile = *selectedSubmarineTravel.at(stem);
                require(program->hasEvent(451), "Submarine source event451 is absent");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + profile.luaSource,
                    "@" + evtName + "+submarine", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Submarine travel Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && overlaid->getHint(451) == program->getHint(451),
                    "Submarine travel overlay changed source registration or hint");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm8/events/maps")
                    / (stem + "_mmmerge_submarine.lua");
                writeConvertedResource(output / overlayPath, {profile.luaSource.begin(), profile.luaSource.end()});
            }
            if (!global && stoneStatue && map.world == "mm8" && stem == stoneStatue->mapStem)
            {
                const std::optional<std::string> statueHint = strings.get(13);
                require(statueHint && !statueHint->empty(), "Stone statue source hint is absent");
                for (uint16_t eventId = 132; eventId <= 136; ++eventId)
                {
                    require(program->hasEvent(eventId), "Stone statue source event is absent");
                }
                const std::string overlay = makeStoneStatueOverlay(*statueHint);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+stone-statues", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Stone statue rescue Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Stone statue overlay changed source registration");
                for (uint16_t eventId = 132; eventId <= 136; ++eventId)
                {
                    require(overlaid->getHint(eventId) == statueHint, "Stone statue source hint changed");
                }
                const std::filesystem::path overlayPath = "worlds/mm8/events/maps/out07_mmmerge_stone_statues.lua";
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && map.world == "mm8" && selectedReentries.contains(stem))
            {
                const Mm8ReentryProfile &profile = *selectedReentries.at(stem);
                std::array<std::string, 4> hints;
                if (stem == "pbp")
                {
                    for (size_t index = 0; index < hints.size(); ++index)
                    {
                        const std::optional<std::string> hint = strings.get(51 + index);
                        require(hint && !hint->empty(), "Prison source hint is absent");
                        hints[index] = *hint;
                    }
                }
                const std::string overlay = makeMm8ReentryOverlay(profile, hints);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+reentry", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "MM8 reentry Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "MM8 reentry overlay changed source registration");
                if (stem == "pbp")
                {
                    for (size_t index = 0; index < hints.size(); ++index)
                        require(overlaid->getHint(502 + index) == hints[index], "Prison source hint differs");
                }
                else require(overlaid->getHint(504) == program->getHint(504), "Crystal entrance hint differs");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm8/events/maps")
                    / (stem + "_mmmerge_reentry.lua");
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && selectedTempleBottle && map.world == "mm7" && stem == templeBottle->mapStem)
            {
                require(program->hasEvent(501), "Temple bottle source exit is absent");
                const std::string overlay = makeTempleBottleOverlay();
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+bottle-return", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Temple bottle return Lua rejected: " + error);
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && overlaid->getHint(501) == program->getHint(501),
                    "Temple bottle overlay changed source registration or hint");
                const std::filesystem::path overlayPath = "worlds/mm7/events/maps/7nwc_mmmerge_bottle_return.lua";
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && emeraldTravel && map.world == "mm7" && stem == emeraldTravel->mapStem)
            {
                require(!program->hasEvent(65489) && lookups.mapNamesByFile.contains("7out01.odm"),
                    "Emerald travel hook conflicts with a source event or destination is absent");
                bool sourceHousePresent = false;
                for (const std::vector<std::string> &row : tableRows(tables / "house_data.txt"))
                {
                    if (rowId(row) == 486)
                    {
                        sourceHousePresent = row.size() > 2 && row[1] == "8" && row[2] == "Boats";
                    }
                }
                require(sourceHousePresent, "Emerald travel source House486 is not Boats8");
                const std::string overlay = makeEmeraldIslandTravelOverlay(lookups.mapNamesByFile.at("7out01.odm"));
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+emerald-travel", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Emerald travel Lua rejected: " + error);
                std::vector<uint16_t> expectedIds = program->eventIds();
                expectedIds.push_back(65489);
                std::sort(expectedIds.begin(), expectedIds.end());
                std::vector<uint16_t> expectedOnLoad = program->onLoadEventIds();
                expectedOnLoad.push_back(65489);
                require(overlaid->eventIds() == expectedIds && overlaid->onLoadEventIds() == expectedOnLoad
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Emerald travel overlay changed unrelated source registration");
                const std::filesystem::path overlayPath = "worlds/mm7/events/maps/7out03_mmmerge_emerald_travel.lua";
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && freeHavenTemple && map.world == "mm6" && stem == freeHavenTemple->mapStem)
            {
                const std::optional<std::string> repairedText = strings.get(29);
                const std::optional<std::string> chaliceText = strings.get(30);
                require(program->hasEvent(19) && !program->hasEvent(140) && repairedText && chaliceText
                    && lookups.houseNames.contains(326) && lookups.houseNames.contains(1607),
                    "Missing Free Haven temple source event, messages or inn house");
                const std::string overlay = makeFreeHavenTempleOverlay(
                    lookups.houseNames.at(326), *repairedText, *chaliceText, lookups.houseNames.at(1607));
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+free-haven-temple", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Free Haven temple Lua rejected: " + error);
                std::vector<uint16_t> expectedIds = program->eventIds();
                expectedIds.push_back(140);
                std::sort(expectedIds.begin(), expectedIds.end());
                require(overlaid->eventIds() == expectedIds
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Free Haven temple overlay changed unrelated source registration");
                const std::filesystem::path overlayPath = "worlds/mm6/events/maps/outc2_mmmerge_temple.lua";
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && freeHavenCouncil && map.world == "mm6" && stem == "outc2")
            {
                require(program->hasEvent(49), "Missing Council source door49");
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + freeHavenCouncil->mapLuaSource,
                    "@outc2.lua+council", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Council entrance Lua rejected: " + error);
                std::vector<uint16_t> expectedIds = program->eventIds();
                if (std::find(expectedIds.begin(), expectedIds.end(), 50) == expectedIds.end())
                    expectedIds.push_back(50);
                std::sort(expectedIds.begin(), expectedIds.end());
                require(overlaid->hasEvent(49) && overlaid->hasEvent(50) && overlaid->eventIds() == expectedIds
                    && overlaid->getHint(49) == program->getHint(49) && overlaid->getHint(50) == program->getHint(50)
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Council entrance overlay changed unrelated registration or door hints");
                writeConvertedResource(output / "worlds/mm6/events/maps/outc2_mmmerge_council.lua",
                    {freeHavenCouncil->mapLuaSource.begin(), freeHavenCouncil->mapLuaSource.end()});
            }
            if (!global && artifactMessenger && map.world == "mm7" && stem == artifactMessenger->mapStem)
            {
                require(program->hasEvent(401), "Missing original artifact messenger event401");
                const auto overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + artifactMessenger->luaSource,
                    "@7out04.lua+messenger", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Artifact messenger Lua rejected: " + error);
                std::vector<uint16_t> expectedOnLoad = program->onLoadEventIds();
                std::erase(expectedOnLoad, 401);
                expectedOnLoad.push_back(401);
                require(overlaid->eventIds() == program->eventIds() && overlaid->onLoadEventIds() == expectedOnLoad
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Artifact messenger overlay changed unrelated source registration");
                writeConvertedResource(output / "worlds/mm7/events/maps/7out04_mmmerge_messenger.lua",
                    {artifactMessenger->luaSource.begin(), artifactMessenger->luaSource.end()});
            }
            if (!global && stoneCity && map.world == "mm7" && stem == stoneCity->mapStem)
            {
                const std::optional<std::string> entranceHint = strings.get(20);
                const std::optional<std::string> lockedText = strings.get(19);
                require(entranceHint && lockedText && program->hasEvent(416),
                    "Missing Stone City entrance event or localized source STR slots");
                const std::string overlay = makeStoneCityOverlay(*entranceHint, *lockedText);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+stone-city", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Stone City entrance Lua rejected: " + error);
                std::vector<uint16_t> expectedIds = program->eventIds();
                expectedIds.push_back(StoneCityNpcExitHookEventId);
                std::sort(expectedIds.begin(), expectedIds.end());
                require(overlaid->eventIds() == expectedIds
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds()
                    && overlaid->getHint(416) == entranceHint,
                    "Stone City entrance overlay changed source registration or localized hint");
                const std::filesystem::path overlayPath = "worlds/mm7/events/maps/7d24_mmmerge_stone_city.lua";
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && lambentCastle && map.world == "mm7" && stem == lambentCastle->mapStem)
            {
                const std::optional<std::string> entranceHint = strings.get(20);
                const std::optional<std::string> lockedText = strings.get(21);
                require(entranceHint && lockedText, "Missing Castle Lambent entrance source STR slots");
                require(program->hasEvent(416), "Castle Lambent source entrance event 416 is absent");
                const std::string overlay = makeLambentCastleOverlay(*entranceHint, *lockedText);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+lambent-castle", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Castle Lambent entrance Lua rejected: " + error);
                require(overlaid->hasEvent(416) && overlaid->eventIds() == program->eventIds(),
                    "Castle Lambent entrance overlay changed source event registration");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                    / (stem + "_mmmerge_lambent_entry.lua");
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && gloamingCastle && map.world == "mm7" && stem == "d03")
            {
                const std::optional<std::string> entranceHint = strings.get(20);
                const std::optional<std::string> lockedText = strings.get(21);
                require(entranceHint && lockedText, "Missing Castle Gloaming entrance source STR slots");
                require(program->hasEvent(5), "Castle Gloaming source entrance event 5 is absent");
                const std::string overlay = makeGloamingCastleOverlay(*entranceHint, *lockedText);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+gloaming-castle", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Castle Gloaming entrance Lua rejected: " + error);
                require(overlaid->hasEvent(5) && overlaid->eventIds() == program->eventIds(),
                    "Castle Gloaming entrance overlay changed source event registration");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                    / (stem + "_mmmerge_gloaming_entry.lua");
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && skeletonTransformer && map.world == "mm8" && stem == "d19")
            {
                const std::optional<std::string> entranceHint = strings.get(2);
                require(entranceHint.has_value(), "Missing skeleton transformer entrance source STR slot 2");
                require(program->hasEvent(15) && program->hasEvent(131),
                    "Skeleton transformer source events 15/131 are absent");
                require(lua.find("SetValue(MapVar(21), 15)") != std::string::npos,
                    "Skeleton transformer generated puzzle counter layout changed; review source Map.Vars[19]");
                const std::string overlay = makeSkeletonTransformerOverlay(*entranceHint);
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay,
                    "@" + evtName + "+skeleton-transformer", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Skeleton transformer Lua rejected: " + error);
                require(overlaid->hasEvent(15) && overlaid->hasEvent(131)
                    && overlaid->eventIds() == program->eventIds(),
                    "Skeleton transformer overlay changed source event registration");
                require(overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Skeleton transformer overlay changed source lifecycle hooks");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm8/events/maps")
                    / (stem + "_mmmerge_skeleton_transformer.lua");
                writeConvertedResource(output / overlayPath, {overlay.begin(), overlay.end()});
            }
            if (!global && smugglersCove && map.world == "mm8" && stem == smugglersCove->mapStem)
            {
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + smugglersCove->luaSource,
                    "@" + evtName + "+smugglers-cove", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Smugglers Cove Lua rejected: " + error);
                require(!overlaid->hasEvent(1) && overlaid->hasEvent(900) && overlaid->hasEvent(901),
                    "Smugglers Cove entry/kill hooks were not registered");
                const std::filesystem::path overlayPath = std::filesystem::path("worlds") / map.world
                    / "events/maps" / (stem + "_mmmerge_hostility.lua");
                writeConvertedResource(output / overlayPath,
                    {smugglersCove->luaSource.begin(), smugglersCove->luaSource.end()});
            }
            if (!global && archibald && map.world == "mm6" && stem == "outd3")
            {
                require(program->hasEvent(42), "Archibald source entrance event 42 is absent");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + archibald->luaSource,
                    "@" + evtName + "+archibald", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "Archibald entrance Lua rejected: " + error);
                require(program->getHint(42).has_value() && overlaid->getHint(42) == program->getHint(42),
                    "Archibald entrance overlay lost the localized door hint");
                require(overlaid->eventIds() == program->eventIds()
                    && overlaid->onLoadEventIds() == program->onLoadEventIds()
                    && overlaid->afterLoadEventIds() == program->afterLoadEventIds()
                    && overlaid->onLeaveEventIds() == program->onLeaveEventIds(),
                    "Archibald entrance overlay changed source event/lifecycle registration");
                const std::filesystem::path overlayPath = "worlds/mm6/events/maps/outd3_mmmerge_archibald.lua";
                writeConvertedResource(output / overlayPath,
                    {archibald->luaSource.begin(), archibald->luaSource.end()});
            }
            if (!global && mm6Hive && map.world == "mm6" && stem == mm6Hive->mapStem)
            {
                for (uint16_t id : {65480, 65481, 65482, 65483})
                {
                    require(!program->hasEvent(id), "MM6 Hive hook ID conflicts with exported EVT");
                }
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + mm6Hive->luaSource,
                    "@" + evtName + "+mm6-hive-ending", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "MM6 Hive ending Lua rejected: " + error);
                for (uint16_t id : {60, 65480, 65481, 65482, 65483})
                {
                    require(overlaid->hasEvent(id), "MM6 Hive ending handler was not registered");
                }
                writeConvertedResource(output / "worlds/mm6/events/maps/hive_mmmerge_ending.lua",
                    {mm6Hive->luaSource.begin(), mm6Hive->luaSource.end()});
            }
            const auto shoalsOverlay = selectedShoalsOverlays.find(stem);
            if (map.world == "mm7" && shoalsOverlay != selectedShoalsOverlays.end())
            {
                const Mm7ShoalsMapOverlay &overlay = *shoalsOverlay->second;
                const uint16_t hookId = stem == "out14" ? 65470 : stem == "7d23" ? 65471 : 65472;
                require(!program->hasEvent(hookId), "MM7 Shoals hook ID conflicts with exported EVT");
                const std::optional<Game::ScriptedEventProgram> overlaid = Game::ScriptedEventProgram::loadFromLuaText(
                    std::string(EventSupportSource) + "\n" + lua + "\n" + overlay.luaSource,
                    "@" + evtName + "+mm7-shoals", Game::ScriptedEventScope::Map, error);
                require(overlaid.has_value(), "MM7 Shoals route Lua rejected: " + error);
                require(overlaid->hasEvent(hookId), "MM7 Shoals route hook was not registered");
                if (stem == "out14")
                {
                    require(std::find(overlaid->mapBoundaryHookEventIds().begin(),
                        overlaid->mapBoundaryHookEventIds().end(), hookId) != overlaid->mapBoundaryHookEventIds().end(),
                        "Avlee map boundary hook was not registered");
                }
                if (stem == "7d23")
                {
                    require(overlaid->hasEvent(501), "Lincoln return event was not registered");
                }
                const std::filesystem::path overlayPath = std::filesystem::path("worlds/mm7/events/maps")
                    / (stem + "_mmmerge_shoals.lua");
                writeConvertedResource(output / overlayPath, {overlay.luaSource.begin(), overlay.luaSource.end()});
            }
            for (const Game::EvtEvent &event : evt.getEvents())
            {
                require(program->hasEvent(event.eventId) || program->isHintOnlyEvent(event.eventId),
                    "Export dropped event " + std::to_string(event.eventId) + " in " + evtName);
            }
            const std::filesystem::path path = global ? std::filesystem::path("engine/events/Global.lua")
                : std::filesystem::path("worlds") / map.world / "events/maps" / (stem + ".lua");
            writeConvertedResource(output / path, {lua.begin(), lua.end()});
            if (!first)
            {
                manifest << ',';
            }
            first = false;
            manifest << "{\"path\":" << std::quoted(path.generic_string()) << ",\"evt_archive\":"
                << std::quoted(members.at(evtName).archive.filename().string()) << ",\"str_archive\":"
                << std::quoted(members.contains(strName) ? members.at(strName).archive.filename().string() : "")
                << ",\"source_events\":" << evt.getEvents().size() << ",\"source_instructions\":"
                << evt.getInstructionCount() << ",\"string_slots\":" << strings.getEntries().size()
                << ",\"restored_glyphs\":" << glyphs << ",\"lua_load_passed\":true}";
        }
        manifest << "]}\n";
        const std::string report = manifest.str();
        writeConvertedResource(output / "event_conversion.json", {report.begin(), report.end()});
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
