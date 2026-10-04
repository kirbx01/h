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
constexpr Color HOVER_TEXT   = { 0x88, 0x08, 0x08, 255 };

int g_buttons = 0;
int g_focusDelta = 0;
int g_hoveredControl = -1;
int g_currentHoveredControl = -1;

bool g_focusArmed = false;

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

}

float CardWidth(const Layout& l) { return std::clamp(l.screenW * 0.40f, 250.0f, 470.0f); }

void Row(Rectangle box, const char* label, bool hover, bool focus, const Font& font)
{
    const float size = box.height * 0.34f;
    const Color color = (hover || focus) ? HOVER_TEXT : COL_TEXT;
    DrawSpacedCentered(font, label, box.x + box.width * 0.5f,
                       box.y + (box.height - size) * 0.5f, size, size * 0.16f, color);
    if (hover || focus)
    {
        const float w = SpacedTextWidth(font, label, size, size * 0.16f);
        DrawLine(box.x + box.width * 0.5f - w * 0.5f, box.y + (box.height - size) * 0.5f + size,
                 box.x + box.width * 0.5f + w * 0.5f, box.y + (box.height - size) * 0.5f + size, color);
    }
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

void PopOnHover(Game& g, Rectangle bounds, int control)
{
    if (!CheckCollisionPointRec(GetMousePosition(), bounds)) return;

    const int id = static_cast<int>(g.screen) * 1000 + control;
    if (id != g_hoveredControl) sound::Pop();
    g_currentHoveredControl = id;
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
    return IFG_ITCH_URL[0] ? std::string(IFG_ITCH_URL)
                            : std::string("https://kirbx01.itch.io/i-forgor");
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
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,   ColorToInt(HOVER_TEXT));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  ColorToInt(COL_TEXT_GHOST));
    GuiSetStyle(BUTTON, BORDER_WIDTH, 1);
    GuiSetStyle(SLIDER, SLIDER_WIDTH, 12);
}

bool UiButton(Game& g, Rectangle box, const char* label)
{
    const int slot = g_buttons++;

    const bool mouseHover = CheckCollisionPointRec(GetMousePosition(), box);
    if (g.screen == SCREEN_MENU || g.screen == SCREEN_SETTINGS)
        PopOnHover(g, box, slot);
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

bool DrawClickableLink(Game& g, const char* text, const char* url, float x, float y,
                       float size, bool underlined)
{
    const float w = SpacedTextWidth(g.font, text, size, LINK_SPACING);
    const float h = MeasureTextEx(g.font, text, size, 0.0f).y;

    const Rectangle bounds = { x, y, w, h };
    const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hover) g.linkFocus = true;

    const std::string target = url ? url : "";
    const bool urlSet = target.compare(0, 8, "https://") == 0 ||
                        target.compare(0, 7, "http://") == 0;

    const bool lit = (hover || g.linkFocus) && urlSet;
    const Color color = lit ? HOVER_TEXT : ColorAlpha(COL_TEXT_DIM, urlSet ? 1.0f : 0.80f);

    DrawSpaced(g.font, text, x, y, size, LINK_SPACING, color);

    const int lineY = (int)y + (int)h + 3;
    DrawLine((int)x, lineY, (int)(x + w), lineY,
             ColorAlpha(lit ? HOVER_TEXT : COL_EDGE, underlined ? 0.9f : 0.5f));
    if (lit) DrawLine((int)x, lineY + 3, (int)(x + w), lineY + 3, ColorAlpha(HOVER_TEXT, 0.95f));

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
        if (urlSet) platform::OpenUrl(target.c_str());
    }

    return fired;
}

