#include "BreachMazePopulationResources.h"

#include "BreachMazeTemplateResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/indoor/IndoorMapData.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace OpenYAMM::Import
{
namespace
{
using Bytes = std::vector<uint8_t>;
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
using IndexedRows = std::map<uint32_t, Row>;

constexpr std::array<std::pair<std::string_view, std::string_view>, 12> LooseSources = {{
    {"Scripts/Core/ConstAndBits.lua", "466b2f8b3ccceb3a2ab2d95795493dfabc523ddf3c3cb57d4c6dc33609f12893"},
    {"Scripts/Core/evt.lua", "c16ccaa1f15d0e22de875c81c14391b11dcaa2ee7dbe58f45af975d1fae6fd03"},
    {"Scripts/Maps/BrBase.lua", "58d0978f2d8683d3d94cfeb6048917c0cc61d9818a8f272ca1baca2e6005d60b"},
    {"Scripts/General/AdaptiveMonstersStats.lua", "327d071c2d42d66bc336fa9850d9a506e1ad4e30f3f9b013edd7deb04bbc7b96"},
    {"Scripts/General/ExtraEvents.lua", "78efe91a0bb581104c4f9ede00f1c7e267d948a626df3d41bf3110b9c548bbc6"},
    {"Scripts/Structs/After/GlobalEventsNewHandler.lua",
        "44a5e278756438e4c8ac9bf1eb5ffc9134d39bb3e0d65356ffcaf5618f392a87"},
    {"Data/Tables/MonList.txt", "9c34e9c5b23d86583214f5656cebb3a95c08b48bd0ed0df3848cd9f7d0454bf4"},
    {"Data/Tables/DecList.txt", "733c4d8af2d8c02adb8bcaea80d9cf342d05d465dcd2720bb11927b8cdb1a2d4"},
    {"Data/Tables/Monster Kinds.txt", "167319d6cab84e48abf3e7ea407472de74d95ca7d247927bbc8e337a212813e3"},
    {"Data/Tables/Bolster - monsters.txt", "4fe782f3b44ec0c7fb56df422b5e21336bdabde8fa74992102a96a8d74e6ab9f"},
    {"Data/Tables/Bolster - maps.txt", "fd0b7f8a6ba0db1130c68b55928d0c2f0b7c504d42b0380502eb92a26224b8ae"},
    {"Data/Tables/Bolster - formulas.txt", "f0be4d23cb7c8f504e57fa83a1ae1dc03230a8a72b6227bb9aa435d23a8db539"},
}};
constexpr std::string_view MonsterMemberSha256 =
    "e37be5cb43d52295a377f780a0e160bd4f21fe91bfd6ab98b58ee6e286833085";

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error("BrBase population source: " + message);
    }
}

std::string sha256(const Bytes &bytes)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), CC_LONG(bytes.size()), hash.data());
    std::ostringstream result;
    for (unsigned char value : hash)
    {
        result << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
    }
    return result.str();
}

std::filesystem::path contained(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    const std::filesystem::path path = std::filesystem::canonical(root / relative);
    const std::filesystem::path check = path.lexically_relative(root);
    require(!check.empty() && !check.is_absolute() && *check.begin() != ".."
        && std::filesystem::is_regular_file(path), "input path escapes source root: " + relative.string());
    return path;
}

Bytes readFile(const std::filesystem::path &path)
{
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "invalid source file size");
    Bytes bytes(size);
    std::ifstream file(path, std::ios::binary);
    require(bool(file.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()))),
        "cannot read input");
    return bytes;
}

uint32_t integer(std::string_view text, uint32_t maximum = std::numeric_limits<uint32_t>::max())
{
    uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    require(result.ec == std::errc() && result.ptr == text.data() + text.size() && value <= maximum,
        "invalid integer: " + std::string(text));
    return value;
}

std::string lower(std::string value)
{
    for (char &letter : value)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return value;
}

Rows table(const Bytes &bytes)
{
    size_t restored = 0;
    const std::optional<Engine::TextTable> parsed =
        Engine::TextTable::parseTabSeparated(decodeLegacyText(bytes, false, restored));
    require(parsed.has_value() && parsed->getRowCount() > 0, "invalid source table");
    Rows result;
    for (size_t index = 0; index < parsed->getRowCount(); ++index)
    {
        result.push_back(parsed->getRow(index));
    }
    return result;
}

IndexedRows indexed(const Rows &rows, size_t width)
{
    IndexedRows result;
    for (const Row &row : rows)
    {
        if (row.empty() || row[0].empty() || row[0].find_first_not_of("0123456789") != std::string::npos)
        {
            continue;
        }
        require(row.size() == width, "source row column count changed");
        require(result.emplace(integer(row[0]), row).second, "duplicate source row ID");
    }
    return result;
}

