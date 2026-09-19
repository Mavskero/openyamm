#include "game/tables/CharacterDollTable.h"

#include "game/StringUtils.h"

#include <cctype>
#include <cstdlib>
#include <limits>

namespace OpenYAMM::Game
{
int CharacterDollTypeEntry::mainHandWeaponAnchorX() const
{
    return rightHandClosedX + mainHandOffsetX;
}

int CharacterDollTypeEntry::mainHandWeaponAnchorY() const
{
    return rightHandClosedY + mainHandOffsetY;
}

int CharacterDollTypeEntry::offHandWeaponAnchorX() const
{
    return leftHandOpenX + offHandOffsetX;
}

int CharacterDollTypeEntry::offHandWeaponAnchorY() const
{
    return leftHandOpenY + offHandOffsetY;
}

namespace
{
bool parseUnsigned(const std::string &text, uint32_t &value)
{
    if (text.empty() || text[0] == '#')
    {
        return false;
    }

    char *pEnd = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &pEnd, 10);

    if (pEnd == text.c_str() || *pEnd != '\0')
    {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseSigned(const std::string &text, int &value)
{
    if (text.empty() || text[0] == '#')
    {
        return false;
    }

    char *pEnd = nullptr;
    const long parsed = std::strtol(text.c_str(), &pEnd, 10);

    if (pEnd == text.c_str() || *pEnd != '\0')
    {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool parseSigned32(const std::string &text, int32_t &value)
{
    int parsed = 0;

    if (!parseSigned(text, parsed))
    {
        return false;
    }

    value = parsed;
    return true;
}

std::string trimAsciiWhitespaceCopy(const std::string &value)
{
    size_t begin = 0;

    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }

    size_t end = value.size();

    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string normalizeAssetName(const std::string &value)
{
    const std::string normalized = toLowerCopy(trimAsciiWhitespaceCopy(value));
    return normalized == "null" ? "none" : normalized;
}
}

bool CharacterDollTable::loadCharacterRows(const std::vector<std::vector<std::string>> &rows)
{
    m_characterEntries.clear();
    const bool originalPortraitRows = !rows.empty() && rows[0].size() == 27
        && rows[0][1] == "Paperdoll type" && rows[0][8] == "Helm X" && rows[0][9] == "Helm Y";

    for (size_t rowIndex = 1; rowIndex < rows.size(); ++rowIndex)
    {
        const std::vector<std::string> &row = rows[rowIndex];

        if (row.size() < (originalPortraitRows ? 27u : 24u))
        {
            continue;
        }

        CharacterDollEntry entry = {};

        if (!parseUnsigned(row[0], entry.id) || (!originalPortraitRows && entry.id == 0))
        {
            continue;
        }
        if (originalPortraitRows)
        {
            if (entry.id == std::numeric_limits<uint32_t>::max())
            {
                return false;
            }
            ++entry.id;
        }

        parseUnsigned(row[1], entry.dollTypeId);
        parseUnsigned(row[2], entry.defaultClassId);
        parseUnsigned(row[3], entry.defaultVoiceId);
        parseUnsigned(row[4], entry.defaultSex);
        entry.availableAtStart = toLowerCopy(trimAsciiWhitespaceCopy(row[5])) == "x";
        parseSigned(row[6], entry.bodyOffsetX);

        int bodyY = 0;

        if (parseSigned(row[7], bodyY))
        {
            // Original Merge tables already use downward-positive screen coordinates.
            // The older converted schema stores upward-positive coordinates instead.
            entry.bodyOffsetY = originalPortraitRows ? bodyY : -bodyY;
        }

        const size_t offset = originalPortraitRows ? 2 : 0;
        if (originalPortraitRows)
        {
            parseSigned(row[8], entry.helmOffsetX);
            // Helm offsets add directly to the complex-item picture's screen coordinates.
            parseSigned(row[9], entry.helmOffsetY);
        }
        entry.backgroundAsset = normalizeAssetName(row[8 + offset]);
        entry.bodyAsset = normalizeAssetName(row[9 + offset]);
        entry.headAsset = normalizeAssetName(row[10 + offset]);
        entry.leftHandClosedAsset = normalizeAssetName(row[11 + offset]);
        entry.leftHandHoldAsset = normalizeAssetName(row[12 + offset]);
        entry.leftHandOpenAsset = normalizeAssetName(row[13 + offset]);
        entry.rightHandFingersAsset = normalizeAssetName(row[14 + offset]);
        entry.rightHandOpenAsset = normalizeAssetName(row[17 + offset]);
        entry.rightHandHoldAsset = normalizeAssetName(row[18 + offset]);
        entry.facePicturesPrefix = originalPortraitRows ? normalizeAssetName(row[19 + offset])
            : trimAsciiWhitespaceCopy(row[19]);
        parseUnsigned(row[20 + offset], entry.defaultMeleeAttack);
        parseUnsigned(row[21 + offset], entry.defaultRangedAttack);
        parseUnsigned(row[22 + offset], entry.npcPictureId);
        parseSigned32(row[23 + offset], entry.raceId);
        m_characterEntries[entry.id] = std::move(entry);
    }

    return !m_characterEntries.empty();
}

bool CharacterDollTable::loadDollTypeRows(const std::vector<std::vector<std::string>> &rows)
{
    m_dollTypeEntries.clear();
    const bool originalHandOrder = !rows.empty() && rows[0].size() >= 18
        && rows[0][14] == "LHo X" && rows[0][16] == "LHc X";

    for (size_t rowIndex = 1; rowIndex < rows.size(); ++rowIndex)
    {
        const std::vector<std::string> &row = rows[rowIndex];

        if (row.size() < 28)
        {
            continue;
        }

        CharacterDollTypeEntry entry = {};

        if (!parseUnsigned(row[0], entry.id))
        {
            continue;
        }

        entry.canEquipBow = toLowerCopy(trimAsciiWhitespaceCopy(row[1])) == "x";
        entry.canEquipArmor = toLowerCopy(trimAsciiWhitespaceCopy(row[2])) == "x";
        entry.canEquipHelm = toLowerCopy(trimAsciiWhitespaceCopy(row[3])) == "x";
        entry.canEquipBelt = toLowerCopy(trimAsciiWhitespaceCopy(row[4])) == "x";
        entry.canEquipBoots = toLowerCopy(trimAsciiWhitespaceCopy(row[5])) == "x";
        entry.canEquipCloak = toLowerCopy(trimAsciiWhitespaceCopy(row[6])) == "x";
        entry.canEquipWeapon = toLowerCopy(trimAsciiWhitespaceCopy(row[7])) == "x";

        int *positions[] = {
            &entry.rightHandOpenX,
            &entry.rightHandOpenY,
            &entry.rightHandClosedX,
            &entry.rightHandClosedY,
            &entry.rightHandFingersX,
            &entry.rightHandFingersY,
            originalHandOrder ? &entry.leftHandOpenX : &entry.leftHandClosedX,
            originalHandOrder ? &entry.leftHandOpenY : &entry.leftHandClosedY,
            originalHandOrder ? &entry.leftHandClosedX : &entry.leftHandOpenX,
            originalHandOrder ? &entry.leftHandClosedY : &entry.leftHandOpenY,
            &entry.leftHandFingersX,
            &entry.leftHandFingersY,
            &entry.offHandOffsetX,
            &entry.offHandOffsetY,
            &entry.mainHandOffsetX,
            &entry.mainHandOffsetY,
            &entry.bowOffsetX,
            &entry.bowOffsetY,
            &entry.shieldX,
            &entry.shieldY
        };

        for (size_t index = 0; index < std::size(positions); index += 2)
        {
            int x = 0;
            int y = 0;
            parseSigned(row[8 + index], x);

            if (parseSigned(row[9 + index], y) && !originalHandOrder)
            {
                y = -y;
            }

            *positions[index] = x;
            *positions[index + 1] = y;
        }

        m_dollTypeEntries[entry.id] = std::move(entry);
    }

    return !m_dollTypeEntries.empty();
}

const CharacterDollEntry *CharacterDollTable::getCharacter(uint32_t characterId) const
{
    const std::unordered_map<uint32_t, CharacterDollEntry>::const_iterator it = m_characterEntries.find(characterId);
    return it != m_characterEntries.end() ? &it->second : nullptr;
}

const CharacterDollTypeEntry *CharacterDollTable::getDollType(uint32_t dollTypeId) const
{
    const std::unordered_map<uint32_t, CharacterDollTypeEntry>::const_iterator it =
        m_dollTypeEntries.find(dollTypeId);
    return it != m_dollTypeEntries.end() ? &it->second : nullptr;
}

const std::unordered_map<uint32_t, CharacterDollEntry> &CharacterDollTable::characters() const
{
    return m_characterEntries;
}
}
