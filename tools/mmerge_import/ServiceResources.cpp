#include "ServiceResources.h"
#include "BreachAlvarCompanionResources.h"
#include "LodResources.h"
#include "PostQuestVisitResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/ChestTable.h"
#include "game/tables/HouseTable.h"
#include "game/tables/NpcDialogTable.h"
#include "game/audio/SoundCatalog.h"
#include "game/gameplay/HouseInteraction.h"
#include "game/StringUtils.h"

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

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
using IndexedRows = std::map<uint32_t, Row>;

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t integer(const std::string &text)
{
    uint32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size() && value <= 65535,
        "Invalid service table integer: " + text);
    return value;
}

std::filesystem::path sourcePath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Source escapes import root: " + name);
    return path;
}

Rows parseText(const std::string &text)
{
    const std::optional<Engine::TextTable> table =
        Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid service text table");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (!row.empty() && !(row.size() == 1 && row[0].empty()))
        {
            rows.push_back(row);
        }
    }
    require(!rows.empty(), "Empty service text table");
    return rows;
}

Rows parse(const std::vector<uint8_t> &bytes, bool chinese, size_t &glyphs)
{
    return parseText(decodeLegacyText(bytes, chinese, glyphs));
}

Rows looseTable(const std::filesystem::path &root, const std::string &name, bool chinese, size_t &glyphs)
{
    const std::filesystem::path path = sourcePath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size <= 16 * 1024 * 1024, "Service table exceeds size limit");
    std::ifstream stream(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read source table: " + name);
    return parse(bytes, chinese, glyphs);
}

IndexedRows indexed(const Rows &rows, size_t columns)
{
    IndexedRows result;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(row.size() == columns, "Wrong service table column count at row " + std::to_string(index + 1));
        require(result.emplace(integer(row[0]), row).second, "Duplicate service table ID: " + row[0]);
    }
    require(!result.empty(), "No service table entries");
    return result;
}

