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

Rectangle CenterBox(int w, int h, int y)
{
    return Rectangle{ (SCREEN_W - w) * 0.5f, (float)y, (float)w, (float)h };
}

// Small faint note used for honest status: a missing track, an unconfigured link.
void DrawStatus(const Game& g, const char* text)
{
    if (!text || !*text) return;
    DrawSpacedCentered(g.font, text, SCREEN_W * 0.5f, (float)SCREEN_H - 52.0f, 14.0f, 2.0f,
                       ColorAlpha(COL_TEXT_FAINT, 0.85f));
}

void DrawTitle(const Game& g, const char* text, int y, int size, float spacing, Color color)
{
    DrawSpacedCentered(g.font, text, SCREEN_W * 0.5f, (float)y, (float)size, spacing, color);
}

} // namespace

//------------------------------------------------------------------------------------
// Resolves the itch.io address once at startup: a plain text file wins so a build can
// be re-pointed without a recompile, otherwise the value baked in at build time is
// used. Nothing is invented when neither is present.
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

    return std::string(ATF_ITCH_URL);
}


//------------------------------------------------------------------------------------
// Font and background plumbing carried over from the original shell
//------------------------------------------------------------------------------------
Font LoadUiFont(bool& loaded, int& size)
{
    static const char* candidates[] = {
        "assets/arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/ARIAL.TTF",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
    };

    loaded = false;
    size = UI_FONT_SIZE;

    for (const char* path : candidates)
    {
        if (!FileExists(path)) continue;

        Font f = LoadFontEx(path, size, nullptr, 0);
        if (f.texture.id == 0 || f.glyphCount == 0 || f.baseSize == 0) continue;

        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        loaded = true;
        return f;
    }

    return GetFontDefault();
}

void StyleUi(Font font)
{
    // Raygui loads its default style the first time a control is drawn, which would
    // overwrite anything set before that point. Loading it here makes the overrides below
    // the last word.
    GuiLoadStyleDefault();

    GuiSetFont(font);

    // Greyscale only. There is no colour in this game, including in its panels.
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,    ColorToInt({ 14, 14, 14, 235 }));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,   ColorToInt({  0,  0,  0, 255 }));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,   ColorToInt({ 44, 44, 44, 255 }));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,  ColorToInt({ 70, 70, 70, 255 }));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt({ 236, 236, 236, 255 }));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt({ 236, 236, 236, 255 }));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,    ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,   ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  ColorToInt(COL_TEXT_GHOST));
    GuiSetStyle(BUTTON,  BORDER_WIDTH,         1);
    GuiSetStyle(BUTTON,  TEXT_ALIGNMENT,       TEXT_ALIGN_CENTER);
    GuiSetStyle(LABEL,   TEXT_ALIGNMENT,       TEXT_ALIGN_LEFT);
    GuiSetStyle(SLIDER,  SLIDER_WIDTH,         12);
}

bool LoadBackground(Game& g)
{
    if (!FileExists(BG_IMAGE)) return false;

    g.bg = LoadTexture(BG_IMAGE);
    if (g.bg.id == 0) return false;

    SetTextureFilter(g.bg, TEXTURE_FILTER_BILINEAR);
    return true;
}

void UnloadBackground(Game& g)
{
    if (g.bg.id != 0) UnloadTexture(g.bg);
    g.bg = {};
    g.bgLoaded = false;
}

void DrawBackground(const Game& g)
{
    if (g.settings.showBg && g.bgLoaded && g.bg.id != 0)
    {
        const float scale = std::max((float)SCREEN_W / (float)g.bg.width,
                                     (float)SCREEN_H / (float)g.bg.height);
        const int w = (int)((float)g.bg.width * scale);
        const int h = (int)((float)g.bg.height * scale);
        DrawTexture(g.bg, (SCREEN_W - w) / 2, (SCREEN_H - h) / 2, WHITE);
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, COL_SCRIM);
    }
}

