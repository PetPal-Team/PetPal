// =============================================================================
//  PetPal - main.cpp
//  Entry point. Builds the Game with the SD-card data directory, runs the main
//  loop, and tears down on exit. All real logic lives in Game and its managers.
// =============================================================================
#include "core/Game.h"

#ifdef __3DS__
static const char* kDataDir = "sdmc:/3ds/PetPal/";
#else
static const char* kDataDir = "./"; // host build (tests / tooling)
#endif

int main(int, char**) {
    petpal::Game game(kDataDir);

    if (!game.init()) {
        // Graphics or core init failed; nothing else we can safely do.
        return 1;
    }

    game.run();      // blocks until the player exits
    game.shutdown();
    return 0;
}
