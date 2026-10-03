#include "game.h"
#include "ui.h"

#include "rlgl.h"   // matrix stack, used to draw the ball rotated by its velocity

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace witness {

namespace {

//------------------------------------------------------------------------------------
// Typography helpers. The only ornament in this game is type, so it gets the spacing
// treatment that a console game normally avoids: quiet, wide, thin.
//------------------------------------------------------------------------------------
float SmoothStep01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float AlphaOf(float a)
{
    return std::clamp(a, 0.0f, 1.0f);
}

// Draws a string with extra tracking. Returns the width it occupied.
float SpacedWidth(const Font& font, const char* text, float size, float spacing)
{
    if (!text || !*text) return 0.0f;

    float w = 0.0f;
    for (int i = 0; text[i]; i++)
    {
        const char one[2] = { text[i], '\0' };
        w += MeasureTextEx(font, one, size, 0.0f).x;
        if (text[i + 1]) w += spacing;
    }
    return w;
}

void SpacedDraw(const Font& font, const char* text, float x, float y, float size,
                float spacing, Color color)
{
    if (!text || !*text) return;

    float cx = x;
    for (int i = 0; text[i]; i++)
    {
        const char one[2] = { text[i], '\0' };
        DrawTextEx(font, one, { cx, y }, size, 0.0f, color);
        cx += MeasureTextEx(font, one, size, 0.0f).x + spacing;
    }
}

//------------------------------------------------------------------------------------
// Pip layout.
//
// Standard domino pip positions, described in a unit square where u runs along the long
// axis of the tile and v across it. Each position carries a "removal priority": outer
// pips are stripped first, so a tile visibly hollows out from the edges instead of
// vanishing at random.
//------------------------------------------------------------------------------------
struct PipSpot
{
    float u, v;
    int   priority;   // 0 = stripped first, 3 = centre, survives longest
};

// Index order is fixed so "which spots does a 5 show" stays stable.
enum
{
    PIP_TL = 0, PIP_BR, PIP_TR, PIP_BL, PIP_LM, PIP_RM, PIP_CENTRE, PIP_COUNT
};

constexpr PipSpot PIP_SPOTS[PIP_COUNT] =
{
    { 0.26f, 0.22f, 0 },   // top-left
    { 0.74f, 0.78f, 0 },   // bottom-right
    { 0.74f, 0.22f, 1 },   // top-right
    { 0.26f, 0.78f, 1 },   // bottom-left
    { 0.24f, 0.50f, 2 },   // left middle
    { 0.76f, 0.50f, 2 },   // right middle
    { 0.50f, 0.50f, 3 },   // centre
};

// Which spots a half showing `count` pips uses, before sorting by removal priority.
constexpr int COUNT_1[1] = { PIP_CENTRE };
constexpr int COUNT_2[2] = { PIP_TL, PIP_BR };
constexpr int COUNT_3[3] = { PIP_TL, PIP_BR, PIP_CENTRE };
constexpr int COUNT_4[4] = { PIP_TL, PIP_BR, PIP_TR, PIP_BL };
constexpr int COUNT_5[5] = { PIP_TL, PIP_BR, PIP_CENTRE, PIP_TR, PIP_BL };
constexpr int COUNT_6[6] = { PIP_TL, PIP_BR, PIP_TR, PIP_BL, PIP_LM, PIP_RM };

// Builds the pip centres for one half of a tile in the order they appear and vanish:
// outermost first, so a tile hollows out from its edges instead of at random.
//
// u runs along the long axis of the tile and v across it: for a standing tile the long
// axis is y, for a tile on its side it is x.
int HalfPips(Rectangle half, bool longAxisIsY, int count, Vector2 out[6])
{
    if (count <= 0) return 0;
    if (count > 6)  count = 6;

    int chosen[6];
    int n = 0;
    switch (count)
    {
        case 1: for (int i = 0; i < 1; i++) chosen[n++] = COUNT_1[i]; break;
        case 2: for (int i = 0; i < 2; i++) chosen[n++] = COUNT_2[i]; break;
        case 3: for (int i = 0; i < 3; i++) chosen[n++] = COUNT_3[i]; break;
        case 4: for (int i = 0; i < 4; i++) chosen[n++] = COUNT_4[i]; break;
        case 5: for (int i = 0; i < 5; i++) chosen[n++] = COUNT_5[i]; break;
        default: for (int i = 0; i < 6; i++) chosen[n++] = COUNT_6[i]; break;
    }

    std::sort(chosen, chosen + n, [](int a, int b)
    {
        return PIP_SPOTS[a].priority < PIP_SPOTS[b].priority;
    });

    for (int i = 0; i < n; i++)
    {
        const PipSpot& p = PIP_SPOTS[chosen[i]];
        const float u = p.u * (longAxisIsY ? half.height : half.width);
        const float v = p.v * (longAxisIsY ? half.width  : half.height);
        out[i] = { half.x + (longAxisIsY ? v : u), half.y + (longAxisIsY ? u : v) };
    }

    return n;
}

// Renders a tile: outline, dividing line, and however many pips are still standing.
void DrawTile(const Domino& d, float alpha)
{
    if (alpha <= 0.001f) return;

    const Rectangle r = TileRect(d);

    if (d.gone)
    {
        // What is left of an erased tile is only the memory of its outline.
        const Color ghost = ColorAlpha(COL_TEXT_GHOST, alpha * 0.55f);

        // Dashed so it reads as a trace rather than a wall.
        const float dash = 7.0f;
        const float gap  = 6.0f;
        for (float x = r.x; x < r.x + r.width; x += dash + gap)
        {
            DrawRectangleRec({ x, r.y, std::min(dash, r.x + r.width - x), 1.0f }, ghost);
        }
        for (float y = r.y; y < r.y + r.height; y += dash + gap)
        {
            DrawRectangleRec({ r.x, y, 1.0f, std::min(dash, r.y + r.height - y) }, ghost);
        }
        return;
    }

    const float base      = d.keystone ? 0.85f : 0.55f;
    const float flash     = d.hitFlash;
    const int   grey      = (int)(40.0f + 60.0f * base + 120.0f * flash);
    const Color edge      = ColorAlpha({ (unsigned char)std::min(grey + 40, 255),
                                         (unsigned char)std::min(grey + 40, 255),
                                         (unsigned char)std::min(grey + 40, 255), 255 }, alpha);
    const Color fill      = ColorAlpha({ (unsigned char)grey, (unsigned char)grey,
                                         (unsigned char)grey, 255 }, alpha * 0.30f);
    const Color divider   = ColorAlpha(COL_EDGE, alpha * 0.85f);
    const Color pipCol    = ColorAlpha(d.keystone ? COL_TEXT : COL_TEXT_DIM, alpha);

    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1.0f, edge);

    // The two halves, with the divider between them.
    const bool longIsY = !d.horizontal;
    Rectangle halfA, halfB;
    if (longIsY)
    {
        const float h = r.height * 0.5f;
        halfA = { r.x, r.y, r.width, h };
        halfB = { r.x, r.y + h, r.width, h };
        DrawLine((int)r.x + 4, (int)(r.y + h), (int)(r.x + r.width) - 4, (int)(r.y + h), divider);
    }
    else
    {
        const float w = r.width * 0.5f;
        halfA = { r.x, r.y, w, r.height };
        halfB = { r.x + w, r.y, w, r.height };
        DrawLine((int)(r.x + w), (int)r.y + 4, (int)(r.x + w), (int)(r.y + r.height) - 4, divider);
    }

    // Interleave the two halves so a tile empties evenly instead of one half
    // disappearing first. Only the first pipsLeft pips of the combined list are drawn.
    Vector2 listA[6], listB[6];
    const int nA = HalfPips(halfA, longIsY, d.valueA, listA);
    const int nB = HalfPips(halfB, longIsY, d.valueB, listB);

    const float inset  = 5.0f;
    const float radius = 3.0f;

    int drawn = 0;
    for (int i = 0; i < nA + nB && drawn < d.pipsLeft; i++)
    {
        const bool takeA = (i % 2 == 0);        // A, B, A, B...
        const int  idx   = i / 2;

        Vector2 p;
        if (takeA)
        {
            if (idx >= nA) break;
            p = listA[idx];
        }
        else
        {
            if (idx >= nB) break;
            p = listB[idx];
        }

        p.x = std::clamp(p.x, r.x + inset, r.x + r.width - inset);
        p.y = std::clamp(p.y, r.y + inset, r.y + r.height - inset);

        DrawCircleV(p, radius, pipCol);
        drawn++;
    }
}

//------------------------------------------------------------------------------------
// The exit. A doorway that stays shut until enough of the arrangement is gone.
//------------------------------------------------------------------------------------
void DrawDoorShape(const Game& g, float alpha)
{
    if (alpha <= 0.001f) return;

    const Rectangle door = { DOOR_X, DOOR_Y, DOOR_W, DOOR_H };
    const bool open = g.board.sealOpen;

    const float pulse = 0.5f + 0.5f * std::sin(g.board.doorPulse * 1.6f);
    const int   grey  = open ? (int)(150 + 70 * pulse) : 62;
    const Color edge  = ColorAlpha({ (unsigned char)grey, (unsigned char)grey,
                                     (unsigned char)grey, 255 }, alpha);
    const Color inner = ColorAlpha(COL_TEXT_GHOST, alpha * (open ? 0.35f + 0.35f * pulse : 0.5f));

    DrawRectangleRec(door, inner);
    DrawRectangleLinesEx(door, 1.0f, edge);

    if (!open)
    {
        // Seal marks: one small square per erasure still required. Diegetic progress,
        // no HUD panel anywhere.
        const int need = g.board.sealNeed;
        for (int i = 0; i < need; i++)
        {
            const float x = door.x + door.width * 0.5f - (need * 9.0f) * 0.5f + i * 9.0f + 3.0f;
            const float y = door.y + door.height - 12.0f;
            const bool  filled = i < g.board.emptied;
            DrawRectangleLinesEx({ x, y, 6.0f, 6.0f }, 1.0f,
                                 ColorAlpha(filled ? COL_TEXT_DIM : COL_EDGE, alpha));
        }
    }
}

//------------------------------------------------------------------------------------
// Small helpers for the full-screen moments
//------------------------------------------------------------------------------------
void DrawCenteredLine(const Game& g, const char* text, float y, float size, float spacing,
                      Color color)
{
    const float w = SpacedWidth(g.font, text, size, spacing);
    SpacedDraw(g.font, text, (SCREEN_W - w) * 0.5f, y, size, spacing, color);
}

// Fade a line in and out inside its own window.
float BeatAlpha(float t, float at, float dur)
{
    if (t < at || t > at + dur) return 0.0f;

    const float fadeIn  = 0.55f;
    const float fadeOut = 0.55f;
    if (t < at + fadeIn) return SmoothStep01((t - at) / fadeIn);
    if (t > at + dur - fadeOut) return SmoothStep01((at + dur - t) / fadeOut);
    return 1.0f;
}

} // namespace

