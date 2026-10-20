#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
enum class JournalTextFlow
{
    Stacked,
    SeparateEntries,
};

struct JournalTextLayoutMetrics
{
    float pageHeight = 0.0f;
    // Includes the tallest rendered line and its shadow; shared by pagination and drawing.
    float lineAdvance = 0.0f;
    float dividerHeight = 0.0f;
    float dividerGapBefore = 0.0f;
    float dividerGapAfter = 0.0f;
};

struct JournalTextPageEntry
{
    size_t sourceEntryIndex = 0;
    size_t firstLineIndex = 0;
    std::vector<std::string> lines;
    float y = 0.0f;
    std::optional<float> dividerY;
};

struct JournalTextPage
{
    std::vector<JournalTextPageEntry> entries;
};

struct JournalTextLayout
{
    float lineAdvance = 0.0f;
    std::vector<JournalTextPage> pages;
};

// Input is already wrapped. Empty strings are real blank lines; empty entries have no content.
// Short entries stay together; an entry taller than a page continues on subsequent pages.
// Invalid metrics, including a viewport shorter than one line, are rejected before pagination.
JournalTextLayout layoutJournalText(
    const std::vector<std::vector<std::string>> &entries,
    const JournalTextLayoutMetrics &metrics,
    JournalTextFlow flow);
}
