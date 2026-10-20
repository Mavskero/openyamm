#include "AppleOutdoorPreview.h"
#include <iostream>

int main(int argc, char **argv)
{
    const bool inspection = argc == 6 && std::string(argv[1]) == "--inspection";
    if (argc != 3 && !inspection)
    {
        std::cerr << "Usage: openyamm_outdoor_preview ASSETS CAPTURE_DIRECTORY\n"
            "       openyamm_outdoor_preview --inspection STAGING BASE MAP NEW_CAPTURE_DIRECTORY\n";
        return 2;
    }
    try
    {
        if (inspection)
        {
            if (std::filesystem::exists(argv[5]))
            {
                throw std::runtime_error("Inspection captures require a new directory");
            }
            std::filesystem::create_directories(argv[5]);
            return runAppleOutdoorPreview(std::filesystem::path(argv[2]) / "inspection/merge-scenes",
                argv[5], true, argv[3], argv[4]);
        }
        std::filesystem::create_directories(argv[2]);
        return runAppleOutdoorPreview(argv[1], argv[2], true);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
