#include "game.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;
int g_checks   = 0;

void Check(bool ok, const char* what)
{
    g_checks++;
    if (ok) { std::printf("  ok   %s\n", what); return; }
    g_failures++;
    std::printf("  FAIL %s\n", what);
}

struct FakeKeys
{
    bool w = false, a = false, s = false, d = false;
} g_keys;

}

bool IsKeyDown(int key)
{
    switch (key)
    {
        case KEY_W: return g_keys.w;
        case KEY_A: return g_keys.a;
        case KEY_S: return g_keys.s;
        case KEY_D: return g_keys.d;
        default:    return false;
    }
}

bool IsKeyPressed(int key)   { (void)key; return false; }
bool IsMouseButtonPressed(int) { return false; }

namespace witness { namespace sound {
void Init() {}
void Update(float) {}
void SetMasterVolume(float) {}
void NotifyUserGesture() {}
void Pop() {}
void Shutdown() {}
bool HasTrack() { return false; }
bool HasPop() { return false; }
const char* TrackPath() { return ""; }
}}

namespace witness {
void DrawFrame(Game&) {}
void DrawScreenUi(Game&) {}
}

namespace {

using namespace witness;

constexpr float STEP = 1.0f / 60.0f;

void Step(Game& g, int frames)
{
    for (int i = 0; i < frames; i++) UpdateGame(g, STEP);
}

void DashAt(Game& g, int tileIndex)
{
    const Rectangle r = TileRect(g.board.tiles[tileIndex]);

    g.ball.pos    = { r.x - 200.0f, r.y + r.height * 0.5f };
    g.ball.vel    = { 0.0f, 0.0f };
    g.ball.lost   = false;
    g.ball.squash = 0.0f;

    g_keys.w = g_keys.s = false;
    g_keys.a = false;
    g_keys.d = true;

    Step(g, 90);

    g_keys.d = false;
    Step(g, 45);
}

struct WindowSize { float w, h; };

void CheckLayout(const WindowSize& win)
{
    const Layout l = ComputeLayoutFor(win.w, win.h);
    char label[96];

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the board fully on screen",
                  win.w, win.h);
    Check(l.viewX >= -0.5f && l.viewY >= -0.5f &&
          l.viewX + l.viewW <= l.screenW + 0.5f &&
          l.viewY + l.viewH <= l.screenH + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the top hint above the board",
                  win.w, win.h);
    Check(l.topRowY + l.hintSize <= l.viewY + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the control rows below the board",
                  win.w, win.h);
    Check(l.viewY + l.viewH <= l.controlRow2Y + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the story line inside the top band",
                  win.w, win.h);
    Check(l.storyY >= 0.0f && l.storyY + l.storySize <= l.topRowY + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the controls inside the bottom band",
                  win.w, win.h);
    Check(l.controlRowY + l.hintSize <= l.screenH + 0.5f &&
          l.controlRowY > l.controlRow2Y, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f never degenerates to a zero scale",
                  win.w, win.h);
    Check(l.scale > 0.05f, label);
}

int TotalPips(const Game& g)
{
    int n = 0;
    for (int i = 0; i < MAX_TILES; i++) n += g.board.tiles[i].pipsLeft;
    return n;
}

}

