#include "DollResources.h"
#include "LodResources.h"
#include "TextResources.h"
#include "engine/TextTable.h"
#include "game/StringUtils.h"
#include "game/party/Party.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/RosterTable.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
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
const Row PortraitHeader = {"#", "Paperdoll type", "Def class", "Def voice", "Def sex", "Available at start",
    "Body X", "Body Y", "Helm X", "Helm Y", "Background", "bod", "head", "LHd", "LHu", "LHo", "RHb", "Unk",
    "Unk", "RHd", "RHu", "Face pics prefix", "DefAttack (melee)", "DefAttack (range)", "NPCPic", "Race", "Notes"};
const Row DollHeader = {"#", "Bow", "Armor", "Helm", "Belt", "Boots", "Cloak", "Weapon", "RHo X", "RHo Y",
    "RHc X", "RHc Y", "RHf X", "RHf Y", "LHo X", "LHo Y", "LHc X", "LHc Y", "LHf X", "LHf Y", "OH Offset X",
    "OH Offset Y", "MH Offset X", "MH Offset Y", "Bow Offset X", "Bow Offset Y", "Shield X", "Shield Y"};

void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int number(const std::string &text, int minimum, int maximum)
{
    int value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size()
        && value >= minimum && value <= maximum, "Invalid doll numeric field: " + text);
    return value;
}

std::string assetName(const std::string &text)
{
    require(!text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    }), "Invalid doll asset name");
    std::string name = text;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c)
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : static_cast<char>(c);
    });
    return name == "null" ? "none" : name;
}

Rows parse(const std::string &text)
{
    const std::optional<Engine::TextTable> table = Engine::TextTable::parseTabSeparated(text);
    require(table.has_value(), "Invalid doll quoting");
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

Rows readRows(const std::filesystem::path &root, const std::string &name, bool legacy)
{
    const std::filesystem::path path = std::filesystem::canonical(root / name);
    const std::filesystem::path relative = path.lexically_relative(root);
    require(!relative.empty() && !relative.is_absolute() && *relative.begin() != "..", "Doll input escapes root");
    const uintmax_t size = std::filesystem::file_size(path);
    require(size > 0 && size <= 4 * 1024 * 1024, "Invalid doll input size");
    std::string text(size, '\0');
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream.read(text.data(), static_cast<std::streamsize>(size))), "Cannot read doll input");
    if (legacy)
    {
        size_t glyphs = 0;
        text = decodeLegacyText(std::vector<uint8_t>(text.begin(), text.end()), false, glyphs);
        require(glyphs == 0, "Unexpected localization in doll mechanics");
    }
    return parse(text);
}

void validateShape(const Rows &rows, const Row &header)
{
    require(rows.size() > 1 && rows.size() <= 257 && rows[0] == header, "Unsupported doll table header or count");
    for (size_t index = 1; index < rows.size(); ++index)
    {
        require(rows[index].size() == header.size(), "Invalid doll row width");
        // The original mod loads these arrays in row order, not by the printed ID.
        require(number(rows[index][0], 0, 255) == static_cast<int>(index - 1), "Doll array identity/order changed");
    }
}

bool flag(const std::string &text)
{
    require(text == "x" || text == "-", "Invalid doll availability flag");
    return text == "x";
}

void checkDoll(const Row &row, const Game::CharacterDollTypeEntry &entry)
{
    const std::vector<bool> flags = {entry.canEquipBow, entry.canEquipArmor, entry.canEquipHelm, entry.canEquipBelt,
        entry.canEquipBoots, entry.canEquipCloak, entry.canEquipWeapon};
    for (size_t index = 0; index < flags.size(); ++index)
    {
        require(flags[index] == flag(row[index + 1]), "Doll equipment availability changed");
    }
    const std::vector<int> positions = {entry.rightHandOpenX, entry.rightHandOpenY, entry.rightHandClosedX,
        entry.rightHandClosedY, entry.rightHandFingersX, entry.rightHandFingersY, entry.leftHandOpenX,
        entry.leftHandOpenY, entry.leftHandClosedX, entry.leftHandClosedY, entry.leftHandFingersX,
        entry.leftHandFingersY, entry.offHandOffsetX, entry.offHandOffsetY, entry.mainHandOffsetX,
        entry.mainHandOffsetY, entry.bowOffsetX, entry.bowOffsetY, entry.shieldX, entry.shieldY};
    for (size_t index = 0; index < positions.size(); ++index)
    {
        const int expected = number(row[index + 8], -32768, 32767);
        require(positions[index] == expected, "Doll hand/weapon coordinate changed");
    }
    require(entry.mainHandWeaponAnchorX() == number(row[10], -32768, 32767) + number(row[22], -32768, 32767)
        && entry.mainHandWeaponAnchorY() == number(row[11], -32768, 32767) + number(row[23], -32768, 32767)
        && entry.offHandWeaponAnchorX() == number(row[14], -32768, 32767) + number(row[20], -32768, 32767)
        && entry.offHandWeaponAnchorY() == number(row[15], -32768, 32767) + number(row[21], -32768, 32767),
        "Doll weapon anchor changed");
}

