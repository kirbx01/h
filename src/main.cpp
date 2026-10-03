#include "game.h"
#include "platform.h"
#include "sound.h"
#include "ui.h"

#include <algorithm>
#include <cstdlib>

using namespace witness;

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, GAME_TITLE);
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL);

    Game g;

    LoadFonts(g);
    LoadBackground(g);
    StyleUi();
    g.itchUrl = ResolveItchUrl();

    if (LoadSession(g))
    {
        g.settings.introSeen = true;
        g.hasSave = !g.finished;
    }

    if (g.settings.skipIntro && g.settings.introSeen) GoToScreen(g, SCREEN_MENU);

    sound::SetMuted(g.settings.muted);
    sound::Init();

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        float dt = GetFrameTime();
        dt = std::clamp(dt, 0.0f, 0.05f);

        sound::NotifyUserGesture();

        HandleInput(g);
        UpdateGame(g, dt);

        sound::SetMasterVolume(g.settings.volume);
        sound::Update(dt);

        DrawFrame(g);
    }

    if (g.screen == SCREEN_PLAYING || g.screen == SCREEN_PAUSE) SaveSession(g);

    sound::Shutdown();
    UnloadBackground(g);
    UnloadFonts(g);
    platform::Shutdown();
    CloseWindow();
    return EXIT_SUCCESS;
}