#define RAYGUI_IMPLEMENTATION

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "ui.h"

#include "platform.h"
#include "raygui.h"
#include "sound.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace witness {

namespace {

constexpr float LINK_SPACING = 3.0f;
constexpr float HOVER_GROW   = 1.16f;

int g_buttons = 0;
int g_focusDelta = 0;

Font LoadFirst(const char* const* paths, int count, int size, const char* const* system,
               int systemCount, bool& loaded)
{
    loaded = false;
    for (int i = 0; i < count + systemCount; i++)
    {
        const char* path = (i < count) ? paths[i] : system[i - count];
        if (!FileExists(path)) continue;

        Font f = LoadFontEx(path, size, nullptr, 0);
        if (f.texture.id == 0 || f.glyphCount == 0 || f.baseSize == 0) continue;

        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        loaded = true;
        return f;
    }
    return GetFontDefault();
}

Font LoadSystem(const char* const* paths, int count, int size, bool& loaded)
{
    static const char* system[] =
    {
        "C:/Windows/Fonts/arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf"
    };
    return LoadFirst(paths, count, size, system, (int)(sizeof(system) / sizeof(system[0])),
                     loaded);
}

void Card(Rectangle r)
{
    DrawRectangleRounded(r, CORNER_R, 5, COL_PANEL);
    DrawRectangleRoundedLinesEx(r, CORNER_R, 5, 1.0f, COL_EDGE);
}

float CardWidth(const Layout& l) { return std::clamp(l.screenW * 0.40f, 250.0f, 470.0f); }

void Row(Rectangle box, const char* label, bool hover, bool focus, const Font& font)
{
    DrawRectangleRec(box, COL_TEXT);
    DrawRectangleLinesEx(box, 1.0f, COL_TEXT);
    const float size = box.height * 0.34f;
    DrawSpacedCentered(font, label, box.x + box.width * 0.5f,
                       box.y + (box.height - size) * 0.5f, size, size * 0.16f, COL_BG);
}

void DrawStatus(const Game& g, const char* text)
{
    if (!text || !*text) return;
    const Layout& l = CurrentLayout();
    DrawSpacedCentered(g.font, text, l.screenW * 0.5f, l.screenH - l.margin * 0.55f,
                       l.hintSize, l.hintSpacing, ColorAlpha(COL_TEXT_FAINT, 1.0f));
}

void Heading(const Game& g, const char* text, float cx, float y, float size, float spacing,
             Color color)
{
    DrawSpacedCentered(g.titleFont, text, cx, y, size, spacing, color);
}

void Label(const Game& g, const char* text, float cx, float y, float size, float spacing,
           Color color)
{
    DrawSpacedCentered(g.font, text, cx, y, size, spacing, color);
}

}

std::string ResolveItchUrl()
{
    if (FileExists(ITCH_URL_FILE))
    {
        FILE* f = fopen(ITCH_URL_FILE, "rb");
        if (f)
        {
            char buf[512] = { 0 };
            const size_t got = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            buf[got] = '\0';

            std::string url(buf);
            while (!url.empty() && (url.back() == '\n' || url.back() == '\r' || url.back() == ' '))
                url.pop_back();
            if (!url.empty()) return url;
        }
    }
    return std::string(IFG_ITCH_URL);
}

void LoadFonts(Game& g)
{
    g.titleFont = LoadSystem(FONT_TITLE_PATHS, (int)(sizeof(FONT_TITLE_PATHS) /
                                sizeof(FONT_TITLE_PATHS[0])), TITLE_FONT_SIZE,
                             g.titleFontLoaded);
    g.font = LoadSystem(FONT_BODY_PATHS, (int)(sizeof(FONT_BODY_PATHS) /
                        sizeof(FONT_BODY_PATHS[0])), UI_FONT_SIZE, g.fontLoaded);
}

void UnloadFonts(Game& g)
{
    if (g.fontLoaded) UnloadFont(g.font);
    if (g.titleFontLoaded) UnloadFont(g.titleFont);
    g.fontLoaded = g.titleFontLoaded = false;
}