bool verifiedHouseSource(const std::filesystem::path &root, std::string_view relativePath,
    std::string_view expectedSha256)
{
    const std::string name(relativePath);
    if (!std::filesystem::exists(root / name))
    {
        return false;
    }
    const std::filesystem::path path = sourcePath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 256 * 1024, "Invalid house source script size: " + name);
    std::string text(size, '\0');
    std::ifstream input(path, std::ios::binary);
    require(bool(input.read(text.data(), std::streamsize(size))), "Cannot read house source script: " + name);
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(text.data(), CC_LONG(text.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    require(digest.str() == expectedSha256, "Unsupported house source script fingerprint: " + name);
    return true;
}

bool verifiedBreachAlvarHouseSource(const std::filesystem::path &root)
{
    return verifiedHouseSource(root, BreachAlvarMapSourcePath, BreachAlvarMapSourceSha256);
}

void applyClankerHouseExitRule(Rows &houses)
{
    Row *pHouse = nullptr;
    for (Row &row : houses)
    {
        if (!row.empty() && row[0] == "395")
        {
            require(pHouse == nullptr && row.size() == 24, "Invalid Clanker house395 source row");
            pHouse = &row;
        }
    }
    require(pHouse != nullptr, "Missing Clanker house395 source row");
    const bool original = (*pHouse)[20] == "0" && (*pHouse)[21] == "0" && (*pHouse)[22] == "0";
    const bool projected = (*pHouse)[20] == "1" && (*pHouse)[21] == "86" && (*pHouse)[22] == "710";
    require(original || projected, "Unexpected Clanker house395 exit identity");
    (*pHouse)[20] = "1";
    (*pHouse)[21] = "86";
    (*pHouse)[22] = "710";
}

void appendClankerHouseExitManifest(std::ostream &manifest, bool enabled)
{
    manifest << ",\"clanker_postquest_exit\":";
    if (!enabled)
    {
        manifest << "null";
        return;
    }
    manifest << "{\"house_id\":395,\"destination_map_id\":86,\"picture_slot\":1,\"required_qbit\":710,"
        "\"source\":\"Scripts/Maps/7out04.lua\",\"source_sha256\":\"" << ClankerPostQuestSourceSha256 << "\"}";
}

const BreachAlvarHousePresentationRule *breachAlvarHouseRule(uint32_t houseId)
{
    const auto found = std::find_if(BreachAlvarHousePresentationRules.begin(), BreachAlvarHousePresentationRules.end(),
        [houseId](const BreachAlvarHousePresentationRule &rule) { return rule.houseId == houseId; });
    return found != BreachAlvarHousePresentationRules.end() ? &*found : nullptr;
}

const Row &numericRow(const Rows &rows, uint32_t id, size_t columns)
{
    const std::string key = std::to_string(id);
    const Row *pResult = nullptr;
    for (const Row &row : rows)
    {
        if (!row.empty() && row[0] == key)
        {
            require(row.size() == columns && pResult == nullptr, "Invalid Breach Alvar name source row: " + key);
            pResult = &row;
        }
    }
    require(pResult != nullptr, "Missing Breach Alvar name source row: " + key);
    return *pResult;
}

std::string breachAlvarSourceMapName(const std::filesystem::path &root, uint32_t mapId,
    bool chinese, size_t &glyphs)
{
    const Rows maps = parse(readLodText(sourcePath(root, "Data/mmmerge.T.lod"), "mapstats.txt"), false, glyphs);
    const Row &source = numericRow(maps, mapId, 34);
    require(!source[1].empty() && !source[2].empty(), "Empty Breach Alvar map name identity");
    if (chinese)
    {
        const Rows translated = parse(readLodText(sourcePath(root, "Data/10 LocZHCN.EnglishT.lod"),
            "mapstats.txt"), true, glyphs);
        const Row &localized = numericRow(translated, mapId, 34);
        // Same ID and map-file check as the shared core map-name translation.
        if (Game::toLowerCopy(source[2]) == Game::toLowerCopy(localized[2]))
        {
            require(!localized[1].empty(), "Breach Alvar map translation removes the name");
            return localized[1];
        }
    }
    return source[1];
}

void applyBreachAlvarHouseRules(Rows &houses, const std::filesystem::path &root, bool chinese, size_t &glyphs)
{
    for (const BreachAlvarHousePresentationRule &rule : BreachAlvarHousePresentationRules)
    {
        const std::string name = rule.nameSource == BreachAlvarHouseNameSource::House
            ? numericRow(houses, rule.nameSourceId, 24)[5]
            : breachAlvarSourceMapName(root, rule.nameSourceId, chinese, glyphs);
        require(!name.empty(), "Empty Breach Alvar source house name");
        const auto target = std::find_if(houses.begin(), houses.end(), [&](const Row &row)
        {
            return !row.empty() && row[0] == std::to_string(rule.houseId);
        });
        require(target != houses.end(), "Missing Breach Alvar destination house");
        if (rule.pictureId)
        {
            (*target)[4] = std::to_string(*rule.pictureId);
        }
        (*target)[5] = name;
    }
}

void writeText(const std::filesystem::path &path, const std::string &text)
{
    writeConvertedResource(path, std::vector<uint8_t>(text.begin(), text.end()));
}

// Static data in the owned MM8 program, indexed by ChestPicture, not ImageIndex.
// The original background draws at (8, 23); the shared engine stores grid
// coordinates relative to that background. See SERVICE_IMPORT_PROFILE.md.
constexpr std::array<int, 8> ChestScreenX = {42, 18, 18, 42, 42, 42, 18, 42};
constexpr std::array<int, 8> ChestScreenY = {49, 45, 45, 49, 49, 49, 45, 49};
}

