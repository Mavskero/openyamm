#pragma once

#include "game/content/BreachAlvarCompanionProfile.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OpenYAMM::Import
{
inline constexpr std::string_view BreachAlvarMapSourcePath = "Scripts/Maps/BrAlvar.lua";
inline constexpr std::string_view BreachAlvarMapSourceSha256 =
    "947567325601e0e120c03125b5f3087c15d311391c80c1949312e7a6feadc7a3";

enum class BreachAlvarHouseNameSource
{
    House,
    Map,
};

struct BreachAlvarHousePresentationRule
{
    uint32_t eventId;
    uint32_t houseId;
    std::optional<uint32_t> pictureId;
    BreachAlvarHouseNameSource nameSource;
    uint32_t nameSourceId;
};

// BrAlvar.lua 237–255 changes Picture/Name; line 294 changes only House712.Name.
// Every destination retains its source HouseType and any presentation field not assigned by the script.
// The normal service importer verifies the source fingerprint before applying these global house facts.
inline constexpr std::array<BreachAlvarHousePresentationRule, 4> BreachAlvarHousePresentationRules{{
    {50, 705, 389, BreachAlvarHouseNameSource::House, 222},
    {51, 706, 119, BreachAlvarHouseNameSource::Map, 52},
    {52, 707, 38, BreachAlvarHouseNameSource::House, 773},
    {5, 712, std::nullopt, BreachAlvarHouseNameSource::House, 1246},
}};

struct BreachAlvarCompanionResources
{
    Game::BreachAlvarCompanionProfile profile;
    std::string mapLuaSource;
    std::string globalLuaSource;
};

// Source gates include the complete default mercenary-generation contract for v2 reserve preparation.
// They authorize this bounded content translation, never normal map admission.
// Serialize profile with Game::serializeBreachAlvarCompanionProfile into the engine package.
// Append mapLuaSource only to the controlled BrAlvar program; append globalLuaSource after Runaway Chaos.
std::optional<BreachAlvarCompanionResources> loadBreachAlvarCompanionResources(
    const std::filesystem::path &source);
}
