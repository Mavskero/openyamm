#pragma once
#include <filesystem>
#include <string>

int runAppleOutdoorPreview(const std::filesystem::path &assets,
    const std::filesystem::path &captureDirectory, bool automated = false,
    const std::filesystem::path &inspectionBase = {}, const std::string &inspectionMap = {});