void convertServiceResources(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    const std::filesystem::path relative = output.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
        "Service output must be outside input root");
    require(!std::filesystem::exists(output), "Service destination already exists");
    size_t glyphs = 0;
    const Rows sourceChests = looseTable(root, "Data/Tables/Chest.txt", false, glyphs);
    require(sourceChests.front() == Row{"#", "Name", "Width", "Height", "ImageIndex"},
        "Unrecognized chest header");
    const IndexedRows chestEntries = indexed(sourceChests, 5);
    require(chestEntries.size() == ChestScreenX.size(), "Unsupported chest profile size");
    Rows chests;
    std::vector<std::string> chestImages;
    for (const auto &[id, sourceRow] : chestEntries)
    {
        require(id == chests.size() && !sourceRow[1].empty() && integer(sourceRow[2]) == 14
            && integer(sourceRow[3]) == 10 && integer(sourceRow[4]) == id + 1,
            "Unsupported chest profile or reordered image at ID " + std::to_string(id));
        const std::string image = "chest0" + std::to_string(id + 1);
        Row row = sourceRow;
        row.insert(row.end(), {image, std::to_string(ChestScreenX[id] - 8),
            std::to_string(ChestScreenY[id] - 23), "9", "9"});
        chests.push_back(row);
        chestImages.push_back(image);
    }
    Game::ChestTable chestCheck;
    require(chestCheck.loadRows(chests) && chestCheck.getEntries().size() == chests.size(),
        "Chest typed loader rejected converted rows");
    for (uint16_t id = 0; id < chests.size(); ++id)
    {
        const Game::ChestEntry *pChest = chestCheck.get(id);
        require(pChest && pChest->width == 14 && pChest->height == 10
            && pChest->textureId == id + 1 && pChest->textureName == chestImages[id]
            && pChest->gridOffsetX + 8 == ChestScreenX[id] && pChest->gridOffsetY + 23 == ChestScreenY[id]
            && pChest->gridWidth == 9 && pChest->gridHeight == 9, "Chest coordinate/identity roundtrip failed");
    }

    const std::filesystem::path baseArchive = sourcePath(root, "Data/mmmerge.T.lod");
    Rows sourceHouses = parse(readLodText(baseArchive, "2devents.txt"), false, glyphs);
    require(sourceHouses.size() >= 3 && sourceHouses[1].size() == 24 && sourceHouses[1][0] == "#"
        && sourceHouses[1][2] == "Type" && sourceHouses[1][5] == "Name", "Unrecognized house source header");
    sourceHouses.erase(sourceHouses.begin());
    const IndexedRows baseHouses = indexed(sourceHouses, 24);
    const bool breachAlvarHouses = verifiedBreachAlvarHouseSource(root);
    const bool clankerHouseExit = verifiedHouseSource(root, ClankerPostQuestSourcePath, ClankerPostQuestSourceSha256);
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishT.lod");
    IndexedRows localizedHouses;
    if (chinese)
    {
        const Rows localized = looseTable(root, "Data/Text localization/ZHCN_2DEvents.txt", true, glyphs);
        require(localized.front() == Row{"#", "Name", "Proprieter Name", "Title", "Enter text"},
            "Unrecognized house localization header");
        localizedHouses = indexed(localized, 5);
    }
    Rows houses;
    std::vector<uint32_t> inactiveHouses;
    std::vector<uint32_t> missingHouseTranslations;
    std::vector<uint32_t> extraHouseTranslations;
    std::vector<uint32_t> ignoredInactiveTranslations;
    size_t translatedHouses = 0;
    for (const auto &[id, sourceRow] : baseHouses)
    {
        require(id > 0, "Zero house ID is not a venue");
        if (sourceRow[5].empty())
        {
            // The normal loader treats unnamed source slots as inactive. Old
            // localization must not create new houses or renumber later entries.
            inactiveHouses.push_back(id);
            if (localizedHouses.contains(id) && !localizedHouses.at(id)[1].empty())
            {
                ignoredInactiveTranslations.push_back(id);
            }
            continue;
        }
        Row row = sourceRow;
        if (localizedHouses.contains(id))
        {
            const Row &localized = localizedHouses.at(id);
            require(!localized[1].empty(), "House translation removes an active name");
            for (size_t column = 1; column <= 4; ++column)
            {
                row[column == 4 ? 23 : column + 4] = localized[column];
            }
            ++translatedHouses;
        }
        else if (chinese)
        {
            missingHouseTranslations.push_back(id);
        }
        houses.push_back(row);
    }
    for (const auto &[id, row] : localizedHouses)
    {
        if (!baseHouses.contains(id))
        {
            extraHouseTranslations.push_back(id);
        }
    }
    if (breachAlvarHouses)
    {
        applyBreachAlvarHouseRules(houses, root, chinese, glyphs);
    }
    if (clankerHouseExit)
    {
        applyClankerHouseExitRule(houses);
    }
    Game::HouseTable baseHouseCheck;
    Game::HouseTable houseCheck;
    require(baseHouseCheck.loadFromRows(sourceHouses) && houseCheck.loadFromRows(houses)
        && houseCheck.entries().size() == houses.size(),
        "House typed loader changed coverage");
    for (const Row &row : houses)
    {
        const Game::HouseEntry *pHouse = houseCheck.get(integer(row[0]));
        const Game::HouseEntry *pBase = baseHouseCheck.get(integer(row[0]));
        const bool clankerExit = clankerHouseExit && integer(row[0]) == 395;
        require(pHouse && pBase && pHouse->name == row[5] && pHouse->proprietorName == row[6]
            && pHouse->proprietorTitle == row[7] && pHouse->enterText == row[23]
            && pHouse->type == pBase->type && pHouse->typeIndex == pBase->typeIndex
            && pHouse->mapId == pBase->mapId && pHouse->videoName == pBase->videoName
            && pHouse->proprietorPictureId == pBase->proprietorPictureId
            && pHouse->priceMultiplier == pBase->priceMultiplier
            && pHouse->skillPriceMultiplier == pBase->skillPriceMultiplier
            && pHouse->templeHealingTier == pBase->templeHealingTier && pHouse->templeBehavior == pBase->templeBehavior
            && pHouse->stockRefreshDays == pBase->stockRefreshDays && pHouse->openHour == pBase->openHour
            && pHouse->closeHour == pBase->closeHour && pHouse->offeredSkills == pBase->offeredSkills
            && pHouse->rawExtraExitPictureIndex == (clankerExit ? 1u : pBase->rawExtraExitPictureIndex)
            && pHouse->rawExtraExitMapId == (clankerExit ? 86u : pBase->rawExtraExitMapId)
            && pHouse->rawExtraExitRestriction == (clankerExit ? 710 : pBase->rawExtraExitRestriction),
            "House localization changed identity/mechanics");
    }

    std::vector<std::filesystem::path> imageArchives;
    std::set<std::string> available;
    for (const std::string &name : {"Data/icons.lod", "Data/mm8.D.lod", "Data/mmmerge.D.lod",
        "Data/10 LocZHCN.EnglishD.lod"})
    {
        if (std::filesystem::exists(root / name))
        {
            const std::filesystem::path archive = sourcePath(root, name);
            const std::vector<std::string> members = listLodMembers(archive);
            available.insert(members.begin(), members.end());
            imageArchives.push_back(archive);
        }
    }
    std::vector<std::string> presentImages;
    std::vector<std::string> missingImages;
    for (const std::string &image : chestImages)
    {
        (available.contains(image) ? presentImages : missingImages).push_back(image);
    }
    require(std::filesystem::create_directory(output), "Cannot create fresh service destination");
    try
    {
        const std::vector<ConvertedLodImage> images = presentImages.empty() ? std::vector<ConvertedLodImage>{}
            : convertLodImages(imageArchives, presentImages, output / "engine/icons");
        std::ostringstream manifest;
        manifest << "{\"schema\":1,\"stage\":\"shared-service-tables\",\"runtime_ready\":false,"
            << "\"typed_validation\":true,\"chest_profile\":\"mm8-8-chests-background-relative\","
            << "\"chests\":" << chests.size() << ",\"source_houses\":" << baseHouses.size()
            << ",\"active_houses\":" << houses.size() << ",\"translated_houses\":" << translatedHouses
            << ",\"inactive_house_ids\":[";
        for (size_t index = 0; index < inactiveHouses.size(); ++index)
        {
            manifest << (index == 0 ? "" : ",") << inactiveHouses[index];
        }
        manifest << "]";
        for (const auto &[name, ids] : std::map<std::string, std::vector<uint32_t>>{
            {"missing_house_translation_ids", missingHouseTranslations},
            {"extra_house_translation_ids", extraHouseTranslations},
            {"ignored_inactive_translation_ids", ignoredInactiveTranslations}})
        {
            manifest << "," << std::quoted(name) << ":[";
            for (size_t index = 0; index < ids.size(); ++index)
            {
                manifest << (index == 0 ? "" : ",") << ids[index];
            }
            manifest << "]";
        }
        manifest << ",\"breach_alvar_source_houses\":[";
        if (breachAlvarHouses)
        {
            for (size_t index = 0; index < BreachAlvarHousePresentationRules.size(); ++index)
            {
                manifest << (index == 0 ? "" : ",") << BreachAlvarHousePresentationRules[index].houseId;
            }
        }
        manifest << "],\"missing_chest_images\":[";
        for (size_t index = 0; index < missingImages.size(); ++index)
        {
            manifest << (index == 0 ? "" : ",") << std::quoted(missingImages[index]);
        }
        manifest << "],\"chest_images\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            const ConvertedLodImage &image = images[index];
            const uint32_t id = integer(image.name.substr(5, 2)) - 1;
            require(id < chests.size() && image.width >= ChestScreenX[id] - 8 + 9 * 32
                && image.height >= ChestScreenY[id] - 23 + 9 * 32, "Chest grid exceeds source background image");
            manifest << (index == 0 ? "" : ",") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive)
                << ",\"width\":" << image.width << ",\"height\":" << image.height << "}";
        }
        manifest << "]";
        appendClankerHouseExitManifest(manifest, clankerHouseExit);
        manifest << ",\"house_animations_ready\":false,\"house_rules_ready\":false,"
            << "\"interactive_chests_ready\":false,\"full_gameplay_loop\":false,"
            << "\"restored_glyphs\":" << glyphs << "}\n";
        writeText(output / "engine/data_tables/chest_data.txt", serializeTextRows(chests));
        writeText(output / "engine/data_tables/house_data.txt", serializeTextRows(houses));
        writeText(output / "service_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}

void convertHousePresentationResources(const std::filesystem::path &source,
    const std::filesystem::path &convertedBase, const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path base = std::filesystem::canonical(convertedBase);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, base})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "House presentation output must be outside both inputs");
    }
    require(!std::filesystem::exists(output), "House presentation destination already exists");
    const auto convertedRows = [&](const char *pName)
    {
        const std::filesystem::path path = base / "engine/data_tables" / pName;
        require(std::filesystem::file_size(path) <= 16 * 1024 * 1024, "Converted dependency exceeds size limit");
        std::ifstream input(path, std::ios::binary);
        require(bool(input), std::string("Cannot read converted dependency: ") + pName);
        return parseText({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
    };

    size_t unusedGlyphs = 0;
    const Rows movieRows = looseTable(root, "Data/Tables/House Movies.txt", false, unusedGlyphs);
    require(movieRows.front() == Row{"#", "FileName", "NPCPic", "HouseType", "Sounds"},
        "Unrecognized House Movies header");
    const IndexedRows movies = indexed(movieRows, 5);
    for (const auto &[id, row] : movies)
    {
        require(row[1] != "." && row[1] != ".." && row[1].find_first_of("/\\") == std::string::npos
            && std::none_of(row[1].begin(), row[1].end(), [](unsigned char value) { return value < 32; }),
            "Unsafe house movie stem at ID " + std::to_string(id));
        require(!row[3].empty() && integer(row[4]) <= 255, "Invalid house movie type or sound group");
        integer(row[2]);
    }

    const std::filesystem::path archive = sourcePath(root, "Data/mmmerge.T.lod");
    Rows originalHouseRows = parse(readLodText(archive, "2devents.txt"), false, unusedGlyphs);
    require(originalHouseRows.size() > 2 && originalHouseRows[1].size() == 24
        && originalHouseRows[1][0] == "#", "Unrecognized original house table");
    originalHouseRows.erase(originalHouseRows.begin());
    const IndexedRows originalHouses = indexed(originalHouseRows, 24);
    const bool breachAlvarHouses = verifiedBreachAlvarHouseSource(root);
    const bool clankerHouseExit = verifiedHouseSource(root, ClankerPostQuestSourcePath, ClankerPostQuestSourceSha256);
    HouseTable originalHouseTable;
    HouseTable houseTable;
    Rows houseRows = convertedRows("house_data.txt");
    if (clankerHouseExit)
    {
        applyClankerHouseExitRule(houseRows);
    }
    std::set<uint32_t> convertedHouseIds;
    for (const Row &row : houseRows)
    {
        require(row.size() >= 24, "Incomplete converted house row");
        if (row[0].starts_with('#'))
        {
            continue;
        }
        const uint32_t id = integer(row[0]);
        const BreachAlvarHousePresentationRule *pRule = breachAlvarHouses ? breachAlvarHouseRule(id) : nullptr;
        require(convertedHouseIds.insert(id).second && originalHouses.contains(id)
            && integer(row[4]) == (pRule != nullptr && pRule->pictureId
                ? *pRule->pictureId : integer(originalHouses.at(id)[4])),
            "Converted house animation identity changed");
    }
    require(originalHouseTable.loadFromRows(originalHouseRows) && houseTable.loadFromRows(houseRows)
        && originalHouseTable.entries().size() == houseTable.entries().size(), "House dependency coverage changed");

    NpcDialogTable originalNpcs;
    NpcDialogTable npcs;
    const Rows originalNpcRows = parse(readLodText(archive, "npcdata.txt"), false, unusedGlyphs);
    require(originalNpcs.loadNpcRows(originalNpcRows) && npcs.loadNpcRows(convertedRows("npc.txt"))
        && originalNpcs.entries().size() == npcs.entries().size(), "NPC dependency coverage changed");
    std::map<uint32_t, std::vector<uint32_t>> residents;
    std::vector<uint32_t> inactiveHouseNpcs;
    size_t residentCount = 0;
    for (const NpcEntry &original : originalNpcs.entries())
    {
        const NpcEntry *pNpc = npcs.getNpc(original.id);
        require(pNpc && pNpc->houseId == original.houseId && pNpc->pictureId == original.pictureId
            && pNpc->topicIds == original.topicIds, "Converted NPC identity changed");
        if (original.houseId == 0)
        {
            continue;
        }
        if (houseTable.get(original.houseId) == nullptr)
        {
            require(originalHouses.contains(original.houseId) && originalHouses.at(original.houseId)[5].empty(),
                "NPC references an unknown house");
            inactiveHouseNpcs.push_back(original.id);
            continue;
        }
        residents[original.houseId].push_back(original.id);
        ++residentCount;
    }
    for (auto &[id, ids] : residents)
    {
        std::sort(ids.begin(), ids.end());
    }

    SoundCatalog sounds;
    std::string error;
    const Rows soundRows = convertedRows("sounds.txt");
    require(soundRows.front() == Row{"#SoundCatalogMode", "MM8OrderedV1"}
        && sounds.loadFromScopedRows(soundRows, {}, error), "House sounds require the original merged sound catalog");
    Rows animations = {{"# House Id", "Animation Id", "Building Name", "NPC List", "Video Name", "Enter Sound",
        "Room Sound Id", "House Sound Base Id", "NPC Picture Id", "House Movie Type"}};
    std::set<uint32_t> missingGreetingIds;
    std::set<std::string> videoStems;
    std::set<uint32_t> portraitIds;
    for (const auto &[id, originalRow] : originalHouses)
    {
        if (originalRow[5].empty())
        {
            continue;
        }
        const HouseEntry *pHouse = houseTable.get(id);
        const HouseEntry *pOriginal = originalHouseTable.get(id);
        require(pHouse && pOriginal && pHouse->mapId == pOriginal->mapId && pHouse->type == pOriginal->type
            && pHouse->videoName == pOriginal->videoName
            && pHouse->proprietorPictureId == pOriginal->proprietorPictureId, "House dependency identity changed");
        const BreachAlvarHousePresentationRule *pRule = breachAlvarHouses ? breachAlvarHouseRule(id) : nullptr;
        const uint32_t animationId = pRule != nullptr && pRule->pictureId
            ? *pRule->pictureId : integer(originalRow[4]);
        require(movies.contains(animationId), "Missing house movie ID: " + std::to_string(animationId));
        const Row &movie = movies.at(animationId);
        const uint32_t group = integer(movie[4]);
        // Supplied MiscTweaks.lua patches the original formula to (group + 3000) * 10.
        // The native sound catalog retains these merged IDs, so no name-based remapping is needed.
        const uint32_t soundBaseId = group == 0 ? 0 : 30000 + group * 10;
        if (soundBaseId != 0 && sounds.findById(soundBaseId + 1) == nullptr)
        {
            missingGreetingIds.insert(soundBaseId + 1);
        }
        std::ostringstream npcList;
        for (uint32_t npcId : residents[id])
        {
            npcList << (npcList.tellp() == 0 ? "" : ",") << npcId;
        }
        animations.push_back({std::to_string(id), std::to_string(animationId), pHouse->name, npcList.str(),
            movie[1], "", "0", std::to_string(soundBaseId), movie[2],
            pRule != nullptr ? pOriginal->type : movie[3]});
        if (!movie[1].empty())
        {
            videoStems.insert(movie[1]);
        }
        if (integer(movie[2]) != 0)
        {
            portraitIds.insert(integer(movie[2]));
        }
    }
    require(houseTable.loadAnimationRows(animations, movieRows), "House presentation loader rejected rows");
    for (size_t index = 1; index < animations.size(); ++index)
    {
        const Row &row = animations[index];
        const uint32_t id = integer(row[0]);
        const HouseEntry *pHouse = houseTable.get(id);
        const uint32_t expectedPortrait = integer(row[8]) == 0
            ? originalHouseTable.get(id)->proprietorPictureId : integer(row[8]);
        const uint32_t soundBaseId = integer(row[7]);
        require(pHouse && pHouse->buildingName == row[2] && pHouse->residentNpcIds == residents[id]
            && pHouse->roomSoundId == 0 && pHouse->houseSoundBaseId == soundBaseId
            && pHouse->proprietorPictureId == expectedPortrait && pHouse->videoName == row[4],
            "House presentation roundtrip changed identity");
        if (breachAlvarHouses && breachAlvarHouseRule(id) != nullptr)
        {
            require(pHouse->type == originalHouseTable.get(id)->type,
                "Breach Alvar picture changed the source house type");
        }
        const std::optional<uint32_t> greeting = deriveHouseSoundId(*pHouse, HouseSoundType::GeneralGreeting);
        require(soundBaseId == 0 ? !greeting : greeting == soundBaseId + 1, "House sound group was misinterpreted");
    }

    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"house_presentation\",\"runtime_ready\":false,"
        "\"typed_validation\":true,\"houses\":" << animations.size() - 1 << ",\"movie_rows\":" << movies.size()
        << ",\"residents\":" << residentCount << ",\"resident_houses\":";
    size_t residentHouses = 0;
    for (const auto &[id, ids] : residents)
    {
        residentHouses += !ids.empty();
    }
    manifest << residentHouses << ",\"referenced_videos\":" << videoStems.size()
        << ",\"referenced_proprietor_portraits\":" << portraitIds.size()
        << ",\"video_payloads_converted\":false,\"portrait_payloads_converted\":false,"
        "\"sound_payloads_converted\":false,\"sound_group_stride\":10,\"inactive_house_npcs\":[";
    for (size_t index = 0; index < inactiveHouseNpcs.size(); ++index)
    {
        manifest << (index == 0 ? "" : ",") << inactiveHouseNpcs[index];
    }
    manifest << "],\"missing_greeting_sound_ids\":[";
    bool first = true;
    for (uint32_t id : missingGreetingIds)
    {
        manifest << (first ? "" : ",") << id;
        first = false;
    }
    manifest << "]";
    appendClankerHouseExitManifest(manifest, clankerHouseExit);
    manifest << "}\n";
    require(std::filesystem::create_directory(output), "Cannot create house presentation output");
    try
    {
        writeText(output / "engine/data_tables/house_animations.txt", serializeTextRows(animations));
        writeText(output / "engine/data_tables/house_movies.txt", serializeTextRows(movieRows));
        if (clankerHouseExit)
        {
            writeText(output / "engine/data_tables/house_data.txt", serializeTextRows(houseRows));
        }
        writeText(output / "house_conversion.json", manifest.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
