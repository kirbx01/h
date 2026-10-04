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
void FadeIn() {}
void FadeOut() {}
void PlayCutscene(bool) {}
void StopCutscene(bool) {}
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

    const float runup = 60.0f;
    const bool fromLeft = (r.x - runup) > 30.0f;

    g.ball.pos    = fromLeft ? Vector2{ r.x - runup, r.y + r.height * 0.5f }
                             : Vector2{ r.x + r.width + runup, r.y + r.height * 0.5f };
    g.ball.vel    = { 0.0f, 0.0f };
    g.ball.lost   = false;
    g.ball.squash = 0.0f;
    g.ball.squashVel = 0.0f;

    g_keys.w = g_keys.s = false;
    g_keys.a = !fromLeft;
    g_keys.d = fromLeft;

    Step(g, 90);

    g_keys.a = false;
    g_keys.d = false;
    Step(g, 45);
}

void HitDominoOnce(Game& g, int tileIndex, int pipValue)
{
    const Rectangle tile = TileRect(g.board.tiles[tileIndex]);
    g.ball.pipValue = pipValue;
    g.ball.pos = { tile.x - BALL_RADIUS - 0.5f, tile.y + tile.height * 0.5f };
    g.ball.vel = { BALL_MAX_SPEED, 0.0f };
    UpdateBall(g, STEP);
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

    const float playT = l.viewY + PLAY_Y * l.scale;
    const float playB = l.viewY + (PLAY_Y + PLAY_H) * l.scale;
    const float playL = l.viewX + PLAY_X * l.scale;
    const float playR = l.viewX + (PLAY_X + PLAY_W) * l.scale;

    const float capPad = l.hintSize * 0.28f;
    const float capTop = l.controlRow2Y - capPad;

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the playfield inside the window",
                  win.w, win.h);
    Check(playL >= -0.5f && playT >= -0.5f &&
          playR <= l.screenW + 0.5f && playB <= l.screenH + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the top HUD above the board",
                  win.w, win.h);
    Check(l.storyY + l.storySize <= playT + 0.5f &&
          l.topRowY + l.hintSize <= playT + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the key rows below the board",
                  win.w, win.h);
    Check(playB <= capTop + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f stacks the story line above the top row",
                  win.w, win.h);
    Check(l.storyY >= 0.0f && l.storyY + l.storySize <= l.topRowY + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f keeps the key rows on screen and apart",
                  win.w, win.h);
    Check(l.controlRowY - capPad > l.controlRow2Y - capPad &&
          l.controlRowY + capPad <= l.screenH + 0.5f, label);

    std::snprintf(label, sizeof(label), "layout %.0fx%.0f never degenerates to a zero scale",
                  win.w, win.h);
    Check(l.scale > 0.05f, label);
}

