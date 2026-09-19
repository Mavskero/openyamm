#include "mmerge_import/UiResources.h"

#include <charconv>
#include <iostream>
#include <string_view>

int main(int argc, char **argv)
{
    uint32_t uiSetId = 0;
    if (argc != 5)
    {
        std::cerr << "Usage: ui_import SOURCE GAMEPLAY_LAYOUT_DIRECTORY UI_SET_ID NEW_DESTINATION\n";
        return 2;
    }
    const std::string_view id(argv[3]);
    const std::from_chars_result parsed = std::from_chars(id.data(), id.data() + id.size(), uiSetId);
    if (parsed.ec != std::errc() || parsed.ptr != id.data() + id.size())
    {
        return 2;
    }
    try
    {
        OpenYAMM::Import::convertUiResources(argv[1], argv[2], uiSetId, argv[4]);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "UI conversion failed: " << error.what() << '\n';
        return 1;
    }
}
