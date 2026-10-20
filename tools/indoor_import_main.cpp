#include "mmerge_import/SceneResources.h"

#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    const bool inspection = argc > 1 && std::string(argv[1]) == "--resource-inspection";
    const bool nativeMerge = argc > 1 && std::string(argv[1]) == "--native-merge";
    const int first = inspection || nativeMerge ? 2 : 1;
    if (argc < first + 4)
    {
        std::cerr << "Usage: scene_import [--resource-inspection|--native-merge] "
                     "SOURCE CONVERTED_BASE NEW_OUTPUT MAP [... ]\n";
        return 2;
    }
    try
    {
        const OpenYAMM::Import::SceneImportMode mode = inspection
            ? OpenYAMM::Import::SceneImportMode::ResourceInspection
            : nativeMerge ? OpenYAMM::Import::SceneImportMode::NativeMergeGameplay
                          : OpenYAMM::Import::SceneImportMode::Gameplay;
        OpenYAMM::Import::convertSceneResources(argv[first], argv[first + 1], argv[first + 2],
            {argv + first + 3, argv + argc}, {}, mode);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
