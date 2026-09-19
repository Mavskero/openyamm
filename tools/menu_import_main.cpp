#include "mmerge_import/MenuResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: menu_import <MMMerge source> <new preview directory>\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertMenuResources(argv[1], argv[2]);
        std::cout << "Converted 13 menu images; gameplay resources are incomplete.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
