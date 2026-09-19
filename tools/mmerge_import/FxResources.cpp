#include "FxResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/events/EventRuntime.h"
#include "game/events/EvtEnums.h"
#include "game/tables/FaceAnimationTable.h"
#include "game/tables/IconFrameTable.h"
#include "game/tables/PortraitFrameTable.h"
#include "game/tables/PortraitFxEventTable.h"
#include "game/tables/SpellFxTable.h"
#include "game/tables/SpellTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
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
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "FX source escapes root");
    return path;
}

Bytes read(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = checkedPath(root, name);
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid FX source size");
    Bytes bytes(size);
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read FX source");
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
    require(table.has_value(), "Invalid FX table quoting");
    Rows rows;
    for (size_t index = 0; index < table->getRowCount(); ++index)
    {
        const Row &row = table->getRow(index);
        if (index + 1 == table->getRowCount() && row == Row{""})
        {
            continue;
        }
        rows.push_back(row);
    }
    return rows;
}

Rows readTable(const std::filesystem::path &root, const std::string &name)
{
    const Bytes bytes = read(root, "engine/data_tables/" + name);
    return parse(std::string(bytes.begin(), bytes.end()));
}

std::map<uint32_t, std::string> faceNames(const std::filesystem::path &root)
{
    const Bytes bytes = read(root, "Scripts/Core/ConstAndBits.lua");
    const std::string text(bytes.begin(), bytes.end());
    const std::string marker = "const.FaceAnimation = {";
    const size_t start = text.find(marker);
    const size_t end = text.find('}', start);
    require(start != std::string::npos && end != std::string::npos, "Missing source face enumeration");
    const std::string body = text.substr(start + marker.size(), end - start - marker.size());
    const std::regex pattern(R"(^\s*([A-Za-z][A-Za-z0-9_]*)\s*=\s*([0-9]{1,3}),\s*(?:--.*)?$)");
    std::istringstream lines(body);
    std::map<uint32_t, std::string> names;
    std::set<std::string> uniqueNames;
    for (std::string line; std::getline(lines, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t\r") == std::string::npos)
        {
            continue;
        }
        std::smatch match;
        require(std::regex_match(line, match, pattern), "Unsupported face enumeration syntax");
        const uint32_t id = std::stoul(match[2].str());
        const std::string name = match[1].str();
        require(id > 0 && id < 110 && names.emplace(id, name).second && uniqueNames.insert(name).second,
            "Invalid/duplicate source face event");
    }
    require(names.contains(49) && names.at(49) == "CastSpell"
        && names.contains(103) && names.at(103) == "Awaken", "Unsupported face enumeration identity");
    return names;
}

// Observed numeric outcomes of the supplied MM8 selector at VA 0x4a6fce.
// These are resource associations, not copied implementation instructions.
// Generic event codes 150-153 are not spell IDs and are handled separately.
const std::array<std::pair<uint32_t, const char *>, 37> SpellAnimations = {{
    {3, "spell03"}, {5, "spell05"}, {8, "spell03"}, {13, "spboost1"}, {14, "spell14"},
    {17, "spell17"}, {19, "spboost2"}, {25, "spell25"}, {27, "spboost2"}, {36, "spell36"},
    {38, "spell38"}, {45, "spboost1"}, {46, "spell46"}, {47, "spboost1"}, {49, "spheal1"},
    {50, "spboost2"}, {51, "spell51"}, {54, "spheal2"}, {55, "spell55"}, {57, "spheal1"},
    {58, "spell58"}, {61, "spheal2"}, {64, "spheal2"}, {67, "spheal1"}, {68, "spheal2"},
    {69, "spell69"}, {71, "spell71"}, {72, "spheal2"}, {73, "spell73"}, {74, "spheal2"},
    {75, "spell75"}, {77, "spheal3"}, {83, "spboost3"}, {85, "spboost3"}, {86, "spboost3"},
    {88, "spboost3"}, {99, "spheal3"}
}};

void writeTable(const std::filesystem::path &path, const Rows &rows)
{
    const std::string serialized = serializeTextRows(rows);
    require(parse(serialized) == rows, "FX serialization changed source fields");
    writeConvertedResource(path, Bytes(serialized.begin(), serialized.end()));
}

void validateEventActions(const Game::PortraitFxEventTable &table)
{
    using namespace Game;
    Party party;
    PartySeed seed;
    Character member;
    member.name = "FX check";
    member.className = "Knight";
    member.health = member.maxHealth = 40;
    seed.members = {member, member};
    party.seed(seed);
    require(party.members().size() >= 2, "Event FX diagnostic needs two members");
    EventRuntimeState state;
    const EventRuntime::VariableRef variable =
        EventRuntime::decodeVariable(static_cast<uint32_t>(EvtVariable::BaseMight));
    const int32_t original = EventRuntime::getVariableValue(state, variable, &party, 0);
    EventRuntime::setVariableValue(state, variable, original + 2, &party, {0});
    EventRuntime::addVariableValue(state, variable, 3, &party, {0});
    EventRuntime::setVariableValue(state, variable, original + 9, &party, {0});
    EventRuntime::subtractVariableValue(state, variable, 2, &party, {0});
    EventRuntime::addVariableValue(state, variable, -1, &party, {0});
    require(state.portraitFxRequests.size() == 5, "Distinct event effects were reordered or merged");
    const std::array<PortraitFxAction, 5> actions = {
        PortraitFxAction::Set, PortraitFxAction::Add, PortraitFxAction::Set,
        PortraitFxAction::Subtract, PortraitFxAction::Add};
    const std::array<std::string, 5> animations = {"spboost1", "spboost2", "spboost1", "spboost3", "spboost2"};
    for (size_t index = 0; index < actions.size(); ++index)
    {
        const EventRuntimeState::PortraitFxRequest &request = state.portraitFxRequests[index];
        const PortraitFxEventEntry *pEntry = table.findByKind(request.kind);
        require(request.action == actions[index] && request.memberIndices == std::vector<size_t>{0}
            && pEntry && pEntry->animationForAction(request.action) == animations[index],
            "Event operation lost its portrait effect");
    }
    require(EventRuntime::getVariableValue(state, variable, &party, 0) == original + 6,
        "Portrait effect tracking changed gameplay arithmetic");
    state.portraitFxRequests.clear();
    EventRuntime::addVariableValue(state, variable, 1, &party, {0});
    EventRuntime::addVariableValue(state, variable, 1, &party, {1});
    require(state.portraitFxRequests.size() == 1
        && state.portraitFxRequests[0].memberIndices == std::vector<size_t>({0, 1}),
        "Adjacent equal event effects lost member targeting");
    state.portraitFxRequests.clear();
    const int32_t current = EventRuntime::getVariableValue(state, variable, &party, 0);
    EventRuntime::setVariableValue(state, variable, current, &party, {0});
    require(state.portraitFxRequests.empty(), "Unchanged stats unexpectedly triggered FX");

    PortraitFxEventTable legacy;
    require(legacy.loadFromRows({{"StatIncrease", "Legacy", "original", "DrinkPotion"}}), "Legacy FX table failed");
    const PortraitFxEventEntry *pLegacy = legacy.findByKind(PortraitFxEventKind::StatIncrease);
    for (PortraitFxAction action : {PortraitFxAction::Default, PortraitFxAction::Set,
        PortraitFxAction::Add, PortraitFxAction::Subtract})
    {
        require(pLegacy && pLegacy->animationForAction(action) == "original"
            && pLegacy->faceAnimationId == FaceAnimationId::DrinkPotion, "Legacy FX semantics changed");
    }
}
}

void convertFxResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path base = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, base})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "FX output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "FX output already exists");
    require(!std::filesystem::exists(root / "Data/Tables/Face Animations.txt"),
        "Loose Face Animations override requires an explicit conversion profile");

    const Bytes executable = read(root, "mm8.exe");
    const std::string executableHash = digest(executable);
    require(executable.size() == 1155072
        && executableHash == "97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b",
        "Unsupported MM8 executable profile; no binary code is executed");
    const std::map<uint32_t, std::string> names = faceNames(root);

    Game::SpellTable spells;
    Game::IconFrameTable icons;
    Game::PortraitFrameTable portraits;
    require(spells.loadFromRows(readTable(base, "spells.txt"))
        && icons.loadRows(readTable(base, "icon_frame_data.txt"))
        && portraits.loadRows(readTable(base, "portrait_frame_data.txt")), "Missing/invalid FX prerequisite tables");

    Rows spellRows;
    for (const auto &[id, animation] : SpellAnimations)
    {
        const Game::SpellEntry *pSpell = spells.findById(id);
        require(pSpell && !pSpell->name.empty(), "Spell FX references an absent spell");
        const std::optional<size_t> frame = icons.findAnimationIdByName(animation);
        require(frame && icons.animationLengthTicks(*frame) > 0, "Spell FX references an absent animation");
        spellRows.push_back({std::to_string(id), pSpell->name, animation});
    }
    Game::SpellFxTable spellCheck;
    require(spellCheck.loadFromRows(parse(serializeTextRows(spellRows))), "Spell FX typed loading failed");
    for (const Row &row : spellRows)
    {
        const Game::SpellFxEntry *pEntry = spellCheck.findBySpellId(std::stoul(row[0]));
        require(pEntry && pEntry->spellName == row[1] && pEntry->animationName == row[2],
            "Spell FX typed loading changed fields");
    }

    // Positive direct notifications use the observed Add selector (151).
    // Event variable operations carry their own selector: Set=150, Add=151,
    // Subtract=152. Monster-special feedback uses 153 independently.
    Rows eventRows;
    const std::array<std::pair<const char *, Game::PortraitFxEventKind>, 7> kinds = {{
        {"Disease", Game::PortraitFxEventKind::Disease}, {"AutoNote", Game::PortraitFxEventKind::AutoNote},
        {"AwardGain", Game::PortraitFxEventKind::AwardGain},
        {"QuestComplete", Game::PortraitFxEventKind::QuestComplete},
        {"StatIncrease", Game::PortraitFxEventKind::StatIncrease},
        {"StatDecrease", Game::PortraitFxEventKind::StatDecrease},
        {"MonsterSpecial", Game::PortraitFxEventKind::MonsterSpecial}
    }};
    for (const auto &[name, kind] : kinds)
    {
        const bool special = kind == Game::PortraitFxEventKind::MonsterSpecial;
        const std::string normal = special ? "zapp"
            : kind == Game::PortraitFxEventKind::StatDecrease ? "spboost3" : "spboost2";
        eventRows.push_back({name, name, normal, "", special ? "" : "spboost1",
            special ? "" : "spboost2", special ? "" : "spboost3"});
    }
    Game::PortraitFxEventTable eventCheck;
    require(eventCheck.loadFromRows(parse(serializeTextRows(eventRows))), "Portrait event FX typed loading failed");
    size_t eventBindings = 0;
    for (const auto &[name, kind] : kinds)
    {
        const Game::PortraitFxEventEntry *pEntry = eventCheck.findByKind(kind);
        require(pEntry && pEntry->name == name && !pEntry->faceAnimationId, "Portrait FX event identity changed");
        for (Game::PortraitFxAction action : {Game::PortraitFxAction::Default, Game::PortraitFxAction::Set,
            Game::PortraitFxAction::Add, Game::PortraitFxAction::Subtract})
        {
            const std::optional<size_t> group = icons.findAnimationIdByName(pEntry->animationForAction(action));
            require(group && icons.animationLengthTicks(*group) > 0, "Portrait event references an absent animation");
            ++eventBindings;
        }
    }
    validateEventActions(eventCheck);

    Rows faceRows;
    std::vector<uint32_t> noExpressionIds;
    std::vector<uint32_t> unsupportedFaceIds;
    size_t expressionCount = 0;
    size_t soundSlots = 0;
    for (uint32_t id = 0; id < 110; ++id)
    {
        // MMExtension identifies VA 0x4fd660, 110 records of eight bytes:
        // two sound bytes, one reserved byte, five ordered expression bytes.
        const size_t offset = 0xfd660 + id * 8;
        require(executable[offset + 2] == 0, "Unexpected face record reserved byte");
        soundSlots += (executable[offset] != 0) + (executable[offset + 1] != 0);
        // The supplied executable's LookDown event points to expression 32,
        // which the supplied loose PFT does not define. Keep that discrepancy
        // explicit instead of inventing a portrait or silently using another.
        if (id == 64 && !portraits.findAnimationId(static_cast<Game::PortraitId>(32)))
        {
            unsupportedFaceIds.push_back(id);
            continue;
        }
        std::string expressionList;
        bool padding = false;
        for (size_t slot = 3; slot < 8; ++slot)
        {
            const uint32_t expression = executable[offset + slot];
            if (expression == 0)
            {
                padding = true;
                continue;
            }
            require(!padding, "Nontrailing face expression padding");
            require(portraits.findAnimationId(static_cast<Game::PortraitId>(expression)).has_value(),
                "Face event references an absent portrait group");
            expressionList += (expressionList.empty() ? "" : ",") + std::to_string(expression);
            ++expressionCount;
        }
        if (expressionList.empty())
        {
            noExpressionIds.push_back(id);
            continue;
        }
        faceRows.push_back({std::to_string(id),
            names.contains(id) ? names.at(id) : std::to_string(id), expressionList});
    }
    Game::FaceAnimationTable faceCheck;
    require(faceCheck.loadFromRows(parse(serializeTextRows(faceRows))), "Face typed loading failed");
    for (const Row &row : faceRows)
    {
        const uint32_t id = std::stoul(row[0]);
        const Game::FaceAnimationEntry *pEntry = faceCheck.find(static_cast<Game::FaceAnimationId>(id));
        require(pEntry && pEntry->animationName == row[1], "Face identity changed");
        for (size_t slot = 0; slot < pEntry->portraitIds.size(); ++slot)
        {
            require(slot < 5 && static_cast<uint32_t>(pEntry->portraitIds[slot])
                == executable[0xfd660 + id * 8 + 3 + slot], "Face choices changed order");
        }
        const size_t count = pEntry->portraitIds.size();
        require(count > 0 && count <= 5 && (count == 5 || executable[0xfd660 + id * 8 + 3 + count] == 0),
            "Face typed loading changed choice count");
    }
    for (uint32_t id : noExpressionIds)
    {
        require(!faceCheck.find(static_cast<Game::FaceAnimationId>(id)), "Empty face event became active");
    }

    std::ostringstream manifest;
    manifest << "{\"schema\":1,\"stage\":\"spell-and-face-associations\",\"runtime_ready\":false,"
        << "\"executable_profile\":\"mm8-merge-2023-supplied\",\"source_executable_sha256\":"
        << std::quoted(executableHash) << ",\"executable_executed\":false,\"spell_bindings\":" << spellRows.size()
        << ",\"face_slots\":110,\"face_events\":" << faceRows.size() << ",\"expression_choices\":" << expressionCount
        << ",\"deferred_sound_slots\":" << soundSlots << ",\"no_expression_ids\":[";
    for (size_t index = 0; index < noExpressionIds.size(); ++index)
    {
        manifest << (index ? "," : "") << noExpressionIds[index];
    }
    manifest << "],\"unsupported_face_event_ids\":[";
    for (size_t index = 0; index < unsupportedFaceIds.size(); ++index)
    {
        manifest << (index ? "," : "") << unsupportedFaceIds[index];
    }
    manifest << "],\"unsupported_face_expression\":32,\"typed_validation\":true,\"portrait_fx_events_ready\":true,"
        << "\"portrait_event_kinds\":" << eventRows.size() << ",\"event_binding_checks\":" << eventBindings
        << ",\"event_operation_order_validated\":true,\"automatic_face_reactions_validated\":false,"
        << "\"runtime_patch_parity\":false,\"animation_art_ready\":false,\"full_gameplay_loop\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh FX output");
    try
    {
        writeTable(output / "engine/data_tables/spell_fx.txt", spellRows);
        writeTable(output / "engine/data_tables/face_animations.txt", faceRows);
        writeTable(output / "engine/data_tables/portrait_fx_events.txt", eventRows);
        const std::string report = manifest.str();
        writeConvertedResource(output / "fx_conversion.json", Bytes(report.begin(), report.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
