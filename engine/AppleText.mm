#include "AppleText.h"
#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <type_traits>

namespace OpenYAMM::Engine
{
namespace
{
template <typename T>
struct CfDeleter
{
    void operator()(T pValue) const
    {
        if (pValue != nullptr)
        {
            CFRelease(pValue);
        }
    }
};
template <typename T> using CfOwner = std::unique_ptr<std::remove_pointer_t<T>, CfDeleter<T>>;

NSAttributedString *attributed(const std::string &text, float size)
{
    if (text.size() > 65536 || !std::isfinite(size) || size < 1 || size > 256)
    {
        return nil;
    }
    NSString *string = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
    if (string == nil)
    {
        return nil;
    }
    const CfOwner<CTFontRef> font(CTFontCreateWithName(CFSTR("PingFangSC-Regular"), size, nullptr));
    return [[NSAttributedString alloc] initWithString:string attributes:@{
        (__bridge NSString *)kCTFontAttributeName: (__bridge id)font.get(),
        (__bridge NSString *)kCTForegroundColorFromContextAttributeName: @YES
    }];
}

float lineWidth(NSAttributedString *text)
{
    const CfOwner<CTLineRef> line(CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)text));
    return float(CTLineGetTypographicBounds(line.get(), nullptr, nullptr, nullptr));
}

struct LineImageBounds
{
    AppleTextImageSize size;
    float advance = 0;
    double left = 0;
    CGFloat descent = 0;
};

LineImageBounds lineImageBounds(CTLineRef pLine)
{
    LineImageBounds bounds;
    CGFloat ascent = 0, leading = 0;
    const double advance = CTLineGetTypographicBounds(pLine, &ascent, &bounds.descent, &leading);
    const CGRect ink = CTLineGetBoundsWithOptions(pLine, kCTLineBoundsUseGlyphPathBounds);
    bounds.left = std::min(0.0, double(CGRectGetMinX(ink)));
    const double right = std::max(advance, double(CGRectGetMaxX(ink)));
    bounds.advance = float(advance);
    bounds.size.width = std::max(1, int(std::ceil(right - bounds.left)) + 4);
    bounds.size.height = std::max(1, int(std::ceil(ascent + bounds.descent + leading)) + 4);
    return bounds;
}
}

std::optional<float> measureAppleText(const std::string &text, float size)
{
    @autoreleasepool
    {
        NSAttributedString *string = attributed(text, size);
        return string == nil ? std::nullopt : std::optional<float>(lineWidth(string));
    }
}

std::optional<AppleTextImageSize> measureAppleTextImageSize(const std::string &text, float size)
{
    @autoreleasepool
    {
        NSAttributedString *string = attributed(text, size);
        if (string == nil)
        {
            return std::nullopt;
        }
        const CfOwner<CTLineRef> line(CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)string));
        return lineImageBounds(line.get()).size;
    }
}

std::optional<AppleTextImage> rasterizeAppleText(const std::string &text, float size, uint32_t colorAbgr)
{
    @autoreleasepool
    {
        NSAttributedString *string = attributed(text, size);
        if (string == nil)
        {
            return std::nullopt;
        }
        const CfOwner<CTLineRef> line(CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)string));
        const LineImageBounds bounds = lineImageBounds(line.get());
        AppleTextImage result;
        result.advance = bounds.advance;
        result.width = bounds.size.width;
        result.height = bounds.size.height;
        if (result.width > 8192 || result.height > 512)
        {
            return std::nullopt;
        }
        result.bgra.resize(size_t(result.width) * result.height * 4, 0);
        const CfOwner<CGColorSpaceRef> colorSpace(CGColorSpaceCreateDeviceRGB());
        const CfOwner<CGContextRef> context(CGBitmapContextCreate(result.bgra.data(), result.width, result.height,
            8, result.width * 4, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little));
        if (!context)
        {
            return std::nullopt;
        }
        CGContextSetRGBFillColor(context.get(), double(colorAbgr & 255) / 255.0,
            double((colorAbgr >> 8) & 255) / 255.0, double((colorAbgr >> 16) & 255) / 255.0,
            double(colorAbgr >> 24) / 255.0);
        CGContextSetTextPosition(context.get(), 2 - bounds.left, 2 + bounds.descent);
        CTLineDraw(line.get(), context.get());
        // bgfx UI blending expects straight alpha. CoreGraphics writes premultiplied BGRA.
        for (size_t i = 0; i < result.bgra.size(); i += 4)
        {
            const unsigned alpha = result.bgra[i + 3];
            if (alpha != 0)
            {
                for (size_t channel = 0; channel < 3; ++channel)
                {
                    result.bgra[i + channel] = uint8_t(std::min(255u, result.bgra[i + channel] * 255u / alpha));
                }
            }
        }
        return result;
    }
}

std::optional<std::vector<std::string>> wrapAppleText(const std::string &text, float size, float width)
{
    @autoreleasepool
    {
        NSAttributedString *string = attributed(text, size);
        if (string == nil || !std::isfinite(width) || width <= 0)
        {
            return std::nullopt;
        }
        std::vector<std::string> lines;
        for (NSString *paragraph in [string.string componentsSeparatedByString:@"\n"])
        {
            if (paragraph.length == 0)
            {
                lines.emplace_back();
                continue;
            }
            NSAttributedString *part = attributed(paragraph.UTF8String, size);
            const CfOwner<CTTypesetterRef> typesetter(
                CTTypesetterCreateWithAttributedString((__bridge CFAttributedStringRef)part));
            NSUInteger start = 0;
            while (start < paragraph.length)
            {
                NSUInteger length = NSUInteger(CTTypesetterSuggestLineBreak(typesetter.get(), CFIndex(start), width));
                if (length == 0)
                {
                    length = [paragraph rangeOfComposedCharacterSequenceAtIndex:start].length;
                }
                const NSRange range = [paragraph rangeOfComposedCharacterSequencesForRange:NSMakeRange(start, length)];
                lines.emplace_back([[paragraph substringWithRange:range] UTF8String]);
                start = NSMaxRange(range);
            }
        }
        return lines;
    }
}

std::optional<std::string> truncateAppleText(const std::string &text, float size, float width)
{
    @autoreleasepool
    {
        NSAttributedString *string = attributed(text, size);
        if (string == nil || !std::isfinite(width) || width < 0)
        {
            return std::nullopt;
        }
        if (lineWidth(string) <= width)
        {
            return text;
        }
        NSString *ellipsis = @"…";
        if (lineWidth(attributed(ellipsis.UTF8String, size)) > width)
        {
            return std::string();
        }
        std::vector<NSUInteger> boundaries = {0};
        for (NSUInteger offset = 0; offset < string.length;)
        {
            offset = NSMaxRange([string.string rangeOfComposedCharacterSequenceAtIndex:offset]);
            boundaries.push_back(offset);
        }
        size_t low = 0, high = boundaries.size() - 1;
        while (low < high)
        {
            const size_t middle = (low + high + 1) / 2;
            NSString *prefix = [string.string substringToIndex:boundaries[middle]];
            NSString *candidate = [prefix stringByAppendingString:ellipsis];
            if (lineWidth(attributed(candidate.UTF8String, size)) <= width)
            {
                low = middle;
            }
            else
            {
                high = middle - 1;
            }
        }
        NSString *prefix = [string.string substringToIndex:boundaries[low]];
        return std::string([[prefix stringByAppendingString:ellipsis] UTF8String]);
    }
}
}
