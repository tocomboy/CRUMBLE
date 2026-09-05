// =============================================================================
// src/render/HealthBar.h — 대칭 분할 HP 바 (메인 스펙 §4.2).
// =============================================================================
//
// 시각 구조:
//   - 화면 상단 가로 한 줄을 좌/우 두 영역으로 나눈다.
//   - 가운데 kGap 픽셀 (P1/P2 점수 dot 영역) 만큼 비워둠.
//   - 좌측 (P1) : 채움 막대가 "오른쪽으로 차오름" — 즉 길이=ratio*half 일 때
//                 빨간 막대의 우측 끝이 중앙에 오도록 그린다 (HP=full).
//                 HP=0 이면 우측 끝까지 사라짐.
//   - 우측 (P2) : 좌측 끝 (kGap 우측) 부터 길이=ratio*half 의 파란 막대.
//                 HP=full 이면 우측 끝까지, HP=0 이면 길이=0.
//
// 결과: HP=0 인 쪽이 "중앙 쪽으로 사라지는" 시각 효과 — 메인 스펙 §4.2 의
//        "대칭 HP 바" 요구.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.5.
//   - 메인 스펙 §4.2 HP bar 시각 디자인.
//   - [4. Rendering.pdf] 의 SDL_RenderFillRect / SDL_RenderDrawRect 패턴.
// =============================================================================

#pragma once

#include <SDL.h>

// DrawSplitHpBar:
//   - r        : SDL_Renderer* (window 의 renderer).
//   - screenW  : 창 가로 픽셀 (P3 = 1280). 좌/우 영역 너비 계산에 사용.
//   - p1Ratio  : 0.0 ~ 1.0. P1 의 HP 비율. 1=가득.
//   - p2Ratio  : 0.0 ~ 1.0. P2 의 HP 비율.
//   - 함수 안에서 ratio 가 [0, 1] 밖이면 클램프.
void DrawSplitHpBar(SDL_Renderer* r, int screenW,
                    float p1Ratio, float p2Ratio);
