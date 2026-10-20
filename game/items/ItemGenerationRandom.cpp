#include "game/items/ItemGenerationRandom.h"

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace OpenYAMM::Game
{
namespace
{
template <typename Weight>
std::optional<size_t> weightedIndexImpl(FixedItemGenerationRandom &rng, std::span<const Weight> weights)
{
    uint64_t total = 0;
    for (Weight weight : weights)
    {
        if constexpr (std::is_signed_v<Weight>)
        {
            if (weight < 0)
            {
                throw std::invalid_argument("Fixed item generation: negative weight");
            }
        }
        if (uint64_t(weight) > std::numeric_limits<uint64_t>::max() - total)
        {
            throw std::overflow_error("Fixed item generation: weight sum overflow");
        }
        total += uint64_t(weight);
    }
    if (total == 0)
    {
        return std::nullopt;
    }

    uint64_t remaining = rng.bounded(total);
    for (size_t index = 0; index < weights.size(); ++index)
    {
        const uint64_t weight = uint64_t(weights[index]);
        if (remaining < weight)
        {
            return index;
        }
        remaining -= weight;
    }
    throw std::logic_error("Fixed item generation: invalid weighted selection");
}
}

FixedItemGenerationRandom::FixedItemGenerationRandom(const FixedItemGenerationCursor &before) : m_cursor(before)
{
    if (before.protocol != FixedItemGenerationProtocol::V1)
    {
        throw std::invalid_argument("Fixed item generation: unsupported protocol");
    }
}

const FixedItemGenerationCursor &FixedItemGenerationRandom::cursor() const
{
    return m_cursor;
}

uint64_t FixedItemGenerationRandom::raw()
{
    if (m_cursor.rawDrawCount == std::numeric_limits<uint64_t>::max())
    {
        throw std::overflow_error("Fixed item generation: draw count overflow");
    }
    m_cursor.state += UINT64_C(0x9e3779b97f4a7c15);
    ++m_cursor.rawDrawCount;
    uint64_t value = m_cursor.state;
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

uint64_t FixedItemGenerationRandom::bounded(uint64_t bound)
{
    if (bound == 0)
    {
        throw std::invalid_argument("Fixed item generation: zero bound");
    }
    const uint64_t threshold = (uint64_t(0) - bound) % bound;
    uint64_t value = raw();
    while (value < threshold)
    {
        value = raw();
    }
    return value % bound;
}

int FixedItemGenerationRandom::intRange(int minimum, int maximum)
{
    if (maximum < minimum)
    {
        throw std::invalid_argument("Fixed item generation: reversed range");
    }
    const uint64_t width = uint64_t(int64_t(maximum) - int64_t(minimum)) + 1;
    return int(int64_t(minimum) + int64_t(bounded(width)));
}

std::optional<size_t> FixedItemGenerationRandom::weightedIndex(std::span<const uint64_t> weights)
{
    return weightedIndexImpl(*this, weights);
}

std::optional<size_t> FixedItemGenerationRandom::weightedIndex(std::span<const int> weights)
{
    return weightedIndexImpl(*this, weights);
}
}
