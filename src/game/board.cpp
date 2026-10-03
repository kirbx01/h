#include "game.h"

#include <algorithm>
#include <cmath>

namespace witness {

namespace {

constexpr float TILE_SHORT = 36.0f;
constexpr float TILE_LONG  = 80.0f;

constexpr struct
{
    float x, y;
    bool  horizontal;
    int   a, b;
    bool  keystone;
} LAYOUT[] =
{

    { 210.0f, 590.0f, false, 6, 6, false },
    { 272.0f, 543.0f, false, 0, 1, true  },
    { 334.0f, 496.0f, false, 5, 5, false },
    { 396.0f, 449.0f, false, 4, 4, false },

    { 566.0f, 424.0f, true,  2, 6, false },
    { 656.0f, 424.0f, true,  5, 6, false },
    { 566.0f, 352.0f, true,  1, 2, true  },
    { 656.0f, 352.0f, true,  3, 3, false },

    { 948.0f, 300.0f, false, 6, 5, false },
    { 1012.0f, 252.0f, false, 2, 3, true  },
    { 1076.0f, 300.0f, false, 4, 6, false },
    { 1012.0f, 348.0f, false, 0, 3, true  },

    { 470.0f, 604.0f, true,  6, 4, false },
    { 772.0f, 528.0f, false, 3, 5, false },
    { 872.0f, 432.0f, true,  0, 2, true  },
    { 704.0f, 626.0f, true,  1, 3, false },
};

static_assert(sizeof(LAYOUT) / sizeof(LAYOUT[0]) == MAX_TILES, "layout must fill the board");

}

const Vector2 BALL_START = { 104.0f, 652.0f };

int TileTotalPips(const Domino& d)
{
    return d.valueA + d.valueB;
}

Rectangle TileRect(const Domino& d)
{
    const float w = d.horizontal ? TILE_LONG : TILE_SHORT;
    const float h = d.horizontal ? TILE_SHORT : TILE_LONG;
    return Rectangle{ d.center.x - w * 0.5f, d.center.y - h * 0.5f, w, h };
}

void InitBoard(Board& b)
{
    b.stage        = 0;
    b.emptied      = 0;
    b.emptiedTotal = 0;
    b.sealOpen     = false;
    b.doorPulse    = 0.0f;

    for (int i = 0; i < MAX_TILES; i++)
    {
        Domino& d   = b.tiles[i];
        d.center    = { LAYOUT[i].x, LAYOUT[i].y };
        d.horizontal= LAYOUT[i].horizontal;
        d.valueA    = LAYOUT[i].a;
        d.valueB    = LAYOUT[i].b;
        d.pipsLeft  = LAYOUT[i].a + LAYOUT[i].b;
        d.gone      = false;
        d.keystone  = LAYOUT[i].keystone;
        d.hitFlash  = 0.0f;
        d.hitCool   = 0.0f;
    }

    const StageConfig& cfg = StageTuning(0);
    b.sealNeed = cfg.sealNeed;
}

static void StripTile(Game& g, Domino& d, float impact)
{
    if (d.gone || d.hitCool > 0.0f) return;

    const int strip = PIPS_PER_HIT + (impact >= PIPS_BONUS_SPEED ? PIPS_BONUS_HIT : 0);

    d.pipsLeft -= strip;
    d.hitFlash  = 1.0f;
    d.hitCool   = HIT_COOLDOWN;

    if (d.pipsLeft <= 0)
    {
        d.pipsLeft = 0;
        d.gone     = true;
        g.board.emptied++;
        g.board.emptiedTotal++;

        if (!g.board.sealOpen && g.board.emptied >= g.board.sealNeed) g.board.sealOpen = true;
    }
}

void UpdateBoard(Game& g, float dt)
{
    for (int i = 0; i < MAX_TILES; i++)
    {
        Domino& d = g.board.tiles[i];
        if (d.hitFlash > 0.0f) d.hitFlash = std::max(0.0f, d.hitFlash - dt * 4.0f);
        if (d.hitCool  > 0.0f) d.hitCool  = std::max(0.0f, d.hitCool  - dt);
    }

    g.board.doorPulse += dt;
}

static bool ResolveCircleBox(Vector2& pos, Vector2& vel, float radius, Rectangle box,
                             float restitution, float& impactOut)
{
    const float closestX = std::clamp(pos.x, box.x, box.x + box.width);
    const float closestY = std::clamp(pos.y, box.y, box.y + box.height);
    const Vector2 closest = { closestX, closestY };

    Vector2 d = VSub(pos, closest);
    float   dist = std::sqrt(d.x * d.x + d.y * d.y);
    if (dist > radius) return false;

    Vector2 n;
    if (dist > 0.0001f)
    {
        n = { d.x / dist, d.y / dist };
        pos = VAdd(closest, VScale(n, radius));
    }
    else
    {

        const float left   = pos.x - box.x;
        const float right  = box.x + box.width - pos.x;
        const float top    = pos.y - box.y;
        const float bottom = box.y + box.height - pos.y;
        const float m      = std::min(std::min(left, right), std::min(top, bottom));

        if      (m == left)   n = { -1.0f,  0.0f };
        else if (m == right)  n = {  1.0f,  0.0f };
        else if (m == top)    n = {  0.0f, -1.0f };
        else                   n = {  0.0f,  1.0f };

        pos = VAdd(pos, VScale(n, radius + m));
    }

    const float vn = VDot(vel, n);
    impactOut = -vn;

    if (vn < 0.0f)
    {
        vel = VSub(vel, VScale(n, vn * (1.0f + restitution)));

        const Vector2 tangent = VSub(vel, VScale(n, VDot(vel, n)));
        vel = VSub(vel, VScale(tangent, 0.14f));
    }

    return true;
}

bool BallInDoor(const Game& g)
{
    if (!g.board.sealOpen) return false;

    const Rectangle inner = { DOOR_X + 14.0f, DOOR_Y + 12.0f, DOOR_W - 28.0f, DOOR_H - 24.0f };
    return g.ball.pos.x > inner.x && g.ball.pos.x < inner.x + inner.width &&
           g.ball.pos.y > inner.y && g.ball.pos.y < inner.y + inner.height;
}

void UpdateBall(Game& g, float dt)
{
    Ball& b = g.ball;

    if (b.lost)
    {
        b.lostTimer += dt;
        return;
    }

    Vector2 dir = { 0.0f, 0.0f };
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;

    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 0.0f) dir = VScale(dir, 1.0f / len);

