#pragma once

// Raygui's implementation lives in exactly one translation unit (ui.cpp) so the
// RAYGUI_IMPLEMENTATION macro is never defined twice in a link.

#include "game.h"

#include <string>

namespace witness {

// Reads the itch.io address from assets/itch_url.txt, falling back to the value baked in
// with -DATF_ITCH_URL=. Returns an empty string when neither is set.
std::string ResolveItchUrl();

Font LoadUiFont(bool& loaded, int& size);
void StyleUi(Font font);

bool LoadBackground(Game& g);
void UnloadBackground(Game& g);
void DrawBackground(const Game& g);

// Menus and credits. Only these screens use Raygui or panels; the play space stays
// completely empty.
void DrawScreenUi(Game& g);
void DrawMenu(Game& g);
void DrawPause(Game& g);
void DrawSettings(Game& g);
void DrawCredits(Game& g);

// A genuinely clickable link: hit tested against the text bounds, hover feedback,
// and activatable from the keyboard. Returns true on the frame it was activated.
bool DrawClickableLink(Game& g, const char* text, float x, float y, float size,
                       bool underlined);

} // namespace witness
