#include "doctest/doctest.h"

#include "tools/LegacyLuaExport.h"
#include "tools/legacy_events/EvtProgram.h"
#include "tools/legacy_events/StrTable.h"
#include "tools/mmerge_import/LodResources.h"
#include "tools/mmerge_import/TextResources.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
std::filesystem::path originalArchive()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001/Data/mmmerge.T.lod";
}

std::string originalGlobalLua()
{
    EvtProgram evtProgram;
    REQUIRE(evtProgram.loadFromBytes(readLodText(originalArchive(), "Global.EVT")));

    StrTable strTable;

    LegacyLuaExportLookups lookups = {};
    return generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        LegacyLuaExportScope::Global,
        LegacyEventVersion::Mm8);
}

std::string_view eventSection(const std::string &lua, uint32_t eventId, uint32_t nextEventId)
{
    const std::string beginMarker = "RegisterGlobalEvent(" + std::to_string(eventId) + ",";
    const std::string endMarker = "RegisterGlobalEvent(" + std::to_string(nextEventId) + ",";
    const size_t begin = lua.find(beginMarker);
    const size_t end = lua.find(endMarker, begin);

    REQUIRE(begin != std::string::npos);
    REQUIRE(end != std::string::npos);
    return std::string_view(lua).substr(begin, end - begin);
}
}

TEST_CASE("original MM8 blackmail EVT and source text preserve acceptance delivery and reward semantics")
{
    const std::vector<uint8_t> originalNpcText = readLodText(originalArchive(), "global.txt");
    REQUIRE_FALSE(originalNpcText.empty());
    size_t restoredGlyphs = 0;
    const std::string originalGlobalText = decodeLegacyText(originalNpcText, false, restoredGlyphs);
    CHECK(originalGlobalText.starts_with("Global Text\t"));

    const std::string lua = originalGlobalLua();
    const std::string_view acceptance = eventSection(lua, 29, 30);
    const std::string_view delivery = eventSection(lua, 32, 33);

    CHECK(acceptance.find("SetQBit(QBit(9))") != std::string_view::npos);
    CHECK(acceptance.find("AddValue(InventoryItem(742), 742)") != std::string_view::npos);
    CHECK(acceptance.find("SetQBit(QBit(222))") != std::string_view::npos);
    CHECK(acceptance.find("SetAutonote(493)") != std::string_view::npos);
    CHECK(acceptance.find("AddValue(Experience, 12000)") != std::string_view::npos);
    CHECK(acceptance.find("SetQBit(QBit(24))") != std::string_view::npos);
    CHECK(acceptance.find("ClearQBit(QBit(284))") != std::string_view::npos);
    CHECK(acceptance.find("SetQBit(QBit(11))") != std::string_view::npos);

    CHECK(delivery.find("SetQBit(QBit(10))") != std::string_view::npos);
    CHECK(delivery.find("ClearQBit(QBit(9))") != std::string_view::npos);
    CHECK(delivery.find("SetQBit(QBit(284))") != std::string_view::npos);
    CHECK(delivery.find("RemoveItem(742)") != std::string_view::npos);
    CHECK(delivery.find("ClearQBit(QBit(222))") != std::string_view::npos);
    CHECK(delivery.find("evt.SetNPCTopic(4, 0, 33)") != std::string_view::npos);
    CHECK(delivery.find("AddValue(History(3), 0)") != std::string_view::npos);
    CHECK(delivery.find("IsActorKilled") == std::string_view::npos);

    const size_t canShowBegin = lua.find("RegisterCanShowTopic(32");
    const size_t canShowEnd = lua.find("RegisterGlobalEvent(33", canShowBegin);
    REQUIRE(canShowBegin != std::string::npos);
    REQUIRE(canShowEnd != std::string::npos);
    const std::string_view canShow =
        std::string_view(lua).substr(canShowBegin, canShowEnd - canShowBegin);
    CHECK(canShow.find("HasItem(742)") != std::string_view::npos);
    CHECK(canShow.find("IsActorKilled") == std::string_view::npos);
}

TEST_CASE("MM7 and MM8 binary history selectors preserve every original journal index")
{
    for (const LegacyEventVersion version : {LegacyEventVersion::Mm7, LegacyEventVersion::Mm8})
    {
        const uint16_t base = version == LegacyEventVersion::Mm8 ? 0x116 : 0x114;
        for (uint16_t index = 1; index <= 29; ++index)
        {
            const uint16_t selector = base + index - 1;
            const std::vector<uint8_t> bytes = {
                10, 1, 0, 0, 16, uint8_t(selector), uint8_t(selector >> 8), 0, 0, 0, 0,
                5, 1, 0, 1, 1, 0};
            EvtProgram program;
            REQUIRE(program.loadFromBytes(bytes));
            const std::string lua = generateLegacyEventLuaChunk(program, {}, {}, LegacyLuaExportScope::Map, version);
            CHECK(lua.find("AddValue(History(" + std::to_string(index) + "), 0)") != std::string::npos);
        }
    }
}