void DrawMenu(Game& g)
{
    const Layout& l = CurrentLayout();
    const float w = CardWidth(l);
    const float rowH = w * 0.135f;
    const float gap = rowH * 0.34f;
    const int rows = g.hasSave ? 6 : 5;

    const float menuH = rows * (rowH + gap);
    const float cx = l.screenW * 0.5f;
    const float startY = (l.screenH - menuH - rowH * 3.0f) * 0.5f;
    const float titleSize = std::clamp(l.screenH * 0.12f, 42.0f, 96.0f);
    Heading(g, GAME_TITLE, cx, startY, titleSize, titleSize * 0.10f,
            ColorAlpha(COL_TEXT, 1.0f));
    Label(g, "everything is temporary", cx, startY + titleSize * 1.15f,
          std::clamp(l.screenH * 0.04f, 16.0f, 24.0f), 3.0f, COL_TEXT_FAINT);

    float y = startY + titleSize * 2.5f;
    const Rectangle row = { cx - w * 0.38f, 0.0f, w * 0.76f, rowH };

    if (g.hasSave)
    {
        if (UiButton(g, { row.x, y, row.width, row.height }, "Continue")) { ContinueGame(g, true); return; }
        y += rowH + gap;
    }

    if (UiButton(g, { row.x, y, row.width, row.height }, g.hasSave ? "Start Over" : "Begin"))
    { StartNewGame(g); return; }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "How to Win"))
    {
        g.returnScreen = SCREEN_MENU;
        g.helpPage = 0;
        g.focus = 1;
        g.helpInputLock = true;
        GoToScreen(g, SCREEN_HELP);
        return;
    }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Settings"))
    {
        g.returnScreen = SCREEN_MENU;
        GoToScreen(g, SCREEN_SETTINGS);
        return;
    }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Credits"))
    { GoToScreen(g, SCREEN_CREDITS); return; }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Quit")) CloseWindow();

}

void DrawPause(Game& g)
{
    const Layout& l = CurrentLayout();
    const float w = CardWidth(l);
    const float rowH = w * 0.135f;
    const float gap = rowH * 0.34f;
    const int rows = 4;

    const float menuH = rows * (rowH + gap);
    const float cx = l.screenW * 0.5f;
    const float startY = (l.screenH - menuH - rowH * 1.5f) * 0.5f;
    Heading(g, "Paused", cx, startY, std::clamp(l.screenH * 0.08f, 32.0f, 48.0f), 6.0f,
            ColorAlpha(COL_TEXT, 1.0f));

    float y = startY + rowH * 1.8f;
    const Rectangle row = { cx - w * 0.38f, 0.0f, w * 0.76f, rowH };

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
    { RestartAttempt(g, false); GoToScreen(g, SCREEN_PLAYING); return; }
    y += rowH + gap;

    if (UiButton(g, { row.x, y, row.width, row.height }, "Leave"))
    { SaveSession(g); GoToScreen(g, SCREEN_MENU); }
}

void DrawSettings(Game& g)
{
    const Layout& l = CurrentLayout();
    const float w = std::clamp(l.screenW * 0.5f, 360.0f, 640.0f);
    const float rowH = std::clamp(l.screenH * 0.08f, 40.0f, 64.0f);

    const float cx = l.screenW * 0.5f;
    const float totalH = rowH * 6.5f;
    const float startY = (l.screenH - totalH - rowH * 2.0f) * 0.5f;

    Heading(g, "Settings", cx, startY,
            std::clamp(l.screenH * 0.09f, 36.0f, 64.0f), 6.0f, ColorAlpha(COL_TEXT, 1.0f));

    GuiSetFont(g.font);
    const float x = cx - w * 0.5f;
    const float cw = w;
    float y = startY + rowH * 2.0f;

    PopOnHover(g, { x, y, cw, 22.0f }, 100);
    if (GuiSliderBar({ x, y, cw, 22.0f }, "Volume", "0 - 100", &g.settings.volume, 0.0f, 1.0f))
        sound::Pop();
    sound::SetMasterVolume(g.settings.volume);
    y += rowH * 1.35f;

    PopOnHover(g, { x, y, cw, 22.0f }, 101);
    if (GuiSliderBar({ x, y, cw, 22.0f }, "Memory", "less", &g.settings.memory, 0.25f, 1.25f))
        sound::Pop();
    y += rowH * 1.5f;

    PopOnHover(g, { x, y, cw, 24.0f }, 102);
    if (GuiCheckBox({ x, y, cw, 24.0f }, "Skip Intro After First Play", &g.settings.skipIntro))
        sound::Pop();
    y += rowH * 2.4f;

    const float bw = w * 0.44f;
    if (UiButton(g, { cx - bw * 0.5f, y, bw, rowH }, "Back"))
        GoToScreen(g, g.returnScreen);

}

