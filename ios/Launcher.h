#pragma once

#import <Foundation/Foundation.h>
#include <filesystem>

// Returns 0 to quit, 1 for game, 2 for Metal check, 3 for menu, 4 for outdoor preview.
int showIosLauncher(NSURL *support, NSString *message, std::filesystem::path &assets);
