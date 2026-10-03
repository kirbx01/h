#pragma once

#include "game.h"

namespace witness {
namespace sound {

// Two tracks, used in different places:
//
//   passage  assets/baseorignal.wav        the music the passages are played over
//   frame    assets/openingandclosing.wav  the opening, the ending and the settings
//                                          screen, including when it is opened from a pause
//
// Both are optional. Whichever is missing is simply never heard, and the game says so
// instead of pretending. The passage track is degraded as the world deteriorates: level
// and dropouts are the reliable baseline, and the low-pass filter is an extra that rides
// on raylib's audio-thread processor callback.

void Init();

// Called once per frame from the main loop. The screen decides which track should be
// playing, and the change is crossfaded rather than cut.
void Update(float dt, Screen screen);

// Browsers refuse to start audio without a user gesture; on web this is called the
// moment a key or click arrives, and is a no-op everywhere else.
void NotifyUserGesture();

// Stage 0..STAGE_COUNT-1: sets level, filter corner and dropout rate for the passage track.
void ApplyStage(int stage);

// Fade to silence at a given rate (units per second). The opening sequence uses a slow
// one, gameplay uses a quick one. FadeOut(seconds) is the same thing in seconds.
void FadeIn(float rate);
void FadeOut(float seconds);

void SetMasterVolume(float volume);

void Shutdown();

// Introspection for the credits: is each track loaded, and which file is it?
bool        Ready();
bool        HasTrack();
bool        HasFrameTrack();
const char* TrackPath();
const char* FrameTrackPath();

} // namespace sound
} // namespace witness