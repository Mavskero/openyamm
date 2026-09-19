#include "mmerge_import/TeacherResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: teacher_import SOURCE BASE_ASSETS NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertTeacherResources(argv[1], argv[2], argv[3]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Teacher conversion failed: " << error.what() << '\n';
        return 1;
    }
}