void checkPortrait(const Row &row, const Game::CharacterDollEntry &entry)
{
    require(entry.id == static_cast<uint32_t>(number(row[0], 0, 255) + 1)
        && entry.dollTypeId == static_cast<uint32_t>(number(row[1], 0, 255))
        && entry.defaultClassId == static_cast<uint32_t>(number(row[2], -1, 255))
        && entry.defaultVoiceId == static_cast<uint32_t>(number(row[3], 0, 255))
        && entry.defaultSex == static_cast<uint32_t>(number(row[4], 0, 1))
        && entry.availableAtStart == flag(row[5]), "Doll portrait identity changed");
    require(entry.bodyOffsetX == number(row[6], -32768, 32767)
        && entry.bodyOffsetY == number(row[7], -32768, 32767)
        && entry.helmOffsetX == number(row[8], -32768, 32767)
        && entry.helmOffsetY == number(row[9], -32768, 32767), "Doll body/helm offset changed");
    const Row actual = {entry.backgroundAsset, entry.bodyAsset, entry.headAsset, entry.leftHandClosedAsset,
        entry.leftHandHoldAsset, entry.leftHandOpenAsset, entry.rightHandFingersAsset, entry.rightHandOpenAsset,
        entry.rightHandHoldAsset, entry.facePicturesPrefix};
    const std::vector<size_t> columns = {10, 11, 12, 13, 14, 15, 16, 19, 20, 21};
    for (size_t index = 0; index < columns.size(); ++index)
    {
        require(actual[index] == assetName(row[columns[index]]), "Doll asset columns shifted");
    }
    require(entry.defaultMeleeAttack == static_cast<uint32_t>(number(row[22], 0, 65535))
        && entry.defaultRangedAttack == static_cast<uint32_t>(number(row[23], 0, 65535))
        && entry.npcPictureId == static_cast<uint32_t>(number(row[24], 0, 65535))
        && entry.raceId == number(row[25], -1, 10), "Doll attack/NPC/race identity changed");
}

void checkPartyIdentity(const Game::CharacterDollTable &dolls, const Game::CharacterDollEntry &entry,
    const std::string &className)
{
    Game::RosterEntry candidate;
    candidate.id = entry.id;
    candidate.pictureId = entry.id - 1;
    candidate.name = "外观验证";
    candidate.className = className;
    candidate.level = 1;
    Game::Party party;
    party.setCharacterDollTable(&dolls);
    require(party.recruitRosterMember(candidate), "Doll recruitment check failed");
    const auto check = [&entry](const Game::Character &member, int32_t voice)
    {
        require(member.characterDataId == entry.id && member.portraitPictureId == entry.id - 1
            && member.raceId == static_cast<uint32_t>(entry.raceId) && member.sexId == entry.defaultSex
            && member.voiceId == voice && member.portraitTextureName == entry.facePicturesPrefix + "01",
            "Recruitment confused portrait texture with character identity");
    };
    check(party.members().back(), static_cast<int32_t>(entry.defaultVoiceId));
    candidate.voiceId = 200;
    require(party.replaceMemberWithRosterEntry(0, candidate), "Doll replacement check failed");
    check(party.members().front(), 200);
}

void checkLegacyRows()
{
    Rows portraits = {Row(24), Row(24)};
    Row &row = portraits[1];
    row[0] = "7";
    row[8] = "Back";
    row[9] = "Body";
    row[19] = "pc99-";
    Game::CharacterDollTable table;
    require(table.loadCharacterRows(portraits), "Legacy doll loading failed");
    const Game::CharacterDollEntry *pEntry = table.getCharacter(7);
    require(pEntry && !table.getCharacter(8) && pEntry->backgroundAsset == "back" && pEntry->bodyAsset == "body"
        && pEntry->helmOffsetX == 0 && pEntry->facePicturesPrefix == "pc99-", "Legacy doll schema changed");
    Rows types = {Row(28), Row(28, "0")};
    types[1][14] = "11";
    types[1][16] = "23";
    require(table.loadDollTypeRows(types), "Legacy doll type loading failed");
    require(table.getDollType(0)->leftHandClosedX == 11 && table.getDollType(0)->leftHandOpenX == 23,
        "Legacy hand column order changed");
}

