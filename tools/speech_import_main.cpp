#include "mmerge_import/SpeechResources.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cerr << "Usage: speech_import SOURCE BASE_ASSETS NEW_DESTINATION\n";
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertSpeechResources(argv[1], argv[2], argv[3]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Speech conversion failed: " << error.what() << '\n';
        return 1;
    }
}
