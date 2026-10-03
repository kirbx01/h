#include "game.h"
#include "platform.h"
#include "sound.h"
#include "ui.h"

#include <cstdlib>

using namespace witness;

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, GAME_TITLE);
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL);          // the game owns ESC itself

    Game g;

    int fontSize = UI_FONT_SIZE;
    g.font = LoadUiFont(g.fontLoaded, fontSize);
    StyleUi(g.font);

    g.bgLoaded = LoadBackground(g);
    g.itchUrl  = ResolveItchUrl();

    // A stored session restores the board as it was left, tiles and all. A finished run
    // is remembered only as the fact that it happened.
    if (LoadSession(g))
    {
        g.settings.introSeen = true;
        g.hasSave            = !g.finished;
    }

    if (g.settings.skipIntro && g.settings.introSeen) GoToScreen(g, SCREEN_MENU);

    sound::Init();

    while (!WindowShouldClose())
    {
        // A clamped dt keeps a hitch from teleporting the ball through the arrangement.
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.0f) dt = 0.0f;

        // Browsers only allow audio after a gesture; this is a no-op on desktop.
        sound::NotifyUserGesture();

        HandleInput(g);
        UpdateGame(g, dt);

        sound::SetMasterVolume(g.settings.volume);
        sound::Update(dt, g.screen);

        DrawFrame(g);
    }

    if (g.screen == SCREEN_PLAYING || g.screen == SCREEN_PAUSE) SaveSession(g);

    sound::Shutdown();

    UnloadBackground(g);
    if (g.fontLoaded) UnloadFont(g.font);

    platform::Shutdown();
    CloseWindow();
    return EXIT_SUCCESS;
}
