#include "mmerge_import/SceneResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        std::cerr << "Usage: scene_import SOURCE CONVERTED_BASE NEW_OUTPUT MAP.odm|MAP.blv [...]\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertSceneResources(argv[1], argv[2], argv[3], {argv + 4, argv + argc});
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
