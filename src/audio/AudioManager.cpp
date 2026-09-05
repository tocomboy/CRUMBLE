// =============================================================================
// src/audio/AudioManager.cpp — SFX / BGM 재생 구현.
// =============================================================================

#include "AudioManager.h"

#include <SDL_mixer.h>

#include <algorithm>

AudioManager::AudioManager(ResourceManager& res) : res_(res) {}

void AudioManager::PlaySfx(const std::string& key) {
    Mix_Chunk* c = res_.GetSfx(key);
    if (!c) return;             // 캐시 miss = 무음.
    // SFX 마다 PlayChannel 직전에 VolumeChunk 호출 — SetSfxVolume 의 갱신이
    // 즉시 반영. 한 chunk 의 볼륨은 재생 중인 채널이 아닌 chunk 자체의
    // 속성이므로 같은 chunk 를 동시 재생하는 채널들이 같은 볼륨을 공유.
    Mix_VolumeChunk(c, sfxVol_);
    Mix_PlayChannel(-1, c, 0);
}

void AudioManager::PlayBgm(const std::string& key, int loops) {
    Mix_Music* m = res_.GetBgm(key);
    if (!m) return;
    Mix_VolumeMusic(bgmVol_);   // BGM 볼륨은 player 단일 — 호출마다 갱신.
    Mix_PlayMusic(m, loops);
}

void AudioManager::StopBgm() { Mix_HaltMusic(); }

void AudioManager::SetSfxVolume(int v) {
    sfxVol_ = std::clamp(v, 0, MIX_MAX_VOLUME);
}

void AudioManager::SetBgmVolume(int v) {
    bgmVol_ = std::clamp(v, 0, MIX_MAX_VOLUME);
}
