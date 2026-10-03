#include "game.h"
#include "ui.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace witness {

namespace {

Layout g_layout;

float Smooth(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float BeatAlpha(float t, float at, float dur)
{
    if (t < at || t > at + dur) return 0.0f;
    if (t < at + 0.55f) return Smooth((t - at) / 0.55f);
    if (t > at + dur - 0.55f) return Smooth((at + dur - t) / 0.55f);
    return 1.0f;
}

struct IntroLine { float at, dur; const char* text; float size, spacing; bool heading; };

constexpr IntroLine INTRO_LINES[] =
{
    {  1.9f, 3.2f, GAME_TITLE,                      54.0f, 6.0f, true  },
    {  5.6f, 2.6f, "everything here is temporary.",  22.0f, 3.0f, false },
    {  8.3f, 2.4f, "some of it stays.",              22.0f, 3.0f, false },
    { 11.0f, 2.0f, "Find the Exit.",                26.0f, 4.0f, true  },
};

struct EndingLine { float at, dur; const char* text; };

constexpr EndingLine ENDING_LINES[] =
{
    { 1.8f, 2.3f, "You Already Won This Game." },
    { 4.4f, 2.3f, "Stop Trying to Restore It." },
    { 7.0f, 2.0f, "What Did You Leave Behind?" },
};

template <class T, int N>
float Brightness(float t, const T (&lines)[N])
{
    float best = 0.0f;
    for (int i = 0; i < N; i++) best = std::max(best, BeatAlpha(t, lines[i].at, lines[i].dur));
    return best;
}

float SpacedWidth(const Font& font, const char* text, float size, float spacing)
{
    float w = 0.0f;
    for (int i = 0; text && text[i]; i++)
    {
        const char one[2] = { text[i], 0 };
        w += MeasureTextEx(font, one, size, 0.0f).x + (text[i + 1] ? spacing : 0.0f);
    }
    return w;
}

void SpacedDraw(const Font& font, const char* text, float x, float y, float size, float spacing,
                Color color)
{
    const float bold = std::max(1.0f, size * 0.045f);
    for (int i = 0; text && text[i]; i++)
    {
        const char one[2] = { text[i], 0 };
        DrawTextEx(font, one, { x, y }, size, 0.0f, color);
        DrawTextEx(font, one, { x + bold, y }, size, 0.0f, color);
        x += MeasureTextEx(font, one, size, 0.0f).x + spacing;
    }
}

void Centered(const Font& font, const char* text, float cx, float y, float size, float spacing,
              Color color)
{
    SpacedDraw(font, text, cx - SpacedWidth(font, text, size, spacing) * 0.5f, y, size, spacing,
               color);
}

struct Pip { float u, v; };

constexpr Pip PIPS[7] =
{
    { 0.26f, 0.22f }, { 0.74f, 0.78f }, { 0.74f, 0.22f },
    { 0.26f, 0.78f }, { 0.24f, 0.50f }, { 0.76f, 0.50f }, { 0.50f, 0.50f }
};

constexpr int PICK[7][6] =
{
    { 6, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 0, 0, 0 },
    { 0, 1, 6, 0, 0, 0 },
    { 0, 1, 2, 3, 0, 0 },
    { 0, 1, 6, 2, 3, 0 },
    { 0, 1, 2, 3, 4, 5 }
};

constexpr int PICK_N[7] = { 1, 2, 3, 4, 5, 6 };

int HalfPips(Rectangle half, bool longIsY, int count, Vector2 out[6])
{
    if (count <= 0) return 0;
    count = count > 6 ? 6 : count;

    const int* src = PICK[count];
    for (int i = 0; i < PICK_N[count]; i++)
    {
        const Pip& p = PIPS[src[i]];
        const float u = p.u * (longIsY ? half.height : half.width);
        const float v = p.v * (longIsY ? half.width  : half.height);
        out[i] = { half.x + (longIsY ? v : u), half.y + (longIsY ? u : v) };
    }
    return PICK_N[count];
}

void DrawTile(const Domino& d, float alpha)
{
    if (alpha <= 0.001f) return;

    const Rectangle r = TileRect(d);

    if (d.gone)
    {
        const Color ghost = ColorAlpha(COL_TEXT_GHOST, alpha * 0.22f);
        for (float x = r.x; x < r.x + r.width; x += 13.0f)
            DrawRectangleRec({ x, r.y, std::min(7.0f, r.x + r.width - x), 1.0f }, ghost);
        for (float y = r.y; y < r.y + r.height; y += 13.0f)
            DrawRectangleRec({ r.x, y, 1.0f, std::min(7.0f, r.y + r.height - y) }, ghost);
        return;
    }

    const int grey = (int)(196.0f - 90.0f * (d.keystone ? 0.85f : 0.55f) - 150.0f * d.hitFlash);
    const unsigned char v = (unsigned char)std::clamp(grey, 12, 255);
    const Color edge = ColorAlpha({ v, v, v, 255 }, alpha);
    const Color fill = ColorAlpha({ v, v, v, 255 }, alpha * 0.22f);

    DrawRectangleRounded(r, CORNER_R, 4, fill);
    DrawRectangleRoundedLinesEx(r, CORNER_R, 4, 1.0f, edge);

    const bool longIsY = !d.horizontal;
    Rectangle halfA, halfB;
    if (longIsY)
    {
        halfA = { r.x, r.y, r.width, r.height * 0.5f };
        halfB = { r.x, r.y + r.height * 0.5f, r.width, r.height * 0.5f };
        DrawLine((int)r.x + 4, (int)(r.y + r.height * 0.5f),
                 (int)(r.x + r.width) - 4, (int)(r.y + r.height * 0.5f), edge);
    }
    else
    {
        halfA = { r.x, r.y, r.width * 0.5f, r.height };
        halfB = { r.x + r.width * 0.5f, r.y, r.width * 0.5f, r.height };
        DrawLine((int)(r.x + r.width * 0.5f), (int)r.y + 4,
                 (int)(r.x + r.width * 0.5f), (int)(r.y + r.height) - 4, edge);
    }

    Vector2 listA[6], listB[6];
    const int nA = HalfPips(halfA, longIsY, d.valueA, listA);
    const int nB = HalfPips(halfB, longIsY, d.valueB, listB);

    int drawn = 0;
    for (int i = 0; i < nA + nB && drawn < d.pipsLeft; i++)
    {
        const bool takeA = (i % 2) == 0;
        const int idx = i / 2;
        Vector2 p;
        if (takeA) { if (idx >= nA) break; p = listA[idx]; }
        else       { if (idx >= nB) break; p = listB[idx]; }

        p.x = std::clamp(p.x, r.x + 5.0f, r.x + r.width - 5.0f);
        p.y = std::clamp(p.y, r.y + 5.0f, r.y + r.height - 5.0f);
        DrawCircleV(p, 3.0f, ColorAlpha(d.keystone ? COL_TEXT : COL_TEXT_DIM, alpha));
        drawn++;
    }
}

void DrawDoorShape(const Game& g, float alpha)
{
    if (alpha <= 0.001f) return;

    const Rectangle door = { DOOR_X, DOOR_Y, DOOR_W, DOOR_H };
    const bool open = g.board.sealOpen;
    const float pulse = 0.5f + 0.5f * std::sin(g.board.doorPulse * 1.6f);
    const int grey = open ? (int)(60 - 55 * pulse) : 193;

    const Color face = ColorAlpha({ (unsigned char)grey, (unsigned char)grey,
                                    (unsigned char)grey, 255 },
                                  alpha * (open ? 0.30f : 0.45f));
    DrawRectangleRounded(door, CORNER_R, 4, face);
    DrawRectangleRoundedLinesEx(door, CORNER_R, 4, 1.0f, ColorAlpha(face, 1.0f));

    if (!open)
        for (int i = 0; i < g.board.sealNeed; i++)
        {
            const float x = door.x + door.width * 0.5f - g.board.sealNeed * 4.5f + i * 9.0f;
            DrawRectangleLinesEx({ x, door.y + door.height - 13.0f, 6.0f, 6.0f }, 1.0f,
                                 ColorAlpha(i < g.board.emptied ? COL_TEXT : COL_EDGE, alpha));
        }
}


}

Layout ComputeLayout()
{
    return ComputeLayoutFor((float)std::max(1, GetScreenWidth()),
                            (float)std::max(1, GetScreenHeight()));
}

const Layout& CurrentLayout() { return g_layout; }

constexpr int BLOB_POINTS = 26;

void DrawBallShape(const Game& g, float alpha)
{
    if (alpha <= 0.001f) return;

    const Ball& b = g.ball;
    const float speed = VLen(b.vel);
    const float t = std::clamp(speed / BALL_MAX_SPEED, 0.0f, 1.0f);
    const float sq = std::clamp(b.squash, 0.0f, BALL_SQUASH_MAX);

    const float rx = b.radius * (1.0f + 0.26f * t) * (1.0f - 0.26f * sq);
    const float ry = b.radius * (1.0f - 0.20f * t) * (1.0f + 0.34f * sq);

    const float angle = speed > 12.0f ? std::atan2(b.vel.y, b.vel.x) : b.impactAngle;
    const float wobble = 0.5f + 0.5f * std::sin(g.clock * 1.7f);

    rlPushMatrix();
    rlTranslatef(b.pos.x, b.pos.y, 0.0f);
    rlRotatef(angle * 60.0f, 0.0f, 0.0f, 1.0f);

    const Color fill = ColorAlpha(COL_TEXT, alpha);
    rlBegin(RL_TRIANGLES);
        rlColor4ub(fill.r, fill.g, fill.b, fill.a);
        rlVertex2f(0.0f, 0.0f);
        for (int i = 0; i <= BLOB_POINTS; i++)
        {
            const float a = (float)i / (float)BLOB_POINTS * 2.0f * PI;
            const float lobe = 1.0f
                             + 0.070f * std::sin(2.0f * a + 0.9f + wobble * 0.6f)
                             + 0.042f * std::sin(3.0f * a - 0.4f)
                             + 0.022f * std::sin(5.0f * a + 1.7f);
            rlVertex2f(std::cos(a) * rx * lobe, std::sin(a) * ry * lobe);
        }
    rlEnd();
    rlPopMatrix();
}


void BeginWorldView(const Layout& l)
{
    const float ox = (l.screenW * 0.5f - l.viewX) / l.scale;
    const float oy = (l.screenH * 0.5f - l.viewY) / l.scale;
    rlPushMatrix();
    rlTranslatef(l.screenW * 0.5f - l.scale * ox, l.screenH * 0.5f - l.scale * oy, 0.0f);
    rlScalef(l.scale, l.scale, 1.0f);
}

void EndWorldView() { rlPopMatrix(); }

float SpacedTextWidth(const Font& font, const char* text, float size, float spacing)
{
    return SpacedWidth(font, text, size, spacing);
}

void DrawSpaced(const Font& font, const char* text, float x, float y, float size, float spacing,
                Color color)
{
    SpacedDraw(font, text, x, y, size, spacing, color);
}

void DrawSpacedCentered(const Font& font, const char* text, float cx, float y, float size,
                        float spacing, Color color)
{
    Centered(font, text, cx, y, size, spacing, color);
}

void DrawBoard(const Game& g, float alpha)
{
    for (int i = 0; i < MAX_TILES; i++)
    {
        const float local = std::clamp(g.boardReveal * 1.35f - i * 0.035f, 0.0f, 1.0f);
        DrawTile(g.board.tiles[i], std::clamp(alpha * local, 0.0f, 1.0f));
    }
    DrawDoorShape(g, alpha);
}

void DrawTrail(const Trail& t, float memoryScale, float now)
{
    const float life = t.life * memoryScale;

    for (int i = 0; i < t.count; i++)
    {
        const TrailSample& s = t.samples[(t.head - t.count + i + TRAIL_CAP * 2) % TRAIL_CAP];
        const float age = now - s.born;
        if (age < 0.0f) continue;

        float a;
        if (s.ghost)
        {
            if (age > t.ghostLife) continue;
            a = (1.0f - age / t.ghostLife) * t.ghostGain;
        }
        else
        {
            if (age > life) continue;
            a = (1.0f - age / life) * 0.55f * std::min(1.0f, age / 0.12f);
        }

        if (a > 0.004f) DrawCircleV(s.pos, 1.1f + a * 1.1f, ColorAlpha(COL_TEXT, a));
    }
}

void DrawDoor(const Game& g, float alpha) { DrawDoorShape(g, alpha); }

void DrawStoryLine(const Game& g)
{
    if (g.story.line.empty() || g.story.hold <= 0.0f) return;

    const Layout& l = CurrentLayout();
    const float a = std::min(1.0f, g.story.hold);
    DrawSpacedCentered(g.font, g.story.line.c_str(), l.screenW * 0.5f, l.storyY, l.storySize,
                       l.storySpacing, ColorAlpha(COL_TEXT_DIM, a));
}

void DrawHints(const Game& g)
{
    const Layout& l = CurrentLayout();
    const float fade = g.stageTime < 6.0f ? 1.0f : 0.90f;
    const Color col = ColorAlpha(COL_TEXT_FAINT, fade);

    DrawSpaced(g.font, "WASD / Arrows - Move", l.margin, l.controlRowY, l.hintSize,
               l.hintSpacing, col);
    DrawSpaced(g.font, "R - Begin Again", l.margin, l.controlRow2Y, l.hintSize, l.hintSpacing,
               col);
    DrawSpaced(g.font, "Esc - Menu", l.margin, l.topRowY, l.hintSize, l.hintSpacing, col);
}

void DrawFade(const Game& g)
{
    if (g.transition.t <= 0.001f) return;
    const Layout& l = CurrentLayout();
    DrawRectangle(0, 0, (int)l.screenW, (int)l.screenH,
                  ColorAlpha(COL_BG_DEEP, g.transition.t));
}

void DrawIntro(const Game& g)
{
    const Layout& L = CurrentLayout();

    DrawBoard(g, g.boardReveal * (1.0f - 0.70f * Brightness(g.sceneTime, INTRO_LINES)));
    DrawBallShape(g, g.ballReveal);

    for (const IntroLine& l : INTRO_LINES)
    {
        const float a = BeatAlpha(g.sceneTime, l.at, l.dur);
        if (a <= 0.001f) continue;

        const float size = l.size * L.scale;
        const float y = L.viewY + L.viewH * 0.5f - size * 0.5f -
                        (l.heading ? 40.0f * L.scale : 0.0f);
        Centered(l.heading ? g.titleFont : g.font, l.text, L.viewX + L.viewW * 0.5f, y, size,
                l.spacing * L.scale, ColorAlpha(COL_TEXT, a * 0.95f));
    }

    const float w = SpacedWidth(g.font, "Skip", L.hintSize, L.hintSpacing);
    DrawSpaced(g.font, "Skip", L.screenW - L.margin - w, L.topRowY, L.hintSize, L.hintSpacing,
               ColorAlpha(COL_TEXT_FAINT, 0.9f * std::min(1.0f, g.sceneTime / 2.0f)));
}

void DrawPlaying(const Game& g)
{
    DrawTrail(g.trail, g.settings.memory, g.clock);
    DrawBoard(g, 1.0f);
    DrawBallShape(g, 1.0f - (g.ball.lost ? std::min(1.0f, g.ball.lostTimer * 2.2f) : 0.0f));
}

void DrawClearBeat(const Game& g)
{
    const Layout& L = CurrentLayout();
    const float a = std::min(1.0f, g.sceneTime * 1.6f) *
                    std::min(1.0f, (1.9f - g.sceneTime) * 1.6f);

    Centered(g.titleFont, ClearLine(g.board.stage), L.viewX + L.viewW * 0.5f,
             L.viewY + L.viewH * 0.5f - 15.0f * L.scale, 30.0f * L.scale, 4.0f * L.scale,
             ColorAlpha(COL_TEXT, a));

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d / %d", g.board.stage + 1, STAGE_COUNT);
    const float w = SpacedWidth(g.font, buf, L.hintSize, L.hintSpacing);
    DrawSpaced(g.font, buf, L.screenW - L.margin - w, L.topRowY, L.hintSize, L.hintSpacing,
               ColorAlpha(COL_TEXT_FAINT, a));
}

void DrawEndingSequence(const Game& g)
{
    const Layout& L = CurrentLayout();
    const float fade = std::max(0.0f, 1.0f - g.sceneTime / 1.4f);

    DrawBoard(g, fade * (1.0f - 0.70f * Brightness(g.sceneTime, ENDING_LINES)));
    DrawBallShape(g, std::max(0.0f, 1.0f - g.sceneTime / 0.9f));

    for (const EndingLine& l : ENDING_LINES)
    {
        const float a = BeatAlpha(g.sceneTime, l.at, l.dur);
        if (a <= 0.001f) continue;

        Centered(g.font, l.text, L.viewX + L.viewW * 0.5f, L.viewY + L.viewH * 0.5f -
                12.0f * L.scale, 24.0f * L.scale, 5.0f * L.scale,
                ColorAlpha(COL_TEXT, a * 0.95f));
    }
}

void (*FrameDrawn)() = nullptr;

void DrawFrame(Game& g)
{
    g_layout = ComputeLayout();

    BeginDrawing();
        ClearBackground(COL_BG);

        if (g.screen == SCREEN_MENU && g.bgLoaded)
            DrawTextureRec(g.bg, { 0.0f, 0.0f, g_layout.screenW, g_layout.screenH },
                           { 0.0f, 0.0f }, WHITE);

        BeginWorldView(g_layout);
            switch (g.screen)
            {
                case SCREEN_INTRO:   DrawIntro(g);          break;
                case SCREEN_PLAYING: DrawPlaying(g);        break;
                case SCREEN_CLEAR:   DrawClearBeat(g);      break;
                case SCREEN_ENDING:  DrawEndingSequence(g); break;
                default: break;
            }
        EndWorldView();

        if (g.screen == SCREEN_PLAYING)
        {
            DrawStoryLine(g);
            DrawHints(g);
            DrawHudIcons(g);
        }

        if (g.screen == SCREEN_MENU || g.screen == SCREEN_PAUSE ||
            g.screen == SCREEN_SETTINGS || g.screen == SCREEN_CREDITS)
        {
            DrawScreenUi(g);
        }

        DrawFade(g);

        if (FrameDrawn) FrameDrawn();
    EndDrawing();
}

}