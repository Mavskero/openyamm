#include "mmerge_import/VoiceResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: voice_import SOURCE BASE_ASSETS NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertVoiceResources(argv[1], argv[2], argv[3]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Voice conversion failed: " << error.what() << '\n';
        return 1;
    }
}
