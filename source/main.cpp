#include <iostream>
#include "game.h"
#include <eng.h>


int main()
{
    Game* game = new Game();
    eng::Engine &engine = eng::Engine::getInstance();
    engine.setApplication(game);

    if(engine.init(1280, 720)){
        engine.run();
    }

    engine.Destroy();
    return 0;
}