#include "WorldRuleResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/tables/HouseTable.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : static_cast<char>(c);
    });
    return value;
}

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int32_t number(const std::string &text, int32_t minimum, int32_t maximum)
{
    const size_t first = text.find_first_not_of(" \r\n\t");
    const std::string token = first == std::string::npos ? ""
        : text.substr(first, text.find_last_not_of(" \r\n\t") - first + 1);
    int32_t value = 0;
    const std::from_chars_result parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == token.data() + token.size()
        && value >= minimum && value <= maximum,
        "Invalid world rule numeric field: " + text);
    return value;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid world rule table quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 != table->getRowCount() || row != Row{""})
        {
            rows.push_back(row);
        }
    }
    return rows;
}

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "World rule input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 4 * 1024 * 1024, "Invalid world rule input size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read world rule dependency");
    if (!legacy)
    {
        return parse(text);
    }
    size_t glyphs = 0;
    text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
    // The source reader splits literal lines/tabs; quote characters in Notes have no CSV meaning.
    Rows rows;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        Row row;
        size_t start = 0;
        while (true)
        {
            const size_t end = line.find('\t', start);
            row.push_back(line.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

bool marker(const std::string &value)
{
    require(value.empty() || value == "-" || value == "x", "Invalid world rule marker");
    return value == "x";
}

int32_t optionalNumber(const Row &row, size_t column)
{
    return column >= row.size() || row[column].empty() ? 0 : number(row[column], INT32_MIN, INT32_MAX);
}

double decimal(const std::string &value)
{
    std::istringstream stream(value);
    stream.imbue(std::locale::classic());
    double result = 0;
    require(bool(stream >> result) && stream.eof() && std::isfinite(result) && result >= 0,
        "Invalid world rule decimal");
    return result;
}

Row assetList(const std::string &value)
{
    Row result;
    std::istringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        const size_t first = token.find_first_not_of(' ');
        require(first != std::string::npos, "Empty world rule asset name");
        token = token.substr(first, token.find_last_not_of(' ') - first + 1);
        require(std::all_of(token.begin(), token.end(), [](unsigned char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '_' || c == '-' || c == '.';
        }) && token != "." && token != "..", "Invalid world rule asset token");
        result.push_back(token);
    }
    require(value.empty() || value.back() != ',', "Trailing empty world rule asset name");
    return result;
}

void validateContinents(const Rows &rows, const Game::MergedContinentSettingTable &table, const Game::MapStats &maps)
{
    require(rows[0] == Row{"#", "Note", "Rep affects guards", "Rep affects shops", "Rep affects NPC",
        "Tell prof news", "NPC Followers", "Saturation", "Softness", "Death movie", "Specific water", "Death map 1",
        "X", "Y", "Z", "Dir", "Death map 2", "X", "Y", "Z", "Dir",
        "Skies (from clear to storm, divided by \",\")", "Loading pics"}, "Unsupported continent header");
    require(table.entries().size() == rows.size() - 1, "Discarded continent settings");
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(row.size() == 23 && number(row[0], 1, 255) == index, "Continent row identity/width changed");
        const Game::MergedContinentSettingEntry *pEntry = table.findById(index);
        require(pEntry && pEntry->note == row[1] && pEntry->reputationAffectsGuards == marker(row[2])
            && pEntry->reputationAffectsShops == marker(row[3]) && pEntry->reputationAffectsNpc == marker(row[4])
            && pEntry->tellProfessionNews == marker(row[5]) && pEntry->npcFollowers == marker(row[6])
            && pEntry->saturation == decimal(row[7]) && pEntry->softness == decimal(row[8])
            && pEntry->deathMovie == row[9] && pEntry->specificWater == row[10], "Continent rule fields changed");
        assetList(row[9]);
        assetList(row[10]);
        require(pEntry->skies == assetList(row[21]) && pEntry->loadingPictures == assetList(row[22]),
            "Continent sky/loading order changed");
        const std::array<std::string, 2> deathMaps = {pEntry->deathMap1, pEntry->deathMap2};
        const std::array<std::array<int32_t, 4>, 2> coordinates =
            {{{pEntry->deathMap1X, pEntry->deathMap1Y, pEntry->deathMap1Z, pEntry->deathMap1Direction},
              {pEntry->deathMap2X, pEntry->deathMap2Y, pEntry->deathMap2Z, pEntry->deathMap2Direction}}};
        for (size_t death = 0; death < deathMaps.size(); ++death)
        {
            const size_t column = 11 + death * 5;
            require(deathMaps[death] == row[column]
                && (row[column].empty() || maps.findByFileName(row[column])), "Unknown continent death map");
            for (size_t component = 0; component < 4; ++component)
            {
                require(coordinates[death][component] == optionalNumber(row, column + component + 1),
                    "Continent death coordinate changed");
                require(!row[column].empty() || row[column + component + 1].empty(),
                    "Death coordinates exist without a destination");
            }
        }
    }
}