void DrawCredits(Game& g)
{
    const Layout& l = CurrentLayout();
    const float t = std::min(1.0f, g.sceneTime / 1.6f);
    const float w = CardWidth(l) * 1.3f;
    const float baseSize = std::min(w * 0.047f, 18.0f);
    const float availableH = std::max(1.0f, l.screenH - l.margin * 2.0f);
    const float fitSize = std::max(6.0f,
        (availableH - l.margin * 3.8f - 20.0f) / (14.5f * 1.9f + 0.9f));
    const float size = std::min(baseSize, fitSize);
    const float gap = size * 1.9f;

    const float ebh = std::max(20.0f, gap * 0.9f);
    const float cardH = gap * 14.5f + l.margin * 1.8f + ebh;
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
    Label(g, "written, directed & sfx by kirbx01 (pyanc) · made with raylib", cx, y, size * 0.88f, 2.0f,
          ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;

    std::string music = "music: none found";
    if (sound::HasTrack()) music = "music: " + std::string(sound::TrackPath());
    Label(g, music.c_str(), cx, y, size * 0.88f, 2.0f, ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;

    Label(g, "type: Dosmic and Seratonin (personal use licence)", cx, y, size * 0.88f, 2.0f,
          ColorAlpha(COL_TEXT_FAINT, t));
    y += gap * 1.4f;

    const char* itchUrl = "https://kirbx01.itch.io";
    const char* githubUrl = "https://github.com/kirbx01";
    const float urlSize = size * 0.82f;
    const float itchWidth = SpacedTextWidth(g.font, itchUrl, urlSize, LINK_SPACING);
    const float githubWidth = SpacedTextWidth(g.font, githubUrl, urlSize, LINK_SPACING);
    const float linkGap = size * 0.8f;
    const float linkLeft = cx - (itchWidth + linkGap + githubWidth) * 0.5f;
    Label(g, "Check out the creator at", cx, y, size * 0.88f, 3.0f,
          ColorAlpha(COL_TEXT_FAINT, t));
    y += gap;
    DrawClickableLink(g, itchUrl, itchUrl, linkLeft, y, urlSize, true);
    DrawClickableLink(g, githubUrl, githubUrl, linkLeft + itchWidth + linkGap, y,
                      urlSize, true);

    const float by = card.y + card.height - l.margin * 0.9f - ebh;
    const float ebw = w * 0.40f;
    const float eby = by;
    const float egap = w * 0.04f;
    if (UiButton(g, { cx - ebw - egap * 0.5f, eby, ebw, ebh }, "Play Again"))
    { StartNewGame(g); return; }
    if (UiButton(g, { cx + egap * 0.5f, eby, ebw, ebh }, "Main Menu"))
    { GoToScreen(g, SCREEN_MENU); return; }

    if (t >= 1.0f)
    {
        const char* restartHint = "R - Play Again";
        const float restartWidth = SpacedTextWidth(g.font, restartHint, l.hintSize, 2.0f);
        DrawSpaced(g.font, restartHint, l.screenW - l.margin - restartWidth,
                   l.screenH - l.hintSize - l.margin * 0.35f, l.hintSize, 2.0f,
                   ColorAlpha(COL_TEXT_FAINT, 0.95f));
    }
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
        break;
    }

    static const char* const images[CUTSCENE_IMAGE_COUNT] =
    {
        "start_1.png", "start_2.png", "start_3.png",
        "end_1.png", "end_2.png", "end_3.png"
    };
    static const char* const directories[] =
    {
        "assets/animation pics/", "res/assets/animation pics/",
        "../assets/animation pics/", "../../assets/animation pics/"
    };

    for (int image = 0; image < CUTSCENE_IMAGE_COUNT; image++)
    {
        for (const char* directory : directories)
        {
            const std::string path = std::string(directory) + images[image];
            if (!FileExists(path.c_str())) continue;
            Texture texture = LoadTexture(path.c_str());
            if (texture.id == 0) continue;
            g.cutsceneTextures[image] = texture;
            break;
        }
    }
}

void UnloadBackground(Game& g)
{
    if (g.bgLoaded) UnloadTexture(g.bg);
    g.bg = {};
    g.bgLoaded = false;
    for (Texture& texture : g.cutsceneTextures)
    {
        if (texture.id != 0) UnloadTexture(texture);
        texture = {};
    }
}

bool UiTextControl(Game& g, Rectangle box, const char* label, float size, float spacing,
                  Color idleColor)
{
    const int slot = g_buttons++;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)) g_focusDelta++;
    if (IsKeyPressed(KEY_UP)) g_focusDelta--;

    const bool hover = CheckCollisionPointRec(GetMousePosition(), box);
    if (hover) g.focus = slot;

    const bool focused = (g.focus == slot);
    const bool lit = hover || focused;

    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                         CheckCollisionPointRec(GetMousePosition(), box);
    const bool fired = clicked || (focused && (IsKeyPressed(KEY_ENTER) ||
                                               IsKeyPressed(KEY_SPACE) ||
                                               IsKeyPressed(KEY_KP_ENTER)));

    DrawSpaced(g.font, label, box.x, box.y, size, spacing,
               lit ? HOVER_TEXT : idleColor);
    if (focused && g_focusArmed)
        DrawRectangleRec({ box.x, box.y + size * 1.18f, box.width, 1.0f },
                         ColorAlpha(HOVER_TEXT, 0.9f));

    if (fired)
    {
        sound::Pop();
        g.focus = slot;
    }

    return fired;
}