template <size_t Size> std::array<std::string, Size> fixedRow(const Row &row)
{
    require(row.size() == Size, "source row width changed");
    std::array<std::string, Size> result;
    std::copy(row.begin(), row.end(), result.begin());
    return result;
}

// Only the original first eight kind bits are needed by this finite profile. Types such as Construct
// and Lizardman remain in the complete raw row, without incorrectly aliasing native enum bit values.
uint8_t kindBit(std::string_view name)
{
    constexpr std::array<std::string_view, 8> names = {
        "Undead", "Dragon", "Swimmer", "Immobile", "Peasant", "NoArena", "Ogre", "Elemental"};
    for (size_t index = 0; index < names.size(); ++index)
    {
        if (name == names[index] || name == std::to_string(index + 1))
        {
            return uint8_t(1u << index);
        }
    }
    return 0;
}

uint8_t extraKinds(const Row &row)
{
    uint8_t result = kindBit(row[2]);
    size_t begin = 0;
    while (begin < row[3].size())
    {
        const size_t comma = row[3].find(',', begin);
        const size_t end = comma == std::string::npos ? row[3].size() : comma;
        std::string_view token(row[3].data() + begin, end - begin);
        while (!token.empty() && token.front() == ' ')
        {
            token.remove_prefix(1);
        }
        while (!token.empty() && token.back() == ' ')
        {
            token.remove_suffix(1);
        }
        result |= kindBit(token);
        begin = end + 1;
    }
    return result;
}

std::array<uint8_t, 199> baseKindTable(const Rows &rows)
{
    require(rows.size() == 200 && rows[0].size() == 9 && rows[0][0] == "Monster",
        "unexpected original kind table range");
    std::array<uint8_t, 199> result = {};
    for (size_t index = 1; index < rows.size(); ++index)
    {
        const Row &row = rows[index];
        require(row.size() == 9, "invalid kind row width");
        const std::string_view label(row[0]);
        const size_t space = label.find(' ');
        const uint32_t id = integer(label.substr(0, space));
        require(id == index - 1, "original kind IDs lost order");
        for (size_t kind = 1; kind < row.size(); ++kind)
        {
            require(row[kind] == "-" || row[kind] == "x", "unknown kind marker");
            if (row[kind] == "x")
            {
                result[id] |= uint8_t(1u << (kind - 1));
            }
        }
    }
    return result;
}
}