size_t validateHouseRules(const Rows &rows, const Game::MergedHouseRuleTable &table,
    const Game::MergedTransportLocationTable &locations)
{
    const Row sections = {"Weapon shops Standart", "Weapon shops Special", "Armor shops Standart",
        "Armor shops Special", "Magic shops Standart", "Magic shops Special", "Alchem shops Standart",
        "Alchem shops Special", "Spellbook shops", "Stables", "Boats", "Training halls", "Arcomage in taverns"};
    const std::array<size_t, 13> widths = {6, 6, 11, 11, 2, 2, 2, 2, 2, 5, 5, 2, 13};
    require(rows[0] == Row{"Index by type", "Quality", "Items1", "Items2", "Items3", "Items4", "Quality",
        "Items1", "Items2", "Items3", "Items4", " ", " "}, "Unsupported house rules header");
    require(table.sections().size() == sections.size(), "House rule section count changed");
    std::array<std::map<int32_t, std::vector<int32_t>>, 13> expected;
    std::set<int32_t> locationIds;
    for (const Game::MergedTransportLocationEntry &entry : locations.entries())
    {
        locationIds.insert(entry.id);
    }
    size_t section = 0;
    bool inSection = false;
    size_t replacements = 0;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(!row.empty(), "Empty house rule row");
        if (row[0] == (inSection && section + 1 < sections.size() ? sections[section + 1] : sections[section]))
        {
            section += inSection;
            inSection = true;
            Row header = {sections[section]};
            if (section == 9 || section == 10)
            {
                header.insert(header.end(), {"Loc1", "Loc2", "Loc3", "Loc4"});
            }
            else if (section == 11)
            {
                header.push_back("Max level");
            }
            else if (section == 12)
            {
                header.insert(header.end(), {"Tower to win", "Res to win", "Tower at start", "Wall at start",
                    "Res1 per turn", "Res2", "Res3", "Res1 at start", "Res2", "Res3", "Ai", "'Rules' text index"});
            }
            require(row == header, "Unexpected house section metadata");
            continue;
        }
        require(inSection && row.size() == widths[section], "House rule section order/width changed");
        std::vector<int32_t> values;
        for (const std::string &cell : row)
        {
            values.push_back(number(cell, 0, 65535));
        }
        require(values.front() > 0, "Invalid house rule ID");
        if (section == 9 || section == 10)
        {
            for (size_t column = 1; column < values.size(); ++column)
            {
                require(locationIds.contains(values[column]),
                    "House route references unknown transport location");
            }
        }
        replacements += expected[section].contains(values.front());
        expected[section][values.front()] = std::move(values);
    }
    require(section == sections.size() - 1, "Incomplete house rule sections");
    for (size_t index = 0; index < sections.size(); ++index)
    {
        const Game::MergedHouseRuleSection &actual = table.sections()[index];
        require(actual.name == sections[index] && actual.numericRows.size() == expected[index].size(),
            "House rule overwrite semantics changed");
        int32_t expectedId = 0;
        for (const auto &[id, values] : expected[index])
        {
            require(id == ++expectedId, "House rule array has an undefined slot");
            const auto found = std::find_if(actual.numericRows.begin(), actual.numericRows.end(),
                [id](const std::vector<int32_t> &row) { return row.front() == id; });
            require(found != actual.numericRows.end() && *found == values, "House rule numbers changed");
        }
    }
    return replacements;
}