void StyleUi()
{
    GuiLoadStyleDefault();
    GuiSetFont(GetFontDefault());

    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,    ColorToInt(COL_PANEL));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,   ColorToInt(COL_BG_DEEP));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,   ColorToInt({ 252, 252, 252, 255 }));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,  ColorToInt(COL_EDGE));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,    ColorToInt(COL_TEXT_DIM));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,   ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  ColorToInt(COL_TEXT_GHOST));
    GuiSetStyle(BUTTON, BORDER_WIDTH, 1);
    GuiSetStyle(SLIDER, SLIDER_WIDTH, 12);
}

bool UiButton(Game& g, Rectangle box, const char* label)
{
    const int slot = g_buttons++;

    const bool mouseHover = CheckCollisionPointRec(GetMousePosition(), box);
    if (mouseHover) g.focus = slot;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)) g_focusDelta++;
    if (IsKeyPressed(KEY_UP)) g_focusDelta--;

    const bool focused = (g.focus == slot);
    const bool lit = mouseHover || focused;
    const float grow = lit ? HOVER_GROW : 1.0f;

    const Rectangle scaled =
    {
        box.x + box.width * (1.0f - grow) * 0.5f,
        box.y + box.height * (1.0f - grow) * 0.5f,
        box.width * grow,
        box.height * grow
    };

    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                         CheckCollisionPointRec(GetMousePosition(), scaled);
    const bool fired = clicked || (focused && (IsKeyPressed(KEY_ENTER) ||
                                               IsKeyPressed(KEY_SPACE) ||
                                               IsKeyPressed(KEY_KP_ENTER)));

    Row(scaled, label, mouseHover, focused, g.font);

    if (fired)
    {
        sound::Pop();
        g.focus = slot;
    }

    return fired;
}

bool DrawClickableLink(Game& g, const char* text, float x, float y, float size, bool underlined)
{
    const float w = SpacedTextWidth(g.font, text, size, LINK_SPACING);
    const float h = MeasureTextEx(g.font, text, size, 0.0f).y;

    const Rectangle bounds = { x, y, w, h };
    const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hover) g.linkFocus = true;

    const bool urlSet = g.itchUrl.compare(0, 8, "https://") == 0 ||
                        g.itchUrl.compare(0, 7, "http://") == 0;

    const bool lit = (hover || g.linkFocus) && urlSet;
    const Color color = lit ? COL_TEXT : ColorAlpha(COL_TEXT_DIM, urlSet ? 1.0f : 0.80f);

    DrawSpaced(g.font, text, x, y, size, LINK_SPACING, color);

    const int lineY = (int)y + (int)h + 3;
    DrawLine((int)x, lineY, (int)(x + w), lineY,
             ColorAlpha(lit ? COL_TEXT : COL_EDGE, underlined ? 0.9f : 0.5f));
    if (lit) DrawLine((int)x, lineY + 3, (int)(x + w), lineY + 3, ColorAlpha(COL_TEXT_FAINT, 0.95f));

    g.linkHover = hover;

    bool clicked = false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover)
    {
        clicked = true;
        g.linkFocus = false;
    }

    const bool fired = clicked || ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                                    IsKeyPressed(KEY_KP_ENTER)) && g.linkFocus && urlSet);
    if (fired)
    {
        sound::Pop();
        if (urlSet) platform::OpenUrl(g.itchUrl.c_str());
    }

    return fired;
}

void DrawMenu(Game& g)
{
    const Layout& l = CurrentLayout();
    const float w = CardWidth(l);
    const float rowH = w * 0.135f;
    const float gap = rowH * 0.34f;
    const int rows = g.hasSave ? 4 : 3;

    const float cardH = rowH * 2.6f + rows * (rowH + gap) + l.margin * 1.2f;
    const Rectangle card = { (l.screenW - w) * 0.5f, (l.screenH - cardH) * 0.5f, w, cardH };
    Card(card);

    const float cx = card.x + w * 0.5f;
    const float titleSize = std::min(w * 0.20f, 52.0f);
    Heading(g, GAME_TITLE, cx, card.y + l.margin * 0.75f, titleSize, titleSize * 0.10f,
            ColorAlpha(COL_TEXT, 0.95f));
    Label(g, "everything is temporary", cx, card.y + l.margin * 0.75f + titleSize * 1.15f,
          std::min(w * 0.062f, 18.0f), 3.0f, COL_TEXT_FAINT);

    float y = card.y + rowH * 2.6f;
    const Rectangle row = { card.x + w * 0.12f, 0.0f, w * 0.76f, rowH };

    if (g.hasSave)
    {
        if (UiButton(g, { row.x, y, row.width, row.height }, "Continue")) { ContinueGame(g, true); return; }
        y += rowH + gap;
    }

    if (UiButton(g, { row.x, y, row.width, row.height }, g.hasSave ? "Start Over" : "Begin"))
    { StartNewGame(g); return; }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Settings"))
    {
        g.returnScreen = SCREEN_MENU;
        GoToScreen(g, SCREEN_SETTINGS);
        return;
    }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Quit")) CloseWindow();

    DrawStatus(g, sound::HasTrack() ? sound::TrackPath() : "no music track found");
}