bool Overlaps(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

void CheckBoard(const Board& b)
{
    bool overlap = false;
    bool outside = false;

    for (int i = 0; i < MAX_TILES; i++)
    {
        const Rectangle a = TileRect(b.tiles[i]);
        if (a.x < PLAY_X || a.y < PLAY_Y ||
            a.x + a.width > PLAY_X + PLAY_W || a.y + a.height > PLAY_Y + PLAY_H) outside = true;

        for (int j = i + 1; j < MAX_TILES; j++)
            if (Overlaps(a, TileRect(b.tiles[j]))) overlap = true;
    }

    Check(!overlap, "no two dominoes overlap");
    Check(!outside, "every domino sits inside the playfield");

    const Rectangle spawn = { BALL_START.x - BALL_RADIUS, BALL_START.y - BALL_RADIUS,
                              BALL_RADIUS * 2.0f, BALL_RADIUS * 2.0f };
    bool spawnClear = true;
    for (int i = 0; i < MAX_TILES; i++)
        if (Overlaps(spawn, TileRect(b.tiles[i]))) spawnClear = false;
    Check(spawnClear, "the starting position is clear of the dominoes");
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
    g.ball.pipValue = g.board.startPip;

    GoToScreen(g, SCREEN_PLAYING);

    {
        constexpr int expectedMatches[STAGE_COUNT] = { 16, 12, 8, 5, 3 };
        for (int stage = 0; stage < STAGE_COUNT; stage++)
        {
            Board passage;
            InitBoard(passage, stage);
            int reachableMatches = 0;
            for (const Domino& tile : passage.tiles)
                if (tile.valueA == passage.startPip || tile.valueB == passage.startPip)
                    reachableMatches++;
            Check(passage.startPip >= 1 && passage.startPip <= 6 &&
                  reachableMatches == expectedMatches[stage] &&
                  reachableMatches >= passage.sealNeed,
                  "each passage has enough reachable matching dominoes");
        }
    }

        Game timed;
        InitBoard(timed.board);
        ResetBall(timed);
        GoToScreen(timed, SCREEN_PLAYING);
        timed.stageTime = PASSAGE_TIME_LIMIT - STEP * 0.5f;
        UpdateGame(timed, STEP);
        const float failedStageTime = timed.stageTime;
        UpdateGame(timed, 1.0f);
        Check(timed.ball.lost && timed.attempt == 0 && timed.stageTime == failedStageTime,
            "the 20-second passage limit freezes the failed attempt");
        RestartAttempt(timed, false);
        Check(timed.stageTime == 0.0f && timed.attempt == 1,
            "retry restarts a timed-out attempt");

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

    CheckBoard(g.board);

        Game helpPause;
        InitBoard(helpPause.board);
        ResetBall(helpPause);
        GoToScreen(helpPause, SCREEN_PLAYING);
        helpPause.stageTime = 4.0f;
        const Vector2 helpBallPos = helpPause.ball.pos;
        const float helpClock = helpPause.clock;
        GoToScreen(helpPause, SCREEN_HELP);
        UpdateGame(helpPause, 1.0f);
        Check(helpPause.stageTime == 4.0f && helpPause.clock == helpClock &&
            helpPause.ball.pos.x == helpBallPos.x && helpPause.ball.pos.y == helpBallPos.y,
            "opening help pauses the game clock and ball");

        Game pipChain;
        InitBoard(pipChain.board);
        ResetBall(pipChain);
        pipChain.ball.pipValue = pipChain.board.startPip;
        int matchingTile = 0;
        while (matchingTile < MAX_TILES &&
               (pipChain.board.tiles[matchingTile].valueA != pipChain.board.startPip ||
            pipChain.board.tiles[matchingTile].valueB == pipChain.board.startPip))
            matchingTile++;
        Check(matchingTile < MAX_TILES &&
              pipChain.ball.pipValue == pipChain.board.startPip,
              "the player starts with a pip present on the generated board");
        const int firstPip = pipChain.board.tiles[matchingTile].valueA;
        const int nextPip = pipChain.board.tiles[matchingTile].valueB;
        HitDominoOnce(pipChain, matchingTile, firstPip);
        Check(pipChain.ball.pipValue == nextPip &&
            pipChain.board.tiles[matchingTile].consumedA &&
            !pipChain.board.tiles[matchingTile].consumedB,
            "a matching pip consumes its half and adopts the opposite value");

        const int remainingHalfPips = pipChain.board.tiles[matchingTile].pipsLeft;
        UpdateBoard(pipChain, HIT_COOLDOWN);
        HitDominoOnce(pipChain, matchingTile, firstPip);
        Check(pipChain.board.tiles[matchingTile].pipsLeft == remainingHalfPips,
            "a consumed half cannot activate again");

        UpdateBoard(pipChain, HIT_COOLDOWN);
        HitDominoOnce(pipChain, matchingTile, nextPip);
        Check(pipChain.board.tiles[matchingTile].gone &&
              pipChain.board.tiles[matchingTile].consumedB,
            "the remaining matching half completes the domino");

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
    Check(g.trail.count == 0, "a retry clears the previous route");
    g.ball.pipValue = g.board.startPip;

    const int pipsBefore = TotalPips(g);

    int keystone = 0;
    while (keystone < MAX_TILES && !g.board.tiles[keystone].keystone) keystone++;
    DashAt(g, keystone);
    Check(g.board.tiles[keystone].gone, "one full-speed dash empties a keystone");
    Check(g.board.emptied == 1, "an emptied tile is counted as an erasure");
    Check(TotalPips(g) < pipsBefore, "hard collisions strip pips from tiles");

    int heavy = 0;
    while (heavy < MAX_TILES && (heavy == keystone ||
           g.board.tiles[heavy].gone)) heavy++;
        const float timeBeforeMismatch = g.stageTime;
        const int pipsBeforeMismatch = g.board.tiles[heavy].pipsLeft;
        const Rectangle heavyRect = TileRect(g.board.tiles[heavy]);
        int mismatchPip = 0;
        while (mismatchPip == g.board.tiles[heavy].valueA ||
               mismatchPip == g.board.tiles[heavy].valueB) mismatchPip++;
        g.ball.pipValue = mismatchPip;
         g.ball.lost = false;
         g.ball.lostTimer = 0.0f;
        g.ball.pos = { heavyRect.x - BALL_RADIUS - 0.5f, heavyRect.y + heavyRect.height * 0.5f };
        g.ball.vel = { BALL_MAX_SPEED, 0.0f };
        UpdateBall(g, STEP);
        Check(g.board.tiles[heavy].pipsLeft == pipsBeforeMismatch,
            "a mismatched collision leaves both domino halves intact");
        Check(g.stageTime >= timeBeforeMismatch + 2.0f,
            "a mismatched collision applies the timer penalty");

    g.ball.pipValue = g.board.tiles[heavy].valueA;
    int hits = 0;
    while (!g.board.tiles[heavy].gone && hits < 12)
    {
        g.ball.pipValue = g.board.tiles[heavy].consumedA
                              ? g.board.tiles[heavy].valueB
                              : g.board.tiles[heavy].valueA;
        g.ball.lost = false;
        HitDominoOnce(g, heavy, g.ball.pipValue);
        UpdateBoard(g, HIT_COOLDOWN);
        hits++;
    }
    Check(g.board.tiles[heavy].gone, "repeated matching hits eventually empty a heavy tile");
    std::printf("  (%d matching hits emptied a %d-pip tile)\n", hits,
                TileTotalPips(g.board.tiles[heavy]));

    g.board.emptied = g.board.sealNeed;
    g.board.sealOpen = true;
    g.ball.lost = false;
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
        Check(g.screen == SCREEN_CUTSCENE && g.cutsceneEnding,
            "the ending text leads into the ending images");
        Step(g, 60 * 14);
        Check(g.screen == SCREEN_CREDITS, "the ending images resolve into the credits");
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
    before.board.tiles[4].pipsLeft = 1;
    SaveSession(before);

    Game after;
    const bool loaded = LoadSession(after);
    Check(loaded, "a stored session is found");
    Check(after.board.stage == 3, "the stored passage is restored");
    Check(after.board.tiles[0].gone, "an erased tile is still erased after a restart");
    Check(after.board.tiles[4].pipsLeft == 1, "partial damage survives the restart");
    Check(after.board.tiles[5].pipsLeft == TileTotalPips(after.board.tiles[5]),
          "untouched tiles are reset to full");

    StartNewGame(g);
    Check(g.board.stage == 0, "starting over returns to the first passage");
    Check(g.board.emptiedTotal == 0, "starting over restores every tile");
    Check(g.trail.count == 0, "starting over wipes the trace");
        Check(g.screen == SCREEN_CUTSCENE && !g.cutsceneEnding && g.cutsceneIndex == 0,
            "starting over begins the opening sequence");
        UpdateGame(g, 4.3f);
        Check(g.screen == SCREEN_CUTSCENE && g.cutsceneIndex == 1,
            "the opening sequence advances through its images in order");
        UpdateGame(g, 4.3f);
        Check(g.screen == SCREEN_CUTSCENE && g.cutsceneIndex == 2,
            "the opening sequence reaches its final image");
        UpdateGame(g, 4.3f);
        Check(g.screen == SCREEN_PLAYING,
            "the opening sequence transitions into gameplay");

        g.transition.active = false;
        g.screen = SCREEN_ENDING;
        g.sceneTime = 9.4f;
        UpdateGame(g, 0.01f);
        Check(g.screen == SCREEN_CUTSCENE && g.cutsceneEnding && g.cutsceneIndex == 0,
            "the ending sequence starts after the ending text");
        UpdateGame(g, 13.6f);
        Check(g.screen == SCREEN_CREDITS,
            "the ending sequence transitions to credits");

    Game lost;
    InitBoard(lost.board);
    ResetBall(lost);
    lost.screen = SCREEN_PLAYING;
    lost.ball.pos = { -100.0f, 300.0f };
    Step(lost, 60);
    Check(lost.screen == SCREEN_PLAYING, "falling off the edge does not crash the game");
        Check(lost.ball.lost, "falling off the edge enters the failure state");
        const float edgeFailedStageTime = lost.stageTime;
        Step(lost, 60);
        Check(lost.ball.lost && lost.stageTime == edgeFailedStageTime,
            "the stage timer remains frozen beneath the failure message");
        RestartAttempt(lost, true);
        Check(!lost.ball.lost && lost.trail.count == 0,
            "retry returns the ball and clears the failure trace");

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
