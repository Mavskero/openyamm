#include "mmerge_import/EventResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "Usage: event_import SOURCE CONVERTED_BASE NEW_DESTINATION [MAP_STEM ...]\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertEventResourcesForMaps(argv[1], argv[2], argv[3], {argv + 4, argv + argc});
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Event conversion failed: " << error.what() << '\n';
        return 1;
    }
}
