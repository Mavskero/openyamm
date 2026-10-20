#include "doctest/doctest.h"
#include "game/ui/JournalTextLayout.h"

#include <limits>
#include <stdexcept>
#include <utility>

using namespace OpenYAMM::Game;

namespace
{
using Entries = std::vector<std::vector<std::string>>;

void checkTextAndBounds(const Entries &source, const JournalTextLayout &layout,
    const JournalTextLayoutMetrics &metrics, JournalTextFlow flow)
{
    Entries reconstructed(source.size());
    std::optional<size_t> previousSource;
    CHECK(layout.lineAdvance == metrics.lineAdvance);
    for (const JournalTextPage &page : layout.pages)
    {
        REQUIRE_FALSE(page.entries.empty());
        if (flow == JournalTextFlow::SeparateEntries)
        {
            REQUIRE(page.entries.size() == 1);
        }
        double previousBottom = 0.0;
        for (size_t index = 0; index < page.entries.size(); ++index)
        {
            const JournalTextPageEntry &entry = page.entries[index];
            REQUIRE(entry.sourceEntryIndex < source.size());
            REQUIRE_FALSE(entry.lines.empty());
            CHECK(entry.firstLineIndex == reconstructed[entry.sourceEntryIndex].size());
            if (previousSource)
            {
                CHECK(entry.sourceEntryIndex >= *previousSource);
            }
            previousSource = entry.sourceEntryIndex;
            CHECK(entry.y >= previousBottom);
            const double bottom = double(entry.y) + double(entry.lines.size()) * layout.lineAdvance;
            CHECK(bottom <= double(metrics.pageHeight) + 0.0001);
            if (index == 0)
            {
                CHECK(entry.y == 0.0f);
                CHECK_FALSE(entry.dividerY.has_value());
            }
            else
            {
                CHECK(entry.y == doctest::Approx(previousBottom + metrics.dividerGapBefore
                    + metrics.dividerHeight + metrics.dividerGapAfter));
                if (metrics.dividerHeight > 0.0f)
                {
                    REQUIRE(entry.dividerY.has_value());
                    CHECK(*entry.dividerY == doctest::Approx(previousBottom + metrics.dividerGapBefore));
                    CHECK(double(*entry.dividerY) + metrics.dividerHeight <= entry.y);
                }
                else
                {
                    CHECK_FALSE(entry.dividerY.has_value());
                }
            }
            previousBottom = bottom;
            std::vector<std::string> &lines = reconstructed[entry.sourceEntryIndex];
            lines.insert(lines.end(), entry.lines.begin(), entry.lines.end());
        }
    }
    CHECK(reconstructed == source);
}
}

TEST_CASE("Journal story uses rendered line advance and preserves blank lines and later histories")
{
    const Entries source = {
        {"请各位聚在一起，", "听我讲述毁灭者之日的故事。", "", "英雄们承担着责任。",
            "The fifth line continues.", "", "尾段完整保留。"},
        {"第二篇日记", "A new history starts on its own page."},
    };
    // A 16px bitmap height cannot represent a 22px fallback raster plus its 2px shadow.
    const JournalTextLayoutMetrics metrics{96.0f, 24.0f};
    const JournalTextLayout layout = layoutJournalText(source, metrics, JournalTextFlow::SeparateEntries);
    REQUIRE(layout.pages.size() == 3);
    CHECK(layout.pages[0].entries[0].lines.size() == 4);
    CHECK(layout.pages[1].entries[0].lines.size() == 3);
    CHECK(layout.pages[1].entries[0].firstLineIndex == 4);
    CHECK(layout.pages[2].entries[0].sourceEntryIndex == 1);
    checkTextAndBounds(source, layout, metrics, JournalTextFlow::SeparateEntries);

    const JournalTextLayoutMetrics bitmapOnly{96.0f, 16.0f};
    const JournalTextLayout wrongMetrics = layoutJournalText(source, bitmapOnly, JournalTextFlow::SeparateEntries);
    CHECK(wrongMetrics.pages[0].entries[0].lines.size() * metrics.lineAdvance > metrics.pageHeight);
}

TEST_CASE("Journal long quest or note continues across pages without discarding its tail")
{
    Entries source{{"短任务"}, {}, {"最后一条笔记"}};
    for (size_t index = 0; index < 10; ++index)
    {
        source[1].push_back("长任务第 " + std::to_string(index) + " 行 / retained text");
    }
    const JournalTextLayoutMetrics metrics{96.0f, 24.0f, 6.0f, 7.0f, 9.0f};
    const JournalTextLayout layout = layoutJournalText(source, metrics, JournalTextFlow::Stacked);
    REQUIRE(layout.pages.size() == 4);
    CHECK(layout.pages[0].entries[0].sourceEntryIndex == 0);
    CHECK(layout.pages[1].entries[0].lines.size() == 4);
    CHECK(layout.pages[2].entries[0].lines.size() == 4);
    REQUIRE(layout.pages[3].entries.size() == 2);
    CHECK(layout.pages[3].entries[0].lines.size() == 2);
    CHECK(layout.pages[3].entries[0].firstLineIndex == 8);
    CHECK(layout.pages[3].entries[1].dividerY == std::optional<float>(55.0f));
    CHECK(layout.pages[3].entries[1].y == 70.0f);
    checkTextAndBounds(source, layout, metrics, JournalTextFlow::Stacked);
}

