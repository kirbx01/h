#include "game.h"
#include "ui.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace witness {

namespace {

Layout g_layout;

constexpr float KEYCAP_PAD_X = 0.42f;
constexpr float KEYCAP_PAD_Y = 0.28f;
constexpr Color HUD_VIOLET = { 0x55, 0x40, 0x78, 255 };

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

constexpr float PIP_GX[3] = { 0.28f, 0.50f, 0.72f };
constexpr float PIP_GY[3] = { 0.27f, 0.50f, 0.73f };

constexpr int PIP_CELL[7][6] =
{
    { -1, -1, -1, -1, -1, -1 },
    {  4, -1, -1, -1, -1, -1 },
    {  0,  8, -1, -1, -1, -1 },
    {  0,  4,  8, -1, -1, -1 },
    {  0,  2,  6,  8, -1, -1 },
    {  0,  2,  4,  6,  8, -1 },
    {  0,  2,  3,  5,  6,  8 },
};

constexpr int PIP_COUNT[7] = { 0, 1, 2, 3, 4, 5, 6 };

void DrawHalfPips(Rectangle half, int value, Color ink)
{
    const int v = std::clamp(value, 0, 6);
    const float r = std::min(half.width, half.height) * 0.085f;

    for (int i = 0; i < PIP_COUNT[v]; i++)
    {
        const int cell = PIP_CELL[v][i];
        const float u = PIP_GX[cell % 3];
        const float w = PIP_GY[cell / 3];
        DrawCircleV({ half.x + u * half.width, half.y + w * half.height }, r, ink);
    }
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

    const float s = std::max(0.05f, CurrentLayout().scale);
    const float corner = std::min({ DOMINO_CORNER / s, r.width * 0.5f, r.height * 0.5f });
    const float bar    = std::max(1.0f / s, 1.0f);

    const Color edge = ColorAlpha(COL_TEXT, alpha);
    const Color fill = ColorAlpha(COL_TEXT, alpha * (0.06f + 0.34f * d.hitFlash));

    DrawRectangleRounded(r, corner, 6, fill);
    DrawRectangleRoundedLinesEx(r, corner, 6, bar, edge);

    const bool longIsY = !d.horizontal;
    const Rectangle halfA = longIsY ? Rectangle{ r.x, r.y, r.width, r.height * 0.5f }
                                     : Rectangle{ r.x, r.y, r.width * 0.5f, r.height };
    const Rectangle halfB = longIsY ? Rectangle{ r.x, r.y + r.height * 0.5f, r.width, r.height * 0.5f }
                                     : Rectangle{ r.x + r.width * 0.5f, r.y, r.width * 0.5f, r.height };

    if (longIsY)
        DrawRectangleRec({ r.x + r.width * 0.30f, r.y + r.height * 0.5f - bar * 0.5f,
                           r.width * 0.40f, bar }, edge);
    else
        DrawRectangleRec({ r.x + r.width * 0.5f - bar * 0.5f, r.y + r.height * 0.30f,
                           bar, r.height * 0.40f }, edge);

    DrawHalfPips(halfA, d.valueA, edge);
    DrawHalfPips(halfB, d.valueB, edge);
}

void DrawDoorShape(const Game& g, float alpha)
{
    if (alpha <= 0.001f) return;

    const Rectangle door = { DOOR_X, DOOR_Y, DOOR_W, DOOR_H };
    const Rectangle inner = { door.x + 7.0f, door.y + 7.0f,
                              door.width - 14.0f, door.height - 14.0f };
    const float progress = std::clamp((float)g.board.emptied /
                                      (float)std::max(1, g.board.sealNeed), 0.0f, 1.0f);
    const float fillHeight = inner.height * progress;
    if (fillHeight > 0.0f)
        DrawRectangleRec({ inner.x, inner.y + inner.height - fillHeight,
                           inner.width, fillHeight }, ColorAlpha(HUD_VIOLET, alpha));

    const Color outline = ColorAlpha(COL_TEXT, alpha);
    DrawRectangleLinesEx(door, 1.0f, outline);
    DrawRectangleLinesEx(inner, 1.0f, outline);
}


}

Layout ComputeLayout()
{
    return ComputeLayoutFor((float)std::max(1, GetScreenWidth()),
                            (float)std::max(1, GetScreenHeight()));
}

const Layout& CurrentLayout() { return g_layout; }

