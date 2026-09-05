// =============================================================================
// src/resource/ResourceManager.h — SFX / BGM / Font / Texture 캐시.
// =============================================================================
//
// 목적:
//   - 게임 코드가 자원을 "키" 로 요청하면 (예: "rifle", "title", "regular:28")
//     본 매니저가 디스크 로드 또는 캐시 반환을 담당.
//   - 자원의 raw 포인터를 UniquePtrs 로 감싸 소멸자에서 자동 해제.
//   - 같은 키로 재로드 호출 시 디스크 IO 없이 캐시된 포인터 반환.
//
// 사용 패턴:
//   ResourceManager rm;
//   rm.LoadSfx("rifle", "assets/sfx/sfx_rifle.wav");
//   audio.PlaySfx("rifle");                 // GetSfx → Mix_PlayChannel.
//
//   rm.LoadTexture("logo", "assets/img/logo.png", renderer);
//   SDL_Texture* tex = rm.GetTexture("logo");
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 9 Task 9.2
//     Step 3.
//   - [6. Resources.pdf] 의 ResourceManager 패턴.
//
// 자산 부재 정책:
//   - Mix_LoadWAV / Mix_LoadMUS / TTF_OpenFont / IMG_LoadTexture 가 nullptr
//     반환하면 캐시 등록 안 함. Get* 도 nullptr 반환. 호출자는 nullptr 을
//     silence / no-op 로 처리하므로 자산 없어도 빌드·실행 안전.
//   - 발표 직전 실제 자산을 채워 넣으면 자동 동작.
//
// 결정론 / 스레드:
//   - LoadSfx / LoadBgm / LoadFont / LoadTexture 는 디스크 IO 라 메인 스레드
//     단독 또는 AsyncLoader 의 단일 worker thread 에서 호출 — 동시 변경 금지.
//   - Get* 은 메인 스레드에서만 호출 (AsyncLoader 가 Load 를 끝낸 후).
// =============================================================================

#pragma once

#include <string>
#include <unordered_map>

#include "UniquePtrs.h"

class ResourceManager {
public:
    // -------- Load* : 캐시 hit 시 기존 포인터, miss 시 디스크에서 로드 후 캐시.
    Mix_Chunk*   LoadSfx    (const std::string& key, const std::string& path);
    Mix_Music*   LoadBgm    (const std::string& key, const std::string& path);
    TTF_Font*    LoadFont   (const std::string& key, const std::string& path, int size);
    // renderer 는 IMG_LoadTexture 에 전달 — GPU 업로드까지 수행. 메인 스레드 전용.
    SDL_Texture* LoadTexture(const std::string& key, const std::string& path,
                             SDL_Renderer* renderer);

    // -------- Get* : 캐시 조회 전용 (Load* 미수행 키는 nullptr).
    Mix_Chunk*   GetSfx    (const std::string& key) const;
    Mix_Music*   GetBgm    (const std::string& key) const;
    TTF_Font*    GetFont   (const std::string& key, int size) const;
    SDL_Texture* GetTexture(const std::string& key) const;

    // 모든 자원 즉시 해제. 보통 ResourceManager 소멸 시 자동 — 명시 호출은
    // 라운드 사이 자산 교체 같은 특수 시나리오 대비.
    void ClearAll();

private:
    // 폰트는 같은 path 라도 size 별로 별도 인스턴스 — 키에 ":size" suffix.
    std::unordered_map<std::string, UniqueChunk>   sfx_;
    std::unordered_map<std::string, UniqueMusic>   bgm_;
    std::unordered_map<std::string, UniqueFont>    fonts_;
    // 텍스처는 key → GPU 업로드된 SDL_Texture RAII 래퍼.
    std::unordered_map<std::string, UniqueTexture> textures_;
};
