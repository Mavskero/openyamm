#include "mmerge_import/DecorationResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: decoration_import SOURCE NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertDecorationResources(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Decoration conversion failed: " << error.what() << '\n';
        return 1;
    }
}
