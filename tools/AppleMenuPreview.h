#pragma once

#include <filesystem>

// Displays the engine's menu with converted source art for eight seconds.
// Returns after capture or failure; this does not start a gameplay session.
int runAppleMenuPreview(const std::filesystem::path &assets, const std::filesystem::path &capturePath);
