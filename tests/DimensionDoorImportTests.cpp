#include "doctest/doctest.h"
#include "tools/mmerge_import/DimensionDoorResources.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

using namespace OpenYAMM::Import;

namespace
{
struct ExpectedMap
{
    std::string_view stem;
    std::string_view relativePath;
    std::string_view sha256;
};

constexpr std::string_view TravelScript = "Scripts/General/2_OutdoorTravels.lua";
constexpr std::string_view TimerScript = "Scripts/Core/timers.lua";
constexpr std::string_view TravelSha256 =
    "0691bb4fc1b5684000753ad6bea86e98cfc5f371cd8a9e346cd88f702de810d2";
constexpr std::string_view TimerSha256 =
    "04108d079d9605cdeb90601e8e9acf096f697e17e19ccaec8c15eb78380f2213";
constexpr std::array<ExpectedMap, 7> ExpectedMaps = {{
    {"out01", "Scripts/Maps/out01.lua",
        "5e5caae49ba76d8128181a695165384e4a8a526b56247fe58f4536659d2eb380"},
    {"out07", "Scripts/Maps/out07.lua",
        "3eb167089254b740051b4242d031065e7c68dc3fd882eb9dd83d9d33677549c4"},
    {"outb3", "Scripts/Maps/outb3.lua",
        "a90314e62c869d4672eea390dffa7ce1bd312652f2e0d6107d8133a40dad7448"},
    {"7out04", "Scripts/Maps/7out04.lua",
        "a3c2c6008c3cd49ee99548b1adae5af8f792e83d6cfb77adc63815a1e9311cb3"},
    {"out09", "Scripts/Maps/out09.lua",
        "3c960b47020cc711d2d71cb04ad7632f8dc22625bb0408acba8fd8f4ffe81ce9"},
    {"oute3", "Scripts/Maps/oute3.lua",
        "731955ac2b119b5ed6f32d7a01ff08ea82fac4cb7b60596b9fca80250f846e08"},
    {"out02", "Scripts/Maps/out02.lua",
        "36ae1b344bed64ccdeca91d969b847247db18299dc501133758b8baa521c83cf"},
}};

std::filesystem::path originalSource()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

class SourceFixture
{
public:
    explicit SourceFixture(std::string_view name)
    {
        const char *pTempDirectory = std::getenv("TMPDIR");
        if (pTempDirectory == nullptr || *pTempDirectory == '\0')
        {
            throw std::runtime_error("TMPDIR must point at the external-storage test directory");
        }

        const uint64_t nonce = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        m_container = std::filesystem::path(pTempDirectory)
            / ("openyamm_dimension_door_profile_" + std::string(name) + "_" + std::to_string(nonce));
        m_source = m_container / "source";
        std::filesystem::create_directories(m_source);
    }

    ~SourceFixture()
    {
        std::error_code error;
        std::filesystem::remove_all(m_container, error);
    }

    const std::filesystem::path &source() const
    {
        return m_source;
    }

    std::filesystem::path pathOutsideSource(std::string_view name) const
    {
        return m_container / name;
    }

    std::filesystem::path copyOriginal(std::string_view relativePath)
    {
        const std::filesystem::path destination = m_source / relativePath;
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(originalSource() / relativePath, destination);
        return destination;
    }

    std::filesystem::path write(std::string_view relativePath, std::string_view text)
    {
        const std::filesystem::path destination = m_source / relativePath;
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output(destination, std::ios::binary);
        output << text;
        return destination;
    }

private:
    std::filesystem::path m_container;
    std::filesystem::path m_source;
};

void copyRecognizedScripts(SourceFixture &fixture, std::optional<std::string_view> skippedMap = std::nullopt)
{
    fixture.copyOriginal(TravelScript);
    fixture.copyOriginal(TimerScript);
    for (const ExpectedMap &map : ExpectedMaps)
    {
        if (!skippedMap || map.stem != *skippedMap)
        {
            fixture.copyOriginal(map.relativePath);
        }
    }
}

std::map<std::string, const DimensionDoorMapOverlay *> overlaysByStem(const DimensionDoorProfile &profile)
{
    std::map<std::string, const DimensionDoorMapOverlay *> overlays;
    for (const DimensionDoorMapOverlay &overlay : profile.maps)
    {
        REQUIRE(overlays.emplace(overlay.mapStem, &overlay).second);
    }
    return overlays;
}
}

