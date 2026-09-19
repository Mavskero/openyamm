#pragma once

#include "game/items/EquipmentPictureVariants.h"

namespace OpenYAMM::Game
{
inline EquipmentPicturePart characterEquipmentPicturePart(const std::string &layoutId)
{
    const std::string id = toLowerCopy(layoutId);
    if (id == "characterdollcloakcollar")
    {
        return EquipmentPicturePart::Collar;
    }
    if (id == "characterdollcloakcollarfront")
    {
        return EquipmentPicturePart::CollarFront;
    }
    return EquipmentPicturePart::Body;
}
}
