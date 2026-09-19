#include "mmerge_import/StoryResources.h"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: story_import SOURCE NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertStoryResources(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
