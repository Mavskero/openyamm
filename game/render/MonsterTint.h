#pragma once

#include <array>
#include <cstdint>

namespace OpenYAMM::Game
{
// MonList's B/G/R/A bytes are imported as 0xAARRGGBB. A zero RGB value means
// no tint. Its alpha is not sprite opacity (normal source entries have A = 0).
inline std::array<float, 3> monsterTintRgb(uint32_t tintArgb, bool enabled)
{
    if (!enabled || (tintArgb & 0x00ffffffu) == 0)
    {
        return {1.0f, 1.0f, 1.0f};
    }

    return {
        static_cast<float>((tintArgb >> 16) & 0xffu) / 255.0f,
        static_cast<float>((tintArgb >> 8) & 0xffu) / 255.0f,
        static_cast<float>(tintArgb & 0xffu) / 255.0f,
    };
}
}
