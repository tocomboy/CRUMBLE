// =============================================================================
// src/resource/SDLGuards.h — SDL family 전역 초기화의 RAII 가드.
// =============================================================================
//
// 목적:
//   - SDL_Init / IMG_Init / Mix_OpenAudio / TTF_Init 와 그 짝의 Quit 호출을
//     C++ RAII 객체로 감싸 main() 의 cleanup 누락을 원천 차단.
//   - 객체가 stack 에서 만들어지는 순서로 init, 소멸 순서 (역순) 로 quit 가
//     자동 호출되어 발표 직전 손으로 SDL_Quit 을 빼먹을 가능성을 제거.
//
// 사용 예:
//   SDLGuard sdl(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
//   IMGGuard img(IMG_INIT_PNG);
//   MixGuard mix(44100, 2, 2048);
//   TTFGuard ttf;
//   // ... 게임 본체 ...
//   // (자동 정리)
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 9 Task 9.2
//     Step 1.
//   - [6. Resources.pdf] 의 RAII 패턴 (페이지는 PDF 도착 후 보강).
//
// 결정론 / 스레드:
//   - 본 객체들은 main() 안에서 단 한 번씩 생성. 메인 스레드 단독.
//   - 초기화 실패는 std::runtime_error 로 즉시 터짐 — 메시지에 SDL_GetError
//     포함해 디버깅 단서 제공.
// =============================================================================

#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <stdexcept>
#include <string>

// SDLGuard: SDL_Init 를 감싸는 RAII. 생성자에서 init, 소멸자에서 SDL_Quit.
struct SDLGuard {
    explicit SDLGuard(Uint32 flags) {
        if (SDL_Init(flags) != 0) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
    }
    ~SDLGuard() { SDL_Quit(); }
    SDLGuard(const SDLGuard&)            = delete;
    SDLGuard& operator=(const SDLGuard&) = delete;
};

// IMGGuard: SDL_image 의 IMG_Init / IMG_Quit RAII.
//   - IMG_Init 의 반환값은 "성공한 flag 비트마스크" (요청과 다를 수 있음).
//   - 요청한 모든 flag 가 설정되어야 성공으로 간주 (& flags == flags).
struct IMGGuard {
    explicit IMGGuard(int flags) {
        if ((IMG_Init(flags) & flags) != flags) {
            throw std::runtime_error(std::string("IMG_Init failed: ") + IMG_GetError());
        }
    }
    ~IMGGuard() { IMG_Quit(); }
    IMGGuard(const IMGGuard&)            = delete;
    IMGGuard& operator=(const IMGGuard&) = delete;
};

// MixGuard: Mix_OpenAudio + Mix_CloseAudio + Mix_Quit RAII.
//   - freq      : 44100 Hz 표준.
//   - channels  : 2 (스테레오).
//   - chunksize : 2048 (낮을수록 latency 감소, 높을수록 underrun 안전).
struct MixGuard {
    MixGuard(int freq, int channels, int chunksize) {
        if (Mix_OpenAudio(freq, MIX_DEFAULT_FORMAT, channels, chunksize) < 0) {
            throw std::runtime_error(std::string("Mix_OpenAudio failed: ") + Mix_GetError());
        }
    }
    ~MixGuard() { Mix_CloseAudio(); Mix_Quit(); }
    MixGuard(const MixGuard&)            = delete;
    MixGuard& operator=(const MixGuard&) = delete;
};

// TTFGuard: TTF_Init / TTF_Quit RAII. 인자 없음 (TTF 는 일괄 초기화).
struct TTFGuard {
    TTFGuard() {
        if (TTF_Init() < 0) {
            throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
        }
    }
    ~TTFGuard() { TTF_Quit(); }
    TTFGuard(const TTFGuard&)            = delete;
    TTFGuard& operator=(const TTFGuard&) = delete;
};
