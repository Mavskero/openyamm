#include "doctest/doctest.h"

#include "EventSupportSource.h"
#include "engine/TextTable.h"
#include "game/content/BreachAlvarCompanionProfile.h"
#include "game/events/EventRuntime.h"
#include "game/party/Party.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/ClassMultiplierTable.h"
#include "tools/mmerge_import/BreachAlvarCompanionResources.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>

using namespace OpenYAMM::Game;
using namespace OpenYAMM::Import;

namespace
{
constexpr std::array<const char *, 4> SourcePaths{
    "Scripts/Maps/BrAlvar.lua", "Scripts/Global/Quest_CrossContinents.lua",
    "Data/Tables/Character portraits.txt", "Data/Tables/Class Extra.txt"};

std::filesystem::path sourceRoot()
{
    return std::filesystem::path(OPENYAMM_SOURCE_DIR).parent_path()
        / "local-data/validation/mm8-runtime-076/source-001";
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE_MESSAGE(input.good(), path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write(const std::filesystem::path &path, const std::string &text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
    REQUIRE(output.good());
}

std::filesystem::path fixture(bool complete = true)
{
    const char *pTemporaryRoot = std::getenv("TMPDIR");
    REQUIRE_MESSAGE(pTemporaryRoot != nullptr, "Breach Alvar tests require external-storage TMPDIR");
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::path(pTemporaryRoot)
        / ("breach-alvar-resources-149-" + std::to_string(nonce));
    REQUIRE_FALSE(std::filesystem::exists(root));
    std::filesystem::create_directories(root);
    if (complete)
    {
        for (const char *pPath : SourcePaths)
        {
            write(root / pPath, read(sourceRoot() / pPath));
        }
    }
    return root;
}

BreachAlvarCompanionResources resources()
{
    const auto result = loadBreachAlvarCompanionResources(sourceRoot());
    REQUIRE(result.has_value());
    return *result;
}

std::vector<std::vector<std::string>> sourceRows(const char *pPath)
{
    const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(read(sourceRoot() / pPath));
    REQUIRE(table.has_value());
    std::vector<std::vector<std::string>> result;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        if (table->getRow(index) != std::vector<std::string>{""})
        {
            result.push_back(table->getRow(index));
        }
    }
    return result;
}

struct OriginalPresentation
{
    CharacterDollTable dolls;
    ClassMultiplierTable classes;
    std::vector<std::vector<std::string>> portraitRows = sourceRows(SourcePaths[2]);
    std::vector<std::vector<std::string>> classRows = sourceRows(SourcePaths[3]);

    OriginalPresentation()
    {
        REQUIRE(dolls.loadCharacterRows(portraitRows));
        REQUIRE(classes.applyClassExtraRows(classRows));
    }
};

std::string changed(std::string text, const std::string &before, const std::string &after)
{
    const size_t position = text.find(before);
    REQUIRE(position != std::string::npos);
    text.replace(position, before.size(), after);
    return text;
}
}

TEST_CASE("Breach Alvar companion source gates bind all four audited inputs")
{
    const auto content = resources();
    const auto &profile = content.profile;
    std::string error;
    CHECK_MESSAGE(validBreachAlvarCompanionProfile(profile, error), error);
    CHECK(profile.sourceSha256 == std::array<std::string, 4>{
        "947567325601e0e120c03125b5f3087c15d311391c80c1949312e7a6feadc7a3",
        "0790af9b1b387b7db901f0b568593df725516e823a86fa0edb2d1fde459bce49",
        "a15cef9da5d78ce9f7c58a3517b81675260b2f16ac73f850a55fc0dc9b5bd456",
        "b3a27af317b8418644c90ca1a4209509553b1d6951e5a09c8d70fbf54650a48a"});
    CHECK(profile.canonicalMapKey == "mmmerge:bralvar.odm");
    CHECK(profile.requiredPartySize == 5);
    CHECK(profile.npcIds == std::array<uint32_t, 4>{772, 773, 774, 775});
    CHECK(profile.houseIds == std::array<uint32_t, 4>{0, 705, 706, 707});
    CHECK(profile.initialTopicId == 1789);
    CHECK(profile.hintTopicId == 1793);
    CHECK(profile.hintExperience == 0);
    CHECK(profile.hintsFollowEncounterOrder);
    CHECK(profile.friendActor == BreachAlvarFriendActorRules{772, -20633, -14746, 4864,
        50, 9999, 5, false, 1, 15, true, true, true, {{0, 0}, {25, 4}, {150, 4}, {217, 4}}});
    CHECK(profile.raceRules == std::vector<BreachAlvarRaceAppearanceRule>{
        {1, 417}, {3, 66}, {4, 60}, {5, 72}, {6, 290}});
    CHECK(profile.classRules == std::vector<BreachAlvarClassAppearanceRule>{
        {1, 204, 477, std::nullopt, 0}, {2, 219, 219, 7, 216}, {5, 633, 519, std::nullopt, 0},
        {6, 261, 261, std::nullopt, 0}, {9, 261, 261, std::nullopt, 0}, {8, 585, 585, std::nullopt, 0},
        {10, 258, 258, std::nullopt, 0}, {11, 408, 15, std::nullopt, 0},
        {14, 293, 420, std::nullopt, 0}, {15, 293, 420, std::nullopt, 0}});
}

TEST_CASE("Breach Alvar companion source rejects partial changed and escaping input")
{
    CHECK_FALSE(loadBreachAlvarCompanionResources(fixture(false)).has_value());
    for (const char *pPath : SourcePaths)
    {
        CAPTURE(pPath);
        const std::filesystem::path root = fixture(false);
        for (const char *pOther : SourcePaths)
        {
            if (std::string(pPath) != pOther)
            {
                write(root / pOther, read(sourceRoot() / pOther));
            }
        }
        CHECK_THROWS_AS(loadBreachAlvarCompanionResources(root), std::runtime_error);
        write(root / pPath, read(sourceRoot() / pPath) + "\n");
        CHECK_THROWS_AS(loadBreachAlvarCompanionResources(root), std::runtime_error);
        write(root / pPath, "");
        CHECK_THROWS_AS(loadBreachAlvarCompanionResources(root), std::runtime_error);
    }
    const std::filesystem::path escape = fixture(false);
    std::filesystem::create_directories(escape / "Scripts/Maps");
    std::filesystem::create_symlink(sourceRoot() / SourcePaths[0], escape / SourcePaths[0]);
    CHECK_THROWS_AS(loadBreachAlvarCompanionResources(escape), std::runtime_error);
    CHECK(loadBreachAlvarCompanionResources(fixture()).has_value());
}

TEST_CASE("Breach Alvar runtime content round trips without source files or duplicated rule tables")
{
    const auto profile = resources().profile;
    const std::string text = serializeBreachAlvarCompanionProfile(profile);
    std::string error;
    const auto loaded = loadBreachAlvarCompanionProfileText(text, error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK(*loaded == profile);
    CHECK(serializeBreachAlvarCompanionProfile(*loaded) == text);
    for (size_t position = text.find('\n'); position != std::string::npos; position = text.find('\n', position + 1))
    {
        if (text.substr(0, position).ends_with("end"))
        {
            continue;
        }
        CHECK_FALSE(loadBreachAlvarCompanionProfileText(text.substr(0, position), error));
        CHECK_FALSE(error.empty());
    }
    const std::vector<std::string> corruptions{
        "", std::string(128 * 1024 + 1, 'x'), text + "unknown 1\n",
        changed(text, "companions 1", "companions 2"), changed(text, "party 5", "party 4"),
        changed(text, "races 5", "races 4294967295"), changed(text, "portraits 77", "portraits -1"),
        changed(text, "classes 51", "classes 4294967296"), changed(text, "races 5", "races 5x"),
        changed(text, "restore 1 15 1 1 1", "restore 1 15 2 1 1"),
        changed(text, "kinds 10", "races 10"), changed(text, "mmmerge:bralvar.odm", "mmmerge:breach.odm")};
    for (const std::string &bad : corruptions)
    {
        CHECK_FALSE(loadBreachAlvarCompanionProfileText(bad, error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("Breach Alvar runtime profile rejects ambiguous missing and invalid content")
{
    const auto profile = resources().profile;
    std::vector<BreachAlvarCompanionProfile> bad(14, profile);
    bad[0].raceRules[1].raceId = bad[0].raceRules[0].raceId;
    bad[1].classRules[1].classKind = bad[1].classRules[0].classKind;
    bad[2].classes[1].classId = bad[2].classes[0].classId;
    bad[3].portraits[1].characterDataId = bad[3].portraits[0].characterDataId;
    bad[4].classRules[1].overrideClassId = 49; // Original reserved class is not an active class.
    bad[5].classRules[0].overrideMonsterId = 99; // Missing override ID must not imply a rule.
    bad[6].raceRules[0].monsterId = 0;
    bad[7].friendActor.hostility[1].targetMonsterKind = bad[7].friendActor.hostility[0].targetMonsterKind;
    bad[8].friendActor.fullHealthMultiplier = 0;
    bad[9].sourceSha256[0] = "missing";
    bad[10].classes.pop_back();
    bad[11].friendActor.restoreSkipsRemoved = false;
    bad[12].hintExperience = 1;
    bad[13].npcIds[0] = 1092;
    std::string error;
    for (const auto &value : bad)
    {
        CHECK_FALSE(validBreachAlvarCompanionProfile(value, error));
        CHECK_FALSE(error.empty());
        CHECK_THROWS_AS(serializeBreachAlvarCompanionProfile(value), std::invalid_argument);
    }
}

TEST_CASE("Breach Alvar actual Face and raw class ID follow every audited source appearance branch")
{
    const auto profile = resources().profile;
    OriginalPresentation original;
    // Source-derived independent expected pairs indexed by raw Class Extra Kind 0..16.
    const std::array<std::array<uint32_t, 2>, 17> byKind{{
        {261, 261}, {204, 477}, {219, 219}, {261, 261}, {261, 261}, {633, 519},
        {261, 261}, {261, 261}, {585, 585}, {261, 261}, {258, 258}, {408, 15},
        {261, 261}, {261, 261}, {293, 420}, {293, 420}, {261, 261}}};
    const std::map<int32_t, uint32_t> byRace{{1, 417}, {3, 66}, {4, 60}, {5, 72}, {6, 290}};
    size_t resolvedCount = 0;
    size_t unusedCount = 0;
    for (size_t portrait = 1; portrait < original.portraitRows.size(); ++portrait)
    {
        const auto &row = original.portraitRows[portrait];
        const uint32_t rawFace = uint32_t(std::stoul(row[0]));
        const uint32_t sex = uint32_t(std::stoul(row[4]));
        const uint32_t npcPicture = uint32_t(std::stoul(row[24]));
        const int32_t race = std::stoi(row[25]);
        for (size_t index = 1; index < original.classRows.size(); ++index)
        {
            const auto &classRow = original.classRows[index];
            const uint32_t kind = uint32_t(std::stoul(classRow[1]));
            if (kind == 0)
            {
                continue;
            }
            const uint32_t classId = uint32_t(std::stoul(classRow[0]));
            Character character;
            character.characterDataId = rawFace + 1;
            character.className = classRow[3];
            // These unrelated Character fields must not replace Game.CharacterPortraits[v.Face].
            character.raceId = 999;
            character.sexId = 1 - sex;
            character.portraitPictureId = 9999;
            character.name = "Unchanged actual character";
            character.memberId = 123;
            character.health = 37;
            character.experience = 123456;
            std::string error;
            const auto appearance = resolveBreachAlvarCompanionAppearance(
                profile, character, original.dolls, original.classes, error);
            if (npcPicture == 0)
            {
                CHECK_FALSE(appearance);
                ++unusedCount;
                continue;
            }
            REQUIRE_MESSAGE(appearance.has_value(), error);
            const uint32_t classMonster = classId == 7 ? 216 : byKind.at(kind).at(sex);
            const uint32_t expectedMonster = byRace.contains(race) ? byRace.at(race) : classMonster;
            CHECK(appearance->originalFace == rawFace);
            CHECK(appearance->npcPictureId == npcPicture);
            CHECK(appearance->monsterId == expectedMonster);
            CHECK(appearance->monsterKind == expectedMonster / 3 + 1);
            CHECK(character.name == "Unchanged actual character");
            CHECK(character.memberId == 123);
            CHECK(character.health == 37);
            CHECK(character.experience == 123456);
            ++resolvedCount;
        }
    }
    CHECK(resolvedCount == 74 * 51);
    CHECK(unusedCount == 3 * 51);
}

TEST_CASE("Breach Alvar missing or stale actual presentation metadata is not guessed")
{
    const auto profile = resources().profile;
    OriginalPresentation original;
    Character character;
    character.characterDataId = 1;
    character.className = "Knight";
    std::string error;
    REQUIRE(resolveBreachAlvarCompanionAppearance(profile, character, original.dolls, original.classes, error));
    CHECK_FALSE(resolveBreachAlvarCompanionAppearance(profile, character, {}, original.classes, error));
    CHECK_FALSE(resolveBreachAlvarCompanionAppearance(profile, character, original.dolls, {}, error));
    for (uint32_t face : {0u, 29u, 30u, 31u, 78u})
    {
        character.characterDataId = face;
        CHECK_FALSE(resolveBreachAlvarCompanionAppearance(profile, character, original.dolls, original.classes, error));
    }
    character.characterDataId = 1;
    character.className = "Unknown class";
    CHECK_FALSE(resolveBreachAlvarCompanionAppearance(profile, character, original.dolls, original.classes, error));
    character.className = "Knight";
    for (size_t column : {4u, 24u, 25u})
    {
        auto rows = original.portraitRows;
        rows[1][column] = column == 24 ? "42" : "1";
        CharacterDollTable wrong;
        REQUIRE(wrong.loadCharacterRows(rows));
        CHECK_FALSE(resolveBreachAlvarCompanionAppearance(profile, character, wrong, original.classes, error));
    }
    auto wrongClasses = original.classRows;
    wrongClasses[17][1] = "1"; // Knight keeps raw ID16 but falsely claims Archer kind.
    ClassMultiplierTable wrong;
    REQUIRE(wrong.applyClassExtraRows(wrongClasses));
    CHECK_FALSE(resolveBreachAlvarCompanionAppearance(profile, character, original.dolls, wrong, error));
}

TEST_CASE("Breach Alvar request overlays preserve unrelated events and require no pretend request API")
{
    const auto content = resources();
    std::string error;
    const std::string support(EventSupportSource);
    const std::string mapBase = support + R"lua(
RegisterEvent(6, "Existing map event", function() evt.SetGlobalVar("ExistingMapEvent", 1) end)
RegisterMapOnLoadEvent(7, nil, function() evt.SetGlobalVar("ExistingLoad", 1) end)
)lua";
    const auto local = ScriptedEventProgram::loadFromLuaText(mapBase + content.mapLuaSource,
        "@br-alvar-additive-map", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(local.has_value(), error);
    CHECK(local->eventIds() == std::vector<uint16_t>{6, 7, 65469});
    CHECK(local->onLoadEventIds() == std::vector<uint16_t>{7, 65469});
    const std::string globalBase = support + R"lua(
RegisterGlobalEvent(1789, nil, function() evt.SetGlobalVar("OtherNpcJoin", 1) end)
RegisterCanShowTopic(1789, function() return evt.GetDialogueNpcId() == 42 end)
RegisterGlobalEvent(1793, nil, function() evt.SetGlobalVar("ExistingHint", 1) end)
)lua";
    const auto global = ScriptedEventProgram::loadFromLuaText(globalBase + content.globalLuaSource,
        "@br-alvar-additive-global", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(global.has_value(), error);
    CHECK(global->eventIds() == std::vector<uint16_t>{1789, 1793});
    CHECK(global->canShowTopicEventIds() == std::vector<uint16_t>{1789});
    CHECK(global->onLoadEventIds().empty());
    EventRuntime events;
    EventRuntimeState state;
    Party party;
    REQUIRE(events.executeEventById(local, global, 6, state, &party, nullptr));
    CHECK(state.namedGlobalVars.at("ExistingMapEvent") == 1);
    const auto npcContext = [&](uint32_t npcId)
    {
        state.pendingDialogueContext = EventRuntimeState::PendingDialogueContext{};
        state.pendingDialogueContext->kind = DialogueContextKind::NpcTalk;
        state.pendingDialogueContext->sourceId = npcId;
    };
    npcContext(42);
    CHECK(events.canShowTopic(global, 1789, state, &party, nullptr));
    REQUIRE(events.executeNpcTopicEventById(local, global, 1789, state, &party, nullptr));
    CHECK(state.namedGlobalVars.at("OtherNpcJoin") == 1);
    npcContext(41);
    CHECK_FALSE(events.canShowTopic(global, 1789, state, &party, nullptr));
    REQUIRE(events.executeNpcTopicEventById(local, global, 1793, state, &party, nullptr));
    CHECK(state.namedGlobalVars.at("ExistingHint") == 1);

    // New request handlers are deliberately not invoked here: no mock API can establish
    // scene authorization, actor preparation or Session publication. Main owns those tests.
    CHECK(content.mapLuaSource.find("evt.RequestBreachAlvarSplit()") != std::string::npos);
    CHECK(content.globalLuaSource.find("evt.RequestBreachAlvarCompanionReturn(npcId)") != std::string::npos);
    for (const std::string &script : {content.mapLuaSource, content.globalLuaSource})
    {
        CHECK(script.find("evt.Set") == std::string::npos);
        CHECK(script.find("evt.MoveNPC") == std::string::npos);
        CHECK(script.find("DismissCharacter") == std::string::npos);
        CHECK(script.find("HireCharacter") == std::string::npos);
        CHECK(script.find("RemoveMapEvent") == std::string::npos);
    }
    CHECK_FALSE(party.hasQuestBit(1714));
}
