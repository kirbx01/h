#include "sound.h"

#include "game.h"
#include "raylib.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace witness {
namespace sound {

namespace {

//------------------------------------------------------------------------------------
// Audio-thread parameters.
//
// These are written by the main thread and read by raylib's audio callback, so they
// are atomics: the callback never blocks and never allocates. Everything else the
// callback touches is plain statics that only the audio thread ever writes.
//
// Only the passage track is processed. The framing track is the one piece of music in
// the game that stays intact, which is the whole point of using it for the opening, the
// ending and the settings.
//------------------------------------------------------------------------------------
std::atomic<float> g_cutoff{ 14000.0f };   // low-pass corner, Hz
std::atomic<float> g_gate{ 1.0f };        // dropout gate target (0 = silence)
std::atomic<int>   g_sampleRate{ 44100 };
std::atomic<int>   g_channels{ 2 };

// Callback-side filter state. One-pole per channel, plus the gate ramp.
float g_z[8]        = { 0 };
float g_gateCurrent = 1.0f;

// Main-thread state.
bool  g_deviceReady = false;
bool  g_wantsGesture = false;
bool  g_warnedPassage = false;
bool  g_warnedFrame   = false;

float g_masterVolume   = 1.0f;
float g_stageVolume    = 0.9f;
float g_stageCutoff    = 14000.0f;
float g_fadeCurrent    = 0.0f;
float g_fadeTarget     = 1.0f;   // 0 = silent, 1 = fully up
float g_fadeSpeed      = 1.2f;   // units per second
float g_dropoutEvery   = 0.0f;
float g_dropoutLen     = 0.0f;
float g_dropoutTimer   = 0.0f;
float g_dropoutLeft    = 0.0f;

// A dropout window, held open for g_dropoutLeft seconds.
void OpenDropout()
{
    if (g_dropoutLen <= 0.0f) return;
    g_dropoutLeft  = g_dropoutLen;
    g_gate.store(0.0f, std::memory_order_relaxed);
}

//------------------------------------------------------------------------------------
// The processor. Runs on raylib's audio thread with interleaved 32-bit float frames.
//
//   1. one-pole low-pass per channel -> the track genuinely loses its top end
//   2. smoothed gate                 -> click-free dropouts
//
// Level is not handled here: each track's volume is set on its Music handle, so a
// crossfade between two tracks does not fight with the audio thread.
//
// No allocation, no locks, no file access, no raylib drawing calls: it can only touch
// its own statics and the atomics above.
//------------------------------------------------------------------------------------
void AudioProcessor(void* bufferData, unsigned int frames)
{
    float* samples = static_cast<float*>(bufferData);
    if (!samples || frames == 0) return;

    const int channels = std::max(1, g_channels.load(std::memory_order_relaxed));
    const float sr     = static_cast<float>(std::max(8000, g_sampleRate.load(std::memory_order_relaxed)));

    // One-pole coefficient: alpha = 1 - exp(-2*pi*fc/sr).
    const float cutoff = std::clamp(g_cutoff.load(std::memory_order_relaxed), 40.0f, sr * 0.45f);
    const float alpha  = 1.0f - std::exp(-2.0f * PI * cutoff / sr);

    const float gateTo = g_gate.load(std::memory_order_relaxed);
    // ~25 ms gate ramp, computed once per block and applied per sample.
    const float gateK  = 1.0f - std::exp(-1.0f / (0.025f * sr));

    const int tracked = std::min(channels, 8);
    const float total = frames * static_cast<unsigned int>(channels);

    for (unsigned int i = 0; i < total; i++)
    {
        const int c = static_cast<int>(i % static_cast<unsigned int>(channels));

        float s = samples[i];
        if (c < tracked) s = g_z[c] += alpha * (s - g_z[c]);
        else             s = 0.0f;   // surplus channels are muted rather than passed dry

        g_gateCurrent += (gateTo - g_gateCurrent) * gateK;
        samples[i] = s * g_gateCurrent;
    }
}

//------------------------------------------------------------------------------------
// One track and where it currently is in its fade.
//------------------------------------------------------------------------------------
struct Slot
{
    Music      music = {};
    std::string path;

    bool  loaded      = false;
    bool  playing     = false;
    bool  processed   = false;
    float gain        = 1.0f;   // extra level for this track (0..1)