TEST_CASE("Dimension Door profile recognizes the complete audited source set")
{
    const std::optional<DimensionDoorProfile> profile = loadDimensionDoorProfile(originalSource());
    REQUIRE(profile.has_value());
    CHECK_EQ(profile->sourceSha256, TravelSha256);
    CHECK_EQ(profile->timerSourceSha256, TimerSha256);

    const std::map<std::string, const DimensionDoorMapOverlay *> overlays = overlaysByStem(*profile);
    REQUIRE_EQ(overlays.size(), ExpectedMaps.size());
    for (const ExpectedMap &expected : ExpectedMaps)
    {
        CAPTURE(expected.stem);
        const auto found = overlays.find(std::string(expected.stem));
        REQUIRE(found != overlays.end());
        CHECK_EQ(found->second->sourceSha256, expected.sha256);
        CHECK_FALSE(found->second->luaSource.empty());
    }
}

TEST_CASE("Dimension Door profile stays absent without its controlling travel script")
{
    SourceFixture fixture("absent_general");
    fixture.copyOriginal(TimerScript);
    fixture.copyOriginal(ExpectedMaps.front().relativePath);
    CHECK_FALSE(loadDimensionDoorProfile(fixture.source()).has_value());
}

TEST_CASE("Dimension Door profile omits an unavailable recognized map without weakening the other source gates")
{
    SourceFixture fixture("missing_map");
    copyRecognizedScripts(fixture, "out09");

    const std::optional<DimensionDoorProfile> profile = loadDimensionDoorProfile(fixture.source());
    REQUIRE(profile.has_value());
    const std::map<std::string, const DimensionDoorMapOverlay *> overlays = overlaysByStem(*profile);
    CHECK_EQ(overlays.size(), ExpectedMaps.size() - 1);
    CHECK_FALSE(overlays.contains("out09"));
    CHECK(overlays.contains("out01"));
    CHECK(overlays.contains("oute3"));
}

TEST_CASE("Dimension Door profile rejects modified controlling and recognized map scripts")
{
    SUBCASE("travel script")
    {
        SourceFixture fixture("modified_general");
        const std::filesystem::path script = fixture.copyOriginal(TravelScript);
        std::ofstream(script, std::ios::binary | std::ios::app) << "\n-- modified\n";
        CHECK_THROWS_AS(loadDimensionDoorProfile(fixture.source()), std::runtime_error);
    }

    SUBCASE("timer core")
    {
        SourceFixture fixture("modified_timer");
        fixture.copyOriginal(TravelScript);
        const std::filesystem::path script = fixture.copyOriginal(TimerScript);
        std::ofstream(script, std::ios::binary | std::ios::app) << "\n-- modified\n";
        CHECK_THROWS_AS(loadDimensionDoorProfile(fixture.source()), std::runtime_error);
    }

    SUBCASE("recognized map")
    {
        SourceFixture fixture("modified_map");
        fixture.copyOriginal(TravelScript);
        fixture.copyOriginal(TimerScript);
        const std::filesystem::path script = fixture.copyOriginal(ExpectedMaps.front().relativePath);
        std::ofstream(script, std::ios::binary | std::ios::app) << "\n-- modified\n";
        CHECK_THROWS_AS(loadDimensionDoorProfile(fixture.source()), std::runtime_error);
    }
}

TEST_CASE("Dimension Door profile ignores an unrelated script without expanding its supported map set")
{
    SourceFixture fixture("unrelated_script");
    copyRecognizedScripts(fixture);
    fixture.write("Scripts/Maps/custom_dimension_test.lua", "return 'unsupported and unrelated'\n");

    const std::optional<DimensionDoorProfile> profile = loadDimensionDoorProfile(fixture.source());
    REQUIRE(profile.has_value());
    CHECK_EQ(profile->maps.size(), ExpectedMaps.size());
    CHECK_FALSE(overlaysByStem(*profile).contains("custom_dimension_test"));
}

TEST_CASE("Dimension Door profile rejects a controlling-script symlink that escapes the source root")
{
    SourceFixture fixture("escaping_symlink");
    const std::filesystem::path outside = fixture.pathOutsideSource("outside_travel.lua");
    std::filesystem::copy_file(originalSource() / TravelScript, outside);
    const std::filesystem::path link = fixture.source() / TravelScript;
    std::filesystem::create_directories(link.parent_path());
    std::filesystem::create_symlink(outside, link);

    CHECK_THROWS_AS(loadDimensionDoorProfile(fixture.source()), std::runtime_error);
}
