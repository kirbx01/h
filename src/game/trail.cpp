#include "game.h"

#include <cmath>

namespace witness {

void Trail::Push(Vector2 p, float now)
{

    const float dx = p.x - lastPush.x;
    const float dy = p.y - lastPush.y;
    if (dx * dx + dy * dy < TRAIL_SPACING * TRAIL_SPACING) return;

    TrailSample& s = samples[head];
    s.pos   = p;
    s.born  = now;
    s.ghost = false;

    head = (head + 1) % TRAIL_CAP;
    if (count < TRAIL_CAP) count++;

    lastPush = p;
}

void Trail::MarkGhost()
{

    for (int i = 0; i < TRAIL_CAP; i++) samples[i].ghost = true;
    lastPush = { -1e9f, -1e9f };
}

void Trail::Clear()
{
    head  = 0;
    count = 0;
    lastPush = { -1e9f, -1e9f };
    for (int i = 0; i < TRAIL_CAP; i++)
    {
        samples[i].pos   = { 0, 0 };
        samples[i].born  = 0.0f;
        samples[i].ghost = false;
    }
}

void UpdateTrail(Game& g, float dt)
{
    (void)dt;

    const StageConfig& cfg = StageTuning(g.board.stage);

    g.trail.life      = cfg.memory * 2.4f;
    g.trail.ghostLife = 8.0f + cfg.memory * 4.0f;
    g.trail.ghostGain = cfg.ghostGain;

    if (g.screen == SCREEN_PLAYING && !g.ball.lost)
    {
        g.trail.Push(g.ball.pos, g.clock);
    }
}

}