int main()
{
    std::printf("i forgor - simulation smoke test\n");

    Game g;
    InitBoard(g.board);
    ResetBall(g);

    GoToScreen(g, SCREEN_PLAYING);

    int intact = 0, keystones = 0, pips = 0;
    for (int i = 0; i < MAX_TILES; i++)
    {
        if (!g.board.tiles[i].gone) intact++;
        if (g.board.tiles[i].keystone) keystones++;
        pips += TileTotalPips(g.board.tiles[i]);
        Check(TileTotalPips(g.board.tiles[i]) > 0, "every tile starts with pips");
    }
    Check(intact == MAX_TILES, "board starts with every tile standing");
    Check(keystones > 0, "board contains faster-to-break keystones");
    Check(pips > 40, "board starts with a meaningful amount of pips");
    std::printf("  (%d tiles, %d keystones, %d pips)\n", intact, keystones, pips);

    const Vector2 start = g.ball.pos;
    g_keys.d = true;
    Step(g, 60);
    g_keys.d = false;
    Check(g.ball.pos.x > start.x + 40.0f, "the ball accelerates on its own");
    Check(VLen(g.ball.vel) > 100.0f, "the ball keeps momentum after input stops");

    Step(g, 240);
    Check(VLen(g.ball.vel) < 5.0f, "the ball coasts to a stop without input");

    Check(g.trail.count > 5, "the trail records the route just travelled");

    g_keys.d = true;
    Step(g, 30);
    g_keys.d = false;

    RestartAttempt(g, false);
    bool ghosts = false;
    for (int i = 0; i < TRAIL_CAP; i++) if (g.trail.samples[i].ghost) ghosts = true;
    Check(ghosts, "a retry keeps the previous route visible as a ghost");
    Check(g.trail.count > 5, "the ghost route is still there after the reset");

    const int pipsBefore = TotalPips(g);

    const int keystone = 1;
    DashAt(g, keystone);
    Check(g.board.tiles[keystone].gone, "one full-speed dash empties a keystone");
    Check(g.board.emptied == 1, "an emptied tile is counted as an erasure");
    Check(TotalPips(g) < pipsBefore, "hard collisions strip pips from tiles");

    const int heavy = 0;
    int runs = 0;
    while (!g.board.tiles[heavy].gone && runs < 12)
    {
        DashAt(g, heavy);
        runs++;
    }
    Check(g.board.tiles[heavy].gone, "repeated committed runs eventually empty a heavy tile");
    std::printf("  (%d committed runs emptied a %d-pip tile)\n", runs,
                TileTotalPips(g.board.tiles[heavy]));

    g.board.emptied = g.board.sealNeed;
    g.board.sealOpen = true;
    Check(g.board.sealOpen, "the door opens once enough tiles are gone");

    const int tilesGoneBefore = g.board.emptiedTotal;
    g.board.stage = 0;
    g.ball.pos = { DOOR_X + DOOR_W * 0.5f, DOOR_Y + DOOR_H * 0.5f };
    Step(g, 2);

    Check(g.transition.target == SCREEN_CLEAR, "reaching the exit queues the clear beat");
    Step(g, 30);
    Check(g.screen == SCREEN_CLEAR, "the clear beat arrives through a fade, not a cut");
    Step(g, 200);
    Check(g.board.stage == 1, "the next passage starts after the clear beat");
    Check(g.board.emptiedTotal == tilesGoneBefore, "erased tiles are not restored on the new passage");

    bool goneSurvived = false;
    for (int i = 0; i < MAX_TILES; i++) if (g.board.tiles[i].gone) goneSurvived = true;
    Check(goneSurvived, "the world keeps its damage");

    const float life1 = g.trail.life;
    EnterStage(g, 3);
    Step(g, 1);
    Check(g.trail.life < life1, "each passage holds less history");

    for (int stage = 1; stage < STAGE_COUNT; stage++)
    {
        g.board.emptied = g.board.sealNeed;
        g.board.sealOpen = true;
        g.ball.lost = false;
        g.ball.pos = { DOOR_X + DOOR_W * 0.5f, DOOR_Y + DOOR_H * 0.5f };
        g.screen = SCREEN_PLAYING;
        Step(g, 2);
        Step(g, 200);
    }
    Check(g.screen == SCREEN_ENDING, "the last exit leads to the ending");

    Step(g, 60 * 12);
    Check(g.screen == SCREEN_CREDITS, "the ending resolves into the credits");
    Check(g.finished, "the finished run is marked as finished");

    Game before;
    InitBoard(before.board);
    before.board.stage = 3;
    before.board.emptied = 2;
    for (int i = 0; i < 5; i++)
    {
        before.board.tiles[i].pipsLeft = i;
        before.board.tiles[i].gone     = (i == 0);
    }
    SaveSession(before);

    Game after;
    const bool loaded = LoadSession(after);
    Check(loaded, "a stored session is found");
    Check(after.board.stage == 3, "the stored passage is restored");
    Check(after.board.tiles[0].gone, "an erased tile is still erased after a restart");
    Check(after.board.tiles[4].pipsLeft == 4, "partial damage survives the restart");
    Check(after.board.tiles[5].pipsLeft == TileTotalPips(after.board.tiles[5]),
          "untouched tiles are reset to full");

    StartNewGame(g);
    Check(g.board.stage == 0, "starting over returns to the first passage");
    Check(g.board.emptiedTotal == 0, "starting over restores every tile");
    Check(g.trail.count == 0, "starting over wipes the trace");
    Check(g.transition.target == SCREEN_PLAYING, "starting over transitions into play");

    Game lost;
    InitBoard(lost.board);
    ResetBall(lost);
    lost.screen = SCREEN_PLAYING;
    lost.ball.pos = { -100.0f, 300.0f };
    Step(lost, 60);
    Check(lost.screen == SCREEN_PLAYING, "falling off the edge does not crash the game");
    Check(!lost.ball.lost, "the ball is returned to the start after leaving the void");

    {
        const WindowSize sizes[] =
        {
            { 1280.0f,  720.0f }, { 1920.0f, 1080.0f }, { 1600.0f,  900.0f },
            { 1024.0f,  600.0f }, {  800.0f,  600.0f }, {  640.0f,  480.0f },
            {  384.0f,  240.0f }, { 3840.0f, 2160.0f },
        };
        for (const WindowSize& win : sizes) CheckLayout(win);
    }

    ClearSession();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
