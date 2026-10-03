// Development tool: opens the real window, drives the real game code with scripted
// states, and writes PNGs of each scene. It exists because the layout, typography and
// pip rendering need to be looked at, and because a remote display cannot be screenshotted
// from outside the process.
//
// Build and run:  make capture            (writes build-capture/*.png)

#include "game.h"
#include "platform.h"
#include "sound.h"
#include "ui.h"

#include "raylib.h"
#include "rlgl.h"

#include <cmath>
#include <cstdio>
#include <string>

#ifndef OUTPUT_DIR
#define OUTPUT_DIR "."
#endif

using namespace witness;

namespace {

int  g_shot = 0;
Game g_shotGame;

char g_shotFile[192];
bool g_shotSave = false;

// Runs from inside DrawFrame, with the finished frame still unread, and exports it.
// Reading the pixels after EndDrawing would return whatever the swapped-back buffer holds.
void OnFrameDrawn()
{
    if (!g_shotSave) return;
    g_shotSave = false;

    // Raylib's 2D drawing is batched and normally flushed by EndDrawing, so the batch has
    // to be pushed out by hand before the pixels are read.
    rlDrawRenderBatchActive();

    unsigned char* pixels = rlReadScreenPixels(SCREEN_W, SCREEN_H);
    Image image = { pixels, SCREEN_W, SCREEN_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    ExportImage(image, g_shotFile);
    RL_FREE(pixels);
}

// Renders one frame and writes it to disk as shot_NN_<name>.png.
void Shot(const char* name)
{
    std::snprintf(g_shotFile, sizeof(g_shotFile), "%s/shot_%02d_%s.png", OUTPUT_DIR, ++g_shot, name);
    g_shotSave = true;

    DrawFrame(g_shotGame);

    std::printf("  %s\n", g_shotFile);
}

// Advances the game a fixed amount of simulated time and renders each frame.
void Advance(float seconds, float step = 1.0f / 60.0f)
{
    for (float t = 0.0f; t < seconds; t += step) UpdateGame(g_shotGame, step);
}

void RenderFrames(int count)
{
    for (int i = 0; i < count; i++) DrawFrame(g_shotGame);
}

} // namespace

int main(int argc, char** argv)
{
    const bool full = (argc > 1 && std::string(argv[1]) == "--full");

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(SCREEN_W, SCREEN_H, "AFTER THE FALL - capture");
    SetTargetFPS(60);

    Game& g = g_shotGame;
    FrameDrawn = OnFrameDrawn;

    InitBoard(g.board);
    ResetBall(g);

    // Same font and Raygui styling the real game uses, otherwise the shots lie about
    // how the menus look.
    int fontSize = 0;
    g.fontLoaded = false;
    g.font = LoadUiFont(g.fontLoaded, fontSize);
    StyleUi(g.font);

    g.itchUrl  = "https://itch.io/example-page";
    g.hasSave  = true;
    g.settings.introSeen = true;

    // Opening sequence, sampled across its timeline.
    GoToScreen(g, SCREEN_INTRO);
    Advance(3.2f);  Shot("intro_title");
    Advance(3.0f);  Shot("intro_line_a");
    Advance(3.0f);  Shot("intro_line_b");
    Advance(3.6f);  Shot("intro_instruction");
    Advance(2.0f);  Shot("intro_into_play");

    // The opening hands over to gameplay through a fade; the scripted shots below are
    // about single screens, so any fade in flight is dropped here.
    g.transition = {};
    g.transition.t   = 0.0f;
    g.transition.speed = 2.0f;

    // Play: a board with a route drawn on it, some tiles stripped, some erased.
    GoToScreen(g, SCREEN_PLAYING);
    g.board.stage    = 1;
    g.board.emptied  = 1;
    g.board.sealOpen = false;
    g.board.tiles[1].gone      = true;   // a keystone erased
    g.board.tiles[6].pipsLeft  = 1;      // half stripped
    g.board.tiles[13].pipsLeft = 3;
    g.board.tiles[0].pipsLeft  = 6;
    g.board.tiles[13].hitFlash = 0.8f;
    g.ball.pos = { 430.0f, 520.0f };
    g.ball.vel  = { 300.0f, -120.0f };
    g.ball.squash = 0.45f;
    g.ball.impactAngle = 2.4f;

    // Lay a fake route through the trail ring buffer so the dotted trace can be seen.
    for (int i = 0; i < 60; i++)
    {
        const float t = i / 60.0f;
        g.trail.Push({ 120.0f + 520.0f * t, 640.0f - 120.0f * t + 60.0f * std::sin(t * 6.0f) }, -t * 1.6f);
    }
    g.trail.ghostGain = 0.34f;
    g.story.Say("YOU LEFT THIS HERE.", 30.0f);
    g.stageTime = 1.0f;
    RenderFrames(2);
    Shot("play_stage2");

    // Later passage: shorter memory, quieter world, more damage.
    g.board.stage   = 4;
    g.board.emptied = 2;
    g.board.emptiedTotal = 9;
    g.board.sealOpen = true;
    g.trail.Clear();
    for (int i = 2; i < 10; i += 2) g.board.tiles[i].gone = true;
    g.story.Say("WHAT DID YOU LEAVE BEHIND?", 30.0f);
    RenderFrames(2);
    Shot("play_stage5_open");

    // Menu, pause, settings.
    GoToScreen(g, SCREEN_MENU);
    RenderFrames(2);
    Shot("menu");

    GoToScreen(g, SCREEN_PAUSE);
    RenderFrames(2);
    Shot("pause");

    g.returnScreen = SCREEN_MENU;
    GoToScreen(g, SCREEN_SETTINGS);
    RenderFrames(2);
    Shot("settings");

    // Stage clear beat. The last passage has no clear line of its own, so this uses a
    // middle passage to show what a player actually sees.
    g.board.stage = 2;
    GoToScreen(g, SCREEN_CLEAR);
    Advance(0.6f);
    Shot("clear");

    // Ending sequence.
    GoToScreen(g, SCREEN_ENDING);
    Advance(2.6f);  Shot("ending_a");
    Advance(2.2f);  Shot("ending_b");
    Advance(2.9f);  Shot("ending_c");

    // Credits.
    GoToScreen(g, SCREEN_CREDITS);
    Advance(2.4f);  Shot("credits");

    if (full)
    {
        Advance(1.6f);
        Shot("credits_after");
    }

    if (g.fontLoaded) UnloadFont(g.font);
    CloseWindow();
    std::printf("wrote %d shots to %s\n", g_shot, OUTPUT_DIR);
    return 0;
}
