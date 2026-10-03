#include "platform.h"

#if defined(PLATFORM_WEB)
#define ATF_WEB 1
#include <emscripten/emscripten.h>
#else
#define ATF_WEB 0
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#endif
#endif

namespace witness {
namespace platform {

namespace {

constexpr const char* kAppFolder  = "after_the_fall";
constexpr const char* kStateName  = "session.v1";
constexpr const char* kStorageKey = "after_the_fall.session.v1";

#if !ATF_WEB

// Per-OS location for a tiny save file. Everything falls back to a temp dir so the
// game still runs on locked-down machines (where it just forgets between runs).
std::filesystem::path StatePath()
{
    // Escape hatch for testing: SAVE_DIR=... redirects the save file somewhere harmless.
    if (const char* forced = getenv("SAVE_DIR"))
    {
        if (*forced) return std::filesystem::path(forced) / kStateName;
    }

    std::filesystem::path base;

#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata && *appdata) base = std::filesystem::path(appdata);
    else base = std::filesystem::temp_directory_path();
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home && *home) base = std::filesystem::path(home) / "Library" / "Application Support";
    else base = std::filesystem::temp_directory_path();
#else
    const char* xdg  = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && *xdg) base = std::filesystem::path(xdg);
    else if (home && *home) base = std::filesystem::path(home) / ".local" / "share";
    else base = std::filesystem::temp_directory_path();
#endif

    return base / kAppFolder / kStateName;
}

#endif // !ATF_WEB

#if ATF_WEB

bool WebGet(const char* key, char* out, int cap)
{
    return EM_ASM_INT({
        const char* k = UTF8ToString($0);
        try {
            const v = window.localStorage.getItem(k);
            if (v === null) return 0;
            stringToUTF8(v, HEAPU8, $2, $1);
            return 1;
        } catch (e) { return 0; }
    }, key, cap, out) != 0;
}

void WebSet(const char* key, const char* value)
{
    EM_ASM({
        try { window.localStorage.setItem(UTF8ToString($0), UTF8ToString($1)); }
        catch (e) { /* private mode / quota: the game simply will not remember */ }
    }, key, value);
}

void WebRemove(const char* key)
{
    EM_ASM({
        try { window.localStorage.removeItem(UTF8ToString($0)); } catch (e) {}
    }, key);
}

#endif // ATF_WEB

} // namespace

bool IsWeb()
{
#if ATF_WEB
    return true;
#else
    return false;
#endif
}

void Init()
{
    // Nothing to set up: both backends are stateless files/localStorage.
}

void Shutdown()
{
}

bool ReadState(std::string& out)
{
#if ATF_WEB
    out.assign(4096, '\0');
    if (!WebGet(kStorageKey, &out[0], 4096)) return false;
    out.resize(std::char_traits<char>::length(out.c_str()));
    return !out.empty();
#else
    std::error_code ec;
    const std::filesystem::path path = StatePath();
    if (!std::filesystem::exists(path, ec)) return false;

    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f) return false;

    out.assign(4096, '\0');
    const size_t got = std::fread(&out[0], 1, 4095, f);
    std::fclose(f);
    out.resize(got);
    return got > 0;
#endif
}

bool WriteState(const std::string& data)
{
#if ATF_WEB
    if (data.empty()) return false;
    WebSet(kStorageKey, data.c_str());
    return true;
#else
    const std::filesystem::path path = StatePath();

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;

    const size_t put = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return put == data.size();
#endif
}

void ClearState()
{
#if ATF_WEB
    WebRemove(kStorageKey);
#else
    std::error_code ec;
    std::filesystem::remove(StatePath(), ec);
#endif
}

void OpenUrl(const char* url)
{
    if (!url || !*url) return;

    // Only accept a plain web address. This doubles as shell-injection protection
    // because the value ends up on a command line on some platforms.
    const char* prefixes[] = { "https://", "http://" };
    bool ok = false;
    for (const char* p : prefixes)
    {
        size_t n = 0;
        while (p[n]) n++;
        size_t i = 0;
        while (url[i] && i < n && url[i] == p[i]) i++;
        if (i == n) ok = true;
    }
    if (!ok) return;

    for (const char* p = url; *p; p++)
    {
        const unsigned char c = (unsigned char)*p;
        const bool safe = std::isalnum(c) || std::strchr(":/?#[]@!$&()*+,;=%._~-", c) != nullptr;
        if (!safe) return;
    }

#if ATF_WEB
    EM_ASM({
        const u = UTF8ToString($0);
        const w = window.open(u, '_blank', 'noopener');
        if (!w) window.location.href = u;
    }, url);
#elif defined(_WIN32)
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    const std::string cmd = std::string("open '") + url + "'";
    std::system(cmd.c_str());
#else
    // xdg-open is the standard, but fall back to gio / sensible-browser on
    // desktops that ship a different portal setup.
    const std::string cmd = std::string("xdg-open '") + url + "' 2>/dev/null || gio open '" + url +
                            "' 2>/dev/null || sensible-browser '" + url + "' >/dev/null 2>&1";
    std::system(cmd.c_str());
#endif
}

} // namespace platform
} // namespace witness