void DrawPause(Game& g)
{
    const Layout& l = CurrentLayout();
    const float w = CardWidth(l);
    const float rowH = w * 0.135f;
    const float gap = rowH * 0.34f;
    const int rows = 4;

    const float cardH = rowH * 2.4f + rows * (rowH + gap) + l.margin;
    const Rectangle card = { (l.screenW - w) * 0.5f, (l.screenH - cardH) * 0.5f, w, cardH };
    Card(card);

    Heading(g, "Paused", card.x + w * 0.5f, card.y + l.margin, std::min(w * 0.16f, 38.0f), 6.0f,
            ColorAlpha(COL_TEXT, 0.95f));

    float y = card.y + rowH * 2.4f;
    const Rectangle row = { card.x + w * 0.12f, 0.0f, w * 0.76f, rowH };

    if (UiButton(g, { row.x, y, row.width, row.height }, "Resume"))
    { GoToScreen(g, SCREEN_PLAYING); return; }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Settings"))
    {
        g.returnScreen = SCREEN_PAUSE;
        GoToScreen(g, SCREEN_SETTINGS);
        return;
    }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Begin Again"))
    { RestartAttempt(g, false); return; }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Leave"))
    { SaveSession(g); GoToScreen(g, SCREEN_MENU); }
}

void DrawSettings(Game& g)
{
    const Layout& l = CurrentLayout();
    const float w = CardWidth(l) * 1.15f;
    const float rowH = w * 0.125f;

    const float cardH = rowH * 7.6f + l.margin * 1.4f;
    const Rectangle card = { (l.screenW - w) * 0.5f, (l.screenH - cardH) * 0.5f, w, cardH };
    Card(card);

    Heading(g, "Settings", card.x + w * 0.5f, card.y + l.margin * 0.8f,
            std::min(w * 0.13f, 34.0f), 6.0f, ColorAlpha(COL_TEXT, 0.95f));

    GuiSetFont(g.font);
    const float x = card.x + w * 0.12f;
    const float cw = w * 0.76f;
    float y = card.y + rowH * 2.1f;

    if (GuiSliderBar({ x, y, cw, 22.0f }, "Volume", "0 - 100", &g.settings.volume, 0.0f, 1.0f))
        sound::Pop();
    sound::SetMasterVolume(g.settings.volume);
    y += rowH * 1.35f;

    if (GuiSliderBar({ x, y, cw, 22.0f }, "Memory", "less", &g.settings.memory, 0.25f, 1.25f))
        sound::Pop();
    y += rowH * 1.5f;

    if (GuiCheckBox({ x, y, cw, 24.0f }, "Skip Intro After First Play", &g.settings.skipIntro))
        sound::Pop();
    y += rowH * 2.4f;

    const float bw = w * 0.44f;
    if (UiButton(g, { card.x + (w - bw) * 0.5f, y, bw, rowH }, "Back"))
        GoToScreen(g, g.returnScreen);

    DrawStatus(g, sound::HasTrack() ? sound::TrackPath() : "no audio track loaded");
}

