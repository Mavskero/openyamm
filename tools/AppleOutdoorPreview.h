#pragma once
#include <filesystem>

int runAppleOutdoorPreview(const std::filesystem::path &assets,
    const std::filesystem::path &captureDirectory, bool automated = false);