//------------------------------------------------------------------------------------
// Text helpers exposed to the UI layer
//------------------------------------------------------------------------------------
float SpacedTextWidth(const Font& font, const char* text, float size, float spacing)
{
    return SpacedWidth(font, text, size, spacing);
}

void DrawSpaced(const Font& font, const char* text, float x, float y, float size,
                float spacing, Color color)
{
    SpacedDraw(font, text, x, y, size, spacing, color);
}

void DrawSpacedCentered(const Font& font, const char* text, float centerX, float y,
                        float size, float spacing, Color color)
{
    const float w = SpacedWidth(font, text, size, spacing);
    SpacedDraw(font, text, centerX - w * 0.5f, y, size, spacing, color);
}

//------------------------------------------------------------------------------------
// Scene pieces
//------------------------------------------------------------------------------------
void DrawBoard(const Game& g, float alpha)
{
    // The arrangement fades up in a stagger, so the world assembles itself out of the dark.
    for (int i = 0; i < MAX_TILES; i++)
    {
        const float local = std::clamp(g.boardReveal * 1.35f - i * 0.035f, 0.0f, 1.0f);
        DrawTile(g.board.tiles[i], AlphaOf(alpha * local));
    }

    DrawDoorShape(g, alpha);
}

void DrawTrail(const Trail& t, float memoryScale, float now)
{
    const float life      = t.life * memoryScale;
    const float ghostLife = t.ghostLife;

    for (int i = 0; i < t.count; i++)
    {
        // Walk from oldest to newest.
        const int index = (t.head - t.count + i + TRAIL_CAP * 2) % TRAIL_CAP;
        const TrailSample& s = t.samples[index];

        const float age = now - s.born;
        if (age < 0.0f) continue;

        float a;
        if (s.ghost)
        {
            if (age > ghostLife) continue;
            a = (1.0f - age / ghostLife) * t.ghostGain;
        }
        else
        {
            if (age > life) continue;
            a = (1.0f - age / life) * 0.55f;
            a *= std::min(1.0f, age / 0.12f);   // let a dot fade in instead of popping
        }

        if (a <= 0.004f) continue;

        // Older dots are also smaller, which reads as depth rather than just dimness.
        const float size = 1.1f + a * 1.1f;
        DrawCircleV(s.pos, size, ColorAlpha(COL_TEXT, AlphaOf(a)));
    }
}

