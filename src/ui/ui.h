#pragma once

#include "game.h"

#include <string>

namespace witness {

namespace sound {

void Init();
void Update(float dt);
void SetMasterVolume(float v);
void NotifyUserGesture();
void Pop();
void Shutdown();

bool HasTrack();
bool HasPop();
const char* TrackPath();

}

std::string ResolveItchUrl();

void LoadFonts(Game& g);
void UnloadFonts(Game& g);
void LoadBackground(Game& g);
void UnloadBackground(Game& g);
void DrawHudControls(Game& g);
void StyleUi();

void DrawScreenUi(Game& g);
void DrawMenu(Game& g);
void DrawPause(Game& g);
void DrawSettings(Game& g);
void DrawCredits(Game& g);

bool UiButton(Game& g, Rectangle box, const char* label);
bool UiTextControl(Game& g, Rectangle box, const char* label, float size, float spacing,
                   Color idleColor = COL_TEXT_FAINT);
bool DrawClickableLink(Game& g, const char* text, const char* url, float x, float y,
                       float size, bool underlined);

}