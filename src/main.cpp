#include "game.h"
#include "sound.h"

#include <cstdlib>

using namespace witness;

int main(void)
{
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(SCREEN_W, SCREEN_H, "i forgor");
    SetTargetFPS(TARGET_FPS);
    SetExitKey(KEY_NULL);

    Game g;

    int fontSize = UI_FONT_SIZE;
    g.font = LoadUiFont(g.fontLoaded, fontSize);

    StyleUi(g.font);

    g.bgLoaded = LoadBackground(g);

    witness::sound::Init();

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.0f) dt = 0.0f;

        HandleInput(g);
        UpdateGame(g, dt);
        witness::sound::Update(dt);

        DrawFrame(g);
    }

    witness::sound::Shutdown();

    UnloadBackground(g);
    if (g.fontLoaded) UnloadFont(g.font);

    CloseWindow();
    return EXIT_SUCCESS;
}