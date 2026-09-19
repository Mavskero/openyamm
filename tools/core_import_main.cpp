#include "mmerge_import/CoreResources.h"
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: core_import SOURCE NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertCoreResources(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Core conversion failed: " << error.what() << '\n';
        return 1;
    }
}
