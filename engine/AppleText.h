#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Engine
{
struct AppleTextImage
{
    int width = 0;
    int height = 0;
    float advance = 0;
    std::vector<uint8_t> bgra;
};

struct AppleTextImageSize
{
    int width = 0;
    int height = 0;
};

// UTF-8 input; system font shaping and fallback. Invalid input is rejected.
// Size and returned measurements are in pixels. No GPU or UIKit dependency.
std::optional<float> measureAppleText(const std::string &text, float size);
// Includes the same transparent padding and rounded bounds as rasterization.
std::optional<AppleTextImageSize> measureAppleTextImageSize(const std::string &text, float size);
std::optional<AppleTextImage> rasterizeAppleText(const std::string &text, float size, uint32_t colorAbgr);
std::optional<std::vector<std::string>> wrapAppleText(const std::string &text, float size, float width);
std::optional<std::string> truncateAppleText(const std::string &text, float size, float width);
}