    if (len > 0.0f) b.vel = VAdd(b.vel, VScale(dir, BALL_ACCEL * dt));
    else            b.vel = VScale(b.vel, std::exp(-BALL_DRAG * dt));

    const float speed = std::sqrt(b.vel.x * b.vel.x + b.vel.y * b.vel.y);
    if (speed > BALL_MAX_SPEED)
    {
        b.vel = VScale(b.vel, BALL_MAX_SPEED / speed);
    }

    b.pos = VAdd(b.pos, VScale(b.vel, dt));

    b.squashVel += (-BALL_SQUASH_STIFFNESS * b.squash - BALL_SQUASH_DAMPING * b.squashVel) * dt;
    b.squash += b.squashVel * dt;
    if (std::abs(b.squash) < 0.001f && std::abs(b.squashVel) < 0.001f)
    {
        b.squash = 0.0f;
        b.squashVel = 0.0f;
    }

    float strongestImpact = 0.0f;
    float strongestAngle  = 0.0f;

    for (int i = 0; i < MAX_TILES; i++)
    {
        Domino& d = g.board.tiles[i];
        if (d.gone) continue;

        float impact = 0.0f;
        if (!ResolveCircleBox(b.pos, b.vel, b.radius, TileRect(d), BALL_RESTITUTION, impact)) continue;

        if (impact > strongestImpact)
        {
            strongestImpact = impact;
            strongestAngle  = std::atan2(-(b.vel.y), -(b.vel.x));
        }

        if (impact >= EROSION_SPEED) StripTile(g, d, impact);
    }

    if (!g.board.sealOpen)
    {
        const Rectangle doorBox = { DOOR_X, DOOR_Y, DOOR_W, DOOR_H };
        float impact = 0.0f;
        if (ResolveCircleBox(b.pos, b.vel, b.radius, doorBox, 0.22f, impact) && impact > 60.0f)
        {
            strongestImpact = std::max(strongestImpact, impact);
            strongestAngle  = std::atan2(-(b.vel.y), -(b.vel.x));

            if (!g.doorHintShown)
            {
                g.doorHintShown = true;
                g.story.Say("Something Is Holding It.", 3.6f);
            }
        }
    }

    if (strongestImpact > 0.0f)
    {
        const float amount = std::min(BALL_SQUASH_MAX, strongestImpact / 900.0f + 0.12f);
        if (amount > b.squash)
        {
            b.squash      = amount;
            b.squashVel   = 0.0f;
            b.impactAngle = strongestAngle;
        }
    }

    const float m = 26.0f;
    if (b.pos.x < -m || b.pos.x > SCREEN_W + m || b.pos.y < -m || b.pos.y > SCREEN_H + m)
    {
        b.lost      = true;
        b.lostTimer = 0.0f;
        b.vel       = { 0.0f, 0.0f };
    }
}

}
