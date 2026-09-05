// =============================================================================
// src/audio/AudioManager.h — SFX / BGM 재생 + 볼륨 제어.
// =============================================================================
//
// 목적:
//   - ResourceManager 가 캐시한 Mix_Chunk* / Mix_Music* 를 키로 받아 재생.
//   - 자산 부재 (캐시 miss → nullptr) 는 "조용히 무음" 으로 처리 → 발표 자산
//     누락 시에도 게임은 정상 진행.
//
// 사용 패턴:
//   AudioManager audio(resources);
//   audio.PlaySfx("rifle");        // 무기 발사 시점.
//   audio.PlayBgm("bgm_battle");   // RoundIntro 진입 시.
//   audio.SetSfxVolume(96);        // 0~128 스케일.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 9 Task 9.3.
//   - SDL_mixer 공식 문서 — Mix_VolumeChunk / Mix_PlayChannel / Mix_VolumeMusic.
//
// 결정론 / 스레드:
//   - 모든 PlaySfx / PlayBgm 은 메인 스레드에서만 호출 (시뮬레이션 결과에 따라).
//   - SDL_mixer 자체가 별도 audio thread 를 운용하지만 본 클래스 API 는 thread
//     안전을 별도 보장하지 않는다 (호출 측이 단일 스레드 책임).
// =============================================================================

#pragma once

#include <string>

#include "../resource/ResourceManager.h"

class AudioManager {
public:
    // 볼륨 기본값 (SDL_mixer 의 0~128 스케일):
    //   - SFX 96  : 무기 발사 / 명중음이 BGM 보다 강조되도록.
    //   - BGM 64  : 발표 환경 (스피커 소음) 에서 SFX 가 잘 들리도록 절제.
    //   필요 시 SetSfxVolume / SetBgmVolume 으로 런타임 조정.
    explicit AudioManager(ResourceManager& res);

    // SFX 재생 — Mix_PlayChannel(-1, ...) 로 빈 채널 자동 할당.
    //   - 캐시 miss (Get* 이 nullptr 반환) 시 조용히 return — 무음.
    //   - SDL_mixer 의 default 채널 수 (8) 를 초과하면 가장 오래된 채널이
    //     자동 교체됨.
    void PlaySfx(const std::string& key);

    // BGM 재생 — loops=-1 이면 무한 반복.
    //   - 캐시 miss 시 무음. 이미 재생 중이면 Mix_PlayMusic 가 자동 교체.
    void PlayBgm(const std::string& key, int loops = -1);
    void StopBgm();

    void SetSfxVolume(int v);     // 0..128 — Mix_VolumeChunk 가 적용.
    void SetBgmVolume(int v);     // 0..128 — Mix_VolumeMusic 가 적용.

private:
    ResourceManager& res_;
    int              sfxVol_ = 96;
    int              bgmVol_ = 64;
};
