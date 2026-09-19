#include "engine/AssetFileSystem.h"
#include "engine/TextTable.h"
#include "game/maps/MapAssetLoader.h"
#include "game/tables/MapStats.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/ObjectTable.h"

#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    if (argc != 3)
    {
        std::cerr << "Usage: openyamm_core_check COMBINED_ASSETS NEW_CHECK_DIRECTORY\n";
        return 2;
    }
    try
    {
        const std::filesystem::path assets = std::filesystem::canonical(argv[1]);
        const std::filesystem::path output = std::filesystem::weakly_canonical(argv[2]);
        const std::filesystem::path relative = output.lexically_relative(assets);
        require(!relative.empty() && *relative.begin() == ".." && !std::filesystem::exists(output),
            "Check output must be new and outside the asset root");
        Engine::AssetFileSystem files;
        require(files.initialize(assets, assets, Engine::AssetScaleTier::X1, "mm6"), "Cannot mount core assets");
        const auto readRows = [&](const char *pName)
        {
            const auto contents = files.readTextFile(std::string("engine/data_tables/") + pName);
            const auto table = contents ? Engine::TextTable::parseTabSeparated(*contents) : std::nullopt;
            require(table.has_value(), std::string("Cannot reload table: ") + pName);
            std::vector<std::vector<std::string>> rows;
            for (size_t index = 0; index < table->getRowCount(); ++index)
            {
                rows.push_back(table->getRow(index));
            }
            return rows;
        };
        Game::MapStats maps;
        Game::MonsterTable monsters;
        Game::ObjectTable objects;
        require(maps.loadFromRows(readRows("map_stats.txt")), "Map registry failed to reload");
        require(monsters.loadEntriesFromRows(readRows("monster_descriptors.txt"))
            && monsters.loadDisplayNamesFromRows(readRows("monster_data.txt"))
            && monsters.loadStatsFromRows(readRows("monster_data.txt"))
            && monsters.loadDeathDropsFromRows(readRows("monster_death_drops.txt"))
            && monsters.loadUniqueNamesFromRows(readRows("english/place_mon.txt"))
            && monsters.loadRelationsFromRows(readRows("hostile.txt")), "Monster tables failed to reload");
        require(objects.loadRows(readRows("object_list.txt")), "Object table failed to reload");
        const Game::MapStatsEntry *pMap = maps.findByFileName("oute3.odm");
        require(pMap && pMap->id == 151 && pMap->worldId == "mm6"
            && pMap->canonicalId == "world.mm6.map.oute3", "New Sorpigal registry identity changed");
        // This is a data/geometry check without a renderer, party or gameplay
        // loop. Explicitly load the already converted original companion.
        const auto map = Game::MapAssetLoader().load(files, *pMap, monsters, objects,
            Game::MapLoadPurpose::HeadlessGameplay, {.allowSceneYml = false, .allowLegacyCompanion = true});
        require(map && map->outdoorMapData && map->outdoorMapDeltaData,
            "New Sorpigal geometry/companion failed to load");
        const auto &actors = map->outdoorMapDeltaData->actors;
        require(actors.size() == 43, "Authored actor count changed");
        std::set<int16_t> actorKinds;
        for (const Game::MapDeltaActor &actor : actors)
        {
            const auto *pEntry = monsters.findById(actor.monsterInfoId);
            const auto *pStats = monsters.findStatsById(actor.monsterInfoId);
            require(pEntry && pStats && pEntry->internalName == pStats->pictureName
                && pStats->hitPoints > 0 && !pStats->name.empty(),
                "Companion actor did not resolve merged appearance/stats/name");
            actorKinds.insert(actor.monsterInfoId);
        }
        for (const auto &row : readRows("monster_death_drops.txt"))
        {
            if (row.size() < 6)
            {
                continue;
            }
            const auto &drops = monsters.deathDropsForMonsterId(static_cast<int16_t>(std::stoi(row[0])));
            require(drops.size() == 1 && drops[0].itemId == std::stoul(row[2])
                && drops[0].chancePercent == std::stoi(row[4]), "Typed death-drop identity or chance changed");
        }
        const auto objectRows = readRows("object_list.txt");
        for (size_t index = 0; index < objectRows.size(); ++index)
        {
            const auto *pObject = objects.get(static_cast<uint16_t>(index));
            require(pObject && pObject->objectId == std::stoi(objectRows[index][2])
                && objects.findDescriptionIdByObjectId(pObject->objectId) == index,
                "Object description index was renumbered");
        }
        require(std::filesystem::create_directory(output), "Cannot create check directory");
        std::ofstream report(output / "core-check.json");
        report << "{\"table_reload\":true,\"map_geometry_companion_load\":true,\"actor_stats_join\":true,"
            << "\"object_indices_preserved\":true,\"drop_rules_reloaded\":true,\"full_gameplay_loop\":false,"
            << "\"canonical_map_id\":\"world.mm6.map.oute3\",\"map_id\":" << pMap->id
            << ",\"maps\":" << maps.getEntries().size() << ",\"monsters\":" << monsters.statsEntries().size()
            << ",\"objects\":" << objects.entries().size() << ",\"actors\":" << actors.size()
            << ",\"actor_kinds\":" << actorKinds.size() << "}\n";
        report.close();
        require(bool(report), "Cannot write core check report");
        std::cout << "Core tables and New Sorpigal actor references passed; full gameplay is not yet ready.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Core check failed: " << error.what() << '\n';
        return 1;
    }
}
