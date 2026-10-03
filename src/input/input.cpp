#include "game.h"

namespace witness {

void HandleInput(Game& g)
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (g.screen == SCREEN_PLAYING || g.screen == SCREEN_ENDING) GoToScreen(g, SCREEN_MENU);
    }

    if (g.screen != SCREEN_PLAYING) return;

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        GoToScreen(g, SCREEN_ENDING);
}

}