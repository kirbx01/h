#pragma once

#include <string>

// Thin platform layer. Everything the game needs from the host OS lives here:
//  * a tiny key/value store for the session save (a file on desktop, localStorage on web)
//  * opening a URL in the user's browser
// Keeping this behind one header is what lets the same sources build for Linux,
// Windows and the web without desktop-only APIs leaking into game code.
namespace witness {
namespace platform {

void Init();
void Shutdown();

// Session persistence. Read returns false when nothing has been stored yet.
bool ReadState(std::string& out);
bool WriteState(const std::string& data);
void ClearState();

// Opens url with the OS default handler. Anything that is not plain http(s) is ignored,
// both to avoid feeding junk to a shell and because a bad build-time URL should be inert.
void OpenUrl(const char* url);

// True when building for the browser (emscripten). Web needs a user gesture before
// audio can start, so the rest of the game asks IsWeb() instead of guessing.
bool IsWeb();

} // namespace platform
} // namespace witness
