#include "mmerge_import/ClassResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: class_import SOURCE NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertClassResources(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Class conversion failed: " << error.what() << '\n';
        return 1;
    }
}