void DrawBallShape(const Ball& b, float alpha)
{
    if (alpha <= 0.001f) return;

    const float speed = std::sqrt(b.vel.x * b.vel.x + b.vel.y * b.vel.y);
    const float t     = std::clamp(speed / BALL_MAX_SPEED, 0.0f, 1.0f);

    // Stretch along travel, squash along the impact axis, blend between the two.
    const float travelAngle = speed > 12.0f ? std::atan2(b.vel.y, b.vel.x) : b.impactAngle;
    const float angle = b.squash > 0.01f
                      ? travelAngle + (b.impactAngle - travelAngle) * std::min(1.0f, b.squash)
                      : travelAngle;

    float rx = b.radius * (1.0f + 0.30f * t) * (1.0f - 0.30f * b.squash);
    float ry = b.radius * (1.0f - 0.24f * t) * (1.0f + 0.42f * b.squash);

    // A faint halo keeps the ball findable on a black field without lighting the void.
    DrawCircleV(b.pos, b.radius * 2.4f, ColorAlpha(COL_TEXT, alpha * 0.045f));

    rlPushMatrix();
    rlTranslatef(b.pos.x, b.pos.y, 0.0f);
    rlRotatef(angle * 60.0f, 0.0f, 0.0f, 1.0f);   // rlRotatef works in degrees
    DrawEllipse(0, 0, rx, ry, ColorAlpha(COL_TEXT, alpha));
    rlPopMatrix();
}

