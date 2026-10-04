#include "platform.h"

#if defined(PLATFORM_WEB)
#define IFG_WEB 1
#include <emscripten/emscripten.h>
#else
#define IFG_WEB 0
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif
#endif

namespace witness {
namespace platform {

namespace {

constexpr const char* kAppFolder  = "i_forgor";
constexpr const char* kStateName  = "session.v1";
constexpr const char* kStorageKey = "i_forgor.session.v1";

constexpr const char* kLegacyAppFolder  = "after_the_fall";
constexpr const char* kLegacyStorageKey = "after_the_fall.session.v1";

#if !IFG_WEB

std::filesystem::path StatePath(const char* appFolder)
{

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

    return base / appFolder / kStateName;
}

bool ReadFile(const std::filesystem::path& path, std::string& out)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;

    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f) return false;

    out.assign(4096, '\0');
    const size_t got = std::fread(&out[0], 1, 4095, f);
    std::fclose(f);
    out.resize(got);
    return got > 0;
}

#endif

#if IFG_WEB

EM_JS(int, IfgLocalStorageGet, (const char* key, char* out, int cap), {
    try {
        var value = window.localStorage.getItem(UTF8ToString(key));
        if (value === null) return 0;
        stringToUTF8(value, out, cap);
        return 1;
    } catch (e) {
        return 0;
    }
});

EM_JS(void, IfgLocalStorageSet, (const char* key, const char* value), {
    try {
        window.localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
    } catch (e) {

    }
});

EM_JS(void, IfgLocalStorageRemove, (const char* key), {
    try {
        window.localStorage.removeItem(UTF8ToString(key));
    } catch (e) {
    }
});

EM_JS(void, IfgOpenUrl, (const char* url), {
    var target = UTF8ToString(url);
    var opened = window.open(target, '_blank', 'noopener');
    if (!opened) window.location.href = target;
});

bool WebGet(const char* key, char* out, int cap)
{
    return IfgLocalStorageGet(key, out, cap) != 0;
}

void WebSet(const char* key, const char* value)
{
    IfgLocalStorageSet(key, value);
}

void WebRemove(const char* key)
{
    IfgLocalStorageRemove(key);
}

#endif

}

bool IsWeb()
{
#if IFG_WEB
    return true;
#else
    return false;
#endif
}

void Init()
{

}

void Shutdown()
{
}

bool ReadState(std::string& out)
{
#if IFG_WEB
    out.assign(4096, '\0');
    if (WebGet(kStorageKey, &out[0], 4096))
    {
        out.resize(std::char_traits<char>::length(out.c_str()));
        return !out.empty();
    }

    out.assign(4096, '\0');
    if (!WebGet(kLegacyStorageKey, &out[0], 4096)) return false;
    out.resize(std::char_traits<char>::length(out.c_str()));
    return !out.empty();
#else
    if (ReadFile(StatePath(kAppFolder), out)) return true;
    return ReadFile(StatePath(kLegacyAppFolder), out);
#endif
}

bool WriteState(const std::string& data)
{
#if IFG_WEB
    if (data.empty()) return false;
    WebSet(kStorageKey, data.c_str());
    return true;
#else
    const std::filesystem::path path = StatePath(kAppFolder);

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
#if IFG_WEB
    WebRemove(kStorageKey);
#else
    std::error_code ec;
    std::filesystem::remove(StatePath(kAppFolder), ec);
#endif
}

void OpenUrl(const char* url)
{
    if (!url || !*url) return;

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

#if IFG_WEB
    IfgOpenUrl(url);
#elif defined(_WIN32)
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    const std::string cmd = std::string("open '") + url + "'";
    std::system(cmd.c_str());
#else

    const std::string cmd = std::string("xdg-open '") + url + "' 2>/dev/null || gio open '" + url +
                            "' 2>/dev/null || sensible-browser '" + url + "' >/dev/null 2>&1";
    std::system(cmd.c_str());
#endif
}

}
}