std::vector<ConvertedLodImage> exportDollImages(const std::filesystem::path &root,
    const std::map<std::string, std::set<uint32_t>> &references, const std::filesystem::path &output)
{
    std::map<std::string, std::filesystem::path> owners;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root / "Data"))
    {
        const std::string name = Game::toLowerCopy(entry.path().filename().string());
        if (!(name == "icons.lod" || name.ends_with(".icons.lod") || name.ends_with(".d.lod")
            || name == "icons.ui.lod" || name.ends_with(".icons.ui.lod")))
        {
            continue;
        }
        const std::filesystem::path archive = std::filesystem::canonical(entry.path());
        const std::filesystem::path relative = archive.lexically_relative(root);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() != ".."
            && std::filesystem::is_regular_file(archive), "Doll image archive escapes source or is not a file");
        for (const std::string &member : listLodMembers(archive))
        {
            if (!references.contains(member))
            {
                continue;
            }
            const auto [owner, inserted] = owners.emplace(member, archive);
            if (!inserted)
            {
                const std::string previous = Game::toLowerCopy(owner->second.filename().string());
                // The merged paperdoll layer patches the original MM8 icons (e.g. pc22rhd).
                // Other competing worlds/mods have no implicit priority in this profile.
                if (previous == "icons.lod" && name == "mmmerge.icons.lod")
                {
                    owner->second = archive;
                }
                else
                {
                    require(previous == "mmmerge.icons.lod" && name == "icons.lod",
                        "Ambiguous paperdoll image ownership: " + member);
                }
            }
        }
    }
    std::map<std::filesystem::path, std::vector<std::string>> byArchive;
    for (const auto &[name, archive] : owners)
    {
        byArchive[archive].push_back(name);
    }
    std::vector<ConvertedLodImage> images;
    for (const auto &[archive, names] : byArchive)
    {
        const std::vector<ConvertedLodImage> converted = convertLodImages({archive}, names, output / "engine/icons");
        require(converted.size() == names.size(), "Paperdoll conversion omitted a selected image");
        images.insert(images.end(), converted.begin(), converted.end());
    }
    std::sort(images.begin(), images.end(), [](const ConvertedLodImage &left, const ConvertedLodImage &right)
    {
        return left.name < right.name;
    });
    return images;
}
}

