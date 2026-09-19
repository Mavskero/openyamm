#include "tools/mmerge_import/RuntimeResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: runtime_import SOURCE_DIRECTORY GAMEPLAY_LAYOUT_DIRECTORY NEW_OUTPUT_DIRECTORY\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertRuntimeResources(argv[1], argv[2], argv[3],
            [](const std::string &stage, size_t current, size_t total)
            {
                std::cerr << current << '/' << total << ' ' << stage << '\n';
            });
        std::cout << "Native runtime conversion and starting-map loading passed; see conversion.json for limits.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
