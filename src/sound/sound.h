#pragma once

namespace witness {

namespace sound {

void Init();
void Update(float dt);
void SetMasterVolume(float v);
void SetMuted(bool m);
bool IsMuted();
void NotifyUserGesture();
void Pop();
void Shutdown();

bool HasTrack();
bool HasPop();
const char* TrackPath();

}

}