#include "AppleStoryCheck.h"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: openyamm_story_check ASSETS NEW_CHECK_DIRECTORY\n";
        return 2;
    }
    return runAppleStoryCheck(argv[1], argv[2]);
}
