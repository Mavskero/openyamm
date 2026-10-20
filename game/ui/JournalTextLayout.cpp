#include "game/ui/JournalTextLayout.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace OpenYAMM::Game
{
JournalTextLayout layoutJournalText(
    const std::vector<std::vector<std::string>> &entries,
    const JournalTextLayoutMetrics &metrics,
    JournalTextFlow flow)
{
    if (!std::isfinite(metrics.pageHeight) || !std::isfinite(metrics.lineAdvance)
        || !std::isfinite(metrics.dividerHeight) || !std::isfinite(metrics.dividerGapBefore)
        || !std::isfinite(metrics.dividerGapAfter) || metrics.lineAdvance <= 0.0f
        || metrics.pageHeight < metrics.lineAdvance || metrics.dividerHeight < 0.0f
        || metrics.dividerGapBefore < 0.0f || metrics.dividerGapAfter < 0.0f)
    {
        throw std::invalid_argument("Invalid journal text layout metrics");
    }

    JournalTextLayout result;
    result.lineAdvance = metrics.lineAdvance;
    JournalTextPage page;
    double usedHeight = 0.0;
    const double dividerGap = double(metrics.dividerGapBefore)
        + metrics.dividerHeight + metrics.dividerGapAfter;
    const auto finishPage = [&]()
    {
        if (!page.entries.empty())
        {
            result.pages.push_back(std::move(page));
            page = {};
            usedHeight = 0.0;
        }
    };

    for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
    {
        const std::vector<std::string> &lines = entries[entryIndex];
        if (lines.empty())
        {
            continue;
        }
        const double entryHeight = double(lines.size()) * metrics.lineAdvance;
        if (flow == JournalTextFlow::SeparateEntries
            || (!page.entries.empty() && usedHeight + dividerGap + entryHeight > metrics.pageHeight))
        {
            finishPage();
        }

        size_t firstLine = 0;
        while (firstLine < lines.size())
        {
            JournalTextPageEntry fragment;
            fragment.sourceEntryIndex = entryIndex;
            fragment.firstLineIndex = firstLine;
            if (!page.entries.empty())
            {
                if (metrics.dividerHeight > 0.0f)
                {
                    fragment.dividerY = float(usedHeight + metrics.dividerGapBefore);
                }
                usedHeight += dividerGap;
            }
            fragment.y = float(usedHeight);
            const double availableLines = std::floor((metrics.pageHeight - usedHeight) / metrics.lineAdvance);
            const size_t remaining = lines.size() - firstLine;
            const size_t count = availableLines >= double(remaining) ? remaining : size_t(availableLines);
            fragment.lines.assign(lines.begin() + firstLine, lines.begin() + firstLine + count);
            page.entries.push_back(std::move(fragment));
            usedHeight += double(count) * metrics.lineAdvance;
            firstLine += count;
            if (firstLine < lines.size())
            {
                finishPage();
            }
        }
    }
    finishPage();
    return result;
}
}
