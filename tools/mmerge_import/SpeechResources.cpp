#include "SpeechResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/audio/SoundCatalog.h"
#include "game/tables/FaceAnimationTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/SpeechReactionTable.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
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
using Game::SpeechId;
constexpr char ExecutableDigest[] = "97be86a7597fbd9763f9b4b0ed1f4a7e47c960703fa30857c32daa056b7d403b";

struct SpeechBinding
{
    SpeechId id;
    const char *pName;
    uint32_t sourceFaceEvent;
    uint32_t directVoiceGroup;
};

// Semantic association between engine reactions and the original face/voice events.
// Zero face event denotes a direct voice-only reaction, not source face event zero.
constexpr auto Bindings = std::to_array<SpeechBinding>({
    {SpeechId::DisarmTrap, "DisarmTrap", 4, 0},
    {SpeechId::StoreClosed, "StoreClosed", 3, 0},
    {SpeechId::TrapExploded, "TrapExploded", 5, 0},
    {SpeechId::SelectCharacter, "SelectCharacter", 102, 0},
    {SpeechId::IdentifyWeakItem, "IdentifyWeakItem", 7, 0},
    {SpeechId::IdentifyGreatItem, "IdentifyGreatItem", 8, 0},
    {SpeechId::IdentifyFailItem, "IdentifyFailItem", 9, 0},
    {SpeechId::RepairSuccess, "RepairSuccess", 10, 0},
    {SpeechId::RepairFail, "RepairFail", 11, 0},
    {SpeechId::SetQuickSpell, "SetQuickSpell", 12, 0},
    {SpeechId::Hungry, "Hungry", 108, 0},
    {SpeechId::DamageMinor, "DamageMinor", 24, 0},
    {SpeechId::DamageMajor, "DamageMajor", 48, 0},
    {SpeechId::Dying, "Dying", 33, 0},
    {SpeechId::Drunk, "Drunk", 31, 0},
    {SpeechId::Insane, "Insane", 29, 0},
    {SpeechId::Poisoned, "Poisoned", 27, 0},
    {SpeechId::Cursed, "Cursed", 30, 0},
    {SpeechId::Falling, "Falling", 66, 0},
    {SpeechId::CantRestHere, "CantRestHere", 13, 0},
    {SpeechId::NotEnoughGold, "NotEnoughGold", 38, 0},
    {SpeechId::InventoryRoom, "InventoryRoom", 15, 0},
    {SpeechId::PotionSuccess, "PotionSuccess", 16, 0},
    {SpeechId::PotionFail, "PotionFail", 17, 0},
    {SpeechId::DoorLocked, "DoorLocked", 18, 0},
    {SpeechId::LearnSpell, "LearnSpell", 21, 0},
    {SpeechId::CantLearnSpell, "CantLearnSpell", 20, 0},
    {SpeechId::CantEquip, "CantEquip", 39, 0},
    {SpeechId::HelloDay, "HelloDay", 22, 0},
    {SpeechId::HelloEvening, "HelloEvening", 23, 0},
    {SpeechId::HelloHouse, "HelloHouse", 84, 0},
    {SpeechId::KillStrongEnemy, "KillStrongEnemy", 2, 0},
    {SpeechId::KillWeakEnemy, "KillWeakEnemy", 1, 0},
    {SpeechId::LastPersonStanding, "LastPersonStanding", 107, 0},
    {SpeechId::LeaveDungeon, "LeaveDungeon", 47, 0},
    {SpeechId::EnterDungeon, "EnterDungeon", 46, 0},
    {SpeechId::Yes, "Yes", 0, 42},
    {SpeechId::ThankYou, "ThankYou", 0, 43},
    {SpeechId::Indignant, "Indignant", 0, 44},
    {SpeechId::Yell, "Yell", 65, 0},
    {SpeechId::AttackHit, "AttackHit", 51, 0},
    {SpeechId::AttackMiss, "AttackMiss", 52, 0},
    {SpeechId::Shoot, "Shoot", 50, 0},
    {SpeechId::CastSpell, "CastSpell", 49, 0},
    {SpeechId::DamagedParty, "DamagedParty", 44, 0},
    {SpeechId::FoundItem, "FoundItem", 60, 0},
    {SpeechId::SkillIncreased, "SkillIncreased", 14, 0},
    {SpeechId::QuestGot, "QuestGot", 93, 0},
    {SpeechId::AwardGot, "AwardGot", 96, 0},
    {SpeechId::TempleHeal, "TempleHeal", 82, 0},
    {SpeechId::TempleDonate, "TempleDonate", 83, 0},
    {SpeechId::TravelBoat, "TravelBoat", 72, 0},
    {SpeechId::TravelHorse, "TravelHorse", 71, 0},
    {SpeechId::ShopIdentify, "ShopIdentify", 73, 0},
    {SpeechId::ShopRepair, "ShopRepair", 74, 0},
    {SpeechId::AlreadyIdentified, "AlreadyIdentified", 76, 0},
    {SpeechId::ItemSold, "ItemSold", 77, 0},
    {SpeechId::WrongShop, "WrongShop", 79, 0},
    {SpeechId::BankDeposit, "BankDeposit", 81, 0},
    {SpeechId::LevelUp, "LevelUp", 87, 0},
    {SpeechId::Beg, "Beg", 53, 0},
    {SpeechId::BegFail, "BegFail", 54, 0},
    {SpeechId::Threat, "Threat", 55, 0},
    {SpeechId::ThreatFail, "ThreatFail", 56, 0},
    {SpeechId::Bribe, "Bribe", 57, 0},
    {SpeechId::BribeFail, "BribeFail", 58, 0},
    {SpeechId::NpcDontTalk, "NpcDontTalk", 59, 0},
    {SpeechId::HireNpc, "HireNpc", 61, 0},
    {SpeechId::TavernPacksFull, "TavernPacksFull", 67, 0},
    {SpeechId::TavernDrink, "TavernDrink", 68, 0},
    {SpeechId::TavernGotDrunk, "TavernGotDrunk", 69, 0},
    {SpeechId::TavernTip, "TavernTip", 70, 0},
    {SpeechId::ShopItemBought, "ShopItemBought", 75, 0},
    {SpeechId::SkillLearned, "SkillLearned", 78, 0},
    {SpeechId::ShopRude, "ShopRude", 80, 0},
    {SpeechId::SkillMasteryIncreased, "SkillMasteryIncreased", 85, 0},
    {SpeechId::JoinedGuild, "JoinedGuild", 86, 0},
    {SpeechId::StatBonusIncreased, "StatBonusIncreased", 91, 0},
    {SpeechId::StatBaseIncreased, "StatBaseIncreased", 92, 0},
    {SpeechId::AfraidSilent, "AfraidSilent", 98, 0},
    {SpeechId::CheatedDeath, "CheatedDeath", 99, 0},
    {SpeechId::InPrison, "InPrison", 100, 0},
    {SpeechId::Awaken, "Awaken", 103, 0},
    {SpeechId::IdentifyMonsterWeak, "IdentifyMonsterWeak", 104, 0},
    {SpeechId::IdentifyMonsterBig, "IdentifyMonsterBig", 105, 0},
    {SpeechId::IdentifyMonsterFail, "IdentifyMonsterFail", 106, 0},
    {SpeechId::DeathBlow, "DeathBlow", 109, 0},
});
static_assert(Bindings.size() == static_cast<size_t>(SpeechId::DeathBlow));

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

