#include "engine/ImageAssetLoader.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace
{
std::vector<uint8_t> readBinaryFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        return {};
    }

    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void appendUInt16(std::vector<uint8_t> &bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
}

void appendUInt32(std::vector<uint8_t> &bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xffu));
}

std::vector<uint8_t> makeTwoPixelIndexedBmp()
{
    constexpr uint32_t fileHeaderSize = 14;
    constexpr uint32_t dibHeaderSize = 40;
    constexpr uint32_t paletteBytes = 2 * 4;
    constexpr uint32_t pixelOffset = fileHeaderSize + dibHeaderSize + paletteBytes;
    constexpr uint32_t rowBytes = 4;
    constexpr uint32_t fileSize = pixelOffset + rowBytes;

    std::vector<uint8_t> bytes;
    bytes.reserve(fileSize);
    bytes.push_back('B');
    bytes.push_back('M');
    appendUInt32(bytes, fileSize);
    appendUInt16(bytes, 0);
    appendUInt16(bytes, 0);
    appendUInt32(bytes, pixelOffset);
    appendUInt32(bytes, dibHeaderSize);
    appendUInt32(bytes, 2);
    appendUInt32(bytes, 1);
    appendUInt16(bytes, 1);
    appendUInt16(bytes, 8);
    appendUInt32(bytes, 0);
    appendUInt32(bytes, rowBytes);
    appendUInt32(bytes, 0);
    appendUInt32(bytes, 0);
    appendUInt32(bytes, 2);
    appendUInt32(bytes, 0);
    bytes.insert(bytes.end(), {252, 252, 0, 0});
    bytes.insert(bytes.end(), {0, 0, 255, 0});
    bytes.insert(bytes.end(), {0, 1, 0, 0});
    return bytes;
}

std::vector<uint8_t> makeTwoPixelIndexedPcx()
{
    std::vector<uint8_t> bytes(128, 0);
    bytes[0] = 0x0a;
    bytes[1] = 5;
    bytes[2] = 1;
    bytes[3] = 8;
    bytes[8] = 1;
    bytes[65] = 1;
    bytes[66] = 2;
    bytes.push_back(0);
    bytes.push_back(1);
    bytes.push_back(0x0c);
    bytes.resize(bytes.size() + 256 * 3, 0);
    const size_t paletteOffset = bytes.size() - 256 * 3;
    bytes[paletteOffset + 1] = 255;
    bytes[paletteOffset + 2] = 255;
    bytes[paletteOffset + 3] = 10;
    bytes[paletteOffset + 4] = 20;
    bytes[paletteOffset + 5] = 30;
    return bytes;
}
}

TEST_CASE("ImageAssetLoader decodes PNG pixels through shared loader")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::filesystem::path pngPath =
        sourceRoot / "editor" / "assets" / "editor_ux_work" / "mockups" / "Level editor UI redesign mockup.png";
    const std::vector<uint8_t> bytes = readBinaryFile(pngPath);

    REQUIRE_FALSE(bytes.empty());

    const std::optional<OpenYAMM::Engine::ImagePixelsBgra> image =
        OpenYAMM::Engine::decodeImagePixelsBgra(bytes, pngPath.generic_string());

    REQUIRE(image.has_value());
    CHECK(image->width > 0);
    CHECK(image->height > 0);
    CHECK(image->pixels.size() == static_cast<size_t>(image->width) * static_cast<size_t>(image->height) * 4);
}

TEST_CASE("ImageAssetLoader applies palette zero transparency without override palette")
{
    OpenYAMM::Engine::ImageDecodeOptions options = {};
    options.applyPaletteZeroTransparencyKey = true;
    const std::vector<uint8_t> bytes = makeTwoPixelIndexedBmp();

    const std::optional<OpenYAMM::Engine::ImagePixelsBgra> image =
        OpenYAMM::Engine::decodeImagePixelsBgra(bytes, "indexed-zero-key.bmp", options);

    REQUIRE(image.has_value());
    REQUIRE(image->width == 2);
    REQUIRE(image->height == 1);
    REQUIRE(image->pixels.size() == 8);
    CHECK(image->pixels[0] == 252);
    CHECK(image->pixels[1] == 252);
    CHECK(image->pixels[2] == 0);
    CHECK(image->pixels[3] == 0);
    CHECK(image->pixels[4] == 0);
    CHECK(image->pixels[5] == 0);
    CHECK(image->pixels[6] == 255);
    CHECK(image->pixels[7] == 255);
}

TEST_CASE("ImageAssetLoader decodes indexed PCX and applies the teal transparency key")
{
    OpenYAMM::Engine::ImageDecodeOptions options = {};
    options.applyTealTransparencyKey = true;
    const std::vector<uint8_t> bytes = makeTwoPixelIndexedPcx();

    const std::optional<OpenYAMM::Engine::ImagePixelsBgra> image =
        OpenYAMM::Engine::decodeImagePixelsBgra(bytes, "dialogue-background.pcx", options);

    REQUIRE(image.has_value());
    REQUIRE(image->width == 2);
    REQUIRE(image->height == 1);
    REQUIRE(image->pixels.size() == 8);
    CHECK(image->pixels[3] == 0);
    CHECK(image->pixels[4] == 30);
    CHECK(image->pixels[5] == 20);
    CHECK(image->pixels[6] == 10);
    CHECK(image->pixels[7] == 255);
}

TEST_CASE("ImageAssetLoader keys the original item index without erasing matching nonzero colors")
{
    std::vector<uint8_t> bytes = makeTwoPixelIndexedBmp();
    for (size_t offset : {54u, 58u})
    {
        bytes[offset] = 216;
        bytes[offset + 1] = 0;
        bytes[offset + 2] = 252;
    }
    OpenYAMM::Engine::ImageDecodeOptions options = {};
    options.applyPaletteZeroTransparencyKey = true;
    options.applyMagentaTransparencyKey = true;
    options.applyTealTransparencyKey = true;
    const std::optional<OpenYAMM::Engine::ImagePixelsBgra> image =
        OpenYAMM::Engine::decodeImagePixelsBgra(bytes, "item-with-duplicate-colors.bmp", options);
    REQUIRE(image.has_value());
    CHECK(image->pixels == std::vector<uint8_t>{216, 0, 252, 0, 216, 0, 252, 255});

    options.applyPaletteZeroTransparencyKey = false;
    const std::optional<OpenYAMM::Engine::ImagePixelsBgra> opaqueImage =
        OpenYAMM::Engine::decodeImagePixelsBgra(bytes, "opaque-background.bmp", options);
    REQUIRE(opaqueImage.has_value());
    CHECK(opaqueImage->pixels == std::vector<uint8_t>{216, 0, 252, 255, 216, 0, 252, 255});
}