void DrawHudControls(Game& g)
{
    const Layout& l = CurrentLayout();
    g_buttons = 0;
    g_focusDelta = 0;

    const float size    = l.hintSize;
    const float spacing = l.hintSpacing;
    const float y       = l.margin * 0.5f;

    const float remaining = std::max(0.0f, PASSAGE_TIME_LIMIT - g.stageTime);
    char timer[32];
    std::snprintf(timer, sizeof(timer), "time %.1fs", remaining);
    const Color timerColor = remaining <= 5.0f ? HOVER_TEXT : COL_TEXT;
    DrawSpaced(g.font, timer, l.margin, y, size, spacing, timerColor);

    const char* soundLabel = g.settings.muted ? "sounds off" : "sounds";
    const char* setLabel   = "settings";
    const char* helpLabel  = "help";

    const float soundW = SpacedTextWidth(g.font, soundLabel, size, spacing);
    const float setW   = SpacedTextWidth(g.font, setLabel,   size, spacing);
    const float helpW  = SpacedTextWidth(g.font, helpLabel,  size, spacing);

    const Rectangle settingsBox =
    {
        l.screenW - l.margin - setW,
        y,
        setW,
        size
    };
    const Rectangle soundBox =
    {
        settingsBox.x - size * 1.6f - soundW,
        y,
        soundW,
        size
    };
    const Rectangle helpBox =
    {
        soundBox.x - size * 1.6f - helpW,
        y,
        helpW,
        size
    };

    if (UiTextControl(g, helpBox, helpLabel, size, spacing))
    {
        g.returnScreen = SCREEN_PLAYING;
        g.helpPage = 0;
        g.focus = 1;
        g.helpInputLock = true;
        GoToScreen(g, SCREEN_HELP);
    }

    if (UiTextControl(g, soundBox, soundLabel, size, spacing,
                      g.settings.muted ? HOVER_TEXT : COL_TEXT))
    {
        g.settings.muted = !g.settings.muted;
        sound::SetMuted(g.settings.muted);
    }

    if (UiTextControl(g, settingsBox, setLabel, size, spacing))
    {
        g.returnScreen = SCREEN_PLAYING;
        GoToScreen(g, SCREEN_SETTINGS);
    }

    if (g_buttons > 0)
    {
        if (g_focusDelta != 0) g_focusArmed = true;
        g.focus = std::clamp(g.focus + g_focusDelta, 0, g_buttons - 1);
        if (g.focus == 0 && g_focusDelta < 0) g.focus = g_buttons - 1;
    }
}

