#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>

namespace OpenYAMM::Game
{
// OpenYAMM's portable generation protocol, not the original Lua/CRT random sequence.
enum class FixedItemGenerationProtocol : uint32_t
{
    V1 = 1,
};

struct FixedItemGenerationCursor
{
    FixedItemGenerationProtocol protocol = FixedItemGenerationProtocol::V1;
    uint64_t state = 0;
    uint64_t rawDrawCount = 0;
    bool operator==(const FixedItemGenerationCursor &) const = default;
};

// Owns a local cursor. The caller commits cursor() only after the complete operation succeeds.
// Invalid ranges/protocols/weights fail before drawing; an exhausted draw counter throws.
class FixedItemGenerationRandom
{
public:
    explicit FixedItemGenerationRandom(const FixedItemGenerationCursor &before);
    const FixedItemGenerationCursor &cursor() const;
    uint64_t raw();
    uint64_t bounded(uint64_t bound);
    int intRange(int minimum, int maximum);
    std::optional<size_t> weightedIndex(std::span<const uint64_t> weights);
    std::optional<size_t> weightedIndex(std::span<const int> weights);

private:
    FixedItemGenerationCursor m_cursor;
};

// Keep legacy distribution types and consumption unchanged while sharing generation decisions.
inline int itemGenerationIntRange(std::mt19937 &rng, int minimum, int maximum)
{
    return std::uniform_int_distribution<int>(minimum, maximum)(rng);
}

inline int itemGenerationIntRange(FixedItemGenerationRandom &rng, int minimum, int maximum)
{
    return rng.intRange(minimum, maximum);
}

inline std::optional<size_t> itemGenerationWeightedIndex(std::mt19937 &rng, std::span<const int> weights)
{
    if (weights.empty())
    {
        return std::nullopt;
    }
    return std::discrete_distribution<size_t>(weights.begin(), weights.end())(rng);
}

inline std::optional<size_t> itemGenerationWeightedIndex(
    FixedItemGenerationRandom &rng, std::span<const int> weights)
{
    return rng.weightedIndex(weights);
}
}
