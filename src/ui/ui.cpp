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
#endif
#include "game.h"
#include "raygui.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace witness {

Font LoadUiFont(bool& loaded, int& size)
{
    static const char* candidates[] = {
        "assets/arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/ARIAL.TTF",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
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
    GuiSetFont(font);

    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,   ColorToInt(COL_PANEL));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,  ColorToInt(COL_EDGE));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,  ColorToInt(COL_ACCENT));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(COL_EDGE));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED,ColorToInt(COL_ACCENT));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED,ColorToInt(COL_ACCENT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,   ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,  ColorToInt(COL_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,  ColorToInt(COL_TEXT));
    GuiSetStyle(BUTTON,  BORDER_WIDTH,        1);
    GuiSetStyle(BUTTON,  TEXT_ALIGNMENT,      TEXT_ALIGN_CENTER);
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
    g.bg = Texture2D{};
    g.bgLoaded = false;
}

namespace {

Rectangle CenterBox(int w, int h)
{
    return Rectangle{ (SCREEN_W - w) * 0.5f, (SCREEN_H - h) * 0.5f, (float)w, (float)h };
}

void DrawTitle(const char* text, int y, int size, Color color)
{
    const int w = MeasureText(text, size);
    DrawText(text, (SCREEN_W - w) / 2, y, size, color);
}

void DrawDim(const char* text, int y, int size, Color color)
{
    const int w = MeasureText(text, size);
    DrawText(text, (SCREEN_W - w) / 2, y, size, color);
}

void DrawFallbackBackground()
{
    for (int y = 0; y < SCREEN_H; y += 4)
    {
        const float t = (float)y / (float)SCREEN_H;
        const unsigned char v = (unsigned char)(7.0f + 16.0f * t);
        DrawRectangle(0, y, SCREEN_W, 4, Color{ v, v, (unsigned char)(v + 3), 255 });
    }
}

}

void DrawBackground(const Game& g)
{
    if (g.showBg && g.bgLoaded && g.bg.id != 0)
    {
        const float scale = fmaxf((float)SCREEN_W / (float)g.bg.width,
                                  (float)SCREEN_H / (float)g.bg.height);
        const int w = (int)((float)g.bg.width * scale);
        const int h = (int)((float)g.bg.height * scale);
        DrawTexture(g.bg, (SCREEN_W - w) / 2, (SCREEN_H - h) / 2, WHITE);
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, COL_SCRIM);
    }
    else
    {
        DrawFallbackBackground();
    }
}

void DrawMenu(Game& g)
{
    DrawTitle("i forgor", 128, 54, COL_TEXT);
    DrawDim("a basic raylib shell", 186, UI_FONT_SIZE, COL_TEXT_DIM);

    Rectangle play = CenterBox(320, 56);
    Rectangle settings = CenterBox(320, 56);
    Rectangle quit = CenterBox(320, 56);

    play.y = 268;
    settings.y = play.y + 72;
    quit.y = settings.y + 72;

    if (GuiButton(play, "PLAY"))     GoToScreen(g, SCREEN_PLAYING);
    if (GuiButton(settings, "SETTINGS")) GoToScreen(g, SCREEN_SETTINGS);
    if (GuiButton(quit, "QUIT"))    CloseWindow();

    DrawDim(g.bgLoaded ? "background: assets/bg.png"
                       : "background: none (drop a png at assets/bg.png)",
            SCREEN_H - 56, 20, COL_TEXT_FAINT);
}

void DrawPlaying(const Game& g)
{
    DrawTitle("PLAYING", 236, 46, COL_TEXT);

    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f s", (double)g.elapsed);
    DrawDim(buf, 306, UI_FONT_SIZE, COL_ACCENT);

    DrawDim("press SPACE or ENTER to continue", SCREEN_H - 120, UI_FONT_SIZE, COL_TEXT_DIM);
}

void DrawEnding(Game& g)
{
    DrawTitle("THE END", 200, 54, COL_TEXT);
    DrawDim("thanks for playing", 268, UI_FONT_SIZE, COL_TEXT_DIM);

    Rectangle back = CenterBox(320, 56);
    back.y = 372;
    if (GuiButton(back, "BACK TO MENU")) GoToScreen(g, SCREEN_MENU);
}

void DrawSettings(Game& g)
{
    DrawTitle("SETTINGS", 176, 46, COL_TEXT);

    Rectangle bgToggle = CenterBox(320, 56);
    bgToggle.y = 272;
    if (GuiButton(bgToggle, g.showBg ? "BACKGROUND: ON" : "BACKGROUND: OFF"))
        g.showBg = !g.showBg;

    Rectangle back = CenterBox(320, 56);
    back.y = 344;
    if (GuiButton(back, "BACK")) GoToScreen(g, SCREEN_MENU);
}

void DrawFrame(Game& g)
{
    BeginDrawing();
        ClearBackground(COL_BG);
        DrawBackground(g);

        switch (g.screen)
        {
            case SCREEN_MENU:     DrawMenu(g);     break;
            case SCREEN_PLAYING:  DrawPlaying(g);  break;
            case SCREEN_ENDING:   DrawEnding(g);   break;
            case SCREEN_SETTINGS: DrawSettings(g); break;
        }
    EndDrawing();
}

}