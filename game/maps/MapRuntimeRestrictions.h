#pragma once

namespace OpenYAMM::Game
{
struct MapRuntimeRestrictions
{
    bool allowSaveGame = true;
    bool allowLloydsBeacon = true;
    bool allowRest = true;
    bool isArena = false;
    bool townPortalUsesDimensionDoor = false;
};
}
