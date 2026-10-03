#include "game.h"

#include <cmath>

namespace witness {

// Records the ball's recent path as a dotted line. A fixed ring buffer keeps memory
// bounded: the oldest dot is overwritten once the buffer is full, which is exactly the
// behaviour the theme wants - the past stops being available.

void Trail::Push(Vector2 p, float now)
{
    // Distance based sampling gives even dots without spending a sample per frame.
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
    // Everything currently on the board becomes the previous attempt's route. It stays
    // readable for a while so the player can learn from it, then it decays like
    // everything else. lastPush is reset so the first new dot appears immediately.
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
    (void)dt;   // decay is computed at draw time from the clock, so there is nothing to integrate

    const StageConfig& cfg = StageTuning(g.board.stage);

    // Every stage shortens the memory a little. The player controls the overall
    // amount through the settings slider, but the game always gets quieter about
    // the past as it goes on.
    g.trail.life      = cfg.memory * 2.4f;   // the settings slider is applied when drawing
    g.trail.ghostLife = 8.0f + cfg.memory * 4.0f;
    g.trail.ghostGain = cfg.ghostGain;

    if (g.screen == SCREEN_PLAYING && !g.ball.lost)
    {
        g.trail.Push(g.ball.pos, g.clock);
    }
}

} // namespace witness