//------------------------------------------------------------------------------------
// Clickable link
//------------------------------------------------------------------------------------
bool DrawClickableLink(Game& g, const char* text, float x, float y, float size, bool underlined)
{
    const float w = SpacedTextWidth(g.font, text, size, LINK_SPACING);
    const float h = MeasureTextEx(g.font, text, size, 0.0f).y;

    const Rectangle bounds = { x, y, w, h };
    const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);

    // Keyboard focus: the same link can be reached and fired without a mouse.
    if (IsKeyPressed(KEY_TAB) ||
        IsKeyPressed(KEY_DOWN) ||
        IsKeyPressed(KEY_UP))
    {
        g.linkFocus = !g.linkFocus;
    }

    const bool active = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                        IsKeyPressed(KEY_KP_ENTER);

    const bool urlSet = g.itchUrl.compare(0, 8, "https://") == 0 ||
                        g.itchUrl.compare(0, 7, "http://") == 0;

    const bool lit = (hover || g.linkFocus) && urlSet;
    const Color color = lit ? COL_TEXT : ColorAlpha(COL_TEXT_DIM, urlSet ? 1.0f : 0.55f);

    DrawSpaced(g.font, text, x, y, size, LINK_SPACING, color);

    // A permanent underline plus hover emphasis: the clickability has to be legible
    // before the player tries it.
    const int lineY = (int)y + (int)h + 3;
    DrawLine((int)x, lineY, (int)(x + w), lineY,
             ColorAlpha(lit ? COL_TEXT : COL_EDGE, underlined ? 0.9f : 0.5f));
    if (lit) DrawLine((int)x, lineY + 3, (int)(x + w), lineY + 3, ColorAlpha(COL_TEXT_DIM, 0.5f));

    g.linkHover = hover;

    bool clicked = false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover)
    {
        clicked = true;
        g.linkFocus = false;   // the click is consumed here; nothing else sees it
    }

    const bool fired = clicked || (active && g.linkFocus && urlSet);

    if (fired)
    {
        if (urlSet) platform::OpenUrl(g.itchUrl.c_str());
    }

    return fired;
}

//------------------------------------------------------------------------------------
// Screens
//------------------------------------------------------------------------------------
void DrawMenu(Game& g)
{
    DrawTitle(g, GAME_TITLE, 150, 46, 8.0f, ColorAlpha(COL_TEXT, 0.95f));
    DrawTitle(g, "everything is temporary", 206, 18, 4.0f, ColorAlpha(COL_TEXT_FAINT, 0.9f));

    int y = 300;
    if (g.hasSave)
    {
        if (GuiButton(CenterBox(300, 50, y), "CONTINUE"))  ContinueGame(g, true);
        y += 62;
    }

    if (GuiButton(CenterBox(300, 50, y), g.hasSave ? "START OVER" : "BEGIN")) StartNewGame(g);
    y += 62;

    if (GuiButton(CenterBox(300, 50, y), "SETTINGS"))
    {
        g.returnScreen = SCREEN_MENU;
        GoToScreen(g, SCREEN_SETTINGS);
    }
    y += 62;

    if (GuiButton(CenterBox(300, 50, y), "QUIT")) CloseWindow();

    // Honest status: which file the music actually came from, if any.
    DrawStatus(g, sound::HasTrack() ? sound::TrackPath()
                                    : "no music track found - drop one in assets/");
}

void DrawPause(Game& g)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, COL_SCRIM);

    DrawTitle(g, "PAUSED", 236, 34, 7.0f, ColorAlpha(COL_TEXT, 0.95f));

    int y = 320;
    if (GuiButton(CenterBox(300, 50, y), "RESUME"))  GoToScreen(g, SCREEN_PLAYING);
    y += 62;
    if (GuiButton(CenterBox(300, 50, y), "SETTINGS"))
    {
        g.returnScreen = SCREEN_PAUSE;
        GoToScreen(g, SCREEN_SETTINGS);
    }
    y += 62;
    if (GuiButton(CenterBox(300, 50, y), "BEGIN AGAIN")) RestartAttempt(g, false);
    y += 62;
    if (GuiButton(CenterBox(300, 50, y), "LEAVE"))   { SaveSession(g); GoToScreen(g, SCREEN_MENU); }
}

void DrawSettings(Game& g)
{
    DrawTitle(g, "SETTINGS", 168, 34, 7.0f, ColorAlpha(COL_TEXT, 0.95f));

    // Raygui 5.1 sliders and checkboxes write through pointers.
    const Rectangle slider = { SCREEN_W * 0.5f - 130.0f, 268.0f, 260.0f, 20.0f };
    GuiSliderBar(slider, "VOLUME", "0 - 100", &g.settings.volume, 0.0f, 1.0f);
    sound::SetMasterVolume(g.settings.volume);

    const Rectangle memory = { SCREEN_W * 0.5f - 130.0f, 320.0f, 260.0f, 20.0f };
    GuiSliderBar(memory, "MEMORY", "less", &g.settings.memory, 0.25f, 1.25f);

    const Rectangle skip = { SCREEN_W * 0.5f - 130.0f, 372.0f, 260.0f, 24.0f };
    GuiCheckBox(skip, "SKIP INTRO AFTER FIRST PLAY", &g.settings.skipIntro);

    const Rectangle bg = { SCREEN_W * 0.5f - 130.0f, 412.0f, 260.0f, 24.0f };
    GuiCheckBox(bg, "USE assets/bg.png (PLACEHOLDER)", &g.settings.showBg);

    if (GuiButton(CenterBox(300, 50, 486), "BACK")) GoToScreen(g, g.returnScreen);

    DrawStatus(g, sound::HasTrack() ? sound::TrackPath() : "no audio track loaded (assets/)");
}

