#pragma once

#include <cstdint>

namespace OpenYAMM::Game
{
inline float mapDecorationFacingDegrees(int directionYawUnits, uint16_t directionDegreesStorage)
{
    // Legacy MapSprite offset 0x10 is a 2048-unit turn. Offset 0x1a is a signed
    // degree value used only when the primary direction is zero. Keep raw map fields intact.
    if (directionYawUnits != 0)
    {
        const int normalized = (directionYawUnits % 2048 + 2048) % 2048;
        return static_cast<float>(normalized) * (360.0f / 2048.0f);
    }
    const int degrees = directionDegreesStorage <= 32767
        ? int(directionDegreesStorage) : int(directionDegreesStorage) - 65536;
    return static_cast<float>((degrees % 360 + 360) % 360);
}
}