constexpr int BLOB_POINTS = 26;

float BlobLobe(float a, float phase)
{
    return 1.0f + 0.070f * std::sin(2.0f * a + 0.9f + phase)
                + 0.042f * std::sin(3.0f * a - 0.4f)
                + 0.022f * std::sin(5.0f * a + 1.7f);
}

void BlobFan(float rx, float ry, float phase, Color fill)
{
    rlSetTexture(0);
    rlBegin(RL_QUADS);
    for (int i = 0; i < BLOB_POINTS; i++)
    {
        const float t0 = (float)i / (float)BLOB_POINTS * 2.0f * PI;
        const float t1 = (float)(i + 1) / (float)BLOB_POINTS * 2.0f * PI;
        const float l0 = BlobLobe(t0, phase);
        const float l1 = BlobLobe(t1, phase);

        const float x0 = std::cos(t0) * rx * l0;
        const float y0 = std::sin(t0) * ry * l0;
        const float x1 = std::cos(t1) * rx * l1;
        const float y1 = std::sin(t1) * ry * l1;

        rlColor4ub(fill.r, fill.g, fill.b, fill.a);
        rlVertex2f(0.0f, 0.0f);
        rlVertex2f(x1, y1);
        rlVertex2f(x0, y0);
        rlVertex2f(x0, y0);
    }
    rlEnd();
}

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
    const float phase = 0.9f + (0.5f + 0.5f * std::sin(g.clock * 1.7f)) * 0.6f;

    const float ring = std::max(1.0f, 2.0f / std::max(0.05f, CurrentLayout().scale));

    rlPushMatrix();
    rlTranslatef(b.pos.x, b.pos.y, 0.0f);
    rlRotatef(angle * 60.0f, 0.0f, 0.0f, 1.0f);
        BlobFan(rx + ring, ry + ring, phase, ColorAlpha({   0,   0,   0, 255 }, alpha));
        BlobFan(rx,          ry,          phase, ColorAlpha({ 255, 255, 255, 255 }, alpha));
    rlPopMatrix();
}