void DrawCredits(Game& g)
{
    const Layout& l = CurrentLayout();
    const float t = std::min(1.0f, g.sceneTime / 1.6f);
    const float w = CardWidth(l) * 1.3f;
    const float size = std::min(w * 0.045f, 17.0f);
    const float gap = size * 1.9f;

    const float cardH = gap * 12.5f + l.margin * 1.6f;
    const Rectangle card = { (l.screenW - w) * 0.5f, (l.screenH - cardH) * 0.5f, w, cardH };
    Card(card);

    const float cx = card.x + w * 0.5f;
    float y = card.y + l.margin;

    Heading(g, GAME_TITLE, cx, y, std::min(w * 0.17f, 40.0f), 7.0f, ColorAlpha(COL_TEXT, t));
    y += gap * 1.5f;
    Label(g, "a game about temporary things", cx, y, size, 3.0f, ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;

    char buf[80];
    std::snprintf(buf, sizeof(buf), "%d passages   %d tiles erased", STAGE_COUNT,
                  g.board.emptiedTotal);
    Label(g, buf, cx, y, size * 0.92f, 2.0f, ColorAlpha(COL_TEXT_FAINT, t));
    y += gap * 1.3f;

    Label(g, "made by", cx, y, size * 0.92f, 3.0f, ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;
    Label(g, IFG_AUTHOR, cx, y, size * 1.25f, 3.0f, ColorAlpha(COL_TEXT_DIM, t));
    y += gap * 1.3f;

    Label(g, "WASD / Arrows - Move     R - Begin Again     Esc - Menu", cx, y, size * 0.88f, 2.0f,
          ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;
    Label(g, "built with raylib and raygui (zlib)", cx, y, size * 0.88f, 2.0f,
          ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;

    std::string music = "music: none found";
    if (sound::HasTrack()) music = "music: " + std::string(sound::TrackPath());
    Label(g, music.c_str(), cx, y, size * 0.88f, 2.0f, ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;

    Label(g, "type: DOSMIC and SERATONIN (personal use licence)", cx, y, size * 0.88f, 2.0f,
          ColorAlpha(COL_TEXT_FAINT, t));
    y += gap * 1.4f;

    const char* url = g.itchUrl.empty() ? "itch.io page not configured" : g.itchUrl.c_str();
    const float urlSize = size * 1.15f;
    const float urlW = SpacedTextWidth(g.font, url, urlSize, LINK_SPACING);
    Label(g, "itch.io", cx, y, size * 0.88f, 3.0f, ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;
    DrawClickableLink(g, url, cx - urlW * 0.5f, y, urlSize, true);
    y += gap * 1.2f;

    Label(g, g.itchUrl.empty() ? "build with -DIFG_ITCH_URL=... or add assets/itch_url.txt"
                               : "click the link, or TAB then ENTER",
          cx, y, size * 0.85f, 2.0f, ColorAlpha(COL_TEXT_FAINT, t));

    const float bw = (w - w * 0.10f) * 0.5f;
    const float by = card.y + card.height - l.margin * 0.9f;
    const float ebw = w * 0.40f;
    const float eby = by;
    const float ebh = l.margin * 1.9f;
    const float egap = w * 0.04f;
    if (UiButton(g, { cx - ebw - egap * 0.5f, eby, ebw, ebh }, "Play Again"))
    { StartNewGame(g); return; }
    UiButton(g, { cx + egap * 0.5f, eby, ebw, ebh }, "Main Menu");

    if (t >= 1.0f)
        Label(g, "R - Play Again", cx, l.screenH - l.margin * 0.35f, l.hintSize, 2.0f,
              ColorAlpha(COL_TEXT_FAINT, 0.95f));
}

void LoadBackground(Game& g)
{
    const int n = (int)(sizeof(BG_IMAGE) / sizeof(BG_IMAGE[0]));
    for (int i = 0; i < n; i++)
    {
        if (!FileExists(BG_IMAGE[i])) continue;
        Texture t = LoadTexture(BG_IMAGE[i]);
        if (t.id == 0) continue;
        g.bg = t;
        g.bgLoaded = true;
        return;
    }
}

void UnloadBackground(Game& g)
{
    if (g.bgLoaded) UnloadTexture(g.bg);
    g.bg = {};
    g.bgLoaded = false;
}

enum { ICON_SOUND = 0, ICON_SETTINGS };

bool UiIconButton(Game& g, Rectangle box, int icon)
{
    const int slot = g_buttons++;

    const bool hover = CheckCollisionPointRec(GetMousePosition(), box);
    if (hover) g.focus = slot;

    const bool focused = (g.focus == slot);
    const bool lit = hover || focused;
    const float grow = lit ? HOVER_GROW : 1.0f;
    const Rectangle s =
    {
        box.x + box.width * (1.0f - grow) * 0.5f,
        box.y + box.height * (1.0f - grow) * 0.5f,
        box.width * grow,
        box.height * grow
    };

    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                         CheckCollisionPointRec(GetMousePosition(), s);
    const bool fired = clicked || (focused && (IsKeyPressed(KEY_ENTER) ||
                                               IsKeyPressed(KEY_SPACE)));

    DrawRectangleRounded(s, CORNER_R, 4, COL_PANEL);
    DrawRectangleRoundedLinesEx(s, CORNER_R, 4, 1.0f, lit ? COL_TEXT : COL_EDGE);

    const float cx = s.x + s.width * 0.5f;
    const float cy = s.y + s.height * 0.5f;
    const float u  = s.width * 0.24f;
    const Color ink = lit ? COL_TEXT : COL_EDGE;

    if (icon == ICON_SOUND)
    {
        DrawRectangleRec({ cx - u * 1.30f, cy - u * 0.55f, u * 0.75f, u * 1.10f }, ink);
        DrawTriangle({ cx - u * 0.60f, cy - u * 1.25f },
                     { cx - u * 0.60f, cy + u * 1.25f },
                     { cx + u * 0.45f, cy }, ink);
        if (sound::IsMuted())
        {
            DrawLine(cx + u * 0.30f, cy - u * 1.00f, cx + u * 1.45f, cy + u * 1.00f, ink);
            DrawLine(cx + u * 1.45f, cy - u * 1.00f, cx + u * 0.30f, cy + u * 1.00f, ink);
        }
        else
        {
            DrawCircleLines((int)(cx + u * 0.95f), (int)cy, u * 0.80f, ColorAlpha(ink, 0.75f));
            DrawCircleLines((int)(cx + u * 0.95f), (int)cy, u * 1.35f, ColorAlpha(ink, 0.45f));
        }
    }
    else
    {
        const float halfW = u * 1.55f;
        const float rows[3] = { cy - u * 0.95f, cy, cy + u * 0.95f };
        const float knob[3] = { -0.55f, 0.45f, -0.15f };
        for (int i = 0; i < 3; i++)
        {
            DrawLine(cx - halfW, rows[i], cx + halfW, rows[i], ink);
            DrawCircle((int)(cx + halfW * knob[i]), (int)rows[i], u * 0.42f, ink);
        }
    }

    if (fired)
    {
        sound::Pop();
        g.focus = slot;
    }

    return fired;
}

void DrawHudIcons(Game& g)
{
    const Layout& l = CurrentLayout();
    g_buttons = 0;
    g_focusDelta = 0;

    const float sz  = l.hintSize * 2.3f;
    const float pad = l.margin * 0.40f;
    const float y   = l.margin * 0.20f;

    const Rectangle settingsBox = { l.screenW - l.margin - sz, y, sz, sz };
    const Rectangle soundBox    = { settingsBox.x - sz - pad, y, sz, sz };

    if (UiIconButton(g, soundBox, ICON_SOUND))
    {
        g.settings.muted = !g.settings.muted;
        sound::SetMuted(g.settings.muted);
    }

    if (UiIconButton(g, settingsBox, ICON_SETTINGS))
    {
        g.returnScreen = SCREEN_PLAYING;
        GoToScreen(g, SCREEN_SETTINGS);
    }
}

void DrawScreenUi(Game& g)
{
    g_buttons = 0;
    g_focusDelta = 0;

    switch (g.screen)
    {
        case SCREEN_MENU:     DrawMenu(g);     break;
        case SCREEN_PAUSE:    DrawPause(g);    break;
        case SCREEN_SETTINGS: DrawSettings(g); break;
        case SCREEN_CREDITS:  DrawCredits(g);  break;
        default: break;
    }

    if (g_buttons > 0)
    {
        g.focus = std::clamp(g.focus + g_focusDelta, 0, g_buttons - 1);
        if (g.focus == 0 && g_focusDelta < 0) g.focus = g_buttons - 1;
    }
}

}