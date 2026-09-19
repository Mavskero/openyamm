#include "AppleOutdoorPreview.h"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: openyamm_outdoor_preview ASSETS CAPTURE_DIRECTORY\n";
        return 2;
    }
    try
    {
        std::filesystem::create_directories(argv[2]);
        return runAppleOutdoorPreview(argv[1], argv[2], true);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
