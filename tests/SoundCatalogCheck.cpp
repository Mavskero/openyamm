#include "game/audio/SoundCatalog.h"

#include <iostream>
#include <stdexcept>

namespace
{
using namespace OpenYAMM::Game;
using Row = std::vector<std::string>;
using Rows = std::vector<Row>;

void require(bool value)
{
    if (!value)
    {
        throw std::runtime_error("Sound catalog contract assertion failed");
    }
}
}

int main()
{
    try
    {
        SoundCatalog catalog;
        std::string error;
        const Rows legacy = {{"fountain", "10", "0", "3d"}, {"click", "19", "system", "-"}};
        require(catalog.loadFromRows(legacy) && catalog.findById(10)->positional);
        require(!catalog.loadFromRows({legacy[0], legacy[0]}));
        Rows ordered = {{"#SoundCatalogMode", "MM8OrderedV1"}, {"Name", "Id", "Type", "Is3D"},
            {"null", "0", "system", "-"}, {"fountain", "10", "0", "x"}, {"other", "10", "swap", "-"},
            {"locked", "19", "lock", "-"}, {"unused", "0", "0", "-"}};
        require(catalog.loadFromScopedRows(ordered, {}, error));
        require(catalog.findById(10)->name == "fountain" && catalog.findById(10)->positional);
        require(catalog.findById(0)->name == "null" && catalog.findById(0)->loadType == 1);
        require(catalog.findById(19)->loadType == 4 && !catalog.findById(19)->positional);
        require(!catalog.loadFromScopedRows({}, ordered, error));
        for (const std::pair<size_t, std::string> &fault : {
            std::pair<size_t, std::string>{1, "-1"}, {1, "4294967296"}, {1, "12x"}, {2, "3"}, {3, "3d"}})
        {
            Rows rows = ordered;
            rows[3][fault.first] = fault.second;
            require(!catalog.loadFromScopedRows(rows, {}, error));
        }
        ordered[0][1] = "Unknown";
        require(!catalog.loadFromScopedRows(ordered, {}, error));
        require(catalog.loadFromRows(legacy) && !catalog.findById(19)->positional);
        std::cout << "Sound catalog source ordering, positioning, types and legacy contracts passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
