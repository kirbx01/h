#include "game.h"
#include "sound.h"

namespace witness {

// Input is deliberately thin: each screen decides what a key means, and nothing is
// handled twice. The play screen reads the movement keys directly in ball.cpp.
void HandleInput(Game& g)
{
    const bool confirm  = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                          IsKeyPressed(KEY_KP_ENTER);
    const bool anyKey   = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_W) ||
                          IsKeyPressed(KEY_S) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
                          IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT);
    const bool anyClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    switch (g.screen)
    {
        case SCREEN_INTRO:
            // Skippable at any point, which is what an accessibility-minded opening
            // needs to be. Starting the game here also fades straight into gameplay.
            if (anyKey || anyClick || confirm)
            {
                g.settings.introSeen = true;
                StartNewGame(g);
            }
            break;

        case SCREEN_PLAYING:
            if (IsKeyPressed(KEY_ESCAPE))          GoToScreen(g, SCREEN_PAUSE);
            else if (IsKeyPressed(KEY_R))          RestartAttempt(g, false);
            else if (IsKeyPressed(KEY_P))          GoToScreen(g, SCREEN_PAUSE);
            break;

        case SCREEN_PAUSE:
        case SCREEN_SETTINGS:
        case SCREEN_MENU:
            if (IsKeyPressed(KEY_ESCAPE))
            {
                if (g.screen == SCREEN_SETTINGS) GoToScreen(g, g.returnScreen);
                else if (g.screen == SCREEN_PAUSE) GoToScreen(g, SCREEN_PLAYING);
                else GoToScreen(g, SCREEN_MENU);
            }
            break;

        case SCREEN_ENDING:
            // The ending can be shortened, but it cannot be jumped into the credits
            // from the first frame - give the last line its moment.
            if ((confirm || anyKey) && g.sceneTime > 2.5f) OpenCredits(g);
            break;

        case SCREEN_CREDITS:
            if (IsKeyPressed(KEY_R) && g.sceneTime > 1.0f) StartNewGame(g);
            else if (IsKeyPressed(KEY_ESCAPE))     GoToScreen(g, SCREEN_MENU);
            break;

        default:
            break;
    }
}

} // namespace witness