void validateTransportLocations(const Rows &rows, const Game::MergedTransportLocationTable &table,
    const Game::MapStats &maps)
{
    require(rows[0] == Row{"#", "Map", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
        "Sunday", "DaysCount", "X", "Y", "Z", "Direction", "QBit"}, "Unsupported transport location header");
    require(table.entries().size() == rows.size() - 1, "Discarded transport locations");
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        const Game::MergedTransportLocationEntry &entry = table.entries()[index - 1];
        require(row.size() == 15 && number(row[0], 0, 255) == index - 1 && entry.id == index - 1
            && entry.mapName == row[1] && maps.findByFileName(row[1]), "Invalid transport identity/destination");
        for (size_t day = 0; day < 7; ++day)
        {
            require(entry.weekdays[day] == marker(row[day + 2]), "Transport weekday availability changed");
        }
        const std::array<int32_t, 6> fields = {static_cast<int32_t>(entry.daysCount), entry.x, entry.y, entry.z,
            entry.direction, static_cast<int32_t>(entry.qbit)};
        for (size_t field = 0; field < fields.size(); ++field)
        {
            const int32_t minimum = field == 0 || field == 5 ? 0 : INT32_MIN;
            require(fields[field] == number(row[field + 9], minimum, INT32_MAX), "Transport coordinate/rule changed");
        }
    }
}

size_t validateOutdoorTravels(const Rows &rows, const Game::MergedOutdoorTravelTable &table, Game::MapStats &maps)
{
    require(rows[0] == Row{"Key map", "Up", "Side", "Days", "Down", "Side", "Days", "Left", "Side", "Days",
        "Right", "Side", "Days", "StraightTravel", "Notes", ""}, "Unsupported outdoor travel header");
    require(table.entries().size() == rows.size() - 1, "Discarded outdoor travel records");
    std::set<std::string> origins;
    size_t directions = 0;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        const Game::MergedOutdoorTravelEntry &entry = table.entries()[index - 1];
        require(row.size() == 16 && entry.keyMap == row[0] && maps.findByFileName(row[0])
            && origins.insert(lower(row[0])).second && row[15].empty(), "Invalid outdoor travel origin/extension");
        require(entry.straightTravel == marker(row[13]) && entry.notes == row[14], "Outdoor travel flags changed");
        const std::array<const Game::MergedOutdoorTravelDirection *, 4> routes =
            {&entry.up, &entry.down, &entry.left, &entry.right};
        for (size_t direction = 0; direction < routes.size(); ++direction)
        {
            const size_t column = 1 + direction * 3;
            const Game::MergedOutdoorTravelDirection &route = *routes[direction];
            require(route.mapName == row[column] && route.side == row[column + 1] && route.requirements.empty(),
                "Outdoor direction changed");
            if (row[column].empty())
            {
                require(row[column + 1].empty() && row[column + 2].empty() && !route.days,
                    "Outdoor travel metadata exists without a destination");
                continue;
            }
            require(maps.findByFileName(row[column]) && (route.side == "up" || route.side == "down"
                || route.side == "left" || route.side == "right") && route.days
                && *route.days == number(row[column + 2], 0, 65535), "Invalid outdoor destination/side/duration");
            ++directions;
        }
    }
    require(maps.applyMergedOutdoorTravels(table), "Cannot apply outdoor travel rules");
    const std::array<Game::MapBoundaryEdge, 4> edges = {Game::MapBoundaryEdge::North, Game::MapBoundaryEdge::South,
        Game::MapBoundaryEdge::West, Game::MapBoundaryEdge::East};
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        const Game::MapStatsEntry *pMap = maps.findByFileName(row[0]);
        require(pMap && pMap->outdoorBounds.enabled, "Outdoor bounds were not applied");
        for (size_t direction = 0; direction < edges.size(); ++direction)
        {
            const std::optional<Game::MapEdgeTransition> &edge = *pMap->edgeTransition(edges[direction]);
            const size_t column = 1 + direction * 3;
            require(edge.has_value() == !row[column].empty(), "Outdoor edge activation changed");
            if (edge)
            {
                require(edge->destinationMapFileName == row[column]
                    && edge->travelDays == number(row[column + 2], 0, 65535)
                    && edge->straightTravel == marker(row[13]), "Applied outdoor route changed");
            }
        }
    }
    return directions;
}

