#include "mmerge_import/OutdoorResources.h"
#include <iostream>
#include <string_view>

int main(int argc, char **argv)
{
    if (argc != 3 && !(argc == 4 && std::string_view(argv[3]) == "--actors"))
    {
        std::cerr << "Usage: outdoor_import SOURCE NEW_DESTINATION [--actors]\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertOutdoorResources(argv[1], argv[2], argc == 4);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