void DrawCredits(Game& g)
{
    const float t = std::min(1.0f, g.sceneTime / 1.6f);
    const float a = t;

    DrawTitle(g, GAME_TITLE, 118, 40, 8.0f, ColorAlpha(COL_TEXT, a));
    DrawTitle(g, "a game about temporary things", 166, 17, 4.0f, ColorAlpha(COL_TEXT_FAINT, a));

    char stageLine[80];
    std::snprintf(stageLine, sizeof(stageLine), "%d passages   %d tiles erased",
                  STAGE_COUNT, g.board.emptiedTotal);
    DrawTitle(g, stageLine, 206, 15, 2.0f, ColorAlpha(COL_TEXT_FAINT, a * 0.8f));

    DrawTitle(g, "made by", 262, 15, 3.0f, ColorAlpha(COL_TEXT_GHOST, a));
    DrawTitle(g, ATF_AUTHOR, 286, 22, 3.0f, ColorAlpha(COL_TEXT_DIM, a));

    // Controls, so the credits also work as a reminder.
    DrawTitle(g, "WASD / ARROWS - MOVE     R - BEGIN AGAIN     ESC - MENU",
              338, 15, 2.0f, ColorAlpha(COL_TEXT_GHOST, a));

    // Attribution: the engine, the UI kit, and the supplied music.
    DrawTitle(g, "built with raylib and raygui (zlib)", 372, 15, 2.0f,
              ColorAlpha(COL_TEXT_GHOST, a * 0.95f));

    std::string passageCredit = "music: none supplied";
    if (sound::HasTrack()) passageCredit = "music: " + std::string(sound::TrackPath());

    std::string frameCredit = "opening, ending and settings: none supplied";
    if (sound::HasFrameTrack())
        frameCredit = "opening, ending and settings: " + std::string(sound::FrameTrackPath());

    DrawTitle(g, passageCredit.c_str(), 396, 15, 2.0f, ColorAlpha(COL_TEXT_GHOST, a * 0.95f));
    DrawTitle(g, frameCredit.c_str(), 418, 15, 2.0f, ColorAlpha(COL_TEXT_GHOST, a * 0.95f));

    // The itch.io page. If no URL was configured at build time the text says so rather
    // than inventing a link.
    const char* url = g.itchUrl.empty() ? "itch.io page not configured"
                                        : g.itchUrl.c_str();
    const float urlW = SpacedTextWidth(g.font, url, 20.0f, LINK_SPACING);
    const float urlX = SCREEN_W * 0.5f - urlW * 0.5f;

    DrawTitle(g, "itch.io", 436, 15, 3.0f, ColorAlpha(COL_TEXT_GHOST, a));
    (void)DrawClickableLink(g, url, urlX, 458.0f, 20.0f, true);

    if (!g.itchUrl.empty())
    {
        DrawTitle(g, "click the link, or TAB then ENTER", 492, 14, 2.0f,
                  ColorAlpha(COL_TEXT_GHOST, a * 0.8f));
    }
    else
    {
        DrawTitle(g, "build with -DATF_ITCH_URL=... or add assets/itch_url.txt", 492, 14, 2.0f,
                  ColorAlpha(COL_TEXT_GHOST, a * 0.8f));
    }

    // Ending choices, then the credits get out of the way.
    int y = 552;
    if (GuiButton(CenterBox(300, 46, y), "PLAY AGAIN")) StartNewGame(g);
    if (GuiButton(CenterBox(300, 46, y + 56), "MAIN MENU")) GoToScreen(g, SCREEN_MENU);

    if (a >= 1.0f)
    {
        DrawSpacedCentered(g.font, "R - PLAY AGAIN", SCREEN_W * 0.5f, (float)SCREEN_H - 46.0f,
                           14.0f, 2.0f, ColorAlpha(COL_TEXT_FAINT, 0.7f));
    }
}

void DrawScreenUi(Game& g)
{
    switch (g.screen)
    {
        case SCREEN_MENU:     DrawMenu(g);     break;
        case SCREEN_PAUSE:    DrawPause(g);    break;
        case SCREEN_SETTINGS: DrawSettings(g); break;
        case SCREEN_CREDITS:  DrawCredits(g);  break;
        default: break;
    }
}

} // namespace witness
