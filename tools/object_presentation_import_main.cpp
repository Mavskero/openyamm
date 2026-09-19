#include "mmerge_import/ObjectPresentationResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: object_presentation_import SOURCE GAME_ASSETS NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertObjectPresentationResources(argv[1], argv[2], argv[3]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Object presentation conversion failed: " << error.what() << '\n';
        return 1;
    }
}
