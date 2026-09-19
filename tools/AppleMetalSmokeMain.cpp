#include "tools/AppleMetalSmoke.h"
#include <SDL3/SDL_main.h>

int main(int argc, char **argv)
{
    return runAppleMetalSmoke(argc > 1 ? argv[1] : "metal-smoke.ppm");
}
