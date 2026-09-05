// =============================================================================
// src/render/Text.h — TTF 텍스트 렌더 헬퍼.
// =============================================================================
//
// 목적:
//   - SDL_ttf 의 Surface→Texture 변환 + RenderCopy + 자원 해제 4 단계를 한
//     호출로 묶어 발표용 UI 텍스트 (Title / Controls / RoundIntro / MatchEnd)
//     를 간결하게 작성.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 9 Task 9.4
//     Step 1.
//   - SDL_ttf 공식 — TTF_RenderUTF8_Blended 가 alpha 가 부드럽게 섞이는
//     anti-aliased 렌더링.
//
// 자산 부재 정책:
//   - font 가 nullptr 이거나 text 가 비어 있으면 즉시 return — 호출 측에서
//     ResourceManager.GetFont 가 nullptr 일 때도 안전.
// =============================================================================

#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>

// DrawText: (x, y) 에 text 를 color 로 한 줄 렌더.
//   - centered=true 면 (x, y) 를 텍스트 중심점으로, false 면 좌상단으로.
//   - UTF-8 입력 — 한글 / 화살표 (←→↑↓) 등 BMP 영역 다국어 안전.
void DrawText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
              int x, int y, SDL_Color color, bool centered = false);

// TextWidth: 본 폰트로 text 를 렌더했을 때의 픽셀 너비. centering 계산이나
// 다음 글자 위치 산정에 사용. font 가 nullptr 이면 0.
int TextWidth(TTF_Font* font, const std::string& text);