void BeginWorldView(const Layout& l)
{
    rlPushMatrix();
    rlTranslatef(l.viewX, l.viewY, 0.0f);
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
    const auto sampleAt = [&](int index) -> const TrailSample&
    {
        return t.samples[(t.head - t.count + index + TRAIL_CAP * 2) % TRAIL_CAP];
    };
    const auto alphaAt = [&](const TrailSample& sample)
    {
        const float age = now - sample.born;
        if (age < 0.0f) return 0.0f;
        if (sample.ghost)
        {
            if (age > t.ghostLife) return 0.0f;
            return (1.0f - age / t.ghostLife) * t.ghostGain;
        }
        if (age > life) return 0.0f;
        return (1.0f - age / life) * 0.55f * std::min(1.0f, age / 0.12f);
    };
    const auto curvePoint = [](Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float u)
    {
        const float u2 = u * u;
        const float u3 = u2 * u;
        return Vector2{
            0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * u +
                    (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * u2 +
                    (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * u3),
            0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * u +
                    (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * u2 +
                    (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * u3)
        };
    };

    constexpr int CURVE_STEPS = 4;
    for (int i = 0; i + 1 < t.count; i++)
    {
        const TrailSample& first = sampleAt(i);
        const TrailSample& second = sampleAt(i + 1);
        if (first.ghost != second.ghost) continue;

        const float firstAlpha = alphaAt(first);
        const float secondAlpha = alphaAt(second);
        if (firstAlpha <= 0.004f && secondAlpha <= 0.004f) continue;

        const Vector2 p0 = (i > 0 && sampleAt(i - 1).ghost == first.ghost &&
                            alphaAt(sampleAt(i - 1)) > 0.004f)
                               ? sampleAt(i - 1).pos : first.pos;
        const Vector2 p3 = (i + 2 < t.count && sampleAt(i + 2).ghost == first.ghost &&
                            alphaAt(sampleAt(i + 2)) > 0.004f)
                               ? sampleAt(i + 2).pos : second.pos;

        Vector2 previous = first.pos;
        float previousAlpha = firstAlpha;
        for (int step = 1; step <= CURVE_STEPS; step++)
        {
            const float u = (float)step / CURVE_STEPS;
            const Vector2 point = curvePoint(p0, first.pos, second.pos, p3, u);
            const float alpha = firstAlpha + (secondAlpha - firstAlpha) * u;
            const float sketch = 0.78f + 0.22f *
                                 std::sin((float)(i * CURVE_STEPS + step) * 2.17f);
            const float lineAlpha = (previousAlpha + alpha) * 0.5f * sketch;
            if (lineAlpha > 0.004f)
                DrawLineEx(previous, point, 0.75f + lineAlpha,
                           ColorAlpha(COL_TEXT, lineAlpha));
            previous = point;
            previousAlpha = alpha;
        }
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

float HintGroupWidth(const Font& font, const char* key, const char* label, float size, float spacing)
{
    const float capW = SpacedWidth(font, key, size, spacing) + size * KEYCAP_PAD_X * 2.0f;
    return capW + size * 0.55f + SpacedWidth(font, label, size, spacing);
}

void HintGroup(const Font& font, const char* key, const char* label, float x, float y,
               float size, float spacing, Color col)
{
    const float keyW  = SpacedWidth(font, key, size, spacing);
    const float capW  = keyW + size * KEYCAP_PAD_X * 2.0f;
    const float capH  = size * (1.0f + KEYCAP_PAD_Y * 2.0f);
    const float capY  = y - size * KEYCAP_PAD_Y;

    DrawRectangleRec({ x, capY, capW, capH }, ColorAlpha(COL_BG, 0.0f));
    DrawRectangleLinesEx({ x, capY, capW, capH }, 1.0f, col);
    DrawSpaced(font, key, x + size * KEYCAP_PAD_X, y, size, spacing, col);
    DrawSpaced(font, label, x + capW + size * 0.55f, y, size, spacing, col);
}

void DrawHints(const Game& g)
{
    const Layout& l = CurrentLayout();
    const float fade = g.stageTime < 6.0f ? 1.0f : 0.90f;
    const Color col = ColorAlpha(HUD_VIOLET, fade);

    const Font& f = g.font;
    const float size = l.hintSize;
    const float spacing = l.hintSpacing;

    const char* k1 = "esc";
    const char* t1 = "- menu";
    const char* k2 = "r";
    const char* t2 = "- begin again";

    const float gap = size * 2.4f;
    const float w1 = HintGroupWidth(f, k1, t1, size, spacing);
    const float w2 = HintGroupWidth(f, k2, t2, size, spacing);
    const float rowX = (l.screenW - (w1 + gap + w2)) * 0.5f;

    HintGroup(f, k1, t1, rowX, l.controlRow2Y, size, spacing, col);
    HintGroup(f, k2, t2, rowX + w1 + gap, l.controlRow2Y, size, spacing, col);

    const char* k3 = "wasd / arrows";
    const char* t3 = "- move";
    const float w3 = HintGroupWidth(f, k3, t3, size, spacing);
    HintGroup(f, k3, t3, (l.screenW - w3) * 0.5f, l.controlRowY, size, spacing, col);
}

void DrawFade(const Game& g)
{
    if (g.transition.t <= 0.001f) return;
    const Layout& l = CurrentLayout();
    DrawRectangle(0, 0, (int)l.screenW, (int)l.screenH,
                  ColorAlpha(g.cutsceneFade ? BLACK : COL_BG_DEEP, g.transition.t));
}

constexpr float DESIGN_EDGE = 34.0f;
constexpr float DESIGN_MID_X = (float)DESIGN_W * 0.5f;
constexpr float DESIGN_MID_Y = (float)DESIGN_H * 0.5f;

void DrawIntro(const Game& g)
{
    DrawBoard(g, g.boardReveal * (1.0f - 0.70f * Brightness(g.sceneTime, INTRO_LINES)));
    DrawBallShape(g, g.ballReveal);

    for (const IntroLine& l : INTRO_LINES)
    {
        const float a = BeatAlpha(g.sceneTime, l.at, l.dur);
        if (a <= 0.001f) continue;

        const float y = DESIGN_MID_Y - l.size * 0.5f - (l.heading ? 40.0f : 0.0f);
        Centered(l.heading ? g.titleFont : g.font, l.text, DESIGN_MID_X, y, l.size, l.spacing,
                 ColorAlpha(COL_TEXT, a * 0.95f));
    }

    const float w = SpacedWidth(g.font, "Skip", DESIGN_EDGE * 0.45f, 3.0f);
    DrawSpaced(g.font, "Skip", (float)DESIGN_W - DESIGN_EDGE - w, DESIGN_EDGE * 0.45f,
               DESIGN_EDGE * 0.45f, 3.0f,
               ColorAlpha(COL_TEXT_FAINT, 0.9f * std::min(1.0f, g.sceneTime / 2.0f)));
}

void DrawPlaying(const Game& g)
{
    DrawTrail(g.trail, g.settings.memory, g.clock);
    DrawBoard(g, 1.0f);
    DrawBallShape(g, 1.0f - (g.ball.lost ? std::min(1.0f, g.ball.lostTimer * 2.2f) : 0.0f));
}

void DrawFailureOverlay(const Game& g)
{
    if (!g.ball.lost) return;

    DrawRectangle(0, 0, (int)g_layout.screenW, (int)g_layout.screenH,
                  ColorAlpha(COL_BG_DEEP, 0.68f));

    const float cx = g_layout.screenW * 0.5f;
    const float cy = g_layout.screenH * 0.5f;
    const float size = std::clamp(g_layout.screenH * 0.055f, 28.0f, 44.0f);
    const Color red = { 0x88, 0x08, 0x08, 255 };

    Centered(g.font, "oops!", cx, cy - size * 1.55f, size, size * 0.12f, red);
    Centered(g.font, "you forgor", cx, cy - size * 0.35f, size * 0.76f,
             size * 0.10f, COL_TEXT);
    Centered(g.font, "press r to try again", cx, cy + size * 0.85f,
             g_layout.hintSize, g_layout.hintSpacing, HUD_VIOLET);
}

void DrawClearBeat(const Game& g)
{
    const float a = std::min(1.0f, g.sceneTime * 1.6f) *
                    std::min(1.0f, (1.9f - g.sceneTime) * 1.6f);

    Centered(g.titleFont, ClearLine(g.board.stage), DESIGN_MID_X, DESIGN_MID_Y - 15.0f, 30.0f,
             4.0f, ColorAlpha(COL_TEXT, a));

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d / %d", g.board.stage + 1, STAGE_COUNT);
    const float size = DESIGN_EDGE * 0.42f;
    const float w = SpacedWidth(g.font, buf, size, 3.0f);
    DrawSpaced(g.font, buf, (float)DESIGN_W - DESIGN_EDGE - w, DESIGN_EDGE * 0.45f, size, 3.0f,
               ColorAlpha(COL_TEXT_FAINT, a));
}

void DrawEndingSequence(const Game& g)
{
    const float fade = std::max(0.0f, 1.0f - g.sceneTime / 1.4f);

    DrawBoard(g, fade * (1.0f - 0.70f * Brightness(g.sceneTime, ENDING_LINES)));
    DrawBallShape(g, std::max(0.0f, 1.0f - g.sceneTime / 0.9f));

    for (const EndingLine& l : ENDING_LINES)
    {
        const float a = BeatAlpha(g.sceneTime, l.at, l.dur);
        if (a <= 0.001f) continue;

        Centered(g.font, l.text, DESIGN_MID_X, DESIGN_MID_Y - 12.0f, 24.0f, 5.0f,
                 ColorAlpha(COL_TEXT, a * 0.95f));
    }
}

void DrawCutscene(const Game& g)
{
    constexpr float FADE_TIME = 0.55f;
    constexpr float PAN_END = 2.30f;
    constexpr float IMAGE_LENGTH = 3.4f;
    struct CameraMove { float fromX, fromY, toX, toY, fromScale, toScale; };
    static constexpr CameraMove moves[] =
    {
        {  0.55f, -0.45f, -0.35f,  0.35f, 0.89f, 0.95f },
        { -0.70f,  0.50f,  0.45f, -0.55f, 0.88f, 0.95f },
        {  0.45f, -0.25f, -0.30f,  0.60f, 0.95f, 0.82f },
        {  0.60f, -0.35f, -0.45f,  0.40f, 0.89f, 0.95f },
        { -0.55f,  0.45f,  0.50f, -0.50f, 0.95f, 0.84f },
        {  0.35f, -0.30f, -0.25f,  0.25f, 0.88f, 0.62f },
    };

    const int image = (g.cutsceneEnding ? 3 : 0) + std::clamp(g.cutsceneIndex, 0, 2);
    const Texture texture = g.cutsceneTextures[image];
    const float time = std::clamp(g.sceneTime, 0.0f, IMAGE_LENGTH);
    const float fadeIn = Smooth(time / FADE_TIME);
    const float fadeOut = Smooth((IMAGE_LENGTH - time) / FADE_TIME);
    const float alpha = std::min(fadeIn, fadeOut);

    DrawRectangle(0, 0, (int)g_layout.screenW, (int)g_layout.screenH, BLACK);
    if (texture.id == 0 || texture.width <= 0 || texture.height <= 0 || alpha <= 0.001f)
        return;

    const CameraMove& move = moves[image];
    const float pan = Smooth((time - FADE_TIME) / (PAN_END - FADE_TIME));
    const float zoom = move.fromScale + (move.toScale - move.fromScale) * pan;
    const float fit = std::min(g_layout.screenW / texture.width,
                               g_layout.screenH / texture.height);
    const float width = texture.width * fit * zoom;
    const float height = texture.height * fit * zoom;
    const float marginX = std::max(0.0f, (g_layout.screenW - width) * 0.5f);
    const float marginY = std::max(0.0f, (g_layout.screenH - height) * 0.5f);
    const float offsetX = marginX * 0.82f * (move.fromX + (move.toX - move.fromX) * pan);
    const float offsetY = marginY * 0.82f * (move.fromY + (move.toY - move.fromY) * pan);
    const Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
    const Rectangle dest = { (g_layout.screenW - width) * 0.5f + offsetX,
                             (g_layout.screenH - height) * 0.5f + offsetY,
                             width, height };

    DrawTexturePro(texture, source, dest, { 0.0f, 0.0f }, 0.0f, ColorAlpha(WHITE, alpha));
}

void (*FrameDrawn)() = nullptr;

void DrawFrame(Game& g)
{
    g_layout = ComputeLayout();

    BeginDrawing();
        ClearBackground(COL_BG);

           if ((g.screen == SCREEN_MENU ||
               (g.screen == SCREEN_HELP && g.returnScreen == SCREEN_MENU)) && g.bgLoaded)
            DrawTextureRec(g.bg, { 0.0f, 0.0f, g_layout.screenW, g_layout.screenH },
                           { 0.0f, 0.0f }, WHITE);

        BeginWorldView(g_layout);
            switch (g.screen)
            {
                case SCREEN_INTRO:   DrawIntro(g);          break;
                case SCREEN_PLAYING: DrawPlaying(g); break;
                case SCREEN_HELP:
                    if (g.returnScreen != SCREEN_MENU) DrawPlaying(g);
                    break;
                case SCREEN_CLEAR:   DrawClearBeat(g);      break;
                case SCREEN_ENDING:  DrawEndingSequence(g); break;
                default: break;
            }
        EndWorldView();

        if (g.screen == SCREEN_CUTSCENE) DrawCutscene(g);

        if (g.screen == SCREEN_PLAYING)
        {
            DrawStoryLine(g);
            DrawHints(g);
            DrawHudControls(g);

            const float boxW = 74.0f;
            const float boxH = 34.0f;
            const float boxX = (g_layout.screenW - boxW) * 0.5f;
                const float boxY = g_layout.topRowY;
            const Rectangle pipBox = { boxX, boxY, boxW, boxH };
            DrawRectangleRec(pipBox, COL_BG);
            DrawRectangleLinesEx(pipBox, 1.0f, HUD_VIOLET);
            DrawHalfPips({ boxX + 3.0f, boxY + 3.0f, 28.0f, 28.0f }, g.ball.pipValue, HUD_VIOLET);

            char pipValue[2] = { (char)('0' + std::clamp(g.ball.pipValue, 0, 6)), '\0' };
            DrawSpaced(g.font, pipValue, boxX + 44.0f, boxY + 9.0f, 16.0f, 0.0f, HUD_VIOLET);
        }

        if (g.screen == SCREEN_PLAYING) DrawFailureOverlay(g);

        if (g.screen == SCREEN_MENU || g.screen == SCREEN_PAUSE ||
            g.screen == SCREEN_SETTINGS || g.screen == SCREEN_CREDITS ||
            g.screen == SCREEN_HELP)
        {
            DrawScreenUi(g);
        }

        DrawFade(g);

        if (FrameDrawn) FrameDrawn();
    EndDrawing();
}

}