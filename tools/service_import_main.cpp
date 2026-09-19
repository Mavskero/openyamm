#include "mmerge_import/ServiceResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: service_import SOURCE NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertServiceResources(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Service conversion failed: " << error.what() << '\n';
        return 1;
    }
}
