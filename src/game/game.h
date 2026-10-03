#pragma once

#include "raylib.h"

namespace witness {

constexpr int SCREEN_W   = 1280;
constexpr int SCREEN_H   = 720;
constexpr int TARGET_FPS = 60;
constexpr int UI_FONT_SIZE = 28;

constexpr const char* BG_IMAGE = "assets/bg.png";

enum Screen
{
    SCREEN_MENU = 0,
    SCREEN_PLAYING,
    SCREEN_ENDING,
    SCREEN_SETTINGS
};

const Color COL_BG        = { 12, 12, 15, 255 };
const Color COL_BG_DEEP   = {  7,  7,  9, 255 };
const Color COL_PANEL     = { 18, 18, 23, 235 };
const Color COL_EDGE      = { 58, 58, 70, 255 };
const Color COL_TEXT      = { 236, 236, 242, 255 };
const Color COL_TEXT_DIM  = { 148, 148, 162, 255 };
const Color COL_TEXT_FAINT= { 92, 92, 104, 255 };
const Color COL_ACCENT    = { 96, 176, 255, 255 };
const Color COL_SCRIM     = {  0,  0,  0, 160 };

struct Game
{
    Screen screen = SCREEN_MENU;

    bool  showBg    = true;
    bool  bgLoaded  = false;
    Texture2D bg    = {};

    Font  font      = {};
    bool  fontLoaded = false;

    float elapsed   = 0.0f;
};

void GoToScreen(Game& g, Screen screen);
void UpdateGame(Game& g, float dt);

void HandleInput(Game& g);

Font LoadUiFont(bool& loaded, int& size);
void StyleUi(Font font);
bool LoadBackground(Game& g);
void UnloadBackground(Game& g);
void DrawBackground(const Game& g);
void DrawMenu(Game& g);
void DrawPlaying(const Game& g);
void DrawEnding(Game& g);
void DrawSettings(Game& g);
void DrawFrame(Game& g);

}