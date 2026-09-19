#include "ArcomageResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/ImageAssetLoader.h"
#include "engine/TextTable.h"
#include "game/arcomage/ArcomageAi.h"
#include "game/arcomage/ArcomageLoader.h"
#include "game/arcomage/ArcomageRules.h"
#include "game/tables/HouseTable.h"
#include "game/tables/MapStats.h"
#include "game/tables/MergedBaseTables.h"

#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
namespace
{
using Bytes = std::vector<uint8_t>;
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;
constexpr size_t CardOffset = 0xf0228;
constexpr size_t CardStride = 108;
constexpr char ExecutableDigest[] = "97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b";
constexpr char CardDigest[] = "3310fae18cbba8c381dc47792279bbbaceb322e9811fcea2039be40d72d6156e";

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::filesystem::path checkedPath(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..",
        "Arcomage input escapes root");
    return path;
}

Bytes read(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid Arcomage input size");
    Bytes bytes(size);
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read Arcomage source");
    return bytes;
}

std::string digest(const Bytes &bytes)
{
    std::array<uint8_t, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), hash.data());
    std::ostringstream result;
    for (uint8_t value : hash)
    {
        result << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    return result.str();
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid Arcomage dependency quoting");
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

Rows readRows(const std::filesystem::path &root, const std::string &name)
{
    const Bytes bytes = read(root, "engine/data_tables/" + name);
    return parse(std::string(bytes.begin(), bytes.end()));
}

uint32_t read32(const Bytes &bytes, size_t offset)
{
    require(offset + 4 <= bytes.size(), "Truncated Arcomage record");
    return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8)
        | (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
}

Rows extractCards(const Bytes &executable)
{
    require(executable.size() == 1155072 && digest(executable) == ExecutableDigest,
        "Unsupported mm8.exe fingerprint; a verified binary profile is required");
    const Bytes data(executable.begin() + CardOffset,
        executable.begin() + CardOffset + Game::ArcomageLibrary::CardCount * CardStride);
    require(digest(data) == CardDigest, "Arcomage binary table fingerprint changed");
    Row header = {"Id", "Name", "Slot", "Resource", "NeedQuarry", "NeedMagic", "NeedDungeon",
        "NeedBricks", "NeedGems", "NeedRecruits", "Discardable", "Condition"};
    const Row fields = {"PlayAgain", "ExtraDraw", "PlayerQuarry", "PlayerMagic", "PlayerDungeon", "PlayerBricks",
        "PlayerGems", "PlayerRecruits", "PlayerBuildings", "PlayerWall", "PlayerTower", "EnemyQuarry", "EnemyMagic",
        "EnemyDungeon", "EnemyBricks", "EnemyGems", "EnemyRecruits", "EnemyBuildings", "EnemyWall", "EnemyTower",
        "BothQuarry", "BothMagic", "BothDungeon", "BothBricks", "BothGems", "BothRecruits", "BothBuildings",
        "BothWall", "BothTower"};
    for (const std::string &prefix : {"Primary", "Secondary"})
    {
        for (const std::string &field : fields)
        {
            header.push_back(prefix + field);
        }
    }
    Rows rows = {{"#ArcomageCardMode", "MM8BinaryV1"}, std::move(header)};
    std::set<std::string> names;
    std::set<uint32_t> slots;
    for (size_t id = 0; id < Game::ArcomageLibrary::CardCount; ++id)
    {
        const size_t offset = id * CardStride;
        size_t length = 0;
        while (length < 32 && data[offset + length] != 0)
        {
            require(data[offset + length] >= 32 && data[offset + length] < 127,
                "Unsupported Arcomage binary name encoding");
            ++length;
        }
        require(length > 0 && length < 32, "Missing Arcomage card name terminator");
        for (size_t index = length; index < 32; ++index)
        {
            require(data[offset + index] == 0, "Nonzero Arcomage name padding");
        }
        const std::string name(data.begin() + offset, data.begin() + offset + length);
        const uint32_t slot = read32(data, offset + 32);
        require(names.insert(name).second && slots.insert(slot).second && slot <= 113
            && data[offset + 106] == 0 && data[offset + 107] == 0, "Invalid Arcomage identity/padding");
        Row row = {std::to_string(id), name, std::to_string(slot)};
        for (size_t column = 36; column < 44; ++column)
        {
            row.push_back(std::to_string(data[offset + column]));
        }
        row.push_back(std::to_string(read32(data, offset + 44)));
        for (size_t column = 48; column < 106; ++column)
        {
            const int value = data[offset + column];
            row.push_back(std::to_string(value < 128 ? value : value - 256));
        }
        require(row.size() == 70, "Arcomage binary field count changed");
        rows.push_back(std::move(row));
    }
    return rows;
}

std::string playerJson(const Game::ArcomagePlayerState &player)
{
    std::ostringstream text;
    text << '[' << player.tower << ',' << player.wall << ',' << player.quarry << ',' << player.magic << ','
        << player.dungeon << ',' << player.bricks << ',' << player.gems << ',' << player.recruits << ']';
    return text.str();
}

std::string verifyCards(const Game::ArcomageLibrary &library)
{
    using namespace Game;
    ArcomageRules rules;
    std::ostringstream text;
    text << "{\"state_fields\":[\"tower\",\"wall\",\"quarry\",\"magic\",\"dungeon\",\"bricks\",\"gems\","
        << "\"recruits\"],\"plays\":[";
    size_t count = 0;
    // Exercise the actual imported deck with lower, equal and higher player stats.
    for (int variant = 0; variant < 3; ++variant)
    {
        for (const ArcomageCardDefinition &card : library.cards)
        {
            ArcomageState state;
            require(rules.initializeMatch(library, library.tavernRulesByHouseId.begin()->first,
                "Player", "Opponent", 17, state), "Cannot initialize Arcomage card check");
            state.winTower = state.winResources = 10000;
            for (size_t index = 0; index < 2; ++index)
            {
                ArcomagePlayerState &player = state.players[index];
                const int value = index == 0 ? (variant + 1) * 10 : 20;
                player.tower = player.wall = value;
                player.quarry = player.magic = player.dungeon = value;
                player.bricks = player.gems = player.recruits = 100;
                player.hand.fill(-1);
            }
            state.players[0].hand[0] = card.id;
            const std::string beforePlayer = playerJson(state.players[0]);
            const std::string beforeEnemy = playerJson(state.players[1]);
            require(rules.playCard(library, 0, state), "Imported Arcomage card could not be played");
            text << (count++ == 0 ? "" : ",") << "{\"id\":" << card.id << ",\"variant\":" << variant
                << ",\"before\":[" << beforePlayer << ',' << beforeEnemy << "],\"after\":["
                << playerJson(state.players[0]) << ',' << playerJson(state.players[1]) << "]}";
        }
    }
    text << "],\"tavern_matches\":[";
    count = 0;
    std::vector<uint32_t> houseIds;
    for (const auto &[id, rule] : library.tavernRulesByHouseId)
    {
        houseIds.push_back(id);
    }
    std::sort(houseIds.begin(), houseIds.end());
    ArcomageAi ai;
    for (uint32_t id : houseIds)
    {
        ArcomageState state;
        require(rules.initializeMatch(library, id, "Player", "Opponent", id + 1, state),
            "Cannot initialize imported Arcomage tavern");
        size_t actions = 0;
        while (!state.result.finished && actions < 2000)
        {
            require(rules.startTurn(library, state), "Arcomage turn start failed");
            if (state.result.finished)
            {
                break;
            }
            const ArcomageAi::Action action = ai.chooseAction(library, state);
            const bool applied = action.kind == ArcomageAi::ActionKind::PlayCard
                ? rules.playCard(library, action.handIndex, state)
                : action.kind == ArcomageAi::ActionKind::DiscardCard
                    && rules.discardCard(library, action.handIndex, state);
            require(applied, "Arcomage AI match has no legal action");
            ++actions;
        }
        require(state.result.finished, "Arcomage AI match exceeded action limit");
        text << (count++ == 0 ? "" : ",") << "{\"house\":" << id << ",\"actions\":" << actions
            << ",\"victory_kind\":" << int(state.result.victoryKind) << '}';
    }
    text << "]}\n";
    return text.str();
}
}

void convertArcomageResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
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
            "Arcomage output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Arcomage output already exists");
    const Rows cards = extractCards(read(root, "mm8.exe"));
    const std::string cardText = serializeTextRows(cards);
    require(parse(cardText) == cards, "Arcomage serialization changed source fields");
    MapStats maps;
    HouseTable houses;
    MergedHouseRuleTable houseRules;
    MergedTransportLocationTable locations;
    require(maps.loadFromRows(readRows(assets, "map_stats.txt"))
        && houses.loadFromRows(readRows(assets, "house_data.txt"))
        && houseRules.loadFromRows(readRows(assets, "house_rules.txt"))
        && locations.loadFromRows(readRows(assets, "transport_locations.txt"))
        && houses.applyHouseRules(houseRules, locations, maps), "Invalid Arcomage house rule dependency");
    ArcomageLoader loader;
    require(loader.loadFromHouseRules(houseRules, houses, parse(cardText)), "Arcomage typed loading failed");
    const ArcomageLibrary &library = loader.library();
    const std::string checks = verifyCards(library);
    std::vector<std::filesystem::path> archives;
    for (const std::string &name : {"mm7.icons.lod", "mm8.D.lod", "mmmerge.D.lod"})
    {
        archives.push_back(checkedPath(root, "Data/" + name));
    }
    const bool chinese = std::filesystem::exists(root / "Data/10 LocZHCN.EnglishD.lod");
    if (chinese)
    {
        archives.push_back(checkedPath(root, "Data/10 LocZHCN.EnglishD.lod"));
    }
    require(std::filesystem::create_directory(output), "Cannot create fresh Arcomage output");
    try
    {
        const std::vector<ConvertedLodImage> images = convertLodImages(archives,
            {"sprites.pcx", "layout.pcx"}, output / "engine/icons");
        require(images.size() == 2 && images[0].width == 960 && images[0].height >= 1756
            && images[1].width == 640 && images[1].height == 480, "Unsupported Arcomage image layout");
        std::ostringstream report;
        report << "{\"schema\":1,\"stage\":\"arcomage\",\"runtime_ready\":false,\"cards\":" << library.cards.size()
            << ",\"binary_mode\":\"MM8BinaryV1\",\"exe_sha256\":\"" << ExecutableDigest
            << "\",\"cards_sha256\":\"" << CardDigest << "\",\"executable_executed\":false,\"tavern_rules\":"
            << library.tavernRulesByHouseId.size() << ",\"typed_validation\":true,\"card_play_checks\":306"
            << ",\"chinese_card_art\":"
            << (images[0].archive == "10 LocZHCN.EnglishD.lod" ? "true" : "false") << ",\"images\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            const ConvertedLodImage &image = images[index];
            const Bytes bytes = read(output, "engine/icons/" + image.name);
            const std::optional<Engine::ImagePixelsBgra> pixels = Engine::decodeImagePixelsBgra(bytes, image.name);
            require(pixels && pixels->width == image.width && pixels->height == image.height
                && pixels->pixels.size() == size_t(image.width) * image.height * 4,
                "Arcomage image failed runtime decoding");
            report << (index == 0 ? "" : ",") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << ",\"bgra_sha256\":\"" << digest(pixels->pixels) << "\"}";
        }
        report << "],\"patched_windows_parity\":false,\"rendered_match_validated\":false,"
            << "\"full_gameplay_loop\":false}\n";
        const std::array<std::pair<std::string, std::string>, 3> files = {{
            {"engine/data_tables/arcomage_cards.txt", cardText}, {"arcomage_checks.json", checks},
            {"arcomage_conversion.json", report.str()}}};
        for (const auto &[name, text] : files)
        {
            writeConvertedResource(output / name, Bytes(text.begin(), text.end()));
        }
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