void DrawDoor(const Game& g, float alpha)
{
    DrawDoorShape(g, alpha);
}

void DrawStoryLine(const Game& g)
{
    if (g.story.line.empty() || g.story.hold <= 0.0f) return;

    // Fade out over the last second so lines leave rather than blink out.
    const float a = std::min(1.0f, g.story.hold / 1.0f);
    DrawCenteredLine(g, g.story.line.c_str(), SCREEN_H - 96.0f, 22.0f, 4.0f,
                     ColorAlpha(COL_TEXT_DIM, a * 0.9f));
}

void DrawHints(const Game& g)
{
    // Controls stay readable but never shout: strong for the first seconds of an
    // attempt, then barely there.
    const float fade = g.stageTime < 6.0f ? 1.0f : 0.35f;
    const Color col  = ColorAlpha(COL_TEXT_FAINT, 0.85f * fade);

    DrawSpaced(g.font, "WASD / ARROWS - MOVE", 34.0f, (float)SCREEN_H - 44.0f, 15.0f, 2.0f, col);
    DrawSpaced(g.font, "R - BEGIN AGAIN", 34.0f, (float)SCREEN_H - 66.0f, 15.0f, 2.0f, col);
    DrawSpaced(g.font, "ESC - MENU", 34.0f, 34.0f, 15.0f, 2.0f, col);
}

void DrawFade(const Game& g)
{
    if (g.transition.t <= 0.001f) return;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, ColorAlpha(COL_BG_DEEP, g.transition.t));
}