TEST_CASE("Journal dividers use the drawn texture height and both gaps at page boundaries")
{
    const Entries source{{"first"}, {"第二条"}};
    JournalTextLayoutMetrics metrics{70.0f, 24.0f, 6.0f, 7.0f, 9.0f};
    JournalTextLayout layout = layoutJournalText(source, metrics, JournalTextFlow::Stacked);
    REQUIRE(layout.pages.size() == 1);
    REQUIRE(layout.pages[0].entries.size() == 2);
    CHECK(layout.pages[0].entries[1].dividerY == std::optional<float>(31.0f));
    CHECK(layout.pages[0].entries[1].y == 46.0f);
    checkTextAndBounds(source, layout, metrics, JournalTextFlow::Stacked);

    metrics.pageHeight = 69.0f;
    layout = layoutJournalText(source, metrics, JournalTextFlow::Stacked);
    CHECK(layout.pages.size() == 2);
    checkTextAndBounds(source, layout, metrics, JournalTextFlow::Stacked);

    // Without a loaded divider texture, drawing and pagination both use zero separator space.
    metrics = {48.0f, 24.0f};
    layout = layoutJournalText(source, metrics, JournalTextFlow::Stacked);
    REQUIRE(layout.pages.size() == 1);
    CHECK(layout.pages[0].entries[1].y == 24.0f);
    checkTextAndBounds(source, layout, metrics, JournalTextFlow::Stacked);
}

TEST_CASE("Journal pagination conserves every wrapped line across fonts page sizes and entry lengths")
{
    Entries source{{}, {""}, {"English text", "中文段落"}, {}, {"", "", ""}};
    for (size_t count : {size_t(1), size_t(2), size_t(7), size_t(12), size_t(29)})
    {
        std::vector<std::string> lines;
        for (size_t index = 0; index < count; ++index)
        {
            lines.push_back(index % 3 == 0 ? "" : "保存第 " + std::to_string(index) + " 行 — mixed text");
        }
        source.push_back(std::move(lines));
    }
    for (JournalTextFlow flow : {JournalTextFlow::Stacked, JournalTextFlow::SeparateEntries})
    {
        for (float advance : {12.0f, 24.0f, 31.25f})
        {
            for (float pageLines : {1.0f, 3.0f, 7.5f})
            {
                for (bool divider : {false, true})
                {
                    CAPTURE(advance);
                    CAPTURE(pageLines);
                    CAPTURE(divider);
                    const JournalTextLayoutMetrics metrics{advance * pageLines, advance,
                        divider ? 4.5f : 0.0f, divider ? 7.0f : 0.0f, divider ? 9.0f : 0.0f};
                    checkTextAndBounds(source, layoutJournalText(source, metrics, flow), metrics, flow);
                }
            }
        }
    }
}

TEST_CASE("Journal empty content remains empty and unusable metrics cannot create overflowing pages")
{
    const JournalTextLayoutMetrics metrics{96.0f, 24.0f};
    CHECK(layoutJournalText({}, metrics, JournalTextFlow::Stacked).pages.empty());
    CHECK(layoutJournalText({{}, {}}, metrics, JournalTextFlow::Stacked).pages.empty());
    const Entries blanks{{"", "", ""}};
    checkTextAndBounds(blanks, layoutJournalText(blanks, metrics, JournalTextFlow::Stacked),
        metrics, JournalTextFlow::Stacked);
    for (JournalTextLayoutMetrics invalid : {
        JournalTextLayoutMetrics{23.0f, 24.0f}, JournalTextLayoutMetrics{96.0f, 0.0f},
        JournalTextLayoutMetrics{96.0f, -1.0f}, JournalTextLayoutMetrics{96.0f, 24.0f, -1.0f},
        JournalTextLayoutMetrics{96.0f, 24.0f, 1.0f, -1.0f},
        JournalTextLayoutMetrics{96.0f, 24.0f, 1.0f, 1.0f, -1.0f},
        JournalTextLayoutMetrics{std::numeric_limits<float>::infinity(), 24.0f},
        JournalTextLayoutMetrics{96.0f, std::numeric_limits<float>::quiet_NaN()},
        JournalTextLayoutMetrics{96.0f, 24.0f, std::numeric_limits<float>::infinity()}})
    {
        CHECK_THROWS_AS(layoutJournalText(blanks, invalid, JournalTextFlow::Stacked), std::invalid_argument);
    }
}
