// =============================================================================
// src/render/HUD.h — 게임 상단 HUD (HP 바 + 라운드 점수 dots).
// =============================================================================
//
// 본 P3 단계의 HUD 구성:
//   - 상단 좌/우 대칭 HP 바 (HealthBar.h).
//   - 가운데 6 개 dot (좌 P1 측 3 개, 우 P2 측 3 개) — 라운드 승수 표시.
//   - 텍스트 (남은 탄 / 라운드 번호 / "Round 1" 카운트다운) 는 TTF 가 P9 에서
//     도입된 후 추가. 본 P3 는 dot 만으로 시각 정보 제공.
//
// 출처:
//   - 메인 플랜 docs/superpowers/plans/2026-04-28-crumble.md Phase 3 Task 3.5.
//   - 메인 스펙 §4 HUD 디자인.
// =============================================================================

#pragma once

#include <SDL.h>

#include "../game/Player.h"

// DrawHUD:
//   - 상단 HP 바 + 가운데 라운드 dot 들을 한 번에 그린다.
//   - 호출자 (main.cpp) 는 매 frame 의 마지막 (clear → world → players →
//     projectiles → HUD → present) 시점에 호출.
//   - p1RoundsWon / p2RoundsWon: GameFlow.P1Score() / P2Score() 를 그대로 전달.
void DrawHUD(SDL_Renderer* r, int screenW,
             const Player& p1, const Player& p2,
             int p1RoundsWon, int p2RoundsWon);