std::optional<BreachMazePopulationProfile> loadBreachMazePopulationProfile(const std::filesystem::path &source)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    if (!std::filesystem::exists(root / "Scripts/Maps/BrBase.lua"))
    {
        return std::nullopt;
    }
    const std::optional<BreachMazeTemplateProfile> templateProfile = loadBreachMazeTemplateProfile(root);
    require(templateProfile.has_value(), "template profile absent for present script");
    BreachMazePopulationProfile result;
    result.mazeTemplate = templateProfile->mazeTemplate;
    Game::BreachMazePopulationCatalog &catalog = result.catalog;
    catalog.templateFingerprint = Game::breachMazePopulationTemplateFingerprint(result.mazeTemplate);
    std::map<std::string, Bytes> loose;
    for (const auto &[name, expectedHash] : LooseSources)
    {
        Bytes bytes = readFile(contained(root, std::string(name)));
        const std::string actualHash = sha256(bytes);
        require(actualHash == expectedHash, "unsupported fingerprint: " + std::string(name));
        catalog.sources.push_back({std::string(name), actualHash});
        loose.emplace(std::string(name), std::move(bytes));
    }
    const Bytes monsterBytes = readLodText(contained(root, "Data/mmmerge.T.lod"), "monsters.txt");
    const std::string monsterHash = sha256(monsterBytes);
    require(monsterHash == MonsterMemberSha256, "unsupported merged monsters.txt member");
    catalog.sources.push_back({"Data/mmmerge.T.lod::monsters.txt", monsterHash});
    const IndexedRows monsters = indexed(table(monsterBytes), 39);
    const IndexedRows monList = indexed(table(loose.at("Data/Tables/MonList.txt")), 22);
    const IndexedRows bolsters = indexed(table(loose.at("Data/Tables/Bolster - monsters.txt")), 17);
    const std::array<uint8_t, 199> baseKinds = baseKindTable(table(loose.at("Data/Tables/Monster Kinds.txt")));
    require(monsters.size() == 652 && monList.size() == 652 && bolsters.size() == 652,
        "incomplete original monster tables");
    for (uint32_t id = 1; id <= monsters.size(); ++id)
    {
        const Row &row = monsters.at(id);
        const Row &model = monList.at(id - 1);
        require(lower(row[2]) == lower(model[13]), "monster picture and MonList identity differ");
        Game::BreachMazeMonsterSource entry;
        entry.id = id;
        entry.level = uint16_t(integer(row[3], 65535));
        entry.baseHitPoints = integer(row[4]);
        entry.height = uint16_t(integer(model[1], 65535));
        entry.radius = uint16_t(integer(model[2], 65535));
        entry.moveSpeed = uint16_t(integer(model[3], 65535));
        entry.toHitRadius = int16_t(integer(model[4], 32767));
        for (size_t channel = 0; channel < 4; ++channel)
        {
            // Same source byte wrapping as the existing CoreResources importer, including values999.
            entry.tintArgb |= (integer(model[5 + channel], 65535) & 255) << (channel * 8);
        }
        for (size_t index = 0; index < entry.soundIds.size(); ++index)
        {
            entry.soundIds[index] = uint16_t(integer(model[9 + index], 65535));
        }
        for (size_t index = 0; index < entry.spriteGroups.size(); ++index)
        {
            entry.spriteGroups[index] = lower(model[14 + index]);
            require(!entry.spriteGroups[index].empty(), "empty actor action group");
        }
        entry.monsterRow = fixedRow<39>(row);
        entry.monListRow = fixedRow<22>(model);
        // Source table0..198 overrides the base. Beyond it, this finite profile explicitly assumes
        // zero base flags; it is not an execution oracle for the original binary fallback function.
        entry.baseKinds = id < baseKinds.size() ? baseKinds[id] : 0;
        if (id <= 651)
        {
            const Row &bolster = bolsters.at(id);
            entry.bolsterRow = fixedRow<17>(bolster);
            entry.bolsterKinds = extraKinds(bolster);
        }
        catalog.monsters.push_back(std::move(entry));
    }
    const IndexedRows decors = indexed(table(loose.at("Data/Tables/DecList.txt")), 23);
    require(decors.size() == 673, "incomplete original decoration table");
    for (uint32_t id = 0; id < decors.size(); ++id)
    {
        const Row &row = decors.at(id);
        Game::BreachMazeDecorationSource entry;
        entry.id = uint16_t(id);
        entry.name = row[1];
        entry.spriteGroup = lower(row[7]);
        entry.height = uint16_t(integer(row[4], 65535));
        entry.radius = uint16_t(integer(row[5], 65535));
        entry.lightRadius = uint16_t(integer(row[6], 65535));
        entry.soundId = uint16_t(integer(row[19], 65535));
        require(row[8] == "x" || row[8] == "-", "unknown decoration collision marker");
        entry.noBlockMovement = row[8] == "x";
        entry.sourceRow = fixedRow<23>(row);
        catalog.decorations.push_back(std::move(entry));
    }
    const IndexedRows mapRows = indexed(table(loose.at("Data/Tables/Bolster - maps.txt")), 10);
    catalog.bolsterMapRow = mapRows.at(206);
    catalog.bolsterFormulaRows = table(loose.at("Data/Tables/Bolster - formulas.txt"));
    const Bytes geometry = readLodMap(contained(root, "Data/breach.games.lod"), "brbase.blv");
    const std::string geometryHash = sha256(geometry);
    require(geometryHash == templateProfile->geometrySha256, "template geometry changed during profile load");
    catalog.sources.push_back({"Data/breach.games.lod::brbase.blv", geometryHash});
    const std::optional<Game::IndoorMapData> map = Game::IndoorMapDataLoader().loadFromBytes(geometry);
    require(map && map->entities.size() == 256 && map->lights.size() == 256, "invalid original population slots");
    for (size_t index = 0; index < catalog.sourceSlots.size(); ++index)
    {
        const Game::IndoorEntity &entity = map->entities[index];
        const Game::IndoorLight &light = map->lights[index];
        require(entity.decorationListId < catalog.decorations.size(), "original sprite has unknown decoration ID");
        require(lower(entity.name) == lower(catalog.decorations[entity.decorationListId].name),
            "original decoration name and table ID differ");
        require(entity.x >= -32768 && entity.x <= 32767 && entity.y >= -32768 && entity.y <= 32767
            && entity.z >= -32768 && entity.z <= 32767, "original slot coordinates out of native light range");
        Game::BreachMazePopulationSourceSlot &slot = catalog.sourceSlots[index];
        slot.decorationId = entity.decorationListId;
        slot.position = {entity.x * 2, entity.y * 2, entity.z * 2};
        slot.facing = entity.facing;
        slot.attributes = entity.aiAttributes;
        slot.light = {light.radius, {light.red, light.green, light.blue}, light.type, light.attributes,
            light.brightness, light.id};
    }
    std::sort(catalog.sources.begin(), catalog.sources.end(),
        [](const Game::BreachMazeSourceFingerprint &left, const Game::BreachMazeSourceFingerprint &right)
        { return left.name < right.name; });
    catalog.contentFingerprint = Game::breachMazePopulationCatalogFingerprint(catalog);
    return result;
}
}