size_t validateTownPortals(const Rows &rows, const Game::MergedTownPortalSwitchTable &table, const Game::MapStats &maps)
{
    require(rows[0] == Row{"#", "Map", "X", "Y", "Z", "Direction", "LookAngle", "IconName", "Icon X", "Icon Y",
        "IconWidth", "IconHeight", "QBitIndex", "Description", ""}, "Unsupported town portal header");
    size_t groupIndex = 0;
    size_t destinationIndex = 0;
    size_t mappedDestinations = 0;
    const Game::MergedTownPortalSwitchGroup *pGroup = nullptr;
    std::set<std::string> names;
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(row.size() >= 3, "Invalid town portal row width");
        if (row[0] == "@")
        {
            require(!pGroup || pGroup->destinations.size() == destinationIndex, "Discarded town portal destinations");
            require(groupIndex < table.groups().size() && names.insert(lower(row[1])).second,
                "Duplicate town portal group");
            pGroup = &table.groups()[groupIndex++];
            destinationIndex = 0;
            require(pGroup->name == row[1] && pGroup->topicId == number(row[2], 1, 65535),
                "Town portal switch QBit changed");
            for (size_t column = 3; column < row.size(); ++column)
            {
                require(row[column].empty(), "Unexpected town portal group metadata");
            }
            continue;
        }
        require(pGroup && (row.size() == 14 || (row.size() == 15 && row[14].empty()))
            && destinationIndex < pGroup->destinations.size(), "Invalid town portal destination layout");
        const Game::MergedTownPortalDestination &entry = pGroup->destinations[destinationIndex];
        require(number(row[0], 0, 5) == destinationIndex && entry.id == destinationIndex,
            "Town portal destination array identity changed");
        ++destinationIndex;
        require(entry.mapId == number(row[1], 0, 65535) && (entry.mapId == 0 || maps.findById(entry.mapId)),
            "Town portal references unknown map");
        const std::array<int32_t, 5> coordinates = {entry.x, entry.y, entry.z, entry.direction, entry.lookAngle};
        for (size_t field = 0; field < coordinates.size(); ++field)
        {
            require(coordinates[field] == number(row[field + 2], INT32_MIN, INT32_MAX),
                "Town portal arrival coordinate changed");
        }
        require(entry.iconName == row[7] && entry.iconX == number(row[8], INT32_MIN, INT32_MAX)
            && entry.iconY == number(row[9], INT32_MIN, INT32_MAX)
            && entry.qbitIndex == number(row[12], 0, 65535) && entry.description == row[13],
            "Town portal icon, condition or source note changed");
        assetList(row[7]);
        const std::array<std::optional<uint32_t>, 2> dimensions = {entry.iconWidth, entry.iconHeight};
        for (size_t field = 0; field < dimensions.size(); ++field)
        {
            require(dimensions[field].has_value() == !row[field + 10].empty(), "Town portal dimension default changed");
            if (dimensions[field])
            {
                require(*dimensions[field] == number(row[field + 10], 1, 65535), "Town portal dimensions changed");
            }
        }
        mappedDestinations += entry.mapId != 0;
    }
    require(pGroup && pGroup->destinations.size() == destinationIndex && groupIndex == table.groups().size(),
        "Incomplete town portal source");
    return mappedDestinations;
}
}

void convertWorldRuleResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    using namespace Game;
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "World rule output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "World rule output already exists");
    std::map<std::string, Rows> sources;
    const std::map<std::string, std::string> names = {{"Continent settings.txt", "continent_settings.txt"},
        {"HW water textures.txt", "hw_water_textures.txt"}, {"House exits.txt", "house_exits.txt"},
        {"House rules.txt", "house_rules.txt"}, {"Outdoor travels.txt", "outdoor_travels.txt"},
        {"Overlay.txt", "overlay.txt"}, {"TownPortalSwitch.txt", "town_portal_switch.txt"},
        {"Transport index.txt", "transport_index.txt"}, {"Transport locations.txt", "transport_locations.txt"}};
    for (const auto &[name, target] : names)
    {
        Rows rows = readRows(root, "Data/Tables/" + name, true);
        require(rows.size() > 1, "Empty world rule source: " + name);
        sources.emplace(target, std::move(rows));
    }
    MapStats maps;
    HouseTable houses;
    require(maps.loadFromRows(readRows(assets, "engine/data_tables/map_stats.txt", false))
        && houses.loadFromRows(readRows(assets, "engine/data_tables/house_data.txt", false)),
        "Invalid world rule registry dependency");
    MergedContinentSettingTable continents;
    MergedHardwareWaterTextureTable water;
    MergedHouseExitTable exits;
    MergedHouseRuleTable rules;
    MergedOutdoorTravelTable outdoor;
    MergedOverlayTable overlays;
    MergedTownPortalSwitchTable portals;
    MergedTransportIndexTable indices;
    MergedTransportLocationTable locations;
    require(continents.loadFromRows(sources.at("continent_settings.txt"))
        && water.loadFromRows(sources.at("hw_water_textures.txt"))
        && exits.loadFromRows(sources.at("house_exits.txt")) && rules.loadFromRows(sources.at("house_rules.txt"))
        && outdoor.loadFromRows(sources.at("outdoor_travels.txt")) && overlays.loadFromRows(sources.at("overlay.txt"))
        && portals.loadFromRows(sources.at("town_portal_switch.txt"))
        && indices.loadFromRows(sources.at("transport_index.txt"))
        && locations.loadFromRows(sources.at("transport_locations.txt")), "World rule typed loading failed");
    validateContinents(sources.at("continent_settings.txt"), continents, maps);
    validateTransportLocations(sources.at("transport_locations.txt"), locations, maps);
    const size_t replacements = validateHouseRules(sources.at("house_rules.txt"), rules, locations);
    const size_t outdoorDirections = validateOutdoorTravels(sources.at("outdoor_travels.txt"), outdoor, maps);
    const size_t portalMaps = validateTownPortals(sources.at("town_portal_switch.txt"), portals, maps);
    const Rows &waterRows = sources.at("hw_water_textures.txt");
    require(waterRows[0] == Row{"SW texture", "HW texture prefix"}
        && water.entries().size() == waterRows.size() - 1, "Unsupported hardware water table");
    std::set<std::string> waterTextures;
    for (size_t index = 1; index < waterRows.size(); ++index)
    {
        const Row &row = waterRows[index];
        const MergedHardwareWaterTextureEntry &entry = water.entries()[index - 1];
        require(row.size() == 2 && assetList(row[0]).size() == 1 && assetList(row[1]).size() == 1
            && waterTextures.insert(lower(row[0])).second && entry.softwareTexture == row[0]
            && entry.hardwareTexturePrefix == row[1], "Hardware water texture binding changed");
    }
    for (const MergedContinentSettingEntry &continent : continents.entries())
    {
        for (const std::string &texture : assetList(continent.specificWater))
        {
            require(waterTextures.contains(lower(texture)), "Continent references unknown water texture");
        }
    }
    const Rows &overlayRows = sources.at("overlay.txt");
    require(overlayRows[0] == Row{"Id", "Type", "SFTGroup"}
        && overlays.entries().size() == overlayRows.size() - 1, "Unsupported overlay table");
    std::set<uint32_t> overlayIds;
    for (size_t index = 1; index < overlayRows.size(); ++index)
    {
        const Row &row = overlayRows[index];
        const MergedOverlayEntry &entry = overlays.entries()[index - 1];
        require(row.size() == 3 && entry.id == number(row[0], 1, 65535) && overlayIds.insert(entry.id).second
            && row[1] == "0" && row[2] == "null" && entry.type == 0 && entry.sftGroup == "null",
            "Active/unknown overlay needs a supported sprite binding profile");
    }
    const Rows &indexRows = sources.at("transport_index.txt");
    require(indexRows[0] == Row{"2D Event", "1", "2", "3"}
        && indices.entries().size() == indexRows.size() - 1, "Unsupported transport index table");
    std::set<uint32_t> indexedHouses;
    for (size_t index = 1; index < indexRows.size(); ++index)
    {
        const Row &row = indexRows[index];
        const MergedTransportIndexEntry &entry = indices.entries()[index - 1];
        require(row.size() == 4 && entry.houseEventId == number(row[0], 1, 65535)
            && houses.get(entry.houseEventId) && indexedHouses.insert(entry.houseEventId).second
            && entry.locationIndicesByPeriod.size() == 3, "Invalid transport index identity");
        for (size_t period = 0; period < 3; ++period)
        {
            require(entry.locationIndicesByPeriod[period]
                == number(row[period + 1], -1, locations.entries().size() - 1),
                "Transport index points outside locations");
        }
    }
    const Rows &exitRows = sources.at("house_exits.txt");
    const MergedHouseExitTableData &exitData = exits.data();
    require(exitRows.size() >= 5 && exitRows[0].size() > 1 && exitRows[0][0] == "NPC pics:"
        && exitRows[1].size() >= 2 && exitRows[1][0] == "Free NPC:"
        && exitRows[2].size() == 2 && exitRows[2][0] == "Free topic:"
        && exitRows[3].size() >= 4 && exitRows[3][0] == "Map name", "Unsupported house exit metadata");
    require(exitData.npcPictureIds.size() == exitRows[0].size() - 1
        && exitData.freeNpcId == number(exitRows[1][1], 1, 65535)
        && exitData.freeTopicId == number(exitRows[2][1], 1, 65535)
        && exitData.exits.size() == exitRows.size() - 4, "House exit metadata changed");
    for (size_t column = 1; column < exitRows[0].size(); ++column)
    {
        require(exitData.npcPictureIds[column - 1] == number(exitRows[0][column], 1, 65535),
            "House exit portrait index changed");
    }
    for (size_t column = 2; column < exitRows[1].size(); ++column)
    {
        require(exitRows[1][column].empty(), "Unexpected free NPC metadata");
    }
    const Row axes = {"X", "Y", "Z"};
    for (size_t column = 1; column < exitRows[3].size(); ++column)
    {
        require(exitRows[3][column] == axes[(column - 1) % 3], "House exit coordinate header changed");
    }
    size_t exitPositions = 0;
    for (size_t index = 4; index < exitRows.size(); ++index)
    {
        const Row &row = exitRows[index];
        const MergedHouseExitEntry &entry = exitData.exits[index - 4];
        require(row.size() == exitRows[3].size() && (row.size() - 1) % 3 == 0
            && maps.findByFileName(row[0]) && entry.mapName == row[0]
            && entry.positions.size() == (row.size() - 1) / 3, "Invalid house exit map/positions");
        for (size_t point = 0; point < entry.positions.size(); ++point)
        {
            const size_t column = 1 + point * 3;
            require(entry.positions[point].x == number(row[column], INT32_MIN, INT32_MAX)
                && entry.positions[point].y == number(row[column + 1], INT32_MIN, INT32_MAX)
                && entry.positions[point].z == number(row[column + 2], INT32_MIN, INT32_MAX),
                "House exit coordinate changed");
            ++exitPositions;
        }
    }
    require(houses.applyHouseRules(rules, locations, maps) && houses.applyHouseExits(exits, maps),
        "World rules could not be applied to production houses");
    size_t houseRoutes = 0;
    size_t extraExits = 0;
    for (const auto &[id, house] : houses.entries())
    {
        houseRoutes += house.transportRoutes.size();
        extraExits += house.extraExit.has_value();
    }
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"world_rules\",\"runtime_ready\":false,\"continents\":"
        << continents.entries().size() << ",\"water_bindings\":" << water.entries().size()
        << ",\"house_rule_sections\":" << rules.sections().size() << ",\"replaced_house_rule_rows\":" << replacements
        << ",\"outdoor_origins\":" << outdoor.entries().size() << ",\"outdoor_directions\":" << outdoorDirections
        << ",\"inactive_overlays\":" << overlays.entries().size() << ",\"portal_groups\":" << portals.groups().size()
        << ",\"portal_map_destinations\":" << portalMaps << ",\"transport_locations\":" << locations.entries().size()
        << ",\"legacy_transport_indices\":" << indices.entries().size() << ",\"house_exit_positions\":" << exitPositions
        << ",\"applied_house_routes\":" << houseRoutes << ",\"applied_extra_exits\":" << extraExits
        << ",\"typed_validation\":true,\"images_exported\":0,\"movies_exported\":0"
        << ",\"legacy_transport_indices_applied\":false,\"original_arrival_side_parity\":false"
        << ",\"rendered_travel_validated\":false,\"full_gameplay_loop\":false}\n";
    std::map<std::string, std::string> files;
    for (const auto &[name, rows] : sources)
    {
        const std::string text = serializeTextRows(rows);
        require(parse(text) == rows, "World rule serialization changed source fields");
        files.emplace("engine/data_tables/" + name, text);
    }
    files.emplace("world_rule_conversion.json", report.str());
    require(std::filesystem::create_directory(output), "Cannot create fresh world rule output");
    try
    {
        for (const auto &[name, text] : files)
        {
            writeConvertedResource(output / name, std::vector<uint8_t>(text.begin(), text.end()));
        }
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
