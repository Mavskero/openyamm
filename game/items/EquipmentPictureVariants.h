#pragma once

#include "game/StringUtils.h"

#include <cstdint>
#include <string>

namespace OpenYAMM::Game
{
enum class EquipmentPicturePart
{
    Body,
    Collar,
    CollarFront
};

// Shared by resource selection and presentation. Merge's complex flag changes hat/belt
// body variants and adds the front layer of a cloak collar.
inline std::string equipmentPictureName(const std::string &iconName, const std::string &equipStat,
    uint32_t dollTypeId, bool complex, bool hasRightHandWeapon,
    EquipmentPicturePart part = EquipmentPicturePart::Body)
{
    const std::string kind = toLowerCopy(equipStat);
    if (part != EquipmentPicturePart::Body && kind != "cloak")
    {
        return {};
    }
    uint64_t variant = static_cast<uint64_t>(dollTypeId) + 1;
    if ((kind == "helm" && dollTypeId == 1 && !complex)
        || (kind == "belt" && (dollTypeId == 2 || dollTypeId == 3) && !complex))
    {
        variant = 1;
    }
    const std::string prefix = toLowerCopy(iconName) + "v" + std::to_string(variant);
    if (kind == "cloak")
    {
        switch (part)
        {
            case EquipmentPicturePart::Body: return prefix + "a";
            case EquipmentPicturePart::Collar: return prefix + "b";
            case EquipmentPicturePart::CollarFront: return complex ? prefix + "c" : std::string{};
        }
    }
    if (kind == "armor")
    {
        return prefix + (hasRightHandWeapon ? "" : "a");
    }
    if (kind == "helm" || kind == "belt" || kind == "boots")
    {
        return prefix;
    }
    return toLowerCopy(iconName);
}
}