Bytes read(const std::filesystem::path &root, const std::string &name)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Speech input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 16 * 1024 * 1024, "Invalid speech input size");
    Bytes bytes(size);
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))),
        "Cannot read speech source");
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
    require(table.has_value(), "Invalid speech dependency quoting");
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
}

void convertSpeechResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
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
            "Speech output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Speech output already exists");
    const Bytes executable = read(root, "mm8.exe");
    require(executable.size() == 1155072 && digest(executable) == ExecutableDigest,
        "Speech mapping needs a verified MM8 executable profile");
    FaceAnimationTable faces;
    MergedCharacterVoiceTable voices;
    SoundCatalog sounds;
    std::string soundError;
    require(faces.loadFromRows(readRows(assets, "face_animations.txt"))
        && voices.loadFromRows(readRows(assets, "character_voices.txt"))
        && sounds.loadFromScopedRows(readRows(assets, "sounds.txt"), {}, soundError),
        "Invalid speech face/voice/sound dependencies");
    require(voices.entries().size() == 100, "Speech groups need all 100 original voice slots");
    const size_t voiceCount = voices.entries()[0].soundIdsByVoiceSetId.size();
    require(voiceCount > 0, "No voice sets for speech");
    // Direct voice-only engine reactions use these original labeled pairs.
    const std::array<Row, 3> directLabels = {{{"Yes1", "Yes2"}, {"Thanks1", "Thanks2"},
        {"Indignant", "Indignant"}}};
    for (size_t pair = 0; pair < directLabels.size(); ++pair)
    {
        for (size_t variant = 0; variant < 2; ++variant)
        {
            require(voices.entries()[82 + pair * 2 + variant].soundType == directLabels[pair][variant],
                "Direct speech voice pair order changed");
        }
    }
    Rows rows = {{"#SpeechReactionMode", "MM8VoiceGroupsV1"},
        {"Event", "Name", "VoiceGroups", "FaceAnimation", "SourceFaceEvent"}};
    std::map<SpeechId, std::vector<uint32_t>> groupsByEvent;
    size_t faceBindings = 0;
    size_t voicedEvents = 0;
    size_t directEvents = 0;
    for (size_t index = 0; index < Bindings.size(); ++index)
    {
        const SpeechBinding &binding = Bindings[index];
        require(static_cast<size_t>(binding.id) == index + 1, "Speech event coverage/order changed");
        Row row = {binding.pName, binding.pName, "", "", ""};
        std::vector<uint32_t> groups;
        if (binding.sourceFaceEvent != 0)
        {
            const size_t offset = 0xfd660 + binding.sourceFaceEvent * 8;
            require(executable[offset + 2] == 0, "Unexpected face-event reserved byte");
            for (size_t slot = 0; slot < 2; ++slot)
            {
                const uint32_t group = executable[offset + slot];
                require(group <= 50, "Face event points outside voice groups");
                if (group != 0)
                {
                    groups.push_back(group);
                }
            }
            std::vector<PortraitId> portraits;
            for (size_t slot = 3; slot < 8; ++slot)
            {
                if (executable[offset + slot] != 0)
                {
                    portraits.push_back(static_cast<PortraitId>(executable[offset + slot]));
                }
            }
            if (!portraits.empty())
            {
                const FaceAnimationEntry *pFace = faces.find(static_cast<FaceAnimationId>(binding.sourceFaceEvent));
                require(pFace && pFace->portraitIds == portraits
                    && faceAnimationIdFromName(pFace->animationName) == pFace->animationId,
                    "Speech event has no matching original face definition");
                row[1] = row[3] = pFace->animationName;
                ++faceBindings;
            }
            row[4] = std::to_string(binding.sourceFaceEvent);
        }
        else
        {
            require(binding.directVoiceGroup >= 42 && binding.directVoiceGroup <= 44,
                "Unknown direct speech binding");
            groups.push_back(binding.directVoiceGroup);
            ++directEvents;
        }
        for (uint32_t group : groups)
        {
            row[2] += (row[2].empty() ? "" : ",") + std::to_string(group);
        }
        voicedEvents += !groups.empty();
        groupsByEvent.emplace(binding.id, std::move(groups));
        rows.push_back(std::move(row));
    }
    const std::string text = serializeTextRows(rows);
    require(parse(text) == rows, "Speech serialization changed bindings");
    SpeechReactionTable reactions;
    require(reactions.loadFromRows(parse(text)), "Speech runtime loading failed");
    size_t selectionChecks = 0;
    size_t silentSelections = 0;
    std::set<uint32_t> missingSoundIds;
    for (size_t index = 0; index < Bindings.size(); ++index)
    {
        const SpeechBinding &binding = Bindings[index];
        const SpeechReactionEntry *pReaction = reactions.find(binding.id);
        const std::vector<uint32_t> &groups = groupsByEvent.at(binding.id);
        require(pReaction && pReaction->voiceGroups == groups && pReaction->soundTypes.empty()
            && pReaction->faceAnimationId == (rows[index + 2][3].empty() ? std::nullopt
                : std::optional<FaceAnimationId>{static_cast<FaceAnimationId>(binding.sourceFaceEvent)}),
            "Speech runtime changed event groups or face binding");
        for (uint32_t voice = 0; voice < voiceCount; ++voice)
        {
            const size_t count = groups.empty() ? 1 : groups.size() * 2;
            for (uint32_t seed = 0; seed < count; ++seed)
            {
                const std::optional<uint32_t> selected = voices.selectSoundIdForGroups(voice, groups, seed);
                uint32_t expected = 0;
                if (!groups.empty())
                {
                    const size_t slot = (groups[seed % groups.size()] - 1) * 2;
                    const size_t variant = seed / groups.size();
                    expected = voices.entries()[slot + variant].soundIdsByVoiceSetId.at(voice);
                    if (expected == 0)
                    {
                        expected = voices.entries()[slot + 1 - variant].soundIdsByVoiceSetId.at(voice);
                    }
                }
                require(selected.value_or(0) == expected, "Voice group/variant selection changed");
                ++selectionChecks;
                silentSelections += !selected;
                if (selected && !sounds.findById(*selected))
                {
                    missingSoundIds.insert(*selected);
                }
            }
        }
    }
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"speech_events\",\"runtime_ready\":false,\"events\":" << Bindings.size()
        << ",\"face_bindings\":" << faceBindings << ",\"voiced_events\":" << voicedEvents
        << ",\"direct_voice_events\":" << directEvents << ",\"voice_sets\":" << voiceCount
        << ",\"selection_checks\":" << selectionChecks << ",\"silent_selections\":" << silentSelections
        << ",\"exe_sha256\":\"" << ExecutableDigest << "\",\"typed_validation\":true,\"missing_sound_ids\":[";
    bool first = true;
    for (uint32_t id : missingSoundIds)
    {
        report << (first ? "" : ",") << id;
        first = false;
    }
    report << "],\"audio_files_exported\":0,\"audio_playback_validated\":false,\"rendered_reactions_validated\":false,"
        << "\"executable_executed\":false,\"original_rng_parity\":false,\"patched_windows_parity\":false}\n";
    require(std::filesystem::create_directory(output), "Cannot create fresh speech output");
    try
    {
        writeConvertedResource(output / "engine/data_tables/character_speech_events.txt",
            Bytes(text.begin(), text.end()));
        const std::string reportText = report.str();
        writeConvertedResource(output / "speech_conversion.json", Bytes(reportText.begin(), reportText.end()));
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
