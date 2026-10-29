#pragma once

#include <cstdint>

namespace OpenYAMM::Game
{
enum class PartyMovementStatus : uint8_t
{
    None,
    Burning,
    Drowning,
};
}
