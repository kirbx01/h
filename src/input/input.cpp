#include "game.h"
#include "sound.h"

namespace witness {

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

            if (anyKey || anyClick || confirm)
            {
                g.settings.introSeen = true;
                StartNewGame(g);
            }
            break;

        case SCREEN_PLAYING:
            if (g.cutsceneInputLock)
            {
                const bool movementHeld = IsKeyDown(KEY_A) || IsKeyDown(KEY_D) ||
                    IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_UP) ||
                    IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT);
                if (!movementHeld) g.cutsceneInputLock = false;
                break;
            }
            if (IsKeyPressed(KEY_ESCAPE))          GoToScreen(g, SCREEN_PAUSE);
            else if (IsKeyPressed(KEY_R))          RestartAttempt(g, false);
            else if (IsKeyPressed(KEY_P))          GoToScreen(g, SCREEN_PAUSE);
            break;

        case SCREEN_CUTSCENE:
            if (g.cutsceneInputLock)
            {
                while (GetKeyPressed() != 0) {}
                const bool inputHeld = IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_SPACE) ||
                    IsKeyDown(KEY_KP_ENTER) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D) ||
                    IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_UP) ||
                    IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) ||
                    IsMouseButtonDown(MOUSE_BUTTON_LEFT);
                if (!inputHeld) g.cutsceneInputLock = false;
                break;
            }
            if (GetKeyPressed() != 0 || anyClick) g.cutsceneSkip = true;
            break;

        case SCREEN_HELP:
            if (g.helpInputLock)
            {
                g.helpInputLock = false;
                break;
            }
            if (IsKeyPressed(KEY_ESCAPE))
                GoToScreen(g, g.returnScreen);
            else if (g.helpPage == 0 &&
                     (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_W) ||
                      IsKeyPressed(KEY_S) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) ||
                      IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)))
            {
                g.helpPage = 1;
                g.helpInputLock = true;
            }
            else if ((g.helpPage == 1 || g.helpPage == 2 || g.helpPage == 3) &&
                     g.focus == 1 && confirm)
            {
                g.helpPage++;
                g.helpInputLock = true;
            }
            else if (g.helpPage == 4 && IsKeyPressed(KEY_R))
            {
                GoToScreen(g, g.returnScreen);
            }
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

}
