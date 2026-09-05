// =============================================================================
// src/resource/UniquePtrs.h — SDL family 자원의 RAII 스마트 포인터 alias.
// =============================================================================
//
// 목적:
//   - SDL_Mixer / SDL_TTF / SDL2 의 raw 포인터 자원 (Mix_Chunk*, Mix_Music*,
//     TTF_Font*, SDL_Texture*) 를 std::unique_ptr 로 감싸 소멸자에서 짝의
//     해제 함수 (Mix_FreeChunk 등) 가 자동 호출되도록 한다.
//   - ResourceManager 의 unordered_map 값 타입으로 사용 — emplace 시점에
//     소유권이 map 으로 넘어가고, ClearAll 또는 ResourceManager 소멸 시 모든
//     자원이 한 번에 정리됨.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 9 Task 9.2
//     Step 2.
//   - [6. Resources.pdf] 의 일반적 RAII 자원 wrap 패턴.
//
// 주의:
//   - decltype(&Func) 형태로 deleter 의 함수 포인터 타입을 캡처. 같은 메모리
//     크기의 일반 함수 포인터이므로 unique_ptr 의 sizeof 가 raw* 의 두 배.
//     본 게임 규모에서 의미 있는 비용 아님.
// =============================================================================

#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <memory>

using UniqueChunk   = std::unique_ptr<Mix_Chunk,   decltype(&Mix_FreeChunk)>;
using UniqueMusic   = std::unique_ptr<Mix_Music,   decltype(&Mix_FreeMusic)>;
using UniqueFont    = std::unique_ptr<TTF_Font,    decltype(&TTF_CloseFont)>;
using UniqueTexture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
