#include "game.h"

namespace witness {

void GoToScreen(Game& g, Screen screen)
{
    if (g.screen == screen) return;
    g.screen = screen;
    if (screen == SCREEN_PLAYING) g.elapsed = 0.0f;
}

void UpdateGame(Game& g, float dt)
{
    if (g.screen == SCREEN_PLAYING) g.elapsed += dt;
}

}