    float fade       = 0.0f;
    float fadeTarget = 0.0f;
    float fadeSpeed  = 1.4f;
};

// Filenames are probed rather than hardcoded to one spelling: the assets keep the names
// they were supplied with, and the usual extensions are tried in turn. ATF_AUDIO and
// ATF_AUDIO_FRAME override everything, which is handy for local builds.
bool TryLoadSlot(Slot& slot, const char* const* stems, std::size_t stemCount, const char* env)
{
    if (slot.loaded || !g_deviceReady) return slot.loaded;

    std::vector<std::string> candidates;

    if (env && *env) candidates.emplace_back(env);

    for (std::size_t i = 0; i < stemCount; i++)
    {
        for (const char* ext : AUDIO_EXTS)
        {
            candidates.push_back(std::string(AUDIO_DIR) + stems[i] + ext);
        }
    }

    for (const std::string& path : candidates)
    {
        if (!FileExists(path.c_str())) continue;

        Music music = LoadMusicStream(path.c_str());
        if (!IsMusicValid(music)) continue;

        slot.music  = music;
        slot.path   = path;
        slot.loaded = true;

        TraceLog(LOG_INFO, "AFTER THE FALL: audio track '%s' (%d Hz, %d ch)",
                 path.c_str(), music.stream.sampleRate, music.stream.channels);
        return true;
    }

    return false;
}

Slot g_passage;   // assets/baseorignal.wav
Slot g_frame;     // assets/openingandclosing.wav

Slot* g_active = nullptr;   // the track fading up
Slot* g_leaving = nullptr;  // the track fading down, if any

// The framing track sits under everything: it is not the point of a menu.
constexpr float FRAME_GAIN = 0.62f;

constexpr float FADE_IN_SECONDS  = 0.8f;
constexpr float FADE_OUT_SECONDS = 0.55f;

// Which track a screen belongs to. A pause holds whatever was already playing: the
// player stepped out of a passage for a moment, not into a different piece of music.
Slot* TrackForScreen(Screen screen)
{
    switch (screen)
    {
        case SCREEN_PLAYING:
        case SCREEN_CLEAR:
            return &g_passage;

        case SCREEN_INTRO:
        case SCREEN_MENU:
        case SCREEN_SETTINGS:
        case SCREEN_CREDITS:
        case SCREEN_ENDING:
            return &g_frame;

        default:
            return nullptr;
    }
}

void StartSlot(Slot& slot)
{
    if (!slot.loaded || slot.playing) return;
    PlayMusicStream(slot.music);
    slot.playing = true;
}

void StopSlot(Slot& slot)
{
    if (!slot.playing) return;
    StopMusicStream(slot.music);
    slot.playing = false;
}

void StartDevice()
{
    if (g_deviceReady) return;

    InitAudioDevice();
    if (!IsAudioDeviceReady())
    {
        TraceLog(LOG_WARNING, "AFTER THE FALL: no audio device available, continuing in silence");
        return;
    }

    g_deviceReady = true;

    if (TryLoadSlot(g_passage, AUDIO_PASSAGE_STEMS, AUDIO_PASSAGE_STEM_COUNT, std::getenv("ATF_AUDIO")))
    {
        g_sampleRate.store(static_cast<int>(g_passage.music.stream.sampleRate), std::memory_order_relaxed);
        g_channels.store(static_cast<int>(g_passage.music.stream.channels), std::memory_order_relaxed);

        // Music wraps a normal AudioStream, so the processor attaches to that stream. This
        // is what makes the deterioration audible rather than merely quiet: the track
        // genuinely loses its top end as the world falls apart. If a platform ever refuses
        // the processor, level and dropouts still work on their own.
        AttachAudioStreamProcessor(g_passage.music.stream, AudioProcessor);
        g_passage.processed = true;
    }
    else if (!g_warnedPassage)
    {
        g_warnedPassage = true;
        TraceLog(LOG_WARNING,
                 "AFTER THE FALL: no passage track found. Put the music at '%sbaseorignal.wav' "
                 "(ogg/wav/mp3/qoa/xm/mod) or set ATF_AUDIO. The game plays on without it.",
                 AUDIO_DIR);
    }

    if (TryLoadSlot(g_frame, AUDIO_FRAME_STEMS, AUDIO_FRAME_STEM_COUNT, std::getenv("ATF_AUDIO_FRAME")))
    {
        g_frame.gain = FRAME_GAIN;
    }
    else if (!g_warnedFrame)
    {
        g_warnedFrame = true;
        TraceLog(LOG_WARNING,
                 "AFTER THE FALL: no opening/closing track found. Put it at "
                 "'%sopeningandclosing.wav' or set ATF_AUDIO_FRAME. The game plays on without it.",
                 AUDIO_DIR);
    }
}

// Moves one slot's fade and keeps the stream state and level in step with it.
void AdvanceSlot(Slot& slot, float dt, float level)
{
    if (!slot.loaded) return;

    if (slot.fadeTarget > slot.fade)      slot.fade = std::min(slot.fadeTarget, slot.fade + slot.fadeSpeed * dt);
    else if (slot.fadeTarget < slot.fade) slot.fade = std::max(slot.fadeTarget, slot.fade - slot.fadeSpeed * dt);

    if (slot.fade > 0.001f) StartSlot(slot);
    else                     StopSlot(slot);

    if (slot.playing)
    {
        // raylib 5.5 refills a Music stream from the calling thread, so this update is not
        // optional. The stream loops by itself; restarting is just belt and braces.
        UpdateMusicStream(slot.music);
        if (!IsMusicStreamPlaying(slot.music)) PlayMusicStream(slot.music);
    }

    SetMusicVolume(slot.music, std::clamp(level * slot.gain, 0.0f, 1.0f));
}

void UnloadSlot(Slot& slot)
{
    if (!slot.loaded) return;

    if (slot.processed) DetachAudioStreamProcessor(slot.music.stream, AudioProcessor);
    StopMusicStream(slot.music);
    UnloadMusicStream(slot.music);

    slot = Slot();
}

} // namespace

void Init()
{
#if defined(PLATFORM_WEB)
    // Browsers will not start an AudioContext without a gesture, so the device is
    // created later, on the first key or click.
    g_wantsGesture = true;
#else
    StartDevice();
#endif
}

void NotifyUserGesture()
{
    if (!g_wantsGesture || g_deviceReady) return;
    g_wantsGesture = false;
    StartDevice();
}

void ApplyStage(int stage)
{
    const StageConfig& cfg = StageTuning(stage);

    g_stageVolume  = cfg.volume;
    g_stageCutoff  = cfg.cutoff;
    g_dropoutEvery = cfg.dropoutEvery;
    g_dropoutLen   = cfg.dropoutLen;

    // Each stage gets a fresh dropout window rather than an immediate one, so the
    // silence never lands on the same beat twice.
    g_dropoutTimer = g_dropoutEvery > 0.0f ? g_dropoutEvery * 0.6f : 0.0f;
    g_dropoutLeft  = 0.0f;

    g_gate.store(1.0f, std::memory_order_relaxed);
}

void FadeIn(float rate)
{
    if (rate > 0.0f) g_fadeSpeed = rate;
    if (g_fadeTarget < 1.0f) g_fadeTarget = 1.0f;
}

void FadeOut(float seconds)
{
    const float step = seconds > 0.0f ? 1.0f / seconds : 1.0f;
    g_fadeTarget = 0.0f;
    g_fadeSpeed  = std::max(g_fadeSpeed, step);
}

void SetMasterVolume(float volume)
{
    g_masterVolume = std::clamp(volume, 0.0f, 1.0f);
}

void Update(float dt, Screen screen)
{
    if (!g_deviceReady) return;

    if (g_fadeTarget > g_fadeCurrent)      g_fadeCurrent = std::min(g_fadeTarget, g_fadeCurrent + g_fadeSpeed * dt);
    else if (g_fadeTarget < g_fadeCurrent) g_fadeCurrent = std::max(g_fadeTarget, g_fadeCurrent - g_fadeSpeed * dt);

    // Dropouts are scheduled from the main thread but applied by the audio thread as a
    // smoothed gate, so a dropout is a fade to silence and back, never a click.
    if (g_dropoutEvery > 0.0f && g_fadeCurrent > 0.15f)
    {
        if (g_dropoutLeft > 0.0f)
        {
            g_dropoutLeft -= dt;
            if (g_dropoutLeft <= 0.0f)
            {
                g_gate.store(1.0f, std::memory_order_relaxed);
                g_dropoutTimer = g_dropoutEvery;
            }
        }
        else
        {
            g_dropoutTimer -= dt;
            if (g_dropoutTimer <= 0.0f) OpenDropout();
        }
    }

    g_cutoff.store(g_stageCutoff, std::memory_order_relaxed);

    // Pick the track this screen belongs to, and fall back to the other one so a missing
    // file never means silence where music was playing a moment ago.
    Slot* wanted = TrackForScreen(screen);
    if (wanted && !wanted->loaded)
    {
        Slot* other = (wanted == &g_passage) ? &g_frame : &g_passage;
        wanted = other->loaded ? other : nullptr;
    }

    if (wanted && wanted != g_active)
    {
        g_leaving = g_active;
        g_active  = wanted;

        g_active->fadeTarget = 1.0f;
        g_active->fadeSpeed  = 1.0f / FADE_IN_SECONDS;
        StartSlot(*g_active);

        if (g_leaving)
        {
            g_leaving->fadeTarget = 0.0f;
            g_leaving->fadeSpeed  = 1.0f / FADE_OUT_SECONDS;
        }
    }

    const float passageLevel = g_masterVolume * g_stageVolume * g_fadeCurrent;
    const float frameLevel   = g_masterVolume * g_fadeCurrent;

    AdvanceSlot(g_passage, dt, passageLevel);
    AdvanceSlot(g_frame,   dt, frameLevel);

    // Once the outgoing track is silent it is no longer the outgoing track.
    if (g_leaving && g_leaving->fade <= 0.001f) g_leaving = nullptr;
}

void Shutdown()
{
    UnloadSlot(g_passage);
    UnloadSlot(g_frame);

    g_active  = nullptr;
    g_leaving = nullptr;

    if (g_deviceReady)
    {
        CloseAudioDevice();
        g_deviceReady = false;
    }
}

bool        Ready()          { return g_deviceReady; }
bool        HasTrack()       { return g_passage.loaded; }
bool        HasFrameTrack()  { return g_frame.loaded; }
const char* TrackPath()      { return g_passage.loaded ? g_passage.path.c_str() : ""; }
const char* FrameTrackPath() { return g_frame.loaded ? g_frame.path.c_str() : ""; }

} // namespace sound
} // namespace witness