void convertDollResources(const std::filesystem::path &source, const std::filesystem::path &baseAssets,
    const std::filesystem::path &destination)
{
    const std::filesystem::path root = std::filesystem::canonical(source);
    const std::filesystem::path assets = std::filesystem::canonical(baseAssets);
    const std::filesystem::path output = std::filesystem::weakly_canonical(destination);
    for (const std::filesystem::path &input : {root, assets})
    {
        const std::filesystem::path relative = output.lexically_relative(input);
        require(!relative.empty() && !relative.is_absolute() && *relative.begin() == "..",
            "Doll output must be outside inputs");
    }
    require(!std::filesystem::exists(output), "Doll output already exists");
    const Rows portraits = readRows(root, "Data/Tables/Character portraits.txt", true);
    const Rows types = readRows(root, "Data/Tables/Character doll types.txt", true);
    validateShape(portraits, PortraitHeader);
    validateShape(types, DollHeader);
    Game::ClassSkillTable classes;
    require(classes.loadClassMetadataFromRows(readRows(assets, "engine/data_tables/class_extra.txt", false)),
        "Invalid doll class dependency");
    Game::RosterTable roster;
    require(roster.loadFromRows(readRows(assets, "engine/data_tables/roster.txt", false), &classes),
        "Invalid doll roster dependency");
    const std::string portraitText = serializeTextRows(portraits);
    const std::string typeText = serializeTextRows(types);
    require(parse(portraitText) == portraits && parse(typeText) == types, "Doll serialization changed source cells");
    Game::CharacterDollTable dolls;
    require(dolls.loadCharacterRows(parse(portraitText)) && dolls.loadDollTypeRows(parse(typeText)),
        "Doll typed loading failed");
    require(dolls.characters().size() == portraits.size() - 1, "Doll portrait zero was lost");
    for (size_t index = 1; index < types.size(); ++index)
    {
        const Game::CharacterDollTypeEntry *pEntry = dolls.getDollType(index - 1);
        require(pEntry, "Doll type zero was lost");
        checkDoll(types[index], *pEntry);
    }
    size_t identityChecks = 0;
    size_t selectable = 0;
    size_t specialAttacks = 0;
    std::set<std::string> imageReferences;
    std::map<std::string, std::set<uint32_t>> dollImageReferences;
    std::vector<uint32_t> unavailableClasses;
    for (size_t index = 1; index < portraits.size(); ++index)
    {
        const Game::CharacterDollEntry *pEntry = dolls.getCharacter(index);
        require(pEntry && dolls.getDollType(pEntry->dollTypeId), "Doll portrait references missing body type");
        checkPortrait(portraits[index], *pEntry);
        for (size_t column = 10; column <= 21; ++column)
        {
            const std::string name = assetName(portraits[index][column]);
            if (name != "none")
            {
                imageReferences.insert(column == 21 ? name + "01" : name);
            }
        }
        // Select typed body assets only. Face animation prefixes have their own frame converter;
        // the two unknown original columns are retained in the table without inventing semantics.
        for (const std::string *pName : {&pEntry->backgroundAsset, &pEntry->bodyAsset, &pEntry->headAsset,
            &pEntry->leftHandClosedAsset, &pEntry->leftHandHoldAsset, &pEntry->leftHandOpenAsset,
            &pEntry->rightHandFingersAsset, &pEntry->rightHandOpenAsset, &pEntry->rightHandHoldAsset})
        {
            if (!pName->empty() && *pName != "none")
            {
                dollImageReferences[*pName].insert(pEntry->id);
            }
        }
        selectable += pEntry->availableAtStart;
        specialAttacks += pEntry->defaultMeleeAttack != 0 || pEntry->defaultRangedAttack != 0;
        if (pEntry->defaultClassId == std::numeric_limits<uint32_t>::max())
        {
            require(!pEntry->availableAtStart && pEntry->raceId == -1, "Invalid selectable doll without class");
            unavailableClasses.push_back(index - 1);
            continue;
        }
        const std::optional<std::string> className = classes.classNameForId(pEntry->defaultClassId);
        require(className.has_value() && pEntry->raceId >= 0, "Doll default class/race is unavailable");
        checkPartyIdentity(dolls, *pEntry, *className);
        ++identityChecks;
    }
    for (const Game::RosterEntry *pEntry : roster.getEntriesSortedById())
    {
        require(dolls.getCharacter(pEntry->pictureId + 1), "Roster portrait was lost in doll import");
    }
    checkLegacyRows();
    std::ostringstream report;
    report << "{\"schema\":1,\"stage\":\"character-dolls\",\"runtime_ready\":false,\"portraits\":"
        << portraits.size() - 1 << ",\"doll_types\":" << types.size() - 1 << ",\"selectable_portraits\":" << selectable
        << ",\"identity_checks\":" << identityChecks << ",\"special_attack_portraits\":" << specialAttacks
        << ",\"image_references\":" << imageReferences.size() << ",\"unavailable_class_ids\":[";
    for (size_t index = 0; index < unavailableClasses.size(); ++index)
    {
        report << (index ? "," : "") << unavailableClasses[index];
    }
    report << "],\"typed_validation\":true,\"source_id_preserved\":true,\"engine_character_id_offset\":1"
        << ",\"rendered_dolls_validated\":false,\"special_attacks_connected\":false,\"full_gameplay_loop\":false";
    require(std::filesystem::create_directory(output), "Cannot create fresh doll output");
    const auto write = [&output](const std::string &name, const std::string &text)
    {
        writeConvertedResource(output / name, std::vector<uint8_t>(text.begin(), text.end()));
    };
    try
    {
        write("engine/data_tables/character_data.txt", portraitText);
        write("engine/data_tables/doll_types.txt", typeText);
        const std::vector<ConvertedLodImage> images = exportDollImages(root, dollImageReferences, output);
        std::set<std::string> missing;
        for (const auto &[name, ids] : dollImageReferences)
        {
            missing.insert(name);
        }
        report << ",\"paperdoll_image_references\":" << dollImageReferences.size()
            << ",\"images_exported\":" << images.size() << ",\"images\":[";
        for (size_t index = 0; index < images.size(); ++index)
        {
            const ConvertedLodImage &image = images[index];
            const std::string name = std::filesystem::path(image.name).stem().string();
            missing.erase(name);
            report << (index ? "," : "") << "{\"name\":" << std::quoted(image.name)
                << ",\"archive\":" << std::quoted(image.archive) << ",\"width\":" << image.width
                << ",\"height\":" << image.height << ",\"character_ids\":[";
            size_t characterIndex = 0;
            for (uint32_t id : dollImageReferences.at(name))
            {
                report << (characterIndex++ ? "," : "") << id;
            }
            report << "]}";
        }
        report << "],\"missing_images\":[";
        size_t missingIndex = 0;
        for (const std::string &name : missing)
        {
            report << (missingIndex++ ? "," : "") << std::quoted(name);
        }
        report << "]}\n";
        write("doll_conversion.json", report.str());
    }
    catch (...)
    {
        std::filesystem::remove_all(output);
        throw;
    }
}
}
