// =============================================================================
// src/game/MapEditor.h — 인게임 마우스 맵 에디터 진입점.
//
// 목적:
//   - Title 화면에서 E 키를 누르면 RunMapEditor() 가 호출되어 독립적인
//     SDL 이벤트+렌더 루프를 실행한다.
//   - 게임 메인 루프는 RunMapEditor 가 반환될 때까지 일시 정지된다.
//   - ESC 를 누르면 RunMapEditor 가 반환되고 Title 화면이 재개된다.
//
// 설계 원칙:
//   - PhysicsWorld 에 절대 접근하지 않는다 — 에디터는 시각/JSON 도구이며
//     물리 시뮬레이션과 무관하다.
//   - 게임 main.cpp 의 변경은 최소 (include 1줄 + E 분기 ~5줄).
//
// 출처:
//   - 메인 스펙 §4.7 Map schema (JSON 키 이름 / 타입).
//   - [4. Rendering.pdf] SDL2 Rect fill 패턴.
//   - [3. Game Loop.pdf p.4] 독립 폴링 루프 구조.
// =============================================================================

#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

// RunMapEditor:
//   - window/renderer/font 는 게임 메인이 소유 — 에디터는 빌려 쓴다.
//   - font 가 nullptr 이면 텍스트 없이도 동작한다 (fallback safe).
//   - ESC 를 누르거나 SDL_QUIT 이벤트가 오면 반환한다.
//   - 반환 후 게임 메인 루프가 그대로 재개된다.
void RunMapEditor(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font);