void DrawHelp(Game& g)
{
    const Layout& l = CurrentLayout();
    static const char* const pages[] =
    {
        "Guide the ball through the passages.",
        "Domino halves show the pips they accept.",
        "Match one half; your pip changes to its partner.",
        "A wrong pip bounces back and costs two seconds.",
        "R restarts this passage. Escape pauses."
    };
    constexpr int PAGE_COUNT = sizeof(pages) / sizeof(pages[0]);

    DrawRectangle(0, 0, (int)l.screenW, (int)l.screenH, ColorAlpha(COL_BG_DEEP, 225));
    const float cx = l.screenW * 0.5f;
    const float titleSize = std::clamp(l.screenH * 0.08f, 32.0f, 52.0f);
    Heading(g, "Help", cx, l.screenH * 0.30f, titleSize, 5.0f, COL_TEXT);

    char progress[32];
    std::snprintf(progress, sizeof(progress), "%d / %d", g.helpPage + 1, PAGE_COUNT);
    Label(g, progress, cx, l.screenH * 0.42f, l.hintSize, l.hintSpacing, COL_TEXT_FAINT);
    Label(g, pages[g.helpPage], cx, l.screenH * 0.48f, l.storySize, l.storySpacing, COL_TEXT);

    if (g.helpPage == 0)
        Label(g, "wasd / arrows", cx, l.screenH * 0.55f, l.storySize, l.storySpacing,
              HOVER_TEXT);
    else if (g.helpPage == 2)
        Label(g, "1  >  [ 0 | 1 ]  >  0", cx, l.screenH * 0.55f, l.storySize,
              l.storySpacing, HOVER_TEXT);

    const char* prompt = g.helpPage == 0 ? "press a direction" :
                         g.helpPage == 4 ? "press r or resume" : "press enter to continue";
    Label(g, prompt, cx, l.screenH * 0.59f, l.hintSize, l.hintSpacing, COL_TEXT_FAINT);

    if (g.helpInputLock) return;

    const float buttonW = std::clamp(l.screenW * 0.15f, 110.0f, 190.0f);
    const float buttonH = std::clamp(l.screenH * 0.065f, 42.0f, 58.0f);
    const float gap = l.margin;
    const float y = l.screenH * 0.63f;
    const float totalW = buttonW * 3.0f + gap * 2.0f;
    const float left = (l.screenW - totalW) * 0.5f;

    if (UiButton(g, { left, y, buttonW, buttonH }, "Back"))
        g.helpPage = (g.helpPage + PAGE_COUNT - 1) % PAGE_COUNT;
    if (UiButton(g, { left + buttonW + gap, y, buttonW, buttonH }, "Next"))
        g.helpPage = (g.helpPage + 1) % PAGE_COUNT;
    if (UiButton(g, { left + (buttonW + gap) * 2.0f, y, buttonW, buttonH }, "Resume"))
        GoToScreen(g, g.returnScreen);
}

void DrawScreenUi(Game& g)
{
    g_buttons = 0;
    g_focusDelta = 0;
    g_focusArmed = false;
    g_currentHoveredControl = -1;

    switch (g.screen)
    {
        case SCREEN_MENU:     DrawMenu(g);     break;
        case SCREEN_PAUSE:    DrawPause(g);    break;
        case SCREEN_SETTINGS: DrawSettings(g); break;
        case SCREEN_CREDITS:  DrawCredits(g);  break;
        case SCREEN_HELP:     DrawHelp(g);     break;
        default: break;
    }

    if (g_buttons > 0)
    {
        g.focus = std::clamp(g.focus + g_focusDelta, 0, g_buttons - 1);
        if (g.focus == 0 && g_focusDelta < 0) g.focus = g_buttons - 1;
    }

    g_hoveredControl = g_currentHoveredControl;
}

}