//------------------------------------------------------------------------------------
// Full screens
//------------------------------------------------------------------------------------
void DrawIntro(const Game& g)
{
    const float t = g.sceneTime;

    DrawBoard(g, g.boardReveal);
    DrawBallShape(g.ball, g.ballReveal);

    struct Line { float at, dur; const char* text; float size, spacing; };
    static const Line LINES[] =
    {
        { 1.9f, 3.2f, GAME_TITLE, 54.0f, 8.0f },
        { 5.6f, 2.6f, "everything here is temporary.", 24.0f, 3.0f },
        { 8.3f, 2.4f, "some of it stays.", 24.0f, 3.0f },
        { 11.0f, 2.0f, "FIND THE EXIT.", 28.0f, 5.0f },
    };

    for (const Line& l : LINES)
    {
        const float a = BeatAlpha(t, l.at, l.dur);
        if (a <= 0.001f) continue;

        // The title sits high; the lines of tone sit on the centre line.
        const float y = (l.size > 40.0f) ? SCREEN_H * 0.5f - 130.0f : SCREEN_H * 0.5f - l.size * 0.5f;
        DrawCenteredLine(g, l.text, y, l.size, l.spacing, ColorAlpha(COL_TEXT, a * 0.95f));
    }

    DrawSpaced(g.font, "SKIP", (float)SCREEN_W - 96.0f, 30.0f, 14.0f, 2.0f,
               ColorAlpha(COL_TEXT_FAINT, 0.5f * std::min(1.0f, t / 2.0f)));
}

void DrawPlaying(const Game& g)
{
    DrawTrail(g.trail, g.settings.memory, g.clock);
    DrawBoard(g, 1.0f);
    DrawBallShape(g.ball, 1.0f - (g.ball.lost ? std::min(1.0f, g.ball.lostTimer * 2.2f) : 0.0f));

    DrawStoryLine(g);
    DrawHints(g);
}

void DrawClearBeat(const Game& g)
{
    const float a = std::min(1.0f, g.sceneTime * 1.6f) * std::min(1.0f, (1.9f - g.sceneTime) * 1.6f);

    DrawCenteredLine(g, ClearLine(g.board.stage), (SCREEN_H - 24.0f) * 0.5f, 26.0f, 5.0f,
                     ColorAlpha(COL_TEXT, a));

    // Stage counter, kept as small type in a corner rather than a HUD element.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d / %d", g.board.stage + 1, STAGE_COUNT);
    DrawSpaced(g.font, buf, (float)SCREEN_W - 92.0f, 30.0f, 15.0f, 2.0f,
               ColorAlpha(COL_TEXT_FAINT, a * 0.8f));
}

void DrawEndingSequence(const Game& g)
{
    const float t = g.sceneTime;

    // The arrangement goes out from the outside in, leaving nothing behind.
    const float fade = std::max(0.0f, 1.0f - t / 1.4f);
    DrawBoard(g, fade);
    DrawBallShape(g.ball, std::max(0.0f, 1.0f - t / 0.9f));

    // One line at a time: each one replaces the last rather than stacking on top of it.
    struct Line { float at, dur; const char* text; };
    static const Line LINES[] =
    {
        { 1.8f, 2.3f, "YOU ALREADY WON THIS GAME." },
        { 4.4f, 2.3f, "STOP TRYING TO RESTORE IT." },
        { 7.0f, 2.0f, "WHAT DID YOU LEAVE BEHIND?" },
    };

    for (const Line& l : LINES)
    {
        const float a = BeatAlpha(t, l.at, l.dur);
        if (a <= 0.001f) continue;

        DrawCenteredLine(g, l.text, (SCREEN_H - 24.0f) * 0.5f, 24.0f, 5.0f,
                         ColorAlpha(COL_TEXT, a * 0.95f));
    }
}

void (*FrameDrawn)() = nullptr;

void DrawFrame(Game& g)
{
    BeginDrawing();
        ClearBackground(COL_BG);
        if (g.settings.showBg) DrawBackground(g);

        switch (g.screen)
        {
            case SCREEN_INTRO:    DrawIntro(g);          break;
            case SCREEN_PLAYING:  DrawPlaying(g);        break;
            case SCREEN_CLEAR:    DrawClearBeat(g);      break;
            case SCREEN_ENDING:   DrawEndingSequence(g); break;
            default:              DrawScreenUi(g);       break;   // menu/pause/settings/credits
        }

        DrawFade(g);

        if (FrameDrawn) FrameDrawn();
    EndDrawing();
}

} // namespace witness
