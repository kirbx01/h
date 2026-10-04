#include "sound.h"

#include "game.h"

#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace witness {
namespace sound {

namespace {

constexpr float FADE_IN  = 2.5f;
constexpr float FADE_OUT = 1.5f;
constexpr float CUTSCENE_FADE = 0.8f;
constexpr float POP_GAIN = 0.5f;

bool g_device = false;
Music g_music;
Music g_cutsceneMusic;
Sound g_pop;
char  g_musicPath[256] = { 0 };
char  g_cutscenePath[256] = { 0 };
char  g_popPath[256]   = { 0 };
float g_volume = 1.0f;
bool  g_muted = false;
float g_fade   = 0.0f;
float g_fadeTarget = 1.0f;
float g_cutsceneLevel = 0.0f;
float g_cutsceneTarget = 0.0f;

bool TryOpen(Music& out, char* log, const char* const* stems, std::size_t count,
             const char* override)
{
    auto attempt = [&](const std::string& path)
    {
        Music m = LoadMusicStream(path.c_str());
        if (m.frameCount == 0) return false;
        out = m;
        std::snprintf(log, 256, "%s", path.c_str());
        return true;
    };

    if (override && *override) return attempt(override);

    static const char* dirs[] = { AUDIO_DIR, "res/assets/", "../assets/", "../../assets/" };
    for (const char* dir : dirs)
    {
        for (std::size_t s = 0; s < count; s++)
        {
            for (std::size_t e = 0; e < sizeof(AUDIO_EXTS) / sizeof(AUDIO_EXTS[0]); e++)
            {
                if (attempt(std::string(dir) + stems[s] + AUDIO_EXTS[e])) return true;
            }
        }
    }
    return false;
}

bool TryOpen(Sound& out, char* log, const char* const* stems, std::size_t count)
{
    auto attempt = [&](const std::string& path)
    {
        Sound s = LoadSound(path.c_str());
        if (s.frameCount == 0) return false;
        out = s;
        std::snprintf(log, 256, "%s", path.c_str());
        return true;
    };

    static const char* dirs[] = { AUDIO_DIR, "res/assets/", "../assets/", "../../assets/" };
    for (const char* dir : dirs)
    {
        for (std::size_t s = 0; s < count; s++)
        {
            for (std::size_t e = 0; e < sizeof(AUDIO_EXTS) / sizeof(AUDIO_EXTS[0]); e++)
                if (attempt(std::string(dir) + stems[s] + AUDIO_EXTS[e])) return true;
        }
    }
    return false;
}

}

void Init()
{
    g_device = true;
    InitAudioDevice();
    g_device = IsAudioDeviceReady();
    if (!g_device)
    {
        TraceLog(LOG_WARNING, "i forgor: no audio device, running in silence");
        return;
    }

    if (TryOpen(g_music, g_musicPath, AUDIO_STEMS, AUDIO_STEM_COUNT, std::getenv("IFG_AUDIO")))
    {
        g_music.looping = true;
        PlayMusicStream(g_music);
        TraceLog(LOG_WARNING, "i forgor: music '%s'", g_musicPath);
    }
    else
    {
        TraceLog(LOG_WARNING, "i forgor: no music found in %s", AUDIO_DIR);
    }

    if (TryOpen(g_pop, g_popPath, POP_STEMS, POP_STEM_COUNT))
        TraceLog(LOG_WARNING, "i forgor: click '%s'", g_popPath);
}

void Update(float dt)
{
    if (!g_device) return;

    if (g_fade < g_fadeTarget)
        g_fade = FADE_IN > 0.0f ? std::min(g_fadeTarget, g_fade + dt / FADE_IN) : g_fadeTarget;
    else if (g_fade > g_fadeTarget)
        g_fade = FADE_OUT > 0.0f ? std::max(g_fadeTarget, g_fade - dt / FADE_OUT) : g_fadeTarget;

    if (g_cutsceneLevel < g_cutsceneTarget)
        g_cutsceneLevel = std::min(g_cutsceneTarget, g_cutsceneLevel + dt / CUTSCENE_FADE);
    else if (g_cutsceneLevel > g_cutsceneTarget)
        g_cutsceneLevel = std::max(g_cutsceneTarget, g_cutsceneLevel - dt / CUTSCENE_FADE);

    const float vol   = g_muted ? 0.0f : std::clamp(g_volume, 0.0f, 1.0f);
    const float level = vol * g_fade * (1.0f - g_cutsceneLevel);
    if (IsMusicValid(g_music))
    {
        UpdateMusicStream(g_music);
        SetMusicVolume(g_music, level);
    }
    if (IsMusicValid(g_cutsceneMusic))
    {
        UpdateMusicStream(g_cutsceneMusic);
        SetMusicVolume(g_cutsceneMusic, vol * g_cutsceneLevel);
        if (g_cutsceneLevel <= 0.0f && g_cutsceneTarget <= 0.0f)
        {
            StopMusicStream(g_cutsceneMusic);
            UnloadMusicStream(g_cutsceneMusic);
            g_cutsceneMusic = {};
            g_cutscenePath[0] = '\0';
        }
    }
    if (IsSoundValid(g_pop))   SetSoundVolume(g_pop, POP_GAIN * vol);
}

void FadeIn() { g_fadeTarget = 1.0f; }

void FadeOut() { g_fadeTarget = 0.0f; }

void PlayCutscene(bool ending)
{
    if (!g_device) return;

    if (IsMusicValid(g_cutsceneMusic)) UnloadMusicStream(g_cutsceneMusic);
    g_cutsceneMusic = {};
    g_cutscenePath[0] = '\0';
    g_cutsceneLevel = 0.0f;
    g_cutsceneTarget = 1.0f;

    if (ending) g_fadeTarget = 0.0f;
    else        FadeIn();

    const char* stems[] = { ending ? "closingcutscene" : "openingcutscene" };
    if (!TryOpen(g_cutsceneMusic, g_cutscenePath, stems, 1, nullptr))
    {
        g_cutsceneTarget = 0.0f;
        TraceLog(LOG_WARNING, "i forgor: no %s cutscene music found", ending ? "closing" : "opening");
        return;
    }

    g_cutsceneMusic.looping = false;
    SetMusicVolume(g_cutsceneMusic, 0.0f);
    PlayMusicStream(g_cutsceneMusic);
    TraceLog(LOG_INFO, "i forgor: cutscene music '%s'", g_cutscenePath);
}

void StopCutscene(bool resumeMain)
{
    g_cutsceneTarget = 0.0f;
    if (resumeMain) FadeIn();
}

void SetMasterVolume(float v) { g_volume = v; }

void SetMuted(bool m) { g_muted = m; }

bool IsMuted() { return g_muted; }

void NotifyUserGesture()
{
    if (!g_device) return;
    if (IsMusicValid(g_music) && !IsMusicStreamPlaying(g_music)) ResumeMusicStream(g_music);
    if (IsMusicValid(g_cutsceneMusic) && !IsMusicStreamPlaying(g_cutsceneMusic))
        ResumeMusicStream(g_cutsceneMusic);
}

void Pop()
{
    if (!g_device || !IsSoundValid(g_pop)) return;
    SetSoundVolume(g_pop, POP_GAIN * std::clamp(g_volume, 0.0f, 1.0f));
    PlaySound(g_pop);
}

void Shutdown()
{
    if (!g_device) return;

    if (IsMusicValid(g_music) && IsMusicStreamPlaying(g_music))
    {
        const float start = std::clamp(g_volume * g_fade, 0.0f, 1.0f);
        for (int i = 0; i < 30; i++)
        {
            const float k = 1.0f - (float)i / 30.0f;
            SetMusicVolume(g_music, start * k);
            WaitTime(1000.0f * FADE_OUT / 30.0f);
        }
        SetMusicVolume(g_music, 0.0f);
    }

    if (IsMusicValid(g_music)) UnloadMusicStream(g_music);
    if (IsMusicValid(g_cutsceneMusic)) UnloadMusicStream(g_cutsceneMusic);
    if (IsSoundValid(g_pop))   UnloadSound(g_pop);
    CloseAudioDevice();
    g_device = false;
}

bool HasTrack() { return IsMusicValid(g_music); }
bool HasPop()   { return IsSoundValid(g_pop); }
const char* TrackPath() { return g_musicPath; }

}
}