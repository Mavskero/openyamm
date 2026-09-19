#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace OpenYAMM::Import
{
struct ConvertedLodImage
{
    std::string name;
    std::string archive;
    uint32_t width;
    uint32_t height;
};

enum class LodImagePixelFormat
{
    Rgb24,
    Indexed8
};

std::vector<uint8_t> readLodMap(const std::filesystem::path &archive, const std::string &name);
std::vector<uint8_t> readLodText(const std::filesystem::path &archive, const std::string &name);
// Read the archive index once, then visit each selected unwrapped payload without retaining all bytes.
void visitLodTextResources(const std::filesystem::path &archive, const std::vector<std::string> &names,
    const std::function<void(const std::string &, const std::vector<uint8_t> &)> &visitor);
std::vector<std::string> listLodMembers(const std::filesystem::path &archive);
// Later archives override earlier ones. Callers own a new staging destination
// and remove it on failure. Indexed mip chains are decoded from their base level.
// Indexed8 preserves the source palette and index-zero identity; wrapped PCX stays unchanged.
std::vector<ConvertedLodImage> convertLodImages(const std::vector<std::filesystem::path> &archives,
    const std::vector<std::string> &names, const std::filesystem::path &destination,
    LodImagePixelFormat pixelFormat = LodImagePixelFormat::Rgb24);
void writeConvertedResource(const std::filesystem::path &path, const std::vector<uint8_t> &bytes);
std::vector<ConvertedLodImage> convertLodSprites(const std::vector<std::filesystem::path> &archives,
    const std::vector<std::string> &names, const std::filesystem::path &destination);
void convertLodPalettes(const std::vector<std::filesystem::path> &archives,
    const std::vector<int16_t> &paletteIds, const std::filesystem::path &destination);
}
