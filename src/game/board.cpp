#include "game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>

namespace witness {

namespace {

constexpr float PIP_MISMATCH_PENALTY = 2.0f;

constexpr struct
{
    float x, y;
    bool  horizontal;
    int   a, b;
    bool  keystone;
} LAYOUT[] =
{
    { 122.0f, 590.0f, false, 6, 6, false },
    { 256.0f, 476.0f, false, 0, 1, true  },
    { 390.0f, 370.0f, false, 5, 5, false },
    { 524.0f, 252.0f, false, 4, 4, false },

    { 658.0f, 476.0f, true,  2, 6, false },
    { 792.0f, 476.0f, true,  5, 6, false },
    { 658.0f, 370.0f, true,  1, 2, true  },
    { 792.0f, 370.0f, true,  3, 3, false },

    { 926.0f, 252.0f, false, 6, 5, false },
    { 926.0f, 134.0f, true,  2, 3, true  },
    { 1060.0f, 370.0f, false, 4, 6, false },
    { 926.0f, 370.0f, false, 0, 3, true  },

    { 390.0f, 590.0f, true,  6, 4, false },
    { 792.0f, 590.0f, true,  3, 5, false },
    { 1060.0f, 590.0f, true, 0, 2, true  },
    { 658.0f, 590.0f, true,  1, 3, false },
};

static_assert(sizeof(LAYOUT) / sizeof(LAYOUT[0]) == MAX_TILES, "layout must fill the board");

}

const Vector2 BALL_START = { 122.0f, 664.0f };

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

void InitBoard(Board& b, int stage)
{
    b.stage        = std::clamp(stage, 0, STAGE_COUNT - 1);
    b.emptied      = 0;
    b.emptiedTotal = 0;
    b.sealOpen     = false;
    b.doorPulse    = 0.0f;

    static std::mt19937 random(std::random_device{}());
    std::uniform_int_distribution<int> startPip(1, 6);
    std::uniform_int_distribution<int> partnerPip(0, 6);
    std::bernoulli_distribution anchorSide;
    std::array<int, MAX_TILES> slots;
    std::iota(slots.begin(), slots.end(), 0);
    std::shuffle(slots.begin(), slots.end(), random);

    b.startPip = startPip(random);
    for (int i = 0; i < MAX_TILES; i++)
    {
        const int slot = slots[i];
        Domino& d   = b.tiles[i];
        d.center    = { LAYOUT[slot].x, LAYOUT[slot].y };
        d.horizontal= LAYOUT[slot].horizontal;
        const int partner = partnerPip(random);
        if (anchorSide(random))
        {
            d.valueA = b.startPip;
            d.valueB = partner;
        }
        else
        {
            d.valueA = partner;
            d.valueB = b.startPip;
        }
        d.consumedA = false;
        d.consumedB = false;
        d.pipsLeft  = d.valueA + d.valueB;
        d.gone      = false;
        d.keystone  = LAYOUT[slot].keystone;
        d.hitFlash  = 0.0f;
        d.hitCool   = 0.0f;
    }

    const StageConfig& cfg = StageTuning(b.stage);
    b.sealNeed = cfg.sealNeed;
}

static void StripTile(Game& g, Domino& d, bool halfA)
{
    if (d.gone || d.hitCool > 0.0f) return;

    const int consumed = halfA ? d.valueA : d.valueB;
    if (halfA)
    {
        if (d.consumedA) return;
        d.consumedA = true;
        d.valueA = 0;
    }
    else
    {
        if (d.consumedB) return;
        d.consumedB = true;
        d.valueB = 0;
    }

    d.pipsLeft = std::max(0, d.pipsLeft - consumed);
    d.hitFlash  = 1.0f;
    d.hitCool   = HIT_COOLDOWN;

    if (d.consumedA && d.consumedB)
    {
        d.pipsLeft = 0;
        d.gone     = true;
        g.board.emptied++;
        g.board.emptiedTotal++;

        if (!g.board.sealOpen && g.board.emptied >= g.board.sealNeed) g.board.sealOpen = true;
    }
}

static void ActivateDomino(Game& g, Domino& d)
{
    if (d.gone || d.hitCool > 0.0f) return;

    if (!d.consumedA && g.ball.pipValue == d.valueA)
    {
        const int nextValue = d.valueB;
        StripTile(g, d, true);
        g.ball.pipValue = nextValue;
    }
    else if (!d.consumedB && g.ball.pipValue == d.valueB)
    {
        const int nextValue = d.valueA;
        StripTile(g, d, false);
        g.ball.pipValue = nextValue;
    }
    else
    {
        d.hitFlash = 1.0f;
        d.hitCool = HIT_COOLDOWN;
        g.stageTime += PIP_MISMATCH_PENALTY;
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

        if (impact >= EROSION_SPEED) ActivateDomino(g, d);
    }

    if (!g.board.sealOpen)
    {
        const Rectangle doorBox = { DOOR_X, DOOR_Y, DOOR_W, DOOR_H };
        float impact = 0.0f;
        if (ResolveCircleBox(b.pos, b.vel, b.radius, doorBox, 0.70f, impact) && impact > 60.0f)
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
    if (b.pos.x < -m || b.pos.x > (float)DESIGN_W + m ||
        b.pos.y < -m || b.pos.y > (float)DESIGN_H + m)
    {
        b.lost      = true;
        b.lostTimer = 0.0f;
        b.vel       = { 0.0f, 0.0f };
    }
}

}
